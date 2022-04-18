/* **********************************************************
 * Copyright (c) 2016-2017 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "configure.h"
#include "dr_api.h"
#include "tools.h"
#include <math.h>
#include <pthread.h>
#include <stdlib.h>

/* XXX i#975: also add an api.static_takeover test that uses drrun
 * -static instead of calling dr_app_*.
 */

#define NUM_APP_THREADS 4
#define NUM_SIDELINE_THREADS 4

static bool finished[NUM_APP_THREADS];
static void *child_alive[NUM_SIDELINE_THREADS];
static void *child_continue[NUM_SIDELINE_THREADS];
static void *child_exit[NUM_SIDELINE_THREADS];
/* We test client sideline threads with synched exit in the first detachment
 * and non-synched exit in the second detachment.
 */
volatile static bool first_detach = true;
static int num_bbs;

static void
sideline_run(void *arg)
{
    int i = (int)(ptr_int_t)(arg);
    dr_fprintf(STDERR, "client thread %d is alive\n", i);
    dr_event_signal(child_alive[i]);
    if (first_detach) {
        /* wait till event_exit in the first detachment */
        dr_event_wait(child_continue[i]);
        dr_event_signal(child_exit[i]);
    }
}

static dr_emit_flags_t
event_bb(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    num_bbs++; /* racy update, but it is ok */
    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
    int i;
    for (i = 0; i < NUM_SIDELINE_THREADS; i++) {
        if (first_detach) {
            /* let child threads exit in the first detachment */
            dr_event_signal(child_continue[i]);
            dr_event_wait(child_exit[i]);
            dr_event_destroy(child_continue[i]);
            dr_event_destroy(child_exit[i]);
        }
        dr_event_destroy(child_alive[i]);
    }
    dr_fprintf(STDERR, "Saw %s bb events\n", num_bbs > 0 ? "some" : "no");
    first_detach = false;
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    int i;
    print("in dr_client_main\n");
    dr_register_bb_event(event_bb);
    dr_register_exit_event(event_exit);
    for (i = 0; i < NUM_SIDELINE_THREADS; i++) {
        child_alive[i] = dr_event_create();
        if (first_detach) {
            child_continue[i] = dr_event_create();
            child_exit[i] = dr_event_create();
        }
        dr_create_client_thread(sideline_run, (void *)(ptr_int_t)i);
        dr_event_wait(child_alive[i]);
    }
    /* XXX i#975: add some more thorough tests of different events */
}

static int
do_some_work(void)
{
    static int iters = 8192;
    int i;
    double val = num_bbs;
    for (i = 0; i < iters; ++i) {
        val += sin(val);
    }
    return (val > 0);
}

void *
thread_func(void *arg)
{
    int idx = (int)(ptr_int_t)arg;
    if (do_some_work() < 0)
        print("error in computation\n");
    finished[idx] = true;
    return NULL;
}

int
main(int argc, const char *argv[])
{
    int i;
    pthread_t thread[NUM_APP_THREADS];

    /* test attaching to multi-threaded app */
    for (i = 0; i < NUM_APP_THREADS; i++) {
        pthread_create(&thread[i], NULL, thread_func, (void *)(ptr_int_t)i);
    }
    print("pre-DR init\n");
    dr_app_setup();
    assert(!dr_app_running_under_dynamorio());

    print("pre-DR start\n");
    dr_app_start();
    assert(dr_app_running_under_dynamorio());

    for (i = 0; i < NUM_APP_THREADS; i++) {
        pthread_join(thread[i], NULL);
        if (!finished[i])
            print("ERROR: thread %d failed to finish\n", i);
    }

    dr_app_stop_and_cleanup();
    print("post-DR detach\n");
    assert(!dr_app_running_under_dynamorio());

    /* i#2157: test re-attach */
    print("re-attach attempt\n");
    if (dr_app_running_under_dynamorio())
        print("ERROR: should not be under DynamoRIO after dr_app_stop!\n");
    dr_app_setup_and_start();
    if (!dr_app_running_under_dynamorio())
        print("ERROR: should be under DynamoRIO after dr_app_start!\n");
    for (i = 0; i < NUM_APP_THREADS; i++) {
        pthread_create(&thread[i], NULL, thread_func, (void *)(ptr_int_t)i);
    }
    /* test detaching from multi-threaded app */
    dr_app_stop_and_cleanup();
    if (dr_app_running_under_dynamorio())
        print("ERROR: should not be under DynamoRIO after dr_app_stop!\n");
    for (i = 0; i < NUM_APP_THREADS; i++) {
        pthread_join(thread[i], NULL);
        if (!finished[i])
            print("ERROR: thread %d failed to finish\n", i);
    }
    print("all done\n");
    return 0;
}
