/* 
 * File:   main.c
 * Author: Michele Dionisio
 *
 * Created on 19 ottobre 2014
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

#include <ucontext.h>

//used for error management
#include <errno.h>
#include <string.h>

typedef enum { false=0, true=1 } bool;

void outdata(int data, bool eof);
bool indata(int *data);

/** produce a sequence of 10 number
 * 
 */
void producer() {
    printf("--> producer start .\n");
    int i;
    for (i=0; i<10; i++) {
        printf("--> produced %d\n", i);
        outdata(i, true);
    }
    outdata(0, false);
    // never reach this point
    printf("--> producer end .you don't see it\n");
}

/** Consume all the data available
 * 
 */
void consumer() {
    int data;
    printf("--> consumer start .\n");
    // while there is data
    while (indata(&data)) {
        printf("<-- consumed %d\n", data);
    }
    printf("--> consumer end .\n");
}

// NOW we start the magic code ...

ucontext_t contextProducer;
volatile bool contextProducerValid = false;

ucontext_t contextConsumer;
volatile bool contextConsumerValid = false;;

int tmpData;
bool tmpEof = true;

bool hereForCallProducer;
bool hereForCallContumer;
        
void outdata(int data, bool eof) {
    hereForCallProducer = true;
    tmpData = data;
    tmpEof = eof;
    printf("--> save producer context \n");
    if (getcontext(&contextProducer) == -1) {
        printf("getcontext error (%d): %s\n", errno, strerror(errno));
    }
    contextProducerValid = true;
    if (contextConsumerValid && hereForCallProducer) {
        hereForCallContumer = false;
        printf("--> goto consumer context \n");
        setcontext(&contextConsumer);
    }
}

bool indata(int *data) {
    hereForCallContumer = true;                  // set a flag to mark
                                                 // that we are here because we 
                                                 // have call the function
    // save context
    printf("--> save consumer context \n");
    if (getcontext(&contextConsumer) == -1) {
        printf("getcontext error (%d): %s\n", errno, strerror(errno));
    }
    contextConsumerValid = true;                 // the consumer context is valid
    
    // if it is the first time that we call indata
    // we have to start producer
    if (!contextProducerValid && hereForCallContumer) {
        printf("--> producer is not started, start \n");
        producer();  // start producer
        // never reach this point
        printf("--> you don't see it\n");
    }
    
    // if we are here because we have call the function we switch to producer 
    // so we ask to poll data
    if (contextProducerValid && hereForCallContumer) {
        hereForCallProducer = false;
        printf("--> goto saved producer context \n");
        setcontext(&contextProducer);
        printf("--> you don't see it\n");
        return (0);
    } else {
        // otherwise the data is pulled and we consume it
        printf("--> data pulled\n");
        *data = tmpData;
        return tmpEof;
    }
}

/*
 * 
 */
int main(int argc, char* argv[]) {
    
    consumer(); // start only consumer because it pull data from producer

    return 0;
}

