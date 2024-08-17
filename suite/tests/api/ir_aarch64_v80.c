/* **********************************************************
 * Copyright (c) 2024 ARM Limited. All rights reserved.
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

/* Define DR_FAST_IR to verify that everything compiles when we call the inline
 * versions of these routines.
 */
#ifndef STANDALONE_DECODER
#    define DR_FAST_IR 1
#endif

/* Uses the DR API, using DR as a standalone library, rather than
 * being a client library working with DR on a target program.
 */

#include "configure.h"
#include "dr_api.h"
#include "tools.h"

#include "ir_aarch64.h"

static const reg_id_t systemreg[] = {
    DR_REG_NZCV,
    DR_REG_FPCR,
    DR_REG_FPSR,
    DR_REG_MDCCSR_EL0,
    DR_REG_DBGDTR_EL0,
    DR_REG_DBGDTRRX_EL0,
    DR_REG_SP_EL0,
    DR_REG_SPSEL,
    DR_REG_CURRENTEL,
    DR_REG_PAN,
    DR_REG_UAO,
    DR_REG_CTR_EL0,
    DR_REG_DCZID_EL0,
    DR_REG_RNDR,
    DR_REG_RNDRRS,
    DR_REG_DAIF,
    DR_REG_DIT,
    DR_REG_SSBS,
    DR_REG_TCO,
    DR_REG_DSPSR_EL0,
    DR_REG_DLR_EL0,
    DR_REG_PMCR_EL0,
    DR_REG_PMCNTENSET_EL0,
    DR_REG_PMCNTENCLR_EL0,
    DR_REG_PMOVSCLR_EL0,
    DR_REG_PMSWINC_EL0,
    DR_REG_PMSELR_EL0,
    DR_REG_PMCEID0_EL0,
    DR_REG_PMCEID1_EL0,
    DR_REG_PMCCNTR_EL0,
    DR_REG_PMXEVTYPER_EL0,
    DR_REG_PMXEVCNTR_EL0,
    DR_REG_PMUSERENR_EL0,
    DR_REG_PMOVSSET_EL0,
    DR_REG_SCXTNUM_EL0,
    DR_REG_CNTFRQ_EL0,
    DR_REG_CNTPCT_EL0,
    DR_REG_CNTP_TVAL_EL0,
    DR_REG_CNTP_CTL_EL0,
    DR_REG_CNTP_CVAL_EL0,
    DR_REG_CNTV_TVAL_EL0,
    DR_REG_CNTV_CTL_EL0,
    DR_REG_CNTV_CVAL_EL0,
    DR_REG_PMEVTYPER0_EL0,
    DR_REG_PMEVTYPER1_EL0,
    DR_REG_PMEVTYPER2_EL0,
    DR_REG_PMEVTYPER3_EL0,
    DR_REG_PMEVTYPER4_EL0,
    DR_REG_PMEVTYPER5_EL0,
    DR_REG_PMEVTYPER6_EL0,
    DR_REG_PMEVTYPER7_EL0,
    DR_REG_PMEVTYPER8_EL0,
    DR_REG_PMEVTYPER9_EL0,
    DR_REG_PMEVTYPER10_EL0,
    DR_REG_PMEVTYPER11_EL0,
    DR_REG_PMEVTYPER12_EL0,
    DR_REG_PMEVTYPER13_EL0,
    DR_REG_PMEVTYPER14_EL0,
    DR_REG_PMEVTYPER15_EL0,
    DR_REG_PMEVTYPER16_EL0,
    DR_REG_PMEVTYPER17_EL0,
    DR_REG_PMEVTYPER18_EL0,
    DR_REG_PMEVTYPER19_EL0,
    DR_REG_PMEVTYPER20_EL0,
    DR_REG_PMEVTYPER21_EL0,
    DR_REG_PMEVTYPER22_EL0,
    DR_REG_PMEVTYPER23_EL0,
    DR_REG_PMEVTYPER24_EL0,
    DR_REG_PMEVTYPER25_EL0,
    DR_REG_PMEVTYPER26_EL0,
    DR_REG_PMEVTYPER27_EL0,
    DR_REG_PMEVTYPER28_EL0,
    DR_REG_PMEVTYPER29_EL0,
    DR_REG_PMEVTYPER30_EL0,
    DR_REG_PMCCFILTR_EL0,
    DR_REG_SPSR_IRQ,
    DR_REG_SPSR_ABT,
    DR_REG_SPSR_UND,
    DR_REG_SPSR_FIQ,
    DR_REG_ID_AA64ISAR0_EL1,
    DR_REG_ID_AA64ISAR1_EL1,
    DR_REG_ID_AA64ISAR2_EL1,
    DR_REG_ID_AA64PFR0_EL1,
    DR_REG_ID_AA64MMFR1_EL1,
    DR_REG_ID_AA64DFR0_EL1,
    DR_REG_ID_AA64ZFR0_EL1,
    DR_REG_ID_AA64PFR1_EL1,
    DR_REG_ID_AA64MMFR2_EL1,
    DR_REG_MIDR_EL1,
    DR_REG_MPIDR_EL1,
    DR_REG_REVIDR_EL1,
};
static const size_t sysreg_count = sizeof(systemreg) / sizeof(systemreg[0]);

