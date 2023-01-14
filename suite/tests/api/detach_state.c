/* **********************************************************
 * Copyright (c) 2018-2022 Google, Inc.  All rights reserved.
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
check_gpr_value_with_alt(const char *name, ptr_uint_t value, ptr_uint_t expect1,
                         ptr_uint_t expect2)
{
    VPRINT("Value of %s is 0x%lx; expect 0x%lx or 0x%lx\n", name, value, expect1,
           expect2);
    if (value != expect1 && value != expect2) {
        print("ERROR: detach changed %s from 0x%lx or 0x%lx to 0x%lx\n", name, expect1,
              expect2, value);
    }
}

static void
check_gpr_value(const char *name, ptr_uint_t value, ptr_uint_t expect)
{
    VPRINT("Value of %s is 0x%lx; expect 0x%lx\n", name, value, expect);
    if (value != expect)
        print("ERROR: detach changed %s from 0x%lx to 0x%lx\n", name, expect, value);
}

static void
check_simd_value(const char *prefix, int idx, const char *suffix, ptr_uint_t value,
                 ptr_uint_t expect)
{
    char name[16];
    int len = snprintf(name, BUFFER_SIZE_ELEMENTS(name), "%s%d.%s", prefix, idx, suffix);
    assert(len > 0 && len < BUFFER_SIZE_ELEMENTS(name));
    check_gpr_value(name, value, expect);
}

/* If selfmod is true, allows xax and xcx to not match (selfmod code had to
 * tweak them).
 */
