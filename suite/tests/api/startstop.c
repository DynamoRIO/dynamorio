/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2008 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include "configure.h"
#include "dr_api.h"
#include "tools.h"
#include "thread.h"
#include "condvar.h"

#define VERBOSE 0

#define NUM_THREADS 10
#define START_STOP_ITERS 10
#define COMPUTE_ITERS 150000

#if VERBOSE
#    define VPRINT(...) print(__VA_ARGS__)
#else
#    define VPRINT(...) /* nothing */
#endif

/* We have event bb look for this to make sure we're instrumenting the sideline
 * thread.
 */
/* We could generate this via macros but that gets pretty obtuse */
#define NUM_FUNCS 10
NOINLINE void
func_0(void)
{
}
NOINLINE void
func_1(void)
{
}
NOINLINE void
func_2(void)
{
}
NOINLINE void
func_3(void)
{
}
NOINLINE void
func_4(void)
{
}
NOINLINE void
func_5(void)
{
}
NOINLINE void
func_6(void)
{
}
NOINLINE void
func_7(void)
{
}
NOINLINE void
func_8(void)
{
}
NOINLINE void
func_9(void)
{
}

typedef void (*void_func_t)(void);
static bool took_over_thread[NUM_THREADS];
static void_func_t funcs[NUM_THREADS];

static dr_emit_flags_t
event_bb(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    int i;
    app_pc pc = instr_get_app_pc(instrlist_first(bb));
    for (i = 0; i < NUM_THREADS; i++) {
        if (pc == (app_pc)funcs[i % NUM_FUNCS])
            took_over_thread[i] = true;
    }
    return DR_EMIT_DEFAULT;
}

static void
event_post_attach(void)
{
    print("in %s\n", __FUNCTION__);
}

static void
event_pre_detach(void)
{
    print("in %s\n", __FUNCTION__);
}

static volatile bool sideline_exit = false;
static void *sideline_continue;
static void *go_native;
static void *sideline_ready[NUM_THREADS];

THREAD_FUNC_RETURN_TYPE
sideline_spinner(void *arg)
{
    unsigned int idx = (unsigned int)(uintptr_t)arg;
    void_func_t sideline_func = funcs[idx % NUM_FUNCS];
    if (dr_app_running_under_dynamorio())
        print("ERROR: thread %d should NOT be under DynamoRIO\n", idx);
    VPRINT("%d signaling sideline_ready\n", idx);
    signal_cond_var(sideline_ready[idx]);

    VPRINT("%d waiting for continue\n", idx);
    wait_cond_var(sideline_continue);
    sideline_func();
    VPRINT("%d signaling sideline_ready\n", idx);
    signal_cond_var(sideline_ready[idx]);

    VPRINT("%d waiting for native\n", idx);
    wait_cond_var(go_native);
    VPRINT("%d signaling sideline_ready\n", idx);
    signal_cond_var(sideline_ready[idx]);

    while (!sideline_exit) {
        VPRINT("%d waiting for continue\n", idx);
        wait_cond_var(sideline_continue);
        if (sideline_exit)
            break;

        if (!dr_app_running_under_dynamorio())
            print("ERROR: thread %d should be under DynamoRIO\n", idx);
        VPRINT("%d signaling sideline_ready\n", idx);
        signal_cond_var(sideline_ready[idx]);

        VPRINT("%d waiting for native\n", idx);
        wait_cond_var(go_native);
        if (dr_app_running_under_dynamorio())
            print("ERROR: thread %d should NOT be under DynamoRIO\n", idx);
        VPRINT("%d signaling sideline_ready\n", idx);
        signal_cond_var(sideline_ready[idx]);
    }
    VPRINT("%d exiting\n", idx);

    return THREAD_FUNC_RETURN_ZERO;
}

void
foo(void)
{
}

