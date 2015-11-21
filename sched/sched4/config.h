
// set define THREADSAFE_SCHED if you want to have threadshafe scheduler
// to have more than one scheduler working on more than one thread
#define THREADSAFE_SCHED 1

// define DEBUG_SCHED_TASK   to have log of task scheduling
#undef DEBUG_SCHED_TASK

// define DEBUG_SCHED_STACK_ALLOC   to have log of stacl allocation
#undef DEBUG_SCHED_STACK_ALLOC

// define stack size
// if not defined it use SIGSTKSZ	/* recommended stack size */
#undef MALLOC_MEMORY_SIZE
