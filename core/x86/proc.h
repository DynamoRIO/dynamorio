/* **********************************************************
 * Copyright (c) 2011 Google, Inc.  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
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
 * proc.h - processor implementation specific interfaces
 */

#ifndef _PROC_H_
#define _PROC_H_ 1

/* DR_API EXPORT TOFILE dr_proc.h */
/* DR_API EXPORT BEGIN */
/****************************************************************************
 * PROCESSOR-SPECIFIC UTILITY ROUTINES AND CONSTANTS
 */
/**
 * @file dr_proc.h
 * @brief Utility routines for identifying features of the processor.
 */

/* page size is 4K on all DR-supported platforms */
#define PAGE_SIZE (4*1024) /**< Size of a page of memory. */

/**< Convenience macro to align to the start of a page of memory. */
#define PAGE_START(x) (((ptr_uint_t)(x)) & ~((PAGE_SIZE)-1))

/** Constants returned by proc_get_vendor(). */
enum {
    VENDOR_INTEL,   /**< proc_get_vendor() processor identification: Intel */
    VENDOR_AMD,     /**< proc_get_vendor() processor identification: AMD */
    VENDOR_UNKNOWN, /**< proc_get_vendor() processor identification: unknown */
};

/* Family and Model
 *   Intel 486                 Family 4
 *   Intel Pentium             Family 5
 *   Intel Pentium Pro         Family 6, Model 0 and 1 
 *   Intel Pentium 2           Family 6, Model 3, 5, and 6
 *   Intel Celeron             Family 6, Model 5 and 6
 *   Intel Pentium 3           Family 6, Model 7, 8, 10, 11
 *   Intel Pentium 4           Family 15, Extended 0
 *   Intel Itanium             Family 7
 *   Intel Itanium 2           Family 15, Extended 1 and 2
 *   Intel Pentium M           Family 6, Model 9 and 13
 *   Intel Core                Family 6, Model 14
 *   Intel Core 2              Family 6, Model 15
 *   Intel Core i7 (Nehalem)   Family 6, Models 26, 30, 31, 37, 44, 47
 */
/* DR_API EXPORT END */
#ifdef IA32_ON_IA64 /* don't export IA64 stuff! */
/* IA-64 */
#define FAMILY_IA64         7
#endif
/* DR_API EXPORT BEGIN */
/* Remember that we add extended family to family as Intel suggests */
#define FAMILY_LLANO        18 /**< proc_get_family() processor family: AMD Llano */
#define FAMILY_ITANIUM_2_DC 17 /**< proc_get_family() processor family: Itanium 2 DC */
#define FAMILY_K8_MOBILE    17 /**< proc_get_family() processor family: AMD K8 Mobile */
#define FAMILY_ITANIUM_2    16 /**< proc_get_family() processor family: Itanium 2 */
#define FAMILY_K8L          16 /**< proc_get_family() processor family: AMD K8L */
#define FAMILY_K8           15 /**< proc_get_family() processor family: AMD K8 */
#define FAMILY_PENTIUM_4    15 /**< proc_get_family() processor family: Pentium 4 */
#define FAMILY_P4           15 /**< proc_get_family() processor family: P4 family */
#define FAMILY_ITANIUM       7 /**< proc_get_family() processor family: Itanium */
/* Pentium Pro, Pentium II, Pentium III, Athlon, Pentium M, Core, Core 2, Core i7 */
#define FAMILY_P6            6 /**< proc_get_family() processor family: P6 family */
#define FAMILY_CORE_I7       6 /**< proc_get_family() processor family: Core i7 */
#define FAMILY_CORE_2        6 /**< proc_get_family() processor family: Core 2 */
#define FAMILY_CORE          6 /**< proc_get_family() processor family: Core */
#define FAMILY_PENTIUM_M     6 /**< proc_get_family() processor family: Pentium M */
#define FAMILY_PENTIUM_3     6 /**< proc_get_family() processor family: Pentium 3 */
#define FAMILY_PENTIUM_2     6 /**< proc_get_family() processor family: Pentium 2 */
#define FAMILY_PENTIUM_PRO   6 /**< proc_get_family() processor family: Pentium Pro */
#define FAMILY_ATHLON        6 /**< proc_get_family() processor family: Athlon */
#define FAMILY_K7            6 /**< proc_get_family() processor family: AMD K7 */
/* Pentium (586) */
#define FAMILY_P5            5 /**< proc_get_family() processor family: P5 family */
#define FAMILY_PENTIUM       5 /**< proc_get_family() processor family: Pentium */
#define FAMILY_K6            5 /**< proc_get_family() processor family: K6 */
#define FAMILY_K5            5 /**< proc_get_family() processor family: K5 */
/* 486 */
#define FAMILY_486           4 /**< proc_get_family() processor family: 486 */

