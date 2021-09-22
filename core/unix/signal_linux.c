/* **********************************************************
 * Copyright (c) 2011-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2000-2001 Hewlett-Packard Company */

/*
 * signal_linux.c - Linux-specific signal code
 */

#include "signal_private.h" /* pulls in globals.h for us, in right order */

#ifndef LINUX
#    error Linux-only
#endif

#include "arch.h"
#include "../hashtable.h"
#include "include/syscall.h"

#include <errno.h>
#undef errno

/* important reference files:
 *   /usr/include/asm/signal.h
 *   /usr/include/asm/siginfo.h
 *   /usr/include/asm/ucontext.h
 *   /usr/include/asm/sigcontext.h
 *   /usr/include/sys/ucontext.h (see notes below...asm one is more useful)
 *   /usr/include/bits/sigaction.h
 *   /usr/src/linux/kernel/signal.c
 *   /usr/src/linux/arch/i386/kernel/signal.c
 *   /usr/src/linux/include/asm-i386/signal.h
 *   /usr/src/linux/include/asm-i386/sigcontext.h
 *   /usr/src/linux/include/asm-i386/ucontext.h
 *   /usr/src/linux/include/linux/signal.h
 *   /usr/src/linux/include/linux/sched.h (sas_ss_flags, on_sig_stack)
 * ditto with x86_64, plus:
 *   /usr/src/linux/arch/x86_64/ia32/ia32_signal.c
 */

int default_action[] = {
    /* nothing    0 */ DEFAULT_IGNORE,
    /* SIGHUP     1 */ DEFAULT_TERMINATE,
    /* SIGINT     2 */ DEFAULT_TERMINATE,
    /* SIGQUIT    3 */ DEFAULT_TERMINATE_CORE,
    /* SIGILL     4 */ DEFAULT_TERMINATE_CORE,
    /* SIGTRAP    5 */ DEFAULT_TERMINATE_CORE,
    /* SIGABRT/SIGIOT 6 */ DEFAULT_TERMINATE_CORE,
    /* SIGBUS     7 */ DEFAULT_TERMINATE, /* should be CORE */
    /* SIGFPE     8 */ DEFAULT_TERMINATE_CORE,
    /* SIGKILL    9 */ DEFAULT_TERMINATE,
    /* SIGUSR1   10 */ DEFAULT_TERMINATE,
    /* SIGSEGV   11 */ DEFAULT_TERMINATE_CORE,
    /* SIGUSR2   12 */ DEFAULT_TERMINATE,
    /* SIGPIPE   13 */ DEFAULT_TERMINATE,
    /* SIGALRM   14 */ DEFAULT_TERMINATE,
    /* SIGTERM   15 */ DEFAULT_TERMINATE,
    /* SIGSTKFLT 16 */ DEFAULT_TERMINATE,
    /* SIGCHLD   17 */ DEFAULT_IGNORE,
    /* SIGCONT   18 */ DEFAULT_CONTINUE,
    /* SIGSTOP   19 */ DEFAULT_STOP,
    /* SIGTSTP   20 */ DEFAULT_STOP,
    /* SIGTTIN   21 */ DEFAULT_STOP,
    /* SIGTTOU   22 */ DEFAULT_STOP,
    /* SIGURG    23 */ DEFAULT_IGNORE,
    /* SIGXCPU   24 */ DEFAULT_TERMINATE,
    /* SIGXFSZ   25 */ DEFAULT_TERMINATE,
    /* SIGVTALRM 26 */ DEFAULT_TERMINATE,
    /* SIGPROF   27 */ DEFAULT_TERMINATE,
    /* SIGWINCH  28 */ DEFAULT_IGNORE,
    /* SIGIO/SIGPOLL/SIGLOST 29 */ DEFAULT_TERMINATE,
    /* SIGPWR    30 */ DEFAULT_TERMINATE,
    /* SIGSYS/SIGUNUSED 31 */ DEFAULT_TERMINATE,

    /* ASSUMPTION: all real-time have default of terminate...XXX: ok? */
    /* 32 */ DEFAULT_TERMINATE,
    /* 33 */ DEFAULT_TERMINATE,
    /* 34 */ DEFAULT_TERMINATE,
    /* 35 */ DEFAULT_TERMINATE,
    /* 36 */ DEFAULT_TERMINATE,
    /* 37 */ DEFAULT_TERMINATE,
    /* 38 */ DEFAULT_TERMINATE,
    /* 39 */ DEFAULT_TERMINATE,
    /* 40 */ DEFAULT_TERMINATE,
    /* 41 */ DEFAULT_TERMINATE,
    /* 42 */ DEFAULT_TERMINATE,
    /* 43 */ DEFAULT_TERMINATE,
    /* 44 */ DEFAULT_TERMINATE,
    /* 45 */ DEFAULT_TERMINATE,
    /* 46 */ DEFAULT_TERMINATE,
    /* 47 */ DEFAULT_TERMINATE,
    /* 48 */ DEFAULT_TERMINATE,
    /* 49 */ DEFAULT_TERMINATE,
    /* 50 */ DEFAULT_TERMINATE,
    /* 51 */ DEFAULT_TERMINATE,
    /* 52 */ DEFAULT_TERMINATE,
    /* 53 */ DEFAULT_TERMINATE,
    /* 54 */ DEFAULT_TERMINATE,
    /* 55 */ DEFAULT_TERMINATE,
    /* 56 */ DEFAULT_TERMINATE,
    /* 57 */ DEFAULT_TERMINATE,
    /* 58 */ DEFAULT_TERMINATE,
    /* 59 */ DEFAULT_TERMINATE,
    /* 60 */ DEFAULT_TERMINATE,
    /* 61 */ DEFAULT_TERMINATE,
    /* 62 */ DEFAULT_TERMINATE,
    /* 63 */ DEFAULT_TERMINATE,
    /* 64 */ DEFAULT_TERMINATE,
};