int
main(void)
{
    double res = 0.;
    int i, j;
    void *stack = NULL;
    thread_t thread[NUM_THREADS];

    /* We could generate this via macros but that gets pretty obtuse */
    funcs[0] = &func_0;
    funcs[1] = &func_1;
    funcs[2] = &func_2;
    funcs[3] = &func_3;
    funcs[4] = &func_4;
    funcs[5] = &func_5;
    funcs[6] = &func_6;
    funcs[7] = &func_7;
    funcs[8] = &func_8;
    funcs[9] = &func_9;

    sideline_continue = create_cond_var();
    go_native = create_cond_var();

    for (i = 0; i < NUM_THREADS; i++) {
        sideline_ready[i] = create_cond_var();
        thread[i] = create_thread(sideline_spinner, (void *)(uintptr_t)i);
    }

    /* Initialize DR */
    dr_app_setup();
    /* XXX: Calling the client interface from the app is not supported.  We're
     * just using it for testing.
     */
    dr_register_bb_event(event_bb);
    if (!dr_register_post_attach_event(event_post_attach))
        print("Failed to register post-attach event");
    dr_register_pre_detach_event(event_pre_detach);

    /* Wait for all the threads to be scheduled */
    VPRINT("waiting for ready\n");
    for (i = 0; i < NUM_THREADS; i++) {
        wait_cond_var(sideline_ready[i]);
        reset_cond_var(sideline_ready[i]);
    }
    /* Now get each thread to call its func_N under DR */
    dr_app_start();
    VPRINT("signaling continue\n");
    signal_cond_var(sideline_continue);
    VPRINT("waiting for ready\n");
    for (i = 0; i < NUM_THREADS; i++) {
        wait_cond_var(sideline_ready[i]);
        reset_cond_var(sideline_ready[i]);
    }
    reset_cond_var(sideline_continue);
    dr_app_stop();
    VPRINT("signaling native\n");
    signal_cond_var(go_native);

    for (j = 0; j < START_STOP_ITERS; j++) {
        for (i = 0; i < NUM_THREADS; i++) {
            wait_cond_var(sideline_ready[i]);
            reset_cond_var(sideline_ready[i]);
        }
        reset_cond_var(go_native);
        if (dr_app_running_under_dynamorio())
            print("ERROR: should not be under DynamoRIO before dr_app_start!\n");
        dr_app_start();
        if (!dr_app_running_under_dynamorio())
            print("ERROR: should be under DynamoRIO after dr_app_start!\n");
        VPRINT("loop %d signaling continue\n", j);
        signal_cond_var(sideline_continue);
        for (i = 0; i < COMPUTE_ITERS; i++) {
            if (i % 2 == 0) {
                res += cos(1. / (double)(i + 1));
            } else {
                res += sin(1. / (double)(i + 1));
            }
        }
        foo();
        if (!dr_app_running_under_dynamorio())
            print("ERROR: should be under DynamoRIO before dr_app_stop!\n");
        for (i = 0; i < NUM_THREADS; i++) {
            wait_cond_var(sideline_ready[i]);
            reset_cond_var(sideline_ready[i]);
        }
        reset_cond_var(sideline_continue);
        dr_app_stop();
        if (dr_app_running_under_dynamorio())
            print("ERROR: should not be under DynamoRIO after dr_app_stop!\n");
        VPRINT("loop %d signaling native\n", j);
        signal_cond_var(go_native);
    }
    /* PR : we get different floating point results on different platforms,
     * so we no longer print out res */
    print("all done: %d iters\n", j);
    for (i = 0; i < NUM_THREADS; i++) {
        wait_cond_var(sideline_ready[i]);
        reset_cond_var(sideline_ready[i]);
    }
    reset_cond_var(go_native);

    /* On x64 Linux it's OK if we call pthread_join natively, but x86-32 has
     * problems.  We start and stop to bracket it.
     */
    dr_app_start();
    sideline_exit = true; /* Break the loops. */
    signal_cond_var(sideline_continue);
    for (i = 0; i < NUM_THREADS; i++) {
        join_thread(thread[i]);
        if (!took_over_thread[i]) {
            print("failed to take over thread %d!\n", i);
        }
    }
    dr_app_stop();
    dr_app_cleanup();

    destroy_cond_var(sideline_continue);
    destroy_cond_var(go_native);
    for (i = 0; i < NUM_THREADS; i++)
        destroy_cond_var(sideline_ready[i]);

    return 0;
}
