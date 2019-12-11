/* **********************************************************
 * Copyright (c) 2016-2019 Google, Inc.  All rights reserved.
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

/* Ensures that static DR can operate with no client at all */

#include "configure.h"
#include "dr_api.h"
#include "tools.h"
#include <assert.h>
#include <math.h>

static int
do_some_work(int seed)
{
    static int iters = 8192;
    int i;
    double val = seed;
    for (i = 0; i < iters; ++i) {
        val += sin(val);
    }
    return (val > 0);
}

static void
test_static_decode_before_attach(void)
{
    /* FIXME i#2040: this hits the app_fls_data assert on Windows. */
#ifdef UNIX
    /* Test restoration of signal state. */
    int res;
    sigset_t mask = {
        0, /* Set padding to 0 so we can use memcmp */
    };
    sigemptyset(&mask);
    sigaddset(&mask, SIGBUS);
    sigaddset(&mask, SIGUSR2);
    res = sigprocmask(SIG_BLOCK, &mask, NULL);
    assert(res == 0);
    intercept_signal(SIGUSR1, (handler_3_t)SIG_IGN, false);
    intercept_signal(SIGBUS, (handler_3_t)SIG_IGN, false);

    /* We test using DR IR routines when statically linked.  We can't use
     * drdecode when statically linked with DR as drdecode relies on symbol
     * replacement, so instead we initialize DR and then "detach" to do a full
     * cleanup (even without an attach) before starting our regular
     * attach+detach testing.
     * XXX: When there's a client, this requires a flag to skip the client init
     * in this first dr_app_setup().
     */
    dr_standalone_init();
    instr_t *instr = XINST_CREATE_return(GLOBAL_DCONTEXT);
    assert(instr_is_return(instr));
    instr_destroy(GLOBAL_DCONTEXT, instr);
    dr_standalone_exit();

    sigset_t check_mask = {
        0, /* Set padding to 0 so we can use memcmp */
    };
    res = sigprocmask(SIG_BLOCK, NULL, &check_mask);
    assert(res == 0 && memcmp(&mask, &check_mask, sizeof(mask)) == 0);
    struct sigaction act;
    res = sigaction(SIGUSR1, NULL, &act);
    assert(res == 0 && (handler_3_t)act.sa_sigaction == (handler_3_t)SIG_IGN);
    res = sigaction(SIGBUS, NULL, &act);
    assert(res == 0 && (handler_3_t)act.sa_sigaction == (handler_3_t)SIG_IGN);
#endif
}

int
main(int argc, const char *argv[])
{
    test_static_decode_before_attach();

    print("pre-DR init\n");
    dr_app_setup();
    assert(!dr_app_running_under_dynamorio());

    print("pre-DR start\n");
    dr_app_start();
    assert(dr_app_running_under_dynamorio());

    if (do_some_work(argc) < 0)
        print("error in computation\n");

    print("pre-DR stop\n");
    dr_app_stop();
    dr_app_cleanup();
    print("all done\n");
    return 0;
}
