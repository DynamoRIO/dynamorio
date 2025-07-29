/* **********************************************************
 * Copyright (c) 2024-2025 Google, Inc.  All rights reserved.
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

    /* Check instruction length.
     */
    ASSERT((next_pc_encode - bytes) == instr_length(dc, instr_synthetic_decoded));
    ASSERT(instr_length(dc, instr_synthetic_decoded) != 0);

    /* Check for overflow.
     */
    ASSERT((next_pc_encode - bytes) <= sizeof(bytes));
    ASSERT((next_pc_decode - bytes) <= sizeof(bytes));

    /* Check operation size.
     */
    ASSERT(instr_get_operation_size(instr_synthetic_decoded) != OPSZ_NA);
    ASSERT(instr_get_operation_size(instr_synthetic_decoded) ==
           instr_get_operation_size(instr_synthetic_converted));

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
    const char *expected_disasm_str_xchg = " 00010022 020a0204 move [4byte]     "
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
    const char *expected_disasm_str_jmp_ind = " 00002800          load branch \n";
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

/* XXX i#6662.
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
    ASSERT(strncmp(get_register_name(DR_REG_VIRT0), rv0, sizeof(rv0)) == 0);

    const char rv1[] = "rv1";
    ASSERT(strncmp(get_register_name(DR_REG_VIRT1), rv1, sizeof(rv1)) == 0);

    const char rv187[] = "rv187";
    ASSERT(strncmp(get_register_name(DR_REG_VIRT187), rv187, sizeof(rv187)) == 0);

    const char rv252[] = "rv252";
    ASSERT(strncmp(get_register_name(DR_REG_VIRT252), rv252, sizeof(rv252)) == 0);

    /* Restore previous ISA mode.
     */
    dr_set_isa_mode(dc, old_isa_mode, NULL);
}

/* We can't compare DR_REG_VIRT and DR_REG_ enum values directly because they belong to
 * different enums and we build with -Werror=enum-compare.  So we use scopes and the
 * temporary variable reg_virtual.
 */
#define ASSERT_NOT_NULL_OR_INVALID(x)                                            \
    do {                                                                         \
        reg_id_t reg_virtual = x;                                                \
        ASSERT((reg_virtual != DR_REG_NULL) && (reg_virtual != DR_REG_INVALID)); \
    } while (0);

/* We can't loop through DR_REG_VIRT enums because we have a gap for value 187
 * (== DR_REG_INVALID for x86), so we unroll the loop and compare every enum manually.
 */
#define CHECK_VIRTUAL_REGISTER_IDS             \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT0)   \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT1)   \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT2)   \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT3)   \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT4)   \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT5)   \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT6)   \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT7)   \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT8)   \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT9)   \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT10)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT11)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT12)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT13)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT14)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT15)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT16)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT17)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT18)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT19)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT20)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT21)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT22)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT23)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT24)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT25)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT26)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT27)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT28)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT29)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT30)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT31)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT32)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT33)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT34)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT35)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT36)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT37)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT38)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT39)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT40)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT41)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT42)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT43)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT44)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT45)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT46)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT47)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT48)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT49)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT50)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT51)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT52)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT53)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT54)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT55)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT56)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT57)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT58)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT59)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT60)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT61)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT62)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT63)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT64)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT65)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT66)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT67)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT68)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT69)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT70)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT71)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT72)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT73)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT74)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT75)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT76)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT77)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT78)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT79)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT80)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT81)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT82)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT83)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT84)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT85)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT86)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT87)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT88)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT89)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT90)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT91)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT92)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT93)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT94)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT95)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT96)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT97)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT98)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT99)  \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT100) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT101) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT102) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT103) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT104) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT105) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT106) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT107) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT108) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT109) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT110) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT111) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT112) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT113) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT114) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT115) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT116) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT117) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT118) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT119) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT120) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT121) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT122) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT123) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT124) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT125) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT126) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT127) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT128) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT129) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT130) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT131) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT132) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT133) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT134) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT135) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT136) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT137) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT138) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT139) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT140) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT141) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT142) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT143) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT144) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT145) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT146) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT147) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT148) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT149) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT150) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT151) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT152) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT153) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT154) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT155) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT156) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT157) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT158) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT159) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT160) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT161) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT162) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT163) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT164) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT165) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT166) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT167) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT168) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT169) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT170) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT171) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT172) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT173) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT174) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT175) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT176) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT177) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT178) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT179) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT180) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT181) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT182) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT183) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT184) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT185) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT186) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT187) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT188) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT189) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT190) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT191) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT192) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT193) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT194) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT195) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT196) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT197) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT198) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT199) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT200) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT201) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT202) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT203) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT204) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT205) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT206) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT207) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT208) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT209) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT210) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT211) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT212) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT213) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT214) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT215) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT216) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT217) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT218) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT219) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT220) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT221) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT222) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT223) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT224) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT225) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT226) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT227) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT228) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT229) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT230) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT231) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT232) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT233) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT234) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT235) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT236) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT237) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT238) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT239) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT240) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT241) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT242) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT243) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT244) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT245) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT246) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT247) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT248) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT249) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT250) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT251) \
    ASSERT_NOT_NULL_OR_INVALID(DR_REG_VIRT252)

void
check_virtual_register_enum_values(void)
{
    CHECK_VIRTUAL_REGISTER_IDS;
}

static void
test_invalid_disasm(void *dcontext)
{
    // Synthesize a regdeps instr.
    instr_t *instr = instr_build(dcontext, OP_UNDECODED, 1, 1);
    instr_set_isa_mode(instr, DR_ISA_REGDEPS);
    instr_set_dst(instr, 0, opnd_create_reg(DR_REG_VIRT2));
    instr_set_src(instr, 0, opnd_create_reg(DR_REG_VIRT5));
    instr_set_operation_size(instr, OPSZ_8);

    // Make sure it doesn't crash if we select a not-fully-supported disasm style
    // and try to print it out.
    disassemble_set_syntax(DR_DISASM_INTEL);
    char buf[128];
    size_t len =
        instr_disassemble_to_buffer(dcontext, instr, buf, BUFFER_SIZE_ELEMENTS(buf));
    ASSERT(len > 0);
    disassemble_set_syntax(DR_DISASM_ATT);
    len = instr_disassemble_to_buffer(dcontext, instr, buf, BUFFER_SIZE_ELEMENTS(buf));
    ASSERT(len > 0);
    disassemble_set_syntax(DR_DISASM_ARM);
    len = instr_disassemble_to_buffer(dcontext, instr, buf, BUFFER_SIZE_ELEMENTS(buf));
    ASSERT(len > 0);

    instr_destroy(dcontext, instr);
    disassemble_set_syntax(DR_DISASM_DR);
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

    test_invalid_disasm(dcontext);

    print("All DR_ISA_REGDEPS tests are done.\n");
    dr_standalone_exit();
    return 0;
}
