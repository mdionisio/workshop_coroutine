/*
 * File:   unused.h
 * Author: miki
 *
 * Created on 24 ottobre 2015, 17.14
 */

#ifndef UNUSED_H
#define UNUSED_H

#ifdef __cplusplus
extern "C" {
#endif

#define UNUSED(x) (void)(x)

#ifdef __GNUC__
#define UNUSED_PARAMETER(x) UNUSED_ ## x __attribute__((__unused__))
#else
#define UNUSED_PARAMETER(x) UNUSED_ ## x
#endif

#ifdef __GNUC__
#define UNUSED_FUNCTION(x) __attribute__((__unused__)) UNUSED_ ## x
#else
#define UNUSED_FUNCTION(x) UNUSED_ ## x
#endif


#ifdef __cplusplus
}
#endif

#endif /* UNUSED_H */

