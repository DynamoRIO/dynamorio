/* **********************************************************
 * Copyright (c) 2022 Rivos, Inc. All rights reserved.
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
 * * Neither the name of Rivos, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL RIVOS, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "../globals.h"
#include "decode.h"

/* Order corresponds to DR_REG_ enum. */
/* clang-format off */
const char *const reg_names[] = {
    "<NULL>", "<invalid>",
    "zero", "ra", "sp", "gp",  "tp",  "t0", "t1", "t2", "fp", "s1", "a0",
    "a1",   "a2", "a3", "a4",  "a5",  "a6", "a7", "s2", "s3", "s4", "s5", "s6",
    "s7",   "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6", "pc",
    "ft0",  "ft1",  "ft2", "ft3", "ft4", "ft5", "ft6",  "ft7",  "fs0", "fs1",
    "fa0",  "fa1",  "fa2", "fa3", "fa4", "fa5", "fa6",  "fa7",  "fs2", "fs3",
    "fs4",  "fs5",  "fs6", "fs7", "fs8", "fs9", "fs10", "fs11", "ft8", "ft9",
    "ft10", "ft11", "fcsr",
};


/* Maps sub-registers to their containing register. */
/* Order corresponds to DR_REG_ enum. */
const reg_id_t dr_reg_fixer[] = { REG_NULL,
                                  REG_NULL,
    DR_REG_X0,  DR_REG_X1,  DR_REG_X2,  DR_REG_X3,  DR_REG_X4,  DR_REG_X5,
    DR_REG_X6,  DR_REG_X7,  DR_REG_X8,  DR_REG_X9,  DR_REG_X10, DR_REG_X11,
    DR_REG_X12, DR_REG_X13, DR_REG_X14, DR_REG_X15, DR_REG_X16, DR_REG_X17,
    DR_REG_X18, DR_REG_X19, DR_REG_X20, DR_REG_X21, DR_REG_X22, DR_REG_X23,
    DR_REG_X24, DR_REG_X25, DR_REG_X26, DR_REG_X27, DR_REG_X28, DR_REG_X29,
    DR_REG_X30, DR_REG_X31, DR_REG_PC,
    DR_REG_F0,  DR_REG_F1,  DR_REG_F2,  DR_REG_F3,  DR_REG_F4,  DR_REG_F5,
    DR_REG_F6,  DR_REG_F7,  DR_REG_F8,  DR_REG_F9,  DR_REG_F10, DR_REG_F11,
    DR_REG_F12, DR_REG_F13, DR_REG_F14, DR_REG_F15, DR_REG_F16, DR_REG_F17,
    DR_REG_F18, DR_REG_F19, DR_REG_F20, DR_REG_F21, DR_REG_F22, DR_REG_F23,
    DR_REG_F24, DR_REG_F25, DR_REG_F26, DR_REG_F27, DR_REG_F28, DR_REG_F29,
    DR_REG_F30, DR_REG_F31, DR_REG_FCSR,
};
/* clang-format on */

#ifdef DEBUG
void
encode_debug_checks(void)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}
#endif

bool
encoding_possible(decode_info_t *di, instr_t *in, const instr_info_t *ii)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

