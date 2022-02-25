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
    /* A workload with indirect branches to help serve as a performance test
     * by checking the cache exits (xref #5352).
     */
    double res = 0.;
    /* Run enough iterations to distinguish good from bad exit counts. */
    for (int i = 0; i < 10 * COMPUTE_ITERS; i++) {
        /* Create multiple return points so a single trace doesn't capture them
         * all, to test table lookups (as in i#5352).
         */
        if (i % 4 == 0)
            res += cos(1. / (double)(i + 1));
        else if (i % 4 == 1)
            res += cos(1. / (double)(i + 2));
        else if (i % 4 == 2)
            res += cos(1. / (double)(i + 3));
        else if (i % 4 == 3)
            res += cos(1. / (double)(i + 4));
    }
    if (res == 0.)
        print("result is 0\n");
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
        if (pc == (app_pc)funcs[i])
            took_over_thread[i] = true;
    }
    return DR_EMIT_DEFAULT;
}

static volatile bool sideline_exit = false;
static void *sideline_continue;
static void *go_native;
static void *sideline_ready[NUM_THREADS];

THREAD_FUNC_RETURN_TYPE
sideline_spinner(void *arg)
{
    unsigned int idx = (unsigned int)(uintptr_t)arg;
    void_func_t sideline_func = funcs[idx];
    if (dr_app_running_under_dynamorio())
        print("ERROR: thread %d should NOT be under DynamoRIO\n", idx);
    VPRINT("%d signaling sideline_ready\n", idx);
    signal_cond_var(sideline_ready[idx]);

    VPRINT("%d waiting for continue\n", idx);
    wait_cond_var(sideline_continue);
    if (!dr_app_running_under_dynamorio())
        print("ERROR: thread %d should be under DynamoRIO\n", idx);
    sideline_func();
    VPRINT("%d signaling sideline_ready\n", idx);
    signal_cond_var(sideline_ready[idx]);

    /* Ideally we'd have a better test that our state after the detach is not
     * perturbed at all (i#3160), though if the PC is correct that's generally
     * half the battle.  The detach_state.c test adds such checks for us in a
     * more controlled threading context.
     */

    VPRINT("%d waiting for native\n", idx);
    wait_cond_var(go_native);
    if (dr_app_running_under_dynamorio())
        print("ERROR: thread %d should NOT be under DynamoRIO\n", idx);
    VPRINT("%d signaling sideline_ready\n", idx);
    signal_cond_var(sideline_ready[idx]);
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
    int i;
    void *stack = NULL;
    thread_t thread[NUM_THREADS]; /* On Linux, the tid. */

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

    /* Initialized DR */
    dr_app_setup();
    /* XXX: Calling the client interface from the app is not supported.  We're
     * just using it for testing.
     */
    dr_register_bb_event(event_bb);

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

    /* Detach */
    VPRINT("detaching\n");
    /* We use the _with_stats variant to catch register errors such as i#4457. */
    dr_stats_t stats = { sizeof(dr_stats_t) };
    dr_app_stop_and_cleanup_with_stats(&stats);
    assert(stats.basic_block_count > 0);
    /* Sanity check: we expect <10K exits but we allow some leniency to avoid flakiness.
     * On a repeat of i#5352 we would see >500K exits.
     */
    assert(stats.num_cache_exits < 15000);

    VPRINT("signaling native\n");
    signal_cond_var(go_native);
    for (i = 0; i < NUM_THREADS; i++) {
        wait_cond_var(sideline_ready[i]);
        reset_cond_var(sideline_ready[i]);
    }

    for (i = 0; i < COMPUTE_ITERS; i++) {
        if (i % 2 == 0) {
            res += cos(1. / (double)(i + 1));
        } else {
            res += sin(1. / (double)(i + 1));
        }
    }
    foo();
    print("all done: %d iters\n", i);

    for (i = 0; i < NUM_THREADS; i++) {
        join_thread(thread[i]);
        if (!took_over_thread[i]) {
            print("failed to take over thread %d!\n", i);
        }
    }

    destroy_cond_var(sideline_continue);
    destroy_cond_var(go_native);
    for (i = 0; i < NUM_THREADS; i++)
        destroy_cond_var(sideline_ready[i]);

    return 0;
}
