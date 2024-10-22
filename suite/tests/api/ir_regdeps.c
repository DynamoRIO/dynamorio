/* **********************************************************
 * Copyright (c) 2024 Google, Inc.  All rights reserved.
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

#include "configure.h"
#include "dr_api.h"
#include "tools.h"

#define ASSERT(x)                                                                 \
    ((void)((!(x)) ? (dr_fprintf(STDERR, "ASSERT FAILURE: %s:%d: %s\n", __FILE__, \
                                 __LINE__, #x),                                   \
                      dr_abort(), 0)                                              \
                   : 0))

/* We are not exporting the defines in core/ir/isa_regdeps/encoding_common.h, so we
 * redefine DR_ISA_REGDEPS alignment requirement here.
 */
#define REGDEPS_ALIGN_BYTES 4

static bool
instr_has_only_register_operands(instr_t *instr)
{
    uint num_dsts = (uint)instr_num_dsts(instr);
    for (uint dst_index = 0; dst_index < num_dsts; ++dst_index) {
        opnd_t dst_opnd = instr_get_dst(instr, dst_index);
        if (!opnd_is_reg(dst_opnd))
            return false;
    }

    uint num_srcs = (uint)instr_num_srcs(instr);
    for (uint src_index = 0; src_index < num_srcs; ++src_index) {
        opnd_t src_opnd = instr_get_src(instr, src_index);
        if (!opnd_is_reg(src_opnd))
            return false;
    }

    return true;
}

static void
test_instr_encode_decode_disassemble_synthetic(void *dc, instr_t *instr,
                                               const char *expected_disasm_str)
{
    /* Encoded synthetic ISA instructions require 4 byte alignment.
     * The largest synthetic encoded instruction has 16 bytes.
     */
    byte ALIGN_VAR(REGDEPS_ALIGN_BYTES) bytes[16];
    memset(bytes, 0, sizeof(bytes));

    /* Convert a real ISA instruction to a synthetic ISA (DR_ISA_REGDEPS) instruction.
     */
    instr_t *instr_synthetic_converted = instr_create(dc);
    instr_convert_to_isa_regdeps(dc, instr, instr_synthetic_converted);

    /* Check that the converted instruction only has register operands.
     */
    ASSERT(instr_has_only_register_operands(instr_synthetic_converted));

    /* Check that we do not have an opcode for the converted instruction.
     */
    ASSERT(instr_get_opcode(instr_synthetic_converted) == OP_UNDECODED);

    /* Encode the synthetic instruction.
     */
    byte *next_pc_encode = instr_encode(dc, instr_synthetic_converted, bytes);
    ASSERT(next_pc_encode != NULL);

    /* Create an instruction where we can decode the previously encoded synthetic
     * instruction.
     */
    instr_t *instr_synthetic_decoded = instr_create(dc);

    dr_isa_mode_t old_isa_mode;
    /* DR uses dcontext_t ISA mode to decode instructions.
     * Since we are decoding synthetic instructions, we set it to DR_ISA_REGDEPS.
     */
    dr_set_isa_mode(dc, DR_ISA_REGDEPS, &old_isa_mode);
    /* Decode the encoded synthetic instruction bytes into instr_synthetic.
     */
    byte *next_pc_decode = decode(dc, bytes, instr_synthetic_decoded);
    ASSERT(next_pc_decode != NULL);
    ASSERT(next_pc_encode == next_pc_decode);
    /* Check for overflow.
     */
    ASSERT((next_pc_encode - bytes) <= sizeof(bytes));
    ASSERT((next_pc_decode - bytes) <= sizeof(bytes));

    /* Disassemble regdeps synthetic encodings to buffer.
     */
    char dbuf[512];
    int len;
    byte *pc_disasm = disassemble_to_buffer(dc, bytes, bytes, false, true, dbuf,
                                            BUFFER_SIZE_ELEMENTS(dbuf), &len);

    /* We need dcontext ISA mode to still be DR_ISA_REGDEPS for dissassemble_to_buffer(),
     * as it calls decode() on synthetic regdepes encodings again. We restore the old mode
     * here, after disassembly.
     */
    dr_set_isa_mode(dc, old_isa_mode, NULL);

    ASSERT(pc_disasm == next_pc_encode);
    /* Check that the string representation of the disassembled regdeps instruction is
     * what we expect (ground truth), if we have it.
     */
    if (expected_disasm_str[0] != '\0')
        ASSERT(strcmp(dbuf, expected_disasm_str) == 0);

    /* Check that the two synthetic instructions are the same.
     */
    ASSERT(instr_same(instr_synthetic_converted, instr_synthetic_decoded));

    /* Cleanup.
     */
    instr_destroy(dc, instr);
    instr_destroy(dc, instr_synthetic_converted);
    instr_destroy(dc, instr_synthetic_decoded);
}

