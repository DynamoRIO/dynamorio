/* **********************************************************
 * Copyright (c) 2011-2016 Google, Inc.  All rights reserved.
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
#include "configure.h"
#include "dr_api.h"
#include "tools.h"
#ifdef WINDOWS
# include <windows.h>
#else
# include <pthread.h>
# include <sched.h>
#endif

#define ITERS 150000

/* We have event bb look for this to make sure we're instrumenting the sideline
 * thread.
 */
NOINLINE void func_0(void) { }
NOINLINE void func_1(void) { }
NOINLINE void func_2(void) { }
NOINLINE void func_3(void) { }
NOINLINE void func_4(void) { }
NOINLINE void func_5(void) { }
NOINLINE void func_6(void) { }
NOINLINE void func_7(void) { }
NOINLINE void func_8(void) { }
NOINLINE void func_9(void) { }

static bool took_over_thread[10];

static dr_emit_flags_t
event_bb(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
         bool translating)
{
    app_pc pc = instr_get_app_pc(instrlist_first(bb));
    if (pc == (app_pc)&func_0) took_over_thread[0] = true;
    if (pc == (app_pc)&func_1) took_over_thread[1] = true;
    if (pc == (app_pc)&func_2) took_over_thread[2] = true;
    if (pc == (app_pc)&func_3) took_over_thread[3] = true;
    if (pc == (app_pc)&func_4) took_over_thread[4] = true;
    if (pc == (app_pc)&func_5) took_over_thread[5] = true;
    if (pc == (app_pc)&func_6) took_over_thread[6] = true;
    if (pc == (app_pc)&func_7) took_over_thread[7] = true;
    if (pc == (app_pc)&func_8) took_over_thread[8] = true;
    if (pc == (app_pc)&func_9) took_over_thread[9] = true;
    return DR_EMIT_DEFAULT;
}

/* This is a thread that spins and calls sideline_func.  It will call
 * sideline_func at least once before returning and joining the parent.
 * We cannot use pthread_cond_var here as we need to keep spinning
 * until DR takes over and hits event_bb.
 */
static volatile bool should_spin = true;

#ifdef WINDOWS
int __stdcall
#else
void *
#endif
sideline_spinner(void *arg)
{
    void (*sideline_func)(void) = (void (*)(void))arg;
    do {
        sideline_func();
#ifdef WINDOWS
        Sleep(0);
#else
        sched_yield();
#endif
    } while (should_spin);
#ifdef WINDOWS
    return 0;
#else
    return NULL;
#endif
}

void foo(void)
{
}

int main(void)
{
    double res = 0.;
    int i,j;
    void *stack = NULL;
#ifdef UNIX
    pthread_t pt[10];  /* On Linux, the tid. */
#else
    uintptr_t thread[10];  /* _beginthreadex doesn't return HANDLE? */
    uint tid[10];
#endif

    /* Create spinning sideline threads. */
#ifdef UNIX
    pthread_create(&pt[0], NULL, sideline_spinner, (void*)func_0);
    pthread_create(&pt[1], NULL, sideline_spinner, (void*)func_1);
    pthread_create(&pt[2], NULL, sideline_spinner, (void*)func_2);
    pthread_create(&pt[3], NULL, sideline_spinner, (void*)func_3);
    pthread_create(&pt[4], NULL, sideline_spinner, (void*)func_4);
    pthread_create(&pt[5], NULL, sideline_spinner, (void*)func_5);
    pthread_create(&pt[6], NULL, sideline_spinner, (void*)func_6);
    pthread_create(&pt[7], NULL, sideline_spinner, (void*)func_7);
    pthread_create(&pt[8], NULL, sideline_spinner, (void*)func_8);
    pthread_create(&pt[9], NULL, sideline_spinner, (void*)func_9);
#else
    thread[0] = _beginthreadex(NULL, 0, sideline_spinner, (void*)func_0, 0, &tid[0]);
    thread[1] = _beginthreadex(NULL, 0, sideline_spinner, (void*)func_1, 0, &tid[1]);
    thread[2] = _beginthreadex(NULL, 0, sideline_spinner, (void*)func_2, 0, &tid[2]);
    thread[3] = _beginthreadex(NULL, 0, sideline_spinner, (void*)func_3, 0, &tid[3]);
    thread[4] = _beginthreadex(NULL, 0, sideline_spinner, (void*)func_4, 0, &tid[4]);
    thread[5] = _beginthreadex(NULL, 0, sideline_spinner, (void*)func_5, 0, &tid[5]);
    thread[6] = _beginthreadex(NULL, 0, sideline_spinner, (void*)func_6, 0, &tid[6]);
    thread[7] = _beginthreadex(NULL, 0, sideline_spinner, (void*)func_7, 0, &tid[7]);
    thread[8] = _beginthreadex(NULL, 0, sideline_spinner, (void*)func_8, 0, &tid[8]);
    thread[9] = _beginthreadex(NULL, 0, sideline_spinner, (void*)func_9, 0, &tid[9]);
#endif

    dr_app_setup();
    /* XXX: Calling the client interface from the app is not supported.  We're
     * just using it for testing.
     */
    dr_register_bb_event(event_bb);

    for (j=0; j<10; j++) {
        if (dr_app_running_under_dynamorio())
            print("ERROR: should not be under DynamoRIO before dr_app_start!\n");
        dr_app_start();
        if (!dr_app_running_under_dynamorio())
            print("ERROR: should be under DynamoRIO after dr_app_start!\n");
        for (i=0; i<ITERS; i++) {
            if (i % 2 == 0) {
                res += cos(1./(double)(i+1));
            } else {
                res += sin(1./(double)(i+1));
            }
        }
        foo();
        if (!dr_app_running_under_dynamorio())
            print("ERROR: should be under DynamoRIO before dr_app_stop!\n");
        /* FIXME i#95: On Linux dr_app_stop only makes the current thread run
         * native.  We should revisit this while implementing full detach for
         * Linux.
         */
        dr_app_stop();
        if (dr_app_running_under_dynamorio())
            print("ERROR: should not be under DynamoRIO after dr_app_stop!\n");
    }
    /* PR : we get different floating point results on different platforms,
     * so we no longer print out res */
    print("all done: %d iters\n", i);

    /* On x64 Linux it's OK if we call pthread_join natively, but x86-32 has
     * problems.  We start and stop to bracket it.
     */
    dr_app_start();
    should_spin = false;  /* Break the loops. */
    for (i = 0; i < 10; i++) {
#ifdef UNIX
        pthread_join(pt[i], NULL);
#else
        WaitForSingleObject((HANDLE)thread[i], INFINITE);
#endif
        if (!took_over_thread[i]) {
            print("failed to take over thread %d==%d!\n", i,
                  IF_WINDOWS_ELSE(tid[i],pt[i]));
        }
    }
    dr_app_stop();
    dr_app_cleanup();

    return 0;
}
