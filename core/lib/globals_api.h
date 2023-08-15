/* **********************************************************
 * Copyright (c) 2010-2023 Google, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2002-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2002 Hewlett-Packard Company */

#ifndef _DR_DEFINES_H_
#define _DR_DEFINES_H_ 1

/****************************************************************************
 * GENERAL TYPEDEFS AND DEFINES
 */

/**
 * @file dr_defines.h
 * @brief Basic defines and type definitions.
 */

#ifdef WINDOWS
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#    include <winbase.h>
#else
#    include <stdio.h>
#    include <stdlib.h>
#endif
#include <stdarg.h> /* for varargs */

#ifndef DYNAMORIO_INTERNAL
/* A client's target operating system and architecture must be specified. */
#    if !defined(LINUX) && !defined(WINDOWS) && !defined(MACOS)
#        error Target operating system unspecified: define WINDOWS, LINUX, or MACOS
#    endif
#    if defined(X86_32) || defined(X86_64)
#        define X86
#        if (defined(X86_64) && defined(X86_32)) || defined(ARM_32) || \
            defined(ARM_64) || defined(RISCV_64)
#            error Target architecture over-specified: must define only one
#        endif
#    elif defined(ARM_32)
#        define ARM
#        define AARCHXX
#        if defined(X86_32) || defined(X86_64) || defined(ARM_64) || defined(RISCV_64)
#            error Target architecture over-specified: must define only one
#        endif
#    elif defined(ARM_64)
#        define AARCH64
#        define AARCHXX
#        if defined(X86_32) || defined(X86_64) || defined(ARM_32) || defined(RISCV_64)
#            error Target architecture over-specified: must define only one
#        endif
#    elif defined(RISCV_64)
#        define RISCV64
#        if defined(X86_32) || defined(X86_64) || defined(ARM_32) || defined(ARM_64)
#            error Target architecture over-specified: must define only one
#        endif
#    else
#        error Target architecture unknown: define X86_32, X86_64, ARM_32, ARM_64 or \
               RISCV_64
#    endif
#    define DR_API                  /* Ignore for clients. */
#    define DR_UNS_EXCEPT_TESTS_API /* Ignore for clients. */
#endif

#if (defined(X86_64) || defined(ARM_64) || defined(RISCV_64)) && !defined(X64)
#    define X64
#endif

#if (defined(LINUX) || defined(MACOS)) && !defined(UNIX)
#    define UNIX
#endif

/* XXX: Defining c++ keywords "inline", "true", and "false" cause clang-format to not
 * behave properly.  It indents the guard.  There seems to be no workaround.
 */
/* clang-format off */
#    ifndef __cplusplus
#        ifdef WINDOWS
#            define inline __inline
#        else
#            define inline __inline__
#        endif
#        ifndef DR_DO_NOT_DEFINE_bool
#            ifdef DR__Bool_EXISTS
/* prefer _Bool as it avoids truncation casting non-zero to zero */
typedef _Bool bool;
#            else
/* char-sized for compatibility with C++ */
typedef char bool;
#            endif
#        endif
#        ifndef true
#            define true (1)
#        endif
#        ifndef false
#            define false (0)
#        endif
#    endif

#    ifdef UNIX
#        include <sys/types.h> /* for pid_t (non-glibc, e.g. musl) */
#    endif
#    ifdef WINDOWS
/* allow nameless struct/union */
#        pragma warning(disable : 4201)
/* VS2005 warns about comparison operator results being cast to bool (i#523) */
#        if _MSC_VER >= 1400 && _MSC_VER < 1500
#            pragma warning(disable : 4244)
#        endif
#    endif
/* clang-format on */

#ifdef WINDOWS
#    define DR_EXPORT __declspec(dllexport)
#    define LINK_ONCE __declspec(selectany)
#    define ALIGN_VAR(x) __declspec(align(x))
#    define INLINE_FORCED __forceinline
#    define WEAK /* no equivalent, but .obj overrides .lib */
#    define NOINLINE __declspec(noinline)
#else
/* We assume gcc is being used.  If the client is using -fvisibility
 * (in gcc >= 3.4) to not export symbols by default, setting
 * USE_VISIBILITY_ATTRIBUTES will properly export.
 */
#    ifdef USE_VISIBILITY_ATTRIBUTES
#        define DR_EXPORT __attribute__((visibility("default")))
#    else
#        define DR_EXPORT
#    endif
#    define LINK_ONCE __attribute__((weak))
#    define ALIGN_VAR(x) __attribute__((aligned(x)))
#    define INLINE_FORCED inline
#    define WEAK __attribute__((weak))
#    define NOINLINE __attribute__((noinline))
#endif

/* We want a consistent size so we stay away from MAX_PATH.
 * MAX_PATH is 260 on Windows, but 4096 on Linux: should up this.
 * XXX: should undef MAX_PATH and define it to an error-producing value
 * and clean up all uses of it
 */
/**
 * Maximum file path length define meant to replace platform-specific defines
 * such as MAX_PATH and PATH_MAX.
 * Currently, internal stack size limits prevent this from being much larger
 * on UNIX.
 */
#ifdef WINDOWS
#    define MAXIMUM_PATH 260
#else
#    define MAXIMUM_PATH 512
#endif

