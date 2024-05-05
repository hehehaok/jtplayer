cmake_minimum_required(VERSION 3.22)

project(sonic)

set(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib)

add_library(sonic SHARED sonic.c)

target_include_directories(sonic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)