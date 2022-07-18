/* **********************************************************
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

static int num_simd_saved;
static int num_simd_registers;
static int num_opmask_registers;

#ifndef DR_HOST_NOT_TARGET

#    define NUM_FEATURE_REGISTERS (sizeof(features_t) / sizeof(uint64))

#    define MRS(REG, IDX, FEATS)                                                     \
        do {                                                                         \
            if (IDX > (NUM_FEATURE_REGISTERS - 1))                                   \
                CLIENT_ASSERT(false, "Reading undefined AArch64 feature register!"); \
            asm("mrs %0, " #REG : "=r"(FEATS[IDX]));                                 \
        } while (0);

void
read_feature_regs(uint64 isa_features[])
{
    MRS(ID_AA64ISAR0_EL1, AA64ISAR0, isa_features);
    MRS(ID_AA64ISAR1_EL1, AA64ISAR1, isa_features);
    MRS(ID_AA64PFR0_EL1, AA64PFR0, isa_features);
    MRS(ID_AA64MMFR1_EL1, AA64MMFR1, isa_features);
}

static void
get_processor_specific_info(void)
{
    uint64 isa_features[NUM_FEATURE_REGISTERS];

    /* FIXME i#5474: Catch and handle SIGILL if MRS not supported.
     * Placeholder for some older kernels on v8.0 systems which do not support
     * this, raising a SIGILL.
     */
    if (!mrs_id_reg_supported()) {
        ASSERT_CURIOSITY(false && "MRS instruction unsupported");
        SYSLOG_INTERNAL_WARNING("MRS instruction unsupported");
        return;
    }

    /* Reads instruction attribute and preocessor feature registers
     * ID_AA64ISAR0_EL1, ID_AA64ISAR1_EL1, ID_AA64PFR0_EL1 and ID_AA64MMFR1_EL1.
     */
    read_feature_regs(isa_features);
    cpu_info.features.flags_aa64isar0 = isa_features[AA64ISAR0];
    cpu_info.features.flags_aa64isar1 = isa_features[AA64ISAR1];
    cpu_info.features.flags_aa64pfr0 = isa_features[AA64PFR0];
    cpu_info.features.flags_aa64mmfr1 = isa_features[AA64MMFR1];
}

#    define LOG_FEATURE(feature)       \
        if (proc_has_feature(feature)) \
            LOG(GLOBAL, LOG_TOP, 1, "   Processor has " #feature "\n");

#endif

void
proc_init_arch(void)
{
    num_simd_saved = MCXT_NUM_SIMD_SLOTS;
    num_simd_registers = MCXT_NUM_SIMD_SLOTS;
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
        LOG(GLOBAL, LOG_TOP, 1, "Processor features:\n ID_AA64ISAR0_EL1 = 0x%016lx\n",
            cpu_info.features.flags_aa64isar0);
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

        LOG(GLOBAL, LOG_TOP, 1, "Processor features:\n ID_AA64ISAR1_EL1 = 0x%016lx\n",
            cpu_info.features.flags_aa64isar1);
        LOG_FEATURE(FEATURE_DPB);
        LOG_FEATURE(FEATURE_DPB2);

        LOG(GLOBAL, LOG_TOP, 1, "Processor features:\n ID_AA64PFR0_EL1 = 0x%016lx\n",
            cpu_info.features.flags_aa64pfr0);
        LOG_FEATURE(FEATURE_FP16);
        LOG_FEATURE(FEATURE_SVE);

        LOG(GLOBAL, LOG_TOP, 1, "Processor features:\n ID_AA64MMFR1_EL1 = 0x%016lx\n",
            cpu_info.features.flags_aa64mmfr1);
        LOG_FEATURE(FEATURE_LOR);
    });
#endif
}

#define GET_FEAT_REG(FEATURE) (feature_reg_idx_t)((((ushort)FEATURE) & 0x7F00) >> 8)
#define GET_FEAT_NIBPOS(FEATURE) ((((ushort)FEATURE) & 0x00F0) >> 4)
#define GET_FEAT_VAL(FEATURE) (((ushort)FEATURE) & 0x000F)
#define GET_FEAT_NSFLAG(FEATURE) ((((ushort)FEATURE) & 0x8000) >> 15)

bool
proc_has_feature(feature_bit_t f)
{
#ifndef DR_HOST_NOT_TARGET
    /* Pretend features are supported for codec tests run on h/w which does not
     * support all features.
     */
#    if defined(BUILD_TESTS)
    if (f == FEATURE_LSE || f == FEATURE_RDM || f == FEATURE_FP16 ||
        f == FEATURE_DotProd || f == FEATURE_SVE || f == FEATURE_LOR)
        return true;
#    endif
    ushort feat_nibble, feat_val, freg_nibble, feat_nsflag;
    uint64 freg_val = 0;

    feature_reg_idx_t feat_reg = GET_FEAT_REG(f);
    if (feat_reg >= AA64ISAR0 && feat_reg <= AA64PFR0) {
        switch (feat_reg) {
        case AA64ISAR0: {
            freg_val = cpu_info.features.flags_aa64isar0;
            break;
        }
        case AA64ISAR1: {
            freg_val = cpu_info.features.flags_aa64isar1;
            break;
        }
        case AA64PFR0: {
            freg_val = cpu_info.features.flags_aa64pfr0;
            break;
        }
        case AA64MMFR1: {
            freg_val = cpu_info.features.flags_aa64mmfr1;
            break;
        }
        default: CLIENT_ASSERT(false, "proc_has_feature: feature register index error");
        }
    } else {
        CLIENT_ASSERT(false, "proc_has_feature: invalid feature register");
    }

    /* Compare the nibble value in the feature register with the input
     * feature nibble value to establish if the feature set represented by the
     * nibble is supported. If the nibble value in the feature register is
     * 0 or 0xF, the feature is not supported at all
     */
    feat_nibble = GET_FEAT_NIBPOS(f);
    freg_nibble = (freg_val >> (feat_nibble * 4)) & 0xFULL;
    feat_nsflag = GET_FEAT_NSFLAG(f);
    if (feat_nsflag == 0 && freg_nibble == 0)
        return false;
    if (feat_nsflag == 1 && freg_nibble == 0xF)
        return false;

    feat_val = GET_FEAT_VAL(f);
    return (freg_nibble >= feat_val) ? true : false;
#else
    return false;
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
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
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
    return num_simd_registers;
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
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

void
dr_insert_restore_fpstate(void *drcontext, instrlist_t *ilist, instr_t *where, opnd_t buf)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

uint64
proc_get_timestamp(void)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return 0;
}
