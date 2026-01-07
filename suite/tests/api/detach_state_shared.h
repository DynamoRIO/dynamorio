/* **********************************************************
 * Copyright (c) 2015-2021 Google, Inc.  All rights reserved.
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

#ifndef _DETACH_STATE_SHARED_H_
#define _DETACH_STATE_SHARED_H_ 1

#define MAKE_HEX_ASM(n) HEX(n)
#define MAKE_HEX(n) 0x##n
#define MAKE_HEX_C(n) MAKE_HEX(n)

#ifdef X86
/**************************************************
 * X86_64
 */
#    if defined(__AVX512F__)
#        define SIMD_REG_SIZE 64
#    elif defined(__AVX__)
#        define SIMD_REG_SIZE 32
#    else
#        define SIMD_REG_SIZE 16
#    endif
#    ifdef X64
#        define NUM_SIMD_SSE_AVX_REGS 16
#        if defined(__AVX512F__)
#            define NUM_SIMD_AVX512_REGS 32
#            define NUM_SIMD_REGS NUM_SIMD_AVX512_REGS
#            define NUM_OPMASK_REGS 8
#            define OPMASK_REG_SIZE 2
#        else
#            define NUM_SIMD_REGS NUM_SIMD_SSE_AVX_REGS
#            define NUM_OPMASK_REGS 0
#            define OPMASK_REG_SIZE 0
#        endif
#    else
#        define NUM_SIMD_SSE_AVX_REGS 8
#        if defined(__AVX512F__)
#            define NUM_SIMD_AVX512_REGS 8
#            define NUM_OPMASK_REGS 8
#            define OPMASK_REG_SIZE 2
#        else
#            define NUM_OPMASK_REGS 0
#            define OPMASK_REG_SIZE 0
#        endif
#        define NUM_SIMD_REGS 8
#    endif

#    ifdef X64
#        define XAX_BASE() 12345678abcdef01
#        define XCX_BASE() 2345678abcdef012
#        define XDX_BASE() 345678abcdef0123
#        define XBX_BASE() 45678abcdef01234
#        define XBP_BASE() 5678abcdef012345
#        define XSI_BASE() 678abcdef0123456
#        define XDI_BASE() 78abcdef01234567
#        define R8_BASE() 1234567890abcdef
#        define R9_BASE() f1234567890abcde
#        define R10_BASE() ef1234567890abcd
#        define R11_BASE() def1234567890abc
#        define R12_BASE() cdef1234567890ab
#        define R13_BASE() bcdef1234567890a
#        define R14_BASE() abcdef1234567890
#        define R15_BASE() 0abcdef123456789
/* ?MMN is formed via ?MM0 << N. */
#        define XMM0_LOW_BASE() 2384626433832795
#        define XMM0_HIGH_BASE() 3141592653589793
#        define YMMH0_LOW_BASE() 0011223344556677
#        define YMMH0_HIGH_BASE() 1122334455667788
#        define ZMMH0_0_BASE() 1112223334445556
#        define ZMMH0_1_BASE() 66777888999aaabb
#        define ZMMH0_2_BASE() bcccdddeeefff121
#        define ZMMH0_3_BASE() 23434565678789a9
#        define OPMASK0_BASE() abcbcdedef0f0424
#    else
#        define XAX_BASE() 12345678
#        define XCX_BASE() 23456780
#        define XDX_BASE() 34567801
#        define XBX_BASE() 45678012
#        define XBP_BASE() 56780123
#        define XSI_BASE() 67801234
#        define XDI_BASE() 78012345
/* TODO i#3160: It will take work to finish porting the detach_state test
 * to 32-bit.
 */
#    endif
#    define XFLAGS_BASE() 00000ed7
#elif defined(AARCH64)
/**************************************************
 * AARCH64
 */
#    define X0_BASE() 0123456789abcdef
#    define X1_BASE() 123456789abcdef0
#    define X2_BASE() 23456789abcdef01 /* We use the bottom byte as a bool. */
#    define X3_BASE() 3456789abcdef012
#    define X4_BASE() 456789abcdef0123
#    define X5_BASE() 56789abcdef01245
#    define X6_BASE() 6789abcdef012345
#    define X7_BASE() 789abcdef0123456
#    define X8_BASE() 89abcdef01234567
#    define X9_BASE() 9abcdef012345678
#    define X10_BASE() abcdef0123456789
#    define X11_BASE() bcdef0123456789a
#    define X12_BASE() cdef0123456789ab
#    define X13_BASE() def0123456789abc
#    define X14_BASE() ef0123456789abcd
#    define X15_BASE() f0123456789abcde
#    define X16_BASE() f123456789abcdef
#    define X17_BASE() f23456789abcdef0
#    define X18_BASE() f3456789abcdef01
#    define X19_BASE() f456789abcdef012
#    define X20_BASE() f56789abcdef0123
#    define X21_BASE() f6789abcdef01245
#    define X22_BASE() f789abcdef012345
#    define X23_BASE() f89abcdef0123456
#    define X24_BASE() f9abcdef01234567
#    define X25_BASE() fabcdef012345678
#    define X26_BASE() fbcdef0123456789
#    define X27_BASE() fcdef0123456789a
#    define X28_BASE() fdef0123456789ab
#    define X29_BASE() fef0123456789abc
#    define X30_BASE() ff0123456789abcd
/* Status register values must only set bits which are guaranteed by the architecture
 * to be supported by all AArch64 implementations.
 */
#    define NZCV_BASE() 00000000a0000000
#    define FPCR_BASE() 0000000007800000
#    define FPSR_BASE() 000000000000009c

