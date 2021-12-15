/* **********************************************************
 * Copyright (c) 2015-2021 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef ASM_CODE_ONLY /* C code */
#    include "tools.h"
#    include "thread.h"
#    include "condvar.h"
#    include <setjmp.h>
#    include <signal.h>

/***************************************************************************
 * Test atomic inc/dec using ldstex.
 * Strategy: we make two threads, one incrementing and the other decrementing.
 * With the same iteration count and no races, at the end we should have
 * the same value.
 */

#    define NUM_COUNTER_ITERS 10000
#    define GLOBAL_COUNTER_VALUE 42
volatile int global_counter = GLOBAL_COUNTER_VALUE;
static void *ready_for_incdec;

void
ldstex_inc(volatile int *counter);
void
ldstex_dec(volatile int *counter);

THREAD_FUNC_RETURN_TYPE
thread_do_inc(void *arg)
{
    wait_cond_var(ready_for_incdec);
    for (int i = 0; i < NUM_COUNTER_ITERS; ++i)
        ldstex_inc(&global_counter);
    return THREAD_FUNC_RETURN_ZERO;
}

THREAD_FUNC_RETURN_TYPE
thread_do_dec(void *arg)
{
    wait_cond_var(ready_for_incdec);
    for (int i = 0; i < NUM_COUNTER_ITERS; ++i)
        ldstex_dec(&global_counter);
    return THREAD_FUNC_RETURN_ZERO;
}

static void
test_atomic_incdec(void)
{
    ready_for_incdec = create_cond_var();
    thread_t thread_inc = create_thread(thread_do_inc, NULL);
    thread_t thread_dec = create_thread(thread_do_dec, NULL);
    signal_cond_var(ready_for_incdec);
    join_thread(thread_inc);
    join_thread(thread_dec);
    destroy_cond_var(ready_for_incdec);
    if (global_counter != GLOBAL_COUNTER_VALUE)
        print("ERROR: race in ldstex atomic inc/dec\n");
}

/***************************************************************************
 * Test stolen register use.
 */

void
ldstex_inc6x_stolen(int *counter);

static void
test_stolen_reg(void)
{
    int my_var = 42;
    ldstex_inc6x_stolen(&my_var);
    if (my_var != 48)
        print("Error in ldstex_inc6x_stolen: %d\n", my_var);
}

/***************************************************************************
 * Test different opcodes and region shapes.
 */

typedef struct {
    int counter1, counter2;
} __attribute__((__aligned__(IF_ARM_ELSE(8, 16)))) two_counters_t;

typedef struct {
    long counter1, counter2;
} __attribute__((__aligned__(IF_ARM_ELSE(8, 16)))) two_counters64_t;

int
ldstex_inc_pair(two_counters_t *counters);
int
ldstex_inc_half(short *counter);
int
ldstex_inc_byte(char *counter);
int
ldstex_inc_shapes(int *counter);

#    ifdef AARCH64
int
ldstex_inc32_with_xzr(two_counters_t *counters);
int
ldstex_inc64_with_xzr(two_counters64_t *counters);
#    endif

static void
test_shapes(void)
{
    two_counters_t my_var = { 42, 117 };
    int added = ldstex_inc_pair(&my_var);
    if (my_var.counter1 != 42 + added || my_var.counter2 != 117 + added)
        print("Error in ldstex_inc_pair: %d %d\n", my_var.counter1, my_var.counter2);
    short half_ctr = 42;
    added = ldstex_inc_half(&half_ctr);
    if (half_ctr != 42 + added)
        print("Error in ldstex_inc_half: %d\n", half_ctr);

    char byte_ctr = 42;
    added = ldstex_inc_byte(&byte_ctr);
    if (byte_ctr != 42 + added)
        print("Error in ldstex_inc_byte: %d\n", byte_ctr);

    int ctr = 42;
    added = ldstex_inc_shapes(&ctr);
    if (ctr != 42 + added)
        print("Error in ldstex_inc_shapes: %d\n", ctr);

#    ifdef AARCH64
    my_var.counter2 = 117;
    added = ldstex_inc32_with_xzr(&my_var);
    /* We zero both and only the 2nd's add sticks. */
    if (my_var.counter1 != 0 || my_var.counter2 != added) {
        print("Error in ldstex_inc32_with_xzr: %d %d\n", my_var.counter1,
              my_var.counter2);
    }
    two_counters64_t my_var64 = { 42, 117 };
    added = ldstex_inc64_with_xzr(&my_var64);
    if (my_var64.counter1 != 0 || my_var64.counter2 != added) {
        print("Error in ldstex_inc64_with_xzr: %ld %ld\n", my_var64.counter1,
              my_var64.counter2);
    }
#    endif
}

