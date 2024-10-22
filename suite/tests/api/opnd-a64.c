/* **********************************************************
 * Copyright (c) 2018 - 2024 Arm Limited.  All rights reserved.
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

/* This file contains unit tests for the APIs exported from opnd.h. */

#include "configure.h"
#include "dr_api.h"
#include <stdio.h>
#include <string.h>

#define ASSERT(x)                                                                        \
    ((void)((!(x)) ? (fprintf(stderr, "ASSERT FAILURE: %s:%d: %s\n", __FILE__, __LINE__, \
                              #x),                                                       \
                      abort(), 0)                                                        \
                   : 0))

static void
test_get_size()
{
    /* Check sizes of special registers. */
    ASSERT(reg_get_size(DR_REG_WZR) == OPSZ_4);
    ASSERT(reg_get_size(DR_REG_XZR) == OPSZ_8);
    ASSERT(reg_get_size(DR_REG_SP) == OPSZ_8);
    ASSERT(reg_get_size(DR_REG_XSP) == OPSZ_8);

    // Check sizes of GPRs.
    for (uint i = 0; i < DR_NUM_GPR_REGS; i++) {
        ASSERT(reg_get_size((reg_id_t)DR_REG_W0 + i) == OPSZ_4);
        ASSERT(reg_get_size((reg_id_t)DR_REG_X0 + i) == OPSZ_8);
    }

    // Check sizes of FP/SIMD regs.
    for (int i = 0; i < proc_num_simd_registers(); i++) {
        if (i < MCXT_NUM_SIMD_SVE_SLOTS) {
            ASSERT(reg_get_size((reg_id_t)DR_REG_H0 + i) == OPSZ_2);
            ASSERT(reg_get_size((reg_id_t)DR_REG_S0 + i) == OPSZ_4);
            ASSERT(reg_get_size((reg_id_t)DR_REG_D0 + i) == OPSZ_8);
            ASSERT(reg_get_size((reg_id_t)DR_REG_Q0 + i) == OPSZ_16);
        }
    }

    opnd_size_t opsz_veclen = OPSZ_NA;  /* Length of a Z vector register in bytes */
    opnd_size_t opsz_predlen = OPSZ_NA; /* Length of a P predicate register in bytes */
    if (proc_has_feature(FEATURE_SVE)) {
        /* Check sizes of SVE vector and predicate registers. Read vector length
         * directly from hardware and compare with OPSZ_ value reg_get_size()
         * returns.
         */
        uint64 vl;
        /* Read vector length from SVE hardware. */
        asm(".inst 0x04bf5020\n" /* rdvl x0, #1 */
            "mov %0, x0"
            : "=r"(vl)
            :
            : "x0");
        opsz_veclen = opnd_size_from_bytes(vl);
        opsz_predlen = opnd_size_from_bytes(vl / 8);
    } else {
        /* Set vector length to 256 bits for unit tests on non-SVE hardware. */
        ASSERT(dr_get_vector_length() == 256);
        opsz_veclen = OPSZ_32;
        opsz_predlen = OPSZ_4;
    }
    for (uint i = 0; i < 32; i++) {
        ASSERT(reg_get_size((reg_id_t)DR_REG_Z0 + i) == opsz_veclen);
    }

    for (uint i = 0; i < 16; i++) {
        ASSERT(reg_get_size((reg_id_t)DR_REG_P0 + i) == opsz_predlen);
    }
}

static void
test_opnd_compute_address()
{
    dr_mcontext_t mc = { .size = sizeof(mc),
                         .flags = DR_MC_ALL,
                         .r0 = 256,
                         .r1 = 4,
                         .r2 = 8,
                         .r3 = -4,
                         .r4 = -8,
                         .xsp = 16 };

    opnd_t memref;
    app_pc loc;

    /* No shift or extend */

    // ldr w0, [sp]
    // 16 + 0 = 16
    memref = opnd_create_base_disp_aarch64(DR_REG_XSP, DR_REG_NULL, DR_EXTEND_UXTX, false,
                                           0, 0, OPSZ_4);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr w0, [sp, #4]
    // 16 + 4 = 20
    memref = opnd_create_base_disp_aarch64(DR_REG_XSP, DR_REG_NULL, DR_EXTEND_UXTX, false,
                                           4, 0, OPSZ_4);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr w0, [sp, #-4]
    // 16 - 4 = 12
    memref = opnd_create_base_disp_aarch64(DR_REG_XSP, DR_REG_NULL, DR_EXTEND_UXTX, false,
                                           -4, 0, OPSZ_4);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    /* Shift and extend: 32 bit variant */

    // ldr w0, [sp, w2, uxtw #0]
    // 16 + 8 = 24
    memref = opnd_create_base_disp_aarch64(DR_REG_XSP, DR_REG_W2, DR_EXTEND_UXTW, false,
                                           0, 0, OPSZ_4);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr w0, [sp, w2, uxtw #3]
    // 16 + (8 << 2) = 48
    memref = opnd_create_base_disp_aarch64(DR_REG_XSP, DR_REG_W2, DR_EXTEND_UXTW, true, 0,
                                           0, OPSZ_4);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr w0, [sp, x1, lsl #0]
    // 16 + 4 = 20
    memref = opnd_create_base_disp_aarch64(DR_REG_XSP, DR_REG_X1, DR_EXTEND_UXTX, false,
                                           0, 0, OPSZ_4);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr w0, [sp, x1, lsl #3]
    // 16 + (4 << 2) = 32
    memref = opnd_create_base_disp_aarch64(DR_REG_XSP, DR_REG_X1, DR_EXTEND_UXTX, true, 0,
                                           0, OPSZ_4);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr w0, [x0, w4, sxtw #0]
    // 256 - 8 = 248
    memref = opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_W4, DR_EXTEND_SXTW, false, 0,
                                           0, OPSZ_4);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr w0, [x0, w4, sxtw #3]
    // 256 - (8 << 2) = 224
    memref = opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_W4, DR_EXTEND_SXTW, true, 0,
                                           0, OPSZ_4);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr w0, [x0, w3, sxtx #0]
    // 256 - 4 = 252
    memref = opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_X3, DR_EXTEND_SXTX, false, 0,
                                           0, OPSZ_4);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr w0, [x0, w3, sxtx #3]
    // 256 - (4 << 2) = 240
    memref = opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_X3, DR_EXTEND_SXTX, true, 0,
                                           0, OPSZ_4);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    /* Shift and extend: 64 bit variant */

    // ldr x0, [sp, w2, uxtw #0]
    // 16 + 8 = 24
    memref = opnd_create_base_disp_aarch64(DR_REG_XSP, DR_REG_W2, DR_EXTEND_UXTW, false,
                                           0, 0, OPSZ_8);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr x0, [sp, w2, uxtw #3]
    // 16 + (8 << 3) = 80
    memref = opnd_create_base_disp_aarch64(DR_REG_XSP, DR_REG_W2, DR_EXTEND_UXTW, true, 0,
                                           0, OPSZ_8);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr x0, [sp, x1, lsl #0]
    // 16 + 4 = 20
    memref = opnd_create_base_disp_aarch64(DR_REG_XSP, DR_REG_X1, DR_EXTEND_UXTX, false,
                                           0, 0, OPSZ_8);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr x0, [sp, x1, lsl #3]
    // 16 + (4 << 3) = 48
    memref = opnd_create_base_disp_aarch64(DR_REG_XSP, DR_REG_X1, DR_EXTEND_UXTX, true, 0,
                                           0, OPSZ_8);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr x0, [x0, w4, sxtw #0]
    // 256 - 8 = 248
    memref = opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_W4, DR_EXTEND_SXTW, false, 0,
                                           0, OPSZ_8);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr x0, [x0, w4, sxtw #3]
    // 256 - (8 << 3) = 192
    memref = opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_W4, DR_EXTEND_SXTW, true, 0,
                                           0, OPSZ_8);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr x0, [x0, w3, sxtx #0]
    // 256 - 4 = 252
    memref = opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_X3, DR_EXTEND_SXTX, false, 0,
                                           0, OPSZ_8);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr x0, [x0, w3, sxtx #3]
    // 256 - (4 << 3) = 224
    memref = opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_X3, DR_EXTEND_SXTX, true, 0,
                                           0, OPSZ_8);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);
}

static void
test_opnd_invert_immed_int()
{
    // 1 bit test
    opnd_t opnd = opnd_invert_immed_int(opnd_create_immed_int(1, OPSZ_1b));
    printf("opnd size: %d, value: 0x%lx\n", opnd_size_in_bits(opnd_get_size(opnd)),
           opnd_get_immed_int(opnd));
    opnd = opnd_invert_immed_int(opnd_create_immed_int(0, OPSZ_1b));
    printf("opnd size: %d, value: 0x%lx\n", opnd_size_in_bits(opnd_get_size(opnd)),
           opnd_get_immed_int(opnd));

    // 3 bit test
    opnd = opnd_invert_immed_int(opnd_create_immed_int(0b001, OPSZ_3b));
    printf("opnd size: %d, value: 0x%lx\n", opnd_size_in_bits(opnd_get_size(opnd)),
           opnd_get_immed_int(opnd));
    opnd = opnd_invert_immed_int(opnd_create_immed_int(0b101, OPSZ_3b));
    printf("opnd size: %d, value: 0x%lx\n", opnd_size_in_bits(opnd_get_size(opnd)),
           opnd_get_immed_int(opnd));

    // 1 byte test
    opnd = opnd_invert_immed_int(opnd_create_immed_int(0x33, OPSZ_1));
    printf("opnd size: %d, value: 0x%lx\n", opnd_size_in_bits(opnd_get_size(opnd)),
           opnd_get_immed_int(opnd));
    opnd = opnd_invert_immed_int(opnd_create_immed_int(0xf0, OPSZ_1));
    printf("opnd size: %d, value: 0x%lx\n", opnd_size_in_bits(opnd_get_size(opnd)),
           opnd_get_immed_int(opnd));

    // 4 byte test
    opnd = opnd_invert_immed_int(opnd_create_immed_int(0x33333333, OPSZ_4));
    printf("opnd size: %d, value: 0x%lx\n", opnd_size_in_bits(opnd_get_size(opnd)),
           opnd_get_immed_int(opnd));
    opnd = opnd_invert_immed_int(opnd_create_immed_int(0xf0f0f0f0, OPSZ_4));
    printf("opnd size: %d, value: 0x%lx\n", opnd_size_in_bits(opnd_get_size(opnd)),
           opnd_get_immed_int(opnd));

// 8 byte test
#ifdef X64
    opnd = opnd_invert_immed_int(opnd_create_immed_int(0xf0f0f0f033333333, OPSZ_8));
    printf("opnd size: %d, value: 0x%lx\n", opnd_size_in_bits(opnd_get_size(opnd)),
           opnd_get_immed_int(opnd));
    opnd = opnd_invert_immed_int(opnd_create_immed_int(0x33333333f0f0f0f0, OPSZ_8));
    printf("opnd size: %d, value: 0x%lx\n", opnd_size_in_bits(opnd_get_size(opnd)),
           opnd_get_immed_int(opnd));
#else
    opnd = opnd_invert_immed_int(opnd_create_immed_int64(0xf0f0f0f033333333, OPSZ_8));
    printf("opnd size: %d, value: 0x%lx\n", opnd_size_in_bits(opnd_get_size(opnd)),
           opnd_get_immed_int64(opnd));
    opnd = opnd_invert_immed_int(opnd_create_immed_int64(0x33333333f0f0f0f0, OPSZ_8));
    printf("opnd size: %d, value: 0x%lx\n", opnd_size_in_bits(opnd_get_size(opnd)),
           opnd_get_immed_int64(opnd));
#endif
}

typedef struct _vector_address_test_expectation_t {
    app_pc *addresses;
    uint num_addresses;
    bool is_write;
} vector_address_test_expectation_t;

void
test_compute_vector_address_helper(void *drcontext, instr_t *instr, dr_mcontext_t *mc,
                                   const vector_address_test_expectation_t *expected,
                                   uint line)
{
    bool printed_instr = false;
#define TEST_FAILED()                                             \
    do {                                                          \
        if (!printed_instr) {                                     \
            printf("%s:%u:\n", __FILE__, line);                   \
            dr_print_instr(drcontext, STDOUT, instr,              \
                           "Failed to compute addresses for:\n"); \
            printed_instr = true;                                 \
        }                                                         \
    } while (0)

#define EXPECT_CMP(cmp, fmt, a, b)                                                   \
    do {                                                                             \
        if (!(a cmp b)) {                                                            \
            TEST_FAILED();                                                           \
            printf("Expected " #a " " #cmp " " #b ":\n    " #a " = " fmt "\n    " #b \
                   " = " fmt "\n",                                                   \
                   a, b);                                                            \
        }                                                                            \
    } while (0)

#define EXPECT_EQ(fmt, a, b) EXPECT_CMP(==, fmt, a, b)
#define EXPECT_LT(fmt, a, b) EXPECT_CMP(<, fmt, a, b)

    app_pc addr;
    bool is_write;
    uint index = 0;
    while (instr_compute_address_ex(instr, mc, index, &addr, &is_write)) {
        EXPECT_LT("%u", index, expected->num_addresses);
        EXPECT_EQ("%p", addr, expected->addresses[index]);
        EXPECT_EQ("%u", is_write, expected->is_write);
        index++;
    }
    EXPECT_EQ("%u", index, expected->num_addresses);

#undef TEST_FAILED
#undef EXPECT_CMP
#undef EXPECT_EQ
#undef EXPECT_LT
}

/* Used by test_compute_vector_address() to determine whether an instruction reads or
 * writes its memory operand and set test expectations.
 * This isn't an exhaustive list of opcodes; it just contains the ones used in the test
 */
static bool
op_is_write(int op)
{
    switch (op) {
    case OP_ld1b:
    case OP_ld1h:
    case OP_ld1w:
    case OP_ld1d:
    case OP_ldnt1b:
    case OP_ldnt1h:
    case OP_ldnt1w:
    case OP_ldnt1d: return false;
    case OP_st1b:
    case OP_st1h:
    case OP_st1w:
    case OP_st1d:
    case OP_stnt1b:
    case OP_stnt1h:
    case OP_stnt1w:
    case OP_stnt1d: return true;

    default: ASSERT(false);
    }
}

/* Used by test_compute_vector_address() to determine whether an instruction reads or
 * writes its memory operand and set test expectations.
 * This isn't an exhaustive list of opcodes; it just contains the ones used in the test
 */
static opnd_size_t
op_mem_size(int op)
{
    switch (op) {
    case OP_ld1b:
    case OP_ldnt1b:
    case OP_st1b:
    case OP_stnt1b: return OPSZ_1;
    case OP_ld1h:
    case OP_ldnt1h:
    case OP_st1h:
    case OP_stnt1h: return OPSZ_2;
    case OP_ld1w:
    case OP_ldnt1w:
    case OP_st1w:
    case OP_stnt1w: return OPSZ_4;
    case OP_ld1d:
    case OP_ldnt1d:
    case OP_st1d:
    case OP_stnt1d: return OPSZ_8;

    default: ASSERT(false);
    }
}

void
test_compute_vector_address(void *drcontext)
{
    const int original_vector_length = dr_get_vector_length();
    ASSERT(dr_set_vector_length(256));

#define SCALAR_BASE_REG 0

#define INDEX_REG_D 0
#define INDEX_REG_S 1
#define BASE_REG_D 2
#define BASE_REG_S 3

    dr_mcontext_t mc = {
        .size = sizeof(dr_mcontext_t),
        .flags = DR_MC_ALL,
        .r0 = 0x8000000000000000, /* SCALAR_BASE_REG */
        .r1 = 1,
        .r2 = 2,
        .r3 = 3,
        .r4 = 4,
        .r5 = 5,
        .r6 = 6,
        .r7 = 7,
        .r8 = 0xffffffffffffffff,
        .simd[INDEX_REG_D].u64 = { 0x0000000000010000, 0x0000000000020000,
                                   0xffffffffffff0000, 0xfffffffffffe0000 },
        .simd[INDEX_REG_S].u32 = { 0x00010000, 0x00020000, 0x00030000, 0x00040000,
                                   0xffff0000, 0xfffd0000, 0xfffc0000, 0xfffb0000 },
        .simd[BASE_REG_D].u64 = { 0x0000000000000000, 0x8000000000000000,
                                  0xffffffffffffffff, 0x0000000010000000 },
        .simd[BASE_REG_S].u32 = { 0x00000000, 0x80000000, 0xffffffff, 0x00010000,
                                  0x10000000, 0x20000000, 0x30000000, 0x40000000 },
    };

    for (size_t i = BASE_REG_S + 1; i < MCXT_NUM_SIMD_SVE_SLOTS; i++) {
        static const uint64 poison[4] = { 0xdeaddeaddeaddead, 0xdeaddeaddeaddead,
                                          0xdeaddeaddeaddead, 0xdeaddeaddeaddead };
        memcpy(&mc.simd[i].u64[0], poison, sizeof(poison));
    }
    for (size_t i = 0; i < MCXT_NUM_SVEP_SLOTS; i++) {
        mc.svep[i].u32[0] = 0xffffffff;
    }

/* Map SVE element sizes to opnd_size_t */
#define ELSZ_B OPSZ_1
#define ELSZ_H OPSZ_2
#define ELSZ_S OPSZ_4
#define ELSZ_D OPSZ_8

/* Declare expected test results.*/
#define EXPECT(...)                                \
    app_pc addresses[] = { __VA_ARGS__ };          \
    vector_address_test_expectation_t expected = { \
        addresses,                                 \
        sizeof(addresses) / sizeof(app_pc),        \
        false,                                     \
    }

#define VEC_ADDR_TEST(op, governing_pred_reg, mask, create_mem_opnd, decl_expect)        \
    {                                                                                    \
        decl_expect;                                                                     \
        expected.is_write = op_is_write(OP_##op);                                        \
        mc.svep[governing_pred_reg].u32[0] = mask;                                       \
        opnd_t mem_opnd = create_mem_opnd;                                               \
        opnd_set_size(&mem_opnd, op_mem_size(OP_##op));                                  \
        instr_t *instr = INSTR_CREATE_##op##_sve_pred(                                   \
            drcontext,                                                                   \
            opnd_create_reg_element_vector(DR_REG_Z31,                                   \
                                           opnd_get_vector_element_size(mem_opnd)),      \
            opnd_create_predicate_reg(DR_REG_P0 + governing_pred_reg, false), mem_opnd); \
        test_compute_vector_address_helper(drcontext, instr, &mc, &expected, __LINE__);  \
        instr_destroy(drcontext, instr);                                                 \
        mc.svep[governing_pred_reg].u32[0] = 0xffffffff;                                 \
    }

#define SCALAR_PLUS_VECTOR(xn, zm, el_size, extend, scale)                             \
    opnd_create_vector_base_disp_aarch64(DR_REG_X0 + xn, DR_REG_Z0 + zm, el_size,      \
                                         DR_EXTEND_##extend, scale > 0, 0, 0, OPSZ_NA, \
                                         scale)

    /* Test all the scalar+vector addressing modes.
     * The opcode used in the instruction shouldn't make a difference to the address
     * calculation, so these tests cover all addressing modes but not all
     * (opcode, addressing mode) combinations.
     */

    /* 32-bit scaled offset [<Xn|SP>, <Zm>.S, <mod> #N] */
    VEC_ADDR_TEST(ld1h, /*governing_pred_reg=*/0, 0x11111111,
                  SCALAR_PLUS_VECTOR(SCALAR_BASE_REG, INDEX_REG_S, ELSZ_S, UXTW, 1),
                  EXPECT((app_pc)0x8000000000020000, (app_pc)0x8000000000040000,
                         (app_pc)0x8000000000060000, (app_pc)0x8000000000080000,
                         (app_pc)0x80000001fffe0000, (app_pc)0x80000001fffa0000,
                         (app_pc)0x80000001fff80000, (app_pc)0x80000001fff60000));
    VEC_ADDR_TEST(st1h, /*governing_pred_reg=*/0, 0x11111111,
                  SCALAR_PLUS_VECTOR(SCALAR_BASE_REG, INDEX_REG_S, ELSZ_S, SXTW, 1),
                  EXPECT((app_pc)0x8000000000020000, (app_pc)0x8000000000040000,
                         (app_pc)0x8000000000060000, (app_pc)0x8000000000080000,
                         (app_pc)0x7ffffffffffe0000, (app_pc)0x7ffffffffffa0000,
                         (app_pc)0x7ffffffffff80000, (app_pc)0x7ffffffffff60000));
    VEC_ADDR_TEST(ld1w, /*governing_pred_reg=*/0, 0x11111111,
                  SCALAR_PLUS_VECTOR(SCALAR_BASE_REG, INDEX_REG_S, ELSZ_S, UXTW, 2),
                  EXPECT((app_pc)0x8000000000040000, (app_pc)0x8000000000080000,
                         (app_pc)0x80000000000c0000, (app_pc)0x8000000000100000,
                         (app_pc)0x80000003fffc0000, (app_pc)0x80000003fff40000,
                         (app_pc)0x80000003fff00000, (app_pc)0x80000003ffec0000));
    VEC_ADDR_TEST(st1w, /*governing_pred_reg=*/0, 0x11111111,
                  SCALAR_PLUS_VECTOR(SCALAR_BASE_REG, INDEX_REG_S, ELSZ_S, SXTW, 2),
                  EXPECT((app_pc)0x8000000000040000, (app_pc)0x8000000000080000,
                         (app_pc)0x80000000000c0000, (app_pc)0x8000000000100000,
                         (app_pc)0x7ffffffffffc0000, (app_pc)0x7ffffffffff40000,
                         (app_pc)0x7ffffffffff00000, (app_pc)0x7fffffffffec0000));

    /* 32-bit unscaled offset [<Xn|SP>, <Zm>.S, <mod>] */
    VEC_ADDR_TEST(ld1w, /*governing_pred_reg=*/1, 0x11111111,
                  SCALAR_PLUS_VECTOR(SCALAR_BASE_REG, INDEX_REG_S, ELSZ_S, UXTW, 0),
                  EXPECT((app_pc)0x8000000000010000, (app_pc)0x8000000000020000,
                         (app_pc)0x8000000000030000, (app_pc)0x8000000000040000,
                         (app_pc)0x80000000ffff0000, (app_pc)0x80000000fffd0000,
                         (app_pc)0x80000000fffc0000, (app_pc)0x80000000fffb0000));
    VEC_ADDR_TEST(st1w, /*governing_pred_reg=*/1, 0x11111111,
                  SCALAR_PLUS_VECTOR(SCALAR_BASE_REG, INDEX_REG_S, ELSZ_S, SXTW, 0),
                  EXPECT((app_pc)0x8000000000010000, (app_pc)0x8000000000020000,
                         (app_pc)0x8000000000030000, (app_pc)0x8000000000040000,
                         (app_pc)0x7fffffffffff0000, (app_pc)0x7ffffffffffd0000,
                         (app_pc)0x7ffffffffffc0000, (app_pc)0x7ffffffffffb0000));

    /* 32-bit unpacked scaled offset [<Xn|SP>, <Zm>.D, <mod> #N] */
    VEC_ADDR_TEST(ld1h, /*governing_pred_reg=*/1, 0x01010101,
                  SCALAR_PLUS_VECTOR(SCALAR_BASE_REG, INDEX_REG_D, ELSZ_D, UXTW, 1),
                  EXPECT((app_pc)0x8000000000020000, (app_pc)0x8000000000040000,
                         (app_pc)0x80000001fffe0000, (app_pc)0x80000001fffc0000));
    VEC_ADDR_TEST(st1h, /*governing_pred_reg=*/1, 0x01010101,
                  SCALAR_PLUS_VECTOR(SCALAR_BASE_REG, INDEX_REG_D, ELSZ_D, SXTW, 1),
                  EXPECT((app_pc)0x8000000000020000, (app_pc)0x8000000000040000,
                         (app_pc)0x7ffffffffffe0000, (app_pc)0x7ffffffffffc0000));
    VEC_ADDR_TEST(ld1w, /*governing_pred_reg=*/1, 0x01010101,
                  SCALAR_PLUS_VECTOR(SCALAR_BASE_REG, INDEX_REG_D, ELSZ_D, UXTW, 2),
                  EXPECT((app_pc)0x8000000000040000, (app_pc)0x8000000000080000,
                         (app_pc)0x80000003fffc0000, (app_pc)0x80000003fff80000));
    VEC_ADDR_TEST(st1w, /*governing_pred_reg=*/1, 0x01010101,
                  SCALAR_PLUS_VECTOR(SCALAR_BASE_REG, INDEX_REG_D, ELSZ_D, SXTW, 2),
                  EXPECT((app_pc)0x8000000000040000, (app_pc)0x8000000000080000,
                         (app_pc)0x7ffffffffffc0000, (app_pc)0x7ffffffffff80000));
    VEC_ADDR_TEST(ld1d, /*governing_pred_reg=*/1, 0x01010101,
                  SCALAR_PLUS_VECTOR(SCALAR_BASE_REG, INDEX_REG_D, ELSZ_D, UXTW, 3),
                  EXPECT((app_pc)0x8000000000080000, (app_pc)0x8000000000100000,
                         (app_pc)0x80000007fff80000, (app_pc)0x80000007fff00000));
    VEC_ADDR_TEST(st1d, /*governing_pred_reg=*/1, 0x01010101,
                  SCALAR_PLUS_VECTOR(SCALAR_BASE_REG, INDEX_REG_D, ELSZ_D, SXTW, 3),
                  EXPECT((app_pc)0x8000000000080000, (app_pc)0x8000000000100000,
                         (app_pc)0x7ffffffffff80000, (app_pc)0x7ffffffffff00000));

    /* 32-bit unpacked unscaled offset [<Xn|SP>, <Zm>.D, <mod>] */
    VEC_ADDR_TEST(ld1d, /*governing_pred_reg=*/1, 0x01010101,
                  SCALAR_PLUS_VECTOR(SCALAR_BASE_REG, INDEX_REG_D, ELSZ_D, UXTW, 0),
                  EXPECT((app_pc)0x8000000000010000, (app_pc)0x8000000000020000,
                         (app_pc)0x80000000ffff0000, (app_pc)0x80000000fffe0000));
    VEC_ADDR_TEST(st1d, /*governing_pred_reg=*/1, 0x01010101,
                  SCALAR_PLUS_VECTOR(SCALAR_BASE_REG, INDEX_REG_D, ELSZ_D, SXTW, 0),
                  EXPECT((app_pc)0x8000000000010000, (app_pc)0x8000000000020000,
                         (app_pc)0x7fffffffffff0000, (app_pc)0x7ffffffffffe0000));

    /* 64-bit scaled offset [<Xn|SP>, <Zm>.D, LSL #N] */
    VEC_ADDR_TEST(ld1h, /*governing_pred_reg=*/1, 0x01010101,
                  SCALAR_PLUS_VECTOR(SCALAR_BASE_REG, INDEX_REG_D, ELSZ_D, UXTX, 1),
                  EXPECT((app_pc)0x8000000000020000, (app_pc)0x8000000000040000,
                         (app_pc)0x7ffffffffffe0000, (app_pc)0x7ffffffffffc0000));
    VEC_ADDR_TEST(st1w, /*governing_pred_reg=*/1, 0x01010101,
                  SCALAR_PLUS_VECTOR(SCALAR_BASE_REG, INDEX_REG_D, ELSZ_D, UXTX, 2),
                  EXPECT((app_pc)0x8000000000040000, (app_pc)0x8000000000080000,
                         (app_pc)0x7ffffffffffc0000, (app_pc)0x7ffffffffff80000));
    VEC_ADDR_TEST(ld1d, /*governing_pred_reg=*/1, 0x01010101,
                  SCALAR_PLUS_VECTOR(SCALAR_BASE_REG, INDEX_REG_D, ELSZ_D, UXTX, 3),
                  EXPECT((app_pc)0x8000000000080000, (app_pc)0x8000000000100000,
                         (app_pc)0x7ffffffffff80000, (app_pc)0x7ffffffffff00000));

    /* 64-bit unscaled offset [<Xn|SP>, <Zm>.D] */
    VEC_ADDR_TEST(st1d, /*governing_pred_reg=*/1, 0x01010101,
                  SCALAR_PLUS_VECTOR(SCALAR_BASE_REG, INDEX_REG_D, ELSZ_D, UXTX, 0),
                  EXPECT((app_pc)0x8000000000010000, (app_pc)0x8000000000020000,
                         (app_pc)0x7fffffffffff0000, (app_pc)0x7ffffffffffe0000));

    /* Test predicate handling. */

    /* Test with all elements inactive */
    VEC_ADDR_TEST(ld1w, /*governing_pred_reg=*/2, 0x00000000,
                  SCALAR_PLUS_VECTOR(SCALAR_BASE_REG, INDEX_REG_S, ELSZ_S, UXTW, 0),
                  EXPECT(/*nothing*/));
    VEC_ADDR_TEST(st1d, /*governing_pred_reg=*/3, 0x00000000,
                  SCALAR_PLUS_VECTOR(SCALAR_BASE_REG, INDEX_REG_D, ELSZ_D, UXTW, 0),
                  EXPECT(/*nothing*/));

    /* Test with every other element active */
    VEC_ADDR_TEST(st1b, /*governing_pred_reg=*/4, 0x01010101,
                  SCALAR_PLUS_VECTOR(SCALAR_BASE_REG, INDEX_REG_S, ELSZ_S, UXTW, 0),
                  EXPECT((app_pc)0x8000000000010000, (app_pc)0x8000000000030000,
                         (app_pc)0x80000000ffff0000, (app_pc)0x80000000fffc0000));
    VEC_ADDR_TEST(st1h, /*governing_pred_reg=*/5, 0x00010001,
                  SCALAR_PLUS_VECTOR(SCALAR_BASE_REG, INDEX_REG_D, ELSZ_D, UXTW, 0),
                  EXPECT((app_pc)0x8000000000010000, (app_pc)0x80000000ffff0000));

    /* Test with a single element active */
    VEC_ADDR_TEST(ld1w, /*governing_pred_reg=*/6, 0x00000010,
                  SCALAR_PLUS_VECTOR(SCALAR_BASE_REG, INDEX_REG_S, ELSZ_S, UXTW, 0),
                  EXPECT((app_pc)0x8000000000020000));
    VEC_ADDR_TEST(st1d, /*governing_pred_reg=*/7, 0x00000100,
                  SCALAR_PLUS_VECTOR(SCALAR_BASE_REG, INDEX_REG_D, ELSZ_D, UXTW, 0),
                  EXPECT((app_pc)0x8000000000020000));
#undef SCALAR_PLUS_VECTOR

#define VECTOR_PLUS_IMM(zn, el_size, imm)                                      \
    opnd_create_vector_base_disp_aarch64(DR_REG_Z0 + zn, DR_REG_NULL, el_size, \
                                         DR_EXTEND_UXTX, 0, imm, 0, OPSZ_NA, 0)

    VEC_ADDR_TEST(ld1b, /*governing_pred_reg=*/0, 0x11111111,
                  VECTOR_PLUS_IMM(BASE_REG_S, ELSZ_S, 0),
                  EXPECT((app_pc)0x0000000000000000, (app_pc)0x0000000080000000,
                         (app_pc)0x00000000ffffffff, (app_pc)0x0000000000010000,
                         (app_pc)0x0000000010000000, (app_pc)0x0000000020000000,
                         (app_pc)0x0000000030000000, (app_pc)0x0000000040000000));
    VEC_ADDR_TEST(st1b, /*governing_pred_reg=*/0, 0x11111111,
                  VECTOR_PLUS_IMM(BASE_REG_S, ELSZ_S, 31),
                  EXPECT((app_pc)0x000000000000001f, (app_pc)0x000000008000001f,
                         (app_pc)0x000000010000001e, (app_pc)0x000000000001001f,
                         (app_pc)0x000000001000001f, (app_pc)0x000000002000001f,
                         (app_pc)0x000000003000001f, (app_pc)0x000000004000001f));
    VEC_ADDR_TEST(ld1b, /*governing_pred_reg=*/0, 0x01010101,
                  VECTOR_PLUS_IMM(BASE_REG_D, ELSZ_D, 0),
                  EXPECT((app_pc)0x0000000000000000, (app_pc)0x8000000000000000,
                         (app_pc)0xffffffffffffffff, (app_pc)0x0000000010000000));
    VEC_ADDR_TEST(st1b, /*governing_pred_reg=*/0, 0x11111111,
                  VECTOR_PLUS_IMM(BASE_REG_D, ELSZ_D, 31),
                  EXPECT((app_pc)0x000000000000001f, (app_pc)0x800000000000001f,
                         (app_pc)0x000000000000001e, (app_pc)0x000000001000001f));

    VEC_ADDR_TEST(ld1h, /*governing_pred_reg=*/0, 0x11111111,
                  VECTOR_PLUS_IMM(BASE_REG_S, ELSZ_S, 62),
                  EXPECT((app_pc)0x000000000000003e, (app_pc)0x000000008000003e,
                         (app_pc)0x000000010000003d, (app_pc)0x000000000001003e,
                         (app_pc)0x000000001000003e, (app_pc)0x000000002000003e,
                         (app_pc)0x000000003000003e, (app_pc)0x000000004000003e));
    VEC_ADDR_TEST(st1h, /*governing_pred_reg=*/0, 0x11111111,
                  VECTOR_PLUS_IMM(BASE_REG_D, ELSZ_D, 62),
                  EXPECT((app_pc)0x000000000000003e, (app_pc)0x800000000000003e,
                         (app_pc)0x000000000000003d, (app_pc)0x000000001000003e));

    VEC_ADDR_TEST(ld1w, /*governing_pred_reg=*/0, 0x11111111,
                  VECTOR_PLUS_IMM(BASE_REG_S, ELSZ_S, 124),
                  EXPECT((app_pc)0x000000000000007c, (app_pc)0x000000008000007c,
                         (app_pc)0x000000010000007b, (app_pc)0x000000000001007c,
                         (app_pc)0x000000001000007c, (app_pc)0x000000002000007c,
                         (app_pc)0x000000003000007c, (app_pc)0x000000004000007c));
    VEC_ADDR_TEST(st1w, /*governing_pred_reg=*/0, 0x11111111,
                  VECTOR_PLUS_IMM(BASE_REG_D, ELSZ_D, 124),
                  EXPECT((app_pc)0x000000000000007c, (app_pc)0x800000000000007c,
                         (app_pc)0x000000000000007b, (app_pc)0x000000001000007c));

    VEC_ADDR_TEST(ld1d, /*governing_pred_reg=*/0, 0x11111111,
                  VECTOR_PLUS_IMM(BASE_REG_D, ELSZ_D, 248),
                  EXPECT((app_pc)0x00000000000000f8, (app_pc)0x80000000000000f8,
                         (app_pc)0x00000000000000f7, (app_pc)0x00000000100000f8));

    /* Test with all elements inactive */
    VEC_ADDR_TEST(ld1w, /*governing_pred_reg=*/0, 0x00000000,
                  VECTOR_PLUS_IMM(BASE_REG_S, ELSZ_S, 124), EXPECT(/*nothing*/));
    VEC_ADDR_TEST(st1w, /*governing_pred_reg=*/0, 0x00000000,
                  VECTOR_PLUS_IMM(BASE_REG_D, ELSZ_D, 124), EXPECT(/*nothing*/));

    /* Test with every other element active */
    VEC_ADDR_TEST(ld1w, /*governing_pred_reg=*/0, 0x01010101,
                  VECTOR_PLUS_IMM(BASE_REG_S, ELSZ_S, 124),
                  EXPECT((app_pc)0x000000000000007c, (app_pc)0x000000010000007b,
                         (app_pc)0x000000001000007c, (app_pc)0x000000003000007c));
    VEC_ADDR_TEST(st1w, /*governing_pred_reg=*/0, 0x00010001,
                  VECTOR_PLUS_IMM(BASE_REG_D, ELSZ_D, 124),
                  EXPECT((app_pc)0x000000000000007c, (app_pc)0x000000000000007b));

    /* Test with a single element active */
    VEC_ADDR_TEST(ld1w, /*governing_pred_reg=*/0, 0x00000010,
                  VECTOR_PLUS_IMM(BASE_REG_S, ELSZ_S, 124),
                  EXPECT((app_pc)0x000000008000007c));
    VEC_ADDR_TEST(st1w, /*governing_pred_reg=*/0, 0x00000100,
                  VECTOR_PLUS_IMM(BASE_REG_D, ELSZ_D, 124),
                  EXPECT((app_pc)0x800000000000007c));

#undef VECTOR_PLUS_IMM

#define VECTOR_PLUS_SCALAR(zn, el_size, xm)                                       \
    opnd_create_vector_base_disp_aarch64(DR_REG_Z0 + zn, DR_REG_X0 + xm, el_size, \
                                         DR_EXTEND_UXTX, 0, 0, 0, OPSZ_NA, 0)
    VEC_ADDR_TEST(ldnt1b, /*governing_pred_reg=*/0, 0x11111111,
                  VECTOR_PLUS_SCALAR(BASE_REG_S, ELSZ_S, 8),
                  EXPECT((app_pc)0xffffffffffffffff, (app_pc)0x000000007fffffff,
                         (app_pc)0x00000000fffffffe, (app_pc)0x000000000000ffff,
                         (app_pc)0x000000000fffffff, (app_pc)0x000000001fffffff,
                         (app_pc)0x000000002fffffff, (app_pc)0x000000003fffffff));
    VEC_ADDR_TEST(stnt1b, /*governing_pred_reg=*/0, 0x01010101,
                  VECTOR_PLUS_SCALAR(BASE_REG_D, ELSZ_D, 7),
                  EXPECT((app_pc)0x0000000000000007, (app_pc)0x8000000000000007,
                         (app_pc)0x0000000000000006, (app_pc)0x0000000010000007));

    /* Test with all elements inactive */
    VEC_ADDR_TEST(ldnt1h, /*governing_pred_reg=*/0, 0x00000000,
                  VECTOR_PLUS_SCALAR(BASE_REG_S, ELSZ_S, 6), EXPECT(/*nothing*/));
    VEC_ADDR_TEST(stnt1h, /*governing_pred_reg=*/0, 0x00000000,
                  VECTOR_PLUS_SCALAR(BASE_REG_D, ELSZ_D, 5), EXPECT(/*nothing*/));

    /* Test with every other element active */
    VEC_ADDR_TEST(ldnt1w, /*governing_pred_reg=*/0, 0x01010101,
                  VECTOR_PLUS_SCALAR(BASE_REG_S, ELSZ_S, 4),
                  EXPECT((app_pc)0x0000000000000004, (app_pc)0x0000000100000003,
                         (app_pc)0x0000000010000004, (app_pc)0x0000000030000004));
    VEC_ADDR_TEST(stnt1w, /*governing_pred_reg=*/0, 0x00010001,
                  VECTOR_PLUS_SCALAR(BASE_REG_D, ELSZ_D, 3),
                  EXPECT((app_pc)0x0000000000000003, (app_pc)0x0000000000000002));

    /* Test with a single element active */
    VEC_ADDR_TEST(ldnt1w, /*governing_pred_reg=*/0, 0x00000010,
                  VECTOR_PLUS_SCALAR(BASE_REG_S, ELSZ_S, 2),
                  EXPECT((app_pc)0x0000000080000002));
    VEC_ADDR_TEST(stnt1d, /*governing_pred_reg=*/0, 0x00000100,
                  VECTOR_PLUS_SCALAR(BASE_REG_D, ELSZ_D, 1),
                  EXPECT((app_pc)0x8000000000000001));

#undef VECTOR_PLUS_SCALAR

#undef EXPECT
#undef VEC_ADDR_TEST

    ASSERT(dr_set_vector_length(original_vector_length));
}

void
test_reg_is_simd()
{
    for (reg_id_t reg = DR_REG_START_32; reg <= DR_REG_STOP_32; reg++)
        ASSERT(!reg_is_simd(reg));

    for (reg_id_t reg = DR_REG_START_64; reg <= DR_REG_STOP_64; reg++)
        ASSERT(!reg_is_simd(reg));

    for (reg_id_t reg = DR_REG_Q0; reg <= DR_REG_Q0 + DR_NUM_SIMD_VECTOR_REGS - 1; reg++)
        ASSERT(reg_is_simd(reg));

    for (reg_id_t reg = DR_REG_D0; reg <= DR_REG_D0 + DR_NUM_SIMD_VECTOR_REGS - 1; reg++)
        ASSERT(reg_is_simd(reg));

    for (reg_id_t reg = DR_REG_S0; reg <= DR_REG_S0 + DR_NUM_SIMD_VECTOR_REGS - 1; reg++)
        ASSERT(reg_is_simd(reg));

    for (reg_id_t reg = DR_REG_H0; reg <= DR_REG_H0 + DR_NUM_SIMD_VECTOR_REGS - 1; reg++)
        ASSERT(reg_is_simd(reg));

    for (reg_id_t reg = DR_REG_B0; reg <= DR_REG_B0 + DR_NUM_SIMD_VECTOR_REGS - 1; reg++)
        ASSERT(reg_is_simd(reg));

    for (reg_id_t reg = DR_REG_START_Z; reg <= DR_REG_STOP_Z; reg++)
        ASSERT(reg_is_simd(reg));

    for (reg_id_t reg = DR_REG_START_P; reg <= DR_REG_STOP_P; reg++)
        ASSERT(!reg_is_simd(reg));
}

void
test_cond()
{
    /* Test dr_pred_type_t -> cond opnd. */
    ASSERT(opnd_get_immed_int(opnd_create_cond(DR_PRED_EQ)) == 0b0000);
    ASSERT(opnd_get_immed_int(opnd_create_cond(DR_PRED_NE)) == 0b0001);
    ASSERT(opnd_get_immed_int(opnd_create_cond(DR_PRED_CS)) == 0b0010);
    ASSERT(opnd_get_immed_int(opnd_create_cond(DR_PRED_CC)) == 0b0011);
    ASSERT(opnd_get_immed_int(opnd_create_cond(DR_PRED_MI)) == 0b0100);
    ASSERT(opnd_get_immed_int(opnd_create_cond(DR_PRED_PL)) == 0b0101);
    ASSERT(opnd_get_immed_int(opnd_create_cond(DR_PRED_VS)) == 0b0110);
    ASSERT(opnd_get_immed_int(opnd_create_cond(DR_PRED_VC)) == 0b0111);
    ASSERT(opnd_get_immed_int(opnd_create_cond(DR_PRED_HI)) == 0b1000);
    ASSERT(opnd_get_immed_int(opnd_create_cond(DR_PRED_LS)) == 0b1001);
    ASSERT(opnd_get_immed_int(opnd_create_cond(DR_PRED_GE)) == 0b1010);
    ASSERT(opnd_get_immed_int(opnd_create_cond(DR_PRED_LT)) == 0b1011);
    ASSERT(opnd_get_immed_int(opnd_create_cond(DR_PRED_GT)) == 0b1100);
    ASSERT(opnd_get_immed_int(opnd_create_cond(DR_PRED_LE)) == 0b1101);
    ASSERT(opnd_get_immed_int(opnd_create_cond(DR_PRED_AL)) == 0b1110);
    ASSERT(opnd_get_immed_int(opnd_create_cond(DR_PRED_NV)) == 0b1111);

    /* Test aliases. */
    ASSERT(opnd_get_immed_int(opnd_create_cond(DR_PRED_HS)) == 0b0010);
    ASSERT(opnd_get_immed_int(opnd_create_cond(DR_PRED_LO)) == 0b0011);

    ASSERT(opnd_get_immed_int(opnd_create_cond(DR_PRED_SVE_NONE)) == 0b0000);
    ASSERT(opnd_get_immed_int(opnd_create_cond(DR_PRED_SVE_ANY)) == 0b0001);
    ASSERT(opnd_get_immed_int(opnd_create_cond(DR_PRED_SVE_NLAST)) == 0b0010);
    ASSERT(opnd_get_immed_int(opnd_create_cond(DR_PRED_SVE_LAST)) == 0b0011);
    ASSERT(opnd_get_immed_int(opnd_create_cond(DR_PRED_SVE_FIRST)) == 0b0100);
    ASSERT(opnd_get_immed_int(opnd_create_cond(DR_PRED_SVE_NFRST)) == 0b0101);
    ASSERT(opnd_get_immed_int(opnd_create_cond(DR_PRED_SVE_PLAST)) == 0b1001);
    ASSERT(opnd_get_immed_int(opnd_create_cond(DR_PRED_SVE_TCONT)) == 0b1010);
    ASSERT(opnd_get_immed_int(opnd_create_cond(DR_PRED_SVE_TSTOP)) == 0b1011);

    /* Test cond opnd -> dr_pred_type_t. */
    ASSERT(opnd_get_cond(OPND_CREATE_INT(0b0000)) == DR_PRED_EQ);
    ASSERT(opnd_get_cond(OPND_CREATE_INT(0b0001)) == DR_PRED_NE);
    ASSERT(opnd_get_cond(OPND_CREATE_INT(0b0010)) == DR_PRED_CS);
    ASSERT(opnd_get_cond(OPND_CREATE_INT(0b0011)) == DR_PRED_CC);
    ASSERT(opnd_get_cond(OPND_CREATE_INT(0b0100)) == DR_PRED_MI);
    ASSERT(opnd_get_cond(OPND_CREATE_INT(0b0101)) == DR_PRED_PL);
    ASSERT(opnd_get_cond(OPND_CREATE_INT(0b0110)) == DR_PRED_VS);
    ASSERT(opnd_get_cond(OPND_CREATE_INT(0b0111)) == DR_PRED_VC);
    ASSERT(opnd_get_cond(OPND_CREATE_INT(0b1000)) == DR_PRED_HI);
    ASSERT(opnd_get_cond(OPND_CREATE_INT(0b1001)) == DR_PRED_LS);
    ASSERT(opnd_get_cond(OPND_CREATE_INT(0b1010)) == DR_PRED_GE);
    ASSERT(opnd_get_cond(OPND_CREATE_INT(0b1011)) == DR_PRED_LT);
    ASSERT(opnd_get_cond(OPND_CREATE_INT(0b1100)) == DR_PRED_GT);
    ASSERT(opnd_get_cond(OPND_CREATE_INT(0b1101)) == DR_PRED_LE);
    ASSERT(opnd_get_cond(OPND_CREATE_INT(0b1110)) == DR_PRED_AL);
    ASSERT(opnd_get_cond(OPND_CREATE_INT(0b1111)) == DR_PRED_NV);
}

int
main(int argc, char *argv[])
{
    /* Required for proc_init() -> proc_init_arch() establishing vector length
     * on SVE h/w. This is validated with the direct read of vector length
     * using the SVE RDVL instruction in test_get_size() above.
     */
    void *drcontext = dr_standalone_init();

    test_get_size();

    test_opnd_compute_address();

    test_opnd_invert_immed_int();

    test_compute_vector_address(drcontext);

    test_reg_is_simd();

    test_cond();

    printf("all done\n");
    return 0;
}
