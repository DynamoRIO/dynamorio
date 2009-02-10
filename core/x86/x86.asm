/* **********************************************************
 * Copyright (c) 2001-2009 VMware, Inc.  All rights reserved.
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
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2001 Hewlett-Packard Company */

/*
 * x86.asm - x86 specific assembly and trampoline code
 *
 * This file is used for both linux and windows.
 * We used to use the gnu assembler on both platforms, but
 * gas does not support 64-bit windows.
 * Thus we now use masm on windows and gas with the new intel-syntax-specifying
 * options so that our code here only needs a minimum of macros to
 * work on both.
 *
 * Note that for gas on cygwin we used to need to prepend _ to global
 * symbols: we don't need that for linux gas or masm so we don't do it anymore.
 */

/* We handle different registers and calling conventions with a CPP pass.
 * It can be difficult to choose registers that work across all ABI's we're
 * trying to support: we need to move each ARG into a register in case
 * it is passed in memory, but we have to pick registers that don't already
 * hold other arguments.  Typically, use this order:
 *   REG_XAX, REG_XBX, REG_XDI, REG_XSI, REG_XDX, REG_XCX
 * Note that REG_XBX is by convention used on linux for PIC base: if we want 
 * to try and avoid relocations (case 7852) we should avoid using it 
 * to avoid confusion (though we can always pick a different register, 
 * even varying by function).
 * FIXME: should we use virtual registers instead?
 * FIXME: should we have ARG1_IN_REG macro that is either nop or load from stack?
 * For now not bothering, but if we add more routines we'll want more support.
 * Naturally the ARG* macros are only valid at function entry.
 */

#include "asm_defines.asm"
START_FILE

#define RESTORE_FROM_DCONTEXT_VIA_REG(reg,offs,dest) mov dest, PTRSZ [offs + reg]
#define SAVE_TO_DCONTEXT_VIA_REG(reg,offs,src) mov PTRSZ [offs + reg], src
        
/* For the few remaining dcontext_t offsets we need here: */
#ifdef WINDOWS
# ifdef X64
#  define UPCXT_EXTRA 8 /* at_syscall + 4 bytes of padding */
# else
#  define UPCXT_EXTRA 4 /* at_syscall */
# endif
#else
# define UPCXT_EXTRA 8 /* errno + at_syscall */
#endif

/* We should give asm_defines.asm all unique names and then include globals.h
 * and avoid all this duplication!
 */
#ifdef X64
# ifdef WINDOWS
#  define NUM_XMM_SLOTS 6 /* xmm0-5 */
# else
#  define NUM_XMM_SLOTS 16 /* xmm0-15 */
# endif
#else
# define NUM_XMM_SLOTS 8 /* xmm0-7 */
#endif
#define XMM_SAVED_SIZE ((NUM_XMM_SLOTS)*16) /* xmm0-5/15 for PR 264138/PR 302107 */

/* Should we generate all of our asm code instead of having it static?
 * As it is we're duplicating insert_push_all_registers(), dr_insert_call(), etc.,
 * but it's not that much code here in these macros, and this is simpler
 * than emit_utils.c-style code.
 */
#ifdef X64
/* push all registers in dr_mcontext_t order.  does NOT make xsp have a
 * pre-push value as no callers need that (they all use PUSH_DR_MCONTEXT).
 * Leaves space for, but does NOT fill in, the xmm0-5 slots (PR 264138),
 * since it's hard to dynamically figure out during bootstrapping whether
 * movdqu or movups are legal instructions.  The caller is expected
 * to fill in the xmm values prior to any calls that may clobber them.
 */
# define PUSHALL \
        lea      rsp, [rsp - XMM_SAVED_SIZE] @N@\
        push     r15 @N@\
        push     r14 @N@\
        push     r13 @N@\
        push     r12 @N@\
        push     r11 @N@\
        push     r10 @N@\
        push     r9  @N@\
        push     r8  @N@\
        push     rax @N@\
        push     rcx @N@\
        push     rdx @N@\
        push     rbx @N@\
        /* not the pusha pre-push rsp value but see above */ @N@\
        push     rsp @N@\
        push     rbp @N@\
        push     rsi @N@\
        push     rdi
# define POPALL        \
        pop      rdi @N@\
        pop      rsi @N@\
        pop      rbp @N@\
        pop      rbx /* rsp into dead rbx */ @N@\
        pop      rbx @N@\
        pop      rdx @N@\
        pop      rcx @N@\
        pop      rax @N@\
        pop      r8  @N@\
        pop      r9  @N@\
        pop      r10 @N@\
        pop      r11 @N@\
        pop      r12 @N@\
        pop      r13 @N@\
        pop      r14 @N@\
        pop      r15 @N@\
        lea      rsp, [rsp + XMM_SAVED_SIZE]
# define DR_MCONTEXT_SIZE (18*ARG_SZ + XMM_SAVED_SIZE)
# define dstack_OFFSET     (DR_MCONTEXT_SIZE+UPCXT_EXTRA+3*ARG_SZ)
#else
# define PUSHALL \
        lea      esp, [esp - XMM_SAVED_SIZE] @N@\
        pusha
# define POPALL  \
        popa @N@\
        lea      esp, [esp + XMM_SAVED_SIZE]
# define DR_MCONTEXT_SIZE (10*ARG_SZ + XMM_SAVED_SIZE)
# define dstack_OFFSET     (DR_MCONTEXT_SIZE+UPCXT_EXTRA+3*ARG_SZ)
#endif
#define is_exiting_OFFSET (dstack_OFFSET+3*ARG_SZ+4)
#define PUSHALL_XSP_OFFS  (3*ARG_SZ)
#define MCONTEXT_XSP_OFFS (PUSHALL_XSP_OFFS)
#define MCONTEXT_PC_OFFS  (DR_MCONTEXT_SIZE-ARG_SZ)

/* Pushes a dr_mcontext_t on the stack, with an xsp value equal to the
 * xsp before the pushing.  Clobbers xax!
 * Does fill in xmm0-5, if necessary, for PR 264138.
 * Assumes that DR has been initialized (get_xmm_vals() checks proc feature bits).
 * Caller should ensure 16-byte stack alignment prior to the push (PR 306421).
 */
#define PUSH_DR_MCONTEXT(pc)                              \
        push     pc                                    @N@\
        PUSHF                                          @N@\
        PUSHALL                                        @N@\
        lea      REG_XAX, [REG_XSP]                    @N@\
        CALLC1(get_xmm_vals, REG_XAX)                  @N@\
        lea      REG_XAX, [DR_MCONTEXT_SIZE + REG_XSP] @N@\
        mov      [PUSHALL_XSP_OFFS + REG_XSP], REG_XAX


/****************************************************************************/
/****************************************************************************/
#ifndef NOT_DYNAMORIO_CORE_PROPER

DECL_EXTERN(get_own_context_integer_control)
DECL_EXTERN(get_xmm_vals)
DECL_EXTERN(auto_setup)
DECL_EXTERN(back_from_native_C)
DECL_EXTERN(dispatch)
#ifdef DR_APP_EXPORTS
DECL_EXTERN(dr_app_start_helper)
#endif
DECL_EXTERN(dynamo_process_exit)
DECL_EXTERN(dynamo_thread_exit)
DECL_EXTERN(dynamo_thread_stack_free_and_exit)
DECL_EXTERN(dynamorio_app_take_over_helper)
DECL_EXTERN(found_modified_code)
DECL_EXTERN(get_cleanup_and_terminate_global_do_syscall_entry)
#ifdef INTERNAL
DECL_EXTERN(internal_error)
#endif
DECL_EXTERN(internal_exception_info)
DECL_EXTERN(is_currently_on_dstack)
DECL_EXTERN(nt_continue_setup)
#if defined(LINUX) && defined(X64)
DECL_EXTERN(master_signal_handler_C)
#endif

/* non-functions: these make us non-PIC! (PR 212290) */
DECL_EXTERN(exiting_thread_count)
DECL_EXTERN(initstack)
DECL_EXTERN(initstack_mutex)
DECL_EXTERN(int_syscall_address)
DECL_EXTERN(syscalls)
DECL_EXTERN(sysenter_ret_address)
DECL_EXTERN(sysenter_tls_offset)
#ifdef WINDOWS
DECL_EXTERN(wow64_index)
# ifdef X64
DECL_EXTERN(syscall_argsz)
# endif
#endif
        

#ifdef WINDOWS
/* dynamo_auto_start: used for non-early follow children.
 * Assumptions: The saved dr_mcontext_t for the start of the app is on
 * the stack, followed by a pointer to a region of memory to free
 * (which can be NULL) and its size.  This routine is reached by a jmp
 * so be aware of that for address calculation. This routine does
 * not return.
 *
 * On win32, note that in order to export this from the dynamo dll, which is
 * required for non early follow children, we have to explicitly tell the
 * linker to do so.  This is done in the Makefile.
 * Note that if it weren't for wanting local go-native code we would have
 * auto_setup in x86_code.c be dynamo_auto_start.
 */
        DECLARE_FUNC(dynamo_auto_start)
