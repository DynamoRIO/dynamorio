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

        DECLARE_FUNC(dynamorio_syscall)
GLOBAL_LABEL(dynamorio_syscall:)
#ifdef LINUX
/*
 * ptr_int_t dynamorio_syscall(uint sysnum, uint num_args, ...);
 *
 * Linux arm64 system call:
 * - x8: syscall number
 * - x0..x6: syscall arguments
 */
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
#elif defined(MACOS)
/*
 * x0 = syscall number
 * x1 = number of arguments
 * sp+8*n = argument n
 */
        mov      x16, x1
        ldr      x1, [sp]
        sub      x16, x16, 1
        cbz      x16, do_svc
        ldr      x2, [sp, #8]
        sub      x16, x16, 1
        cbz      x16, do_svc
        ldr      x3, [sp, #16]
        sub      x16, x16, 1
        cbz      x16, do_svc
        ldr      x4, [sp, #24]
        sub      x16, x16, 1
        cbz      x16, do_svc
        ldr      x5, [sp, #32]
        sub      x16, x16, 1
        cbz      x16, do_svc
        ldr      x6, [sp, #40]
        sub      x16, x16, 1
        cbz      x16, do_svc
        ldr      x7, [sp, #48]
        sub      x16, x16, 1
        cbz      x16, do_svc
        ldr      x8, [sp, #56]
do_svc:
        mov      x16, #0
        svc      #0x80
        b.cs     err_cf
        ret
err_cf:
        mov      x1, x0
        mov      x0, #-1
        ret
#endif
        END_FUNC(dynamorio_syscall)

        DECLARE_FUNC(dr_fpu_exception_init)
GLOBAL_LABEL(dr_fpu_exception_init:)
        mov      x0, #0
        msr      fpcr, x0
        msr      fpsr, x0
        ret
        END_FUNC(dr_fpu_exception_init)

#ifdef MACOS
        DECLARE_FUNC(dynamorio_mach_dep_syscall)
GLOBAL_LABEL(dynamorio_mach_dep_syscall:)
        /* TODO i#5383: Use proper gateway. */
        brk 0xc001 /* For now we break with a unique code. */
        END_FUNC(dynamorio_mach_dep_syscall)

        DECLARE_FUNC(dynamorio_mach_syscall)
GLOBAL_LABEL(dynamorio_mach_syscall:)
        b _dynamorio_syscall
        END_FUNC(dynamorio_mach_syscall)
#endif

END_FILE
