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
test_instr_encode_decode_synthetic(void *dc, instr_t *instr)
{
    /* Encoded synthetic ISA instructions require 4 byte alignment.
     * The largest synthetic encoded instruction has 16 bytes.
     */
    byte ALIGN_VAR(REGDEPS_ALIGN_BYTES) bytes[16];

    /* Convert a real ISA instruction to a synthetic ISA (DR_ISA_REGDEPS) instruction.
     */
    instr_t *instr_synthetic_converted = instr_create(dc);
    instr_convert_to_isa_regdeps(dc, instr, instr_synthetic_converted);

    /* Check that the converted instruction only has register operands.
     */
    ASSERT(instr_has_only_register_operands(instr_synthetic_converted));

    /* Check that we do not have an opcode for the converted instruction.
     */
    ASSERT(instr_get_opcode(instr_synthetic_converted) == OP_INVALID);

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
    dr_set_isa_mode(dc, old_isa_mode, NULL);
    ASSERT(next_pc_decode != NULL);
    ASSERT(next_pc_encode == next_pc_decode);
    /* Check for overflow.
     */
    ASSERT((next_pc_encode - bytes) <= sizeof(bytes));
    ASSERT((next_pc_decode - bytes) <= sizeof(bytes));
    /* Check that the two synthetic instructions are the same.
     */
    ASSERT(instr_same(instr_synthetic_converted, instr_synthetic_decoded));

    instr_destroy(dc, instr);
    instr_destroy(dc, instr_synthetic_converted);
    instr_destroy(dc, instr_synthetic_decoded);
}

#ifdef X86_64
static void
test_instr_create_encode_decode_synthetic_x86_64(void *dc)
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
    test_instr_encode_decode_synthetic(dc, instr);

    instr = INSTR_CREATE_pop(dc, opnd_create_reg(SEG_FS));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    test_instr_encode_decode_synthetic(dc, instr);

    opnd_t abs_addr = opnd_create_abs_addr((void *)0xdeadbeefdeadbeef, OPSZ_8);
    instr = INSTR_CREATE_mov_ld(dc, opnd_create_reg(DR_REG_RAX), abs_addr);
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    test_instr_encode_decode_synthetic(dc, instr);

    instr = INSTR_CREATE_cmps_1(dc);
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    test_instr_encode_decode_synthetic(dc, instr);

    instr = INSTR_CREATE_maskmovq(dc, opnd_create_reg(DR_REG_MM0),
                                  opnd_create_reg(DR_REG_MM1));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    test_instr_encode_decode_synthetic(dc, instr);

    instr =
        INSTR_CREATE_xchg(dc, opnd_create_reg(DR_REG_R8D), opnd_create_reg(DR_REG_EAX));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    test_instr_encode_decode_synthetic(dc, instr);

    instr = INSTR_CREATE_add(dc, opnd_create_reg(DR_REG_RAX), OPND_CREATE_INT32(42));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    test_instr_encode_decode_synthetic(dc, instr);

    instr_t *tgt = INSTR_CREATE_mov_imm(dc, opnd_create_reg(DR_REG_XAX),
                                        opnd_create_immed_int(0xdeadbeef, OPSZ_PTR));
    instr = INSTR_CREATE_jmp_ind(dc, opnd_create_mem_instr(tgt, 2, OPSZ_PTR));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    test_instr_encode_decode_synthetic(dc, instr);
    instr_destroy(dc, tgt);

    instr =
        INSTR_CREATE_bsf(dc, opnd_create_reg(DR_REG_EAX), opnd_create_reg(DR_REG_ECX));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    test_instr_encode_decode_synthetic(dc, instr);

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
    test_instr_encode_decode_synthetic(dc, instr);
}
#endif

#ifdef ARM
static void
test_instr_create_encode_decode_synthetic_arm(void *dc)
{
    byte buf[128];
    instr_t *instr;

    instr = INSTR_CREATE_lsls(dc, opnd_create_reg(DR_REG_R0), opnd_create_reg(DR_REG_R1),
                              OPND_CREATE_INT(4));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    test_instr_encode_decode_synthetic(dc, instr);

    instr = INSTR_CREATE_sel(dc, opnd_create_reg(DR_REG_R0), opnd_create_reg(DR_REG_R1),
                             opnd_create_reg(DR_REG_R1));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    test_instr_encode_decode_synthetic(dc, instr);

    instr = INSTR_CREATE_movs(dc, opnd_create_reg(DR_REG_R0), OPND_CREATE_INT(4));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    test_instr_encode_decode_synthetic(dc, instr);

    instr = INSTR_CREATE_movs(dc, opnd_create_reg(DR_REG_R0), opnd_create_reg(DR_REG_R1));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    test_instr_encode_decode_synthetic(dc, instr);
}
#endif

