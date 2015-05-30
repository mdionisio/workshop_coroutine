/* 
 * File:   main.c
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

//used for list and map management
#include <uthash.h>
#include <utlist.h>

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
      (out)->prev = (out);                                                     \
      (out)->next = (out);                                                     \
    } else {                                                                   \
      (out) = NULL;                                                            \
    }                                                                          \
  } while (0)

//used for error management
#include <errno.h>
#include <string.h>

//used to have functions for context management
#include <ucontext.h>


// *****************************************************************************
// *                        TASK/SCHEDULER DEFINITION                          *
// *****************************************************************************

typedef unsigned int tid_t; /**< type for unique task identifier */

tid_t gTaskId = 0;          /**< global variable to give uniqui id to any task 
                             */

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
    WAIT          /**< task is waiting to be scheduled */
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

/** map type for from tid to task
 */
typedef struct mapTask {
    tid_t tid;                /**< map key */
    task_t *task;             /**< map value */
    UT_hash_handle hh;        /**< makes this structure hashable */
} mapTask_t;

typedef struct queueTask queueTask_t; /**< task queue type */

/** element of task list used as task queue
 */
struct queueTask {
    task_t *task;             /**< element */
    queueTask_t *prev;        /**< needed for a doubly-linked list only */
    queueTask_t *next;        /**< needed for singly- or doubly-linked lists */
};

mapTask_t *gAvailableTask = NULL; /**< define map for all available task */

/** scheduler type
 */
struct sched {
    unsigned int numAvailableTask;/**< number of task managed */
    queueTask_t *readyTask;       /**< list of ready to run task 
                                   * (uesed as queue) */
    ucontext_t tempData;          /**< structure to save task data when 
                                   * interrupted */
};

// *****************************************************************************
// *                          TASK IMPLEMENTATION                              *
// *****************************************************************************

/** Create a new task
 * 
 * @param ltaskFunc function pointer to the function to call
 * @param lsched scheduler that is managing this task
 * @return return a valid task
 */
task_t * create_task(taskFunc_t ltaskFunc, sched_t * const lsched)
{
    task_t * const task = (task_t *) malloc(sizeof (task_t));
    task->tid = ++gTaskId;       // set unique id
    task->taskFunc = ltaskFunc;  // set function pointer to call
    task->status = WAIT;         // set task status
    task->returnValue = 0;       // set default return value
    task->sched = lsched;        // set scheduler where the task run

    // initialize takein it from what is running now
    if (getcontext(&(task->tempData)) == -1) {
        printf("getcontext error (%d): %s\n", errno, strerror(errno));
    }

    // set the context where to go at the end
    task->tempData.uc_link          = &(task->sched->tempData);
    // initialize where to save the stask
    task->tempData.uc_stack.ss_sp   = task->stack;
    task->tempData.uc_stack.ss_size = sizeof(task->stack);

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
    task->returnValue = value;
    task->status = TERMINATED;
}

/** run or wakeup a task
 * 
 * @param task
 */
void run_task(task_t * const task)
{
    printf("run_task tid=%u %d\n", task->tid, task->status);

    switch(task->status) {
    case WAIT:
        task->status = RUNNING;
        if (swapcontext(&(task->sched->tempData), &(task->tempData)) == -1) {
            printf("swapcontext error (%d): %s\n", errno, strerror(errno));
        }
        break;
    case RUNNING:
        // if you reach this point means that the task is terminated without call
        // setReturnValue_task. This is teorically a programming problem but it 
        // can be easy managed and so ...
        printf("missing setting of return value\n");
        setReturnValue_task(task, 0);
        break;
    case TERMINATED:
        printf("run task in not valid state %d\n", task->status);
    }

}

/** destroy a task
 * 
 * @param task task to be destroyed
 */
void destroy_task(task_t ** const task)
{
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
    task->status = WAIT;
    if (swapcontext(&(task->tempData), &(task->sched->tempData)) == -1) {
        printf("yield swapcontext error (%d): %s\n", errno, strerror(errno));
    }
    // if you are here means that you are waking up
}


///
/// SCHEDULER IMPLEMENTATION
///

/** Create a new scheduler
 *
 * @return return a valid scheduler
 */
sched_t * create_sched()
{
    sched_t * const sched = (sched_t *) malloc(sizeof (sched_t));
    sched->numAvailableTask = 0;                        // set no available task
    sched->readyTask = NULL;                            // set empty list

    return sched;
}

/** destroy a scheduler
 *
 * @param sched scheduler to be destroyed
 */
void destroy_sched(sched_t ** const sched)
{
    if (NULL != *sched) {
        free(*sched);
        *sched = NULL;
    }
}

/** put a task in the list of ready to run
 * 
 * @param sched scheduler
 * @param task task ready
 */
void schedule_sched(sched_t * const sched, task_t * const task)
{
    // create new element for ready list
    queueTask_t * const newElem = (queueTask_t *) malloc(sizeof (queueTask_t));
    newElem->task = task;
    // add task to the ready list
    CDL_PREPEND(sched->readyTask, newElem);
}

/** Create a new schedulable task
 * 
 * @param sched scheduler
 * @param ltaskFunc function fointer to the task function
 */
void new_task_sched(sched_t * const sched, taskFunc_t ltaskFunc)
{
    // create new element for available map
    mapTask_t * const newTask = (mapTask_t *) malloc(sizeof(mapTask_t));
    newTask->task = create_task(ltaskFunc, sched);
    newTask->tid = newTask->task->tid;
    //
    printf("-> newtask ttid=%u\n", newTask->tid);
    // add new element to the map
    HASH_ADD_INT(gAvailableTask, tid, newTask);
    ++(sched->numAvailableTask);
    // schede created task
    schedule_sched(sched, newTask->task);
}

