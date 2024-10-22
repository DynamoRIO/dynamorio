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
#include "arch.h"
#include "decode.h"
#include "decode_private.h"
#include "disassemble.h"
#include "codec.h"

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
    "ft10", "ft11", "fcsr", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
    "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "v16", "v17", "v18",
    "v19", "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29",
    "v30", "v31"
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
    DR_REG_VR0,  DR_REG_VR1,  DR_REG_VR2,  DR_REG_VR3,  DR_REG_VR4,  DR_REG_VR5,
    DR_REG_VR6,  DR_REG_VR7,  DR_REG_VR8,  DR_REG_VR9,  DR_REG_VR10, DR_REG_VR11,
    DR_REG_VR12, DR_REG_VR13, DR_REG_VR14, DR_REG_VR15, DR_REG_VR16, DR_REG_VR17,
    DR_REG_VR18, DR_REG_VR19, DR_REG_VR20, DR_REG_VR21, DR_REG_VR22, DR_REG_VR23,
    DR_REG_VR24, DR_REG_VR25, DR_REG_VR26, DR_REG_VR27, DR_REG_VR28, DR_REG_VR29,
    DR_REG_VR30, DR_REG_VR31,
};
/* clang-format on */

/* Maps real ISA registers to their corresponding virtual DR_ISA_REGDEPS register.
 * Note that we map real sub-registers to their corresponding containing virtual register.
 * Same size as dr_reg_fixer[], keep them synched.
 */
const reg_id_t d_r_reg_id_to_virtual[] = {
    DR_REG_NULL, /* DR_REG_NULL */
    DR_REG_NULL, /* DR_REG_NULL */
    DR_REG_V0,   /* DR_REG_X0 */
    DR_REG_V1,   /* DR_REG_X1 */
    DR_REG_V2,   /* DR_REG_X2 */
    DR_REG_V3,   /* DR_REG_X3 */
    DR_REG_V4,   /* DR_REG_X4 */
    DR_REG_V5,   /* DR_REG_X5 */
    DR_REG_V6,   /* DR_REG_X6 */
    DR_REG_V7,   /* DR_REG_X7 */
    DR_REG_V8,   /* DR_REG_X8 */
    DR_REG_V9,   /* DR_REG_X9 */
    DR_REG_V10,  /* DR_REG_X10 */
    DR_REG_V11,  /* DR_REG_X11 */
    DR_REG_V12,  /* DR_REG_X12 */
    DR_REG_V13,  /* DR_REG_X13 */
    DR_REG_V14,  /* DR_REG_X14 */
    DR_REG_V15,  /* DR_REG_X15 */
    DR_REG_V16,  /* DR_REG_X16 */
    DR_REG_V17,  /* DR_REG_X17 */
    DR_REG_V18,  /* DR_REG_X18 */
    DR_REG_V19,  /* DR_REG_X19 */
    DR_REG_V20,  /* DR_REG_X20 */
    DR_REG_V21,  /* DR_REG_X21 */
    DR_REG_V22,  /* DR_REG_X22 */
    DR_REG_V23,  /* DR_REG_X23 */
    DR_REG_V24,  /* DR_REG_X24 */
    DR_REG_V25,  /* DR_REG_X25 */
    DR_REG_V26,  /* DR_REG_X26 */
    DR_REG_V27,  /* DR_REG_X27 */
    DR_REG_V28,  /* DR_REG_X28 */
    DR_REG_V29,  /* DR_REG_X29 */
    DR_REG_V30,  /* DR_REG_X30 */
    DR_REG_V31,  /* DR_REG_X31 */
    DR_REG_V32,  /* DR_REG_PC */

    DR_REG_V33, /* DR_REG_F0 */
    DR_REG_V34, /* DR_REG_F1 */
    DR_REG_V35, /* DR_REG_F2 */
    DR_REG_V36, /* DR_REG_F3 */
    DR_REG_V37, /* DR_REG_F4 */
    DR_REG_V38, /* DR_REG_F5 */
    DR_REG_V39, /* DR_REG_F6 */
    DR_REG_V40, /* DR_REG_F7 */
    DR_REG_V41, /* DR_REG_F8 */
    DR_REG_V42, /* DR_REG_F9 */
    DR_REG_V43, /* DR_REG_F10 */
    DR_REG_V44, /* DR_REG_F11 */
    DR_REG_V45, /* DR_REG_F12 */
    DR_REG_V46, /* DR_REG_F13 */
    DR_REG_V47, /* DR_REG_F14 */
    DR_REG_V48, /* DR_REG_F15 */
    DR_REG_V49, /* DR_REG_F16 */
    DR_REG_V50, /* DR_REG_F17 */
    DR_REG_V51, /* DR_REG_F18 */
    DR_REG_V52, /* DR_REG_F19 */
    DR_REG_V53, /* DR_REG_F20 */
    DR_REG_V54, /* DR_REG_F21 */
    DR_REG_V55, /* DR_REG_F22 */
    DR_REG_V56, /* DR_REG_F23 */
    DR_REG_V57, /* DR_REG_F24 */
    DR_REG_V58, /* DR_REG_F25 */
    DR_REG_V59, /* DR_REG_F26 */
    DR_REG_V60, /* DR_REG_F27 */
    DR_REG_V61, /* DR_REG_F28 */
    DR_REG_V62, /* DR_REG_F29 */
    DR_REG_V63, /* DR_REG_F30 */
    DR_REG_V64, /* DR_REG_F31 */
    DR_REG_V65, /* DR_REG_FCSR */

    DR_REG_V66, /* DR_REG_VR0 */
    DR_REG_V67, /* DR_REG_VR1 */
    DR_REG_V68, /* DR_REG_VR2 */
    DR_REG_V69, /* DR_REG_VR3 */
    DR_REG_V70, /* DR_REG_VR4 */
    DR_REG_V71, /* DR_REG_VR5 */
    DR_REG_V72, /* DR_REG_VR6 */
    DR_REG_V73, /* DR_REG_VR7 */
    DR_REG_V74, /* DR_REG_VR8 */
    DR_REG_V75, /* DR_REG_VR9 */
    DR_REG_V76, /* DR_REG_VR10 */
    DR_REG_V77, /* DR_REG_VR11 */
    DR_REG_V78, /* DR_REG_VR12 */
    DR_REG_V79, /* DR_REG_VR13 */
    DR_REG_V80, /* DR_REG_VR14 */
    DR_REG_V81, /* DR_REG_VR15 */
    DR_REG_V82, /* DR_REG_VR16 */
    DR_REG_V83, /* DR_REG_VR17 */
    DR_REG_V84, /* DR_REG_VR18 */
    DR_REG_V85, /* DR_REG_VR19 */
    DR_REG_V86, /* DR_REG_VR20 */
    DR_REG_V87, /* DR_REG_VR21 */
    DR_REG_V88, /* DR_REG_VR22 */
    DR_REG_V89, /* DR_REG_VR23 */
    DR_REG_V90, /* DR_REG_VR24 */
    DR_REG_V91, /* DR_REG_VR25 */
    DR_REG_V92, /* DR_REG_VR26 */
    DR_REG_V93, /* DR_REG_VR27 */
    DR_REG_V94, /* DR_REG_VR28 */
    DR_REG_V95, /* DR_REG_VR29 */
    DR_REG_V96, /* DR_REG_VR30 */
    DR_REG_V97, /* DR_REG_VR31 */
};

