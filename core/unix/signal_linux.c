/* **********************************************************
 * Copyright (c) 2011-2014 Google, Inc.  All rights reserved.
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
# error Linux-only
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
    /* nothing    0 */   DEFAULT_IGNORE,
    /* SIGHUP     1 */   DEFAULT_TERMINATE,
    /* SIGINT     2 */   DEFAULT_TERMINATE,
    /* SIGQUIT    3 */   DEFAULT_TERMINATE_CORE,
    /* SIGILL     4 */   DEFAULT_TERMINATE_CORE,
    /* SIGTRAP    5 */   DEFAULT_TERMINATE_CORE,
    /* SIGABRT/SIGIOT 6 */   DEFAULT_TERMINATE_CORE,
    /* SIGBUS     7 */   DEFAULT_TERMINATE, /* should be CORE */
    /* SIGFPE     8 */   DEFAULT_TERMINATE_CORE,
    /* SIGKILL    9 */   DEFAULT_TERMINATE,
    /* SIGUSR1   10 */   DEFAULT_TERMINATE,
    /* SIGSEGV   11 */   DEFAULT_TERMINATE_CORE,
    /* SIGUSR2   12 */   DEFAULT_TERMINATE,
    /* SIGPIPE   13 */   DEFAULT_TERMINATE,
    /* SIGALRM   14 */   DEFAULT_TERMINATE,
    /* SIGTERM   15 */   DEFAULT_TERMINATE,
    /* SIGSTKFLT 16 */   DEFAULT_TERMINATE,
    /* SIGCHLD   17 */   DEFAULT_IGNORE,
    /* SIGCONT   18 */   DEFAULT_CONTINUE,
    /* SIGSTOP   19 */   DEFAULT_STOP,
    /* SIGTSTP   20 */   DEFAULT_STOP,
    /* SIGTTIN   21 */   DEFAULT_STOP,
    /* SIGTTOU   22 */   DEFAULT_STOP,
    /* SIGURG    23 */   DEFAULT_IGNORE,
    /* SIGXCPU   24 */   DEFAULT_TERMINATE,
    /* SIGXFSZ   25 */   DEFAULT_TERMINATE,
    /* SIGVTALRM 26 */   DEFAULT_TERMINATE,
    /* SIGPROF   27 */   DEFAULT_TERMINATE,
    /* SIGWINCH  28 */   DEFAULT_IGNORE,
    /* SIGIO/SIGPOLL/SIGLOST 29 */ DEFAULT_TERMINATE,
    /* SIGPWR    30 */   DEFAULT_TERMINATE,
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
    /* nothing    0 */             true,
    /* SIGHUP     1 */             true,
    /* SIGINT     2 */             true,
    /* SIGQUIT    3 */             true,
    /* SIGILL     4 */            false,
    /* SIGTRAP    5 */            false,
    /* SIGABRT/SIGIOT 6 */        false,
    /* SIGBUS     7 */            false, 
    /* SIGFPE     8 */            false,
    /* SIGKILL    9 */             true,
    /* SIGUSR1   10 */             true,
    /* SIGSEGV   11 */            false,
    /* SIGUSR2   12 */             true,
    /* SIGPIPE   13 */            false,
    /* SIGALRM   14 */             true,
    /* SIGTERM   15 */             true,
    /* SIGSTKFLT 16 */            false,
    /* SIGCHLD   17 */             true,
    /* SIGCONT   18 */             true,
    /* SIGSTOP   19 */             true,
    /* SIGTSTP   20 */             true,
    /* SIGTTIN   21 */             true,
    /* SIGTTOU   22 */             true,
    /* SIGURG    23 */             true,
    /* SIGXCPU   24 */            false,
    /* SIGXFSZ   25 */             true,
    /* SIGVTALRM 26 */             true,
    /* SIGPROF   27 */             true,
    /* SIGWINCH  28 */             true,
    /* SIGIO/SIGPOLL/SIGLOST 29 */ true,
    /* SIGPWR    30 */             true,
    /* SIGSYS/SIGUNUSED 31 */     false,

    /* ASSUMPTION: all real-time can be delayed */
    /* 32 */                       true,
    /* 33 */                       true,
    /* 34 */                       true,
    /* 35 */                       true,
    /* 36 */                       true,
    /* 37 */                       true,
    /* 38 */                       true,
    /* 39 */                       true,
    /* 40 */                       true,
    /* 41 */                       true,
    /* 42 */                       true,
    /* 43 */                       true,
    /* 44 */                       true,
    /* 45 */                       true,
    /* 46 */                       true,
    /* 47 */                       true,
    /* 48 */                       true,
    /* 49 */                       true,
    /* 50 */                       true,
    /* 51 */                       true,
    /* 52 */                       true,
    /* 53 */                       true,
    /* 54 */                       true,
    /* 55 */                       true,
    /* 56 */                       true,
    /* 57 */                       true,
    /* 58 */                       true,
    /* 59 */                       true,
    /* 60 */                       true,
    /* 61 */                       true,
    /* 62 */                       true,
    /* 63 */                       true,
    /* 64 */                       true,
};

