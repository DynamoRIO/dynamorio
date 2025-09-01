/* **********************************************************
 * Copyright (c) 2020-2025 Google, Inc. All rights reserved.
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
/* clang-format off */
const char *const reg_names[] = {
    "<NULL>", "<invalid>",

    "x0",  "x1",  "x2",  "x3",  "x4",  "x5",  "x6",  "x7",  "x8",  "x9",
    "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x18", "x19",
    "x20", "x21", "x22", "x23", "x24", "x25", "x26", "x27", "x28", "x29",
    "x30", "sp",  "xzr",

    "w0",  "w1",  "w2",  "w3",  "w4",  "w5",  "w6",  "w7",  "w8",  "w9",
    "w10", "w11", "w12", "w13", "w14", "w15", "w16", "w17", "w18", "w19",
    "w20", "w21", "w22", "w23", "w24", "w25", "w26", "w27", "w28", "w29",
    "w30", "wsp", "wzr",

    "z0",  "z1",  "z2",  "z3",  "z4",  "z5",  "z6",  "z7",  "z8",  "z9",
    "z10", "z11", "z12", "z13", "z14", "z15", "z16", "z17", "z18", "z19",
    "z20", "z21", "z22", "z23", "z24", "z25", "z26", "z27", "z28", "z29",
    "z30", "z31",

    "q0",  "q1",  "q2",  "q3",  "q4",  "q5",  "q6",  "q7",  "q8",  "q9",
    "q10", "q11", "q12", "q13", "q14", "q15", "q16", "q17", "q18", "q19",
    "q20", "q21", "q22", "q23", "q24", "q25", "q26", "q27", "q28", "q29",
    "q30", "q31",

    "d0",  "d1",  "d2",  "d3",  "d4",  "d5",  "d6",  "d7",  "d8",  "d9",
    "d10", "d11", "d12", "d13", "d14", "d15", "d16", "d17", "d18", "d19",
    "d20", "d21", "d22", "d23", "d24", "d25", "d26", "d27", "d28", "d29",
    "d30", "d31",

    "s0",  "s1",  "s2",  "s3",  "s4",  "s5",  "s6",  "s7",  "s8",  "s9",
    "s10", "s11", "s12", "s13", "s14", "s15", "s16", "s17", "s18", "s19",
    "s20", "s21", "s22", "s23", "s24", "s25", "s26", "s27", "s28", "s29",
    "s30", "s31",

    "h0",  "h1",  "h2",  "h3",  "h4",  "h5",  "h6",  "h7",  "h8",  "h9",
    "h10", "h11", "h12", "h13", "h14", "h15", "h16", "h17", "h18", "h19",
    "h20", "h21", "h22", "h23", "h24", "h25", "h26", "h27", "h28", "h29",
    "h30", "h31",

    "b0",  "b1",  "b2",  "b3",  "b4",  "b5",  "b6",  "b7",  "b8",  "b9",
    "b10", "b11", "b12", "b13", "b14", "b15", "b16", "b17", "b18", "b19",
    "b20", "b21", "b22", "b23", "b24", "b25", "b26", "b27", "b28", "b29",
    "b30", "b31",

    /* System registers */
    "nzcv", "fpcr", "fpsr", "mdccsr_el0", "dbgdtr_el0", "dbgdtrrx_el0",
    "sp_el0", "spsel", "daifset", "daifclr", "currentel", "pan", "uao",
    "ctr_el0", "dczid_el0", "rndr", "rndrrs",  "daif", "dit", "ssbs", "tco",
    "dspsr_el0", "dlr_el0", "pmcr_el0", "pmcntenset_el0", "pmcntenclr_el0",
    "pmovsclr_el0", "pmswinc_el0", "pmselr_el0", "pmceid0_el0", "pmceid1_el0",
    "pmccntr_el0", "pmxevtyper_el0", "pmxevcntr_el0", "pmuserenr_el0",
    "pmovsset_el0", "scxtnum_el0", "cntfrq_el0", "cntpct_el0", "cntp_tval_el0",
    "cntp_ctl_el0", "cntp_cval_el0", "cntv_tval_el0", "cntv_ctl_el0",
    "cntv_cval_el0", "pmevcntr0_el0", "pmevcntr1_el0", "pmevcntr2_el0",
    "pmevcntr3_el0", "pmevcntr4_el0", "pmevcntr5_el0", "pmevcntr6_el0",
    "pmevcntr7_el0", "pmevcntr8_el0", "pmevcntr9_el0", "pmevcntr10_el0",
    "pmevcntr11_el0", "pmevcntr12_el0", "pmevcntr13_el0", "pmevcntr14_el0",
    "pmevcntr15_el0", "pmevcntr16_el0", "pmevcntr17_el0", "pmevcntr18_el0",
    "pmevcntr19_el0", "pmevcntr20_el0", "pmevcntr21_el0", "pmevcntr22_el0",
    "pmevcntr23_el0", "pmevcntr24_el0", "pmevcntr25_el0", "pmevcntr26_el0",
    "pmevcntr27_el0", "pmevcntr28_el0", "pmevcntr29_el0", "pmevcntr30_el0",
    "pmevtyper0_el0", "pmevtyper1_el0", "pmevtyper2_el0", "pmevtyper3_el0",
    "pmevtyper4_el0", "pmevtyper5_el0", "pmevtyper6_el0", "pmevtyper7_el0",
    "pmevtyper8_el0", "pmevtyper9_el0", "pmevtyper10_el0", "pmevtyper11_el0",
    "pmevtyper12_el0", "pmevtyper13_el0", "pmevtyper14_el0", "pmevtyper15_el0",
    "pmevtyper16_el0", "pmevtyper17_el0", "pmevtyper18_el0", "pmevtyper19_el0",
    "pmevtyper20_el0", "pmevtyper21_el0", "pmevtyper22_el0", "pmevtyper23_el0",
    "pmevtyper24_el0", "pmevtyper25_el0", "pmevtyper26_el0", "pmevtyper27_el0",
    "pmevtyper28_el0", "pmevtyper29_el0", "pmevtyper30_el0", "pmccfiltr_el0",
    "spsr_irq", "spsr_abt", "spsr_und", "spsr_fiq", "tpidr_el0", "tpidrro_el0",

    "p0",  "p1",  "p2",  "p3",  "p4",  "p5",  "p6",  "p7",  "p8",  "p9",
    "p10", "p11", "p12", "p13", "p14", "p15",
    "ffr",

    "cntvct_el0", "id_aa64isar0_el1", "id_aa64isar1_el1", "id_aa64isar2_el1",
    "id_aa64pfr0_el1", "id_aa64mmfr1_el1", "id_aa64dfr0_el1", "id_aa64zfr0_el1",
    "id_aa64pfr1_el1", "id_aa64mmfr2_el1", "midr_el1", "mpidr_el1", "revidr_el1",

    "fpmr", "contextidr_el1", "elr_el1", "spsr_el1", "tpidr_el1", "accdata_el1"
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

#define ZREGS                                                                            \
    DR_REG_Z0, DR_REG_Z1, DR_REG_Z2, DR_REG_Z3, DR_REG_Z4, DR_REG_Z5, DR_REG_Z6,         \
        DR_REG_Z7, DR_REG_Z8, DR_REG_Z9, DR_REG_Z10, DR_REG_Z11, DR_REG_Z12, DR_REG_Z13, \
        DR_REG_Z14, DR_REG_Z15, DR_REG_Z16, DR_REG_Z17, DR_REG_Z18, DR_REG_Z19,          \
        DR_REG_Z20, DR_REG_Z21, DR_REG_Z22, DR_REG_Z23, DR_REG_Z24, DR_REG_Z25,          \
        DR_REG_Z26, DR_REG_Z27, DR_REG_Z28, DR_REG_Z29, DR_REG_Z30, DR_REG_Z31,
                                          ZREGS                     /* Z0-Z31 */
                                              ZREGS                 /* Q0-Q31*/
                                                  ZREGS             /* D0-D31 */
                                                      ZREGS         /* S0-S31 */
                                                          ZREGS     /* H0-H31 */
                                                              ZREGS /* B0-B31 */
#undef ZREGS

    DR_REG_NZCV, DR_REG_FPCR, DR_REG_FPSR,
    DR_REG_MDCCSR_EL0, DR_REG_DBGDTR_EL0, DR_REG_DBGDTRRX_EL0, DR_REG_SP_EL0,
    DR_REG_SPSEL, DR_REG_DAIFSET, DR_REG_DAIFCLR, DR_REG_CURRENTEL, DR_REG_PAN,
    DR_REG_UAO, DR_REG_CTR_EL0, DR_REG_DCZID_EL0, DR_REG_RNDR, DR_REG_RNDRRS,
    DR_REG_DAIF, DR_REG_DIT, DR_REG_SSBS, DR_REG_TCO, DR_REG_DSPSR_EL0,
    DR_REG_DLR_EL0, DR_REG_PMCR_EL0, DR_REG_PMCNTENSET_EL0,
    DR_REG_PMCNTENCLR_EL0, DR_REG_PMOVSCLR_EL0,
    DR_REG_PMSWINC_EL0, DR_REG_PMSELR_EL0, DR_REG_PMCEID0_EL0,
    DR_REG_PMCEID1_EL0, DR_REG_PMCCNTR_EL0, DR_REG_PMXEVTYPER_EL0,
    DR_REG_PMXEVCNTR_EL0, DR_REG_PMUSERENR_EL0, DR_REG_PMOVSSET_EL0,
    DR_REG_SCXTNUM_EL0, DR_REG_CNTFRQ_EL0, DR_REG_CNTPCT_EL0,
    DR_REG_CNTP_TVAL_EL0, DR_REG_CNTP_CTL_EL0, DR_REG_CNTP_CVAL_EL0,
    DR_REG_CNTV_TVAL_EL0, DR_REG_CNTV_CTL_EL0, DR_REG_CNTV_CVAL_EL0,
    DR_REG_PMEVCNTR0_EL0, DR_REG_PMEVCNTR1_EL0, DR_REG_PMEVCNTR2_EL0,
    DR_REG_PMEVCNTR3_EL0, DR_REG_PMEVCNTR4_EL0, DR_REG_PMEVCNTR5_EL0,
    DR_REG_PMEVCNTR6_EL0, DR_REG_PMEVCNTR7_EL0, DR_REG_PMEVCNTR8_EL0,
    DR_REG_PMEVCNTR9_EL0, DR_REG_PMEVCNTR10_EL0, DR_REG_PMEVCNTR11_EL0,
    DR_REG_PMEVCNTR12_EL0, DR_REG_PMEVCNTR13_EL0, DR_REG_PMEVCNTR14_EL0,
    DR_REG_PMEVCNTR15_EL0, DR_REG_PMEVCNTR16_EL0, DR_REG_PMEVCNTR17_EL0,
    DR_REG_PMEVCNTR18_EL0, DR_REG_PMEVCNTR19_EL0, DR_REG_PMEVCNTR20_EL0,
    DR_REG_PMEVCNTR21_EL0, DR_REG_PMEVCNTR22_EL0, DR_REG_PMEVCNTR23_EL0,
    DR_REG_PMEVCNTR24_EL0, DR_REG_PMEVCNTR25_EL0, DR_REG_PMEVCNTR26_EL0,
    DR_REG_PMEVCNTR27_EL0, DR_REG_PMEVCNTR28_EL0, DR_REG_PMEVCNTR29_EL0,
    DR_REG_PMEVCNTR30_EL0, DR_REG_PMEVTYPER0_EL0, DR_REG_PMEVTYPER1_EL0,
    DR_REG_PMEVTYPER2_EL0, DR_REG_PMEVTYPER3_EL0, DR_REG_PMEVTYPER4_EL0,
    DR_REG_PMEVTYPER5_EL0, DR_REG_PMEVTYPER6_EL0, DR_REG_PMEVTYPER7_EL0,
    DR_REG_PMEVTYPER8_EL0, DR_REG_PMEVTYPER9_EL0, DR_REG_PMEVTYPER10_EL0,
    DR_REG_PMEVTYPER11_EL0, DR_REG_PMEVTYPER12_EL0, DR_REG_PMEVTYPER13_EL0,
    DR_REG_PMEVTYPER14_EL0, DR_REG_PMEVTYPER15_EL0, DR_REG_PMEVTYPER16_EL0,
    DR_REG_PMEVTYPER17_EL0, DR_REG_PMEVTYPER18_EL0, DR_REG_PMEVTYPER19_EL0,
    DR_REG_PMEVTYPER20_EL0, DR_REG_PMEVTYPER21_EL0, DR_REG_PMEVTYPER22_EL0,
    DR_REG_PMEVTYPER23_EL0, DR_REG_PMEVTYPER24_EL0, DR_REG_PMEVTYPER25_EL0,
    DR_REG_PMEVTYPER26_EL0, DR_REG_PMEVTYPER27_EL0, DR_REG_PMEVTYPER28_EL0,
    DR_REG_PMEVTYPER29_EL0, DR_REG_PMEVTYPER30_EL0, DR_REG_PMCCFILTR_EL0,
    DR_REG_SPSR_IRQ, DR_REG_SPSR_ABT, DR_REG_SPSR_UND, DR_REG_SPSR_FIQ,
    DR_REG_TPIDR_EL0, DR_REG_TPIDRRO_EL0,

    DR_REG_P0, DR_REG_P1, DR_REG_P2, DR_REG_P3, DR_REG_P4, DR_REG_P5,
    DR_REG_P6, DR_REG_P7, DR_REG_P8, DR_REG_P9, DR_REG_P10, DR_REG_P11,
    DR_REG_P12, DR_REG_P13, DR_REG_P14, DR_REG_P15,
    DR_REG_FFR,

    DR_REG_CNTVCT_EL0, DR_REG_ID_AA64ISAR0_EL1, DR_REG_ID_AA64ISAR1_EL1,
    DR_REG_ID_AA64ISAR2_EL1, DR_REG_ID_AA64PFR0_EL1, DR_REG_ID_AA64MMFR1_EL1,
    DR_REG_ID_AA64DFR0_EL1, DR_REG_ID_AA64ZFR0_EL1, DR_REG_ID_AA64PFR1_EL1,
    DR_REG_ID_AA64MMFR2_EL1, DR_REG_MIDR_EL1, DR_REG_MPIDR_EL1, DR_REG_REVIDR_EL1,

    DR_REG_FPMR, DR_REG_CONTEXTIDR_EL1, DR_REG_ELR_EL1, DR_REG_SPSR_EL1,
    DR_REG_TPIDR_EL1, DR_REG_ACCDATA_EL1
};

