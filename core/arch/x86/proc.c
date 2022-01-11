/* **********************************************************
 * Copyright (c) 2013-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2000-2008 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2000-2001 Hewlett-Packard Company */

/*
 * proc.c - processor-specific routines
 */

#include "../globals.h"
#include "proc.h"
#include "instr.h"               /* for dr_insert_{save,restore}_fpstate */
#include "instrument.h"          /* for dr_insert_{save,restore}_fpstate */
#include "instr_create_shared.h" /* for dr_insert_{save,restore}_fpstate */
#include "decode.h"              /* for dr_insert_{save,restore}_fpstate */

#ifdef DEBUG
/* case 10450: give messages to clients */
/* we can't undef ASSERT b/c of DYNAMO_OPTION */
#    undef ASSERT_TRUNCATE
#    undef ASSERT_BITFIELD_TRUNCATE
#    undef ASSERT_NOT_REACHED
#    define ASSERT_TRUNCATE DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#    define ASSERT_BITFIELD_TRUNCATE DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#    define ASSERT_NOT_REACHED DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#endif

/* Intel processors:  ebx:edx:ecx spell GenuineIntel */
#define INTEL_EBX /* Genu */ 0x756e6547
#define INTEL_EDX /* ineI */ 0x49656e69
#define INTEL_ECX /* ntel */ 0x6c65746e

/* AMD processors:  ebx:edx:ecx spell AuthenticAMD */
#define AMD_EBX /* Auth */ 0x68747541
#define AMD_EDX /* enti */ 0x69746e65
#define AMD_ECX /* cAMD */ 0x444d4163

static bool avx_enabled;
static bool avx512_enabled;

static int num_simd_saved;
static int num_simd_registers;
static int num_simd_sse_avx_registers;
static int num_simd_sse_avx_saved;
static int num_opmask_registers;
static int xstate_area_kmask_offs;
static int xstate_area_zmm_hi256_offs;
static int xstate_area_hi16_zmm_offs;
/* TODO i#3581: mpx state offsets. */

/* global writable variable for debug registers value */
DECLARE_NEVERPROT_VAR(app_pc d_r_debug_register[DEBUG_REGISTERS_NB], { 0 });

static void
get_cache_sizes_amd(uint max_ext_val)
{
    uint cpuid_res_local[4]; /* eax, ebx, ecx, and edx registers (in that order) */

    if (max_ext_val >= 0x80000005) {
        our_cpuid((int *)cpuid_res_local, 0x80000005, 0);
        proc_set_cache_size((cpuid_res_local[2] /*ecx*/ >> 24), &cpu_info.L1_icache_size);
        proc_set_cache_size((cpuid_res_local[3] /*edx*/ >> 24), &cpu_info.L1_dcache_size);
    }

    if (max_ext_val >= 0x80000006) {
        our_cpuid((int *)cpuid_res_local, 0x80000006, 0);
        proc_set_cache_size((cpuid_res_local[2] /*ecx*/ >> 16), &cpu_info.L2_cache_size);
    }
}

