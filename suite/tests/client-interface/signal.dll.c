/* **********************************************************
 * Copyright (c) 2017 Google, Inc.  All rights reserved.
 * Copyright (c) 2009-2010 VMware, Inc.  All rights reserved.
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
 * API regression test for signal event
 */

#include "dr_api.h"
#include "client_tools.h"
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

static app_pc redirect_tag;

static void *child_alive;
static void *child_dead;
static void *sigchld_received;
static pid_t child_pid, child_tid;

static
dr_signal_action_t signal_event(void *dcontext, dr_siginfo_t *info)
{
    static int count = -1;
    count++;
    dr_fprintf(STDERR, "signal event %d sig=%d\n", count, info->sig);

    if (info->sig == SIGURG) {
        static int count_urg = -1;
        count_urg++;
        switch (count_urg) {
        /* test delayable signal with each return value */
        case 0: return DR_SIGNAL_DELIVER;
        case 1: return DR_SIGNAL_SUPPRESS;
        case 2: return DR_SIGNAL_BYPASS;
        case 3: return DR_SIGNAL_DELIVER;
        case 4: return DR_SIGNAL_SUPPRESS;
        case 5: return DR_SIGNAL_BYPASS;
        default: dr_fprintf(STDERR, "too many SIGURG\n");
        }
    } else if (info->sig == SIGTERM) {
        return DR_SIGNAL_SUPPRESS;
    } else if (info->sig == SIGUSR2) {
        ASSERT(redirect_tag != NULL);
        info->mcontext->pc = redirect_tag;
        return DR_SIGNAL_REDIRECT;
    } else if (info->sig == SIGSEGV) {
        /* test non-delayable signal */
        static int count_segv = -1;
        count_segv++;
        if (count_segv == 0)
            return DR_SIGNAL_SUPPRESS;
        else {
            /* test mcontext changes on delivery.
             * fix up to avoid crash on re-execution.
             */
            info->mcontext->xax = info->mcontext->xcx;
            return DR_SIGNAL_DELIVER;
        }
    } else if (info->sig == SIGCHLD) {
        dr_event_signal(sigchld_received);
    }

    return DR_SIGNAL_DELIVER;
}

static dr_emit_flags_t
bb_event(void *drcontext, void* tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr, *next_instr, *next_next_instr;
    /* Look for 3 nops to locate redirection target */
    for (instr = instrlist_first(bb); instr != NULL; instr = next_instr) {
        next_instr = instr_get_next(instr);
        if (next_instr != NULL)
            next_next_instr = instr_get_next(next_instr);
        else
            next_next_instr = NULL;
        if (instr_is_nop(instr) &&
            next_instr != NULL && instr_is_nop(next_instr) &&
            next_next_instr != NULL && instr_is_call_direct(next_next_instr)) {

            redirect_tag = tag;
            break;
        }
    }
    return DR_EMIT_DEFAULT;
}

static void
thread_func(void *arg)
{
    int fd = (int)(long) arg;
    char buf[16];
    child_pid = getpid();
    child_tid = syscall(SYS_gettid);
    dr_event_signal(child_alive);
    dr_mark_safe_to_suspend(dr_get_current_drcontext(), true);
    int res = read(fd, buf, 2);
    if (res < 0)
        perror("error during read");
    else
        dr_fprintf(STDERR, "got %d bytes == %d %d\n", res, buf[0], buf[1]);
    dr_mark_safe_to_suspend(dr_get_current_drcontext(), false);
    close(fd);
    dr_event_signal(child_dead);
}

DR_EXPORT
void dr_init(client_id_t id)
{
    dr_register_bb_event(bb_event);
    dr_register_signal_event(signal_event);

    /* We test syscall auto-restart (i#2659) by having another thread
     * sit at a blocking read while it receives signals.
     * It's hard to arrange this w/ an app thread and app signals so we use a
     * client thread and direct signals.
     */
    child_alive = dr_event_create();
    child_dead = dr_event_create();
    sigchld_received = dr_event_create();
#ifdef LINUX
    int pipefd[2];
    int res = pipe(pipefd);
    ASSERT(res != -1);
    bool success = dr_create_client_thread(thread_func, (void*)(long)pipefd[0]);
    ASSERT(success);
    dr_event_wait(child_alive);
    /* XXX: there's no easy race-free solution here: we need the thread to
     * be inside the read().
     */
    sleep(1);
    /* Send a default-ignore signal */
    res = syscall(SYS_tgkill, child_pid, child_tid, SIGCHLD);
    ASSERT(res == 0);
    dr_event_wait(sigchld_received);
    /* Now finish up */
    write(pipefd[1], "ab", 2);
    close(pipefd[1]);
    dr_event_wait(child_dead);
#elif defined(MACOS)
    /* FIXME i#58: dr_create_client_thread is NYI, and we need the
     * thread port to use SYS___pthread_kill.
     */
    kill(getpid(), SIGCHLD);
#else
# error Unsupported OS
#endif
    dr_event_destroy(child_alive);
    dr_event_destroy(child_dead);
    dr_event_destroy(sigchld_received);
}
