/* **********************************************************
 * Copyright (c) 2018 Google, Inc.  All rights reserved.
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

/* Tests that all app state is properly restored during detach.
 * Further tests could be added:
 * - check mxcsr
 * - check ymm
 * - check segment state
 */

#ifndef ASM_CODE_ONLY /* C code */
#    include <assert.h>
#    include <stdio.h>
#    include <math.h>
#    include <stdint.h>
#    include "configure.h"
#    include "dr_api.h"
#    include "tools.h"
#    include "thread.h"
#    include "condvar.h"
#    include "detach_state_shared.h"
#    ifdef UNIX
#        include <signal.h>
#        include <ucontext.h>
#        define ALT_STACK_SIZE (SIGSTKSZ * 2)
#    endif

#    ifndef X86
#        error Only X86 is supported for this test.
#    endif

#    define VERBOSE 0
#    if VERBOSE
#        define VPRINT(...) print(__VA_ARGS__)
#    else
#        define VPRINT(...) /* nothing */
#    endif

/* Asm funcs */
extern void
thread_check_gprs_from_cache(void);
extern void
thread_check_gprs_from_DR(void);
extern void
thread_check_eflags_from_cache(void);
extern void
thread_check_eflags_from_DR(void);
extern void
thread_check_xsp_from_cache(void);
extern void
thread_check_xsp_from_DR(void);

volatile bool sideline_exit = false;
volatile bool sideline_ready_for_detach = false;
static void *sideline_ready_for_attach;
static void *sideline_continue;
char *safe_stack;

#    ifdef UNIX
static void
thread_check_sigstate(void)
{
    /* We test sigaltstack with attach+detach to avoid bugs like i#3116 */
    stack_t sigstack;
    sigstack.ss_sp = (char *)malloc(ALT_STACK_SIZE);
    sigstack.ss_size = ALT_STACK_SIZE;
    sigstack.ss_flags = 0;
    int res = sigaltstack(&sigstack, NULL);
    assert(res == 0);

    /* Block a few signals */
    sigset_t mask = {
        0, /* Set padding to 0 so we can use memcmp */
    };
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGILL);
    res = sigprocmask(SIG_SETMASK, &mask, NULL);
    assert(res == 0);

    sideline_ready_for_detach = 1;
    while (sideline_exit == 0) {
        thread_yield();
    }

    sigset_t check_mask = {
        0, /* Set padding to 0 so we can use memcmp */
    };
    res = sigprocmask(SIG_BLOCK, NULL, &check_mask);
    assert(res == 0 && memcmp(&mask, &check_mask, sizeof(mask)) == 0);

    stack_t check_stack;
    res = sigaltstack(NULL, &check_stack);
    VPRINT("res=%d, orig=%p-%p %d, now=%p-%p %d\n", res, sigstack.ss_sp,
           sigstack.ss_sp + sigstack.ss_size, sigstack.ss_flags, check_stack.ss_sp,
           check_stack.ss_sp + check_stack.ss_size, check_stack.ss_flags);
    assert(res == 0 && check_stack.ss_sp == sigstack.ss_sp &&
           check_stack.ss_size == sigstack.ss_size &&
           check_stack.ss_flags == sigstack.ss_flags);
    sigstack.ss_flags = SS_DISABLE;
    res = sigaltstack(&sigstack, NULL);
    assert(res == 0);
    free(sigstack.ss_sp);
}

static void
signal_handler(int sig, siginfo_t *info, ucontext_t *ucxt)
{
    VPRINT("in signal handler\n");
    stack_t *frame_stack = ((stack_t *)ucxt) - 1;
    assert(sig == SIGUSR1);
    sideline_ready_for_detach = 1;
    while (sideline_exit == 0) {
        thread_yield();
    }
}

static void
print_sigset(sigset_t *set, const char *prefix)
{
    VPRINT("sigset %s blocked: ", prefix);
    for (int i = 1; i < 32; i++) {
        if (sigismember(set, i))
            VPRINT("%d ", i);
    }
    VPRINT("\n");
}

