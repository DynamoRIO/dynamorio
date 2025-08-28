/* **********************************************************
 * Copyright (c) 2018-2025 Google, Inc.  All rights reserved.
 * Copyright (c) 2025 Arm Limited  All rights reserved.
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
thread_check_gprs_from_DR1(void);
extern void
thread_check_gprs_from_DR2(void);
extern void
thread_check_status_reg_from_cache(void);
extern void
thread_check_status_reg_from_DR(void);
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
check_simd_value(const char *prefix, int idx,
                 const char *suffix _IF_AARCH64(uint element_num), ptr_uint_t value,
                 ptr_uint_t expect)
{
    char name[16];
#    if defined(AARCH64)
    int len = snprintf(name, BUFFER_SIZE_ELEMENTS(name), "%s%d.%s[%u]", prefix, idx,
                       suffix, element_num);
#    else
    int len = snprintf(name, BUFFER_SIZE_ELEMENTS(name), "%s%d.%s", prefix, idx, suffix);
#    endif
    assert(len > 0 && len < BUFFER_SIZE_ELEMENTS(name));
    check_gpr_value(name, value, expect);
}

/*
 * Check the values of all SIMD and GPR restisters saved to the stack against reference
 * values. Some tests clobber values of certain registers meaning those registers can't be
 * checked. gpr_check_mask contains a mask indicating which GPRs should be checked.
 */
void
check_gpr_vals(ptr_uint_t *xsp, uint gpr_check_mask)
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
#        define CHECK_GPR(name, num, reference_value)                       \
            do {                                                            \
                if (TEST(1 << (num), gpr_check_mask))                       \
                    check_gpr_value(name, *(xsp + (num)), reference_value); \
            } while (0)
    CHECK_GPR("r15", 15, MAKE_HEX_C(R15_BASE()));
    CHECK_GPR("r14", 14, MAKE_HEX_C(R14_BASE()));
    CHECK_GPR("r13", 13, MAKE_HEX_C(R13_BASE()));
    CHECK_GPR("r12", 12, MAKE_HEX_C(R12_BASE()));
    CHECK_GPR("r11", 11, MAKE_HEX_C(R11_BASE()));
    CHECK_GPR("r10", 10, MAKE_HEX_C(R10_BASE()));
    CHECK_GPR("r9", 9, MAKE_HEX_C(R9_BASE()));
    CHECK_GPR("r8", 8, MAKE_HEX_C(R8_BASE()));
    CHECK_GPR("xax", 7, MAKE_HEX_C(XAX_BASE()));
    CHECK_GPR("xcx", 6, MAKE_HEX_C(XCX_BASE()));
    CHECK_GPR("xdx", 5, MAKE_HEX_C(XDX_BASE()));
    CHECK_GPR("xbx", 4, MAKE_HEX_C(XBX_BASE()));
    CHECK_GPR("xbp", 2, MAKE_HEX_C(XBP_BASE()));
    CHECK_GPR("xsi", 1, MAKE_HEX_C(XSI_BASE()));
    CHECK_GPR("xdi", 0, MAKE_HEX_C(XDI_BASE()));
#        undef CHECK_GPR
#    elif defined(AARCH64)
    /**************************************************
     * AARCH64
     */

    if (TEST(1, gpr_check_mask)) {
        /* Unfortunately, since it's RISC, we have to use x0 in the asm loop.
         * Its value could be either 0x1 or &sideline_exit.
         */
        check_gpr_value_with_alt("x0", *(xsp + 0), 0x1, (ptr_uint_t)&sideline_exit);
    }
#        define CHECK_GPR(name, num, reference_value)                       \
            do {                                                            \
                if (TEST(1 << (num), gpr_check_mask))                       \
                    check_gpr_value(name, *(xsp + (num)), reference_value); \
            } while (0)
    CHECK_GPR("x1", 1, MAKE_HEX_C(X1_BASE()));
    CHECK_GPR("x2", 2, MAKE_HEX_C(X2_BASE()));
    CHECK_GPR("x3", 3, MAKE_HEX_C(X3_BASE()));
    CHECK_GPR("x4", 4, MAKE_HEX_C(X4_BASE()));
    CHECK_GPR("x5", 5, MAKE_HEX_C(X5_BASE()));
    CHECK_GPR("x6", 6, MAKE_HEX_C(X6_BASE()));
    CHECK_GPR("x7", 7, MAKE_HEX_C(X7_BASE()));
    CHECK_GPR("x8", 8, MAKE_HEX_C(X8_BASE()));
    CHECK_GPR("x9", 9, MAKE_HEX_C(X9_BASE()));
    CHECK_GPR("x10", 10, MAKE_HEX_C(X10_BASE()));
    CHECK_GPR("x11", 11, MAKE_HEX_C(X11_BASE()));
    CHECK_GPR("x12", 12, MAKE_HEX_C(X12_BASE()));
    CHECK_GPR("x13", 13, MAKE_HEX_C(X13_BASE()));
    CHECK_GPR("x14", 14, MAKE_HEX_C(X14_BASE()));
    CHECK_GPR("x15", 15, MAKE_HEX_C(X15_BASE()));
    CHECK_GPR("x16", 16, MAKE_HEX_C(X16_BASE()));
    CHECK_GPR("x17", 17, MAKE_HEX_C(X17_BASE()));
    CHECK_GPR("x18", 18, MAKE_HEX_C(X18_BASE()));
    CHECK_GPR("x19", 19, MAKE_HEX_C(X19_BASE()));
    CHECK_GPR("x20", 20, MAKE_HEX_C(X20_BASE()));
    CHECK_GPR("x21", 21, MAKE_HEX_C(X21_BASE()));
    CHECK_GPR("x22", 22, MAKE_HEX_C(X22_BASE()));
    CHECK_GPR("x23", 23, MAKE_HEX_C(X23_BASE()));
    CHECK_GPR("x24", 24, MAKE_HEX_C(X24_BASE()));
    CHECK_GPR("x25", 25, MAKE_HEX_C(X25_BASE()));
    CHECK_GPR("x26", 26, MAKE_HEX_C(X26_BASE()));
    CHECK_GPR("x27", 27, MAKE_HEX_C(X27_BASE()));
    CHECK_GPR("x28", 28, MAKE_HEX_C(X28_BASE()));
    CHECK_GPR("x29", 29, MAKE_HEX_C(X29_BASE()));
    CHECK_GPR("x30", 30, MAKE_HEX_C(X30_BASE()));