bool
sysnum_is_not_restartable(int sysnum)
{
    /* Check the list of non-restartable syscalls.
     * We're missing:
     * + SYS_read from an inotify file descriptor.
     * We're overly aggressive on:
     * + Socket interfaces: supposed to restart if no timeout has been set
     *   but we never restart for simplicity for now.
     */
    return (sysnum == SYS_pause || 
            sysnum == SYS_rt_sigsuspend ||
            sysnum == SYS_rt_sigtimedwait ||
            IF_X64(sysnum == SYS_epoll_wait_old ||)
            sysnum == SYS_epoll_wait || sysnum == SYS_epoll_pwait ||
            sysnum == SYS_poll || sysnum == SYS_ppoll ||
            sysnum == SYS_select || sysnum == SYS_pselect6 ||
#ifdef X64
            sysnum == SYS_msgrcv || sysnum == SYS_msgsnd || sysnum == SYS_semop ||
            sysnum == SYS_semtimedop ||
            /* XXX: these should be restarted if there's no timeout */
            sysnum == SYS_accept || sysnum == SYS_accept4 ||
            sysnum == SYS_recvfrom || sysnum == SYS_recvmsg || sysnum == SYS_recvmmsg ||
            sysnum == SYS_connect || sysnum == SYS_sendto ||
            sysnum == SYS_sendmmsg || sysnum == SYS_sendfile ||
#else
            /* XXX: some should be restarted if there's no timeout */
            sysnum == SYS_ipc ||
#endif
            sysnum == SYS_clock_nanosleep || sysnum == SYS_nanosleep ||
            sysnum == SYS_io_getevents);
}

/**** floating point support ********************************************/

/* The following code is based on routines in
 *   /usr/src/linux/arch/i386/kernel/i387.c
 * and definitions in
 *   /usr/src/linux/include/asm-i386/processor.h
 *   /usr/src/linux/include/asm-i386/i387.h
 */

struct i387_fsave_struct {
    long        cwd;
    long        swd;
    long        twd;
    long        fip;
    long        fcs;
    long        foo;
    long        fos;
    long        st_space[20];   /* 8*10 bytes for each FP-reg = 80 bytes */
    long        status;         /* software status information */
};

/* note that fxsave requires that i387_fxsave_struct be aligned on
 * a 16-byte boundary
 */
struct i387_fxsave_struct {
    unsigned short      cwd;
    unsigned short      swd;
    unsigned short      twd;
    unsigned short      fop;
#ifdef X64
    long        rip;
    long        rdp;
    int         mxcsr;
    int         mxcsr_mask;
    int         st_space[32];   /* 8*16 bytes for each FP-reg = 128 bytes */
    int         xmm_space[64];  /* 16*16 bytes for each XMM-reg = 256 bytes */
    int         padding[24];
#else
    long        fip;
    long        fcs;
    long        foo;
    long        fos;
    long        mxcsr;
    long        reserved;
    long        st_space[32];   /* 8*16 bytes for each FP-reg = 128 bytes */
    long        xmm_space[32];  /* 8*16 bytes for each XMM-reg = 128 bytes */
    long        padding[56];
#endif
} __attribute__ ((aligned (16)));

