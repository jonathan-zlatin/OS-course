//
// Created by skche on 04/06/2024.
//

#include "Thread.h"
#include "uthreads.h"
#include <iostream>
#include <list>
#include <sys/time.h>
#include <csignal>
#include <csetjmp>
#include <queue>

#define RUNNING_JMP 1
#define BLOCKED_JMP 2
#define READY_JMP 3
#define ENDED_JMP 4

Thread* threads[MAX_THREAD_NUM] = {}; // array of all pointers to threads


std::list<Thread *> ready_threads;
std::list<Thread *> blocked_threads;

sigset_t masked_set;

static int current_thread_id = 0;
struct itimerval timer;
struct sigaction sa;

int first_available_id(){
    for(int i = 0 ; i < MAX_THREAD_NUM; i ++){
        if(threads[i] == nullptr){
            return i;
        }
    }
    return -1;
}

/**
 * @brief initializes the thread library.
 *
 * Once this function returns, the main thread (tid == 0) will be set as RUNNING. There is no need to
 * provide an entry_point or to create a stack for the main thread - it will be using the "regular" stack and PC.
 * You may assume that this function is called before any other thread library function, and that it is called
 * exactly once.
 * The input to the function is the length of a quantum in micro-seconds.
 * It is an error to call this function with non-positive quantum_usecs.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs){
    if(quantum_usecs <= 0){
        printf("thread library error: quantum_usecs must be positive\n",stderr);
    }
    threads[0] = new Thread();
    if(threads[0] == nullptr){
        printf("system error: memory allocation failed\n",stderr);
        exit(1);
    }
    threads[0]->state = State::RUNNING;
    threads[0]->quantums_to_run = quantum_usecs;
    siglongjmp(threads[0]->env, 1);
    return 1;
}

/**
 * @brief Creates a new thread, whose entry point is the function entry_point with the signature
 * void entry_point(void).
 *
 * The thread is added to the end of the READY threads list.
 * The uthread_spawn function should fail if it would cause the number of concurrent threads to exceed the
 * limit (MAX_THREAD_NUM).
 * Each thread should be allocated with a stack of size STACK_SIZE bytes.
 * It is an error to call this function with a null entry_point.
 *
 * @return On success, return the ID of the created thread. On failure, return -1.
*/
int uthread_spawn(thread_entry_point entry_point){
    if(entry_point == nullptr){
        printf("thread library error: entry_point is null\n",stderr);
        return -1;
    }
    int tid = first_available_id();
    if(tid==-1){
        printf("thread library error: maximum number of threads exceeded\n",stderr);
        return -1;
    }
    threads[tid] = new Thread(tid,entry_point);
    if(threads[tid] == nullptr){
        printf("system error: memory allocation failed\n",stderr);
        exit(1);
    }
    ready_threads.push_back(threads[tid]);
    return tid;
}

int terminate_all_threads(){
    return 0;
}

/**
 * @brief Terminates the thread with ID tid and deletes it from all relevant control structures.
 *
 * All the resources allocated by the library for this thread should be released. If no thread with ID tid exists it
 * is considered an error. Terminating the main thread (tid == 0) will result in the termination of the entire
 * process using exit(0) (after releasing the assigned library memory).
 *
 * @return The function returns 0 if the thread was successfully terminated and -1 otherwise. If a thread terminates
 * itself or the main thread is terminated, the function does not return.
*/
int uthread_terminate(int tid){
    if(tid < 0 || tid >= MAX_THREAD_NUM){
        printf("thread library error: out of bounds thread id \n",stderr);
        return -1;
    }
    if(threads[tid] == nullptr){
        printf("thread library error: thread id doesnt exist \n",stderr);
        return -1;
    }
    if(tid == 0){
        terminate_all_threads();
    }
    if(tid == current_thread_id){
        ////
    }
    Thread* thread_to_erase = threads[tid];
    ready_threads.remove(thread_to_erase);
    blocked_threads.remove(thread_to_erase);
    delete threads[tid];
    threads[tid] = nullptr;
    return EXIT_SUCCESS;
}

int jump_to_next_thread() {
    Thread* next_thread = ready_threads.front();
    ready_threads.remove(next_thread);
    current_thread_id = next_thread->thread_id;
    siglongjmp(next_thread->env,1);
}

/**
 * @brief Blocks the thread with ID tid. The thread may be resumed later using uthread_resume.
 *
 * If no thread with ID tid exists it is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision should be made. Blocking a thread in
 * BLOCKED state has no effect and is not considered an error.
 *
 * @return On success, return 0. On failure, return - 1
*/
int uthread_block(int tid){
    if (tid < 0 || tid >= MAX_THREAD_NUM){
        printf("thread library error: thread id is out of bound \n",stderr);
        return -1;
    } if (tid == 0) {
        printf("thread library error: can't block the main thread \n", stderr);
        return -1;
    }
    if (threads[tid] == nullptr) {
        printf("thread library error: the thread given doesn't exists \n", stderr);
        return -1;
    }

    if (threads[tid]->state != State::BLOCKED){
        if (threads[tid]->state == State::READY){
            threads[tid]->state = State::BLOCKED;
            blocked_threads.push_back(threads[tid]);
            ready_threads.remove(threads[tid]);
        }
        if (threads[tid]->state == State::RUNNING){
            threads[tid]->state = State::BLOCKED;
            blocked_threads.push_back(threads[tid]);
            jump_to_next_thread();
        }
    }

}