#ifndef NULL
#    ifdef __cplusplus
#        define NULL nullptr
#    else
#        define NULL ((void *)0)
#    endif
#endif

/* on Windows where bool is char casting can truncate non-zero to zero
 * so we use this macro
 */
#define CAST_TO_bool(x) (!!(x))

#ifndef DR_DO_NOT_DEFINE_uint
typedef unsigned int uint;
#endif
#ifndef DR_DO_NOT_DEFINE_ushort
typedef unsigned short ushort;
#endif
#ifndef DR_DO_NOT_DEFINE_byte
typedef unsigned char byte;
#endif
#ifndef DR_DO_NOT_DEFINE_sbyte
typedef signed char sbyte;
#endif
typedef byte *app_pc;

typedef void (*generic_func_t)();

#ifdef DR_DEFINE_FOR_uint64
#    undef DR_DO_NOT_DEFINE_uint64
#endif

#ifdef DR_DEFINE_FOR_int64
#    undef DR_DO_NOT_DEFINE_int64
#endif

#ifdef WINDOWS
#    ifndef DR_DEFINE_FOR_uint64
#        define DR_DEFINE_FOR_uint64 unsigned __int64
#    endif
#    ifndef DR_DEFINE_FOR_int64
#        define DR_DEFINE_FOR_int64 __int64
#    endif
#    ifdef X64
typedef __int64 ssize_t;
#    else
typedef int ssize_t;
#    endif
#    define _SSIZE_T_DEFINED
#    ifndef INT64_FORMAT
#        define INT64_FORMAT "I64"
#    endif
#else /* Linux */
#    ifdef X64
#        ifndef DR_DEFINE_FOR_uint64
#            define DR_DEFINE_FOR_uint64 unsigned long int
#        endif
#        ifndef DR_DEFINE_FOR_int64
#            define DR_DEFINE_FOR_int64 long int
#        endif
#        ifndef INT64_FORMAT
#            define INT64_FORMAT "l"
#        endif
#    else
#        ifndef DR_DEFINE_FOR_uint64
#            define DR_DEFINE_FOR_uint64 unsigned long long int
#        endif
#        ifndef DR_DEFINE_FOR_int64
#            define DR_DEFINE_FOR_int64 long long int
#        endif
#        ifndef INT64_FORMAT
#            define INT64_FORMAT "ll"
#        endif
#    endif
#endif

#ifndef DR_DO_NOT_DEFINE_int64
typedef DR_DEFINE_FOR_int64 int64;
#endif
#ifndef DR_DO_NOT_DEFINE_uint64
typedef DR_DEFINE_FOR_uint64 uint64;
#endif

/* a register value: could be of any type; size is what matters. */
#ifdef X64
typedef uint64 reg_t;
#else
typedef uint reg_t;
#endif
/* integer whose size is based on pointers: ptr diff, mask, etc. */
typedef reg_t ptr_uint_t;
#ifdef X64
typedef int64 ptr_int_t;
#else
typedef int ptr_int_t;
#endif
/* for memory region sizes, use size_t */

/**
 * Application offset from module base.
 * PE32+ modules are limited to 2GB, but not ELF x64 med/large code model.
 */
typedef size_t app_rva_t;

#define PTR_UINT_0 ((ptr_uint_t)0U)
#define PTR_UINT_1 ((ptr_uint_t)1U)
#define PTR_UINT_MINUS_1 ((ptr_uint_t)-1)

#ifdef WINDOWS
typedef ptr_uint_t thread_id_t;
typedef ptr_uint_t process_id_t;
#elif defined(MACOS)
typedef uint64 thread_id_t;
typedef pid_t process_id_t;
#else /* Linux */
typedef pid_t thread_id_t;
typedef pid_t process_id_t;
#endif

#define INVALID_PROCESS_ID PTR_UINT_MINUS_1

#ifndef DYNAMORIO_INTERNAL
#    ifdef WINDOWS
/* since a FILE cannot be used outside of the DLL it was created in,
 * we have to use HANDLE on Windows
 * we hide the distinction behind the file_t type
 */
typedef HANDLE file_t;
/** The sentinel value for an invalid file_t. */
#        define INVALID_FILE INVALID_HANDLE_VALUE
/* dr_get_stdout_file and dr_get_stderr_file return errors as
 * INVALID_HANDLE_VALUE.  We leave INVALID_HANDLE_VALUE as is,
 * since it equals INVALID_FILE
 */
/** The file_t value for standard output. */
#        define STDOUT (dr_get_stdout_file())
/** The file_t value for standard error. */
#        define STDERR (dr_get_stderr_file())
/** The file_t value for standard input. */
#        define STDIN (dr_get_stdin_file())
#    endif
#endif

#ifdef UNIX
typedef int file_t;
/** The sentinel value for an invalid file_t. */
#    define INVALID_FILE -1
/** Allow use of stdout after the application closes it. */
extern file_t our_stdout;
/** Allow use of stderr after the application closes it. */
extern file_t our_stderr;
/** Allow use of stdin after the application closes it. */
extern file_t our_stdin;
/** The file_t value for standard output. */
#    define STDOUT our_stdout
/** The file_t value for standard error. */
#    define STDERR our_stderr
/** The file_t value for standard error. */
#    define STDIN our_stdin
#endif