#ifdef X86_64
static void
test_instr_create_encode_decode_disassemble_synthetic_x86_64(void *dc)
{
    byte buf[128];
    instr_t *instr;

    instr = INSTR_CREATE_push(dc, opnd_create_reg(SEG_FS));
    /* Instructions generated by INSTR_CREATE_ or XINST_CREATE_ are not fully decoded.
     * For example, their categories are not set.  So we encode and decode them to obtain
     * a fully decoded instruction before testing our DR_ISA_REGDEPS synthetic encoding
     * and decoding.  We do so for all instructions of all architectures.
     */
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    const char *expected_disasm_str_push =
        " 00001021 26060606 store [8byte]       %rv4 %rv36 -> %rv4\n";
    test_instr_encode_decode_disassemble_synthetic(dc, instr, expected_disasm_str_push);

    instr = INSTR_CREATE_pop(dc, opnd_create_reg(SEG_FS));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    const char *expected_disasm_str_pop =
        " 00000812 06260606 load [8byte]       %rv4 -> %rv4 %rv36\n";
    test_instr_encode_decode_disassemble_synthetic(dc, instr, expected_disasm_str_pop);

    opnd_t abs_addr = opnd_create_abs_addr((void *)0xdeadbeefdeadbeef, OPSZ_8);
    instr = INSTR_CREATE_mov_ld(dc, opnd_create_reg(DR_REG_RAX), abs_addr);
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    const char *expected_disasm_str_mov =
        " 00000801 00000206 load [8byte]        -> %rv0\n";
    test_instr_encode_decode_disassemble_synthetic(dc, instr, expected_disasm_str_mov);

    instr = INSTR_CREATE_cmps_1(dc);
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    const char *expected_disasm_str_cmps = " 00000942 08090806 load [8byte]       %rv6 "
                                           "%rv7 %rv32 %rv35 -> %rv6 %rv7\n 00252209\n";
    test_instr_encode_decode_disassemble_synthetic(dc, instr, expected_disasm_str_cmps);

    instr = INSTR_CREATE_maskmovq(dc, opnd_create_reg(DR_REG_MM0),
                                  opnd_create_reg(DR_REG_MM1));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    const char *expected_disasm_str_maskmovq =
        " 00005040 13120906 store simd [8byte]       %rv7 %rv16 %rv17 %rv35\n 00000025\n";
    test_instr_encode_decode_disassemble_synthetic(dc, instr,
                                                   expected_disasm_str_maskmovq);

    instr =
        INSTR_CREATE_xchg(dc, opnd_create_reg(DR_REG_R8D), opnd_create_reg(DR_REG_EAX));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    const char *expected_disasm_str_xchg = " 00000022 020a0204 uncategorized [4byte]     "
                                           "  %rv0 %rv8 -> %rv0 %rv8\n 0000000a\n";
    test_instr_encode_decode_disassemble_synthetic(dc, instr, expected_disasm_str_xchg);

    instr = INSTR_CREATE_add(dc, opnd_create_reg(DR_REG_RAX), OPND_CREATE_INT32(42));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    const char *expected_disasm_str_add =
        " 00040111 00020206 math [8byte]       %rv0 -> %rv0\n";
    test_instr_encode_decode_disassemble_synthetic(dc, instr, expected_disasm_str_add);

    instr_t *tgt = INSTR_CREATE_mov_imm(dc, opnd_create_reg(DR_REG_XAX),
                                        opnd_create_immed_int(0xdeadbeef, OPSZ_PTR));
    instr = INSTR_CREATE_jmp_ind(dc, opnd_create_mem_instr(tgt, 2, OPSZ_PTR));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    const char *expected_disasm_str_jmp_ind = " 00002800 load branch \n";
    test_instr_encode_decode_disassemble_synthetic(dc, instr,
                                                   expected_disasm_str_jmp_ind);
    instr_destroy(dc, tgt);

    instr =
        INSTR_CREATE_bsf(dc, opnd_create_reg(DR_REG_EAX), opnd_create_reg(DR_REG_ECX));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    const char *expected_disasm_str_bsf =
        " 00000111 00030204 uncategorized [4byte]       %rv1 -> %rv0\n";
    test_instr_encode_decode_disassemble_synthetic(dc, instr, expected_disasm_str_bsf);

    /* Containing-register IDs can be >=256, hence their value does not fit in the
     * allotted 8 bits per register operand of regdeps encoding. This was causing a
     * memory corruption in instr_convert_to_isa_regdeps() where src_reg_used and
     * dst_reg_used have only 256 elements and are laid out next to each other in memory.
     * Writing to index >=256 into one was overwriting the other. Fix: remap containing
     * register IDs starting from 0 for all architectures, since we still have only up to
     * 198 unique containing registers (max number of containing registers for AARCH64).
     * DR_REG_K0 and the DR_REG_ZMM_ registers are some of these "problematic" registers
     * with enum value >=256. We use them here to test our fix.
     */
    instr = INSTR_CREATE_vdpbf16ps_mask(
        dc, opnd_create_reg(DR_REG_ZMM30), opnd_create_reg(DR_REG_K0),
        opnd_create_reg(DR_REG_ZMM29), opnd_create_reg(DR_REG_ZMM28));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    const char *expected_disasm_str_vdpbf16ps_mask =
        " 00000031 65646640 uncategorized [64byte]       %rv98 %rv99 %rv102 -> %rv100\n "
        "00000068\n";
    test_instr_encode_decode_disassemble_synthetic(dc, instr,
                                                   expected_disasm_str_vdpbf16ps_mask);
}
#endif

