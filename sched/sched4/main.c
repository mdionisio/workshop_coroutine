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
#include <assert.h>

#include "unused.h"

////////////////////////////////////////////////////////////////////////////////

/// Log Macro

#ifndef __FILENAME__
#define __FILENAME__ "main.c"
#endif

#include "logmacro.h"

////////////////////////////////////////////////////////////////////////////////



/// Start Example
static char const MESSAGE[] = "Hello, World!\n";

/** task that read from socket
 *
 * @param tid  my thread id
 */
void task_socket(tid_t const tid) {
    uint8_t readingBuffer[1024];
    int datalen;
    int i;

    INIT_TASK;

    evutil_socket_t * const fd = (evutil_socket_t *) (THIS_TASK->data);
    /* We got a new connection! Set up a bufferevent for it. */
    struct bufferevent *bev = bufferevent_socket_new(THIS_SCHED->base, *fd, BEV_OPT_CLOSE_ON_FREE);

    DEF_SOCKET_RETVAL(retval);

    int running = 1;

    while ((NULL != bev) && running && !ISSTOPREQUESTTASK) {
        datalen = YIELD_EVSOCKET_READ(bev, retval, (void *) readingBuffer, sizeof (readingBuffer));

checkreturn: // label to mark check returned value
        switch (retval.type) {
            case READ:
                if (datalen > 0) {
                    LOG("receive %d data:", datalen);
                    BEGINLOG();
                    for (i = 0; i < datalen; i++) {
                        if (isprint(readingBuffer[i])) {
                            SIMPLELOG("%c", (char) readingBuffer[i]);
                        } else {
                            SIMPLELOG("\\x%02x", (int) readingBuffer[i]);
                        }
                    }
                    ENDLOG();

                    RESET_SOCKET_RETVAL(retval);
                    YIELD_EVSOCKET_WRITE(bev, (void *) MESSAGE, sizeof (MESSAGE), retval);
                    goto checkreturn; // goto check feedback
                } else { // there is something strange
                    LOG("no data to read any more");
                    running = 0;
                }
                break;
            case WRITE:
                LOG("data write");
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
                LOG("operation canceled");
                running = 0;
                break;
        }
        RESET_SOCKET_RETVAL(retval);
    } // end while

    if (NULL != bev) {
        bufferevent_free(bev);
        bev = NULL;
    }

    LOG("end");

    RETURN_TASK(2);
}

////////////////////////////////////////////////////////////////////////////////

/** task that accept new socket
 *
 * @param tid  my thread id
 */
