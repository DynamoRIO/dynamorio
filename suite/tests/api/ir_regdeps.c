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

#define ALIGN_BYTES 4

static void
test_instr_encode_decode_synthetic(void *dc, instr_t *instr)
{
    /* Encoded synthetic ISA instructions require 4 byte alignment.
     * The largest synthetic encoded instruction has 16 bytes.
     */
    byte ALIGN_VAR(ALIGN_BYTES) bytes[16];

    /* Convert a real ISA instruction to a synthetic ISA (DR_ISA_REGDEPS) instruction.
     */
    instr_t *instr_synthetic_converted = instr_create(dc);
    instr_convert_to_isa_regdeps(dc, instr, instr_synthetic_converted);

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
    instr_t *instr;

    instr = INSTR_CREATE_push(dc, opnd_create_reg(SEG_FS));
    test_instr_encode_decode_synthetic(dc, instr);

    instr = INSTR_CREATE_pop(dc, opnd_create_reg(SEG_FS));
    test_instr_encode_decode_synthetic(dc, instr);

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

    instr_t *tgt = INSTR_CREATE_mov_imm(dc, opnd_create_reg(DR_REG_XAX),
                                        opnd_create_immed_int(0xdeadbeef, OPSZ_PTR));
    instr = INSTR_CREATE_jmp_ind(dc, opnd_create_mem_instr(tgt, 2, OPSZ_PTR));
    test_instr_encode_decode_synthetic(dc, instr);
    instr_destroy(dc, tgt);

    instr =
        INSTR_CREATE_bsf(dc, opnd_create_reg(DR_REG_EAX), opnd_create_reg(DR_REG_ECX));
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
