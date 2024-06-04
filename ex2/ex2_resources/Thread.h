//
// Created by skche on 04/06/2024.
//
#include <csetjmp>
#include "uthreads.h"

#ifndef EX2_RESOURCES_THREAD_H
#define EX2_RESOURCES_THREAD_H

//The Threads
//Initially, a program is comprised of the default main thread, whose ID is 0.
//All other threads will be explicitly created. Each existing thread has a unique thread ID, which is a non-negative
//        integer. The ID given to a new thread must be the smallest non-negative integer not already
//        taken by an existing thread (i.e. if a thread with ID 1 is terminated and then a new thread is
//spawned, it should receive 1 as its ID).
//The maximal number of threads the library should support (including the main thread) is MAX_THREAD_NUM.
//Thread State Diagram
//        At any given time during the running of the user's program,
//each thread in the program is in one of the states shown in the following state diagram.
//Transitions from state to state occur as a
//result of calling one of the library functions, or from elapsing of time, as explained below.
//This state diagram must not be changed: do not add or remove states.
//Each thread can be in one of the following states: RUNNING, BLOCKED, or READY.

enum State {
    RUNNING,
    BLOCKED,
    READY
};

class Thread{
public:
    Thread(); // default constructor
    Thread(int thread_id, thread_entry_point entry_point_func);
    int thread_id; // thread's id
    State state;
    thread_entry_point entry_point_func; // thread's entry point function defined in uthreads.h




    int quantums_to_run; // overall time for the thread to run
    int quantums_to_sleep; //total time for thread to sleep

    sigjmp_buf env; // thread's environment data, including PC and SP, also the signal mask when declared
    char stack[STACK_SIZE]; // thread's stack
};


#endif //EX2_RESOURCES_THREAD_H