TEST_INSTR(mrs)
{
    /* Testing MRS     <Xt>, <systemreg> */
    TEST_LOOP_EXPECT(
        mrs, sysreg_count,
        INSTR_CREATE_mrs(dc, opnd_create_reg(DR_REG_X0 + (i % 31)),
                         opnd_create_reg(systemreg[i])),
        {
            EXPECT_DISASSEMBLY(
                /* clang-format off */
                "mrs    %nzcv -> %x0",             "mrs    %fpcr -> %x1",
                "mrs    %fpsr -> %x2",             "mrs    %mdccsr_el0 -> %x3",
                "mrs    %dbgdtr_el0 -> %x4",       "mrs    %dbgdtrrx_el0 -> %x5",
                "mrs    %sp_el0 -> %x6",           "mrs    %spsel -> %x7",
                "mrs    %currentel -> %x8",        "mrs    %pan -> %x9",
                "mrs    %uao -> %x10",             "mrs    %ctr_el0 -> %x11",
                "mrs    %dczid_el0 -> %x12",       "mrs    %rndr -> %x13",
                "mrs    %rndrrs -> %x14",          "mrs    %daif -> %x15",
                "mrs    %dit -> %x16",             "mrs    %ssbs -> %x17",
                "mrs    %tco -> %x18",             "mrs    %dspsr_el0 -> %x19",
                "mrs    %dlr_el0 -> %x20",         "mrs    %pmcr_el0 -> %x21",
                "mrs    %pmcntenset_el0 -> %x22",  "mrs    %pmcntenclr_el0 -> %x23",
                "mrs    %pmovsclr_el0 -> %x24",    "mrs    %pmswinc_el0 -> %x25",
                "mrs    %pmselr_el0 -> %x26",      "mrs    %pmceid0_el0 -> %x27",
                "mrs    %pmceid1_el0 -> %x28",     "mrs    %pmccntr_el0 -> %x29",
                "mrs    %pmxevtyper_el0 -> %x30",  "mrs    %pmxevcntr_el0 -> %x0",
                "mrs    %pmuserenr_el0 -> %x1",    "mrs    %pmovsset_el0 -> %x2",
                "mrs    %scxtnum_el0 -> %x3",      "mrs    %cntfrq_el0 -> %x4",
                "mrs    %cntpct_el0 -> %x5",       "mrs    %cntp_tval_el0 -> %x6",
                "mrs    %cntp_ctl_el0 -> %x7",     "mrs    %cntp_cval_el0 -> %x8",
                "mrs    %cntv_tval_el0 -> %x9",    "mrs    %cntv_ctl_el0 -> %x10",
                "mrs    %cntv_cval_el0 -> %x11",   "mrs    %pmevtyper0_el0 -> %x12",
                "mrs    %pmevtyper1_el0 -> %x13",  "mrs    %pmevtyper2_el0 -> %x14",
                "mrs    %pmevtyper3_el0 -> %x15",  "mrs    %pmevtyper4_el0 -> %x16",
                "mrs    %pmevtyper5_el0 -> %x17",  "mrs    %pmevtyper6_el0 -> %x18",
                "mrs    %pmevtyper7_el0 -> %x19",  "mrs    %pmevtyper8_el0 -> %x20",
                "mrs    %pmevtyper9_el0 -> %x21",  "mrs    %pmevtyper10_el0 -> %x22",
                "mrs    %pmevtyper11_el0 -> %x23", "mrs    %pmevtyper12_el0 -> %x24",
                "mrs    %pmevtyper13_el0 -> %x25", "mrs    %pmevtyper14_el0 -> %x26",
                "mrs    %pmevtyper15_el0 -> %x27", "mrs    %pmevtyper16_el0 -> %x28",
                "mrs    %pmevtyper17_el0 -> %x29", "mrs    %pmevtyper18_el0 -> %x30",
                "mrs    %pmevtyper19_el0 -> %x0",  "mrs    %pmevtyper20_el0 -> %x1",
                "mrs    %pmevtyper21_el0 -> %x2",  "mrs    %pmevtyper22_el0 -> %x3",
                "mrs    %pmevtyper23_el0 -> %x4",  "mrs    %pmevtyper24_el0 -> %x5",
                "mrs    %pmevtyper25_el0 -> %x6",  "mrs    %pmevtyper26_el0 -> %x7",
                "mrs    %pmevtyper27_el0 -> %x8",  "mrs    %pmevtyper28_el0 -> %x9",
                "mrs    %pmevtyper29_el0 -> %x10", "mrs    %pmevtyper30_el0 -> %x11",
                "mrs    %pmccfiltr_el0 -> %x12",   "mrs    %spsr_irq -> %x13",
                "mrs    %spsr_abt -> %x14",        "mrs    %spsr_und -> %x15",
                "mrs    %spsr_fiq -> %x16",        "mrs    %id_aa64isar0_el1 -> %x17",
                "mrs    %id_aa64isar1_el1 -> %x18","mrs    %id_aa64isar2_el1 -> %x19",
                "mrs    %id_aa64pfr0_el1 -> %x20", "mrs    %id_aa64mmfr1_el1 -> %x21",
                "mrs    %id_aa64dfr0_el1 -> %x22", "mrs    %id_aa64zfr0_el1 -> %x23",
                "mrs    %id_aa64pfr1_el1 -> %x24", "mrs    %id_aa64mmfr2_el1 -> %x25",
                "mrs    %midr_el1 -> %x26",        "mrs    %mpidr_el1 -> %x27",
                "mrs    %revidr_el1 -> %x28"
                /* clang-format on */
            );
            switch (systemreg[i]) {
            case DR_REG_NZCV:
                EXPECT_TRUE(TEST(EFLAGS_READ_NZCV,
                                 instr_get_arith_flags(instr, DR_QUERY_INCLUDE_ALL)));
                EXPECT_FALSE(TEST(EFLAGS_WRITE_NZCV,
                                  instr_get_arith_flags(instr, DR_QUERY_INCLUDE_ALL)));
                break;
            case DR_REG_RNDR:
            case DR_REG_RNDRRS:
                EXPECT_FALSE(TEST(EFLAGS_READ_NZCV,
                                  instr_get_arith_flags(instr, DR_QUERY_INCLUDE_ALL)));
                EXPECT_TRUE(TEST(EFLAGS_WRITE_NZCV,
                                 instr_get_arith_flags(instr, DR_QUERY_INCLUDE_ALL)));
                break;
            default:
                EXPECT_FALSE(TEST(EFLAGS_READ_NZCV,
                                  instr_get_arith_flags(instr, DR_QUERY_INCLUDE_ALL)));
                EXPECT_FALSE(TEST(EFLAGS_WRITE_NZCV,
                                  instr_get_arith_flags(instr, DR_QUERY_INCLUDE_ALL)));
            }
        });
}

