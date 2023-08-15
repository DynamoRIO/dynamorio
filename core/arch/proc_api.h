/* **********************************************************
 * Copyright (c) 2010-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2002-2010 VMware, Inc.  All rights reserved.
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

#ifndef _DR_PROC_H_
#define _DR_PROC_H_ 1

/****************************************************************************
 * PROCESSOR-SPECIFIC UTILITY ROUTINES AND CONSTANTS
 */
/**
 * @file dr_proc.h
 * @brief Utility routines for identifying features of the processor.
 */

/**
 * The maximum possible required size of floating point state buffer for
 * processors with different features (i.e., the processors with the FXSR
 * feature on x86, or the processors with the VFPv3 feature on ARM).
 * \note The actual required buffer size may vary depending on the processor
 * feature.  \note proc_fpstate_save_size() can be used to determine the
 * particular size needed.
 */
#ifdef X86
#    define DR_FPSTATE_BUF_SIZE 512
#elif defined(RISCV64)
/* FIXME i#3544: Not implemented */
#    define DR_FPSTATE_BUF_SIZE 1
#elif defined(ARM) || defined(AARCH64)
/* On ARM/AArch64 proc_save_fpstate saves nothing, so use the smallest
 * legal size for an array.
 */
#    define DR_FPSTATE_BUF_SIZE 1
#endif

