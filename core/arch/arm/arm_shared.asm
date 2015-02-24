/* **********************************************************
 * Copyright (c) 2014-2015 Google, Inc.  All rights reserved.
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
 * so we simply set up all r0..r6 as arguments and ignore the passed in num_args.
 */
        DECLARE_FUNC(dynamorio_syscall)
GLOBAL_LABEL(dynamorio_syscall:)
        push     {REG_R4-REG_R8}
        /* shift r7 pointing to the call args */
        add      REG_R8, sp, #20         /* size for {r4-r8} */
        mov      REG_R7, ARG1            /* sysnum */
        mov      REG_R0, ARG3            /* syscall arg1 */
        mov      REG_R1, ARG4            /* syscall arg2 */
        ldmfd    REG_R8, {REG_R2-REG_R6} /* syscall arg3..arg7 */
        svc      #0
        pop      {REG_R4-REG_R8}
        bx       lr

END_FILE