static void
get_cache_sizes_intel(uint max_val)
{
    /* declare as uint so compiler won't complain when we write GP regs to the array */
    uint cache_codes[4];
    int i;

    if (max_val < 2)
        return;

    our_cpuid((int *)cache_codes, 2, 0);
    /* The lower 8 bits of eax specify the number of times cpuid
     * must be executed to obtain a complete picture of the cache
     * characteristics.
     */
    CLIENT_ASSERT((cache_codes[0] & 0xff) == 1, "cpuid error");
    cache_codes[0] &= ~0xff;

    /* Cache codes are stored in consecutive bytes in the
     * GP registers.  For each register, a 1 in bit 31
     * indicates that the codes should be ignored... zero
     * all four bytes when that happens
     */
    for (i = 0; i < 4; i++) {
        if (cache_codes[i] & 0x80000000)
            cache_codes[i] = 0;
    }

    /* Table 3-17, pg 3-171 of IA-32 instruction set reference lists
     * all codes.  Omitting L3 cache characteristics for now...
     */
    for (i = 0; i < 16; i++) {
        switch (((uchar *)cache_codes)[i]) {
        case 0x06: cpu_info.L1_icache_size = CACHE_SIZE_8_KB; break;
        case 0x08: cpu_info.L1_icache_size = CACHE_SIZE_16_KB; break;
        case 0x0a: cpu_info.L1_dcache_size = CACHE_SIZE_8_KB; break;
        case 0x0c: cpu_info.L1_dcache_size = CACHE_SIZE_16_KB; break;
        case 0x2c: cpu_info.L1_dcache_size = CACHE_SIZE_32_KB; break;
        case 0x30: cpu_info.L1_icache_size = CACHE_SIZE_32_KB; break;
        case 0x41: cpu_info.L2_cache_size = CACHE_SIZE_128_KB; break;
        case 0x42: cpu_info.L2_cache_size = CACHE_SIZE_256_KB; break;
        case 0x43: cpu_info.L2_cache_size = CACHE_SIZE_512_KB; break;
        case 0x44: cpu_info.L2_cache_size = CACHE_SIZE_1_MB; break;
        case 0x45: cpu_info.L2_cache_size = CACHE_SIZE_2_MB; break;
        case 0x60: cpu_info.L1_dcache_size = CACHE_SIZE_16_KB; break;
        case 0x66: cpu_info.L1_dcache_size = CACHE_SIZE_8_KB; break;
        case 0x67: cpu_info.L1_dcache_size = CACHE_SIZE_16_KB; break;
        case 0x68: cpu_info.L1_dcache_size = CACHE_SIZE_32_KB; break;
        case 0x78: cpu_info.L2_cache_size = CACHE_SIZE_1_MB; break;
        case 0x79: cpu_info.L2_cache_size = CACHE_SIZE_128_KB; break;
        case 0x7a: cpu_info.L2_cache_size = CACHE_SIZE_256_KB; break;
        case 0x7b: cpu_info.L2_cache_size = CACHE_SIZE_512_KB; break;
        case 0x7c: cpu_info.L2_cache_size = CACHE_SIZE_1_MB; break;
        case 0x7d: cpu_info.L2_cache_size = CACHE_SIZE_2_MB; break;
        case 0x7f: cpu_info.L2_cache_size = CACHE_SIZE_512_KB; break;
        case 0x82: cpu_info.L2_cache_size = CACHE_SIZE_256_KB; break;
        case 0x83: cpu_info.L2_cache_size = CACHE_SIZE_512_KB; break;
        case 0x84: cpu_info.L2_cache_size = CACHE_SIZE_1_MB; break;
        case 0x85: cpu_info.L2_cache_size = CACHE_SIZE_2_MB; break;
        case 0x86: cpu_info.L2_cache_size = CACHE_SIZE_512_KB; break;
        case 0x87: cpu_info.L2_cache_size = CACHE_SIZE_1_MB; break;
        default: break;
        }
    }
}

static void
get_xstate_area_offsets(bool has_kmask, bool has_zmm_hi256, bool has_hi16_zmm)
{
    int cpuid_res_local[4]; /* eax, ebx, ecx, and edx registers (in that order) */
    DOLOG(1, LOG_TOP, {
        if (has_kmask || has_zmm_hi256 || has_hi16_zmm) {
            LOG(GLOBAL, LOG_TOP, 1, "\tExtended xstate area offsets:\n",
                xstate_area_kmask_offs);
        }
    });
    if (has_kmask) {
        our_cpuid(cpuid_res_local, 0xd, 5);
        xstate_area_kmask_offs = cpuid_res_local[1];
        LOG(GLOBAL, LOG_TOP, 1, "\t\tkmask: %d\n", xstate_area_kmask_offs);
    }
    if (has_zmm_hi256) {
        our_cpuid(cpuid_res_local, 0xd, 6);
        xstate_area_zmm_hi256_offs = cpuid_res_local[1];
        LOG(GLOBAL, LOG_TOP, 1, "\t\tzmm_hi256: %d\n", xstate_area_zmm_hi256_offs);
    }
    if (has_hi16_zmm) {
        our_cpuid(cpuid_res_local, 0xd, 7);
        xstate_area_hi16_zmm_offs = cpuid_res_local[1];
        LOG(GLOBAL, LOG_TOP, 1, "\t\thi16_zmm: %d\n", xstate_area_hi16_zmm_offs);
    }
}

