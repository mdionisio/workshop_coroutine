/*
 *   This file is part of Cooperative-LibEvent.
 *
 *   Cooperative-LibEvent is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Cooperative-LibEvent is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with Cooperative-LibEvent.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * File:   taskev.c
 * Author: Michele Dionisio
 *
 * Created on 18 ottobre 2014
 */

#include "taskev.h"
#include "unused.h"

#include <assert.h>

#include <event2/event.h>
#include <errno.h>
#include <event2/util.h>
#include <event2/buffer.h>

// SIGNAL

typedef struct signalinfo {
    task_t * task;
    short event;
    short signal;
} signal_t;

static void
signal_cb(evutil_socket_t UNUSED_PARAMETER(fd), short UNUSED_PARAMETER(event), void *arg) {
    signal_t * const signal = (signal_t *) arg;
    assert(NULL != signal);

    signal->event = EV_SIGNAL;
    schedule_task(signal->task);
}

int yield_signal_ev(task_t * const task, const short signal) {
    assert(NULL != task);
    assert(NULL != task->sched);
    assert(NULL != task->sched->base);
    struct event *ev = NULL;
    signal_t arg;

    arg.task = task;
    arg.event = 0;
    arg.signal = signal;

    /* call sighup_function on a HUP signal */
    ev = evsignal_new(task->sched->base, signal, signal_cb, &arg);
    assert(NULL != ev);

    event_add(ev, NULL);

    suspend_task(task); // task suspend;

    event_del(ev);
    event_free(ev);
    ev = NULL;

    return (int) arg.event;
}

// TIMEOUT

typedef struct timeoutinfo {
    task_t * task;
    short event;
} timeout_t;

static void ev_timeout_cb(evutil_socket_t UNUSED_PARAMETER(fd), short what, void *arg) {
    timeout_t * const timeout = (timeout_t*) arg;
    assert(NULL != timeout);
    assert(NULL != timeout->task);

    timeout->event = what;
    schedule_task(timeout->task);
}

int yield_sleep_ev(task_t * const task, const struct timeval * const time) {
    assert(NULL != task);
    assert(NULL != task->sched);
    assert(NULL != task->sched->base);
    assert(NULL != time);

    timeout_t arg;
    arg.event = 0;
    arg.task = task;

    struct event * ev = event_new(task->sched->base,
            -1,
            EV_TIMEOUT,
            ev_timeout_cb,
            &arg);
    assert(NULL != ev);

    event_add(ev, time);

    suspend_task(task); // task suspend;

    event_del(ev);
    event_free(ev);
    ev = NULL;

    return (int) arg.event;
}

#ifdef THREADSAFE_SCHED

struct callback_param {
    struct event * ev;
    tid_t tid;
};

static void ev_timeout_cb_thread(evutil_socket_t UNUSED_PARAMETER(fd), short UNUSED_PARAMETER(what), void *arg) {
    struct callback_param * param = (struct callback_param*) arg;
    assert(NULL != param);
    assert(NULL != param->ev);

    event_del(param->ev);
    param->ev = NULL;

    task_t * task = getTask(param->tid);
    if (task != NULL) {
        schedule_task(task);
    }

    free(param);
    param = NULL;
}

void wakeup_immediate(tid_t tid) {
    static struct timeval const immediate = {0 /* s */, 0 /* us */};

    task_t * task = getTask(tid);
    if (task != NULL) {
        struct callback_param * const param = (struct callback_param *) malloc(sizeof (struct callback_param));
        assert(NULL != param);

        struct event * ev = event_new(task->sched->base,
                -1,
                EV_TIMEOUT,
                ev_timeout_cb_thread,
                param);
        assert(NULL != ev);

        param->tid = tid;
        param->ev = ev;

        event_add(ev, &immediate);
    }
}
#endif

// LISTEN

static void init_acceptReturn(acceptReturn_t * const retval, struct evconnlistener * const listener, task_t * const task) {
    assert(NULL != retval);
    assert(NULL != listener);
    assert(NULL != task);
    retval->isError = 0;
    retval->fd = -1;
    retval->err = 0;
    retval->task = task;
}

