
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

/* This application links in drmemtrace_static and acquires a trace during
 * a "burst" of execution in the middle of the application.  It then detaches.
 */
#include "dr_api.h"
#include "../../../suite/tests/client_tools.h"
#include <assert.h>
#include <iostream>
#include <signal.h>
#include <setjmp.h>

#define ASSERT_NOT_REACHED() ASSERT_MSG(false, "Shouldn't be reached")

static sigjmp_buf mark;
static int handled_sigill_count = 0;

bool
my_setenv(const char *var, const char *value)
{
    return setenv(var, value, 1 /*override*/) == 0;
}

void
sigill_handler(int signal)
{
    handled_sigill_count++;
    siglongjmp(mark, handled_sigill_count);
    ASSERT_NOT_REACHED();
}

/* Attempts to execute the privileged 'dc ivac' instruction.
 * This will raise a SIGILL. Caller must register a SIGILL handler before
 * invoking this function.
 */
static void
dc_ivac()
{
    int d = 0;
    // Expected to raise SIGILL.
    // Control will be transferred to sigill_handler.
    __asm__ __volatile__("dc ivac, %0" : : "r"(&d));
}

static void
dc_unprivileged_flush()
{
    int d = 0;
    __asm__ __volatile__("dc civac, %0" : : "r"(&d));
    __asm__ __volatile__("dc cvac , %0" : : "r"(&d));
    __asm__ __volatile__("dc cvau , %0" : : "r"(&d));
}

static void
ic_unprivileged_flush()
{
    __asm__ __volatile__("ic ivau , %0" : : "r"(ic_unprivileged_flush));
}

static void
do_some_work()
{
    dc_unprivileged_flush();
    ic_unprivileged_flush();

    // Testing privileged instructions requires handling SIGILL.
    // We use sigsetjmp/siglongjmp to resume execution after handling the
    // signal.
    handled_sigill_count = 0;
    int i = sigsetjmp(mark, 1);
    switch (i) {
    case 0:
        dc_ivac();
        // TODO i#4406: Test other privileged dc and ic flush instructions too.
    }
    return;
}

int
main(int argc, const char *argv[])
{
    signal(SIGILL, sigill_handler);
    if (!my_setenv("DYNAMORIO_OPTIONS",
                   // XXX i#4425: Fix debug-build stack overflow issue and
                   // remove custom signal_stack_size below.
                   "-stderr_mask 0xc -signal_stack_size 64K "
                   "-client_lib ';;-offline'"))
        std::cerr << "failed to set env var!\n";

    std::cerr << "pre-DR init\n";
    dr_app_setup();
    assert(!dr_app_running_under_dynamorio());
    std::cerr << "pre-DR start\n";
    dr_app_start();
    assert(dr_app_running_under_dynamorio());
    do_some_work();
    dr_app_stop_and_cleanup();
    std::cerr << "all done\n";
    return 0;
}
