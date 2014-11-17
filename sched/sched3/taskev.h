#ifndef __TASKEV_H__
#define __TASKEV_H__

#include "task.h"

#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>

#include <signal.h>

int yield_signal_ev(task_t * const task, const short signal);

#define YIELD_EVSIGNAL(x) yield_signal_ev(THIS_TASK, x)

int yield_sleep_ev(task_t * const task, const struct timeval * const time);

#define YIELD_EVSLEEP(x) yield_sleep_ev(THIS_TASK, &x)

typedef struct acceptReturn {
  int isError;
  evutil_socket_t fd;
  int err;
  task_t * task;
  struct evconnlistener *listener;
} acceptReturn_t;

void reset_acceptReturn(acceptReturn_t *const retval);
int yield_accept_ev(task_t * const task, struct evconnlistener * const listen, acceptReturn_t * const returnValue);

#define DEF_LISTEN_RETVAL(x) acceptReturn_t x; reset_acceptReturn(&x)
#define RESET_LISTEN_RETVAL(x) reset_acceptReturn(&x)
#define YIELD_EVACCEPT(x,y) yield_accept_ev(THIS_TASK, x, &y)

typedef struct socketReturn {
  enum { UNKONWN = 0, READ, WRITE, EVENT } type;
  short events;
  task_t * task;
} socketReturn_t;

void reset_socketReturn(socketReturn_t *const retval);
int yield_socket_ev(task_t * const task, struct bufferevent * const bev, socketReturn_t * const returnValue);
int yield_socket_read(task_t * const task, struct bufferevent * const bev, socketReturn_t * const returnValue, void * const data, size_t datalen);

#define DEF_SOCKET_RETVAL(x) socketReturn_t x; reset_socketReturn(&x)
#define RESET_SOCKET_RETVAL(x) reset_socketReturn(&x)
#define YIELD_EVSOCKET(x,y) yield_socket_ev(THIS_TASK, x, &y)
#define YIELD_EVSOCKET_READ(x,y,data,sizedata) yield_socket_read(THIS_TASK, x, &y, data, sizedata)

int yield_socket_write(task_t * const task, struct bufferevent *bufev, void * const data, size_t size, socketReturn_t * const returnValue);

#define YIELD_EVSOCKET_WRITE(bev, data, len, y) yield_socket_write(THIS_TASK, bev, data, len, &y)

#endif