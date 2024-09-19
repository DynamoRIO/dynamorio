/* *******************************************************************************
 * Copyright (c) 2013-2023 Google, Inc.  All rights reserved.
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
#    error Mac-only
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
    true,  /*  0 unused */
    true,  /*  1 SIGHUP */
    true,  /*  2 SIGINT */
    true,  /*  3 SIGQUIT */
    false, /*  4 SIGILL */
    false, /*  5 SIGTRAP */
    false, /*  6 SIGABRT/SIGIOT */
    true,  /*  7 SIGEMT/SIGPOLL */
    false, /*  8 SIGFPE */
    true,  /*  9 SIGKILL */
    false, /* 10 SIGBUS */
    false, /* 11 SIGSEGV */
    false, /* 12 SIGSYS */
    false, /* 13 SIGPIPE */
    true,  /* 14 SIGALRM */
    true,  /* 15 SIGTERM */
    true,  /* 16 SIGURG */
    true,  /* 17 SIGSTOP */
    true,  /* 18 SIGTSTP */
    true,  /* 19 SIGCONT */
    true,  /* 20 SIGCHLD */
    true,  /* 21 SIGTTIN */
    true,  /* 22 SIGTTOU */
    true,  /* 23 SIGIO */
    false, /* 24 SIGXCPU */
    true,  /* 25 SIGXFSZ */
    true,  /* 26 SIGVTALRM */
    true,  /* 27 SIGPROF */
    true,  /* 28 SIGWINCH  */
    true,  /* 29 SIGINFO */
    true,  /* 30 SIGUSR1 */
    true,  /* 31 SIGUSR2 */
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
    return (sysnum != SYS_open && sysnum != SYS_open_nocancel && sysnum != SYS_read &&
            sysnum != SYS_read_nocancel && sysnum != SYS_write &&
            sysnum != SYS_write_nocancel && sysnum != SYS_sendto &&
            sysnum != SYS_sendto_nocancel && sysnum != SYS_recvfrom &&
            sysnum != SYS_recvfrom_nocancel && sysnum != SYS_sendmsg &&
            sysnum != SYS_sendmsg_nocancel && sysnum != SYS_recvmsg &&
            sysnum != SYS_recvmsg_nocancel && sysnum != SYS_wait4 &&
            sysnum != SYS_wait4_nocancel && sysnum != SYS_waitid &&
            sysnum != SYS_waitid_nocancel &&
#ifdef SYS_waitevent
            sysnum != SYS_waitevent &&
#endif
            sysnum != SYS_ioctl);
}

void
save_fpstate(dcontext_t *dcontext, sigframe_rt_t *frame)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: MacOS signal handling NYI */
}

void
sigcontext_to_mcontext_simd(priv_mcontext_t *mc, sig_full_cxt_t *sc_full)
{
#ifdef AARCH64
    _STRUCT_ARM_NEON_STATE64 *fpc = (_STRUCT_ARM_NEON_STATE64 *)sc_full->fp_simd_state;
    if (fpc == NULL)
        return;
    mc->fpsr = fpc->__fpsr;
    mc->fpcr = fpc->__fpcr;
    ASSERT(sizeof(mc->simd) == sizeof(fpc->__v));
    memcpy(&mc->simd, &fpc->__v, sizeof(mc->simd));
#elif defined(X86)
    /* We assume that _STRUCT_X86_FLOAT_STATE* matches exactly the first
     * half of _STRUCT_X86_AVX_STATE*, and similarly for AVX and AVX512.
     */
    sigcontext_t *sc = sc_full->sc;
    int i;
    for (i = 0; i < proc_num_simd_sse_avx_registers(); i++) {
        memcpy(&mc->simd[i], &sc->__fs.__fpu_xmm0 + i, XMM_REG_SIZE);
    }
    if (YMM_ENABLED()) {
        for (i = 0; i < proc_num_simd_sse_avx_registers(); i++) {
            memcpy(&mc->simd[i].u32[4], &sc->__fs.__fpu_ymmh0 + i, YMMH_REG_SIZE);
        }
    }
#    if DISABLED_UNTIL_AVX512_SUPPORT_ADDED
    /* TODO i#1979/i#1312: See the comments in os_public.h: once we've resolved how
     * to expose __darwin_mcontext_avx512_64 we'd enable the code here.
     */
    if (ZMM_ENABLED()) {
        for (i = 0; i < proc_num_simd_sse_avx_registers(); i++) {
            memcpy(&mc->simd[i].u32[8], &sc->__fs.__fpu_zmmh0 + i, ZMMH_REG_SIZE);
        }
#        ifdef X64
        for (i = proc_num_simd_sse_avx_registers(); i < proc_num_simd_registers(); i++) {
            memcpy(&mc->simd[i], &sc->__fs.__fpu_zmm16 + i, ZMM_REG_SIZE);
        }
#        endif
    }
#    endif
#endif
}

