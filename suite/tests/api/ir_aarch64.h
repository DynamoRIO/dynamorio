/* **********************************************************
 * Copyright (c) 2015-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2022 ARM Limited. All rights reserved.
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

static byte buf[8192];

#ifdef STANDALONE_DECODER
#    define ASSERT(x)                                                                 \
        ((void)((!(x)) ? (fprintf(stderr, "ASSERT FAILURE (standalone): %s:%d: %s\n", \
                                  __FILE__, __LINE__, #x),                            \
                          abort(), 0)                                                 \
                       : 0))
#else
#    define ASSERT(x)                                                                \
        ((void)((!(x)) ? (dr_fprintf(STDERR, "ASSERT FAILURE (client): %s:%d: %s\n", \
                                     __FILE__, __LINE__, #x),                        \
                          dr_abort(), 0)                                             \
                       : 0))
#endif

#define TEST_INSTR(instruction_name) \
    void test_instr_##instruction_name(void *dc, instr_t *instr, bool *psuccess)

#define RUN_INSTR_TEST(instruction_name)                          \
    test_result = true;                                           \
    test_instr_##instruction_name(dcontext, instr, &test_result); \
    if (test_result == false) {                                   \
        print("test for " #instruction_name " failed.\n");        \
        result = false;                                           \
    }

#define TEST_NO_OPNDS(opcode, create_name, expected)            \
    instr = INSTR_CREATE_##create_name(dc);                     \
    if (!test_instr_encoding(dc, OP_##opcode, instr, expected)) \
        *psuccess = false;

#define TEST_LOOP(opcode, create_name, number, expected, args...)   \
    for (int i = 0; i < number; i++) {                              \
        instr = INSTR_CREATE_##create_name(dc, args);               \
        if (!test_instr_encoding(dc, OP_##opcode, instr, expected)) \
            *psuccess = false;                                      \
    }

static bool
test_instr_encoding(void *dc, uint opcode, instr_t *instr, const char *expected)
{
    instr_t *decin;
    byte *pc;
    char *end, *big;
    size_t len = strlen(expected);
    size_t buflen = (len < 100 ? 100 : len) + 2;
    bool result = true;
    char *buf = malloc(buflen);

    if (instr_get_opcode(instr) != opcode) {
        print("incorrect opcode for instr %s: %s\n\n", decode_opcode_name(opcode),
              decode_opcode_name(instr_get_opcode(instr)));
        instr_destroy(dc, instr);
        return false;
    }
    instr_disassemble_to_buffer(dc, instr, buf, buflen);
    end = buf + strlen(buf);
    if (end > buf && *(end - 1) == '\n')
        --end;

    if (!instr_is_encoding_possible(instr)) {
        print("encoding for expected %s not possible\n", expected);
        instr_destroy(dc, instr);
        return false;
    }

    if (end - buf != len || memcmp(buf, expected, len) != 0) {
        print("dissassembled as:\n");
        print("   %s\n", buf);
        print("but expected:\n");
        print("   %s\n\n", expected);
        instr_destroy(dc, instr);
        return false;
    }

    pc = instr_encode(dc, instr, buf);
    decin = instr_create(dc);
    decode(dc, buf, decin);
    if (!instr_same(instr, decin)) {
        print("Reencoding failed, dissassembled as:\n   ");
        instr_disassemble(dc, decin, STDERR);
        print("\n");
        print("but expected:\n");
        print("   %s\n", expected);
        print("Encoded as:\n");
        print("   0x%08x\n\n", *((int *)pc));
        result = false;
    }

    instr_destroy(dc, instr);
    instr_destroy(dc, decin);

    return result;
}

#define TEST_REG_ARRAY6(name, r0, r1, r2, r3, r4, r5)                 \
    const reg_id_t name[6] = { DR_REG_##r0, DR_REG_##r1, DR_REG_##r2, \
                               DR_REG_##r3, DR_REG_##r4, DR_REG_##r5 }
TEST_REG_ARRAY6(Zn_six_offset_0, Z0, Z5, Z10, Z16, Z21, Z31);
TEST_REG_ARRAY6(Zn_six_offset_1, Z0, Z6, Z11, Z17, Z22, Z31);
TEST_REG_ARRAY6(Zn_six_offset_2, Z0, Z7, Z12, Z18, Z23, Z31);
TEST_REG_ARRAY6(Zn_six_offset_3, Z0, Z8, Z13, Z19, Z24, Z31);

TEST_REG_ARRAY6(Pn_half_six_offset_0, P0, P2, P3, P5, P6, P7);
TEST_REG_ARRAY6(Pn_six_offset_0, P0, P2, P5, P8, P10, P15);
TEST_REG_ARRAY6(Pn_six_offset_1, P0, P3, P6, P9, P11, P15);
TEST_REG_ARRAY6(Pn_six_offset_2, P0, P4, P7, P10, P12, P15);

TEST_REG_ARRAY6(Wn_six_offset_0, W0, W5, W10, W15, W20, W30);
TEST_REG_ARRAY6(Xn_six_offset_0, X0, X5, X10, X15, X20, X30);

TEST_REG_ARRAY6(Wn_six_offset_0sp, W0, W7, W12, W17, W22, WSP);
TEST_REG_ARRAY6(Wn_six_offset_1sp, W0, W8, W13, W18, W23, WSP);
TEST_REG_ARRAY6(Wn_six_offset_2sp, W0, W9, W14, W19, W24, WSP);

TEST_REG_ARRAY6(Wn_six_offset_0zr, W0, W7, W12, W17, W22, WZR);
TEST_REG_ARRAY6(Wn_six_offset_1zr, W0, W8, W13, W18, W23, WZR);
TEST_REG_ARRAY6(Wn_six_offset_2zr, W0, W9, W14, W19, W24, WZR);

TEST_REG_ARRAY6(Xn_six_offset_0sp, X0, X7, X12, X17, X22, SP);
TEST_REG_ARRAY6(Xn_six_offset_1sp, X0, X8, X13, X18, X23, SP);
TEST_REG_ARRAY6(Xn_six_offset_2sp, X0, X9, X14, X19, X24, SP);

TEST_REG_ARRAY6(Xn_six_offset_0zr, X0, X7, X12, X17, X22, XZR);
TEST_REG_ARRAY6(Xn_six_offset_1zr, X0, X8, X13, X18, X23, XZR);
TEST_REG_ARRAY6(Xn_six_offset_2zr, X0, X9, X14, X19, X24, XZR);

TEST_REG_ARRAY6(Bn_six_offset_0, B0, B5, B10, B16, B21, B31);
TEST_REG_ARRAY6(Bn_six_offset_1, B0, B6, B11, B17, B22, B31);
TEST_REG_ARRAY6(Bn_six_offset_2, B0, B7, B12, B18, B23, B31);

TEST_REG_ARRAY6(Hn_six_offset_0, H0, H5, H10, H16, H21, H31);
TEST_REG_ARRAY6(Hn_six_offset_1, H0, H6, H11, H17, H22, H31);
TEST_REG_ARRAY6(Hn_six_offset_2, H0, H7, H12, H18, H23, H31);

TEST_REG_ARRAY6(Sn_six_offset_0, S0, S5, S10, S16, S21, S31);
TEST_REG_ARRAY6(Sn_six_offset_1, S0, S6, S11, S17, S22, S31);
TEST_REG_ARRAY6(Sn_six_offset_2, S0, S7, S12, S18, S23, S31);

TEST_REG_ARRAY6(Dn_six_offset_0, D0, D5, D10, D16, D21, D31);
TEST_REG_ARRAY6(Dn_six_offset_1, D0, D6, D11, D17, D22, D31);
TEST_REG_ARRAY6(Dn_six_offset_2, D0, D7, D12, D18, D23, D31);
#undef TEST_REG_ARRAY6
