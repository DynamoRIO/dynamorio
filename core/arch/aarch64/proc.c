/* **********************************************************
 * Copyright (c) 2022-2025 Google, Inc.  All rights reserved.
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
#include "proc.h"
#include "arch.h"
#include "instr.h"

#if defined(MACOS)
#    include <sys/sysctl.h>
#endif

static int num_simd_saved;
static int num_simd_registers;
static int num_svep_registers;
static int num_ffr_registers;
static int num_opmask_registers;

#define GET_FEAT_REG(FEATURE) (feature_reg_idx_t)((((ushort)FEATURE) & 0x3F00) >> 8)
#define GET_FEAT_NIBPOS(FEATURE) ((((ushort)FEATURE) & 0x00F0) >> 4)
#define GET_FEAT_VAL(FEATURE) (((ushort)FEATURE) & 0x000F)
#define GET_FEAT_NSFLAG(FEATURE) ((((ushort)FEATURE) & 0x8000) >> 15)
#define GET_FEAT_EXACT_MATCH(FEATURE) ((((ushort)FEATURE) & 0x4000) >> 14)

void
proc_set_feature(feature_bit_t feature_bit, bool enable)
{
    uint64 *freg_val = cpu_info.features.isa_features;
    ushort feat_nibble = GET_FEAT_NIBPOS(feature_bit);
    bool feat_nsflag = GET_FEAT_NSFLAG(feature_bit);
    uint64 feat_val = GET_FEAT_VAL(feature_bit);

    feature_reg_idx_t feat_reg = GET_FEAT_REG(feature_bit);
    freg_val += feat_reg;

    /* Clear the current feature state. */
    *freg_val &= ~(0xFULL << (feat_nibble * 4));
    if (enable) {
        /* Write the feature value into the feature nibble. */
        *freg_val |= feat_val << (feat_nibble * 4);
    } else if (feat_nsflag) {
        /* If the not-set flag is 0xF, then that needs manually setting. */
        *freg_val |= 0xF << (feat_nibble * 4);
    }
}

#ifndef DR_HOST_NOT_TARGET

#    define NUM_FEATURE_REGISTERS (sizeof(features_t) / sizeof(uint64))