/***************************************************************************
 * Test faults in ldstex regions.
 */

static SIGJMP_BUF mark;
static int count;

static void
handle_signal(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    print("Got signal %d; count %d\n", signal, count);
    count++;
    if (count == 1) { /* First fault in ldstex_fault_stex(). */
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        /* Ensure that DR restored the spilled r0 value. */
        if (sc->SC_R0 != 0)
            print("Error: r0 was not restored to 0: %p\n", sc->SC_R0);
        /* Re-execute with a safe base. */
        sc->SC_R0 = sc->SC_R5;
        sc->SC_R1 = sc->SC_R5;
        return;
    } else if (count == 2) { /* Second fault in ldstex_fault_stex(). */
        sigcontext_t *sc = SIGCXT_FROM_UCXT(ucxt);
        /* Ensure that DR restored the spilled r0 value. */
        if (sc->SC_R0 != 0)
            print("Error: r0 was not restored to 0: %p\n", sc->SC_R0);
        /* Re-execute with a safe base. */
        sc->SC_R0 = sc->SC_R5;
        sc->IF_ARM_ELSE(SC_R10, SC_R28) = sc->SC_R5;
        return;
    }
    SIGLONGJMP(mark, count);
}

void
ldstex_fault_stex(int *counter);
void
ldstex_fault_ldex(int *counter);
void
ldstex_fault_between(int *counter);

static void
test_faults(void)
{
    intercept_signal(SIGILL, (handler_3_t)&handle_signal, false);
    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal, false);
    int my_val = 42;
    /* The precise order and content is relied upon in handle_signal(). */
    if (SIGSETJMP(mark) == 0)
        ldstex_fault_stex(&my_val);
    if (SIGSETJMP(mark) == 0)
        ldstex_fault_ldex(&my_val);
    if (SIGSETJMP(mark) == 0)
        ldstex_fault_between(&my_val);
}

/***************************************************************************
 * Test clrex handling.
 */

void
ldstex_clrex(int *counter);

static void
test_clrex(void)
{
    int my_var = 42;
    ldstex_clrex(&my_var);
    if (my_var != 42)
        print("Error in ldstex_clrex: %d\n", my_var);
}

/***************************************************************************
 * Main.
 */