/* Note that we considered using a 128-bit GUID for the client ID,
 * but decided it was unnecessary since the client registration
 * routine will complain about conflicting IDs.  Also, we're storing
 * this value in the registry, so no reason to make it any longer
 * than we have to.
 */
/**
 * ID used to uniquely identify a client.  This value is set at
 * client registration and passed to the client in dr_client_main().
 */
typedef uint client_id_t;

#ifndef DR_FAST_IR
/**
 * Internal structure of opnd_t is below abstraction layer.
 * But compiler needs to know field sizes to copy it around
 */
typedef struct {
#    ifdef X64
    uint black_box_uint;
    uint64 black_box_uint64;
#    else
    uint black_box_uint[3];
#    endif
} opnd_t;

/**
 * Internal structure of instr_t is below abstraction layer, but we
 * provide its size so that it can be used in stack variables
 * instead of always allocated on the heap.
 */
typedef struct {
#    ifdef X64
    uint black_box_uint[28];
#    else
    uint black_box_uint[19];
#    endif
} instr_t;
#else
struct _opnd_t;
typedef struct _opnd_t opnd_t;
struct _instr_t;
typedef struct _instr_t instr_t;
#endif

#ifndef IN
#    define IN /* marks input param */
#endif
#ifndef OUT
#    define OUT /* marks output param */
#endif
#ifndef INOUT
#    define INOUT /* marks input+output param */
#endif

#ifdef X86
#    define IF_X86(x) x
#    define IF_X86_ELSE(x, y) x
#    define IF_X86_(x) x,
#    define _IF_X86(x) , x
#    define IF_NOT_X86(x)
#    define IF_NOT_X86_(x)
#    define _IF_NOT_X86(x)
#else
#    define IF_X86(x)
#    define IF_X86_ELSE(x, y) y
#    define IF_X86_(x)
#    define _IF_X86(x)
#    define IF_NOT_X86(x) x
#    define IF_NOT_X86_(x) x,
#    define _IF_NOT_X86(x) , x
#endif

#ifdef ARM
#    define IF_ARM(x) x
#    define IF_ARM_ELSE(x, y) x
#    define IF_ARM_(x) x,
#    define _IF_ARM(x) , x
#    define IF_NOT_ARM(x)
#    define _IF_NOT_ARM(x)
#else
#    define IF_ARM(x)
#    define IF_ARM_ELSE(x, y) y
#    define IF_ARM_(x)
#    define _IF_ARM(x)
#    define IF_NOT_ARM(x) x
#    define _IF_NOT_ARM(x) , x
#endif

#ifdef AARCH64
#    define IF_AARCH64(x) x
#    define IF_AARCH64_ELSE(x, y) x
#    define IF_AARCH64_(x) x,
#    define _IF_AARCH64(x) , x
#    define IF_NOT_AARCH64(x)
#    define _IF_NOT_AARCH64(x)
#else
#    define IF_AARCH64(x)
#    define IF_AARCH64_ELSE(x, y) y
#    define IF_AARCH64_(x)
#    define _IF_AARCH64(x)
#    define IF_NOT_AARCH64(x) x
#    define _IF_NOT_AARCH64(x) , x
#endif

#ifdef AARCHXX
#    define IF_AARCHXX(x) x
#    define IF_AARCHXX_ELSE(x, y) x
#    define IF_AARCHXX_(x) x,
#    define _IF_AARCHXX(x) , x
#    define IF_NOT_AARCHXX(x)
#    define _IF_NOT_AARCHXX(x)
#else
#    define IF_AARCHXX(x)
#    define IF_AARCHXX_ELSE(x, y) y
#    define IF_AARCHXX_(x)
#    define _IF_AARCHXX(x)
#    define IF_NOT_AARCHXX(x) x
#    define _IF_NOT_AARCHXX(x) , x
#endif

#ifdef RISCV64
#    define IF_RISCV64(x) x
#    define IF_RISCV64_ELSE(x, y) x
#    define IF_RISCV64_(x) x,
#    define _IF_RISCV64(x) , x
#    define IF_NOT_RISCV64(x)
#    define _IF_NOT_RISCV64(x)
#else
#    define IF_RISCV64(x)
#    define IF_RISCV64_ELSE(x, y) y
#    define IF_RISCV64_(x)
#    define _IF_RISCV64(x)
#    define IF_NOT_RISCV64(x) x
#    define _IF_NOT_RISCV64(x) , x
#endif

#ifdef ANDROID
#    define IF_ANDROID(x) x
#    define IF_ANDROID_ELSE(x, y) x
#    define IF_NOT_ANDROID(x)
#else
#    define IF_ANDROID(x)
#    define IF_ANDROID_ELSE(x, y) y
#    define IF_NOT_ANDROID(x) x
#endif

#ifdef X64
#    define IF_X64(x) x
#    define IF_X64_ELSE(x, y) x
#    define IF_X64_(x) x,
#    define _IF_X64(x) , x
#    define IF_NOT_X64(x)
#    define _IF_NOT_X64(x)
#else
#    define IF_X64(x)
#    define IF_X64_ELSE(x, y) y
#    define IF_X64_(x)
#    define _IF_X64(x)
#    define IF_NOT_X64(x) x
#    define _IF_NOT_X64(x) , x
#endif

