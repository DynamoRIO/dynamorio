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

/* TODO i#2985: Currently, this test doesn't do anything but running scatter/gather
 * sequences in AVX-512 and AVX2 and checking for correctness. This test will get extended
 * to include the future drx_expand_scatter_gather() extension.
 */

#ifndef ASM_CODE_ONLY /* C code */
#    include <stdint.h>
#    include "tools.h"

#    ifndef X86
#        error "This test is x86 specific."
#    endif

void
test_avx512_vpscatterdd(uint32_t *xmm_ymm_zmm, uint32_t *test_idx32_vec,
                        uint32_t *output_sparse_test_buf OUT);
void
test_avx512_vpscatterdq(uint32_t *xmm_ymm_zmm, uint32_t *test_idx32_vec,
                        uint32_t *output_sparse_test_buf OUT);
void
test_avx512_vpscatterqd(uint32_t *xmm_ymm_zmm, uint32_t *test_idx32_vec,
                        uint32_t *output_sparse_test_buf OUT);
void
test_avx512_vpscatterqq(uint32_t *xmm_ymm_zmm, uint32_t *test_idx32_vec,
                        uint32_t *output_sparse_test_buf OUT);
/* Even though this is a floating point instruction, we implictly cast and treat the
 * result vector as a vector of integers and compare the results this way.
 */
void
test_avx512_vscatterdps(uint32_t *xmm_ymm_zmm, uint32_t *test_idx32_vec,
                        uint32_t *output_sparse_test_buf OUT);
/* See comment above. */
void
test_avx512_vscatterdpd(uint32_t *xmm_ymm_zmm, uint32_t *test_idx32_vec,
                        uint32_t *output_sparse_test_buf OUT);
/* See comment above. */
void
test_avx512_vscatterqpd(uint32_t *xmm_ymm_zmm, uint32_t *test_idx32_vec,
                        uint32_t *output_sparse_test_buf OUT);
/* See comment above. */
void
test_avx512_vscatterqps(uint32_t *xmm_ymm_zmm, uint32_t *test_idx32_vec,
                        uint32_t *output_sparse_test_buf OUT);
void
test_avx512_vpgatherdd(uint32_t *ref_sparse_test_buf, uint32_t *test_idx32_vec,
                       uint32_t *output_xmm_ymm_zmm OUT);
void
test_avx512_vpgatherdq(uint32_t *ref_sparse_test_buf, uint32_t *test_idx32_vec,
                       uint32_t *output_xmm_ymm_zmm OUT);
void
test_avx512_vpgatherqd(uint32_t *ref_sparse_test_buf, uint32_t *test_idx64_vec,
                       uint32_t *output_xmm_ymm_zmm OUT);
void
test_avx512_vpgatherqq(uint32_t *ref_sparse_test_buf, uint32_t *test_idx64_vec,
                       uint32_t *output_xmm_ymm_zmm OUT);
void
/* Even though this is a floating point instruction, we implictly cast and treat the
 * result vector as a vector of integers and compare the results this way.
 */
test_avx512_vgatherdps(uint32_t *ref_sparse_test_buf, uint32_t *test_idx32_vec,
                       uint32_t *output_xmm_ymm_zmm OUT);
/* See comment above. */
void
test_avx512_vgatherdpd(uint32_t *ref_sparse_test_buf, uint32_t *test_idx32_vec,
                       uint32_t *output_xmm_ymm_zmm OUT);
/* See comment above. */
void
test_avx512_vgatherqps(uint32_t *ref_sparse_test_buf, uint32_t *test_idx32_vec,
                       uint32_t *output_xmm_ymm_zmm OUT);
/* See comment above. */
void
test_avx512_vgatherqpd(uint32_t *ref_sparse_test_buf, uint32_t *test_idx32_vec,
                       uint32_t *output_xmm_ymm_zmm OUT);
void
test_avx2_vpgatherdd(uint32_t *ref_sparse_test_buf, uint32_t *test_idx32_vec,
                     uint32_t *output_xmm_ymm_zmm OUT);
void
test_avx2_vpgatherdq(uint32_t *ref_sparse_test_buf, uint32_t *test_idx32_vec,
                     uint32_t *output_xmm_ymm_zmm OUT);
void
test_avx2_vpgatherqd(uint32_t *ref_sparse_test_buf, uint32_t *test_idx32_vec,
                     uint32_t *output_xmm_ymm_zmm OUT);
void
test_avx2_vpgatherqq(uint32_t *ref_sparse_test_buf, uint32_t *test_idx32_vec,
                     uint32_t *output_xmm_ymm_zmm OUT);
/* Even though this is a floating point instruction, we implictly cast and treat the
 * result vector as a vector of integers and compare the results this way.
 */
void
test_avx2_vgatherdps(uint32_t *ref_sparse_test_buf, uint32_t *test_idx32_vec,
                     uint32_t *output_xmm_ymm_zmm OUT);
/* See comment above. */
void
test_avx2_vgatherdpd(uint32_t *ref_sparse_test_buf, uint32_t *test_idx32_vec,
                     uint32_t *output_xmm_ymm_zmm OUT);
/* See comment above. */
void
test_avx2_vgatherqps(uint32_t *ref_sparse_test_buf, uint32_t *test_idx32_vec,
                     uint32_t *output_xmm_ymm_zmm OUT);
