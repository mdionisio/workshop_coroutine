/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   utlist_ex.h
 * Author: miki
 *
 * Created on 2 gennaio 2016, 13.59
 */

#ifndef UTLIST_EX_H
#define UTLIST_EX_H

#include <utlist.h>

#define CDL_POP_LAST(head, out)                                                \
  do {                                                                         \
    if ((head) != NULL) {                                                      \
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

#endif /* UTLIST_EX_H */

