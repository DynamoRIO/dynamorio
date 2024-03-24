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

#define MAX_NUM_REGS 256

#define ASSERT(x)                                                                 \
    ((void)((!(x)) ? (dr_fprintf(STDERR, "ASSERT FAILURE: %s:%d: %s\n", __FILE__, \
                                 __LINE__, #x),                                   \
                      dr_abort(), 0)                                              \
                   : 0))

/* XXX i#6717: this code duplicates part of the synthetic ISA encoding in
 * core/ir/synthetic/encoding.c and should be kept in synch. A whole-instr register
 * operand iterator would allow us to remove this duplicate code.
 */
static void
get_instr_src_and_dst_registers(instr_t *instr, uint max_num_regs,
                                opnd_size_t *src_reg_to_size,
                                opnd_size_t *dst_reg_to_size)
{
    memset((void *)src_reg_to_size, 0, max_num_regs);
    memset((void *)dst_reg_to_size, 0, max_num_regs);
    uint original_num_dsts = (uint)instr_num_dsts(instr);
    for (uint dst_index = 0; dst_index < original_num_dsts; ++dst_index) {
        opnd_t dst_opnd = instr_get_dst(instr, dst_index);
        uint num_regs_used_by_opnd = (uint)opnd_num_regs_used(dst_opnd);
        if (opnd_is_memory_reference(dst_opnd)) {
            for (uint opnd_index = 0; opnd_index < num_regs_used_by_opnd; ++opnd_index) {
                reg_id_t reg = opnd_get_reg_used(dst_opnd, opnd_index);
                /* Map sub-registers to their containing register.
                 */
                reg_id_t reg_canonical = reg_to_pointer_sized(reg);
                opnd_size_t reg_size = reg_get_size(reg);
                if (src_reg_to_size[reg_canonical] != 0)
                    src_reg_to_size[reg_canonical] = reg_size;
            }
        } else {
            for (uint opnd_index = 0; opnd_index < num_regs_used_by_opnd; ++opnd_index) {
                reg_id_t reg = opnd_get_reg_used(dst_opnd, opnd_index);
                /* Map sub-registers to their containing register.
                 */
                reg_id_t reg_canonical = reg_to_pointer_sized(reg);
                opnd_size_t reg_size = reg_get_size(reg);
                if (dst_reg_to_size[reg_canonical] != 0)
                    dst_reg_to_size[reg_canonical] = reg_size;
            }
        }
    }

    uint original_num_srcs = (uint)instr_num_srcs(instr);
    for (uint i = 0; i < original_num_srcs; ++i) {
        opnd_t src_opnd = instr_get_src(instr, i);
        uint num_regs_used_by_opnd = (uint)opnd_num_regs_used(src_opnd);
        for (uint opnd_index = 0; opnd_index < num_regs_used_by_opnd; ++opnd_index) {
            reg_id_t reg = opnd_get_reg_used(src_opnd, opnd_index);
            /* Map sub-registers to their containing register.
             */
            reg_id_t reg_canonical = reg_to_pointer_sized(reg);
            opnd_size_t reg_size = reg_get_size(reg);
            if (src_reg_to_size[reg_canonical] != 0)
                src_reg_to_size[reg_canonical] = reg_size;
        }
    }
}

static bool
instr_synthetic_matches_real(instr_t *instr_real, instr_t *instr_synthetic)
{
    /* Check that instr_t ISA modes are the same.
     */
    if (instr_get_isa_mode(instr_real) != instr_get_isa_mode(instr_synthetic))
        return false;

    /* Check that instr_t categories are the same.
     */
    if (instr_get_category(instr_real) != instr_get_category(instr_synthetic))
        return false;

    /* Check that register operands are the same.
     * This also ensures the two instructions have the same number of source and
     * destination operands that are registers.
     */
    opnd_size_t src_reg_to_size_real_instr[MAX_NUM_REGS];
    opnd_size_t dst_reg_to_size_real_instr[MAX_NUM_REGS];
    get_instr_src_and_dst_registers(instr_real, MAX_NUM_REGS, src_reg_to_size_real_instr,
                                    dst_reg_to_size_real_instr);
    opnd_size_t src_reg_to_size_synthetic_instr[MAX_NUM_REGS];
    opnd_size_t dst_reg_to_size_synthetic_instr[MAX_NUM_REGS];
    get_instr_src_and_dst_registers(instr_synthetic, MAX_NUM_REGS,
                                    src_reg_to_size_synthetic_instr,
                                    dst_reg_to_size_synthetic_instr);
    if (memcmp(src_reg_to_size_real_instr, src_reg_to_size_synthetic_instr,
               sizeof(src_reg_to_size_real_instr)) != 0)
        return false;
    if (memcmp(dst_reg_to_size_real_instr, dst_reg_to_size_synthetic_instr,
               sizeof(dst_reg_to_size_real_instr)) != 0)
        return false;

    /* Check that arithmetic flags are the same.
     */
    uint eflags_instr_real = instr_get_arith_flags(instr_real, DR_QUERY_DEFAULT);
    uint eflags_instr_synthetic =
        instr_get_arith_flags(instr_synthetic, DR_QUERY_DEFAULT);
    uint eflags_real = 0x0;
    if (TESTANY(EFLAGS_WRITE_ARITH, eflags_instr_real))
        eflags_real |= 0x1;
    if (TESTANY(EFLAGS_READ_ARITH, eflags_instr_real))
        eflags_real |= 0x2;
    uint eflags_synthetic = 0x0;
    if (TESTANY(EFLAGS_WRITE_ARITH, eflags_instr_synthetic))
        eflags_synthetic |= 0x1;
    if (TESTANY(EFLAGS_READ_ARITH, eflags_instr_synthetic))
        eflags_synthetic |= 0x2;
    if (eflags_real != eflags_synthetic)
        return false;

    return true;
}

static void
test_instr_encode_decode_synthetic(void *dc, instr_t *instr)
{
    /* Encoded synthetic ISA instructions require 4 byte alignment.
     * The biggest synthetic encoded instruction reaches 12 bytes.
     */
    byte ALIGN_VAR(4) bytes[12];
    instr_t *instr_synthetic = instr_create(dc);
    ASSERT(instr_synthetic != NULL);
    /* DR uses instr_t ISA mode to encode instructions.
     * Since we are encoding synthetic instructions, we set it to DR_ISA_SYNTHETIC for
     * both the instruction we are encoding (instr) and the instruction we are decoding to
     * (instr_synthetic).
     */
    bool is_isa_mode_set = instr_set_isa_mode(instr_synthetic, DR_ISA_SYNTHETIC);
    ASSERT(is_isa_mode_set);
    is_isa_mode_set = instr_set_isa_mode(instr, DR_ISA_SYNTHETIC);
    ASSERT(is_isa_mode_set);
    /* Encode instr (which comes from a real ISA) to a synthetic instruction into bytes.
     */
    byte *next_pc_encode = instr_encode(dc, instr, bytes);
    ASSERT(next_pc_encode != NULL);
    dr_isa_mode_t old_isa_mode;
    /* DR uses dcontext_t ISA mode to decode instructions.
     * Since we are decoding synthetic instructions, we set it to DR_ISA_SYNTHETIC.
     */
    dr_set_isa_mode(dc, DR_ISA_SYNTHETIC, &old_isa_mode);
    /* Decode the encoded synthetic instruction bytes into instr_synthetic.
     */
    byte *next_pc_decode = decode(dc, bytes, instr_synthetic);
    dr_set_isa_mode(dc, old_isa_mode, NULL);
    ASSERT(next_pc_decode != NULL);
    ASSERT(next_pc_encode == next_pc_decode);
    ASSERT(instr_length(dc, instr_synthetic) <= sizeof(bytes));
    ASSERT(instr_synthetic_matches_real(instr, instr_synthetic));
    instr_destroy(dc, instr);
    instr_destroy(dc, instr_synthetic);
}

#ifdef X86_64
static void
test_instr_create_encode_decode_synthetic_x86_64(void *dc)
{
    instr_t *instr;

    opnd_t abs_addr = opnd_create_abs_addr((void *)0xdeadbeefdeadbeef, OPSZ_8);
    instr = INSTR_CREATE_mov_ld(dc, opnd_create_reg(DR_REG_RAX), abs_addr);
    test_instr_encode_decode_synthetic(dc, instr);

    instr = INSTR_CREATE_cmps_1(dc);
    test_instr_encode_decode_synthetic(dc, instr);

    instr = INSTR_CREATE_maskmovq(dc, opnd_create_reg(DR_REG_MM0),
                                  opnd_create_reg(DR_REG_MM1));
    test_instr_encode_decode_synthetic(dc, instr);

    instr =
        INSTR_CREATE_xchg(dc, opnd_create_reg(DR_REG_R8D), opnd_create_reg(DR_REG_EAX));
    test_instr_encode_decode_synthetic(dc, instr);

    instr = INSTR_CREATE_add(dc, opnd_create_reg(DR_REG_RAX), OPND_CREATE_INT32(42));
    test_instr_encode_decode_synthetic(dc, instr);
}
#endif

#ifdef ARM
static void
test_instr_create_encode_decode_synthetic_arm(void *dc)
{
    instr_t *instr;

    instr = INSTR_CREATE_lsls(dc, opnd_create_reg(DR_REG_R0), opnd_create_reg(DR_REG_R1),
                              OPND_CREATE_INT(4));
    test_instr_encode_decode_synthetic(dc, instr);

    instr = INSTR_CREATE_sel(dc, opnd_create_reg(DR_REG_R0), opnd_create_reg(DR_REG_R1),
                             opnd_create_reg(DR_REG_R1));
    test_instr_encode_decode_synthetic(dc, instr);

    instr = INSTR_CREATE_movs(dc, opnd_create_reg(DR_REG_R0), OPND_CREATE_INT(4));
    test_instr_encode_decode_synthetic(dc, instr);

    instr = INSTR_CREATE_movs(dc, opnd_create_reg(DR_REG_R0), opnd_create_reg(DR_REG_R1));
    test_instr_encode_decode_synthetic(dc, instr);
}
#endif

#ifdef AARCH64
static void
test_instr_create_encode_decode_synthetic_aarch64(void *dc)
{
    instr_t *instr;

    instr = INSTR_CREATE_add(dc, opnd_create_reg(DR_REG_X0), opnd_create_reg(DR_REG_SP),
                             opnd_create_reg(DR_REG_X1));
    test_instr_encode_decode_synthetic(dc, instr);

    instr = INSTR_CREATE_sub(dc, opnd_create_reg(DR_REG_X0), opnd_create_reg(DR_REG_SP),
                             opnd_create_reg(DR_REG_X1));
    test_instr_encode_decode_synthetic(dc, instr);

    instr =
        INSTR_CREATE_adds_imm(dc, opnd_create_reg(DR_REG_W0), opnd_create_reg(DR_REG_W1),
                              opnd_create_immed_int(0, OPSZ_12b), OPND_CREATE_INT8(0));
    test_instr_encode_decode_synthetic(dc, instr);

    instr = INSTR_CREATE_adr(dc, opnd_create_reg(DR_REG_X1),
                             OPND_CREATE_ABSMEM((void *)0x0000000010010208, OPSZ_0));
    test_instr_encode_decode_synthetic(dc, instr);

    instr = INSTR_CREATE_ldpsw(
        dc, opnd_create_reg(DR_REG_X1), opnd_create_reg(DR_REG_X2),
        opnd_create_reg(DR_REG_X0),
        opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_NULL, 0, false, 4, 0, OPSZ_8),
        OPND_CREATE_INT(4));
    test_instr_encode_decode_synthetic(dc, instr);
}
#endif