TEST_INSTR(msr)
{
    /* Testing MSR     <systemreg>, <Xt> */
    TEST_LOOP_EXPECT(
        msr, sysreg_count,
        INSTR_CREATE_msr(dc, opnd_create_reg(systemreg[i]),
                         opnd_create_reg(DR_REG_X0 + (i % 31))),
        {
            EXPECT_DISASSEMBLY(
                /* clang-format off */
                "msr    %x0 -> %nzcv",             "msr    %x1 -> %fpcr",
                "msr    %x2 -> %fpsr",             "msr    %x3 -> %mdccsr_el0",
                "msr    %x4 -> %dbgdtr_el0",       "msr    %x5 -> %dbgdtrrx_el0",
                "msr    %x6 -> %sp_el0",           "msr    %x7 -> %spsel",
                "msr    %x8 -> %currentel",        "msr    %x9 -> %pan",
                "msr    %x10 -> %uao",             "msr    %x11 -> %ctr_el0",
                "msr    %x12 -> %dczid_el0",       "msr    %x13 -> %rndr",
                "msr    %x14 -> %rndrrs",          "msr    %x15 -> %daif",
                "msr    %x16 -> %dit",             "msr    %x17 -> %ssbs",
                "msr    %x18 -> %tco",             "msr    %x19 -> %dspsr_el0",
                "msr    %x20 -> %dlr_el0",         "msr    %x21 -> %pmcr_el0",
                "msr    %x22 -> %pmcntenset_el0",  "msr    %x23 -> %pmcntenclr_el0",
                "msr    %x24 -> %pmovsclr_el0",    "msr    %x25 -> %pmswinc_el0",
                "msr    %x26 -> %pmselr_el0",      "msr    %x27 -> %pmceid0_el0",
                "msr    %x28 -> %pmceid1_el0",     "msr    %x29 -> %pmccntr_el0",
                "msr    %x30 -> %pmxevtyper_el0",  "msr    %x0 -> %pmxevcntr_el0",
                "msr    %x1 -> %pmuserenr_el0",    "msr    %x2 -> %pmovsset_el0",
                "msr    %x3 -> %scxtnum_el0",      "msr    %x4 -> %cntfrq_el0",
                "msr    %x5 -> %cntpct_el0",       "msr    %x6 -> %cntp_tval_el0",
                "msr    %x7 -> %cntp_ctl_el0",     "msr    %x8 -> %cntp_cval_el0",
                "msr    %x9 -> %cntv_tval_el0",    "msr    %x10 -> %cntv_ctl_el0",
                "msr    %x11 -> %cntv_cval_el0",   "msr    %x12 -> %pmevtyper0_el0",
                "msr    %x13 -> %pmevtyper1_el0",  "msr    %x14 -> %pmevtyper2_el0",
                "msr    %x15 -> %pmevtyper3_el0",  "msr    %x16 -> %pmevtyper4_el0",
                "msr    %x17 -> %pmevtyper5_el0",  "msr    %x18 -> %pmevtyper6_el0",
                "msr    %x19 -> %pmevtyper7_el0",  "msr    %x20 -> %pmevtyper8_el0",
                "msr    %x21 -> %pmevtyper9_el0",  "msr    %x22 -> %pmevtyper10_el0",
                "msr    %x23 -> %pmevtyper11_el0", "msr    %x24 -> %pmevtyper12_el0",
                "msr    %x25 -> %pmevtyper13_el0", "msr    %x26 -> %pmevtyper14_el0",
                "msr    %x27 -> %pmevtyper15_el0", "msr    %x28 -> %pmevtyper16_el0",
                "msr    %x29 -> %pmevtyper17_el0", "msr    %x30 -> %pmevtyper18_el0",
                "msr    %x0 -> %pmevtyper19_el0",  "msr    %x1 -> %pmevtyper20_el0",
                "msr    %x2 -> %pmevtyper21_el0",  "msr    %x3 -> %pmevtyper22_el0",
                "msr    %x4 -> %pmevtyper23_el0",  "msr    %x5 -> %pmevtyper24_el0",
                "msr    %x6 -> %pmevtyper25_el0",  "msr    %x7 -> %pmevtyper26_el0",
                "msr    %x8 -> %pmevtyper27_el0",  "msr    %x9 -> %pmevtyper28_el0",
                "msr    %x10 -> %pmevtyper29_el0", "msr    %x11 -> %pmevtyper30_el0",
                "msr    %x12 -> %pmccfiltr_el0",   "msr    %x13 -> %spsr_irq",
                "msr    %x14 -> %spsr_abt",        "msr    %x15 -> %spsr_und",
                "msr    %x16 -> %spsr_fiq",        "msr    %x17 -> %id_aa64isar0_el1",
                "msr    %x18 -> %id_aa64isar1_el1","msr    %x19 -> %id_aa64isar2_el1",
                "msr    %x20 -> %id_aa64pfr0_el1", "msr    %x21 -> %id_aa64mmfr1_el1",
                "msr    %x22 -> %id_aa64dfr0_el1", "msr    %x23 -> %id_aa64zfr0_el1",
                "msr    %x24 -> %id_aa64pfr1_el1", "msr    %x25 -> %id_aa64mmfr2_el1",
                "msr    %x26 -> %midr_el1",        "msr    %x27 -> %mpidr_el1",
                "msr    %x28 -> %revidr_el1"
                /* clang-format on */
            );
            switch (systemreg[i]) {
            case DR_REG_NZCV:
                EXPECT_FALSE(TEST(EFLAGS_READ_NZCV,
                                  instr_get_arith_flags(instr, DR_QUERY_INCLUDE_ALL)));
                EXPECT_TRUE(TEST(EFLAGS_WRITE_NZCV,
                                 instr_get_arith_flags(instr, DR_QUERY_INCLUDE_ALL)));
                break;
            default:
                EXPECT_FALSE(TEST(EFLAGS_READ_NZCV,
                                  instr_get_arith_flags(instr, DR_QUERY_INCLUDE_ALL)));
                EXPECT_FALSE(TEST(EFLAGS_WRITE_NZCV,
                                  instr_get_arith_flags(instr, DR_QUERY_INCLUDE_ALL)));
            }
        });

    /* Testing MSR     <pstatefield>, <imm> */
    static const reg_id_t pstatefields[] = {
        DR_REG_UAO, DR_REG_PAN, DR_REG_SPSEL,   DR_REG_SSBS,
        DR_REG_DIT, DR_REG_TCO, DR_REG_DAIFSET, DR_REG_DAIFCLR,
    };
    static const size_t pstate_count = sizeof(pstatefields) / sizeof(pstatefields[0]);

    TEST_LOOP_EXPECT(
        msr, pstate_count,
        INSTR_CREATE_msr(dc, opnd_create_reg(pstatefields[i]),
                         opnd_create_immed_int(i, OPSZ_4b)),
        {
            EXPECT_DISASSEMBLY("msr    %uao $0x00", "msr    %pan $0x01",
                               "msr    %spsel $0x02", "msr    %ssbs $0x03",
                               "msr    %dit $0x04", "msr    %tco $0x05",
                               "msr    %daifset $0x06", "msr    %daifclr $0x07");
            EXPECT_FALSE(TEST(EFLAGS_READ_NZCV,
                              instr_get_arith_flags(instr, DR_QUERY_INCLUDE_ALL)));
            EXPECT_FALSE(TEST(EFLAGS_WRITE_NZCV,
                              instr_get_arith_flags(instr, DR_QUERY_INCLUDE_ALL)));
        });
}

