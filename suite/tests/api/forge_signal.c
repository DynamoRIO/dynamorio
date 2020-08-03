/* **********************************************************
 * Copyright (c) 2020 ARM Limited. All rights reserved.
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
 * * Neither the name of ARM Limited nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL ARM LIMITED OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "dr_api.h"

#include <assert.h>
#include <signal.h>
#include <string.h>

static void
clean_callee(app_pc pc)
{
    int sig = SIGUSR2;
    dr_fprintf(STDERR, "forging signal %d\n", sig);
    dr_mcontext_t mc = {
        .size = sizeof(mc),
        .flags = DR_MC_ALL
    };
    dr_get_mcontext(dr_get_current_drcontext(), &mc);
    dr_forge_signal(pc, sig, &mc);
}

static dr_emit_flags_t
event_bb(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr = instrlist_last(bb);
    app_pc pc = instr_get_app_pc(instr);
    dr_insert_clean_call(drcontext, bb, instr, clean_callee, false, 1,
        OPND_CREATE_INT64(pc));
    dr_fprintf(STDERR, "inserted clean call\n");
    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
    dr_fprintf(STDERR, "exit event\n");
}

static dr_signal_action_t
event_signal(void *drcontext, dr_siginfo_t *info)
{
    dr_fprintf(STDERR, "received signal %d (%s)\n", info->sig, strsignal(info->sig));
    return DR_SIGNAL_DELIVER;
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_fprintf(STDERR, "client main\n");
    dr_register_bb_event(event_bb);
    dr_register_signal_event(event_signal);
    dr_register_exit_event(event_exit);
}

static int
do_some_work(void)
{
    double val = 0;
    return (val > 0);
}

int
main(int argc, const char *argv[])
{
    dr_app_setup();
    assert(!dr_app_running_under_dynamorio());
    dr_fprintf(STDERR, "hello signal\n");
    dr_app_start();
    assert(dr_app_running_under_dynamorio());
    do_some_work();
    dr_app_stop_and_cleanup();
    assert(!dr_app_running_under_dynamorio());
    return 0;
}