static void
thread_check_sigstate_from_handler(void)
{
    /* We test sigaltstack with a detach while in an app signal handler to ensure
     * the app frame is fully set to app values.
     */
    stack_t sigstack;
    sigstack.ss_sp = (char *)malloc(ALT_STACK_SIZE);
    sigstack.ss_size = ALT_STACK_SIZE;
    sigstack.ss_flags = 0;
    int res = sigaltstack(&sigstack, NULL);
    assert(res == 0);

    /* Block a few signals */
    sigset_t mask = {
        0, /* Set padding to 0 so we can use memcmp */
    };
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGILL);
    res = sigprocmask(SIG_SETMASK, &mask, NULL);
    assert(res == 0);
    print_sigset(&mask, "pre-handler mask");

    intercept_signal(SIGUSR1, (handler_3_t)signal_handler, true /*SA_ONSTACK*/);
    pthread_kill(pthread_self(), SIGUSR1);

    sigset_t check_mask = {
        0, /* Set padding to 0 so we can use memcmp */
    };
    res = sigprocmask(SIG_BLOCK, NULL, &check_mask);
    print_sigset(&check_mask, "post-handler mask");
    assert(res == 0 && memcmp(&mask, &check_mask, sizeof(mask)) == 0);

    stack_t check_stack;
    res = sigaltstack(NULL, &check_stack);
    VPRINT("res=%d, orig=%p-%p %d, now=%p-%p %d\n", res, sigstack.ss_sp,
           sigstack.ss_sp + sigstack.ss_size, sigstack.ss_flags, check_stack.ss_sp,
           check_stack.ss_sp + check_stack.ss_size, check_stack.ss_flags);
    assert(res == 0 && check_stack.ss_sp == sigstack.ss_sp &&
           check_stack.ss_size == sigstack.ss_size &&
           check_stack.ss_flags == sigstack.ss_flags);
    sigstack.ss_flags = SS_DISABLE;
    res = sigaltstack(&sigstack, NULL);
    assert(res == 0);
    free(sigstack.ss_sp);
}
#    endif

THREAD_FUNC_RETURN_TYPE
sideline_func(void *arg)
{
    void (*asm_func)(void) = (void (*)(void))arg;
    signal_cond_var(sideline_ready_for_attach);
    wait_cond_var(sideline_continue);
    (*asm_func)();
    return THREAD_FUNC_RETURN_ZERO;
}

static void
check_gpr_value(const char *name, ptr_uint_t value, ptr_uint_t expect)
{
    VPRINT("Value of %s is 0x%lx; expect 0x%lx\n", name, value, expect);
    if (value != expect)
        print("ERROR: detach changed %s from 0x%lx to 0x%lx\n", name, expect, value);
}

static void
check_xmm_value(const char *name, ptr_uint_t value, ptr_uint_t expect)
{
    VPRINT("Value of %s is 0x%lx; expect 0x%lx\n", name, value, expect);
    if (value != expect)
        print("ERROR: detach changed %s from 0x%lx to 0x%lx\n", name, expect, value);
}

/* If selfmod is true, allows xax and xcx to not match (selfmod code had to
 * tweak them).
 */
