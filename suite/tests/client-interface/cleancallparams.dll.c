/* **********************************************************
 * Copyright (c) 2017 ARM Limited. All rights reserved.
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

/* Marshalling the arguments to a clean call is non-trivial when the arguments
 * are passed in registers and the values in some registers need to be permuted
 * (and we wish to do this efficiently). This test checks some interesting cases.
 *
 * It makes no difference what app is used. The first basic block encountered will
 * be instrumented with a sequence of clean calls followed by a call via finish()
 * to dr_exit_process(), so no app code is executed.
 *
 * This test can easily be ported to a new architecture but only tests the
 * marshalling of arguments. The test "cleancall" tests some other aspects of
 * clean calling.
 */

#include <stdarg.h>

#include "dr_api.h"
#include "client_tools.h"

#define MAX_NUM_ARGS 2

/* Some registers that we will use for testing. The registers used for
 * parameter passing should come first, in the right order, as some
 * test cases are designed to be interesting when the registers are
 * numbered in this way.
 */
static reg_id_t regs[] = {
#if defined(AARCH64)
    DR_REG_X0, DR_REG_X1, DR_REG_X2, DR_REG_X3,
    DR_REG_X4, DR_REG_X5, DR_REG_X6, DR_REG_X7,
    /* end of parameter registers */
    DR_REG_X8, DR_REG_X30
#elif defined(ARM)
    DR_REG_R0, DR_REG_R1, DR_REG_R2, DR_REG_R3,
    /* end of parameter registers */
    DR_REG_R4, DR_REG_R14
#elif defined(X86_32)
    /* no parameter registers */
    DR_REG_EAX, DR_REG_ECX, DR_REG_EDX, DR_REG_EBX
#elif defined(X86_64)
# ifdef UNIX
    DR_REG_RDI, DR_REG_RSI, DR_REG_RDX, DR_REG_RCX, DR_REG_R8, DR_REG_R9,
    /* end of parameter registers */
    DR_REG_R10, DR_REG_R11
# else
    DR_REG_RCX, DR_REG_RDX, DR_REG_R8, DR_REG_R9,
    /* end of parameter registers */
    DR_REG_R10, DR_REG_R11, DR_REG_RDI, DR_REG_RSI
# endif
#else
# error NYI
#endif
};

#define NUM_REGS (sizeof(regs) / sizeof(*regs))

/* Table of test cases.
 * Positive values represent registers, with the parameters being 1, 2, 3,...
 * Zero represents zero. Other negative values represent other constant values.
 * It is not an error to use a register number that is too large for the current
 * architecture as such values are automatically converted to constants.
 */
static const struct {
    signed char num_args;
    signed char args[MAX_NUM_ARGS];
} tests[] = {
    { 1, { -1 } },
    { 1, { -2 } },
    { 1, { 1 } },
    { 1, { 2 } },
    { 2, { -1, -2 } },
    { 2, { 1, 2 } },
#ifdef X86
    /* FIXME i#2210: Make this case work on ARM/AArch64, and add more cases. */
    { 2, { 2, 1 } }, /* Swap first two parameter registers. */
#endif
};

#define NUM_TESTS (sizeof(tests) / sizeof(*tests))

static const ptr_uint_t constants[] = {
#ifdef X64
    0x1234567890abcdef, 0xabcdef0123456789
#else
    0x12abcdef, 0xabcdef12
#endif
};

#define NUM_CONSTANTS (sizeof(constants) / sizeof(*constants))

#define REG_BASE_VAL 0x100

static void
fail(const char *s)
{
    dr_printf("Error: %s\n", s);
    dr_exit_process(1);
}

static ptr_uint_t
convert_arg_to_int(int x)
{
    if (x < 0) {
        if (-x > NUM_CONSTANTS)
            fail("bad constant");
        return constants[-x - 1];
    } else if (x > 0) {
        return REG_BASE_VAL + x;
    } else
        return 0;
}

static opnd_t
convert_arg_to_opnd(int x)
{
    if (0 < x && x <= NUM_REGS)
        return opnd_create_reg(regs[x - 1]);
    else
        return opnd_create_immed_uint(convert_arg_to_int(x), OPSZ_PTR);
}

/****************************************************************************
 * Functions called from fragment cache.
 */

static int call_count = 0; /* the number of calls we have seen */

static void
callee(int num_args, ...)
{
    va_list ap;
    int i;

    if (call_count >= NUM_TESTS)
        fail("too many calls");
    if (num_args != tests[call_count].num_args) {
        dr_printf("Wrong number of args for call %d: expected %d, saw %d\n",
                  call_count + 1, tests[call_count].num_args, num_args);
        fail("wrong number of args");
    }
    va_start(ap, num_args);
    for (i = 0; i < num_args; i++) {
        ptr_uint_t expected = convert_arg_to_int(tests[call_count].args[i]);
        ptr_uint_t seen = va_arg(ap, ptr_uint_t);
        if (expected != seen) {
            dr_printf("Wrong value for call %d, arg %d: expected %llx, saw %llx\n",
                      call_count + 1, i + 1, expected, seen);
            fail("wrong value");
        }
    }
    ++call_count;
}

static void
finish(void)
{
    if (call_count != NUM_TESTS)
        fail("missing calls");
    dr_printf("Finished\n");
    dr_exit_process(0);
}

/* We tediously define callee_N for N = 0, 1,...  because an architecture may
 * use an entirely different calling convention for variadic functions than for
 * non-variadic functions.
 */

static void
callee_0(void)
{
    callee(0);
}

static void
callee_1(ptr_uint_t a0)
{
    callee(1, a0);
}

static void
callee_2(ptr_uint_t a0, ptr_uint_t a1)
{
    callee(2, a0, a1);
}

static void *
callee_n(int n)
{
    ASSERT(n <= MAX_NUM_ARGS);
    switch (n) {
    case 0: return (void *)callee_0;
    case 1: return (void *)callee_1;
    case 2: return (void *)callee_2;
    }
    return NULL;
}

/****************************************************************************
 * Instrumentation.
 */

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating)
{
    instr_t *where = instrlist_first_app(bb);
    int i, j;

    /* Initialise registers. */
    for (i = 0; i < NUM_REGS; i++) {
        instrlist_insert_mov_immed_ptrsz(drcontext, REG_BASE_VAL + i + 1,
                                         opnd_create_reg(regs[i]),
                                         bb, where, NULL, NULL);
    }

    /* Insert clean calls */
    for (i = 0; i < NUM_TESTS; i++) {
        int num_args = tests[i].num_args;
        opnd_t args[MAX_NUM_ARGS];
        ASSERT(num_args <= MAX_NUM_ARGS);
        for (j = 0; j < num_args; j++)
            args[j] = convert_arg_to_opnd(tests[i].args[j]);
        /* Update the following two statements if the value of MAX_NUM_ARGS is changed. */
        ASSERT(MAX_NUM_ARGS == 2);
        dr_insert_clean_call_ex(drcontext, bb, where, callee_n(num_args), false, num_args,
                                args[0], args[1]);
    }

    /* Exit now. We do not run the app. */
    dr_insert_clean_call(drcontext, bb, where, (void *)finish, false, 0);

    return DR_EMIT_DEFAULT;
}

DR_EXPORT void
dr_init(client_id_t id)
{
    /* register events */
    dr_register_bb_event(event_basic_block);
}
