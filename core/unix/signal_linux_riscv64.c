/* **********************************************************
 * Copyright (c) 2022 Rivos, Inc.  All rights reserved.
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
 * * Neither the name of Rivos, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL RIVOS, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/***************************************************************************
 * signal_linux_riscv64.c - signal code for RISC-V Linux
 */

#include "signal_private.h" /* pulls in globals.h for us, in right order */

#ifndef LINUX
#    error Linux-only
#endif

#ifndef RISCV64
#    error RISC-V-only
#endif

#include "arch.h"

void
save_fpstate(dcontext_t *dcontext, sigframe_rt_t *frame)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

#ifdef DEBUG
void
dump_sigcontext(dcontext_t *dcontext, sigcontext_t *sc)
{

    LOG(THREAD, LOG_ASYNCH, 1, "\tpc  = " PFX "\n", sc->sc_regs.pc);
    LOG(THREAD, LOG_ASYNCH, 1, "\tra  = " PFX "\n", sc->sc_regs.ra);
    LOG(THREAD, LOG_ASYNCH, 1, "\tsp  = " PFX "\n", sc->sc_regs.sp);
    LOG(THREAD, LOG_ASYNCH, 1, "\tgp  = " PFX "\n", sc->sc_regs.gp);
    LOG(THREAD, LOG_ASYNCH, 1, "\ttp  = " PFX "\n", sc->sc_regs.tp);
    LOG(THREAD, LOG_ASYNCH, 1, "\tt0  = " PFX "\n", sc->sc_regs.t0);
    LOG(THREAD, LOG_ASYNCH, 1, "\tt1  = " PFX "\n", sc->sc_regs.t1);
    LOG(THREAD, LOG_ASYNCH, 1, "\tt2  = " PFX "\n", sc->sc_regs.t2);
    LOG(THREAD, LOG_ASYNCH, 1, "\ts0  = " PFX "\n", sc->sc_regs.s0);
    LOG(THREAD, LOG_ASYNCH, 1, "\ts1  = " PFX "\n", sc->sc_regs.s1);
    LOG(THREAD, LOG_ASYNCH, 1, "\ta0  = " PFX "\n", sc->sc_regs.a0);
    LOG(THREAD, LOG_ASYNCH, 1, "\ta1  = " PFX "\n", sc->sc_regs.a1);
    LOG(THREAD, LOG_ASYNCH, 1, "\ta2  = " PFX "\n", sc->sc_regs.a2);
    LOG(THREAD, LOG_ASYNCH, 1, "\ta3  = " PFX "\n", sc->sc_regs.a3);
    LOG(THREAD, LOG_ASYNCH, 1, "\ta4  = " PFX "\n", sc->sc_regs.a4);
    LOG(THREAD, LOG_ASYNCH, 1, "\ta5  = " PFX "\n", sc->sc_regs.a5);
    LOG(THREAD, LOG_ASYNCH, 1, "\ta6  = " PFX "\n", sc->sc_regs.a6);
    LOG(THREAD, LOG_ASYNCH, 1, "\ta7  = " PFX "\n", sc->sc_regs.a7);
    LOG(THREAD, LOG_ASYNCH, 1, "\ts2  = " PFX "\n", sc->sc_regs.s2);
    LOG(THREAD, LOG_ASYNCH, 1, "\ts3  = " PFX "\n", sc->sc_regs.s3);
    LOG(THREAD, LOG_ASYNCH, 1, "\ts4  = " PFX "\n", sc->sc_regs.s4);
    LOG(THREAD, LOG_ASYNCH, 1, "\ts5  = " PFX "\n", sc->sc_regs.s5);
    LOG(THREAD, LOG_ASYNCH, 1, "\ts6  = " PFX "\n", sc->sc_regs.s6);
    LOG(THREAD, LOG_ASYNCH, 1, "\ts7  = " PFX "\n", sc->sc_regs.s7);
    LOG(THREAD, LOG_ASYNCH, 1, "\ts8  = " PFX "\n", sc->sc_regs.s8);
    LOG(THREAD, LOG_ASYNCH, 1, "\ts9  = " PFX "\n", sc->sc_regs.s9);
    LOG(THREAD, LOG_ASYNCH, 1, "\ts10 = " PFX "\n", sc->sc_regs.s10);
    LOG(THREAD, LOG_ASYNCH, 1, "\ts11 = " PFX "\n", sc->sc_regs.s11);
    LOG(THREAD, LOG_ASYNCH, 1, "\tt3  = " PFX "\n", sc->sc_regs.t3);
    LOG(THREAD, LOG_ASYNCH, 1, "\tt4  = " PFX "\n", sc->sc_regs.t4);
    LOG(THREAD, LOG_ASYNCH, 1, "\tt5  = " PFX "\n", sc->sc_regs.t5);
    LOG(THREAD, LOG_ASYNCH, 1, "\tt6  = " PFX "\n", sc->sc_regs.t6);
}
#endif /* DEBUG */

void
sigcontext_to_mcontext_simd(priv_mcontext_t *mc, sig_full_cxt_t *sc_full)
{
    struct __riscv_d_ext_state *fpc =
        (struct __riscv_d_ext_state *)sc_full->fp_simd_state;
    if (fpc == NULL)
        return;
    mc->fcsr = fpc->fcsr;
    memcpy(&mc->f0, &fpc->f, sizeof(fpc->f));
}

void
mcontext_to_sigcontext_simd(sig_full_cxt_t *sc_full, priv_mcontext_t *mc)
{
    struct __riscv_d_ext_state *fpc =
        (struct __riscv_d_ext_state *)sc_full->fp_simd_state;
    if (fpc == NULL)
        return;
    fpc->fcsr = mc->fcsr;
    memcpy(&fpc->f, &mc->f0, sizeof(fpc->f));
}

size_t
signal_frame_extra_size(bool include_alignment)
{
    return 0;
}

void
signal_arch_init(void)
{
    /* Nothing. */
}
