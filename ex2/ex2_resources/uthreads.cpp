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
#include <array>

#define RUNNING_JMP 1
#define BLOCKED_JMP 2
#define READY_JMP 3
#define TERMINATED_JMP 4

std::array<Thread*, MAX_THREAD_NUM> threads = {};

std::list<Thread *> ready_threads;
std::list<Thread *> blocked_threads;

static int current_thread_id = 0;
static int quantum_duration = 0;
static int total_quantums = 0;

struct itimerval timer;
struct sigaction sa;

sigset_t ms;

void jump_to_next_thread(int state);

void timer_handler(int sig){

    jump_to_next_thread(READY_JMP);
}

int initiate_timer(int quantum_usecs){

    if(sigemptyset(&ms) == -1){
        printf("system error: failed to empty set\n.");
    }
    if(sigaddset(&ms,SIGVTALRM) == -1){
        printf("system error: failed to add signal to mask\n.");
    }

    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = quantum_usecs;

    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;

    // Start a virtual timer. It counts down whenever this process is executing.
    if (setitimer(ITIMER_VIRTUAL, &timer, NULL))
    {
        printf("system error: timer failed to start\n.");
    }
    return EXIT_SUCCESS;
}

int first_available_id(){
    for(int i = 0 ; i < MAX_THREAD_NUM; i ++){
        if(threads[i] == nullptr){
            return i;
        }
    }
    return -1;
}

void block_mask_sig(){
    if(sigprocmask(SIG_BLOCK,&ms, nullptr) == -1){
        printf("system error: failed to block signal\n.");
    }
}

void unblock_mask_sig(){
    if(sigprocmask(SIG_UNBLOCK,&ms, nullptr) == -1){
        printf("system error: failed to unblock signal\n.");
    }
}

//Reached here because quantum expired
//We move running thread to ready and first ready to running
int timer_handler(){
    sigsetjmp(threads[current_thread_id]->env, 1);
    ready_threads.push_back(threads[current_thread_id]);
    jump_to_next_thread(READY_JMP);
    return 0;
}

int handle_valid_thread_id(int tid){
    if(tid < 0 || tid >= MAX_THREAD_NUM){
        printf("thread library error: out of bounds thread id \n",stderr);
        return -1;
    }
    if(threads[tid] == nullptr){
        printf("thread library error: thread id is null\n",stderr);
        return -1;
    }
    return 0;
}

void delete_single_thread(int tid){
    block_mask_sig();

    ready_threads.remove(threads[tid]);
    blocked_threads.remove(threads[tid]);
    delete threads[tid];
    threads[tid] = nullptr;

    unblock_mask_sig();
}

int terminate_all_threads(){
    for(int i = 0 ; i < MAX_THREAD_NUM ; i ++){
        delete_single_thread(i);
    }
    return EXIT_SUCCESS;
}

bool is_thread_blocked(int tid) {
    bool thread_in_blocked = false;
    for(auto t : blocked_threads){
        if(t->thread_id == tid){
            thread_in_blocked = true;
            break;
        }
    }
    return thread_in_blocked;
}

int decrease_sleeping() {
    for(int i = 0; i < MAX_THREAD_NUM; i ++){
        if(threads[i] != nullptr){
            if(threads[i]->quantums_to_sleep > 0){
                threads[i]->quantums_to_sleep--;
                bool thread_in_blocked = is_thread_blocked(i);
                if(threads[i]->quantums_to_sleep == 0 && !thread_in_blocked){
                    threads[i]->state = State::READY;
                    ready_threads.push_back(threads[i]);
                }
            }
        }
    }
    return 0;
}

