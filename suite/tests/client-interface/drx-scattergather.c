/* **********************************************************
 * Copyright (c) 2019-2021 Google, Inc.  All rights reserved.
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

/* TODO i#2985: Currently, this test doesn't do anything but run scatter/gather sequences
 * in AVX-512 and AVX2 and check for correctness. This test will get extended to include
 * the future drx_expand_scatter_gather() extension.
 */

/* Each gather test gathers data from a buffer called sparse_test_buf, runs the xmm, ymm,
 * and zmm versions of the gather instruction, and concatenates the results of each
 * version into a new buffer called xmm_ymm_zmm.
 *
 * Similarly, the scatter tests do the inverse and scatter the xmm, ymm, and zmm data
 * of each instruction from xmm_ymm_zmm into a sparse buffer.
 *
 * The results are compared for correctness.
 */

#ifndef ASM_CODE_ONLY /* C code */
#    include <stdint.h>
#    include <setjmp.h>
#    include "tools.h"
#    include "drx-scattergather-shared.h"

#    ifndef X86
#        error "This test is x86 specific."
#    endif

#    ifdef __AVX512F__
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
#    endif
#    ifdef __AVX__
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
#    endif
#    ifdef __AVX512F__
/* See comment above. */
void
test_avx512_restore_gather_mask_fault(uint32_t *ref_sparse_test_buf,
                                      uint32_t *test_idx32_vec);
/* See comment above. */
void
test_avx512_restore_gather_scratch_xmm_fault(uint32_t *ref_sparse_test_buf,
                                             uint32_t *test_idx32_vec,
                                             uint32_t *scratch_xmm_val);
/* See comment above. */
void
test_avx512_restore_gather_mask_clobber(uint32_t *ref_sparse_test_buf,
                                        uint32_t *test_idx32_vec);
/* See comment above. */
void
test_avx512_restore_gather_mask_update(uint32_t *ref_sparse_test_buf,
                                       uint32_t *test_idx32_vec);
/* See comment above. */
void
test_avx512_restore_scatter_mask_fault(uint32_t *xmm_ymm_zmm, uint32_t *test_idx32_vec,
                                       uint32_t *output_sparse_test_buf OUT);
/* See comment above. */
void
test_avx512_restore_scatter_scratch_xmm_fault(uint32_t *xmm_ymm_zmm,
                                              uint32_t *test_idx32_vec,
                                              uint32_t *output_sparse_test_buf OUT,
                                              uint32_t *scratch_xmm_fault);
/* See comment above. */
void
test_avx512_restore_scatter_mask_clobber(uint32_t *xmm_ymm_zmm, uint32_t *test_idx32_vec,
                                         uint32_t *output_sparse_test_buf OUT);
/* See comment above. */
void
test_avx512_restore_scatter_mask_update(uint32_t *xmm_ymm_zmm, uint32_t *test_idx32_vec,
                                        uint32_t *output_sparse_test_buf OUT);
#    endif
#    ifdef __AVX__
/* See comment above. */
void
test_avx2_restore_gather_mask_update(uint32_t *ref_sparse_test_buf,
                                     uint32_t *test_idx32_vec);
/* See comment above. */
void
test_avx2_restore_gather_scratch_xmm_fault(uint32_t *ref_sparse_test_buf,
                                           uint32_t *test_idx32_vec,
                                           uint32_t *scratch_xmm_val);
#    endif

#    define SPARSE_FACTOR 4
#    define XMM_REG_SIZE 16
#    define YMM_REG_SIZE 32
#    define ZMM_REG_SIZE 64
#    define CONCAT_XMM_YMM_ZMM_U32 \
        ((XMM_REG_SIZE + YMM_REG_SIZE + ZMM_REG_SIZE) / sizeof(uint32_t))
#    define CONCAT_XMM_YMM_U32 ((XMM_REG_SIZE + YMM_REG_SIZE) / sizeof(uint32_t))
#    define SPARSE_TEST_BUF_SIZE_U32 (SPARSE_FACTOR * ZMM_REG_SIZE / sizeof(uint32_t))
#    define POISON 0xf
#    define CPUID_KMASK_COMP 5

#    ifdef UNIX
static SIGJMP_BUF mark;
static uint32_t scratch_xmm_val[] = { 0xdead, 0xbeef, 0x8bad, 0xf00d };
static int
get_xstate_area_offs(int xstate_component)
{
    int offs;
    __asm__ __volatile__("cpuid" : "=b"(offs) : "a"(0xd), "c"(xstate_component));
    return offs;
}

static void
signal_handler_check_k0(int sig, siginfo_t *siginfo, ucontext_t *ucxt)
{
#        ifdef X64
    kernel_xstate_t *xstate = (kernel_xstate_t *)ucxt->uc_mcontext.fpregs;
    __u32 *xstate_kmask_offs =
        (__u32 *)((byte *)xstate + get_xstate_area_offs(CPUID_KMASK_COMP));
    if (xstate_kmask_offs[0] != 0xffff)
        print("ERROR: expected k0 == 0xffff, but is 0x%x\n", xstate_kmask_offs[0]);
#        else
    /* XXX i#1312: it is unclear if and how the components are arranged in
     * 32-bit mode by the kernel.
     */
#        endif
    SIGLONGJMP(mark, 1);
}

