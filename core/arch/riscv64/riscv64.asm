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

/* sizeof(priv_mcontext_t) rounded up to a multiple of 16 */
/* The reserved space for SIMD is also included. */
#define PRIV_MCONTEXT_SIZE 0x290

/* offset of priv_mcontext_t in dr_mcontext_t */
#define PRIV_MCONTEXT_OFFSET 16

#if PRIV_MCONTEXT_OFFSET < 16 || PRIV_MCONTEXT_OFFSET % 16 != 0
# error PRIV_MCONTEXT_OFFSET
#endif

/* offsetof(dcontext_t, dstack) */
#define dstack_OFFSET     0x2d8
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
        sd       ra, 16(sp)
        /* Use two callee-saved regs to call func. */
        sd       s0, 8 (sp)
        sd       s1, 0 (sp)
        /* Check mutex_to_free. */
        beqz     ARG4, call_dispatch_alt_stack_no_free
        /* Release the mutex. */
        sd       x0, 0(ARG4)
call_dispatch_alt_stack_no_free:
        /* Copy ARG5 (return_on_return) to callee-saved reg. */
        mv       s1, ARG5
        /* Switch the stack. */
        mv       s0, sp
        mv       sp, ARG2
        /* Call func. */
        jalr     ARG3
        /* Switch stack back. */
        mv       sp, s0
        beqz     s1, GLOBAL_LABEL(unexpected_return)
        /* Restore the stack. */
        ld       s1, 0 (sp)
        ld       s0, 8 (sp)
        ld       ra, 16(sp)
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

/* Save priv_mcontext_t, except for x0(zero), x1(ra), x2(sp), x3(scratch),
 * x10(arg1) and pc, to the address in ARG1.
 * Typically the caller will save those five registers itself before calling this.
 */
save_priv_mcontext_helper:
        sd       x4,   4*ARG_SZ(ARG1)
        sd       x5,   5*ARG_SZ(ARG1)
        sd       x6,   6*ARG_SZ(ARG1)
        sd       x7,   7*ARG_SZ(ARG1)
        sd       x8,   8*ARG_SZ(ARG1)
        sd       x9,   9*ARG_SZ(ARG1)
        /* a0 (10*ARG_SZ) is already saved. */
        sd       x11, 11*ARG_SZ(ARG1)
        sd       x12, 12*ARG_SZ(ARG1)
        sd       x13, 13*ARG_SZ(ARG1)
        sd       x14, 14*ARG_SZ(ARG1)
        sd       x15, 15*ARG_SZ(ARG1)
        sd       x16, 16*ARG_SZ(ARG1)
        sd       x17, 17*ARG_SZ(ARG1)
        sd       x18, 18*ARG_SZ(ARG1)
        sd       x19, 19*ARG_SZ(ARG1)
        sd       x20, 20*ARG_SZ(ARG1)
        sd       x21, 21*ARG_SZ(ARG1)
        sd       x22, 22*ARG_SZ(ARG1)
        sd       x23, 23*ARG_SZ(ARG1)
        sd       x24, 24*ARG_SZ(ARG1)
        sd       x25, 25*ARG_SZ(ARG1)
        sd       x26, 26*ARG_SZ(ARG1)
        sd       x27, 27*ARG_SZ(ARG1)
        sd       x28, 28*ARG_SZ(ARG1)
        sd       x29, 29*ARG_SZ(ARG1)
        sd       x30, 30*ARG_SZ(ARG1)
        sd       x31, 31*ARG_SZ(ARG1)
        /* pc (32*ARG_SZ) is already saved. */
        fsd      f0,  33*ARG_SZ(ARG1)
        fsd      f1,  34*ARG_SZ(ARG1)
        fsd      f2,  35*ARG_SZ(ARG1)
        fsd      f3,  36*ARG_SZ(ARG1)
        fsd      f4,  37*ARG_SZ(ARG1)
        fsd      f5,  38*ARG_SZ(ARG1)
        fsd      f6,  39*ARG_SZ(ARG1)
        fsd      f7,  40*ARG_SZ(ARG1)
        fsd      f8,  41*ARG_SZ(ARG1)
        fsd      f9,  42*ARG_SZ(ARG1)
        fsd      f10, 43*ARG_SZ(ARG1)
        fsd      f11, 44*ARG_SZ(ARG1)
        fsd      f12, 45*ARG_SZ(ARG1)
        fsd      f13, 46*ARG_SZ(ARG1)
        fsd      f14, 47*ARG_SZ(ARG1)
        fsd      f15, 48*ARG_SZ(ARG1)
        fsd      f16, 49*ARG_SZ(ARG1)
        fsd      f17, 50*ARG_SZ(ARG1)
        fsd      f18, 51*ARG_SZ(ARG1)
        fsd      f19, 52*ARG_SZ(ARG1)
        fsd      f20, 53*ARG_SZ(ARG1)
        fsd      f21, 54*ARG_SZ(ARG1)
        fsd      f22, 55*ARG_SZ(ARG1)
        fsd      f23, 56*ARG_SZ(ARG1)
        fsd      f24, 57*ARG_SZ(ARG1)
        fsd      f25, 58*ARG_SZ(ARG1)
        fsd      f26, 59*ARG_SZ(ARG1)
        fsd      f27, 60*ARG_SZ(ARG1)
        fsd      f28, 61*ARG_SZ(ARG1)
        fsd      f29, 62*ARG_SZ(ARG1)
        fsd      f30, 63*ARG_SZ(ARG1)
        fsd      f31, 64*ARG_SZ(ARG1)
        frcsr    x3
        sd       x3,  65*ARG_SZ(ARG1)
        /* No need to save simd registers, at least for now. */
        ret

        DECLARE_EXPORTED_FUNC(dr_app_start)