GLOBAL_LABEL(dynamo_auto_start:)
        /* we pass a pointer to TOS as a parameter.
         * a param in xsp won't work w/ win64 padding so put in xax */
        mov      REG_XAX, REG_XSP
        CALLC1(auto_setup, REG_XAX)
        /* if auto_setup returns, we need to go native */
        jmp      load_dynamo_failure
        END_FUNC(dynamo_auto_start)
#endif

#ifdef LINUX
/* We avoid performance problems with messing up the RSB by using
 * a separate routine.  The caller needs to use a plain call
 * with _GLOBAL_OFFSET_TABLE_ on the exact return address instruction.
 */
        DECLARE_FUNC(get_pic_xax)
GLOBAL_LABEL(get_pic_xax:)
        mov      REG_XAX, PTRSZ [REG_XSP]
        ret
        END_FUNC(get_pic_xax)

        DECLARE_FUNC(get_pic_xdi)
GLOBAL_LABEL(get_pic_xdi:)
        mov      REG_XDI, PTRSZ [REG_XSP]
        ret
        END_FUNC(get_pic_xdi)
#endif

/*
 * Calls the specified function 'func' after switching to the stack 'stack'.  If we're
 * currently on the initstack 'free_initstack' should be set so we release the
 * initstack lock.  The supplied 'dcontext' will be passed as an argument to 'func'.
 * If 'func' returns then 'return_on_return' is checked. If set we swap back stacks and
 * return to the caller.  If not set then it's assumed that func wasn't supposed to
 * return and we go to an error routine unexpected_return() below.
 *
 * void call_switch_stack(dcontext_t *dcontext,       // 1*ARG_SZ+XAX
 *                        byte *stack,                // 2*ARG_SZ+XAX
 *                        void (*func)(dcontext_t *), // 3*ARG_SZ+XAX
 *                        bool free_initstack,        // 4*ARG_SZ+XAX
 *                        bool return_on_return)      // 5*ARG_SZ+XAX
 */
        DECLARE_FUNC(call_switch_stack)
GLOBAL_LABEL(call_switch_stack:)
        /* get all args with same offset(xax) regardless of plaform */
#ifdef X64
# ifdef WINDOWS
        mov      REG_XAX, REG_XSP
        lea      REG_XSP, [-ARG_SZ + REG_XSP] /* maintain align-16: offset retaddr */
# else
        /* no padding so we make our own space. odd #slots keeps align-16 w/ retaddr */
        lea      REG_XSP, [-5*ARG_SZ + REG_XSP]
        /* xax points one beyond TOS to get same offset as having retaddr there */
        lea      REG_XAX, [-ARG_SZ + REG_XSP]
        mov      [5*ARG_SZ + REG_XAX], ARG5
# endif
        mov      [1*ARG_SZ + REG_XAX], ARG1
        mov      [2*ARG_SZ + REG_XAX], ARG2
        mov      [3*ARG_SZ + REG_XAX], ARG3
        mov      [4*ARG_SZ + REG_XAX], ARG4
#else
        mov      REG_XAX, REG_XSP
#endif
        /* we need a callee-saved reg across our call so save it onto stack */
        push     REG_XBX
        push     REG_XDI /* still 16-aligned */
        mov      REG_XBX, REG_XAX
        mov      REG_XDI, REG_XSP
        /* set up for call */
        mov      REG_XDX, [3*ARG_SZ + REG_XAX] /* func */
        mov      REG_XCX, [1*ARG_SZ + REG_XAX] /* dcontext */
        mov      REG_XSP, [2*ARG_SZ + REG_XAX] /* stack */
        cmp      DWORD [4*ARG_SZ + REG_XAX], 0 /* free_initstack */
        je       call_dispatch_alt_stack_no_free
#if !defined(X64) && defined(LINUX)
        /* PR 212290: avoid text relocations: get PIC base into xax
         * Can't use CALLC0 since it inserts a nop: we need the exact retaddr.
         */
        call     get_pic_xax
	lea      REG_XAX, [_GLOBAL_OFFSET_TABLE_ + REG_XAX]
	lea      REG_XAX, VAR_VIA_GOT(REG_XAX, initstack_mutex)
        mov      DWORD [REG_XAX], 0
#else
        mov      DWORD SYMREF(initstack_mutex), 0 /* rip-relative on x64 */
#endif
call_dispatch_alt_stack_no_free:
        CALLC1(REG_XDX, REG_XCX)
        mov      REG_XSP, REG_XDI
        mov      REG_XAX, REG_XBX
        cmp      DWORD [5*ARG_SZ + REG_XAX], 0 /* return_on_return */
        je       unexpected_return
        pop      REG_XDI
        pop      REG_XBX
#ifdef X64
# ifdef WINDOWS
        mov      REG_XSP, REG_XAX
# else
        lea      REG_XSP, [5*ARG_SZ + REG_XSP]
# endif
#else
        mov      REG_XSP, REG_XAX
#endif
        ret
        END_FUNC(call_switch_stack)

/*
 * For debugging: report an error if the function called by call_switch_stack()
 * unexpectedly returns.
 */
        DECLARE_FUNC(unexpected_return)
GLOBAL_LABEL(unexpected_return:)
#ifdef INTERNAL
        CALLC3(internal_error, 0, -99 /* line # */, 0)
        /* internal_error never returns */
#endif
        /* infinite loop is intentional: FIXME: do better in release build!
         * FIXME - why not an int3? */
        jmp      unexpected_return
        END_FUNC(unexpected_return)

/*
 * dr_app_start - Causes application to run under Dynamo control 
 */
#ifdef DR_APP_EXPORTS
        DECLARE_EXPORTED_FUNC(dr_app_start)
GLOBAL_LABEL(dr_app_start:)

        /* grab exec state and pass as param in a dr_mcontext_t struct */
        PUSH_DR_MCONTEXT([REG_XSP])  /* push return address as pc */

        /* do the rest in C */
        lea      REG_XAX, [REG_XSP] /* stack grew down, so dr_mcontext_t at tos */
        CALLC1(dr_app_start_helper, REG_XAX)

        /* if we come back, then DR is not taking control so 
         * clean up stack and return */
        add      REG_XSP, DR_MCONTEXT_SIZE
        ret
        END_FUNC(dr_app_start)

/*
 * dr_app_take_over - For the client interface, we'll export 'dr_app_take_over'
 * for consistency with the dr_ naming convention of all exported functions.  
 * We'll keep 'dynamorio_app_take_over' for compatibility with the preinjector.
 */
        DECLARE_EXPORTED_FUNC(dr_app_take_over)
GLOBAL_LABEL(dr_app_take_over:  )
        jmp      dynamorio_app_take_over 
        END_FUNC(dr_app_take_over)
#endif
                
/*
 * dynamorio_app_take_over - Causes application to run under Dynamo
 * control.  Dynamo never releases control.
 */
        DECLARE_EXPORTED_FUNC(dynamorio_app_take_over)
GLOBAL_LABEL(dynamorio_app_take_over:)

        /* grab exec state and pass as param in a dr_mcontext_t struct */
        PUSH_DR_MCONTEXT([REG_XSP])  /* push return address as pc */

        /* do the rest in C */
        lea      REG_XAX, [REG_XSP] /* stack grew down, so dr_mcontext_t at tos */
        CALLC1(dynamorio_app_take_over_helper, REG_XAX)

        /* if we come back, then DR is not taking control so 
         * clean up stack and return */
        add      REG_XSP, DR_MCONTEXT_SIZE
        ret
        END_FUNC(dynamorio_app_take_over)
        
/*
 * cleanup_and_terminate(dcontext_t *dcontext,     // 1*ARG_SZ+XBP
 *                       int sysnum,               // 2*ARG_SZ+XBP = syscall #
 *                       int sys_arg1/param_base,  // 3*ARG_SZ+XBP = arg1 for syscall
 *                       int sys_arg2,             // 4*ARG_SZ+XBP = arg2 for syscall
 *                       bool exitproc)            // 5*ARG_SZ+XBP
 *
 * Calls dynamo_exit_process if exitproc is true, else calls dynamo_exit_thread.
 * Uses the current dstack, but instructs the cleanup routines not to
 * de-allocate it, does a custom de-allocate after swapping to initstack (don't
 * want to use initstack the whole time, that's too long to hold the mutex).
 * Then calls system call sysnum with parameter base param_base, which is presumed
 * to be either NtTerminateThread or NtTerminateProcess or exit.
 * For x64 Windows, args are in ecx and edx (terminate syscalls have only 2 args).
 * For x64 Linux, 1st 2 args are in rdi and rsi.
 *
 * Note that the caller is responsible for placing the actual syscall arguments
 * at the correct offset from edx (or ebx).  See SYSCALL_PARAM_OFFSET in
 * win32 os.c for more info.
 *
 * Note that this routine does not return and thus clobbers callee-saved regs.
 */
        DECLARE_FUNC(cleanup_and_terminate)
GLOBAL_LABEL(cleanup_and_terminate:)
        /* get all args with same offset(xbp) regardless of plaform, to save
         * across our calls.
         */
#ifdef X64
# ifdef WINDOWS
        mov      REG_XBP, REG_XSP
        lea      REG_XSP, [-ARG_SZ + REG_XSP] /* maintain align-16: offset retaddr */
