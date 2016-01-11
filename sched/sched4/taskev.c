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

#ifdef THREADSAFE_SCHED
#include <event2/thread.h>
#endif

#include <event2/event.h>
#include <errno.h>
#include <event2/util.h>
#include <event2/buffer.h>

#include "task_loop_private.h"

////////////////////////////////////////////////////////////////////////////////
#ifndef DEBUG_EV_TASK
#define FAKELOG
#endif

/// Log Macro

#ifndef __FILENAME__
#define __FILENAME__ "taskev.c"
#endif

#include "logmacro.h"

#include "config.h"

#include "likely.h"

// SIGNAL

typedef struct signalinfo {
    task_t * task;
    short event;
    short signal;
} signal_t;

const char* socketReturn_type_to_str(socketReturn_type_t type) {
    static char *toStr[] = {"UNKONWN", "READ", "WRITE", "EVENT"};
    return toStr[type];
}

static void
signal_cb(evutil_socket_t UNUSED_PARAMETER(fd), short UNUSED_PARAMETER(event), void *arg) {
    signal_t * const signal = (signal_t *) arg;
    assert(NULL != signal);
    assert(NULL != signal->task);

    LOG("signal callback for task: %u", signal->task->tid);

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

    LOG("task %u, wait signal", task->tid);

    suspend_task(task); // task suspend;

    //todo check if signal ...
    LOG("task %u, resume for signal", task->tid);

    event_del(ev);
    event_free(ev);
    ev = NULL;

    if (likely(EV_SIGNAL == arg.event)) {
        return (int) arg.event;
    } else {
        LOG("task %u, resume for signal but ...", task->tid);
        return -1;
    }
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

    LOG("timeout callback for task: %u", timeout->task->tid);

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

    LOG("task %u, wait timeout", task->tid);

    suspend_task(task); // task suspend;

    LOG("task %u, resume for signal", task->tid);

    event_del(ev);
    event_free(ev);
    ev = NULL;

    return (int) arg.event;
}

int yield_sleep_ev_sec(task_t * const task, unsigned int sec) {
    struct timeval time = {0, 0};
    time.tv_sec = sec;
    return yield_sleep_ev(task, &time);
}

int yield_sleep_ev_msec(task_t * const task, unsigned int msec) {
    struct timeval time = {0, 0};
    const unsigned int sec = msec / 1000;
    const unsigned int usec = (msec % 1000) * 1000;
    time.tv_sec = sec;
    time.tv_usec = usec;
    return yield_sleep_ev(task, &time);
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
    if (likely(NULL != task)) {
        schedule_task(task);
    }

    free(param);
    param = NULL;
}