void reset_acceptReturn(acceptReturn_t * const retval) {
    retval->isError = 0;
    retval->fd = -1;
    retval->err = 0;
}

static void accept_conn_cb(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int UNUSED_PARAMETER(socklen), void *ctx) {
    acceptReturn_t * const retval = (acceptReturn_t *) ctx;
    assert(NULL != retval);
    assert(NULL != address);
    assert(NULL != listener);
    assert(NULL != retval->task);
    assert(0 != fd);

    retval->isError = 0;
    retval->fd = fd;
    retval->err = 0;
    schedule_task(retval->task);
    // don't call any other callback on this
    evconnlistener_disable(listener);
    evconnlistener_set_cb(listener, NULL, NULL);
    evconnlistener_set_error_cb(listener, NULL);
}

static void accept_error_cb(struct evconnlistener *listener, void *ctx) {
    acceptReturn_t * const retval = (acceptReturn_t *) ctx;
    assert(NULL != retval);
    assert(NULL != listener);

    retval->isError = 1;
    retval->fd = 0;
    retval->err = EVUTIL_SOCKET_ERROR();
    schedule_task(retval->task);
    // don't call any other callback on this
    evconnlistener_disable(listener);
    evconnlistener_set_cb(listener, NULL, NULL);
    evconnlistener_set_error_cb(listener, NULL);
}

int yield_accept_ev(task_t * const task, struct evconnlistener * const listen, acceptReturn_t * const returnValue) {
    assert(NULL != returnValue);
    assert(NULL != listen);
    assert(NULL != task);
    init_acceptReturn(returnValue, listen, task);

    evconnlistener_set_cb(listen, accept_conn_cb, returnValue);
    evconnlistener_set_error_cb(listen, accept_error_cb);
    evconnlistener_enable(listen);

    suspend_task(task); // task suspend;

    evconnlistener_disable(listen);
    evconnlistener_set_cb(listen, NULL, NULL);
    evconnlistener_set_error_cb(listen, NULL);

    return returnValue->fd;
}

// SOCKET

static void init_socketReturn(socketReturn_t * const retval, task_t * const task) {
    assert(NULL != retval);
    assert(NULL != task);
    retval->type = UNKONWN;
    retval->task = task;
    retval->events = 0;
}

void reset_socketReturn(socketReturn_t * const retval) {
    assert(NULL != retval);
    retval->type = UNKONWN;
    retval->events = 0;
}

static void echo_read_cb(struct bufferevent *bev, void *ctx) {
    socketReturn_t * const retval = (socketReturn_t *) ctx;
    assert(NULL != retval);
    assert(NULL != bev);

    retval->type = READ;
    retval->events = EV_READ;
    schedule_task(retval->task);
    // don't call any other callback on this

    bufferevent_disable(bev, EV_READ | EV_WRITE);
    bufferevent_setcb(bev, NULL, NULL, NULL, NULL);
}

static void echo_event_cb(struct bufferevent *bev, short events, void *ctx) {
    socketReturn_t * const retval = (socketReturn_t *) ctx;
    assert(NULL != retval);
    assert(NULL != bev);

    retval->type = EVENT;
    retval->events = events;
    schedule_task(retval->task);
    // don't call any other callback on this

    bufferevent_disable(bev, EV_READ | EV_WRITE);
    bufferevent_setcb(bev, NULL, NULL, NULL, NULL);
}

static void echo_write_cb(struct bufferevent *bev, void *ctx) {
    socketReturn_t * const retval = (socketReturn_t *) ctx;
    assert(NULL != retval);
    assert(NULL != bev);

    retval->type = WRITE;
    retval->events = EV_WRITE;
    schedule_task(retval->task);
    // don't call any other callback on this

    bufferevent_disable(bev, EV_READ | EV_WRITE);
    bufferevent_setcb(bev, NULL, NULL, NULL, NULL);
}

