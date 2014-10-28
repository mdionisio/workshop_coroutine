/* 
 * File:   main.c
 * Author: Michele Dionisio
 *
 * Created on 18 ottobre 2014
 */

#include "task.h"

#include <stdio.h>


/// Start Example

void t1(tid_t const tid);
void t2(tid_t const tid);
void t3(tid_t const tid);

/** example of task using macro
 *
 * @param tid  my thread id
 */
void t1(tid_t const tid)
{
    INIT_TASK;

    printf("Hello t1 1! %u\n", tid);
    SUSPEND;

    printf("Hello t1 2! %u\n", tid);
    YIELD;

    printf("Hello t1 3! %u\n", tid);

    RETURN_TASK(1);

    printf("### YOU NEVER SEE THIS LINE\n");
}



/** example of task that clearly use the stack for (i)
 *
 * @param tid  my thread id
 */
void t2(tid_t const tid)
{
    INIT_TASK;

    for (int i=0; i<10; ++i) {
        printf("Hello t2 %d! %u\n", i, tid);
        YIELD;
    }
    printf("Hello t2 RESUME 1! %u\n", tid);
    RESUME(1);

    RETURN_TASK(2);
}

/** example of task that clearly use the stack for (i)
 *
 * @param tid  my thread id
 */
void t3(tid_t const tid)
{
    INIT_TASK;
    
    printf("Hello t3 1! %u\n", tid);
    
    WAITTASK(2);
    
    printf("Hello t3 2! %u\n", tid);

    RETURN_TASK(3);
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
    new_task_sched(sched, (taskFunc_t) t1);
    new_task_sched(sched, (taskFunc_t) t2);
    new_task_sched(sched, (taskFunc_t) t3);

    // start scheduler
    mainloop_sched(sched);

    // scheduling is terminated so the scheduler can be detroyed
    destroy_sched(&sched);

    return 0;
}

