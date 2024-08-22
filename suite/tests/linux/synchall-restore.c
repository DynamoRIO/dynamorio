/* **********************************************************
 * Copyright (c) 2011-2024 Google, Inc.  All rights reserved.
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

/* Tests resuming from check_wait_at_safe_spot => thread_set_self_context,
 * triggered by another thread flushing (causing a synchall). Test based on
 * linux.sigcontext.
 */

/* XXX: This test only verifies that XMM state is restored, not X87 state. */

#include "tools.h"
#include "thread.h"
#include "condvar.h"

/* we want the latest defs so we can get at ymm state */
#include "../../../core/unix/include/sigcontext.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <ucontext.h>
#include <errno.h>
#include <stddef.h>

/* For sharing NUM_*_REGS constants. */
#include "../api/detach_state_shared.h"

#ifndef X86
#    error synchall-restore test is only supported on X86.
#endif

#define INTS_PER_XMM 4
#define INTS_PER_YMM 8
#define INTS_PER_ZMM 16

NOINLINE void
dummy2()
{
    for (int i = 0; i < 10; i++) {
        asm volatile("add %rdi, %rdi");
    }
}

static void *child_started;

void *
thread()
{
    signal_cond_var(child_started);
    for (int i = 0; i < 100000; i++) {
        dummy2();
    }
}

int
main(int argc, char *argv[])
{
    int buf[INTS_PER_XMM * NUM_SIMD_SSE_AVX_REGS];
    char *ptr = (char *)buf;
    int i, j;

    print("Starting test.\n");

    child_started = create_cond_var();
    thread_t flusher = create_thread(thread, NULL);
    wait_cond_var(child_started);

    print("Saving regs.\n");

    /* put known values in xmm regs (we assume processor has xmm) */
    for (i = 0; i < NUM_SIMD_SSE_AVX_REGS; i++) {
        for (j = 0; j < INTS_PER_XMM; j++)
            buf[i * INTS_PER_XMM + j] = 0xdeadbeef << i;
    }

    /* XXX: Try to share with sigcontext.c to avoid duplicating all the
     * SIMD filling and checking code.
     */
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

        /* This is the start of the critical XMM-preserving section. */

        const char before_msg[] = "Before synchall loop.\n";
        const char after_msg[] = "After synchall loop.\n";

        /* print() would clobber XMM regs here, so we use write() instead. */
        write(STDERR_FILENO, before_msg, sizeof(before_msg) - 1);

        /* Sometime in this loop, we will synch with the other thread. */
        for (int i = 0; i < 100; i++) {
            dummy2();
        }

        /* print() would clobber XMM regs here, so we use write() instead. */
        write(STDERR_FILENO, after_msg, sizeof(after_msg) - 1);

        /* This is the end of the critical XMM-saving section. */

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
            for (j = 0; j < INTS_PER_YMM; j++) {
                assert(buf2[i * INTS_PER_YMM + j] == 0xdeadbeef + i * INTS_PER_ZMM + j);
            }
        }
#    endif
    }
#endif

    print("All done.\n");
    return 0;
}
