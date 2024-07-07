cmake_minimum_required(VERSION 3.25)
project(hw1 C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(hw1
        os.c
        os.h pt.c)