void
check_gpr_vals(ptr_uint_t *xsp, bool selfmod)
{
    int i;
#    ifdef X86_64
/**************************************************
 * X86_64
 */
#        define NUM_GPRS 16
#        define SIMD_SZ_IN_PTRS (SIMD_REG_SIZE / sizeof(ptr_uint_t))
#        ifdef __AVX512F__
    for (i = 0; i < NUM_SIMD_REGS; i++) {
        check_simd_value("xmm", i, "lo", *(xsp + NUM_GPRS + i * SIMD_SZ_IN_PTRS + 0),
                         MAKE_HEX_C(XMM0_LOW_BASE()) << i);
        check_simd_value("xmm", i, "hi", *(xsp + NUM_GPRS + i * SIMD_SZ_IN_PTRS + 1),
                         MAKE_HEX_C(XMM0_HIGH_BASE()) << i);
        check_simd_value("ymmh", i, "lo", *(xsp + NUM_GPRS + i * SIMD_SZ_IN_PTRS + 2),
                         MAKE_HEX_C(YMMH0_LOW_BASE()) << i);
        check_simd_value("ymmh", i, "hi", *(xsp + NUM_GPRS + i * SIMD_SZ_IN_PTRS + 3),
                         MAKE_HEX_C(YMMH0_HIGH_BASE()) << i);
        check_simd_value("zmmh", i, "0", *(xsp + NUM_GPRS + i * SIMD_SZ_IN_PTRS + 4),
                         MAKE_HEX_C(ZMMH0_0_BASE()) << i);
        check_simd_value("zmmh", i, "1", *(xsp + NUM_GPRS + i * SIMD_SZ_IN_PTRS + 5),
                         MAKE_HEX_C(ZMMH0_1_BASE()) << i);
        check_simd_value("zmmh", i, "2", *(xsp + NUM_GPRS + i * SIMD_SZ_IN_PTRS + 6),
                         MAKE_HEX_C(ZMMH0_2_BASE()) << i);
        check_simd_value("zmmh", i, "3", *(xsp + NUM_GPRS + i * SIMD_SZ_IN_PTRS + 7),
                         MAKE_HEX_C(ZMMH0_3_BASE()) << i);
    }
    assert(sizeof(unsigned short) == OPMASK_REG_SIZE);
    unsigned short *kbase = (unsigned short *)(xsp + NUM_GPRS + NUM_SIMD_REGS * 8);
    for (i = 0; i < NUM_OPMASK_REGS; i++) {
        ptr_uint_t val = *(kbase + i);
        check_simd_value("k", i, "", val,
                         (unsigned short)(MAKE_HEX_C(OPMASK0_BASE()) << i));
    }
#        elif defined(__AVX__)
    for (i = 0; i < NUM_SIMD_REGS; i++) {
        check_simd_value("xmm", i, "lo", *(xsp + NUM_GPRS + i * SIMD_SZ_IN_PTRS + 0),
                         MAKE_HEX_C(XMM0_LOW_BASE()) << i);
        check_simd_value("xmm", i, "hi", *(xsp + NUM_GPRS + i * SIMD_SZ_IN_PTRS + 1),
                         MAKE_HEX_C(XMM0_HIGH_BASE()) << i);
        check_simd_value("ymmh", i, "lo", *(xsp + NUM_GPRS + i * SIMD_SZ_IN_PTRS + 2),
                         MAKE_HEX_C(YMMH0_LOW_BASE()) << i);
        check_simd_value("ymmh", i, "hi", *(xsp + NUM_GPRS + i * SIMD_SZ_IN_PTRS + 3),
                         MAKE_HEX_C(YMMH0_HIGH_BASE()) << i);
    }
#        else
    for (i = 0; i < NUM_SIMD_REGS; i++) {
        check_simd_value("xmm", i, "lo", *(xsp + NUM_GPRS + i * SIMD_SZ_IN_PTRS + 0),
                         MAKE_HEX_C(XMM0_LOW_BASE()) << i);
        check_simd_value("xmm", i, "hi", *(xsp + NUM_GPRS + i * SIMD_SZ_IN_PTRS + 1),
                         MAKE_HEX_C(XMM0_HIGH_BASE()) << i);
    }
#        endif
    check_gpr_value("r15", *(xsp + 15), MAKE_HEX_C(R15_BASE()));
    check_gpr_value("r14", *(xsp + 14), MAKE_HEX_C(R14_BASE()));
    check_gpr_value("r13", *(xsp + 13), MAKE_HEX_C(R13_BASE()));
    check_gpr_value("r12", *(xsp + 12), MAKE_HEX_C(R12_BASE()));
    check_gpr_value("r11", *(xsp + 11), MAKE_HEX_C(R11_BASE()));
    check_gpr_value("r10", *(xsp + 10), MAKE_HEX_C(R10_BASE()));
    check_gpr_value("r9", *(xsp + 9), MAKE_HEX_C(R9_BASE()));
    check_gpr_value("r8", *(xsp + 8), MAKE_HEX_C(R8_BASE()));
    if (!selfmod)
        check_gpr_value("xax", *(xsp + 7), MAKE_HEX_C(XAX_BASE()));
    check_gpr_value("xcx", *(xsp + 6), MAKE_HEX_C(XCX_BASE()));
    if (!selfmod)
        check_gpr_value("xdx", *(xsp + 5), MAKE_HEX_C(XDX_BASE()));
    check_gpr_value("xbx", *(xsp + 4), MAKE_HEX_C(XBX_BASE()));
    check_gpr_value("xbp", *(xsp + 2), MAKE_HEX_C(XBP_BASE()));
    check_gpr_value("xsi", *(xsp + 1), MAKE_HEX_C(XSI_BASE()));
    check_gpr_value("xdi", *(xsp + 0), MAKE_HEX_C(XDI_BASE()));
#    elif defined(AARCH64)
    /**************************************************
     * AARCH64
     */
    /* Unfortunately, since it's RISC, we have to use x0 in the asm loop.
     * Its value could be either 0x1 or &sideline_exit.
     */
    check_gpr_value_with_alt("x0", *(xsp + 0), 0x1, (ptr_uint_t)&sideline_exit);
    check_gpr_value("x1", *(xsp + 1), MAKE_HEX_C(X1_BASE()));
    check_gpr_value("x2", *(xsp + 2), MAKE_HEX_C(X2_BASE()));
    check_gpr_value("x3", *(xsp + 3), MAKE_HEX_C(X3_BASE()));
    check_gpr_value("x4", *(xsp + 4), MAKE_HEX_C(X4_BASE()));
    check_gpr_value("x5", *(xsp + 5), MAKE_HEX_C(X5_BASE()));
    check_gpr_value("x6", *(xsp + 6), MAKE_HEX_C(X6_BASE()));
    check_gpr_value("x7", *(xsp + 7), MAKE_HEX_C(X7_BASE()));
    check_gpr_value("x8", *(xsp + 8), MAKE_HEX_C(X8_BASE()));
    check_gpr_value("x9", *(xsp + 9), MAKE_HEX_C(X9_BASE()));
    check_gpr_value("x10", *(xsp + 10), MAKE_HEX_C(X10_BASE()));
    check_gpr_value("x11", *(xsp + 11), MAKE_HEX_C(X11_BASE()));
    check_gpr_value("x12", *(xsp + 12), MAKE_HEX_C(X12_BASE()));
    check_gpr_value("x13", *(xsp + 13), MAKE_HEX_C(X13_BASE()));
    check_gpr_value("x14", *(xsp + 14), MAKE_HEX_C(X14_BASE()));
    check_gpr_value("x15", *(xsp + 15), MAKE_HEX_C(X15_BASE()));
    check_gpr_value("x16", *(xsp + 16), MAKE_HEX_C(X16_BASE()));
    check_gpr_value("x17", *(xsp + 17), MAKE_HEX_C(X17_BASE()));
    check_gpr_value("x18", *(xsp + 18), MAKE_HEX_C(X18_BASE()));
    check_gpr_value("x19", *(xsp + 19), MAKE_HEX_C(X19_BASE()));
    check_gpr_value("x20", *(xsp + 20), MAKE_HEX_C(X20_BASE()));
    check_gpr_value("x21", *(xsp + 21), MAKE_HEX_C(X21_BASE()));
    check_gpr_value("x22", *(xsp + 22), MAKE_HEX_C(X22_BASE()));
    check_gpr_value("x23", *(xsp + 23), MAKE_HEX_C(X23_BASE()));
    check_gpr_value("x24", *(xsp + 24), MAKE_HEX_C(X24_BASE()));
    check_gpr_value("x25", *(xsp + 25), MAKE_HEX_C(X25_BASE()));
    check_gpr_value("x26", *(xsp + 26), MAKE_HEX_C(X26_BASE()));
    check_gpr_value("x27", *(xsp + 27), MAKE_HEX_C(X27_BASE()));
    check_gpr_value("x28", *(xsp + 28), MAKE_HEX_C(X28_BASE()));
    check_gpr_value("x29", *(xsp + 29), MAKE_HEX_C(X29_BASE()));
    check_gpr_value("x30", *(xsp + 30), MAKE_HEX_C(X30_BASE()));
    /* TODO i#4698: Check SIMD values on AArch64. */
#    else
#        error NYI /* TODO i#3160: Add 32-bit support. */
#    endif
}

#    ifdef X86
void
check_eflags(ptr_uint_t *xsp)
{
    check_gpr_value("eflags", *xsp, MAKE_HEX_C(XFLAGS_BASE()));
}

void
check_xsp(ptr_uint_t *xsp)
{
    check_gpr_value("xsp", *xsp, (ptr_uint_t)safe_stack);
#        ifdef X64
    /* Ensure redzone unchanged. */
    check_gpr_value("*(xsp-1)", *(-1 + (ptr_uint_t *)(*xsp)), MAKE_HEX_C(XAX_BASE()));
    check_gpr_value("*(xsp-2)", *(-2 + (ptr_uint_t *)(*xsp)), MAKE_HEX_C(XDX_BASE()));
#        endif
}
#    else
/* TODO i#4698: Port to AArch64. */
#    endif

