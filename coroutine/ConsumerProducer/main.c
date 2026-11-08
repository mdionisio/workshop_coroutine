/* 
 * File:   main.c
 * Author: Michele Dionisio
 *
 * Created on 19 ottobre 2014
 */

// if you build with -std=c99 you need the following header
// otherwise ucontext.h will be different
// if you build with -std=gnu99 everything is ok
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

#include <ucontext.h>

//used for error management
#include <errno.h>
#include <string.h>

#include <stdbool.h>

void outdata(int data, bool more);
bool indata(int *data);

/** produce a sequence of 10 numbers
 * 
 */
void producer(void) {
    printf("--> producer start .\n");
    int i;
    for (i=0; i<10; i++) {
        printf("--> produced %d\n", i);
        outdata(i, true);
    }
    outdata(0, false);
    // never reach this point: the consumer stops pulling after the last
    // outdata, so the producer stays suspended forever
    printf("--> producer end .you don't see it\n");
}

/** Consume all the data available
 * 
 */
void consumer(void) {
    int data;
    printf("--> consumer start .\n");
    // while there is data
    while (indata(&data)) {
        printf("<-- consumed %d\n", data);
    }
    printf("--> consumer end .\n");
}

// NOW we start the magic code ...
//
// The producer runs on its own private stack (created with makecontext).
// If producer and consumer shared the same stack, every call made by the
// consumer after a switch would overwrite the suspended producer frames
// (return address, locals, ...) and resuming the producer would crash.

#define PRODUCER_STACK_SIZE (64 * 1024)

static ucontext_t contextProducer;
static ucontext_t contextConsumer;
static char producerStack[PRODUCER_STACK_SIZE];
static bool producerStarted = false;

static int tmpData;
static bool tmpMore = false;   // true if tmpData contains a valid value

void outdata(int data, bool more) {
    tmpData = data;
    tmpMore = more;
    // save producer context and go back to the consumer
    printf("--> goto consumer context \n");
    if (swapcontext(&contextProducer, &contextConsumer) == -1) {
        printf("swapcontext error (%d): %s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

bool indata(int *data) {
    // if it is the first time that we call indata
    // we have to create the producer context with its own stack
    if (!producerStarted) {
        printf("--> producer is not started, create it \n");
        if (getcontext(&contextProducer) == -1) {
            printf("getcontext error (%d): %s\n", errno, strerror(errno));
            exit(EXIT_FAILURE);
        }
        contextProducer.uc_stack.ss_sp = producerStack;
        contextProducer.uc_stack.ss_size = sizeof(producerStack);
        contextProducer.uc_link = &contextConsumer; // if producer returns, come back here
        makecontext(&contextProducer, producer, 0);
        producerStarted = true;
    }

    // if producer ends without calling outdata, there is no more data
    tmpMore = false;

    // save consumer context and ask the producer to produce data
    printf("--> goto producer context \n");
    if (swapcontext(&contextConsumer, &contextProducer) == -1) {
        printf("swapcontext error (%d): %s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // here the data is pulled and we consume it
    printf("--> data pulled\n");
    *data = tmpData;
    return tmpMore;
}

/*
 * 
 */
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    consumer(); // start only consumer because it pulls data from producer

    return 0;
}