/* We do not enumerate all models; just relevant ones needed to distinguish
 * major processors in the same family.
 */
#define MODEL_I7_WESTMERE_EX  47 /**< proc_get_model(): Core i7 Westmere Ex */
#define MODEL_I7_WESTMERE     44 /**< proc_get_model(): Core i7 Westmere */
#define MODEL_I7_CLARKDALE    37 /**< proc_get_model(): Core i7 Clarkdale/Arrandale */
#define MODEL_I7_HAVENDALE    31 /**< proc_get_model(): Core i7 Havendale/Auburndale */
#define MODEL_I7_CLARKSFIELD  30 /**< proc_get_model(): Core i7 Clarksfield/Lynnfield */
#define MODEL_ATOM            28 /**< proc_get_model(): Atom */
#define MODEL_I7_GAINESTOWN   26 /**< proc_get_model(): Core i7 Gainestown */
#define MODEL_CORE_PENRYN     23 /**< proc_get_model(): Core 2 Penryn */
#define MODEL_CORE_2          15 /**< proc_get_model(): Core 2 Merom/Conroe */
#define MODEL_CORE            14 /**< proc_get_model(): Core Yonah */
#define MODEL_PENTIUM_M       13 /**< proc_get_model(): Pentium M 2MB L2 */
#define MODEL_PENTIUM_M_1MB    9 /**< proc_get_model(): Pentium M 1MB L2 */

/**
 * Struct to hold all 4 32-bit feature values returned by cpuid.
 * Used by proc_get_all_feature_bits().
 */
typedef struct {
    uint flags_edx;             /**< feature flags stored in edx */
    uint flags_ecx;             /**< feature flags stored in ecx */
    uint ext_flags_edx;         /**< extended feature flags stored in edx */
    uint ext_flags_ecx;         /**< extended feature flags stored in ecx */
} features_t;

/**
 * Feature bits returned by cpuid.  Pass one of these values to proc_has_feature() to
 * determine whether the underlying processor has the feature.
 */
