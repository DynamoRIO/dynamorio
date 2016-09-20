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
 * AArch64-specific assembly and trampoline code
 */

#include "../asm_defines.asm"
START_FILE
#include "include/syscall.h"

#ifndef UNIX
# error Non-Unix is not supported
#endif

/* sizeof(priv_mcontext_t) rounded up to a multiple of 16 */
#define PRIV_MCONTEXT_SIZE 800

/* offsetof(priv_mcontext_t, simd) */
#define simd_OFFSET (16 * ARG_SZ*2 + 32)
/* offsetof(dcontext_t, dstack) */
#define dstack_OFFSET     0x368
/* offsetof(dcontext_t, is_exiting) */
#define is_exiting_OFFSET (dstack_OFFSET+1*ARG_SZ)
/* offsetof(struct tlsdesc_t, arg */
#define tlsdesc_arg_OFFSET 8

#ifndef X64
# error X64 must be defined
#endif

#if defined(UNIX)
DECL_EXTERN(dr_setjmp_sigmask)
#endif

/* All CPU ID registers are accessible only in privileged modes. */
        DECLARE_FUNC(cpuid_supported)
GLOBAL_LABEL(cpuid_supported:)
        mov      w0, #0
        ret
        END_FUNC(cpuid_supported)

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

#ifdef CLIENT_INTERFACE
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
#endif /* CLIENT_INTERFACE */

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
 * cleanup_and_terminate(dcontext_t *dcontext,     // X0 -> X19
 *                       int sysnum,               // W1 -> W20 = syscall #
 *                       int sys_arg1/param_base,  // W2 -> W21 = arg1 for syscall
 *                       int sys_arg2,             // W3 -> W22 = arg2 for syscall
 *                       bool exitproc,            // W4 -> W23
 *                       (2 more args that are ignored: Mac-only))
 *
 * See decl in arch_exports.h for description.
 */
        DECLARE_FUNC(cleanup_and_terminate)