#if defined(X86) && !defined(X64)
#    define IF_X86_32(x) x
#else
#    define IF_X86_32(x)
#endif

#if defined(X86) && defined(X64)
#    define IF_X86_64(x) x
#    define IF_X86_64_ELSE(x, y) x
#    define IF_X86_64_(x) x,
#    define _IF_X86_64(x) , x
#    define IF_NOT_X86_64(x)
#    define _IF_NOT_X86_64(x)
#else
#    define IF_X86_64(x)
#    define IF_X86_64_ELSE(x, y) y
#    define IF_X86_64_(x)
#    define _IF_X86_64(x)
#    define IF_NOT_X86_64(x) x
#    define _IF_NOT_X86_64(x) , x
#endif

#if defined(X64) || defined(ARM)
#    define IF_X64_OR_ARM(x) x
#    define IF_NOT_X64_OR_ARM(x)
#else
#    define IF_X64_OR_ARM(x)
#    define IF_NOT_X64_OR_ARM(x) x
#endif

/* Convenience defines for cross-platform printing.
 * For printing pointers: if using system printf, for %p gcc prepends 0x and uses
 * lowercase while cl does not prepend, puts leading 0's, and uses uppercase.
 * Also, the C standard does not allow min width for %p.
 * However, with our own d_r_vsnprintf, we are able to use %p and thus satisfy
 * format string compiler warnings.
 * Two macros:
 * - PFMT == Pointer Format == with leading zeros
 * - PIFMT == Pointer Integer Format == no leading zeros
 * Convenience macros to shrink long lines:
 * - PFX == Pointer Format with leading 0x
 * - PIFX == Pointer Integer Format with leading 0x
 * For printing memory region sizes:
 * - SZFMT == Size Format
 * - SSZFMT == Signed Size Format
 * For printing 32-bit integers as hex we use %x.  We could use a macro
 * there and then disallow %x, to try and avoid 64-bit printing bugs,
 * but it wouldn't be a panacea.
 */
#define UINT64_FORMAT_CODE INT64_FORMAT "u"
#define INT64_FORMAT_CODE INT64_FORMAT "d"
#define UINT64_FORMAT_STRING "%" UINT64_FORMAT_CODE
#define INT64_FORMAT_STRING "%" INT64_FORMAT_CODE
#define HEX64_FORMAT_STRING "%" INT64_FORMAT "x"
#define ZHEX64_FORMAT_STRING "%016" INT64_FORMAT "x"
#ifdef UNIX
#    define ZHEX32_FORMAT_STRING "%08x"
#    define HEX32_FORMAT_STRING "%x"
#else
#    ifdef X64
#        define ZHEX32_FORMAT_STRING "%08I32x"
#        define HEX32_FORMAT_STRING "%I32x"
#    else
#        define ZHEX32_FORMAT_STRING "%08x"
#        define HEX32_FORMAT_STRING "%x"
#    endif
#endif
#ifdef X64
#    define PFMT ZHEX64_FORMAT_STRING
#    define PIFMT HEX64_FORMAT_STRING
#    define SZFMT INT64_FORMAT_STRING
#    define SSZFMT INT64_FORMAT_STRING
#    define SZFC UINT64_FORMAT_CODE
#    define SSZFC INT64_FORMAT_CODE
#else
#    define PFMT ZHEX32_FORMAT_STRING
#    define PIFMT HEX32_FORMAT_STRING
#    define SZFMT "%u"
#    define SSZFMT "%d"
#    define SZFC "u"
#    define SSZFC "d"
#endif
#define PFX "%p"        /**< printf format code for pointers */
#define PIFX "0x" PIFMT /**< printf format code for pointer-sized integers */

#ifndef INFINITE
#    define INFINITE 0xFFFFFFFF
#endif

/* printf codes for {thread,process}_id_t */
#ifdef WINDOWS
#    define PIDFMT SZFMT /**< printf format code for process_id_t */
#    define TIDFMT SZFMT /**< printf format code for thread_id_t */
#else
#    define PIDFMT "%d" /**< printf format code for process_id_t */
#    ifdef MACOS
#        define TIDFMT                                                   \
            UINT64_FORMAT_STRING /**< printf format code for thread_id_t \
                                  */
#    else
#        define TIDFMT "%d" /**< printf format code for thread_id_t */
#    endif
#endif

/** 128-bit XMM register. */
typedef union _dr_xmm_t {
#ifdef X64
    uint64 u64[2]; /**< Representation as 2 64-bit integers. */
#endif
    uint u32[4];                  /**< Representation as 4 32-bit integers. */
    byte u8[16];                  /**< Representation as 16 8-bit integers. */
    reg_t reg[IF_X64_ELSE(2, 4)]; /**< Representation as 2 or 4 registers. */
} dr_xmm_t;