/* FIXME i#6662.
 * Test currently not runningin in "ci-aarchxx-cross / arm-cross-compile" because of:
 * "/lib/ld-linux-armhf.so.3: No such file or directory".
 */
#ifdef ARM
static void
test_instr_create_encode_decode_disassemble_synthetic_arm(void *dc)
{
    byte buf[128];
    instr_t *instr;

    instr = INSTR_CREATE_lsls(dc, opnd_create_reg(DR_REG_R0), opnd_create_reg(DR_REG_R1),
                              OPND_CREATE_INT(4));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    test_instr_encode_decode_disassemble_synthetic(dc, instr, "");

    instr = INSTR_CREATE_sel(dc, opnd_create_reg(DR_REG_R0), opnd_create_reg(DR_REG_R1),
                             opnd_create_reg(DR_REG_R1));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    test_instr_encode_decode_disassemble_synthetic(dc, instr, "");

    instr = INSTR_CREATE_movs(dc, opnd_create_reg(DR_REG_R0), OPND_CREATE_INT(4));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    test_instr_encode_decode_disassemble_synthetic(dc, instr, "");

    instr = INSTR_CREATE_movs(dc, opnd_create_reg(DR_REG_R0), opnd_create_reg(DR_REG_R1));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    test_instr_encode_decode_disassemble_synthetic(dc, instr, "");
}
#endif

#ifdef AARCH64
static void
test_instr_create_encode_decode_disassemble_synthetic_aarch64(void *dc)
{
    byte buf[128];
    instr_t *instr;

    instr = INSTR_CREATE_add(dc, opnd_create_reg(DR_REG_X0), opnd_create_reg(DR_REG_SP),
                             opnd_create_reg(DR_REG_X1));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    const char *expected_disasm_str_add =
        " 00040021 21030206 math [8byte]       %rv1 %rv31 -> %rv0\n";
    test_instr_encode_decode_disassemble_synthetic(dc, instr, expected_disasm_str_add);

    instr = INSTR_CREATE_sub(dc, opnd_create_reg(DR_REG_X0), opnd_create_reg(DR_REG_SP),
                             opnd_create_reg(DR_REG_X1));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    const char *expected_disasm_str_sub =
        " 00040021 21030206 math [8byte]       %rv1 %rv31 -> %rv0\n";
    test_instr_encode_decode_disassemble_synthetic(dc, instr, expected_disasm_str_sub);

    instr =
        INSTR_CREATE_adds_imm(dc, opnd_create_reg(DR_REG_W0), opnd_create_reg(DR_REG_W1),
                              opnd_create_immed_int(0, OPSZ_12b), OPND_CREATE_INT8(0));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    const char *expected_disasm_adds_imm =
        " 00040111 00030204 math [4byte]       %rv1 -> %rv0\n";
    test_instr_encode_decode_disassemble_synthetic(dc, instr, expected_disasm_adds_imm);

    instr = INSTR_CREATE_adc(dc, opnd_create_reg(DR_REG_W0), opnd_create_reg(DR_REG_W1),
                             opnd_create_reg(DR_REG_W2));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    const char *expected_disasm_adc =
        " 00040221 04030204 math [4byte]       %rv1 %rv2 -> %rv0\n";
    test_instr_encode_decode_disassemble_synthetic(dc, instr, expected_disasm_adc);

    instr = INSTR_CREATE_ldpsw(
        dc, opnd_create_reg(DR_REG_X1), opnd_create_reg(DR_REG_X2),
        opnd_create_reg(DR_REG_X0),
        opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 4, 0, OPSZ_8),
        OPND_CREATE_INT(4));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    const char *expected_disasm_str_ldpsw =
        " 00000813 04030206 load [8byte]       %rv0 -> %rv0 %rv1 %rv2\n 00000002\n";
    test_instr_encode_decode_disassemble_synthetic(dc, instr, expected_disasm_str_ldpsw);
}
#endif