/* Maps real ISA registers to their corresponding virtual DR_ISA_REGDEPS register.
 * Note that we map real sub-registers to their corresponding containing virtual register.
 * Same size as dr_reg_fixer[], keep them synched.
 */
const reg_id_t d_r_reg_id_to_virtual[] = {
    DR_REG_NULL, /* DR_REG_NULL */
    DR_REG_NULL, /* DR_REG_NULL */

#define VIRTUAL_XREGS                                                                   \
    DR_REG_VIRT0, DR_REG_VIRT1, DR_REG_VIRT2, DR_REG_VIRT3, DR_REG_VIRT4, DR_REG_VIRT5, \
    DR_REG_VIRT6, DR_REG_VIRT7, DR_REG_VIRT8, DR_REG_VIRT9, DR_REG_VIRT10,              \
    DR_REG_VIRT11, DR_REG_VIRT12, DR_REG_VIRT13, DR_REG_VIRT14, DR_REG_VIRT15,          \
    DR_REG_VIRT16, DR_REG_VIRT17, DR_REG_VIRT18, DR_REG_VIRT19, DR_REG_VIRT20,          \
    DR_REG_VIRT21, DR_REG_VIRT22, DR_REG_VIRT23, DR_REG_VIRT24, DR_REG_VIRT25,          \
    DR_REG_VIRT26, DR_REG_VIRT27, DR_REG_VIRT28, DR_REG_VIRT29, DR_REG_VIRT30,          \
    DR_REG_VIRT31, DR_REG_VIRT32,

    VIRTUAL_XREGS /* from DR_REG_X0 to DR_REG_XZR */
    VIRTUAL_XREGS /* from DR_REG_W0 to DR_REG_WZR */
#undef VIRTUAL_XREGS

#define VIRTUAL_ZREGS                                                          \
    DR_REG_VIRT33, DR_REG_VIRT34, DR_REG_VIRT35, DR_REG_VIRT36, DR_REG_VIRT37, \
    DR_REG_VIRT38, DR_REG_VIRT39, DR_REG_VIRT40, DR_REG_VIRT41, DR_REG_VIRT42, \
    DR_REG_VIRT43, DR_REG_VIRT44, DR_REG_VIRT45, DR_REG_VIRT46, DR_REG_VIRT47, \
    DR_REG_VIRT48, DR_REG_VIRT49, DR_REG_VIRT50, DR_REG_VIRT51, DR_REG_VIRT52, \
    DR_REG_VIRT53, DR_REG_VIRT54, DR_REG_VIRT55, DR_REG_VIRT56, DR_REG_VIRT57, \
    DR_REG_VIRT58, DR_REG_VIRT59, DR_REG_VIRT60, DR_REG_VIRT61, DR_REG_VIRT62, \
    DR_REG_VIRT63, DR_REG_VIRT64,

    VIRTUAL_ZREGS /* from DR_REG_Z0 to DR_REG_Z31 */
    VIRTUAL_ZREGS /* from DR_REG_Q0 to DR_REG_Q31 */
    VIRTUAL_ZREGS /* from DR_REG_D0 to DR_REG_D31 */
    VIRTUAL_ZREGS /* from DR_REG_S0 to DR_REG_S31 */
    VIRTUAL_ZREGS /* from DR_REG_H0 to DR_REG_H31 */
    VIRTUAL_ZREGS /* from DR_REG_B0 to DR_REG_B31 */
#undef VIRTUAL_ZREGS

    DR_REG_VIRT65,  /* DR_REG_NZCV */
    DR_REG_VIRT66,  /* DR_REG_FPCR */
    DR_REG_VIRT67,  /* DR_REG_FPSR */
    DR_REG_VIRT68,  /* DR_REG_MDCCSR_EL0 */
    DR_REG_VIRT69,  /* DR_REG_DBGDTR_EL0 */
    DR_REG_VIRT70,  /* DR_REG_DBGDTRRX_EL0 */
    DR_REG_VIRT71,  /* DR_REG_SP_EL0 */
    DR_REG_VIRT72,  /* DR_REG_SPSEL */
    DR_REG_VIRT73,  /* DR_REG_DAIFSET */
    DR_REG_VIRT74,  /* DR_REG_DAIFCLR */
    DR_REG_VIRT75,  /* DR_REG_CURRENTEL */
    DR_REG_VIRT76,  /* DR_REG_PAN */
    DR_REG_VIRT77,  /* DR_REG_UAO */
    DR_REG_VIRT78,  /* DR_REG_CTR_EL0 */
    DR_REG_VIRT79,  /* DR_REG_DCZID_EL0 */
    DR_REG_VIRT80,  /* DR_REG_RNDR */
    DR_REG_VIRT81,  /* DR_REG_RNDRRS */
    DR_REG_VIRT82,  /* DR_REG_DAIF */
    DR_REG_VIRT83,  /* DR_REG_DIT */
    DR_REG_VIRT84,  /* DR_REG_SSBS */
    DR_REG_VIRT85,  /* DR_REG_TCO */
    DR_REG_VIRT86,  /* DR_REG_DSPSR_EL0 */
    DR_REG_VIRT87,  /* DR_REG_DLR_EL0 */
    DR_REG_VIRT88,  /* DR_REG_PMCR_EL0 */
    DR_REG_VIRT89,  /* DR_REG_PMCNTENSET_EL0 */
    DR_REG_VIRT90,  /* DR_REG_PMCNTENCLR_EL0 */
    DR_REG_VIRT91,  /* DR_REG_PMOVSCLR_EL0 */
    DR_REG_VIRT92,  /* DR_REG_PMSWINC_EL0 */
    DR_REG_VIRT93,  /* DR_REG_PMSELR_EL0 */
    DR_REG_VIRT94,  /* DR_REG_PMCEID0_EL0 */
    DR_REG_VIRT95,  /* DR_REG_PMCEID1_EL0 */
    DR_REG_VIRT96,  /* DR_REG_PMCCNTR_EL0 */
    DR_REG_VIRT97,  /* DR_REG_PMXEVTYPER_EL0 */
    DR_REG_VIRT98,  /* DR_REG_PMXEVCNTR_EL0 */
    DR_REG_VIRT99,  /* DR_REG_PMUSERENR_EL0 */
    DR_REG_VIRT100, /* DR_REG_PMOVSSET_EL0 */
    DR_REG_VIRT101, /* DR_REG_SCXTNUM_EL0 */
    DR_REG_VIRT102, /* DR_REG_CNTFRQ_EL0 */
    DR_REG_VIRT103, /* DR_REG_CNTPCT_EL0 */
    DR_REG_VIRT104, /* DR_REG_CNTP_TVAL_EL0 */
    DR_REG_VIRT105, /* DR_REG_CNTP_CTL_EL0 */
    DR_REG_VIRT106, /* DR_REG_CNTP_CVAL_EL0 */
    DR_REG_VIRT107, /* DR_REG_CNTV_TVAL_EL0 */
    DR_REG_VIRT108, /* DR_REG_CNTV_CTL_EL0 */
    DR_REG_VIRT109, /* DR_REG_CNTV_CVAL_EL0 */
    DR_REG_VIRT110, /* DR_REG_PMEVCNTR0_EL0 */
    DR_REG_VIRT111, /* DR_REG_PMEVCNTR1_EL0 */
    DR_REG_VIRT112, /* DR_REG_PMEVCNTR2_EL0 */
    DR_REG_VIRT113, /* DR_REG_PMEVCNTR3_EL0 */
    DR_REG_VIRT114, /* DR_REG_PMEVCNTR4_EL0 */
    DR_REG_VIRT115, /* DR_REG_PMEVCNTR5_EL0 */
    DR_REG_VIRT116, /* DR_REG_PMEVCNTR6_EL0 */
    DR_REG_VIRT117, /* DR_REG_PMEVCNTR7_EL0 */
    DR_REG_VIRT118, /* DR_REG_PMEVCNTR8_EL0 */
    DR_REG_VIRT119, /* DR_REG_PMEVCNTR9_EL0 */
    DR_REG_VIRT120, /* DR_REG_PMEVCNTR10_EL0 */
    DR_REG_VIRT121, /* DR_REG_PMEVCNTR11_EL0 */
    DR_REG_VIRT122, /* DR_REG_PMEVCNTR12_EL0 */
    DR_REG_VIRT123, /* DR_REG_PMEVCNTR13_EL0 */
    DR_REG_VIRT124, /* DR_REG_PMEVCNTR14_EL0 */
    DR_REG_VIRT125, /* DR_REG_PMEVCNTR15_EL0 */
    DR_REG_VIRT126, /* DR_REG_PMEVCNTR16_EL0 */
    DR_REG_VIRT127, /* DR_REG_PMEVCNTR17_EL0 */
    DR_REG_VIRT128, /* DR_REG_PMEVCNTR18_EL0 */
    DR_REG_VIRT129, /* DR_REG_PMEVCNTR19_EL0 */
    DR_REG_VIRT130, /* DR_REG_PMEVCNTR20_EL0 */
    DR_REG_VIRT131, /* DR_REG_PMEVCNTR21_EL0 */
    DR_REG_VIRT132, /* DR_REG_PMEVCNTR22_EL0 */
    DR_REG_VIRT133, /* DR_REG_PMEVCNTR23_EL0 */
    DR_REG_VIRT134, /* DR_REG_PMEVCNTR24_EL0 */
    DR_REG_VIRT135, /* DR_REG_PMEVCNTR25_EL0 */
    DR_REG_VIRT136, /* DR_REG_PMEVCNTR26_EL0 */
    DR_REG_VIRT137, /* DR_REG_PMEVCNTR27_EL0 */
    DR_REG_VIRT138, /* DR_REG_PMEVCNTR28_EL0 */
    DR_REG_VIRT139, /* DR_REG_PMEVCNTR29_EL0 */
    DR_REG_VIRT140, /* DR_REG_PMEVCNTR30_EL0 */
    DR_REG_VIRT141, /* DR_REG_PMEVTYPER0_EL0 */
    DR_REG_VIRT142, /* DR_REG_PMEVTYPER1_EL0 */
    DR_REG_VIRT143, /* DR_REG_PMEVTYPER2_EL0 */
    DR_REG_VIRT144, /* DR_REG_PMEVTYPER3_EL0 */
    DR_REG_VIRT145, /* DR_REG_PMEVTYPER4_EL0 */
    DR_REG_VIRT146, /* DR_REG_PMEVTYPER5_EL0 */
    DR_REG_VIRT147, /* DR_REG_PMEVTYPER6_EL0 */
    DR_REG_VIRT148, /* DR_REG_PMEVTYPER7_EL0 */
    DR_REG_VIRT149, /* DR_REG_PMEVTYPER8_EL0 */
    DR_REG_VIRT150, /* DR_REG_PMEVTYPER9_EL0 */
    DR_REG_VIRT151, /* DR_REG_PMEVTYPER10_EL0 */
    DR_REG_VIRT152, /* DR_REG_PMEVTYPER11_EL0 */
    DR_REG_VIRT153, /* DR_REG_PMEVTYPER12_EL0 */
    DR_REG_VIRT154, /* DR_REG_PMEVTYPER13_EL0 */
    DR_REG_VIRT155, /* DR_REG_PMEVTYPER14_EL0 */
    DR_REG_VIRT156, /* DR_REG_PMEVTYPER15_EL0 */
    DR_REG_VIRT157, /* DR_REG_PMEVTYPER16_EL0 */
    DR_REG_VIRT158, /* DR_REG_PMEVTYPER17_EL0 */
    DR_REG_VIRT159, /* DR_REG_PMEVTYPER18_EL0 */
    DR_REG_VIRT160, /* DR_REG_PMEVTYPER19_EL0 */
    DR_REG_VIRT161, /* DR_REG_PMEVTYPER20_EL0 */
    DR_REG_VIRT162, /* DR_REG_PMEVTYPER21_EL0 */
    DR_REG_VIRT163, /* DR_REG_PMEVTYPER22_EL0 */
    DR_REG_VIRT164, /* DR_REG_PMEVTYPER23_EL0 */
    DR_REG_VIRT165, /* DR_REG_PMEVTYPER24_EL0 */
    DR_REG_VIRT166, /* DR_REG_PMEVTYPER25_EL0 */
    DR_REG_VIRT167, /* DR_REG_PMEVTYPER26_EL0 */
    DR_REG_VIRT168, /* DR_REG_PMEVTYPER27_EL0 */
    DR_REG_VIRT169, /* DR_REG_PMEVTYPER28_EL0 */
    DR_REG_VIRT170, /* DR_REG_PMEVTYPER29_EL0 */
    DR_REG_VIRT171, /* DR_REG_PMEVTYPER30_EL0 */
    DR_REG_VIRT172, /* DR_REG_PMCCFILTR_EL0 */
    DR_REG_VIRT173, /* DR_REG_SPSR_IRQ */
    DR_REG_VIRT174, /* DR_REG_SPSR_ABT */
    DR_REG_VIRT175, /* DR_REG_SPSR_UND */
    DR_REG_VIRT176, /* DR_REG_SPSR_FIQ */
    DR_REG_VIRT177, /* DR_REG_TPIDR_EL0 */
    DR_REG_VIRT178, /* DR_REG_TPIDRRO_EL0 */

    DR_REG_VIRT179, /* DR_REG_P0 */
    DR_REG_VIRT180, /* DR_REG_P1 */
    DR_REG_VIRT181, /* DR_REG_P2 */
    DR_REG_VIRT182, /* DR_REG_P3 */
    DR_REG_VIRT183, /* DR_REG_P4 */
    DR_REG_VIRT184, /* DR_REG_P5 */
    DR_REG_VIRT185, /* DR_REG_P6 */
    DR_REG_VIRT186, /* DR_REG_P7 */
    DR_REG_VIRT187, /* DR_REG_P8 */
    DR_REG_VIRT188, /* DR_REG_P9 */
    DR_REG_VIRT189, /* DR_REG_P10 */
    DR_REG_VIRT190, /* DR_REG_P11 */
    DR_REG_VIRT191, /* DR_REG_P12 */
    DR_REG_VIRT192, /* DR_REG_P13 */
    DR_REG_VIRT193, /* DR_REG_P14 */
    DR_REG_VIRT194, /* DR_REG_P15 */
    DR_REG_VIRT195, /* DR_REG_FFR */

    DR_REG_VIRT196, /* DR_REG_CNTVCT_EL0 */
    DR_REG_VIRT197, /* DR_REG_ID_AA64ISAR0_EL1 */
    DR_REG_VIRT198, /* DR_REG_ID_AA64ISAR1_EL1 */
    DR_REG_VIRT199, /* DR_REG_ID_AA64ISAR2_EL1 */
    DR_REG_VIRT200, /* DR_REG_ID_AA64PFR0_EL1 */
    DR_REG_VIRT201, /* DR_REG_ID_AA64MMFR1_EL1 */
    DR_REG_VIRT202, /* DR_REG_ID_AA64DFR0_EL1 */
    DR_REG_VIRT203, /* DR_REG_ID_AA64ZFR0_EL1 */
    DR_REG_VIRT204, /* DR_REG_ID_AA64PFR1_EL1 */
    DR_REG_VIRT205, /* DR_REG_ID_AA64MMFR2_EL1 */
    DR_REG_VIRT206, /* DR_REG_MIDR_EL1 */
    DR_REG_VIRT207, /* DR_REG_MPIDR_EL1 */
    DR_REG_VIRT208, /* DR_REG_REVIDR_EL1 */

    DR_REG_VIRT209, /* DR_REG_FPMR */
    DR_REG_VIRT210, /* DR_REG_CONTEXTIDR_EL1 */
    DR_REG_VIRT211, /* DR_REG_ELR_EL1 */
    DR_REG_VIRT212, /* DR_REG_SPSR_EL1 */
    DR_REG_VIRT213, /* DR_REG_TPIDR_EL1 */
    DR_REG_VIRT214, /* DR_REG_ACCDATA_EL1 */
};
/* clang-format on */

#ifdef DEBUG
void
encode_debug_checks(void)
{
    CLIENT_ASSERT(sizeof(d_r_reg_id_to_virtual) == sizeof(dr_reg_fixer),
                  "register to virtual register map size error");

    /* TODO i#1569: NYI */
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
                /* Avoid complaining for !assert_reachable as we hit failures for
                 * branches to labels during instrumentation (i#5297).
                 */
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