void
mcontext_to_sigcontext_simd(sig_full_cxt_t *sc_full, priv_mcontext_t *mc)
{
#ifdef AARCH64
    _STRUCT_ARM_NEON_STATE64 *fpc = (_STRUCT_ARM_NEON_STATE64 *)sc_full->fp_simd_state;
    if (fpc == NULL)
        return;
    fpc->__fpsr = mc->fpsr;
    fpc->__fpcr = mc->fpcr;
    ASSERT(sizeof(mc->simd) == sizeof(fpc->__v));
    memcpy(&fpc->__v, &mc->simd, sizeof(mc->simd));
#elif defined(X86)
    sigcontext_t *sc = sc_full->sc;
    int i;
    for (i = 0; i < proc_num_simd_registers(); i++) {
        memcpy(&sc->__fs.__fpu_xmm0 + i, &mc->simd[i], XMM_REG_SIZE);
    }
    if (YMM_ENABLED()) {
        for (i = 0; i < proc_num_simd_sse_avx_registers(); i++) {
            memcpy(&sc->__fs.__fpu_ymmh0 + i, &mc->simd[i].u32[4], YMMH_REG_SIZE);
        }
    }
#    if DISABLED_UNTIL_AVX512_SUPPORT_ADDED
    /* TODO i#1979/i#1312: See the comments in os_public.h: once we've resolved how
     * to expose __darwin_mcontext_avx512_64 we'd enable the code here.
     */
    if (ZMM_ENABLED()) {
        for (i = 0; i < proc_num_simd_sse_avx_registers(); i++) {
            memcpy(&sc->__fs.__fpu_zmmh0 + i, &mc->simd[i].u32[8], ZMMH_REG_SIZE);
        }
#        ifdef X64
        for (i = proc_num_simd_sse_avx_registers(); i < proc_num_simd_registers(); i++) {
            memcpy(&sc->__fs.__fpu_zmm16 + i, &mc->simd[i], ZMM_REG_SIZE);
        }
#        endif
    }
#    endif
#endif
}