void
check_gpr_vals(ptr_uint_t *xsp, bool selfmod)
{
#    ifdef X64
    check_gpr_value("xmm0.hi", *(xsp + 17), MAKE_HEX_C(XMM0_HIGH_BASE()));
    check_gpr_value("xmm0.lo", *(xsp + 16), MAKE_HEX_C(XMM0_LOW_BASE()));
    check_gpr_value("xmm1.hi", *(xsp + 19), MAKE_HEX_C(XMM1_HIGH_BASE()));
    check_gpr_value("xmm1.lo", *(xsp + 18), MAKE_HEX_C(XMM1_LOW_BASE()));
    check_gpr_value("xmm2.hi", *(xsp + 21), MAKE_HEX_C(XMM2_HIGH_BASE()));
    check_gpr_value("xmm2.lo", *(xsp + 20), MAKE_HEX_C(XMM2_LOW_BASE()));
    check_gpr_value("xmm3.hi", *(xsp + 23), MAKE_HEX_C(XMM3_HIGH_BASE()));
    check_gpr_value("xmm3.lo", *(xsp + 22), MAKE_HEX_C(XMM3_LOW_BASE()));
    check_gpr_value("xmm4.hi", *(xsp + 25), MAKE_HEX_C(XMM4_HIGH_BASE()));
    check_gpr_value("xmm4.lo", *(xsp + 24), MAKE_HEX_C(XMM4_LOW_BASE()));
    check_gpr_value("xmm5.hi", *(xsp + 27), MAKE_HEX_C(XMM5_HIGH_BASE()));
    check_gpr_value("xmm5.lo", *(xsp + 26), MAKE_HEX_C(XMM5_LOW_BASE()));
    check_gpr_value("xmm6.hi", *(xsp + 29), MAKE_HEX_C(XMM6_HIGH_BASE()));
    check_gpr_value("xmm6.lo", *(xsp + 28), MAKE_HEX_C(XMM6_LOW_BASE()));
    check_gpr_value("xmm7.hi", *(xsp + 31), MAKE_HEX_C(XMM7_HIGH_BASE()));
    check_gpr_value("xmm7.lo", *(xsp + 30), MAKE_HEX_C(XMM7_LOW_BASE()));
    check_gpr_value("xmm8.hi", *(xsp + 33), MAKE_HEX_C(XMM8_HIGH_BASE()));
    check_gpr_value("xmm8.lo", *(xsp + 32), MAKE_HEX_C(XMM8_LOW_BASE()));
    check_gpr_value("xmm9.hi", *(xsp + 35), MAKE_HEX_C(XMM9_HIGH_BASE()));
    check_gpr_value("xmm9.lo", *(xsp + 34), MAKE_HEX_C(XMM9_LOW_BASE()));
    check_gpr_value("xmm10.hi", *(xsp + 37), MAKE_HEX_C(XMM10_HIGH_BASE()));
    check_gpr_value("xmm10.lo", *(xsp + 36), MAKE_HEX_C(XMM10_LOW_BASE()));
    check_gpr_value("xmm11.hi", *(xsp + 39), MAKE_HEX_C(XMM11_HIGH_BASE()));
    check_gpr_value("xmm11.lo", *(xsp + 38), MAKE_HEX_C(XMM11_LOW_BASE()));
    check_gpr_value("xmm12.hi", *(xsp + 41), MAKE_HEX_C(XMM12_HIGH_BASE()));
    check_gpr_value("xmm12.lo", *(xsp + 40), MAKE_HEX_C(XMM12_LOW_BASE()));
    check_gpr_value("xmm13.hi", *(xsp + 43), MAKE_HEX_C(XMM13_HIGH_BASE()));
    check_gpr_value("xmm13.lo", *(xsp + 42), MAKE_HEX_C(XMM13_LOW_BASE()));
    check_gpr_value("xmm14.hi", *(xsp + 45), MAKE_HEX_C(XMM14_HIGH_BASE()));
    check_gpr_value("xmm14.lo", *(xsp + 44), MAKE_HEX_C(XMM14_LOW_BASE()));
    check_gpr_value("xmm15.hi", *(xsp + 47), MAKE_HEX_C(XMM15_HIGH_BASE()));
    check_gpr_value("xmm15.lo", *(xsp + 46), MAKE_HEX_C(XMM15_LOW_BASE()));
    check_gpr_value("r15", *(xsp + 15), MAKE_HEX_C(R15_BASE()));
    check_gpr_value("r14", *(xsp + 14), MAKE_HEX_C(R14_BASE()));
    check_gpr_value("r13", *(xsp + 13), MAKE_HEX_C(R13_BASE()));
    check_gpr_value("r12", *(xsp + 12), MAKE_HEX_C(R12_BASE()));
    check_gpr_value("r11", *(xsp + 11), MAKE_HEX_C(R11_BASE()));
    check_gpr_value("r10", *(xsp + 10), MAKE_HEX_C(R10_BASE()));
    check_gpr_value("r9", *(xsp + 9), MAKE_HEX_C(R9_BASE()));
    check_gpr_value("r8", *(xsp + 8), MAKE_HEX_C(R8_BASE()));
#    endif
    if (!selfmod)
        check_gpr_value("xax", *(xsp + 7), MAKE_HEX_C(XAX_BASE()));
    check_gpr_value("xcx", *(xsp + 6), MAKE_HEX_C(XCX_BASE()));
    if (!selfmod)
        check_gpr_value("xdx", *(xsp + 5), MAKE_HEX_C(XDX_BASE()));
    check_gpr_value("xbx", *(xsp + 4), MAKE_HEX_C(XBX_BASE()));
    check_gpr_value("xbp", *(xsp + 2), MAKE_HEX_C(XBP_BASE()));
    check_gpr_value("xsi", *(xsp + 1), MAKE_HEX_C(XSI_BASE()));
    check_gpr_value("xdi", *(xsp + 0), MAKE_HEX_C(XDI_BASE()));
}

