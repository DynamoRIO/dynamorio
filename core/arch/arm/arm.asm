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
        push     {REG_R4-REG_R7}
        /* shift r7 pointing to the call args */
        add      REG_R7, sp, #16         /* size for {r4-r7} */
        mov      REG_R0, REG_R2          /* syscall arg1 */
        mov      REG_R1, REG_R3          /* syscall arg2 */
        ldmfd    REG_R7, {REG_R2-REG_R6} /* syscall arg3..arg7 */
        mov      REG_R7, REG_R0          /* sysnum */
        svc      #0
        pop      {REG_R4-REG_R7}
        bx       lr

/* void call_switch_stack(dcontext_t *dcontext,       // REG_R0
 *                        byte *stack,                // REG_R1
 *                        void (*func)(dcontext_t *), // REG_R2
 *                        void *mutex_to_free,        // REG_R3
 *                        bool return_on_return)      // [REG_SP]
 */
        DECLARE_FUNC(call_switch_stack)
GLOBAL_LABEL(call_switch_stack:)
        /* we need a callee-saved reg across our call so save it onto stack */
        push    {REG_R4, lr}
        /* check mutex_to_free */
        cmp     ARG4, #0
        beq     call_dispatch_alt_stack_no_free
        /* release the mutex */
        mov     REG_R4, #0
        str     REG_R4, [ARG4]
call_dispatch_alt_stack_no_free:
        /* switch stack */
        mov     REG_R4, sp
        mov     sp, ARG2
        /* call func */
        blx     ARG3
        /* switch stack back */
        mov     sp, REG_R4
        /* load ARG5 (return_on_return) from stack after the push at beginning */
        /* after call, so we can use REG_R3 as the scratch register */
        ldr     REG_R3, [sp, #8/* r4, lr */] /* ARG5 */
        cmp     REG_R3, #0
        beq     GLOBAL_REF(unexpected_return)
        /* restore and return */
        pop     {REG_R4, pc}
        END_FUNC(call_switch_stack)

# endif /* !X64 */
#endif /* UNIX */

END_FILE
