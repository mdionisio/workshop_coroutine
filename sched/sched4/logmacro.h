/* 
 * File:   logmacro.h
 * Author: miki
 *
 * Created on 23 agosto 2015, 8.29
 */

#ifndef __LOGMACRO_H__
#define	__LOGMACRO_H__

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef __FILENAME__
#define __FILENAME__ __FILE__
#endif
    
#ifdef FAKELOG

#define BEGINLOG() 
#define SIMPLELOG(format, ...) 
#define ENDLOG() 
    
#else
    
#include <stdio.h>

#define BEGINLOG() printf("%s:%d (%s) ", __FILENAME__, __LINE__, __FUNCTION__)
#define SIMPLELOG(format, ...) printf(format, ##__VA_ARGS__ )
#define ENDLOG() printf("\n")

#endif
    
#define LOG(format, ...) BEGINLOG(); SIMPLELOG(format, ##__VA_ARGS__); ENDLOG();

#ifdef	__cplusplus
}
#endif

#endif	/* __LOGMACRO_H__ */