typedef enum {
    /* features returned in edx */
    FEATURE_FPU =       0,              /**< Floating-point unit on chip */
    FEATURE_VME =       1,              /**< Virtual Mode Extension */
    FEATURE_DE =        2,              /**< Debugging Extension */
    FEATURE_PSE =       3,              /**< Page Size Extension */
    FEATURE_TSC =       4,              /**< Time-Stamp Counter */
    FEATURE_MSR =       5,              /**< Model Specific Registers */
    FEATURE_PAE =       6,              /**< Physical Address Extension */
    FEATURE_MCE =       7,              /**< Machine Check Exception */
    FEATURE_CX8 =       8,              /**< CMPXCHG8 Instruction Supported */
    FEATURE_APIC =      9,              /**< On-chip APIC Hardware Supported */
    FEATURE_SEP =       11,             /**< Fast System Call */
    FEATURE_MTRR =      12,             /**< Memory Type Range Registers */
    FEATURE_PGE =       13,             /**< Page Global Enable */
    FEATURE_MCA =       14,             /**< Machine Check Architecture */
    FEATURE_CMOV =      15,             /**< Conditional Move Instruction */
    FEATURE_PAT =       16,             /**< Page Attribute Table */
    FEATURE_PSE_36 =    17,             /**< 36-bit Page Size Extension */
    FEATURE_PSN =       18,             /**< Processor serial # present & enabled */
    FEATURE_CLFSH =     19,             /**< CLFLUSH Instruction supported */
    FEATURE_DS =        21,             /**< Debug Store */
    FEATURE_ACPI =      22,             /**< Thermal monitor & SCC supported */
    FEATURE_MMX =       23,             /**< MMX technology supported */
    FEATURE_FXSR =      24,             /**< Fast FP save and restore */
    FEATURE_SSE =       25,             /**< SSE Extensions supported */
    FEATURE_SSE2 =      26,             /**< SSE2 Extensions supported */
    FEATURE_SS =        27,             /**< Self-snoop */
    FEATURE_HTT =       28,             /**< Hyper-threading Technology */
    FEATURE_TM =        29,             /**< Thermal Monitor supported */
    FEATURE_IA64 =      30,             /**< IA64 Capabilities */
    FEATURE_PBE =       31,             /**< Pending Break Enable */
    /* features returned in ecx */
    FEATURE_SSE3 =      0 + 32,         /**< SSE3 Extensions supported */
    FEATURE_PCLMULQDQ = 1 + 32,         /**< OP_pclmulqdq supported */
    FEATURE_MONITOR =   3 + 32,         /**< MONITOR/MWAIT instructions supported */
    FEATURE_DS_CPL =    4 + 32,         /**< CPL Qualified Debug Store */
    FEATURE_VMX =       5 + 32,         /**< Virtual Machine Extensions */
    FEATURE_EST =       7 + 32,         /**< Enhanced Speedstep Technology */
    FEATURE_TM2 =       8 + 32,         /**< Thermal Monitor 2 */
    FEATURE_SSSE3 =     9 + 32,         /**< SSSE3 Extensions supported */
    FEATURE_CID =       10 + 32,        /**< Context ID */ 
    FEATURE_FMA =       12 + 32,        /**< FMA instructions supported */
    FEATURE_CX16 =      13 + 32,        /**< CMPXCHG16B instruction supported */
    FEATURE_xPTR =      14 + 32,        /**< Send Task Priority Messages */
    FEATURE_SSE41 =     19 + 32,        /**< SSE4.1 Extensions supported */
    FEATURE_SSE42 =     20 + 32,        /**< SSE4.2 Extensions supported */
    FEATURE_MOVBE =     22 + 32,        /**< OP_movbe supported */
    FEATURE_POPCNT =    23 + 32,        /**< OP_popcnt supported */
    FEATURE_AES =       25 + 32,        /**< AES instructions supported */
    FEATURE_XSAVE =     26 + 32,        /**< OP_xsave supported */
    FEATURE_OSXSAVE =   27 + 32,        /**< OP_xgetbv supported in user mode */
    FEATURE_AVX =       28 + 32,        /**< AVX instructions supported */
    /* extended features returned in edx */
    FEATURE_SYSCALL =   11 + 64,        /**< SYSCALL/SYSRET instructions supported */
    FEATURE_XD_Bit =    20 + 64,        /**< Execution Disable bit */
    FEATURE_EM64T =     29 + 64,        /**< Extended Memory 64 Technology */
    /* extended features returned in ecx */
    FEATURE_LAHF =      0 + 96          /**< LAHF/SAHF available in 64-bit mode */
} feature_bit_t;

/**
 * L1 and L2 cache sizes, used by proc_get_L1_icache_size(),
 * proc_get_L1_dcache_size(), proc_get_L2_cache_size(), and
 * proc_get_cache_size_str().
 */
#ifdef AVOID_API_EXPORT
/* Make sure to keep this in sync with proc_get_cache_size_str() in proc.c */
#endif
typedef enum {
    CACHE_SIZE_8_KB,    /**< L1 or L2 cache size of 8 KB. */
    CACHE_SIZE_16_KB,   /**< L1 or L2 cache size of 16 KB. */
    CACHE_SIZE_32_KB,   /**< L1 or L2 cache size of 32 KB. */
    CACHE_SIZE_64_KB,   /**< L1 or L2 cache size of 64 KB. */
    CACHE_SIZE_128_KB,  /**< L1 or L2 cache size of 128 KB. */
    CACHE_SIZE_256_KB,  /**< L1 or L2 cache size of 256 KB. */
    CACHE_SIZE_512_KB,  /**< L1 or L2 cache size of 512 KB. */
    CACHE_SIZE_1_MB,    /**< L1 or L2 cache size of 1 MB. */
    CACHE_SIZE_2_MB,    /**< L1 or L2 cache size of 2 MB. */
    CACHE_SIZE_UNKNOWN  /**< Unknown L1 or L2 cache size. */
} cache_size_t;

