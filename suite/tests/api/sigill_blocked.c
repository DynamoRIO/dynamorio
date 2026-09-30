/* ****************************************************
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 * ***************************************************/

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

/* This test case validates the behaviour enabled by the
 * -attach_unmask_suspend_signal option, which for internal attach, uses
 *  ptrace to take over threads masking SIGILL or are blocked in one of the
 *  sigwait syscalls. Two threads are created:
 * - sig_thread: all signals are masked and blocked and thread waits in
 *               sigwaitinfo().
 * - busy_thread: uses a distinctive signal mask which does not include SIGILL
 *                and runs without being blocked in any sigwait syscall.
 *
 * The test also checks the threads' original signal masks are restored after
 * attach.
 */

#include "configure.h"
#include "dr_api.h"
#include "tools.h"
#include "condvar.h"
#include "thread.h"

#include <errno.h>
#include <signal.h>
#ifdef LINUX
#    include <sys/syscall.h>
#    include <unistd.h>
#endif

/* Synchronisation variables. */
static void *signals_blocked;
static void *busy_started;
static void *sigusr1_received;
static void *dr_stopped;
static void *busy_mask_checked;

/* The main() thread writes to busy_check_mask while busy_thread() reads it. We
 * cannot use a condition variable to synchronize because it would make the
 * busy_thread() block instead of remaining busy, so use atomic release/store
 * and acquire/load.
 */
static bool busy_check_mask;
static pid_t busy_tid;

/* Test threads' signal mask flags to indicate if they have been restored
 * correctly after attach. Although busy_thread() uses the normal signal based
 * attach (rather than ptrace assisted attach), we should still check that
 * enabling -attach_unmask_suspend_signal does not corrupt its mask.
 */
static bool sig_mask_attached_equal;
static bool sig_mask_detached_equal;
static bool busy_mask_attached_equal;
static bool busy_mask_detached_equal;

static bool
are_signal_masks_equal(const sigset_t *orig, const sigset_t *curr, int line)
{
    bool equal = true;

    for (int sig = 1; sig <= SIGRTMAX; ++sig) {
        int orig_member = sigismember(orig, sig);
        int curr_member = sigismember(curr, sig);
        assert(orig_member != -1);
        assert(curr_member != -1);
        if (orig_member != curr_member) {
            print("signal mask mismatch at line %d for signal %d: original=%s, "
                  "current=%s\n",
                  line, sig, orig_member ? "blocked" : "unblocked",
                  curr_member ? "blocked" : "unblocked");
            equal = false;
        }
    }
    return equal;
}

static THREAD_FUNC_RETURN_TYPE
sig_thread(void *arg)
{
    sigset_t set;
    sigset_t original_mask;
    sigset_t current_mask;
    siginfo_t info;
    int res;

    sigfillset(&set);
    res = pthread_sigmask(SIG_BLOCK, &set, NULL);
    assert(res == 0);
    res = pthread_sigmask(SIG_SETMASK, NULL, &original_mask);
    assert(res == 0);
    assert(sigismember(&original_mask, SIGILL) == 1);

    print("sig_thread blocked signals\n");
    signal_cond_var(signals_blocked);

    while (true) {
        res = sigwaitinfo(&set, &info);
        if (res == -1) {
            if (errno == EINTR)
                continue;
        }
        assert(res != -1);
        if (res == SIGUSR1) {
            print("sig_thread received SIGUSR1\n");
            res = pthread_sigmask(SIG_SETMASK, NULL, &current_mask);
            assert(res == 0);
            sig_mask_attached_equal =
                are_signal_masks_equal(&original_mask, &current_mask, __LINE__);
            signal_cond_var(sigusr1_received);
            break;
        }
    }

    /* Wait until detach is complete, then verify that attach and detach
     * preserved the thread's original signal mask.
     */
    wait_cond_var(dr_stopped);
    res = pthread_sigmask(SIG_SETMASK, NULL, &current_mask);
    assert(res == 0);
    sig_mask_detached_equal =
        are_signal_masks_equal(&original_mask, &current_mask, __LINE__);
    return NULL;
}

static THREAD_FUNC_RETURN_TYPE
busy_thread(void *arg)
{
    sigset_t set;
    sigset_t original_mask;
    sigset_t current_mask;
    int res;

    sigemptyset(&set);
    sigaddset(&set, SIGUSR2);
    res = pthread_sigmask(SIG_SETMASK, &set, NULL);
    assert(res == 0);
    res = pthread_sigmask(SIG_SETMASK, NULL, &original_mask);
    assert(res == 0);
    assert(sigismember(&original_mask, SIGUSR2) == 1);
    assert(sigismember(&original_mask, SIGILL) == 0);
#ifdef LINUX
    busy_tid = (pid_t)syscall(SYS_gettid);
    print("busy_thread starting (tid=%d)\n", busy_tid);
#else
    print("busy_thread starting\n");
#endif
    signal_cond_var(busy_started);
    while (!__atomic_load_n(&busy_check_mask, __ATOMIC_ACQUIRE)) {
        thread_yield();
    }
    res = pthread_sigmask(SIG_SETMASK, NULL, &current_mask);
    assert(res == 0);
    busy_mask_attached_equal =
        are_signal_masks_equal(&original_mask, &current_mask, __LINE__);
    signal_cond_var(busy_mask_checked);

    wait_cond_var(dr_stopped);
    res = pthread_sigmask(SIG_SETMASK, NULL, &current_mask);
    assert(res == 0);
    busy_mask_detached_equal =
        are_signal_masks_equal(&original_mask, &current_mask, __LINE__);
    print("busy_thread exiting\n");
    return NULL;
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    print("in dr_client_main\n");
}

int
main(int argc, const char *argv[])
{
    signals_blocked = create_cond_var();
    busy_started = create_cond_var();
    busy_mask_checked = create_cond_var();
    sigusr1_received = create_cond_var();
    dr_stopped = create_cond_var();

    print("starting busy_thread\n");
    thread_t busy = create_thread(busy_thread, NULL);
    wait_cond_var(busy_started);

    print("starting sig_thread\n");
    thread_t thread = create_thread(sig_thread, NULL);

    wait_cond_var(signals_blocked);

    print("pre-DR start\n");
    assert(!dr_app_running_under_dynamorio());
    /* This will hang sig_thread unless ptrace takeover is enabled using the
     * -attach_unmask_suspend_signal option.
     */
    dr_app_setup_and_start();
    assert(dr_app_running_under_dynamorio());

    print("checking busy_thread signal mask\n");
    __atomic_store_n(&busy_check_mask, true, __ATOMIC_RELEASE);
    wait_cond_var(busy_mask_checked);

    print("sending SIGUSR1 to sig_thread\n");
    pthread_kill(thread, SIGUSR1);
    wait_cond_var(sigusr1_received);

    print("pre-DR stop\n");
    dr_app_stop_and_cleanup();
    assert(!dr_app_running_under_dynamorio());
    signal_cond_var(dr_stopped);
    join_thread(busy);
    join_thread(thread);

    assert(busy_mask_attached_equal);
    assert(busy_mask_detached_equal);
    assert(sig_mask_attached_equal);
    assert(sig_mask_detached_equal);

    destroy_cond_var(signals_blocked);
    destroy_cond_var(busy_started);
    destroy_cond_var(busy_mask_checked);
    destroy_cond_var(sigusr1_received);
    destroy_cond_var(dr_stopped);

    print("all done\n");
    return 0;
}
