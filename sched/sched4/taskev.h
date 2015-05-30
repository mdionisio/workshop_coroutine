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
 * File:   taskev.h
 * Author: Michele Dionisio
 *
 * Created on 18 ottobre 2014
 */

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

int yield_socket_ev_connect(task_t * const task, 
                            struct bufferevent * const bev, 
                            struct evdns_base * const dns_base, 
                            int family, const char * hostname, int port,
                            socketReturn_t * const returnValue);

#define DEF_SOCKET_RETVAL(x) socketReturn_t x; reset_socketReturn(&x)
#define RESET_SOCKET_RETVAL(x) reset_socketReturn(&x)
#define YIELD_EVSOCKET(x,y) yield_socket_ev(THIS_TASK, x, &y)
#define YIELD_EVSOCKET_READ(x,y,data,sizedata) yield_socket_read(THIS_TASK, x, &y, data, sizedata)

#define YIELD_EVSOCKET_CONNECT(b, d, f, h, p, y) yield_socket_ev_connect(THIS_TASK, b, d, f, h, p, &y)

int yield_socket_write(task_t * const task, struct bufferevent *bufev, void * const data, size_t size, socketReturn_t * const returnValue);

#define YIELD_EVSOCKET_WRITE(bev, data, len, y) yield_socket_write(THIS_TASK, bev, data, len, &y)

#ifdef THREADSAFE_SCHED

void wakeup_immediate(tid_t tid);
#define RESUME_FROM_THREAD(tid) wakeup_immediate(tid)

#endif


#endif