/** 256-bit YMM register. */
typedef union _dr_ymm_t {
    /* We avoid having 8-byte-aligned fields here for 32-bit: they cause
     * cl to add padding in app_state_at_intercept_t and unprotected_context_t,
     * which messes up our interception stack layout and our x86.asm offsets.
     * We don't access these very often, so we just omit this field.
     *
     * With the new dr_mcontext_t's size field pushing its ymm field to 0x44
     * having an 8-byte-aligned field here adds 4 bytes padding.
     * We could shrink PRE_XMM_PADDING for client header files but simpler
     * to just have u64 only be there for 64-bit for clients.
     * We do the same thing for dr_xmm_t just to look consistent.
     */
#ifndef DYNAMORIO_INTERNAL
#    ifdef X64
    uint64 u64[4]; /**< Representation as 4 64-bit integers. */
#    endif
#endif
    uint u32[8];                  /**< Representation as 8 32-bit integers. */
    byte u8[32];                  /**< Representation as 32 8-bit integers. */
    reg_t reg[IF_X64_ELSE(4, 8)]; /**< Representation as 4 or 8 registers. */
} dr_ymm_t;

/** 512-bit ZMM register. */
typedef union _dr_zmm_t {
#ifndef DYNAMORIO_INTERNAL
#    ifdef X64
    uint64 u64[8]; /**< Representation as 8 64-bit integers. */
#    endif
#endif
    uint u32[16];                  /**< Representation as 16 32-bit integers. */
    byte u8[64];                   /**< Representation as 64 8-bit integers. */
    reg_t reg[IF_X64_ELSE(8, 16)]; /**< Representation as 8 or 16 registers. */
} dr_zmm_t;

/* The register may be only 16 bits wide on systems without AVX512BW, but can be up to
 * MAX_KL = 64 bits.
 */
/** AVX-512 OpMask (k-)register. */
typedef uint64 dr_opmask_t;

#if defined(AARCHXX)
/**
 * 512-bit ARM Scalable Vector Extension (SVE) vector registers Zn and
 * predicate registers Pn. Low 128 bits of Zn overlap with existing ARM
 * Advanced SIMD (NEON) Vn registers. The SVE specification defines the
 * following valid vector lengths:
 * 128 256 384 512 640 768 896 1024 1152 1280 1408 1536 1664 1792 1920 2048
 * We currently support 512-bit maximum due to DR's stack size limitation,
 * (machine context stored in the stack). In AArch64, align to 16 bytes for
 * better performance. In AArch32, we're not using any uint64 fields here to
 * avoid alignment padding in sensitive structs. We could alternatively use
 * pragma pack.
 */
#    ifdef X64
typedef union ALIGN_VAR(16) _dr_simd_t {
    byte b;       /**< Byte (8 bit, Bn) scalar element of Vn, Zn, or Pn.        */
    ushort h;     /**< Halfword (16 bit, Hn) scalar element of Vn, Zn and Pn.   */
    uint s;       /**< Singleword (32 bit, Sn) scalar element of Vn, Zn and Pn. */
    uint64 d;     /**< Doubleword (64 bit, Dn) scalar element of Vn, Zn and Pn. */
    uint q[4];    /**< The full 128 bit Vn register, Qn as q[3]:q[2]:q[1]:q[0]. */
    uint u32[16]; /**< The full 512 bit Zn, Pn and FFR registers. */
} dr_simd_t;
#    else
typedef union _dr_simd_t {
    uint s[4];   /**< Representation as 4 32-bit Sn elements. */
    uint d[4];   /**< Representation as 2 64-bit Dn elements: d[3]:d[2]; d[1]:d[0]. */
    uint u32[4]; /**< The full 128-bit register. */
} dr_simd_t;
#    endif
#    ifdef X64
#        define MCXT_NUM_SIMD_SVE_SLOTS                                  \
            32 /**< Number of 128-bit SIMD Vn/Zn slots in dr_mcontext_t. \
                */
#        define MCXT_NUM_SVEP_SLOTS 16 /**< Number of SIMD Pn slots in dr_mcontext_t. */
#        define MCXT_NUM_FFR_SLOTS \
            1 /**< Number of first-fault register slots in dr_mcontext_t. */
              /** Total number of SIMD register slots in dr_mcontext_t. */
#        define MCXT_NUM_SIMD_SLOTS \
            (MCXT_NUM_SIMD_SVE_SLOTS + MCXT_NUM_SVEP_SLOTS + MCXT_NUM_FFR_SLOTS)
#    else
#        define MCXT_NUM_SIMD_SLOTS                                   \
            16 /**< Number of 128-bit SIMD Vn slots in dr_mcontext_t. \
                */
/* 32bit ARM does not have these slots, but they are defined for compatibility.
 */
#        define MCXT_NUM_SVEP_SLOTS 0
#        define MCXT_NUM_FFR_SLOTS 0
#    endif
#    define PRE_SIMD_PADDING                                        \
        0 /**< Bytes of padding before xmm/ymm dr_mcontext_t slots. \
           */
#    define MCXT_NUM_OPMASK_SLOTS                                    \
        0 /**< Number of 16-64-bit OpMask Kn slots in dr_mcontext_t, \
           * if architecture supports.                               \
           */

#elif defined(X86)

/* If this is increased, you'll probably need to increase the size of
 * inject_into_thread's buf and INTERCEPTION_CODE_SIZE (for Windows).
 * Also, update MCXT_NUM_SIMD_SLOTS in x86.asm and get_xmm_caller_saved.
 * i#437: YMM is an extension of XMM from 128-bit to 256-bit without
 * adding new ones, so code operating on XMM often also operates on YMM,
 * and thus some *XMM* macros also apply to *YMM*.
 */
