/* **********************************************************
 * Copyright (c) 2016-2025 Google, Inc.  All rights reserved.
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

// Tests detaching mid-basic-block (i#7572).

#include "test_helpers.h"
#include "dr_api.h"
#include <assert.h>
#include <atomic>
#include <iostream>
#include <string>
#include <thread>
#include "../../../suite/tests/thread.h"
#include "../../../suite/tests/condvar.h"

namespace dynamorio {
namespace drmemtrace {

static std::atomic<bool> child_should_exit;

bool
my_setenv(const char *var, const char *value)
{
    return setenv(var, value, 1 /*override*/) == 0;
}

static THREAD_FUNC_RETURN_TYPE
thread_func(void *arg)
{
    while (!child_should_exit.load(std::memory_order_acquire)) {
#if defined(X86_64) && defined(LINUX)
        // We want a long basic block in DR with slow instructions to increase
        // the chance a detach happens mid-block.
        // DR's default block limit is 256 instructions.
        // We need at least one load or store toward the end to trigger the bug.
        // We can't use generated code as the i#7572 bug only happens with
        // static code.
        __asm__ __volatile__("fpatan\n\t" // 1
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t" // 10
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t" // 20
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t" // 30
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t" // 40
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t" // 50
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t" // 60
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t" // 70
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t" // 80
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t" // 90
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t" // 100
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t" // 110
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t" // 120
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t" // 130
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t" // 140
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t" // 150
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t" // 160
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t" // 170
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t" // 180
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t" // 190
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t"
                             "fpatan\n\t" // 200
                             "mov (%rsp),%rax\n\t");
#else
        // It is not worth the effort to replicate on other platforms.
#    error Not implemented
#endif
    }
    return nullptr;
}

static int
do_some_work()
{
    // To reduce the trace size we just sleep and let the other thread
    // be the only one generating data.
    constexpr int MILLIS_TO_SLEEP = 10;
    std::this_thread::sleep_for(std::chrono::milliseconds(MILLIS_TO_SLEEP));
    return 1;
}

static void
gather_trace()
{
    // We need -no_align_endpoints to make reproducing the bug much more likely.
    // Otherwise, the tracer switches to nop mode and the detach happens mid-block
    // but tracing has already ended.
    if (!my_setenv("DYNAMORIO_OPTIONS",
                   "-stderr_mask 0xc -client_lib ';;-offline -no_align_endpoints"))
        std::cerr << "failed to set env var!\n";
    std::cerr << "pre-DR init\n" << std::flush;
    dr_app_setup_and_start();
    assert(dr_app_running_under_dynamorio());
    if (do_some_work() < 0)
        std::cerr << "error in computation\n";
    // TODO i#6490: This app produces incorrect output when run under DR if we do
    // not flush. std::endl makes the issue worse even though it should do an
    // internal flush.
    std::cerr << "pre-DR detach\n" << std::flush;
    dr_app_stop_and_cleanup();
    std::cerr << "all done\n" << std::flush;
}

int
test_main(int argc, const char *argv[])
{
    // Start up a thread that spends most of its time in a long block.
    thread_t thread = create_thread(thread_func, nullptr);
    // Now gather a trace where the detach should often split the block.
    gather_trace();
    child_should_exit.store(true, std::memory_order_release);
    join_thread(thread);
    // The test harness will post-process the trace and run invariant_checker
    // (finding the raw dir via glob on test name) to finish the test.
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
