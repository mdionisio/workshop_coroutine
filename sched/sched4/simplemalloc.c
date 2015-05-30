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
 * File:   simplemalloc.c
 * Author: Michele Dionisio
 *
 * Created on 30 may 2015
 */


#include "simplemalloc.h"

//used for list and map management
#include <utlist.h>

//for libc malloc and free
#include <stdlib.h>

#include "config.h"

#ifdef DEBUG_SCHED_STACK_ALLOC
#include <stdio.h>
#define LOG printf
#else
#define LOG(...)
#endif

typedef struct elemnt_available_block elemnt_available_block_t; /**< element of list of available memory */

struct elemnt_available_block {
    elemnt_available_block_t *next;        /**< needed for singly- or doubly-linked lists */
};

static elemnt_available_block_t *head = NULL; /* list of available memory block */
static unsigned int list_size         = 0U;


#ifndef MALLOC_MEMORY_SIZE
#include <signal.h>
#include <bits/sigstack.h>                // this has to be included from signal.h but there is a bug in gcc header
#define MALLOC_MEMORY_SIZE SIGSTKSZ
#endif

size_t simple_sizeof(void *p) { return MALLOC_MEMORY_SIZE; }

inline static void * create_block() {
    void * p = malloc(MALLOC_MEMORY_SIZE + sizeof(elemnt_available_block_t));
    elemnt_available_block_t *el = (elemnt_available_block_t *) p;
    el->next = NULL;
    return p;
}

void * simple_malloc() {
  void *p = NULL;
  if (head == NULL) {
     LOG("memory allocation");
     p = create_block();
  } else {
   assert (list_size != 0);
   LOG("memory block available");
   
   elemnt_available_block_t *el = head;
   p = (void *) el;
   LL_DELETE(head, head);
   list_size --;   
   el->next = NULL;
  }
  
  p += + sizeof(elemnt_available_block_t);
  
  LOG(" %p\n", p);
  return p;
}

void simple_free(void **p) {
 if ((p != NULL) && (*p != NULL)) {
    LOG("memory block mark as free %p\n", *p);
    elemnt_available_block_t *el = (elemnt_available_block_t *) ((*p) - sizeof(elemnt_available_block_t));
    el->next = NULL;      // useless because is already NULL from malloc, but ...
    LL_PREPEND(head, el);
    list_size ++;
    p = NULL;
 }
}

void simple_compact(unsigned int n) {
   LOG("simple_compact start\n");
   for (int i=list_size; i<n; ++i) { // preallocate
      void *p = create_block();
      simple_free(&p);
   }
   for (int i=n; i<list_size; ++i) { // compact
      elemnt_available_block_t *el = head;
      LL_DELETE(head, head);
      free(el);
   }
   LOG("simple_compact end\n");
}