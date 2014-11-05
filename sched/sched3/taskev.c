#include "taskev.h"

#include <event2/event.h>
#include <errno.h>
#include <event2/util.h>

// TIMEOUT

typedef struct timeoutinfo { 
    task_t * task;
    int evEvent;
} timeout_t;

static void ev_timeout_cb(evutil_socket_t fd, short what, void *arg) {
    timeout_t * const timeout = (timeout_t*)arg;
    
    timeout->evEvent = 1;
    schedule_task(timeout->task);
}

int yield_sleep_ev(task_t * const task, const struct timeval * const time) {
    timeout_t arg;
    arg.task = task;
    arg.evEvent = 0;
    
    struct event * ev = event_new(  task->sched->base,       
                                    -1,                
                                    EV_TIMEOUT,        
                                    ev_timeout_cb,     
                                    &arg);        
    event_add(ev, time);      
    
    suspend_task(task);  // task suspend;
    
    event_del(ev);                                         
    event_free(ev);  
    
    return arg.evEvent;
}

// LISTEN


static void init_acceptReturn(acceptReturn_t * const retval, struct evconnlistener * const listener, task_t * const task) {
    retval->evEvent = 0;
    retval->isError = 0;
    retval->fd = -1;
    retval->err = 0;
    retval->task = task;
    retval->listener = listener;
}

void reset_acceptReturn(acceptReturn_t *const retval) {
    retval->evEvent = 0;
    retval->isError = 0;
    retval->fd = -1;
    retval->err = 0;
}

static void accept_conn_cb(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen, void *ctx)
{
    acceptReturn_t * const retval = (acceptReturn_t *) ctx;
    retval->isError = 0;
    retval->fd = fd;
    retval->err = 0;
    retval->evEvent = 1;
    schedule_task(retval->task);
    // don't call any other callback on this
    evconnlistener_disable(retval->listener);
    evconnlistener_set_cb(listener, NULL, NULL);
    evconnlistener_set_error_cb(listener, NULL);
}
 
static void accept_error_cb(struct evconnlistener *listener, void *ctx)
{
    acceptReturn_t * const retval = (acceptReturn_t *) ctx;
    retval->isError = 1;
    retval->fd = 0;
    retval->err = EVUTIL_SOCKET_ERROR();
    retval->evEvent = 1;
    schedule_task(retval->task);
    // don't call any other callback on this
    evconnlistener_disable(retval->listener);
    evconnlistener_set_cb(listener, NULL, NULL);
    evconnlistener_set_error_cb(listener, NULL);
}

int yield_accept_ev(task_t * const task, struct evconnlistener * const listen, acceptReturn_t * const returnValue) {
    init_acceptReturn(returnValue, listen, task);
    
    evconnlistener_set_cb(listen, accept_conn_cb, returnValue);
    evconnlistener_set_error_cb(listen, accept_error_cb);
    evconnlistener_enable(listen);
    
    suspend_task(task);  // task suspend;
      
    evconnlistener_disable(listen);
    evconnlistener_set_cb(listen, NULL, NULL);
    evconnlistener_set_error_cb(listen, NULL);
    
    return returnValue->evEvent;
}

// SOCKET

static void init_socketReturn(socketReturn_t * const retval, task_t * const task) {
    retval->task = task;
    retval->evEvent = 0;
}

void reset_socketReturn(socketReturn_t *const retval) {
    retval->evEvent = 0;
}

static void echo_read_cb(struct bufferevent *bev, void *ctx)
{
    socketReturn_t * const retval = (socketReturn_t *)ctx;
    retval->type = READ;
    retval->evEvent = 1;
    schedule_task(retval->task);
    // don't call any other callback on this
    
    bufferevent_disable(bev, EV_READ | EV_WRITE);
    bufferevent_setcb(bev, NULL, NULL, NULL, NULL);  
}

static void echo_event_cb(struct bufferevent *bev, short events, void *ctx)
{
    socketReturn_t * const retval = (socketReturn_t *)ctx;
    retval->type = EVENT;
    retval->events = events;
    retval->evEvent = 1;
    schedule_task(retval->task);
    // don't call any other callback on this
    
    bufferevent_disable(bev, EV_READ | EV_WRITE);
    bufferevent_setcb(bev, NULL, NULL, NULL, NULL);  
}

static void echo_write_cb(struct bufferevent *bev, void *ctx)
{
    socketReturn_t * const retval = (socketReturn_t *)ctx;
    retval->type = WRITE;
    retval->events = 1;
    schedule_task(retval->task);
    // don't call any other callback on this
    
    bufferevent_disable(bev, EV_READ | EV_WRITE);
    bufferevent_setcb(bev, NULL, NULL, NULL, NULL);  
}

int yield_socket_ev(task_t * const task, struct bufferevent * const bev, socketReturn_t * const returnValue) {
    init_socketReturn(returnValue, task);
    bufferevent_setcb(bev, /* read */echo_read_cb, /* write */ echo_write_cb, /* event */ echo_event_cb, returnValue);
    bufferevent_enable(bev, EV_READ );
    suspend_task(task);  // task suspend;
    bufferevent_disable(bev, EV_READ );
    bufferevent_setcb(bev, NULL, NULL, NULL, NULL);
    return returnValue->evEvent;
}

int bufferevent_write(struct bufferevent *bufev,
    const void *data, size_t size);

int yield_socket_write(task_t * const task, struct bufferevent * const bev, void * const data, size_t size) {
    socketReturn_t returnValue;
    init_socketReturn(&returnValue, task);
    bufferevent_setcb(bev, /* read */NULL, /* write */ echo_write_cb, /* event */ echo_event_cb, &returnValue);
    bufferevent_enable(bev, EV_WRITE );
    bufferevent_write(bev, data, size);
    suspend_task(task);  // task suspend;
    bufferevent_disable(bev, EV_WRITE );
    bufferevent_setcb(bev, NULL, NULL, NULL, NULL);
    return returnValue.evEvent;
}