void
check_eflags(ptr_uint_t *xsp)
{
    check_gpr_value("eflags", *xsp, MAKE_HEX_C(XFLAGS_BASE()));
}

void
check_xsp(ptr_uint_t *xsp)
{
    check_gpr_value("xsp", *xsp, (ptr_uint_t)safe_stack);
#    ifdef X64
    /* Ensure redzone unchanged. */
    check_gpr_value("*(xsp-1)", *(-1 + (ptr_uint_t *)(*xsp)), MAKE_HEX_C(XAX_BASE()));
    check_gpr_value("*(xsp-2)", *(-2 + (ptr_uint_t *)(*xsp)), MAKE_HEX_C(XDX_BASE()));
#    endif
}

void
make_writable(ptr_uint_t pc)
{
    protect_mem((void *)pc, 1024, ALLOW_READ | ALLOW_WRITE | ALLOW_EXEC);
}

static void
test_thread_func(void (*asm_func)(void))
{
    thread_t thread = create_thread(sideline_func, asm_func);
    dr_app_setup();
    wait_cond_var(sideline_ready_for_attach);
    VPRINT("Starting DR\n");
    dr_app_start();
    signal_cond_var(sideline_continue);
    while (!sideline_ready_for_detach)
        thread_sleep(5);

    VPRINT("Detaching\n");
    dr_app_stop_and_cleanup();
    sideline_exit = true;
    join_thread(thread);

    reset_cond_var(sideline_continue);
    reset_cond_var(sideline_ready_for_attach);
    sideline_exit = false;
    sideline_ready_for_detach = false;
}