void jump_to_next_thread(int state) {

    //update run time of current thread before going to next one
    if(state != TERMINATED_JMP)
        threads[current_thread_id]->total_run_time ++;
   // sigsetjmp(threads[current_thread_id]->env,1);

    //chose behaviour according to how we reached the function
    int ret_val = 0;
    switch (state) {
        case BLOCKED_JMP:
            uthread_block(current_thread_id);
            ret_val = sigsetjmp(threads[current_thread_id]->env,1);
            break;
        case READY_JMP:
            threads[current_thread_id]->state = State::READY;
            ready_threads.push_back(threads[current_thread_id]);
            ret_val = sigsetjmp(threads[current_thread_id]->env,1);
            break;
        case TERMINATED_JMP:
//            uthread_terminate(current_thread_id);
            break;
        case RUNNING_JMP:
            ready_threads.push_back(threads[0]);
            break;
        default:
            break;
    }
//    if(ret_val == 0){
//    }

    //general updates
    decrease_sleeping();

    //choose new thread
    if(ret_val == 0){
        total_quantums++;
        //while (ready_threads.front() == nullptr);
        Thread* next_thread = ready_threads.front();
        if(next_thread == nullptr){
            printf("thread library error: tried to run next thread but ready threads are empty");
        }

        ready_threads.remove(next_thread);
        current_thread_id = next_thread->thread_id;
        threads[current_thread_id]->state = State::RUNNING;
        int timer_return_val = initiate_timer(quantum_duration);
    //activate new thread
        if(state != RUNNING_JMP)
            siglongjmp(next_thread->env,1);}
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
    quantum_duration = quantum_usecs;
    if(quantum_usecs < 0){
        printf("thread library error: quantum_usecs must be positive\n",stderr);
        return -1;
    }
    threads[0] = new Thread();
    if(threads[0] == nullptr){
        printf("system error: memory allocation failed\n",stderr);
        exit(1);
    }
    threads[0]->state = State::RUNNING;
    threads[0]->total_run_time = 0;
//    ready_threads.push_back(threads[0]);
    // Action to take when alarm sounds
    sa.sa_handler = &timer_handler;
    if (sigaction(SIGVTALRM, &sa, NULL) < 0)
    {
        printf("sigaction error.");
    }
    //
    initiate_timer(quantum_usecs);
    jump_to_next_thread(RUNNING_JMP);
    return 0;
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
    block_mask_sig();
    threads[tid] = new Thread(tid,entry_point);
    if(threads[tid] == nullptr){
        printf("system error: memory allocation failed\n",stderr);
        unblock_mask_sig();
        exit(1);
    }
    ready_threads.push_back(threads[tid]);
    unblock_mask_sig();
    threads[tid]->total_run_time=1;
    return tid;
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
    if(handle_valid_thread_id(tid) < 0){
        return -1;
    }
    if(tid == 0){
        terminate_all_threads();
        exit(0);
    }
    delete_single_thread(tid);
    jump_to_next_thread(TERMINATED_JMP);
    return EXIT_SUCCESS;
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
    if(handle_valid_thread_id(tid) < 0){
        return -1;
    }

    if (tid == 0) {
        printf("thread library error: can't block the main thread\n", stderr);
        return -1;
    }
    if (threads[tid]->state != State::BLOCKED){

        block_mask_sig();

        blocked_threads.push_back(threads[tid]);
        threads[tid]->state = State::BLOCKED;
        ready_threads.remove(threads[tid]);

        unblock_mask_sig();

        if(tid==current_thread_id){
            jump_to_next_thread(BLOCKED_JMP);
        }
    }
    return 0;
}

/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid){
    if(handle_valid_thread_id(tid) < 0){
        return -1;
    }

    if(threads[tid]->state != State::BLOCKED){
        return 0;
    }
    //we reach here if the state was actually blocked
    block_mask_sig();

    blocked_threads.remove(threads[tid]);
    ready_threads.push_back(threads[tid]);

    unblock_mask_sig();

    return 0;
}

/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of the READY queue.
 * If the thread which was just RUNNING should also be added to the READY queue, or if multiple threads wake up
 * at the same time, the order in which they're added to the end of the READY queue doesn't matter.
 * The number of quantums refers to the number of times a new quantum starts, regardless of the reason. Specifically,
 * the quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * It is considered an error if the main thread (tid == 0) calls this function.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_sleep(int num_quantums){
    if(current_thread_id == 0){
        printf("thread library error: the main thread cannot go to sleep\n", stderr);
        return -1;
    }

    block_mask_sig();

    threads[current_thread_id]->quantums_to_sleep = num_quantums + 1;
    jump_to_next_thread(BLOCKED_JMP);

    unblock_mask_sig();
    return 0;
}

/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
*/
int uthread_get_tid(){
    return current_thread_id;
}

/**
 * @brief Returns the total number of quantums since the library was initialized, including the current quantum.
 *
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should be increased by 1.
 *
 * @return The total number of quantums.
*/
int uthread_get_total_quantums(){
    return total_quantums;
}

/**
 * @brief Returns the number of quantums the thread with ID tid was in RUNNING state.
 *
 * On the first time a thread runs, the function should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state when this function is called, include
 * also the current quantum). If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums(int tid){
    if(handle_valid_thread_id(tid) < 0){
        return -1;
    }
    return threads[tid]->total_run_time;
}