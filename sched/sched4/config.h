
// set define THREADSAFE_SCHED if you want to have threadshafe scheduler
// to have more than one scheduler working on more than one thread
#define THREADSAFE_SCHED 1
// #undef THREADSAFE_SCHED

// #define DEBUG_SCHED_TASK   to have log of task scheduling
#undef DEBUG_SCHED_TASK

// define DEBUG_SCHED_STACK_ALLOC   to have log of stack allocation
#undef DEBUG_SCHED_STACK_ALLOC

// define DEBUG_EV_TASK to have log of libevent
#undef DEBUG_EV_TASK

// define stack size
// if not defined it use SIGSTKSZ	/* recommended stack size */
#undef MALLOC_MEMORY_SIZE