#ifdef AARCH64
static void
test_instr_create_encode_decode_synthetic_aarch64(void *dc)
{
    byte buf[128];
    instr_t *instr;

    instr = INSTR_CREATE_add(dc, opnd_create_reg(DR_REG_X0), opnd_create_reg(DR_REG_SP),
                             opnd_create_reg(DR_REG_X1));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    test_instr_encode_decode_synthetic(dc, instr);

    instr = INSTR_CREATE_sub(dc, opnd_create_reg(DR_REG_X0), opnd_create_reg(DR_REG_SP),
                             opnd_create_reg(DR_REG_X1));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    test_instr_encode_decode_synthetic(dc, instr);

    instr =
        INSTR_CREATE_adds_imm(dc, opnd_create_reg(DR_REG_W0), opnd_create_reg(DR_REG_W1),
                              opnd_create_immed_int(0, OPSZ_12b), OPND_CREATE_INT8(0));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    test_instr_encode_decode_synthetic(dc, instr);

    instr = INSTR_CREATE_adc(dc, opnd_create_reg(DR_REG_W0), opnd_create_reg(DR_REG_W1),
                             opnd_create_reg(DR_REG_W2));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    test_instr_encode_decode_synthetic(dc, instr);

    instr = INSTR_CREATE_ldpsw(
        dc, opnd_create_reg(DR_REG_X1), opnd_create_reg(DR_REG_X2),
        opnd_create_reg(DR_REG_X0),
        opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 4, 0, OPSZ_8),
        OPND_CREATE_INT(4));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    test_instr_encode_decode_synthetic(dc, instr);
}
#endif

#ifdef RISCV64
static void
test_instr_create_encode_decode_synthetic_riscv64(void *dc)
{
    byte buf[128];
    instr_t *instr;

    instr =
        INSTR_CREATE_lwu(dc, opnd_create_reg(DR_REG_A0),
                         opnd_create_base_disp(DR_REG_X31, DR_REG_NULL, 0, 0, OPSZ_4));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    test_instr_encode_decode_synthetic(dc, instr);

    instr = INSTR_CREATE_sw(
        dc, opnd_create_base_disp(DR_REG_X31, DR_REG_NULL, 0, (1 << 11) - 1, OPSZ_4),
        opnd_create_reg(DR_REG_X0));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    test_instr_encode_decode_synthetic(dc, instr);

    instr = INSTR_CREATE_flw(dc, opnd_create_reg(DR_REG_F0),
                             opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, OPSZ_4));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    test_instr_encode_decode_synthetic(dc, instr);

    instr =
        INSTR_CREATE_lr_d(dc, opnd_create_reg(DR_REG_X0),
                          opnd_create_base_disp(DR_REG_X31, DR_REG_NULL, 0, 0, OPSZ_8),
                          opnd_create_immed_int(0b10, OPSZ_2b));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    test_instr_encode_decode_synthetic(dc, instr);

    instr = INSTR_CREATE_fmadd_d(dc, opnd_create_reg(DR_REG_F31),
                                 opnd_create_immed_int(0b000, OPSZ_3b),
                                 opnd_create_reg(DR_REG_F0), opnd_create_reg(DR_REG_F2),
                                 opnd_create_reg(DR_REG_F3));
    instr_encode(dc, instr, buf);
    instr_reset(dc, instr);
    decode(dc, buf, instr);
    test_instr_encode_decode_synthetic(dc, instr);
}
#endif

void
test_virtual_register(void)
{
    const char rv0[] = "rv0";
    ASSERT(strncmp(dr_get_virtual_register_name(DR_REG_V0), rv0, sizeof(rv0)) == 0);

    const char rv1[] = "rv1";
    ASSERT(strncmp(dr_get_virtual_register_name(DR_REG_V1), rv1, sizeof(rv1)) == 0);

    const char rv187[] = "rv187";
    ASSERT(strncmp(dr_get_virtual_register_name(DR_REG_V187), rv187, sizeof(rv187)) == 0);

    const char rv252[] = "rv252";
    ASSERT(strncmp(dr_get_virtual_register_name(DR_REG_V252), rv252, sizeof(rv252)) == 0);
}

int
main(int argc, char *argv[])
{
    void *dcontext = dr_standalone_init();
    ASSERT(!dr_running_under_dynamorio());

#ifdef X86_64
    test_instr_create_encode_decode_synthetic_x86_64(dcontext);
#endif

#ifdef ARM
    test_instr_create_encode_decode_synthetic_arm(dcontext);
#endif

#ifdef AARCH64
    test_instr_create_encode_decode_synthetic_aarch64(dcontext);
#endif

#ifdef RISCV64
    test_instr_create_encode_decode_synthetic_riscv64(dcontext);
#endif

    test_virtual_register();

    print("All DR_ISA_REGDEPS tests are done.\n");
    dr_standalone_exit();
    return 0;
}