void
decode_info_init_for_instr(decode_info_t *di, instr_t *instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

/* FIXME: Code generation. */
#define R_type(funct7, rs2, rs1, funct3, rd, opcode)    ((funct7)<<25 | (rs2)<<20 | (rs1)<<15 | (funct3)<<12 | (rd)<<7 | (opcode))
#define I_type(imm12, rs1, funct3, rd, opcode)    ((imm12)<<20 | (rs1)<<15 | (funct3)<<12 | (rd)<<7 | (opcode))
#define S_type(imm12, rs2, rs1, funct3, opcode)    (((imm12)>>5)<<25 | (rs2)<<20 | (rs1)<<15 | (funct3)<<12 | ((imm12)&31)<<7 | (opcode))
#define B_type(imm13, rs2, rs1, funct3, opcode)      ((((imm13)>>12)&1)<<31 | (((imm13)>>5)&63)<<25 | (rs2)<<20 | (rs1)<<15 | (funct3)<<12 | (((imm13)>>1)&15)<<8 | (((imm13)>>11)&1)<<7 | (opcode))
#define U_type(imm32, rd, opcode)    (((imm32)>>12)<<12 | (rd)<<7 | (opcode))
#define J_type(imm21, rd, opcode)    ((((imm21)>>20)&1)<<31 | (((imm21)>>1)&0b1111111111)<<21 | (((imm21)>>11)&1)<<20 | (((imm21)>>12)&0b11111111)<<12 | (rd)<<7 | (opcode))

#define LD(rd, rs1, imm12)      I_type(imm12, rs1, 0b011, rd, 0b0000011)
#define SD(rs2, rs1, imm12)     S_type(imm12, rs2, rs1, 0b011, 0b0100011)
#define JAL(rd, imm21)          J_type(imm21, rd, 0b1101111)
#define JALR(rd, rs1, imm12)    I_type(imm12, rs1, 0b000, rd, 0b1100111)
#define ADD(rd, rs1, rs2)       R_type(0b0000000, rs2, rs1, 0b000, rd, 0b0110011)
#define ADDI(rd, rs1, imm12)    I_type((imm12)&0b111111111111, rs1, 0b000, rd, 0b0010011)
#define AUIPC(rd, imm20)        U_type((imm20)<<12, rd, 0b0010111)

byte *
instr_encode_arch(dcontext_t *dcontext, instr_t *instr, byte *copy_pc, byte *final_pc,
                  bool check_reachable,
                  bool *has_instr_opnds /*OUT OPTIONAL*/
                      _IF_DEBUG(bool assert_reachable))
{
    /* FIXME i#3544: Incomplete */
    uint enc;

    if (has_instr_opnds != NULL)
        *has_instr_opnds = false;

    if (instr_is_label(instr))
        return copy_pc;

    switch (instr->opcode) {
        case OP_c_nop:
            enc = 0x1;
            break;
        case OP_ecall:
            enc = 0x73;
            break;
        case OP_addi:
            enc = ADDI(instr->dsts[0].value.reg_and_element_size.reg,
                       instr->src0.value.reg_and_element_size.reg,
                       instr->srcs[0].value.immed_int);
            break;
        case OP_add:
            enc = ADD(instr->dsts[0].value.reg_and_element_size.reg,
                      instr->src0.value.reg_and_element_size.reg,
                      instr->srcs[0].value.reg_and_element_size.reg);
            break;
        case OP_ld:
            enc = LD(instr->dsts[0].value.reg_and_element_size.reg,
                     instr->src0.value.base_disp.base_reg,
                     instr->src0.value.base_disp.disp);
            break;
        case OP_sd:
            enc = SD(instr->dsts[0].value.reg_and_element_size.reg,
                     instr->src0.value.base_disp.base_reg,
                     instr->src0.value.base_disp.disp);
            break;
        case OP_jal:
            uint raw = (int)(instr->src0.value.pc - final_pc);
            enc = JAL(instr->dsts[0].value.reg_and_element_size.reg, raw);
            break;
        case OP_jalr:
            enc = JALR(instr->dsts[0].value.reg_and_element_size.reg,
                       instr->src0.value.reg_and_element_size.reg,
                       instr->srcs[0].value.immed_int);
            break;
        case OP_auipc:
            enc = AUIPC(instr->dsts[0].value.reg_and_element_size.reg,
                       instr->src0.value.immed_int);
            break;
        default:
            enc = 0x0;
            break;
    }

    *(uint *)copy_pc = enc;
    return copy_pc + 4;
}

byte *
copy_and_re_relativize_raw_instr(dcontext_t *dcontext, instr_t *instr, byte *dst_pc,
                                 byte *final_pc)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}