union i387_union {
    struct i387_fsave_struct    fsave;
    struct i387_fxsave_struct   fxsave;
};

#ifndef X64
/* For 32-bit if we use fxsave we need to convert it to the kernel's struct.
 * For 64-bit the kernel's struct is identical to the fxsave format.
 */
static uint
twd_fxsr_to_i387(struct i387_fxsave_struct *fxsave)
{
    struct _fpxreg *st = NULL;
    uint twd = (uint) fxsave->twd;
    uint tag;
    uint ret = 0xffff0000;
    int i;
    for (i = 0 ; i < 8 ; i++) {
        if (TEST(0x1, twd)) {
            st = (struct _fpxreg *) &fxsave->st_space[i*4];

            switch (st->exponent & 0x7fff) {
            case 0x7fff:
                tag = 2;        /* Special */
                break;
            case 0x0000:
                if (st->significand[0] == 0 &&
                    st->significand[1] == 0 &&
                    st->significand[2] == 0 &&
                    st->significand[3] == 0) {
                    tag = 1;    /* Zero */
                } else {
                    tag = 2;    /* Special */
                }
                break;
            default:
                if (TEST(0x8000, st->significand[3])) {
                    tag = 0;    /* Valid */
                } else {
                    tag = 2;    /* Special */
                }
                break;
            }
        } else {
            tag = 3;            /* Empty */
        }
        ret |= (tag << (2 * i));
        twd = twd >> 1;
    }
    return ret;
}

static void
convert_fxsave_to_fpstate(struct _fpstate *fpstate,
                          struct i387_fxsave_struct *fxsave)
{
    int i;

    fpstate->cw = (uint)fxsave->cwd | 0xffff0000;
    fpstate->sw = (uint)fxsave->swd | 0xffff0000;
    fpstate->tag = twd_fxsr_to_i387(fxsave);
    fpstate->ipoff = fxsave->fip;
    fpstate->cssel = fxsave->fcs | ((uint)fxsave->fop << 16);
    fpstate->dataoff = fxsave->foo;
    fpstate->datasel = fxsave->fos;

    for (i = 0; i < 8; i++) {
        memcpy(&fpstate->_st[i], &fxsave->st_space[i*4], sizeof(fpstate->_st[i]));
    }

    fpstate->status = fxsave->swd;
    fpstate->magic = X86_FXSR_MAGIC;

    memcpy(&fpstate->_fxsr_env[0], fxsave,
           sizeof(struct i387_fxsave_struct));
}
#endif /* !X64 */

static void
save_xmm(dcontext_t *dcontext, sigframe_rt_t *frame)
{
    /* see comments at call site: can't just do xsave */
    int i;
    sigcontext_t *sc = get_sigcontext_from_rt_frame(frame);
    struct _xstate *xstate = (struct _xstate *) sc->fpstate;
    if (!preserve_xmm_caller_saved())
        return;
    if (YMM_ENABLED()) {
        /* all ymm regs are in our mcontext.  the only other thing
         * in xstate is the xgetbv.
         */
        uint bv_high, bv_low;
        dr_xgetbv(&bv_high, &bv_low);
        xstate->xstate_hdr.xstate_bv = (((uint64)bv_high)<<32) | bv_low;
    }
    for (i=0; i<NUM_XMM_SAVED; i++) {
        /* we assume no padding */
#ifdef X64
        /* __u32 xmm_space[64] */
        memcpy(&sc->fpstate->xmm_space[i*4], &get_mcontext(dcontext)->ymm[i],
               XMM_REG_SIZE);
        if (YMM_ENABLED()) {
            /* i#637: ymm top halves are inside struct _xstate */
            memcpy(&xstate->ymmh.ymmh_space[i * 4], 
                   ((void *)&get_mcontext(dcontext)->ymm[i]) + XMM_REG_SIZE,
                   YMMH_REG_SIZE);
        }
#else
        memcpy(&sc->fpstate->_xmm[i], &get_mcontext(dcontext)->ymm[i], XMM_REG_SIZE);
        if (YMM_ENABLED()) {
            /* i#637: ymm top halves are inside struct _xstate */
            memcpy(&xstate->ymmh.ymmh_space[i * 4], 
                   ((void *)&get_mcontext(dcontext)->ymm[i]) + XMM_REG_SIZE,
                   YMMH_REG_SIZE);
        }
#endif
    }
}