#ifdef RISCV64
static void
test_instr_create_encode_decode_disassemble_synthetic_riscv64(void *dc)
{
    byte buf[128];
    instr_t *instr;

    instr =
        INSTR_CREATE_lwu(dc, opnd_create_reg(DR_REG_A0),
                         opnd_create_base_disp(DR_REG_X31, DR_REG_NULL, 0, 0, OPSZ_4));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    const char *expected_disasm_str_lwu =
        " 00000011 00210c04 uncategorized [4byte]       %rv31 -> %rv10\n";
    test_instr_encode_decode_disassemble_synthetic(dc, instr, expected_disasm_str_lwu);

    instr = INSTR_CREATE_sw(
        dc, opnd_create_base_disp(DR_REG_X31, DR_REG_NULL, 0, (1 << 11) - 1, OPSZ_4),
        opnd_create_reg(DR_REG_X0));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    const char *expected_disasm_str_sw =
        " 00000020 00210206 uncategorized [8byte]       %rv0 %rv31\n";
    test_instr_encode_decode_disassemble_synthetic(dc, instr, expected_disasm_str_sw);

    instr = INSTR_CREATE_flw(dc, opnd_create_reg(DR_REG_F0),
                             opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, OPSZ_4));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    const char *expected_disasm_str_flw =
        " 00000011 000d2304 uncategorized [4byte]       %rv11 -> %rv33\n";
    test_instr_encode_decode_disassemble_synthetic(dc, instr, expected_disasm_str_flw);

    instr =
        INSTR_CREATE_lr_d(dc, opnd_create_reg(DR_REG_X0),
                          opnd_create_base_disp(DR_REG_X31, DR_REG_NULL, 0, 0, OPSZ_8),
                          opnd_create_immed_int(0b10, OPSZ_2b));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    const char *expected_disasm_str_lr_d =
        " 00000011 00210206 uncategorized [8byte]       %rv31 -> %rv0\n";
    test_instr_encode_decode_disassemble_synthetic(dc, instr, expected_disasm_str_lr_d);

    instr = INSTR_CREATE_fmadd_d(dc, opnd_create_reg(DR_REG_F31),
                                 opnd_create_immed_int(0b000, OPSZ_3b),
                                 opnd_create_reg(DR_REG_F0), opnd_create_reg(DR_REG_F2),
                                 opnd_create_reg(DR_REG_F3));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    const char *expected_disasm_str_fmadd_d =
        " 00000031 25234206 uncategorized [8byte]       %rv33 %rv35 %rv36 -> %rv64\n "
        "00000026\n";
    test_instr_encode_decode_disassemble_synthetic(dc, instr,
                                                   expected_disasm_str_fmadd_d);
}
#endif

void
test_virtual_register_names(void *dc)
{
    dr_isa_mode_t old_isa_mode;
    /* DR uses the dcontext_t ISA mode to decide whether a register is virtual or real and
     * to return its correct name.  Since we are testing virtual register names, we set it
     * to DR_ISA_REGDEPS.
     */
    dr_set_isa_mode(dc, DR_ISA_REGDEPS, &old_isa_mode);

    const char rv0[] = "rv0";
    ASSERT(strncmp(get_register_name(DR_REG_V0), rv0, sizeof(rv0)) == 0);

    const char rv1[] = "rv1";
    ASSERT(strncmp(get_register_name(DR_REG_V1), rv1, sizeof(rv1)) == 0);

    const char rv187[] = "rv187";
    ASSERT(strncmp(get_register_name(DR_REG_V187), rv187, sizeof(rv187)) == 0);

    const char rv252[] = "rv252";
    ASSERT(strncmp(get_register_name(DR_REG_V252), rv252, sizeof(rv252)) == 0);

    /* Restore previous ISA mode.
     */
    dr_set_isa_mode(dc, old_isa_mode, NULL);
}