#ifdef RISCV64
static void
test_instr_create_encode_decode_synthetic_riscv64(void *dc)
{
    instr_t *instr;

    instr =
        INSTR_CREATE_lwu(dc, opnd_create_reg(DR_REG_A0),
                         opnd_create_base_disp(DR_REG_X31, DR_REG_NULL, 0, 0, OPSZ_4));
    test_instr_encode_decode_synthetic(dc, instr);

    instr = INSTR_CREATE_sw(
        dc, opnd_create_base_disp(DR_REG_X31, DR_REG_NULL, 0, (1 << 11) - 1, OPSZ_4),
        opnd_create_reg(DR_REG_X0));
    test_instr_encode_decode_synthetic(dc, instr);

    instr = INSTR_CREATE_flw(dc, opnd_create_reg(DR_REG_F0),
                             opnd_create_base_disp(DR_REG_A1, DR_REG_NULL, 0, 0, OPSZ_4));
    test_instr_encode_decode_synthetic(dc, instr);

    instr =
        INSTR_CREATE_lr_d(dc, opnd_create_reg(DR_REG_X0),
                          opnd_create_base_disp(DR_REG_X31, DR_REG_NULL, 0, 0, OPSZ_8),
                          opnd_create_immed_int(0b10, OPSZ_2b));
    test_instr_encode_decode_synthetic(dc, instr);

    instr = INSTR_CREATE_fmadd_d(dc, opnd_create_reg(DR_REG_F31),
                                 opnd_create_immed_int(0b000, OPSZ_3b),
                                 opnd_create_reg(DR_REG_F0), opnd_create_reg(DR_REG_F2),
                                 opnd_create_reg(DR_REG_F3));
    test_instr_encode_decode_synthetic(dc, instr);
}
#endif

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

    print("All synthetic tests are done.\n");
    dr_standalone_exit();
    return 0;
}
