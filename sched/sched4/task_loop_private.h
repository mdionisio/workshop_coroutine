/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   task_loop_private.h
 * Author: miki
 *
 * Created on 10 gennaio 2016, 20.06
 */

#ifndef TASK_LOOP_PRIVATE_H
#define TASK_LOOP_PRIVATE_H

#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

    struct sched_mainloop_interface {
        void *(*create)(void);
        void (*destroy)(void *);

        void (*event_poll)(void *);
    };

#ifdef __cplusplus
}
#endif

#endif /* TASK_LOOP_PRIVATE_H */

