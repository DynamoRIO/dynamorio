/* **********************************************************
 * Copyright (c) 2011-2020 Google, Inc.  All rights reserved.
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

/* Tests a signal handler accessing sigcontext */

#include "tools.h"
/* we want the latest defs so we can get at ymm state */
#include "../../../core/unix/include/sigcontext.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <ucontext.h>
#include <errno.h>
#include <stddef.h>

/* For sharing NUM_*_REGS constants. */
#include "../api/detach_state_shared.h"

/* TODO i#1312: This test has been prepared for - and executes AVX-512 code, but doesn't
 * test any AVX-512 state yet.
 */

#ifdef X64
#    define XAX "rax"
#else
#    define XAX "eax"
#endif
#define INTS_PER_XMM 4
#define INTS_PER_YMM 8
#define INTS_PER_ZMM 16
#define CPUID_KMASK_COMP 5
#define CPUID_ZMM_HI256_COMP 6
#define CPUID_HI16_ZMM_COMP 7

static int
get_xstate_area_offs(int comp)
{
    int offs;
    __asm__ __volatile__("cpuid" : "=b"(offs) : "a"(0xd), "c"(comp));
    return offs;
}

static void
signal_handler(int sig, siginfo_t *siginfo, ucontext_t *ucxt)
{
    int i, j;
    switch (sig) {
    case SIGUSR1: {
        if (ucxt->uc_mcontext.fpregs == NULL) {
            print("fpstate is NULL\n");
        } else {
            /* SIGUSR1 is delayable so we're testing propagation of the
             * fpstate with xmm inside on delayed signals
             */
            kernel_fpstate_t *fp = (kernel_fpstate_t *)ucxt->uc_mcontext.fpregs;
            for (i = 0; i < NUM_SIMD_SSE_AVX_REGS; i++) {
                print("xmm[%d] = 0x%x 0x%x 0x%x 0x%x\n", i,
#ifdef X64
                      fp->xmm_space[i * 4], fp->xmm_space[i * 4 + 1],
                      fp->xmm_space[i * 4 + 2], fp->xmm_space[i * 4 + 3]
#else
                      fp->_xmm[i].element[0], fp->_xmm[i].element[1],
                      fp->_xmm[i].element[2], fp->_xmm[i].element[3]
#endif
                );
                for (j = 0; j < INTS_PER_XMM; j++) {
#ifdef X64
                    assert(fp->xmm_space[i * 4 + j] == 0xdeadbeef << i);
#else
                    assert(fp->_xmm[i].element[j] == 0xdeadbeef << i);
#endif
                }
            }
        }
        break;
    }
    case SIGUSR2: {
        if (ucxt->uc_mcontext.fpregs == NULL) {
            print("fpstate is NULL\n");
        } else {
            /* SIGUSR2 is delayable so we're testing propagation of the
             * xstate with ymm inside on delayed signals on AVX processors
             */
            kernel_fpstate_t *fp = (kernel_fpstate_t *)ucxt->uc_mcontext.fpregs;
            for (i = 0; i < NUM_SIMD_SSE_AVX_REGS; i++) {
                print("xmm[%d] = 0x%x 0x%x 0x%x 0x%x\n", i,
#ifdef X64
                      fp->xmm_space[i * 4], fp->xmm_space[i * 4 + 1],
                      fp->xmm_space[i * 4 + 2], fp->xmm_space[i * 4 + 3]
#else
                      fp->_xmm[i].element[0], fp->_xmm[i].element[1],
                      fp->_xmm[i].element[2], fp->_xmm[i].element[3]
#endif
                );
            }
            kernel_xstate_t *xstate = (kernel_xstate_t *)ucxt->uc_mcontext.fpregs;
            if (xstate->fpstate.sw_reserved.magic1 == FP_XSTATE_MAGIC1) {
                assert(xstate->fpstate.sw_reserved.extended_size >= sizeof(*xstate));
#ifdef __AVX__
                for (i = 0; i < NUM_SIMD_SSE_AVX_REGS; i++) {
                    print("ymmh[%d] = 0x%x 0x%x 0x%x 0x%x\n", i,
                          xstate->ymmh.ymmh_space[i * 4],
                          xstate->ymmh.ymmh_space[i * 4 + 1],
                          xstate->ymmh.ymmh_space[i * 4 + 2],
                          xstate->ymmh.ymmh_space[i * 4 + 3]);
                }
#endif
#ifdef __AVX512F__
#    ifdef X64
                __u32 *xstate_kmask_offs =
                    (__u32 *)((byte *)xstate + get_xstate_area_offs(CPUID_KMASK_COMP));
                for (i = 0; i < NUM_OPMASK_REGS; i++) {
                    print("kmask[%d] = 0x%x\n", i, xstate_kmask_offs[i * 2]);
                }
                __u32 *xstate_zmm_hi256_offs =
                    (__u32 *)((byte *)xstate +
                              get_xstate_area_offs(CPUID_ZMM_HI256_COMP));
                for (i = 0; i < NUM_SIMD_SSE_AVX_REGS; i++) {
                    print("zmm_hi256[%d] = 0x%x 0x%x 0x%x 0x%x\n", i,
                          xstate_zmm_hi256_offs[i * 8], xstate_zmm_hi256_offs[i * 8 + 1],
                          xstate_zmm_hi256_offs[i * 8 + 2],
                          xstate_zmm_hi256_offs[i * 8 + 3]);
                }
                __u32 *xstate_hi16_zmm_offs =
                    (__u32 *)((byte *)xstate + get_xstate_area_offs(CPUID_HI16_ZMM_COMP));
                for (i = 0; i < NUM_SIMD_AVX512_REGS - NUM_SIMD_SSE_AVX_REGS; i++) {
                    print("hi16_zmm[%d] = 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", i,
                          xstate_hi16_zmm_offs[i * 16], xstate_hi16_zmm_offs[i * 16 + 1],
                          xstate_hi16_zmm_offs[i * 16 + 2],
                          xstate_hi16_zmm_offs[i * 16 + 3],
                          xstate_hi16_zmm_offs[i * 16 + 4],
                          xstate_hi16_zmm_offs[i * 16 + 5],
                          xstate_hi16_zmm_offs[i * 16 + 6],
                          xstate_hi16_zmm_offs[i * 16 + 7]);
                }
#    else
                /* XXX i#1312: it is unclear if and how the components are arranged in
                 * 32-bit mode by the kernel.
                 */
#    endif
#endif
            }
        }
        break;
    }
    default: assert(0);
    }
}

