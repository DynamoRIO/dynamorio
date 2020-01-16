/* **********************************************************
 * Copyright (c) 2020 Google, Inc. All rights reserved.
 * Copyright (c) 2016 ARM Limited. All rights reserved.
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

#include "../globals.h"
#include "arch.h"
#include "instr.h"
#include "decode.h"
#include "decode_private.h"
#include "disassemble.h"
#include "codec.h"

/* Extra logging for encoding */
#define ENC_LEVEL 6

/* Order corresponds to DR_REG_ enum. */
const char *const reg_names[] = {
    "<NULL>",      "<invalid>", "x0",  "x1",  "x2",   "x3",   "x4",   "x5",
    "x6",          "x7",        "x8",  "x9",  "x10",  "x11",  "x12",  "x13",
    "x14",         "x15",       "x16", "x17", "x18",  "x19",  "x20",  "x21",
    "x22",         "x23",       "x24", "x25", "x26",  "x27",  "x28",  "x29",
    "x30",         "sp",        "xzr", "w0",  "w1",   "w2",   "w3",   "w4",
    "w5",          "w6",        "w7",  "w8",  "w9",   "w10",  "w11",  "w12",
    "w13",         "w14",       "w15", "w16", "w17",  "w18",  "w19",  "w20",
    "w21",         "w22",       "w23", "w24", "w25",  "w26",  "w27",  "w28",
    "w29",         "w30",       "wsp", "wzr", "q0",   "q1",   "q2",   "q3",
    "q4",          "q5",        "q6",  "q7",  "q8",   "q9",   "q10",  "q11",
    "q12",         "q13",       "q14", "q15", "q16",  "q17",  "q18",  "q19",
    "q20",         "q21",       "q22", "q23", "q24",  "q25",  "q26",  "q27",
    "q28",         "q29",       "q30", "q31", "d0",   "d1",   "d2",   "d3",
    "d4",          "d5",        "d6",  "d7",  "d8",   "d9",   "d10",  "d11",
    "d12",         "d13",       "d14", "d15", "d16",  "d17",  "d18",  "d19",
    "d20",         "d21",       "d22", "d23", "d24",  "d25",  "d26",  "d27",
    "d28",         "d29",       "d30", "d31", "s0",   "s1",   "s2",   "s3",
    "s4",          "s5",        "s6",  "s7",  "s8",   "s9",   "s10",  "s11",
    "s12",         "s13",       "s14", "s15", "s16",  "s17",  "s18",  "s19",
    "s20",         "s21",       "s22", "s23", "s24",  "s25",  "s26",  "s27",
    "s28",         "s29",       "s30", "s31", "h0",   "h1",   "h2",   "h3",
    "h4",          "h5",        "h6",  "h7",  "h8",   "h9",   "h10",  "h11",
    "h12",         "h13",       "h14", "h15", "h16",  "h17",  "h18",  "h19",
    "h20",         "h21",       "h22", "h23", "h24",  "h25",  "h26",  "h27",
    "h28",         "h29",       "h30", "h31", "b0",   "b1",   "b2",   "b3",
    "b4",          "b5",        "b6",  "b7",  "b8",   "b9",   "b10",  "b11",
    "b12",         "b13",       "b14", "b15", "b16",  "b17",  "b18",  "b19",
    "b20",         "b21",       "b22", "b23", "b24",  "b25",  "b26",  "b27",
    "b28",         "b29",       "b30", "b31", "nzcv", "fpcr", "fpsr", "tpidr_el0",
    "tpidrro_el0", "z0",        "z1",  "z2",  "q3",   "z4",   "z5",   "z6",
    "z7",          "z8",        "z9",  "z10", "z11",  "z12",  "z13",  "z14",
    "z15",         "z16",       "z17", "z18", "z19",  "z20",  "z21",  "z22",
    "z23",         "z24",       "z25", "z26", "z27",  "z28",  "z29",  "z30",
    "z31",         "p0",        "p1",  "p2",  "p3",   "p4",   "p5",   "p6",
    "p7",          "p8",        "p9",  "p10", "p11",  "p12",  "p13",  "p14",
    "p15",
};

