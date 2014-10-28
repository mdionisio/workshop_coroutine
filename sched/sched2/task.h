/* 
 * File:   task.h
 * Author: Michele Dionisio
 *
 * Created on 18 ottobre 2014
 */

// if you build with -std=c99 you need the following header
// otherwise ucontext.h will be different
// if you uild with -std=gnu99 everything is ok
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#else
#if _POSIX_C_SOURCE < 200809L
#error require posix compatibility up to 200809L
#endif
#endif

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#else
#if _XOPEN_SOURCE < 700
#error require _XOPEN_SOURCE compatibility up to 700
#endif
#endif

#include <stdio.h>
#include <stdlib.h>

//used to have functions for context management
#include <ucontext.h>

// *****************************************************************************
// *                        TASK/SCHEDULER DEFINITION                          *
// *****************************************************************************

typedef unsigned int tid_t; /**< type for unique task identifier */

typedef struct task task_t;   /**< task type */
typedef struct sched sched_t; /**< scheduler type */

typedef void (*taskFunc_t)(task_t *task); /**< prototype for any task functions 
                                           */

/** task status type
 */
typedef enum taskStatus {
    RUNNING=0,    /**< task is running */
    TERMINATED,   /**< task terminated and waiting to be removed from available 
                   * tasks
                   */
    WAIT,          /**< task is waiting to be scheduled */
    SUSPENDED      /**< task suspended from the scheduler */
} taskStatus_t;

/** task data type
 */
struct task {
    tid_t tid;                 /**< unique thread identifier */
    taskStatus_t status;       /**< task status */
    int returnValue;           /**< return value valid if status == TERMINATED 
                                */
    taskFunc_t taskFunc;       /**< pointer to the task function */
    sched_t *sched;            /**< scheduler that manage this task */
    ucontext_t tempData;       /**< structure to save task data when interrupted 
                                */
    char stack[SIGSTKSZ];      /**< task stack */
};

typedef struct queueTask queueTask_t; /**< task queue type */

/** element of task list used as task queue
 */
struct queueTask {
    task_t *task;             /**< element */
    queueTask_t *prev;        /**< needed for a doubly-linked list only */
    queueTask_t *next;        /**< needed for singly- or doubly-linked lists */
};

/** scheduler type
 */
struct sched {
    unsigned int numAvailableTask;/**< number of task managed */
    queueTask_t *readyTask;       /**< list of ready to run task 
                                   * (uesed as queue) */
    ucontext_t tempData;          /**< structure to save task data when 
                                   * interrupted */
};

// task
void setReturnValue_task(task_t * const task, int value);
void yield_task(task_t * const task);
void wait_task(task_t * const task, tid_t tid);
void suspend_task(task_t * const task);
void schedule_task(task_t * const task);

// schedule
sched_t * create_sched();
void destroy_sched(sched_t ** const sched);
void new_task_sched(sched_t * const sched, taskFunc_t ltaskFunc);
void mainloop_sched(sched_t * const sched);

// utiles
task_t * getTask(tid_t tid);


#define INIT_TASK task_t * const THIS_TASK = getTask(tid)
 
#define RETURN_TASK(x) setReturnValue_task(THIS_TASK, 1); return

#define YIELD yield_task(THIS_TASK)
#define WAITTASK(x) wait_task(THIS_TASK, x)
#define SUSPEND suspend_task(THIS_TASK)
#define RESUME(tid) schedule_task(getTask(tid))
#define THIS_SCHED THIS_TASK->sched