#        undef CHECK_GPR

#        if defined(__ARM_FEATURE_SVE)
    size_t vector_length_in_bytes;
    asm("rdvl %[dest], 1" : [dest] "=r"(vector_length_in_bytes));
    static const char *prefix = "z";
#        else
    static const size_t vector_length_in_bytes = 16;
    static const char *prefix = "v";
#        endif

    const size_t num_vector_registers = 32;
    const size_t simd_sz_in_ptrs = vector_length_in_bytes / sizeof(ptr_uint_t);

    /* The expected value for 2048-bit Z0. This is directly compared to the value of
     * z0/v0 and used to calculate expected values for all other vector registers.
     * We just use a subset of the array for vector lengths < 2048-bits.
     */
    ptr_uint_t vec_expected[32] = {
#        define XL(n) MAKE_HEX_C(UINT64_C(n))
#        define E(n)                                            \
            SIMD_UNIQUE_VAL_DOUBLEWORD_ELEMENT(XL(Z0_0_BASE()), \
                                               XL(Z_ELEMENT_INCREMENT_BASE()), n)
        E(0),   E(8),   E(16),  E(24),  E(32),  E(40),  E(48),  E(56),
        E(64),  E(72),  E(80),  E(88),  E(96),  E(104), E(112), E(120),
        E(128), E(136), E(144), E(152), E(160), E(168), E(176), E(184),
        E(192), E(200), E(208), E(216), E(224), E(232), E(240), E(248),
#        undef E
#        undef XL
    };
    /* There are 31 AArch64 general purpose registers but x0 is saved twice in order to
     * keep the stack 16-byte aligned.
     */
    static const ptr_uint_t gprs_on_stack = 32;
    for (i = 0; i < num_vector_registers; i++) {
        const ptr_uint_t *vector_reg_data = xsp + gprs_on_stack + (i * simd_sz_in_ptrs);

        for (uint element = 0; element < simd_sz_in_ptrs; element++) {
            check_simd_value(prefix, i, "d", element, vector_reg_data[element],
                             vec_expected[element]);
        }

        /* Increment each byte to generate the expected value for the next register. */
        for (size_t j = 0; j < vector_length_in_bytes; j++) {
            ((byte *)vec_expected)[j] += MAKE_HEX_C(Z_REGISTER_DIFFERENCE_BASE());
        }
    }
#        if defined(__ARM_FEATURE_SVE)
    /* Predicate registers are 1/8 the size of the vector registers. We check the vector
     * register 8 bytes at a time so we check the predicate registers 1 byte at a time.
     */
    byte ffr_expected[32] = {
#            define E(n)                                               \
                SIMD_UNIQUE_VAL_BYTE_ELEMENT(MAKE_HEX_C(FFR_0_BASE()), \
                                             MAKE_HEX_C(P_ELEMENT_INCREMENT_BASE()), n)
        E(0),  E(1),  E(2),  E(3),  E(4),  E(5),  E(6),  E(7),  E(8),  E(9),  E(10),
        E(11), E(12), E(13), E(14), E(15), E(16), E(17), E(18), E(19), E(20), E(21),
        E(22), E(23), E(24), E(25), E(26), E(27), E(28), E(29), E(30), E(31),
#            undef E
    };
    const byte *ffr =
        (byte *)(xsp + gprs_on_stack + (num_vector_registers * simd_sz_in_ptrs));
    for (uint element = 0; element < simd_sz_in_ptrs; element++) {
        check_simd_value("ffr", 0, "d", element, ffr[element], ffr_expected[element]);
    }

    byte pred_expected[32] = {
#            define E(n)                                              \
                SIMD_UNIQUE_VAL_BYTE_ELEMENT(MAKE_HEX_C(P0_0_BASE()), \
                                             MAKE_HEX_C(P_ELEMENT_INCREMENT_BASE()), n)
        E(0),  E(1),  E(2),  E(3),  E(4),  E(5),  E(6),  E(7),  E(8),  E(9),  E(10),
        E(11), E(12), E(13), E(14), E(15), E(16), E(17), E(18), E(19), E(20), E(21),
        E(22), E(23), E(24), E(25), E(26), E(27), E(28), E(29), E(30), E(31),
#            undef E
    };
    const byte *predicate_reg_start = ffr + 32;
    for (i = 0; i < 16; i++) {
        const byte *predicate_reg =
            predicate_reg_start + (i * (vector_length_in_bytes / 8));
        for (uint element = 0; element < simd_sz_in_ptrs; element++) {
            check_simd_value("p", i, "d", element, predicate_reg[element],
                             pred_expected[element]);
        }

        /* Increment each byte to generate the expected value for the next register. */
        for (size_t j = 0; j < vector_length_in_bytes; j++) {
            pred_expected[j] += MAKE_HEX_C(P_REGISTER_DIFFERENCE_BASE());
        }
    }
