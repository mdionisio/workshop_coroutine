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
static const char MESSAGE[] = "Hello, World!\n";
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

    while (running) {
        datalen = YIELD_EVSOCKET_READ(bev, retval, (void *)readingBuffer, sizeof(readingBuffer)-1);
        
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
                    
                    YIELD_EVSOCKET_WRITE(bev, (void *)MESSAGE, strlen(MESSAGE), retval);
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
                perror("operation canceled\n");
                running = 0;
                break;
        }
        RESET_SOCKET_RETVAL(retval);
    } // end while
    
    
    bufferevent_free(bev);

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
    
    while (running) {
      if (YIELD_EVACCEPT(listener, returnValue) > 0) {
        if (returnValue.isError == 0) {
          printf("connection accepted\n");
          tid_t const tid = new_task_sched(THIS_SCHED, (taskFunc_t) task_socket);
          
          evutil_socket_t * const pTmp = (evutil_socket_t *)malloc(sizeof(evutil_socket_t));
          *(pTmp) = returnValue.fd;
          
          task_t * const newReadtask = getTask(tid);
          newReadtask->data = (void *)pTmp;
        } else {
          fprintf(stderr, "Got an error %d (%s) on the listener. "
                  "Shutting down.\n", returnValue.err, evutil_socket_error_to_string(returnValue.err));
          running = 1;
        }
      } else {
            perror("operation canceled\n");
            running = 1;
      }
      RESET_LISTEN_RETVAL(returnValue);
    } // end while
    
    evconnlistener_free(listener);
    listener = NULL;

    RETURN_TASK(0);
}


////////////////////////////////////////////////////////////////////////////////
	


/** task that accept new socket
 *
 * @param tid  my thread id
 */
void task_counter(tid_t const tid) {
    INIT_TASK;
    
    struct timeval const five_sec = { 5, 0 };
    
    int i = 0;
    int running = 1;
    while (running) {        
        if (YIELD_EVSLEEP(five_sec)) {
            printf("counter %d\n", i++);
        } else {
            perror("operation canceled\n");
            running = 1;
        }
    }
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
    // create a new cooperative scheduler
    sched_t *sched = create_sched();

    // add to the scheduler same task
    new_task_sched(sched, (taskFunc_t) task_listen);
    new_task_sched(sched, (taskFunc_t) task_counter);

    // start scheduler
    mainloop_sched(sched);

    // scheduling is terminated so the scheduler can be detroyed
    destroy_sched(&sched);

    return 0;
}

