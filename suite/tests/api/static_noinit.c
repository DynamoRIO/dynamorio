/* **********************************************************
 * Copyright (c) 2016 Google, Inc.  All rights reserved.
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

/* Ensures that static DR can operate with a client who does not have
 * an exported init routine.
 */

#include "configure.h"
#include "dr_api.h"
#include "tools.h"
#include <math.h>

static int num_bbs;

static dr_emit_flags_t
event_bb(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    num_bbs++;
    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
    dr_fprintf(STDERR, "Saw %s bb events\n", num_bbs > 0 ? "some" : "no");
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

int
main(int argc, const char *argv[])
{
    print("pre-DR init\n");
    dr_app_setup();
    assert(!dr_app_running_under_dynamorio());

    print("registering for DR events outside of dr_client_main\n");
    dr_register_bb_event(event_bb);
    dr_register_exit_event(event_exit);
    /* XXX i#975: add some more thorough tests of different events */

    print("pre-DR start\n");
    dr_app_start();
    assert(dr_app_running_under_dynamorio());

    if (do_some_work() < 0)
        print("error in computation\n");

    print("pre-DR stop\n");
    dr_app_stop();
    dr_app_cleanup();
    print("all done\n");
    return 0;
}
