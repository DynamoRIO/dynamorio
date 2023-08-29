/* **********************************************************
 * Copyright (c) 2019-2023 Google, Inc. All rights reserved.
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
 * AArch64-specific assembly and trampoline code
 */

#include "../asm_defines.asm"
START_FILE

#if !(defined(MACOS) && defined(AARCH64))
#include "include/syscall.h"
#endif

#ifndef UNIX
# error Non-Unix is not supported
#endif

/* sizeof(priv_mcontext_t) rounded up to a multiple of 16 */
#define PRIV_MCONTEXT_SIZE 3424

/* offsetof(spill_state_t, r0) */
#define spill_state_r0_OFFSET 0
/* offsetof(spill_state_t, r1) */
#define spill_state_r1_OFFSET 8
/* offsetof(spill_state_t, r2) */
#define spill_state_r2_OFFSET 16
/* offsetof(spill_state_t, r3) */
#define spill_state_r3_OFFSET 24
/* offsetof(spill_state_t, r4) */
#define spill_state_r4_OFFSET 32
/* offsetof(spill_state_t, r5) */
#define spill_state_r5_OFFSET 40
/* offsetof(spill_state_t, dcontext) */
#define spill_state_dcontext_OFFSET 56
/* offsetof(spill_state_t, fcache_return) */
#define spill_state_fcache_return_OFFSET 64

/* offsetof(priv_mcontext_t, simd) */
#define simd_OFFSET (16 * ARG_SZ*2 + 32)
/* offsetof(dcontext_t, dstack) */
#define dstack_OFFSET     0xda8
/* offsetof(dcontext_t, is_exiting) */
#define is_exiting_OFFSET (dstack_OFFSET+1*ARG_SZ)
/* offsetof(struct tlsdesc_t, arg) */
#define tlsdesc_arg_OFFSET 8

/* offsetof(icache_op_struct_t, flag) */
#define icache_op_struct_flag_OFFSET 0
/* offsetof(icache_op_struct_t, lock) */
#define icache_op_struct_lock_OFFSET 4
/* offsetof(icache_op_struct_t, linesize) */
#define icache_op_struct_linesize_OFFSET 8
/* offsetof(icache_op_struct_t, begin) */
#define icache_op_struct_begin_OFFSET 16
/* offsetof(icache_op_struct_t, end) */
#define icache_op_struct_end_OFFSET 24
/* offsetof(icache_op_struct_t, spill) */
#define icache_op_struct_spill_OFFSET 32

#ifndef X64
# error X64 must be defined
#endif

#if defined(UNIX)
DECL_EXTERN(dr_setjmp_sigmask)
#endif

DECL_EXTERN(d_r_internal_error)

DECL_EXTERN(exiting_thread_count)
DECL_EXTERN(d_r_initstack)
DECL_EXTERN(initstack_mutex)
DECL_EXTERN(icache_op_struct)
DECL_EXTERN(linkstub_selfmod)

/* For debugging: report an error if the function called by call_switch_stack()
 * unexpectedly returns.  Also used elsewhere.
 */
        DECLARE_FUNC(unexpected_return)
GLOBAL_LABEL(unexpected_return:)
        CALLC3(GLOBAL_REF(d_r_internal_error), HEX(0), HEX(0), HEX(0))
        /* d_r_internal_error normally never returns */
        /* Infinite loop is intentional.  Can we do better in release build?
         * XXX: why not a debug instr?
         */
        JUMP  GLOBAL_REF(unexpected_return)
        END_FUNC(unexpected_return)

/* bool mrs_id_reg_supported(void)
 * Checks for kernel support of the MRS instr when reading system registers
 * above exception level EL0, by attempting to read Instruction Set Attribute
 * Register 0. Some older Linux kernels do not support reading system registers
 * above exception level 0 (EL0), raising a SIGILL. This is rare now as later
 * versions have all implemented a trap-and-emulate mechanism for a set of
 * system registers above EL0, of which ID_AA64ISAR0_EL1 is one.
 */
        DECLARE_FUNC(mrs_id_reg_supported)
GLOBAL_LABEL(mrs_id_reg_supported:)
        mrs     x0, ID_AA64ISAR0_EL1
        mov     w0, #1
        ret
        END_FUNC(mrs_id_reg_supported)

/* void call_switch_stack(void *func_arg,             // REG_X0
 *                        byte *stack,                // REG_X1
 *                        void (*func)(void *arg),    // REG_X2
 *                        void *mutex_to_free,        // REG_X3
 *                        bool return_on_return)      // REG_W4
 */
        DECLARE_FUNC(call_switch_stack)