#        endif
#    else
#        error NYI /* TODO i#3160: Add 32-bit support. */
#    endif
}

#    if defined(X86) || defined(AARCH64)
void
check_status_reg(ptr_uint_t *xsp)
{
#        if defined(X86)
    check_gpr_value("eflags", *xsp, MAKE_HEX_C(XFLAGS_BASE()));
#        elif defined(AARCH64)
    check_gpr_value("nzcv", *xsp, MAKE_HEX_C(NZCV_BASE()));
    check_gpr_value("fpcr", *(xsp + 1), MAKE_HEX_C(FPCR_BASE()));
    check_gpr_value("fpsr", *(xsp + 2), MAKE_HEX_C(FPSR_BASE()));
#        endif
}
#    endif

#    ifdef X86
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
#    if defined(X86) || defined(AARCH64)
    test_thread_func(thread_check_gprs_from_DR1);
    test_thread_func(thread_check_gprs_from_DR2);

    test_thread_func(thread_check_status_reg_from_cache);
    test_thread_func(thread_check_status_reg_from_DR);
#    endif

#    ifdef X86 /* TODO i#4698: Port to AArch64. */
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
#if defined(__ARM_FEATURE_SVE)
#    define PUSHALL \
        /* p0 does not keep its pre-push value */ @N@\
        addpl    sp, sp, #-16 @N@\
        str      p15, [sp, #15, mul vl] @N@\
        str      p14, [sp, #14, mul vl] @N@\
        str      p13, [sp, #13, mul vl] @N@\
        str      p12, [sp, #12, mul vl] @N@\
        str      p11, [sp, #11, mul vl] @N@\
        str      p10, [sp, #10, mul vl] @N@\
        str      p9, [sp, #9, mul vl] @N@\
        str      p8, [sp, #8, mul vl] @N@\
        str      p7, [sp, #7, mul vl] @N@\
        str      p6, [sp, #6, mul vl] @N@\
        str      p5, [sp, #5, mul vl] @N@\
        str      p4, [sp, #4, mul vl] @N@\
        str      p3, [sp, #3, mul vl] @N@\
        str      p2, [sp, #2, mul vl] @N@\
        str      p1, [sp, #1, mul vl] @N@\
        str      p0, [sp, #0, mul vl] @N@\
        sub      sp, sp, #32 @N@\
        rdffr    p0.b @N@ \
        str      p0, [sp, #0, mul vl] @N@\
        addvl    sp, sp, #-32 @N@\
        str      z31, [sp, #31, mul vl] @N@\
        str      z30, [sp, #30, mul vl] @N@\
        str      z29, [sp, #29, mul vl] @N@\
        str      z28, [sp, #28, mul vl] @N@\
        str      z27, [sp, #27, mul vl] @N@\
        str      z26, [sp, #26, mul vl] @N@\
        str      z25, [sp, #25, mul vl] @N@\
        str      z24, [sp, #24, mul vl] @N@\
        str      z23, [sp, #23, mul vl] @N@\
        str      z22, [sp, #22, mul vl] @N@\
        str      z21, [sp, #21, mul vl] @N@\
        str      z20, [sp, #20, mul vl] @N@\
        str      z19, [sp, #19, mul vl] @N@\
        str      z18, [sp, #18, mul vl] @N@\
        str      z17, [sp, #17, mul vl] @N@\
        str      z16, [sp, #16, mul vl] @N@\
        str      z15, [sp, #15, mul vl] @N@\
        str      z14, [sp, #14, mul vl] @N@\
        str      z13, [sp, #13, mul vl] @N@\
        str      z12, [sp, #12, mul vl] @N@\
        str      z11, [sp, #11, mul vl] @N@\
        str      z10, [sp, #10, mul vl] @N@\
        str      z9, [sp, #9, mul vl] @N@\
        str      z8, [sp, #8, mul vl] @N@\
        str      z7, [sp, #7, mul vl] @N@\
        str      z6, [sp, #6, mul vl] @N@\
        str      z5, [sp, #5, mul vl] @N@\
        str      z4, [sp, #4, mul vl] @N@\
        str      z3, [sp, #3, mul vl] @N@\
        str      z2, [sp, #2, mul vl] @N@\
        str      z1, [sp, #1, mul vl] @N@\
        str      z0, [sp, #0, mul vl] @N@\
        PUSH_GPRS
#else /* defined(__ARM_FEATURE_SVE) */
#    define PUSHALL \
        stp      q30, q31, [sp, #-32]! @N@\
        stp      q28, q29, [sp, #-32]! @N@\
        stp      q26, q27, [sp, #-32]! @N@\
        stp      q24, q25, [sp, #-32]! @N@\
        stp      q22, q23, [sp, #-32]! @N@\
        stp      q20, q21, [sp, #-32]! @N@\
        stp      q18, q19, [sp, #-32]! @N@\
        stp      q16, q17, [sp, #-32]! @N@\
        stp      q14, q15, [sp, #-32]! @N@\
        stp      q12, q13, [sp, #-32]! @N@\
        stp      q10, q11, [sp, #-32]! @N@\
        stp      q8, q9, [sp, #-32]! @N@\
        stp      q6, q7, [sp, #-32]! @N@\
        stp      q4, q5, [sp, #-32]! @N@\
        stp      q2, q3, [sp, #-32]! @N@\
        stp      q0, q1, [sp, #-32]! @N@\
        PUSH_GPRS
#endif /* defined(__ARM_FEATURE_SVE) */
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
#if defined(__ARM_FEATURE_SVE)
#    define POPALL \
        POP_GPRS @N@\
        ldr      z31, [sp, #31, mul vl] @N@\
        ldr      z30, [sp, #30, mul vl] @N@\
        ldr      z29, [sp, #29, mul vl] @N@\
        ldr      z28, [sp, #28, mul vl] @N@\
        ldr      z27, [sp, #27, mul vl] @N@\
        ldr      z26, [sp, #26, mul vl] @N@\
        ldr      z25, [sp, #25, mul vl] @N@\
        ldr      z24, [sp, #24, mul vl] @N@\
        ldr      z23, [sp, #23, mul vl] @N@\
        ldr      z22, [sp, #22, mul vl] @N@\
        ldr      z21, [sp, #21, mul vl] @N@\
        ldr      z20, [sp, #20, mul vl] @N@\
        ldr      z19, [sp, #19, mul vl] @N@\
        ldr      z18, [sp, #18, mul vl] @N@\
        ldr      z17, [sp, #17, mul vl] @N@\
        ldr      z16, [sp, #16, mul vl] @N@\
        ldr      z15, [sp, #15, mul vl] @N@\
        ldr      z14, [sp, #14, mul vl] @N@\
        ldr      z13, [sp, #13, mul vl] @N@\
        ldr      z12, [sp, #12, mul vl] @N@\
        ldr      z11, [sp, #11, mul vl] @N@\
        ldr      z10, [sp, #10, mul vl] @N@\
        ldr      z9, [sp, #9, mul vl] @N@\
        ldr      z8, [sp, #8, mul vl] @N@\
        ldr      z7, [sp, #7, mul vl] @N@\
        ldr      z6, [sp, #6, mul vl] @N@\
        ldr      z5, [sp, #5, mul vl] @N@\
        ldr      z4, [sp, #4, mul vl] @N@\
        ldr      z3, [sp, #3, mul vl] @N@\
        ldr      z2, [sp, #2, mul vl] @N@\
        ldr      z1, [sp, #1, mul vl] @N@\
        ldr      z0, [sp, #0, mul vl] @N@\
        /* We need to add 32*vector_length_in_bytes to sp but addvl has a maximum */ @N@\
        /* immediate value of 31 so we have to split it into two instructions. */ @N@\
        addvl    sp, sp, #31 @N@\
        addvl    sp, sp, #1 @N@\
        ldr      p0, [sp] @N@\
        wrffr    p0.b @N@\
        add      sp, sp, #32 @N@\
        ldr      p15, [sp, #15, mul vl] @N@\
        ldr      p14, [sp, #14, mul vl] @N@\
        ldr      p13, [sp, #13, mul vl] @N@\
        ldr      p12, [sp, #12, mul vl] @N@\
        ldr      p11, [sp, #11, mul vl] @N@\
        ldr      p10, [sp, #10, mul vl] @N@\
        ldr      p9, [sp, #9, mul vl] @N@\
        ldr      p8, [sp, #8, mul vl] @N@\
        ldr      p7, [sp, #7, mul vl] @N@\
        ldr      p6, [sp, #6, mul vl] @N@\
        ldr      p5, [sp, #5, mul vl] @N@\
        ldr      p4, [sp, #4, mul vl] @N@\
        ldr      p3, [sp, #3, mul vl] @N@\
        ldr      p2, [sp, #2, mul vl] @N@\
        ldr      p1, [sp, #1, mul vl] @N@\
        ldr      p0, [sp, #0, mul vl] @N@\
        addpl    sp, sp, #16
#else /* defined(__ARM_FEATURE_SVE) */
#    define POPALL \
        POP_GPRS @N@\
        ldp      q0, q1, [sp, #32]! @N@\
        ldp      q2, q3, [sp, #32]! @N@\
        ldp      q4, q5, [sp, #32]! @N@\
        ldp      q6, q7, [sp, #32]! @N@\
        ldp      q8, q9, [sp, #32]! @N@\
        ldp      q10, q11, [sp, #32]! @N@\
        ldp      q12, q13, [sp, #32]! @N@\
        ldp      q14, q15, [sp, #32]! @N@\
        ldp      q16, q17, [sp, #32]! @N@\
        ldp      q18, q19, [sp, #32]! @N@\
        ldp      q20, q21, [sp, #32]! @N@\
        ldp      q22, q23, [sp, #32]! @N@\
        ldp      q24, q25, [sp, #32]! @N@\
        ldp      q26, q27, [sp, #32]! @N@\
        ldp      q28, q29, [sp, #32]! @N@\
        ldp      q30, q31, [sp, #32]!
#endif /* defined(__ARM_FEATURE_SVE) */
#    define POP_CALLEE_SAVED \
        ldp      x19, x20, [sp], #16 @N@\
        ldp      x21, x22, [sp], #16 @N@\
        ldp      x23, x24, [sp], #16 @N@\
        ldp      x25, x26, [sp], #16 @N@\
        ldp      x27, x28, [sp], #16 @N@\
        ldp      x29, x30, [sp], #16
#elif defined(RISCV64)
    /* XXX i#3544: Not implemented */
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

#    define SET_GPR_UNIQUE_REGISTER_VALS \
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

#if defined (__ARM_FEATURE_SVE)

# define SET_Z_PATTERN(reg, start) \
        movz     w0, start @N@\
        movz     w1, MAKE_HEX_ASM(Z_ELEMENT_INCREMENT_BASE()) @N@\
        index    reg.b, w0, w1 @N@\
/* It is not possible to move arbitrary patterns into P registers directly so we go via
 * memory.
 */
# define SET_P_PATTERN(reg, start) \
        movz     w0, start @N@\
        movz     w1, MAKE_HEX_ASM(P_ELEMENT_INCREMENT_BASE()) @N@\
        index    z0.b, w0, w1 @N@\
        str      z0, [sp] @N@\
        ldr      reg, [sp]

/* We can't set LR in a callee so we inline.
 * SVE vector and predicate register lengths are immplementation defined. To keep the
 * implementation of SET_UNIQUE_REGISTER_VALS simple, it is designed to be vector length
 * agnostic and uses the index instruction to generate the unique values.
 * index takes a start value and an increment and generates an arithmetic progression in
 * the destination register:
 *    Zn.T[0] = start
 *    Zn.T[x] = Zn.T[x - 1] + increment
 */
#    define SET_UNIQUE_REGISTER_VALS \
        addvl    sp, sp, #-1 @N@\
        SET_P_PATTERN(p0, MAKE_HEX_ASM(FFR_0_BASE())) @N@\
        wrffr    p0.b @N@\
        SET_P_PATTERN(p0, MAKE_HEX_ASM(P0_0_BASE())) @N@\
        SET_P_PATTERN(p1, MAKE_HEX_ASM(P1_0_BASE())) @N@\
        SET_P_PATTERN(p2, MAKE_HEX_ASM(P2_0_BASE())) @N@\
        SET_P_PATTERN(p3, MAKE_HEX_ASM(P3_0_BASE())) @N@\
        SET_P_PATTERN(p4, MAKE_HEX_ASM(P4_0_BASE())) @N@\
        SET_P_PATTERN(p5, MAKE_HEX_ASM(P5_0_BASE())) @N@\
        SET_P_PATTERN(p6, MAKE_HEX_ASM(P6_0_BASE())) @N@\
        SET_P_PATTERN(p7, MAKE_HEX_ASM(P7_0_BASE())) @N@\
        SET_P_PATTERN(p8, MAKE_HEX_ASM(P8_0_BASE())) @N@\
        SET_P_PATTERN(p9, MAKE_HEX_ASM(P9_0_BASE())) @N@\
        SET_P_PATTERN(p10, MAKE_HEX_ASM(P10_0_BASE())) @N@\
        SET_P_PATTERN(p11, MAKE_HEX_ASM(P11_0_BASE())) @N@\
        SET_P_PATTERN(p12, MAKE_HEX_ASM(P12_0_BASE())) @N@\
        SET_P_PATTERN(p13, MAKE_HEX_ASM(P13_0_BASE())) @N@\
        SET_P_PATTERN(p14, MAKE_HEX_ASM(P14_0_BASE())) @N@\
        SET_P_PATTERN(p15, MAKE_HEX_ASM(P15_0_BASE())) @N@\
        addvl    sp, sp, #1 @N@\
        SET_Z_PATTERN(z0, MAKE_HEX_ASM(Z0_0_BASE())) @N@\
        SET_Z_PATTERN(z1, MAKE_HEX_ASM(Z1_0_BASE())) @N@\
        SET_Z_PATTERN(z2, MAKE_HEX_ASM(Z2_0_BASE())) @N@\
        SET_Z_PATTERN(z3, MAKE_HEX_ASM(Z3_0_BASE())) @N@\
        SET_Z_PATTERN(z4, MAKE_HEX_ASM(Z4_0_BASE())) @N@\
        SET_Z_PATTERN(z5, MAKE_HEX_ASM(Z5_0_BASE())) @N@\
        SET_Z_PATTERN(z6, MAKE_HEX_ASM(Z6_0_BASE())) @N@\
        SET_Z_PATTERN(z7, MAKE_HEX_ASM(Z7_0_BASE())) @N@\
        SET_Z_PATTERN(z8, MAKE_HEX_ASM(Z8_0_BASE())) @N@\
        SET_Z_PATTERN(z9, MAKE_HEX_ASM(Z9_0_BASE())) @N@\
        SET_Z_PATTERN(z10, MAKE_HEX_ASM(Z10_0_BASE())) @N@\
        SET_Z_PATTERN(z11, MAKE_HEX_ASM(Z11_0_BASE())) @N@\
        SET_Z_PATTERN(z12, MAKE_HEX_ASM(Z12_0_BASE())) @N@\
        SET_Z_PATTERN(z13, MAKE_HEX_ASM(Z13_0_BASE())) @N@\
        SET_Z_PATTERN(z14, MAKE_HEX_ASM(Z14_0_BASE())) @N@\
        SET_Z_PATTERN(z15, MAKE_HEX_ASM(Z15_0_BASE())) @N@\
        SET_Z_PATTERN(z16, MAKE_HEX_ASM(Z16_0_BASE())) @N@\
        SET_Z_PATTERN(z17, MAKE_HEX_ASM(Z17_0_BASE())) @N@\
        SET_Z_PATTERN(z18, MAKE_HEX_ASM(Z18_0_BASE())) @N@\
        SET_Z_PATTERN(z19, MAKE_HEX_ASM(Z19_0_BASE())) @N@\
        SET_Z_PATTERN(z20, MAKE_HEX_ASM(Z20_0_BASE())) @N@\
        SET_Z_PATTERN(z21, MAKE_HEX_ASM(Z21_0_BASE())) @N@\
        SET_Z_PATTERN(z22, MAKE_HEX_ASM(Z22_0_BASE())) @N@\
        SET_Z_PATTERN(z23, MAKE_HEX_ASM(Z23_0_BASE())) @N@\
        SET_Z_PATTERN(z24, MAKE_HEX_ASM(Z24_0_BASE())) @N@\
        SET_Z_PATTERN(z25, MAKE_HEX_ASM(Z25_0_BASE())) @N@\
        SET_Z_PATTERN(z26, MAKE_HEX_ASM(Z26_0_BASE())) @N@\
        SET_Z_PATTERN(z27, MAKE_HEX_ASM(Z27_0_BASE())) @N@\
        SET_Z_PATTERN(z28, MAKE_HEX_ASM(Z28_0_BASE())) @N@\
        SET_Z_PATTERN(z29, MAKE_HEX_ASM(Z29_0_BASE())) @N@\
        SET_Z_PATTERN(z30, MAKE_HEX_ASM(Z30_0_BASE())) @N@\
        SET_Z_PATTERN(z31, MAKE_HEX_ASM(Z31_0_BASE())) @N@\
        SET_GPR_UNIQUE_REGISTER_VALS
# else
#define V0_ELEMENT(n) SIMD_UNIQUE_VAL_DOUBLEWORD_ELEMENT(MAKE_HEX_C(Z0_0_BASE()), MAKE_HEX_C(Z_ELEMENT_INCREMENT_BASE()), n)
    /* We can't set LR in a callee so we inline: */
#    define SET_UNIQUE_REGISTER_VALS \
        /* Create a pattern that mimics the SVE index instruction we use to */ @N@\
        /* generate Z register patterns. */ @N@\
        SET_GPR_IMMED(x0, V0_ELEMENT(0)) @N@\
        SET_GPR_IMMED(x1, V0_ELEMENT(8)) @N@\
        mov      v0.d[0], x0 @N@\
        mov      v0.d[1], x1 @N@\
        movi     v31.16b, MAKE_HEX_ASM(Z_REGISTER_DIFFERENCE_BASE()) @N@\
        add      v1.16b, v0.16b, v31.16b @N@\
        add      v2.16b, v1.16b, v31.16b @N@\
        add      v3.16b, v2.16b, v31.16b @N@\
        add      v4.16b, v3.16b, v31.16b @N@\
        add      v5.16b, v4.16b, v31.16b @N@\
        add      v6.16b, v5.16b, v31.16b @N@\
        add      v7.16b, v6.16b, v31.16b @N@\
        add      v8.16b, v7.16b, v31.16b @N@\
        add      v9.16b, v8.16b, v31.16b @N@\
        add      v10.16b, v9.16b, v31.16b @N@\
        add      v11.16b, v10.16b, v31.16b @N@\
        add      v12.16b, v11.16b, v31.16b @N@\
        add      v13.16b, v12.16b, v31.16b @N@\
        add      v14.16b, v13.16b, v31.16b @N@\
        add      v15.16b, v14.16b, v31.16b @N@\
        add      v16.16b, v15.16b, v31.16b @N@\
        add      v17.16b, v16.16b, v31.16b @N@\
        add      v18.16b, v17.16b, v31.16b @N@\
        add      v19.16b, v18.16b, v31.16b @N@\
        add      v20.16b, v19.16b, v31.16b @N@\
        add      v21.16b, v20.16b, v31.16b @N@\
        add      v22.16b, v21.16b, v31.16b @N@\
        add      v23.16b, v22.16b, v31.16b @N@\
        add      v24.16b, v23.16b, v31.16b @N@\
        add      v25.16b, v24.16b, v31.16b @N@\
        add      v26.16b, v25.16b, v31.16b @N@\
        add      v27.16b, v26.16b, v31.16b @N@\
        add      v28.16b, v27.16b, v31.16b @N@\
        add      v29.16b, v28.16b, v31.16b @N@\
        add      v30.16b, v29.16b, v31.16b @N@\
        add      v31.16b, v30.16b, v31.16b @N@\
        SET_GPR_UNIQUE_REGISTER_VALS
#undef E
#endif /*defined (__ARM_FEATURE_SVE) */


#elif defined(RISCV64)
    /* XXX i#3544: Not implemented */
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
    /* XXX i#3544: Not implemented */
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
        mov      REG_SCRATCH1, 0xffffffff /* no regs changed */
        CALLC2(GLOBAL_REF(check_gpr_vals), REG_SCRATCH0, REG_SCRATCH1)
        POPALL
        POP_CALLEE_SAVED
        UNALIGN_STACK_ON_FUNC_EXIT
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#if defined(X86)
#define MAKE_WRITEABLE \
        call     LOCAL_LABEL(retaddr) @N@\
LOCAL_LABEL(retaddr): @N@\
        pop      REG_XAX @N@\
        CALLC1(GLOBAL_REF(make_mem_writable), REG_XAX)

#define SELFMOD_INIT(reg) \
        mov      reg, HEX(0)

#define SELFMOD(counter_reg, addr_reg) \
        inc      counter_reg @N@\
        lea      addr_reg, SYMREF(LOCAL_LABEL(immed_plus_four) - 4) @N@\
        mov      DWORD [addr_reg], counter_reg /* selfmod write */ @N@\
        mov      addr_reg, HEX(0)              /* mov_imm to modify */ @N@\
ADDRTAKEN_LABEL(LOCAL_LABEL(immed_plus_four:))

/* Define two versions of the SELFMOD macros which use disjoint registers.
 * This allows us to create two versions of thread_check_gprs_from_DR() which clobber
 * different registers so between them they achieve coverage of all registers.
 */
#define SELFMOD_INIT1 SELFMOD_INIT(eax)
#define SELFMOD1 SELFMOD(eax, REG_XDX)
#define SELFMOD_GPR_MASK1 ~((1 << 7) | (1 << 5))

#define SELFMOD_INIT2 SELFMOD_INIT(ebx)
#define SELFMOD2 SELFMOD(ebx, REG_XCX)
#define SELFMOD_GPR_MASK2 ~((1 << 6) | (1 << 4))
#endif

#if defined(AARCH64)
#define MAKE_WRITEABLE \
        adr      REG_SCRATCH0, 0 @N@\
        CALLC1(GLOBAL_REF(make_mem_writable), REG_SCRATCH0)

/* Clean a single cache line covering the address in addr_reg.
 * We can't call tool_clear_icache() because that would disturb the register state
 * and interfere with the test. Instead we need to use this sequence of instructions
 * from Arm ARM B2.7.4.2:
 */
#define CLEAN_CACHE_LINE(addr_reg) \
        dc       cvau, addr_reg @N@\
        dsb      ish @N@\
        ic       ivau, addr_reg @N@\
        dsb      ish @N@\
        isb

/* Generate the encoding of:
 *      movz      w(scratch_reg_num), #0
 */
#define SELFMOD_INIT(reg, scratch_reg_num) \
        movz reg, @P@scratch_reg_num @N@\
        movk reg, @P@0x5280, lsl @P@16

/* Self-modifying code that increments the immediate field in a movz instruction. */
#define SELFMOD(counter_reg, addr_reg, scratch_reg32) \
        /* Extract the immedate field from the instr. */ @N@\
        ubfx     scratch_reg32, counter_reg, @P@5, @P@16 @N@\
        /* Increment the immediate value. */ @N@\
        add      scratch_reg32, scratch_reg32, @P@1 @N@\
        /* Put the modified immed value back in. */ @N@\
        bfi      counter_reg, scratch_reg32, @P@5, @P@16 @N@\
        adr      addr_reg, GLOBAL_REF(LOCAL_LABEL(instr_to_modify)) @N@\
        /* And write the instruction back to memory. */ @N@\
        str      counter_reg, [addr_reg] @N@\
        CLEAN_CACHE_LINE(addr_reg) @N@\
ADDRTAKEN_LABEL(LOCAL_LABEL(instr_to_modify:)) @N@\
        movz     scratch_reg32, @P@0          /* This instruction is modified. */

/* Define two versions of the SELFMOD macros which use disjoint registers.
 * This allows us to create two versions of thread_check_gprs_from_DR() which clobber
 * different registers so between them they achieve coverage of all registers.
 */
#define SELFMOD_INIT1 SELFMOD_INIT(w2, 0)
#define SELFMOD1 SELFMOD(w2, x0, w0)
#define SELFMOD_GPR_MASK1 (~((1 << 0) | (1 << 2)))

#define SELFMOD_INIT2 SELFMOD_INIT(w5, 6)
#define SELFMOD2 SELFMOD(w5, x6, w6)
#define SELFMOD_GPR_MASK2 (~((1 << 5) | (1 << 6)))
#endif

#if defined(X86) || defined(AARCH64)

/* Any changes to this function should be replicated in thread_check_gprs_from_DR2 below.
 */
#define FUNCNAME thread_check_gprs_from_DR1
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        ALIGN_STACK_ON_FUNC_ENTRY
        /* Preserve callee-saved state. */
        PUSH_CALLEE_SAVED
        /* Make code writable for selfmod below */
        MAKE_WRITEABLE
        /* Put in unique values. */
        SET_UNIQUE_REGISTER_VALS
        /* Signal that we are ready for a detach. */
        SET_SIDELINE_READY
        /* Now modify our own code so we're likely to detach from DR.
         * The DR code's changed state means we're more likely to see
         * errors in restoring the app state.
         */
        SELFMOD_INIT1
check_gprs_from_DR1_spin:
        SELFMOD1
        CHECK_SIDELINE_EXIT
        JUMP_NOT_EQUAL check_gprs_from_DR1_spin
        PUSHALL
        mov      REG_SCRATCH0, REG_SP
        mov      REG_SCRATCH1, SELFMOD_GPR_MASK1
        CALLC2(GLOBAL_REF(check_gpr_vals), REG_SCRATCH0, REG_SCRATCH1)
        POPALL
        POP_CALLEE_SAVED
        UNALIGN_STACK_ON_FUNC_EXIT
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

/*
 * This is identical to thread_check_gprs_from_DR1 except that the SELDMOD loop clobbers
 * different registers. By running both tests we get coverage of all GPRs.
 */
#define FUNCNAME thread_check_gprs_from_DR2
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        ALIGN_STACK_ON_FUNC_ENTRY
        /* Preserve callee-saved state. */
        PUSH_CALLEE_SAVED
        /* Make code writable for selfmod below */
        MAKE_WRITEABLE
        /* Put in unique values. */
        SET_UNIQUE_REGISTER_VALS
        /* Signal that we are ready for a detach. */
        SET_SIDELINE_READY
        /* Now modify our own code so we're likely to detach from DR.
         * The DR code's changed state means we're more likely to see
         * errors in restoring the app state.
         */
        SELFMOD_INIT2
check_gprs_from_DR2_spin:
        SELFMOD2
        CHECK_SIDELINE_EXIT
        JUMP_NOT_EQUAL check_gprs_from_DR2_spin
        PUSHALL
        mov      REG_SCRATCH0, REG_SP
        mov      REG_SCRATCH1, SELFMOD_GPR_MASK2
        CALLC2(GLOBAL_REF(check_gpr_vals), REG_SCRATCH0, REG_SCRATCH1)
        POPALL
        POP_CALLEE_SAVED
        UNALIGN_STACK_ON_FUNC_EXIT
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#if defined(X86)
#define PUSH_STATUS_REG \
        PUSHF

#define POP_STATUS_REG \
        pop      REG_XAX

#define SET_UNIQUE_STATUS_REG_VALS \
        mov      REG_XAX, MAKE_HEX_ASM(XFLAGS_BASE()) @N@\
        push     REG_XAX @N@\
        POPF

#define CHECK_SIDELINE_EXIT_WITHOUT_USING_FLAGS(loop_label) \
        mov      cl, BYTE SYMREF(sideline_exit) @N@\
        jecxz    loop_label

#elif defined(AARCH64)
#define PUSH_STATUS_REG \
        mrs      x0, fpsr @N@\
        stp      x0, x0, [sp, #-16]! @N@\
        mrs      x0, nzcv @N@\
        mrs      x1, fpcr @N@\
        stp      x0, x1, [sp, #-16]!

#define POP_STATUS_REG \
        add      sp, sp, #32

#define SET_UNIQUE_STATUS_REG_VALS \
        mov      x0, MAKE_HEX_ASM(NZCV_BASE()) @N@\
        msr      nzcv, x0 @N@\
        SET_GPR_IMMED(x0, MAKE_HEX_C(FPCR_BASE())) @N@\
        msr      fpcr, x0 @N@\
        SET_GPR_IMMED(x0, MAKE_HEX_C(FPSR_BASE())) @N@\
        msr      fpsr, x0

#define CHECK_SIDELINE_EXIT_WITHOUT_USING_FLAGS(loop_label) \
        adrp     x0, sideline_exit @N@ \
        add      x0, x0, :lo12:sideline_exit @N@ \
        ldrb     w0, [x0] @N@\
        cbz      x0, loop_label
#endif

#define FUNCNAME thread_check_status_reg_from_cache
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        ALIGN_STACK_ON_FUNC_ENTRY
        /* Preserve callee-saved state. */
        PUSH_CALLEE_SAVED
        /* Put in a unique value. */
        SET_UNIQUE_STATUS_REG_VALS
        /* Signal that we are ready for a detach. */
        SET_SIDELINE_READY
        /* Now spin so we'll detach from the code cache. */
check_status_reg_from_cache_spin:
        CHECK_SIDELINE_EXIT_WITHOUT_USING_FLAGS(check_status_reg_from_cache_spin)
        PUSH_STATUS_REG
        mov      REG_SCRATCH0, REG_SP
        CALLC1(GLOBAL_REF(check_status_reg), REG_SCRATCH0)
        POP_STATUS_REG
        POP_CALLEE_SAVED
        UNALIGN_STACK_ON_FUNC_EXIT
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME thread_check_status_reg_from_DR
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        ALIGN_STACK_ON_FUNC_ENTRY
        /* Preserve callee-saved state. */
        PUSH_CALLEE_SAVED
        /* Make code writable for selfmod below */
        MAKE_WRITEABLE
        /* Put in a unique value. */
        SET_UNIQUE_STATUS_REG_VALS
        /* Signal that we are ready for a detach. */
        SET_SIDELINE_READY
        /* Now modify our own code so we're likely to detach from DR.
         * The DR code's changed state means we're more likely to see
         * errors in restoring the app state.
         */
        SELFMOD_INIT1
check_status_reg_from_DR_spin:
        SELFMOD1
        CHECK_SIDELINE_EXIT_WITHOUT_USING_FLAGS(check_status_reg_from_cache_spin)
        PUSH_STATUS_REG
        mov      REG_SCRATCH0, REG_SP
        CALLC1(GLOBAL_REF(check_status_reg), REG_SCRATCH0)
        POP_STATUS_REG
        POP_CALLEE_SAVED
        UNALIGN_STACK_ON_FUNC_EXIT
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#endif /* defined(X86) || defined(AARCH64) */

#if defined(X86)

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