/* We can't tell whether the app has used fpstate yet so we preserve every time
 * (i#641 covers optimizing that)
 */
void
save_fpstate(dcontext_t *dcontext, sigframe_rt_t *frame)
{
    /* FIXME: is there a better way to align this thing?
     * the __attribute__ on the struct above doesn't do it
     */
    char align[sizeof(union i387_union) + 16];
    union i387_union *temp = (union i387_union *)
        ( (((ptr_uint_t)align) + 16) & ((ptr_uint_t)-16) );
    sigcontext_t *sc = get_sigcontext_from_rt_frame(frame);
    LOG(THREAD, LOG_ASYNCH, 3, "save_fpstate\n");
    if (sc->fpstate == NULL) {
        /* Nothing to do: there was no fpstate to save at the time the kernel
         * gave us this frame.
         * It's possible that by the time we deliver the signal
         * there is some state: but it's up to the caller to set up room
         * for fpstate and point at it in that case.
         */
        return;
    } else {
        LOG(THREAD, LOG_ASYNCH, 3, "ptr="PFX"\n", sc->fpstate);
    }
    if (proc_has_feature(FEATURE_FXSR)) {
        LOG(THREAD, LOG_ASYNCH, 3, "\ttemp="PFX"\n", temp);
#ifdef X64
        /* this is "unlazy_fpu" */
        /* fxsaveq is only supported with gas >= 2.16 but we have that */
        asm volatile( "fxsaveq %0 ; fnclex"
                      : "=m" (temp->fxsave) );
        /* now convert into struct _fpstate form */
        ASSERT(sizeof(struct _fpstate) == sizeof(struct i387_fxsave_struct));
        memcpy(sc->fpstate, &temp->fxsave, sizeof(struct i387_fxsave_struct));
#else
        /* this is "unlazy_fpu" */
        asm volatile( "fxsave %0 ; fnclex"
                      : "=m" (temp->fxsave) );
        /* now convert into struct _fpstate form */
        convert_fxsave_to_fpstate(sc->fpstate, &temp->fxsave);
#endif
    } else {
        /* FIXME NYI: need to convert to fxsave format for sc->fpstate */
        IF_X64(ASSERT_NOT_IMPLEMENTED(false));
        /* this is "unlazy_fpu" */
        asm volatile( "fnsave %0 ; fwait"
                      : "=m" (temp->fsave) );
        /* now convert into struct _fpstate form */
        temp->fsave.status = temp->fsave.swd;
        memcpy(sc->fpstate, &temp->fsave, sizeof(struct i387_fsave_struct));
    }

    /* the app's xmm registers may be saved away in priv_mcontext_t, in which
     * case we need to copy those values instead of using what was in
     * the physical xmm registers.
     * because of this, we can't just execute "xsave".  we still need to
     * execute xgetbv though.  xsave is very expensive so not worth doing
     * when xgetbv is all we need; if in the future they add status words,
     * etc. we can't get any other way then we'll have to do it, but best
     * to avoid for now.
     */
    save_xmm(dcontext, frame);
}