bool can_always_delay[] = {
    /* nothing    0 */ true,
    /* SIGHUP     1 */ true,
    /* SIGINT     2 */ true,
    /* SIGQUIT    3 */ true,
    /* SIGILL     4 */ false,
    /* SIGTRAP    5 */ false,
    /* SIGABRT/SIGIOT 6 */ false,
    /* SIGBUS     7 */ false,
    /* SIGFPE     8 */ false,
    /* SIGKILL    9 */ true,
    /* SIGUSR1   10 */ true,
    /* SIGSEGV   11 */ false,
    /* SIGUSR2   12 */ true,
    /* SIGPIPE   13 */ false,
    /* SIGALRM   14 */ true,
    /* SIGTERM   15 */ true,
    /* SIGSTKFLT 16 */ false,
    /* SIGCHLD   17 */ true,
    /* SIGCONT   18 */ true,
    /* SIGSTOP   19 */ true,
    /* SIGTSTP   20 */ true,
    /* SIGTTIN   21 */ true,
    /* SIGTTOU   22 */ true,
    /* SIGURG    23 */ true,
    /* SIGXCPU   24 */ false,
    /* SIGXFSZ   25 */ true,
    /* SIGVTALRM 26 */ true,
    /* SIGPROF   27 */ true,
    /* SIGWINCH  28 */ true,
    /* SIGIO/SIGPOLL/SIGLOST 29 */ true,
    /* SIGPWR    30 */ true,
    /* SIGSYS/SIGUNUSED 31 */ false,

    /* ASSUMPTION: all real-time can be delayed */
    /* 32 */ true,
    /* 33 */ true,
    /* 34 */ true,
    /* 35 */ true,
    /* 36 */ true,
    /* 37 */ true,
    /* 38 */ true,
    /* 39 */ true,
    /* 40 */ true,
    /* 41 */ true,
    /* 42 */ true,
    /* 43 */ true,
    /* 44 */ true,
    /* 45 */ true,
    /* 46 */ true,
    /* 47 */ true,
    /* 48 */ true,
    /* 49 */ true,
    /* 50 */ true,
    /* 51 */ true,
    /* 52 */ true,
    /* 53 */ true,
    /* 54 */ true,
    /* 55 */ true,
    /* 56 */ true,
    /* 57 */ true,
    /* 58 */ true,
    /* 59 */ true,
    /* 60 */ true,
    /* 61 */ true,
    /* 62 */ true,
    /* 63 */ true,
    /* 64 */ true,
};

