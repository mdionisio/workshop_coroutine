project(sched4)
cmake_minimum_required(VERSION 2.8)

# Locate required packages
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

aux_source_directory(. SRC_LIST)

find_package(LibEvent REQUIRED)
find_package(LibEventPthread REQUIRED)

set(LIBS ${LIBS} ${LIBEVENT_LIBRARIES} ${LIBEVENTPTHREAD_LIBRARIES})

include_directories(../../uthash/src/ ${LIBEVENT_INCLUDE_DIR} ${LIBEVENTPTHREAD_INCLUDE_DIR})
add_executable(${PROJECT_NAME} ${SRC_LIST})

target_link_libraries(${PROJECT_NAME} ${LIBS})

option(SplitTask "Enable SplitTask option" OFF)

if(SplitTask)
set(SPLITTASKOPTION "-O0 -fsplit-stack -DUSING_SPLIT_STACK" )
set(SPLITTASKOPTION_LINKER_FLAGS "-fuse-ld=gold")
else()
set(SPLITTASKOPTION "" )
set(SPLITTASKOPTION_LINKER_FLAGS "")
endif()

MESSAGE("SOURCELIST: ${SRC_LIST}")
MESSAGE("SPLITTASKOPTION: ${SPLITTASKOPTION}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SPLITTASKOPTION_LINKER_FLAGS}")

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -fuse-ld=gold -fsplit-stack -fno-stack-protector -DUSING_SPLIT_STACK -Wall -Werror -Wextra -D__FILENAME__='\"$(subst ${CMAKE_SOURCE_DIR}/,,$(abspath $<))\"'")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall -Werror -Wextra -D__FILENAME__='\"$(subst ${CMAKE_SOURCE_DIR}/,,$(abspath $<))\"'")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -DDEBUG_SCHED_TASK -DDEBUG_EV_TASK -Wall -Werror -Wextra -D__FILENAME__='\"$(subst ${CMAKE_SOURCE_DIR}/,,$(abspath $<))\"'")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SPLITTASKOPTION} -std=c99 -Wall -Werror -Wextra -D__FILENAME__='\"$(subst ${CMAKE_SOURCE_DIR}/,,$(abspath $<))\"'")