GLOBAL_LABEL(dr_app_start:)
        /* Save link register for the case that DR is not taking over. */
        addi     sp, sp, -16
        sd       ra, 0(sp)

        /* Preverse stack space. */
        addi     sp, sp, -PRIV_MCONTEXT_SIZE
        /* Push x3 on to stack to use it as a scratch. */
        sd       x3, 3*ARG_SZ(sp)
        /* Compute original sp. */
        addi     x3, sp, PRIV_MCONTEXT_SIZE+16
        /* Save original sp on to stack. */
        sd       x3, 2*ARG_SZ(sp)
        /* Save link register on to stack. */
        sd       ra, 1*ARG_SZ(sp)
        /* Save ra as pc */
        sd       ra, 32*ARG_SZ(sp)
        /* Save arg1 */
        sd       ARG1, 10*ARG_SZ(sp)
        CALLC1(save_priv_mcontext_helper, sp)
        CALLC1(GLOBAL_REF(dr_app_start_helper), sp)
        /* If we get here, DR is not taking over. */
        addi     sp, sp, PRIV_MCONTEXT_SIZE
        ld       ra, 0(sp)
        addi     sp, sp, 16
        ret
        END_FUNC(dr_app_start)

        DECLARE_EXPORTED_FUNC(dr_app_take_over)
GLOBAL_LABEL(dr_app_take_over:)
        j        GLOBAL_REF(dynamorio_app_take_over)
        END_FUNC(dr_app_take_over)

        DECLARE_EXPORTED_FUNC(dr_app_running_under_dynamorio)
GLOBAL_LABEL(dr_app_running_under_dynamorio:)
        li       ARG1, 0 /* This instruction is manged by mangle_pre_client. */
        ret
        END_FUNC(dr_app_running_under_dynamorio)

#endif /* DR_APP_EXPORTS */

        DECLARE_EXPORTED_FUNC(dynamorio_app_take_over)
GLOBAL_LABEL(dynamorio_app_take_over:)
        /* Save link register for the case that DR is not taking over. */
        addi     sp, sp, -16
        sd       ra, 0(sp)

        /* Preverse stack space. */
        addi     sp, sp, -PRIV_MCONTEXT_SIZE
        /* Push x3 on to stack to use it as a scratch. */
        sd       x3, 3*ARG_SZ(sp)
        /* Compute original sp. */
        addi     x3, sp, PRIV_MCONTEXT_SIZE+16
        /* Save original sp on to stack. */
        sd       x3, 2*ARG_SZ(sp)
        /* Save link register on to stack. */
        sd       ra, 1*ARG_SZ(sp)
        /* Save ra as pc */
        sd       ra, 32*ARG_SZ(sp)
        /* Save arg1 */
        sd       ARG1, 10*ARG_SZ(sp)
        CALLC1(save_priv_mcontext_helper, sp)
        CALLC1(GLOBAL_REF(dynamorio_app_take_over_helper), sp)
        /* If we get here, DR is not taking over. */
        addi     sp, sp, PRIV_MCONTEXT_SIZE
        ld       ra, 0(sp)
        addi     sp, sp, 16
        ret
        END_FUNC(dynamorio_app_take_over)

/*
 * cleanup_and_terminate(dcontext_t *dcontext,            // A0
 *                       int sysnum,                      // A1
 *                       ptr_uint_t sys_arg1/param_base,  // A2
 *                       ptr_uint_t sys_arg2,             // A3
 *                       bool exitproc)                   // A4
 *
 * See decl in arch_exports.h for description.
 */
        DECLARE_FUNC(cleanup_and_terminate)
