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

#ifdef X64
#	define REGARG(reg) opnd_create_reg(DR_REG_##reg)
#	define REGARG_PARTIAL(reg, sz) opnd_create_reg_partial(DR_REG_##reg, sz)
#endif

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

#ifdef X64
static void
test_avx512_bf16_encoding(void *dc, byte *buf)
{
    /* These tests are taken from
     * binutils-2.37.90/gas/testsuite/gas/i386/x86-64-avx512_bf16.{s,d}
     */
    // clang-format off
    // 62 02 17 40 72 f4     vcvtne2ps2bf16 %zmm28,%zmm29,%zmm30
    // 62 22 17 47 72 b4 f5 00 00 00 10  vcvtne2ps2bf16 0x10000000\(%rbp,%r14,8\),%zmm29,%zmm30\{%k7\}
    // 62 42 17 50 72 31     vcvtne2ps2bf16 \(%r9\)\{1to16\},%zmm29,%zmm30
    // 62 62 17 40 72 71 7f  vcvtne2ps2bf16 0x1fc0\(%rcx\),%zmm29,%zmm30
    // 62 62 17 d7 72 b2 00 e0 ff ff   vcvtne2ps2bf16 -0x2000\(%rdx\)\{1to16\},%zmm29,%zmm30\{%k7\}\{z\}
    // 62 02 7e 48 72 f5     vcvtneps2bf16 %zmm29,%ymm30
    // 62 22 7e 4f 72 b4 f5 00 00 00 10  vcvtneps2bf16 0x10000000\(%rbp,%r14,8\),%ymm30\{%k7\}
    // 62 42 7e 58 72 31     vcvtneps2bf16 \(%r9\)\{1to16\},%ymm30
    // 62 62 7e 48 72 71 7f  vcvtneps2bf16 0x1fc0\(%rcx\),%ymm30
    // 62 62 7e df 72 b2 00 e0 ff ff   vcvtneps2bf16 -0x2000\(%rdx\)\{1to16\},%ymm30\{%k7\}\{z\}
    // 62 02 16 40 52 f4     vdpbf16ps %zmm28,%zmm29,%zmm30
    // 62 22 16 47 52 b4 f5 00 00 00 10  vdpbf16ps 0x10000000\(%rbp,%r14,8\),%zmm29,%zmm30\{%k7\}
    // 62 42 16 50 52 31     vdpbf16ps \(%r9\)\{1to16\},%zmm29,%zmm30
    // 62 62 16 40 52 71 7f  vdpbf16ps 0x1fc0\(%rcx\),%zmm29,%zmm30
    // 62 62 16 d7 52 b2 00 e0 ff ff   vdpbf16ps -0x2000\(%rdx\)\{1to16\},%zmm29,%zmm30\{%k7\}\{z\}
    byte out00[] = { 0x62, 0x02, 0x17, 0x40, 0x72, 0xf4,}; //
    byte out01[] = { 0x62, 0x22, 0x17, 0x47, 0x72, 0xb4, 0xf5, 0x00, 0x00, 0x00, 0x10,}; //
    byte out02[] = { 0x62, 0x42, 0x17, 0x50, 0x72, 0x31,}; //
    byte out03[] = { 0x62, 0x62, 0x17, 0x40, 0x72, 0x71, 0x7f,}; //
    byte out04[] = { 0x62, 0x62, 0x17, 0xd7, 0x72, 0xb2, 0x00, 0xe0, 0xff, 0xff,}; //
    byte out05[] = { 0x62, 0x02, 0x7e, 0x48, 0x72, 0xf5,}; //
    byte out06[] = { 0x62, 0x22, 0x7e, 0x4f, 0x72, 0xb4, 0xf5, 0x00, 0x00, 0x00, 0x10,}; //
    byte out07[] = { 0x62, 0x42, 0x7e, 0x58, 0x72, 0x31,}; //
    byte out08[] = { 0x62, 0x62, 0x7e, 0x48, 0x72, 0x71, 0x7f,}; //
    byte out09[] = { 0x62, 0x62, 0x7e, 0xdf, 0x72, 0xb2, 0x00, 0xe0, 0xff, 0xff,}; //
    byte out10[] = { 0x62, 0x02, 0x16, 0x40, 0x52, 0xf4,}; //
    byte out11[] = { 0x62, 0x22, 0x16, 0x47, 0x52, 0xb4, 0xf5, 0x00, 0x00, 0x00, 0x10,}; //
    byte out12[] = { 0x62, 0x42, 0x16, 0x50, 0x52, 0x31,}; //
    byte out13[] = { 0x62, 0x62, 0x16, 0x40, 0x52, 0x71, 0x7f,}; //
    byte out14[] = { 0x62, 0x62, 0x16, 0xd7, 0x52, 0xb2, 0x00, 0xe0, 0xff, 0xff,}; //
    opnd_t test0[][4] = {
        { REGARG(ZMM30), REGARG(K0), REGARG(ZMM29), REGARG(ZMM28) },
        { REGARG(ZMM30), REGARG(K7), REGARG(ZMM29), opnd_create_base_disp(DR_REG_RBP, DR_REG_R14 , 8, 0x10000000, OPSZ_64) },
        { REGARG(ZMM30), REGARG(K0), REGARG(ZMM29), opnd_create_base_disp(DR_REG_R9 , DR_REG_NULL, 0,          0, OPSZ_4) },
        { REGARG(ZMM30), REGARG(K0), REGARG(ZMM29), opnd_create_base_disp(DR_REG_RCX, DR_REG_NULL, 0,     0x1fc0, OPSZ_64) },
        { REGARG(ZMM30), REGARG(K7), REGARG(ZMM29), opnd_create_base_disp(DR_REG_RDX, DR_REG_NULL, 0,    -0x2000, OPSZ_4) },
    };
    opnd_t test1[][3] = {
        { REGARG_PARTIAL(ZMM30, OPSZ_32), REGARG(K0), REGARG(ZMM29) },
        { REGARG_PARTIAL(ZMM30, OPSZ_32), REGARG(K7), opnd_create_base_disp(DR_REG_RBP, DR_REG_R14,  8,  0x10000000, OPSZ_64) },
        { REGARG_PARTIAL(ZMM30, OPSZ_32), REGARG(K0), opnd_create_base_disp(DR_REG_R9,  DR_REG_NULL, 0,  0,          OPSZ_4) },
        { REGARG_PARTIAL(ZMM30, OPSZ_32), REGARG(K0), opnd_create_base_disp(DR_REG_RCX, DR_REG_NULL, 0,  0x1fc0,     OPSZ_64) },
        { REGARG_PARTIAL(ZMM30, OPSZ_32), REGARG(K7), opnd_create_base_disp(DR_REG_RDX, DR_REG_NULL, 0, -0x2000,     OPSZ_4) },
    };
    // clang-format on

    // TODO: remove once these are available in some header
#    define PREFIX_EVEX_z 0x000800000
    uint prefixes[] = { 0, 0, 0, 0, PREFIX_EVEX_z };

    int expected_sizes[] = {
        sizeof(out00), sizeof(out01), sizeof(out02), sizeof(out03), sizeof(out04),
        sizeof(out05), sizeof(out06), sizeof(out07), sizeof(out08), sizeof(out09),
        sizeof(out10), sizeof(out11), sizeof(out12), sizeof(out13), sizeof(out14),
    };

    byte *expected_output[] = {
        out00, out01, out02, out03, out04, out05, out06, out07,
        out08, out09, out10, out11, out12, out13, out14,
    };

    for (int i = 0; i < 15; i++) {
        instr_t *instr;
        int set = i / 5;
        int idx = i % 5;
        if (set == 0) {
            instr = INSTR_CREATE_vcvtne2ps2bf16_mask(dc, test0[idx][0], test0[idx][1],
                                                     test0[idx][2], test0[idx][3]);
        } else if (set == 1) {
            instr = INSTR_CREATE_vcvtneps2bf16_mask(dc, test1[idx][0], test1[idx][1],
                                                    test1[idx][2]);
        } else if (set == 2) {
            instr = INSTR_CREATE_vdpbf16ps_mask(dc, test0[idx][0], test0[idx][1],
                                                test0[idx][2], test0[idx][3]);
        }
        instr_set_prefix_flag(instr, prefixes[idx]);
        instr_encode(dc, instr, buf);
        instr_reset(dc, instr);
        decode(dc, buf, instr);
        test_instr_encode_decode_synthetic(dc, instr);
        for (int b = 0; b < expected_sizes[i]; b++) {
            ASSERT(expected_output[i][b] == buf[b]);
        }
        // instr_destroy() called by test_instr_encode_decode_synthetic().
    }
}

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
     * DR_REG_K0, DR_REG_K7, and the DR_REG_ZMM_ registers are some of these "problematic"
     * registers with enum value >=256. test_avx512_bf16_encoding() uses them to test our
     * fix.
     */
    test_avx512_bf16_encoding(dc, buf);
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
    ASSERT(strncmp(get_virtual_register_name((reg_id_t)0), rv0, sizeof(rv0)) == 0);

    const char rv1[] = "rv1";
    ASSERT(strncmp(get_virtual_register_name((reg_id_t)1), rv1, sizeof(rv1)) == 0);

    const char rv187[] = "rv187";
    ASSERT(strncmp(get_virtual_register_name((reg_id_t)187), rv187, sizeof(rv187)) == 0);

    const char rv255[] = "rv255";
    ASSERT(strncmp(get_virtual_register_name((reg_id_t)255), rv255, sizeof(rv255)) == 0);
}

int
main(int argc, char *argv[])
{
    void *dcontext = dr_standalone_init();
    ASSERT(!dr_running_under_dynamorio());

#ifdef X64
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