/* See comment above. */
void
test_avx2_vgatherqpd(uint32_t *ref_sparse_test_buf, uint32_t *test_idx32_vec,
                     uint32_t *output_xmm_ymm_zmm OUT);

#    define SPARSE_FACTOR 4
#    define XMM_REG_SIZE 16
#    define YMM_REG_SIZE 32
#    define ZMM_REG_SIZE 64
#    define CONCAT_XMM_YMM_ZMM_U32 \
        (XMM_REG_SIZE + YMM_REG_SIZE + ZMM_REG_SIZE) / sizeof(uint32_t)
#    define CONCAT_XMM_YMM_U32 (XMM_REG_SIZE + YMM_REG_SIZE) / sizeof(uint32_t)
#    define SPARSE_TEST_BUF_SIZE_U32 SPARSE_FACTOR *ZMM_REG_SIZE / sizeof(uint32_t)

static bool
test_avx512_gather(void (*test_func)(uint32_t *, uint32_t *, uint32_t *),
                   uint32_t *ref_sparse_test_buf, uint32_t *ref_xmm_ymm_zmm,
                   uint32_t *test_idx_vec, uint32_t *output_xmm_ymm_zmm OUT)
{
    memset(output_xmm_ymm_zmm, 0, CONCAT_XMM_YMM_ZMM_U32 * sizeof(uint32_t));
    test_func(ref_sparse_test_buf, test_idx_vec, output_xmm_ymm_zmm);
    if (memcmp(output_xmm_ymm_zmm, ref_xmm_ymm_zmm,
               CONCAT_XMM_YMM_ZMM_U32 * sizeof(uint32_t)) != 0) {
        print("ERROR: gather result does not match\n");
        return false;
    }
    return true;
}

static bool
test_avx2_gather(void (*test_func)(uint32_t *, uint32_t *, uint32_t *),
                 uint32_t *ref_sparse_test_buf, uint32_t *ref_xmm_ymm,
                 uint32_t *test_idx_vec, uint32_t *output_xmm_ymm OUT)
{
    memset(output_xmm_ymm, 0, CONCAT_XMM_YMM_U32 * sizeof(uint32_t));
    test_func(ref_sparse_test_buf, test_idx_vec, output_xmm_ymm);
    if (memcmp(output_xmm_ymm, ref_xmm_ymm, CONCAT_XMM_YMM_U32 * sizeof(uint32_t)) != 0) {
        print("ERROR: gather result does not match\n");
        return false;
    }
    return true;
}

static bool
test_avx512_scatter(void (*test_func)(uint32_t *, uint32_t *, uint32_t *),
                    uint32_t *ref_sparse_test_buf, uint32_t *ref_xmm_ymm_zmm,
                    uint32_t *test_idx_vec, bool check_half, bool check_64bit_values,
                    uint32_t *output_sparse_test_buf OUT)
{
    /* For scatters with maximal 8 indices, only half the sparse array is scattered. */
    int check_size = check_half ? SPARSE_TEST_BUF_SIZE_U32 / 2 : SPARSE_TEST_BUF_SIZE_U32;
    memset(output_sparse_test_buf, 0, SPARSE_TEST_BUF_SIZE_U32 * sizeof(uint32_t));
    test_func(ref_xmm_ymm_zmm, test_idx_vec, output_sparse_test_buf);
    for (int i = 0; i < check_size; i += SPARSE_FACTOR) {
        if (check_64bit_values) {
            if (*(uint64_t *)&output_sparse_test_buf[i] !=
                *(uint64_t *)&ref_sparse_test_buf[i]) {
                print("ERROR: scatter result does not match\n");
                return false;
            }
        } else {
            if (*(uint32_t *)&output_sparse_test_buf[i] !=
                *(uint32_t *)&ref_sparse_test_buf[i]) {
                print("ERROR: scatter result does not match\n");
                return false;
            }
        }
    }
    return true;
}