#ifdef DEBUG
void
encode_debug_checks(void)
{
    CLIENT_ASSERT(sizeof(d_r_reg_id_to_virtual) == sizeof(dr_reg_fixer),
                  "register to virtual register map size error");

    /* FIXME i#3544: NYI */
}
#endif

bool
encoding_possible(decode_info_t *di, instr_t *in, const instr_info_t *ii)
{
    uint enc;

    byte tmp[RISCV64_INSTR_SIZE];
    enc = encode_common(&tmp[0], in, di);
    return enc != ENCFAIL;
}

void
decode_info_init_for_instr(decode_info_t *di, instr_t *instr)
{
    di->check_reachable = false;
}

byte *
instr_encode_arch(dcontext_t *dcontext, instr_t *instr, byte *copy_pc, byte *final_pc,
                  bool check_reachable,
                  bool *has_instr_opnds /*OUT OPTIONAL*/
                      _IF_DEBUG(bool assert_reachable))
{
    decode_info_t di;
    uint enc;
    int instr_length;

    if (has_instr_opnds != NULL) {
        *has_instr_opnds = false;
    }

    if (instr_is_label(instr)) {
        return copy_pc;
    }

    /* First, handle the already-encoded instructions. */
    if (instr_raw_bits_valid(instr)) {
        CLIENT_ASSERT(check_reachable,
                      "internal encode error: cannot encode raw "
                      "bits and ignore reachability");
        /* Copy raw bits, possibly re-relativizing */
        return copy_and_re_relativize_raw_instr(dcontext, instr, copy_pc, final_pc);
    }
    CLIENT_ASSERT(instr_operands_valid(instr), "instr_encode error: operands invalid");
    di.check_reachable = check_reachable;
    enc = encode_common(final_pc, instr, &di);
    if (enc == ENCFAIL) {
        IF_DEBUG({
            if (assert_reachable) {
                char disas_instr[MAX_INSTR_DIS_SZ];
                instr_disassemble_to_buffer(dcontext, instr, disas_instr,
                                            MAX_INSTR_DIS_SZ);
                SYSLOG_INTERNAL_ERROR("Internal Error: Failed to encode instruction:"
                                      " '%s'",
                                      disas_instr);
            }
        });
        return NULL;
    }
    instr_length = instr_length_arch(dcontext, instr);
    if (instr_length == RISCV64_INSTR_COMPRESSED_SIZE) {
        *(ushort *)copy_pc = (ushort)enc;
    } else {
        ASSERT(instr_length == RISCV64_INSTR_SIZE);
        *(uint *)copy_pc = enc;
    }

    return copy_pc + instr_length;
}

byte *
copy_and_re_relativize_raw_instr(dcontext_t *dcontext, instr_t *instr, byte *dst_pc,
                                 byte *final_pc)
{
    /* TODO i#3544: re-relativizing is NYI */
    ASSERT(instr_raw_bits_valid(instr));
    memcpy(dst_pc, instr->bytes, instr->length);
    return dst_pc + instr->length;
}