/*
 * On Pentium through Pentium III, I-cache lines are 32 bytes.
 * On Pentium IV they are 64 bytes.
 */
static void
get_processor_specific_info(void)
{
    /* use cpuid instruction to get processor info.  For details, see
     * http://download.intel.com/design/Xeon/applnots/24161830.pdf
     * "AP-485: Intel Processor Identification and the CPUID
     * instruction", 96 pages, January 2006
     */
    uint res_eax, res_ebx = 0, res_ecx = 0, res_edx = 0;
    uint max_val, max_ext_val;
    int cpuid_res_local[4]; /* eax, ebx, ecx, and edx registers (in that order) */

    /* First check for existence of the cpuid instruction
     * by attempting to modify bit 21 of eflags
     */
    /* FIXME: Perhaps we should abort when the cpuid instruction
     * doesn't exist since the cache_line_size may be incorrect.
     * (see case 463 for discussion)
     */
    if (!cpuid_supported()) {
        ASSERT_CURIOSITY(false && "cpuid instruction unsupported");
        SYSLOG_INTERNAL_WARNING("cpuid instruction unsupported -- cache_line_size "
                                "may be incorrect");
        return;
    }

    /* first verify on Intel processor */
    our_cpuid(cpuid_res_local, 0, 0);
    res_eax = cpuid_res_local[0];
    res_ebx = cpuid_res_local[1];
    res_ecx = cpuid_res_local[2];
    res_edx = cpuid_res_local[3];
    max_val = res_eax;

    if (res_ebx == INTEL_EBX) {
        cpu_info.vendor = VENDOR_INTEL;
        CLIENT_ASSERT(res_edx == INTEL_EDX && res_ecx == INTEL_ECX,
                      "unknown Intel processor type");
    } else if (res_ebx == AMD_EBX) {
        cpu_info.vendor = VENDOR_AMD;
        CLIENT_ASSERT(res_edx == AMD_EDX && res_ecx == AMD_ECX,
                      "unknown AMD processor type");
    } else {
        cpu_info.vendor = VENDOR_UNKNOWN;
        SYSLOG_INTERNAL_ERROR("Running on unknown processor type");
        LOG(GLOBAL, LOG_TOP, 1, "cpuid returned " PFX " " PFX " " PFX " " PFX "\n",
            res_eax, res_ebx, res_ecx, res_edx);
    }

    /* Try to get extended cpuid information */
    our_cpuid(cpuid_res_local, 0x80000000, 0);
    max_ext_val = cpuid_res_local[0] /*eax*/;

    /* Extended feature flags */
    if (max_ext_val >= 0x80000001) {
        our_cpuid(cpuid_res_local, 0x80000001, 0);
        res_ecx = cpuid_res_local[2];
        res_edx = cpuid_res_local[3];
        cpu_info.features.ext_flags_edx = res_edx;
        cpu_info.features.ext_flags_ecx = res_ecx;
    }

    /* Get structured extended feature flags */
    if (max_val >= 0x7) {
        our_cpuid(cpuid_res_local, 0x7, 0);
        res_ebx = cpuid_res_local[1];
        cpu_info.features.sext_flags_ebx = res_ebx;
    }

    /* now get processor info */
    our_cpuid(cpuid_res_local, 1, 0);
    res_eax = cpuid_res_local[0];
    res_ebx = cpuid_res_local[1];
    res_ecx = cpuid_res_local[2];
    res_edx = cpuid_res_local[3];
    /* eax contains basic info:
     *   extended family, extended model, type, family, model, stepping id
     *   20:27,           16:19,          12:13, 8:11,  4:7,   0:3
     */
    cpu_info.type = (res_eax >> 12) & 0x3;
    cpu_info.family = (res_eax >> 8) & 0xf;
    cpu_info.model = (res_eax >> 4) & 0xf;
    cpu_info.stepping = res_eax & 0xf;

    /* Pages 3-164 and 3-165 of the IA-32 instruction set
     * reference instruct us to adjust the family and model
     * numbers as follows.
     */
    if (cpu_info.family == 0x6 || cpu_info.family == 0xf) {
        uint ext_model = (res_eax >> 16) & 0xf;
        cpu_info.model += (ext_model << 4);

        if (cpu_info.family == 0xf) {
            uint ext_family = (res_eax >> 20) & 0xff;
            cpu_info.family += ext_family;
        }
    }

    cpu_info.features.flags_edx = res_edx;
    cpu_info.features.flags_ecx = res_ecx;

    /* Now features.* are complete and we can query */
    if (proc_has_feature(FEATURE_CLFSH)) {
        /* The new manuals imply ebx always holds the
         * cache line size for clflush, not just on P4
         */
        cache_line_size = (res_ebx & 0x0000ff00) >> 5; /* (x >> 8) * 8 == x >> 5 */
    } else if (cpu_info.vendor == VENDOR_INTEL &&
               (cpu_info.family == FAMILY_PENTIUM_3 ||
                cpu_info.family == FAMILY_PENTIUM_2)) {
        /* Pentium III, Pentium II */
        cache_line_size = 32;
    } else if (cpu_info.vendor == VENDOR_AMD && cpu_info.family == FAMILY_ATHLON) {
        /* Athlon */
        cache_line_size = 64;
    } else {
        LOG(GLOBAL, LOG_TOP, 1, "Warning: running on unsupported processor family %d\n",
            cpu_info.family);
        cache_line_size = 32;
    }
    /* people who use this in ALIGN* macros are assuming it's a power of 2 */
    CLIENT_ASSERT((cache_line_size & (cache_line_size - 1)) == 0,
                  "invalid cache line size");

    /* get L1 and L2 cache sizes */
    if (cpu_info.vendor == VENDOR_AMD)
        get_cache_sizes_amd(max_ext_val);
    else
        get_cache_sizes_intel(max_val);

    /* Processor brand string */
    if (max_ext_val >= 0x80000004) {
        our_cpuid((int *)&cpu_info.brand_string[0], 0x80000002, 0);
        our_cpuid((int *)&cpu_info.brand_string[4], 0x80000003, 0);
        our_cpuid((int *)&cpu_info.brand_string[8], 0x80000004, 0);
    }
}