static bool
test_avx2_avx512_scatter_gather(void)
{
#    if defined(__AVX512F__) || defined(__AVX__)
    uint32_t ref_sparse_test_buf[] = {
        0x0, 0x1, 0xf, 0xf, 0x1, 0x2, 0xf, 0xf, 0x2, 0x3, 0xf, 0xf, 0x3, 0x4, 0xf, 0xf,
        0x4, 0x5, 0xf, 0xf, 0x5, 0x6, 0xf, 0xf, 0x6, 0x7, 0xf, 0xf, 0x7, 0x8, 0xf, 0xf,
        0x8, 0x9, 0xf, 0xf, 0x9, 0xa, 0xf, 0xf, 0xa, 0xb, 0xf, 0xf, 0xb, 0xc, 0xf, 0xf,
        0xc, 0xd, 0xf, 0xf, 0xd, 0xe, 0xf, 0xf, 0xe, 0xf, 0xf, 0xf, 0xf, 0x0, 0xf, 0xf
    };
    uint32_t ref_idx32_val32_xmm_ymm_zmm[] = { /* xmm */
                                               0x0, 0x1, 0x2, 0x3,
                                               /* ymm */
                                               0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
                                               /* zmm */
                                               0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
                                               0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf
    };
    uint32_t ref_idx32_val64_xmm_ymm_zmm[] = { /* xmm */
                                               0x0, 0x1, 0x1, 0x2,
                                               /* ymm */
                                               0x0, 0x1, 0x1, 0x2, 0x2, 0x3, 0x3, 0x4,
                                               /* zmm */
                                               0x0, 0x1, 0x1, 0x2, 0x2, 0x3, 0x3, 0x4,
                                               0x4, 0x5, 0x5, 0x6, 0x6, 0x7, 0x7, 0x8
    };
    uint32_t ref_idx64_val32_xmm_ymm_zmm[] = { /* xmm */
                                               0x0, 0x1, 0x0, 0x0,
                                               /* ymm */
                                               0x0, 0x1, 0x2, 0x3, 0x0, 0x0, 0x0, 0x0,
                                               /* zmm */
                                               0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
                                               0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
    };
    uint32_t ref_idx64_val64_xmm_ymm_zmm[] = { /* xmm */
                                               0x0, 0x1, 0x1, 0x2,
                                               /* ymm */
                                               0x0, 0x1, 0x1, 0x2, 0x2, 0x3, 0x3, 0x4,
                                               /* zmm */
                                               0x0, 0x1, 0x1, 0x2, 0x2, 0x3, 0x3, 0x4,
                                               0x4, 0x5, 0x5, 0x6, 0x6, 0x7, 0x7, 0x8
    };
    uint32_t test_idx32_vec[] = { 0x0,  0x4,  0x8,  0xc,  0x10, 0x14, 0x18, 0x1c,
                                  0x20, 0x24, 0x28, 0x2c, 0x30, 0x34, 0x38, 0x3c };
    uint32_t test_idx64_vec[] = { 0x0,  0x0, 0x4,  0x0, 0x8,  0x0, 0xc,  0x0,
                                  0x10, 0x0, 0x14, 0x0, 0x18, 0x0, 0x1c, 0x0,
                                  0x20, 0x0, 0x24, 0x0, 0x28, 0x0, 0x2c, 0x0,
                                  0x30, 0x0, 0x34, 0x0, 0x38, 0x0, 0x3c, 0x0 };
    uint32_t output_xmm_ymm_zmm[CONCAT_XMM_YMM_ZMM_U32];
    uint32_t output_sparse_test_buf[SPARSE_TEST_BUF_SIZE_U32];
#    endif
#    ifdef __AVX512F__
    if (!test_avx512_gather(test_avx512_vpgatherdd, ref_sparse_test_buf,
                            ref_idx32_val32_xmm_ymm_zmm, test_idx32_vec,
                            output_xmm_ymm_zmm))
        return false;
    if (!test_avx512_gather(test_avx512_vgatherdps, ref_sparse_test_buf,
                            ref_idx32_val32_xmm_ymm_zmm, test_idx32_vec,
                            output_xmm_ymm_zmm))
        return false;
    if (!test_avx512_gather(test_avx512_vpgatherdq, ref_sparse_test_buf,
                            ref_idx32_val64_xmm_ymm_zmm, test_idx32_vec,
                            output_xmm_ymm_zmm))
        return false;
    if (!test_avx512_gather(test_avx512_vgatherdpd, ref_sparse_test_buf,
                            ref_idx32_val64_xmm_ymm_zmm, test_idx32_vec,
                            output_xmm_ymm_zmm))
        return false;
    if (!test_avx512_gather(test_avx512_vpgatherqd, ref_sparse_test_buf,
                            ref_idx64_val32_xmm_ymm_zmm, test_idx64_vec,
                            output_xmm_ymm_zmm))
        return false;
    if (!test_avx512_gather(test_avx512_vgatherqps, ref_sparse_test_buf,
                            ref_idx64_val32_xmm_ymm_zmm, test_idx64_vec,
                            output_xmm_ymm_zmm))
        return false;
    if (!test_avx512_gather(test_avx512_vpgatherqq, ref_sparse_test_buf,
                            ref_idx64_val64_xmm_ymm_zmm, test_idx64_vec,
                            output_xmm_ymm_zmm))
        return false;
    if (!test_avx512_gather(test_avx512_vgatherqpd, ref_sparse_test_buf,
                            ref_idx64_val64_xmm_ymm_zmm, test_idx64_vec,
                            output_xmm_ymm_zmm))
        return false;
    if (!test_avx512_scatter(test_avx512_vpscatterdd, ref_sparse_test_buf,
                             ref_idx32_val32_xmm_ymm_zmm, test_idx32_vec,
                             false /* check full sparse array */,
                             false /* 32-bit values */, output_sparse_test_buf))
        return false;
    if (!test_avx512_scatter(test_avx512_vscatterdps, ref_sparse_test_buf,
                             ref_idx32_val32_xmm_ymm_zmm, test_idx32_vec,
                             false /* check full sparse array */,
                             false /* 32-bit values */, output_sparse_test_buf))
        return false;
    if (!test_avx512_scatter(test_avx512_vpscatterdq, ref_sparse_test_buf,
                             ref_idx32_val64_xmm_ymm_zmm, test_idx32_vec,
                             true /* check half of sparse array */,
                             true /* 64-bit values */, output_sparse_test_buf))
        return false;
    if (!test_avx512_scatter(test_avx512_vscatterdpd, ref_sparse_test_buf,
                             ref_idx32_val64_xmm_ymm_zmm, test_idx32_vec,
                             true /* check half of sparse array */,
                             true /* 64-bit values */, output_sparse_test_buf))
        return false;
    if (!test_avx512_scatter(test_avx512_vpscatterqd, ref_sparse_test_buf,
                             ref_idx64_val32_xmm_ymm_zmm, test_idx64_vec,
                             true /* check half of sparse array */,
                             false /* 32-bit values */, output_sparse_test_buf))
        return false;
    if (!test_avx512_scatter(test_avx512_vscatterqps, ref_sparse_test_buf,
                             ref_idx64_val32_xmm_ymm_zmm, test_idx64_vec,
                             true /* check half of sparse array */,
                             false /* 32-bit values */, output_sparse_test_buf))
        return false;
    if (!test_avx512_scatter(test_avx512_vpscatterqq, ref_sparse_test_buf,
                             ref_idx64_val64_xmm_ymm_zmm, test_idx64_vec,
                             true /* check half of sparse array */,
                             true /* 64-bit values */, output_sparse_test_buf))
        return false;
    if (!test_avx512_scatter(test_avx512_vscatterqpd, ref_sparse_test_buf,
                             ref_idx64_val64_xmm_ymm_zmm, test_idx64_vec,
                             true /* check half of sparse array */,
                             true /* 64-bit values */, output_sparse_test_buf))
        return false;
#    endif
#    ifdef __AVX__
    if (!test_avx2_gather(test_avx2_vpgatherdd, ref_sparse_test_buf,
                          ref_idx32_val32_xmm_ymm_zmm, test_idx32_vec,
                          output_xmm_ymm_zmm))
        return false;
    if (!test_avx2_gather(test_avx2_vgatherdps, ref_sparse_test_buf,
                          ref_idx32_val32_xmm_ymm_zmm, test_idx32_vec,
                          output_xmm_ymm_zmm))
        return false;
    if (!test_avx2_gather(test_avx2_vpgatherdq, ref_sparse_test_buf,
                          ref_idx32_val64_xmm_ymm_zmm, test_idx32_vec,
                          output_xmm_ymm_zmm))
        return false;
    if (!test_avx2_gather(test_avx2_vgatherdpd, ref_sparse_test_buf,
                          ref_idx32_val64_xmm_ymm_zmm, test_idx32_vec,
                          output_xmm_ymm_zmm))
        return false;
    if (!test_avx2_gather(test_avx2_vpgatherqd, ref_sparse_test_buf,
                          ref_idx64_val32_xmm_ymm_zmm, test_idx64_vec,
                          output_xmm_ymm_zmm))
        return false;
    if (!test_avx2_gather(test_avx2_vgatherqps, ref_sparse_test_buf,
                          ref_idx64_val32_xmm_ymm_zmm, test_idx64_vec,
                          output_xmm_ymm_zmm))
        return false;
    if (!test_avx2_gather(test_avx2_vpgatherqq, ref_sparse_test_buf,
                          ref_idx64_val64_xmm_ymm_zmm, test_idx64_vec,
                          output_xmm_ymm_zmm))
        return false;
    if (!test_avx2_gather(test_avx2_vgatherqpd, ref_sparse_test_buf,
                          ref_idx64_val64_xmm_ymm_zmm, test_idx64_vec,
                          output_xmm_ymm_zmm))
        return false;
#    endif
    return true;
}