# else
        /* no padding so we make our own space. odd #slots keeps align-16 w/ retaddr */
        lea      REG_XSP, [-5*ARG_SZ + REG_XSP]
        /* xbp points one beyond TOS to get same offset as having retaddr there */
        lea      REG_XBP, [-ARG_SZ + REG_XSP]
        mov      [5*ARG_SZ + REG_XBP], ARG5
# endif
        mov      [1*ARG_SZ + REG_XBP], ARG1
        mov      [2*ARG_SZ + REG_XBP], ARG2
        mov      [3*ARG_SZ + REG_XBP], ARG3
        mov      [4*ARG_SZ + REG_XBP], ARG4
#else
        mov      REG_XBP, REG_XSP
#endif
        /* increment exiting_thread_count so that we don't get killed after 
         * thread_exit removes us from the all_threads list */
#if !defined(X64) && defined(LINUX)
        /* PR 212290: avoid text relocations: get PIC base into callee-saved xdi.
         * Can't use CALLC0 since it inserts a nop: we need the exact retaddr.
         */
        call     get_pic_xdi
	lea      REG_XDI, [_GLOBAL_OFFSET_TABLE_ + REG_XDI]
	lea      REG_XAX, VAR_VIA_GOT(REG_XDI, exiting_thread_count)
        lock inc DWORD [REG_XAX]
#else
        lock inc DWORD SYMREF(exiting_thread_count) /* rip-rel for x64 */
#endif
        /* save dcontext->dstack for freeing later and set dcontext->is_exiting */
        mov      REG_XBX, ARG1 /* xbx is callee-saved and not an x64 param */
        SAVE_TO_DCONTEXT_VIA_REG(REG_XBX,is_exiting_OFFSET,1)
        CALLC1(is_currently_on_dstack, REG_XBX) /* xbx is callee-saved */
        cmp      REG_XAX, 0
        jnz      cat_save_dstack
        mov      REG_XBX, 0 /* save 0 for dstack to avoid double-free */
        jmp      cat_done_saving_dstack
cat_save_dstack:
        RESTORE_FROM_DCONTEXT_VIA_REG(REG_XBX,dstack_OFFSET,REG_XBX)
cat_done_saving_dstack:
        /* PR 306421: xbx is callee-saved for all platforms, so don't push yet,
         * to maintain 16-byte stack alignment
         */
        /* avoid sygate sysenter version as our stack may be static const at 
         * that point, caller will take care of sygate hack */
        CALLC0(get_cleanup_and_terminate_global_do_syscall_entry)
        push     REG_XBX /* 16-byte aligned again */
        push     REG_XAX
        mov      REG_XSI, [5*ARG_SZ + REG_XBP] /* exitproc */
        cmp      REG_XSI, 0
        jz       cat_thread_only
        CALLC0(dynamo_process_exit)
        jmp      cat_no_thread
cat_thread_only:
        CALLC0(dynamo_thread_exit)
cat_no_thread:
        /* now switch to initstack for cleanup of dstack 
         * could use initstack for whole thing but that's too long 
         * of a time to hold global initstack_mutex */
        mov      ecx, 1
#if !defined(X64) && defined(LINUX)
        /* PIC base is still in xdi */
	lea      REG_XAX, VAR_VIA_GOT(REG_XDI, initstack_mutex)
#endif
cat_spin:       
#if !defined(X64) && defined(LINUX)
        xchg     DWORD [REG_XAX], ecx
#else
        xchg     DWORD SYMREF(initstack_mutex), ecx /* rip-relative on x64 */
#endif
        jecxz    cat_have_lock
        /* try again -- too few free regs to call sleep() */
        pause    /* good thing gas now knows about pause */
        jmp      cat_spin
cat_have_lock:
        /* need to grab everything off dstack first */
        mov      REG_XSI, [2*ARG_SZ + REG_XBP]  /* sysnum */
        pop      REG_XAX             /* syscall */
        pop      REG_XCX             /* dstack */
        mov      REG_XBX, [3*ARG_SZ + REG_XBP] /* sys_arg1 */
        mov      REG_XDX, [4*ARG_SZ + REG_XBP] /* sys_arg2 */
        /* swap stacks */
#if !defined(X64) && defined(LINUX)
        /* PIC base is still in xdi */
	lea      REG_XBP, VAR_VIA_GOT(REG_XDI, initstack)
        mov      REG_XSP, PTRSZ [REG_XBP]
#else
        mov      REG_XSP, PTRSZ SYMREF(initstack) /* rip-relative on x64 */
#endif
        /* now save registers */
        push     REG_XDX   /* sys_arg2 */
        push     REG_XBX   /* sys_arg1 */
        push     REG_XAX   /* syscall */
        push     REG_XSI   /* sysnum => xsp 16-byte aligned */
        /* free dstack and call the EXIT_DR_HOOK */
        CALLC1(dynamo_thread_stack_free_and_exit, REG_XCX) /* pass dstack */
        /* finally, execute the termination syscall */
        pop      REG_XAX   /* sysnum */
#ifdef X64
        /* We assume we're doing "syscall" on Windows & Linux, where r10 is dead */
        pop      r10       /* syscall, in reg dead at syscall */
# ifdef LINUX
        pop      REG_XDI   /* sys_arg1 */
        pop      REG_XSI   /* sys_arg2 */
# else
        pop      REG_XCX   /* sys_arg1 */
        pop      REG_XDX   /* sys_arg2 */
# endif
#else
        pop      REG_XSI   /* syscall */
# ifdef LINUX
        pop      REG_XBX   /* sys_arg1 */
        pop      REG_XCX   /* sys_arg2 */
# else
        pop      REG_XDX   /* sys_arg1 == param_base */
        /* sys_arg2 unused */
# endif
#endif
        /* give up initstack mutex -- potential problem here with a thread getting 
         *   an asynch event that then uses initstack, but syscall should only care 
         *   about ebx and edx */
#if !defined(X64) && defined(LINUX)
        /* PIC base is still in xdi */
	lea      REG_XBP, VAR_VIA_GOT(REG_XDI, initstack_mutex)
        mov      DWORD [REG_XBP], 0
#else
        mov      DWORD SYMREF(initstack_mutex), 0 /* rip-relative on x64 */
#endif
        /* we are finished with all shared resources, decrement the  
         * exiting_thread_count (allows another thread to kill us) */
#if !defined(X64) && defined(LINUX)
        /* PIC base is still in xdi */
	lea      REG_XBP, VAR_VIA_GOT(REG_XDI, exiting_thread_count)
        lock dec DWORD [REG_XBP]
#else
        lock dec DWORD SYMREF(exiting_thread_count) /* rip-rel on x64 */
#endif
#ifdef X64
        jmp      r10      /* go do the syscall! */
#else
        jmp      REG_XSI  /* go do the syscall! */
#endif
        END_FUNC(cleanup_and_terminate)

/*
 * cleanup_and_terminate_client_thread(
 *                       int sysnum,               // 1*ARG_SZ+XBP = syscall #
 *                       byte *stack_base,         // 2*ARG_SZ+XBP =base of stack to free
 *                       int sys_arg1/param_base,  // 3*ARG_SZ+XBP = arg1 for syscall
 *                       int sys_arg2)              // 4*ARG_SZ+XBP = arg2 for syscall
 *
 * Swaps to initstack, calls nt_remote_free_virtual_memory on
 * [stack_base, stack_base+stack_size), and then calls system call sysnum
 * with parameter base param_base.
 *
 * Note that the caller is responsible for placing the actual syscall arguments
 * at the correct offset from edx (or ebx).  See SYSCALL_PARAM_OFFSET in
 * win32 os.c for more info.
 * Assumes the caller already incremented exiting_thread_count and
 * removed the thread from all_threads_list.
 */
#ifdef CLIENT_SIDELINE /* PR 222812 */
#ifdef WINDOWS /* we hardcoded nt_free_virtual_memory for now */
        DECLARE_FUNC(cleanup_and_terminate_client_thread)
GLOBAL_LABEL(cleanup_and_terminate_client_thread:)
        mov      REG_XSP, REG_XBP
#ifdef X64
        lea      REG_XSP, [-ARG_SZ + REG_XSP] /* maintain align-16: undo retaddr */
        /* param regs are caller-saved so we must save to stack here */
# ifdef LINUX
        /* no padding so we make our own space */
        lea      REG_XSP, [-4*ARG_SZ + REG_XSP]
# endif
        mov      [1*ARG_SZ + REG_XBP], ARG1
        mov      [2*ARG_SZ + REG_XBP], ARG2
        mov      [3*ARG_SZ + REG_XBP], ARG3
        mov      [4*ARG_SZ + REG_XBP], ARG4
#endif
        /* switch to initstack for cleanup of current stack */
        mov      ecx, 1
catct_spin:       
        xchg     ecx, DWORD SYMREF(initstack_mutex) /* rip-relative on x64 */
        jecxz    catct_have_lock
        /* try again -- too few free regs to call sleep() */
        pause
        jmp      catct_spin
