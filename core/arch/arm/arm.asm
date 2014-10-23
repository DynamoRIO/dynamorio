/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
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
 * ARM-specific assembly and trampoline code
 */

#include "../asm_defines.asm"
START_FILE


/* FIXME i#1551: just a shell to get things compiling.  We need to fill
 * in all the real functions later.
 */
#define FUNCNAME dr_fpu_exception_init
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        bx       lr
        END_FUNC(FUNCNAME)
#undef FUNCNAME

/* we share dynamorio_syscall w/ preload */
#ifdef UNIX
# ifdef X64
#  error AArch64 is not supported
# else /* !X64 */
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
        push     {r4-r7}
        /* shift r7 pointing to the call args */
        add      r7, sp, #16 /* size for {r4-r7} */
        mov      r0, r2      /* syscall arg1 */
        mov      r1, r3      /* syscall arg2 */
        ldmfd    r7, {r2-r6} /* syscall arg3..arg7 */
        mov      r7, r0      /* sysnum */
        svc      #0
        pop      {r4-r7}
        bx       lr
# endif /* !X64 */
#endif /* UNIX */

END_FILE
