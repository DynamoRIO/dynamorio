/* **********************************************************
 * Copyright (c) 2014-2015 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 * signal_linux_arm.c - Linux and ARM specific signal code
 */

#include "signal_private.h" /* pulls in globals.h for us, in right order */

#ifndef LINUX
# error Linux-only
#endif

#ifndef ARM
# error ARM-only
#endif

#include "arch.h"

/**** floating point support ********************************************/

void
save_fpstate(dcontext_t *dcontext, sigframe_rt_t *frame)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
}


#ifdef DEBUG
void
dump_sigcontext(dcontext_t *dcontext, sigcontext_t *sc)
{
    LOG(THREAD, LOG_ASYNCH, 1, "\tr0  ="PFX"\n", sc->SC_R0);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr1  ="PFX"\n", sc->SC_R1);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr2  ="PFX"\n", sc->SC_R2);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr3  ="PFX"\n", sc->SC_R3);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr4  ="PFX"\n", sc->SC_R4);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr5  ="PFX"\n", sc->SC_R5);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr6  ="PFX"\n", sc->SC_R6);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr7  ="PFX"\n", sc->SC_R7);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr8  ="PFX"\n", sc->SC_R8);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr9  ="PFX"\n", sc->SC_R9);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr10 ="PFX"\n", sc->SC_R10);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr11 ="PFX"\n", sc->SC_R11);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr12 ="PFX"\n", sc->SC_R12);
    LOG(THREAD, LOG_ASYNCH, 1, "\tsp  ="PFX"\n", sc->SC_XSP);
    LOG(THREAD, LOG_ASYNCH, 1, "\tr14 ="PFX"\n", sc->SC_LR);
    LOG(THREAD, LOG_ASYNCH, 1, "\tpc  ="PFX"\n", sc->SC_XIP);
    LOG(THREAD, LOG_ASYNCH, 1, "\tcpsr="PFX"\n", sc->SC_XFLAGS);
    /* XXX: should we take in sig_full_cxt_t to dump SIMD regs? */
}
#endif /* DEBUG */

void
sigcontext_to_mcontext_simd(priv_mcontext_t *mc, sig_full_cxt_t *sc_full)
{
    struct vfp_sigframe *vfp = (struct vfp_sigframe *) sc_full->fp_simd_state;
    ASSERT(sizeof(mc->simd) == sizeof(vfp->ufp.fpregs));
    ASSERT(vfp->magic == VFP_MAGIC);
    ASSERT(vfp->size == sizeof(struct vfp_sigframe));
    memcpy(&mc->simd[0], &vfp->ufp.fpregs[0], sizeof(mc->simd));
}

void
mcontext_to_sigcontext_simd(sig_full_cxt_t *sc_full, priv_mcontext_t *mc)
{
    struct vfp_sigframe *vfp = (struct vfp_sigframe *) sc_full->fp_simd_state;
    ASSERT(sizeof(mc->simd) == sizeof(vfp->ufp.fpregs));
    vfp->magic = VFP_MAGIC;
    vfp->size = sizeof(struct vfp_sigframe);
    memcpy(&vfp->ufp.fpregs[0], &mc->simd[0], sizeof(vfp->ufp.fpregs));
}