catct_have_lock:
        /* swap stacks */
        mov      REG_XSP, DWORD SYMREF(initstack) /* rip-relative on x64 */
        /* save what we need for termination syscall */
        push     [4*ARG_SZ + REG_XBP] /* sys_arg2 */
        push     [3*ARG_SZ + REG_XBP] /* sys_arg1 => 16-byte aligned */
        /* avoid sygate sysenter version as our stack may be static const at 
         * that point, caller will take care of sygate hack */
        CALLC0(get_cleanup_and_terminate_global_do_syscall_entry)
        push     REG_XAX
        push     [1*ARG_SZ + REG_XBP]  /* sysnum => 16-byte aligned */
        /* make call to nt_remote_free_virtual_memory */
        mov      REG_XAX, [2*ARG_SZ + REG_XBP]  /* stack_base */
        CALLC1(nt_free_virtual_memory, REG_XAX)
        /* finally, execute the termination syscall */
        pop      REG_XAX   /* sysnum */
#ifdef X64
        /* We assume we're doing "syscall" on Windows & Linux, where r10 is dead */
        pop      r10       /* syscall, in reg dead at syscall */
# ifdef LINUX
        pop      REG_XDI   /* sys_arg1 */
        pop      REG_XSI   /* sys_arg2 */
# else
        pop      REG_XCX   /* sys_arg1 */
        pop      REG_XDX   /* sys_arg2 */
# endif
#else
        pop      REG_XSI   /* syscall */
# ifdef LINUX
        pop      REG_XBX   /* sys_arg1 */
        pop      REG_XCX   /* sys_arg2 */
# else
        pop      REG_XDX   /* sys_arg1 == param_base */
        /* sys_arg2 unused */
# endif
#endif
        /* give up initstack mutex -- potential problem here with a thread getting 
         *   an asynch event that then uses initstack, but syscall should only care 
         *   about ebx and edx */
        mov      DWORD SYMREF(initstack_mutex), 0 /* rip-relative on x64 */
        /* we are finished with all shared resources, decrement the  
         * exiting_thread_count (allows another thread to kill us) */
        lock dec DWORD SYMREF(exiting_thread_count) /* rip-relative on x64 */
#ifdef X64
        jmp      r10      /* go do the syscall! */
#else
        jmp      REG_XSI  /* go do the syscall! */
#endif
        END_FUNC(cleanup_and_terminate_client_thread)
#endif /* WINDOWS */
#endif /* CLIENT_SIDELINE */

/* global_do_syscall_int
 * Caller is responsible for all set up.  For windows this means putting the
 * syscall num in eax and putting the args at edx.  For linux this means putting
 * the syscall num in eax, and the args in ebx, ecx, edx, esi, edi and ebp (in
 * that order, as needed).  global_do_syscall is only used with system calls
 * that we don't expect to return, so for debug builds we go into an infinite
 * loop if syscall returns.
 */
        DECLARE_FUNC(global_do_syscall_int)
GLOBAL_LABEL(global_do_syscall_int:)
#ifdef WINDOWS
        int      HEX(2e)
#else
        int      HEX(80)
#endif
#ifdef DEBUG
        jmp      debug_infinite_loop
#endif
        END_FUNC(global_do_syscall_int)

/* For sygate hack need to indirect the system call through ntdll. */
#ifdef WINDOWS
        DECLARE_FUNC(global_do_syscall_sygate_int)
GLOBAL_LABEL(global_do_syscall_sygate_int:)
        /* would be nicer to call so we could return to debug_infinite_loop on 
         * failure, but on some paths (cleanup_and_terminate) we can no longer 
         * safetly use the stack */
        jmp      PTRSZ SYMREF(int_syscall_address)
        END_FUNC(global_do_syscall_sygate_int)
#endif

/* global_do_syscall_sysenter
 * Caller is responsible for all set up, this means putting the syscall num
 * in eax and putting the args at edx+8 (windows specific, we don't yet support
 * linux sysenter).  global_do_syscall is only used with system calls that we
 * don't expect to return.  As edx becomes esp, if the syscall does return it
 * will go to the address in [edx] (again windows specific) (if any debugging
 * code is desired should be pointed to there, do note that edx will become esp
 * so be aware of stack limitations/protections).
 */
        DECLARE_FUNC(global_do_syscall_sysenter)
GLOBAL_LABEL(global_do_syscall_sysenter:)
#if defined(X64) && defined(WINDOWS)
        syscall  /* FIXME ml64 won't take "sysenter" so half-fixing now */
#else
        sysenter
#endif
#ifdef DEBUG
        /* We'll never ever reach here, sysenter won't/can't return to this 
         * address since it doesn't know it, but we'll put in a jmp to 
         * debug_infinite_loop just in case */
        jmp      debug_infinite_loop
#endif
        END_FUNC(global_do_syscall_sysenter)

/* Sygate case 5441 hack - the return address (edx) needs to point to
 * ntdll to pass their verification.  Global_do_syscall is really only
 * used with system calls that aren't expected to return so we don't
 * have to be too careful.  Just shuffle the stack using the sysret addr.
 * If there is already a return address we'll keep that (just move down
 * a slot).
 */
#ifdef WINDOWS
        DECLARE_FUNC(global_do_syscall_sygate_sysenter)
GLOBAL_LABEL(global_do_syscall_sygate_sysenter:)
        mov      REG_XSP, REG_XDX
        /* move existing ret down a slot (note target address is 
         * computed with already inc'ed esp [see intel docs]) */
        pop      PTRSZ [REG_XSP]
        push     PTRSZ SYMREF(sysenter_ret_address)
#if defined(X64) && defined(WINDOWS)
        syscall  /* FIXME ml64 won't take "sysenter" so half-fixing now */
#else
        sysenter
#endif
#ifdef DEBUG
        /* We'll never ever reach here, sysenter won't/can't return to this 
         * address since it doesn't know it, but we'll put in a jmp to 
         * debug_infinite_loop just in case */
        jmp      debug_infinite_loop
#endif
        END_FUNC(global_do_syscall_sygate_sysenter)
#endif

/* Both Windows and Linux put rcx into r10 since rcx is used as the return addr */
#ifdef X64
/* global_do_syscall_syscall
 * Caller is responsible for all set up: putting the syscall num in eax
 * and the args in registers/memory.  Only used with system calls
 * that we don't expect to return, so for debug builds we go into an infinite
 * loop if syscall returns.
  */
       DECLARE_FUNC(global_do_syscall_syscall)
GLOBAL_LABEL(global_do_syscall_syscall:)
        mov      r10, REG_XCX
        syscall
#   ifdef DEBUG
        jmp      debug_infinite_loop
#   endif
        END_FUNC(global_do_syscall_syscall)
#endif

#ifdef WINDOWS

/* global_do_syscall_wow64
 * Xref case 3922
 * Caller is responsible for all set up: putting the syscall num in eax,
 * the wow64 index into ecx, and the args in edx.  Only used with system calls
 * that we don't expect to return, so for debug builds we go into an infinite
 * loop if syscall returns.
  */
       DECLARE_FUNC(global_do_syscall_wow64)
GLOBAL_LABEL(global_do_syscall_wow64:)
        call     PTRSZ SEGMEM(fs,HEX(0c0))
#ifdef DEBUG
        jmp      debug_infinite_loop
#endif
        END_FUNC(global_do_syscall_wow64)

/* global_do_syscall_wow64_index0
 * Sames as global_do_syscall_wow64, except zeros out ecx.
 */
        DECLARE_FUNC(global_do_syscall_wow64_index0)
GLOBAL_LABEL(global_do_syscall_wow64_index0:)
        xor      ecx, ecx
        call     PTRSZ SEGMEM(fs,HEX(0c0))
#ifdef DEBUG
        jmp      debug_infinite_loop
#endif
        END_FUNC(global_do_syscall_wow64_index0)

#endif /* WINDOWS */

#ifdef DEBUG
/* Just an infinite CPU eating loop used to mark certain failures.
 */
        DECLARE_FUNC(debug_infinite_loop)
GLOBAL_LABEL(debug_infinite_loop:)
        jmp      debug_infinite_loop
        END_FUNC(debug_infinite_loop)
#endif
        
#ifdef WINDOWS
/* We use our own syscall wrapper for key win32 system calls.
 *
 * We would use a dynamically generated routine created by decoding
 * a real ntdll wrapper and tweaking it, but we need to use
 * this for our own syscalls and have a bootstrapping problem -- so 
 * rather than hacking to get the power to decode w/o a heap, we hardcode
 * the types we support here.
 *
 * We assume that all syscall wrappers are identical, and they have
 * specific instruction sequences -- thus this routine needs to be updated
 * with any syscall sequence change in a future version of ntdll.dll!
 *
 * We construct our own minimalist versions that use C calling convention
 * and take as a first argument the system call number:
 *
 * ref case 5217, for Sygate compatibility the int needs to come from
 * ntdll.dll, we use a call to NtYieldExecution+9 (int 2e; ret;)
 *
 * 1)   mov immed, eax        mov 4(esp), eax  
 *      lea 4(esp), edx  ==>  lea 8(esp), edx  
 *      int 2e                  int 2e            
 *      ret 4*numargs          ret               
 *
 * 2)   mov immed, eax               mov 4(esp), eax          
 *      mov 0x7ffe0300, edx          mov esp, edx
 *      call {edx,(edx)}             < juggle stack, see below >
 *         NOTE - to support the sygate case 5441 hack the actual instructions
 *              - we use are different, but the end up doing the same thing
 *    callee:                   ==>    sysenter                   
 *        mov esp, edx             our_ret:
 *        sysenter                     ret                    
 *        ret                                         
 *      ret 4*numargs
 *
 * => signature: dynamorio_syscall_{int2e,sysenter}(sysnum, arg1, arg2, ...)
 */
        DECLARE_FUNC(dynamorio_syscall_int2e)
