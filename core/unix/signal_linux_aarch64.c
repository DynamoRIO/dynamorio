/* **********************************************************
 * Copyright (c) 2017-2025 Google, Inc.  All rights reserved.
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
    ASSERT_NOT_IMPLEMENTED(false); /* TODO i#1569 */
}

#ifdef DEBUG
/* Representation of quadwords (128 bits) as 2 doublewords. */
typedef union {
    __uint128_t as_128;
    struct {
        uint64 lo;
        uint64 hi;
    } as_2x64;
} reinterpret128_2x64_t;

void
dump_sigcontext(dcontext_t *dcontext, sigcontext_t *sc)
{
#    ifdef DR_HOST_NOT_TARGET
    ASSERT_NOT_REACHED();
#    endif
    LOG(THREAD, LOG_ASYNCH, 1, "\tSignal context:\n");
    int i;
    for (i = 0; i <= DR_REG_X30 - DR_REG_X0; i++)
        LOG(THREAD, LOG_ASYNCH, 1, "\tx%-2d    = " PFX "\n", i, sc->regs[i]);
    LOG(THREAD, LOG_ASYNCH, 1, "\tsp     = " PFX "\n", sc->sp);
    LOG(THREAD, LOG_ASYNCH, 1, "\tpc     = " PFX "\n", sc->pc);
    LOG(THREAD, LOG_ASYNCH, 1, "\tpstate = " PFX "\n", sc->pstate);
    LOG(THREAD, LOG_ASYNCH, 1, "\n");

    struct _aarch64_ctx *head = (struct _aarch64_ctx *)sc->__reserved;
    ASSERT(head->magic == FPSIMD_MAGIC);
    ASSERT(head->size == sizeof(struct fpsimd_context));

    struct fpsimd_context *fpsimd = (struct fpsimd_context *)sc->__reserved;
    LOG(THREAD, LOG_ASYNCH, 2, "\tfpsr 0x%x\n", fpsimd->fpsr);
    LOG(THREAD, LOG_ASYNCH, 2, "\tfpcr 0x%x\n", fpsimd->fpcr);
    reinterpret128_2x64_t vreg;
    for (i = 0; i < MCXT_NUM_SIMD_SVE_SLOTS; i++) {
        vreg.as_128 = fpsimd->vregs[i];
        LOG(THREAD, LOG_ASYNCH, 2, "\tq%-2d  0x%016lx %016lx\n", i, vreg.as_2x64.hi,
            vreg.as_2x64.lo);
    }
    LOG(THREAD, LOG_ASYNCH, 2, "\n");

#    ifndef DR_HOST_NOT_TARGET
    if (proc_has_feature(FEATURE_SVE)) {
        size_t offset = sizeof(struct fpsimd_context);
        struct _aarch64_ctx *next_head = (struct _aarch64_ctx *)(sc->__reserved + offset);
        while (next_head->magic != 0) {
            switch (next_head->magic) {
            case ESR_MAGIC: break;
            case EXTRA_MAGIC: break;
            case SVE_MAGIC: {
                const struct sve_context *sve = (struct sve_context *)(next_head);
                LOG(THREAD, LOG_ASYNCH, 2, "\tSVE vector length %d bytes\n", sve->vl);
                ASSERT(sve->vl == proc_get_vector_length_bytes());
                const unsigned int quads_per_vector = sve_vecquad_from_veclen(sve->vl);
                /* The size and offset macros below are used to access register
                 * state from memory. These are defined in the kernel's
                 * sigcontext.h. For scalable vectors, we typically deal in
                 * units of bytes and quadwords (128 bits). All scalable
                 * vectors are mutiples of 128 bits. For the purposes of signal
                 * context handling, these are the simplest and most consistent
                 * units for calculating sizes and offsets in order to access
                 * scalable register state in memory.
                 */
                LOG(THREAD, LOG_ASYNCH, 2, "\tQuadwords (128 bits) per vector %d\n\n",
                    quads_per_vector);
                LOG(THREAD, LOG_ASYNCH, 2, "\tSVE_SIG_ZREG_SIZE %d\n",
                    SVE_SIG_ZREG_SIZE(quads_per_vector));
                LOG(THREAD, LOG_ASYNCH, 2, "\tSVE_SIG_PREG_SIZE %d\n",
                    SVE_SIG_PREG_SIZE(quads_per_vector));
                LOG(THREAD, LOG_ASYNCH, 2, "\tSVE_SIG_FFR_SIZE  %d\n",
                    SVE_SIG_FFR_SIZE(quads_per_vector));
                LOG(THREAD, LOG_ASYNCH, 2, "\tsve->head.size %d\n\n", sve->head.size);
                LOG(THREAD, LOG_ASYNCH, 2, "\tSVE_SIG_ZREGS_OFFSET %d\n",
                    SVE_SIG_ZREGS_OFFSET);
                LOG(THREAD, LOG_ASYNCH, 2, "\tSVE_SIG_PREGS_OFFSET %d\n",
                    SVE_SIG_PREGS_OFFSET(quads_per_vector));
                LOG(THREAD, LOG_ASYNCH, 2, "\tSVE_SIG_FFR_OFFSET   %d\n\n",
                    SVE_SIG_FFR_OFFSET(quads_per_vector));

                dr_simd_t z;
                int boff; /* Byte offset for each doubleword in a vector. */
                for (i = 0; i < MCXT_NUM_SIMD_SVE_SLOTS; i++) {
                    LOG(THREAD, LOG_ASYNCH, 2, "\tz%-2d  0x", i);
                    for (boff = ((quads_per_vector * 2) - 1); boff >= 0; boff--) {
                        /* We access data in the scalable vector using the kernel's
                         * SVE_SIG_ZREG_OFFSET macro which gives the byte offset into a
                         * vector based on units of 128 bits (quadwords). In this loop we
                         * offset from the start of struct sve_context. We log the data
                         * as 64 bit ints, so 2 per quadword (2 x 64 bits -> 128 bits).
                         *
                         * For example, for a 256 bit vector (2 quadwords (2 x 128 bits),
                         * 4 doublewords (4 x 64 bits)), the byte offset (boff) for each
                         * scalable vector register is:
                         * boff=3  z.u64[3] = sve + SVE_SIG_ZREG_OFFSET + 24
                         * boff=2  z.u64[2] = sve + SVE_SIG_ZREG_OFFSET + 16
                         * boff=1  z.u64[1] = sve + SVE_SIG_ZREG_OFFSET + 8
                         * boff=0  z.u64[0] = sve + SVE_SIG_ZREG_OFFSET
                         *
                         * Note that at present we support little endian only.
                         * All major Linux arm64 kernel distributions are
                         * little-endian.
                         */
                        z.u64[boff] = *((uint64 *)((
                            ((byte *)sve) + (SVE_SIG_ZREG_OFFSET(quads_per_vector, i)) +
                            (boff * 8))));
                        LOG(THREAD, LOG_ASYNCH, 2, "%016lx ", z.u64[boff]);
                    }
                    LOG(THREAD, LOG_ASYNCH, 2, "\n");
                }
                LOG(THREAD, LOG_ASYNCH, 2, "\n");
                /* We access data in predicate and first-fault registers using
                 * the kernel's SVE_SIG_PREG_OFFSET and SVE_SIG_FFR_OFFSET
                 * macros. SVE predicate and FFR registers are an 1/8th the
                 * size of SVE vector registers (1 bit per byte) and are logged
                 * as 32 bit ints.
                 */
                dr_simd_t p;
                for (i = 0; i < MCXT_NUM_SVEP_SLOTS; i++) {
                    p.u32[i] = *((uint32 *)((byte *)sve +
                                            SVE_SIG_PREG_OFFSET(quads_per_vector, i)));
                    LOG(THREAD, LOG_ASYNCH, 2, "\tp%-2d  0x%08lx\n", i, p.u32[i]);
                }
                LOG(THREAD, LOG_ASYNCH, 2, "\n");
                LOG(THREAD, LOG_ASYNCH, 2, "\tFFR  0x%08lx\n\n",
                    *((uint32 *)((byte *)sve + SVE_SIG_FFR_OFFSET(quads_per_vector))));
                break;
            }
            default:
                SYSLOG_INTERNAL_WARNING("%s %d Unknown section found in signal context "
                                        "with magic number 0x%x",
                                        __func__, __LINE__, next_head->magic);
                break;
            }
            offset += next_head->size;
            next_head = (struct _aarch64_ctx *)(sc->__reserved + offset);
        }
    }
#    endif
}
#endif /* DEBUG */