bool
sysnum_is_not_restartable(int sysnum)
{
    /* Check the list of non-restartable syscalls.
     * Since we only check the number, we're inaccurate!
     * We err on the side of thinking more things are non-restartable
     * than actually are, as this is only really used for inserting
     * nops to ensure post-syscall points are safe spots, and too many
     * nops is better than too few.
     * We're missing:
     * + SYS_read from an inotify file descriptor.
     * We're overly aggressive on:
     * + Socket interfaces: supposed to restart if no timeout has been set.
     */
    return (
#ifdef SYS_pause
        sysnum == SYS_pause ||
#endif
        sysnum == SYS_rt_sigsuspend || sysnum == SYS_rt_sigtimedwait ||
        IF_X86_64(sysnum == SYS_epoll_wait_old ||)
#ifdef SYS_epoll_wait
                sysnum == SYS_epoll_wait ||
#endif
        sysnum == SYS_epoll_pwait ||
#ifdef SYS_poll
        sysnum == SYS_poll ||
#endif
        sysnum == SYS_ppoll || IF_X86(sysnum == SYS_select ||) sysnum == SYS_pselect6 ||
#ifdef X64
        sysnum == SYS_msgrcv || sysnum == SYS_msgsnd || sysnum == SYS_semop ||
        sysnum == SYS_semtimedop ||
        /* XXX: these should be restarted if there's no timeout */
        sysnum == SYS_accept || sysnum == SYS_accept4 || sysnum == SYS_recvfrom ||
        sysnum == SYS_recvmsg || sysnum == SYS_recvmmsg || sysnum == SYS_connect ||
        sysnum == SYS_sendto || sysnum == SYS_sendmmsg || sysnum == SYS_sendfile ||
#elif !defined(ARM)
        /* XXX: some should be restarted if there's no timeout */
        sysnum == SYS_ipc ||
#endif
        sysnum == SYS_clock_nanosleep || sysnum == SYS_nanosleep ||
        sysnum == SYS_io_getevents);
}

/***************************************************************************
 * SIGNALFD
 */

/* Strategy: a real signalfd is a read-only file, so we can't write to one to
 * emulate signal delivery.  We also can't block signals we care about (and
 * for clients we don't want to block anything).  Thus we must emulate
 * signalfd via a pipe.  The kernel's pipe buffer should easily hold
 * even a big queue of RT signals.  Xref i#1138.
 *
 * Although signals are per-thread, fds are global, and one thread
 * could use a signalfd to see signals on another thread.
 *
 * Thus we have:
 * + global data struct "sigfd_pipe_t" stores pipe write fd and refcount
 * + global hashtable mapping read fd to sigfd_pipe_t
 * + thread has array of pointers to sigfd_pipe_t, one per signum
 * + on SYS_close, we decrement refcount
 * + on SYS_dup*, we add a new hashtable entry
 *
 * This pipe implementation has a hole: it cannot properly handle two
 * signalfds with different but overlapping signal masks (i#1189: see below).
 */
static generic_table_t *sigfd_table;

struct _sigfd_pipe_t {
    file_t write_fd;
    file_t read_fd;
    uint refcount;
    dcontext_t *dcontext;
};

static void
sigfd_pipe_free(dcontext_t *dcontext, void *ptr)
{
    sigfd_pipe_t *pipe = (sigfd_pipe_t *)ptr;
    ASSERT(pipe->refcount > 0);
    pipe->refcount--;
    if (pipe->refcount == 0) {
        if (pipe->dcontext != NULL) {
            /* Update the owning thread's info.
             * We write a NULL which is atomic.
             * The thread on exit grabs the table lock for synch and clears dcontext.
             */
            thread_sig_info_t *info = (thread_sig_info_t *)pipe->dcontext->signal_field;
            int sig;
            for (sig = 1; sig <= MAX_SIGNUM; sig++) {
                if (info->signalfd[sig] == pipe)
                    info->signalfd[sig] = NULL;
            }
        }
        os_close_protected(pipe->write_fd);
        os_close_protected(pipe->read_fd);
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, pipe, sigfd_pipe_t, ACCT_OTHER, PROTECTED);
    }
}

