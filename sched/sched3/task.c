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
 * File:   task.c
 * Author: Michele Dionisio
 *
 * Created on 18 ottobre 2014
 */

#include "task.h"

#include <assert.h>

#include <stdio.h>
#include <stdlib.h>

//used for list and map management
#include <utlist.h>
#include <uthash.h>

// set define THREADSAFE_SCHED if you want to have threadshafe scheduler
// to have more than one scheduler working on more than one thread
//#define THREADSAFE_SCHED 1

#ifdef THREADSAFE_SCHED
#include <pthread.h>
#endif

#define CDL_POP_LAST(head, out)                                                \
  do {                                                                         \
    if (head) {                                                                \
      (out) = (head)->prev;                                                    \
      if ((out) == (head)) {                                                   \
        (head) = NULL;                                                         \
      } else {                                                                 \
        (head)->prev = (out)->prev;                                            \
        (head)->prev->next = (head);                                           \
      }                                                                        \
      (out)->prev = NULL;                                                      \
      (out)->next = NULL;                                                      \
    } else {                                                                   \
      (out) = NULL;                                                            \
    }                                                                          \
  } while (0)

//used for error management
#include <errno.h>
#include <string.h>

// *****************************************************************************
// *                        TASK/SCHEDULER DEFINITION                          *
// *****************************************************************************

static tid_t gTaskId = 0;          /**< global variable to give unique id to any task */
static tid_t gSchedId = 0;         /**< global variable to give unique id to any scheduler */

/** map type for from sid to sched
 */
typedef struct mapSched mapSched_t;

/** map scheduler
 */
struct mapSched {
    sid_t sid;                  /**< map key */
    sched_t * sched;            /**< map value */
    UT_hash_handle hh;          /**< makes this structure hashable */
};

typedef struct elemTaskList elemTaskList_t;

struct elemTaskList {
      task_t *task;
      elemTaskList_t *next;
};

/** map type for from tid to task
 */
typedef struct mapTask mapTask_t;

/** map data type for from tid to task
 */
struct mapTask {
    tid_t tid;                  /**< map key */
    task_t * task;               /**< map value */
    elemTaskList_t *listWaiting; /**< list of task waiting the task's end */
    UT_hash_handle hh;          /**< makes this structure hashable */
};

static mapTask_t * create_mapTask(task_t * const task) {
    mapTask_t * const newTask = (mapTask_t *) malloc(sizeof(mapTask_t));
    newTask->task = task;
    newTask->tid = task->tid;
    newTask->listWaiting = NULL;
    return newTask;
}

static void destroy_task(task_t ** const task);

static void destroy_mapTask(mapTask_t ** const delTask) {
    assert(NULL != delTask);
    if (NULL != *delTask) {
        elemTaskList_t *elIter = NULL;    
        elemTaskList_t *elIterNext = NULL;
        LL_FOREACH_SAFE((*delTask)->listWaiting, elIter, elIterNext) {
            schedule_task(elIter->task);
            LL_DELETE((*delTask)->listWaiting, elIter);
            free(elIter);
        }
        --((*delTask)->task->sched->numAvailableTask);  // decrease task counter
        destroy_task(&((*delTask)->task));              // destroy task
        free(*delTask);                                 // release memory
        *delTask = NULL;
    }
}

static mapSched_t *gMapSched = NULL;     /**< define map for all available scheduler */
static mapTask_t *gAvailableTask = NULL; /**< define map for all available task */
#ifdef THREADSAFE_SCHED
pthread_mutex_t gMutexAvailableTask = PTHREAD_MUTEX_INITIALIZER;
#define LOCK_AVAILABLE_TASK pthread_mutex_lock(&gMutexAvailableTask)
#define UNLOCK_AVAILABLE_TASK pthread_mutex_unlock(&gMutexAvailableTask)
#else
#define LOCK_AVAILABLE_TASK 
#define UNLOCK_AVAILABLE_TASK 
#endif


// *****************************************************************************
// *                          TASK IMPLEMENTATION                              *
// *****************************************************************************

