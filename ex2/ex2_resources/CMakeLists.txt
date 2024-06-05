cmake_minimum_required(VERSION 3.26)
project(ex2_resources)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(ex2_resources
#        demo_itimer.c)
#        demo_jmp.c
#        demo_singInt_handler.c
#        Thread.cpp
        uthreads.cpp
        uthreads.h
        test0_sanity.txt
        test0_sanity.cpp
)