/* arch specific proc info */
void
proc_init_arch(void)
{
    size_t i;

    get_processor_specific_info();
#ifdef X64
    CLIENT_ASSERT(proc_has_feature(FEATURE_LAHF),
                  "Unsupported processor type - processor must support LAHF/SAHF in "
                  "64bit mode.");
    if (!proc_has_feature(FEATURE_LAHF)) {
        FATAL_USAGE_ERROR(UNSUPPORTED_PROCESSOR_LAHF, 2, get_application_name(),
                          get_application_pid());
    }
#endif

#ifdef DEBUG
    /* FIXME: This is a small subset of processor features.  If we
     * care enough to add more, it would probably be best to loop
     * through a const array of feature names.
     */
    if (d_r_stats->loglevel > 0 && (d_r_stats->logmask & LOG_TOP) != 0) {
        LOG(GLOBAL, LOG_TOP, 1, "Processor features:\n\tedx = 0x%08x\n\tecx = 0x%08x\n",
            cpu_info.features.flags_edx, cpu_info.features.flags_ecx);
        LOG(GLOBAL, LOG_TOP, 1, "\text_edx = 0x%08x\n\text_ecx = 0x%08x\n",
            cpu_info.features.ext_flags_edx, cpu_info.features.ext_flags_ecx);
        LOG(GLOBAL, LOG_TOP, 1, "\tsext_ebx = 0x%08x\n",
            cpu_info.features.sext_flags_ebx);
        if (proc_has_feature(FEATURE_XD_Bit))
            LOG(GLOBAL, LOG_TOP, 1, "\tProcessor has XD Bit\n");
        if (proc_has_feature(FEATURE_MMX))
            LOG(GLOBAL, LOG_TOP, 1, "\tProcessor has MMX\n");
        if (proc_has_feature(FEATURE_FXSR))
            LOG(GLOBAL, LOG_TOP, 1, "\tProcessor has fxsave/fxrstor\n");
        if (proc_has_feature(FEATURE_SSE))
            LOG(GLOBAL, LOG_TOP, 1, "\tProcessor has SSE\n");
        if (proc_has_feature(FEATURE_SSE2))
            LOG(GLOBAL, LOG_TOP, 1, "\tProcessor has SSE2\n");
        if (proc_has_feature(FEATURE_SSE3))
            LOG(GLOBAL, LOG_TOP, 1, "\tProcessor has SSE3\n");
        if (proc_has_feature(FEATURE_AVX))
            LOG(GLOBAL, LOG_TOP, 1, "\tProcessor has AVX\n");
        if (proc_has_feature(FEATURE_AVX512F))
            LOG(GLOBAL, LOG_TOP, 1, "\tProcessor has AVX-512F\n");
        if (proc_has_feature(FEATURE_AVX512BW))
            LOG(GLOBAL, LOG_TOP, 1, "\tProcessor has AVX-512BW\n");
        if (proc_has_feature(FEATURE_OSXSAVE))
            LOG(GLOBAL, LOG_TOP, 1, "\tProcessor has OSXSAVE\n");
    }
#endif
    /* PR 264138: for 32-bit CONTEXT we assume fxsave layout */
    CLIENT_ASSERT((proc_has_feature(FEATURE_FXSR) && proc_has_feature(FEATURE_SSE)) ||
                      (!proc_has_feature(FEATURE_FXSR) && !proc_has_feature(FEATURE_SSE)),
                  "Unsupported processor type: SSE and FXSR must match");

    /* As part of lazy context switching of AVX-512 state, the number of saved registers
     * is initialized excluding extended AVX-512 registers.
     */
    num_simd_saved = MCXT_NUM_SIMD_SSE_AVX_SLOTS;
    /* The number of total registers may be switched to include extended AVX-512
     * registers if OS and processor support will be detected further below.
     */
    num_simd_registers = MCXT_NUM_SIMD_SSE_AVX_SLOTS;
    /* Please note that this constant is not assigned based on feature support. It
     * represents the xstate/fpstate/sigcontext structure sizes for non-AVX-512 state.
     */
    num_simd_sse_avx_registers = MCXT_NUM_SIMD_SSE_AVX_SLOTS;
    num_simd_sse_avx_saved = MCXT_NUM_SIMD_SSE_AVX_SLOTS;
    num_opmask_registers = 0;

    if (proc_has_feature(FEATURE_OSXSAVE)) {
        uint bv_high = 0, bv_low = 0;
        dr_xgetbv(&bv_high, &bv_low);
        if (proc_has_feature(FEATURE_AVX)) {
            /* Even if the processor supports AVX, it will #UD on any AVX instruction
             * if the OS hasn't enabled YMM and XMM state saving.
             * To check that, we invoke xgetbv -- for which we need FEATURE_OSXSAVE.
             * FEATURE_OSXSAVE is also listed as one of the 3 steps in Intel Vol 1
             * Fig 13-1: 1) cpuid OSXSAVE; 2) xgetbv 0x6; 3) cpuid AVX.
             * Xref i#1278, i#1030, i#437.
             */
            LOG(GLOBAL, LOG_TOP, 2, "\txgetbv => 0x%08x%08x\n", bv_high, bv_low);
            if (TESTALL(XCR0_AVX | XCR0_SSE, bv_low)) {
                avx_enabled = true;
                LOG(GLOBAL, LOG_TOP, 1, "\tProcessor and OS fully support AVX\n");
            } else {
                LOG(GLOBAL, LOG_TOP, 1, "\tOS does NOT support AVX\n");
            }
        }
        if (proc_has_feature(FEATURE_AVX512F)) {
            if (TESTALL(XCR0_HI16_ZMM | XCR0_ZMM_HI256 | XCR0_OPMASK, bv_low)) {
#if !defined(UNIX)
                /* FIXME i#1312: AVX-512 is not fully supported and is untested on all
                 * non-UNIX builds. A SYSLOG_INTERNAL_ERROR_ONCE is issued on Windows
                 * if AVX-512 code is encountered. Setting DynamoRIO to a state that
                 * partially supports AVX-512 is causing problems, xref i#3949. We
                 * therefore completely disable AVX-512 support in these builds for now.
                 */
#else
                /* XXX i#1312: It had been unclear whether the kernel uses CR0
                 * bits to disable AVX-512 for its own lazy context switching
                 * optimization. If it did, then our lazy context switch would
                 * interfere with the kernel's and more support would be needed.
                 * We have concluded that the Linux kernel does not do its own
                 * lazy context switch optimization for AVX-512 at this time.
                 *
                 * Please note that the 32-bit UNIX build is missing support for
                 * handling AVX-512 state with signals. A SYSLOG_INTERNAL_ERROR_ONCE
                 * will be issued if AVX-512 code is encountered for 32-bit. 64-bit
                 * builds are fully supported.
                 */
                avx512_enabled = true;
                num_simd_registers = MCXT_NUM_SIMD_SLOTS;
                num_opmask_registers = MCXT_NUM_OPMASK_SLOTS;
                LOG(GLOBAL, LOG_TOP, 1, "\tProcessor and OS fully support AVX-512\n");
#endif
            } else {
                LOG(GLOBAL, LOG_TOP, 1, "\tOS does NOT support AVX-512\n");
            }
            get_xstate_area_offsets(TEST(XCR0_OPMASK, bv_low),
                                    TEST(XCR0_ZMM_HI256, bv_low),
                                    TEST(XCR0_HI16_ZMM, bv_low));
        }
    }
    for (i = 0; i < DEBUG_REGISTERS_NB; i++) {
        d_r_debug_register[i] = NULL;
    }
}

