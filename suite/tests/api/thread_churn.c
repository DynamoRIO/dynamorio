/* **********************************************************
 * Copyright (c) 2020 Google, Inc.  All rights reserved.
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
#include "thread.h"
#include <math.h>

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    print("in dr_client_main\n");
}

static THREAD_FUNC_RETURN_TYPE
thread_function(void *arg)
{
    return THREAD_FUNC_RETURN_ZERO;
}

static void
churn_threads(int count)
{
    thread_t thread;
    for (int i = 0; i < count; ++i) {
        thread = create_thread(thread_function, NULL);
        join_thread(thread);
    }
}

int
main(int argc, char **argv)
{
    /* We test thread exit leaks by ensuring memory usage is the same after
     * both 5 threads and 500 threads.
     */
    const int count_A = 10;
    const int count_B = 500;
#define VERBOSE 0
#if VERBOSE
    if (!my_setenv("DYNAMORIO_OPTIONS", "-rstats_to_stderr -stderr_mask 0xc"))
        print("Failed to set env var!\n");
#endif

    assert(!dr_app_running_under_dynamorio());
    dr_app_setup_and_start();
    assert(dr_app_running_under_dynamorio());
    churn_threads(count_A);
    dr_stats_t stats_A = { sizeof(dr_stats_t) };
    dr_app_stop_and_cleanup_with_stats(&stats_A);
    assert(!dr_app_running_under_dynamorio());
    assert(stats_A.peak_num_threads == 2);
    assert(stats_A.num_threads_created == count_A + 1);

    assert(!dr_app_running_under_dynamorio());
    dr_app_setup_and_start();
    assert(dr_app_running_under_dynamorio());
    churn_threads(count_B);
    dr_stats_t stats_B = { sizeof(dr_stats_t) };
    dr_app_stop_and_cleanup_with_stats(&stats_B);
    assert(!dr_app_running_under_dynamorio());
    assert(stats_B.peak_num_threads == 2);
    assert(stats_B.num_threads_created == count_B + 1);

    /* XXX: Somehow the first run has *more* heap blocks.  2nd and any subsequent
     * are identical.  Just living with that and requiring <=.
     */
    assert(stats_B.peak_vmm_blocks_unreach_heap <= stats_A.peak_vmm_blocks_unreach_heap);
    assert(stats_B.peak_vmm_blocks_unreach_stack <=
           stats_A.peak_vmm_blocks_unreach_stack);
    assert(stats_B.peak_vmm_blocks_unreach_special_heap <=
           stats_A.peak_vmm_blocks_unreach_special_heap);
    assert(stats_B.peak_vmm_blocks_unreach_special_mmap <=
           stats_A.peak_vmm_blocks_unreach_special_mmap);
    assert(stats_B.peak_vmm_blocks_reach_heap <= stats_A.peak_vmm_blocks_reach_heap);
    assert(stats_B.peak_vmm_blocks_reach_cache <= stats_A.peak_vmm_blocks_reach_cache);
    assert(stats_B.peak_vmm_blocks_reach_special_heap <=
           stats_A.peak_vmm_blocks_reach_special_heap);
    assert(stats_B.peak_vmm_blocks_reach_special_mmap <=
           stats_A.peak_vmm_blocks_reach_special_mmap);

    print("all done\n");
    return 0;
}
