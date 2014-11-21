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

#ifndef UNIX
# error Non-Unix is not supported
#endif

#ifdef X64
#  error AArch64 is not supported
#endif

/****************************************************************************/
#ifndef NOT_DYNAMORIO_CORE_PROPER
/****************************************************************************/

#ifdef UNIX
# if !defined(STANDALONE_UNIT_TEST) && !defined(STATIC_LIBRARY)
        DECLARE_FUNC(_start)
GLOBAL_LABEL(_start:)
        /* FIXME i#1551: NYI on ARM */
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(_start)
# endif /* !STANDALONE_UNIT_TEST && !STATIC_LIBRARY */
#endif /* UNIX */

/****************************************************************************/
#endif /* !NOT_DYNAMORIO_CORE_PROPER */
/****************************************************************************/

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


/* FIXME i#1551: NYI on ARM */
/*
 * dr_app_start - Causes application to run under Dynamo control
 */
#ifdef DR_APP_EXPORTS
        DECLARE_EXPORTED_FUNC(dr_app_start)
GLOBAL_LABEL(dr_app_start:)
        /* FIXME i#1551: NYI on ARM */
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(dr_app_start)

/*
 * dr_app_take_over - For the client interface, we'll export 'dr_app_take_over'
 * for consistency with the dr_ naming convention of all exported functions.
 * We'll keep 'dynamorio_app_take_over' for compatibility with the preinjector.
 */
        DECLARE_EXPORTED_FUNC(dr_app_take_over)
GLOBAL_LABEL(dr_app_take_over:)
        /* FIXME i#1551: NYI on ARM */
        b        GLOBAL_REF(dynamorio_app_take_over)
        END_FUNC(dr_app_take_over)

/* dr_app_running_under_dynamorio - Indicates whether the current thread
 * is running within the DynamoRIO code cache.
 * Returns false (not under dynamorio) by default.
 * The function is mangled by dynamorio to return true instead when
 * it is brought into the code cache.
 */
        DECLARE_EXPORTED_FUNC(dr_app_running_under_dynamorio)
GLOBAL_LABEL(dr_app_running_under_dynamorio:)
        /* FIXME i#1551: NYI on ARM */
        mov      r0, #0
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(dr_app_running_under_dynamorio)
#endif /* DR_APP_EXPORTS */

/*
 * dynamorio_app_take_over - Causes application to run under Dynamo
 * control.  Dynamo never releases control.
 */
        DECLARE_EXPORTED_FUNC(dynamorio_app_take_over)
GLOBAL_LABEL(dynamorio_app_take_over:)
        /* FIXME i#1551: NYI on ARM */
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(dynamorio_app_take_over)

        DECLARE_FUNC(cleanup_and_terminate)
GLOBAL_LABEL(cleanup_and_terminate:)
        /* FIXME i#1551: NYI on ARM */
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(cleanup_and_terminate)

        DECLARE_FUNC(global_do_syscall_int)
GLOBAL_LABEL(global_do_syscall_int:)
        /* FIXME i#1551: NYI on ARM */
        svc      #0
        END_FUNC(global_do_syscall_int)

DECLARE_GLOBAL(safe_read_asm_recover)

        DECLARE_FUNC(safe_read_asm)
GLOBAL_LABEL(safe_read_asm:)
        /* FIXME i#1551: NYI on ARM */
ADDRTAKEN_LABEL(safe_read_asm_recover:)
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(safe_read_asm)

#ifdef CLIENT_INTERFACE
/* int cdecl dr_setjmp(dr_jmp_buf *buf);
 */
        DECLARE_FUNC(dr_setjmp)
GLOBAL_LABEL(dr_setjmp:)
        /* FIXME i#1551: NYI on ARM */
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(dr_set_jmp)

/* int cdecl dr_longjmp(dr_jmp_buf *buf, int retval);
 */
        DECLARE_FUNC(dr_longjmp)
GLOBAL_LABEL(dr_longjmp:)
        /* FIXME i#1551: NYI on ARM */
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(dr_longjmp)

/* uint atomic_swap(uint *addr, uint value)
 * return current contents of addr and replace contents with value.
 * on win32 could use InterlockedExchange intrinsic instead.
 */
        DECLARE_FUNC(atomic_swap)
GLOBAL_LABEL(atomic_swap:)
        /* FIXME i#1551: NYI on ARM */
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(atomic_swap)

        DECLARE_FUNC(cpuid_supported)
GLOBAL_LABEL(cpuid_supported:)
        /* FIXME i#1551: NYI on ARM */
        mov      r0, #0
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(cpuid_supported)

        DECLARE_FUNC(our_cpuid)
GLOBAL_LABEL(our_cpuid:)
        /* FIXME i#1551: NYI on ARM */
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(our_cpuid)

#endif /* CLIENT_INTERFACE */

#ifdef UNIX
        DECLARE_FUNC(client_int_syscall)
GLOBAL_LABEL(client_int_syscall:)
        /* FIXME i#1551: NYI on ARM */
        svc      #0
        blx      lr
        END_FUNC(client_int_syscall)

        DECLARE_FUNC(native_plt_call)
GLOBAL_LABEL(native_plt_call:)
        /* FIXME i#1551: NYI on ARM */
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(native_plt_call)

        DECLARE_FUNC(_dynamorio_runtime_resolve)
GLOBAL_LABEL(_dynamorio_runtime_resolve:)
        /* FIXME i#1551: NYI on ARM */
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(_dynamorio_runtime_resolve)

#endif /* UNIX */

#ifdef LINUX

        DECLARE_FUNC(dynamorio_clone)
GLOBAL_LABEL(dynamorio_clone:)
        /* FIXME i#1551: NYI on ARM */
        mov      r0, #0
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(dynamorio_clone)

        DECLARE_FUNC(dynamorio_sigreturn)
GLOBAL_LABEL(dynamorio_sigreturn:)
        /* FIXME i#1551: NYI on ARM */
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(dynamorio_sigreturn)

        DECLARE_FUNC(dynamorio_nonrt_sigreturn)
GLOBAL_LABEL(dynamorio_nonrt_sigreturn:)
        /* FIXME i#1551: NYI on ARM */
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(dynamorio_nonrt_sigreturn)

        DECLARE_FUNC(master_signal_handler)
GLOBAL_LABEL(master_signal_handler:)
        /* FIXME i#1551: NYI on ARM */
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(master_signal_handler)

#endif /* LINUX */
/* void hashlookup_null_handler(void)
 * PR 305731: if the app targets NULL, it ends up here, which indirects
 * through hashlookup_null_target to end up in an ibl miss routine.
 */
        DECLARE_FUNC(hashlookup_null_handler)
GLOBAL_LABEL(hashlookup_null_handler:)
        /* FIXME i#1551: NYI on ARM */
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(hashlookup_null_handler)

        DECLARE_FUNC(back_from_native_retstubs)
GLOBAL_LABEL(back_from_native_retstubs:)
        /* FIXME i#1551: NYI on ARM */
DECLARE_GLOBAL(back_from_native_retstubs_end)
ADDRTAKEN_LABEL(back_from_native_retstubs_end:)
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(back_from_native_retstubs)

        DECLARE_FUNC(back_from_native)
GLOBAL_LABEL(back_from_native:)
        /* FIXME i#1551: NYI on ARM */
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(back_from_native)

END_FILE