bool
proc_has_feature(feature_bit_t f)
{
    uint bit, val = 0;

    /* Cast to int to avoid tautological comparison if feature_bit_t enum is
     * unsigned. */
    if ((int)f >= 0 && f <= 31) {
        val = cpu_info.features.flags_edx;
    } else if (f >= 32 && f <= 63) {
        val = cpu_info.features.flags_ecx;
    } else if (f >= 64 && f <= 95) {
        val = cpu_info.features.ext_flags_edx;
    } else if (f >= 96 && f <= 127) {
        val = cpu_info.features.ext_flags_ecx;
    } else if (f >= 128 && f <= 159) {
        val = cpu_info.features.sext_flags_ebx;
    } else {
        CLIENT_ASSERT(false, "proc_has_feature: invalid parameter");
    }

    bit = f % 32;
    return TEST((1 << bit), val);
}

/* No synchronization routines necessary.  The Pentium hardware
 * guarantees that the i and d caches are consistent. */
void
machine_cache_sync(void *pc_start, void *pc_end, bool flush_icache)
{
    /* empty */
}

DR_API
size_t
proc_fpstate_save_size(void)
{
    CLIENT_ASSERT(opnd_size_in_bytes(OPSZ_512) == 512 &&
                      opnd_size_in_bytes(OPSZ_108) == 108,
                  "internal sizing discrepancy");
    return (proc_has_feature(FEATURE_FXSR) ? 512 : 108);
}