/* Maps sub-registers to their containing register. */
/* Order corresponds to DR_REG_ enum. */
const reg_id_t dr_reg_fixer[] = { REG_NULL,
                                  REG_NULL,

#define XREGS                                                                            \
    DR_REG_X0, DR_REG_X1, DR_REG_X2, DR_REG_X3, DR_REG_X4, DR_REG_X5, DR_REG_X6,         \
        DR_REG_X7, DR_REG_X8, DR_REG_X9, DR_REG_X10, DR_REG_X11, DR_REG_X12, DR_REG_X13, \
        DR_REG_X14, DR_REG_X15, DR_REG_X16, DR_REG_X17, DR_REG_X18, DR_REG_X19,          \
        DR_REG_X20, DR_REG_X21, DR_REG_X22, DR_REG_X23, DR_REG_X24, DR_REG_X25,          \
        DR_REG_X26, DR_REG_X27, DR_REG_X28, DR_REG_X29, DR_REG_X30, DR_REG_XSP,          \
        DR_REG_XZR,
                                  XREGS     /* X0-XSP */
                                      XREGS /* W0-WSP */
#undef XREGS

#define QREGS                                                                            \
    DR_REG_Q0, DR_REG_Q1, DR_REG_Q2, DR_REG_Q3, DR_REG_Q4, DR_REG_Q5, DR_REG_Q6,         \
        DR_REG_Q7, DR_REG_Q8, DR_REG_Q9, DR_REG_Q10, DR_REG_Q11, DR_REG_Q12, DR_REG_Q13, \
        DR_REG_Q14, DR_REG_Q15, DR_REG_Q16, DR_REG_Q17, DR_REG_Q18, DR_REG_Q19,          \
        DR_REG_Q20, DR_REG_Q21, DR_REG_Q22, DR_REG_Q23, DR_REG_Q24, DR_REG_Q25,          \
        DR_REG_Q26, DR_REG_Q27, DR_REG_Q28, DR_REG_Q29, DR_REG_Q30, DR_REG_Q31,
                                          QREGS                 /* Q0-Q31*/
                                              QREGS             /* D0-D31 */
                                                  QREGS         /* S0-S31 */
                                                      QREGS     /* H0-H31 */
                                                          QREGS /* B0-B31 */
#undef QREGS

                                                              DR_REG_NZCV,
                                  DR_REG_FPCR,
                                  DR_REG_FPSR,
                                  DR_REG_TPIDR_EL0,
                                  DR_REG_TPIDRRO_EL0 };

#ifdef DEBUG
void
encode_debug_checks(void)
{
    /* FIXME i#1569: NYI */
}
#endif

bool
encoding_possible(decode_info_t *di, instr_t *in, const instr_info_t *ii)
{
    uint enc;

    byte tmp[AARCH64_INSTR_SIZE];
    enc = encode_common(&tmp[0], in, di);
    return enc != ENCFAIL;
}

void
decode_info_init_for_instr(decode_info_t *di, instr_t *instr)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

byte *
instr_encode_arch(dcontext_t *dcontext, instr_t *instr, byte *copy_pc, byte *final_pc,
                  bool check_reachable,
                  bool *has_instr_opnds /*OUT OPTIONAL*/
                      _IF_DEBUG(bool assert_reachable))
{
    decode_info_t di;
    uint enc;

    if (has_instr_opnds != NULL)
        *has_instr_opnds = false;

    if (instr_is_label(instr))
        return copy_pc;

    /* First, handle the already-encoded instructions */
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
        /* This is to make decoding reversible. We would not normally encode an OP_xx. */
        if (instr_get_opcode(instr) == OP_xx && instr_num_srcs(instr) > 0 &&
            opnd_is_immed_int(instr_get_src(instr, 0))) {
            enc = (uint)opnd_get_immed_int(instr_get_src(instr, 0));
        } else {
            /* We were unable to encode this instruction. */
            IF_DEBUG({
                char disas_instr[MAX_INSTR_DIS_SZ];
                instr_disassemble_to_buffer(dcontext, instr, disas_instr,
                                            MAX_INSTR_DIS_SZ);
                SYSLOG_INTERNAL_ERROR("Internal Error: Failed to encode instruction:"
                                      " '%s'\n",
                                      disas_instr);
            });
            return NULL;
        }
    }
    *(uint *)copy_pc = enc;
    return copy_pc + 4;
}

byte *
copy_and_re_relativize_raw_instr(dcontext_t *dcontext, instr_t *instr, byte *dst_pc,
                                 byte *final_pc)
{
    /* TODO i#4016: re-relativizing is NYI */
    /* OP_ldstex is always relocatable. */
    ASSERT(instr_raw_bits_valid(instr) || instr_get_opcode(instr) == OP_ldstex);
    memcpy(dst_pc, instr->bytes, instr->length);
    return dst_pc + instr->length;
}
