/* **********************************************************
 * Copyright (c) 2014-2022 Google, Inc.  All rights reserved.
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
#include "include/syscall.h"

DECL_EXTERN(dynamorio_app_take_over_helper)
#if defined(UNIX)
DECL_EXTERN(main_signal_handler_C)
DECL_EXTERN(dr_setjmp_sigmask)
#endif
DECL_EXTERN(relocate_dynamorio)
DECL_EXTERN(privload_early_inject)

DECL_EXTERN(exiting_thread_count)
DECL_EXTERN(d_r_initstack)
DECL_EXTERN(initstack_mutex)

#define RESTORE_FROM_DCONTEXT_VIA_REG(reg,offs,dest) ldr dest, PTRSZ [reg, POUND (offs)]
#define SAVE_TO_DCONTEXT_VIA_REG(reg,offs,src) str src, PTRSZ [reg, POUND (offs)]

/* offsetof(dcontext_t, dstack) */
#define dstack_OFFSET     0x16c
/* offsetof(dcontext_t, is_exiting) */
#define is_exiting_OFFSET (dstack_OFFSET+1*ARG_SZ)

#ifdef X64
# define MCXT_NUM_SIMD_SLOTS 32
# define SIMD_REG_SIZE       16
# define NUM_GPR_SLOTS       33 /* incl flags */
# define GPR_REG_SIZE         8
#else
# define MCXT_NUM_SIMD_SLOTS 16
# define SIMD_REG_SIZE       16
# define NUM_GPR_SLOTS       17 /* incl flags */
# define GPR_REG_SIZE         4
#endif
#define PRE_SIMD_PADDING     0
#define PRIV_MCXT_SIMD_SIZE (PRE_SIMD_PADDING + MCXT_NUM_SIMD_SLOTS*SIMD_REG_SIZE)
#define PRIV_MCXT_SIZE (NUM_GPR_SLOTS*GPR_REG_SIZE + PRIV_MCXT_SIMD_SIZE)
#define PRIV_MCXT_SP_FROM_SIMD (-(4*GPR_REG_SIZE)) /* flags, pc, lr, then sp */
#define PRIV_MCXT_PC_FROM_SIMD (-(2*GPR_REG_SIZE)) /* flags, then pc */

#ifndef UNIX
# error Non-Unix is not supported
#endif

/* all of the CPUID registers are only accessible in privileged modes */
        DECLARE_FUNC(cpuid_supported)
GLOBAL_LABEL(cpuid_supported:)
        mov      r0, #0
        bx       lr
        END_FUNC(cpuid_supported)

/* void call_switch_stack(void *func_arg,             // REG_R0
 *                        byte *stack,                // REG_R1
 *                        void (*func)(void *arg),    // REG_R2
 *                        void *mutex_to_free,        // REG_R3
 *                        bool return_on_return)      // [REG_SP]
 */
        DECLARE_FUNC(call_switch_stack)
GLOBAL_LABEL(call_switch_stack:)
        /* we need a callee-saved reg across our call so save it onto stack */
        push     {REG_R4, lr}
        /* check mutex_to_free */
        cmp      ARG4, #0
        beq      call_dispatch_alt_stack_no_free
        /* release the mutex */
        mov      REG_R4, #0
        str      REG_R4, [ARG4]