#ifdef DEBUG
static void
dump_fpstate(dcontext_t *dcontext, struct _fpstate *fp)
{
    int i,j;
#ifdef X64
    LOG(THREAD, LOG_ASYNCH, 1, "\tcwd="PFX"\n", fp->cwd);
    LOG(THREAD, LOG_ASYNCH, 1, "\tswd="PFX"\n", fp->swd);
    LOG(THREAD, LOG_ASYNCH, 1, "\ttwd="PFX"\n", fp->twd);
    LOG(THREAD, LOG_ASYNCH, 1, "\tfop="PFX"\n", fp->fop);
    LOG(THREAD, LOG_ASYNCH, 1, "\trip="PFX"\n", fp->rip);
    LOG(THREAD, LOG_ASYNCH, 1, "\trdp="PFX"\n", fp->rdp);
    LOG(THREAD, LOG_ASYNCH, 1, "\tmxcsr="PFX"\n", fp->mxcsr);
    LOG(THREAD, LOG_ASYNCH, 1, "\tmxcsr_mask="PFX"\n", fp->mxcsr_mask);
    for (i=0; i<8; i++) {
        LOG(THREAD, LOG_ASYNCH, 1, "\tst%d = 0x", i);
        for (j=0; j<4; j++) {
            LOG(THREAD, LOG_ASYNCH, 1, "%08x", fp->st_space[i*4+j]);
        }
        LOG(THREAD, LOG_ASYNCH, 1, "\n");
    }
    for (i=0; i<16; i++) {
        LOG(THREAD, LOG_ASYNCH, 1, "\txmm%d = 0x", i);
        for (j=0; j<4; j++) {
            LOG(THREAD, LOG_ASYNCH, 1, "%08x", fp->xmm_space[i*4+j]);
        }
        LOG(THREAD, LOG_ASYNCH, 1, "\n");
    }
#else
    LOG(THREAD, LOG_ASYNCH, 1, "\tcw="PFX"\n", fp->cw);
    LOG(THREAD, LOG_ASYNCH, 1, "\tsw="PFX"\n", fp->sw);
    LOG(THREAD, LOG_ASYNCH, 1, "\ttag="PFX"\n", fp->tag);
    LOG(THREAD, LOG_ASYNCH, 1, "\tipoff="PFX"\n", fp->ipoff);
    LOG(THREAD, LOG_ASYNCH, 1, "\tcssel="PFX"\n", fp->cssel);
    LOG(THREAD, LOG_ASYNCH, 1, "\tdataoff="PFX"\n", fp->dataoff);
    LOG(THREAD, LOG_ASYNCH, 1, "\tdatasel="PFX"\n", fp->datasel);
    for (i=0; i<8; i++) {
        LOG(THREAD, LOG_ASYNCH, 1, "\tst%d = ", i);
        for (j=0; j<4; j++)
            LOG(THREAD, LOG_ASYNCH, 1, "%04x ", fp->_st[i].significand[j]);
        LOG(THREAD, LOG_ASYNCH, 1, "^ %04x\n", fp->_st[i].exponent);
    }
    LOG(THREAD, LOG_ASYNCH, 1, "\tstatus=0x%04x\n", fp->status);
    LOG(THREAD, LOG_ASYNCH, 1, "\tmagic=0x%04x\n", fp->magic);

    /* FXSR FPU environment */
    for (i=0; i<6; i++)
        LOG(THREAD, LOG_ASYNCH, 1, "\tfxsr_env[%d] = "PFX"\n", i, fp->_fxsr_env[i]);
    LOG(THREAD, LOG_ASYNCH, 1, "\tmxcsr="PFX"\n", fp->mxcsr);
    LOG(THREAD, LOG_ASYNCH, 1, "\treserved="PFX"\n", fp->reserved);
    for (i=0; i<8; i++) {
        LOG(THREAD, LOG_ASYNCH, 1, "\tfxsr_st%d = ", i);
        for (j=0; j<4; j++)
            LOG(THREAD, LOG_ASYNCH, 1, "%04x ", fp->_fxsr_st[i].significand[j]);
        LOG(THREAD, LOG_ASYNCH, 1, "^ %04x\n", fp->_fxsr_st[i].exponent);
        /* ignore padding */
    }
    for (i=0; i<8; i++) {
        LOG(THREAD, LOG_ASYNCH, 1, "\txmm%d = ", i);
        for (j=0; j<4; j++)
            LOG(THREAD, LOG_ASYNCH, 1, "%04x ", fp->_xmm[i].element[j]);
        LOG(THREAD, LOG_ASYNCH, 1, "\n");
    }
#endif
    /* ignore padding */
    if (YMM_ENABLED()) {
        struct _xstate *xstate = (struct _xstate *) fp;
        if (fp->sw_reserved.magic1 == FP_XSTATE_MAGIC1) {
            /* i#718: for 32-bit app on 64-bit OS, the xstate_size in sw_reserved
             * is obtained via cpuid, which is the xstate size of 64-bit arch.
             */
            ASSERT(fp->sw_reserved.extended_size >= sizeof(*xstate));
            ASSERT(TEST(XCR0_AVX, fp->sw_reserved.xstate_bv));
            LOG(THREAD, LOG_ASYNCH, 1, "\txstate_bv = 0x"HEX64_FORMAT_STRING"\n",
                xstate->xstate_hdr.xstate_bv);
            for (i=0; i<NUM_XMM_SLOTS; i++) {
                LOG(THREAD, LOG_ASYNCH, 1, "\tymmh%d = ", i);
                for (j=0; j<4; j++)
                    LOG(THREAD, LOG_ASYNCH, 1, "%04x ", xstate->ymmh.ymmh_space[i*4+j]);
                LOG(THREAD, LOG_ASYNCH, 1, "\n");
            }
        }
    }
}

