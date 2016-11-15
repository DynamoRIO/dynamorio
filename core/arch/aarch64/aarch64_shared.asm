/* **********************************************************
 * Copyright (c) 2016 ARM Limited. All rights reserved.
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
 * * Neither the name of ARM Limited nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL ARM LIMITED OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/***************************************************************************
 * AArch64-specific assembly and trampoline code, shared with non-core-DR-lib
 */

#include "../asm_defines.asm"
START_FILE

/*
 * ptr_int_t dynamorio_syscall(uint sysnum, uint num_args, ...);
 *
 * Linux arm64 system call:
 * - x8: syscall number
 * - x0..x6: syscall arguments
 */
        DECLARE_FUNC(dynamorio_syscall)
GLOBAL_LABEL(dynamorio_syscall:)
        cmp      w1, #7
        mov      x8,x0
        mov      x0,x2
        mov      x1,x3
        mov      x2,x4
        mov      x3,x5
        mov      x4,x6
        mov      x5,x7
        /* We set up first 6 args unconditionally, but read 7th arg from stack
         * only if there are at least 7 args.
         */
        b.cc     1f
        ldr      x6,[sp]
1:
        svc      #0
        ret

#define FUNCNAME dr_fpu_exception_init
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      x0, #0
        msr      fpcr, x0
        msr      fpsr, x0
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

/* CTR_EL0 [19:16] : Log2 of number of words in smallest dcache line
 * CTR_EL0 [3:0]   : Log2 of number of words in smallest icache line
 *
 * PoC = Point of Coherency
 * PoU = Point of Unification
 *
 * void cache_sync_asm(void *beg, void *end);
 *
 * FIXME i#1569: The values read from CTR_EL0 should be cached as
 * MRS Xt, CTR_EL0 may be emulated by kernel. This function could
 * be implemented in C with inline assembly.
 */
        DECLARE_FUNC(cache_sync_asm)
GLOBAL_LABEL(cache_sync_asm:)
        mrs      x3, ctr_el0
        mov      w4, #4
        ubfx     w2, w3, #16, #4
        lsl      w2, w4, w2 /* bytes in dcache line */
        and      w3, w3, #15
        lsl      w3, w4, w3 /* bytes in icache line */
        sub      w4, w2, #1
        bic      x4, x0, x4 /* aligned beg */
        b        2f
1:      dc       cvau, x4 /* Data Cache Clean by VA to PoU */
        add      x4, x4, x2
2:      cmp      x4, x1
        b.cc     1b
        dsb      ish /* Data Synchronization Barrier, Inner Shareable */
        sub      w4, w3, #1
        bic      x4, x0, x4 /* aligned beg */
        b        4f
3:      ic       ivau, x4 /* Instruction Cache Invalidate by VA to PoU */
        add      x4, x4, x3
4:      cmp      x4, x1
        b.cc     3b
        dsb      ish /* Data Synchronization Barrier, Inner Shareable */
        isb      /* Instruction Synchronization Barrier */
        ret
        END_FUNC(cache_sync_asm)

END_FILE
