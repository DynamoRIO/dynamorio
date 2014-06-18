/* ******************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
 * ******************************************************/

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

#ifndef _DYNAMORIO_ANNOTATION_ASM_H_
#define _DYNAMORIO_ANNOTATION_ASM_H_ 1

#include <stddef.h>

#ifndef DYNAMORIO_ANNOTATIONS_X64
# ifdef _MSC_VER
#  ifdef _WIN64
#   define DYNAMORIO_ANNOTATIONS_X64 1
#  endif
# else
#  ifdef __LP64__
#   define DYNAMORIO_ANNOTATIONS_X64 1
#  endif
# endif
#endif

#ifdef _MSC_VER /* Microsoft Visual Studio */
/* The value of this intrinsic is assumed to be non-zero, though in the rare case of
 * annotations occurring within managed code (i.e., C# or VB), zero is possible. In
 * this case the annotation would be executed in a native run (it will not crash or
 * alter the program behavior in any way, other than wasting a cache fetch). */
# pragma intrinsic(_AddressOfReturnAddress)

# define DR_ANNOTATION(annotation, ...) \
do { \
    if (_AddressOfReturnAddress() == (void *) 0) { \
        annotation##_tag(); \
        annotation(__VA_ARGS__); \
    } \
} while (0)
# define DR_ANNOTATION_EXPRESSION(annotation, ...) \
    (((_AddressOfReturnAddress() == (void *) 0) ? \
        (annotation##_tag() | (uintptr_t) annotation(__VA_ARGS__)) : 0) != 0)
#else /* GCC or Intel (may be Unix or Windows) */
/* Each reference to _GLOBAL_OFFSET_TABLE_ is adjusted by the linker to be
 * XIP-relative, and no relocations are generated for the operand. */
# ifdef DYNAMORIO_ANNOTATIONS_X64
#  define DR_ANNOTATION(annotation, ...) \
({ \
    __label__ jump_to; \
    extern const char *annotation##_name; \
    __asm__ volatile goto ("jmp %l0; \
                            mov _GLOBAL_OFFSET_TABLE_,%%rax; \
                            bsf "#annotation"_name@GOT,%%rax;" \
                            ::: "%rax" : jump_to); \
    annotation(__VA_ARGS__); \
    jump_to:; \
})
#  define DR_ANNOTATION_EXPRESSION(annotation, ...) \
({ \
    __label__ jump_to; \
    extern const char *annotation##_name; \
    __asm__ volatile goto ("jmp %l0; \
                            mov _GLOBAL_OFFSET_TABLE_,%%rax; \
                            bsr "#annotation"_name@GOT,%%rax;" \
                            ::: "%rax" : jump_to); \
    jump_to: \
    annotation(__VA_ARGS__); \
})
# else
#  define DR_ANNOTATION(annotation, ...) __extension__ \
({ \
    __label__ jump_to; \
    extern const char *annotation##_name; \
    __asm__ volatile goto ("jmp %l0; \
                            mov _GLOBAL_OFFSET_TABLE_,%%eax; \
                            bsf "#annotation"_name@GOT,%%eax;" \
                            ::: "%eax" : jump_to); \
    annotation(__VA_ARGS__); \
    jump_to:; \
})
#  define DR_ANNOTATION_EXPRESSION(annotation, ...) __extension__ \
({ \
    __label__ jump_to; \
    extern const char *annotation##_name; \
    __asm__ volatile goto ("jmp %l0; \
                            mov _GLOBAL_OFFSET_TABLE_,%%eax; \
                            bsr "#annotation"_name@GOT,%%eax;" \
                            ::: "%eax" : jump_to); \
    jump_to: \
    annotation(__VA_ARGS__); \
})
# endif
#endif

#ifdef _MSC_VER
# define DR_WEAK_DECLARATION
# define DR_DEFINE_ANNOTATION_TAG(annotation) \
    static void annotation##_tag() \
    { \
        extern const char *annotation##_name; \
        _m_prefetch(annotation##_name); \
    }
# define DR_DECLARE_ANNOTATION(annotation) \
    DR_DEFINE_ANNOTATION_TAG(annotation) \
    void __fastcall annotation
# define DR_DEFINE_ANNOTATION_EXPRESSION_TAG(annotation) \
    static uintptr_t annotation##_tag() \
    { \
        extern const char *annotation##_name; \
        _m_prefetch(annotation##_name); \
        return 1; \
    }
# define DR_DECLARE_ANNOTATION_EXPRESSION(return_type, annotation) \
    DR_DEFINE_ANNOTATION_EXPRESSION_TAG(annotation) \
    return_type __fastcall annotation
#else
# define DR_WEAK_DECLARATION __attribute__ ((weak))
# ifdef DYNAMORIO_ANNOTATIONS_X64
#  define DR_DECLARE_ANNOTATION(annotation) \
    __attribute__((noinline, visibility("hidden"))) void annotation
#  define DR_DECLARE_ANNOTATION_EXPRESSION(return_type, annotation) \
    __attribute__((noinline, visibility("hidden"))) return_type annotation
# else
#  define DR_DECLARE_ANNOTATION(annotation) \
    __attribute__((noinline, fastcall, visibility("hidden"))) void annotation
#  define DR_DECLARE_ANNOTATION_EXPRESSION(return_type, annotation) \
    __attribute__((noinline, fastcall, visibility("hidden"))) return_type annotation
# endif
#endif

#ifdef _MSC_VER
# define DR_DEFINE_ANNOTATION(return_type, annotation, ...) \
    const char *annotation##_name = "dynamorio-annotation:"#annotation; \
    return_type __fastcall annotation
#else
# ifdef DYNAMORIO_ANNOTATIONS_X64
#  define DR_DEFINE_ANNOTATION(return_type, annotation) \
    const char *annotation##_name = "dynamorio-annotation:"#annotation; \
    __attribute__((noinline, visibility("hidden"))) return_type annotation
# else
#  define DR_DEFINE_ANNOTATION(return_type, annotation) \
    const char *annotation##_name = "dynamorio-annotation:"#annotation; \
    __attribute__((noinline, fastcall, visibility("hidden"))) return_type annotation
# endif
#endif

#endif

