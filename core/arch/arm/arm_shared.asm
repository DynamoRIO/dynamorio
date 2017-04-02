/* **********************************************************
 * Copyright (c) 2014-2016 Google, Inc.  All rights reserved.
 * ********************************************************** */

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
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 * ARM-specific assembly and trampoline code, shared w/ non-core-DR-lib.
 */

#include "../asm_defines.asm"
START_FILE

/* we share dynamorio_syscall w/ preload */
/* To avoid libc wrappers we roll our own syscall here.
 * Hardcoded to use svc/swi for 32-bit -- FIXME: use something like do_syscall
 * signature: dynamorio_syscall(sys_num, num_args, arg1, arg2, ...)
 * For Linux, the argument max is 6.
 */
/* Linux system call on AArch32:
 * - r7: syscall number
 * - r0..r6: syscall arguments
 * We need to not de-ref stack args that weren't passed so we can't ignore num_args.
 */
        DECLARE_FUNC(dynamorio_syscall)
GLOBAL_LABEL(dynamorio_syscall:)
        push     {REG_R4-REG_R9}
        /* Point r8 at the args on the stack */
        add      REG_R8, sp, #(6*ARG_SZ) /* size for {r4-r9} */
        mov      REG_R7, ARG1            /* sysnum */
        mov      REG_R0, ARG3            /* syscall arg1 */
        mov      REG_R9, ARG4            /* syscall arg2 */
        cmp      ARG2, #2
        blt      syscall_0_or_1arg
        beq      syscall_2args
        cmp      ARG2, #3
        beq      syscall_3args
        cmp      ARG2, #4
        beq      syscall_4args
        cmp      ARG2, #5
        beq      syscall_5args
        cmp      ARG2, #6
        beq      syscall_6args
syscall_7args:
        ldr      REG_R6, [REG_R8, #(4*ARG_SZ)]  /* syscall arg7 */
syscall_6args:
        ldr      REG_R5, [REG_R8, #(3*ARG_SZ)]  /* syscall arg6 */
syscall_5args:
        ldr      REG_R4, [REG_R8, #(2*ARG_SZ)]  /* syscall arg5 */
syscall_4args:
        ldr      REG_R3, [REG_R8, #(1*ARG_SZ)]  /* syscall arg4 */
syscall_3args:
        ldr      REG_R2, [REG_R8, #(0*ARG_SZ)]  /* syscall arg3 */
syscall_2args:
        mov      REG_R1, REG_R9                 /* syscall arg2 */
syscall_0_or_1arg:
        /* arg1 is already in place */
        svc      #0
        pop      {REG_R4-REG_R9}
        bx       lr

/* FIXME i#1551: just a shell to get things compiling.  We need to fill
 * in the real implementation.
 */
#define FUNCNAME dr_fpu_exception_init
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        bx       lr
        END_FUNC(FUNCNAME)
#undef FUNCNAME

END_FILE