int
main(void)
{
    /* AVX and AVX-512 drx_expand_scatter_gather() tests.
     * TODO i#2985: add expand sequences and tests.
     */
    if (test_avx2_avx512_scatter_gather())
        print("AVX2/AVX-512 scatter/gather checks ok\n");

    return 0;
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
/* clang-format off */
START_FILE

#ifdef X64
# define FRAME_PADDING 0
#else
# define FRAME_PADDING 0
#endif

#define JOIN(x, y) x ## y
#define FUNCNAME(opcode) JOIN(test_avx512_, opcode)

#ifdef __AVX512F__
#define TEST_AVX512_GATHER_IDX32_VAL32(funcname, opcode)  @N@\
DECLARE_FUNC_SEH(funcname)                                @N@\
  GLOBAL_LABEL(funcname:)                                 @N@\
        mov        REG_XAX, ARG1                          @N@\
        mov        REG_XCX, ARG3                          @N@\
        mov        REG_XDX, ARG2                          @N@\
        PUSH_CALLEE_SAVED_REGS()                          @N@\
        sub        REG_XSP, FRAME_PADDING                 @N@\
        END_PROLOG                                        @N@\
        vmovdqu32  zmm1, [REG_XDX]                        @N@\
        movw       dx, 0xffff                             @N@\
        kmovw      k1, edx                                @N@\
        opcode     xmm0 {k1}, [REG_XAX + xmm1 * 4]        @N@\
        vmovdqu32  [REG_XCX], xmm0                        @N@\
        kmovw      k1, edx                                @N@\
        opcode     ymm0 {k1}, [REG_XAX + ymm1 * 4]        @N@\
        vmovdqu32  [REG_XCX + 16], ymm0                   @N@\
        kmovw      k1, edx                                @N@\
        opcode     zmm0 {k1}, [REG_XAX + zmm1 * 4]        @N@\
        vmovdqu32  [REG_XCX + 48], zmm0                   @N@\
        add        REG_XSP, FRAME_PADDING                 @N@\
        POP_CALLEE_SAVED_REGS()                           @N@\
        ret                                               @N@\
        END_FUNC(funcname)

#undef OPCODE
#define OPCODE vpgatherdd
TEST_AVX512_GATHER_IDX32_VAL32(FUNCNAME(OPCODE), OPCODE)
#undef OPCODE
#define OPCODE vgatherdps
TEST_AVX512_GATHER_IDX32_VAL32(FUNCNAME(OPCODE), OPCODE)

#define TEST_AVX512_GATHER_IDX32_VAL64(funcname, opcode)  @N@\
DECLARE_FUNC_SEH(funcname)                                @N@\
  GLOBAL_LABEL(funcname:)                                 @N@\
        mov        REG_XAX, ARG1                          @N@\
        mov        REG_XCX, ARG3                          @N@\
        mov        REG_XDX, ARG2                          @N@\
        PUSH_CALLEE_SAVED_REGS()                          @N@\
        sub        REG_XSP, FRAME_PADDING                 @N@\
        END_PROLOG                                        @N@\
        vmovdqu32  zmm1, [REG_XDX]                        @N@\
        movw       dx, 0xffff                             @N@\
        kmovw      k1, edx                                @N@\
        opcode     xmm0 {k1}, [REG_XAX + xmm1 * 4]        @N@\
        vmovdqu32  [REG_XCX], xmm0                        @N@\
        kmovw      k1, edx                                @N@\
        opcode     ymm0 {k1}, [REG_XAX + xmm1 * 4]        @N@\
        vmovdqu32  [REG_XCX + 16], ymm0                   @N@\
        kmovw      k1, edx                                @N@\
        opcode     zmm0 {k1}, [REG_XAX + ymm1 * 4]        @N@\
        vmovdqu32  [REG_XCX + 48], zmm0                   @N@\
        add        REG_XSP, FRAME_PADDING                 @N@\
        POP_CALLEE_SAVED_REGS()                           @N@\
        ret                                               @N@\
        END_FUNC(funcname)

#undef OPCODE
#define OPCODE vpgatherdq
TEST_AVX512_GATHER_IDX32_VAL64(FUNCNAME(OPCODE), OPCODE)
#undef OPCODE
#define OPCODE vgatherdpd
TEST_AVX512_GATHER_IDX32_VAL64(FUNCNAME(OPCODE), OPCODE)

e#define TEST_AVX512_GATHER_IDX64_VAL32(funcname, opcode)  @N@\
DECLARE_FUNC_SEH(funcname)                                 @N@\
  GLOBAL_LABEL(funcname:)                                  @N@\
        mov        REG_XAX, ARG1                           @N@\
        mov        REG_XCX, ARG3                           @N@\
        mov        REG_XDX, ARG2                           @N@\
        PUSH_CALLEE_SAVED_REGS()                           @N@\
        sub        REG_XSP, FRAME_PADDING                  @N@\
        END_PROLOG                                         @N@\
        vmovdqu32  zmm1, [REG_XDX]                         @N@\
        movw       dx, 0xffff                              @N@\
        kmovw      k1, edx                                 @N@\
        opcode     xmm0 {k1}, [REG_XAX + xmm1 * 4]         @N@\
        vmovdqu32  [REG_XCX], xmm0                         @N@\
        kmovw      k1, edx                                 @N@\
        opcode     xmm0 {k1}, [REG_XAX + ymm1 * 4]         @N@\
        vmovdqu32  [REG_XCX + 16], ymm0                    @N@\
        kmovw      k1, edx                                 @N@\
        opcode     ymm0 {k1}, [REG_XAX + zmm1 * 4]         @N@\
        vmovdqu32  [REG_XCX + 48], zmm0                    @N@\
        add        REG_XSP, FRAME_PADDING                  @N@\
        POP_CALLEE_SAVED_REGS()                            @N@\
        ret                                                @N@\
        END_FUNC(funcname)

#undef OPCODE
#define OPCODE vpgatherqd
TEST_AVX512_GATHER_IDX64_VAL32(FUNCNAME(OPCODE), OPCODE)
#undef OPCODE
#define OPCODE vgatherqps
TEST_AVX512_GATHER_IDX64_VAL32(FUNCNAME(OPCODE), OPCODE)

#define TEST_AVX512_GATHER_IDX64_VAL64(funcname, opcode)  @N@\
DECLARE_FUNC_SEH(funcname)                                @N@\
  GLOBAL_LABEL(funcname:)                                 @N@\
        mov        REG_XAX, ARG1                          @N@\
        mov        REG_XCX, ARG3                          @N@\
        mov        REG_XDX, ARG2                          @N@\
        PUSH_CALLEE_SAVED_REGS()                          @N@\
        sub        REG_XSP, FRAME_PADDING                 @N@\
        END_PROLOG                                        @N@\
        vmovdqu32  zmm1, [REG_XDX]                        @N@\
        movw       dx, 0xffff                             @N@\
        kmovw      k1, edx                                @N@\
        vpgatherqq xmm0 {k1}, [REG_XAX + xmm1 * 4]        @N@\
        vmovdqu32  [REG_XCX], xmm0                        @N@\
        kmovw      k1, edx                                @N@\
        vpgatherqq ymm0 {k1}, [REG_XAX + ymm1 * 4]        @N@\
        vmovdqu32  [REG_XCX + 16], ymm0                   @N@\
        kmovw      k1, edx                                @N@\
        vpgatherqq zmm0 {k1}, [REG_XAX + zmm1 * 4]        @N@\
        vmovdqu32  [REG_XCX + 48], zmm0                   @N@\
        add        REG_XSP, FRAME_PADDING                 @N@\
        POP_CALLEE_SAVED_REGS()                           @N@\
        ret                                               @N@\
        END_FUNC(funcname)

#undef OPCODE
#define OPCODE vpgatherqq
TEST_AVX512_GATHER_IDX64_VAL64(FUNCNAME(OPCODE), OPCODE)
#undef OPCODE
#define OPCODE vgatherqpd
TEST_AVX512_GATHER_IDX64_VAL64(FUNCNAME(OPCODE), OPCODE)

#define TEST_AVX512_SCATTER_IDX32_VAL32(funcname, opcode)  @N@\
DECLARE_FUNC_SEH(funcname)                                 @N@\
  GLOBAL_LABEL(funcname:)                                  @N@\
        mov        REG_XAX, ARG1                           @N@\
        mov        REG_XCX, ARG3                           @N@\
        mov        REG_XDX, ARG2                           @N@\
        PUSH_CALLEE_SAVED_REGS()                           @N@\
        sub        REG_XSP, FRAME_PADDING                  @N@\
        END_PROLOG                                         @N@\
        vmovdqu32  zmm1, [REG_XDX]                         @N@\
        movw       dx, 0xffff                              @N@\
        kmovw      k1, edx                                 @N@\
        vmovdqu32  xmm0, [REG_XAX]                         @N@\
        opcode     [REG_XCX + xmm1 * 4] {k1}, xmm0         @N@\
        kmovw      k1, edx                                 @N@\
        vmovdqu32  ymm0, [REG_XAX + 16]                    @N@\
        opcode     [REG_XCX + ymm1 * 4] {k1}, ymm0         @N@\
        vmovdqu32  [REG_XCX + 16], ymm0                    @N@\
        kmovw      k1, edx                                 @N@\
        vmovdqu32  zmm0, [REG_XAX + 48]                    @N@\
        opcode     [REG_XCX + zmm1 * 4] {k1}, zmm0         @N@\
        add        REG_XSP, FRAME_PADDING                  @N@\
        POP_CALLEE_SAVED_REGS()                            @N@\
        ret                                                @N@\
        END_FUNC(funcname)

#undef OPCODE
#define OPCODE vpscatterdd
TEST_AVX512_SCATTER_IDX32_VAL32(FUNCNAME(OPCODE), OPCODE)
#undef OPCODE
#define OPCODE vscatterdps
TEST_AVX512_SCATTER_IDX32_VAL32(FUNCNAME(OPCODE), OPCODE)

#define TEST_AVX512_SCATTER_IDX32_VAL64(funcname, opcode)  @N@\
DECLARE_FUNC_SEH(funcname)                                 @N@\
  GLOBAL_LABEL(funcname:)                                  @N@\
        mov        REG_XAX, ARG1                           @N@\
        mov        REG_XCX, ARG3                           @N@\
        mov        REG_XDX, ARG2                           @N@\
        PUSH_CALLEE_SAVED_REGS()                           @N@\
        sub        REG_XSP, FRAME_PADDING                  @N@\
        END_PROLOG                                         @N@\
        vmovdqu32  zmm1, [REG_XDX]                         @N@\
        movw       dx, 0xffff                              @N@\
        kmovw      k1, edx                                 @N@\
        vmovdqu32  xmm0, [REG_XAX]                         @N@\
        opcode     [REG_XCX + xmm1 * 4] {k1}, xmm0         @N@\
        kmovw      k1, edx                                 @N@\
        vmovdqu32  ymm0, [REG_XAX + 16]                    @N@\
        opcode     [REG_XCX + xmm1 * 4] {k1}, ymm0         @N@\
        vmovdqu32  [REG_XCX + 16], ymm0                    @N@\
        kmovw      k1, edx                                 @N@\
        vmovdqu32  zmm0, [REG_XAX + 48]                    @N@\
        opcode     [REG_XCX + ymm1 * 4] {k1}, zmm0         @N@\
        add        REG_XSP, FRAME_PADDING                  @N@\
        POP_CALLEE_SAVED_REGS()                            @N@\
        ret                                                @N@\
        END_FUNC(funcname)

#undef OPCODE
#define OPCODE vpscatterdq
TEST_AVX512_SCATTER_IDX32_VAL64(FUNCNAME(OPCODE), OPCODE)
#undef OPCODE
#define OPCODE vscatterdpd
TEST_AVX512_SCATTER_IDX32_VAL64(FUNCNAME(OPCODE), OPCODE)

#define TEST_AVX512_SCATTER_IDX64_VAL32(funcname, opcode)  @N@\
DECLARE_FUNC_SEH(funcname)                                 @N@\
  GLOBAL_LABEL(funcname:)                                  @N@\
        mov        REG_XAX, ARG1                           @N@\
        mov        REG_XCX, ARG3                           @N@\
        mov        REG_XDX, ARG2                           @N@\
        PUSH_CALLEE_SAVED_REGS()                           @N@\
        sub        REG_XSP, FRAME_PADDING                  @N@\
        END_PROLOG                                         @N@\
        vmovdqu32  zmm1, [REG_XDX]                         @N@\
        movw       dx, 0xffff                              @N@\
        kmovw      k1, edx                                 @N@\
        vmovdqu32  xmm0, [REG_XAX]                         @N@\
        opcode     [REG_XCX + xmm1 * 4] {k1}, xmm0         @N@\
        kmovw      k1, edx                                 @N@\
        vmovdqu32  ymm0, [REG_XAX + 16]                    @N@\
        opcode     [REG_XCX + ymm1 * 4] {k1}, xmm0         @N@\
        vmovdqu32  [REG_XCX + 16], ymm0                    @N@\
        kmovw      k1, edx                                 @N@\
        vmovdqu32  zmm0, [REG_XAX + 48]                    @N@\
        opcode     [REG_XCX + zmm1 * 4] {k1}, ymm0         @N@\
        add        REG_XSP, FRAME_PADDING                  @N@\
        POP_CALLEE_SAVED_REGS()                            @N@\
        ret                                                @N@\
        END_FUNC(funcname)

#undef OPCODE
#define OPCODE vpscatterqd
TEST_AVX512_SCATTER_IDX64_VAL32(FUNCNAME(OPCODE), OPCODE)
#undef OPCODE
#define OPCODE vscatterqps
TEST_AVX512_SCATTER_IDX64_VAL32(FUNCNAME(OPCODE), OPCODE)

#define TEST_AVX512_SCATTER_IDX64_VAL64(funcname, opcode)  @N@\
DECLARE_FUNC_SEH(funcname)                                 @N@\
  GLOBAL_LABEL(funcname:)                                  @N@\
        mov        REG_XAX, ARG1                           @N@\
        mov        REG_XCX, ARG3                           @N@\
        mov        REG_XDX, ARG2                           @N@\
        PUSH_CALLEE_SAVED_REGS()                           @N@\
        sub        REG_XSP, FRAME_PADDING                  @N@\
        END_PROLOG                                         @N@\
        vmovdqu32  zmm1, [REG_XDX]                         @N@\
        movw       dx, 0xffff                              @N@\
        kmovw      k1, edx                                 @N@\
        vmovdqu32  xmm0, [REG_XAX]                         @N@\
        opcode     [REG_XCX + xmm1 * 4] {k1}, xmm0         @N@\
        kmovw      k1, edx                                 @N@\
        vmovdqu32  ymm0, [REG_XAX + 16]                    @N@\
        opcode     [REG_XCX + ymm1 * 4] {k1}, ymm0         @N@\
        vmovdqu32  [REG_XCX + 16], ymm0                    @N@\
        kmovw      k1, edx                                 @N@\
        vmovdqu32  zmm0, [REG_XAX + 48]                    @N@\
        opcode     [REG_XCX + zmm1 * 4] {k1}, zmm0         @N@\
        add        REG_XSP, FRAME_PADDING                  @N@\
        POP_CALLEE_SAVED_REGS()                            @N@\
        ret                                                @N@\
        END_FUNC(funcname)

#undef OPCODE
#define OPCODE vpscatterqq
TEST_AVX512_SCATTER_IDX64_VAL64(FUNCNAME(OPCODE), OPCODE)
#undef OPCODE
#define OPCODE vscatterqpd
TEST_AVX512_SCATTER_IDX64_VAL64(FUNCNAME(OPCODE), OPCODE)
#endif /* __AVX512F__ */

#undef FUNCNAME
#define FUNCNAME(opcode) JOIN(test_avx2_, opcode)

#ifdef __AVX__
#define TEST_AVX2_GATHER_IDX32_VAL32(funcname, opcode)  @N@\
DECLARE_FUNC_SEH(funcname)                              @N@\
  GLOBAL_LABEL(funcname:)                               @N@\
        mov           REG_XAX, ARG1                     @N@\
        mov           REG_XCX, ARG3                     @N@\
        mov           REG_XDX, ARG2                     @N@\
        PUSH_CALLEE_SAVED_REGS()                        @N@\
        sub           REG_XSP, FRAME_PADDING            @N@\
        END_PROLOG                                      @N@\
        vmovdqu       ymm1, [REG_XDX]                   @N@\
        vpcmpeqd      ymm2, ymm2, ymm2                  @N@\
        opcode        xmm0, [REG_XAX + xmm1 * 4], xmm2  @N@\
        vmovdqu       [REG_XCX], xmm0                   @N@\
        vpcmpeqd      ymm2, ymm2, ymm2                  @N@\
        opcode        ymm0, [REG_XAX + ymm1 * 4], ymm2  @N@\
        vmovdqu       [REG_XCX + 16], ymm0              @N@\
        add           REG_XSP, FRAME_PADDING            @N@\
        POP_CALLEE_SAVED_REGS()                         @N@\
        ret                                             @N@\
        END_FUNC(funcname)

#undef OPCODE
#define OPCODE vpgatherdd
TEST_AVX2_GATHER_IDX32_VAL32(FUNCNAME(OPCODE), OPCODE)
#undef OPCODE
#define OPCODE vgatherdps
TEST_AVX2_GATHER_IDX32_VAL32(FUNCNAME(OPCODE), OPCODE)

#define TEST_AVX2_GATHER_IDX32_VAL64(funcname, opcode)  @N@\
DECLARE_FUNC_SEH(funcname)                              @N@\
  GLOBAL_LABEL(funcname:)                               @N@\
        mov           REG_XAX, ARG1                     @N@\
        mov           REG_XCX, ARG3                     @N@\
        mov           REG_XDX, ARG2                     @N@\
        PUSH_CALLEE_SAVED_REGS()                        @N@\
        sub           REG_XSP, FRAME_PADDING            @N@\
        END_PROLOG                                      @N@\
        vmovdqu       ymm1, [REG_XDX]                   @N@\
        vpcmpeqd      ymm2, ymm2, ymm2                  @N@\
        opcode        xmm0, [REG_XAX + xmm1 * 4], xmm2  @N@\
        vmovdqu       [REG_XCX], xmm0                   @N@\
        vpcmpeqd      ymm2, ymm2, ymm2                  @N@\
        opcode        ymm0, [REG_XAX + xmm1 * 4], ymm2  @N@\
        vmovdqu       [REG_XCX + 16], ymm0              @N@\
        add           REG_XSP, FRAME_PADDING            @N@\
        POP_CALLEE_SAVED_REGS()                         @N@\
        ret                                             @N@\
        END_FUNC(funcname)

#undef OPCODE
#define OPCODE vpgatherdq
TEST_AVX2_GATHER_IDX32_VAL64(FUNCNAME(OPCODE), OPCODE)
#undef OPCODE
#define OPCODE vgatherdpd
TEST_AVX2_GATHER_IDX32_VAL64(FUNCNAME(OPCODE), OPCODE)

#define TEST_AVX2_GATHER_IDX64_VAL32(funcname, opcode)  @N@\
DECLARE_FUNC_SEH(funcname)                              @N@\
  GLOBAL_LABEL(funcname:)                               @N@\
        mov           REG_XAX, ARG1                     @N@\
        mov           REG_XCX, ARG3                     @N@\
        mov           REG_XDX, ARG2                     @N@\
        PUSH_CALLEE_SAVED_REGS()                        @N@\
        sub           REG_XSP, FRAME_PADDING            @N@\
        END_PROLOG                                      @N@\
        vmovdqu       ymm1, [REG_XDX]                   @N@\
        vpcmpeqd      ymm2, ymm2, ymm2                  @N@\
        opcode        xmm0, [REG_XAX + xmm1 * 4], xmm2  @N@\
        vmovdqu       [REG_XCX], xmm0                   @N@\
        vpcmpeqd      ymm2, ymm2, ymm2                  @N@\
        opcode        xmm0, [REG_XAX + ymm1 * 4], xmm2  @N@\
        vmovdqu       [REG_XCX + 16], ymm0              @N@\
        add           REG_XSP, FRAME_PADDING            @N@\
        POP_CALLEE_SAVED_REGS()                         @N@\
        ret                                             @N@\
        END_FUNC(funcname)

#undef OPCODE
#define OPCODE vpgatherqd
TEST_AVX2_GATHER_IDX64_VAL32(FUNCNAME(OPCODE), OPCODE)
#undef OPCODE
#define OPCODE vgatherqps
TEST_AVX2_GATHER_IDX64_VAL32(FUNCNAME(OPCODE), OPCODE)

#define TEST_AVX2_GATHER_IDX64_VAL64(funcname, opcode)  @N@\
DECLARE_FUNC_SEH(funcname)                              @N@\
  GLOBAL_LABEL(funcname:)                               @N@\
        mov           REG_XAX, ARG1                     @N@\
        mov           REG_XCX, ARG3                     @N@\
        mov           REG_XDX, ARG2                     @N@\
        PUSH_CALLEE_SAVED_REGS()                        @N@\
        sub           REG_XSP, FRAME_PADDING            @N@\
        END_PROLOG                                      @N@\
        vmovdqu       ymm1, [REG_XDX]                   @N@\
        vpcmpeqd      ymm2, ymm2, ymm2                  @N@\
        opcode        xmm0, [REG_XAX + xmm1 * 4], xmm2  @N@\
        vmovdqu       [REG_XCX], xmm0                   @N@\
        vpcmpeqd      ymm2, ymm2, ymm2                  @N@\
        opcode        ymm0, [REG_XAX + ymm1 * 4], ymm2  @N@\
        vmovdqu       [REG_XCX + 16], ymm0              @N@\
        add           REG_XSP, FRAME_PADDING            @N@\
        POP_CALLEE_SAVED_REGS()                         @N@\
        ret                                             @N@\
        END_FUNC(funcname)

#undef OPCODE
#define OPCODE vpgatherqq
TEST_AVX2_GATHER_IDX64_VAL64(FUNCNAME(OPCODE), OPCODE)
#undef OPCODE
#define OPCODE vgatherqpd
TEST_AVX2_GATHER_IDX64_VAL64(FUNCNAME(OPCODE), OPCODE)
#endif /* __AVX__ */

END_FILE
/* clang-format on */
#endif
