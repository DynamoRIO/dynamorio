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
 * proc.h - processor implementation specific interfaces
 */

#ifndef _PROC_H_
#define _PROC_H_ 1

#include "proc_api.h"

/* exported for efficient access */
extern size_t cache_line_size;

#define CACHE_LINE_SIZE() cache_line_size

/* xcr0 and xstate_bv feature bits, as actually used by the processor. */
enum {
    /* Component for entire zmm16-zmm31 registers. */
    XCR0_HI16_ZMM = 0x80,
    /* Component for upper half of each of zmm0-zmm15 registers. */
    XCR0_ZMM_HI256 = 0x40,
    XCR0_OPMASK = 0x20,
    /* TODO i#3581: mpx state */
    XCR0_AVX = 0x4,
    XCR0_SSE = 0x2,
    XCR0_FP = 0x1,
};

/* information about a processor */
typedef struct _cpu_info_t {
    /* FIXME i#1551: x86 and arm use different description of cpu models
     * - x86: vendor, family, type, model, steeping,
     * - arm: implementer, architecture, variant, part, revision, model name, hardware.
     */
    uint vendor;
#ifdef AARCHXX
    uint architecture;
    uint sve_vector_length_bytes;
#endif
    uint family;
    uint type;
    uint model;
    uint stepping;
    uint L1_icache_size;
    uint L1_dcache_size;
    uint L2_cache_size;
    /* Feature bits in 4 32-bit values:
     * on X86:
     * - features in edx,
     * - features in ecx,
     * - extended features in edx, and
     * - extended features in ecx
     */
    features_t features;
    /* The brand string is a 48-character, null terminated string.
     * Declare as a 12-element uint so the compiler won't complain
     * when we store GPRs to it.  Initialization is "unknown" .
     */
    uint brand_string[12];
} cpu_info_t;
extern cpu_info_t cpu_info;

void
proc_init(void);

void
proc_init_arch(void);

void
proc_set_cache_size(uint val, uint *dst);

#ifdef AARCHXX
uint
proc_get_architecture(void);
#endif

void
machine_cache_sync(void *pc_start, void *pc_end, bool flush_icache);

/*
 * This function is internal only.
 *
 * Setter function for proc_num_simd_saved(). It should be used in order to change the
 * number of saved SIMD registers, which currently happens once AVX-512 code has been
 * detected.
 */
void
proc_set_num_simd_saved(int num);

/*
 * This function is internal only.
 *
 * Returns the number of SIMD registers, but excluding AVX-512 extended registers. The
 * function is only used internally. It shall be primarily used for xstate, fpstate,
 * or sigcontext state access. For example, the xmm or ymmh fields are always of SSE or
 * AVX size, while the extended AVX-512 register state is stored on top of that.
 * proc_num_simd_registers() and proc_num_simd_saved() are not suitable to use in this
 * case.
 */
int
proc_num_simd_sse_avx_registers(void);

/*
 * This function is internal only.
 *
 * Returns the number of SIMD registers preserved for a context switch, but excluding
 * AVX-512 extended registers. Its usage model is the same as
 * proc_num_simd_sse_avx_registers(), but reflects the actual number of saved registers,
 * the same way as proc_num_simd_saved() does.
 */
int
proc_num_simd_sse_avx_saved(void);

/*
 * This function is internal only.
 *
 * Returns the AVX-512 kmask xstate component's offset in bytes, as reported by CPUID
 * on the system.
 */
int
proc_xstate_area_kmask_offs(void);

/*
 * This function is internal only.
 *
 * Returns the AVX-512 zmm_hi256 xstate component's offset in bytes, as reported by CPUID
 * on the system.
 */
int
proc_xstate_area_zmm_hi256_offs(void);

/*
 * This function is internal only.
 *
 * Returns the AVX-512 hi16_zmm xstate component's offset in bytes, as reported by CPUID
 * on the system.
 */
int
proc_xstate_area_hi16_zmm_offs(void);

#endif /* _PROC_H_ */