GLOBAL_LABEL(dynamorio_syscall_int2e:)
        mov      eax, [4 + esp]
        lea      edx, [8 + esp]
        int      HEX(2e)
        ret
        END_FUNC(dynamorio_syscall_int2e)

        DECLARE_FUNC(dynamorio_syscall_sygate_int2e)
GLOBAL_LABEL(dynamorio_syscall_sygate_int2e:)
        mov      eax, [4 + esp]
        lea      edx, [8 + esp]
        call     PTRSZ SYMREF(int_syscall_address)
        ret
        END_FUNC(dynamorio_syscall_sygate_int2e)
        
        DECLARE_FUNC(dynamorio_syscall_sysenter)
GLOBAL_LABEL(dynamorio_syscall_sysenter:)
        /* esp + 0    return address 
         *       4    syscall num 
         *       8+   syscall args 
         * Ref case 5461 edx serves as both the argument pointer (edx+8) and the 
         * top of stack for the kernel sysexit. */
        mov      eax, [4 + esp]
        mov      REG_XDX, REG_XSP
#if defined(X64) && defined(WINDOWS)
        syscall  /* FIXME ml64 won't take "sysenter" so half-fixing now */
#else
        sysenter
#endif
        /* Kernel sends control to hardcoded location, which does ret, 
         * which will return directly back to the caller.  Thus the following 
         * ret will never execute. */
        ret
        END_FUNC(dynamorio_syscall_sysenter)

        DECLARE_GLOBAL(dynamorio_sysenter_fixup)
        DECLARE_FUNC(dynamorio_syscall_sygate_sysenter)
GLOBAL_LABEL(dynamorio_syscall_sygate_sysenter:)
        /* stack looks like: 
         * esp + 0    return address 
         *       4    syscall num 
         *       8+   syscall args 
         * Ref case 5461 edx serves as both the argument pointer (edx+8) and the 
         * top  of stack for the kernel sysexit. While we could do nothing and 
         * just have the sysenter return straight back to the caller, we use 
         * sysenter_ret_address indirection to support the Sygate compatibility 
         * fix for case 5441 where steal a ret from ntdll.dll so need to mangle 
         * our stack to look like 
         * esp + 0    sysenter_ret_address 
         *       4    dynamorio_sysenter_fixup 
         *       8+   syscall args 
         * sysenter_tls_slot    return address 
         * before we do the edx <- esp 
         * 
         * NOTE - we can NOT just have 
         * esp + 0    sysenter_ret_address 
         *       4    return address 
         *       8    args 
         * as even though this will go the right place, the stack will be one 
         * off on the return (debug builds with frame ptr are ok, but not 
         * release).  We could roll our own custom calling convention for this 
         * but would be a pain given how this function is called.  So we use a 
         * tls slot to store the return address around the system call since 
         * there isn't room on the stack, thus is not re-entrant, but neither is 
         * dr and we don't make alertable system calls.  An alternate scheme 
         * kept the return address off the top of the stack which works fine 
         * (nothing alertable), but just seemed too risky. 
         * FIXME - any perf impact from breaking hardware return predictor */
        pop      REG_XDX
        mov      eax, DWORD SYMREF(sysenter_tls_offset)
        mov      SEGMEM(fs,eax), edx
        pop      REG_XAX
#ifdef X64
        /* Can't push a 64-bit immed */
        mov      REG_XCX, dynamorio_sysenter_fixup
        push     REG_XCX
#else
        push     dynamorio_sysenter_fixup
#endif
        push     PTRSZ SYMREF(sysenter_ret_address)
        mov      REG_XDX, REG_XSP
#if defined(X64) && defined(WINDOWS)
        syscall  /* FIXME ml64 won't take "sysenter" so half-fixing now */
#else
        sysenter
#endif
ADDRTAKEN_LABEL(dynamorio_sysenter_fixup:)
        /* push whatever (was the slot for the eax arg) */
        push     REG_XAX
        /* ecx/edx should be dead here, just borrow one */
        mov      edx, DWORD SYMREF(sysenter_tls_offset)
        push     PTRSZ SEGMEM(fs,edx)
        ret
        END_FUNC(dynamorio_syscall_sygate_sysenter)

# ifdef X64
/* With the 1st 4 args in registers, we don't want the sysnum to shift them
 * all as it's not easy to un-shift.  So, we put the 1st arg last, and 
 * the SYS enum value first.  We use the syscall_argsz array to restore 
 * the 1st arg.  Since the return value is never larger than 64 bits, we
 * never have to worry about a hidden 1st arg that shifts the rest.
 */
        DECLARE_FUNC(dynamorio_syscall_syscall)
GLOBAL_LABEL(dynamorio_syscall_syscall:)
        mov      rax, QWORD SYMREF(syscalls)
        /* the upper 32 bits are automatically zeroed */
        mov      eax, DWORD [rax + ARG1*4] /* sysnum in rax */
        mov      r10, syscall_argsz
        /* the upper 32 bits are automatically zeroed */
        mov      r10d, DWORD [r10 + ARG1*4] /* # args in r10 */
        cmp      r10, 0
        je       dynamorio_syscall_syscall_ready
        cmp      r10, 1
        je       dynamorio_syscall_syscall_1arg
        cmp      r10, 2
        je       dynamorio_syscall_syscall_2arg
        cmp      r10, 3
        je       dynamorio_syscall_syscall_3arg
        /* else, >= 4 args, so pull from arg slot of (#args + 1) */
        mov      ARG1, QWORD [rsp + r10*8 + 8]
        jmp      dynamorio_syscall_syscall_ready
dynamorio_syscall_syscall_1arg:
        mov      ARG1, ARG2
        jmp      dynamorio_syscall_syscall_ready
dynamorio_syscall_syscall_2arg:
        mov      ARG1, ARG3
        jmp      dynamorio_syscall_syscall_ready
dynamorio_syscall_syscall_3arg:
        mov      ARG1, ARG4
        /* fall-through */
dynamorio_syscall_syscall_ready:
        mov      r10, rcx /* put rcx in r10 just like Nt wrappers (syscall writes rcx) */
        syscall
        ret
        END_FUNC(dynamorio_syscall_syscall)
# endif

/* For WOW64 (case 3922) the syscall wrappers call *teb->WOW32Reserved (==
 * wow64cpu!X86SwitchTo64BitMode), which is a far jmp that switches to the
 * 64-bit cs segment (0x33 selector).  They pass in ecx an index into
 * a function table of argument conversion routines.
 *        
 * 3)   mov sysnum, eax
 *      mov tableidx, ecx
 *      call *fs:0xc0
 *    callee:
 *        jmp  0x33:wow64cpu!CpupReturnFromSimulatedCode
 *      ret 4*numargs
 *
 * rather than taking in sysnum and tableidx, we take in sys_enum and
 * look up the sysnum and tableidx to keep the same args as the other
 * dynamorio_syscall_* routines
 * => signature: dynamorio_syscall_wow64(sys_enum, arg1, arg2, ...)
 */
        DECLARE_FUNC(dynamorio_syscall_wow64)
GLOBAL_LABEL(dynamorio_syscall_wow64:)
        mov      eax, [4 + esp]
        mov      edx, DWORD SYMREF(wow64_index)
        mov      ecx, [edx + eax*4]
        mov      edx, DWORD SYMREF(syscalls)
        mov      eax, [edx + eax*4]
        lea      edx, [8 + esp]
        call     PTRSZ SEGMEM(fs,HEX(0c0))
        ret
        END_FUNC(dynamorio_syscall_wow64)
      
#else /* LINUX  */
/* to avoid libc wrappers we roll our own syscall here
 * hardcoded to use int 0x80 for 32-bit -- FIXME: use something like do_syscall
 * and syscall for 64-bit.
 * signature: dynamorio_syscall(sysnum, num_args, arg1, arg2, ...)
 */
        DECLARE_FUNC(dynamorio_syscall)
GLOBAL_LABEL(dynamorio_syscall:)
        /* x64 kernel doesn't clobber all the callee-saved registers */
        push     REG_XBX
# ifdef X64
        /* reverse order so we don't clobber earlier args */
        mov      REG_XBX, ARG2 /* put num_args where we can reference it longer */
        mov      rax, ARG1 /* sysnum: only need eax, but need rax to use ARG1 (or movzx) */
        cmp      REG_XBX, 0
        je       syscall_ready
        mov      ARG1, ARG3
        cmp      REG_XBX, 1
        je       syscall_ready
        mov      ARG2, ARG4
        cmp      REG_XBX, 2
        je       syscall_ready
        mov      ARG3, ARG5
        cmp      REG_XBX, 3
        je       syscall_ready
        mov      ARG4, ARG6
        cmp      REG_XBX, 4
        je       syscall_ready
        mov      ARG5, [2*ARG_SZ + REG_XSP] /* arg7: above xbx and retaddr */
        cmp      REG_XBX, 5
        je       syscall_ready
        mov      ARG6, [3*ARG_SZ + REG_XSP] /* arg8: above arg7, xbx, retaddr */