void
dump_sigcontext(dcontext_t *dcontext, sigcontext_t *sc)
{
    LOG(THREAD, LOG_ASYNCH, 1, "\tgs=0x%04x"IF_NOT_X64(", __gsh=0x%04x")"\n",
        sc->gs _IF_NOT_X64(sc->__gsh));
    LOG(THREAD, LOG_ASYNCH, 1, "\tfs=0x%04x"IF_NOT_X64(", __fsh=0x%04x")"\n",
        sc->fs _IF_NOT_X64(sc->__fsh));
#ifndef X64
    LOG(THREAD, LOG_ASYNCH, 1, "\tes=0x%04x, __esh=0x%04x\n", sc->es, sc->__esh);
    LOG(THREAD, LOG_ASYNCH, 1, "\tds=0x%04x, __dsh=0x%04x\n", sc->ds, sc->__dsh);
#endif
    LOG(THREAD, LOG_ASYNCH, 1, "\txdi="PFX"\n", sc->SC_XDI);
    LOG(THREAD, LOG_ASYNCH, 1, "\txsi="PFX"\n", sc->SC_XSI);
    LOG(THREAD, LOG_ASYNCH, 1, "\txbp="PFX"\n", sc->SC_XBP);
    LOG(THREAD, LOG_ASYNCH, 1, "\txsp="PFX"\n", sc->SC_XSP);
    LOG(THREAD, LOG_ASYNCH, 1, "\txbx="PFX"\n", sc->SC_XBX);
    LOG(THREAD, LOG_ASYNCH, 1, "\txdx="PFX"\n", sc->SC_XDX);
    LOG(THREAD, LOG_ASYNCH, 1, "\txcx="PFX"\n", sc->SC_XCX);
    LOG(THREAD, LOG_ASYNCH, 1, "\txax="PFX"\n", sc->SC_XAX);
#ifdef X64
    LOG(THREAD, LOG_ASYNCH, 1, "\t r8="PFX"\n", sc->r8);
    LOG(THREAD, LOG_ASYNCH, 1, "\t r9="PFX"\n", sc->r8);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr10="PFX"\n", sc->r10);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr11="PFX"\n", sc->r11);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr12="PFX"\n", sc->r12);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr13="PFX"\n", sc->r13);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr14="PFX"\n", sc->r14);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr15="PFX"\n", sc->r15);
#endif
    LOG(THREAD, LOG_ASYNCH, 1, "\ttrapno="PFX"\n", sc->trapno);
    LOG(THREAD, LOG_ASYNCH, 1, "\terr="PFX"\n", sc->err);
    LOG(THREAD, LOG_ASYNCH, 1, "\txip="PFX"\n", sc->SC_XIP);
    LOG(THREAD, LOG_ASYNCH, 1, "\tcs=0x%04x"IF_NOT_X64(", __esh=0x%04x")"\n",
        sc->cs _IF_NOT_X64(sc->__csh));
    LOG(THREAD, LOG_ASYNCH, 1, "\teflags="PFX"\n", sc->SC_XFLAGS);