int
main(void)
{
    sideline_continue = create_cond_var();
    sideline_ready_for_attach = create_cond_var();

    test_thread_func(thread_check_gprs_from_cache);
    test_thread_func(thread_check_gprs_from_DR);
    test_thread_func(thread_check_eflags_from_cache);
    test_thread_func(thread_check_eflags_from_DR);

    /* DR's detach assumes the app has its regular xsp so we can't put in
     * a weird sentinel value, unfortunately.
     */
    size_t stack_size = 128 * 1024;
    safe_stack = stack_size + allocate_mem(stack_size, ALLOW_READ | ALLOW_WRITE);
    test_thread_func(thread_check_xsp_from_cache);
    test_thread_func(thread_check_xsp_from_DR);
    free_mem(safe_stack - stack_size, stack_size);

    test_thread_func(thread_check_sigstate);
    test_thread_func(thread_check_sigstate_from_handler);

    destroy_cond_var(sideline_continue);
    destroy_cond_var(sideline_ready_for_attach);
    print("all done\n");
    return 0;
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
#    include "detach_state_shared.h"
/* clang-format off */
START_FILE

#    ifdef X64
/* This does NOT make xsp have its pre-push value. */
#        define PUSHALL \
        lea      rsp, [rsp - 16*16] @N@ \
        movups   [rsp +  0*16], xmm0 @N@ \
        movups   [rsp +  1*16], xmm1 @N@ \
        movups   [rsp +  2*16], xmm2 @N@ \
        movups   [rsp +  3*16], xmm3 @N@ \
        movups   [rsp +  4*16], xmm4 @N@ \
        movups   [rsp +  5*16], xmm5 @N@ \
        movups   [rsp +  6*16], xmm6 @N@ \
        movups   [rsp +  7*16], xmm7 @N@ \
        movups   [rsp +  8*16], xmm8 @N@ \
        movups   [rsp +  9*16], xmm9 @N@ \
        movups   [rsp + 10*16], xmm10 @N@ \
        movups   [rsp + 11*16], xmm11 @N@ \
        movups   [rsp + 12*16], xmm12 @N@ \
        movups   [rsp + 13*16], xmm13 @N@ \
        movups   [rsp + 14*16], xmm14 @N@ \
        movups   [rsp + 15*16], xmm15 @N@ \
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
#        define POPALL        \
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
        lea      rsp, [rsp + 16*16] @N@
#        define SET_XMM_IMMED(num, low_base, high_base)  \
        mov      rax, MAKE_HEX_ASM(low_base) @N@ \
        movq     xmm##num, rax @N@ \
        mov      rax, MAKE_HEX_ASM(high_base) @N@ \
        pinsrq   xmm##num, rax, 1 @N@
#        define PUSH_CALLEE_SAVED \
        push     REG_XBP @N@ \
        push     REG_XBX @N@ \
        push     r12 @N@ \
        push     r13 @N@ \
        push     r14 @N@ \
        push     r15 @N@
#        define POP_CALLEE_SAVED \
        pop      r15 @N@ \
        pop      r14 @N@ \
        pop      r13 @N@ \
        pop      r12 @N@ \
        pop      REG_XBX @N@ \
        pop      REG_XBP @N@
#    else
#        define PUSHALL \
        pusha
#        define POPALL  \
        popa
#    error NYI
#endif

DECL_EXTERN(sideline_exit)
DECL_EXTERN(sideline_ready_for_detach)
DECL_EXTERN(make_writable)
DECL_EXTERN(check_gpr_vals)
DECL_EXTERN(safe_stack)

#define FUNCNAME unique_values_to_registers
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        SET_XMM_IMMED(0, XMM0_LOW_BASE(), XMM0_HIGH_BASE())
        SET_XMM_IMMED(1, XMM1_LOW_BASE(), XMM1_HIGH_BASE())
        SET_XMM_IMMED(2, XMM2_LOW_BASE(), XMM2_HIGH_BASE())
        SET_XMM_IMMED(3, XMM3_LOW_BASE(), XMM3_HIGH_BASE())
        SET_XMM_IMMED(4, XMM4_LOW_BASE(), XMM4_HIGH_BASE())
        SET_XMM_IMMED(5, XMM5_LOW_BASE(), XMM5_HIGH_BASE())
        SET_XMM_IMMED(6, XMM6_LOW_BASE(), XMM6_HIGH_BASE())
        SET_XMM_IMMED(7, XMM7_LOW_BASE(), XMM7_HIGH_BASE())
        SET_XMM_IMMED(8, XMM8_LOW_BASE(), XMM8_HIGH_BASE())
        SET_XMM_IMMED(9, XMM9_LOW_BASE(), XMM9_HIGH_BASE())
        SET_XMM_IMMED(10, XMM10_LOW_BASE(), XMM10_HIGH_BASE())
        SET_XMM_IMMED(11, XMM11_LOW_BASE(), XMM11_HIGH_BASE())
        SET_XMM_IMMED(12, XMM12_LOW_BASE(), XMM12_HIGH_BASE())
        SET_XMM_IMMED(13, XMM13_LOW_BASE(), XMM13_HIGH_BASE())
        SET_XMM_IMMED(14, XMM14_LOW_BASE(), XMM14_HIGH_BASE())
        SET_XMM_IMMED(15, XMM15_LOW_BASE(), XMM15_HIGH_BASE())
#ifdef X64
        mov      r15, MAKE_HEX_ASM(R15_BASE())
        mov      r14, MAKE_HEX_ASM(R14_BASE())
        mov      r13, MAKE_HEX_ASM(R13_BASE())
        mov      r12, MAKE_HEX_ASM(R12_BASE())
        mov      r11, MAKE_HEX_ASM(R11_BASE())
        mov      r10, MAKE_HEX_ASM(R10_BASE())
        mov      r9,  MAKE_HEX_ASM(R9_BASE())
        mov      r8,  MAKE_HEX_ASM(R8_BASE())
#endif
        mov      REG_XAX, MAKE_HEX_ASM(XAX_BASE())
        mov      REG_XCX, MAKE_HEX_ASM(XCX_BASE())
        mov      REG_XDX, MAKE_HEX_ASM(XDX_BASE())
        mov      REG_XBX, MAKE_HEX_ASM(XBX_BASE())
        mov      REG_XBP, MAKE_HEX_ASM(XBP_BASE())
        mov      REG_XSI, MAKE_HEX_ASM(XSI_BASE())
        mov      REG_XDI, MAKE_HEX_ASM(XDI_BASE())
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME thread_check_gprs_from_cache
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* Preserve callee-saved state. */
        PUSH_CALLEE_SAVED
        /* Put in unique values. */
        CALLC0(unique_values_to_registers)
        /* Signal that we are ready for a detach. */
        mov      BYTE SYMREF(sideline_ready_for_detach), HEX(1)
        /* Now spin so we'll detach from the code cache. */
check_gprs_from_cache_spin:
        cmp      BYTE SYMREF(sideline_exit), HEX(1)
        jne      check_gprs_from_cache_spin
        PUSHALL
        mov      REG_XAX, REG_XSP
        mov      REG_XCX, 0 /* no regs changed */
        CALLC2(GLOBAL_REF(check_gpr_vals), REG_XAX, REG_XCX)
        POPALL
        POP_CALLEE_SAVED
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME thread_check_gprs_from_DR
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* Preserve callee-saved state. */
        PUSH_CALLEE_SAVED
        /* Make code writable for selfmod below */
        call     check_gprs_from_DR_retaddr
check_gprs_from_DR_retaddr:
        pop      REG_XAX
        CALLC1(GLOBAL_REF(make_writable), REG_XAX)
        /* Put in unique values. */
        CALLC0(unique_values_to_registers)
        /* Signal that we are ready for a detach. */
        mov      BYTE SYMREF(sideline_ready_for_detach), HEX(1)
        /* Now modify our own code so we're likely to detach from DR.
         * The DR code's changed state means we're more likely to see
         * errors in restoring the app state.
         */
        mov      eax, HEX(0)
check_gprs_from_DR_spin:
        inc      eax
        lea      REG_XDX, SYMREF(check_gprs_immed_plus_four - 4)
        mov      DWORD [REG_XDX], eax        /* selfmod write */
        mov      REG_XDX, HEX(0)             /* mov_imm to modify */
ADDRTAKEN_LABEL(check_gprs_immed_plus_four:)
        cmp      BYTE SYMREF(sideline_exit), HEX(1)
        jne      check_gprs_from_DR_spin
        PUSHALL
        mov      REG_XAX, REG_XSP
        mov      REG_XCX, 1 /* regs had to be changed */
        CALLC2(GLOBAL_REF(check_gpr_vals), REG_XAX, REG_XCX)
        POPALL
        POP_CALLEE_SAVED
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME thread_check_eflags_from_cache
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* Preserve callee-saved state. */
        PUSH_CALLEE_SAVED
        /* Put in a unique value. */
        mov      REG_XAX, MAKE_HEX_ASM(XFLAGS_BASE())
        push     REG_XAX
        POPF
        /* Signal that we are ready for a detach. */
        mov      BYTE SYMREF(sideline_ready_for_detach), HEX(1)
        /* Now spin so we'll detach from the code cache. */
check_eflags_from_cache_spin:
        mov      cl, BYTE SYMREF(sideline_exit)
        jecxz    check_eflags_from_DR_spin
        PUSHF
        mov      REG_XAX, REG_XSP
        CALLC1(GLOBAL_REF(check_eflags), REG_XAX)
        pop      REG_XAX
        POP_CALLEE_SAVED
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME thread_check_eflags_from_DR
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* Preserve callee-saved state. */
        PUSH_CALLEE_SAVED
        /* Make code writable for selfmod below */
        call     check_eflags_from_DR_retaddr
check_eflags_from_DR_retaddr:
        pop      REG_XAX
        CALLC1(GLOBAL_REF(make_writable), REG_XAX)
        /* Put in a unique value. */
        mov      REG_XAX, MAKE_HEX_ASM(XFLAGS_BASE())
        push     REG_XAX
        POPF
        /* Signal that we are ready for a detach. */
        mov      BYTE SYMREF(sideline_ready_for_detach), HEX(1)
        /* Now modify our own code so we're likely to detach from DR.
         * The DR code's changed state means we're more likely to see
         * errors in restoring the app state.
         */
        mov      eax, HEX(0)
check_eflags_from_DR_spin:
        lea      REG_XAX, [1 + REG_XAX]
        lea      REG_XDX, SYMREF(check_eflags_immed_plus_four - 4)
        mov      DWORD [REG_XDX], eax        /* selfmod write */
        mov      REG_XDX, HEX(0)             /* mov_imm to modify */
ADDRTAKEN_LABEL(check_eflags_immed_plus_four:)
        mov      cl, BYTE SYMREF(sideline_exit)
        jecxz    check_eflags_from_DR_spin
        PUSHF
        mov      REG_XAX, REG_XSP
        CALLC1(GLOBAL_REF(check_eflags), REG_XAX)
        pop      REG_XAX
        POP_CALLEE_SAVED
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME thread_check_xsp_from_cache
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* Preserve callee-saved state. */
        PUSH_CALLEE_SAVED
        /* Put unique values in the redzone and in the stack pointer. */
        mov      REG_XCX, REG_XSP
        mov      REG_XSP, PTRSZ SYMREF(safe_stack)
#ifdef X64
        /* Put unique values in the redzone. */
        mov      REG_XAX, MAKE_HEX_ASM(XAX_BASE())
        mov      [-8 + REG_XSP], REG_XAX
        mov      REG_XDX, MAKE_HEX_ASM(XDX_BASE())
        mov      [-16 + REG_XSP], REG_XDX
#endif
        /* Signal that we are ready for a detach. */
        mov      BYTE SYMREF(sideline_ready_for_detach), HEX(1)
        /* Now spin so we'll detach from the code cache. */
check_xsp_from_cache_spin:
        cmp      BYTE SYMREF(sideline_exit), HEX(1)
        jne      check_xsp_from_DR_spin
        mov      [-16 + REG_XCX], REG_XSP
        lea      REG_XSP, [-16 + REG_XCX]
        mov      REG_XAX, REG_XSP
        CALLC1(GLOBAL_REF(check_xsp), REG_XAX)
        lea      REG_XSP, [16 + REG_XSP]
        POP_CALLEE_SAVED
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME thread_check_xsp_from_DR
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* Preserve callee-saved state. */
        PUSH_CALLEE_SAVED
        /* Make code writable for selfmod below */
        call     check_xsp_from_DR_retaddr
check_xsp_from_DR_retaddr:
        pop      REG_XAX
        CALLC1(GLOBAL_REF(make_writable), REG_XAX)
        /* Put in a unique value. */
        mov      REG_XCX, REG_XSP
        mov      REG_XSP, PTRSZ SYMREF(safe_stack)
#ifdef X64
        /* Put unique values in the redzone. */
        mov      REG_XAX, MAKE_HEX_ASM(XAX_BASE())
        mov      [-8 + REG_XSP], REG_XAX
        mov      REG_XDX, MAKE_HEX_ASM(XDX_BASE())
        mov      [-16 + REG_XSP], REG_XDX
#endif
        /* Signal that we are ready for a detach. */
        mov      BYTE SYMREF(sideline_ready_for_detach), HEX(1)
        /* Now modify our own code so we're likely to detach from DR.
         * The DR code's changed state means we're more likely to see
         * errors in restoring the app state.
         */
        mov      eax, HEX(0)
check_xsp_from_DR_spin:
        lea      REG_XAX, [1 + REG_XAX]
        lea      REG_XDX, SYMREF(check_xsp_immed_plus_four - 4)
        mov      DWORD [REG_XDX], eax        /* selfmod write */
        mov      REG_XDX, HEX(0)             /* mov_imm to modify */
ADDRTAKEN_LABEL(check_xsp_immed_plus_four:)
        cmp      BYTE SYMREF(sideline_exit), HEX(1)
        jne      check_xsp_from_DR_spin
        mov      [-16 + REG_XCX], REG_XSP
        lea      REG_XSP, [-16 + REG_XCX]
        mov      REG_XAX, REG_XSP
        CALLC1(GLOBAL_REF(check_xsp), REG_XAX)
        lea      REG_XSP, [16 + REG_XSP]
        POP_CALLEE_SAVED
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

END_FILE
/* clang-format on */
#endif