#    ifdef X64
#        ifdef WINDOWS
/* TODO i#1312: support AVX-512 extended registers. */
/**< Number of [xyz]mm0-5 reg slots in dr_mcontext_t pre AVX-512 in-use. */
#            define MCXT_NUM_SIMD_SSE_AVX_SLOTS 6
/**< Number of [xyz]mm0-5 reg slots in dr_mcontext_t */
#            define MCXT_NUM_SIMD_SLOTS 6
#        else
/**< Number of [xyz]mm-15 reg slots in dr_mcontext_t pre AVX-512 in-use. */
#            define MCXT_NUM_SIMD_SSE_AVX_SLOTS 16
/**< Number of [xyz]mm0-31 reg slots in dr_mcontext_t */
#            define MCXT_NUM_SIMD_SLOTS 32
#        endif
/**< Bytes of padding before simd dr_mcontext_t slots */
#        define PRE_XMM_PADDING 48
#    else
/**< Number of [xyz]mm0-7 reg slots in dr_mcontext_t pre AVX-512 in-use. */
#        define MCXT_NUM_SIMD_SSE_AVX_SLOTS 8
/**< Number of [xyz]mm0-7 reg slots in dr_mcontext_t */
#        define MCXT_NUM_SIMD_SLOTS 8
/**< Bytes of padding before simd dr_mcontext_t slots */
#        define PRE_XMM_PADDING 24
#    endif
/**< Number of 16-64-bit OpMask Kn slots in dr_mcontext_t, if architecture supports. */
#    define MCXT_NUM_OPMASK_SLOTS 8

#elif defined(RISCV64)

/* FIXME i#3544: Not implemented. Definitions just for compiling. */
typedef union ALIGN_VAR(16) _dr_simd_t {
    byte b;      /**< Bottom  8 bits of Vn == Bn. */
    ushort h;    /**< Bottom 16 bits of Vn == Hn. */
    uint s;      /**< Bottom 32 bits of Vn == Sn. */
    uint d[2];   /**< Bottom 64 bits of Vn == Dn as d[1]:d[0]. */
    uint q[4];   /**< 128-bit Qn as q[3]:q[2]:q[1]:q[0]. */
    uint u32[4]; /**< The full 128-bit register. */
} dr_simd_t;
#    define MCXT_NUM_SIMD_SLOTS 8
#    define MCXT_NUM_OPMASK_SLOTS 0
#else
#    error NYI
#endif /* AARCHXX/X86/RISCV64 */

#ifdef DR_NUM_SIMD_SLOTS_COMPATIBILITY

#    undef NUM_SIMD_SLOTS
/**
 * Number of saved SIMD slots in dr_mcontext_t.
 */
#    define NUM_SIMD_SLOTS proc_num_simd_saved()

#    define NUM_XMM_SLOTS NUM_SIMD_SLOTS /* for backward compatibility */

#endif /* DR_NUM_SIMD_SLOTS_COMPATIBILITY */

/** Values for the flags field of dr_mcontext_t */
typedef enum {
    /**
     * On x86, selects the xdi, xsi, xbp, xbx, xdx, xcx, xax, and r8-r15 fields (i.e.,
     * all of the general-purpose registers excluding xsp, xip, and xflags).
     * On ARM, selects r0-r12 and r14.
     * On AArch64, selects r0-r30.
     */
    DR_MC_INTEGER = 0x01,
    /* XXX i#2710: The link register should be under DR_MC_CONTROL */
    /**
     * On x86, selects the xsp, xflags, and xip fields.
     * On ARM, selects the sp, pc, and cpsr fields.
     * On AArch64, selects the sp, pc, and nzcv fields.
     * On RISC-V, selects the sp, pc and fcsr fields.
     * \note: The xip/pc field is only honored as an input for
     * dr_redirect_execution(), and as an output for system call
     * events.
     */
    DR_MC_CONTROL = 0x02,
    /**
     * Selects the simd fields.  This flag is ignored unless
     * dr_mcontext_xmm_fields_valid() returns true.  If
     * dr_mcontext_xmm_fields_valid() returns false, the application values of
     * the multimedia registers remain in the registers themselves.
     */
    DR_MC_MULTIMEDIA = 0x04,
    /** Selects all fields */
    DR_MC_ALL = (DR_MC_INTEGER | DR_MC_CONTROL | DR_MC_MULTIMEDIA),
} dr_mcontext_flags_t;

/**
 * Machine context structure.
 */
typedef struct _dr_mcontext_t {
    /**
     * The size of this structure.  This field must be set prior to filling
     * in the fields to support forward compatibility.
     */
    size_t size;
    /**
     * The valid fields of this structure.  This field must be set prior to
     * filling in the fields.  For input requests (dr_get_mcontext()), this
     * indicates which fields should be written.  Writing the multimedia fields
     * frequently can incur a performance hit.  For output requests
     * (dr_set_mcontext() and dr_redirect_execution()), this indicates which
     * fields will be copied to the actual context.
     */
    dr_mcontext_flags_t flags;
#ifdef DYNAMORIO_INTERNAL
#    include "mcxtx_api.h"
#else
#    include "dr_mcxtx.h"
#endif
} dr_mcontext_t;

/** The opaque type used to represent linear lists of #instr_t instructions. */
typedef struct _instrlist_t instrlist_t;
/** Alias for the #_module_data_t structure holding library information. */
typedef struct _module_data_t module_data_t;