call_dispatch_alt_stack_no_free:
        /* switch stack */
        mov      REG_R4, sp
        mov      sp, ARG2
        /* call func */
        blx      ARG3
        /* switch stack back */
        mov      sp, REG_R4
        /* load ARG5 (return_on_return) from stack after the push at beginning */
        /* after call, so we can use REG_R3 as the scratch register */
        ldr      REG_R3, [sp, #8/* r4, lr */] /* ARG5 */
        cmp      REG_R3, #0
        beq      GLOBAL_REF(unexpected_return)
        /* restore and return */
        pop      {REG_R4, pc}
        END_FUNC(call_switch_stack)


/*
 * Calls the specified function 'func' after switching to the DR stack
 * for the thread corresponding to 'drcontext'.
 * Passes in 8 arguments.  Uses the C calling convention, so 'func' will work
 * just fine even if if takes fewer than 8 args.
 * Swaps the stack back upon return and returns the value returned by 'func'.
 *
 * void * dr_call_on_clean_stack(void *drcontext,            //
 *                               void *(*func)(arg1...arg8), // 0*ARG_SZ+r4
 *                               void *arg1,                 // 1*ARG_SZ+r4
 *                               void *arg2,                 // 2*ARG_SZ+r4
 *                               void *arg3,                 // 6*ARG_SZ+r4
 *                               void *arg4,                 // 7*ARG_SZ+r4
 *                               void *arg5,                 // 8*ARG_SZ+r4
 *                               void *arg6,                 // 9*ARG_SZ+r4
 *                               void *arg7,                 //10*ARG_SZ+r4
 *                               void *arg8)                 //11*ARG_SZ+r4
 */
        DECLARE_EXPORTED_FUNC(dr_call_on_clean_stack)
GLOBAL_LABEL(dr_call_on_clean_stack:)
        /* We need a callee-saved reg across the call: we use r4. */
        push     {REG_R1-REG_R5, lr}
        mov      REG_R4, REG_SP /* save sp across the call */
        mov      REG_R5, ARG2 /* save function in non-param reg */
        /* Swap stacks */
        RESTORE_FROM_DCONTEXT_VIA_REG(REG_R0, dstack_OFFSET, REG_SP)
        /* Set up args */
        sub      REG_SP, #(4*ARG_SZ)
        ldr      REG_R0, [REG_R4, #(11*ARG_SZ)]
        str      REG_R0, ARG8
        ldr      REG_R0, [REG_R4, #(10*ARG_SZ)]
        str      REG_R0, ARG7
        ldr      REG_R0, [REG_R4, #(9*ARG_SZ)]
        str      REG_R0, ARG6
        ldr      REG_R0, [REG_R4, #(8*ARG_SZ)]
        str      REG_R0, ARG5
        ldr      ARG4, [REG_R4, #(7*ARG_SZ)]
        ldr      ARG3, [REG_R4, #(6*ARG_SZ)]
        ldr      ARG2, [REG_R4, #(2*ARG_SZ)]
        ldr      ARG1, [REG_R4, #(1*ARG_SZ)]
        blx      REG_R5
        mov      REG_SP, REG_R4
        pop      {REG_R1-REG_R5, pc} /* don't need r1-r3 values but this is simplest */
        END_FUNC(dr_call_on_clean_stack)


#ifndef NOT_DYNAMORIO_CORE_PROPER

/*
 * dr_app_start - Causes application to run under Dynamo control
 */
#ifdef DR_APP_EXPORTS
        DECLARE_EXPORTED_FUNC(dr_app_start)
GLOBAL_LABEL(dr_app_start:)
        push     {lr}
        vstmdb   sp!, {d16-d31}
        vstmdb   sp!, {d0-d15}
        mrs      REG_R0, cpsr /* r0 is scratch */
        push     {REG_R0}
        /* We can't push all regs w/ writeback */
        stmdb    sp, {REG_R0-r15}
        str      lr, [sp, #(PRIV_MCXT_PC_FROM_SIMD+4)] /* +4 b/c we pushed cpsr */
        /* we need the sp at function entry */
        mov      REG_R0, sp
        add      REG_R0, REG_R0, #(PRIV_MCXT_SIMD_SIZE + 8) /* offset simd,cpsr,lr */
        str      REG_R0, [sp, #(PRIV_MCXT_SP_FROM_SIMD+4)] /* +4 b/c we pushed cpsr */
        sub      sp, sp, #(PRIV_MCXT_SIZE-PRIV_MCXT_SIMD_SIZE-4) /* simd,cpsr */
        mov      REG_R0, sp
        CALLC1(GLOBAL_REF(dr_app_start_helper), REG_R0)
        /* if we get here, DR is not taking over */
        add      sp, sp, #PRIV_MCXT_SIZE
        pop      {pc}
        END_FUNC(dr_app_start)

/*
 * dr_app_take_over - For the client interface, we'll export 'dr_app_take_over'
 * for consistency with the dr_ naming convention of all exported functions.
 * We'll keep 'dynamorio_app_take_over' for compatibility with the preinjector.
 */
        DECLARE_EXPORTED_FUNC(dr_app_take_over)
GLOBAL_LABEL(dr_app_take_over:)
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
        mov      r0, #0
        bx       lr
        END_FUNC(dr_app_running_under_dynamorio)
#endif /* DR_APP_EXPORTS */

/*
 * dynamorio_app_take_over - Causes application to run under DR
 * control.  DR never releases control.
 */
        DECLARE_EXPORTED_FUNC(dynamorio_app_take_over)
GLOBAL_LABEL(dynamorio_app_take_over:)
        push     {lr}
        vstmdb   sp!, {d16-d31}
        vstmdb   sp!, {d0-d15}
        mrs      REG_R0, cpsr /* r0 is scratch */
        push     {REG_R0}
        /* We can't push all regs w/ writeback */
        stmdb    sp, {REG_R0-r15}
        str      lr, [sp, #(PRIV_MCXT_PC_FROM_SIMD+4)] /* +4 b/c we pushed cpsr */
        /* we need the sp at function entry */
        mov      REG_R0, sp
        add      REG_R0, REG_R0, #(PRIV_MCXT_SIMD_SIZE + 8) /* offset simd,cpsr,lr */
        str      REG_R0, [sp, #(PRIV_MCXT_SP_FROM_SIMD+4)] /* +4 b/c we pushed cpsr */
        sub      sp, sp, #(PRIV_MCXT_SIZE-PRIV_MCXT_SIMD_SIZE-4) /* simd,cpsr */
        mov      REG_R0, sp
        CALLC1(GLOBAL_REF(dynamorio_app_take_over_helper), REG_R0)
        /* if we get here, DR is not taking over */
        add      sp, sp, #PRIV_MCXT_SIZE
        pop      {pc}
        END_FUNC(dynamorio_app_take_over)


/*
 * cleanup_and_terminate(dcontext_t *dcontext,     // 0*ARG_SZ+sp
 *                   int sysnum,               // 1*ARG_SZ+sp = syscall #
 *                   ptr_uint_t sys_arg1/param_base,  // 2*ARG_SZ+sp = arg1 for syscall
 *                   ptr_uint_t sys_arg2,             // 3*ARG_SZ+sp = arg2 for syscall
 *                   bool exitproc,            // 4*ARG_SZ+sp
 *                   (2 more args that are ignored: Mac-only))
 *
 * See decl in arch_exports.h for description.
 */
        DECLARE_FUNC(cleanup_and_terminate)
GLOBAL_LABEL(cleanup_and_terminate:)
        push     {REG_R0-REG_R3}
        /* inc exiting_thread_count to avoid being killed once off all_threads list */
        /* We use the following multi-instr and multi-stored-data sequence to maintain
         * PIC code.  Note that using "ldr r0, SYMREF(exiting_thread_count)" does
         * NOT result in PIC code: it uses a stored absolute address.
         */
        ldr      REG_R2, .Lgot0
        add      REG_R2, REG_R2, pc
        ldr      REG_R0, .Lexiting_thread_count
.LPIC0: ldr      REG_R0, [REG_R0, REG_R2]
        mov      REG_R2, #1
        CALLC2(atomic_add, REG_R0, REG_R2)
        /* save dcontext->dstack for freeing later and set dcontext->is_exiting */
        ldr      REG_R4, PTRSZ [sp, #(0*ARG_SZ)] /* dcontext */
        mov      REG_R1, #1
        SAVE_TO_DCONTEXT_VIA_REG(REG_R4, is_exiting_OFFSET, REG_R1)
        CALLC1(GLOBAL_REF(is_currently_on_dstack), REG_R4) /* r4 is callee-saved */
        cmp      REG_R0, #0
        bne      cat_save_dstack
        mov      REG_R4, #0 /* save 0 for dstack to avoid double-free */
        b        cat_done_saving_dstack
cat_save_dstack:
        RESTORE_FROM_DCONTEXT_VIA_REG(REG_R4, dstack_OFFSET, REG_R4)
cat_done_saving_dstack:
        CALLC0(GLOBAL_REF(get_cleanup_and_terminate_global_do_syscall_entry))
        mov      REG_R5, REG_R0
        ldrb     REG_R0, BYTE [sp, #(4*ARG_SZ)] /* exitproc */
        cmp      REG_R0, #0
        beq      cat_thread_only
        CALLC0(GLOBAL_REF(dynamo_process_exit))
        b        cat_no_thread
cat_thread_only:
        CALLC0(GLOBAL_REF(dynamo_thread_exit))
cat_no_thread:
        /* switch to d_r_initstack for cleanup of dstack */
        /* we use r6, r7, and r8 here so that atomic_swap doesn't clobber them */
        mov      REG_R6, #1
        ldr      REG_R8, .Lgot1
        add      REG_R8, REG_R8, pc
        ldr      REG_R7, .Linitstack_mutex
.LPIC1: ldr      REG_R7, [REG_R7, REG_R8]
cat_spin:
        CALLC2(atomic_swap, REG_R7, REG_R6)
        cmp      REG_R0, #0
        beq      cat_have_lock
        yield
        b        cat_spin
cat_have_lock:
        /* need to grab everything off dstack first */
        ldr      REG_R7, [sp, #(1*ARG_SZ)]  /* sysnum */
        ldr      REG_R6, [sp, #(2*ARG_SZ)]  /* sys_arg1 */
        ldr      REG_R8, [sp, #(3*ARG_SZ)]  /* sys_arg2 */
        /* swap stacks */
        ldr      REG_R2, .Lgot2
        add      REG_R2, REG_R2, pc
        ldr      REG_R3, .Ld_r_initstack
.LPIC2: ldr      REG_R3, [REG_R3, REG_R2]
        ldr      sp, [REG_R3]
        /* free dstack and call the EXIT_DR_HOOK */
        CALLC1(GLOBAL_REF(dynamo_thread_stack_free_and_exit), REG_R4) /* pass dstack */
        /* give up initstack_mutex */
        ldr      REG_R2, .Lgot3
        add      REG_R2, REG_R2, pc
        ldr      REG_R3, .Linitstack_mutex
.LPIC3: ldr      REG_R3, [REG_R3, REG_R2]
        mov      REG_R2, #0
        str      REG_R2, [REG_R3]
        /* dec exiting_thread_count (allows another thread to kill us) */
        ldr      REG_R2, .Lgot4
        add      REG_R2, REG_R2, pc
        ldr      REG_R3, .Lexiting_thread_count
.LPIC4: ldr      REG_R3, [REG_R3, REG_R2]
        mov      REG_R2, #-1
        CALLC2(atomic_add, REG_R3, REG_R2)
        /* finally, execute the termination syscall */
        mov      REG_R0, REG_R6  /* sys_arg1 */
        mov      REG_R1, REG_R8  /* sys_arg2 */
        /* sysnum is in r7 already */
        bx       REG_R5  /* go do the syscall! */
        END_FUNC(cleanup_and_terminate)
/* Data for PIC code above */
.Lgot0:
        .long   _GLOBAL_OFFSET_TABLE_-.LPIC0
.Lgot1:
        .long   _GLOBAL_OFFSET_TABLE_-.LPIC1
.Lgot2:
        .long   _GLOBAL_OFFSET_TABLE_-.LPIC2
.Lgot3:
        .long   _GLOBAL_OFFSET_TABLE_-.LPIC3
.Lgot4:
        .long   _GLOBAL_OFFSET_TABLE_-.LPIC4
.Lexiting_thread_count:
        .word   exiting_thread_count(GOT)
.Ld_r_initstack:
        .word   d_r_initstack(GOT)
.Linitstack_mutex:
        .word   initstack_mutex(GOT)
#endif /* NOT_DYNAMORIO_CORE_PROPER */

/* Pass in the pointer-sized address to modify in ARG1 and the val to add in ARG2. */
        DECLARE_FUNC(atomic_add)
GLOBAL_LABEL(atomic_add:)
1:      ldrex    REG_R2, [ARG1]
        add      REG_R2, REG_R2, ARG2
        strex    REG_R3, REG_R2, [ARG1]
        cmp      REG_R3, #0
        bne      1b
        bx       lr
        END_FUNC(atomic_add)

        DECLARE_FUNC(global_do_syscall_int)
GLOBAL_LABEL(global_do_syscall_int:)
        /* FIXME i#1551: NYI on ARM */
        svc      #0
        END_FUNC(global_do_syscall_int)


DECLARE_GLOBAL(safe_read_asm_pre)
DECLARE_GLOBAL(safe_read_asm_mid)
DECLARE_GLOBAL(safe_read_asm_post)
DECLARE_GLOBAL(safe_read_asm_recover)

/* i#350: We implement safe_read in assembly and save the PCs that can fault.
 * If these PCs fault, we return from the signal handler to the epilog, which
 * can recover.  We return the source pointer from ARG2, and the caller uses this
 * to determine how many bytes were copied and whether it matches size.
 *
 * FIXME i#1551: NYI: we need to save the PC's that can fault and have
 * is_safe_read_pc() identify them.
 *
 * FIXME i#1551: we should optimize this as it can be on the critical path.
 *
 * void *
 * safe_read_asm(void *dst, const void *src, size_t n);
 */
        DECLARE_FUNC(safe_read_asm)
GLOBAL_LABEL(safe_read_asm:)
        cmp      ARG3,   #0
1:      beq      safe_read_asm_recover
ADDRTAKEN_LABEL(safe_read_asm_pre:)
        ldrb     REG_R3, [ARG2]
ADDRTAKEN_LABEL(safe_read_asm_mid:)
ADDRTAKEN_LABEL(safe_read_asm_post:)
        strb     REG_R3, [ARG1]
        subs     ARG3, ARG3, #1
        add      ARG2, ARG2, #1
        add      ARG1, ARG1, #1
        b        1b
ADDRTAKEN_LABEL(safe_read_asm_recover:)
        mov      REG_R0, ARG2
        bx       lr
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

/* We save only the callee-saved registers: R4-R11, SP, LR, D8-D15:
 * a total of 26 reg_t (32-bit) slots. See definition of dr_jmp_buf_t.
 *
 * int dr_setjmp(dr_jmp_buf_t *buf);
 */
        DECLARE_FUNC(dr_setjmp)
GLOBAL_LABEL(dr_setjmp:)
        mov      REG_R2, ARG1
        stm      REG_R2!, {REG_R4-REG_R11, sp, lr}
        vstm     REG_R2!, {d8-d15}
        push     {r12,lr} /* save two registers for SP-alignment */
        CALLC1(GLOBAL_REF(dr_setjmp_sigmask), ARG1)
        mov      REG_R0, #0
        pop      {r12,pc}
        END_FUNC(dr_setjmp)

/* int dr_longjmp(dr_jmp_buf *buf, int retval);
 */
        DECLARE_FUNC(dr_longjmp)
GLOBAL_LABEL(dr_longjmp:)
        mov      REG_R2, ARG1
        mov      REG_R0, ARG2
        ldm      REG_R2!, {REG_R4-REG_R11, sp, lr}
        vldm     REG_R2!, {d8-d15}
        bx       lr
        END_FUNC(dr_longjmp)

/* int atomic_swap(volatile int *addr, int value)
 * return current contents of addr and replace contents with value.
 */
        DECLARE_FUNC(atomic_swap)
GLOBAL_LABEL(atomic_swap:)
1:      ldrex    REG_R2, [ARG1]
        strex    REG_R3, ARG2, [ARG1]
        cmp      REG_R3, #0
        bne      1b
        mov      REG_R0, REG_R2
        bx       lr
        END_FUNC(atomic_swap)

        DECLARE_FUNC(our_cpuid)
GLOBAL_LABEL(our_cpuid:)
        /* FIXME i#1551: NYI on ARM */
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(our_cpuid)

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
/* thread_id_t dynamorio_clone(uint flags, byte *newsp, void *ptid, void *tls,
 *                             void *ctid, void (*func)(void))
 */
        DECLARE_FUNC(dynamorio_clone)
GLOBAL_LABEL(dynamorio_clone:)
        /* Save callee-saved regs we clobber in the parent. */
        push     {r4, r5, r7}
        ldr      r4, [sp, #12] /* ARG5 minus the pushes above */
        ldr      r5, [sp, #16] /* ARG6 minus the pushes above */
        /* All args are now in syscall registers. */
        /* Push func on the new stack. */
        stmdb    ARG2!, {r5}
        mov      r7, #SYS_clone
        svc      0
        cmp      r0, #0
        bne      dynamorio_clone_parent
        ldmia    sp!, {r0}
        blx      r0
        bl       GLOBAL_REF(unexpected_return)
dynamorio_clone_parent:
        pop      {r4, r5, r7}
        bx       lr
        END_FUNC(dynamorio_clone)

        DECLARE_FUNC(dynamorio_sigreturn)
GLOBAL_LABEL(dynamorio_sigreturn:)
        mov      r7, #SYS_rt_sigreturn
        svc      0
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(dynamorio_sigreturn)

        DECLARE_FUNC(dynamorio_nonrt_sigreturn)
GLOBAL_LABEL(dynamorio_nonrt_sigreturn:)
        mov      r7, #SYS_sigreturn
        svc      0
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(dynamorio_nonrt_sigreturn)


/* we need to exit without using any stack, to support
 * THREAD_SYNCH_TERMINATED_AND_CLEANED.
 */
        DECLARE_FUNC(dynamorio_sys_exit)
GLOBAL_LABEL(dynamorio_sys_exit:)
        mov      r0, #0 /* exit code: hardcoded */
        mov      r7, #SYS_exit
        svc      0
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(dynamorio_sys_exit)


#ifndef NOT_DYNAMORIO_CORE_PROPER

#ifndef HAVE_SIGALTSTACK
# error NYI
#endif
        DECLARE_FUNC(main_signal_handler)
GLOBAL_LABEL(main_signal_handler:)
        mov      ARG4, sp /* pass as extra arg */
        /* i#2107: we repeat the mov to work around odd behavior on Android
         * where sometimes the kernel sends control to the 2nd instruction here.
         */
        mov      ARG4, sp /* pass as extra arg */
        b        GLOBAL_REF(main_signal_handler_C)
        /* main_signal_handler_C will do the ret */
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(main_signal_handler)

#endif /* LINUX */

#endif /* NOT_DYNAMORIO_CORE_PROPER */

/* void hashlookup_null_handler(void)
 * PR 305731: if the app targets NULL, it ends up here, which indirects
 * through hashlookup_null_target to end up in an ibl miss routine.
 */
        DECLARE_FUNC(hashlookup_null_handler)
GLOBAL_LABEL(hashlookup_null_handler:)
        ldr      pc, [pc, #-4]
        nop      /* will be replaced w/ target */
        END_FUNC(hashlookup_null_handler)


        DECLARE_FUNC(back_from_native_retstubs)
GLOBAL_LABEL(back_from_native_retstubs:)
        /* FIXME i#1582: NYI on ARM */
DECLARE_GLOBAL(back_from_native_retstubs_end)
ADDRTAKEN_LABEL(back_from_native_retstubs_end:)
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(back_from_native_retstubs)

        DECLARE_FUNC(back_from_native)
GLOBAL_LABEL(back_from_native:)
        /* FIXME i#1582: NYI on ARM */
        bl       GLOBAL_REF(unexpected_return)
        END_FUNC(back_from_native)

END_FILE