GLOBAL_LABEL(cleanup_and_terminate:)
        /* move argument registers to callee saved registers */
        mov      x19, x0  /* dcontext ptr size */
        mov      w20, w1  /* sysnum 32-bit int */
        mov      w21, w2  /* sys_arg1 32-bit int */
        mov      w22, w3  /* sys_arg2 32-bit int */
        mov      w23, w4  /* exitproc 32-bit int */
                          /* x24 reserved for dstack ptr */
                          /* x25 reserved for syscall ptr */

        /* inc exiting_thread_count to avoid being killed once off all_threads list */
        adrp     x0, :got:exiting_thread_count
        ldr      x0, [x0, #:got_lo12:exiting_thread_count]
        CALLC2(atomic_add, x0, #1)

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
        /* switch to initstack for cleanup of dstack */
        adrp     x26, :got:initstack_mutex
        ldr      x26, [x26, #:got_lo12:initstack_mutex]
cat_spin:
        CALLC2(atomic_swap, x26, #1)
        cbz      w0, cat_have_lock
        yield
        b        cat_spin

cat_have_lock:
        /* switch stack */
        adrp     x0, :got:initstack
        ldr      x0, [x0, #:got_lo12:initstack]
        ldr      x0, [x0]
        mov      sp, x0

        /* free dstack and call the EXIT_DR_HOOK */
        CALLC1(GLOBAL_REF(dynamo_thread_stack_free_and_exit), x24) /* pass dstack */

        /* give up initstack mutex */
        adrp     x0, :got:initstack_mutex
        ldr      x0, [x0, #:got_lo12:initstack_mutex]
        mov      x1, #0
        str      x1, [x0]

        /* dec exiting_thread_count (allows another thread to kill us) */
        adrp     x0, :got:exiting_thread_count
        ldr      x0, [x0, #:got_lo12:exiting_thread_count]
        CALLC2(atomic_add, x0, #-1)

        /* put system call number in x8 */
        mov      w0, w21 /* sys_arg1 32-bit int */
        mov      w1, w22 /* sys_arg2 32-bit int */
        mov      w8, w20 /* int sys_call */

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
        /* FIXME i#1569: NYI on AArch64 */
        svc      #0
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
1:      b.eq     safe_read_asm_recover
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
        mov      x0, ARG2
        ret
        END_FUNC(safe_read_asm)

#ifdef UNIX
/* i#46: Private memcpy and memset for libc isolation.  Xref comment in x86.asm.
 */

/* Private memcpy.
 * FIXME i#1569: We should optimize this as it can be on the critical path.
 */
        DECLARE_FUNC(memcpy)
GLOBAL_LABEL(memcpy:)
        mov      x3, ARG1
        cbz      ARG3, 2f
1:      ldrb     w4, [ARG2], #1
        strb     w4, [x3], #1
        sub      ARG3, ARG3, #1
        cbnz     ARG3, 1b
2:      ret
        END_FUNC(memcpy)

/* Private memset.
 * FIXME i#1569: we should optimize this as it can be on the critical path.
 */
        DECLARE_FUNC(memset)
GLOBAL_LABEL(memset:)
        mov      x3, ARG1
        cbz      ARG3, 2f
1:      strb     w1, [x3], #1
        sub      ARG3, ARG3, #1
        cbnz     ARG3, 1b
2:      ret
        END_FUNC(memset)

/* See x86.asm notes about needing these to avoid gcc invoking *_chk */
.global __memcpy_chk
.hidden __memcpy_chk
.set __memcpy_chk,memcpy

.global __memset_chk
.hidden __memset_chk
.set __memset_chk,memset
#endif /* UNIX */

#ifdef CLIENT_INTERFACE

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
        cmp      w0, #0
        csinc    w0, w0, wzr, ne
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

#endif /* CLIENT_INTERFACE */

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

        DECLARE_FUNC(dynamorio_clone)
GLOBAL_LABEL(dynamorio_clone:)
        bl       GLOBAL_REF(unexpected_return) /* FIXME i#1569: NYI */
        END_FUNC(dynamorio_clone)

        DECLARE_FUNC(dynamorio_sigreturn)
GLOBAL_LABEL(dynamorio_sigreturn:)
        mov      w8, #SYS_rt_sigreturn
        svc      #0
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(dynamorio_sigreturn)

        DECLARE_FUNC(dynamorio_sys_exit)
GLOBAL_LABEL(dynamorio_sys_exit:)
        mov      w0, #0 /* exit code: hardcoded */
        mov      w8, #SYS_exit
        svc      #0
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(dynamorio_sys_exit)

# ifndef NOT_DYNAMORIO_CORE_PROPER

#  ifndef HAVE_SIGALTSTACK
#   error NYI
#  endif
        DECLARE_FUNC(master_signal_handler)
GLOBAL_LABEL(master_signal_handler:)
        mov      ARG4, sp /* pass as extra arg */
        b        GLOBAL_REF(master_signal_handler_C) /* chain call */
        END_FUNC(master_signal_handler)

# endif /* NOT_DYNAMORIO_CORE_PROPER */

#endif /* LINUX */

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

/* CTR_EL0 [19:16] : Log2 of number of words in smallest dcache line
 * CTR_EL0 [3:0]   : Log2 of number of words in smallest icache line
 *
 * PoC = Point of Coherency
 * PoU = Point of Unification
 *
 * void cache_sync_asm(void *beg, void *end);
 */
        DECLARE_FUNC(cache_sync_asm)
GLOBAL_LABEL(cache_sync_asm:)
        mrs      x3, ctr_el0
        mov      w4, #4
        ubfx     w2, w3, #16, #4
        lsl      w2, w4, w2 /* bytes in dcache line */
        and      w3, w3, #15
        lsl      w3, w4, w3 /* bytes in icache line */
        sub      w4, w2, #1
        bic      x4, x0, x4 /* aligned beg */
        b        2f
1:      dc       cvau, x4 /* Data Cache Clean by VA to PoU */
        add      x4, x4, x2
2:      cmp      x4, x1
        b.cc     1b
        dsb      ish /* Data Synchronization Barrier, Inner Shareable */
        sub      w4, w3, #1
        bic      x4, x0, x4 /* aligned beg */
        b        4f
3:      ic       ivau, x4 /* Instruction Cache Invalidate by VA to PoU */
        add      x4, x4, x3
4:      cmp      x4, x1
        b.cc     3b
        dsb      ish /* Data Synchronization Barrier, Inner Shareable */
        isb      /* Instruction Synchronization Barrier */
        ret
        END_FUNC(cache_sync_asm)

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
        ldr     x0,[x0,#tlsdesc_arg_OFFSET]
        ret

END_FILE
