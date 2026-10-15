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
    DR_REG_NULL,   /* DR_REG_NULL */
    DR_REG_NULL,   /* DR_REG_NULL */
    DR_REG_VIRT0,  /* DR_REG_X0 */
    DR_REG_VIRT1,  /* DR_REG_X1 */
    DR_REG_VIRT2,  /* DR_REG_X2 */
    DR_REG_VIRT3,  /* DR_REG_X3 */
    DR_REG_VIRT4,  /* DR_REG_X4 */
    DR_REG_VIRT5,  /* DR_REG_X5 */
    DR_REG_VIRT6,  /* DR_REG_X6 */
    DR_REG_VIRT7,  /* DR_REG_X7 */
    DR_REG_VIRT8,  /* DR_REG_X8 */
    DR_REG_VIRT9,  /* DR_REG_X9 */
    DR_REG_VIRT10, /* DR_REG_X10 */
    DR_REG_VIRT11, /* DR_REG_X11 */
    DR_REG_VIRT12, /* DR_REG_X12 */
    DR_REG_VIRT13, /* DR_REG_X13 */
    DR_REG_VIRT14, /* DR_REG_X14 */
    DR_REG_VIRT15, /* DR_REG_X15 */
    DR_REG_VIRT16, /* DR_REG_X16 */
    DR_REG_VIRT17, /* DR_REG_X17 */
    DR_REG_VIRT18, /* DR_REG_X18 */
    DR_REG_VIRT19, /* DR_REG_X19 */
    DR_REG_VIRT20, /* DR_REG_X20 */
    DR_REG_VIRT21, /* DR_REG_X21 */
    DR_REG_VIRT22, /* DR_REG_X22 */
    DR_REG_VIRT23, /* DR_REG_X23 */
    DR_REG_VIRT24, /* DR_REG_X24 */
    DR_REG_VIRT25, /* DR_REG_X25 */
    DR_REG_VIRT26, /* DR_REG_X26 */
    DR_REG_VIRT27, /* DR_REG_X27 */
    DR_REG_VIRT28, /* DR_REG_X28 */
    DR_REG_VIRT29, /* DR_REG_X29 */
    DR_REG_VIRT30, /* DR_REG_X30 */
    DR_REG_VIRT31, /* DR_REG_X31 */
    DR_REG_VIRT32, /* DR_REG_PC */

    DR_REG_VIRT33, /* DR_REG_F0 */
    DR_REG_VIRT34, /* DR_REG_F1 */
    DR_REG_VIRT35, /* DR_REG_F2 */
    DR_REG_VIRT36, /* DR_REG_F3 */
    DR_REG_VIRT37, /* DR_REG_F4 */
    DR_REG_VIRT38, /* DR_REG_F5 */
    DR_REG_VIRT39, /* DR_REG_F6 */
    DR_REG_VIRT40, /* DR_REG_F7 */
    DR_REG_VIRT41, /* DR_REG_F8 */
    DR_REG_VIRT42, /* DR_REG_F9 */
    DR_REG_VIRT43, /* DR_REG_F10 */
    DR_REG_VIRT44, /* DR_REG_F11 */
    DR_REG_VIRT45, /* DR_REG_F12 */
    DR_REG_VIRT46, /* DR_REG_F13 */
    DR_REG_VIRT47, /* DR_REG_F14 */
    DR_REG_VIRT48, /* DR_REG_F15 */
    DR_REG_VIRT49, /* DR_REG_F16 */
    DR_REG_VIRT50, /* DR_REG_F17 */
    DR_REG_VIRT51, /* DR_REG_F18 */
    DR_REG_VIRT52, /* DR_REG_F19 */
    DR_REG_VIRT53, /* DR_REG_F20 */
    DR_REG_VIRT54, /* DR_REG_F21 */
    DR_REG_VIRT55, /* DR_REG_F22 */
    DR_REG_VIRT56, /* DR_REG_F23 */
    DR_REG_VIRT57, /* DR_REG_F24 */
    DR_REG_VIRT58, /* DR_REG_F25 */
    DR_REG_VIRT59, /* DR_REG_F26 */
    DR_REG_VIRT60, /* DR_REG_F27 */
    DR_REG_VIRT61, /* DR_REG_F28 */
    DR_REG_VIRT62, /* DR_REG_F29 */
    DR_REG_VIRT63, /* DR_REG_F30 */
    DR_REG_VIRT64, /* DR_REG_F31 */
    DR_REG_VIRT65, /* DR_REG_FCSR */

    DR_REG_VIRT66, /* DR_REG_VR0 */
    DR_REG_VIRT67, /* DR_REG_VR1 */
    DR_REG_VIRT68, /* DR_REG_VR2 */
    DR_REG_VIRT69, /* DR_REG_VR3 */
    DR_REG_VIRT70, /* DR_REG_VR4 */
    DR_REG_VIRT71, /* DR_REG_VR5 */
    DR_REG_VIRT72, /* DR_REG_VR6 */
    DR_REG_VIRT73, /* DR_REG_VR7 */
    DR_REG_VIRT74, /* DR_REG_VR8 */
    DR_REG_VIRT75, /* DR_REG_VR9 */
    DR_REG_VIRT76, /* DR_REG_VR10 */
    DR_REG_VIRT77, /* DR_REG_VR11 */
    DR_REG_VIRT78, /* DR_REG_VR12 */
    DR_REG_VIRT79, /* DR_REG_VR13 */
    DR_REG_VIRT80, /* DR_REG_VR14 */
    DR_REG_VIRT81, /* DR_REG_VR15 */
    DR_REG_VIRT82, /* DR_REG_VR16 */
    DR_REG_VIRT83, /* DR_REG_VR17 */
    DR_REG_VIRT84, /* DR_REG_VR18 */
    DR_REG_VIRT85, /* DR_REG_VR19 */
    DR_REG_VIRT86, /* DR_REG_VR20 */
    DR_REG_VIRT87, /* DR_REG_VR21 */
    DR_REG_VIRT88, /* DR_REG_VR22 */
    DR_REG_VIRT89, /* DR_REG_VR23 */
    DR_REG_VIRT90, /* DR_REG_VR24 */
    DR_REG_VIRT91, /* DR_REG_VR25 */
    DR_REG_VIRT92, /* DR_REG_VR26 */
    DR_REG_VIRT93, /* DR_REG_VR27 */
    DR_REG_VIRT94, /* DR_REG_VR28 */
    DR_REG_VIRT95, /* DR_REG_VR29 */
    DR_REG_VIRT96, /* DR_REG_VR30 */
    DR_REG_VIRT97, /* DR_REG_VR31 */
};

#ifdef DEBUG
void
encode_debug_checks(void)
{
    CLIENT_ASSERT(sizeof(d_r_reg_id_to_virtual) == sizeof(dr_reg_fixer),
                  "register to virtual register map size error");

    /* TODO i#3544: NYI */
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