/** scheduler mainloop
 * 
 * @param sched
 */
void mainloop_sched(sched_t * const sched)
{
    queueTask_t * task = NULL;

    // loop while there is available task
    while (sched->numAvailableTask > 0) {
        if (!task) {
            // get a task that is waiting to run
            CDL_POP_LAST(sched->readyTask, task);
        }

        if (NULL == task) { // there is no task available to be scheduled
            printf("no task ready to be scheduled\n");
        } else { // there is a task that can be scheduled
            printf("scheduler work on task ttid=%u status=%d\n",
                   task->task->tid, task->task->status);
            switch (task->task->status) {
            case RUNNING:
                printf("task ttid=%u terminate "
                       "without call setReturnValue_task\n", task->task->tid);
                setReturnValue_task(task->task, 0);
            case TERMINATED :
                // task is terminated so has to be removed
                // from the list of available task
                printf("task terminated with return: %d "
                       "so remove it from availale task\n",
                       task->task->returnValue);
                mapTask_t *delTask = NULL;
                HASH_FIND_INT(gAvailableTask, &(task->task->tid), delTask);
                assert(delTask != NULL);
                HASH_DEL(gAvailableTask, delTask);
                free(delTask);
                destroy_task(&(task->task));
                free(task);
                --(sched->numAvailableTask);
                break;
            case WAIT :
                run_task(task->task);
                CDL_PREPEND(sched->readyTask, task);
                break;
            }

            task = NULL; // it is important to force next loop to choose another task
        }
    }
}

#define INIT_TASK mapTask_t *__mapTask__ = NULL;                               \
                  HASH_FIND_INT(gAvailableTask, &(tid), __mapTask__);          \
                  assert(__mapTask__ != NULL);                                 \
                  task_t * const THIS_TASK = __mapTask__->task
 
#define RETURN_TASK(x) setReturnValue_task(THIS_TASK, 1); return

#define YIELD yield_task(THIS_TASK)
#define THIS_SCHED THIS_TASK->sched

// ****************************************************************************
// ****************************************************************************
// ****************************************************************************

/// Start Example

void t0(tid_t const tid);
void t1(tid_t const tid);
void t2(tid_t const tid);
void t3(tid_t const tid);
void t4(tid_t const tid);

/** example of task
 *
 * @param tid  my thread id
 */
void t0(tid_t const tid)
{
    // take task from tid
    mapTask_t *mapTask = NULL;
    HASH_FIND_INT(gAvailableTask, &(tid), mapTask);
    assert(mapTask != NULL);
    task_t * const task = mapTask->task;

    // real task activity

    printf("Hello t0 1! %u\n", tid);
    yield_task(task);

    printf("Hello t0 2! %u\n", tid);
    yield_task(task);

    printf("Hello t0 3! %u\n", tid);
    yield_task(task);

    // set return value
    setReturnValue_task(task, 0);
}

/** example of task using macro
 *
 * @param tid  my thread id
 */
void t1(tid_t const tid)
{
    INIT_TASK;

    printf("Hello t1 1! %u\n", tid);
    YIELD;

    printf("Hello t1 2! %u\n", tid);
    YIELD;

    printf("Hello t1 3! %u\n", tid);

    RETURN_TASK(1);

    printf("### YOU NEVER SEE THIS LINE\n");

}

/** example of task that create another task
 *
 * @param tid  my thread id
 */
void t2(tid_t const tid)
{
    INIT_TASK;

    printf("Hello t2 1! %u\n", tid);
    YIELD;

    printf("Hello t2 2! %u\n", tid);
    YIELD;

    printf("Hello t2 3! %u\n", tid);
    new_task_sched(THIS_SCHED, (taskFunc_t) t4);
    printf("Hello t2 4! %u\n", tid);
    YIELD;

    RETURN_TASK(2);
}

/** example of task not well done. Missing return
 *
 * @param tid  my thread id
 */
void t3(tid_t const tid)
{
    INIT_TASK;

    printf("Hello t3 1! %u\n", tid);
    // forget to set result
}

/** example of task that clearly use the stack for (i)
 *
 * @param tid  my thread id
 */
void t4(tid_t const tid)
{
    INIT_TASK;

    for (int i=0; i<10; ++i) {
        printf("Hello t4 %d! %u\n", i, tid);
        YIELD;
    }

    RETURN_TASK(4);
}

/** test function called from t5
 * 
 * @param THIS_TASK
 */
void func1(task_t * const THIS_TASK) {
    printf("function func1 start\n");
    YIELD;
    printf("function func1 end\n");
}

/** example of task that clearly use the stack for function call
 *
 * @param tid  my thread id
 */
void t5(tid_t const tid)
{
    INIT_TASK;

    printf("Hello t5 1! %u\n", tid);
    func1(THIS_TASK);
    printf("Hello t5 2! %u\n", tid);

    RETURN_TASK(4);
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
    new_task_sched(sched, (taskFunc_t) t0);
    new_task_sched(sched, (taskFunc_t) t1);
    new_task_sched(sched, (taskFunc_t) t2);
    new_task_sched(sched, (taskFunc_t) t3);
    // t4 is created dinamically
    new_task_sched(sched, (taskFunc_t) t5);

    // start scheduler
    mainloop_sched(sched);

    // scheduling is terminated so the scheduler can be detroyed
    destroy_sched(&sched);

    return 0;
}

