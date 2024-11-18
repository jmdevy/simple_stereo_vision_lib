cmake_minimum_required(VERSION 3.22)

project(main)
add_executable(main main.c)
target_include_directories(main PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)