GLOBAL_LABEL(cleanup_and_terminate:)
        /* Move argument registers to callee saved registers. */
        mv       s0, ARG1  /* dcontext ptr size */
        mv       s1, ARG2  /* sysnum */
        mv       s2, ARG3  /* sys_arg1 */
        mv       s3, ARG4  /* sys_arg2 */
        mv       s4, ARG5  /* exitproc */
                          /* s5 reserved for dstack ptr */
                          /* s6 reserved for syscall ptr */

        li       a1, 1
        /* Inc exiting_thread_count to avoid being killed once off all_threads list. */
        la       a0, GLOBAL_REF(exiting_thread_count)
        amoadd.w zero, a1, (a0)

        /* Save dcontext->dstack for freeing later and set dcontext->is_exiting. */
        sw       a1, is_exiting_OFFSET(s0) /* dcontext->is_exiting = 1 */
        CALLC1(GLOBAL_REF(is_currently_on_dstack), s0)
        bnez     a0, cat_save_dstack
        li       s5, 0
        j        cat_done_saving_dstack
cat_save_dstack:
        ld       s5, dstack_OFFSET(s0)
cat_done_saving_dstack:
        CALLC0(GLOBAL_REF(get_cleanup_and_terminate_global_do_syscall_entry))
        mv       s6, a0
        beqz     s4, cat_thread_only
        CALLC0(GLOBAL_REF(dynamo_process_exit))
        j        cat_no_thread
cat_thread_only:
        CALLC0(GLOBAL_REF(dynamo_thread_exit))
cat_no_thread:
        /* Switch to d_r_initstack for cleanup of dstack. */
        la       s7, GLOBAL_REF(initstack_mutex)
cat_spin:
        /* We don't have a YIELD-like hint instruction like what's available on
         * Aarch64, so we use a plain spin lock here.
         */
        li       a0, 1
        amoswap.w a0, a0, (s7)
        bnez     a0, cat_spin

        /* We have the spin lock. */

        /* Switch stack. */
        la       a0, GLOBAL_REF(d_r_initstack)
        ld       sp, 0(a0)

        /* Free dstack and call the EXIT_DR_HOOK. */
        CALLC1(GLOBAL_REF(dynamo_thread_stack_free_and_exit), s5) /* pass dstack */

        /* Give up initstack_mutex. */
        la       a0, GLOBAL_REF(initstack_mutex)
        sd       zero, 0(a0)

        /* Dec exiting_thread_count (allows another thread to kill us). */
        la       a0, GLOBAL_REF(exiting_thread_count)
        li       a1, -1
        amoadd.w zero, a1, (a0)

        /* Put system call number in a7. */
        mv       a0, s2 /* sys_arg1 */
        mv       a1, s3 /* sys_arg2 */
        mv       SYSNUM_REG, s1 /* sys_call */

        jr       s6  /* Go do the syscall! */
        jal      GLOBAL_REF(unexpected_return)
        END_FUNC(cleanup_and_terminate)

#endif /* NOT_DYNAMORIO_CORE_PROPER */

        DECLARE_FUNC(global_do_syscall_int)
GLOBAL_LABEL(global_do_syscall_int:)
        ecall
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
        addi     ARG1, ARG1, TRY_CXT_SETJMP_OFFS
        j        GLOBAL_REF(dr_setjmp)
        END_FUNC(dr_try_start)

/* We save only callee-saved registers and ra: ra, SP, x8/fp, x9, x18-x27, f8-9, f18-27:
 * a total of 26 reg_t (64-bit) slots. See definition of dr_jmp_buf_t.
 *
 * int dr_setjmp(dr_jmp_buf_t *buf);
 */
        DECLARE_FUNC(dr_setjmp)
GLOBAL_LABEL(dr_setjmp:)
        sd       ra, 0 (ARG1)
        sd       sp, ARG_SZ (ARG1)
        sd       s0, 2*ARG_SZ (ARG1)
        sd       s1, 3*ARG_SZ (ARG1)
        sd       s2, 4*ARG_SZ (ARG1)
        sd       s3, 5*ARG_SZ (ARG1)
        sd       s4, 6*ARG_SZ (ARG1)
        sd       s5, 7*ARG_SZ (ARG1)
        sd       s6, 8*ARG_SZ (ARG1)
        sd       s7, 9*ARG_SZ (ARG1)
        sd       s8, 10*ARG_SZ (ARG1)
        sd       s9, 11*ARG_SZ (ARG1)
        sd       s10, 12*ARG_SZ (ARG1)
        sd       s11, 13*ARG_SZ (ARG1)
        fsd      fs0, 14*ARG_SZ (ARG1)
        fsd      fs1, 15*ARG_SZ (ARG1)
        fsd      fs2, 16*ARG_SZ (ARG1)
        fsd      fs3, 17*ARG_SZ (ARG1)
        fsd      fs4, 18*ARG_SZ (ARG1)
        fsd      fs5, 19*ARG_SZ (ARG1)
        fsd      fs6, 20*ARG_SZ (ARG1)
        fsd      fs7, 21*ARG_SZ (ARG1)
        fsd      fs8, 22*ARG_SZ (ARG1)
        fsd      fs9, 23*ARG_SZ (ARG1)
        fsd      fs10, 24*ARG_SZ (ARG1)
        fsd      fs11, 25*ARG_SZ (ARG1)
