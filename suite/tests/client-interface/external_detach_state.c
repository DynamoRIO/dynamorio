/* **********************************************************
 * Copyright (c) 2025 Arm Limited  All rights reserved.
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

#include <stdint.h>
#include "tools.h"
#include "thread.h"
#include "api/detach_state_shared.h"
#include <signal.h>
#include <ucontext.h>

static bool seen_sigterm = false;

static void
signal_handler(int sig, siginfo_t *info, ucontext_t *ucxt)
{
    /* runall.cmake kills the test app by sending it SIGTERM after it has detached. We
     * intercept that signal and use it to trigger the test function to stop spinning and
     * run its post-detach check.
     */

    if (seen_sigterm) {
        /* If we see a second SIGTERM assume that something has gone wrong and the test
         * is hanging.
         */
        print("Exit after receiving multiple SIGTEM signals.\n");
        exit(1);
    }

    seen_sigterm = true;
    set_sideline_exit();
}

typedef struct _test_t {
    const char *name;
    void (*func)(void);
} test_t;

static const test_t tests[] = {
    { "gprs_from_cache", thread_check_gprs_from_cache },
#if defined(X86) || defined(AARCH64)
    { "gprs_from_DR1", thread_check_gprs_from_DR1 },
    { "gprs_from_DR2", thread_check_gprs_from_DR2 },
    { "status_reg_from_cache", thread_check_status_reg_from_cache },
    { "status_reg_from_DR", thread_check_status_reg_from_DR },
    { "xsp_from_cache", thread_check_xsp_from_cache },
    { "xsp_from_DR", thread_check_xsp_from_DR },
    { "sigstate_from_handler", thread_check_sigstate_from_handler },
    { "sigstate", thread_check_sigstate },
#endif
};
static const size_t num_tests = sizeof(tests) / sizeof(tests[0]);

THREAD_FUNC_RETURN_TYPE
run_func(void *arg)
{
    void (*asm_func)(void) = (void (*)(void))arg;
    (*asm_func)();
    return THREAD_FUNC_RETURN_ZERO;
}

int
main(int argc, const char **argv)
{
    if (argc < 2) {
        print("invalid args\n");
        return 1;
    }

    print("starting\n");
    for (size_t i = 0; i < num_tests; i++) {
        if (strcmp(argv[1], tests[i].name) == 0) {
            intercept_signal(SIGTERM, (handler_3_t)signal_handler, false);
            detach_state_shared_init();
            (*tests[i].func)();
            detach_state_shared_cleanup();
            print("done\n");
            return 0;
        }
    }

    print("Unrecognised test name \"%s\"\n", argv[1]);
    detach_state_shared_cleanup();

    return 1;
}
