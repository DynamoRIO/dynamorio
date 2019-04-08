/* **********************************************************
 * Copyright (c) 2011-2013 Google, Inc.  All rights reserved.
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

/* we want the latest defs so we can get at ymm state */
#include "../../../core/unix/include/sigcontext.h"

#include "tools.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <ucontext.h>
#include <errno.h>

#ifdef X64
#    define NUM_SIMD_REGS 16
#    define XAX "rax"
#else
#    define NUM_SIMD_REGS 8
#    define XAX "eax"
#endif
#define INTS_PER_XMM 4
#define INTS_PER_YMM 8

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
            for (i = 0; i < NUM_SIMD_REGS; i++) {
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
            kernel_xstate_t *xstate = (kernel_xstate_t *)ucxt->uc_mcontext.fpregs;
            if (xstate->fpstate.sw_reserved.magic1 == FP_XSTATE_MAGIC1) {
                assert(xstate->fpstate.sw_reserved.xstate_size >= sizeof(*xstate));
                /* we can't print b/c not all processors have avx */
                for (i = 0; i < NUM_SIMD_REGS; i++) {
#if VERBOSE
                    print("ymmh[%d] = 0x%x 0x%x 0x%x 0x%x\n", i,
                          xstate->ymmh.ymmh_space[i * 4],
                          xstate->ymmh.ymmh_space[i * 4 + 1],
                          xstate->ymmh.ymmh_space[i * 4 + 2],
                          xstate->ymmh.ymmh_space[i * 4 + 3]);
#endif
                    for (j = 0; j < 4; j++)
                        assert(xstate->ymmh.ymmh_space[i * 4 + j] == 0xdeadbeef << i);
                }
            }
        }
        break;
    }
    default: assert(0);
    }
}

/* looks for "avx" flag in /proc/cpuinfo */
static int
determine_avx(void)
{
    FILE *cpuinfo;
    char line[1024];
    ulong cpu_mhz = 1, cpu_khz = 0;

    cpuinfo = fopen("/proc/cpuinfo", "r");
    assert(cpuinfo != NULL);

    while (!feof(cpuinfo)) {
        if (fgets(line, sizeof(line), cpuinfo) == NULL)
            break;
        if (strstr(line, "flags") == line && strstr(line, "avx") != NULL) {
            return 1;
        }
    }
    fclose(cpuinfo);
    return 0;
}