static bool
check_scratch_xmm_val(ucontext_t *ucxt, int xmm_reg_id)
{
    kernel_fpstate_t *fp = (kernel_fpstate_t *)ucxt->uc_mcontext.fpregs;
#        ifdef X64
    return fp->xmm_space[xmm_reg_id * 4] == scratch_xmm_val[0] &&
        fp->xmm_space[xmm_reg_id * 4 + 1] == scratch_xmm_val[1] &&
        fp->xmm_space[xmm_reg_id * 4 + 2] == scratch_xmm_val[2] &&
        fp->xmm_space[xmm_reg_id * 4 + 3] == scratch_xmm_val[3];
#        else
    return fp->_xmm[xmm_reg_id].element[0] == scratch_xmm_val[0] &&
        fp->_xmm[xmm_reg_id].element[1] == scratch_xmm_val[1] &&
        fp->_xmm[xmm_reg_id].element[2] == scratch_xmm_val[2] &&
        fp->_xmm[xmm_reg_id].element[3] == scratch_xmm_val[3];
#        endif
}

static void
signal_handler_check_xmm2(int sig, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (!check_scratch_xmm_val(ucxt, 2))
        print("Scratch xmm2 not restored\n");
    SIGLONGJMP(mark, 1);
}

static void
signal_handler_check_xmm3(int sig, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (!check_scratch_xmm_val(ucxt, 3))
        print("Scratch xmm3 not restored\n");
    SIGLONGJMP(mark, 1);
}

static void
signal_handler_check_k1(int sig, siginfo_t *siginfo, ucontext_t *ucxt)
{
#        ifdef X64
    kernel_xstate_t *xstate = (kernel_xstate_t *)ucxt->uc_mcontext.fpregs;
    __u32 *xstate_kmask_offs =
        (__u32 *)((byte *)xstate + get_xstate_area_offs(CPUID_KMASK_COMP));
    /* This check hardcodes an assumption that the rest of the test will make the SIGILL
     * event appear in the mask update code right after the first scalar load, so we
     * expect the lowest mask bit to be cleared.
     */
    if (xstate_kmask_offs[2] != 0xfffe)
        print("ERROR: expected k1 == 0xfffe, but is 0x%x\n", xstate_kmask_offs[2]);
#        else
    /* XXX i#1312: it is unclear if and how the components are arranged in
     * 32-bit mode by the kernel.
     */
#        endif
    SIGLONGJMP(mark, 1);
}

static void
signal_handler_check_ymm1(int sig, siginfo_t *siginfo, ucontext_t *ucxt)
{
    kernel_fpstate_t *fp = (kernel_fpstate_t *)ucxt->uc_mcontext.fpregs;
    /* We are expecting the AVX2 gather instruction, or in this case, the
     * emulated sequence, to clear the mask bit. This depends heavily on the
     * exact behavior of the test, which in order to perform this check,
     * had inserted a ud2 right after the scalar load, but before the mask
     * register update.
     */
#        ifdef X64
    if (((fp->xmm_space[8] >> 31) & 0x1) != 0) {
#        else
    if (((fp->_xmm[2].element[0] >> 31) & 0x1) != 0) {
#        endif
        print("ERROR: expected xmm2[31:30] == 0\n");
    }
    SIGLONGJMP(mark, 1);
}
#    endif

static bool
test_avx512_mask_all_zero(void)
{
#    ifdef UNIX
    /* XXX i#2985: add check to non-UNIX systems. */
    byte k_buf[2] = { 0 };
    byte all_zero_buf[2] = { 0 };
    __asm__ __volatile__("kmovw %%k1, %0" : : "m"(k_buf));
    if (memcmp(k_buf, all_zero_buf, sizeof(k_buf)) != 0)
        return false;
#    endif
    return true;
}

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
    if (!test_avx512_mask_all_zero()) {
        print("ERROR: mask is not zero\n");
        return false;
    }
    print("AVX-512 gather ok\n");
    return true;
}

static bool
test_avx2_gather(void (*test_func)(uint32_t *, uint32_t *, uint32_t *),
                 uint32_t *ref_sparse_test_buf, uint32_t *ref_xmm_ymm,
                 uint32_t *test_idx_vec, uint32_t *output_xmm_ymm OUT)
{
    memset(output_xmm_ymm, 0, CONCAT_XMM_YMM_U32 * sizeof(uint32_t));
#    ifdef UNIX
    byte ymm_buf[32] = { 0 };
    byte zero_buf[32] = { 0 };
    test_func(ref_sparse_test_buf, test_idx_vec, output_xmm_ymm);
    /* XXX i#2985: add check to non-UNIX systems. */
    __asm__ __volatile__("vmovdqu %%ymm2, %0" : : "m"(ymm_buf) : "ymm2");
    if (memcmp(ymm_buf, zero_buf, sizeof(ymm_buf)) != 0) {
        print("ERROR: mask is not zero\n");
        return false;
    }
#    endif
    if (memcmp(output_xmm_ymm, ref_xmm_ymm, CONCAT_XMM_YMM_U32 * sizeof(uint32_t)) != 0) {
        print("ERROR: gather result does not match\n");
        return false;
    }
    print("AVX2 gather ok\n");
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
    if (!test_avx512_mask_all_zero()) {
        print("ERROR: mask is not zero\n");
        return false;
    }
    print("AVX-512 scatter ok\n");
    return true;
}