static void
dump_fpstate(dcontext_t *dcontext, sigcontext_t *sc)
{
#ifdef AARCH64
    _STRUCT_ARM_NEON_STATE64 *fpc = &sc->__ns;
    LOG(THREAD, LOG_ASYNCH, 1, "\tfpsr=0x%08x\n", fpc->__fpsr);
    LOG(THREAD, LOG_ASYNCH, 1, "\tfpcr=0x%08x\n", fpc->__fpcr);
    int i, j;
    for (i = 0; i < sizeof(fpc->__v) / sizeof(fpc->__v[0]); i++) {
        LOG(THREAD, LOG_ASYNCH, 1, "\tv[%d] = 0x", i);
        for (j = 0; j < 4; j++) {
            LOG(THREAD, LOG_ASYNCH, 1, "%08x", *(((uint *)&fpc->__v[i]) + j));
        }
        LOG(THREAD, LOG_ASYNCH, 1, "\n");
    }
#elif defined(X86)
    int i, j;
    LOG(THREAD, LOG_ASYNCH, 1, "\tfcw=0x%04x\n", *(ushort *)&sc->__fs.__fpu_fcw);
    LOG(THREAD, LOG_ASYNCH, 1, "\tfsw=0x%04x\n", *(ushort *)&sc->__fs.__fpu_fsw);
    LOG(THREAD, LOG_ASYNCH, 1, "\tftw=0x%02x\n", sc->__fs.__fpu_ftw);
    LOG(THREAD, LOG_ASYNCH, 1, "\tfop=0x%04x\n", sc->__fs.__fpu_fop);
    LOG(THREAD, LOG_ASYNCH, 1, "\tip=0x%08x\n", sc->__fs.__fpu_ip);
    LOG(THREAD, LOG_ASYNCH, 1, "\tcs=0x%04x\n", sc->__fs.__fpu_cs);
    LOG(THREAD, LOG_ASYNCH, 1, "\tdp=0x%08x\n", sc->__fs.__fpu_dp);
    LOG(THREAD, LOG_ASYNCH, 1, "\tds=0x%04x\n", sc->__fs.__fpu_ds);
    LOG(THREAD, LOG_ASYNCH, 1, "\tmxcsr=0x%08x\n", sc->__fs.__fpu_mxcsr);
    LOG(THREAD, LOG_ASYNCH, 1, "\tmxcsrmask=0x%08x\n", sc->__fs.__fpu_mxcsrmask);
    for (i = 0; i < 8; i++) {
        LOG(THREAD, LOG_ASYNCH, 1, "\tst%d = ", i);
        for (j = 0; j < 5; j++) {
            LOG(THREAD, LOG_ASYNCH, 1, "%04x ",
                *((ushort *)(&sc->__fs.__fpu_stmm0 + i) + j));
        }
        LOG(THREAD, LOG_ASYNCH, 1, "\n");
    }
    /* XXX i#1312: this needs to get extended to AVX-512. */
    for (i = 0; i < proc_num_simd_sse_avx_registers(); i++) {
        LOG(THREAD, LOG_ASYNCH, 1, "\txmm%d = ", i);
        for (j = 0; j < 4; j++) {
            LOG(THREAD, LOG_ASYNCH, 1, "%08x ",
                *((uint *)(&sc->__fs.__fpu_xmm0 + i) + j));
        }
        LOG(THREAD, LOG_ASYNCH, 1, "\n");
    }
    if (YMM_ENABLED()) {
        for (i = 0; i < proc_num_simd_sse_avx_registers(); i++) {
            LOG(THREAD, LOG_ASYNCH, 1, "\tymmh%d = ", i);
            for (j = 0; j < 4; j++) {
                LOG(THREAD, LOG_ASYNCH, 1, "%08x ",
                    *((uint *)(&sc->__fs.__fpu_ymmh0 + i) + j));
            }
            LOG(THREAD, LOG_ASYNCH, 1, "\n");
        }
    }
    /* XXX i#1312: AVX-512 extended register copies missing yet. */
#endif
}

void
dump_sigcontext(dcontext_t *dcontext, sigcontext_t *sc)
{
#ifndef AARCH64
    LOG(THREAD, LOG_ASYNCH, 1, "\txdi=" PFX "\n", sc->SC_XDI);
    LOG(THREAD, LOG_ASYNCH, 1, "\txsi=" PFX "\n", sc->SC_XSI);
    LOG(THREAD, LOG_ASYNCH, 1, "\txbp=" PFX "\n", sc->SC_XBP);
    LOG(THREAD, LOG_ASYNCH, 1, "\txsp=" PFX "\n", sc->SC_XSP);
    LOG(THREAD, LOG_ASYNCH, 1, "\txbx=" PFX "\n", sc->SC_XBX);
    LOG(THREAD, LOG_ASYNCH, 1, "\txdx=" PFX "\n", sc->SC_XDX);
    LOG(THREAD, LOG_ASYNCH, 1, "\txcx=" PFX "\n", sc->SC_XCX);
    LOG(THREAD, LOG_ASYNCH, 1, "\txax=" PFX "\n", sc->SC_XAX);
#    ifdef X64
    LOG(THREAD, LOG_ASYNCH, 1, "\t r8=" PFX "\n", sc->SC_R8);
    LOG(THREAD, LOG_ASYNCH, 1, "\t r9=" PFX "\n", sc->SC_R8);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr10=" PFX "\n", sc->SC_R10);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr11=" PFX "\n", sc->SC_R11);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr12=" PFX "\n", sc->SC_R12);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr13=" PFX "\n", sc->SC_R13);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr14=" PFX "\n", sc->SC_R14);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr15=" PFX "\n", sc->SC_R15);
