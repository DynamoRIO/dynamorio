/* *******************************************************************************
 * Copyright (c) 2013-2014 Google, Inc.  All rights reserved.
 * *******************************************************************************/

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
 * signal_macos.c - MacOS-specific signal code
 *
 * FIXME i#58: NYI (see comments below as well):
 * + many pieces are not at all implemented, but it should be straightforward
 * + longer-term i#1291: use raw syscalls instead of libSystem wrappers
 */

#include "signal_private.h" /* pulls in globals.h for us, in right order */
#include <sys/syscall.h>

#ifndef MACOS
# error Mac-only
#endif

/* based on xnu bsd/sys/signalvar.h */
int default_action[] = {
    0,                      /*  0 unused */
    DEFAULT_TERMINATE,      /*  1 SIGHUP */
    DEFAULT_TERMINATE,      /*  2 SIGINT */
    DEFAULT_TERMINATE_CORE, /*  3 SIGQUIT */
    DEFAULT_TERMINATE_CORE, /*  4 SIGILL */
    DEFAULT_TERMINATE_CORE, /*  5 SIGTRAP */
    DEFAULT_TERMINATE_CORE, /*  6 SIGABRT/SIGIOT */
    DEFAULT_TERMINATE_CORE, /*  7 SIGEMT/SIGPOLL */
    DEFAULT_TERMINATE_CORE, /*  8 SIGFPE */
    DEFAULT_TERMINATE,      /*  9 SIGKILL */
    DEFAULT_TERMINATE_CORE, /* 10 SIGBUS */
    DEFAULT_TERMINATE_CORE, /* 11 SIGSEGV */
    DEFAULT_TERMINATE_CORE, /* 12 SIGSYS */
    DEFAULT_TERMINATE,      /* 13 SIGPIPE */
    DEFAULT_TERMINATE,      /* 14 SIGALRM */
    DEFAULT_TERMINATE,      /* 15 SIGTERM */
    DEFAULT_IGNORE,         /* 16 SIGURG */
    DEFAULT_STOP,           /* 17 SIGSTOP */
    DEFAULT_STOP,           /* 18 SIGTSTP */
    DEFAULT_CONTINUE,       /* 19 SIGCONT */
    DEFAULT_IGNORE,         /* 20 SIGCHLD */
    DEFAULT_STOP,           /* 21 SIGTTIN */
    DEFAULT_STOP,           /* 22 SIGTTOU */
    DEFAULT_IGNORE,         /* 23 SIGIO */
    DEFAULT_TERMINATE,      /* 24 SIGXCPU */
    DEFAULT_TERMINATE,      /* 25 SIGXFSZ */
    DEFAULT_TERMINATE,      /* 26 SIGVTALRM */
    DEFAULT_TERMINATE,      /* 27 SIGPROF */
    DEFAULT_IGNORE,         /* 28 SIGWINCH  */
    DEFAULT_IGNORE,         /* 29 SIGINFO */
    DEFAULT_TERMINATE,      /* 30 SIGUSR1 */
    DEFAULT_TERMINATE,      /* 31 SIGUSR2 */
    /* no real-time support */
};

bool can_always_delay[] = {
     true, /*  0 unused */
     true, /*  1 SIGHUP */
     true, /*  2 SIGINT */
     true, /*  3 SIGQUIT */
    false, /*  4 SIGILL */
    false, /*  5 SIGTRAP */
    false, /*  6 SIGABRT/SIGIOT */
     true, /*  7 SIGEMT/SIGPOLL */
    false, /*  8 SIGFPE */
     true, /*  9 SIGKILL */
    false, /* 10 SIGBUS */
    false, /* 11 SIGSEGV */
    false, /* 12 SIGSYS */
    false, /* 13 SIGPIPE */
     true, /* 14 SIGALRM */
     true, /* 15 SIGTERM */
     true, /* 16 SIGURG */
     true, /* 17 SIGSTOP */
     true, /* 18 SIGTSTP */
     true, /* 19 SIGCONT */
     true, /* 20 SIGCHLD */
     true, /* 21 SIGTTIN */
     true, /* 22 SIGTTOU */
     true, /* 23 SIGIO */
    false, /* 24 SIGXCPU */
     true, /* 25 SIGXFSZ */
     true, /* 26 SIGVTALRM */
     true, /* 27 SIGPROF */
     true, /* 28 SIGWINCH  */
     true, /* 29 SIGINFO */
     true, /* 30 SIGUSR1 */
     true, /* 31 SIGUSR2 */
    /* no real-time support */
};

bool
sysnum_is_not_restartable(int sysnum)
{
    /* Man page says these are restarted:
     *  The affected system calls include open(2), read(2), write(2),
     *  sendto(2), recvfrom(2), sendmsg(2) and recvmsg(2) on a
     *  communications channel or a slow device (such as a terminal,
     *  but not a regular file) and during a wait(2) or ioctl(2).
     */
    return (sysnum != SYS_open && sysnum != SYS_open_nocancel &&
            sysnum != SYS_read && sysnum != SYS_read_nocancel &&
            sysnum != SYS_write && sysnum != SYS_write_nocancel &&
            sysnum != SYS_sendto && sysnum != SYS_sendto_nocancel &&
            sysnum != SYS_recvfrom && sysnum != SYS_recvfrom_nocancel &&
            sysnum != SYS_sendmsg && sysnum != SYS_sendmsg_nocancel &&
            sysnum != SYS_recvmsg && sysnum != SYS_recvmsg_nocancel &&
            sysnum != SYS_wait4 && sysnum != SYS_wait4_nocancel &&
            sysnum != SYS_waitid && sysnum != SYS_waitid_nocancel &&
            sysnum != SYS_waitevent &&
            sysnum != SYS_ioctl);
}

void
save_fpstate(dcontext_t *dcontext, sigframe_rt_t *frame)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: MacOS signal handling NYI */
}

void
sigcontext_to_mcontext_mm(priv_mcontext_t *mc, sigcontext_t *sc)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: MacOS signal handling NYI */
}

void
mcontext_to_sigcontext_mm(sigcontext_t *sc, priv_mcontext_t *mc)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: MacOS signal handling NYI */
}

void
dump_sigcontext(dcontext_t *dcontext, sigcontext_t *sc)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: MacOS signal handling NYI */
}

/* XXX i#1286: move to nudge_macos.c once we implement that */
bool
send_nudge_signal(process_id_t pid, uint action_mask,
                  client_id_t client_id, uint64 client_arg)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1286: MacOS nudges NYI */
    return false;
}
