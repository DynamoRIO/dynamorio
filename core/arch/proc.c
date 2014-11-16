/* **********************************************************
 * Copyright (c) 2013 Google, Inc.  All rights reserved.
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
#include "instr.h" /* for dr_insert_{save,restore}_fpstate */
#include "instrument.h" /* for dr_insert_{save,restore}_fpstate */
#include "instr_create.h" /* for dr_insert_{save,restore}_fpstate */
#include "decode.h" /* for dr_insert_{save,restore}_fpstate */

#ifdef DEBUG
/* case 10450: give messages to clients */
/* we can't undef ASSERT b/c of DYNAMO_OPTION */
# undef ASSERT_TRUNCATE
# undef ASSERT_BITFIELD_TRUNCATE
# undef ASSERT_NOT_REACHED
# define ASSERT_TRUNCATE DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
# define ASSERT_BITFIELD_TRUNCATE DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
# define ASSERT_NOT_REACHED DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#endif

/* Intel processors:  ebx:edx:ecx spell GenuineIntel */
#define INTEL_EBX /* Genu */ 0x756e6547
#define INTEL_EDX /* ineI */ 0x49656e69
#define INTEL_ECX /* ntel */ 0x6c65746e

/* AMD processors:  ebx:edx:ecx spell AuthenticAMD */
#define AMD_EBX /* Auth */ 0x68747541
#define AMD_EDX /* enti */ 0x69746e65
#define AMD_ECX /* cAMD */ 0x444d4163

/* cache_line_size is exported for efficient access.
 * FIXME: In case the processor doesn't support the
 * cpuid instruction, use a default value of 32.
 * (see case 463 for discussion)
 */
size_t cache_line_size = 32;
static ptr_uint_t mask; /* bits that should be 0 to be cache-line-aligned */

static uint L1_icache_size = CACHE_SIZE_UNKNOWN;
static uint L1_dcache_size = CACHE_SIZE_UNKNOWN;
static uint L2_cache_size  = CACHE_SIZE_UNKNOWN;

static uint vendor   = VENDOR_UNKNOWN;
static uint family   = 0;
static uint type     = 0;
static uint model    = 0;
static uint stepping = 0;

/* Feature bits in 4 32-bit values: features in edx,
 * features in ecx, extended features in edx, and
 * extended features in ecx
 */
static features_t features = {0, 0, 0, 0};

/* The brand string is a 48-character, null terminated string.
 * Declare as a 12-element uint so the compiler won't complain
 * when we store GPRs to it.  Initialization is "unknown" .
 */
static uint brand_string[12] = {0x6e6b6e75, 0x006e776f};

static bool avx_enabled;

static void
set_cache_size(uint val, uint *dst)
{
    CLIENT_ASSERT(dst != NULL, "invalid internal param");
    switch (val) {
        case 8:    *dst = CACHE_SIZE_8_KB;   break;
        case 16:   *dst = CACHE_SIZE_16_KB;  break;
        case 32:   *dst = CACHE_SIZE_32_KB;  break;
        case 64:   *dst = CACHE_SIZE_64_KB;  break;
        case 128:  *dst = CACHE_SIZE_128_KB; break;
        case 256:  *dst = CACHE_SIZE_256_KB; break;
        case 512:  *dst = CACHE_SIZE_512_KB; break;
        case 1024: *dst = CACHE_SIZE_1_MB;   break;
        case 2048: *dst = CACHE_SIZE_2_MB;   break;
        default: SYSLOG_INTERNAL_ERROR("Unknown processor cache size"); break;
    }
}