int yield_socket_ev(task_t * const task, struct bufferevent * const bev, socketReturn_t * const returnValue) {
    assert(NULL != returnValue);
    assert(NULL != bev);
    assert(NULL != task);
    init_socketReturn(returnValue, task);

    // check if thereis data to drain
    struct evbuffer * const input = bufferevent_get_input(bev);
    if (NULL != input) {
        if (evbuffer_get_length(input) > 0) {
            returnValue->type = READ;
            returnValue->events = EV_READ;
            return returnValue->events;
        }
    }

    bufferevent_setcb(bev, /* read */ echo_read_cb,
            /* write */ echo_write_cb,
            /* event */ echo_event_cb, returnValue);
    bufferevent_enable(bev, EV_READ || EV_WRITE);
    suspend_task(task); // task suspend;
    bufferevent_disable(bev, EV_READ || EV_WRITE);
    bufferevent_setcb(bev, NULL, NULL, NULL, NULL);
    return returnValue->events;
}

int yield_socket_ev_connect(task_t * const task,
        struct bufferevent * const bev,
        struct evdns_base * const dns_base,
        int family, const char * hostname, int port,
        socketReturn_t * const returnValue) {
    assert(NULL != returnValue);
    assert(NULL != bev);
    assert(NULL != hostname);
    assert(NULL != task);
    init_socketReturn(returnValue, task);

    bufferevent_setcb(bev, /* read */ echo_read_cb,
            /* write */ echo_write_cb,
            /* event */ echo_event_cb, returnValue);
    bufferevent_enable(bev, EV_READ || EV_WRITE);

    if (bufferevent_socket_connect_hostname(bev, dns_base, family, hostname, port) == -1) {
        bufferevent_disable(bev, EV_READ || EV_WRITE);
        bufferevent_setcb(bev, NULL, NULL, NULL, NULL);
        returnValue->type = UNKONWN;
        returnValue->events = 0;
        return -1;
    } else {
        suspend_task(task); // task suspend;
        bufferevent_disable(bev, EV_READ || EV_WRITE);
        bufferevent_setcb(bev, NULL, NULL, NULL, NULL);

        if ((returnValue->type == EVENT) && (returnValue->events & BEV_EVENT_CONNECTED)) {
            return 0;
        } else {
            return -2;
        }
    }
}

int yield_socket_read(task_t * const task, struct bufferevent * const bev, socketReturn_t * const returnValue, void * const data, size_t datalen) {
    assert(NULL != returnValue);
    assert(NULL != data);
    assert(0 != datalen);
    assert(NULL != task);
    assert(NULL != bev);
    init_socketReturn(returnValue, task);

    // check if thereis data to drain
    struct evbuffer * input = bufferevent_get_input(bev);
    if (NULL != input) {
        if (evbuffer_get_length(input) > 0) {
            returnValue->type = READ;
            returnValue->events = EV_READ;
            goto ENDFUNC;
        }
    }

    bufferevent_setcb(bev, /* read */ echo_read_cb,
            /* write */ NULL,
            /* event */ echo_event_cb, returnValue);
    bufferevent_enable(bev, EV_READ);
    suspend_task(task); // task suspend;
    bufferevent_disable(bev, EV_READ);
    bufferevent_setcb(bev, NULL, NULL, NULL, NULL);

ENDFUNC:
    if (returnValue->type == READ) {
        input = bufferevent_get_input(bev);
        if (NULL != input) {
            return evbuffer_remove(input, data, datalen);
        }
    }
    return -1;
}

int bufferevent_write(struct bufferevent *bufev,
        const void *data, size_t size);

int yield_socket_write(task_t * const task, struct bufferevent * const bev, void * const data, size_t size, socketReturn_t * const returnValue) {
    assert(NULL != returnValue);
    assert(NULL != task);
    assert(NULL != bev);
    if ((size > 0) && (data != NULL)) {
        init_socketReturn(returnValue, task);
        bufferevent_setcb(bev, /* read */ NULL,
                /* write */ echo_write_cb,
                /* event */ echo_event_cb, returnValue);
        bufferevent_enable(bev, EV_WRITE);
        bufferevent_write(bev, data, size);
        suspend_task(task); // task suspend;
        bufferevent_disable(bev, EV_WRITE);
        bufferevent_setcb(bev, NULL, NULL, NULL, NULL);
        return returnValue->events;
    } else {
        returnValue->type = UNKONWN;
        returnValue->events = 0;
        return -1;
    }
}