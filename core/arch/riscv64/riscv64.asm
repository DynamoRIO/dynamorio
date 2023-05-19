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

/* RISC-V-specific assembly and trampoline code */

#include "../asm_defines.asm"
START_FILE
#include "include/syscall.h"

#ifndef LINUX
# error Non-Linux is not supported
#endif

/* offsetof(dcontext_t, dstack) */
#define dstack_OFFSET     0x458
/* offsetof(dcontext_t, is_exiting) */
#define is_exiting_OFFSET (dstack_OFFSET+1*ARG_SZ)

#ifndef RISCV64
# error RISCV64 must be defined
#endif

/* All CPU ID registers are accessible only in privileged modes. */
        DECLARE_FUNC(cpuid_supported)
GLOBAL_LABEL(cpuid_supported:)
        li      a0, 0
        ret
        END_FUNC(cpuid_supported)

/* void call_switch_stack(void *func_arg,             // REG_A0
 *                        byte *stack,                // REG_A1
 *                        void (*func)(void *arg),    // REG_A2
 *                        void *mutex_to_free,        // REG_A3
 *                        bool return_on_return)      // REG_A4
 */
        DECLARE_FUNC(call_switch_stack)
GLOBAL_LABEL(call_switch_stack:)
        /* Init the stack. */
        addi     sp, sp, -32
        /* Use two callee-save regs to call func. */
        sd       ra, 24 (sp)
        sd       s0, 16 (sp)
        sd       s1, 8 (sp)
        sd       s2, 0 (sp)
        /* Check mutex_to_free. */
        beqz     ARG4, call_dispatch_alt_stack_no_free
        /* Release the mutex. */
        sd       x0, 0 (ARG4)
call_dispatch_alt_stack_no_free:
        /* Copy AGG5 (return_on_return) to callee-save reg. */
        mv       s2, ARG5
        /* Switch the stack. */
        addi     s0, sp, 0
        addi     sp, ARG2, 0
        /* Call func. */
        jr       ARG3
        /* Switch stack back. */
        addi     sp, s0, 0
        beqz     s2, GLOBAL_LABEL(unexpected_return)
        /* Restore the stack. */
        ld       s2, 0 (sp)
        ld       s1, 8 (sp)
        ld       s0, 16 (sp)
        ld       ra, 24 (sp)
        addi     sp, sp, 32
        ret
        END_FUNC(call_switch_stack)

/*
 * Calls the specified function 'func' after switching to the DR stack
 * for the thread corresponding to 'drcontext'.
 * Passes in 8 arguments.  Uses the C calling convention, so 'func' will work
 * just fine even if it takes fewer than 8 args.
 * Swaps the stack back upon return and returns the value returned by 'func'.
 *
 * void * dr_call_on_clean_stack(void *drcontext,
 *                               void *(*func)(arg1...arg8),
 *                               void *arg1,
 *                               void *arg2,
 *                               void *arg3,
 *                               void *arg4,
 *                               void *arg5,
 *                               void *arg6,
 *                               void *arg7,
 *                               void *arg8)
 */
        DECLARE_EXPORTED_FUNC(dr_call_on_clean_stack)
GLOBAL_LABEL(dr_call_on_clean_stack:)
        addi     sp, sp, -16
        sd       ra, 8(sp)   /* Save link register. */
        sd       s0, 0(sp)
        mv       s0, sp      /* Save sp across the call. */
        /* Swap stacks. */
        ld       sp, dstack_OFFSET(ARG1)
        /* Set up args. */
        mv       ra,   ARG2 /* void *(*func)(arg1...arg8) */
        mv       ARG1, ARG3          /* void *arg1 */
        mv       ARG2, ARG4          /* void *arg2 */
        mv       ARG3, ARG5          /* void *arg3 */
        mv       ARG4, ARG6          /* void *arg4 */
        mv       ARG5, ARG7          /* void *arg5 */
        mv       ARG6, ARG8          /* void *arg6 */
        ld       ARG7, 2*ARG_SZ(s0)  /* void *arg7, retrived from the stack */
        ld       ARG8, 3*ARG_SZ(s0)  /* void *arg8, retrived from the stack */
        jalr     ra
        /* Swap stacks. */
        mv       sp, s0
        ld       s0, 0 (sp)
        ld       ra, 8 (sp)
        addi     sp, sp, 16
        ret
        END_FUNC(dr_call_on_clean_stack)

#ifndef NOT_DYNAMORIO_CORE_PROPER

#ifdef DR_APP_EXPORTS

        DECLARE_EXPORTED_FUNC(dr_app_start)
GLOBAL_LABEL(dr_app_start:)
/* FIXME i#3544: Not implemented */
        ret
        END_FUNC(dr_app_start)

        DECLARE_EXPORTED_FUNC(dr_app_take_over)
GLOBAL_LABEL(dr_app_take_over:)
/* FIXME i#3544: Not implemented */
        j        GLOBAL_REF(dynamorio_app_take_over)
        END_FUNC(dr_app_take_over)

        DECLARE_EXPORTED_FUNC(dr_app_running_under_dynamorio)
