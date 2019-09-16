/* **********************************************************
 * Copyright (c) 2019 Google, Inc.  All rights reserved.
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

#ifndef ASM_CODE_ONLY /* C code */
#    include "tools.h"
#    include "avx512ctx-shared.h"

#    ifndef __AVX512F__
#        error "Build error, should only be added with AVX-512 support."
#    endif

NOINLINE void
marker();
/* asm routines */
void
init_zmm(byte *buf);
void
get_zmm(byte *buf);
void
init_opmask(byte *buf);
void
get_opmask(byte *buf);

NOINLINE void
test1_marker();
NOINLINE void
test2_marker();
/* asm routines */
NOINLINE void
init_zmm(byte *buf);
NOINLINE void
get_zmm(byte *buf);
NOINLINE void
init_opmask(byte *buf);
NOINLINE void
get_opmask(byte *buf);

static void
print_zmm(byte zmm_buf[], byte zmm_ref[])
{
    int zmm_off = 0;
    for (int i = 0; i < NUM_SIMD_REGS; ++i, zmm_off += 64) {
        print("got zmm[%d]:\n", i);
        for (int b = zmm_off; b < zmm_off + 64; ++b)
            print("%x", zmm_buf[b]);
        print("\nref zmm[%d]:\n", i);
        for (int b = zmm_off; b < zmm_off + 64; ++b)
            print("%x", zmm_ref[b]);
        print("\n");
    }
}

static void
print_opmask(byte opmask_buf[], byte opmask_ref[])
{
    int opmask_off = 0;
    for (int i = 0; i < NUM_OPMASK_REGS; ++i, opmask_off += 2) {
        print("got k[%i]:\n", i);
        for (int b = opmask_off; b < opmask_off + 2; ++b)
            print("%x", opmask_buf[b]);
        print("\nref k[%d]:\n", i);
        for (int b = opmask_off; b < opmask_off + 2; ++b)
            print("%x", opmask_ref[b]);
        print("\n");
    }
}

static void
run_avx512_ctx_test(void (*marker)())
{
    byte zmm_buf[NUM_SIMD_REGS * 64];
    byte zmm_ref[NUM_SIMD_REGS * 64];
    byte opmask_buf[NUM_OPMASK_REGS * 2];
    byte opmask_ref[NUM_OPMASK_REGS * 2];
    memset(zmm_buf, 0xde, sizeof(zmm_buf));
    memset(zmm_ref, 0xab, sizeof(zmm_ref));
    /* Even though DynamoRIO is capable of handling AVX512BW wide 64-bit mask registers,
     * we're simplifying the test here and are checking only AVX512F wide 16-bit mask
     * registers. This also lets us run the test in 32-bit mode.
     */
    memset(opmask_buf, 0xde, sizeof(opmask_buf));
    memset(opmask_ref, 0xab, sizeof(opmask_ref));

    init_zmm(zmm_ref);
    init_opmask(opmask_ref);

    (*marker)();

    get_zmm(zmm_buf);
    get_opmask(opmask_buf);

    if (memcmp(zmm_buf, zmm_ref, sizeof(zmm_buf)) != 0) {
#    if VERBOSE
        print_zmm(zmm_buf, zmm_ref);
#    endif
        print("ERROR: wrong zmm value\n");
    }
    if (memcmp(opmask_buf, opmask_ref, sizeof(opmask_buf)) != 0) {
#    if VERBOSE
        print_opmask(opmask_buf, opmask_ref);
#    endif
        print("ERROR: wrong mask value\n");
    }
}

static void
run_avx512_all_tests()
{
    print("Testing code cache context switch\n");
    run_avx512_ctx_test(test1_marker);

    print("Testing clean call context switch\n");
    run_avx512_ctx_test(test2_marker);

    print("Ok\n");
}