syscall_ready:
        mov      r10, rcx
        syscall
# else
        push     REG_XBP
        push     REG_XSI
        push     REG_XDI
        /* add 16 to skip the 4 pushes 
         * FIXME: rather than this dispatch, could have separate routines 
         * for each #args, or could just blindly read upward on the stack. 
         * for dispatch, if assume size of mov instr can do single ind jmp */
        mov      ecx, [16+ 8 + esp] /* num_args */
        cmp      ecx, 0
        je       syscall_0args
        cmp      ecx, 1
        je       syscall_1args
        cmp      ecx, 2
        je       syscall_2args
        cmp      ecx, 3
        je       syscall_3args
        cmp      ecx, 4
        je       syscall_4args
        cmp      ecx, 5
        je       syscall_5args
        mov      ebp, [16+32 + esp] /* arg6 */
syscall_5args:
        mov      edi, [16+28 + esp] /* arg5 */
syscall_4args:
        mov      esi, [16+24 + esp] /* arg4 */
syscall_3args:
        mov      edx, [16+20 + esp] /* arg3 */
syscall_2args:
        mov      ecx, [16+16 + esp] /* arg2 */
syscall_1args:
        mov      ebx, [16+12 + esp] /* arg1 */
syscall_0args:
        mov      eax, [16+ 4 + esp] /* sysnum */
        /* PR 254280: we assume int$80 is ok even for LOL64 */
        int      HEX(80)
        pop      REG_XDI
        pop      REG_XSI
        pop      REG_XBP
#endif
        pop      REG_XBX
        /* return val is in eax for us */
        ret
        END_FUNC(dynamorio_syscall)

/* while with pre-2.6.9 kernels we were able to rely on the kernel's
 * default sigreturn code sequence and be more platform independent,
 * case 6700 necessitates having our own code, which for now, like
 * dynamorio_syscall, hardcodes int 0x80
 */
        DECLARE_FUNC(dynamorio_sigreturn)
GLOBAL_LABEL(dynamorio_sigreturn:)
#ifdef X64
        mov      eax, HEX(f)
        mov      r10, rcx
        syscall
#else
        mov      eax, HEX(ad)
        /* PR 254280: we assume int$80 is ok even for LOL64 */
        int      HEX(80)
#endif
        /* should not return.  if we somehow do,infinite loop is intentional.
         * FIXME: do better in release build! FIXME - why not an int3? */
        jmp      unexpected_return
        END_FUNC(dynamorio_sigreturn)
        
#ifndef X64
/* since our handler is rt, we have no source for the kernel's/libc's
 * default non-rt sigreturn, so we set up our own.
 */
        DECLARE_FUNC(dynamorio_nonrt_sigreturn)
GLOBAL_LABEL(dynamorio_nonrt_sigreturn:)
        pop      eax /* I don't understand why */
        mov      eax, HEX(77)
        /* PR 254280: we assume int$80 is ok even for LOL64 */
        int      HEX(80)
        /* should not return.  if we somehow do,infinite loop is intentional.
         * FIXME: do better in release build! FIXME - why not an int3? */
        jmp      unexpected_return
        END_FUNC(dynamorio_sigreturn)
#endif
        
#ifdef X64
/* PR 305020: for x64 we can't use args to get the original stack pointer,
 * so we use a stub routine here that adds a 4th arg to our C routine:
 *   master_signal_handler(int sig, siginfo_t *siginfo, kernel_ucontext_t *ucxt)
 */
        DECLARE_FUNC(master_signal_handler)
GLOBAL_LABEL(master_signal_handler:)
        mov      rcx, rsp /* pass as 4th arg */
        jmp      master_signal_handler_C
        /* master_signal_handler_C will do the ret */
        END_FUNC(master_signal_handler)
#endif

#endif /* LINUX */


#ifdef WINDOWS    
/*
 * nt_continue_dynamo_start -- invoked to give dynamo control over
 * exception handler continuation (after a call to NtContinue).
 * identical to internal_dynamo_start except it calls nt_continue_start_setup
 * to get the real next pc, and has an esp adjustment at the start.
 */
        DECLARE_FUNC(nt_continue_dynamo_start)
GLOBAL_LABEL(nt_continue_dynamo_start:)
        /* assume valid esp  
         * FIXME: this routine should really not assume esp */

        /* grab exec state and pass as param in a dr_mcontext_t struct */
        PUSH_DR_MCONTEXT(0 /* for dr_mcontext_t.pc */)
        lea      REG_XAX, [REG_XSP] /* stack grew down, so dr_mcontext_t at tos */

        /* Call nt_continue_setup passing the dr_mcontext_t.  It will 
         * obtain and initialize this thread's dcontext pointer and
         * begin execution with the passed-in state.
         */
        CALLC1(nt_continue_setup, REG_XAX)
        /* should not return */
        jmp      unexpected_return
        END_FUNC(nt_continue_dynamo_start)
#endif /* WINDOWS */


/*
 * back_from_native -- for taking control back after letting a module
 * execute natively
 * assumptions: app stack is valid
 */
        DECLARE_FUNC(back_from_native)
GLOBAL_LABEL(back_from_native:)
        /* assume valid esp  
         * FIXME: more robust if don't use app's esp -- should use initstack
         */
        /* grab exec state and pass as param in a dr_mcontext_t struct */
        PUSH_DR_MCONTEXT(0 /* for dr_mcontext_t.pc */)
        lea      REG_XAX, [REG_XSP] /* stack grew down, so dr_mcontext_t at tos */

        /* Call back_from_native_C passing the dr_mcontext_t.  It will 
         * obtain this thread's dcontext pointer and
         * begin execution with the passed-in state.
         */
        CALLC1(back_from_native_C, REG_XAX)
        /* should not return */
        jmp      unexpected_return
        END_FUNC(back_from_native)

        
#ifdef RETURN_STACK
/*#############################################################################
 *#############################################################################
 *
 * FIXME: this is the final routine that uses the old method of
 * having a statically specified templace of assembly code that
 * is copied and patched to produce thread-private versions.
 * All the rest use exclusively generated code using our
 * IR.  This needs to be changed as well, or else the macros
 * need to handle dynamically changing offsets.
 * UPDATE: this code has been removed.  We can pull it out of the
 * attic and fix it up if necessary.
 * We used to have unlinked_return, end_return_lookup, and return_lookup
 * entry points defined here.
 */
.error RETURN_STACK needs generated routine not asm template
/*#############################################################################
 *#############################################################################
 */
#endif /* RETURN_STACK */

/* Our version of setjmp & long jmp.  We don't want to modify app state like
 * SEH or do unwinding which is done by standard versions.
 *
 * int cdecl dr_setjmp(dr_jmp_buf *buf);
 */
        DECLARE_FUNC(dr_setjmp)
GLOBAL_LABEL(dr_setjmp:)
        mov      REG_XDX, ARG1
        mov      [       0 + REG_XDX], REG_XBX
        mov      [  ARG_SZ + REG_XDX], REG_XCX
        mov      [2*ARG_SZ + REG_XDX], REG_XDI
        mov      [3*ARG_SZ + REG_XDX], REG_XSI
        mov      [4*ARG_SZ + REG_XDX], REG_XBP
        mov      [5*ARG_SZ + REG_XDX], REG_XSP
        mov      REG_XAX, [REG_XSP]
        mov      [6*ARG_SZ + REG_XDX], REG_XAX
#ifdef X64
        mov      [ 7*ARG_SZ + REG_XDX], r8
        mov      [ 8*ARG_SZ + REG_XDX], r9
        mov      [ 9*ARG_SZ + REG_XDX], r10
        mov      [10*ARG_SZ + REG_XDX], r11
        mov      [11*ARG_SZ + REG_XDX], r12
        mov      [12*ARG_SZ + REG_XDX], r13
        mov      [13*ARG_SZ + REG_XDX], r14
        mov      [14*ARG_SZ + REG_XDX], r15
#endif
        xor      eax, eax
        ret
        END_FUNC(dr_setjmp)

/* int cdecl dr_longjmp(dr_jmp_buf *buf, int retval);
 */
        DECLARE_FUNC(dr_longjmp)
GLOBAL_LABEL(dr_longjmp:)
        mov      REG_XDX, ARG1
        mov      REG_XAX, ARG2

        mov      REG_XBX, [       0 + REG_XDX]
        mov      REG_XDI, [2*ARG_SZ + REG_XDX]
        mov      REG_XSI, [3*ARG_SZ + REG_XDX]
        mov      REG_XBP, [4*ARG_SZ + REG_XDX]
        mov      REG_XSP, [5*ARG_SZ + REG_XDX] /* now we've switched to the old stack */
        mov      REG_XCX, [6*ARG_SZ + REG_XDX]
        mov      [REG_XSP], REG_XCX    /* restore the return address on to the stack */
        mov      REG_XCX, [  ARG_SZ + REG_XDX]