/** Create a new task
 * 
 * @param ltaskFunc function pointer to the function to call
 * @param lsched scheduler that is managing this task
 * @return return a valid task
 */
static task_t * create_task(taskFunc_t ltaskFunc, 
                            sched_t * const lsched,
                            unsigned int config)
{
    assert(NULL != ltaskFunc);
    assert(NULL != lsched);
    task_t * const task = (task_t *) malloc(sizeof (task_t));
    LOCK_AVAILABLE_TASK;                   // safe access to gAvailableTask
    task->tid = ++gTaskId;       // set unique id
    UNLOCK_AVAILABLE_TASK;                   // safe access to gAvailableTask
    task->taskFunc = ltaskFunc;  // set function pointer to call
    task->status = WAIT;         // set task status
    task->returnValue = 0;       // set default return value
    task->sched = lsched;        // set scheduler where the task run
    task->data = NULL;           // in data for thread
    task->stop = 0;              // set request to stop to false
    task->config = config;       // set configuration

    // initialize takein it from what is running now
    if (getcontext(&(task->tempData)) == -1) {
        printf("getcontext error (%d): %s\n", errno, strerror(errno));
    }

    // set the context where to go at the end
    task->tempData.uc_link          = &(task->sched->tempData);
    // initialize where to save the stask
    task->tempData.uc_stack.ss_sp   = task->stack;
    task->tempData.uc_stack.ss_size = sizeof(task->stack);
    sigfillset(&(task->tempData.uc_sigmask));  // is used to store the set of signals blocked in the context

    makecontext(&(task->tempData), (void (*)(void)) ltaskFunc, 1, task->tid);

    return task;
}

/** ser return value and mark task as terminated
 * 
 * @param task
 * @param value
 */
void setReturnValue_task(task_t * const task, int value)
{
    assert(NULL != task);
    task->returnValue = value;
    task->status = TERMINATED;
}

/** run or wakeup a task
 * 
 * @param task
 */
static void run_task(task_t * const task)
{
    assert(NULL != task);
    printf("run_task tid=%u %d\n", task->tid, task->status);

    switch(task->status) {
    case WAIT:
        task->status = RUNNING;
        if (swapcontext(&(task->sched->tempData), &(task->tempData)) == -1) {
            printf("swapcontext error (%d): %s\n", errno, strerror(errno));
        }
        break;
    case RUNNING:
        printf("missing setting of return value\n");
        setReturnValue_task(task, 0);
        break;
    default:
        printf("run task in not valid state %d\n", task->status);
    }

}

/** destroy a task
 * 
 * @param task task to be destroyed
 */
static void destroy_task(task_t ** const task)
{
    assert(NULL != task);
    if (NULL != *task) {
        free(*task);
        *task = NULL;
    }
}

/** put a task in sleep and switch to the scheduler
 * 
 * @param task
 */
void yield_task(task_t * const task)
{
    assert(NULL != task);
    task->status = WAIT;
    if (swapcontext(&(task->tempData), &(task->sched->tempData)) == -1) {
        printf("yield swapcontext error (%d): %s\n", errno, strerror(errno));
    }
    // if you are here means that you are waking up
}

/** wait
 * 
 * @param task
 */
void wait_task(task_t * const task, tid_t tid)
{
    assert(NULL != task);
    assert(task->tid != tid);
    
    LOCK_AVAILABLE_TASK;                   // safe access to gAvailableTask
    
    mapTask_t *__mapTask__ = NULL;
    HASH_FIND_INT(gAvailableTask, &(tid), __mapTask__);
    assert(NULL != __mapTask__);

    elemTaskList_t * const newWaitCond = (elemTaskList_t *) malloc(sizeof (elemTaskList_t));
    newWaitCond->next = NULL;
    newWaitCond->task = task;
    LL_PREPEND(__mapTask__->listWaiting, newWaitCond);
    
    UNLOCK_AVAILABLE_TASK;                 // safe access to gAvailableTask
    
    suspend_task(task);
}

/** put a task in sleep and switch to the scheduler
 * 
 * @param task
 */