#ifdef X64
/**
 * Upper note values are reserved for core DR.
 */
#    define DR_NOTE_FIRST_RESERVED 0xffffffffffff0000ULL
#else
/**
 * Upper note values are reserved for core DR.
 */
#    define DR_NOTE_FIRST_RESERVED 0xffff0000UL
#endif
enum {
    /**
     * Identifies an annotation point.  This label will be replaced by a
     * clean call to the registered annotation handler.
     */
    DR_NOTE_ANNOTATION = DR_NOTE_FIRST_RESERVED + 1,
    DR_NOTE_RSEQ,
    DR_NOTE_LDEX,
    /** Identifies the end of a clean call. */
    /* This is used to allow instrumentation pre-and-post a clean call for i#4128. */
    DR_NOTE_CLEAN_CALL_END,
    /**
     * Identifies a point at which clients should restore all registers to
     * their application values, as required for DR's internal block mangling.
     */
    DR_NOTE_REG_BARRIER,
    /**
     * Used for internal translation from an instruction list.  These apply not only to
     * client-inserted clean calls but all inserted calls whether inserted by
     * clients or DR and whether fully clean or not.  This is thus distinct from
     * #DR_NOTE_CLEAN_CALL_END.
     */
    DR_NOTE_CALL_SEQUENCE_START,
    DR_NOTE_CALL_SEQUENCE_END,
    /**
     * Placed at the top of a basic block, this identifies the entry to an "rseq" (Linux
     * restartable sequence) region.  The first two label data fields (see
     * instr_get_label_data_area()) are filled in with this rseq region's end PC
     * and its abort handler PC, in that order.
     */
    DR_NOTE_RSEQ_ENTRY,
};

/**
 * Structure written by dr_get_time() to specify the current time.
 */
typedef struct {
    uint year;         /**< The current year. */
    uint month;        /**< The current month, in the range 1 to 12. */
    uint day_of_week;  /**< The day of the week, in the range 0 to 6. */
    uint day;          /**< The day of the month, in the range 1 to 31. */
    uint hour;         /**< The hour of the day, in the range 0 to 23. */
    uint minute;       /**< The minutes past the hour. */
    uint second;       /**< The seconds past the minute. */
    uint milliseconds; /**< The milliseconds past the second. */
} dr_time_t;

/**
 * Used by dr_get_stats() and dr_app_stop_and_cleanup_with_stats()
 */
typedef struct _dr_stats_t {
    /** The size of this structure. Set this to sizeof(dr_stats_t). */
    size_t size;
    /** The total number of basic blocks ever built so far, globally. This
     *  includes duplicates and blocks that were deleted for consistency
     *  or capacity reasons or thread-private caches.
     */
    uint64 basic_block_count;
    /** Peak number of simultaneous threads under DR control. */
    uint64 peak_num_threads;
    /** Accumulated total number of threads encountered by DR. */
    uint64 num_threads_created;
    /**
     * Thread synchronization attempts retried due to the target thread being at
     * an un-translatable spot.
     */
    uint64 synchs_not_at_safe_spot;
    /** Peak number of memory blocks used for unreachable heaps. */
    uint64 peak_vmm_blocks_unreach_heap;
    /** Peak number of memory blocks used for (unreachable) thread stacks. */
    uint64 peak_vmm_blocks_unreach_stack;
    /** Peak number of memory blocks used for unreachable specialized heaps. */
    uint64 peak_vmm_blocks_unreach_special_heap;
    /** Peak number of memory blocks used for other unreachable mappings. */
    uint64 peak_vmm_blocks_unreach_special_mmap;
    /** Peak number of memory blocks used for reachable heaps. */
    uint64 peak_vmm_blocks_reach_heap;
    /** Peak number of memory blocks used for (reachable) code caches. */
    uint64 peak_vmm_blocks_reach_cache;
    /** Peak number of memory blocks used for reachable specialized heaps. */
    uint64 peak_vmm_blocks_reach_special_heap;
    /** Peak number of memory blocks used for other reachable mappings. */
    uint64 peak_vmm_blocks_reach_special_mmap;
    /** Signals delivered to native threads. */
    uint64 num_native_signals;
    /** Number of exits from the code cache. */
    uint64 num_cache_exits;
} dr_stats_t;

/**
 * Error codes of DR API routines.
 */
typedef enum {
    /**
     * Invalid parameter passed to the API routine.
     */
    DR_ERROR_INVALID_PARAMETER = 1,
    /**
     * Insufficient size of passed buffer.
     */
    DR_ERROR_INSUFFICIENT_SPACE = 2,
    /**
     * String encoding is unknown.
     */
    DR_ERROR_UNKNOWN_ENCODING = 3,
    /**
     * Feature of API routine not yet implemented.
     */
    DR_ERROR_NOT_IMPLEMENTED = 4,
} dr_error_code_t;

/**
 * Identifies where a thread's control is at any one point.
 * Used with client PC sampling using dr_set_itimer().
 */