/* Representation of quadword (128 bits) as 4 words, used for SIMD. */
typedef union {
    __uint128_t as_128;
    struct {
        uint32 lowest;
        uint32 lo;
        uint32 hi;
        uint32 highest;
    } as_4x32;
} reinterpret128_4x32_t;

void
sigcontext_to_mcontext_simd(priv_mcontext_t *mc, sig_full_cxt_t *sc_full)
{
#ifdef DR_HOST_NOT_TARGET
    ASSERT_NOT_REACHED();
#endif
    struct fpsimd_context *fpc = (struct fpsimd_context *)sc_full->fp_simd_state;
    if (fpc == NULL)
        return;
    ASSERT(fpc->head.magic == FPSIMD_MAGIC);
    ASSERT(fpc->head.size == sizeof(struct fpsimd_context));
    mc->fpsr = fpc->fpsr;
    mc->fpcr = fpc->fpcr;
    ASSERT((sizeof(mc->simd->q) * MCXT_NUM_SIMD_SVE_SLOTS) == sizeof(fpc->vregs));
    int i;
    for (i = 0; i < MCXT_NUM_SIMD_SVE_SLOTS; i++) {
        memcpy(&mc->simd[i].q, &fpc->vregs[i], sizeof(mc->simd->q));
    }

#ifndef DR_HOST_NOT_TARGET
    if (proc_has_feature(FEATURE_SVE)) {
        size_t offset = sizeof(struct fpsimd_context);
        /* fpsimd_context is always the first section. After that the esr_context,
         * extra_context and sve_context sections can be in any order.
         */
        struct _aarch64_ctx *next_head =
            (struct _aarch64_ctx *)(sc_full->sc->__reserved + offset);
        while (next_head->magic != 0) {
            ASSERT(next_head->magic == ESR_MAGIC || next_head->magic == SVE_MAGIC ||
                   next_head->magic == EXTRA_MAGIC);
            switch (next_head->magic) {
            case ESR_MAGIC: break;
            case EXTRA_MAGIC: break;
            case SVE_MAGIC: {
                const struct sve_context *sve = (struct sve_context *)(next_head);
                ASSERT(sve->vl == proc_get_vector_length_bytes());
                const unsigned int quads_per_vector = sve_vecquad_from_veclen(sve->vl);
                if (sve->head.size != sizeof(struct sve_context)) {
                    for (i = 0; i < MCXT_NUM_SIMD_SVE_SLOTS; i++) {
                        /* SVE specifies that AArch64's SIMD&FP registers
                         * (V0-V31) which hold FP scalars and NEON 128-bit
                         * vectors overlay the bottom 128 bits of the SVE
                         * registers (Z0-Z31). For backward compatibility
                         * reasons, bits 0->127 of Z0-Z31 are always restored
                         * from the corresponding members of fpsimd_context's
                         * vregs and not from sve_context.
                         */
                        memcpy(&mc->simd[i].u32,
                               (byte *)sve + SVE_SIG_ZREG_OFFSET(quads_per_vector, i),
                               SVE_SIG_ZREG_SIZE(quads_per_vector));
                        memcpy(&mc->simd[i].q, &fpc->vregs[i], sizeof(mc->simd->q));
                    }
                    for (i = 0; i < MCXT_NUM_SVEP_SLOTS; i++) {
                        memcpy(&mc->svep[i].u16,
                               (byte *)sve + SVE_SIG_PREG_OFFSET(quads_per_vector, i),
                               SVE_SIG_PREG_SIZE(quads_per_vector));
                    }
                    memcpy(&mc->ffr, (byte *)sve + SVE_SIG_FFR_OFFSET(quads_per_vector),
                           SVE_SIG_FFR_SIZE(quads_per_vector));
                }
                break;
            }
            default:
                SYSLOG_INTERNAL_WARNING("%s %d Unhandled section with magic number 0x%x",
                                        __func__, __LINE__, next_head->magic);
            }
            offset += next_head->size;
            next_head = (struct _aarch64_ctx *)(sc_full->sc->__reserved + offset);
        }
    }
#endif
}