void
signalfd_init(void)
{
#define SIGNALFD_HTABLE_INIT_SIZE 6
    sigfd_table =
        generic_hash_create(GLOBAL_DCONTEXT, SIGNALFD_HTABLE_INIT_SIZE,
                            80 /* load factor: not perf-critical */,
                            HASHTABLE_ENTRY_SHARED | HASHTABLE_SHARED |
                                HASHTABLE_PERSISTENT | HASHTABLE_RELAX_CLUSTER_CHECKS,
                            sigfd_pipe_free _IF_DEBUG("signalfd table"));
    /* XXX: we need our lock rank to be higher than fd_table's so we
     * can call os_close_protected() when freeing.  We should
     * parametrize the generic table rank.  For now we just change it afterward
     * (we'll have issues if we ever call _resurrect):
     */
    ASSIGN_INIT_READWRITE_LOCK_FREE(sigfd_table->rwlock, sigfdtable_lock);
}

void
signalfd_exit(void)
{
    generic_hash_destroy(GLOBAL_DCONTEXT, sigfd_table);
}

void
signalfd_thread_exit(dcontext_t *dcontext, thread_sig_info_t *info)
{
    /* We don't free the pipe until the app closes its fd's but we need to
     * update the dcontext in the global data
     */
    int sig;
    TABLE_RWLOCK(sigfd_table, write, lock);
    for (sig = 1; sig <= MAX_SIGNUM; sig++) {
        if (info->signalfd[sig] != NULL)
            info->signalfd[sig]->dcontext = NULL;
    }
    TABLE_RWLOCK(sigfd_table, write, unlock);
}

bool
handle_pre_extended_syscall_sigmasks(dcontext_t *dcontext, kernel_sigset_t *sigmask,
                                     size_t sizemask, bool *pending)
{
    thread_sig_info_t *info = (thread_sig_info_t *)dcontext->signal_field;

    /* XXX i#2311, #3240: We may currently deliver incorrect signals, because the
     * native sigprocmask the system call may get interrupted by may not be the same
     * as the native app expects. In addition to this, the p* variants of above syscalls
     * are not properly emulated w.r.t. their atomicity setting the sigprocmask and
     * executing the syscall.
     */
    *pending = false;
    if (sizemask != sizeof(kernel_sigset_t))
        return false;
    ASSERT(sigmask != NULL);
    ASSERT(!info->pre_syscall_app_sigprocmask_valid);
    info->pre_syscall_app_sigprocmask_valid = true;
    info->pre_syscall_app_sigprocmask = info->app_sigblocked;
    signal_set_mask(dcontext, sigmask);
    /* Make sure we deliver pending signals that are now unblocked. */
    check_signals_pending(dcontext, info);
    *pending = dcontext->signals_pending;
    return true;
}

void
handle_post_extended_syscall_sigmasks(dcontext_t *dcontext, bool success)
{
    thread_sig_info_t *info = (thread_sig_info_t *)dcontext->signal_field;
    ASSERT(info->pre_syscall_app_sigprocmask_valid);
    /* We restore the mask here *before* we make it back to dispatch for
     * receive_pending_signal().  We rely on sigpending_t.unblocked_at_receipt
     * to deliver the signal ignoring the now-restored mask.
     */
    info->pre_syscall_app_sigprocmask_valid = false;
    signal_set_mask(dcontext, &info->pre_syscall_app_sigprocmask);
}