void suspend_task(task_t * const task)
{
    assert(NULL != task);
    task->status = SUSPENDED;
    if (swapcontext(&(task->tempData), &(task->sched->tempData)) == -1) {
        printf("yield swapcontext error (%d): %s\n", errno, strerror(errno));
    }
    // if you are here means that you are waking up
}

unsigned int isStopRequested_task(const task_t * const task) {
    assert(NULL != task);
    return task->stop;
}

void stopRequest_task(task_t * const task) {
    task->stop = 1;
}

///
/// SCHEDULER IMPLEMENTATION
///

/** Create a new scheduler
 *
 * @return return a valid scheduler
 */
sched_t * create_sched(unsigned int config)
{
    sched_t * const sched = (sched_t *) malloc(sizeof (sched_t));
    LOCK_AVAILABLE_TASK;                   // safe access to gAvailableTask
    sched->sid = ++gSchedId;                            // set scheduler id
    sched->numAvailableTask = 0;                        // set no available task
    sched->readyTask = NULL;                            // set empty list
    sched->config = config;                             // configure scheduler
    sched->stop = 0;                                    // set stop to don't stop
    
    sched->base = event_base_new();
    
    mapSched_t * const newSched = (mapSched_t *) malloc(sizeof (mapSched_t));
    newSched->sid = sched->sid;
    newSched->sched = sched;
    HASH_ADD_INT(gMapSched, sched->sid, newSched);
    
    UNLOCK_AVAILABLE_TASK;                   // safe access to gAvailableTask

    return sched;
}

unsigned int isStopRequested_sched(const sched_t * const sched) {
    return sched->stop;
}

void stopRequest_sched(sched_t * const sched) {
    sched->stop = 1;
}

/** destroy a scheduler
 *
 * @param sched scheduler to be destroyed
 */
void destroy_sched(sched_t ** const sched)
{
    assert(NULL != sched);
    if (NULL != *sched) {
        LOCK_AVAILABLE_TASK;                   // safe access to gAvailableTask
        mapSched_t * delSched = NULL;
        HASH_FIND_INT(gMapSched, &((*sched)->sid), delSched);
        HASH_DEL(gMapSched, delSched);
        assert( (*sched) == (delSched->sched) );
        free(delSched);
        UNLOCK_AVAILABLE_TASK;                   // safe access to gAvailableTask
        event_base_free((*sched)->base);
        free(*sched);
        *sched = NULL;
    }
}

/** put a task in the list of ready to run
 * 
 * @param task task ready
 */
void schedule_task(task_t * const task)
{
    assert(NULL != task);
    
    // already running
    if (task->status == RUNNING) {
        return;
    }
    
    queueTask_t * elIter = NULL;    
    CDL_FOREACH(task->sched->readyTask, elIter) {
        if (elIter->task == task) {
            return ;                     // task already in the ready list
        }
    }
    
    task->status = WAIT;
    // create new element for ready list
    queueTask_t * const newElem = (queueTask_t *) malloc(sizeof (queueTask_t));
    newElem->task = task;
    // add task to the ready list
    CDL_PREPEND(task->sched->readyTask, newElem);
}

/** Create a new schedulable task
 * 
 * @param sched scheduler
 * @param ltaskFunc function fointer to the task function
 * @param config task config
 */
tid_t new_task_sched(sched_t * const sched, 
                     taskFunc_t ltaskFunc, 
                     unsigned int config)
{
    assert(NULL != sched);
    assert(NULL != ltaskFunc);
    // create new element for available map
    mapTask_t * const newTask = create_mapTask(create_task(ltaskFunc, sched, config));
    //
    printf("-> newtask ttid=%u\n", newTask->tid);
    
    LOCK_AVAILABLE_TASK;                   // safe access to gAvailableTask
    
    // add new element to the map
    HASH_ADD_INT(gAvailableTask, tid, newTask);
    ++(sched->numAvailableTask);
    
    UNLOCK_AVAILABLE_TASK;                   // safe access to gAvailableTask
    
    // schede created task
    schedule_task(newTask->task);
    
    return newTask->tid;
}

