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

/* Tests resuming from check_wait_at_safe_spot => thread_set_self_context,
 * triggered by another thread performing a synch. Test based on
 * linux.sigcontext.
 */

#ifndef THREADSET_CLIENT
#include "tools.h"
#include "thread.h"
#endif

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


#define INTS_PER_XMM 4
#define INTS_PER_YMM 8
#define INTS_PER_ZMM 16

/* This file contains both the client and the target program */

#ifdef THREADSET_CLIENT
#include "dr_api.h"


/* this handler gets called for every bb, and flushes the current bb
 * with 2% probability.
 */
static void bb_event(void *p) {
    void *drcontext = dr_get_current_drcontext();
    static int count = 0;

    /* Avoids executing the hook twice after redirecting execution */
    if (dr_get_tls_field(drcontext) != NULL) {
        dr_set_tls_field(drcontext, (void *)0);
        return;
    }

    if (++count % 25 == 0) {
        dr_flush_region(p, 1);
        /* if we don't sleep, we will interrupt the other thread
         * too quickly, hitting the (count++ > 3) assert in os.c
         */
        dr_sleep(1); /* 1ms */

        dr_mcontext_t mcontext;
        mcontext.size = sizeof(mcontext);
        mcontext.flags = DR_MC_ALL;
        dr_get_mcontext(drcontext, &mcontext);
        mcontext.pc = (app_pc)p;

        dr_set_tls_field(drcontext, (void *)1);
        dr_redirect_execution(&mcontext);
    }
}

static dr_emit_flags_t instrument_bb(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                          bool translating)
{
    instr_t *instr = instrlist_first(bb);
    if (!instr_is_app(instr))
        return DR_EMIT_DEFAULT;

    dr_insert_clean_call(drcontext, bb, instr, (void *)bb_event,
                         true /* save fpstate */, 1, OPND_CREATE_INTPTR(instr_get_app_pc(instr)));
    return DR_EMIT_DEFAULT;
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_register_bb_event(instrument_bb);
}

#else


__attribute__((noinline))
void dummy2() {
    for (int i = 0; i < 10; i++) {
        asm volatile("add %rdi, %rdi");
    }
}

void *thread() {
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

    /* this test deliberately uses write() instead of the other libc calls, since those appeared
     * to cause crashes (likely due to us accidentically triggering the xmm saving bug :/).
     */

    write(2, "Starting test.\n", 16);
    thread_t flusher = create_thread(thread, NULL);
    sleep(1);
    write(2, "Saving regs.\n", 13);

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

        write(2, "before\n", 7);

        /* Sometime in this loop, we will synch with the other thread */
        for (int i=0; i<100; i++) {
            dummy2();
        }

        write(2, "after\n", 6);

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
                if (buf2[i * INTS_PER_ZMM + j] != 0xdeadbeef + i * INTS_PER_ZMM + j) {
                    write(2, "Assertion failed.\n", 19);
                    _exit(1);
                }
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
                if (buf2[i * INTS_PER_YMM + j] != 0xdeadbeef + i * INTS_PER_ZMM + j) {
                    write(2, "Assertion failed.\n", 19);
                    _exit(1);
                }
            }
        }
#    endif
    }
#endif

    write(2, "All done\n", 9);
    return 0;
}

#endif /* THREADSET_CLIENT */