void
make_mem_writable(ptr_uint_t pc)
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

/***************************************************************************
 * Client code
 */

static void *load_from;

static dr_emit_flags_t
event_bb(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
#    ifdef X86
    /* Test i#5786 where a tool-inserted pc-relative load that doesn't reach is
     * mangled by DR and the resulting DR-mangling with no translation triggers
     * DR's (fragile) clean call identication and incorrectly tries to restore
     * state from (garbage) on the dstack.
     */
    instr_t *first = instrlist_first(bb);
    dr_save_reg(drcontext, bb, first, DR_REG_XAX, SPILL_SLOT_1);
    instrlist_meta_preinsert(bb, first,
                             XINST_CREATE_load(drcontext, opnd_create_reg(DR_REG_XAX),
                                               OPND_CREATE_ABSMEM(load_from, OPSZ_PTR)));
    dr_restore_reg(drcontext, bb, first, DR_REG_XAX, SPILL_SLOT_1);
    /* XXX i#4232: If we do not give translations, detach can fail as there are so
     * many no-translation instrs here a thread can end up never landing on a
     * full-state-translatable PC.  Thus, we set the translation, and provide a
     * restore-state event.  i#4232 covers are more automated/convenient way to solve
     * this.
     */
    for (instr_t *inst = instrlist_first(bb); inst != first; inst = instr_get_next(inst))
        instr_set_translation(inst, instr_get_app_pc(first));
#    endif
    /* Test both by alternating and assuming we hit both at the detach points
     * in the various tests.
     */
    if ((ptr_uint_t)tag % 2 == 0)
        return DR_EMIT_DEFAULT;
    else
        return DR_EMIT_STORE_TRANSLATIONS;
}

static bool
event_restore(void *drcontext, bool restore_memory, dr_restore_state_info_t *info)
{
#    ifdef X86
    /* Because we set the translation (to avoid detach timeouts) we need to restore
     * our register too.
     */
    if (info->fragment_info.cache_start_pc == NULL ||
        /* At the first instr should require no translation. */
        info->raw_mcontext->pc <= info->fragment_info.cache_start_pc)
        return true;
#        ifdef X64
    /* We have a code cache address here: verify load_from is not 32-bit-displacement
     * reachable from here.
     */
    assert(
        (ptr_int_t)load_from - (ptr_int_t)info->fragment_info.cache_start_pc < INT_MIN ||
        (ptr_int_t)load_from - (ptr_int_t)info->fragment_info.cache_start_pc > INT_MAX);
#        endif
    instr_t inst;
    instr_init(drcontext, &inst);
    byte *pc = info->fragment_info.cache_start_pc;
    while (pc < info->raw_mcontext->pc) {
        instr_reset(drcontext, &inst);
        pc = decode(drcontext, pc, &inst);
        bool tls;
        uint offs;
        bool spill;
        reg_id_t reg;
        if (instr_is_reg_spill_or_restore(drcontext, &inst, &tls, &spill, &reg, &offs) &&
            tls && !spill && reg == DR_REG_XAX) {
            /* The target point is past our restore. */
            instr_free(drcontext, &inst);
            return true;
        }
    }
    instr_free(drcontext, &inst);
    reg_set_value(DR_REG_XAX, info->mcontext, dr_read_saved_reg(drcontext, SPILL_SLOT_1));
#    endif
    return true;
}

