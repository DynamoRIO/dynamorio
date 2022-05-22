/* **********************************************************
 * Copyright (c) 2013-2020 Google, Inc.  All rights reserved.
 * Copyright (c) 2001-2010 VMware, Inc.  All rights reserved.
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
/* Copyright (c) 2001 Hewlett-Packard Company */
/* Copyright (c) 2019 Google, Inc.  All rights reserved. */
/*
 * x86_code_test.c - unit tests for auxiliary asm and some C routines
 */
#include "../globals.h"
#include "../fragment.h"
#include "../dispatch.h"
#include "../monitor.h"
#include "arch.h"
#if defined(UNIX) && defined(X86)
#    include <immintrin.h>
#endif

#if defined(STANDALONE_UNIT_TEST)

#    define CONST_BYTE 0x1f
#    define TEST_STACK_SIZE 4096
/* Align stack to 16 bytes: sufficient for all current architectures. */
byte ALIGN_VAR(16) test_stack[TEST_STACK_SIZE];
static dcontext_t *static_dc;

static void
check_var(byte *var)
{
    EXPECT(*var, CONST_BYTE);
}

static void (*check_var_ptr)(byte *) = check_var;

static void
test_func(dcontext_t *dcontext)
{
    /* i#1577: we want to read the stack without bothering with a separate
     * assembly routine and without getting an uninit var warning from the
     * compiler.  We go through a separate function and avoid compiler analysis
     * of that function via an indirect call.
     */
    byte var;
    check_var_ptr(&var);
    EXPECT((ptr_uint_t)dcontext, (ptr_uint_t)static_dc);
    return;
}

static void
test_call_switch_stack(dcontext_t *dc)
{
    byte *stack_ptr = test_stack + TEST_STACK_SIZE;
    static_dc = dc;
    print_file(STDERR, "testing asm call_switch_stack\n");
    memset(test_stack, CONST_BYTE, sizeof(test_stack));
    call_switch_stack(dc, stack_ptr, (void (*)(void *))test_func, NULL,
                      true /* should return */);
}

static void
test_cpuid()
{
#    ifdef X86
    int cpuid_res[4] = { 0 };
    print_file(STDERR, "testing asm cpuid\n");
    EXPECT(cpuid_supported(), true);
    our_cpuid(cpuid_res, 0, 0); /* get vendor id */
    /* cpuid_res[1..3] stores vendor info like "GenuineIntel" or "AuthenticAMD" for X86 */
    EXPECT_NE(cpuid_res[1], 0);
    EXPECT_NE(cpuid_res[2], 0);
    EXPECT_NE(cpuid_res[3], 0);
#    endif
}

#    if !defined(DR_HOST_NOT_TARGET) && defined(__AVX__)