GLOBAL_LABEL(dr_app_running_under_dynamorio:)
/* FIXME i#3544: Not implemented */
        ret
        END_FUNC(dr_app_running_under_dynamorio)

#endif /* DR_APP_EXPORTS */

        DECLARE_EXPORTED_FUNC(dynamorio_app_take_over)
GLOBAL_LABEL(dynamorio_app_take_over:)
/* FIXME i#3544: Not implemented */
        ret
        END_FUNC(dynamorio_app_take_over)

/*
 * cleanup_and_terminate(dcontext_t *dcontext,            // A0
 *                       int sysnum,                      // A1
 *                       ptr_uint_t sys_arg1/param_base,  // A2
 *                       ptr_uint_t sys_arg2,             // A3
 *                       bool exitproc,                   // A4
 *                       (2 more args that are ignored: Mac-only))
 *
 * See decl in arch_exports.h for description.
 */
        DECLARE_FUNC(cleanup_and_terminate)
GLOBAL_LABEL(cleanup_and_terminate:)
/* FIXME i#3544: Not implemented */
        jal       GLOBAL_REF(unexpected_return)
        END_FUNC(cleanup_and_terminate)

#endif /* NOT_DYNAMORIO_CORE_PROPER */

        /* void atomic_add(int *adr, int val) */
        DECLARE_FUNC(atomic_add)
GLOBAL_LABEL(atomic_add:)
1:      lr.d       a2, (a0)
        add        a2, a2, a1
        sc.d       a3, a2, (a0)
        bnez       a3, 1b
        ret
        END_FUNC(atomic_add)

        DECLARE_FUNC(global_do_syscall_int)
GLOBAL_LABEL(global_do_syscall_int:)
/* FIXME i#3544: Not implemented */
        jal       GLOBAL_REF(unexpected_return)
        END_FUNC(global_do_syscall_int)

DECLARE_GLOBAL(safe_read_asm_pre)
DECLARE_GLOBAL(safe_read_asm_mid)
DECLARE_GLOBAL(safe_read_asm_post)
DECLARE_GLOBAL(safe_read_asm_recover)

/*
 * void *safe_read_asm(void *dst, const void *src, size_t n);
 */
        DECLARE_FUNC(safe_read_asm)
GLOBAL_LABEL(safe_read_asm:)
1:      beqz    ARG3, safe_read_asm_recover
ADDRTAKEN_LABEL(safe_read_asm_pre:)
        lbu     a3, 0(ARG2)
ADDRTAKEN_LABEL(safe_read_asm_mid:)
ADDRTAKEN_LABEL(safe_read_asm_post:)
        sb      a3, 0(ARG1)
        addi    ARG3, ARG3, -1
        addi    ARG2, ARG2, 1
        addi    ARG1, ARG1, 1
        j       1b
ADDRTAKEN_LABEL(safe_read_asm_recover:)
        mv      a0, ARG2
        ret
        END_FUNC(safe_read_asm)

/*
 * int dr_try_start(try_except_context_t *cxt) ;
 */
        DECLARE_EXPORTED_FUNC(dr_try_start)
GLOBAL_LABEL(dr_try_start:)
        addi      ARG1, ARG1, TRY_CXT_SETJMP_OFFS
        j        GLOBAL_REF(dr_setjmp)
        END_FUNC(dr_try_start)

/*
 * int dr_setjmp(dr_jmp_buf_t *buf);
 */
        DECLARE_FUNC(dr_setjmp)
GLOBAL_LABEL(dr_setjmp:)
/* FIXME i#3544: Not implemented */
        ret
        END_FUNC(dr_setjmp)

/*
 * int dr_longjmp(dr_jmp_buf_t *buf,  int val);
 */
        DECLARE_FUNC(dr_longjmp)
GLOBAL_LABEL(dr_longjmp:)
/* FIXME i#3544: Not implemented */
        ret
        END_FUNC(dr_longjmp)

        /* int atomic_swap(int *adr, int val) */
        DECLARE_FUNC(atomic_swap)
GLOBAL_LABEL(atomic_swap:)
/* FIXME i#3544: Not implemented */
        ret
        END_FUNC(atomic_swap)

#ifdef UNIX
        DECLARE_FUNC(client_int_syscall)
GLOBAL_LABEL(client_int_syscall:)
/* FIXME i#3544: Not implemented */
        jal       GLOBAL_REF(unexpected_return)
        END_FUNC(client_int_syscall)

        DECLARE_FUNC(native_plt_call)
GLOBAL_LABEL(native_plt_call:)
/* FIXME i#3544: Not implemented */
        jal       GLOBAL_REF(unexpected_return)
        END_FUNC(native_plt_call)

        DECLARE_FUNC(_dynamorio_runtime_resolve)
GLOBAL_LABEL(_dynamorio_runtime_resolve:)
/* FIXME i#3544: Not implemented */
        jal       GLOBAL_REF(unexpected_return)
        END_FUNC(_dynamorio_runtime_resolve)
#endif /* UNIX */

#ifdef LINUX
/*
 * thread_id_t dynamorio_clone(uint flags, byte *newsp, void *ptid, void *tls,
 *                             void *ctid, void (*func)(void))
 */
        DECLARE_FUNC(dynamorio_clone)