/* We can't compare DR_REG_V and DR_REG_ enum values directly because they belong to
 * different enums and we build with -Werror=enum-compare.  So we use scopes and the
 * temporary variable reg_virtual.
 */
#define ASSERT_NOT_NULL_OR_INVALID(x)                                            \
    do {                                                                         \
        reg_id_t reg_virtual = x;                                                \
        ASSERT((reg_virtual != DR_REG_NULL) && (reg_virtual != DR_REG_INVALID)); \
    } while (0);

/* We can't loop through DR_REG_V enums because we have a gap for value 187
 * (== DR_REG_INVALID for x86), so we unroll the loop and compare every enum manually.
 */
#define CHECK_VIRTUAL_REGISTER_IDS          \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V0)   \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V1)   \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V2)   \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V3)   \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V4)   \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V5)   \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V6)   \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V7)   \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V8)   \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V9)   \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V10)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V11)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V12)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V13)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V14)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V15)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V16)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V17)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V18)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V19)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V20)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V21)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V22)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V23)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V24)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V25)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V26)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V27)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V28)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V29)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V30)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V31)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V32)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V33)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V34)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V35)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V36)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V37)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V38)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V39)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V40)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V41)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V42)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V43)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V44)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V45)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V46)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V47)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V48)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V49)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V50)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V51)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V52)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V53)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V54)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V55)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V56)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V57)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V58)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V59)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V60)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V61)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V62)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V63)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V64)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V65)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V66)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V67)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V68)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V69)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V70)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V71)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V72)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V73)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V74)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V75)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V76)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V77)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V78)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V79)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V80)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V81)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V82)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V83)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V84)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V85)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V86)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V87)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V88)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V89)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V90)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V91)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V92)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V93)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V94)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V95)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V96)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V97)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V98)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V99)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V100) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V101) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V102) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V103) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V104) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V105) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V106) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V107) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V108) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V109) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V110) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V111) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V112) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V113) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V114) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V115) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V116) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V117) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V118) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V119) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V120) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V121) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V122) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V123) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V124) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V125) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V126) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V127) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V128) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V129) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V130) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V131) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V132) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V133) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V134) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V135) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V136) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V137) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V138) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V139) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V140) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V141) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V142) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V143) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V144) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V145) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V146) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V147) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V148) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V149) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V150) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V151) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V152) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V153) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V154) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V155) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V156) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V157) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V158) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V159) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V160) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V161) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V162) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V163) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V164) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V165) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V166) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V167) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V168) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V169) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V170) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V171) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V172) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V173) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V174) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V175) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V176) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V177) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V178) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V179) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V180) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V181) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V182) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V183) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V184) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V185) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V186) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V187) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V188) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V189) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V190) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V191) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V192) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V193) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V194) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V195) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V196) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V197) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V198) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V199) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V200) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V201) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V202) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V203) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V204) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V205) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V206) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V207) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V208) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V209) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V210) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V211) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V212) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V213) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V214) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V215) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V216) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V217) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V218) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V219) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V220) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V221) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V222) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V223) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V224) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V225) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V226) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V227) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V228) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V229) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V230) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V231) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V232) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V233) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V234) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V235) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V236) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V237) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V238) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V239) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V240) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V241) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V242) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V243) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V244) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V245) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V246) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V247) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V248) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V249) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V250) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V251) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_V252)

void
check_virtual_register_enum_values(void)
{
    CHECK_VIRTUAL_REGISTER_IDS;
}

int
main(int argc, char *argv[])
{
    void *dcontext = dr_standalone_init();
    ASSERT(!dr_running_under_dynamorio());

#ifdef X86_64
    test_instr_create_encode_decode_disassemble_synthetic_x86_64(dcontext);
#endif

#ifdef ARM
    test_instr_create_encode_decode_disassemble_synthetic_arm(dcontext);
#endif

#ifdef AARCH64
    test_instr_create_encode_decode_disassemble_synthetic_aarch64(dcontext);
#endif

#ifdef RISCV64
    test_instr_create_encode_decode_disassemble_synthetic_riscv64(dcontext);
#endif

    test_virtual_register_names(dcontext);

    check_virtual_register_enum_values();

    print("All DR_ISA_REGDEPS tests are done.\n");
    dr_standalone_exit();
    return 0;
}
