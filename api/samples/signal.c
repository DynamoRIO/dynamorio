/* **********************************************************
 * Copyright (c) 2014-2017 Google, Inc.  All rights reserved.
 * Copyright (c) 2009 VMware, Inc.  All rights reserved.
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

/* Code Manipulation API Sample:
 * signal.c
 *
 * Monitors application signals.
 */

#include "dr_api.h"
#include "drmgr.h"
#include <signal.h>

#ifdef WINDOWS
#    define DISPLAY_STRING(msg) dr_messagebox(msg)
#else
#    define DISPLAY_STRING(msg) dr_printf("%s\n", msg);
#endif

static int num_signals;

static void
event_exit(void);
#ifdef UNIX
static dr_signal_action_t
event_signal(void *drcontext, dr_siginfo_t *info);
#endif

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO Sample Client 'signal'", "http://dynamorio.org/issues");
    drmgr_init();
#ifdef UNIX
    drmgr_register_signal_event(event_signal);
#endif
    dr_register_exit_event(event_exit);
#ifdef SHOW_RESULTS
    if (dr_is_notify_on()) {
#    ifdef WINDOWS
        /* ask for best-effort printing to cmd window.  must be called at init. */
        dr_enable_console_printing();
#    endif
        dr_fprintf(STDERR, "Client signal is running\n");
    }
#endif
}

static void
event_exit(void)
{
#ifdef SHOW_RESULTS
    char msg[512];
    int len;
    /* Note that using %f with dr_printf or dr_fprintf on Windows will print
     * garbage as they use ntdll._vsnprintf, so we must use dr_snprintf.
     */
    len = dr_snprintf(msg, sizeof(msg) / sizeof(msg[0]), "<Number of signals seen: %d>",
                      num_signals);
    DR_ASSERT(len > 0);
    msg[sizeof(msg) / sizeof(msg[0]) - 1] = '\0';
    DISPLAY_STRING(msg);
#endif /* SHOW_RESULTS */
    drmgr_exit();
}

#ifdef UNIX
static dr_signal_action_t
event_signal(void *drcontext, dr_siginfo_t *info)
{
    dr_atomic_add32_return_sum(&num_signals, 1);
    if (info->sig == SIGTERM) {
        /* Ignore TERM */
        return DR_SIGNAL_SUPPRESS;
    } else if (info->sig == SIGSEGV) {
        /* Skip the faulting instruction.  This is a sample only! */
        app_pc pc = decode_next_pc(drcontext, info->mcontext->pc);
        if (pc != NULL)
            info->mcontext->pc = pc;
        return DR_SIGNAL_REDIRECT;
    }

    return DR_SIGNAL_DELIVER;
}
#endif