static bool
test_avx2_avx512_scatter_gather(void)
{
#    if defined(__AVX512F__) || defined(__AVX__)
    /* The data in the uint32_t sparse array is laid out in 2 uint32_t, followed by 2
     * poison 0xf. The values are randomly chosen to be N = 0, N+1, 0xf, 0xf, N+1, N+2,
     * ... The dword value scatter/gather instructions write/read 1 uint32_t, while the
     * qword scatter/gather tests write/read both uint32_t. There are maximal 16 values
     * being scattered/gathered. So the array is 16x4 uint32_t long.
     */
    uint32_t ref_sparse_test_buf[] = {
        0x0, 0x1, POISON, POISON, 0x1, 0x2, POISON, POISON, 0x2, 0x3, POISON, POISON,
        0x3, 0x4, POISON, POISON, 0x4, 0x5, POISON, POISON, 0x5, 0x6, POISON, POISON,
        0x6, 0x7, POISON, POISON, 0x7, 0x8, POISON, POISON, 0x8, 0x9, POISON, POISON,
        0x9, 0xa, POISON, POISON, 0xa, 0xb, POISON, POISON, 0xb, 0xc, POISON, POISON,
        0xc, 0xd, POISON, POISON, 0xd, 0xe, POISON, POISON, 0xe, 0xf, POISON, POISON,
        0xf, 0x0, POISON, POISON
    };
    /* As pointed out above, the ref_xmm_ymm_zmm are the concatenated results (gather
     * tests) or sources (scatter tests) for the xmm, ymm, and zmm versions of the gather
     * or scatter instructions. idx32/64 means a dword/qword index, while val32/64 is a
     * dword/qword value.
     */
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
    /* As pointed out above, the gather tests gather data from ref_sparse_test_buf, and
     * concatenate the results of each xmm, ymm and zmm version of the gather instruction
     * in output_xmm_ymm_zmm. The output is expected to be the ref_xmm_ymm_zmm buffer.
     */
    if (!test_avx512_gather(test_avx512_vpgatherdd, ref_sparse_test_buf,
                            ref_idx32_val32_xmm_ymm_zmm, test_idx32_vec,
                            output_xmm_ymm_zmm))
        return false;
    if (!test_avx512_gather(test_avx512_vgatherdps, ref_sparse_test_buf,
                            ref_idx32_val32_xmm_ymm_zmm, test_idx32_vec,
                            output_xmm_ymm_zmm))
        return false;
#        ifdef X64
    /* FIXME i#2985: we do not yet support expansion of the qword index and value
     * scatter/gather versions in 32-bit mode.
     */
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
#        endif
    /* As pointed out above, the scatter tests scatter data from ref_xmm_ymm_zmm into the
     * array output_sparse_test_buf. It's the inverse of the gather test, so the source
     * data for each xmm, ymm, and zmm scatter instruction is concatenated in
     * ref_xmm_ymm_zmm.
     */
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
#        ifdef X64
    /* FIXME i#2985: we do not yet support expansion of the qword index and value
     * scatter/gather versions in 32-bit mode.
     */
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
#        endif
#    endif
#    ifdef __AVX__
    /* Run in a loop to trigger trace creation and stress things like cloning
     * (i#3962).
     */
    for (int i = 0; i < 100; i++) {
        if (!test_avx2_gather(test_avx2_vpgatherdd, ref_sparse_test_buf,
                              ref_idx32_val32_xmm_ymm_zmm, test_idx32_vec,
                              output_xmm_ymm_zmm))
            return false;
    }
    if (!test_avx2_gather(test_avx2_vgatherdps, ref_sparse_test_buf,
                          ref_idx32_val32_xmm_ymm_zmm, test_idx32_vec,
                          output_xmm_ymm_zmm))
        return false;
#        ifdef X64
    /* FIXME i#2985: we do not yet support expansion of the qword index and value
     * scatter/gather versions in 32-bit mode.
     */
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
#        endif
#    endif
#    ifdef UNIX
#        ifdef __AVX512F__
    intercept_signal(SIGSEGV, (handler_3_t)&signal_handler_check_k0, false);
    /* This index will cause a fault. The index number is arbitrary.*/
    test_idx32_vec[9] = 0xefffffff;
    print("Test restoring the AVX-512 gather scratch mask register upon a fault\n");
    if (SIGSETJMP(mark) == 0) {
        test_avx512_restore_gather_mask_fault(ref_sparse_test_buf, test_idx32_vec);
        print("ERROR: Expected a fault\n");
    }
    print("Test restoring the AVX-512 scatter scratch mask register upon a fault\n");
    if (SIGSETJMP(mark) == 0) {
        test_avx512_restore_scatter_mask_fault(ref_idx32_val32_xmm_ymm_zmm,
                                               test_idx32_vec, output_sparse_test_buf);
        print("ERROR: Expected a fault\n");
    }

    intercept_signal(SIGSEGV, (handler_3_t)&signal_handler_check_xmm2, false);
    print("Test restoring the AVX-512 gather scratch xmm register upon a fault\n");
    if (SIGSETJMP(mark) == 0) {
        test_avx512_restore_gather_scratch_xmm_fault(ref_sparse_test_buf, test_idx32_vec,
                                                     scratch_xmm_val);
        print("ERROR: Expected a fault\n");
    }
    print("Test restoring the AVX-512 scatter scratch xmm register upon a fault\n");
    if (SIGSETJMP(mark) == 0) {
        test_avx512_restore_scatter_scratch_xmm_fault(
            ref_idx32_val32_xmm_ymm_zmm, test_idx32_vec, output_sparse_test_buf,
            scratch_xmm_val);
        print("ERROR: Expected a fault\n");
    }
    /* We will get the SIGILL from a ud2 instruction that the client will insert. */
    intercept_signal(SIGILL, (handler_3_t)&signal_handler_check_k0, false);
    /* Restore to a valid value. */
    test_idx32_vec[9] = 0x24;
    print("Test restoring the AVX-512 gather mask register upon translation events\n");
    if (SIGSETJMP(mark) == 0)
        test_avx512_restore_gather_mask_clobber(ref_sparse_test_buf, test_idx32_vec);
    print("Test restoring the AVX-512 scatter mask register upon translation events\n");
    if (SIGSETJMP(mark) == 0) {
        test_avx512_restore_scatter_mask_clobber(ref_sparse_test_buf, test_idx32_vec,
                                                 output_sparse_test_buf);
    }
    /* We will get the SIGILL from a ud2 instruction that the client will insert. */
    intercept_signal(SIGILL, (handler_3_t)&signal_handler_check_k1, false);
    print("Test updating the AVX-512 gather mask register upon translation events\n");
    if (SIGSETJMP(mark) == 0)
        test_avx512_restore_gather_mask_update(ref_idx32_val32_xmm_ymm_zmm,
                                               test_idx32_vec);
    print("Test updating the AVX-512 scatter mask register upon translation events\n");
    if (SIGSETJMP(mark) == 0) {
        test_avx512_restore_scatter_mask_update(ref_idx32_val32_xmm_ymm_zmm,
                                                test_idx32_vec, output_sparse_test_buf);
    }
#        endif
#    endif
#    ifdef __AVX__
    /* For AVX2 gather, no fault test is needed because this is covered by drreg. We don't
     * even know which scratch xmm drreg will give the expansion sequence to use. This
     * also applies to the mask clobber test.
     */
    /* We will get the SIGILL from a ud2 instruction that the client will insert. */
    intercept_signal(SIGILL, (handler_3_t)&signal_handler_check_ymm1, false);
    print("Test updating the AVX2 gather mask register upon translation events\n");
    if (SIGSETJMP(mark) == 0)
        test_avx2_restore_gather_mask_update(ref_idx32_val32_xmm_ymm_zmm, test_idx32_vec);
    print("Test restoring the AVX2 gather scratch xmm register upon a fault\n");
    intercept_signal(SIGSEGV, (handler_3_t)&signal_handler_check_xmm3, false);
    test_idx32_vec[1] = 0xefffffff;
    if (SIGSETJMP(mark) == 0) {
        test_avx2_restore_gather_scratch_xmm_fault(ref_idx32_val32_xmm_ymm_zmm,
                                                   test_idx32_vec, scratch_xmm_val);
        print("ERROR: Expected a fault\n");
    }
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
#    include "drx-scattergather-shared.h"
/* clang-format off */
START_FILE

#ifdef X64
# define FRAME_PADDING 0
#else
# define FRAME_PADDING 0
#endif

#define JOIN(x, y) x ## y
#define FUNCNAME(opcode) JOIN(test_avx512_, opcode)

/****************************************************************************
 * AVX-512 test functions.
 */

#ifdef __AVX512F__
#define TEST_AVX512_GATHER_IDX32_VAL32(opcode)            @N@\
DECLARE_FUNC_SEH(FUNCNAME(opcode))                        @N@\
  GLOBAL_LABEL(FUNCNAME(opcode):)                         @N@\
        /* uint32_t *ref_sparse_test_buf */               @N@\
        mov        REG_XAX, ARG1                          @N@\
        /* uint32_t *output_xmm_ymm_zmm OUT */            @N@\
        mov        REG_XCX, ARG3                          @N@\
        /* uint32_t *test_idx32_vec */                    @N@\
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
        END_FUNC(FUNCNAME(opcode))

TEST_AVX512_GATHER_IDX32_VAL32(vpgatherdd)
TEST_AVX512_GATHER_IDX32_VAL32(vgatherdps)

#define TEST_AVX512_GATHER_IDX32_VAL64(opcode)            @N@\
DECLARE_FUNC_SEH(FUNCNAME(opcode))                        @N@\
  GLOBAL_LABEL(FUNCNAME(opcode):)                         @N@\
        /* uint32_t *ref_sparse_test_buf */               @N@\
        mov        REG_XAX, ARG1                          @N@\
        /* uint32_t *output_xmm_ymm_zmm OUT */            @N@\
        mov        REG_XCX, ARG3                          @N@\
        /* uint32_t *test_idx32_vec */                    @N@\
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
        END_FUNC(FUNCNAME(opcode))

TEST_AVX512_GATHER_IDX32_VAL64(vpgatherdq)
TEST_AVX512_GATHER_IDX32_VAL64(vgatherdpd)

#define TEST_AVX512_GATHER_IDX64_VAL32(opcode)            @N@\
DECLARE_FUNC_SEH(FUNCNAME(opcode))                        @N@\
  GLOBAL_LABEL(FUNCNAME(opcode):)                         @N@\
        /* uint32_t *ref_sparse_test_buf */               @N@\
        mov        REG_XAX, ARG1                          @N@\
        /* uint32_t *output_xmm_ymm_zmm OUT */            @N@\
        mov        REG_XCX, ARG3                          @N@\
        /* uint32_t *test_idx32_vec */                    @N@\
        mov        REG_XDX, ARG2                          @N@\
        PUSH_CALLEE_SAVED_REGS()                          @N@\
        sub        REG_XSP, FRAME_PADDING                 @N@\
        END_PROLOG                                        @N@\
        vmovdqu32  zmm1, [REG_XDX]                        @N@\
        movw       dx, 0xffff                             @N@\
        kmovw      k1, edx                                @N@\
        /* For the first variant below, with the xmm1
         * index, only the first half of the xmm0 dest
         * is set by the opcode. So we need to
         * explicitly zero the remaining part.
         */                                               @N@\
        vpxor      xmm0, xmm0, xmm0                       @N@\
        opcode     xmm0 {k1}, [REG_XAX + xmm1 * 4]        @N@\
        vmovdqu32  [REG_XCX], xmm0                        @N@\
        kmovw      k1, edx                                @N@\
        opcode     xmm0 {k1}, [REG_XAX + ymm1 * 4]        @N@\
        vmovdqu32  [REG_XCX + 16], ymm0                   @N@\
        kmovw      k1, edx                                @N@\
        opcode     ymm0 {k1}, [REG_XAX + zmm1 * 4]        @N@\
        vmovdqu32  [REG_XCX + 48], zmm0                   @N@\
        add        REG_XSP, FRAME_PADDING                 @N@\
        POP_CALLEE_SAVED_REGS()                           @N@\
        ret                                               @N@\
        END_FUNC(FUNCNAME(opcode))

TEST_AVX512_GATHER_IDX64_VAL32(vpgatherqd)
TEST_AVX512_GATHER_IDX64_VAL32(vgatherqps)

#define TEST_AVX512_GATHER_IDX64_VAL64(opcode)            @N@\
DECLARE_FUNC_SEH(FUNCNAME(opcode))                        @N@\
  GLOBAL_LABEL(FUNCNAME(opcode):)                         @N@\
        /* uint32_t *ref_sparse_test_buf */               @N@\
        mov        REG_XAX, ARG1                          @N@\
        /* uint32_t *output_xmm_ymm_zmm OUT */            @N@\
        mov        REG_XCX, ARG3                          @N@\
        /* uint32_t *test_idx32_vec */                    @N@\
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
        END_FUNC(FUNCNAME(opcode))

TEST_AVX512_GATHER_IDX64_VAL64(vpgatherqq)
TEST_AVX512_GATHER_IDX64_VAL64(vgatherqpd)

#define TEST_AVX512_SCATTER_IDX32_VAL32(opcode)            @N@\
DECLARE_FUNC_SEH(FUNCNAME(opcode))                         @N@\
  GLOBAL_LABEL(FUNCNAME(opcode):)                          @N@\
        /* uint32_t *xmm_ymm_zmm */                        @N@\
        mov        REG_XAX, ARG1                           @N@\
        /* uint32_t *output_sparse_test_buf OUT */         @N@\
        mov        REG_XCX, ARG3                           @N@\
        /* uint32_t *test_idx32_vec */                     @N@\
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
        END_FUNC(FUNCNAME(opcode))

TEST_AVX512_SCATTER_IDX32_VAL32(vpscatterdd)
TEST_AVX512_SCATTER_IDX32_VAL32(vscatterdps)

#define TEST_AVX512_SCATTER_IDX32_VAL64(opcode)            @N@\
DECLARE_FUNC_SEH(FUNCNAME(opcode))                         @N@\
  GLOBAL_LABEL(FUNCNAME(opcode):)                          @N@\
        /* uint32_t *xmm_ymm_zmm */                        @N@\
        mov        REG_XAX, ARG1                           @N@\
        /* uint32_t *output_sparse_test_buf OUT */         @N@\
        mov        REG_XCX, ARG3                           @N@\
        /* uint32_t *test_idx32_vec */                     @N@\
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
        END_FUNC(FUNCNAME(opcode))

TEST_AVX512_SCATTER_IDX32_VAL64(vpscatterdq)
TEST_AVX512_SCATTER_IDX32_VAL64(vscatterdpd)

#define TEST_AVX512_SCATTER_IDX64_VAL32(opcode)            @N@\
DECLARE_FUNC_SEH(FUNCNAME(opcode))                         @N@\
  GLOBAL_LABEL(FUNCNAME(opcode):)                          @N@\
        /* uint32_t *xmm_ymm_zmm */                        @N@\
        mov        REG_XAX, ARG1                           @N@\
        /* uint32_t *output_sparse_test_buf OUT */         @N@\
        mov        REG_XCX, ARG3                           @N@\
        /* uint32_t *test_idx32_vec */                     @N@\
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
        END_FUNC(FUNCNAME(opcode))

TEST_AVX512_SCATTER_IDX64_VAL32(vpscatterqd)
TEST_AVX512_SCATTER_IDX64_VAL32(vscatterqps)

#define TEST_AVX512_SCATTER_IDX64_VAL64(opcode)            @N@\
DECLARE_FUNC_SEH(FUNCNAME(opcode))                         @N@\
  GLOBAL_LABEL(FUNCNAME(opcode):)                          @N@\
        /* uint32_t *xmm_ymm_zmm */                        @N@\
        mov        REG_XAX, ARG1                           @N@\
        /* uint32_t *output_sparse_test_buf OUT */         @N@\
        mov        REG_XCX, ARG3                           @N@\
        /* uint32_t *test_idx32_vec */                     @N@\
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
        END_FUNC(FUNCNAME(opcode))

TEST_AVX512_SCATTER_IDX64_VAL64(vpscatterqq)
TEST_AVX512_SCATTER_IDX64_VAL64(vscatterqpd)

#define TEST_AVX512_GATHER_MASK_RESTORE_EVENT(name, marker) @N@\
DECLARE_FUNC_SEH(FUNCNAME(name))                            @N@\
  GLOBAL_LABEL(FUNCNAME(name):)                             @N@\
        /* uint32_t *ref_sparse_test_buf */                 @N@\
        mov        REG_XAX, ARG1                            @N@\
        /* uint32_t *test_idx32_vec */                      @N@\
        mov        REG_XDX, ARG2                            @N@\
        PUSH_CALLEE_SAVED_REGS()                            @N@\
        sub        REG_XSP, FRAME_PADDING                   @N@\
        END_PROLOG                                          @N@\
        mov        REG_XCX, marker                          @N@\
        mov        REG_XCX, marker                          @N@\
        vmovdqu32  zmm1, [REG_XDX]                          @N@\
        movw       dx, 0xffff                               @N@\
        kmovw      k0, edx                                  @N@\
        kmovw      k1, edx                                  @N@\
        vpgatherdd zmm0 {k1}, [REG_XAX + zmm1 * 4]          @N@\
        add        REG_XSP, FRAME_PADDING                   @N@\
        POP_CALLEE_SAVED_REGS()                             @N@\
        ret                                                 @N@\
        END_FUNC(FUNCNAME(name))

#define TEST_AVX512_SCATTER_MASK_RESTORE_EVENT(name, marker) @N@\
DECLARE_FUNC_SEH(FUNCNAME(name))                             @N@\
  GLOBAL_LABEL(FUNCNAME(name):)                              @N@\
        /* uint32_t *xmm_ymm_zmm */                          @N@\
        mov         REG_XAX, ARG1                            @N@\
        /* uint32_t *output_sparse_test_buf OUT */           @N@\
        mov         REG_XCX, ARG3                            @N@\
        /* uint32_t *test_idx32_vec */                       @N@\
        mov         REG_XDX, ARG2                            @N@\
        PUSH_CALLEE_SAVED_REGS()                             @N@\
        sub         REG_XSP, FRAME_PADDING                   @N@\
        END_PROLOG                                           @N@\
        vmovdqu32   zmm0, [REG_XAX + 48]                     @N@\
        mov         REG_XAX, marker                          @N@\
        mov         REG_XAX, marker                          @N@\
        vmovdqu32   zmm1, [REG_XDX]                          @N@\
        movw        dx, 0xffff                               @N@\
        kmovw       k0, edx                                  @N@\
        kmovw       k1, edx                                  @N@\
        vpscatterdd [REG_XCX + zmm1 * 4] {k1}, zmm0          @N@\
        add         REG_XSP, FRAME_PADDING                   @N@\
        POP_CALLEE_SAVED_REGS()                              @N@\
        ret                                                  @N@\
        END_FUNC(FUNCNAME(name))

/* No marker is needed for the fault test. */
TEST_AVX512_GATHER_MASK_RESTORE_EVENT(restore_gather_mask_fault, 0x0)
TEST_AVX512_SCATTER_MASK_RESTORE_EVENT(restore_scatter_mask_fault, 0x0)
/* These tests depend on markers being present. */
TEST_AVX512_GATHER_MASK_RESTORE_EVENT(restore_gather_mask_clobber,
                                      TEST_AVX512_GATHER_MASK_CLOBBER_MARKER)
TEST_AVX512_GATHER_MASK_RESTORE_EVENT(restore_gather_mask_update,
                                      TEST_AVX512_GATHER_MASK_UPDATE_MARKER)
TEST_AVX512_SCATTER_MASK_RESTORE_EVENT(restore_scatter_mask_clobber,
                                       TEST_AVX512_SCATTER_MASK_CLOBBER_MARKER)
TEST_AVX512_SCATTER_MASK_RESTORE_EVENT(restore_scatter_mask_update,
                                       TEST_AVX512_SCATTER_MASK_UPDATE_MARKER)

#define TEST_AVX512_GATHER_SCRATCH_XMM_RESTORE_EVENT(name)  @N@\
DECLARE_FUNC_SEH(FUNCNAME(name))                            @N@\
  GLOBAL_LABEL(FUNCNAME(name):)                             @N@\
        /* uint32_t *ref_sparse_test_buf */                 @N@\
        mov        REG_XAX, ARG1                            @N@\
        /* uint32_t *scratch_xmm_val */                     @N@\
        mov        REG_XCX, ARG3                            @N@\
        /* uint32_t *test_idx32_vec */                      @N@\
        mov        REG_XDX, ARG2                            @N@\
        PUSH_CALLEE_SAVED_REGS()                            @N@\
        sub        REG_XSP, FRAME_PADDING                   @N@\
        END_PROLOG                                          @N@\
        vmovdqu32  xmm2, [REG_XCX]                          @N@\
        vmovdqu32  zmm1, [REG_XDX]                          @N@\
        movw       dx, 0xffff                               @N@\
        kmovw      k1, edx                                  @N@\
        vpgatherdd zmm0 {k1}, [REG_XAX + zmm1 * 4]          @N@\
        add        REG_XSP, FRAME_PADDING                   @N@\
        POP_CALLEE_SAVED_REGS()                             @N@\
        ret                                                 @N@\
        END_FUNC(FUNCNAME(name))

#define TEST_AVX512_SCATTER_SCRATCH_XMM_RESTORE_EVENT(name)  @N@\
DECLARE_FUNC_SEH(FUNCNAME(name))                             @N@\
  GLOBAL_LABEL(FUNCNAME(name):)                              @N@\
        /* uint32_t *xmm_ymm_zmm */                          @N@\
        mov         REG_XAX, ARG1                            @N@\
        /* uint32_t *scratch_xmm_val */                      @N@\
        mov         REG_XDI, ARG4                            @N@\
        /* uint32_t *output_sparse_test_buf OUT */           @N@\
        mov         REG_XCX, ARG3                            @N@\
        /* uint32_t *test_idx32_vec */                       @N@\
        mov         REG_XDX, ARG2                            @N@\
        PUSH_CALLEE_SAVED_REGS()                             @N@\
        sub         REG_XSP, FRAME_PADDING                   @N@\
        END_PROLOG                                           @N@\
        vmovdqu32   xmm2, [REG_XDI]                          @N@\
        vmovdqu32   zmm0, [REG_XAX + 48]                     @N@\
        vmovdqu32   zmm1, [REG_XDX]                          @N@\
        movw        dx, 0xffff                               @N@\
        kmovw       k1, edx                                  @N@\
        vpscatterdd [REG_XCX + zmm1 * 4] {k1}, zmm0          @N@\
        add         REG_XSP, FRAME_PADDING                   @N@\
        POP_CALLEE_SAVED_REGS()                              @N@\
        ret                                                  @N@\
        END_FUNC(FUNCNAME(name))

TEST_AVX512_GATHER_SCRATCH_XMM_RESTORE_EVENT(restore_gather_scratch_xmm_fault)
TEST_AVX512_SCATTER_SCRATCH_XMM_RESTORE_EVENT(restore_scatter_scratch_xmm_fault)
#endif /* __AVX512F__ */

/****************************************************************************
 * AVX2 test functions.
 */

#undef FUNCNAME
#define FUNCNAME(opcode) JOIN(test_avx2_, opcode)

#ifdef __AVX__
#define TEST_AVX2_GATHER_IDX32_VAL32(opcode)            @N@\
DECLARE_FUNC_SEH(FUNCNAME(opcode))                      @N@\
  GLOBAL_LABEL(FUNCNAME(opcode):)                       @N@\
        /* uint32_t *ref_sparse_test_buf */             @N@\
        mov           REG_XAX, ARG1                     @N@\
        /* uint32_t *output_xmm_ymm_zmm OUT */          @N@\
        mov           REG_XCX, ARG3                     @N@\
        /* uint32_t *test_idx32_vec */                  @N@\
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
        END_FUNC(FUNCNAME(opcode))

TEST_AVX2_GATHER_IDX32_VAL32(vpgatherdd)
TEST_AVX2_GATHER_IDX32_VAL32(vgatherdps)

#define TEST_AVX2_GATHER_IDX32_VAL64(opcode)            @N@\
DECLARE_FUNC_SEH(FUNCNAME(opcode))                      @N@\
  GLOBAL_LABEL(FUNCNAME(opcode):)                       @N@\
        /* uint32_t *ref_sparse_test_buf */             @N@\
        mov           REG_XAX, ARG1                     @N@\
        /* uint32_t *output_xmm_ymm_zmm OUT */          @N@\
        mov           REG_XCX, ARG3                     @N@\
        /* uint32_t *test_idx32_vec */                  @N@\
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
        END_FUNC(FUNCNAME(opcode))

TEST_AVX2_GATHER_IDX32_VAL64(vpgatherdq)
TEST_AVX2_GATHER_IDX32_VAL64(vgatherdpd)

#define TEST_AVX2_GATHER_IDX64_VAL32(opcode)            @N@\
DECLARE_FUNC_SEH(FUNCNAME(opcode))                      @N@\
  GLOBAL_LABEL(FUNCNAME(opcode):)                       @N@\
        /* uint32_t *ref_sparse_test_buf */             @N@\
        mov           REG_XAX, ARG1                     @N@\
        /* uint32_t *output_xmm_ymm_zmm OUT */          @N@\
        mov           REG_XCX, ARG3                     @N@\
        /* uint32_t *test_idx32_vec */                  @N@\
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
        END_FUNC(FUNCNAME(opcode))

TEST_AVX2_GATHER_IDX64_VAL32(vpgatherqd)
TEST_AVX2_GATHER_IDX64_VAL32(vgatherqps)

#define TEST_AVX2_GATHER_IDX64_VAL64(opcode)            @N@\
DECLARE_FUNC_SEH(FUNCNAME(opcode))                      @N@\
  GLOBAL_LABEL(FUNCNAME(opcode):)                       @N@\
        /* uint32_t *ref_sparse_test_buf */             @N@\
        mov           REG_XAX, ARG1                     @N@\
        /* uint32_t *output_xmm_ymm_zmm OUT */          @N@\
        mov           REG_XCX, ARG3                     @N@\
        /* uint32_t *test_idx32_vec */                  @N@\
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
        END_FUNC(FUNCNAME(opcode))

TEST_AVX2_GATHER_IDX64_VAL64(vpgatherqq)
TEST_AVX2_GATHER_IDX64_VAL64(vgatherqpd)

#define TEST_AVX2_GATHER_MASK_RESTORE_EVENT(name, marker) @N@\
DECLARE_FUNC_SEH(FUNCNAME(name))                          @N@\
  GLOBAL_LABEL(FUNCNAME(name):)                           @N@\
        /* uint32_t *ref_sparse_test_buf */               @N@\
        mov           REG_XAX, ARG1                       @N@\
        /* uint32_t *test_idx32_vec */                    @N@\
        mov           REG_XDX, ARG2                       @N@\
        PUSH_CALLEE_SAVED_REGS()                          @N@\
        sub           REG_XSP, FRAME_PADDING              @N@\
        END_PROLOG                                        @N@\
        mov           REG_XCX, marker                     @N@\
        mov           REG_XCX, marker                     @N@\
        vmovdqu       ymm1, [REG_XDX]                     @N@\
        vpcmpeqd      ymm2, ymm2, ymm2                    @N@\
        vpgatherdd    ymm0, [REG_XAX + ymm1 * 4], ymm2    @N@\
        add           REG_XSP, FRAME_PADDING              @N@\
        POP_CALLEE_SAVED_REGS()                           @N@\
        ret                                               @N@\
        END_FUNC(FUNCNAME(name))

/* Note that there is no test for mask clobbering, because the AVX2 version
 * of gather expansion does not clobber a mask in drx.
 */
TEST_AVX2_GATHER_MASK_RESTORE_EVENT(restore_gather_mask_update,
                                    TEST_AVX2_GATHER_MASK_UPDATE_MARKER)

#define TEST_AVX2_GATHER_SCRATCH_XMM_RESTORE_EVENT(name)  @N@\
DECLARE_FUNC_SEH(FUNCNAME(name))                          @N@\
  GLOBAL_LABEL(FUNCNAME(name):)                           @N@\
        /* uint32_t *ref_sparse_test_buf */               @N@\
        mov           REG_XAX, ARG1                       @N@\
        /* uint32_t *scratch_xmm_val */                   @N@\
        mov           REG_XCX, ARG3                       @N@\
        /* uint32_t *test_idx32_vec */                    @N@\
        mov           REG_XDX, ARG2                       @N@\
        PUSH_CALLEE_SAVED_REGS()                          @N@\
        sub           REG_XSP, FRAME_PADDING              @N@\
        END_PROLOG                                        @N@\
        vmovdqu       xmm3, [REG_XCX]                     @N@\
        vmovdqu       ymm1, [REG_XDX]                     @N@\
        vpcmpeqd      ymm2, ymm2, ymm2                    @N@\
        vpgatherdd    ymm0, [REG_XAX + ymm1 * 4], ymm2    @N@\
        add           REG_XSP, FRAME_PADDING              @N@\
        POP_CALLEE_SAVED_REGS()                           @N@\
        ret                                               @N@\
        END_FUNC(FUNCNAME(name))

TEST_AVX2_GATHER_SCRATCH_XMM_RESTORE_EVENT(restore_gather_scratch_xmm_fault)

#endif /* __AVX__ */

END_FILE
/* clang-format on */
#endif