void task_listen(tid_t const tid) {
    INIT_TASK;

    struct sockaddr_in sin;

    /* Clear the sockaddr before using it, in case there are extra
     * platform-specific fields that can mess us up. */
    memset(&sin, 0, sizeof (sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(1234);

    /* Create a new listener */
    struct evconnlistener *listener = evconnlistener_new_bind(THIS_SCHED->base, NULL, NULL,
            LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1,
            (struct sockaddr *) &sin, sizeof (sin));

    if (!listener) {
        perror("Couldn't create listener\n");
        RETURN_TASK(1);
    }

    DEF_LISTEN_RETVAL(returnValue);
    int running = 1;

    while (running && !ISSTOPREQUESTTASK) {
        if (YIELD_EVACCEPT(listener, returnValue) > 0) { // task wakeup because something received
            if (returnValue.isError == 0) { // if there is no error
                LOG("connection accepted, create new task to manage connection");

                // create new task but do not automatically start the tasck
                tid_t const tid = new_task_sched(THIS_SCHED,
                        (taskFunc_t) task_socket,
                        TASK_CREATE_NOT_READY,
                        0 /* DEFAULT STACK SIZE */);

                // create and set data (socket) to send to the task
                evutil_socket_t * const pTmp = (evutil_socket_t *) malloc(sizeof (evutil_socket_t));
                *(pTmp) = returnValue.fd;

                task_t * const newReadtask = getTask(tid);
                // this memory will be free when the task will be destroyed
                newReadtask->data = (void *) pTmp;

                // now all the data is configured so we are ready to start the task
                schedule_task(newReadtask);
            } else { // there was en error in socket
                fprintf(stderr, "Got an error %d (%s) on the listener. "
                        "Shutting down.\n", returnValue.err, evutil_socket_error_to_string(returnValue.err));
                running = 0;
            }
        } else { // task wakeup for other reason
            LOG("operation canceled");
            running = 0;
        }
        RESET_LISTEN_RETVAL(returnValue);
    } // end while

    evconnlistener_free(listener);
    listener = NULL;

    LOG("end");

    RETURN_TASK(0);
}

/** task that connect to a listening socket
 *
 * @param tid  my thread id
 */
void task_connect(tid_t const tid) {
    INIT_TASK;

    DEF_SOCKET_RETVAL(retval);

    LOG("start");

    struct bufferevent *bev = bufferevent_socket_new(THIS_SCHED->base, -1, BEV_OPT_CLOSE_ON_FREE);

    if ((NULL != bev) && YIELD_EVSOCKET_CONNECT(bev, NULL, AF_UNSPEC, "127.0.0.1", 1234, retval) == 0) {
        LOG("connected");
        RESET_SOCKET_RETVAL(retval);
        YIELD_EVSOCKET_WRITE(bev, (void *) MESSAGE, sizeof (MESSAGE), retval);
        // wait answer
        RESET_SOCKET_RETVAL(retval);

        uint8_t readingBuffer[1024];
        int datalen = YIELD_EVSOCKET_READ(bev, retval, (void *) readingBuffer, sizeof (readingBuffer));

        if (datalen > 0) {
            LOG("Stress receive %d data:", datalen);
            BEGINLOG();
            for (int i = 0; i < datalen; i++) {
                if (isprint(readingBuffer[i])) {
                    SIMPLELOG("%c", (char) readingBuffer[i]);
                } else {
                    SIMPLELOG("\\x%02x", (int) readingBuffer[i]);
                }
            }
            ENDLOG();
        }
    } else {
        LOG("error connecting");
    }

    if (bev) {
        bufferevent_free(bev);
        bev = NULL;
    }

    LOG("end");

    RETURN_TASK(0);
}


////////////////////////////////////////////////////////////////////////////////

/** task that show how sleep works
 *
 * @param tid  my thread id
 */
void task_stress(tid_t const tid) {
    INIT_TASK;

    struct timeval const five_sec = {0, 500000 /* us */};

    int i = 0;
    int running = 1;
    while (running && !ISSTOPREQUESTTASK) {
        if (YIELD_EVSLEEP(five_sec)) { // task wakeup for timeout

            // create new task but do not automatically start the tasck

            LOG("stress connect %d", i);

            tid_t const tid = new_task_sched(THIS_SCHED,
                    (taskFunc_t) task_connect,
                    TASK_NONE,
                    0 /* DEFAULT STACK SIZE */);

            LOG("stress created connect %d %u", i++, tid);


        } else { // task wakeup for other reason
            LOG("operation canceled");
            running = 0;
        }
    } // end while

    LOG("end");
    RETURN_TASK(0);
}


////////////////////////////////////////////////////////////////////////////////


#include <sys/time.h>                // for gettimeofday()

/** task that show how sleep works
 *
 * @param tid  my thread id
 */
void task_counter(tid_t const tid) {
    INIT_TASK;

    struct timeval t1;
    struct timeval t2;
    double elapsedTime;

    struct timeval const five_sec = {5, 0};

    int i = 0;
    int running = 1;
    while (running && !ISSTOPREQUESTTASK) {
        gettimeofday(&t1, NULL);
        if (YIELD_EVSLEEP(five_sec)) { // task wakeup for timeout
            gettimeofday(&t2, NULL);

            // compute and print the elapsed time in millisec
            elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0; // sec to ms
            elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0; // us to ms

            LOG("counter %d (%lf ms)", i++, elapsedTime);
        } else { // task wakeup for other reason
            LOG("operation canceled");
            running = 0;
        }
    } // end while

    LOG("end");
    RETURN_TASK(0);
}

////////////////////////////////////////////////////////////////////////////////

/** task that manage SIGINT signal
 *
 * @param tid  my thread id
 */
void task_signal(tid_t const tid) {
    INIT_TASK;

    int running = 1;
    while (running && !ISSTOPREQUESTTASK) {
        if (YIELD_EVSIGNAL(SIGINT)) { // task wakeup for signal received
            LOG("signal received");
            // iter over all Scheduled task and mark it to be stopped
            iterSchedTask(stopRequest_task, THIS_SCHED);
            // iter over all Scheduled task and wakeup it
            iterSchedTask(schedule_task, THIS_SCHED);
        } else { // task wakeup for other reason
            LOG("operation canceled");
            running = 0;
        }
    } // end while

    LOG("end");
    RETURN_TASK(0);
}

////////////////////////////////////////////////////////////////////////////////

int recursion(int i) {
    char pippo[256] = "- ALLOCATE VARIABLE ON STACK TO TEST -";
    pippo[0] = '+'; // only use variable to avoid optimization;
    LOG("[%p] test recursion %d, '%s'", &i, i, pippo);
    if (i <= 0) {
        return i;
    } else {
        return i + recursion(i - 1);
    }
}

/** task that test stack usage
 *
 * @param tid  my thread id
 */
void task_stack_test(tid_t const tid) {
    INIT_TASK;

    int running = 1;
    int i = 1;
    while (running && !ISSTOPREQUESTTASK) {
        YIELD_EVSLEEP_MSEC(100);
        LOG("[%p] test start recursion %d", &running, i);
        int res = recursion(i);
        LOG("[%p] test end recursion %d = %d", &running, i, res);
        i += 1;
#ifndef  USING_SPLIT_STACK
        if (i > 10) { // stack increase too much
            i = 1;
        }
#endif
    } // end while

    LOG("end");
    RETURN_TASK(0);
}

/** MAIN
 *
 * @param argc
 * @param argv
 * @return
 */
int main(int UNUSED_PARAMETER(argc), char ** UNUSED_PARAMETER(argv)) {
    LOG("Create Scheduler");
    // create a new cooperative scheduler
    sched_t *sched = create_sched(SCHED_NONE || SCHED_END_WITHOUT_TASKS);
    assert(NULL != sched);

    LOG("Create Signal Task");
    new_task_sched(sched, (taskFunc_t) task_signal, TASK_NONE, 0 /* DEFAULT STACK SIZE */);

    LOG("Create Listening Task");
    new_task_sched(sched, (taskFunc_t) task_listen, TASK_NONE, 0 /* DEFAULT STACK SIZE */);

    LOG("Create Counter Task");
    new_task_sched(sched, (taskFunc_t) task_counter, TASK_NONE, 8192 /* STACK SIZE */);

    // LOG("Create Connect Task");
    // new_task_sched(sched, (taskFunc_t) task_connect, TASK_NONE, 0 /* DEFAULT STACK SIZE */);

    LOG("Create Stress Connection Task");
    new_task_sched(sched, (taskFunc_t) task_stress, TASK_NONE, 0 /* DEFAULT STACK SIZE */);

    LOG("Create stack stress Task");
    new_task_sched(sched, (taskFunc_t) task_stack_test, TASK_NONE, 16384 /* DEFAULT STACK SIZE */);

    LOG("Start Mainloop");
    // start scheduler
    mainloop_sched(sched);

    LOG("Destroy Scheduler");
    // scheduling is terminated so the scheduler can be detroyed
    destroy_sched(&sched);

    LOG("mainloop terminated");

    return 0;
}