ptr_int_t
handle_pre_signalfd(dcontext_t *dcontext, int fd, kernel_sigset_t *mask, size_t sizemask,
                    int flags)
{
    thread_sig_info_t *info = (thread_sig_info_t *)dcontext->signal_field;
    int sig;
    kernel_sigset_t local_set;
    kernel_sigset_t *set;
    ptr_int_t retval = -1;
    sigfd_pipe_t *pipe = NULL;
    LOG(THREAD, LOG_ASYNCH, 2, "%s: fd=%d, flags=0x%x\n", __FUNCTION__, fd, flags);
    if (sizemask == sizeof(sigset_t)) {
        copy_sigset_to_kernel_sigset((sigset_t *)mask, &local_set);
        set = &local_set;
    } else {
        ASSERT(sizemask == sizeof(kernel_sigset_t));
        set = mask;
    }
    if (fd != -1) {
        TABLE_RWLOCK(sigfd_table, read, lock);
        pipe = (sigfd_pipe_t *)generic_hash_lookup(GLOBAL_DCONTEXT, sigfd_table, fd);
        TABLE_RWLOCK(sigfd_table, read, unlock);
        if (pipe == NULL)
            return -EINVAL;
    } else {
        /* FIXME i#1189: currently we do not properly handle two signalfds with
         * different but overlapping signal masks, as we do not monitor the
         * read/poll syscalls and thus cannot provide a set of pipes that
         * matches the two signal sets.  For now we err on the side of sending
         * too many signals and simply conflate such sets into a single pipe.
         */
        for (sig = 1; sig <= MAX_SIGNUM; sig++) {
            if (sig == SIGKILL || sig == SIGSTOP)
                continue;
            if (kernel_sigismember(set, sig) && info->signalfd[sig] != NULL) {
                pipe = info->signalfd[sig];
                retval = dup_syscall(pipe->read_fd);
                break;
            }
        }
    }
    if (pipe == NULL) {
        int fds[2];
        /* SYS_signalfd is even newer than SYS_pipe2, so pipe2 must be avail.
         * We pass the flags through b/c the same ones (SFD_NONBLOCK ==
         * O_NONBLOCK, SFD_CLOEXEC == O_CLOEXEC) are accepted by pipe.
         */
        ptr_int_t res = dynamorio_syscall(SYS_pipe2, 2, fds, flags);
        if (res < 0)
            return res;
        pipe = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, sigfd_pipe_t, ACCT_OTHER, PROTECTED);
        pipe->dcontext = dcontext;
        pipe->refcount = 1;

        /* Keep our write fd in the private fd space */
        pipe->write_fd = fd_priv_dup(fds[1]);
        os_close(fds[1]);
        if (TEST(SFD_CLOEXEC, flags))
            fd_mark_close_on_exec(pipe->write_fd);
        fd_table_add(pipe->write_fd, 0 /*keep across fork*/);

        /* We need an un-closable copy of the read fd in case we need to dup it */
        pipe->read_fd = fd_priv_dup(fds[0]);
        if (TEST(SFD_CLOEXEC, flags))
            fd_mark_close_on_exec(pipe->read_fd);
        fd_table_add(pipe->read_fd, 0 /*keep across fork*/);

        TABLE_RWLOCK(sigfd_table, write, lock);
        generic_hash_add(GLOBAL_DCONTEXT, sigfd_table, fds[0], (void *)pipe);
        TABLE_RWLOCK(sigfd_table, write, unlock);

        LOG(THREAD, LOG_ASYNCH, 2, "created signalfd pipe app r=%d DR r=%d w=%d\n",
            fds[0], pipe->read_fd, pipe->write_fd);
        retval = fds[0];
    }
    for (sig = 1; sig <= MAX_SIGNUM; sig++) {
        if (sig == SIGKILL || sig == SIGSTOP)
            continue;
        if (kernel_sigismember(set, sig)) {
            if (info->signalfd[sig] == NULL)
                info->signalfd[sig] = pipe;
            else
                ASSERT(info->signalfd[sig] == pipe);
            LOG(THREAD, LOG_ASYNCH, 2, "adding signalfd pipe %d for signal %d\n",
                pipe->write_fd, sig);
        } else if (info->signalfd[sig] != NULL) {
            info->signalfd[sig] = NULL;
            LOG(THREAD, LOG_ASYNCH, 2, "removing signalfd pipe=%d for signal %d\n",
                pipe->write_fd, sig);
        }
    }
    return retval;
}