TEST_INSTR(wfe)
{
    /* Testing WFE */
    TEST_LOOP_EXPECT(wfe, 1, INSTR_CREATE_wfe(dc), EXPECT_DISASSEMBLY("wfe"));
}

TEST_INSTR(wfi)
{
    /* Testing WFI */
    TEST_LOOP_EXPECT(wfi, 1, INSTR_CREATE_wfi(dc), EXPECT_DISASSEMBLY("wfi"));
}

TEST_INSTR(orr)
{
    /* Testing ORR     <Xd|SP>, <Xn>, #<imm> */
    static const uint64 imm13_64[6] = { 0x100000001,        0xffc7ffc7ffc7ffc7,
                                        0xfffff807fffff807, 0x700000007,
                                        0xffc7ffc7ffc7ffc7, 0xeeeeeeeeeeeeeeee };
    TEST_LOOP_EXPECT(orr, 6,
                     INSTR_CREATE_orr(dc, opnd_create_reg(Xn_six_offset_0_sp[i]),
                                      opnd_create_reg(Xn_six_offset_1[i]),
                                      OPND_CREATE_INT(imm13_64[i])),
                     EXPECT_DISASSEMBLY("orr    %x0 $0x0000000100000001 -> %x0",
                                        "orr    %x6 $0xffc7ffc7ffc7ffc7 -> %x5",
                                        "orr    %x11 $0xfffff807fffff807 -> %x10",
                                        "orr    %x16 $0x0000000700000007 -> %x15",
                                        "orr    %x21 $0xffc7ffc7ffc7ffc7 -> %x20",
                                        "orr    %x30 $0xeeeeeeeeeeeeeeee -> %sp"););

    /* Testing ORR     <Wd|SP>, <Wn>, #<imm> */
    static const reg_id_t Rd_1_0[6] = { DR_REG_W0,  DR_REG_W5,  DR_REG_W10,
                                        DR_REG_W15, DR_REG_W20, DR_REG_WSP };
    static const uint imm13_32[6] = { 0x1, 0xffc7ffc7, 0xfffff807,
                                      0x7, 0xffc7ffc7, 0xeeeeeeee };

    TEST_LOOP_EXPECT(
        orr, 6,
        INSTR_CREATE_orr(dc, opnd_create_reg(Wn_six_offset_0_sp[i]),
                         opnd_create_reg(Wn_six_offset_1[i]),
                         OPND_CREATE_INT32(imm13_32[i])),
        EXPECT_DISASSEMBLY(
            "orr    %w0 $0x00000001 -> %w0", "orr    %w6 $0xffc7ffc7 -> %w5",
            "orr    %w11 $0xfffff807 -> %w10", "orr    %w16 $0x00000007 -> %w15",
            "orr    %w21 $0xffc7ffc7 -> %w20", "orr    %w30 $0xeeeeeeee -> %wsp"););
}