/** scheduler mainloop
 * 
 * @param sched
 */
void mainloop_sched(sched_t * const sched)
{
    assert(NULL != sched);
    queueTask_t * task = NULL;
    
    mapTask_t *delTask = NULL;          // tmp variable to delete terminated task

    // loop while there is available task
    
    while ( (!isStopRequested_sched(sched)) &&                          // is not stop requested
            ! ( ((sched->config && SCHED_END_WITHOUT_TASKS) != 0) &&    // is configured to end if there is no task
                (sched->numAvailableTask == 0) ) ) {                     // if there is some task running
        if (!task) {
            // get a task that is waiting to run
            CDL_POP_LAST(sched->readyTask, task);
        }

        if (NULL == task) { // there is no task available to be scheduled
            printf("no task ready to be scheduled\n");
            // libevent task dispaching        
	    event_base_loop(sched->base, EVLOOP_ONCE /* EVLOOP_NONBLOCK */);
        } else { // there is a task that can be scheduled
            printf("scheduler work on task ttid=%u status=%d\n",
                   task->task->tid, task->task->status);
            int wakeup = 0;
            // loop to check status before and after a task execution
            do {
                switch (task->task->status) {
                case TERMINATED :
                    // task is terminated so has to be removed
                    // from the list of available task
                    printf("task terminated with return: %d "
                           "so remove it from availale task\n",
                           task->task->returnValue);
                    delTask = NULL;

                    LOCK_AVAILABLE_TASK;                   // safe access to gAvailableTask
                    HASH_FIND_INT(gAvailableTask, &(task->task->tid), delTask);
                    HASH_DEL(gAvailableTask, delTask);
                    UNLOCK_AVAILABLE_TASK;                   // safe access to gAvailableTask

                    destroy_mapTask(&delTask);
                    free(task);
                    wakeup = 0;     // stop check status
                    break;
                case WAIT :
                    if (!wakeup) {
                        run_task(task->task);
                        wakeup = 1; // check status again
                    } else {
                        CDL_PREPEND(sched->readyTask, task);
                        wakeup = 0; // stop check status
                    }
                    break;
                case RUNNING:
                    printf("task ttid=%u terminate "
                           "without call setReturnValue_task\n", task->task->tid);
                    setReturnValue_task(task->task, 0);
                    CDL_PREPEND(sched->readyTask, task);
                    wakeup = 0;    // stop check status
                    break;
                case SUSPENDED :
                    free(task);
                    wakeup = 0;   // stop check status
                    break;
                }
            } while (wakeup);

            task = NULL;
        }
    }
}

task_t * getTask(tid_t tid) {
  mapTask_t *__mapTask__ = NULL;
  LOCK_AVAILABLE_TASK;                   // safe access to gAvailableTask
  HASH_FIND_INT(gAvailableTask, &(tid), __mapTask__);
  UNLOCK_AVAILABLE_TASK;                   // safe access to gAvailableTask
  if (NULL != __mapTask__) {
    return __mapTask__->task;
  } else {
    return NULL;
  }
}

/** iterate over all the task available
 * 
 * @param f function to execute over each thread
 */
void iterAllTask(iterOnTask f) {
    static mapTask_t *current_task, *tmp;
    LOCK_AVAILABLE_TASK;                   // safe access to gAvailableTask
    HASH_ITER(hh, gAvailableTask, current_task, tmp) {
        (*f)(current_task->task);
    }
    UNLOCK_AVAILABLE_TASK;                   // safe access to gAvailableTask
}

/** iterate over all the task available for the selected scheduler
 * 
 * @param f function to execute over each thread
 * @param sched scheduler
 */
void iterSchedTask(iterOnTask f, const sched_t * const sched) {
    static mapTask_t *current_task, *tmp;
    LOCK_AVAILABLE_TASK;                   // safe access to gAvailableTask
    HASH_ITER(hh, gAvailableTask, current_task, tmp) {
        if (current_task->task->sched == sched) {
            (*f)(current_task->task);
        }
    }
    UNLOCK_AVAILABLE_TASK;                   // safe access to gAvailableTask
}