/* DR_API EXPORT END */

/* exported for efficient access */
extern size_t cache_line_size;

#define CACHE_LINE_SIZE() cache_line_size

/* xcr0 and xstate_bv feature bits */
#define XCR0_AVX 4
#define XCR0_SSE 2
#define XCR0_FP  1

void proc_init(void);

DR_API
/** Returns the cache line size in bytes of the processor. */
size_t
proc_get_cache_line_size(void);

DR_API
/** Returns true only if \p addr is cache-line-aligned. */
bool 
proc_is_cache_aligned(void *addr);

void
machine_cache_sync(void *pc_start, void *pc_end, bool flush_icache);

DR_API
/** Returns n >= \p sz such that n is a multiple of the cache line size. */
ptr_uint_t
proc_bump_to_end_of_cache_line(ptr_uint_t sz);

DR_API
/** Returns n <= \p addr such that n is a multiple of the page size. */
void *
proc_get_containing_page(void *addr);

DR_API
/** Returns one of the VENDOR_ constants. */
uint
proc_get_vendor(void);

DR_API
/**
 * Sets the vendor to the given VENDOR_ constant.
 * This function is supplied to support decoding or encoding with respect to
 * other than the current processor being executed on.  The change in vendor
 * will be seen by the decoder and encoder, as well as the rest of the
 * system.
 * \return the prior vendor, or -1 on an invalid argument.
 */
int
proc_set_vendor(uint new_vendor);

DR_API
/** 
 * Returns the processor family as given by the cpuid instruction,
 * adjusted by the extended family as described in the Intel documentation.
 * The FAMILY_ constants identify important family values.
 */
uint
proc_get_family(void);

DR_API
/** Returns the processor type as given by the cpuid instruction. */
uint
proc_get_type(void);

DR_API
/** 
 * Returns the processor model as given by the cpuid instruction,
 * adjusted by the extended model as described in the Intel documentation.
 * The MODEL_ constants identify important model values.
 */
uint
proc_get_model(void);

DR_API
/** Returns the processor stepping ID. */
uint
proc_get_stepping(void);

DR_API
/** Tests if processor has selected feature. */
bool
proc_has_feature(feature_bit_t feature);

DR_API
/**
 * Returns all 4 32-bit feature values.  Use proc_has_feature to test
 * for specific features.
 */
features_t *
proc_get_all_feature_bits(void);

DR_API
/** Returns the processor brand string as given by the cpuid instruction. */
char*
proc_get_brand_string(void);

DR_API
/** Returns the size of the L1 instruction cache. */
cache_size_t
proc_get_L1_icache_size(void);

DR_API
/** Returns the size of the L1 data cache. */
cache_size_t
proc_get_L1_dcache_size(void);

DR_API
/** Returns the size of the L2 cache. */
cache_size_t
proc_get_L2_cache_size(void);

DR_API
/** Converts a cache_size_t type to a string. */
const char*
proc_get_cache_size_str(cache_size_t size);

DR_API
/**
 * Returns the size in bytes needed for a buffer for saving the floating point state.
 */
size_t
proc_fpstate_save_size(void);

DR_API
/**
 * Saves the floating point state into the 16-byte-aligned buffer \p buf,
 * which must be 512 bytes for processors with the FXSR feature, and
 * 108 bytes for those without (where this routine does not support
 * 16-bit operand sizing).  \note proc_fpstate_save_size() can be used
 * to determine the particular size needed.
 * 
 * DR does NOT save the application's floating-point, MMX, or SSE state
 * on context switches!  Thus if a client performs any floating-point 
 * operations in its main routines called by DR, the client must save 
 * and restore the floating-point/MMX/SSE state.
 * If the client needs to do so inside the code cache the client should implement
 * that itself.
 * Returns number of bytes written.
 */
size_t
proc_save_fpstate(byte *buf);

DR_API
/**
 * Restores the floating point state from the 16-byte-aligned buffer
 * \p buf, which must be 512 bytes for processors with the FXSR feature,
 * and 108 bytes for those without (where this routine does not
 * support 16-bit operand sizing).  \note proc_fpstate_save_size() can
 * be used to determine the particular size needed.
 */
void 
proc_restore_fpstate(byte *buf);

#endif /* _PROC_H_ */