DR_API
int
proc_num_simd_saved(void)
{
    return num_simd_saved;
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

void
proc_set_num_simd_saved(int num)
{
#if !defined(UNIX)
    /* FIXME i#1312: support and test. */
#else
    SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
    ATOMIC_4BYTE_WRITE(&num_simd_saved, num, false);
    SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
#endif
}

int
proc_num_simd_sse_avx_registers(void)
{
    return num_simd_sse_avx_registers;
}

int
proc_num_simd_sse_avx_saved(void)
{
    return num_simd_sse_avx_saved;
}

int
proc_xstate_area_kmask_offs(void)
{
    return xstate_area_kmask_offs;
}

int
proc_xstate_area_zmm_hi256_offs(void)
{
    return xstate_area_zmm_hi256_offs;
}

int
proc_xstate_area_hi16_zmm_offs(void)
{
    return xstate_area_hi16_zmm_offs;
}

DR_API
size_t
proc_save_fpstate(byte *buf)
{
    /* MUST be 16-byte aligned */
    CLIENT_ASSERT((((ptr_uint_t)buf) & 0x0000000f) == 0,
                  "proc_save_fpstate: buf must be 16-byte aligned");
    if (proc_has_feature(FEATURE_FXSR)) {
        /* Not using inline asm for identical cross-platform code
         * here.  An extra function call won't hurt here.
         */
#ifdef X64
        if (X64_MODE_DC(get_thread_private_dcontext()))
            dr_fxsave(buf);
        else
            dr_fxsave32(buf);
#else
        dr_fxsave(buf);
#endif
    } else {
#ifdef WINDOWS
        dr_fnsave(buf);
#else
        asm volatile("fnsave %0 ; fwait" : "=m"((*buf)));
#endif
    }
    return proc_fpstate_save_size();
}

DR_API
void
proc_restore_fpstate(byte *buf)
{
    /* MUST be 16-byte aligned */
    CLIENT_ASSERT((((ptr_uint_t)buf) & 0x0000000f) == 0,
                  "proc_restore_fpstate: buf must be 16-byte aligned");
    if (proc_has_feature(FEATURE_FXSR)) {
        /* Not using inline asm for identical cross-platform code
         * here.  An extra function call won't hurt here.
         */
#ifdef X64
        if (X64_MODE_DC(get_thread_private_dcontext()))
            dr_fxrstor(buf);
        else
            dr_fxrstor32(buf);
#else
        dr_fxrstor(buf);
#endif
    } else {
#ifdef WINDOWS
        dr_frstor(buf);
#else
        asm volatile("frstor %0" : : "m"((*buf)));
#endif
    }
}

/* XXX: we do not translate the last fp pc (xref i#698).  If a client ever needs that
 * we can try to support it in the future.
 */
void
dr_insert_save_fpstate(void *drcontext, instrlist_t *ilist, instr_t *where, opnd_t buf)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    if (proc_has_feature(FEATURE_FXSR)) {
        /* we want "fxsave, fnclex, finit" */
        CLIENT_ASSERT(opnd_get_size(buf) == OPSZ_512,
                      "dr_insert_save_fpstate: opnd size must be OPSZ_512");
        if (X64_MODE_DC(dcontext))
            instrlist_meta_preinsert(ilist, where, INSTR_CREATE_fxsave64(dcontext, buf));
        else
            instrlist_meta_preinsert(ilist, where, INSTR_CREATE_fxsave32(dcontext, buf));
        instrlist_meta_preinsert(ilist, where, INSTR_CREATE_fnclex(dcontext));
        instrlist_meta_preinsert(ilist, where, INSTR_CREATE_fwait(dcontext));
        instrlist_meta_preinsert(ilist, where, INSTR_CREATE_fninit(dcontext));
    } else {
        /* auto-adjust opnd size so it will encode */
        if (opnd_get_size(buf) == OPSZ_512)
            opnd_set_size(&buf, OPSZ_108);
        /* FIXME: why is this appending fwait, vs "fsave" which prepends? */
        instrlist_meta_preinsert(ilist, where, INSTR_CREATE_fnsave(dcontext, buf));
        instrlist_meta_preinsert(ilist, where, INSTR_CREATE_fwait(dcontext));
    }
}

void
dr_insert_restore_fpstate(void *drcontext, instrlist_t *ilist, instr_t *where, opnd_t buf)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    if (proc_has_feature(FEATURE_FXSR)) {
        CLIENT_ASSERT(opnd_get_size(buf) == OPSZ_512,
                      "dr_insert_save_fpstate: opnd size must be OPSZ_512");
        if (X64_MODE_DC(dcontext))
            instrlist_meta_preinsert(ilist, where, INSTR_CREATE_fxrstor64(dcontext, buf));
        else
            instrlist_meta_preinsert(ilist, where, INSTR_CREATE_fxrstor32(dcontext, buf));
    } else {
        /* auto-adjust opnd size so it will encode */
        if (opnd_get_size(buf) == OPSZ_512)
            opnd_set_size(&buf, OPSZ_108);
        instrlist_meta_preinsert(ilist, where, INSTR_CREATE_frstor(dcontext, buf));
    }
}

bool
proc_avx_enabled(void)
{
    return avx_enabled;
}

bool
proc_avx512_enabled(void)
{
    return avx512_enabled;
}