static void
event_exit(void)
{
    dr_custom_free(NULL, 0, load_from, dr_page_size());
    dr_unregister_bb_event(event_bb);
    dr_unregister_restore_state_ex_event(event_restore);
    dr_unregister_exit_event(event_exit);
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    static bool do_once;
    if (!do_once) {
        print("in dr_client_main\n");
        do_once = true;
    }
    dr_register_bb_event(event_bb);
    dr_register_restore_state_ex_event(event_restore);
    dr_register_exit_event(event_exit);

    /* Try to get an address that is not 32-bit-displacement reachable from
     * the cache.  We have an assert on reachability in event_restore().
     */
    load_from = dr_custom_alloc(NULL, 0, dr_page_size(),
                                DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
}

/***************************************************************************
 * Top-level
 */

int
main(void)
{
    sideline_continue = create_cond_var();
    sideline_ready_for_attach = create_cond_var();

    test_thread_func(thread_check_gprs_from_cache);
#    ifdef X86 /* TODO i#1698: Port to AArch64. */
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
#    endif

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

#if defined(X86) && defined(X64)
/**************************************************
 * X86_64
 */
#    define PUSH_GPRS \
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
/* This does NOT make xsp have its pre-push value. */
#    ifdef __AVX512F__
#        define PUSHALL                 \
        lea      rsp, [rsp - NUM_SIMD_REGS*SIMD_REG_SIZE - \
                       NUM_OPMASK_REGS*OPMASK_REG_SIZE] @N@     \
        vmovups  [rsp +  0*SIMD_REG_SIZE], zmm0 @N@ \
        vmovups  [rsp +  1*SIMD_REG_SIZE], zmm1 @N@ \
        vmovups  [rsp +  2*SIMD_REG_SIZE], zmm2 @N@ \
        vmovups  [rsp +  3*SIMD_REG_SIZE], zmm3 @N@ \
        vmovups  [rsp +  4*SIMD_REG_SIZE], zmm4 @N@ \
        vmovups  [rsp +  5*SIMD_REG_SIZE], zmm5 @N@ \
        vmovups  [rsp +  6*SIMD_REG_SIZE], zmm6 @N@ \
        vmovups  [rsp +  7*SIMD_REG_SIZE], zmm7 @N@ \
        vmovups  [rsp +  8*SIMD_REG_SIZE], zmm8 @N@ \
        vmovups  [rsp +  9*SIMD_REG_SIZE], zmm9 @N@ \
        vmovups  [rsp + 10*SIMD_REG_SIZE], zmm10 @N@ \
        vmovups  [rsp + 11*SIMD_REG_SIZE], zmm11 @N@ \
        vmovups  [rsp + 12*SIMD_REG_SIZE], zmm12 @N@ \
        vmovups  [rsp + 13*SIMD_REG_SIZE], zmm13 @N@ \
        vmovups  [rsp + 14*SIMD_REG_SIZE], zmm14 @N@ \
        vmovups  [rsp + 15*SIMD_REG_SIZE], zmm15 @N@ \
        vmovups  [rsp + 16*SIMD_REG_SIZE], zmm16 @N@ \
        vmovups  [rsp + 17*SIMD_REG_SIZE], zmm17 @N@ \
        vmovups  [rsp + 18*SIMD_REG_SIZE], zmm18 @N@ \
        vmovups  [rsp + 19*SIMD_REG_SIZE], zmm19 @N@ \
        vmovups  [rsp + 20*SIMD_REG_SIZE], zmm20 @N@ \
        vmovups  [rsp + 21*SIMD_REG_SIZE], zmm21 @N@ \
        vmovups  [rsp + 22*SIMD_REG_SIZE], zmm22 @N@ \
        vmovups  [rsp + 23*SIMD_REG_SIZE], zmm23 @N@ \
        vmovups  [rsp + 24*SIMD_REG_SIZE], zmm24 @N@ \
        vmovups  [rsp + 25*SIMD_REG_SIZE], zmm25 @N@ \
        vmovups  [rsp + 26*SIMD_REG_SIZE], zmm26 @N@ \
        vmovups  [rsp + 27*SIMD_REG_SIZE], zmm27 @N@ \
        vmovups  [rsp + 28*SIMD_REG_SIZE], zmm28 @N@ \
        vmovups  [rsp + 29*SIMD_REG_SIZE], zmm29 @N@ \
        vmovups  [rsp + 30*SIMD_REG_SIZE], zmm30 @N@ \
        vmovups  [rsp + 31*SIMD_REG_SIZE], zmm31 @N@ \
        kmovw    [rsp + NUM_SIMD_REGS*SIMD_REG_SIZE + 0*OPMASK_REG_SIZE], k0 @N@ \
        kmovw    [rsp + NUM_SIMD_REGS*SIMD_REG_SIZE + 1*OPMASK_REG_SIZE], k1 @N@ \
        kmovw    [rsp + NUM_SIMD_REGS*SIMD_REG_SIZE + 2*OPMASK_REG_SIZE], k2 @N@ \
        kmovw    [rsp + NUM_SIMD_REGS*SIMD_REG_SIZE + 3*OPMASK_REG_SIZE], k3 @N@ \
        kmovw    [rsp + NUM_SIMD_REGS*SIMD_REG_SIZE + 4*OPMASK_REG_SIZE], k4 @N@ \
        kmovw    [rsp + NUM_SIMD_REGS*SIMD_REG_SIZE + 5*OPMASK_REG_SIZE], k5 @N@ \
        kmovw    [rsp + NUM_SIMD_REGS*SIMD_REG_SIZE + 6*OPMASK_REG_SIZE], k6 @N@ \
        kmovw    [rsp + NUM_SIMD_REGS*SIMD_REG_SIZE + 7*OPMASK_REG_SIZE], k7 @N@ \
        PUSH_GPRS
#    elif defined(__AVX__)
#        define PUSHALL                 \
        lea      rsp, [rsp - NUM_SIMD_REGS*SIMD_REG_SIZE] @N@ \
        vmovdqu  [rsp +  0*SIMD_REG_SIZE], ymm0 @N@ \
        vmovdqu  [rsp +  1*SIMD_REG_SIZE], ymm1 @N@ \
        vmovdqu  [rsp +  2*SIMD_REG_SIZE], ymm2 @N@ \
        vmovdqu  [rsp +  3*SIMD_REG_SIZE], ymm3 @N@ \
        vmovdqu  [rsp +  4*SIMD_REG_SIZE], ymm4 @N@ \
        vmovdqu  [rsp +  5*SIMD_REG_SIZE], ymm5 @N@ \
        vmovdqu  [rsp +  6*SIMD_REG_SIZE], ymm6 @N@ \
        vmovdqu  [rsp +  7*SIMD_REG_SIZE], ymm7 @N@ \
        vmovdqu  [rsp +  8*SIMD_REG_SIZE], ymm8 @N@ \
        vmovdqu  [rsp +  9*SIMD_REG_SIZE], ymm9 @N@ \
        vmovdqu  [rsp + 10*SIMD_REG_SIZE], ymm10 @N@ \
        vmovdqu  [rsp + 11*SIMD_REG_SIZE], ymm11 @N@ \
        vmovdqu  [rsp + 12*SIMD_REG_SIZE], ymm12 @N@ \
        vmovdqu  [rsp + 13*SIMD_REG_SIZE], ymm13 @N@ \
        vmovdqu  [rsp + 14*SIMD_REG_SIZE], ymm14 @N@ \
        vmovdqu  [rsp + 15*SIMD_REG_SIZE], ymm15 @N@ \
        PUSH_GPRS
#    else
#        define PUSHALL                 \
        lea      rsp, [rsp - NUM_SIMD_REGS*SIMD_REG_SIZE] @N@ \
        movups   [rsp +  0*SIMD_REG_SIZE], xmm0 @N@ \
        movups   [rsp +  1*SIMD_REG_SIZE], xmm1 @N@ \
        movups   [rsp +  2*SIMD_REG_SIZE], xmm2 @N@ \
        movups   [rsp +  3*SIMD_REG_SIZE], xmm3 @N@ \
        movups   [rsp +  4*SIMD_REG_SIZE], xmm4 @N@ \
        movups   [rsp +  5*SIMD_REG_SIZE], xmm5 @N@ \
        movups   [rsp +  6*SIMD_REG_SIZE], xmm6 @N@ \
        movups   [rsp +  7*SIMD_REG_SIZE], xmm7 @N@ \
        movups   [rsp +  8*SIMD_REG_SIZE], xmm8 @N@ \
        movups   [rsp +  9*SIMD_REG_SIZE], xmm9 @N@ \
        movups   [rsp + 10*SIMD_REG_SIZE], xmm10 @N@ \
        movups   [rsp + 11*SIMD_REG_SIZE], xmm11 @N@ \
        movups   [rsp + 12*SIMD_REG_SIZE], xmm12 @N@ \
        movups   [rsp + 13*SIMD_REG_SIZE], xmm13 @N@ \
        movups   [rsp + 14*SIMD_REG_SIZE], xmm14 @N@ \
        movups   [rsp + 15*SIMD_REG_SIZE], xmm15 @N@ \
        PUSH_GPRS
#    endif
#    define POPALL        \
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
        lea      rsp, [rsp + NUM_SIMD_REGS*SIMD_REG_SIZE + \
                       NUM_OPMASK_REGS*OPMASK_REG_SIZE] @N@
#    ifdef __AVX512F__
/* There seems to be no way to move our immeds into zmm using just rax
 * and zmm##num, since the top part beyond xmm##num is zeroed.
 * We use memory instead.
 */
#        define PUSH_IMMED(immed, num) \
        mov      rax, MAKE_HEX_ASM(immed) @N@ \
        shl      rax, num @N@ \
        push     rax @N@
#        define SET_ZMM_IMMED(num, xmm_low, xmm_high, ymmh_low, ymmh_high, \
                              zmmh0, zmmh1, zmmh2, zmmh3) \
        PUSH_IMMED(zmmh3, num) @N@ \
        PUSH_IMMED(zmmh2, num) @N@ \
        PUSH_IMMED(zmmh1, num) @N@ \
        PUSH_IMMED(zmmh0, num) @N@ \
        PUSH_IMMED(ymmh_high, num) @N@ \
        PUSH_IMMED(ymmh_low, num) @N@ \
        PUSH_IMMED(xmm_high, num) @N@ \
        PUSH_IMMED(xmm_low, num) @N@ \
        vmovups  zmm##num, [rsp] @N@       \
        lea      rsp, [rsp + SIMD_REG_SIZE] @N@
#        define SET_OPMASK_IMMED(num, immed) \
        mov      rax, MAKE_HEX_ASM(immed) @N@ \
        shl      rax, num @N@ \
        kmovw    k##num, eax
#    else
#        define SET_XMM_IMMED(num, low_base, high_base)  \
        mov      rax, MAKE_HEX_ASM(low_base) @N@ \
        shl      rax, num @N@ \
        movq     xmm##num, rax @N@ \
        mov      rax, MAKE_HEX_ASM(high_base) @N@ \
        shl      rax, num @N@ \
        pinsrq   xmm##num, rax, 1 @N@
#        define SET_YMM_IMMED(num, xmm_low, xmm_high, ymmh_low, ymmh_high) \
        SET_XMM_IMMED(num, ymmh_low, ymmh_high) @N@ \
        vinserti128 ymm##num, ymm##num, xmm##num, 1 @N@ \
        SET_XMM_IMMED(num, xmm_low, xmm_high)
#    endif
#        define ALIGN_STACK_ON_FUNC_ENTRY \
        lea      rsp, [rsp - ARG_SZ] @N@
#        define UNALIGN_STACK_ON_FUNC_EXIT \
        lea      rsp, [rsp + ARG_SZ] @N@
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
#elif defined(X86) && !defined(X64)
/**************************************************
 * X86_32
 */
#    define PUSHALL \
        pusha
#    define POPALL  \
        popa
#    error NYI /* TODO i#3160: Add 32-bit support. */
#elif defined(AARCH64)
/**************************************************
 * AARCH64
 */
#    define PUSH_GPRS                           \
        /* SP is checked separately so value here doesn't matter. */ \
        stp      x30, x0, [sp, #-16]! @N@\
        stp      x28, x29, [sp, #-16]! @N@\
        stp      x26, x27, [sp, #-16]! @N@\
        stp      x24, x25, [sp, #-16]! @N@\
        stp      x22, x23, [sp, #-16]! @N@\
        stp      x20, x21, [sp, #-16]! @N@\
        stp      x18, x19, [sp, #-16]! @N@\
        stp      x16, x17, [sp, #-16]! @N@\
        stp      x14, x15, [sp, #-16]! @N@\
        stp      x12, x13, [sp, #-16]! @N@\
        stp      x10, x11, [sp, #-16]! @N@\
        stp      x8, x9, [sp, #-16]! @N@\
        stp      x6, x7, [sp, #-16]! @N@\
        stp      x4, x5, [sp, #-16]! @N@\
        stp      x2, x3, [sp, #-16]! @N@\
        stp      x0, x1, [sp, #-16]!
#    define PUSHALL \
        PUSH_GPRS
        /* TODO i#4698: Push and check SIMD regs. */
#    define PUSH_CALLEE_SAVED \
        stp      x29, x30, [sp, #-16]! @N@\
        stp      x27, x28, [sp, #-16]! @N@\
        stp      x25, x26, [sp, #-16]! @N@\
        stp      x23, x24, [sp, #-16]! @N@\
        stp      x21, x22, [sp, #-16]! @N@\
        stp      x19, x20, [sp, #-16]!
#    define POP_GPRS \
        ldp      x0, x1, [sp], #16 @N@\
        ldp      x2, x3, [sp], #16 @N@\
        ldp      x4, x5, [sp], #16 @N@\
        ldp      x6, x7, [sp], #16 @N@\
        ldp      x8, x9, [sp], #16 @N@\
        ldp      x10, x11, [sp], #16 @N@\
        ldp      x12, x13, [sp], #16 @N@\
        ldp      x14, x15, [sp], #16 @N@\
        ldp      x16, x17, [sp], #16 @N@\
        ldp      x18, x19, [sp], #16 @N@\
        ldp      x20, x21, [sp], #16 @N@\
        ldp      x22, x23, [sp], #16 @N@\
        ldp      x24, x25, [sp], #16 @N@\
        ldp      x26, x27, [sp], #16 @N@\
        ldp      x28, x29, [sp], #16 @N@\
        ldp      x30, x0, [sp], #16
#    define ALIGN_STACK_ON_FUNC_ENTRY /* Nothing. */
#    define UNALIGN_STACK_ON_FUNC_EXIT /* Nothing. */
#    define POPALL \
        POP_GPRS
        /* TODO i#4698: Pop SIMD regs. */
#    define POP_CALLEE_SAVED \
        ldp      x19, x20, [sp], #16 @N@\
        ldp      x21, x22, [sp], #16 @N@\
        ldp      x23, x24, [sp], #16 @N@\
        ldp      x25, x26, [sp], #16 @N@\
        ldp      x27, x28, [sp], #16 @N@\
        ldp      x29, x30, [sp], #16
#elif defined(RISCV64)
    /* FIXME i#3544: Not implemented */
#    define PUSH_GPRS
#    define PUSHALL
#    define PUSH_CALLEE_SAVED
#    define POP_GPRS
#    define ALIGN_STACK_ON_FUNC_ENTRY
#    define UNALIGN_STACK_ON_FUNC_EXIT
#    define POPALL
#    define POP_CALLEE_SAVED
#endif

DECL_EXTERN(sideline_exit)
DECL_EXTERN(sideline_ready_for_detach)
DECL_EXTERN(make_mem_writable)
DECL_EXTERN(check_gpr_vals)
DECL_EXTERN(safe_stack)

#ifdef X86
/**************************************************
 * X86_64
 */
#define FUNCNAME unique_values_to_registers
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        ALIGN_STACK_ON_FUNC_ENTRY
#    ifdef __AVX512F__
#        define Z0 XMM0_LOW_BASE()
#        define Z1 XMM0_HIGH_BASE()
#        define Z2 YMMH0_LOW_BASE()
#        define Z3 YMMH0_HIGH_BASE()
#        define Z4 ZMMH0_0_BASE()
#        define Z5 ZMMH0_1_BASE()
#        define Z6 ZMMH0_2_BASE()
#        define Z7 ZMMH0_3_BASE()
        SET_ZMM_IMMED(0, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(1, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(2, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(3, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(4, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(5, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(6, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(7, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
#        ifdef X64
        SET_ZMM_IMMED(8, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(9, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(10, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(11, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(12, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(13, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(14, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(15, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(16, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(17, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(18, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(19, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(20, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(21, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(22, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(23, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(24, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(25, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(26, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(27, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(28, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(29, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(30, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
        SET_ZMM_IMMED(31, Z0, Z1, Z2, Z3, Z4, Z5, Z6, Z7)
#        endif
#        undef Z0
#        undef Z1
#        undef Z2
#        undef Z3
#        undef Z4
#        undef Z5
#        undef Z6
#        undef Z7
        SET_OPMASK_IMMED(0, OPMASK0_BASE())
        SET_OPMASK_IMMED(1, OPMASK0_BASE())
        SET_OPMASK_IMMED(2, OPMASK0_BASE())
        SET_OPMASK_IMMED(3, OPMASK0_BASE())
        SET_OPMASK_IMMED(4, OPMASK0_BASE())
        SET_OPMASK_IMMED(5, OPMASK0_BASE())
        SET_OPMASK_IMMED(6, OPMASK0_BASE())
        SET_OPMASK_IMMED(7, OPMASK0_BASE())
#    elif defined (__AVX__)
#        define Y0 XMM0_LOW_BASE()
#        define Y1 XMM0_HIGH_BASE()
#        define Y2 YMMH0_LOW_BASE()
#        define Y3 YMMH0_HIGH_BASE()
        SET_YMM_IMMED(0, Y0, Y1, Y2, Y3)
        SET_YMM_IMMED(1, Y0, Y1, Y2, Y3)
        SET_YMM_IMMED(2, Y0, Y1, Y2, Y3)
        SET_YMM_IMMED(3, Y0, Y1, Y2, Y3)
        SET_YMM_IMMED(4, Y0, Y1, Y2, Y3)
        SET_YMM_IMMED(5, Y0, Y1, Y2, Y3)
        SET_YMM_IMMED(6, Y0, Y1, Y2, Y3)
        SET_YMM_IMMED(7, Y0, Y1, Y2, Y3)
#        ifdef X64
        SET_YMM_IMMED(8, Y0, Y1, Y2, Y3)
        SET_YMM_IMMED(9, Y0, Y1, Y2, Y3)
        SET_YMM_IMMED(10, Y0, Y1, Y2, Y3)
        SET_YMM_IMMED(11, Y0, Y1, Y2, Y3)
        SET_YMM_IMMED(12, Y0, Y1, Y2, Y3)
        SET_YMM_IMMED(13, Y0, Y1, Y2, Y3)
        SET_YMM_IMMED(14, Y0, Y1, Y2, Y3)
        SET_YMM_IMMED(15, Y0, Y1, Y2, Y3)
#        endif
#        undef Y0
#        undef Y1
#        undef Y2
#        undef Y3
#    else
        SET_XMM_IMMED(0, XMM0_LOW_BASE(), XMM0_HIGH_BASE())
        SET_XMM_IMMED(1, XMM0_LOW_BASE(), XMM0_HIGH_BASE())
        SET_XMM_IMMED(2, XMM0_LOW_BASE(), XMM0_HIGH_BASE())
        SET_XMM_IMMED(3, XMM0_LOW_BASE(), XMM0_HIGH_BASE())
        SET_XMM_IMMED(4, XMM0_LOW_BASE(), XMM0_HIGH_BASE())
        SET_XMM_IMMED(5, XMM0_LOW_BASE(), XMM0_HIGH_BASE())
        SET_XMM_IMMED(6, XMM0_LOW_BASE(), XMM0_HIGH_BASE())
        SET_XMM_IMMED(7, XMM0_LOW_BASE(), XMM0_HIGH_BASE())
        SET_XMM_IMMED(8, XMM0_LOW_BASE(), XMM0_HIGH_BASE())
        SET_XMM_IMMED(9, XMM0_LOW_BASE(), XMM0_HIGH_BASE())
        SET_XMM_IMMED(10, XMM0_LOW_BASE(), XMM0_HIGH_BASE())
        SET_XMM_IMMED(11, XMM0_LOW_BASE(), XMM0_HIGH_BASE())
        SET_XMM_IMMED(12, XMM0_LOW_BASE(), XMM0_HIGH_BASE())
        SET_XMM_IMMED(13, XMM0_LOW_BASE(), XMM0_HIGH_BASE())
        SET_XMM_IMMED(14, XMM0_LOW_BASE(), XMM0_HIGH_BASE())
        SET_XMM_IMMED(15, XMM0_LOW_BASE(), XMM0_HIGH_BASE())
#    endif
#    ifdef X64
        mov      r15, MAKE_HEX_ASM(R15_BASE())
        mov      r14, MAKE_HEX_ASM(R14_BASE())
        mov      r13, MAKE_HEX_ASM(R13_BASE())
        mov      r12, MAKE_HEX_ASM(R12_BASE())
        mov      r11, MAKE_HEX_ASM(R11_BASE())
        mov      r10, MAKE_HEX_ASM(R10_BASE())
        mov      r9,  MAKE_HEX_ASM(R9_BASE())
        mov      r8,  MAKE_HEX_ASM(R8_BASE())
#    endif
        mov      REG_XAX, MAKE_HEX_ASM(XAX_BASE())
        mov      REG_XCX, MAKE_HEX_ASM(XCX_BASE())
        mov      REG_XDX, MAKE_HEX_ASM(XDX_BASE())
        mov      REG_XBX, MAKE_HEX_ASM(XBX_BASE())
        mov      REG_XBP, MAKE_HEX_ASM(XBP_BASE())
        mov      REG_XSI, MAKE_HEX_ASM(XSI_BASE())
        mov      REG_XDI, MAKE_HEX_ASM(XDI_BASE())
        UNALIGN_STACK_ON_FUNC_EXIT
        ret
        END_FUNC(FUNCNAME)
#    undef FUNCNAME
#    define SET_UNIQUE_REGISTER_VALS \
        CALLC0(unique_values_to_registers)
#elif defined(AARCH64)
/**************************************************
 * AARCH64
 */
# define SET_GPR_IMMED(reg, val) \
        movz reg, @P@((val) & 0xffff) @N@\
        movk reg, @P@((val) >> 16 & 0xffff), lsl @P@16 @N@\
        movk reg, @P@((val) >> 32 & 0xffff), lsl @P@32 @N@\
        movk reg, @P@((val) >> 48 & 0xffff), lsl @P@48

    /* We can't set LR in a callee so we inline: */
#    define SET_UNIQUE_REGISTER_VALS \
        SET_GPR_IMMED(x0, MAKE_HEX_C(X0_BASE())) @N@\
        SET_GPR_IMMED(x1, MAKE_HEX_C(X1_BASE())) @N@\
        SET_GPR_IMMED(x2, MAKE_HEX_C(X2_BASE())) @N@\
        SET_GPR_IMMED(x3, MAKE_HEX_C(X3_BASE())) @N@\
        SET_GPR_IMMED(x4, MAKE_HEX_C(X4_BASE())) @N@\
        SET_GPR_IMMED(x5, MAKE_HEX_C(X5_BASE())) @N@\
        SET_GPR_IMMED(x6, MAKE_HEX_C(X6_BASE())) @N@\
        SET_GPR_IMMED(x7, MAKE_HEX_C(X7_BASE())) @N@\
        SET_GPR_IMMED(x8, MAKE_HEX_C(X8_BASE())) @N@\
        SET_GPR_IMMED(x9, MAKE_HEX_C(X9_BASE())) @N@\
        SET_GPR_IMMED(x10, MAKE_HEX_C(X10_BASE())) @N@\
        SET_GPR_IMMED(x11, MAKE_HEX_C(X11_BASE())) @N@\
        SET_GPR_IMMED(x12, MAKE_HEX_C(X12_BASE())) @N@\
        SET_GPR_IMMED(x13, MAKE_HEX_C(X13_BASE())) @N@\
        SET_GPR_IMMED(x14, MAKE_HEX_C(X14_BASE())) @N@\
        SET_GPR_IMMED(x15, MAKE_HEX_C(X15_BASE())) @N@\
        SET_GPR_IMMED(x16, MAKE_HEX_C(X16_BASE())) @N@\
        SET_GPR_IMMED(x17, MAKE_HEX_C(X17_BASE())) @N@\
        SET_GPR_IMMED(x18, MAKE_HEX_C(X18_BASE())) @N@\
        SET_GPR_IMMED(x19, MAKE_HEX_C(X19_BASE())) @N@\
        SET_GPR_IMMED(x20, MAKE_HEX_C(X20_BASE())) @N@\
        SET_GPR_IMMED(x21, MAKE_HEX_C(X21_BASE())) @N@\
        SET_GPR_IMMED(x22, MAKE_HEX_C(X22_BASE())) @N@\
        SET_GPR_IMMED(x23, MAKE_HEX_C(X23_BASE())) @N@\
        SET_GPR_IMMED(x24, MAKE_HEX_C(X24_BASE())) @N@\
        SET_GPR_IMMED(x25, MAKE_HEX_C(X25_BASE())) @N@\
        SET_GPR_IMMED(x26, MAKE_HEX_C(X26_BASE())) @N@\
        SET_GPR_IMMED(x27, MAKE_HEX_C(X27_BASE())) @N@\
        SET_GPR_IMMED(x28, MAKE_HEX_C(X28_BASE())) @N@\
        SET_GPR_IMMED(x29, MAKE_HEX_C(X29_BASE())) @N@\
        SET_GPR_IMMED(x30, MAKE_HEX_C(X30_BASE()))
#elif defined(RISCV64)
    /* FIXME i#3544: Not implemented */
#    define SET_UNIQUE_REGISTER_VALS
#endif

#ifdef X86
#   define SET_SIDELINE_READY \
        mov      BYTE SYMREF(sideline_ready_for_detach), HEX(1)
#elif defined(AARCH64)
/* We use the x2 immed, which ends in 0x01, to mean "true". */
#   define SET_SIDELINE_READY \
        SET_GPR_IMMED(x2, MAKE_HEX_C(X2_BASE())) @N@ \
        adrp     x0, sideline_ready_for_detach @N@ \
        add      x0, x0, :lo12:sideline_ready_for_detach @N@ \
        strb     w2, [x0]
#elif defined(RISCV64)
    /* FIXME i#3544: Not implemented */
#   define SET_SIDELINE_READY
#endif

#ifdef X86
#   define CHECK_SIDELINE_EXIT \
        cmp      BYTE SYMREF(sideline_exit), HEX(1)
#elif defined(AARCH64)
#   define CHECK_SIDELINE_EXIT \
        adrp     x0, sideline_exit @N@ \
        add      x0, x0, :lo12:sideline_exit @N@ \
        ldrb     w0, [x0] @N@\
        cmp      x0, #1
#elif defined(RISCV64)
/* Since comparison is against 1 we can use just a single register by
 * decrementing the loaded value by 1 and comparing to 0.
 */
#   define CHECK_SIDELINE_EXIT(reg) \
        lb reg, sideline_exit @N@\
        addi reg, reg, -1@N@
#endif

#define FUNCNAME thread_check_gprs_from_cache
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        ALIGN_STACK_ON_FUNC_ENTRY
        /* Preserve callee-saved state. */
        PUSH_CALLEE_SAVED
        /* Put in unique values. */
        SET_UNIQUE_REGISTER_VALS
        /* Signal that we are ready for a detach. */
        SET_SIDELINE_READY
        /* Now spin so we'll detach from the code cache. */
check_gprs_from_cache_spin:
#if defined(RISCV64)
        CHECK_SIDELINE_EXIT(REG_SCRATCH2)
        JUMP_NOT_EQUAL(REG_SCRATCH2, x0) check_gprs_from_cache_spin
#else
        CHECK_SIDELINE_EXIT
        JUMP_NOT_EQUAL check_gprs_from_cache_spin
#endif
        PUSHALL
        mov      REG_SCRATCH0, REG_SP
        mov      REG_SCRATCH1, 0 /* no regs changed */
        CALLC2(GLOBAL_REF(check_gpr_vals), REG_SCRATCH0, REG_SCRATCH1)
        POPALL
        POP_CALLEE_SAVED
        UNALIGN_STACK_ON_FUNC_EXIT
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#ifdef X86 /* TODO i#4698: Port to AArch64. */
#define FUNCNAME thread_check_gprs_from_DR
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        ALIGN_STACK_ON_FUNC_ENTRY
        /* Preserve callee-saved state. */
        PUSH_CALLEE_SAVED
        /* Make code writable for selfmod below */
        call     check_gprs_from_DR_retaddr
check_gprs_from_DR_retaddr:
        pop      REG_XAX
        CALLC1(GLOBAL_REF(make_mem_writable), REG_XAX)
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
        UNALIGN_STACK_ON_FUNC_EXIT
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME thread_check_eflags_from_cache
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        ALIGN_STACK_ON_FUNC_ENTRY
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
        UNALIGN_STACK_ON_FUNC_EXIT
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME thread_check_eflags_from_DR
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        ALIGN_STACK_ON_FUNC_ENTRY
        /* Preserve callee-saved state. */
        PUSH_CALLEE_SAVED
        /* Make code writable for selfmod below */
        call     check_eflags_from_DR_retaddr
check_eflags_from_DR_retaddr:
        pop      REG_XAX
        CALLC1(GLOBAL_REF(make_mem_writable), REG_XAX)
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
        UNALIGN_STACK_ON_FUNC_EXIT
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME thread_check_xsp_from_cache
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        ALIGN_STACK_ON_FUNC_ENTRY
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
        UNALIGN_STACK_ON_FUNC_EXIT
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME thread_check_xsp_from_DR
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        ALIGN_STACK_ON_FUNC_ENTRY
        /* Preserve callee-saved state. */
        PUSH_CALLEE_SAVED
        /* Make code writable for selfmod below */
        call     check_xsp_from_DR_retaddr
check_xsp_from_DR_retaddr:
        pop      REG_XAX
        CALLC1(GLOBAL_REF(make_mem_writable), REG_XAX)
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
        UNALIGN_STACK_ON_FUNC_EXIT
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME
#endif /* X86 */

END_FILE
/* Not turning clang format back on b/c of a clang-format-diff wart:
 * https://github.com/DynamoRIO/dynamorio/pull/4708#issuecomment-772854835
 */
#endif