TEST_INSTR(orr_shift)
{
    static const uint imm6[6] = { 0, 8, 13, 19, 24, 31 };

    /* Testing ORR     <Wd>, <Wn>, <Wm>, <extend> #<imm> */
    TEST_LOOP_EXPECT(
        orr, 6,
        INSTR_CREATE_orr_shift(dc, opnd_create_reg(Wn_six_offset_0[i]),
                               opnd_create_reg(Wn_six_offset_1[i]),
                               opnd_create_reg(Wn_six_offset_2[i]), OPND_CREATE_LSL(),
                               opnd_create_immed_uint(imm6[i], OPSZ_6b)),
        EXPECT_DISASSEMBLY(
            "orr    %w0 %w0 lsl $0x00 -> %w0", "orr    %w6 %w7 lsl $0x08 -> %w5",
            "orr    %w11 %w12 lsl $0x0d -> %w10", "orr    %w16 %w17 lsl $0x13 -> %w15",
            "orr    %w21 %w22 lsl $0x18 -> %w20", "orr    %w30 %w30 lsl $0x1f -> %w30"););

    TEST_LOOP_EXPECT(
        orr, 6,
        INSTR_CREATE_orr_shift(dc, opnd_create_reg(Wn_six_offset_0[i]),
                               opnd_create_reg(Wn_six_offset_1[i]),
                               opnd_create_reg(Wn_six_offset_2[i]), OPND_CREATE_LSR(),
                               opnd_create_immed_uint(imm6[i], OPSZ_6b)),
        EXPECT_DISASSEMBLY(
            "orr    %w0 %w0 lsr $0x00 -> %w0", "orr    %w6 %w7 lsr $0x08 -> %w5",
            "orr    %w11 %w12 lsr $0x0d -> %w10", "orr    %w16 %w17 lsr $0x13 -> %w15",
            "orr    %w21 %w22 lsr $0x18 -> %w20", "orr    %w30 %w30 lsr $0x1f -> %w30"););

    TEST_LOOP_EXPECT(
        orr, 6,
        INSTR_CREATE_orr_shift(dc, opnd_create_reg(Wn_six_offset_0[i]),
                               opnd_create_reg(Wn_six_offset_1[i]),
                               opnd_create_reg(Wn_six_offset_2[i]), OPND_CREATE_ASR(),
                               opnd_create_immed_uint(imm6[i], OPSZ_6b)),
        EXPECT_DISASSEMBLY(
            "orr    %w0 %w0 asr $0x00 -> %w0", "orr    %w6 %w7 asr $0x08 -> %w5",
            "orr    %w11 %w12 asr $0x0d -> %w10", "orr    %w16 %w17 asr $0x13 -> %w15",
            "orr    %w21 %w22 asr $0x18 -> %w20", "orr    %w30 %w30 asr $0x1f -> %w30"););

    TEST_LOOP_EXPECT(
        orr, 6,
        INSTR_CREATE_orr_shift(dc, opnd_create_reg(Wn_six_offset_0[i]),
                               opnd_create_reg(Wn_six_offset_1[i]),
                               opnd_create_reg(Wn_six_offset_2[i]), OPND_CREATE_ROR(),
                               opnd_create_immed_uint(imm6[i], OPSZ_6b)),
        EXPECT_DISASSEMBLY(
            "orr    %w0 %w0 ror $0x00 -> %w0", "orr    %w6 %w7 ror $0x08 -> %w5",
            "orr    %w11 %w12 ror $0x0d -> %w10", "orr    %w16 %w17 ror $0x13 -> %w15",
            "orr    %w21 %w22 ror $0x18 -> %w20", "orr    %w30 %w30 ror $0x1f -> %w30"););

    /* Testing ORR     <Xd>, <Xn>, <Xm>, <extend> #<imm> */
    TEST_LOOP_EXPECT(
        orr, 6,
        INSTR_CREATE_orr_shift(dc, opnd_create_reg(Xn_six_offset_0[i]),
                               opnd_create_reg(Xn_six_offset_1[i]),
                               opnd_create_reg(Xn_six_offset_2[i]), OPND_CREATE_LSL(),
                               opnd_create_immed_uint(imm6[i], OPSZ_6b)),
        EXPECT_DISASSEMBLY(
            "orr    %x0 %x0 lsl $0x00 -> %x0", "orr    %x6 %x7 lsl $0x08 -> %x5",
            "orr    %x11 %x12 lsl $0x0d -> %x10", "orr    %x16 %x17 lsl $0x13 -> %x15",
            "orr    %x21 %x22 lsl $0x18 -> %x20", "orr    %x30 %x30 lsl $0x1f -> %x30"););

    TEST_LOOP_EXPECT(
        orr, 6,
        INSTR_CREATE_orr_shift(dc, opnd_create_reg(Xn_six_offset_0[i]),
                               opnd_create_reg(Xn_six_offset_1[i]),
                               opnd_create_reg(Xn_six_offset_2[i]), OPND_CREATE_LSR(),
                               opnd_create_immed_uint(imm6[i], OPSZ_6b)),
        EXPECT_DISASSEMBLY(
            "orr    %x0 %x0 lsr $0x00 -> %x0", "orr    %x6 %x7 lsr $0x08 -> %x5",
            "orr    %x11 %x12 lsr $0x0d -> %x10", "orr    %x16 %x17 lsr $0x13 -> %x15",
            "orr    %x21 %x22 lsr $0x18 -> %x20", "orr    %x30 %x30 lsr $0x1f -> %x30"););

    TEST_LOOP_EXPECT(
        orr, 6,
        INSTR_CREATE_orr_shift(dc, opnd_create_reg(Xn_six_offset_0[i]),
                               opnd_create_reg(Xn_six_offset_1[i]),
                               opnd_create_reg(Xn_six_offset_2[i]), OPND_CREATE_ASR(),
                               opnd_create_immed_uint(imm6[i], OPSZ_6b)),
        EXPECT_DISASSEMBLY(
            "orr    %x0 %x0 asr $0x00 -> %x0", "orr    %x6 %x7 asr $0x08 -> %x5",
            "orr    %x11 %x12 asr $0x0d -> %x10", "orr    %x16 %x17 asr $0x13 -> %x15",
            "orr    %x21 %x22 asr $0x18 -> %x20", "orr    %x30 %x30 asr $0x1f -> %x30"););

    TEST_LOOP_EXPECT(
        orr, 6,
        INSTR_CREATE_orr_shift(dc, opnd_create_reg(Xn_six_offset_0[i]),
                               opnd_create_reg(Xn_six_offset_1[i]),
                               opnd_create_reg(Xn_six_offset_2[i]), OPND_CREATE_ROR(),
                               opnd_create_immed_uint(imm6[i], OPSZ_6b)),
        EXPECT_DISASSEMBLY(
            "orr    %x0 %x0 ror $0x00 -> %x0", "orr    %x6 %x7 ror $0x08 -> %x5",
            "orr    %x11 %x12 ror $0x0d -> %x10", "orr    %x16 %x17 ror $0x13 -> %x15",
            "orr    %x21 %x22 ror $0x18 -> %x20", "orr    %x30 %x30 ror $0x1f -> %x30"););
}