# ifdef UNIX
        addi     sp, sp, -16
        sd       ra, 0 (sp)
        jal      GLOBAL_REF(dr_setjmp_sigmask)
        ld       ra, 0 (sp)
        add      sp, sp, 16
# endif
        li       a0, 0
        ret
        END_FUNC(dr_setjmp)

/*
 * int dr_longjmp(dr_jmp_buf_t *buf,  int val);
 */
        DECLARE_FUNC(dr_longjmp)
GLOBAL_LABEL(dr_longjmp:)
        ld       ra, 0 (ARG1) /* Restore return address from buf */
        ld       sp, ARG_SZ (ARG1)
        ld       s0, 2*ARG_SZ (ARG1)
        ld       s1, 3*ARG_SZ (ARG1)
        ld       s2, 4*ARG_SZ (ARG1)
        ld       s3, 5*ARG_SZ (ARG1)
        ld       s4, 6*ARG_SZ (ARG1)
        ld       s5, 7*ARG_SZ (ARG1)
        ld       s6, 8*ARG_SZ (ARG1)
        ld       s7, 9*ARG_SZ (ARG1)
        ld       s8, 10*ARG_SZ (ARG1)
        ld       s9, 11*ARG_SZ (ARG1)
        ld       s10, 12*ARG_SZ (ARG1)
        ld       s11, 13*ARG_SZ (ARG1)
        fld      fs0, 14*ARG_SZ (ARG1)
        fld      fs1, 15*ARG_SZ (ARG1)
        fld      fs2, 16*ARG_SZ (ARG1)
        fld      fs3, 17*ARG_SZ (ARG1)
        fld      fs4, 18*ARG_SZ (ARG1)
        fld      fs5, 19*ARG_SZ (ARG1)
        fld      fs6, 20*ARG_SZ (ARG1)
        fld      fs7, 21*ARG_SZ (ARG1)
        fld      fs8, 22*ARG_SZ (ARG1)
        fld      fs9, 23*ARG_SZ (ARG1)
        fld      fs10, 24*ARG_SZ (ARG1)
        fld      fs11, 25*ARG_SZ (ARG1)
        seqz     ARG1, ARG2
        add      ARG1, ARG1, ARG2 /* ARG1 = ( ARG2 == 0 ) ? 1 : ARG2 */
        ret
        END_FUNC(dr_longjmp)

        /* int atomic_swap(int *adr, int val) */
        DECLARE_FUNC(atomic_swap)
GLOBAL_LABEL(atomic_swap:)
        amoswap.w       ARG1, ARG2, (ARG1)
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

        DECLARE_FUNC(dynamorio_sigreturn)
GLOBAL_LABEL(dynamorio_sigreturn:)
        li       SYSNUM_REG, SYS_rt_sigreturn
        ecall
        jal      GLOBAL_REF(unexpected_return)
        END_FUNC(dynamorio_sigreturn)

        DECLARE_FUNC(dynamorio_sys_exit)
GLOBAL_LABEL(dynamorio_sys_exit:)
        li       a0, 0 /* exit code */
        li       SYSNUM_REG, SYS_exit
        ecall
        jal      GLOBAL_REF(unexpected_return)
        END_FUNC(dynamorio_sys_exit)

# ifndef NOT_DYNAMORIO_CORE_PROPER

#  ifndef HAVE_SIGALTSTACK
#   error NYI
#  endif
        DECLARE_FUNC(main_signal_handler)
GLOBAL_LABEL(main_signal_handler:)
        mv       ARG4, sp /* pass as extra arg */
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

        /* Clear 2nd & 3rd args to distinguish from xfer_to_new_libdr. */
        mv       ARG2, x0
        mv       ARG3, x0

        /* Entry from xfer_to_new_libdr is here.  It has set up 2nd & 3rd args already. */
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
        /* Skip prologue that calls relocate_dynamorio() and clears args 2+3 by
         * adjusting the _start in the reloaded DR by the same distance as in
         * the current DR, but w/o clobbering ARG3 or ARG4.
         */
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