int
main()
{
    run_avx512_all_tests();
    return 0;
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
#    include "avx512ctx-shared.h"
/* clang-format off */
START_FILE

#ifdef X64
#    define FRAME_PADDING 0
#else
#    define FRAME_PADDING 0
#endif

#define FUNCNAME test1_marker
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        sub      REG_XSP, FRAME_PADDING
        END_PROLOG

        mov      MARKER_REG, TEST1_MARKER
        mov      MARKER_REG, TEST1_MARKER

        add      REG_XSP, FRAME_PADDING
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME test2_marker
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        sub      REG_XSP, FRAME_PADDING
        END_PROLOG

        mov      MARKER_REG, TEST2_MARKER
        mov      MARKER_REG, TEST2_MARKER

        add      REG_XSP, FRAME_PADDING
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME init_zmm
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      REG_XCX, ARG1
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING
        END_PROLOG

        vmovups  zmm0,  [REG_XCX]
        vmovups  zmm1,  [REG_XCX + 64]
        vmovups  zmm2,  [REG_XCX + 64*2]
        vmovups  zmm3,  [REG_XCX + 64*3]
        vmovups  zmm4,  [REG_XCX + 64*4]
        vmovups  zmm5,  [REG_XCX + 64*5]
        vmovups  zmm6,  [REG_XCX + 64*6]
        vmovups  zmm7,  [REG_XCX + 64*7]
#ifdef X64
        vmovups  zmm8,  [REG_XCX + 64*8]
        vmovups  zmm9,  [REG_XCX + 64*9]
        vmovups  zmm10, [REG_XCX + 64*10]
        vmovups  zmm11, [REG_XCX + 64*11]
        vmovups  zmm12, [REG_XCX + 64*12]
        vmovups  zmm13, [REG_XCX + 64*13]
        vmovups  zmm14, [REG_XCX + 64*14]
        vmovups  zmm15, [REG_XCX + 64*15]
        vmovups  zmm16, [REG_XCX + 64*16]
        vmovups  zmm17, [REG_XCX + 64*17]
        vmovups  zmm18, [REG_XCX + 64*18]
        vmovups  zmm19, [REG_XCX + 64*19]
        vmovups  zmm20, [REG_XCX + 64*20]
        vmovups  zmm21, [REG_XCX + 64*21]
        vmovups  zmm22, [REG_XCX + 64*22]
        vmovups  zmm23, [REG_XCX + 64*23]
        vmovups  zmm24, [REG_XCX + 64*24]
        vmovups  zmm25, [REG_XCX + 64*25]
        vmovups  zmm26, [REG_XCX + 64*26]
        vmovups  zmm27, [REG_XCX + 64*27]
        vmovups  zmm28, [REG_XCX + 64*28]
        vmovups  zmm29, [REG_XCX + 64*29]
        vmovups  zmm30, [REG_XCX + 64*30]
        vmovups  zmm31, [REG_XCX + 64*31]
#endif

        add      REG_XSP, FRAME_PADDING
        POP_CALLEE_SAVED_REGS()
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME get_zmm
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      REG_XCX, ARG1
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING
        END_PROLOG

       vmovups  [REG_XCX],         zmm0
       vmovups  [REG_XCX + 64*1],  zmm1
       vmovups  [REG_XCX + 64*2],  zmm2
       vmovups  [REG_XCX + 64*3],  zmm3
       vmovups  [REG_XCX + 64*4], zmm4
       vmovups  [REG_XCX + 64*5], zmm5
       vmovups  [REG_XCX + 64*6], zmm6
       vmovups  [REG_XCX + 64*7], zmm7
#ifdef X64
       vmovups  [REG_XCX + 64*8], zmm8
       vmovups  [REG_XCX + 64*9], zmm9
       vmovups  [REG_XCX + 64*10], zmm10
       vmovups  [REG_XCX + 64*11], zmm11
       vmovups  [REG_XCX + 64*12], zmm12
       vmovups  [REG_XCX + 64*13], zmm13
       vmovups  [REG_XCX + 64*14], zmm14
       vmovups  [REG_XCX + 64*15], zmm15
       vmovups  [REG_XCX + 64*16], zmm16
       vmovups  [REG_XCX + 64*17], zmm17
       vmovups  [REG_XCX + 64*18], zmm18
       vmovups  [REG_XCX + 64*19], zmm19
       vmovups  [REG_XCX + 64*20], zmm20
       vmovups  [REG_XCX + 64*21], zmm12
       vmovups  [REG_XCX + 64*22], zmm22
       vmovups  [REG_XCX + 64*23], zmm23
       vmovups  [REG_XCX + 64*24], zmm24
       vmovups  [REG_XCX + 64*25], zmm25
       vmovups  [REG_XCX + 64*26], zmm26
       vmovups  [REG_XCX + 64*27], zmm27
       vmovups  [REG_XCX + 64*28], zmm28
       vmovups  [REG_XCX + 64*29], zmm29
       vmovups  [REG_XCX + 64*30], zmm30
       vmovups  [REG_XCX + 64*31], zmm31
#endif

        add      REG_XSP, FRAME_PADDING
        POP_CALLEE_SAVED_REGS()
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME init_opmask
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      REG_XCX, ARG1
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING
        END_PROLOG

        mov      ax, WORD [REG_XCX]
        kmovw    k0, eax
        mov      ax, WORD [REG_XCX + 2*1]
        kmovw    k1, eax
        mov      ax, WORD [REG_XCX + 2*2]
        kmovw    k2, eax
        mov      ax, WORD [REG_XCX + 2*3]
        kmovw    k3, eax
        mov      ax, WORD [REG_XCX + 2*4]
        kmovw    k4, eax
        mov      ax, WORD [REG_XCX + 2*5]
        kmovw    k5, eax
        mov      ax, WORD [REG_XCX + 2*6]
        kmovw    k6, eax
        mov      ax, WORD [REG_XCX + 2*7]
        kmovw    k7, eax

        add      REG_XSP, FRAME_PADDING
        POP_CALLEE_SAVED_REGS()
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME get_opmask
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      REG_XCX, ARG1
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING
        END_PROLOG

        kmovw    eax, k0
        mov      WORD [REG_XCX], ax
        kmovw    eax, k1
        mov      WORD [REG_XCX + 2*1], ax
        kmovw    eax, k2
        mov      WORD [REG_XCX + 2*2], ax
        kmovw    eax, k3
        mov      WORD [REG_XCX + 2*3], ax
        kmovw    eax, k4
        mov      WORD [REG_XCX + 2*4], ax
        kmovw    eax, k5
        mov      WORD [REG_XCX + 2*5], ax
        kmovw    eax, k6
        mov      WORD [REG_XCX + 2*6], ax
        kmovw    eax, k7
        mov      WORD [REG_XCX + 2*7], ax

        add      REG_XSP, FRAME_PADDING
        POP_CALLEE_SAVED_REGS()
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

END_FILE
/* clang-format on */
#endif
