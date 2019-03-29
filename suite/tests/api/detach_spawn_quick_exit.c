/* **********************************************************
 * Copyright (c) 2011-2019 Google, Inc.  All rights reserved.
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

/* This test is a modified version of api.detach_spawn. It adds start/stop re-attach to
 * api.detach_spawn. It only spawns one thread. The thread is synch'd such that it exits
 * while running native. This case is not supported by DynamoRIO's dr_app_stop w/o detach,
 * but must work w/ dr_app_stop_and_cleanup()
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
#include <sys/syscall.h>

#define VERBOSE 0
#define NUM_THREADS 1

static void *thread_ready;
static volatile bool thread_do_stuff = true;
#if VERBOSE
#    define VPRINT(...) print(__VA_ARGS__)
#else
#    define VPRINT(...) /* nothing */
#endif

THREAD_FUNC_RETURN_TYPE
thread_func(void *arg)
{
    signal_cond_var(thread_ready);
    while (thread_do_stuff)
        ;
    return THREAD_FUNC_RETURN_ZERO;
}

int
main(void)
{
    thread_ready = create_cond_var();
    int i;
    thread_t threads[NUM_THREADS];

    print("Starting thread(s)\n");

    for (i = 0; i < NUM_THREADS; ++i)
        threads[i] = create_thread(thread_func, NULL);

    pid_t tid = dynamorio_syscall(SYS_gettid, 0);

    /* We setup and start at once to avoid process memory changing much between
     * the two.
     */
    wait_cond_var(thread_ready);

    dr_app_setup_and_start();

    if (!dr_app_running_under_dynamorio())
        print("ERROR: should be running under DynamoRio before calling dr_app_stop()\n");

    print("Running under DynamoRIO\n");

    dr_app_stop_and_cleanup();

    thread_sleep(50);

    if (dr_app_running_under_dynamorio())
        print("ERROR: should not be running under DynamoRio before calling "
              "dr_app_stop()\n");

    print("Not running under DynamoRIO\n");

    thread_do_stuff = false;
    thread_sleep(50);

    dr_app_setup_and_start();

    if (!dr_app_running_under_dynamorio())
        print("ERROR: should be running under DynamoRio before calling dr_app_stop()\n");

    print("Running under DynamoRIO\n");

    dr_app_stop_and_cleanup();

    print("Not running under DynamoRIO, exiting\n");

    for (i = 0; i < NUM_THREADS; ++i)
        join_thread(threads[i]);
    destroy_cond_var(thread_ready);

    print("all done\n");
    return 0;
}