#ifndef X64
    LOG(THREAD, LOG_ASYNCH, 1, "\tesp_at_signal="PFX"\n", sc->esp_at_signal);
    LOG(THREAD, LOG_ASYNCH, 1, "\tss=0x%04x, __ssh=0x%04x\n", sc->ss, sc->__ssh);
#endif
    if (sc->fpstate == NULL)
        LOG(THREAD, LOG_ASYNCH, 1, "\tfpstate=<NULL>\n");
    else
        dump_fpstate(dcontext, sc->fpstate);
    LOG(THREAD, LOG_ASYNCH, 1, "\toldmask="PFX"\n", sc->oldmask);
    LOG(THREAD, LOG_ASYNCH, 1, "\tcr2="PFX"\n", sc->cr2);
}
#endif /* DEBUG */

void
sigcontext_to_mcontext_mm(priv_mcontext_t *mc, sigcontext_t *sc)
{
    if (sc->fpstate != NULL) {
        int i;
        for (i=0; i<NUM_XMM_SLOTS; i++) {
            memcpy(&mc->ymm[i], &sc->fpstate->IF_X64_ELSE(xmm_space[i*4],_xmm[i]),
                   XMM_REG_SIZE);
        }
        if (YMM_ENABLED()) {
            struct _xstate *xstate = (struct _xstate *) sc->fpstate;
            if (sc->fpstate->sw_reserved.magic1 == FP_XSTATE_MAGIC1) {
                /* i#718: for 32-bit app on 64-bit OS, the xstate_size in sw_reserved
                 * is obtained via cpuid, which is the xstate size of 64-bit arch.
                 */
                ASSERT(sc->fpstate->sw_reserved.extended_size >= sizeof(*xstate));
                ASSERT(TEST(XCR0_AVX, sc->fpstate->sw_reserved.xstate_bv));
                for (i=0; i<NUM_XMM_SLOTS; i++) {
                    memcpy(&mc->ymm[i].u32[4], &xstate->ymmh.ymmh_space[i*4],
                           YMMH_REG_SIZE);
                }
            }
        }
    }
}

