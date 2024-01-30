/* **********************************************************
 * Copyright (c) 2022 Rivos, Inc.  All rights reserved.
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
 * * Neither the name of Rivos, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL RIVOS, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/***************************************************************************
 * RISC-V-specific assembly and trampoline code, shared with non-core-DR-lib
 */

#include "../asm_defines.asm"
START_FILE

#ifndef LINUX
# error Non-Linux is not supported
#endif

#include "include/syscall.h"

DECL_EXTERN(unexpected_return)

/*
 * ptr_int_t dynamorio_syscall(uint sysnum, uint num_args, ...);
 *
 * Linux riscv64 system call:
 * - a7: syscall number
 * - a0..a5: syscall arguments
 */
        DECLARE_FUNC(dynamorio_syscall)
GLOBAL_LABEL(dynamorio_syscall:)
        mv      t0,a0
        mv      a0,a2
        mv      a1,a3
        mv      a2,a4
        mv      a3,a5
        mv      a4,a6
        mv      a5,a7
        mv      a7,t0
        ecall
        ret

#define FUNCNAME dr_fpu_exception_init
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        fsflags x0
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

/*
 * thread_id_t dynamorio_clone(uint flags, byte *newsp, void *ptid, void *tls,
 *                             void *ctid, void (*func)(void))
 */
        DECLARE_FUNC(dynamorio_clone)
GLOBAL_LABEL(dynamorio_clone:)
        addi     ARG2, ARG2, -16 /* Description: newsp = newsp - 16. */
        sd       ARG6, 0 (ARG2) /* The func is now on TOS of newsp. */
        li       SYSNUM_REG, SYS_clone /* All args are already in syscall registers.*/
        ecall
        bnez     ARG1, dynamorio_clone_parent
        ld       ARG1, 0 (sp)
        addi     sp, sp, 16
        jalr     ARG1
        jal      GLOBAL_REF(unexpected_return)
dynamorio_clone_parent:
        ret
        END_FUNC(dynamorio_clone)

END_FILE