static void
get_cache_sizes_amd(uint max_ext_val)
{
    uint cpuid_res_local[4]; /* eax, ebx, ecx, and edx registers (in that order) */

    if (max_ext_val >= 0x80000005) {
#ifdef UNIX
        our_cpuid((int*)cpuid_res_local, 0x80000005);
#else
        __cpuid(cpuid_res_local, 0x80000005);
#endif
        set_cache_size((cpuid_res_local[2]/*ecx*/ >> 24), &L1_icache_size);
        set_cache_size((cpuid_res_local[3]/*edx*/ >> 24), &L1_dcache_size);
    }

    if (max_ext_val >= 0x80000006) {
#ifdef UNIX
        our_cpuid((int*)cpuid_res_local, 0x80000006);
#else
        __cpuid(cpuid_res_local, 0x80000006);
#endif
        set_cache_size((cpuid_res_local[2]/*ecx*/ >> 16), &L2_cache_size);
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

#ifdef UNIX
    our_cpuid((int*)cache_codes, 2);
#else
    __cpuid(cache_codes, 2);
#endif
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
    for (i=0; i<4; i++) {
        if (cache_codes[i] & 0x80000000)
            cache_codes[i] = 0;
    }

    /* Table 3-17, pg 3-171 of IA-32 instruction set reference lists
     * all codes.  Omitting L3 cache characteristics for now...
     */
    for (i=0; i<16; i++) {
        switch (((uchar*)cache_codes)[i]) {
            case 0x06: L1_icache_size = CACHE_SIZE_8_KB; break;
            case 0x08: L1_icache_size = CACHE_SIZE_16_KB; break;
            case 0x0a: L1_dcache_size = CACHE_SIZE_8_KB; break;
            case 0x0c: L1_dcache_size = CACHE_SIZE_16_KB; break;
            case 0x2c: L1_dcache_size = CACHE_SIZE_32_KB; break;
            case 0x30: L1_icache_size = CACHE_SIZE_32_KB; break;
            case 0x41: L2_cache_size = CACHE_SIZE_128_KB; break;
            case 0x42: L2_cache_size = CACHE_SIZE_256_KB; break;
            case 0x43: L2_cache_size = CACHE_SIZE_512_KB; break;
            case 0x44: L2_cache_size = CACHE_SIZE_1_MB; break;
            case 0x45: L2_cache_size = CACHE_SIZE_2_MB; break;
            case 0x60: L1_dcache_size = CACHE_SIZE_16_KB; break;
            case 0x66: L1_dcache_size = CACHE_SIZE_8_KB; break;
            case 0x67: L1_dcache_size = CACHE_SIZE_16_KB; break;
            case 0x68: L1_dcache_size = CACHE_SIZE_32_KB; break;
            case 0x78: L2_cache_size = CACHE_SIZE_1_MB; break;
            case 0x79: L2_cache_size = CACHE_SIZE_128_KB; break;
            case 0x7a: L2_cache_size = CACHE_SIZE_256_KB; break;
            case 0x7b: L2_cache_size = CACHE_SIZE_512_KB; break;
            case 0x7c: L2_cache_size = CACHE_SIZE_1_MB; break;
            case 0x7d: L2_cache_size = CACHE_SIZE_2_MB; break;
            case 0x7f: L2_cache_size = CACHE_SIZE_512_KB; break;
            case 0x82: L2_cache_size = CACHE_SIZE_256_KB; break;
            case 0x83: L2_cache_size = CACHE_SIZE_512_KB; break;
            case 0x84: L2_cache_size = CACHE_SIZE_1_MB; break;
            case 0x85: L2_cache_size = CACHE_SIZE_2_MB; break;
            case 0x86: L2_cache_size = CACHE_SIZE_512_KB; break;
            case 0x87: L2_cache_size = CACHE_SIZE_1_MB; break;
            default: break;
        }
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
#ifdef UNIX
    our_cpuid(cpuid_res_local, 0);
#else
    __cpuid(cpuid_res_local, 0);
#endif
    res_eax = cpuid_res_local[0];
    res_ebx = cpuid_res_local[1];
    res_ecx = cpuid_res_local[2];
    res_edx = cpuid_res_local[3];
    max_val = res_eax;

    if (res_ebx == INTEL_EBX) {
        vendor = VENDOR_INTEL;
        CLIENT_ASSERT(res_edx == INTEL_EDX && res_ecx == INTEL_ECX,
                      "unknown Intel processor type");
    } else if (res_ebx == AMD_EBX) {
        vendor = VENDOR_AMD;
        CLIENT_ASSERT(res_edx == AMD_EDX && res_ecx == AMD_ECX,
                      "unknown AMD processor type");
    } else {
        vendor = VENDOR_UNKNOWN;
        SYSLOG_INTERNAL_ERROR("Running on unknown processor type");
        LOG(GLOBAL, LOG_TOP, 1, "cpuid returned "PFX" "PFX" "PFX" "PFX"\n",
            res_eax, res_ebx, res_ecx, res_edx);
    }

    /* Try to get extended cpuid information */
#ifdef UNIX
    our_cpuid(cpuid_res_local, 0x80000000);
#else
    __cpuid(cpuid_res_local, 0x80000000);
#endif
    max_ext_val = cpuid_res_local[0]/*eax*/;

    /* Extended feature flags */
    if (max_ext_val >= 0x80000001) {
#ifdef UNIX
        our_cpuid(cpuid_res_local, 0x80000001);
#else
        __cpuid(cpuid_res_local, 0x80000001);
#endif
        res_ecx = cpuid_res_local[2];
        res_edx = cpuid_res_local[3];
        features.ext_flags_edx = res_edx;
        features.ext_flags_ecx = res_ecx;
    }

    /* now get processor info */
#ifdef UNIX
    our_cpuid(cpuid_res_local, 1);
#else
    __cpuid(cpuid_res_local, 1);
#endif
    res_eax = cpuid_res_local[0];
    res_ebx = cpuid_res_local[1];
    res_ecx = cpuid_res_local[2];
    res_edx = cpuid_res_local[3];
    /* eax contains basic info:
     *   extended family, extended model, type, family, model, stepping id
     *   20:27,           16:19,          12:13, 8:11,  4:7,   0:3
     */
    type   = (res_eax >> 12) & 0x3;
    family = (res_eax >>  8) & 0xf;
    model  = (res_eax >>  4) & 0xf;
    stepping = res_eax & 0xf;

    /* Pages 3-164 and 3-165 of the IA-32 instruction set
     * reference instruct us to adjust the family and model
     * numbers as follows.
     */
    if (family == 0x6 || family == 0xf) {
        uint ext_model = (res_eax >> 16) & 0xf;
        model += (ext_model << 4);

        if (family == 0xf) {
            uint ext_family = (res_eax >> 20) & 0xff;
            family += ext_family;
        }
    }

    features.flags_edx = res_edx;
    features.flags_ecx = res_ecx;

    /* Now features.* are complete and we can query */
    if (proc_has_feature(FEATURE_CLFSH)) {
        /* The new manuals imply ebx always holds the
         * cache line size for clflush, not just on P4
         */
        cache_line_size = (res_ebx & 0x0000ff00) >> 5; /* (x >> 8) * 8 == x >> 5 */
    } else if (vendor == VENDOR_INTEL &&
               (family == FAMILY_PENTIUM_3 || family == FAMILY_PENTIUM_2)) {
        /* Pentium III, Pentium II */
        cache_line_size = 32;
    } else if (vendor == VENDOR_AMD && family == FAMILY_ATHLON) {
        /* Athlon */
        cache_line_size = 64;
#ifdef IA32_ON_IA64
    } else if (vendor == VENDOR_INTEL && family == FAMILY_IA64) {
        /* Itanium */
        cache_line_size = 32;
#endif
    } else {
        LOG(GLOBAL, LOG_TOP, 1, "Warning: running on unsupported processor family %d\n",
            family);
        cache_line_size = 32;
    }
    /* people who use this in ALIGN* macros are assuming it's a power of 2 */
    CLIENT_ASSERT((cache_line_size & (cache_line_size - 1)) == 0,
                  "invalid cache line size");

    /* get L1 and L2 cache sizes */
    if (vendor == VENDOR_AMD)
        get_cache_sizes_amd(max_ext_val);
    else
        get_cache_sizes_intel(max_val);

    /* Processor brand string */
    if (max_ext_val >= 0x80000004) {
#ifdef UNIX
        our_cpuid((int*)&brand_string[0], 0x80000002);
        our_cpuid((int*)&brand_string[4], 0x80000003);
        our_cpuid((int*)&brand_string[8], 0x80000004);
#else
        __cpuid(&brand_string[0], 0x80000002);
        __cpuid(&brand_string[4], 0x80000003);
        __cpuid(&brand_string[8], 0x80000004);
#endif
    }
}

void
proc_init(void)
{
    LOG(GLOBAL, LOG_TOP, 1, "Running on a %d CPU machine\n", get_num_processors());

    get_processor_specific_info();
    CLIENT_ASSERT(cache_line_size > 0, "invalid cache line size");
    mask = (cache_line_size - 1);

    LOG(GLOBAL, LOG_TOP, 1, "Cache line size is %d bytes\n", cache_line_size);
    LOG(GLOBAL, LOG_TOP, 1, "L1 icache=%s, L1 dcache=%s, L2 cache=%s\n",
        proc_get_cache_size_str(proc_get_L1_icache_size()),
        proc_get_cache_size_str(proc_get_L1_dcache_size()),
        proc_get_cache_size_str(proc_get_L2_cache_size()));
    LOG(GLOBAL, LOG_TOP, 1, "Processor brand string = %s\n", brand_string);
    LOG(GLOBAL, LOG_TOP, 1, "Type=0x%x, Family=0x%x, Model=0x%x, Stepping=0x%x\n",
        type, family, model, stepping);

#ifdef X86
# ifdef X64
    CLIENT_ASSERT(proc_has_feature(FEATURE_LAHF),
                  "Unsupported processor type - processor must support LAHF/SAHF in "
                  "64bit mode.");
    if (!proc_has_feature(FEATURE_LAHF)) {
        FATAL_USAGE_ERROR(UNSUPPORTED_PROCESSOR_LAHF, 2,
                          get_application_name(), get_application_pid());
    }
# endif

# ifdef DEBUG
    /* FIXME: This is a small subset of processor features.  If we
     * care enough to add more, it would probably be best to loop
     * through a const array of feature names.
    */
    if (stats->loglevel > 0 && (stats->logmask & LOG_TOP) != 0) {
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
        if (proc_has_feature(FEATURE_OSXSAVE))
            LOG(GLOBAL, LOG_TOP, 1, "\tProcessor has OSXSAVE\n");
    }
# endif
    /* PR 264138: for 32-bit CONTEXT we assume fxsave layout */
    CLIENT_ASSERT((proc_has_feature(FEATURE_FXSR) && proc_has_feature(FEATURE_SSE)) ||
                  (!proc_has_feature(FEATURE_FXSR) && !proc_has_feature(FEATURE_SSE)),
                  "Unsupported processor type: SSE and FXSR must match");

    if (proc_has_feature(FEATURE_AVX) && proc_has_feature(FEATURE_OSXSAVE)) {
        /* Even if the processor supports AVX, it will #UD on any AVX instruction
         * if the OS hasn't enabled YMM and XMM state saving.
         * To check that, we invoke xgetbv -- for which we need FEATURE_OSXSAVE.
         * FEATURE_OSXSAVE is also listed as one of the 3 steps in Intel Vol 1
         * Fig 13-1: 1) cpuid OSXSAVE; 2) xgetbv 0x6; 3) cpuid AVX.
         * Xref i#1278, i#1030, i#437.
         */
        uint bv_high = 0, bv_low = 0;
        dr_xgetbv(&bv_high, &bv_low);
        LOG(GLOBAL, LOG_TOP, 2, "\txgetbv => 0x%08x%08x\n", bv_high, bv_low);
        if (TESTALL(XCR0_AVX|XCR0_SSE, bv_low)) {
            avx_enabled = true;
            LOG(GLOBAL, LOG_TOP, 1, "\tProcessor and OS fully support AVX\n");
        } else {
            LOG(GLOBAL, LOG_TOP, 1, "\tOS does NOT support AVX\n");
        }
    }
#endif /* X86 */
}

uint
proc_get_vendor(void)
{
    return vendor;
}

DR_API
int
proc_set_vendor(uint new_vendor)
{
    if (new_vendor == VENDOR_INTEL ||
        new_vendor == VENDOR_AMD) {
        uint old_vendor = vendor;
        SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
        vendor = new_vendor;
        SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
        return old_vendor;
    } else {
        CLIENT_ASSERT(false, "invalid vendor");
        return -1;
    }
}

uint
proc_get_family(void)
{
    return family;
}

uint proc_get_type(void)
{
    return type;
}

/* FIXME: Add MODEL_ constants to proc.h?? */
uint proc_get_model(void)
{
    return model;
}

uint proc_get_stepping(void)
{
    return stepping;
}

bool
proc_has_feature(feature_bit_t f)
{
    uint bit, val = 0;

    /* Cast to int to avoid tautological comparison if feature_bit_t enum is
     * unsigned. */
    if ((int)f >= 0 && f <= 31) {
        val = features.flags_edx;
    } else if (f >= 32 && f <= 63) {
        val = features.flags_ecx;
    } else if (f >= 64 && f <= 95) {
        val = features.ext_flags_edx;
    } else if (f >= 96 && f <= 127) {
        val = features.ext_flags_ecx;
    } else {
        CLIENT_ASSERT(false, "proc_has_feature: invalid parameter");
    }

    bit = f % 32;
    return TEST((1 << bit), val);
}

features_t *
proc_get_all_feature_bits(void)
{
    return &features;
}

char *
proc_get_brand_string(void)
{
    return (char *)brand_string;
}

cache_size_t
proc_get_L1_icache_size(void)
{
    return L1_icache_size;
}

cache_size_t
proc_get_L1_dcache_size(void)
{
    return L1_dcache_size;
}

cache_size_t
proc_get_L2_cache_size(void)
{
    return L2_cache_size;
}

const char *
proc_get_cache_size_str(cache_size_t size)
{
    static const char *strings[] = {
        "8 KB",
        "16 KB",
        "32 KB",
        "64 KB",
        "128 KB",
        "256 KB",
        "512 KB",
        "1 MB",
        "2 MB",
        "unknown"
    };
    CLIENT_ASSERT(size <= CACHE_SIZE_UNKNOWN, "proc_get_cache_size_str: invalid size");
    return strings[size];
}

size_t
proc_get_cache_line_size(void)
{
    return cache_line_size;
}

/* check to see if addr is cache aligned */
bool
proc_is_cache_aligned(void *addr)
{
    return (((ptr_uint_t)addr & mask) == 0x0);
}

/* Given an address or number of bytes sz, return a number >= sz that is divisible
   by the cache line size. */
ptr_uint_t
proc_bump_to_end_of_cache_line(ptr_uint_t sz)
{
    if ((sz & mask) == 0x0)
        return sz;      /* sz already a multiple of the line size */

    return ((sz + cache_line_size) & ~mask);
}

/* yes same result as PAGE_START...FIXME: get rid of one of them? */
void *
proc_get_containing_page(void *addr)
{
    return (void *) (((ptr_uint_t)addr) & ~(PAGE_SIZE-1));
}

/* No synchronization routines necessary.  The Pentium hardware
 * guarantees that the i and d caches are consistent. */
void
machine_cache_sync(void *pc_start, void *pc_end, bool flush_icache)
{
    /* empty */
}

DR_API
/**
 * Returns the size in bytes needed for a buffer for saving the floating point state.
 */
size_t
proc_fpstate_save_size(void)
{
    CLIENT_ASSERT(opnd_size_in_bytes(OPSZ_512) == 512 &&
                  opnd_size_in_bytes(OPSZ_108) == 108,
                  "internal sizing discrepancy");
    return (proc_has_feature(FEATURE_FXSR) ?  512 : 108);
}

DR_API
/* Saves the floating point state into the 16-byte-aligned buffer buf,
 * which must be 512 bytes for processors with the FXSR feature, and
 * 108 bytes for those without (where this routine does not support
 * 16-bit operand sizing).
 *
 * DynamoRIO does NOT save the application's floating-point, MMX, or SSE state
 * on context switches!  Thus if a client performs any floating-point operations
 * in its main routines called by DynamoRIO, the client must save and restore
 * the floating-point/MMX/SSE state.
 * If the client needs to do so inside the code cache the client should implement
 * that itself.
 * return number of bytes written
 *
 * XXX: we do not translate the last fp pc (xref i#698).  If a client ever needs that
 * we can try to support it in the future.
 */
size_t
proc_save_fpstate(byte *buf)
{
    /* MUST be 16-byte aligned */
    CLIENT_ASSERT((((ptr_uint_t)buf) & 0x0000000f) == 0,
                  "proc_save_fpstate: buf must be 16-byte aligned");
#ifdef X86
    if (proc_has_feature(FEATURE_FXSR)) {
        /* Not using inline asm for identical cross-platform code
         * here.  An extra function call won't hurt here.
         */
# ifdef X64
        if (X64_MODE_DC(get_thread_private_dcontext()))
            dr_fxsave(buf);
        else
            dr_fxsave32(buf);
# else
        dr_fxsave(buf);
# endif
    } else {
# ifdef WINDOWS
        dr_fnsave(buf);
# else
        asm volatile("fnsave %0 ; fwait" : "=m" ((*buf)));
# endif
    }
    return proc_fpstate_save_size();
#elif defined(ARM)
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
    return 0;
#endif /* X86/ARM */
}

DR_API
/* Restores the floating point state from the 16-byte-aligned buffer
 * buf, which must be 512 bytes for processors with the FXSR feature,
 * and 108 bytes for those without (where this routine does not
 * support 16-bit operand sizing).
 */
void
proc_restore_fpstate(byte *buf)
{
    /* MUST be 16-byte aligned */
    CLIENT_ASSERT((((ptr_uint_t)buf) & 0x0000000f) == 0,
                  "proc_restore_fpstate: buf must be 16-byte aligned");
#ifdef X86
    if (proc_has_feature(FEATURE_FXSR)) {
        /* Not using inline asm for identical cross-platform code
         * here.  An extra function call won't hurt here.
         */
# ifdef X64
        if (X64_MODE_DC(get_thread_private_dcontext()))
            dr_fxrstor(buf);
        else
            dr_fxrstor32(buf);
# else
        dr_fxrstor(buf);
# endif
    } else {
# ifdef WINDOWS
        dr_frstor(buf);
# else
        asm volatile("frstor %0" : : "m" ((*buf)));
# endif
    }
#elif defined(ARM)
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
#endif /* X86/ARM */
}

/* XXX: we do not translate the last fp pc (xref i#698).  If a client ever needs that
 * we can try to support it in the future.
 */
void
dr_insert_save_fpstate(void *drcontext, instrlist_t *ilist, instr_t *where,
                       opnd_t buf)
{
#ifdef X86
    dcontext_t *dcontext = (dcontext_t *) drcontext;
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
#elif defined(ARM)
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
#endif /* X86/ARM */
}

void
dr_insert_restore_fpstate(void *drcontext, instrlist_t *ilist, instr_t *where,
                          opnd_t buf)
{
#ifdef X86
    dcontext_t *dcontext = (dcontext_t *) drcontext;
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
#elif defined(ARM)
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
#endif /* X86/ARM */
}

bool
proc_avx_enabled(void)
{
    return avx_enabled;
}