int
main(int argc, const char *argv[])
{
    /* Run twice: on the first run, the client will insert clean calls, which will test
     * that we're avoiding infinite loops (from all the inserted memory operations) as
     * well as often thwarting same-block optimizations due to register writes in the
     * clean calls.  So we run a second time where the client avoids inserting anything
     * (the client flushes in between to obtain new blocks) to test the same-block
     * optimization path.
     */
    for (int i = 0; i < 2; i++) {
        test_atomic_incdec();
        test_stolen_reg();
        test_shapes();
        test_faults();
        test_clrex();
        /* Notify client to flush and change modes. */
        NOP;
        NOP;
        NOP;
        NOP;
        count = 0; /* Reset for test_faults(). */
    }
    print("Test finished\n");
    return 0;
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
/* clang-format off */
START_FILE

/* TODO i#1698: Add T32-mode versions of the test code.
 * T32 exclusive loads seem to SIGILL on my test machine though!
 * For now it's all A32 mode.  It's written to compile either way.
 */
/* TODO i#1698: Add ARM predication tests. */

/* void ldstex_inc(int *counter) */
#define FUNCNAME ldstex_inc
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef AARCH64
      1:
        ldaxr    w1, [x0]
        add      w2, w1, #0x1
        stxr     w3, w2, [x0]
        cbnz     w3, 1b
        ret
#elif defined(ARM)
      1:
        ldaex    r1, [r0]
        add      r2, r1, #0x1
        strex    r3, r2, [r0]
        cmp      r3, #0
        bne      1b
        bx       lr
#else
#    error Unsupported arch
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

/* void ldstex_dec(int *counter) */
#define FUNCNAME ldstex_dec
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef AARCH64
      1:
        ldaxr    w1, [x0]
        sub      w2, w1, #0x1
        stxr     w3, w2, [x0]
        cbnz     w3, 1b
        ret
#elif defined(ARM)
      1:
        ldaex    r1, [r0]
        sub      r2, r1, #0x1
        strex    r3, r2, [r0]
        cmp      r3, #0
        bne      1b
        bx       lr
#else
#    error Unsupported arch
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

/* void ldstex_inc6x_stolen(int *counter) */
/* We have a check in the client main that x28 and r10 are the stolen regs. */
#define FUNCNAME ldstex_inc6x_stolen
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef AARCH64
        stp      x28, x29, [sp, #-32]!
        /* First, use the stolen reg as the base reg. */
        mov      x28, x0
      1:
        ldaxr    w1, [x28]
        add      w2, w1, #0x1
        stxr     w3, w2, [x28]
        cbnz     w3, 1b
        /* Next, use the stolen reg as the value reg. */
      2:
        ldaxr    w28, [x0]
        /* Clobbering the value reg forces save-restore mangling: that's ok. */
        add      w28, w28, #0x1
        stxr     w3, w28, [x0]
        cbnz     w3, 2b
        /* Finally, use the stolen reg as the status reg. */
      3:
        ldaxr    w1, [x0]
        add      w2, w1, #0x1
        stxr     w28, w2, [x0]
        cbnz     w28, 3b
        /* Now repeat with ldex and stex in separate blocks. */
        mov      x28, x0
      4:
        ldaxr    w1, [x28]
        add      w2, w1, #0x1
        cbz      w2, 5f
        stxr     w3, w2, [x28]
        cbnz     w3, 4b
      5:
        ldaxr    w28, [x0]
        add      w28, w28, #0x1
        cbz      w28, 6f
        stxr     w3, w28, [x0]
        cbnz     w3, 5b
      6:
        ldaxr    w1, [x0]
        add      w2, w1, #0x1
        cbz      w2, 7f
        stxr     w28, w2, [x0]
        cbnz     w28, 6b
      7:
        ldp      x28, x29, [sp], #32
        ret
#elif defined(ARM)
        push     {r10}
        /* First, use the stolen reg as the base reg. */
        mov      r10, r0
      1:
        ldaex    r1, [r10]
        add      r2, r1, #0x1
        strex    r3, r2, [r10]
        cmp      r3, #0
        bne      1b
        /* Next, use the stolen reg as the value reg. */
      2:
        ldaex    r10, [r0]
        add      r10, r10, #0x1
        strex    r3, r10, [r0]
        cmp      r3, #0
        bne      2b
        /* Finally, use the stolen reg as the status reg. */
      3:
        ldaex    r1, [r0]
        add      r2, r1, #0x1
        strex    r10, r2, [r0]
        cmp      r10, #0
        bne      3b
        /* Now repeat with ldex and stex in separate blocks. */
        mov      r10, r0
      4:
        ldaex    r1, [r10]
        add      r2, r1, #0x1
        cmp      r2, #0
        beq      5f
        strex    r3, r2, [r10]
        cmp      r3, #0
        bne      4b
      5:
        ldaex    r10, [r0]
        add      r10, r10, #0x1
        cmp      r0, #0
        beq      6f
        strex    r3, r10, [r0]
        cmp      r3, #0
        bne      5b
      6:
        ldaex    r1, [r0]
        add      r2, r1, #0x1
        cmp      r2, #0
        beq      7f
        strex    r10, r2, [r0]
        cmp      r10, #0
        bne      6b
      7:
        pop      {r10}
        bx       lr
#else
#    error Unsupported arch
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

/* int ldstex_inc_pair(two_counters_t *counters) */
#define FUNCNAME ldstex_inc_pair
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef AARCH64
      1:
        ldaxp    w1, w2, [x0]
        add      w4, w1, #0x1
        add      w5, w2, #0x1
        stlxp    w3, w4, w5, [x0]
        cbnz     w3, 1b
      2:
        ldxp     w1, w2, [x0]
        add      w1, w1, #0x1
        add      w2, w2, #0x1
        stxp     w3, w1, w2, [x0]
        cbnz     w3, 2b
        /* Test pair4-single8. */
      3:
        ldxp     w1, w2, [x0]
        add      w1, w1, #0x1
        add      w2, w2, #0x1
        orr      x1, x1, x2, lsl #32
        stxr     w3, x1, [x0]
        cbnz     w3, 3b
        /* Test single8-pair4. */
      4:
        ldxr     x1, [x0]
        lsr      x2, x1, #32
        add      w1, w1, #0x1
        add      w2, w2, #0x1
        stxp     w3, w1, w2, [x0]
        cbnz     w3, 4b
        /* Test store-res == load-dest (i#5247). */
      5:
        ldaxp    w1, w2, [x0]
        add      w4, w1, #0x1
        add      w5, w2, #0x1
        stlxp    w1, w4, w5, [x0]
        cbnz     w1, 5b
      6:
        ldaxp    w2, w1, [x0] /* Test the other order too. */
        add      w4, w2, #0x1
        add      w5, w1, #0x1
        stlxp    w1, w4, w5, [x0]
        cbnz     w1, 6b
      7:
        stp      x28, x29, [sp, #-16]!
        ldaxp    w2, w28, [x0] /* Test stolen reg. */
        add      w4, w2, #0x1
        add      w5, w28, #0x1
        stlxp    w28, w4, w5, [x0]
        cbnz     w28, 7b
        ldp      x28, x29, [sp], #16
        mov      w0, #7
        ret
#elif defined(ARM)
      1:
        ldaexd   r2, r3, [r0]
        add      r2, r2, #0x1
        add      r3, r3, #0x1
        stlexd   r1, r2, r3, [r0]
        cmp      r1, #0
        bne      1b
      2:
        ldrexd   r2, r3, [r0]
        add      r2, r2, #0x1
        add      r3, r3, #0x1
        strexd   r1, r2, r3, [r0]
        cmp      r1, #0
        bne      2b
        /* Test store-res == load-dest (i#5247). */
        push     {r4, r5, r10, r11}
      3:
        ldaexd   r2, r3, [r0]
        add      r4, r2, #0x1
        add      r5, r3, #0x1
        stlexd   r2, r4, r5, [r0]
        cmp      r2, #0
        bne      3b
      4:
        ldaexd   r10, r11, [r0] /* Test stolen reg. */
        add      r4, r10, #0x1
        add      r5, r11, #0x1
        stlexd   r10, r4, r5, [r0]
        cmp      r10, #0
        bne      4b
        /* ARM pairs must be in order so we can't re-order. */
        pop      {r4, r5, r10, r11}
        mov      r0, #4
        bx       lr
#else
#    error Unsupported arch
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

/* void ldstex_inc_half(short *counter) */
#define FUNCNAME ldstex_inc_half
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef AARCH64
      1:
        ldaxrh   w1, [x0]
        add      w2, w1, #0x1
        stxrh    w3, w2, [x0]
        cbnz     w3, 1b
        mov      w0, #1
        ret
#elif defined(ARM)
      1:
        ldaexh   r1, [r0]
        add      r2, r1, #0x1
        strexh   r3, r2, [r0]
        cmp      r3, #0
        bne      1b
        mov      r0, #1
        bx       lr
#else
#    error Unsupported arch
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

/* void ldstex_inc_byte(short *counter) */
#define FUNCNAME ldstex_inc_byte
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef AARCH64
      1:
        ldaxrb   w1, [x0]
        add      w2, w1, #0x1
        stxrb    w3, w2, [x0]
        cbnz     w3, 1b
        mov      w0, #1
        ret
#elif defined(ARM)
      1:
        ldaexb   r1, [r0]
        add      r2, r1, #0x1
        strexb   r3, r2, [r0]
        cmp      r3, #0
        bne      1b
        mov      r0, #1
        bx       lr
#else
#    error Unsupported arch
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

/* int ldstex_inc_shapes(int *counter) */
#define FUNCNAME ldstex_inc_shapes
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef AARCH64
        /* Include many branches, including direct branches, that make it difficult
         * for simple static transformations to handle this.
         */
      1:
        ldaxr    w1, [x0]
        add      w2, w1, #0x1
        cbz      w2, 4f  /* Never taken. */
        cbnz     w2, 2f  /* Always taken. */
        add      w3, w4, w5 /* Never reached.  Use a lot of low registers. */
        b        4f      /* Never reached. */
      2:
        cbz      w2, 4f  /* Never taken. */
        cbnz     w2, 3f  /* Always taken. */
        b        4f      /* Never reached. */
      3:
        cbz      w2, 4f  /* Never taken. */
        nop
        cbz      w2, 4f  /* Never taken. */
        nop
        cbz      w2, 4f  /* Never taken. */
        nop
        stlxr    w3, w2, [x0]
        cbnz     w3, 1b
      4:
        /* Test unpaired cases and sp as a base. */
        sub      sp, sp, #16
        ldxr     x2, [sp]
        clrex
        stxr     w1, x0, [sp]
        stxr     w1, x0, [sp]
        stxr     w1, x0, [sp]
        /* Test wrong sizes paired.
         * On some processors, if the stxr's address range is a subset of the ldxp's
         * range, it will succeed.  However, the manual states that this is CONSTRAINED
         * UNPREDICTABLE behavior: B2.9.5 says "software can rely on a LoadExcl /
         * StoreExcl pair to eventually succeed only if the LoadExcl and the StoreExcl
         * have the same transaction size."  Similarly for the target VA and reg count.
         * Thus, given the complexity of trying to match the actual processor behavior
         * and comparing ranges and whatnot, we're ok with DR enforcing a strict
         * equality, until or unless we see real apps relying on processor quirks.
         * That means that while this ldxp;stxr might succeed natively on some processors
         * (symptoms: "Error in ldstex_inc_shapes: 43"), it will fail under DR and
         * our test will pass.
         */
        ldxp     x1, x2, [sp]
        stxr     w3, x1, [sp]
        cbnz     w3, 5f
        mov      w0, #8 /* Should never come here; this will fail caller. */
        add      sp, sp, #16
        ret
      5:
        add      sp, sp, #16
        ldaxr    w1, [x0]
        ldaxr    w1, [x0]
      6:
        ldaxr    w1, [x0]
        add      w2, w1, #0x1
        stlxr    w3, w2, [x0]
        cbnz     w3, 6b
        mov      w0, #2
        ret
#elif defined(ARM)
      1:
        ldaex    r1, [r0]
        add      r2, r1, #0x1
        cmp      r2, #0
        beq      4f  /* Never taken. */
        cmp      r2, #0
        bne      2f  /* Always taken. */
        add      r3, r4, r5 /* Never reached.  Use a lot of low registers. */
        b        4f      /* Never reached. */
      2:
        cmp      r2, #0
        beq      4f  /* Never taken. */
        cmp      r2, #0
        bne      3f  /* Always taken. */
        b        4f      /* Never reached. */
      3:
        cmp      r2, #0
        beq      4f  /* Never taken. */
        nop
        cmp      r2, #0
        beq      4f  /* Never taken. */
        nop
        cmp      r2, #0
        beq      4f  /* Never taken. */
        nop
        stlex    r3, r2, [r0]
        cmp      r3, #0
        bne      1b
      4:
        /* Test unpaired cases and sp as a base. */
        sub      sp, sp, #16
        ldrex    r2, [sp]
        clrex
        strex    r1, r0, [sp]
        strex    r1, r0, [sp]
        strex    r1, r0, [sp]
        /* Test wrong sizes paired.
         * See comment above about the unpredictability of behavior here.
         */
        ldrexd   r2, r3, [sp]
        strex    r3, r2, [sp]
        cmp      r3, #0
        bne      5f
        mov      r0, #8 /* Should never come here; this will fail caller. */
        bx       lr
      5:
        add      sp, sp, #16
        ldaex    r1, [r0]
        ldaex    r1, [r0]
      6:
        ldaex    r1, [r0]
        add      r2, r1, #0x1
        stlex    r3, r2, [r0]
        cmp      r3, #0
        bne      6b
        mov      r0, #2
        bx       lr
#else
#    error Unsupported arch
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

/* void ldstex_fault_stex(int *counter) */
#define FUNCNAME ldstex_fault_stex
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef AARCH64
        /* Test spilled register restores.
         * These first two re-execute the fault.
         * We don't use x0 in stxr to ensure DR will use it as the scratch.
         * We can't just want the counter address in there b/c DR will happen
         * to put it there for the base equality check, so we have more logic
         * with 0 in x0 and the address we want in x5.
         */
        mov      x5, x0
        mov      x1, #0
        /* Place repeat point below the zeroing of the base so the 2nd iter's
         * strex will not fault and thus we are not in danger of spinning.
         */
      1:
        ldaxr    w3, [x0]
        mov      x0, #0
        /* Split blocks to ensure scratch regs are used. */
        cbz      x5, 2f
        stxr     w4, w3, [x1]
        cbnz     w4, 1b
        /* Repeat but with stolen registers in there. */
        mov      x0, x5
        mov      x28, #0
      2:
        ldaxr    w3, [x0]
        mov      x0, #0
        cbz      x5, 3f
        stxr     w4, w3, [x28]
        cbnz     w4, 2b
        /* Fault by changing the base and thus failing our checks. */
      3:
        ldaxr    w1, [x0]
        add      w2, w1, #0x1
        mov      x0, #0
        stxr     w3, w2, [x0]
        cbnz     w3, 3b
        ret
#elif defined(ARM)
        /* See comments above. */
        mov      r5, r0
        mov      r1, #0
      1:
        ldaex    r3, [r0]
        mov      r0, #0
        cmp      r5, #0
        beq      2f
        strex    r4, r3, [r1]
        cmp      r4, #0
        bne      1b
        /* Repeat but rith stolen registers in there. */
        mov      r0, r5
        mov      r10, #0
      2:
        ldaex    r3, [r0]
        mov      r0, #0
        cmp      r5, #0
        beq      3f
        strex    r4, r3, [r10]
        cmp      r4, #0
        bne      2b
        /* Fault by changing the base and thus failing our checks. */
      3:
        ldaex    r1, [r0]
        add      r2, r1, #0x1
        mov      r0, #0
        strex    r3, r2, [r0]
        cmp      r3, #0
        bne      3b
        bx       lr
#else
#    error Unsupported arch
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

/* void ldstex_fault_ldex(int *counter) */
#define FUNCNAME ldstex_fault_ldex
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef AARCH64
      1:
        mov      x0, #0
        ldaxr    w1, [x0]
        add      w2, w1, #0x1
        stxr     w3, w2, [x0]
        cbnz     w3, 1b
        ret
#elif defined(ARM)
      1:
        mov      r0, #0
        ldaex    r1, [r0]
        add      r2, r1, #0x1
        strex    r3, r2, [r0]
        cmp      r3, #0
        bne      1b
        bx       lr
#else
#    error Unsupported arch
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

/* void ldstex_fault_between(int *counter) */
#define FUNCNAME ldstex_fault_between
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef AARCH64
      1:
        ldaxr    w1, [x0]
        add      w2, w1, #0x1
        .word    0 /* udf */
        stxr     w3, w2, [x0]
        cbnz     w3, 1b
        ret
#elif defined(ARM)
      1:
        ldaex    r1, [r0]
        add      r2, r1, #0x1
        udf      #0
        strex    r3, r2, [r0]
        cmp      r3, #0
        bne      1b
        bx       lr
#else
#    error Unsupported arch
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

END_FILE

/* void ldstex_clrex(int *counter) */
#define FUNCNAME ldstex_clrex
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* Try 10x to do atomic inc w/ clrex in there.  All should fail. */
#ifdef AARCH64
        /* Single bb. */
        mov      w4, #10
      1:
        ldaxr    w1, [x0]
        add      w2, w1, #0x1
        sub      w4, w4, #0x1
        clrex
        stxr     w3, w2, [x0]
        cbz      w4, 2f
        cbnz     w3, 1b
      2:
        /* Multi-bb. */
        mov      w4, #10
      3:
        ldaxr    w1, [x0]
        add      w2, w1, #0x1
        sub      w4, w4, #0x1
        clrex
        cbz      x0, 4f
        stxr     w3, w2, [x0]
        cbz      w4, 4f
        cbnz     w3, 3b
      4:
        ret
#elif defined(ARM)
        /* Single bb. */
        mov      r4, #10
      1:
        ldaex    r1, [r0]
        add      r2, r1, #0x1
        sub      r4, r4, #0x1
        clrex
        strex    r3, r2, [r0]
        cmp      r4, #0
        beq      2f
        cmp      r3, #0
        bne      1b
      2:
        /* Multi-bb. */
        mov      r4, #10
      3:
        ldaex    r1, [r0]
        add      r2, r1, #0x1
        sub      r4, r4, #0x1
        clrex
        cmp      r0, #0
        beq      4f
        strex    r3, r2, [r0]
        cmp      r4, #0
        beq      4f
        cmp      r3, #0
        bne      3b
      4:
        bx       lr
#else
#    error Unsupported arch
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#ifdef AARCH64
/* int ldstex_inc32_with_xzr(two_counters_t *counter)
 */
#define FUNCNAME ldstex_inc32_with_xzr
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* The clean call version thwarts the single-block optimized mangling,
         * so we do not need to make separate-block versions of these as we
         * have tests of both the fastpath and slowpath.
         */
      1:
        ldaxr    wzr, [x0]
        stlxr    w3, wzr, [x0]
        cbnz     w3, 1b
        str      x0, [x0] /* Ensure we'd loop forever w/o i#5245 on next test. */
      2:
        ldaxrh   wzr, [x0]
        stlxrh   w3, wzr, [x0]
        cbnz     w3, 2b
        str      x0, [x0] /* Ensure we'd loop forever w/o i#5245 on next test. */
      3:
        ldaxrb   wzr, [x0]
        stlxrb   w3, wzr, [x0]
        cbnz     w3, 3b
        str      x0, [x0] /* Ensure we'd loop forever w/o i#5245 on next test. */
        /* Test each LDAXP dest being xzr (both raises SIGILL). */
      4:
        ldaxp    w1, wzr, [x0]
        add      w2, w1, #0x1
        stlxp    w3, w2, wzr, [x0]
        cbnz     w3, 4b
      5:
        ldaxp    wzr, w1, [x0]
        add      w2, w1, #0x1
        stlxp    w3, wzr, w2, [x0]
        cbnz     w3, 5b
        mov      w0, #1
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME
/* int ldstex_inc64_with_xzr(two_counters64_t *counter)
 */
#define FUNCNAME ldstex_inc64_with_xzr
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* Test each LDAXP dest being xzr (both raises SIGILL). */
      1:
        ldaxp    x1, xzr, [x0]
        add      x2, x1, #0x1
        stlxp    w3, x2, xzr, [x0]
        cbnz     w3, 1b
      2:
        ldaxp    xzr, x1, [x0]
        add      x2, x1, #0x1
        stlxp    w3, xzr, x2, [x0]
        cbnz     w3, 2b
        mov      w0, #1
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME
#endif

END_FILE

/* clang-format on */
#endif
