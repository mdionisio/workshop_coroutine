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
 * File:   main.c
 * Author: Michele Dionisio
 *
 * Created on 18 ottobre 2014
 */

#include "task.h"
#include "taskev.h"

#include <stdio.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

////////////////////////////////////////////////////////////////////////////////




/// Start Example
static char const MESSAGE[] = "Hello, World!\n";
/** task that read from socket
 *
 * @param tid  my thread id
 */
void task_socket(tid_t const tid)
{
    uint8_t readingBuffer[1024];
    int datalen;
    int i;
    
    INIT_TASK;
    
    evutil_socket_t * const fd = (evutil_socket_t *)(THIS_TASK->data);
    /* We got a new connection! Set up a bufferevent for it. */
    struct bufferevent *bev = bufferevent_socket_new(THIS_SCHED->base, *fd, BEV_OPT_CLOSE_ON_FREE);
        
    DEF_SOCKET_RETVAL(retval);
    
    int running = 1;

    while (running && !ISSTOPREQUESTTASK) {
        datalen = YIELD_EVSOCKET_READ(bev, retval, (void *)readingBuffer, sizeof(readingBuffer));
        
checkreturn:            // label to mark check returned value
        switch(retval.type) {
            case READ:
                if (datalen>0) {
                    printf("receive %d data:\n", datalen);
                    for (i=0; i<datalen; i++) {
                        if (isprint(readingBuffer[i])) {
                            printf("%c", (char)readingBuffer[i]);
                        } else {
                            printf("\\x%02x", (int)readingBuffer[i]);
                        }
                    }
                    printf("\n");
                    
                    RESET_SOCKET_RETVAL(retval);
                    YIELD_EVSOCKET_WRITE(bev, (void *)MESSAGE, sizeof(MESSAGE), retval);
                    goto checkreturn; // goto check feedback
                } else { // there is something strange
                    printf("no data to read any more \n");
                    running = 0;
                }
                break;
            case WRITE:
                printf("data write\n");
                break;
            case EVENT:
                if (retval.events & BEV_EVENT_ERROR) {
                    perror("Error from bufferevent\n");
                }
                if (retval.events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
                    running = 0;
                }
                break;
            case UNKONWN:
                printf("operation canceled %s\n", __func__);
                running = 0;
                break;
        }
        RESET_SOCKET_RETVAL(retval);
    } // end while
    
    bufferevent_free(bev);
    
    printf("end %s\n", __func__);

    RETURN_TASK(2);
}

////////////////////////////////////////////////////////////////////////////////


/** task that accept new socket
 *
 * @param tid  my thread id
 */
void task_listen(tid_t const tid)
{
    INIT_TASK;
    
    struct evconnlistener *listener;
    struct sockaddr_in sin;

    /* Clear the sockaddr before using it, in case there are extra
     * platform-specific fields that can mess us up. */
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(1234);
    
    /* Create a new listener */
    listener = evconnlistener_new_bind(THIS_SCHED->base, NULL, NULL,
				       LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1,
				       (struct sockaddr *) &sin, sizeof(sin));
    
    if (!listener) {
	perror("Couldn't create listener\n");
	RETURN_TASK(1);
    }
    
    DEF_LISTEN_RETVAL(returnValue);
    int running = 1;
    
    while (running && !ISSTOPREQUESTTASK) {
      if (YIELD_EVACCEPT(listener, returnValue) > 0) {
        if (returnValue.isError == 0) {
          printf("connection accepted\n");
          tid_t const tid = new_task_sched(THIS_SCHED, 
                                           (taskFunc_t) task_socket, 
                                           TASK_NONE);
          
          evutil_socket_t * const pTmp = (evutil_socket_t *)malloc(sizeof(evutil_socket_t));
          *(pTmp) = returnValue.fd;
          
          task_t * const newReadtask = getTask(tid);
          newReadtask->data = (void *)pTmp;
        } else {
          fprintf(stderr, "Got an error %d (%s) on the listener. "
                  "Shutting down.\n", returnValue.err, evutil_socket_error_to_string(returnValue.err));
          running = 0;
        }
      } else {
        printf("operation canceled %s\n", __func__);
        running = 0;
      }
      RESET_LISTEN_RETVAL(returnValue);
    } // end while
    
    evconnlistener_free(listener);
    listener = NULL;
    
    printf("end %s\n", __func__);

    RETURN_TASK(0);
}


////////////////////////////////////////////////////////////////////////////////
	


/** task that show how sleep works
 *
 * @param tid  my thread id
 */
void task_counter(tid_t const tid) {
    INIT_TASK;
    
    struct timeval const five_sec = { 5, 0 };
    
    int i = 0;
    int running = 1;
    while (running && !ISSTOPREQUESTTASK) {        
        if (YIELD_EVSLEEP(five_sec)) {
            printf("counter %d\n", i++);
        } else {
            printf("operation canceled %s\n", __func__);
            running = 0;
        }
    } // end while
    printf("end %s\n", __func__);
    RETURN_TASK(0);
}

/** task that manage SIGINT signal
 *
 * @param tid  my thread id
 */
void task_signal(tid_t const tid) {
    INIT_TASK;
    
    int running = 1;
    while (running && !ISSTOPREQUESTTASK) {        
        if (YIELD_EVSIGNAL(SIGINT)) {
            printf("signal received\n");
            // iter over all Scheduled task and mark it to be stopped
            iterSchedTask(stopRequest_task, THIS_SCHED);
            // iter over all Scheduled task and wakeup it
            iterSchedTask(schedule_task, THIS_SCHED);
        } else {
            printf("operation canceled %s\n", __func__);
            running = 0;
        }
    } // end while
    
    printf("end %s\n", __func__);
    RETURN_TASK(0);
}



/** MAIN
 * 
 * @param argc
 * @param argv
 * @return 
 */
int main(int argc, char *argv[])
{
    printf("Create Scheduler\n");
    // create a new cooperative scheduler
    sched_t *sched = create_sched(SCHED_NONE || SCHED_END_WITHOUT_TASKS);
    
    printf("Create Signal Task\n");
    new_task_sched(sched, (taskFunc_t) task_signal, TASK_NONE);

    printf("Create Listening Task\n");
    new_task_sched(sched, (taskFunc_t) task_listen, TASK_NONE);
    
    printf("Create Counter Task\n");
    new_task_sched(sched, (taskFunc_t) task_counter, TASK_NONE);

    printf("Start Mainloop\n");
    // start scheduler
    mainloop_sched(sched);

    printf("Destroy Scheduler\n");
    // scheduling is terminated so the scheduler can be detroyed
    destroy_sched(&sched);
    
    printf("mainloop terminated\n");

    return 0;
}