void
mcontext_to_sigcontext_mm(sigcontext_t *sc, priv_mcontext_t *mc)
{
    if (sc->fpstate != NULL) {
        int i;
        for (i=0; i<NUM_XMM_SLOTS; i++) {
            memcpy(&sc->fpstate->IF_X64_ELSE(xmm_space[i*4],_xmm[i]), &mc->ymm[i],
                   XMM_REG_SIZE);
        }
        if (YMM_ENABLED()) {
            struct _xstate *xstate = (struct _xstate *) sc->fpstate;
            if (sc->fpstate->sw_reserved.magic1 == FP_XSTATE_MAGIC1) {
                /* i#718: for 32-bit app on 64-bit OS, the xstate_size in sw_reserved
                 * is obtained via cpuid, which is the xstate size of 64-bit arch.
                 */
                ASSERT(sc->fpstate->sw_reserved.extended_size >= sizeof(*xstate));
                ASSERT(TEST(XCR0_AVX, sc->fpstate->sw_reserved.xstate_bv));
                for (i=0; i<NUM_XMM_SLOTS; i++) {
                    memcpy(&xstate->ymmh.ymmh_space[i*4], &mc->ymm[i].u32[4],
                           YMMH_REG_SIZE);
                }
            }
        }
    }
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
sigfd_pipe_free(void *ptr)
{
    sigfd_pipe_t *pipe = (sigfd_pipe_t *) ptr;
    ASSERT(pipe->refcount > 0);
    pipe->refcount--;
    if (pipe->refcount == 0) {
        if (pipe->dcontext != NULL) {
            /* Update the owning thread's info.
             * We write a NULL which is atomic.
             * The thread on exit grabs the table lock for synch and clears dcontext.
             */
            thread_sig_info_t *info = (thread_sig_info_t *)
                pipe->dcontext->signal_field;
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
#   define SIGNALFD_HTABLE_INIT_SIZE 6
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

ptr_int_t
handle_pre_signalfd(dcontext_t *dcontext, int fd, kernel_sigset_t *mask,
                    size_t sizemask, int flags)
{
    thread_sig_info_t *info = (thread_sig_info_t *) dcontext->signal_field;
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
        pipe = (sigfd_pipe_t *) generic_hash_lookup(GLOBAL_DCONTEXT, sigfd_table, fd);
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
        fd_table_add(pipe->write_fd, 0/*keep across fork*/);

        /* We need an un-closable copy of the read fd in case we need to dup it */
        pipe->read_fd = fd_priv_dup(fds[0]);
        if (TEST(SFD_CLOEXEC, flags))
            fd_mark_close_on_exec(pipe->read_fd);
        fd_table_add(pipe->read_fd, 0/*keep across fork*/);

        TABLE_RWLOCK(sigfd_table, write, lock);
        generic_hash_add(GLOBAL_DCONTEXT, sigfd_table, fds[0], (void *) pipe);
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
            LOG(THREAD, LOG_ASYNCH, 2,
                "adding signalfd pipe %d for signal %d\n", pipe->write_fd, sig);
        } else if (info->signalfd[sig] != NULL) {
            info->signalfd[sig] = NULL;
            LOG(THREAD, LOG_ASYNCH, 2,
                "removing signalfd pipe=%d for signal %d\n", pipe->write_fd, sig);
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
        struct signalfd_siginfo towrite = {0,};

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
        /* XXX: check older glibc headers */
        towrite.ssi_tid = frame->info._sifields._timer.si_tid;
        towrite.ssi_overrun = frame->info.si_overrun;
        towrite.ssi_status = frame->info.si_status;
        towrite.ssi_utime = frame->info.si_utime;
        towrite.ssi_stime = frame->info.si_stime;
#ifdef __ARCH_SI_TRAPNO
        towrite.ssi_trapno = frame->info.si_trapno;
#endif
        towrite.ssi_addr = (ptr_int_t) frame->info.si_addr;

        /* XXX: if the pipe is full, don't write to it as it could block.  We
         * can poll to determine.  This is quite unlikely (kernel buffer is 64K
         * since 2.6.11) so for now we do not do so.
         */
        res = write_syscall(pipe->write_fd, &towrite, sizeof(towrite));
        LOG(THREAD, LOG_ASYNCH, 2,
            "writing to signalfd fd=%d for signal %d => %d\n", pipe->write_fd, sig, res);
        return true; /* signal consumed */
    }
    return false;
}

void
signal_handle_dup(dcontext_t *dcontext, file_t src, file_t dst)
{
    sigfd_pipe_t *pipe;
    TABLE_RWLOCK(sigfd_table, read, lock);
    pipe = (sigfd_pipe_t *) generic_hash_lookup(GLOBAL_DCONTEXT, sigfd_table, src);
    TABLE_RWLOCK(sigfd_table, read, unlock);
    if (pipe == NULL)
        return;
    TABLE_RWLOCK(sigfd_table, write, lock);
    pipe = (sigfd_pipe_t *) generic_hash_lookup(GLOBAL_DCONTEXT, sigfd_table, src);
    if (pipe != NULL) {
        pipe->refcount++;
        generic_hash_add(GLOBAL_DCONTEXT, sigfd_table, dst, (void *) pipe);
    }
    TABLE_RWLOCK(sigfd_table, write, unlock);
}

void
signal_handle_close(dcontext_t *dcontext, file_t fd)
{
    sigfd_pipe_t *pipe;
    TABLE_RWLOCK(sigfd_table, read, lock);
    pipe = (sigfd_pipe_t *) generic_hash_lookup(GLOBAL_DCONTEXT, sigfd_table, fd);
    TABLE_RWLOCK(sigfd_table, read, unlock);
    if (pipe == NULL)
        return;
    TABLE_RWLOCK(sigfd_table, write, lock);
    pipe = (sigfd_pipe_t *) generic_hash_lookup(GLOBAL_DCONTEXT, sigfd_table, fd);
    if (pipe != NULL) {
        /* this will call sigfd_pipe_free() */
        generic_hash_remove(GLOBAL_DCONTEXT, sigfd_table, fd);
    }
    TABLE_RWLOCK(sigfd_table, write, unlock);
}