#ifdef X64
        mov      r8,  [ 7*ARG_SZ + REG_XDX]
        mov      r9,  [ 8*ARG_SZ + REG_XDX]
        mov      r10, [ 9*ARG_SZ + REG_XDX]
        mov      r11, [10*ARG_SZ + REG_XDX]
        mov      r12, [11*ARG_SZ + REG_XDX]
        mov      r13, [12*ARG_SZ + REG_XDX]
        mov      r14, [13*ARG_SZ + REG_XDX]
        mov      r15, [14*ARG_SZ + REG_XDX]
#endif
        ret
        END_FUNC(dr_longjmp)


/*#############################################################################
 *#############################################################################
 * Utility routines moved here due to the lack of inline asm support
 * in VC8.
 */

/* uint atomic_swap(uint *addr, uint value)
 * return current contents of addr and replace contents with value.
 * on win32 could use InterlockedExchange intrinsic instead.
 */
        DECLARE_FUNC(atomic_swap)
GLOBAL_LABEL(atomic_swap:)
        mov      REG_XAX, ARG2
        mov      REG_XCX, ARG1 /* nop on win64 (ditto for linux64 if used rdi) */
        xchg     [REG_XCX], eax
        ret
        END_FUNC(atomic_swap)

/* bool cpuid_supported(void)
 * Checks for existence of the cpuid instr by attempting to modify bit 21 of eflags
 */
        DECLARE_FUNC(cpuid_supported)
GLOBAL_LABEL(cpuid_supported:)
        PUSHF
        pop      REG_XAX
        mov      ecx, eax      /* original eflags in ecx */
        xor      eax, HEX(200000) /* try to modify bit 21 of eflags */
        push     REG_XAX
        POPF
        PUSHF
        pop      REG_XAX
        cmp      ecx, eax
        mov      eax, 0        /* zero out top bytes */
        setne    al
        push     REG_XCX         /* now restore original eflags */
        POPF
        ret
        END_FUNC(cpuid_supported)

/* void our_cpuid(int res[4], int eax)
 * Executes cpuid instr, which is hard for x64 inline asm b/c clobbers rbx and can't
 * push in middle of func.
 */
        DECLARE_FUNC(our_cpuid)
GLOBAL_LABEL(our_cpuid:)
        mov      REG_XDX, ARG1
        mov      REG_XAX, ARG2
        push     REG_XBX /* callee-saved */
        push     REG_XDI /* callee-saved */
        /* not making a call so don't bother w/ 16-byte stack alignment */
        mov      REG_XDI, REG_XDX
        cpuid
        mov      [ 0 + REG_XDI], eax
        mov      [ 4 + REG_XDI], ebx
        mov      [ 8 + REG_XDI], ecx
        mov      [12 + REG_XDI], edx
        pop      REG_XDI /* callee-saved */
        pop      REG_XBX /* callee-saved */
        ret
        END_FUNC(our_cpuid)

#ifdef WINDOWS /* on linux we use inline asm versions */

/* byte *get_frame_ptr(void)
 * returns the value of ebp
 */
        DECLARE_FUNC(get_frame_ptr)
GLOBAL_LABEL(get_frame_ptr:)
        mov      REG_XAX, REG_XBP
        ret
        END_FUNC(get_frame_ptr)

/* void dr_fxsave(byte *buf_aligned) */
        DECLARE_FUNC(dr_fxsave)
GLOBAL_LABEL(dr_fxsave:)
        mov      REG_XAX, ARG1
        fxsave   [REG_XAX]
        fnclex
        finit
        ret
        END_FUNC(dr_fxsave)

/* void fnsave(byte *buf_aligned) */
        DECLARE_FUNC(dr_fnsave)
GLOBAL_LABEL(dr_fnsave:)
        mov      REG_XAX, ARG1
        /* FIXME: do we need an fwait prior to the fnsave? */
        fnsave   [REG_XAX]
        fwait
        ret
        END_FUNC(dr_fnsave)

/* void fxrstor(byte *buf_aligned) */
        DECLARE_FUNC(dr_fxrstor)
GLOBAL_LABEL(dr_fxrstor:)
        mov      REG_XAX, ARG1
        fxrstor  [REG_XAX]
        ret
        END_FUNC(dr_fxrstor)

/* void frstor(byte *buf_aligned) */
        DECLARE_FUNC(dr_frstor)
GLOBAL_LABEL(dr_frstor:)
        mov      REG_XAX, ARG1
        frstor   [REG_XAX]
        ret
        END_FUNC(dr_frstor)

/*
 * void call_modcode_alt_stack(dcontext_t *dcontext,
 *                             EXCEPTION_RECORD *pExcptRec,
 *                             CONTEXT *cxt, app_pc target, uint flags,
 *                             bool using_initstack)
 * custom routine used to transfer control from check_for_modified_code()
 * to found_modified_code() win32/callback.c.
 */
#define dcontext        ARG1
#define pExcptRec       ARG2
#define cxt             ARG3
#define target          ARG4
#define flags           ARG5
#define using_initstack ARG6
        DECLARE_FUNC(call_modcode_alt_stack)
GLOBAL_LABEL(call_modcode_alt_stack:)
        mov      REG_XAX, dcontext /* be careful not to clobber other in-reg params */
        mov      REG_XBX, pExcptRec
        mov      REG_XDI, cxt
        mov      REG_XSI, target
        mov      REG_XDX, flags
        cmp      using_initstack, 0
        je       call_modcode_alt_stack_no_free
        mov      DWORD SYMREF(initstack_mutex), 0 /* rip-relative on x64 */
call_modcode_alt_stack_no_free:
        RESTORE_FROM_DCONTEXT_VIA_REG(REG_XAX,dstack_OFFSET,REG_XSP)
        CALLC5(found_modified_code, REG_XAX, REG_XBX, REG_XDI, REG_XSI, REG_XDX)
        /* should never return */
        jmp      unexpected_return
        ret
        END_FUNC(call_modcode_alt_stack)
#undef dcontext
#undef pExcptRec
#undef cxt
#undef target
#undef flags
#undef using_initstack

#ifdef STACK_GUARD_PAGE
/*
 * void call_intr_excpt_alt_stack(dcontext_t *dcontext, EXCEPTION_RECORD *pExcptRec,
 *                                CONTEXT *cxt, byte *stack)
 *       
 * Routine to switch to a separate exception stack before calling
 * internal_exception_info().  This switch is useful if the dstack
 * is exhausted and we want to ensure we have enough space for
 * error reporting.
 */
#define dcontext        ARG1
#define pExcptRec       ARG2
#define cxt             ARG3
#define stack           ARG4
        DECLARE_FUNC(call_intr_excpt_alt_stack)
GLOBAL_LABEL(call_intr_excpt_alt_stack:)
        mov      REG_XAX, dcontext
        mov      REG_XBX, pExcptRec
        mov      REG_XDI, cxt
        mov      REG_XSI, REG_XSP
        mov      REG_XSP, stack
# ifdef X64
        /* retaddr + this push => 16-byte alignment prior to call */
# endif
        push     REG_XSI       /* save xsp */
        CALLC4(internal_exception_info, \
               REG_XAX /* dcontext */,  \
               REG_XBX /* pExcptRec */, \
               REG_XDI /* cxt */,       \
               1       /* dstack overflow == true */)
        pop      REG_XSP
        ret
        END_FUNC(call_intr_excpt_alt_stack)
#undef dcontext
#undef pExcptRec
#undef cxt
#undef stack
#endif


/* void get_segments_defg(cxt_seg_t *ds, cxt_seg_t *es, cxt_seg_t *fs, cxt_seg_t *gs) */
        DECLARE_FUNC(get_segments_defg)
/* CONTEXT.Seg* is WORD for x64 but DWORD for x86 */
#ifdef X64
# define REG_XAX_SEGWIDTH ax
#else
# define REG_XAX_SEGWIDTH eax
#endif
GLOBAL_LABEL(get_segments_defg:)
        xor      eax, eax
        mov      REG_XBX, ARG1
        mov      ax, ds
        mov      [REG_XBX], REG_XAX_SEGWIDTH
        mov      REG_XBX, ARG2
        mov      ax, es
        mov      [REG_XBX], REG_XAX_SEGWIDTH
        mov      REG_XBX, ARG3
        mov      ax, fs
        mov      [REG_XBX], REG_XAX_SEGWIDTH
        mov      REG_XBX, ARG4
        mov      ax, gs
        mov      [REG_XBX], REG_XAX_SEGWIDTH
        ret
        END_FUNC(get_segments_defg)
#undef REG_XAX_SEGWIDTH

/* void get_own_context_helper(CONTEXT *cxt)
 * does not fix up xsp to match the call site
 * does not preserve callee-saved registers
 */
        DECLARE_FUNC(get_own_context_helper)
GLOBAL_LABEL(get_own_context_helper:)
        /* push callee-saved registers that we use */
        push     REG_XBX
        push     REG_XSI
        push     REG_XDI
#ifdef  X64
        /* w/ retaddr, we're now at 16-byte alignment */
        /* save argument register (PUSH_DR_MCONTEXT calls out to c code) */
        mov REG_XDI, ARG1
