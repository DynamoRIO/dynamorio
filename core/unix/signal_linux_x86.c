/* **********************************************************
 * Copyright (c) 2011-2017 Google, Inc.  All rights reserved.
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
 * signal_linux_x86.c - Linux and X86 specific signal code
 */

#include "signal_private.h" /* pulls in globals.h for us, in right order */

#ifndef LINUX
# error Linux-only
#endif

#ifndef X86
# error X86-only
#endif

#include "arch.h"

/* We have to dynamically size kernel_xstate_t to account for kernel changes
 * over time.
 */
static size_t xstate_size;
static bool xstate_has_extra_fields;

#define XSTATE_QUERY_SIG SIGILL

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
    kernel_fpxreg_t *st = NULL;
    uint twd = (uint) fxsave->twd;
    uint tag;
    uint ret = 0xffff0000;
    int i;
    for (i = 0 ; i < 8 ; i++) {
        if (TEST(0x1, twd)) {
            st = (kernel_fpxreg_t *) &fxsave->st_space[i*4];

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
convert_fxsave_to_fpstate(kernel_fpstate_t *fpstate,
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
    /* The app's xmm registers may be saved away in priv_mcontext_t, in which
     * case we need to copy those values instead of using what was in
     * the physical xmm registers.
     * Because of this, we can't just execute "xsave".  We still need to
     * execute xgetbv though.  Xsave is very expensive so not worth doing
     * when xgetbv is all we need, so we avoid it unless there are extra fields.
     */
    int i;
    sigcontext_t *sc = get_sigcontext_from_rt_frame(frame);
    kernel_xstate_t *xstate = (kernel_xstate_t *) sc->fpstate;
    if (!preserve_xmm_caller_saved())
        return;
    if (xstate_has_extra_fields) {
        /* Fill in the extra fields first and then clobber xmm+ymm below.
         * We assume that DR's code does not touch this extra state.
         */
        ASSERT(ALIGNED(xstate, AVX_ALIGNMENT));
        /* A processor w/o xsave but w/ extra xstate fields should not exist. */
        ASSERT(proc_has_feature(FEATURE_XSAVE));
        /* XXX i#1312: use xsaveopt if available (need to add FEATURE_XSAVEOPT) */
#ifdef X64
        /* Some assemblers, including on Travis, don't know "xsave64", so we
         * have to use raw bytes for:
         *    48 0f ae 20  xsave64 (%rax)
         */
        asm volatile("mov %0, %%rax; .byte 0x48; .byte 0x0f; .byte 0xae; .byte 0x20"
                     : "=m" (xstate) : : "rax");
#else
        asm volatile("xsave %0" : "=m" (*xstate));
#endif
    }
    if (YMM_ENABLED()) {
        /* all ymm regs are in our mcontext.  the only other thing
         * in xstate is the xgetbv.
         */
        uint bv_high, bv_low;
        dr_xgetbv(&bv_high, &bv_low);
        xstate->xstate_hdr.xstate_bv = (((uint64)bv_high)<<32) | bv_low;
    }
    for (i=0; i<NUM_SIMD_SAVED; i++) {
        /* we assume no padding */
#ifdef X64
        /* __u32 xmm_space[64] */
        memcpy(&sc->fpstate->xmm_space[i*4], &get_mcontext(dcontext)->ymm[i],
               XMM_REG_SIZE);
        if (YMM_ENABLED()) {
            /* i#637: ymm top halves are inside kernel_xstate_t */
            memcpy(&xstate->ymmh.ymmh_space[i * 4],
                   ((void *)&get_mcontext(dcontext)->ymm[i]) + XMM_REG_SIZE,
                   YMMH_REG_SIZE);
        }
#else
        memcpy(&sc->fpstate->_xmm[i], &get_mcontext(dcontext)->ymm[i], XMM_REG_SIZE);
        if (YMM_ENABLED()) {
            /* i#637: ymm top halves are inside kernel_xstate_t */
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
        /* now convert into kernel_fpstate_t form */
        ASSERT(sizeof(kernel_fpstate_t) == sizeof(struct i387_fxsave_struct));
        memcpy(sc->fpstate, &temp->fxsave, sizeof(struct i387_fxsave_struct));
#else
        /* this is "unlazy_fpu" */
        asm volatile( "fxsave %0 ; fnclex"
                      : "=m" (temp->fxsave) );
        /* now convert into kernel_fpstate_t form */
        convert_fxsave_to_fpstate(sc->fpstate, &temp->fxsave);
#endif
    } else {
        /* FIXME NYI: need to convert to fxsave format for sc->fpstate */
        IF_X64(ASSERT_NOT_IMPLEMENTED(false));
        /* this is "unlazy_fpu" */
        asm volatile( "fnsave %0 ; fwait"
                      : "=m" (temp->fsave) );
        /* now convert into kernel_fpstate_t form */
        temp->fsave.status = temp->fsave.swd;
        memcpy(sc->fpstate, &temp->fsave, sizeof(struct i387_fsave_struct));
    }

    save_xmm(dcontext, frame);
}

#ifdef DEBUG
static void
dump_fpstate(dcontext_t *dcontext, kernel_fpstate_t *fp)
{
    int i,j;
# ifdef X64
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
# else
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
# endif
    /* ignore padding */
    if (YMM_ENABLED()) {
        kernel_xstate_t *xstate = (kernel_xstate_t *) fp;
        if (fp->sw_reserved.magic1 == FP_XSTATE_MAGIC1) {
            /* i#718: for 32-bit app on 64-bit OS, the xstate_size in sw_reserved
             * is obtained via cpuid, which is the xstate size of 64-bit arch.
             */
            ASSERT(fp->sw_reserved.extended_size >= sizeof(*xstate));
            ASSERT(TEST(XCR0_AVX, fp->sw_reserved.xstate_bv));
            LOG(THREAD, LOG_ASYNCH, 1, "\txstate_bv = 0x"HEX64_FORMAT_STRING"\n",
                xstate->xstate_hdr.xstate_bv);
            for (i=0; i<NUM_SIMD_SLOTS; i++) {
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
# ifndef X64
    LOG(THREAD, LOG_ASYNCH, 1, "\tes=0x%04x, __esh=0x%04x\n", sc->es, sc->__esh);
    LOG(THREAD, LOG_ASYNCH, 1, "\tds=0x%04x, __dsh=0x%04x\n", sc->ds, sc->__dsh);
# endif
    LOG(THREAD, LOG_ASYNCH, 1, "\txdi="PFX"\n", sc->SC_XDI);
    LOG(THREAD, LOG_ASYNCH, 1, "\txsi="PFX"\n", sc->SC_XSI);
    LOG(THREAD, LOG_ASYNCH, 1, "\txbp="PFX"\n", sc->SC_XBP);
    LOG(THREAD, LOG_ASYNCH, 1, "\txsp="PFX"\n", sc->SC_XSP);
    LOG(THREAD, LOG_ASYNCH, 1, "\txbx="PFX"\n", sc->SC_XBX);
    LOG(THREAD, LOG_ASYNCH, 1, "\txdx="PFX"\n", sc->SC_XDX);
    LOG(THREAD, LOG_ASYNCH, 1, "\txcx="PFX"\n", sc->SC_XCX);
    LOG(THREAD, LOG_ASYNCH, 1, "\txax="PFX"\n", sc->SC_XAX);
# ifdef X64
    LOG(THREAD, LOG_ASYNCH, 1, "\t r8="PFX"\n", sc->r8);
    LOG(THREAD, LOG_ASYNCH, 1, "\t r9="PFX"\n", sc->r8);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr10="PFX"\n", sc->r10);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr11="PFX"\n", sc->r11);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr12="PFX"\n", sc->r12);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr13="PFX"\n", sc->r13);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr14="PFX"\n", sc->r14);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr15="PFX"\n", sc->r15);
# endif
    LOG(THREAD, LOG_ASYNCH, 1, "\ttrapno="PFX"\n", sc->trapno);
    LOG(THREAD, LOG_ASYNCH, 1, "\terr="PFX"\n", sc->err);
    LOG(THREAD, LOG_ASYNCH, 1, "\txip="PFX"\n", sc->SC_XIP);
    LOG(THREAD, LOG_ASYNCH, 1, "\tcs=0x%04x"IF_NOT_X64(", __esh=0x%04x")"\n",
        sc->cs _IF_NOT_X64(sc->__csh));
    LOG(THREAD, LOG_ASYNCH, 1, "\teflags="PFX"\n", sc->SC_XFLAGS);
# ifndef X64
    LOG(THREAD, LOG_ASYNCH, 1, "\tesp_at_signal="PFX"\n", sc->esp_at_signal);
    LOG(THREAD, LOG_ASYNCH, 1, "\tss=0x%04x, __ssh=0x%04x\n", sc->ss, sc->__ssh);
# endif
    if (sc->fpstate == NULL)
        LOG(THREAD, LOG_ASYNCH, 1, "\tfpstate=<NULL>\n");
    else
        dump_fpstate(dcontext, sc->fpstate);
    LOG(THREAD, LOG_ASYNCH, 1, "\toldmask="PFX"\n", sc->oldmask);
    LOG(THREAD, LOG_ASYNCH, 1, "\tcr2="PFX"\n", sc->cr2);
}
#endif /* DEBUG */

void
sigcontext_to_mcontext_simd(priv_mcontext_t *mc, sig_full_cxt_t *sc_full)
{
    sigcontext_t *sc = sc_full->sc;
    if (sc->fpstate != NULL) {
        int i;
        for (i=0; i<NUM_SIMD_SLOTS; i++) {
            memcpy(&mc->ymm[i], &sc->fpstate->IF_X64_ELSE(xmm_space[i*4],_xmm[i]),
                   XMM_REG_SIZE);
        }
        if (YMM_ENABLED()) {
            kernel_xstate_t *xstate = (kernel_xstate_t *) sc->fpstate;
            if (sc->fpstate->sw_reserved.magic1 == FP_XSTATE_MAGIC1) {
                /* i#718: for 32-bit app on 64-bit OS, the xstate_size in sw_reserved
                 * is obtained via cpuid, which is the xstate size of 64-bit arch.
                 */
                ASSERT(sc->fpstate->sw_reserved.extended_size >= sizeof(*xstate));
                ASSERT(TEST(XCR0_AVX, sc->fpstate->sw_reserved.xstate_bv));
                for (i=0; i<NUM_SIMD_SLOTS; i++) {
                    memcpy(&mc->ymm[i].u32[4], &xstate->ymmh.ymmh_space[i*4],
                           YMMH_REG_SIZE);
                }
            }
        }
    }
}

void
mcontext_to_sigcontext_simd(sig_full_cxt_t *sc_full, priv_mcontext_t *mc)
{
    sigcontext_t *sc = sc_full->sc;
    if (sc->fpstate != NULL) {
        int i;
        for (i=0; i<NUM_SIMD_SLOTS; i++) {
            memcpy(&sc->fpstate->IF_X64_ELSE(xmm_space[i*4],_xmm[i]), &mc->ymm[i],
                   XMM_REG_SIZE);
        }
        if (YMM_ENABLED()) {
            kernel_xstate_t *xstate = (kernel_xstate_t *) sc->fpstate;
            if (sc->fpstate->sw_reserved.magic1 == FP_XSTATE_MAGIC1) {
                /* i#718: for 32-bit app on 64-bit OS, the xstate_size in sw_reserved
                 * is obtained via cpuid, which is the xstate size of 64-bit arch.
                 */
                ASSERT(sc->fpstate->sw_reserved.extended_size >= sizeof(*xstate));
                ASSERT(TEST(XCR0_AVX, sc->fpstate->sw_reserved.xstate_bv));
                for (i=0; i<NUM_SIMD_SLOTS; i++) {
                    memcpy(&xstate->ymmh.ymmh_space[i*4], &mc->ymm[i].u32[4],
                           YMMH_REG_SIZE);
                }
            }
        }
    }
}

size_t
signal_frame_extra_size(bool include_alignment)
{
    /* Extra space needed to put the signal frame on the app stack.  We include the
     * size of the extra padding potentially needed to align these structs.  We
     * assume the stack pointer is 4-aligned already, so we over estimate padding
     * size by the alignment minus 4.
     */
    size_t size = YMM_ENABLED() ? xstate_size : sizeof(kernel_fpstate_t);
    if (include_alignment)
        size += (YMM_ENABLED() ? AVX_ALIGNMENT : FPSTATE_ALIGNMENT) - 4;
    return size;
}

/* To handle varying xstate sizes as kernels add more state over time, we query
 * the size by sending ourselves a signal at init time and reading what the
 * kernel saved.  We assume that DR's own code does not touch this state, so
 * that we can update it to the app's latest at delivery time by executing
 * xsave in save_xmm().
 *
 * XXX: If the kernel ever does lazy state saving for any part of the new state
 * and that affects the size, like it does with fpstate, this initial signal
 * state may not match later state.  Currently it seems to be all-or-nothing.
 */
static void
xstate_query_signal_handler(int sig, siginfo_t *siginfo, kernel_ucontext_t *ucxt)
{
    ASSERT_CURIOSITY(sig == XSTATE_QUERY_SIG);
    if (sig == XSTATE_QUERY_SIG) {
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        if (YMM_ENABLED()) {
            ASSERT(sc->fpstate != NULL); /* i#2438: we force-initialized xmm state */
            ASSERT_CURIOSITY(sc->fpstate->sw_reserved.magic1 == FP_XSTATE_MAGIC1);
            LOG(GLOBAL, LOG_ASYNCH, 1, "orig xstate size = " SZFMT"\n", xstate_size);
            if (sc->fpstate->sw_reserved.extended_size != xstate_size) {
                xstate_size = sc->fpstate->sw_reserved.extended_size;
                xstate_has_extra_fields = true;
            }
            LOG(GLOBAL, LOG_ASYNCH, 1, "new xstate size = " SZFMT"\n", xstate_size);
        }
    }
}

void
signal_arch_init(void)
{
    xstate_size = sizeof(kernel_xstate_t) + 4 /* trailing FP_XSTATE_MAGIC2 */;
    if (YMM_ENABLED()) {
        kernel_sigaction_t act, oldact;
        int rc;
        /* i#2438: it's possible that our init code to this point has not yet executed
         * fpu or xmm operations and that thus fpstate will be NULL.  We force it
         * with an explicit xmm ref here.  We mark it "asm volatile" to prevent the
         * compiler from optimizing it away.
         * XXX i#641, i#639: this breaks transparency to some extent until the
         * app uses fpu/xmm but we live with it.
         */
        __asm__ __volatile__("movd %%xmm0, %0" : "=g"(rc));
        memset(&act, 0, sizeof(act));
        set_handler_sigact(&act, XSTATE_QUERY_SIG,
                           (handler_t) xstate_query_signal_handler);
        rc = sigaction_syscall(XSTATE_QUERY_SIG, &act, &oldact);
        ASSERT(rc == 0);
        thread_signal(get_process_id(), get_sys_thread_id(), XSTATE_QUERY_SIG);
        rc = sigaction_syscall(XSTATE_QUERY_SIG, &oldact, NULL);
        ASSERT(rc == 0);
    }
}