static const dr_pred_type_t cond_codes[] = {
    DR_PRED_EQ, DR_PRED_NE, DR_PRED_CS, DR_PRED_CC, DR_PRED_MI, DR_PRED_PL,
    DR_PRED_VS, DR_PRED_VC, DR_PRED_HI, DR_PRED_LS, DR_PRED_GE, DR_PRED_LT,
    DR_PRED_GT, DR_PRED_LE, DR_PRED_AL, DR_PRED_NV,
};
static const size_t cond_count = sizeof(cond_codes) / sizeof(cond_codes[0]);

TEST_INSTR(ccmp)
{
    /* Testing CCMP <Wn>, #<imm>, #<nzcv>, <cond> */
    TEST_LOOP_EXPECT(
        ccmp, cond_count,
        INSTR_CREATE_ccmp(
            dc, CYCLE_REG(W, 2 * i), opnd_create_immed_int((2 * i) % 32, OPSZ_5b),
            opnd_create_immed_int(i & 0xf, OPSZ_4b), opnd_create_cond(cond_codes[i])),
        {
            EXPECT_DISASSEMBLY("ccmp   %w0 $0x00 $0x00 eq", "ccmp   %w2 $0x02 $0x01 ne",
                               "ccmp   %w4 $0x04 $0x02 cs", "ccmp   %w6 $0x06 $0x03 cc",
                               "ccmp   %w8 $0x08 $0x04 mi", "ccmp   %w10 $0x0a $0x05 pl",
                               "ccmp   %w12 $0x0c $0x06 vs", "ccmp   %w14 $0x0e $0x07 vc",
                               "ccmp   %w16 $0x10 $0x08 hi", "ccmp   %w18 $0x12 $0x09 ls",
                               "ccmp   %w20 $0x14 $0x0a ge", "ccmp   %w22 $0x16 $0x0b lt",
                               "ccmp   %w24 $0x18 $0x0c gt", "ccmp   %w26 $0x1a $0x0d le",
                               "ccmp   %w28 $0x1c $0x0e al",
                               "ccmp   %w30 $0x1e $0x0f nv");
            EXPECT_TRUE(
                TEST(EFLAGS_READ_NZCV, instr_get_arith_flags(instr, DR_QUERY_DEFAULT)));
            EXPECT_TRUE(
                TEST(EFLAGS_WRITE_NZCV, instr_get_arith_flags(instr, DR_QUERY_DEFAULT)));
        });

    /* Testing CCMP <Xn>, #<imm>, #<nzcv>, <cond> */
    TEST_LOOP_EXPECT(
        ccmp, cond_count,
        INSTR_CREATE_ccmp(
            dc, CYCLE_REG(X, 2 * i), opnd_create_immed_int((2 * i) % 32, OPSZ_5b),
            opnd_create_immed_int(i & 0xf, OPSZ_4b), opnd_create_cond(cond_codes[i])),
        {
            EXPECT_DISASSEMBLY("ccmp   %x0 $0x00 $0x00 eq", "ccmp   %x2 $0x02 $0x01 ne",
                               "ccmp   %x4 $0x04 $0x02 cs", "ccmp   %x6 $0x06 $0x03 cc",
                               "ccmp   %x8 $0x08 $0x04 mi", "ccmp   %x10 $0x0a $0x05 pl",
                               "ccmp   %x12 $0x0c $0x06 vs", "ccmp   %x14 $0x0e $0x07 vc",
                               "ccmp   %x16 $0x10 $0x08 hi", "ccmp   %x18 $0x12 $0x09 ls",
                               "ccmp   %x20 $0x14 $0x0a ge", "ccmp   %x22 $0x16 $0x0b lt",
                               "ccmp   %x24 $0x18 $0x0c gt", "ccmp   %x26 $0x1a $0x0d le",
                               "ccmp   %x28 $0x1c $0x0e al",
                               "ccmp   %x30 $0x1e $0x0f nv");
            EXPECT_TRUE(
                TEST(EFLAGS_READ_NZCV, instr_get_arith_flags(instr, DR_QUERY_DEFAULT)));
            EXPECT_TRUE(
                TEST(EFLAGS_WRITE_NZCV, instr_get_arith_flags(instr, DR_QUERY_DEFAULT)));
        });

    /* Testing CCMP <Wn>, <Wm>, #<nzcv>, <cond> */
    TEST_LOOP_EXPECT(
        ccmp, cond_count,
        INSTR_CREATE_ccmp(dc, CYCLE_REG(W, 2 * i), CYCLE_REG(W, (2 * i) + 1),
                          opnd_create_immed_int(i & 0xf, OPSZ_4b),
                          opnd_create_cond(cond_codes[i])),
        {
            EXPECT_DISASSEMBLY("ccmp   %w0 %w1 $0x00 eq", "ccmp   %w2 %w3 $0x01 ne",
                               "ccmp   %w4 %w5 $0x02 cs", "ccmp   %w6 %w7 $0x03 cc",
                               "ccmp   %w8 %w9 $0x04 mi", "ccmp   %w10 %w11 $0x05 pl",
                               "ccmp   %w12 %w13 $0x06 vs", "ccmp   %w14 %w15 $0x07 vc",
                               "ccmp   %w16 %w17 $0x08 hi", "ccmp   %w18 %w19 $0x09 ls",
                               "ccmp   %w20 %w21 $0x0a ge", "ccmp   %w22 %w23 $0x0b lt",
                               "ccmp   %w24 %w25 $0x0c gt", "ccmp   %w26 %w27 $0x0d le",
                               "ccmp   %w28 %w29 $0x0e al", "ccmp   %w30 %wzr $0x0f nv");
            EXPECT_TRUE(
                TEST(EFLAGS_READ_NZCV, instr_get_arith_flags(instr, DR_QUERY_DEFAULT)));
            EXPECT_TRUE(
                TEST(EFLAGS_WRITE_NZCV, instr_get_arith_flags(instr, DR_QUERY_DEFAULT)));
        });

    /* Testing CCMP <Xn>, <Xm>, #<nzcv>, <cond> */
    TEST_LOOP_EXPECT(
        ccmp, cond_count,
        INSTR_CREATE_ccmp(dc, CYCLE_REG(X, 2 * i), CYCLE_REG(X, (2 * i) + 1),
                          opnd_create_immed_int(i & 0xf, OPSZ_4b),
                          opnd_create_cond(cond_codes[i])),
        {
            EXPECT_DISASSEMBLY("ccmp   %x0 %x1 $0x00 eq", "ccmp   %x2 %x3 $0x01 ne",
                               "ccmp   %x4 %x5 $0x02 cs", "ccmp   %x6 %x7 $0x03 cc",
                               "ccmp   %x8 %x9 $0x04 mi", "ccmp   %x10 %x11 $0x05 pl",
                               "ccmp   %x12 %x13 $0x06 vs", "ccmp   %x14 %x15 $0x07 vc",
                               "ccmp   %x16 %x17 $0x08 hi", "ccmp   %x18 %x19 $0x09 ls",
                               "ccmp   %x20 %x21 $0x0a ge", "ccmp   %x22 %x23 $0x0b lt",
                               "ccmp   %x24 %x25 $0x0c gt", "ccmp   %x26 %x27 $0x0d le",
                               "ccmp   %x28 %x29 $0x0e al", "ccmp   %x30 %xzr $0x0f nv");
            EXPECT_TRUE(
                TEST(EFLAGS_READ_NZCV, instr_get_arith_flags(instr, DR_QUERY_DEFAULT)));
            EXPECT_TRUE(
                TEST(EFLAGS_WRITE_NZCV, instr_get_arith_flags(instr, DR_QUERY_DEFAULT)));
        });
}

