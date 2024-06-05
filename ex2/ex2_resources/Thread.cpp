//
// Created by skche on 04/06/2024.
//

#include "Thread.h"
#include <setjmp.h>
#include <signal.h>

// Code from demo_jmp.c
#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
        "rol    $0x11,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

#else
///* code for 32 bit Intel arch */
//
//typedef unsigned int address_t;
//#define JB_SP 4
//#define JB_PC 5
//
//
///* A translation is required when using an address of a variable.
//   Use this as a black box in your code. */
//address_t translate_address(address_t addr)
//{
//    address_t ret;
//    asm volatile("xor    %%gs:0x18,%0\n"
//                 "rol    $0x9,%0\n"
//            : "=g" (ret)
//            : "0" (addr));
//    return ret;
//}


#endif

//Default constructor
Thread::Thread()
{
    thread_id = 0;
    state = State::READY;
    total_run_time = 0;
    quantums_to_sleep = 0;
    sigemptyset(&env->__saved_mask); // taken from demo_jmp.c
}

Thread::Thread(int thread_id, thread_entry_point entry_point_func)  : thread_id(thread_id), state(State::READY),
                                                                      entry_point_func(entry_point_func), total_run_time(0), quantums_to_sleep(0){
    // ===========          CODE FROM demo_jmp.c  =============

    address_t sp = (address_t) stack + STACK_SIZE - sizeof(address_t);
    address_t pc = (address_t) entry_point_func;
    sigsetjmp(env, 1);
    (env->__jmpbuf)[JB_SP] = translate_address(sp);
    (env->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&env->__saved_mask);
}