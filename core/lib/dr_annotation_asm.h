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
# define PASTE1(x, y) x##y
# define PASTE(x, y) PASTE1(x, y)

# ifdef DYNAMORIO_ANNOTATIONS_X64
#  pragma intrinsic(_AddressOfReturnAddress)
#  define GET_RETURN_ADDRESS _AddressOfReturnAddress
#  define DR_DEFINE_ANNOTATION_CALL_SITE_TAG(annotation) \
    static void annotation##_tag() \
    { \
        extern const char *annotation##_name; \
        _m_prefetch(annotation##_name); \
    }
#  define DR_ANNOTATION_OR_NATIVE(annotation, native_version, ...) \
do { \
    if (_AddressOfReturnAddress() == (void *) 0) { \
        annotation##_tag(); \
        annotation(__VA_ARGS__); \
    } else { \
        native_version; \
    } \
} while (0)
// how to distinguish from tag?
#  define DR_ANNOTATION_FUNCTION_TAG(annotation) \
    extern const char *annotation##_name; \
    _m_prefetch(annotation##_name);
# else
#  define DR_DEFINE_ANNOTATION_CALL_SITE_TAG(annotation)
#  define DR_ANNOTATION_OR_NATIVE(annotation, native_version, ...) \
    { \
        extern const char *annotation##_name; \
        __asm { \
            __asm _emit 0xeb \
            __asm _emit 0x06 \
            __asm mov eax, annotation##_name \
            __asm _emit 0x01 \
            __asm jmp PASTE(native_execution_, __LINE__) \
            __asm jmp PASTE(native_end_marker_, __LINE__) \
        } \
        annotation(__VA_ARGS__); \
        PASTE(native_execution_, __LINE__) : native_version; \
        PASTE(native_end_marker_, __LINE__): ; \
    }
#  define DR_ANNOTATION_FUNCTION_TAG(annotation) \
    __asm { \
        __asm _emit 0xeb \
        __asm _emit 0x06 \
        __asm mov eax, annotation##_name \
        __asm _emit 0x00 \
    }
# endif
# define DR_DECLARE_ANNOTATION(return_type, annotation, parameters) \
    DR_DEFINE_ANNOTATION_CALL_SITE_TAG(annotation) \
    return_type __fastcall annotation parameters
# define DR_DEFINE_ANNOTATION(return_type, annotation, parameters, body) \
    const char *annotation##_name = "dynamorio-annotation:"#annotation; \
    return_type __fastcall annotation parameters \
    { \
        DR_ANNOTATION_FUNCTION_TAG(annotation) \
        body; \
    }
#else /* GCC or Intel (may be Unix or Windows) */
/* Each reference to _GLOBAL_OFFSET_TABLE_ is adjusted by the linker to be
 * XIP-relative, and no relocations are generated for the operand. */
# ifdef DYNAMORIO_ANNOTATIONS_X64
#  define LABEL_REFERENCE_LENGTH "0x11"
#  define LABEL_REFERENCE_REGISTER "%rax"
#  define _CALL_TYPE
# else
#  define LABEL_REFERENCE_LENGTH "0xc"
#  define LABEL_REFERENCE_REGISTER "%eax"
#  define _CALL_TYPE , fastcall
# endif
# define DR_ANNOTATION_ATTRIBUTES \
    __attribute__((noinline, visibility("hidden") _CALL_TYPE))
# define DR_WEAK_DECLARATION __attribute__ ((weak))
# define DR_ANNOTATION_OR_NATIVE(annotation, native_version, ...) \
({ \
    __label__ native_run, skip_native_run; \
    extern const char *annotation##_name; \
    __asm__ volatile goto (".byte 0xeb; .byte "LABEL_REFERENCE_LENGTH"; \
                            mov _GLOBAL_OFFSET_TABLE_,%"LABEL_REFERENCE_REGISTER"; \
                            bsf "#annotation"_name@GOT,%"LABEL_REFERENCE_REGISTER"; \
                            jmp %l0; \
                            jmp %l1;" \
                            ::: LABEL_REFERENCE_REGISTER \
                            : native_run, skip_native_run); \
    annotation(__VA_ARGS__); \
    native_run: native_version; \
    skip_native_run: ; \
})

# define DR_DECLARE_ANNOTATION(return_type, annotation, parameters) \
     DR_ANNOTATION_ATTRIBUTES return_type annotation parameters DR_WEAK_DECLARATION
# define DR_DEFINE_ANNOTATION(return_type, annotation, parameters, body) \
    const char *annotation##_name = "dynamorio-annotation:"#annotation; \
    DR_ANNOTATION_ATTRIBUTES return_type annotation parameters \
    { \
        extern const char *annotation##_name; \
        __asm__ volatile (".byte 0xeb; .byte "LABEL_REFERENCE_LENGTH"; \
                           mov _GLOBAL_OFFSET_TABLE_,%"LABEL_REFERENCE_REGISTER"; \
                           bsr "#annotation"_name@GOT,%"LABEL_REFERENCE_REGISTER";" \
                           ::: LABEL_REFERENCE_REGISTER); \
        body; \
    }
#endif

#define DR_ANNOTATION(annotation, ...) \
    DR_ANNOTATION_OR_NATIVE(annotation, , __VA_ARGS__)

#endif