#    endif

    LOG(THREAD, LOG_ASYNCH, 1, "\txip=" PFX "\n", sc->SC_XIP);
    LOG(THREAD, LOG_ASYNCH, 1, "\teflags=" PFX "\n", sc->SC_XFLAGS);

    LOG(THREAD, LOG_ASYNCH, 1, "\tcs=0x%04x\n", sc->__ss.__cs);
#    ifndef X64
    LOG(THREAD, LOG_ASYNCH, 1, "\tds=0x%04x\n", sc->__ss.__ds);
    LOG(THREAD, LOG_ASYNCH, 1, "\tes=0x%04x\n", sc->__ss.__es);
#    endif
    LOG(THREAD, LOG_ASYNCH, 1, "\tfs=0x%04x\n", sc->__ss.__fs);
    LOG(THREAD, LOG_ASYNCH, 1, "\tgs=0x%04x\n", sc->__ss.__gs);

    LOG(THREAD, LOG_ASYNCH, 1, "\ttrapno=0x%04x\n", sc->__es.__trapno);
    LOG(THREAD, LOG_ASYNCH, 1, "\tcpu=0x%04x\n", sc->__es.__cpu);
    LOG(THREAD, LOG_ASYNCH, 1, "\terr=0x%08x\n", sc->__es.__err);
    LOG(THREAD, LOG_ASYNCH, 1, "\tfaultvaddr=" PFX "\n", sc->__es.__faultvaddr);
#else
    LOG(THREAD, LOG_ASYNCH, 1, "\tfault=" PFX "\n", sc->__es.__far);
    LOG(THREAD, LOG_ASYNCH, 1, "\tesr=0x08x\n", sc->__es.__esr);
    LOG(THREAD, LOG_ASYNCH, 1, "\tcount=0x%08x\n", sc->__es.__exception);
    int i;
    for (i = 0; i < 29; i++)
        LOG(THREAD, LOG_ASYNCH, 1, "\tr%d=" PFX "\n", i, sc->__ss.__x[i]);
    LOG(THREAD, LOG_ASYNCH, 1, "\tfp=" PFX "\n", sc->__ss.__fp);
    LOG(THREAD, LOG_ASYNCH, 1, "\tlr=" PFX "\n", sc->__ss.__lr);
    LOG(THREAD, LOG_ASYNCH, 1, "\tsp=" PFX "\n", sc->__ss.__sp);
    LOG(THREAD, LOG_ASYNCH, 1, "\tpc=" PFX "\n", sc->__ss.__pc);
    LOG(THREAD, LOG_ASYNCH, 1, "\tcpsr=0x%08x\n", sc->__ss.__cpsr);
#endif

    dump_fpstate(dcontext, sc);
}

/* XXX i#1286: move to nudge_macos.c once we implement that */
bool
send_nudge_signal(process_id_t pid, uint action_mask, client_id_t client_id,
                  uint64 client_arg)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1286: MacOS nudges NYI */
    return false;
}

size_t
signal_frame_extra_size(bool include_alignment)
{
    /* Currently assuming __darwin_mcontext_avx{32,64} is always used in the
     * frame.  If instead __darwin_mcontext{32,64} is used (w/ just float and no AVX)
     * on, say, older machines or OSX versions, we'll have to revisit this.
     */
    return 0;
}

void
signal_arch_init(void)
{
    /* Nothing. */
}
