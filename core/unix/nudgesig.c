/* **********************************************************
 * Copyright (c) 2012-2022 Google, Inc.  All rights reserved.
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

/*
 * nudgesig.c - nudge signal sending code
 *
 * shared between core & tools/nudgeunix.c
 */

#include "configure.h"
#include <unistd.h>
#include "include/syscall.h"
#include "include/siginfo.h"

#include "globals_shared.h"
#ifndef NOT_DYNAMORIO_CORE
#    include "../globals.h" /* for arch_exports.h for dynamorio_syscall */
#else
#    include <string.h> /* for memset */
#endif

/* shared with tools/nudgeunix.c */
bool
create_nudge_signal_payload(kernel_siginfo_t *info OUT, uint action_mask, uint flags,
                            client_id_t client_id, uint64 client_arg)
{
    nudge_arg_t *arg;

    memset(info, 0, sizeof(*info));
    info->si_signo = NUDGESIG_SIGNUM;
    info->si_code = SI_QUEUE;

    arg = (nudge_arg_t *)info;
    arg->version = NUDGE_ARG_CURRENT_VERSION;
    arg->nudge_action_mask = action_mask;
    /* We only have 2 bits for flags. */
    if (flags >= 4)
        return false;
    arg->flags = flags;
    arg->client_id = client_id;
    arg->client_arg = client_arg;

    /* ensure nudge_arg_t overlays how we expect it to */
    if (info->si_signo != NUDGESIG_SIGNUM || info->si_code != SI_QUEUE ||
        info->si_errno == 0)
        return false;

    return true;
}

#ifndef NOT_DYNAMORIO_CORE
bool
send_nudge_signal(process_id_t pid, uint action_mask, client_id_t client_id,
                  uint64 client_arg)
{
    kernel_siginfo_t info;
    int res;
    if (!create_nudge_signal_payload(&info, action_mask, 0, client_id, client_arg))
        return false;
    res = dynamorio_syscall(SYS_rt_sigqueueinfo, 3, pid, NUDGESIG_SIGNUM, &info);
    return (res >= 0);
}
#endif /* !NOT_DYNAMORIO_CORE */
