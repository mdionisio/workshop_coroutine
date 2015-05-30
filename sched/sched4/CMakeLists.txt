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

MESSAGE("SOURCELIST: ${SRC_LIST}")


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall")
