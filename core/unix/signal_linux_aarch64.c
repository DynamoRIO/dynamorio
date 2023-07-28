/* **********************************************************
 * Copyright (c) 2017-2020 Google, Inc.  All rights reserved.
 * Copyright (c) 2016 ARM Limited. All rights reserved.
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

/***************************************************************************
 * signal_linux_aarch64.c - signal code for arm64 Linux
 */

#include "signal_private.h" /* pulls in globals.h for us, in right order */

#ifndef LINUX
#    error Linux-only
#endif

#ifndef AARCH64
#    error AArch64-only
#endif

#include "arch.h"

void
save_fpstate(dcontext_t *dcontext, sigframe_rt_t *frame)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

#ifdef DEBUG
void
dump_sigcontext(dcontext_t *dcontext, sigcontext_t *sc)
{
    int i;
    for (i = 0; i <= DR_REG_X30 - DR_REG_X0; i++)
        LOG(THREAD, LOG_ASYNCH, 1, "\tx%-2d    = " PFX "\n", i, sc->regs[i]);
    LOG(THREAD, LOG_ASYNCH, 1, "\tsp     = " PFX "\n", sc->sp);
    LOG(THREAD, LOG_ASYNCH, 1, "\tpc     = " PFX "\n", sc->pc);
    LOG(THREAD, LOG_ASYNCH, 1, "\tpstate = " PFX "\n", sc->pstate);
}
#endif /* DEBUG */

void
sigcontext_to_mcontext_simd(priv_mcontext_t *mc, sig_full_cxt_t *sc_full)
{
    struct fpsimd_context *fpc = (struct fpsimd_context *)sc_full->fp_simd_state;
    if (fpc == NULL)
        return;
    ASSERT(fpc->head.magic == FPSIMD_MAGIC);
    ASSERT(fpc->head.size == sizeof(struct fpsimd_context));
    mc->fpsr = fpc->fpsr;
    mc->fpcr = fpc->fpcr;
    ASSERT((sizeof(mc->simd->q) * MCXT_NUM_SIMD_SVE_SLOTS) == sizeof(fpc->vregs));
    memcpy(&mc->simd, &fpc->vregs, sizeof(mc->simd));
    /* TODO i#5365: memcpy(&mc->simd->u32,...)
     * See also sve_context in core/unix/include/sigcontext.h.
     */
}

void
mcontext_to_sigcontext_simd(sig_full_cxt_t *sc_full, priv_mcontext_t *mc)
{
    struct fpsimd_context *fpc = (struct fpsimd_context *)sc_full->fp_simd_state;
    if (fpc == NULL)
        return;
    struct _aarch64_ctx *next = (void *)((char *)fpc + sizeof(struct fpsimd_context));
    fpc->head.magic = FPSIMD_MAGIC;
    fpc->head.size = sizeof(struct fpsimd_context);
    fpc->fpsr = mc->fpsr;
    fpc->fpcr = mc->fpcr;
    ASSERT(sizeof(fpc->vregs) == (sizeof(mc->simd->q) * MCXT_NUM_SIMD_SVE_SLOTS));
    memcpy(&fpc->vregs, &mc->simd, sizeof(fpc->vregs));
    /* TODO i#5365: memcpy(..., &mc->simd->u32)
     * See also sve_context in core/unix/include/sigcontext.h.
     */
    next->magic = 0;
    next->size = 0;
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