int
main(int argc, char *argv[])
{
    int buf[INTS_PER_XMM * NUM_SIMD_REGS];
    char *ptr = (char *)buf;
    int i, j;

    /* do this first to avoid messing w/ xmm state */
    intercept_signal(SIGUSR1, signal_handler, false);
    print("Sending SIGUSR1\n");

    /* put known values in xmm regs (we assume processor has xmm) */
    for (i = 0; i < NUM_SIMD_REGS; i++) {
        for (j = 0; j < INTS_PER_XMM; j++)
            buf[i * INTS_PER_XMM + j] = 0xdeadbeef << i;
    }
    /* XXX: unfortunately there's no way to do this w/o unrolling it */
    __asm("mov %0, %%" XAX "; movdqu     (%%" XAX "),%%xmm0" : : "m"(ptr) : "%" XAX);
    __asm("mov %0, %%" XAX "; movdqu 0x10(%%" XAX "),%%xmm1" : : "m"(ptr) : "%" XAX);
    __asm("mov %0, %%" XAX "; movdqu 0x20(%%" XAX "),%%xmm2" : : "m"(ptr) : "%" XAX);
    __asm("mov %0, %%" XAX "; movdqu 0x30(%%" XAX "),%%xmm3" : : "m"(ptr) : "%" XAX);
    __asm("mov %0, %%" XAX "; movdqu 0x40(%%" XAX "),%%xmm4" : : "m"(ptr) : "%" XAX);
    __asm("mov %0, %%" XAX "; movdqu 0x50(%%" XAX "),%%xmm5" : : "m"(ptr) : "%" XAX);
    __asm("mov %0, %%" XAX "; movdqu 0x60(%%" XAX "),%%xmm6" : : "m"(ptr) : "%" XAX);
    __asm("mov %0, %%" XAX "; movdqu 0x70(%%" XAX "),%%xmm7" : : "m"(ptr) : "%" XAX);
#ifdef X64
    __asm("mov %0, %%" XAX "; movdqu 0x80(%%" XAX "),%%xmm8" : : "m"(ptr) : "%" XAX);
    __asm("mov %0, %%" XAX "; movdqu 0x90(%%" XAX "),%%xmm9" : : "m"(ptr) : "%" XAX);
    __asm("mov %0, %%" XAX "; movdqu 0xa0(%%" XAX "),%%xmm10" : : "m"(ptr) : "%" XAX);
    __asm("mov %0, %%" XAX "; movdqu 0xb0(%%" XAX "),%%xmm11" : : "m"(ptr) : "%" XAX);
    __asm("mov %0, %%" XAX "; movdqu 0xc0(%%" XAX "),%%xmm12" : : "m"(ptr) : "%" XAX);
    __asm("mov %0, %%" XAX "; movdqu 0xd0(%%" XAX "),%%xmm13" : : "m"(ptr) : "%" XAX);
    __asm("mov %0, %%" XAX "; movdqu 0xe0(%%" XAX "),%%xmm14" : : "m"(ptr) : "%" XAX);
    __asm("mov %0, %%" XAX "; movdqu 0xf0(%%" XAX "),%%xmm15" : : "m"(ptr) : "%" XAX);
#endif
    /* we assume xmm* won't be overwritten by this library call before the signal */
    kill(getpid(), SIGUSR1);

    if (determine_avx()) {
        /* put known values in ymm regs */
        int buf[INTS_PER_YMM * NUM_SIMD_REGS];
        char *ptr = (char *)buf;
        int i, j;
        intercept_signal(SIGUSR2, signal_handler, false);
        /* put known values in xmm regs (we assume processor has xmm) */
        for (i = 0; i < NUM_SIMD_REGS; i++) {
            for (j = 0; j < INTS_PER_YMM; j++)
                buf[i * INTS_PER_YMM + j] = 0xdeadbeef << i;
        }
        /* XXX: unfortunately there's no way to do this w/o unrolling it */
        __asm("mov %0, %%" XAX "; vmovdqu     (%%" XAX "),%%ymm0" : : "m"(ptr) : "%" XAX);
        __asm("mov %0, %%" XAX "; vmovdqu 0x20(%%" XAX "),%%ymm1" : : "m"(ptr) : "%" XAX);
        __asm("mov %0, %%" XAX "; vmovdqu 0x40(%%" XAX "),%%ymm2" : : "m"(ptr) : "%" XAX);
        __asm("mov %0, %%" XAX "; vmovdqu 0x60(%%" XAX "),%%ymm3" : : "m"(ptr) : "%" XAX);
        __asm("mov %0, %%" XAX "; vmovdqu 0x80(%%" XAX "),%%ymm4" : : "m"(ptr) : "%" XAX);
        __asm("mov %0, %%" XAX "; vmovdqu 0xa0(%%" XAX "),%%ymm5" : : "m"(ptr) : "%" XAX);
        __asm("mov %0, %%" XAX "; vmovdqu 0xc0(%%" XAX "),%%ymm6" : : "m"(ptr) : "%" XAX);
        __asm("mov %0, %%" XAX "; vmovdqu 0xe0(%%" XAX "),%%ymm7" : : "m"(ptr) : "%" XAX);
#ifdef X64
        __asm("mov %0, %%" XAX "; vmovdqu 0x100(%%" XAX "),%%ymm8"
              :
              : "m"(ptr)
              : "%" XAX);
        __asm("mov %0, %%" XAX "; vmovdqu 0x120(%%" XAX "),%%ymm9"
              :
              : "m"(ptr)
              : "%" XAX);
        __asm("mov %0, %%" XAX "; vmovdqu 0x140(%%" XAX "),%%ymm10"
              :
              : "m"(ptr)
              : "%" XAX);
        __asm("mov %0, %%" XAX "; vmovdqu 0x160(%%" XAX "),%%ymm11"
              :
              : "m"(ptr)
              : "%" XAX);
        __asm("mov %0, %%" XAX "; vmovdqu 0x180(%%" XAX "),%%ymm12"
              :
              : "m"(ptr)
              : "%" XAX);
        __asm("mov %0, %%" XAX "; vmovdqu 0x1a0(%%" XAX "),%%ymm13"
              :
              : "m"(ptr)
              : "%" XAX);
        __asm("mov %0, %%" XAX "; vmovdqu 0x1c0(%%" XAX "),%%ymm14"
              :
              : "m"(ptr)
              : "%" XAX);
        __asm("mov %0, %%" XAX "; vmovdqu 0x1e0(%%" XAX "),%%ymm15"
              :
              : "m"(ptr)
              : "%" XAX);
#endif
        /* now make sure they show up in signal context */
        kill(getpid(), SIGUSR2);
    }

    print("All done\n");
    return 0;
}