GLOBAL_LABEL(call_switch_stack:)
        /* We need two callee-save regs across the call to func. */
        stp      x20, x30, [sp, #-32]!
        str      x19, [sp, #16]
        /* Check mutex_to_free. */
        cbz      ARG4, call_dispatch_alt_stack_no_free
        /* Release the mutex. */
        str      wzr, [ARG4]
call_dispatch_alt_stack_no_free:
        /* Copy ARG5 (return_on_return) to callee-save reg. */
        mov      w20, w4
        /* Switch stack. */
        mov      x19, sp
        mov      sp, ARG2
        /* Call func. */
        blr      ARG3
        /* Switch stack back. */
        mov      sp, x19
        /* Test return_on_return. */
        cbz      w20, GLOBAL_REF(unexpected_return)
        /* Restore and return. */
        ldr      x19, [sp, #16]
        ldp      x20, x30, [sp], #32
        ret
        END_FUNC(call_switch_stack)

/*
 * Calls the specified function 'func' after switching to the DR stack
 * for the thread corresponding to 'drcontext'.
 * Passes in 8 arguments.  Uses the C calling convention, so 'func' will work
 * just fine even if if takes fewer than 8 args.
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
        /* We know that there are two arguments on stack. */
        stp      x29, x30, [sp, #-16]! /* Save frame pointer and link register. */
        mov      x29, sp /* Save sp across the call. */
        /* Swap stacks. */
        ldr      x30, [x0, #dstack_OFFSET]
        mov      sp, x30
        /* Set up args. */
        mov      x30, x1 /* void *(*func)(arg1...arg8) */
        mov      x0, x2  /* void *arg1 */
        mov      x1, x3  /* void *arg2 */
        mov      x2, x4  /* void *arg3 */
        mov      x3, x5  /* void *arg4 */
        mov      x4, x6  /* void *arg5 */
        mov      x5, x7  /* void *arg6 */
        ldp      x6, x7, [x29, #(2 * ARG_SZ)]   /* void *arg7, *arg8 */
        blr      x30
        /* Swap stacks. */
        mov      sp, x29
        ldp      x29, x30, [sp], #16
        ret
        END_FUNC(dr_call_on_clean_stack)

#ifndef NOT_DYNAMORIO_CORE_PROPER

#ifdef DR_APP_EXPORTS

/* Save priv_mcontext_t, except for X0, X1, X30, SP and PC, to the address in X0.
 * Typically the caller will save those five registers itself before calling this.
 * Clobbers X1-X4.
 */
save_priv_mcontext_helper:
        stp      x2, x3, [x0, #(1 * ARG_SZ*2)]
        stp      x4, x5, [x0, #(2 * ARG_SZ*2)]
        stp      x6, x7, [x0, #(3 * ARG_SZ*2)]
        stp      x8, x9, [x0, #(4 * ARG_SZ*2)]
        stp      x10, x11, [x0, #(5 * ARG_SZ*2)]
        stp      x12, x13, [x0, #(6 * ARG_SZ*2)]
        stp      x14, x15, [x0, #(7 * ARG_SZ*2)]
        stp      x16, x17, [x0, #(8 * ARG_SZ*2)]
        stp      x18, x19, [x0, #(9 * ARG_SZ*2)]
        stp      x20, x21, [x0, #(10 * ARG_SZ*2)]
        stp      x22, x23, [x0, #(11 * ARG_SZ*2)]
        stp      x24, x25, [x0, #(12 * ARG_SZ*2)]
        stp      x26, x27, [x0, #(13 * ARG_SZ*2)]
        stp      x28, x29, [x0, #(14 * ARG_SZ*2)]
        mrs      x1, nzcv
        mrs      x2, fpcr
        mrs      x3, fpsr
        str      w1, [x0, #(16 * ARG_SZ*2 + 8)]
        str      w2, [x0, #(16 * ARG_SZ*2 + 12)]
        str      w3, [x0, #(16 * ARG_SZ*2 + 16)]
        add      x4, x0, #simd_OFFSET
        st1      {v0.2d-v3.2d}, [x4], #64
        st1      {v4.2d-v7.2d}, [x4], #64
        st1      {v8.2d-v11.2d}, [x4], #64
        st1      {v12.2d-v15.2d}, [x4], #64
        st1      {v16.2d-v19.2d}, [x4], #64
        st1      {v20.2d-v23.2d}, [x4], #64
        st1      {v24.2d-v27.2d}, [x4], #64
        st1      {v28.2d-v31.2d}, [x4], #64
        /* TODO i#5365: Save Z/P regs as well? Will require runtime check of
         * ID_AA64PFR0_EL1 for FEAT_SVE.
         */
        ret

        DECLARE_EXPORTED_FUNC(dr_app_start)
GLOBAL_LABEL(dr_app_start:)
        /* Save FP and LR for the case that DR is not taking over. */
        stp      x29, x30, [sp, #-16]!
        /* Build a priv_mcontext_t on the stack. */
        sub      sp, sp, #PRIV_MCONTEXT_SIZE
        stp      x0, x1, [sp, #(0 * ARG_SZ*2)]
        add      x0, sp, #(PRIV_MCONTEXT_SIZE + 16) /* compute original SP */
        stp      x30, x0, [sp, #(15 * ARG_SZ*2)]
        str      x30, [sp, #(16 * ARG_SZ*2)] /* save LR as PC */
        CALLC1(save_priv_mcontext_helper, sp)
        CALLC1(GLOBAL_REF(dr_app_start_helper), sp)
        /* If we get here, DR is not taking over. */
        add      sp, sp, #PRIV_MCONTEXT_SIZE
        ldp      x29, x30, [sp], #16
        ret
        END_FUNC(dr_app_start)

        DECLARE_EXPORTED_FUNC(dr_app_take_over)
GLOBAL_LABEL(dr_app_take_over:)
        b        GLOBAL_REF(dynamorio_app_take_over)
        END_FUNC(dr_app_take_over)

        DECLARE_EXPORTED_FUNC(dr_app_running_under_dynamorio)
GLOBAL_LABEL(dr_app_running_under_dynamorio:)
        movz     w0, #0 /* This instruction is manged by mangle_pre_client. */
        ret
        END_FUNC(dr_app_running_under_dynamorio)

#endif /* DR_APP_EXPORTS */

        DECLARE_EXPORTED_FUNC(dynamorio_app_take_over)
GLOBAL_LABEL(dynamorio_app_take_over:)
        /* Save FP and LR for the case that DR is not taking over. */
        stp      x29, x30, [sp, #-16]!
        /* Build a priv_mcontext_t on the stack. */
        sub      sp, sp, #PRIV_MCONTEXT_SIZE
        stp      x0, x1, [sp, #(0 * ARG_SZ*2)]
        add      x0, sp, #(PRIV_MCONTEXT_SIZE + 16) /* compute original SP */
        stp      x30, x0, [sp, #(15 * ARG_SZ*2)]
        str      x30, [sp, #(16 * ARG_SZ*2)] /* save LR as PC */
        CALLC1(save_priv_mcontext_helper, sp)
        CALLC1(GLOBAL_REF(dynamorio_app_take_over_helper), sp)
        /* If we get here, DR is not taking over. */
        add      sp, sp, #PRIV_MCONTEXT_SIZE
        ldp      x29, x30, [sp], #16
        ret
        END_FUNC(dynamorio_app_take_over)

/*
 * cleanup_and_terminate(dcontext_t *dcontext,        // X0 -> X19
 *                   int sysnum,                      // X1 -> X20 = syscall #
 *                   ptr_uint_t sys_arg1/param_base,  // X2 -> X21 = arg1 for syscall
 *                   ptr_uint_t sys_arg2,             // X3 -> X22 = arg2 for syscall
 *                   bool exitproc,                   // X4 -> X23
 *                   (These 2 args are only used for Mac thread exit:)
 *                   ptr_uint_t sys_arg3,             // X5 -> X26 = arg3 for syscall
 *                   ptr_uint_t sys_arg4)             // X6 -> X27 = arg4 for syscall
 *
 * See decl in arch_exports.h for description.
 */
        DECLARE_FUNC(cleanup_and_terminate)
GLOBAL_LABEL(cleanup_and_terminate:)
        /* move argument registers to callee saved registers */
        mov      x19, x0  /* dcontext ptr size */
        mov      x20, x1  /* sysnum */
        mov      x21, x2  /* sys_arg1 */
        mov      x22, x3  /* sys_arg2 */
        mov      x23, x4  /* exitproc */
                          /* x24 reserved for dstack ptr */
                          /* x25 reserved for syscall ptr */
#ifdef MACOS
        mov      x26, x5  /* sys_arg3 */
        mov      x27, x6  /* sys_arg4 */
#endif

        /* inc exiting_thread_count to avoid being killed once off all_threads list */
        AARCH64_ADRP_GOT(GLOBAL_REF(exiting_thread_count), x0)
        CALLC2(GLOBAL_REF(atomic_add), x0, #1)

        /* save dcontext->dstack for freeing later and set dcontext->is_exiting */
        mov      w1, #1
        mov      x2, #(is_exiting_OFFSET)
        str      w1, [x19, x2] /* dcontext->is_exiting = 1 */
        CALLC1(GLOBAL_REF(is_currently_on_dstack), x19)
        cbnz     w0, cat_save_dstack
        mov      x24, #0
        b        cat_done_saving_dstack
cat_save_dstack:
        mov      x2, #(dstack_OFFSET)
        ldr      x24, [x19, x2]
cat_done_saving_dstack:
        CALLC0(GLOBAL_REF(get_cleanup_and_terminate_global_do_syscall_entry))
        mov      x25, x0
        cbz      w23, cat_thread_only
        CALLC0(GLOBAL_REF(dynamo_process_exit))
        b        cat_no_thread
cat_thread_only:
        CALLC0(GLOBAL_REF(dynamo_thread_exit))
cat_no_thread:
        /* switch to d_r_initstack for cleanup of dstack */
        AARCH64_ADRP_GOT(GLOBAL_REF(initstack_mutex), x26)
cat_spin:
        CALLC2(GLOBAL_REF(atomic_swap), x26, #1)
        cbz      w0, cat_have_lock
        yield
        b        cat_spin

cat_have_lock:
        /* switch stack */
        AARCH64_ADRP_GOT(GLOBAL_REF(d_r_initstack), x0)
        ldr      x0, [x0]
        mov      sp, x0

        /* free dstack and call the EXIT_DR_HOOK */
        CALLC1(GLOBAL_REF(dynamo_thread_stack_free_and_exit), x24) /* pass dstack */

        /* give up initstack_mutex */
        AARCH64_ADRP_GOT(GLOBAL_REF(initstack_mutex), x0)
        mov      x1, #0
        str      x1, [x0]

        /* dec exiting_thread_count (allows another thread to kill us) */
        AARCH64_ADRP_GOT(GLOBAL_REF(exiting_thread_count), x0)
        CALLC2(GLOBAL_REF(atomic_add), x0, #-1)

        /* put system call number in x8 */
        mov      x0, x21 /* sys_arg1 */
        mov      x1, x22 /* sys_arg2 */
#ifdef MACOS
        mov      x3, x26  /* sys_arg3 */
        mov      x4, x27  /* sys_arg4 */
#endif
        mov      SYSNUM_REG, w20 /* sys_call */

        br       x25  /* go do the syscall! */
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(cleanup_and_terminate)

#endif /* NOT_DYNAMORIO_CORE_PROPER */

        /* void atomic_add(int *adr, int val) */
        DECLARE_FUNC(atomic_add)
GLOBAL_LABEL(atomic_add:)
1:      ldxr     w2, [x0]
        add      w2, w2, w1
        stxr     w3, w2, [x0]
        cbnz     w3, 1b
        ret

        DECLARE_FUNC(global_do_syscall_int)
GLOBAL_LABEL(global_do_syscall_int:)
#ifdef MACOS
        svc      #0x80
#else
        /* FIXME i#1569: NYI on AArch64 */
        svc      #0
#endif
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(global_do_syscall_int)

DECLARE_GLOBAL(safe_read_asm_pre)
DECLARE_GLOBAL(safe_read_asm_mid)
DECLARE_GLOBAL(safe_read_asm_post)
DECLARE_GLOBAL(safe_read_asm_recover)

/* i#350: Xref comment in x86.asm about safe_read.
 *
 * FIXME i#1569: NYI: We need to save the PC's that can fault and have
 * is_safe_read_pc() identify them.
 *
 * FIXME i#1569: We should optimize this as it can be on the critical path.
 *
 * void *safe_read_asm(void *dst, const void *src, size_t n);
 */
        DECLARE_FUNC(safe_read_asm)
GLOBAL_LABEL(safe_read_asm:)
        cmp      ARG3, #0
1:      b.eq     safe_read_asm_recover_local
ADDRTAKEN_LABEL(safe_read_asm_pre:)
        ldrb     w3, [ARG2]
ADDRTAKEN_LABEL(safe_read_asm_mid:)
ADDRTAKEN_LABEL(safe_read_asm_post:)
        strb     w3, [ARG1]
        subs     ARG3, ARG3, #1
        add      ARG2, ARG2, #1
        add      ARG1, ARG1, #1
        b        1b
ADDRTAKEN_LABEL(safe_read_asm_recover:)
safe_read_asm_recover_local:
        mov      x0, ARG2
        ret
        END_FUNC(safe_read_asm)

/* Xref x86.asm dr_try_start about calling dr_setjmp without a call frame.
 *
 * int dr_try_start(try_except_context_t *cxt) ;
 */
        DECLARE_EXPORTED_FUNC(dr_try_start)
GLOBAL_LABEL(dr_try_start:)
        add      ARG1, ARG1, #TRY_CXT_SETJMP_OFFS
        b        GLOBAL_REF(dr_setjmp)
        END_FUNC(dr_try_start)

/* We save only the callee-saved registers: X19-X30, (gap), SP, D8-D15:
 * a total of 22 reg_t (64-bit) slots. See definition of dr_jmp_buf_t.
 * The gap is for better alignment of the D registers.
 *
 * int dr_setjmp(dr_jmp_buf_t *buf);
 */
        DECLARE_FUNC(dr_setjmp)
GLOBAL_LABEL(dr_setjmp:)
        stp      x19, x20, [ARG1]
        stp      x21, x22, [ARG1, #16]
        stp      x23, x24, [ARG1, #32]
        stp      x25, x26, [ARG1, #48]
        stp      x27, x28, [ARG1, #64]
        stp      x29, x30, [ARG1, #80]
        mov      x7, sp
        str      x7, [ARG1, #104]
        stp      d8, d9, [ARG1, #112]
        stp      d10, d11, [ARG1, #128]
        stp      d12, d13, [ARG1, #144]
        stp      d14, d15, [ARG1, #160]
# ifdef UNIX
        str      x30, [sp, #-16]! /* save link register */
        bl       GLOBAL_REF(dr_setjmp_sigmask)
        ldr      x30, [sp], #16
# endif
        mov      w0, #0
        ret
        END_FUNC(dr_setjmp)

/* int dr_longjmp(dr_jmp_buf_t *buf,  int val);
 */
        DECLARE_FUNC(dr_longjmp)
GLOBAL_LABEL(dr_longjmp:)
        ldp      x19, x20, [ARG1]
        ldp      x21, x22, [ARG1, #16]
        ldp      x23, x24, [ARG1, #32]
        ldp      x25, x26, [ARG1, #48]
        ldp      x27, x28, [ARG1, #64]
        ldp      x29, x30, [ARG1, #80]
        ldr      x7, [ARG1, #104]
        mov      sp, x7
        ldp      d8, d9, [ARG1, #112]
        ldp      d10, d11, [ARG1, #128]
        ldp      d12, d13, [ARG1, #144]
        ldp      d14, d15, [ARG1, #160]
        cmp      w1, #0
        csinc    w0, w1, wzr, ne
        br       x30
        END_FUNC(dr_longjmp)

        /* int atomic_swap(int *adr, int val) */
        DECLARE_FUNC(atomic_swap)
GLOBAL_LABEL(atomic_swap:)
1:      ldxr     w2, [x0]
        stxr     w3, w1, [x0]
        cbnz     w3, 1b
        mov      w0, w2
        ret
        END_FUNC(atomic_swap)

#ifdef UNIX
        DECLARE_FUNC(client_int_syscall)
GLOBAL_LABEL(client_int_syscall:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(client_int_syscall)

        DECLARE_FUNC(native_plt_call)
GLOBAL_LABEL(native_plt_call:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(native_plt_call)

        DECLARE_FUNC(_dynamorio_runtime_resolve)
GLOBAL_LABEL(_dynamorio_runtime_resolve:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(_dynamorio_runtime_resolve)
#endif /* UNIX */

#ifdef LINUX
/* thread_id_t dynamorio_clone(uint flags, byte *newsp, void *ptid, void *tls,
 *                             void *ctid, void (*func)(void))
 */
        DECLARE_FUNC(dynamorio_clone)
GLOBAL_LABEL(dynamorio_clone:)
        stp      ARG6, x0, [ARG2, #-16]! /* func is now on TOS of newsp */
        /* All args are already in syscall registers. */
        mov      SYSNUM_REG, #SYS_clone
        svc      #0
        cbnz     x0, dynamorio_clone_parent
        ldp      x0, x1, [sp], #16
        blr      x0
        bl       GLOBAL_REF(unexpected_return)
dynamorio_clone_parent:
        ret
        END_FUNC(dynamorio_clone)

        DECLARE_FUNC(dynamorio_sigreturn)
GLOBAL_LABEL(dynamorio_sigreturn:)
        mov      SYSNUM_REG, #SYS_rt_sigreturn
        svc      #0
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(dynamorio_sigreturn)

        DECLARE_FUNC(dynamorio_sys_exit)
GLOBAL_LABEL(dynamorio_sys_exit:)
        mov      w0, #0 /* exit code: hardcoded */
        mov      SYSNUM_REG, #SYS_exit
        svc      #0
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(dynamorio_sys_exit)

# ifndef NOT_DYNAMORIO_CORE_PROPER

#  ifndef HAVE_SIGALTSTACK
#   error NYI
#  endif
        DECLARE_FUNC(main_signal_handler)
GLOBAL_LABEL(main_signal_handler:)
        mov      ARG4, sp /* pass as extra arg */
        b        GLOBAL_REF(main_signal_handler_C) /* chain call */
        END_FUNC(main_signal_handler)

# endif /* NOT_DYNAMORIO_CORE_PROPER */

#endif /* LINUX */


#if defined(MACOS) && defined(AARCH64)
        DECLARE_FUNC(dynamorio_sigreturn)
GLOBAL_LABEL(dynamorio_sigreturn:)
        mov      w16, #184 /* SYS_sigreturn. */
        svc      #0x80
        END_FUNC(dynamorio_sigreturn)

        DECLARE_FUNC(dynamorio_sys_exit)
GLOBAL_LABEL(dynamorio_sys_exit:)
        mov      w16, #1 /* SYS_exit. */
        svc      #0x80
        END_FUNC(dynamorio_sys_exit)

        DECLARE_FUNC(new_bsdthread_intercept)
GLOBAL_LABEL(new_bsdthread_intercept:)
        /* TODO i#5383: Get correct syscall number for svc. */
        brk 0xb003 /* For now we break with a unique code. */
        END_FUNC(new_bsdthread_intercept)
#endif

#ifdef MACOS
        DECLARE_FUNC(main_signal_handler)
GLOBAL_LABEL(main_signal_handler:)
        /* See sendsig_set_thread_state64 in unix_signal.c */
        mov      ARG7, sp
        /* Save 3 args (ucxt=5th, style=2nd, token=6th) for sigreturn. */
        stp      ARG5, ARG6, [sp, #-32]!
        str      ARG2, [sp, #16]
        mov      ARG6, ARG7
        bl       GLOBAL_REF(main_signal_handler_C)
        ldr      ARG2, [sp, #16]
        ldp      ARG1, ARG3, [sp], #32
        CALLC0(GLOBAL_REF(dynamorio_sigreturn))
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(main_signal_handler)
#endif

        DECLARE_FUNC(hashlookup_null_handler)
GLOBAL_LABEL(hashlookup_null_handler:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(hashlookup_null_handler)

        DECLARE_FUNC(back_from_native_retstubs)
GLOBAL_LABEL(back_from_native_retstubs:)
DECLARE_GLOBAL(back_from_native_retstubs_end)
ADDRTAKEN_LABEL(back_from_native_retstubs_end:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(back_from_native_retstubs)

        DECLARE_FUNC(back_from_native)
GLOBAL_LABEL(back_from_native:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(back_from_native)

/* A static resolver for TLS descriptors, implemented in assembler as
 * it does not use the standard calling convention. In C, it could be:
 *
 * ptrdiff_t
 * tlsdesc_resolver(struct tlsdesc_t *tlsdesc)
 * {
 *     return (ptrdiff_t)tlsdesc->arg;
 * }
 */
        DECLARE_FUNC(tlsdesc_resolver)
GLOBAL_LABEL(tlsdesc_resolver:)
        ldr      x0, [x0, #tlsdesc_arg_OFFSET]
        ret

/* This function is called from the fragment cache when the original code had
 * IC IVAU, Xt. Typically it just records which cache lines have been invalidated
 * and sets icache_op_struct.flag. However, if non-contiguous cache lines have
 * been invalidated we branch to fcache_return instead of returning.
 * When we enter here:
 *
 * X0 contains the pointer to spill_state_t.
 * X30 contains the return address in the fragment cache.
 * TLS_REG0_SLOT contains app's X0.
 * TLS_REG1_SLOT contains app's X30.
 * TLS_REG2_SLOT contains the argument of "IC IVAU, Xt".
 * TLS_REG3_SLOT contains the original address of the instruction after the IC.
 *
 * If we return, the first two slots and all registers except X0 and X30 must
 * be preserved.
 *
 * XXX: We do not correctly handle the case where the set of contiguous cache
 * lines covers the entire address space, so begin == end again, but that would
 * require more than 1e14 calls to this function even with the largest possible
 * icache line.
 */
        DECLARE_FUNC(icache_op_ic_ivau_asm)
GLOBAL_LABEL(icache_op_ic_ivau_asm:)
        /* Spill X1 and X2 to TLS_REG4_SLOT and TLS_REG5_SLOT. */
        stp      x1, x2, [x0, #spill_state_r4_OFFSET]
        /* Point X1 at icache_op_struct.lock. */
        AARCH64_ADRP_GOT((GLOBAL_REF(icache_op_struct) + icache_op_struct_lock_OFFSET), x1)
        /* Acquire lock. */
        prfm     pstl1keep, [x1]
1:
        ldaxr    w2, [x1]
        cbnz     w2, 1b
        stxr     w2, w1, [x1] /* w1 is non-zero! */
        cbnz     w2, 1b
        /* Point X1 at iccache_op_struct. */
        sub      x1, x1, #icache_op_struct_lock_OFFSET
        /* Spill X3 and X4 to icache_op_struct.spill. */
        stp      x3, x4, [x1, #icache_op_struct_spill_OFFSET]
        /* Load size of icache line to X2. */
        ldr      x2, [x1, #icache_op_struct_linesize_OFFSET]
        cbz      x2, set_linesize
linesize_set:
        /* Align argument to cache line. */
        ldr      x3, [x0, #spill_state_r2_OFFSET]
        sub      x4, x2, #1
        bic      x3, x3, x4
        str      x3, [x0, #spill_state_r2_OFFSET]
        /* Is (begin == end)? */
        ldp      x3, x4, [x1, #icache_op_struct_begin_OFFSET]
        eor      x3, x3, x4
        cbnz     x3, 2f
        /* Yes, so set begin, end, flag, and return. */
        ldr      x3, [x0, #spill_state_r2_OFFSET]
        add      x4, x3, x2
        stp      x3, x4, [x1, #icache_op_struct_begin_OFFSET]
        mov      w3, #1
        str      w3, [x1, #icache_op_struct_flag_OFFSET]
        b        ic_ivau_return
2:
        /* Is (argument == end)? */
        ldr      x3, [x0, #spill_state_r2_OFFSET]
        ldr      x4, [x1, #(icache_op_struct_begin_OFFSET + 8)]
        eor      x4, x3, x4
        cbnz     x4, 3f
        /* Yes, so increment end by linesize, and return. */
        add      x3, x3, x2
        str      x3, [x1, #(icache_op_struct_begin_OFFSET + 8)]
        b        ic_ivau_return
3:
        /* Is (argument == begin - linesize)? */
        ldr      x4, [x1, #icache_op_struct_begin_OFFSET]
        sub      x4, x4, x2
        eor      x4, x3, x4
        cbnz     x4, 4f
        /* Yes, so decrement begin by linesize, and return. */
        str      x3, [x1, #icache_op_struct_begin_OFFSET]
        b        ic_ivau_return
4:
        /* Is argument in the range from begin to end? */
        ldp      x2, x4, [x1, #icache_op_struct_begin_OFFSET]
        sub      x3, x3, x2 /* (argument - begin) */
        sub      x4, x4, x2 /* (end - begin) */
        lsr      x3, x3, #1
        sub      x3, x3, x4, lsr #1 /* ((argument - begin) / 2 - (end - begin) / 2) */
        tbz      x3, #63, 5f
        /* Yes, so just return. */
ic_ivau_return:
        /* Restore X3 and X4 from icache_op_struct.spill. */
        ldp      x3, x4, [x1, #icache_op_struct_spill_OFFSET]
        /* Point X1 at icache_op_struct_lock. */
        add      x1, x1, #icache_op_struct_lock_OFFSET
        /* Release lock. */
        stlr     wzr, [x1]
        /* Restore X1 and X2 from TLS_REG4_SLOT and TLS_REG5_SLOT. */
        ldp      x1, x2, [x0, #spill_state_r4_OFFSET]
        /* Return to fragment cache. */
        ret
5:
        /* The new cache line is not contiguous with the previous set. */
        /* Restore X30 from TLS_REG1_SLOT. */
        ldr      x30, [x0, #spill_state_r1_OFFSET]
        /* Move PC and X1 from slots 3 and 4 to slots 4 and 1. */
        ldp      x3, x4, [x0, #spill_state_r3_OFFSET]
        str      x3, [x0, #spill_state_r4_OFFSET]
        str      x4, [x0, #spill_state_r1_OFFSET]
        /* Load argument from TLS_REG2_SLOT to X2. */
        ldr      x2, [x0, #spill_state_r2_OFFSET]
        /* Save (begin, end) to TLS_REG_SLOT2 and TLS_REG_SLOT3. */
        ldp      x3, x4, [x1, #icache_op_struct_begin_OFFSET]
        stp      x3, x4, [x0, #spill_state_r2_OFFSET]
        /* Set icache_op_struct. */
        ldr      x4, [x1, #icache_op_struct_linesize_OFFSET]
        add      x3, x2, x4
        stp      x2, x3, [x1, #icache_op_struct_begin_OFFSET]
        /* Restore X2 from TLS_REG5_SLOT. */
        ldr      x2, [x0, #spill_state_r5_OFFSET]
        /* Restore X3 and X4 from icache_op_struct.spill. */
        ldp      x3, x4, [x1, #icache_op_struct_spill_OFFSET]
        /* Release lock. */
        add      x1, x1, #icache_op_struct_lock_OFFSET
        stlr     wzr, [x1]
        /* Load fcache_return into X1. */
        ldr      x1, [x0, #spill_state_fcache_return_OFFSET]
        /* Point X0 at fake linkstub. */
        AARCH64_ADRP_GOT(GLOBAL_REF(linkstub_selfmod), x0)
        /* Branch to fcache_return. */
        br       x1

set_linesize:
        mrs      x3, ctr_el0
        and      w3, w3, #15
        mov      x2, #4
        lsl      x2, x2, x3
        str      x2, [x1, #icache_op_struct_linesize_OFFSET]
        b        linesize_set

        END_FUNC(icache_op_ic_ivau_asm)

/* This code is branched to from the fragment cache when the original code had
 * ISB and icache_op_struct.flag was found to be set. We must reset icache_op_struct,
 * then branch to fcache_return, where we will call flush_fragments_from_region.
 * When we enter here:
 *
 * X0 contains the pointer to spill_state_t.
 * X1 contains the original address of the instruction after the ISB.
 * X2 is corrupted.
 * TLS_REG0_SLOT contains app's X0.
 * TLS_REG1_SLOT contains app's X1.
 * TLS_REG2_SLOT contains app's X2.
 */
        DECLARE_FUNC(icache_op_isb_asm)
GLOBAL_LABEL(icache_op_isb_asm:)
        /* Save PC to TLS_REG4_SLOT, and move X2 to TLS_REG5_SLOT. */
        ldr      x2, [x0, #spill_state_r2_OFFSET]
        stp      x1, x2, [x0, #spill_state_r4_OFFSET]
        /* Point X1 at icache_op_struct.lock. */
        AARCH64_ADRP_GOT((GLOBAL_REF(icache_op_struct) + icache_op_struct_lock_OFFSET), x1)
        /* Acquire lock. */
        prfm     pstl1keep, [x1]
1:
        ldaxr    w2, [x1]
        cbnz     w2, 1b
        stxr     w2, w1, [x1] /* w2 is non-zero! */
        cbnz     w2, 1b
        /* Point X1 at icache_op_struct. */
        sub      x1, x1, #icache_op_struct_lock_OFFSET
        /* Save (begin, end) to TLS_REG_SLOT2 and TLS_REG_SLOT3. */
        ldr      x2, [x1, #icache_op_struct_begin_OFFSET]
        str      x2, [x0, #spill_state_r2_OFFSET]
        ldr      x2, [x1, #icache_op_struct_end_OFFSET]
        str      x2, [x0, #spill_state_r3_OFFSET]
        /* Reset icache_op_struct. */
        str      wzr, [x1, #icache_op_struct_flag_OFFSET]
        stp      xzr, xzr, [x1, #icache_op_struct_begin_OFFSET]
        /* Point X1 at icache_op_struct.lock. */
        add      x1, x1, #4
        /* Release lock. */
        stlr     wzr, [x1]
        /* Restore X2 from TLS_REG5_SLOT. */
        ldr      x2, [x0, #spill_state_r5_OFFSET]
        /* Load fcache_return into X1. */
        ldr      x1, [x0, #spill_state_fcache_return_OFFSET]
        /* Point X0 at fake linkstub. */
        AARCH64_ADRP_GOT(GLOBAL_REF(linkstub_selfmod), x0)
        /* Branch to fcache_return. */
        br       x1
        END_FUNC(icache_op_isb_asm)

END_FILE