void wakeup_immediate(tid_t tid) {
    static struct timeval const immediate = {0 /* s */, 0 /* us */};

    task_t * const task = getTask(tid);
    if (likely(NULL != task)) {
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
    assert(NULL != retval);
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

    LOG("accept connection callback for task: %u", retval->task->tid);

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

    LOG("accept connection callback for task: %u (error)", retval->task->tid);

    retval->isError = 1;
    retval->fd = -1;
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

    LOG("task %u wait accept connection", task->tid);

    suspend_task(task); // task suspend;

    LOG("task %u resume for accept connection", task->tid);

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
    assert(NULL != retval->task);
    assert(NULL != bev);

    LOG("read callback for task: %u", retval->task->tid);

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
    assert(NULL != retval->task);
    assert(NULL != bev);

    LOG("event callback for task: %u", retval->task->tid);

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
    assert(NULL != retval->task);
    assert(NULL != bev);

    LOG("write callback for task: %u", retval->task->tid);

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
    if ((NULL != input) && (evbuffer_get_length(input) > 0)) {
        returnValue->type = READ;
        returnValue->events = EV_READ;
        return returnValue->events;
    }

    bufferevent_setcb(bev,
            /* read  */ echo_read_cb,
            /* write */ echo_write_cb,
            /* event */ echo_event_cb, returnValue);
    bufferevent_enable(bev, EV_READ || EV_WRITE);
    LOG("task %u wait read/write", task->tid);
    suspend_task(task); // task suspend;
    LOG("task %u resume for read/write", task->tid);
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

    bufferevent_setcb(bev,
            /* read  */ echo_read_cb,
            /* write */ echo_write_cb,
            /* event */ echo_event_cb, returnValue);
    bufferevent_enable(bev, EV_READ || EV_WRITE);

    if (unlikely(bufferevent_socket_connect_hostname(
            bev, dns_base, family, hostname, port) == -1)) {
        bufferevent_disable(bev, EV_READ || EV_WRITE);
        bufferevent_setcb(bev, NULL, NULL, NULL, NULL);
        returnValue->type = UNKONWN;
        returnValue->events = 0;
        return -1;
    } else {
        LOG("task %u wait read/write/event", task->tid);
        suspend_task(task); // task suspend;
        LOG("task %u wait read/write/event", task->tid);
        bufferevent_disable(bev, EV_READ || EV_WRITE);
        bufferevent_setcb(bev, NULL, NULL, NULL, NULL);

        if (likely((returnValue->type == EVENT) &&
                (0 != (returnValue->events & BEV_EVENT_CONNECTED)))) {
            return 0;
        } else {
            LOG("task %u resumed but with error", task->tid);
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
    if (likely(NULL != input)) {
        if (evbuffer_get_length(input) > 0) {
            returnValue->type = READ;
            returnValue->events = EV_READ;
            goto ENDFUNC;
        }
    }

    bufferevent_setcb(bev,
            /* read  */ echo_read_cb,
            /* write */ NULL,
            /* event */ echo_event_cb, returnValue);
    bufferevent_enable(bev, EV_READ);
    LOG("task %u wait read", task->tid);
    suspend_task(task); // task suspend;
    LOG("task %u resume for read", task->tid);
    bufferevent_disable(bev, EV_READ);
    bufferevent_setcb(bev, NULL, NULL, NULL, NULL);

ENDFUNC:
    if (likely(returnValue->type == READ)) {
        input = bufferevent_get_input(bev);
        if (likely(NULL != input)) {
            return evbuffer_remove(input, data, datalen);
        } else {
            LOG("task %u resumed but with error", task->tid);
        }
    } else {
        LOG("task %u resumed but with error and event: %s", task->tid, socketReturn_type_to_str(returnValue->type));
    }
    return -1;
}

int bufferevent_write(struct bufferevent *bufev,
        const void *data, size_t size);

int yield_socket_write(task_t * const task, struct bufferevent * const bev, void * const data, size_t size, socketReturn_t * const returnValue) {
    assert(NULL != returnValue);
    assert(NULL != task);
    assert(NULL != bev);
    if (likely((size > 0) && (data != NULL))) {
        init_socketReturn(returnValue, task);
        bufferevent_setcb(bev, /* read */ NULL,
                /* write */ echo_write_cb,
                /* event */ echo_event_cb, returnValue);
        bufferevent_enable(bev, EV_WRITE);
        bufferevent_write(bev, data, size);
        LOG("task %u wait write", task->tid);
        suspend_task(task); // task suspend;
        LOG("task %u resume for write", task->tid);
        bufferevent_disable(bev, EV_WRITE);
        bufferevent_setcb(bev, NULL, NULL, NULL, NULL);
        return returnValue->events;
    } else {
        returnValue->type = UNKONWN;
        returnValue->events = 0;
        return -1;
    }
}

void * taskev_create(void) {
    static int lib_event_initialized = 0;

    if (unlikely(0 == lib_event_initialized)) {
#ifdef THREADSAFE_SCHED
#ifdef WIN32
        evthread_use_windows_threads();
#else
        evthread_use_pthreads();
#endif
#endif
        lib_event_initialized = 1;
    }

    void * retval = (void *) event_base_new();
    assert(NULL != retval);
    return retval;
}

void taskev_destroy(void *base) {
    assert(NULL != base);
    event_base_free((struct event_base *) base);
}

void taskev_event_poll(void *base) {
    assert(NULL != base);
    event_base_loop((struct event_base *) base, EVLOOP_ONCE /* EVLOOP_NONBLOCK */);
}

const struct sched_mainloop_interface task_libevent = {
    /* create = */ taskev_create,
    /* destroy = */ taskev_destroy,
    /* event_poll = */taskev_event_poll
};