TEST_INSTR(ccmn)
{
    /* Testing CCMN <Wn>, #<imm>, #<nzcv>, <cond> */
    TEST_LOOP_EXPECT(
        ccmn, cond_count,
        INSTR_CREATE_ccmn(
            dc, CYCLE_REG(W, 2 * i), opnd_create_immed_int((2 * i) % 32, OPSZ_5b),
            opnd_create_immed_int(i & 0xf, OPSZ_4b), opnd_create_cond(cond_codes[i])),
        {
            EXPECT_DISASSEMBLY("ccmn   %w0 $0x00 $0x00 eq", "ccmn   %w2 $0x02 $0x01 ne",
                               "ccmn   %w4 $0x04 $0x02 cs", "ccmn   %w6 $0x06 $0x03 cc",
                               "ccmn   %w8 $0x08 $0x04 mi", "ccmn   %w10 $0x0a $0x05 pl",
                               "ccmn   %w12 $0x0c $0x06 vs", "ccmn   %w14 $0x0e $0x07 vc",
                               "ccmn   %w16 $0x10 $0x08 hi", "ccmn   %w18 $0x12 $0x09 ls",
                               "ccmn   %w20 $0x14 $0x0a ge", "ccmn   %w22 $0x16 $0x0b lt",
                               "ccmn   %w24 $0x18 $0x0c gt", "ccmn   %w26 $0x1a $0x0d le",
                               "ccmn   %w28 $0x1c $0x0e al",
                               "ccmn   %w30 $0x1e $0x0f nv");
            EXPECT_TRUE(
                TEST(EFLAGS_READ_NZCV, instr_get_arith_flags(instr, DR_QUERY_DEFAULT)));
            EXPECT_TRUE(
                TEST(EFLAGS_WRITE_NZCV, instr_get_arith_flags(instr, DR_QUERY_DEFAULT)));
        });

    /* Testing CCMN <Xn>, #<imm>, #<nzcv>, <cond> */
    TEST_LOOP_EXPECT(
        ccmn, cond_count,
        INSTR_CREATE_ccmn(
            dc, CYCLE_REG(X, 2 * i), opnd_create_immed_int((2 * i) % 32, OPSZ_5b),
            opnd_create_immed_int(i & 0xf, OPSZ_4b), opnd_create_cond(cond_codes[i])),
        {
            EXPECT_DISASSEMBLY("ccmn   %x0 $0x00 $0x00 eq", "ccmn   %x2 $0x02 $0x01 ne",
                               "ccmn   %x4 $0x04 $0x02 cs", "ccmn   %x6 $0x06 $0x03 cc",
                               "ccmn   %x8 $0x08 $0x04 mi", "ccmn   %x10 $0x0a $0x05 pl",
                               "ccmn   %x12 $0x0c $0x06 vs", "ccmn   %x14 $0x0e $0x07 vc",
                               "ccmn   %x16 $0x10 $0x08 hi", "ccmn   %x18 $0x12 $0x09 ls",
                               "ccmn   %x20 $0x14 $0x0a ge", "ccmn   %x22 $0x16 $0x0b lt",
                               "ccmn   %x24 $0x18 $0x0c gt", "ccmn   %x26 $0x1a $0x0d le",
                               "ccmn   %x28 $0x1c $0x0e al",
                               "ccmn   %x30 $0x1e $0x0f nv");
            EXPECT_TRUE(
                TEST(EFLAGS_READ_NZCV, instr_get_arith_flags(instr, DR_QUERY_DEFAULT)));
            EXPECT_TRUE(
                TEST(EFLAGS_WRITE_NZCV, instr_get_arith_flags(instr, DR_QUERY_DEFAULT)));
        });

    /* Testing CCMN <Wn>, <Wm>, #<nzcv>, <cond> */
    TEST_LOOP_EXPECT(
        ccmn, cond_count,
        INSTR_CREATE_ccmn(dc, CYCLE_REG(W, 2 * i), CYCLE_REG(W, (2 * i) + 1),
                          opnd_create_immed_int(i & 0xf, OPSZ_4b),
                          opnd_create_cond(cond_codes[i])),
        {
            EXPECT_DISASSEMBLY("ccmn   %w0 %w1 $0x00 eq", "ccmn   %w2 %w3 $0x01 ne",
                               "ccmn   %w4 %w5 $0x02 cs", "ccmn   %w6 %w7 $0x03 cc",
                               "ccmn   %w8 %w9 $0x04 mi", "ccmn   %w10 %w11 $0x05 pl",
                               "ccmn   %w12 %w13 $0x06 vs", "ccmn   %w14 %w15 $0x07 vc",
                               "ccmn   %w16 %w17 $0x08 hi", "ccmn   %w18 %w19 $0x09 ls",
                               "ccmn   %w20 %w21 $0x0a ge", "ccmn   %w22 %w23 $0x0b lt",
                               "ccmn   %w24 %w25 $0x0c gt", "ccmn   %w26 %w27 $0x0d le",
                               "ccmn   %w28 %w29 $0x0e al", "ccmn   %w30 %wzr $0x0f nv");
            EXPECT_TRUE(
                TEST(EFLAGS_READ_NZCV, instr_get_arith_flags(instr, DR_QUERY_DEFAULT)));
            EXPECT_TRUE(
                TEST(EFLAGS_WRITE_NZCV, instr_get_arith_flags(instr, DR_QUERY_DEFAULT)));
        });

    /* Testing CCMN <Xn>, <Xm>, #<nzcv>, <cond> */
    TEST_LOOP_EXPECT(
        ccmn, cond_count,
        INSTR_CREATE_ccmn(dc, CYCLE_REG(X, 2 * i), CYCLE_REG(X, (2 * i) + 1),
                          opnd_create_immed_int(i & 0xf, OPSZ_4b),
                          opnd_create_cond(cond_codes[i])),
        {
            EXPECT_DISASSEMBLY("ccmn   %x0 %x1 $0x00 eq", "ccmn   %x2 %x3 $0x01 ne",
                               "ccmn   %x4 %x5 $0x02 cs", "ccmn   %x6 %x7 $0x03 cc",
                               "ccmn   %x8 %x9 $0x04 mi", "ccmn   %x10 %x11 $0x05 pl",
                               "ccmn   %x12 %x13 $0x06 vs", "ccmn   %x14 %x15 $0x07 vc",
                               "ccmn   %x16 %x17 $0x08 hi", "ccmn   %x18 %x19 $0x09 ls",
                               "ccmn   %x20 %x21 $0x0a ge", "ccmn   %x22 %x23 $0x0b lt",
                               "ccmn   %x24 %x25 $0x0c gt", "ccmn   %x26 %x27 $0x0d le",
                               "ccmn   %x28 %x29 $0x0e al", "ccmn   %x30 %xzr $0x0f nv");
            EXPECT_TRUE(
                TEST(EFLAGS_READ_NZCV, instr_get_arith_flags(instr, DR_QUERY_DEFAULT)));
            EXPECT_TRUE(
                TEST(EFLAGS_WRITE_NZCV, instr_get_arith_flags(instr, DR_QUERY_DEFAULT)));
        });
}

int
main(int argc, char *argv[])
{
#ifdef STANDALONE_DECODER
    void *dcontext = GLOBAL_DCONTEXT;
#else
    void *dcontext = dr_standalone_init();
#endif
    bool result = true;
    bool test_result;
    instr_t *instr;

    enable_all_test_cpu_features();

    RUN_INSTR_TEST(mrs);
    RUN_INSTR_TEST(msr);

    RUN_INSTR_TEST(wfe);
    RUN_INSTR_TEST(wfi);

    RUN_INSTR_TEST(orr);
    RUN_INSTR_TEST(orr_shift);

    RUN_INSTR_TEST(ccmp);
    RUN_INSTR_TEST(ccmn);

    print("All v8.0 tests complete.\n");
#ifndef STANDALONE_DECODER
    dr_standalone_exit();
#endif
    if (result)
        return 0;
    return 1;
}