typedef enum {
    DR_WHERE_APP = 0,         /**< Control is in native application code. */
    DR_WHERE_INTERP,          /**< Control is in basic block building. */
    DR_WHERE_DISPATCH,        /**< Control is in d_r_dispatch. */
    DR_WHERE_MONITOR,         /**< Control is in trace building. */
    DR_WHERE_SYSCALL_HANDLER, /**< Control is in system call handling. */
    DR_WHERE_SIGNAL_HANDLER,  /**< Control is in signal handling. */
    DR_WHERE_TRAMPOLINE,      /**< Control is in trampoline hooks. */
    DR_WHERE_CONTEXT_SWITCH,  /**< Control is in context switching. */
    DR_WHERE_IBL,             /**< Control is in inlined indirect branch lookup. */
    DR_WHERE_FCACHE,          /**< Control is in the code cache. */
    DR_WHERE_CLEAN_CALLEE,    /**< Control is in a clean call. */
    DR_WHERE_UNKNOWN,         /**< Control is in an unknown location. */
#ifdef HOT_PATCHING_INTERFACE
    DR_WHERE_HOTPATCH, /**< Control is in hotpatching. */
#endif
    DR_WHERE_LAST /**< Equals the count of DR_WHERE_xxx locations. */
} dr_where_am_i_t;

/**
 * Flags to request non-default preservation of state in a clean call
 * as well as other call options.  This is used with dr_insert_clean_call_ex(),
 * dr_insert_clean_call_ex_varg(), and dr_register_clean_call_insertion_event().
 */
typedef enum {
    /**
     * Save legacy floating-point state (x86-specific; not saved by default).
     * The last floating-point instruction address (FIP) in the saved state is
     * left in an untranslated state (i.e., it may point into the code cache).
     * This flag is orthogonal to the saving of SIMD registers and related flags below.
     */
    DR_CLEANCALL_SAVE_FLOAT = 0x0001,
    /**
     * Skip saving the flags and skip clearing the flags (including
     * DF) for client execution.  Note that this can cause problems
     * if dr_redirect_execution() is called from a clean call,
     * as an uninitialized flags value can cause subtle errors.
     */
    DR_CLEANCALL_NOSAVE_FLAGS = 0x0002,
    /** Skip saving any XMM or YMM registers (saved by default). */
    DR_CLEANCALL_NOSAVE_XMM = 0x0004,
    /** Skip saving any XMM or YMM registers that are never used as parameters. */
    DR_CLEANCALL_NOSAVE_XMM_NONPARAM = 0x0008,
    /** Skip saving any XMM or YMM registers that are never used as return values. */
    DR_CLEANCALL_NOSAVE_XMM_NONRET = 0x0010,
    /**
     * Requests that an indirect call be used to ensure reachability, both for
     * reaching the callee and for any out-of-line helper routine calls.
     * Only honored for 64-bit mode, where r11 will be used for the indirection.
     */
    DR_CLEANCALL_INDIRECT = 0x0020,
    /* internal use only: maps to META_CALL_RETURNS_TO_NATIVE in insert_meta_call_vargs */
    DR_CLEANCALL_RETURNS_TO_NATIVE = 0x0040,
    /**
     * Requests that out-of-line state save and restore routines be used even
     * when a subset of the state does not need to be preserved for this callee.
     * Also disables inlining.
     * This helps guarantee that the inserted code remains small.
     */
    DR_CLEANCALL_ALWAYS_OUT_OF_LINE = 0x0080,
    /**
     * Indicates that the callee will access the application context (either as
     * passed parameters or by calling dr_get_mcontext()).  This flag is passed
     * to callbacks registered with dr_register_clean_call_insertion_event()
     * requesting that register reservation code in clients and libraries restore
     * application values to all registers.  Without this flag, register values
     * observed by the callee may be values written by instrumentation rather than
     * application values.  If the intent is to have a mixture of application and
     * tool values in registers, manual restoration is required rather than passing
     * this automation flag.
     */
    DR_CLEANCALL_READS_APP_CONTEXT = 0x0100,
    /**
     * Indicates that the callee will modify the application context (by calling
     * dr_set_mcontext()).  This flag is passed to callbacks registered with
     * dr_register_clean_call_insertion_event() requesting that register reservation
     * code in clients and libraries update spilled application register values.
     * Without this flag, changes made by dr_set_mcontext() may be undone by later
     * restores of spilled values.
     */
    DR_CLEANCALL_WRITES_APP_CONTEXT = 0x0200,
    /**
     * Indicates that the clean call may be skipped by inserted tool control flow.
     * This affects how register spilling and restoring occurs when combined with the
     * #DR_CLEANCALL_READS_APP_CONTEXT flag.  Tool values may be clobbered when this
     * flag is used.  If control flow and clean call context access is used with
     * registers holding tool values across the clean call, manual restoration may be
     * required rather than passing any of these automated flags.
     *
     * Combining this flag with #DR_CLEANCALL_WRITES_APP_CONTEXT is not supported.
     * Manual updates are required for such a combination.
     */
    /* XXX i#5168: Supporting multipath with writing requires extra drreg
     * functionality for stateless respills, but given that we have not seen any code
     * that requires it we have not put the effort into supporting it.  It is possible
     * that the component inserting the multi-path clean call is not the one controlling
     * the drreg usage and so would have a hard time inserting manual restores.
     */
    DR_CLEANCALL_MULTIPATH = 0x0400,
} dr_cleancall_save_t;

#endif /* _DR_DEFINES_H_ */