int
main(int argc, char *argv[])
{
    int buf[INTS_PER_XMM * NUM_SIMD_SSE_AVX_REGS];
    char *ptr = (char *)buf;
    int i, j;

    /* do this first to avoid messing w/ xmm state */
    intercept_signal(SIGUSR1, signal_handler, false);
    print("Sending SIGUSR1\n");

    /* put known values in xmm regs (we assume processor has xmm) */
    for (i = 0; i < NUM_SIMD_SSE_AVX_REGS; i++) {
        for (j = 0; j < INTS_PER_XMM; j++)
            buf[i * INTS_PER_XMM + j] = 0xdeadbeef << i;
    }
#define MOVE_TO_XMM(buf, num)                           \
    __asm__ __volatile__("movdqu %0, %%xmm" #num        \
                         :                              \
                         : "m"(buf[num * INTS_PER_XMM]) \
                         : "xmm" #num);
    MOVE_TO_XMM(buf, 0)
    MOVE_TO_XMM(buf, 1)
    MOVE_TO_XMM(buf, 2)
    MOVE_TO_XMM(buf, 3)
    MOVE_TO_XMM(buf, 4)
    MOVE_TO_XMM(buf, 5)
    MOVE_TO_XMM(buf, 6)
    MOVE_TO_XMM(buf, 7)
#ifdef X64
    MOVE_TO_XMM(buf, 8)
    MOVE_TO_XMM(buf, 9)
    MOVE_TO_XMM(buf, 10)
    MOVE_TO_XMM(buf, 11)
    MOVE_TO_XMM(buf, 12)
    MOVE_TO_XMM(buf, 13)
    MOVE_TO_XMM(buf, 14)
    MOVE_TO_XMM(buf, 15)
#endif
    /* we assume xmm* won't be overwritten by this library call before the signal */
    kill(getpid(), SIGUSR1);

#if defined(__AVX__) || defined(__AVX512F__)
    {
        /* put known values in ymm regs */
#    ifdef __AVX512F__
        int buf[INTS_PER_ZMM * NUM_SIMD_AVX512_REGS];
#    else
        int buf[INTS_PER_YMM * NUM_SIMD_SSE_AVX_REGS];
#    endif
        char *ptr = (char *)buf;
        int i, j;
        intercept_signal(SIGUSR2, signal_handler, false);
        print("Sending SIGUSR2\n");

        /* put known values in xmm regs (we assume processor has xmm) */
#    ifdef __AVX512F__
        for (i = 0; i < NUM_SIMD_AVX512_REGS; i++) {
            for (j = 0; j < INTS_PER_ZMM; j++)
                buf[i * INTS_PER_ZMM + j] = 0xdeadbeef + i * INTS_PER_ZMM + j;
        }
#    else
        for (i = 0; i < NUM_SIMD_SSE_AVX_REGS; i++) {
            for (j = 0; j < INTS_PER_YMM; j++)
                buf[i * INTS_PER_YMM + j] = 0xdeadbeef + i * INTS_PER_ZMM + j;
        }
#    endif
#    ifdef __AVX512F__
#        define MOVE_TO_ZMM(buf, num)                           \
            __asm__ __volatile__("vmovdqu64 %0, %%zmm" #num     \
                                 :                              \
                                 : "m"(buf[num * INTS_PER_ZMM]) \
                                 : "zmm" #num);
        MOVE_TO_ZMM(buf, 0)
        MOVE_TO_ZMM(buf, 1)
        MOVE_TO_ZMM(buf, 2)
        MOVE_TO_ZMM(buf, 3)
        MOVE_TO_ZMM(buf, 4)
        MOVE_TO_ZMM(buf, 5)
        MOVE_TO_ZMM(buf, 6)
        MOVE_TO_ZMM(buf, 7)
#        ifdef X64
        MOVE_TO_ZMM(buf, 8)
        MOVE_TO_ZMM(buf, 9)
        MOVE_TO_ZMM(buf, 10)
        MOVE_TO_ZMM(buf, 11)
        MOVE_TO_ZMM(buf, 12)
        MOVE_TO_ZMM(buf, 13)
        MOVE_TO_ZMM(buf, 14)
        MOVE_TO_ZMM(buf, 15)
        MOVE_TO_ZMM(buf, 16)
        MOVE_TO_ZMM(buf, 17)
        MOVE_TO_ZMM(buf, 18)
        MOVE_TO_ZMM(buf, 19)
        MOVE_TO_ZMM(buf, 20)
        MOVE_TO_ZMM(buf, 21)
        MOVE_TO_ZMM(buf, 22)
        MOVE_TO_ZMM(buf, 23)
        MOVE_TO_ZMM(buf, 24)
        MOVE_TO_ZMM(buf, 25)
        MOVE_TO_ZMM(buf, 26)
        MOVE_TO_ZMM(buf, 27)
        MOVE_TO_ZMM(buf, 28)
        MOVE_TO_ZMM(buf, 29)
        MOVE_TO_ZMM(buf, 30)
        MOVE_TO_ZMM(buf, 31)
#        endif
        /* Re-using INTS_PER_ZMM here to get same data patterns as above. */
#        define MOVE_TO_OPMASK(buf, num)                        \
            __asm__ __volatile__("kmovw %0, %%k" #num           \
                                 :                              \
                                 : "m"(buf[num * INTS_PER_ZMM]) \
                                 : "k" #num);
        MOVE_TO_OPMASK(buf, 0)
        MOVE_TO_OPMASK(buf, 1)
        MOVE_TO_OPMASK(buf, 2)
        MOVE_TO_OPMASK(buf, 3)
        MOVE_TO_OPMASK(buf, 4)
        MOVE_TO_OPMASK(buf, 5)
        MOVE_TO_OPMASK(buf, 6)
        MOVE_TO_OPMASK(buf, 7)
#    else
#        define MOVE_TO_YMM(buf, num)                           \
            __asm__ __volatile__("vmovdqu %0, %%ymm" #num       \
                                 :                              \
                                 : "m"(buf[num * INTS_PER_YMM]) \
                                 : "ymm" #num);
        MOVE_TO_YMM(buf, 0)
        MOVE_TO_YMM(buf, 1)
        MOVE_TO_YMM(buf, 2)
        MOVE_TO_YMM(buf, 3)
        MOVE_TO_YMM(buf, 4)
        MOVE_TO_YMM(buf, 5)
        MOVE_TO_YMM(buf, 6)
        MOVE_TO_YMM(buf, 7)
#        ifdef X64
        MOVE_TO_YMM(buf, 8)
        MOVE_TO_YMM(buf, 9)
        MOVE_TO_YMM(buf, 10)
        MOVE_TO_YMM(buf, 11)
        MOVE_TO_YMM(buf, 12)
        MOVE_TO_YMM(buf, 13)
        MOVE_TO_YMM(buf, 14)
        MOVE_TO_YMM(buf, 15)
#        endif
#    endif
        /* now make sure they show up in signal context */
        kill(getpid(), SIGUSR2);

        /* Ensure they are preserved across the sigreturn (xref i#3812). */
#    ifdef __AVX512F__
        /* Use a new buffer to avoid the old values. We could do a custom memset
         * with rep movs in asm instead (regular memset may clobber SIMD regs).
         */
        int buf2[INTS_PER_ZMM * NUM_SIMD_AVX512_REGS];
#        define MOVE_FROM_ZMM(buf, num)                          \
            __asm__ __volatile__("vmovdqu64 %%zmm" #num ", %0"   \
                                 : "=m"(buf[num * INTS_PER_ZMM]) \
                                 :                               \
                                 : "zmm" #num);
        MOVE_FROM_ZMM(buf2, 0)
        MOVE_FROM_ZMM(buf2, 1)
        MOVE_FROM_ZMM(buf2, 2)
        MOVE_FROM_ZMM(buf2, 3)
        MOVE_FROM_ZMM(buf2, 4)
        MOVE_FROM_ZMM(buf2, 5)
        MOVE_FROM_ZMM(buf2, 6)
        MOVE_FROM_ZMM(buf2, 7)
#        ifdef X64
        MOVE_FROM_ZMM(buf2, 8)
        MOVE_FROM_ZMM(buf2, 9)
        MOVE_FROM_ZMM(buf2, 10)
        MOVE_FROM_ZMM(buf2, 11)
        MOVE_FROM_ZMM(buf2, 12)
        MOVE_FROM_ZMM(buf2, 13)
        MOVE_FROM_ZMM(buf2, 14)
        MOVE_FROM_ZMM(buf2, 15)
        MOVE_FROM_ZMM(buf2, 16)
        MOVE_FROM_ZMM(buf2, 17)
        MOVE_FROM_ZMM(buf2, 18)
        MOVE_FROM_ZMM(buf2, 19)
        MOVE_FROM_ZMM(buf2, 20)
        MOVE_FROM_ZMM(buf2, 21)
        MOVE_FROM_ZMM(buf2, 22)
        MOVE_FROM_ZMM(buf2, 23)
        MOVE_FROM_ZMM(buf2, 24)
        MOVE_FROM_ZMM(buf2, 25)
        MOVE_FROM_ZMM(buf2, 26)
        MOVE_FROM_ZMM(buf2, 27)
        MOVE_FROM_ZMM(buf2, 28)
        MOVE_FROM_ZMM(buf2, 29)
        MOVE_FROM_ZMM(buf2, 30)
        MOVE_FROM_ZMM(buf2, 31)
#        endif
        for (i = 0; i < NUM_SIMD_AVX512_REGS; i++) {
            for (j = 0; j < INTS_PER_ZMM; j++) {
                assert(buf2[i * INTS_PER_ZMM + j] == 0xdeadbeef + i * INTS_PER_ZMM + j);
            }
        }
        /* Re-using INTS_PER_ZMM here to get same data patterns as above. */
        int buf3[INTS_PER_ZMM * NUM_OPMASK_REGS];
#        define MOVE_FROM_OPMASK(buf, num)                       \
            __asm__ __volatile__("kmovw %%k" #num ", %0"         \
                                 : "=m"(buf[num * INTS_PER_ZMM]) \
                                 :                               \
                                 : "k" #num);
        MOVE_FROM_OPMASK(buf3, 0)
        MOVE_FROM_OPMASK(buf3, 1)
        MOVE_FROM_OPMASK(buf3, 2)
        MOVE_FROM_OPMASK(buf3, 3)
        MOVE_FROM_OPMASK(buf3, 4)
        MOVE_FROM_OPMASK(buf3, 5)
        MOVE_FROM_OPMASK(buf3, 6)
        MOVE_FROM_OPMASK(buf3, 7)
        for (i = 0; i < NUM_OPMASK_REGS; i++) {
            short bufval = (short)buf3[i * INTS_PER_ZMM];
            short expect = (short)(0xdeadbeef + i * INTS_PER_ZMM);
            assert(bufval == expect);
        }
#    else
        int buf2[INTS_PER_YMM * NUM_SIMD_SSE_AVX_REGS];
#        define MOVE_FROM_YMM(buf, num)                          \
            __asm__ __volatile__("vmovdqu %%ymm" #num ", %0"     \
                                 : "=m"(buf[num * INTS_PER_YMM]) \
                                 :                               \
                                 : "ymm" #num);
        MOVE_FROM_YMM(buf2, 0)
        MOVE_FROM_YMM(buf2, 1)
        MOVE_FROM_YMM(buf2, 2)
        MOVE_FROM_YMM(buf2, 3)
        MOVE_FROM_YMM(buf2, 4)
        MOVE_FROM_YMM(buf2, 5)
        MOVE_FROM_YMM(buf2, 6)
        MOVE_FROM_YMM(buf2, 7)
#        ifdef X64
        MOVE_FROM_YMM(buf2, 8)
        MOVE_FROM_YMM(buf2, 9)
        MOVE_FROM_YMM(buf2, 10)
        MOVE_FROM_YMM(buf2, 11)
        MOVE_FROM_YMM(buf2, 12)
        MOVE_FROM_YMM(buf2, 13)
        MOVE_FROM_YMM(buf2, 14)
        MOVE_FROM_YMM(buf2, 15)
#        endif
        for (i = 0; i < NUM_SIMD_SSE_AVX_REGS; i++) {
            for (j = 0; j < INTS_PER_YMM; j++)
                assert(buf2[i * INTS_PER_YMM + j] == 0xdeadbeef + i * INTS_PER_ZMM + j);
        }
#    endif
    }
#endif

    print("All done\n");
    return 0;
}