bool
notify_signalfd(dcontext_t *dcontext, thread_sig_info_t *info, int sig,
                sigframe_rt_t *frame)
{
    sigfd_pipe_t *pipe = info->signalfd[sig];
    if (pipe != NULL) {
        int res;
        struct signalfd_siginfo towrite = {
            0,
        };

        /* XXX: we should limit to a single non-RT signal until it's read (by
         * polling pipe->read_fd to see whether it has data), except we delay
         * signals and thus do want to accumulate multiple non-RT to some extent.
         * For now we go ahead and treat RT and non-RT the same.
         */
        towrite.ssi_signo = sig;
        towrite.ssi_errno = frame->info.si_errno;
        towrite.ssi_code = frame->info.si_code;
        towrite.ssi_pid = frame->info.si_pid;
        towrite.ssi_uid = frame->info.si_uid;
        towrite.ssi_fd = frame->info.si_fd;
        towrite.ssi_band = frame->info.si_band;
        towrite.ssi_tid = frame->info.si_timerid;
        towrite.ssi_overrun = frame->info.si_overrun;
        towrite.ssi_status = frame->info.si_status;
        towrite.ssi_utime = frame->info.si_utime;
        towrite.ssi_stime = frame->info.si_stime;
#ifdef __ARCH_SI_TRAPNO /* XXX: should update include/siginfo.h */
        towrite.ssi_trapno = frame->info.si_trapno;
#endif
        towrite.ssi_addr = (ptr_int_t)frame->info.si_addr;

        /* XXX: if the pipe is full, don't write to it as it could block.  We
         * can poll to determine.  This is quite unlikely (kernel buffer is 64K
         * since 2.6.11) so for now we do not do so.
         */
        res = write_syscall(pipe->write_fd, &towrite, sizeof(towrite));
        LOG(THREAD, LOG_ASYNCH, 2, "writing to signalfd fd=%d for signal %d => %d\n",
            pipe->write_fd, sig, res);
        return true; /* signal consumed */
    }
    return false;
}

void
signal_handle_dup(dcontext_t *dcontext, file_t src, file_t dst)
{
    sigfd_pipe_t *pipe;
    TABLE_RWLOCK(sigfd_table, read, lock);
    pipe = (sigfd_pipe_t *)generic_hash_lookup(GLOBAL_DCONTEXT, sigfd_table, src);
    TABLE_RWLOCK(sigfd_table, read, unlock);
    if (pipe == NULL)
        return;
    TABLE_RWLOCK(sigfd_table, write, lock);
    pipe = (sigfd_pipe_t *)generic_hash_lookup(GLOBAL_DCONTEXT, sigfd_table, src);
    if (pipe != NULL) {
        pipe->refcount++;
        generic_hash_add(GLOBAL_DCONTEXT, sigfd_table, dst, (void *)pipe);
    }
    TABLE_RWLOCK(sigfd_table, write, unlock);
}

void
signal_handle_close(dcontext_t *dcontext, file_t fd)
{
    sigfd_pipe_t *pipe;
    TABLE_RWLOCK(sigfd_table, read, lock);
    pipe = (sigfd_pipe_t *)generic_hash_lookup(GLOBAL_DCONTEXT, sigfd_table, fd);
    TABLE_RWLOCK(sigfd_table, read, unlock);
    if (pipe == NULL)
        return;
    TABLE_RWLOCK(sigfd_table, write, lock);
    pipe = (sigfd_pipe_t *)generic_hash_lookup(GLOBAL_DCONTEXT, sigfd_table, fd);
    if (pipe != NULL) {
        /* this will call sigfd_pipe_free() */
        generic_hash_remove(GLOBAL_DCONTEXT, sigfd_table, fd);
    }
    TABLE_RWLOCK(sigfd_table, write, unlock);
}
