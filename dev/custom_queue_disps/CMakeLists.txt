cmake_minimum_required(VERSION 3.10)

set(PRJ custom_queue_disps)

project(${PRJ})

get_filename_component(CURRENT_FILE_DIR ${CMAKE_CURRENT_LIST_FILE} DIRECTORY)
get_filename_component(CURRENT_FILE_DIR ${CURRENT_FILE_DIR} DIRECTORY)
set(CQD_INCLUDE_PATH ${CURRENT_FILE_DIR})
unset(CURRENT_FILE_DIR)

add_library(${PRJ} STATIC one_thread.cpp)

target_include_directories(${PRJ}
   PUBLIC
      $<BUILD_INTERFACE:${CQD_INCLUDE_PATH}>
)
target_link_libraries(${PRJ} sobjectizer::StaticLib)