#    define MRS(REG, IDX, FEATS)                                                     \
        do {                                                                         \
            if (IDX >= NUM_FEATURE_REGISTERS)                                        \
                CLIENT_ASSERT(false, "Reading undefined AArch64 feature register!"); \
            asm("mrs %0, " #REG : "=r"(FEATS[IDX]));                                 \
        } while (0)

void
read_feature_regs(uint64 isa_features[])
{
    MRS(ID_AA64ISAR0_EL1, AA64ISAR0, isa_features);
    MRS(ID_AA64ISAR1_EL1, AA64ISAR1, isa_features);
    MRS(ID_AA64PFR0_EL1, AA64PFR0, isa_features);
    MRS(ID_AA64MMFR1_EL1, AA64MMFR1, isa_features);
    MRS(ID_AA64DFR0_EL1, AA64DFR0, isa_features);
    MRS(ID_AA64PFR1_EL1, AA64PFR1, isa_features);

    /* TODO i#3044: Can't use the MRS macro with id_aa64zfr0_el1 as current GCC
     * binutils assembler fails to recognise it without -march=armv9-a+bf16+i8mm
     * build option.
     */
    asm(".inst 0xd5380480\n" /* mrs x0, ID_AA64ZFR0_EL1 */
        "mov %0, x0"
        : "=r"(isa_features[AA64ZFR0])
        :
        : "x0");

    asm(".inst 0xd5380640\n" /* mrs x0, ID_AA64ISAR2_EL1 */
        "mov %0, x0"
        : "=r"(isa_features[AA64ISAR2])
        :
        : "x0");

    if (IF_LINUX_ELSE(!IS_STRING_OPTION_EMPTY(xarch_root), false)) {
        /* We assume we're under QEMU, where this causes a fatal SIGILL (i#7315).
         * XXX i#7315: We'd prefer to use TRY_EXCEPT_ALLOW_NO_DCONTEXT here and
         * remove this xarch_root check, but proc_init() is called prior to
         * init-time signal handling being set up: and we'd need to add SIGILL
         * to the ones caught at init time, which complicates later uses of
         * SIGILL for NUDGESIG_SIGNUM and suspend_signum (and on x86
         * XSTATE_QUERY_SIG): so we'd want SIGILL to only work for try-except
         * at init time. This is all a little too involved to implement right now.
         */
        LOG(GLOBAL, LOG_TOP | LOG_ASYNCH, 1,
            "Skipping MRS of ID_AA64MMFR2_EL1 under QEMU\n");
    } else {
        asm(".inst 0xd5380740\n" /* mrs x0, ID_AA64MMFR2_EL1 */
            "mov %0, x0"
            : "=r"(isa_features[AA64MMFR2])
            :
            : "x0");
    }
}

#    if !defined(MACOS)
static void
get_processor_specific_info(void)
{
    /* XXX i#5474: Catch and handle SIGILL if MRS not supported.
     * Placeholder for some older kernels on v8.0 systems which do not support
     * this, raising a SIGILL.
     */
    if (!mrs_id_reg_supported()) {
        ASSERT_CURIOSITY(false && "MRS instruction unsupported");
        SYSLOG_INTERNAL_WARNING("MRS instruction unsupported");
        return;
    }

    /* Reads instruction attribute and preocessor feature registers
     * ID_AA64ISAR0_EL1, ID_AA64ISAR1_EL1, ID_AA64ISAR2_EL1, ID_AA64PFR0_EL1,
     * ID_AA64MMFR1_EL1, ID_AA64DFR0_EL1, ID_AA64ZFR0_EL1, ID_AA64PFR1_EL1,
     * ID_AA64MMFR2_EL1.
     */
    read_feature_regs(cpu_info.features.isa_features);

    /* The SVE vector length is set to:
     * - A value read from the host hardware.
     * or:
     * - 32 bytes, 256 bits.
     * Which of the above depends on:
     * - SVE or non-SVE AArch64 or x86 host h/w.
     * and:
     * - Release or development test build.
     */
#        if !defined(DR_HOST_NOT_TARGET)
    if (proc_has_feature(FEATURE_SVE)) {
        uint64 vl;
        /* This RDVL instruction is inserted as raw hex because we don't build
         * with SVE enabled: i.e. not -march=armv8-a+sve, so that we can run a
         * single DynamoRIO release on both SVE and non-SVE h/w.
         * i#6852: Some compiler toolchains were observed to generate incorrect
         * asm where the following was no longer gated by the above if-condition
         * which then causes a crash on non-SVE hardware. We apply the volatile
         * modifier to this inline asm block to prevent the compiler from making
         * any potentially problematic transformations or reorderings.
         * TODO i#5365: Ideally this should be generated by INSTR_CREATE_rdvl()
         * and executed at startup time with other initialisation code.
         */
        asm volatile(".inst 0x04bf5020\n" /* rdvl x0, #1 */
                     "mov %0, x0"
                     : "=r"(vl)
                     :
                     : "x0");
        cpu_info.sve_vector_length_bytes = vl;
        dr_set_vector_length(vl * 8);
    } else {
        cpu_info.sve_vector_length_bytes = 32;
        dr_set_vector_length(256);
    }
#        else
    /* Set SVE vector length for unit testing the off-line decoder. */
    dr_set_vector_length(256);
#        endif
}
#    else /* defined(MACOS) */

/* On macOS, MRS appears to be restricted. We'll use sysctl's instead.
 * XXX i#5383: Add remaining features from other sysctls.
 */
static void
get_processor_specific_info(void)
{
    memset(&cpu_info.features, 0, sizeof(cpu_info.features));

#        define SET_FEAT_IF_SYSCTL_EQ(FEATURE, SYSCTL, TY, VAL)                         \
            TY FEATURE##_tmp;                                                           \
            size_t FEATURE##_tmp_buflen = sizeof(FEATURE##_tmp);                        \
            if (sysctlbyname(SYSCTL, &FEATURE##_tmp, &FEATURE##_tmp_buflen, NULL, 0) == \
                -1) {                                                                   \
                ASSERT_CURIOSITY(false && SYSCTL " sysctl failed");                     \
                SYSLOG_INTERNAL_WARNING("Failed to read " SYSCTL " sysctl");            \
            } else if (FEATURE##_tmp_buflen == sizeof(FEATURE##_tmp)) {                 \
                if (FEATURE##_tmp == VAL) {                                             \
                    proc_set_feature(FEATURE, true);                                    \
                }                                                                       \
            }

    SET_FEAT_IF_SYSCTL_EQ(FEATURE_PAUTH, "hw.optional.arm.FEAT_PAuth", uint32_t, 1);
    SET_FEAT_IF_SYSCTL_EQ(FEATURE_FPAC, "hw.optional.arm.FEAT_FPAC", uint32_t, 1);
}
#    endif

#    define LOG_FEATURE(feature)       \
        if (proc_has_feature(feature)) \
            LOG(GLOBAL, LOG_TOP, 1, "   Processor has " #feature "\n");

#endif

void
proc_init_arch(void)
{
    num_simd_saved = MCXT_NUM_SIMD_SVE_SLOTS;
    num_simd_registers = MCXT_NUM_SIMD_SVE_SLOTS;
    num_svep_registers = MCXT_NUM_SVEP_SLOTS;
    num_ffr_registers = MCXT_NUM_FFR_SLOTS;
    num_opmask_registers = MCXT_NUM_OPMASK_SLOTS;

    /* When DR_HOST_NOT_TARGET, get_cache_line_size returns false and does
     * not set any value in given args.
     */
    if (!get_cache_line_size(&cache_line_size,
                             /* icache_line_size= */ NULL)) {
        LOG(GLOBAL, LOG_TOP, 1, "Unable to obtain cache line size");
    }

#ifndef DR_HOST_NOT_TARGET
    get_processor_specific_info();

    DOLOG(1, LOG_TOP, {
        LOG(GLOBAL, LOG_TOP, 1, "Processor features:\n");
        LOG(GLOBAL, LOG_TOP, 1, "ID_AA64ISAR0_EL1 = 0x%016lx\n",
            cpu_info.features.isa_features[AA64ISAR0]);
        LOG_FEATURE(FEATURE_AESX);
        LOG_FEATURE(FEATURE_PMULL);
        LOG_FEATURE(FEATURE_SHA1);
        LOG_FEATURE(FEATURE_SHA256);
        LOG_FEATURE(FEATURE_SHA512);
        LOG_FEATURE(FEATURE_CRC32);
        LOG_FEATURE(FEATURE_LSE);
        LOG_FEATURE(FEATURE_RDM);
        LOG_FEATURE(FEATURE_SHA3);
        LOG_FEATURE(FEATURE_SM3);
        LOG_FEATURE(FEATURE_SM4);
        LOG_FEATURE(FEATURE_DotProd);
        LOG_FEATURE(FEATURE_FHM);
        LOG_FEATURE(FEATURE_FlagM);
        LOG_FEATURE(FEATURE_FlagM2);
        LOG_FEATURE(FEATURE_RNG);

        LOG(GLOBAL, LOG_TOP, 1, "ID_AA64ISAR1_EL1 = 0x%016lx\n",
            cpu_info.features.isa_features[AA64ISAR1]);
        LOG_FEATURE(FEATURE_DPB);
        LOG_FEATURE(FEATURE_DPB2);
        LOG_FEATURE(FEATURE_JSCVT);
        LOG_FEATURE(FEATURE_PAUTH);
        LOG_FEATURE(FEATURE_LS64);
        LOG_FEATURE(FEATURE_LS64V);
        LOG_FEATURE(FEATURE_LS64ACCDATA);

        LOG(GLOBAL, LOG_TOP, 1, "ID_AA64PFR0_EL1 = 0x%016lx\n",
            cpu_info.features.isa_features[AA64PFR0]);
        LOG_FEATURE(FEATURE_FP16);
        LOG_FEATURE(FEATURE_RAS);
        LOG_FEATURE(FEATURE_SVE);
        LOG_FEATURE(FEATURE_DIT);

        LOG(GLOBAL, LOG_TOP, 1, "ID_AA64MMFR1_EL1 = 0x%016lx\n",
            cpu_info.features.isa_features[AA64MMFR1]);
        LOG_FEATURE(FEATURE_LOR);

        LOG(GLOBAL, LOG_TOP, 1, "ID_AA64DFR0_EL1 = 0x%016lx\n",
            cpu_info.features.isa_features[AA64DFR0]);
        LOG_FEATURE(FEATURE_SPE);
        LOG_FEATURE(FEATURE_LRCPC);
        LOG_FEATURE(FEATURE_LRCPC2);
        LOG_FEATURE(FEATURE_FRINTTS);

        LOG(GLOBAL, LOG_TOP, 1, "ID_AA64ZFR0_EL1 = 0x%016lx\n",
            cpu_info.features.isa_features[AA64ZFR0]);
        LOG_FEATURE(FEATURE_BF16);
        LOG_FEATURE(FEATURE_I8MM);
        LOG_FEATURE(FEATURE_F64MM);

        LOG(GLOBAL, LOG_TOP, 1, "ID_AA64PFR1_EL1 = 0x%016lx\n",
            cpu_info.features.isa_features[AA64PFR1]);
        LOG_FEATURE(FEATURE_MTE);
        LOG_FEATURE(FEATURE_MTE2);
        LOG_FEATURE(FEATURE_BTI);
        LOG_FEATURE(FEATURE_SSBS);
        LOG_FEATURE(FEATURE_SSBS2);

        LOG(GLOBAL, LOG_TOP, 1, "ID_AA64ISAR2_EL1 = 0x%016lx\n",
            cpu_info.features.isa_features[AA64ISAR2]);
        LOG_FEATURE(FEATURE_PAUTH2);
        LOG_FEATURE(FEATURE_FPAC);
        LOG_FEATURE(FEATURE_FPACCOMBINE);
        LOG_FEATURE(FEATURE_CONSTPACFIELD);
        LOG_FEATURE(FEATURE_WFxT);

        LOG(GLOBAL, LOG_TOP, 1, "ID_AA64MMFR2_EL1 = 0x%016lx\n",
            cpu_info.features.isa_features[AA64MMFR2]);
        LOG_FEATURE(FEATURE_LSE2);
    });
#endif
}

void
enable_all_test_cpu_features()
{
    const feature_bit_t features[] = {
        FEATURE_LSE,        FEATURE_RDM,         FEATURE_FP16,          FEATURE_DotProd,
        FEATURE_SVE,        FEATURE_LOR,         FEATURE_FHM,           FEATURE_SM3,
        FEATURE_SM4,        FEATURE_SHA512,      FEATURE_SHA3,          FEATURE_RAS,
        FEATURE_SPE,        FEATURE_PAUTH,       FEATURE_LRCPC,         FEATURE_LRCPC2,
        FEATURE_BF16,       FEATURE_I8MM,        FEATURE_F64MM,         FEATURE_FlagM,
        FEATURE_JSCVT,      FEATURE_DPB,         FEATURE_DPB2,          FEATURE_SVE2,
        FEATURE_SVEAES,     FEATURE_SVEBitPerm,  FEATURE_SVESHA3,       FEATURE_SVESM4,
        FEATURE_MTE,        FEATURE_BTI,         FEATURE_FRINTTS,       FEATURE_PAUTH2,
        FEATURE_MTE2,       FEATURE_FlagM2,      FEATURE_CONSTPACFIELD, FEATURE_SSBS,
        FEATURE_SSBS2,      FEATURE_DIT,         FEATURE_LSE2,          FEATURE_WFxT,
        FEATURE_FPAC,       FEATURE_FPACCOMBINE, FEATURE_LS64,          FEATURE_LS64V,
        FEATURE_LS64ACCDATA
    };
    for (int i = 0; i < BUFFER_SIZE_ELEMENTS(features); ++i) {
        proc_set_feature(features[i], true);
    }
    dr_set_vector_length(256);
}

#ifndef DR_HOST_NOT_TARGET
static uint64
get_reg_val(feature_reg_idx_t feat_reg)
{
    return cpu_info.features.isa_features[feat_reg];
}

static bool
proc_has_feature_imp(feature_bit_t feature_bit)
{
    bool has_feature = false;

    uint64 reg_val = get_reg_val(GET_FEAT_REG(feature_bit));
    ushort nibble_pos = GET_FEAT_NIBPOS(feature_bit);
    ushort reg_nibble = (reg_val >> (nibble_pos * 4)) & 0xFULL;

    bool feat_nsflag = GET_FEAT_NSFLAG(feature_bit);
    ushort feat_nibble = GET_FEAT_VAL(feature_bit);
    bool exact_match = GET_FEAT_EXACT_MATCH(feature_bit);

    if (feat_nsflag) {
        // special case where a value of 0xF signifies the feature is not present
        has_feature = (reg_nibble != 0xF);
    } else {
        if (exact_match)
            has_feature = (reg_nibble == feat_nibble);
        else
            has_feature = (reg_nibble >= feat_nibble);
    }

    return has_feature;
}

/*
 * Some features are identified by more than one nibble.
 * In this case we need to add extra mappings between features and nibble values
 */
static uint32 feature_ids[] = {
    (FEATURE_PAUTH << 16) |
        DEF_FEAT(AA64ISAR1, 1, 1,
                 FEAT_GR_EQ), /* APA - QARMA5 algorithm for address authentication */
    (FEATURE_PAUTH << 16) |
        DEF_FEAT(AA64ISAR1, 6, 1,
                 FEAT_GR_EQ), /* GPA - QARMA5 algorithm for generic code authentication */
    (FEATURE_PAUTH << 16) |
        DEF_FEAT(AA64ISAR1, 7, 1, FEAT_GR_EQ), /* GPI - IMPLEMENTATION DEFINED algorithm
                                               for generic code authentication */
    (FEATURE_PAUTH << 16) |
        DEF_FEAT(
            AA64ISAR2, 2, 1,
            FEAT_GR_EQ), /* GPA3 - QARMA3 algorithm for generic code authentication  */
    (FEATURE_PAUTH << 16) |
        DEF_FEAT(AA64ISAR2, 3, 1,
                 FEAT_GR_EQ), /* APA3 - QARMA3 algorithm for address authentication */
    (FEATURE_PAUTH2 << 16) |
        DEF_FEAT(AA64ISAR1, 1, 3, FEAT_GR_EQ), /* APA (QARMA5 - EnhancedPAC2) */
    (FEATURE_PAUTH2 << 16) |
        DEF_FEAT(AA64ISAR1, 2, 3, FEAT_GR_EQ), /* API (IMP DEF algorithm) */
    (FEATURE_I8MM << 16) |
        DEF_FEAT(AA64ISAR1, 13, 1, FEAT_EQ), /* I8MM (Int8 Matrix mul.) */

    (FEATURE_FPAC << 16) |
        DEF_FEAT(AA64ISAR1, 1, 4, FEAT_GR_EQ), /* APA (QARMA5 - FPAC) */
    (FEATURE_FPAC << 16) |
        DEF_FEAT(AA64ISAR1, 2, 4, FEAT_GR_EQ), /* API (IMP DEF algorithm - FPAC) */

    (FEATURE_FPACCOMBINE << 16) |
        DEF_FEAT(AA64ISAR1, 1, 5, FEAT_GR_EQ), /* APA (QARMA5 - FPACCOMBINE) */
    (FEATURE_FPACCOMBINE << 16) |
        DEF_FEAT(AA64ISAR1, 2, 5, FEAT_GR_EQ), /* API (IMP DEF algorithm - FPACCOMBINE) */
};

static bool
check_extra_nibbles(feature_bit_t feature_bit)
{
    int array_length = sizeof(feature_ids) / sizeof(*feature_ids);

    int i;
    for (i = 0; i < array_length; i++) {
        ushort feat_bit = feature_ids[i] >> 16;

        if (feat_bit == (ushort)feature_bit) {
            if (proc_has_feature_imp((ushort)feature_ids[i]))
                return true;
        }
    }

    return false;
}
#endif /* DR_HOST_NOT_TARGET */

bool
proc_has_feature(feature_bit_t feature_bit)
{
#ifdef DR_HOST_NOT_TARGET
    return false;
#else
    bool has_feature = proc_has_feature_imp(feature_bit);

    /* Some features are identified in more than one place. */
    if (!has_feature)
        has_feature = check_extra_nibbles(feature_bit);

    return has_feature;
#endif
}

void
machine_cache_sync(void *pc_start, void *pc_end, bool flush_icache)
{
    clear_icache(pc_start, pc_end);
}

DR_API
size_t
proc_fpstate_save_size(void)
{
    ASSERT_NOT_IMPLEMENTED(false); /* TODO i#1569 */
    return 0;
}

DR_API
int
proc_num_simd_saved(void)
{
    return num_simd_saved;
}

void
proc_set_num_simd_saved(int num)
{
    SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
    ATOMIC_4BYTE_WRITE(&num_simd_saved, num, false);
    SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
}

DR_API
int
proc_num_simd_registers(void)
{
    return num_simd_registers +
        (proc_has_feature(FEATURE_SVE) ? (num_svep_registers + num_ffr_registers) : 0);
}

DR_API
int
proc_num_opmask_registers(void)
{
    return num_opmask_registers;
}

int
proc_num_simd_sse_avx_registers(void)
{
    CLIENT_ASSERT(false, "Incorrect usage for ARM/AArch64.");
    return 0;
}

int
proc_num_simd_sse_avx_saved(void)
{
    CLIENT_ASSERT(false, "Incorrect usage for ARM/AArch64.");
    return 0;
}

int
proc_xstate_area_kmask_offs(void)
{
    /* Does not apply to AArch64. */
    ASSERT_NOT_REACHED();
    return 0;
}

int
proc_xstate_area_zmm_hi256_offs(void)
{
    /* Does not apply to AArch64. */
    ASSERT_NOT_REACHED();
    return 0;
}

int
proc_xstate_area_hi16_zmm_offs(void)
{
    /* Does not apply to AArch64. */
    ASSERT_NOT_REACHED();
    return 0;
}

DR_API
size_t
proc_save_fpstate(byte *buf)
{
    /* All registers are saved by insert_push_all_registers so nothing extra
     * needs to be saved here.
     */
    return DR_FPSTATE_BUF_SIZE;
}

DR_API
void
proc_restore_fpstate(byte *buf)
{
    /* Nothing to restore. */
}

void
dr_insert_save_fpstate(void *drcontext, instrlist_t *ilist, instr_t *where, opnd_t buf)
{
    ASSERT_NOT_IMPLEMENTED(false); /* TODO i#1569 */
}

void
dr_insert_restore_fpstate(void *drcontext, instrlist_t *ilist, instr_t *where, opnd_t buf)
{
    ASSERT_NOT_IMPLEMENTED(false); /* TODO i#1569 */
}

uint64
proc_get_timestamp(void)
{
    ASSERT_NOT_IMPLEMENTED(false); /* TODO i#1569 */
    return 0;
}
