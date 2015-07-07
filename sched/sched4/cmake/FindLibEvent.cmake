# - Try to find event
# Once done this will define
#  LIBEVENT_FOUND - System has libevent
#  LIBEVENT_INCLUDE_DIRS - The libevent include directories
#  LIBEVENT_LIBRARIES - The libraries needed to use libevent
#  LIBEVENT_DEFINITIONS - Compiler switches required for using libevent

find_package(PkgConfig)
pkg_check_modules(PC_EVENT QUIET libevent)
set(LIBEVENT_DEFINITIONS ${PC_EVENT_CFLAGS_OTHER})

find_path(LIBEVENT_INCLUDE_DIR event.h
          HINTS ${PC_LIBEVENT_INCLUDEDIR} ${PC_LIBEVENT_INCLUDE_DIRS}
          )

find_library(LIBEVENT_LIBRARY NAMES event event-2.0 libevent-2.0
             HINTS ${PC_LIBEVENT_LIBDIR} ${PC_LIBEVENT_LIBRARY_DIRS} )

set(LIBEVENT_LIBRARIES ${LIBEVENT_LIBRARY} )
set(LIBEVENT_INCLUDE_DIRS ${LIBEVENT_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBEVENT_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LibEvent  DEFAULT_MSG
                                  LIBEVENT_LIBRARY LIBEVENT_INCLUDE_DIR)

mark_as_advanced(LIBEVENT_INCLUDE_DIR LIBEVENT_LIBRARY )