static void
unit_test_get_ymm_caller_saved()
{
    dr_zmm_t ref_buffer[MCXT_NUM_SIMD_SLOTS];
    dr_zmm_t get_buffer[MCXT_NUM_SIMD_SLOTS];
    ASSERT(sizeof(dr_zmm_t) == ZMM_REG_SIZE);
    uint base = 0x78abcdef;

    register __m256 ymm0 asm("ymm0");
    register __m256 ymm1 asm("ymm1");
    register __m256 ymm2 asm("ymm2");
    register __m256 ymm3 asm("ymm3");
    register __m256 ymm4 asm("ymm4");
    register __m256 ymm5 asm("ymm5");
    register __m256 ymm6 asm("ymm6");
    register __m256 ymm7 asm("ymm7");
#        ifdef X64
    register __m256 ymm8 asm("ymm8");
    register __m256 ymm9 asm("ymm9");
    register __m256 ymm10 asm("ymm10");
    register __m256 ymm11 asm("ymm11");
    register __m256 ymm12 asm("ymm12");
    register __m256 ymm13 asm("ymm13");
    register __m256 ymm14 asm("ymm14");
    register __m256 ymm15 asm("ymm15");
#        endif

    /* The function get_ymm_caller_saved is intended to be used for AVX (no AVX-512). It
     * doesn't cover extended AVX-512 registers.
     */
    for (int regno = 0; regno < proc_num_simd_sse_avx_registers(); ++regno) {
        for (int dword = 0; dword < sizeof(dr_ymm_t) / sizeof(uint); ++dword) {
            get_buffer[regno].u32[dword] = 0;
            ref_buffer[regno].u32[dword] = base++;
        }
        memset(&get_buffer[regno].u32[sizeof(dr_ymm_t) / sizeof(uint)], 0,
               sizeof(dr_zmm_t) - sizeof(dr_ymm_t));
        memset(&ref_buffer[regno].u32[sizeof(dr_ymm_t) / sizeof(uint)], 0,
               sizeof(dr_zmm_t) - sizeof(dr_ymm_t));
    }

#        define MAKE_YMM_REG(num) ymm##num
#        define MOVE_TO_YMM(buf, num) \
            asm volatile("vmovdqu %1, %0" : "=v"(MAKE_YMM_REG(num)) : "m"(buf[num]) :);

    MOVE_TO_YMM(ref_buffer, 0)
    MOVE_TO_YMM(ref_buffer, 1)
    MOVE_TO_YMM(ref_buffer, 2)
    MOVE_TO_YMM(ref_buffer, 3)
    MOVE_TO_YMM(ref_buffer, 4)
    MOVE_TO_YMM(ref_buffer, 5)
    MOVE_TO_YMM(ref_buffer, 6)
    MOVE_TO_YMM(ref_buffer, 7)
#        ifdef X64
    MOVE_TO_YMM(ref_buffer, 8)
    MOVE_TO_YMM(ref_buffer, 9)
    MOVE_TO_YMM(ref_buffer, 10)
    MOVE_TO_YMM(ref_buffer, 11)
    MOVE_TO_YMM(ref_buffer, 12)
    MOVE_TO_YMM(ref_buffer, 13)
    MOVE_TO_YMM(ref_buffer, 14)
    MOVE_TO_YMM(ref_buffer, 15)
#        endif

    get_ymm_caller_saved(get_buffer);

    /* Even though it was experimentally determined that it is not needed, this barrier
     * prevents the compiler from moving SSE code before the call above.
     */
    asm volatile("" ::: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7");
#        ifdef X64
    asm volatile("" ::
                     : "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14",
                       "xmm15");
#        endif

    for (int regno = 0; regno < proc_num_simd_sse_avx_registers(); ++regno) {
        print_file(STDERR, "YMM%d ref\n:", regno);
        dump_buffer_as_bytes(STDERR, &ref_buffer[regno], sizeof(ref_buffer[regno]),
                             DUMP_RAW | DUMP_DWORD);
        print_file(STDERR, "\nYMM%d get\n:", regno);
        dump_buffer_as_bytes(STDERR, &get_buffer[regno], sizeof(get_buffer[regno]),
                             DUMP_RAW | DUMP_DWORD);
        print_file(STDERR, "\n");
    }
    EXPECT(memcmp(ref_buffer, get_buffer,
                  proc_num_simd_sse_avx_registers() * MCXT_SIMD_SLOT_SIZE),
           0);
}

#    endif

#    ifdef __AVX512F__

static void
unit_test_get_zmm_caller_saved()
{
    dr_zmm_t ref_buffer[MCXT_NUM_SIMD_SLOTS];
    dr_zmm_t get_buffer[MCXT_NUM_SIMD_SLOTS];
    ASSERT(sizeof(dr_zmm_t) == ZMM_REG_SIZE);
    uint base = 0x78abcdef;
    ASSERT(ZMM_ENABLED());
    register __m512 zmm0 asm("zmm0");
    register __m512 zmm1 asm("zmm1");
    register __m512 zmm2 asm("zmm2");
    register __m512 zmm3 asm("zmm3");
    register __m512 zmm4 asm("zmm4");
    register __m512 zmm5 asm("zmm5");
    register __m512 zmm6 asm("zmm6");
    register __m512 zmm7 asm("zmm7");
#        ifdef X64
    register __m512 zmm8 asm("zmm8");
    register __m512 zmm9 asm("zmm9");
    register __m512 zmm10 asm("zmm10");
    register __m512 zmm11 asm("zmm11");
    register __m512 zmm12 asm("zmm12");
    register __m512 zmm13 asm("zmm13");
    register __m512 zmm14 asm("zmm14");
    register __m512 zmm15 asm("zmm15");
    register __m512 zmm16 asm("zmm16");
    register __m512 zmm17 asm("zmm17");
    register __m512 zmm18 asm("zmm18");
    register __m512 zmm19 asm("zmm19");
    register __m512 zmm20 asm("zmm20");
    register __m512 zmm21 asm("zmm21");
    register __m512 zmm22 asm("zmm22");
    register __m512 zmm23 asm("zmm23");
    register __m512 zmm24 asm("zmm24");
    register __m512 zmm25 asm("zmm25");
    register __m512 zmm26 asm("zmm26");
    register __m512 zmm27 asm("zmm27");
    register __m512 zmm28 asm("zmm28");
    register __m512 zmm29 asm("zmm29");
    register __m512 zmm30 asm("zmm30");
    register __m512 zmm31 asm("zmm31");
#        endif

    for (int regno = 0; regno < proc_num_simd_registers(); ++regno) {
        for (int dword = 0; dword < sizeof(dr_zmm_t) / sizeof(uint); ++dword) {
            get_buffer[regno].u32[dword] = 0;
            ref_buffer[regno].u32[dword] = base++;
        }
    }

#        define MAKE_ZMM_REG(num) zmm##num
#        define MOVE_TO_ZMM(buf, num) \
            asm volatile("vmovdqu32 %1, %0" : "=v"(MAKE_ZMM_REG(num)) : "m"(buf[num]) :);

    MOVE_TO_ZMM(ref_buffer, 0)
    MOVE_TO_ZMM(ref_buffer, 1)
    MOVE_TO_ZMM(ref_buffer, 2)
    MOVE_TO_ZMM(ref_buffer, 3)
    MOVE_TO_ZMM(ref_buffer, 4)
    MOVE_TO_ZMM(ref_buffer, 5)
    MOVE_TO_ZMM(ref_buffer, 6)
    MOVE_TO_ZMM(ref_buffer, 7)
#        ifdef X64
    MOVE_TO_ZMM(ref_buffer, 8)
    MOVE_TO_ZMM(ref_buffer, 9)
    MOVE_TO_ZMM(ref_buffer, 10)
    MOVE_TO_ZMM(ref_buffer, 11)
    MOVE_TO_ZMM(ref_buffer, 12)
    MOVE_TO_ZMM(ref_buffer, 13)
    MOVE_TO_ZMM(ref_buffer, 14)
    MOVE_TO_ZMM(ref_buffer, 15)
    MOVE_TO_ZMM(ref_buffer, 16)
    MOVE_TO_ZMM(ref_buffer, 17)
    MOVE_TO_ZMM(ref_buffer, 18)
    MOVE_TO_ZMM(ref_buffer, 19)
    MOVE_TO_ZMM(ref_buffer, 20)
    MOVE_TO_ZMM(ref_buffer, 21)
    MOVE_TO_ZMM(ref_buffer, 22)
    MOVE_TO_ZMM(ref_buffer, 23)
    MOVE_TO_ZMM(ref_buffer, 24)
    MOVE_TO_ZMM(ref_buffer, 25)
    MOVE_TO_ZMM(ref_buffer, 26)
    MOVE_TO_ZMM(ref_buffer, 27)
    MOVE_TO_ZMM(ref_buffer, 28)
    MOVE_TO_ZMM(ref_buffer, 29)
    MOVE_TO_ZMM(ref_buffer, 30)
    MOVE_TO_ZMM(ref_buffer, 31)
#        endif

    get_zmm_caller_saved(get_buffer);

    /* Even though it was experimentally determined that it is not needed, this barrier
     * prevents the compiler from moving SSE code before the call above.
     */
    asm volatile("" ::: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7");
#        ifdef X64
    asm volatile("" ::
                     : "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14",
                       "xmm15", "xmm16", "xmm17", "xmm18", "xmm19", "xmm20", "xmm21",
                       "xmm22", "xmm23", "xmm24", "xmm25", "xmm26", "xmm27", "xmm28",
                       "xmm29", "xmm30", "xmm31");
#        endif

    for (int regno = 0; regno < proc_num_simd_registers(); ++regno) {
        print_file(STDERR, "ZMM%d ref\n:", regno);
        dump_buffer_as_bytes(STDERR, &ref_buffer[regno], sizeof(ref_buffer[regno]),
                             DUMP_RAW | DUMP_DWORD);
        print_file(STDERR, "\nZMM%d get\n:", regno);
        dump_buffer_as_bytes(STDERR, &get_buffer[regno], sizeof(get_buffer[regno]),
                             DUMP_RAW | DUMP_DWORD);
        print_file(STDERR, "\n");
    }
    EXPECT(
        memcmp(ref_buffer, get_buffer, proc_num_simd_registers() * MCXT_SIMD_SLOT_SIZE),
        0);
}

static void
unit_test_get_opmask_caller_saved()
{
    /* While DynamoRIO's dr_opmask_t type is 8 bytes, the actual machine register is
     * really only 8 bytes if the processor and OS support AVX512BW. Otherwise it is
     * 2 Bytes.
     */
    dr_opmask_t ref_buffer[MCXT_NUM_OPMASK_SLOTS];
    dr_opmask_t get_buffer[MCXT_NUM_OPMASK_SLOTS];
    ASSERT(sizeof(dr_opmask_t) == OPMASK_AVX512BW_REG_SIZE);
    uint base = 0x0000348e;

#        ifdef __AVX512BW__
    /* i#1312: Modern AVX-512 machines support AVX512BW which extends the OpMask registers
     * to 8 bytes. The right compile flags must then to be used to compile this test, and
     * the type will be __mmask64. Also DynamoRIO's get_opmask_caller_saved has to
     * dynamically switch dependent on a proc_ flag indicating AVX512BW is enabled.
     */
#            error "Unimplemented. Should test using __mmask64 instructions."
#        else
    ASSERT(MCXT_NUM_OPMASK_SLOTS == 8);
    register __mmask16 k0 asm("k0");
    register __mmask16 k1 asm("k1");
    register __mmask16 k2 asm("k2");
    register __mmask16 k3 asm("k3");
    register __mmask16 k4 asm("k4");
    register __mmask16 k5 asm("k5");
    register __mmask16 k6 asm("k6");
    register __mmask16 k7 asm("k7");
#        endif

    for (int regno = 0; regno < proc_num_opmask_registers(); ++regno) {
        get_buffer[regno] = 0;
        ref_buffer[regno] = base++;
    }

#        define MAKE_OPMASK_REG(num) k##num
#        define MOVE_TO_OPMASK(buf, num) \
            asm volatile("kmovw %1, %0" : "=k"(MAKE_OPMASK_REG(num)) : "m"(buf[num]) :);

    MOVE_TO_OPMASK(ref_buffer, 0)
    MOVE_TO_OPMASK(ref_buffer, 1)
    MOVE_TO_OPMASK(ref_buffer, 2)
    MOVE_TO_OPMASK(ref_buffer, 3)
    MOVE_TO_OPMASK(ref_buffer, 4)
    MOVE_TO_OPMASK(ref_buffer, 5)
    MOVE_TO_OPMASK(ref_buffer, 6)
    MOVE_TO_OPMASK(ref_buffer, 7)

    get_opmask_caller_saved(get_buffer);

    /* Barrier, as described in unit_test_get_zmm_caller_saved. */
    asm volatile("" ::: "k0", "k1", "k2", "k3", "k4", "k5", "k6", "k7");

    for (int regno = 0; regno < proc_num_opmask_registers(); ++regno) {
        print_file(STDERR, "K%d ref\n:", regno);
        dump_buffer_as_bytes(STDERR, &ref_buffer[regno], sizeof(ref_buffer[regno]),
                             DUMP_RAW | DUMP_DWORD);
        print_file(STDERR, "\nK%d get\n:", regno);
        dump_buffer_as_bytes(STDERR, &get_buffer[regno], sizeof(get_buffer[regno]),
                             DUMP_RAW | DUMP_DWORD);
        print_file(STDERR, "\n");
    }
    EXPECT(memcmp(ref_buffer, get_buffer, MCXT_NUM_OPMASK_SLOTS * sizeof(dr_opmask_t)),
           0);
}

#    endif

void
unit_test_asm(dcontext_t *dc)
{
    print_file(STDERR, "testing asm\n");
    test_call_switch_stack(dc);
    test_cpuid();
#    if defined(UNIX) && !defined(DR_HOST_NOT_TARGET)
#        ifdef __AVX__
    unit_test_get_ymm_caller_saved();
#        endif
#        ifdef __AVX512F__
    unit_test_get_zmm_caller_saved();
    unit_test_get_opmask_caller_saved();
#        endif
#    endif
}

#endif /* STANDALONE_UNIT_TEST */
