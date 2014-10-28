project(sched2)
cmake_minimum_required(VERSION 2.8)
aux_source_directory(. SRC_LIST)
include_directories(../../uthash/src/)
add_executable(${PROJECT_NAME} ${SRC_LIST})

MESSAGE("SOURCELIST: ${SRC_LIST}")

#SET(CMAKE_VERBOSE_MAKEFILE ON)
#FOREACH(i IN LISTS ${SRC_LIST})
#    set_source_files_properties(${i} PROPERTIES ${CMAKE_C_DIALECT_C99})
#ENDFOREACH()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall")