GLOBAL_LABEL(dynamorio_clone:)
/* FIXME i#3544: Not implemented */
        ret
        END_FUNC(dynamorio_clone)

        DECLARE_FUNC(dynamorio_sigreturn)
GLOBAL_LABEL(dynamorio_sigreturn:)
        li        SYSNUM_REG, SYS_rt_sigreturn
        ecall
        jal       GLOBAL_REF(unexpected_return)
        END_FUNC(dynamorio_sigreturn)

        DECLARE_FUNC(dynamorio_sys_exit)
GLOBAL_LABEL(dynamorio_sys_exit:)
        li        a0, 0 /* exit code */
        li        SYSNUM_REG, SYS_exit /* SYS_exit number */
        ecall
        jal       GLOBAL_REF(unexpected_return)
        END_FUNC(dynamorio_sys_exit)

# ifndef NOT_DYNAMORIO_CORE_PROPER

#  ifndef HAVE_SIGALTSTACK
#   error NYI
#  endif
        DECLARE_FUNC(main_signal_handler)
GLOBAL_LABEL(main_signal_handler:)
        mv      ARG4, sp /* pass as extra arg */
        j        GLOBAL_REF(main_signal_handler_C) /* chain call */
        END_FUNC(main_signal_handler)

# endif /* NOT_DYNAMORIO_CORE_PROPER */

#endif /* LINUX */

        DECLARE_FUNC(hashlookup_null_handler)
GLOBAL_LABEL(hashlookup_null_handler:)
/* FIXME i#3544: Not implemented */
        jal       GLOBAL_REF(unexpected_return)
        END_FUNC(hashlookup_null_handler)

        DECLARE_FUNC(back_from_native_retstubs)
GLOBAL_LABEL(back_from_native_retstubs:)
DECLARE_GLOBAL(back_from_native_retstubs_end)
ADDRTAKEN_LABEL(back_from_native_retstubs_end:)
/* FIXME i#3544: Not implemented */
        jal       GLOBAL_REF(unexpected_return)
        END_FUNC(back_from_native_retstubs)

        DECLARE_FUNC(back_from_native)
GLOBAL_LABEL(back_from_native:)
/* FIXME i#3544: Not implemented */
        jal       GLOBAL_REF(unexpected_return)
        END_FUNC(back_from_native)

#ifdef UNIX
# if !defined(STANDALONE_UNIT_TEST) && !defined(STATIC_LIBRARY)
        DECLARE_FUNC(_start)
GLOBAL_LABEL(_start:)
        nop
        mv       fp, x0  /* Clear frame ptr for stack trace bottom. */

        CALLC3(GLOBAL_REF(relocate_dynamorio), 0, 0, sp)
        mv       ARG2, x0
        mv       ARG3, x0

        /* Clear 2nd & 3rd args to distinguish from xfer_to_new_libdr. */
GLOBAL_LABEL(.L_start_invoke_C:)
        mv       fp, x0 /* Clear frame ptr for stack trace bottom. */
        mv       ARG1, sp /* 1st arg to privload_early_inject. */
        jal      GLOBAL_REF(privload_early_inject)
        /* Shouldn't return. */
        jal     GLOBAL_REF(unexpected_return)
        END_FUNC(_start)

/* i#1227: on a conflict with the app we reload ourselves.
 * xfer_to_new_libdr(entry, init_sp, cur_dr_map, cur_dr_size)
 * =>
 * Invokes entry after setting sp to init_sp and placing the current (old)
 * libdr bounds in registers for the new libdr to unmap.
 */
        DECLARE_FUNC(xfer_to_new_libdr)
GLOBAL_LABEL(xfer_to_new_libdr:)
        mv       s0, ARG1
        /* Restore sp */
        mv       sp, ARG2
        la       ARG1, GLOBAL_REF(.L_start_invoke_C)
        la       ARG2, GLOBAL_REF(_start)
        sub      ARG1, ARG1, ARG2
        add      s0, s0, ARG1
        /* _start expects these as 2nd & 3rd args */
        mv       ARG2, ARG3
        mv       ARG3, ARG4
        jr       s0
        ret
        END_FUNC(xfer_to_new_libdr)
# endif /* !STANDALONE_UNIT_TEST && !STATIC_LIBRARY */
#endif /* UNIX */
/* We need to call futex_wakeall without using any stack.
 * Takes KSYNCH_TYPE* in a0 and the post-syscall jump target in a1
 */
        DECLARE_FUNC(dynamorio_condvar_wake_and_jmp)
GLOBAL_LABEL(dynamorio_condvar_wake_and_jmp:)
        mv       REG_R9, ARG2 /* save across syscall */
        li       ARG6, 0
        li       ARG5, 0
        li       ARG4, 0
        li       ARG3, 0x7fffffff /* arg3 = INT_MAX */
        li       ARG2, 1 /* arg2 = FUTEX_WAKE */
        li       SYSNUM_REG, 98 /* SYS_futex */
        ecall
        jr REG_R9
        END_FUNC(dynamorio_condvar_wake_and_jmp)

END_FILE