#endif
        
        /* grab exec state and pass as param in a dr_mcontext_t struct */
        PUSH_DR_MCONTEXT([(3 * ARG_SZ) + REG_XSP] /* use retaddr for pc */)
        /* we don't have enough registers to avoid parameter regs so we carefully
         * use the suggested register order
         */
        lea      REG_XSI, [REG_XSP] /* stack grew down, so dr_mcontext_t at tos */
#ifdef X64
        mov      REG_XAX, REG_XDI
#else
        /* 4 * arg_sz = 3 callee saved registers pushed to stack plus return addr */
        mov      REG_XAX, [DR_MCONTEXT_SIZE + (4 * ARG_SZ) + REG_XSP]
#endif
        xor      edi, edi
        mov      di, ss
        xor      ebx, ebx
        mov      bx, cs
        CALLC4(get_own_context_integer_control, REG_XAX, REG_XBX, REG_XDI, REG_XSI)
        add      REG_XSP, DR_MCONTEXT_SIZE
        pop      REG_XDI
        pop      REG_XSI
        pop      REG_XBX
        ret
        END_FUNC(get_own_context_helper)

#endif /* WINDOWS */

/* void get_xmm_caller_saved(byte *xmm_caller_saved_buf)
 *   stores the values of xmm0 through xmm5 consecutively into xmm_caller_saved_buf.
 *   xmm_caller_saved_buf need not be 16-byte aligned.
 *   for linux, also saves xmm6-15 (PR 302107).
 *   caller must ensure that the underlying processor supports SSE!
 * FIXME PR 266305: AMD optimization guide says to use movlps+movhps for unaligned
 * stores, instead of movups (movups is best for loads): but for
 * simplicity I'm sticking with movups (assumed not perf-critical here).
 */
        DECLARE_FUNC(get_xmm_caller_saved)
GLOBAL_LABEL(get_xmm_caller_saved:)
        mov      REG_XAX, ARG1
        movups   [REG_XAX + 0*16], xmm0
        movups   [REG_XAX + 1*16], xmm1
        movups   [REG_XAX + 2*16], xmm2
        movups   [REG_XAX + 3*16], xmm3
        movups   [REG_XAX + 4*16], xmm4
        movups   [REG_XAX + 5*16], xmm5
#if defined(LINUX) || !defined(X64)
        movups   [REG_XAX + 6*16], xmm6
        movups   [REG_XAX + 7*16], xmm7
#endif
#if defined(LINUX) && defined(X64)
        movups   [REG_XAX + 8*16], xmm8
        movups   [REG_XAX + 9*16], xmm9
        movups   [REG_XAX + 10*16], xmm10
        movups   [REG_XAX + 11*16], xmm11
        movups   [REG_XAX + 12*16], xmm12
        movups   [REG_XAX + 13*16], xmm13
        movups   [REG_XAX + 14*16], xmm14
        movups   [REG_XAX + 15*16], xmm15
#endif
        ret
        END_FUNC(get_xmm_caller_saved)

/*#############################################################################
 *#############################################################################
 */

/****************************************************************************/
/****************************************************************************/
#endif /* !NOT_DYNAMORIO_CORE_PROPER */

/****************************************************************************
 * routines shared with NOT_DYNAMORIO_CORE_PROPER
 */
#ifdef WINDOWS

/* byte *get_stack_ptr(void)
 * returns the value of xsp before the call
 */
        DECLARE_FUNC(get_stack_ptr)
GLOBAL_LABEL(get_stack_ptr:)
        mov      REG_XAX, REG_XSP
        add      REG_XAX, ARG_SZ /* remove return address space */
        ret
        END_FUNC(get_stack_ptr)

/* void load_dynamo(void)
 *
 * used for injection into a child process
 * N.B.:  if the code here grows, SIZE_OF_LOAD_DYNAMO in win32/inject.c
 * must be updated.
 */
        DECLARE_FUNC(load_dynamo)
GLOBAL_LABEL(load_dynamo:)
    /* the code for this routine is copied into an allocation in the app
       and invoked upon return from the injector. When it is invoked, 
       it expects the app's stack to look like this:

                xsp-->| &LoadLibrary  |
                      | &dynamo_path  |
                      | &GetProcAddr  |
                      | &dynamo_entry |___
                      |               |   |
                      |(saved context)| dr_mcontext_t struct
                      | &code_alloc   |   | pointer to the code allocation
                      | sizeof(code_alloc)| size of the code allocation
                      |_______________|___|
       &dynamo_path-->|               |   |
                      | (dynamo path) | TEXT(DYNAMORIO_DLL_PATH)
                      |_______________|___|
      &dynamo_entry-->|               |   |
                      | (dynamo entry)| "dynamo_auto_start"
                      |               |___|
                      

        in separate allocation         ___
                      |               |   |
                      |     CODE      |  load_dynamo() code
                      |               |___|

        The load_dynamo routine will load the dynamo DLL into memory, then jump
        to its dynamo_auto_start entry point, passing it the saved app context as
        parameters.
     */
        /* two byte NOP to satisfy third party braindead-ness documented in case 3821 */
        mov      edi, edi
#ifdef LOAD_DYNAMO_DEBUGBREAK
        /* having this code in front may hide the problem addressed with the 
         * above padding */
        /* giant loop so can attach debugger, then change ebx to 1 
         * to step through rest of code */
        mov      ebx, HEX(7fffffff)
load_dynamo_repeat_outer:       
        mov      eax, HEX(7fffffff)
load_dynamo_repeatme:   
        dec      eax
        cmp      eax, 0
        jg       load_dynamo_repeatme
        dec      ebx
        cmp      ebx, 0
        jg       load_dynamo_repeat_outer

        /* TOS has &DebugBreak */
        pop      REG_XBX /* pop   REG_XBX = &DebugBreak */
        CALLWIN0(REG_XBX) /* call DebugBreak  (in kernel32.lib) */
#endif
        /* TOS has &LoadLibraryA */
        pop      REG_XBX /* pop   REG_XBX = &LoadLibraryA */
        /* TOS has &dynamo_path */
        pop      REG_XAX /* for 32-bit we're doing "pop eax, push eax" */
        CALLWIN1(REG_XBX, REG_XAX) /* call LoadLibraryA  (in kernel32.lib) */

        /* check result */
        cmp      REG_XAX, 0
        jne      load_dynamo_success
        pop      REG_XBX /* pop off &GetProcAddress */
        pop      REG_XBX /* pop off &dynamo_entry */
        jmp      load_dynamo_failure
load_dynamo_success:
        /* TOS has &GetProcAddress */
        pop      REG_XBX /* pop   REG_XBX = &GetProcAddress */
        /* dynamo_handle is now in REG_XAX (returned by call LoadLibrary) */
        /* TOS has &dynamo_entry */
        pop      REG_XDI /* for 32-bit we're doing "pop edi, push edi" */
        CALLWIN2(REG_XBX, REG_XAX, REG_XDI) /* call GetProcAddress */
        cmp      REG_XAX, 0
        je       load_dynamo_failure

        /* jump to dynamo_auto_start (returned by GetProcAddress) */
        jmp      REG_XAX
        /* dynamo_auto_start will take over or continue natively at the saved 
         * context via load_dynamo_failure.
        */
        END_FUNC(load_dynamo)
/* N.B.: load_dynamo_failure MUST follow load_dynamo, as both are
 * copied in one fell swoop by inject_into_thread()!
 */
/* not really a function but having issues getting both masm and gas to
 * let other asm routines jump here.
 * targeted by load_dynamo and dynamo_auto_start by a jump, not a call,
 * when we should not take over and should go native instead.
 * Xref case 7654: we come here to the child's copy from dynamo_auto_start 
 * instead of returning to the parent's copy post-load_dynamo to avoid
 * incompatibilites with stack layout accross dr versions.
 */
        DECLARE_FUNC(load_dynamo_failure)
GLOBAL_LABEL(load_dynamo_failure:)
        /* Would be nice if we could free our allocation here as well, but 
         * that's too much of a pain (esp. here). 
         * Note TOS has the saved context at this point, xref layout in 
         * auto_setup. Note this code is duplicated in dynamo_auto_start. */
        mov      REG_XAX, [MCONTEXT_XSP_OFFS + REG_XSP] /* load app xsp */
        mov      REG_XBX, [MCONTEXT_PC_OFFS + REG_XSP] /* load app start_pc */
        /* write app start_pc off top of app stack */
        mov      [-ARG_SZ + REG_XAX], REG_XBX
        /* it's ok to write past app TOS since we're just overwriting part of 
         * the dynamo_entry string which is dead at this point, won't affect 
         * the popping of the saved context */
        POPALL /* we assume xmm0-5 do not need to be restored */
        POPF
        /* we assume reading beyond TOS is ok here (no signals on windows) */
         /* restore app xsp (POPALL doesn't): didn't pop pc, so +ARG_SZ */
        mov      REG_XSP, [-DR_MCONTEXT_SIZE + MCONTEXT_XSP_OFFS + ARG_SZ + REG_XSP]
        jmp      PTRSZ [-ARG_SZ + REG_XSP]      /* jmp to app start_pc */

        ret
        END_FUNC(load_dynamo_failure)
        
#endif /* WINDOWS */

END_FILE
