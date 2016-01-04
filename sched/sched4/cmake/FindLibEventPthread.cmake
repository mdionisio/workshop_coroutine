# - Try to find event
# Once done this will define
#  LIBEVENTPTHREAD_FOUND - System has libevent
#  LIBEVENTPTHREAD_INCLUDE_DIRS - The libevent include directories
#  LIBEVENTPTHREAD_LIBRARIES - The libraries needed to use libevent
#  LIBEVENTPTHREAD_DEFINITIONS - Compiler switches required for using libevent

find_package(PkgConfig)
pkg_check_modules(PC_EVENT QUIET libevent_pthreads)
set(LIBEVENTPTHREAD_DEFINITIONS ${PC_EVENT_CFLAGS_OTHER})

find_path(LIBEVENTPTHREAD_INCLUDE_DIR event.h
          HINTS ${PC_LIBEVENTPTHREAD_INCLUDEDIR} ${PC_LIBEVENTPTHREAD_INCLUDE_DIRS}
          )

find_library(LIBEVENTPTHREAD_LIBRARY NAMES event_pthreads event_pthreads-2.0 libevent_pthreads libevent_pthreads-2.0
             HINTS ${PC_LIBEVENTPTHREAD_LIBDIR} ${PC_LIBEVENTPTHREAD_LIBRARY_DIRS} )

set(LIBEVENTPTHREAD_LIBRARIES ${LIBEVENTPTHREAD_LIBRARY} )
set(LIBEVENTPTHREAD_INCLUDE_DIRS ${LIBEVENTPTHREAD_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBEVENTPTHREAD_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LibEventPthread  DEFAULT_MSG
                                  LIBEVENTPTHREAD_LIBRARY LIBEVENTPTHREAD_INCLUDE_DIR)

mark_as_advanced(LIBEVENTPTHREAD_INCLUDE_DIR LIBEVENTPTHREAD_LIBRARY )