void
mcontext_to_sigcontext_simd(sig_full_cxt_t *sc_full, priv_mcontext_t *mc)
{
#ifdef DR_HOST_NOT_TARGET
    ASSERT_NOT_REACHED();
#endif
    /* sig_full_initialize() will have set the fp_simd_state pointer in the
     * user level machine context's (uc_mcontext) to __reserved.
     */
    struct fpsimd_context *fpc = (struct fpsimd_context *)sc_full->fp_simd_state;
    if (fpc == NULL)
        return;
    fpc->head.magic = FPSIMD_MAGIC;
    fpc->head.size = sizeof(struct fpsimd_context);
    fpc->fpsr = mc->fpsr;
    fpc->fpcr = mc->fpcr;
    ASSERT(sizeof(fpc->vregs) == (sizeof(mc->simd->q) * MCXT_NUM_SIMD_SVE_SLOTS));
    int i;
    for (i = 0; i < MCXT_NUM_SIMD_SVE_SLOTS; i++) {
        memcpy(&fpc->vregs[i], &mc->simd[i].u32[0], sizeof(fpc->vregs[i]));
    }

#ifndef DR_HOST_NOT_TARGET
    if (proc_has_feature(FEATURE_SVE)) {
        struct _aarch64_ctx *esr = (void *)((byte *)fpc + sizeof(struct fpsimd_context));
        esr->magic = ESR_MAGIC;
        esr->size = sizeof(struct esr_context);

        struct sve_context *sve = (void *)((byte *)esr + sizeof(struct esr_context));
        // Set other fields of sve_context to zero. New fields may be added and
        // unexpected values in those fields may cause problems. This is a small
        // struct so the compiler will allocate it to a virtual register and
        // optimise this initialisation.
        const struct sve_context sve_zero = { 0 };
        *sve = sve_zero;
        sve->head.magic = SVE_MAGIC;
        sve->vl = proc_get_vector_length_bytes();
        const uint quads_per_vector = sve_vecquad_from_veclen(sve->vl);
        sve->head.size = ALIGN_FORWARD(SVE_SIG_CONTEXT_SIZE(quads_per_vector), 16);
        for (uint i = 0; i < MCXT_NUM_SIMD_SVE_SLOTS; i++) {
            memcpy((byte *)sve + SVE_SIG_ZREG_OFFSET(quads_per_vector, i),
                   &mc->simd[i].u32, sve->vl);
        }
        for (uint i = 0; i < MCXT_NUM_SVEP_SLOTS; i++) {
            memcpy((byte *)sve + SVE_SIG_PREG_OFFSET(quads_per_vector, i),
                   &mc->svep[i].u16, sve->vl / 8);
        }
        memcpy((byte *)sve + SVE_SIG_FFR_OFFSET(quads_per_vector), &mc->ffr, sve->vl / 8);

        size_t offset = (proc_get_vector_length_bytes() * MCXT_NUM_SIMD_SVE_SLOTS) +
            ((proc_get_vector_length_bytes() / 8) * MCXT_NUM_SVEP_SLOTS) + 16;
        struct _aarch64_ctx *null =
            (void *)((byte *)sve + sizeof(struct sve_context) + offset);
        null->magic = 0;
        null->size = 0;
    }
#endif
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