/* SVE SIMD register length is implementation defined and can be any power of two between
 * 16 and 256 bytes. Instead of a full 256 bytes of data for each Z register we use the
 * SVE index instruction to generate a pattern.
 * index takes a start value and an increment and generates an arithmetic progression in
 * the destination register:
 *    Zn.T[0] = start
 *    Zn.T[x] = Zn.T[x - 1] + increment
 *
 * If SVE is not available we generate the same pattern in the 16-byte Vn Neon registers.
 */
#    define FFR_0_BASE() f5
#    define P_ELEMENT_INCREMENT_BASE() 1f
#    define P_REGISTER_DIFFERENCE_BASE() 11
#    define P0_0_BASE() 00
#    define P1_0_BASE() 11
#    define P2_0_BASE() 22
#    define P3_0_BASE() 33
#    define P4_0_BASE() 44
#    define P5_0_BASE() 55
#    define P6_0_BASE() 66
#    define P7_0_BASE() 77
#    define P8_0_BASE() 88
#    define P9_0_BASE() 99
#    define P10_0_BASE() aa
#    define P11_0_BASE() bb
#    define P12_0_BASE() cc
#    define P13_0_BASE() dd
#    define P14_0_BASE() ee
#    define P15_0_BASE() ff
#    define Z_ELEMENT_INCREMENT_BASE() f
#    define Z_REGISTER_DIFFERENCE_BASE() 1
#    define Z0_0_BASE() 00
#    define Z1_0_BASE() 01
#    define Z2_0_BASE() 02
#    define Z3_0_BASE() 03
#    define Z4_0_BASE() 04
#    define Z5_0_BASE() 05
#    define Z6_0_BASE() 06
#    define Z7_0_BASE() 07
#    define Z8_0_BASE() 08
#    define Z9_0_BASE() 09
#    define Z10_0_BASE() 0a
#    define Z11_0_BASE() 0b
#    define Z12_0_BASE() 0c
#    define Z13_0_BASE() 0d
#    define Z14_0_BASE() 0e
#    define Z15_0_BASE() 0f
#    define Z16_0_BASE() 10
#    define Z17_0_BASE() 11
#    define Z18_0_BASE() 12
#    define Z19_0_BASE() 13
#    define Z20_0_BASE() 14
#    define Z21_0_BASE() 15
#    define Z22_0_BASE() 16
#    define Z23_0_BASE() 17
#    define Z24_0_BASE() 18
#    define Z25_0_BASE() 19
#    define Z26_0_BASE() 1a
#    define Z27_0_BASE() 1b
#    define Z28_0_BASE() 1c
#    define Z29_0_BASE() 1d
#    define Z30_0_BASE() 1e
#    define Z31_0_BASE() 1f

/* Macro to calculate SIMD register element values at compile time. */
#    define SIMD_UNIQUE_VAL_BYTE_ELEMENT(start, increment, n) \
        ((start + (increment * n)) & 0xff)

/* Combine 8 byte elements into a single doubleword element starting at byte offset n.*/
#    define SIMD_UNIQUE_VAL_DOUBLEWORD_ELEMENT(start, increment, n)        \
        ((SIMD_UNIQUE_VAL_BYTE_ELEMENT(start, increment, (n + 7)) << 56) | \
         (SIMD_UNIQUE_VAL_BYTE_ELEMENT(start, increment, (n + 6)) << 48) | \
         (SIMD_UNIQUE_VAL_BYTE_ELEMENT(start, increment, (n + 5)) << 40) | \
         (SIMD_UNIQUE_VAL_BYTE_ELEMENT(start, increment, (n + 4)) << 32) | \
         (SIMD_UNIQUE_VAL_BYTE_ELEMENT(start, increment, (n + 3)) << 24) | \
         (SIMD_UNIQUE_VAL_BYTE_ELEMENT(start, increment, (n + 2)) << 16) | \
         (SIMD_UNIQUE_VAL_BYTE_ELEMENT(start, increment, (n + 1)) << 8) |  \
         SIMD_UNIQUE_VAL_BYTE_ELEMENT(start, increment, n))
#endif

#ifndef ASM_CODE_ONLY

/* Detach test functions
 * Any new tests added should be added to both the internal and external detach tests:
 * internal tests: api/detach_state.c
 * external tests: client-interface/external_detach_state.c
 */
void
thread_check_gprs_from_cache(void);

/* The two versions of thread_check_gprs_from_DR*() perform the same test but clobber
 * different registers so between them we get coverage of all GPRs.
 */
void
thread_check_gprs_from_DR1(void);
void
thread_check_gprs_from_DR2(void);

void
thread_check_status_reg_from_cache(void);
void
thread_check_status_reg_from_DR(void);
void
thread_check_xsp_from_cache(void);
void
thread_check_xsp_from_DR(void);
#    ifdef UNIX
void
thread_check_sigstate_from_handler(void);
void
thread_check_sigstate(void);
#    endif

/* Initialize the global detach_state test state.
 * This should be called once before any thread_check_* functions are executed.
 */
void
detach_state_shared_init(void);

/* Clean up the global detach_state test state.
 * This should be called once after all thread_check_* functions are executed.
 */
void
detach_state_shared_cleanup(void);

/* Signal to the running test that it should exit its loop and perform the state check.
 */
void
set_sideline_exit(void);

/* Returns true if the running test is ready to detach. */
bool
get_sideline_ready_for_detach(void);

/* Reset global sideline state.
 * Test apps that call multiple thread_check_* functions should call this in between to
 * reset state.
 */
void
reset_sideline_state(void);

#endif /* ASM_CODE_ONLY */
#endif /* _DETACH_STATE_SHARED_H_ */