/** The alignment requirements of floating point state buffer. */
#if defined(X86) || defined(AARCH64)
#    define DR_FPSTATE_ALIGN 16
#elif defined(RISCV64)
#    define DR_FPSTATE_ALIGN 1
#elif defined(ARM)
#    define DR_FPSTATE_ALIGN 1
#endif
/** Constants returned by proc_get_vendor(). */
enum {
    VENDOR_INTEL,   /**< proc_get_vendor() processor identification: Intel */
    VENDOR_AMD,     /**< proc_get_vendor() processor identification: AMD */
    VENDOR_ARM,     /**< proc_get_vendor() processor identification: ARM */
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
 *   Intel Nehalem             Family 6, Models 26 (0x1a), 30 (0x1e), 31 (0x1f)
 *   Intel SandyBridge         Family 6, Models 37 (0x25), 42 (0x2a), 44 (0x2c),
 *                                              45 (0x2d), 47 (0x2f)
 *   Intel IvyBridge           Family 6, Model 58 (0x3a)
 *   Intel Atom                Family 6, Model 28 (0x1c), 38 (0x26), 54 (0x36)
 */
/* Remember that we add extended family to family as Intel suggests */
#define FAMILY_LLANO 18        /**< proc_get_family() processor family: AMD Llano */
#define FAMILY_ITANIUM_2_DC 17 /**< proc_get_family() processor family: Itanium 2 DC */
#define FAMILY_K8_MOBILE 17    /**< proc_get_family() processor family: AMD K8 Mobile */
#define FAMILY_ITANIUM_2 16    /**< proc_get_family() processor family: Itanium 2 */
#define FAMILY_K8L 16          /**< proc_get_family() processor family: AMD K8L */
#define FAMILY_K8 15           /**< proc_get_family() processor family: AMD K8 */
#define FAMILY_PENTIUM_4 15    /**< proc_get_family() processor family: Pentium 4 */
#define FAMILY_P4 15           /**< proc_get_family() processor family: P4 family */
#define FAMILY_ITANIUM 7       /**< proc_get_family() processor family: Itanium */
/* Pentium Pro, Pentium II, Pentium III, Athlon, Pentium M, Core, Core 2+ */
#define FAMILY_P6 6          /**< proc_get_family() processor family: P6 family */
#define FAMILY_IVYBRIDGE 6   /**< proc_get_family() processor family: IvyBridge */
#define FAMILY_SANDYBRIDGE 6 /**< proc_get_family() processor family: SandyBridge */
#define FAMILY_NEHALEM 6     /**< proc_get_family() processor family: Nehalem */
#define FAMILY_CORE_I7 6     /**< proc_get_family() processor family: Core i7 */
#define FAMILY_CORE_2 6      /**< proc_get_family() processor family: Core 2 */
#define FAMILY_CORE 6        /**< proc_get_family() processor family: Core */
#define FAMILY_PENTIUM_M 6   /**< proc_get_family() processor family: Pentium M */
#define FAMILY_PENTIUM_3 6   /**< proc_get_family() processor family: Pentium 3 */
#define FAMILY_PENTIUM_2 6   /**< proc_get_family() processor family: Pentium 2 */
#define FAMILY_PENTIUM_PRO 6 /**< proc_get_family() processor family: Pentium Pro */
#define FAMILY_ATHLON 6      /**< proc_get_family() processor family: Athlon */
#define FAMILY_K7 6          /**< proc_get_family() processor family: AMD K7 */
/* Pentium (586) */
#define FAMILY_P5 5      /**< proc_get_family() processor family: P5 family */
#define FAMILY_PENTIUM 5 /**< proc_get_family() processor family: Pentium */
#define FAMILY_K6 5      /**< proc_get_family() processor family: K6 */
#define FAMILY_K5 5      /**< proc_get_family() processor family: K5 */
/* 486 */
#define FAMILY_486 4 /**< proc_get_family() processor family: 486 */

/* We do not enumerate all models; just relevant ones needed to distinguish
 * major processors in the same family.
 */
#define MODEL_HASWELL 60        /**< proc_get_model(): Haswell */
#define MODEL_IVYBRIDGE 58      /**< proc_get_model(): Ivybridge */
#define MODEL_I7_WESTMERE_EX 47 /**< proc_get_model(): Sandybridge Westmere Ex */
#define MODEL_SANDYBRIDGE_E 45  /**< proc_get_model(): Sandybridge-E, -EN, -EP */
#define MODEL_I7_WESTMERE 44    /**< proc_get_model(): Westmere */
#define MODEL_SANDYBRIDGE 42    /**< proc_get_model(): Sandybridge */
#define MODEL_I7_CLARKDALE 37   /**< proc_get_model(): Westmere Clarkdale/Arrandale */
#define MODEL_I7_HAVENDALE 31   /**< proc_get_model(): Core i7 Havendale/Auburndale */
#define MODEL_I7_CLARKSFIELD 30 /**< proc_get_model(): Core i7 Clarksfield/Lynnfield */
#define MODEL_ATOM_CEDARVIEW 54 /**< proc_get_model(): Atom Cedarview */
#define MODEL_ATOM_LINCROFT 38  /**< proc_get_model(): Atom Lincroft */
#define MODEL_ATOM 28           /**< proc_get_model(): Atom */
#define MODEL_I7_GAINESTOWN 26  /**< proc_get_model(): Core i7 Gainestown (Nehalem) */
#define MODEL_CORE_PENRYN 23    /**< proc_get_model(): Core 2 Penryn */
#define MODEL_CORE_2 15         /**< proc_get_model(): Core 2 Merom/Conroe */
#define MODEL_CORE_MEROM 15     /**< proc_get_model(): Core 2 Merom */
#define MODEL_CORE 14           /**< proc_get_model(): Core Yonah */
#define MODEL_PENTIUM_M 13      /**< proc_get_model(): Pentium M 2MB L2 */
#define MODEL_PENTIUM_M_1MB 9   /**< proc_get_model(): Pentium M 1MB L2 */

#ifdef X86
/**
 * For X86 this struct holds all 4 32-bit feature values returned by cpuid.
 * Used by proc_get_all_feature_bits().
 */
typedef struct {
    uint flags_edx;      /**< X86 feature flags stored in edx */
    uint flags_ecx;      /**< X86 feature flags stored in ecx */
    uint ext_flags_edx;  /**< X86 extended feature flags stored in edx */
    uint ext_flags_ecx;  /**< X86 extended feature flags stored in ecx */
    uint sext_flags_ebx; /**< structured X86 extended feature flags stored in ebx */
} features_t;
#endif
/* We avoid using #elif here because otherwise doxygen will be unable to
 * document both defines, for X86 and for AARCHXX. See also i#5496.
 */
#ifdef AARCHXX
/**
 * For AArch64 this struct holds features registers' values read by MRS instructions.
 * Used by proc_get_all_feature_bits().
 */
typedef struct {
    uint64 flags_aa64isar0; /**< AArch64 feature flags stored in ID_AA64ISAR0_EL1 */
    uint64 flags_aa64isar1; /**< AArch64 feature flags stored in ID_AA64ISAR1_EL1 */
    uint64 flags_aa64pfr0;  /**< AArch64 feature flags stored in ID_AA64PFR0_EL1 */
    uint64 flags_aa64mmfr1; /**< AArch64 feature flags stored in ID_AA64MMFR1_EL1 */
    uint64 flags_aa64dfr0;  /**< AArch64 feature flags stored in ID_AA64DFR0_EL1 */
    uint64 flags_aa64zfr0;  /**< AArch64 feature flags stored in ID_AA64ZFR0_EL1 */
    uint64 flags_aa64pfr1;  /**< AArch64 feature flags stored in ID_AA64PFR1_EL1 */
} features_t;
typedef enum {
    AA64ISAR0 = 0,
    AA64ISAR1 = 1,
    AA64PFR0 = 2,
    AA64MMFR1 = 3,
    AA64DFR0 = 4,
    AA64ZFR0 = 5,
    AA64PFR1 = 6,
} feature_reg_idx_t;
#endif
#ifdef RISCV64
/* FIXME i#3544: Not implemented */
/**
 * For RISC-V64 there are no features readable from userspace. Hence only a
 * dummy flag is there. May be replaced by actual feature flags in the future.
 * Used by proc_get_all_feature_bits().
 */
typedef struct {
    uint64 dummy; /**< Dummy member to keep size non-0. */
} features_t;
#endif

#ifdef X86
/**
 * Feature bits returned by cpuid for X86 and mrs for AArch64. Pass one of
 * these values to proc_has_feature() to determine whether the underlying
 * processor has the feature.
 */
typedef enum {
    /* features returned in edx */
    FEATURE_FPU = 0,     /**< Floating-point unit on chip (X86) */
    FEATURE_VME = 1,     /**< Virtual Mode Extension (X86) */
    FEATURE_DE = 2,      /**< Debugging Extension (X86) */
    FEATURE_PSE = 3,     /**< Page Size Extension (X86) */
    FEATURE_TSC = 4,     /**< Time-Stamp Counter (X86) */
    FEATURE_MSR = 5,     /**< Model Specific Registers (X86) */
    FEATURE_PAE = 6,     /**< Physical Address Extension (X86) */
    FEATURE_MCE = 7,     /**< Machine Check Exception (X86) */
    FEATURE_CX8 = 8,     /**< #OP_cmpxchg8b supported (X86) */
    FEATURE_APIC = 9,    /**< On-chip APIC Hardware supported (X86) */
    FEATURE_SEP = 11,    /**< Fast System Call (X86) */
    FEATURE_MTRR = 12,   /**< Memory Type Range Registers (X86) */
    FEATURE_PGE = 13,    /**< Page Global Enable (X86) */
    FEATURE_MCA = 14,    /**< Machine Check Architecture (X86) */
    FEATURE_CMOV = 15,   /**< Conditional Move Instruction (X86) */
    FEATURE_PAT = 16,    /**< Page Attribute Table (X86) */
    FEATURE_PSE_36 = 17, /**< 36-bit Page Size Extension (X86) */
    FEATURE_PSN = 18,    /**< Processor serial # present & enabled (X86) */
    FEATURE_CLFSH = 19,  /**< #OP_clflush supported (X86) */
    FEATURE_DS = 21,     /**< Debug Store (X86) */
    FEATURE_ACPI = 22,   /**< Thermal monitor & SCC supported (X86) */
    FEATURE_MMX = 23,    /**< MMX technology supported (X86) */
    FEATURE_FXSR = 24,   /**< Fast FP save and restore (X86) */
    FEATURE_SSE = 25,    /**< SSE Extensions supported (X86) */
    FEATURE_SSE2 = 26,   /**< SSE2 Extensions supported (X86) */
    FEATURE_SS = 27,     /**< Self-snoop (X86) */
    FEATURE_HTT = 28,    /**< Hyper-threading Technology (X86) */
    FEATURE_TM = 29,     /**< Thermal Monitor supported (X86) */
    FEATURE_IA64 = 30,   /**< IA64 Capabilities (X86) */
    FEATURE_PBE = 31,    /**< Pending Break Enable (X86) */
    /* features returned in ecx */
    FEATURE_SSE3 = 0 + 32,      /**< SSE3 Extensions supported (X86) */
    FEATURE_PCLMULQDQ = 1 + 32, /**< #OP_pclmulqdq supported (X86) */
    FEATURE_DTES64 = 2 + 32,    /**< 64-bit debug store supported (X86) */
    FEATURE_MONITOR = 3 + 32,   /**< #OP_monitor/#OP_mwait supported (X86) */
    FEATURE_DS_CPL = 4 + 32,    /**< CPL Qualified Debug Store (X86) */
    FEATURE_VMX = 5 + 32,       /**< Virtual Machine Extensions (X86) */
    FEATURE_SMX = 6 + 32,       /**< Safer Mode Extensions (X86) */
    FEATURE_EST = 7 + 32,       /**< Enhanced Speedstep Technology (X86) */
    FEATURE_TM2 = 8 + 32,       /**< Thermal Monitor 2 (X86) */
    FEATURE_SSSE3 = 9 + 32,     /**< SSSE3 Extensions supported (X86) */
    FEATURE_CID = 10 + 32,      /**< Context ID (X86) */
    FEATURE_FMA = 12 + 32,      /**< FMA instructions supported (X86) */
    FEATURE_CX16 = 13 + 32,     /**< #OP_cmpxchg16b supported (X86) */
    FEATURE_xTPR = 14 + 32,     /**< Send Task Priority Messages (X86) */
    FEATURE_PDCM = 15 + 32,     /**< Perfmon and Debug Capability (X86) */
    FEATURE_PCID = 17 + 32,     /**< Process-context identifiers (X86) */
    FEATURE_DCA = 18 + 32,      /**< Prefetch from memory-mapped devices (X86) */
    FEATURE_SSE41 = 19 + 32,    /**< SSE4.1 Extensions supported (X86) */
    FEATURE_SSE42 = 20 + 32,    /**< SSE4.2 Extensions supported (X86) */
    FEATURE_x2APIC = 21 + 32,   /**< x2APIC supported (X86) */
    FEATURE_MOVBE = 22 + 32,    /**< #OP_movbe supported (X86) */
    FEATURE_POPCNT = 23 + 32,   /**< #OP_popcnt supported (X86) */
    FEATURE_AES = 25 + 32,      /**< AES instructions supported (X86) */
    FEATURE_XSAVE = 26 + 32,    /**< OP_xsave* supported (X86) */
    FEATURE_OSXSAVE = 27 + 32,  /**< #OP_xgetbv supported in user mode (X86) */
    FEATURE_AVX = 28 + 32,      /**< AVX instructions supported (X86) */
    FEATURE_F16C = 29 + 32,     /**< 16-bit floating-point conversion supported (X86) */
    FEATURE_RDRAND = 30 + 32,   /**< #OP_rdrand supported (X86) */
    /* extended features returned in edx */
    FEATURE_SYSCALL = 11 + 64,   /**< #OP_syscall/#OP_sysret supported (X86) */
    FEATURE_XD_Bit = 20 + 64,    /**< Execution Disable bit (X86) */
    FEATURE_MMX_EXT = 22 + 64,   /**< AMD MMX Extensions (X86) */
    FEATURE_PDPE1GB = 26 + 64,   /**< Gigabyte pages (X86) */
    FEATURE_RDTSCP = 27 + 64,    /**< #OP_rdtscp supported (X86) */
    FEATURE_EM64T = 29 + 64,     /**< Extended Memory 64 Technology (X86) */
    FEATURE_3DNOW_EXT = 30 + 64, /**< AMD 3DNow! Extensions (X86) */
    FEATURE_3DNOW = 31 + 64,     /**< AMD 3DNow! instructions supported (X86) */
    /* extended features returned in ecx */
    FEATURE_LAHF = 0 + 96,    /**< #OP_lahf/#OP_sahf available in 64-bit mode (X86) */
    FEATURE_SVM = 2 + 96,     /**< AMD Secure Virtual Machine (X86) */
    FEATURE_LZCNT = 5 + 96,   /**< #OP_lzcnt supported (X86) */
    FEATURE_SSE4A = 6 + 96,   /**< AMD SSE4A Extensions supported (X86) */
    FEATURE_PRFCHW = 8 + 96,  /**< #OP_prefetchw supported (X86) */
    FEATURE_XOP = 11 + 96,    /**< AMD XOP supported (X86) */
    FEATURE_SKINIT = 12 + 96, /**< AMD #OP_skinit/#OP_stgi supported (X86) */
    FEATURE_FMA4 = 16 + 96,   /**< AMD FMA4 supported (X86) */
    FEATURE_TBM = 21 + 96,    /**< AMD Trailing Bit Manipulation supported (X86) */
    /* structured extended features returned in ebx */
    FEATURE_FSGSBASE = 0 + 128,  /**< #OP_rdfsbase, etc. supported (X86) */
    FEATURE_BMI1 = 3 + 128,      /**< BMI1 instructions supported (X86) */
    FEATURE_HLE = 4 + 128,       /**< Hardware Lock Elision supported (X86) */
    FEATURE_AVX2 = 5 + 128,      /**< AVX2 instructions supported (X86) */
    FEATURE_BMI2 = 8 + 128,      /**< BMI2 instructions supported (X86) */
    FEATURE_ERMSB = 9 + 128,     /**< Enhanced rep movsb/stosb supported (X86) */
    FEATURE_INVPCID = 10 + 128,  /**< #OP_invpcid supported (X86) */
    FEATURE_RTM = 11 + 128,      /**< Restricted Transactional Memory supported (X86) */
    FEATURE_AVX512F = 16 + 128,  /**< AVX-512F instructions supported (X86) */
    FEATURE_AVX512BW = 30 + 128, /**< AVX-512BW instructions supported (X86) */
} feature_bit_t;
#endif
/* We avoid using #elif here because otherwise doxygen will be unable to
 * document both defines, for X86 and for AARCHXX. See also i#5496.
 */
#ifdef AARCHXX
/* On Arm, architectural features are defined and stored very differently from
 * X86. Specifically:
 * - There are multiple 64 bit system registers for features storage only, FREG.
 * - Each register is divided into nibbles representing a feature, NIBPOS.
 * - The value of a nibble represents a certain level of support for that feature, FVAL.
 * - The values can range from 0 to 15. In most cases 0 means a feature is not
 *   supported at all but in some cases 15 means a feature is not supported at
 *   all, NSFLAG.
 * The helper macro below packs feature data into 16 bits (ushort).
 */
#    define DEF_FEAT(FREG, NIBPOS, FVAL, NSFLAG) \
        ((ushort)((NSFLAG << 15) | (FREG << 8) | (NIBPOS << 4) | FVAL))
/**
 * Feature bits returned by cpuid for X86 and mrs for AArch64. Pass one of
 * these values to proc_has_feature() to determine whether the underlying
 * processor has the feature.
 */
typedef enum {
    /* Feature values returned in ID_AA64ISAR0_EL1 Instruction Set Attribute
     * Register 0
     */
    FEATURE_AESX = DEF_FEAT(AA64ISAR0, 1, 1, 0),     /**< AES<x> (AArch64) */
    FEATURE_PMULL = DEF_FEAT(AA64ISAR0, 1, 2, 0),    /**< PMULL/PMULL2 (AArch64) */
    FEATURE_SHA1 = DEF_FEAT(AA64ISAR0, 2, 1, 0),     /**< SHA1<x> (AArch64) */
    FEATURE_SHA256 = DEF_FEAT(AA64ISAR0, 3, 1, 0),   /**< SHA256<x> (AArch64) */
    FEATURE_SHA512 = DEF_FEAT(AA64ISAR0, 3, 2, 0),   /**< SHA512<x> (AArch64) */
    FEATURE_CRC32 = DEF_FEAT(AA64ISAR0, 4, 1, 0),    /**< CRC32<x> (AArch64) */
    FEATURE_LSE = DEF_FEAT(AA64ISAR0, 5, 2, 0),      /**< Atomic instructions (AArch64) */
    FEATURE_RDM = DEF_FEAT(AA64ISAR0, 7, 1, 0),      /**< SQRDMLAH,SQRDMLSH (AArch64) */
    FEATURE_SHA3 = DEF_FEAT(AA64ISAR0, 8, 1, 0),     /**< EOR3,RAX1,XAR,BCAX (AArch64) */
    FEATURE_SM3 = DEF_FEAT(AA64ISAR0, 9, 1, 0),      /**< SM3<x> (AArch64) */
    FEATURE_SM4 = DEF_FEAT(AA64ISAR0, 10, 1, 0),     /**< SM4E, SM4EKEY (AArch64) */
    FEATURE_DotProd = DEF_FEAT(AA64ISAR0, 11, 1, 0), /**< UDOT, SDOT (AArch64) */
    FEATURE_FHM = DEF_FEAT(AA64ISAR0, 12, 1, 0),     /**< FMLAL, FMLSL (AArch64) */
    FEATURE_FlagM = DEF_FEAT(AA64ISAR0, 13, 1, 0),   /**< CFINV,RMIF,SETF<x> (AArch64) */
    FEATURE_FlagM2 = DEF_FEAT(AA64ISAR0, 13, 2, 0),  /**< AXFLAG, XAFLAG (AArch64) */
    FEATURE_RNG = DEF_FEAT(AA64ISAR0, 15, 1, 0),     /**< RNDR, RNDRRS (AArch64) */
    FEATURE_DPB = DEF_FEAT(AA64ISAR1, 0, 1, 0),      /**< DC CVAP (AArch64) */
    FEATURE_DPB2 = DEF_FEAT(AA64ISAR1, 0, 2, 0),     /**< DC CVAP, DC CVADP (AArch64) */
    FEATURE_JSCVT = DEF_FEAT(AA64ISAR1, 3, 1, 0),    /**< FJCVTZS (AArch64) */
    FEATURE_FP16 = DEF_FEAT(AA64PFR0, 4, 1, 1),      /**< Half-precision FP (AArch64) */
    FEATURE_RAS = DEF_FEAT(AA64PFR0, 7, 1, 0),       /**< RAS extension (AArch64) */
    FEATURE_SVE = DEF_FEAT(AA64PFR0, 8, 1, 0),       /**< Scalable Vectors (AArch64) */
    FEATURE_LOR = DEF_FEAT(AA64MMFR1, 4, 1, 0),    /**< Limited order regions (AArch64) */
    FEATURE_SPE = DEF_FEAT(AA64DFR0, 8, 1, 0),     /**< Profiling extension (AArch64) */
    FEATURE_PAUTH = DEF_FEAT(AA64ISAR1, 2, 1, 0),  /**< PAuth extension (AArch64) */
    FEATURE_LRCPC = DEF_FEAT(AA64ISAR1, 5, 1, 0),  /**< LDAPR, LDAPRB, LDAPRH (AArch64) */
    FEATURE_LRCPC2 = DEF_FEAT(AA64ISAR1, 5, 2, 0), /**< LDAPUR*, STLUR* (AArch64) */
    FEATURE_BF16 = DEF_FEAT(AA64ZFR0, 5, 1, 0),    /**< SVE BFloat16 */
    FEATURE_I8MM = DEF_FEAT(AA64ZFR0, 11, 1, 0),   /**< SVE Int8 matrix multiplication */
    FEATURE_F64MM = DEF_FEAT(AA64ZFR0, 14, 1, 0),  /**< SVE FP64 matrix multiplication */
    FEATURE_SVE2 = DEF_FEAT(AA64ZFR0, 0, 1, 0),    /**< Scalable vectors 2 (AArch64) */
    FEATURE_SVEAES = DEF_FEAT(AA64ZFR0, 1, 1, 0),  /**< SVE2 + AES(AArch64) */
    FEATURE_SVESHA3 = DEF_FEAT(AA64ZFR0, 8, 1, 0), /**< SVE2 + SHA3(AArch64) */
    FEATURE_SVESM4 = DEF_FEAT(AA64ZFR0, 10, 1, 0), /**< SVE2 + SM4(AArch64) */
    FEATURE_SVEBitPerm = DEF_FEAT(AA64ZFR0, 4, 1, 0), /**< SVE2 + BitPerm(AArch64) */
    FEATURE_MTE = DEF_FEAT(AA64PFR1, 2, 1, 0),        /**< Memory Tagging Extension */
} feature_bit_t;
#endif
#ifdef RISCV64
/* FIXME i#3544: Not implemented */
/**
 * Feature bits passed to proc_has_feature() to determine whether the underlying
 * processor has the feature.
 */
typedef enum {
    FEATURE_DUMMY = 0, /**< Dummy, non-existent feature. */
} feature_bit_t;
#endif

/* Make sure to keep this in sync with proc_get_cache_size_str() in proc.c. */
/**
 * L1 and L2 cache sizes, used by proc_get_L1_icache_size(),
 * proc_get_L1_dcache_size(), proc_get_L2_cache_size(), and
 * proc_get_cache_size_str().
 */
typedef enum {
    CACHE_SIZE_8_KB,   /**< L1 or L2 cache size of 8 KB. */
    CACHE_SIZE_16_KB,  /**< L1 or L2 cache size of 16 KB. */
    CACHE_SIZE_32_KB,  /**< L1 or L2 cache size of 32 KB. */
    CACHE_SIZE_64_KB,  /**< L1 or L2 cache size of 64 KB. */
    CACHE_SIZE_128_KB, /**< L1 or L2 cache size of 128 KB. */
    CACHE_SIZE_256_KB, /**< L1 or L2 cache size of 256 KB. */
    CACHE_SIZE_512_KB, /**< L1 or L2 cache size of 512 KB. */
    CACHE_SIZE_1_MB,   /**< L1 or L2 cache size of 1 MB. */
    CACHE_SIZE_2_MB,   /**< L1 or L2 cache size of 2 MB. */
    CACHE_SIZE_UNKNOWN /**< Unknown L1 or L2 cache size. */
} cache_size_t;

DR_API
/** Returns the cache line size in bytes of the processor. */
size_t
proc_get_cache_line_size(void);

DR_API
/** Returns true only if \p addr is cache-line-aligned. */
bool
proc_is_cache_aligned(void *addr);

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

#if defined(AARCH64) && defined(BUILD_TESTS)
DR_API
/**
 * Allows overriding the available state of CPU features.
 * This is only for unit testing and offline decode, and must be called after
 * proc_init_arch() (e.g. after dr_standalone_init() or dr_app_setup()).
 */
void
proc_set_feature(feature_bit_t f, bool enable);

DR_API
/**
 * Uses proc_set_feature() to forcibly enable CPU features for unit testing and offline
 * decode.
 */
void
enable_all_test_cpu_features();
#endif

DR_API
/**
 * Returns all 4 32-bit feature values on X86 and architectural feature
 * registers' values on AArch64. Use proc_has_feature() to test for specific
 * features.
 */
features_t *
proc_get_all_feature_bits(void);

DR_API
/** Returns the processor brand string as given by the cpuid instruction. */
char *
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
const char *
proc_get_cache_size_str(cache_size_t size);

#ifdef AARCHXX
DR_API
/**
 * Returns the size in bytes of the SVE registers' vector length set by the
 * AArch64 hardware implementor. Length can be from 128 to 2048 bits in
 * multiples of 128 bits:
 * 128 256 384 512 640 768 896 1024 1152 1280 1408 1536 1664 1792 1920 2048
 * Currently DynamoRIO supports implementations of up to 512 bits.
 */
uint
proc_get_vector_length_bytes(void);
#endif

DR_API
/**
 * Returns the size in bytes needed for a buffer for saving the x87 floating point state.
 */
size_t
proc_fpstate_save_size(void);

DR_API
/**
 * Returns the number of SIMD registers preserved for a context switch. DynamoRIO
 * may decide to optimize the number of registers saved, in which case this number
 * may be less than proc_num_simd_registers(). For x86 this only includes xmm/ymm/zmm.
 *
 * The number of saved SIMD registers may be variable. For example, we may decide
 * to optimize the number of saved registers in a context switch to avoid frequency
 * scaling (https://github.com/DynamoRIO/dynamorio/issues/3169).
 */
/* PR 306394: for 32-bit xmm0-7 are caller-saved, and are touched by
 * libc routines invoked by DR in some Linux systems (xref i#139),
 * so they should be saved in 32-bit Linux.
 *
 * Xref i#139:
 * XMM register preservation will cause extra runtime overhead.
 * We test it over 32-bit SPEC2006 on a 64-bit Debian Linux, which shows
 * that DR with xmm preservation adds negligible overhead over DR without
 * xmm preservation.
 * It means xmm preservation would have little performance impact over
 * DR base system. This is mainly because DR's own operations' overhead
 * is much higher than the context switch overhead.
 * However, if a program is running with a DR client which performs many
 * clean calls (one or more per basic block), xmm preservation may
 * have noticable impacts, i.e. pushing bbs over the max size limit,
 * and could have a noticeable performance hit.
 */
int
proc_num_simd_saved(void);

DR_API
/**
 * Returns the number of SIMD registers. The number returned here depends on the
 * processor and OS feature bits on a given machine. For x86 this only includes
 * xmm/ymm/zmm.
 *
 */
int
proc_num_simd_registers(void);

DR_API
/**
 * Returns the number of AVX-512 mask registers. The number returned here depends on the
 * processor and OS feature bits on a given machine.
 *
 */
int
proc_num_opmask_registers(void);

DR_API
/**
 * Saves the x87 floating point state into the buffer \p buf.
 *
 * On x86, the buffer must be 16-byte-aligned, and it must be
 * 512 (#DR_FPSTATE_BUF_SIZE) bytes for processors with the FXSR feature,
 * and 108 bytes for those without (where this routine does not support
 * 16-bit operand sizing).  On ARM/AArch64, nothing needs to be saved as the
 * SIMD/FP registers are saved together with the general-purpose registers.
 *
 * \note proc_fpstate_save_size() can be used to determine the particular
 * size needed.
 *
 * When the FXSR feature is present, the fxsave format matches the bitwidth
 * of the ISA mode of the current thread (see dr_get_isa_mode()).
 *
 * The last floating-point instruction address is left in an
 * untranslated state (i.e., it may point into the code cache).
 *
 * DR does NOT save the application's x87 floating-point or MMX state
 * on context switches!  Thus if a client performs any floating-point
 * operations in its main routines called by DR and cannot prove that its
 * compiler will not use x87 operations, the client must save
 * and restore the x87 floating-point/MMX state.
 * If the client needs to do so inside the code cache the client should implement
 * that itself.
 * Returns number of bytes written.
 */
size_t
proc_save_fpstate(byte *buf);

DR_API
/**
 * Restores the x87 floating point state from the buffer \p buf.
 * On x86, the buffer must be 16-byte-aligned, and it must be
 * 512 (#DR_FPSTATE_BUF_SIZE) bytes for processors with the FXSR feature,
 * and 108 bytes for those without (where this routine does not support
 * 16-bit operand sizing).  On ARM/AArch64, nothing needs to be restored as the
 * SIMD/FP registers are restored together with the general-purpose registers.
 *
 * \note proc_fpstate_save_size() can be used to determine the particular
 * size needed.
 *
 * When the FXSR feature is present, the fxsave format matches the bitwidth
 * of the ISA mode of the current thread (see dr_get_isa_mode()).
 */
void
proc_restore_fpstate(byte *buf);

DR_API
/**
 * Returns whether AVX (or AVX2) is enabled by both the processor and the OS.
 * Even if the processor supports AVX, if the OS does not enable AVX, then
 * AVX instructions will fault.
 */
bool
proc_avx_enabled(void);

DR_API
/**
 * Returns whether AVX-512 is enabled by both the processor and the OS.
 * Even if the processor supports AVX-512, if the OS does not enable AVX-512,
 * then AVX-512 instructions will fault.
 */
bool
proc_avx512_enabled(void);

#endif /* _DR_PROC_H_ */
