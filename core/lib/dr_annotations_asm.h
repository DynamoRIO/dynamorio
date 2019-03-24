/* ******************************************************
 * Copyright (c) 2014-2015 Google, Inc.  All rights reserved.
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

#ifndef _DYNAMORIO_ANNOTATIONS_ASM_H_
#define _DYNAMORIO_ANNOTATIONS_ASM_H_ 1

/* This header defines the annotation functions and code sequences. For a high-level
 * overview, see the wiki page https://github.com/DynamoRIO/dynamorio/wiki/Annotations.
 */

/* To simplify project configuration, this pragma excludes the file from GCC warnings. */
#ifdef __GNUC__
#    pragma GCC system_header
#endif

#include <stddef.h>
#ifdef _MSC_VER
#    include <intrin.h>
#    pragma warning(disable : 4100) /* disable "unreferenced formal parameter" warning \
                                     */
#endif

#ifndef DYNAMORIO_ANNOTATIONS_X64
#    ifdef _MSC_VER
#        ifdef _WIN64
#            define DYNAMORIO_ANNOTATIONS_X64 1
#        endif
#    else
#        ifdef __LP64__
#            define DYNAMORIO_ANNOTATIONS_X64 1
#        endif
#    endif
#endif

#ifdef _MSC_VER
#    ifdef __cplusplus
#        define EXTERN extern "C"
#    else
#        define EXTERN extern
#    endif
#else
#    ifdef __cplusplus
#        define EXTERN_C extern "C"
#    else
#        define EXTERN_C
#    endif
#endif

#ifdef _MSC_VER /* Microsoft Visual Studio */
#    define PASTE1(x, y) x##y
#    define PASTE(x, y) PASTE1(x, y)

#    ifdef DYNAMORIO_ANNOTATIONS_X64
/* The value of this intrinsic is assumed to be non-zero. In the rare case of
 * annotations occurring within managed code (i.e., C# or VB), it may not properly
 * be the address of the return address on the stack, but it still should never be
 * zero.
 */
#        pragma intrinsic(_AddressOfReturnAddress, __debugbreak, __int2c, _m_prefetchw)
#        define GET_RETURN_PTR _AddressOfReturnAddress
#        define DR_DEFINE_ANNOTATION_LABELS(annotation, return_type)             \
            const char *annotation##_expression_label =                          \
                "dynamorio-annotation:expression:" #return_type ":" #annotation; \
            const char *annotation##_statement_label =                           \
                "dynamorio-annotation:statement:" #return_type ":" #annotation;
#        define DR_EXTERN_ANNOTATION_LABELS(annotation, return_type) \
            EXTERN const char *annotation##_expression_label;        \
            EXTERN const char *annotation##_statement_label;
/* The magic numbers for the head and tail are specially selected to establish
 * immovable "bookends" on the annotation around which the compiler and optimizers
 * will not reorder instructions. The values 0xfffffffffffffff0 and
 * 0xfffffffffffffff1 are chosen because: (1) the stack can never appear in this
 * address range in Windows x64, and (2) there is a little space below
 * 0xffffffffffffffff to prevent optimizations based on the number of possible
 * values greater than the magic number. The factor of 2 prevents optimizers from
 * sharing a single instance of the conditional expression between the tail of one
 * annotation and the head of the next when they appear on subsequent lines. The
 * compiler will always reduce the magic number expression to a single constant,
 * even in a debug build with all optimizations disabled.
 */
#        define DR_ANNOTATION_STATEMENT_HEAD (0xfffffffffffffff1 - (2 * __LINE__))
#        define DR_ANNOTATION_STATEMENT_TAIL (0xfffffffffffffff0 - (2 * __LINE__))
#        define DR_ANNOTATION_OR_NATIVE(annotation, native_version, ...)                 \
            do {                                                                         \
                if ((unsigned __int64)GET_RETURN_PTR() > DR_ANNOTATION_STATEMENT_HEAD) { \
                    __int2c();                                                           \
                    _m_prefetchw(annotation##_statement_label);                          \
                    __debugbreak();                                                      \
                    annotation(__VA_ARGS__);                                             \
                } else {                                                                 \
                    native_version;                                                      \
                }                                                                        \
            } while ((unsigned __int64)GET_RETURN_PTR() > DR_ANNOTATION_STATEMENT_TAIL)
#        define DR_ANNOTATION_FUNCTION(annotation, body)     \
            if (GET_RETURN_PTR() == (void *)0) {             \
                __int2c();                                   \
                _m_prefetchw(annotation##_expression_label); \
                __debugbreak();                              \
            } else {                                         \
                body;                                        \
            }
#    else
/* The assembly sequence for this platform begins with absolute bytes "0xeb 0x06"
 * (jmp +6) to facilitate an optimized detection algorithm in the DR interpreter.
 * The target of this jump is always the following jump, i.e. (mov=5 + pop/nop=1)
 * => 6.
 */
#        define DR_DEFINE_ANNOTATION_LABELS(annotation, return_type) \
            const char *annotation##_label =                         \
                "dynamorio-annotation:" #return_type ":" #annotation;
#        define DR_EXTERN_ANNOTATION_LABELS(annotation, return_type) \
            EXTERN const char *annotation##_label;
#        define DR_ANNOTATION_OR_NATIVE_INSTANCE(unique_id, annotation, native_version, \
                                                 ...)                                   \
            {                                                                           \
                __asm { \
            __asm _emit 0xeb \
            __asm _emit 0x06 \
            __asm mov eax, annotation##_label \
            __asm pop eax \
            __asm jmp PASTE(native_run, unique_id) }   \
                annotation(__VA_ARGS__);                                                \
                goto PASTE(native_end_marker, unique_id);                               \
                PASTE(native_run, unique_id)                                            \
                    : native_version;                                                   \
                PASTE(native_end_marker, unique_id)                                     \
                    :;                                                                  \
            }
#        define DR_ANNOTATION_OR_NATIVE(annotation, native_version, ...)              \
            DR_ANNOTATION_OR_NATIVE_INSTANCE(__COUNTER__, annotation, native_version, \
                                             __VA_ARGS__)
#        define DR_ANNOTATION_FUNCTION_INSTANCE(unique_id, annotation, body)  \
            __asm { \
        __asm _emit 0xeb \
        __asm _emit 0x06 \
        __asm mov eax, annotation##_label \
        __asm nop \
        __asm jmp PASTE(native_run, unique_id) } \
            goto PASTE(native_end_marker, unique_id);                         \
            PASTE(native_run, unique_id)                                      \
                : body;                                                       \
            PASTE(native_end_marker, unique_id)                               \
                :;
#        define DR_ANNOTATION_FUNCTION(annotation, body) \
            DR_ANNOTATION_FUNCTION_INSTANCE(__COUNTER__, annotation, body)
#    endif
#    define DR_DECLARE_ANNOTATION(return_type, annotation, parameters) \
        DR_EXTERN_ANNOTATION_LABELS(annotation, return_type)           \
        return_type __fastcall annotation parameters
#    define DR_DEFINE_ANNOTATION(return_type, annotation, parameters, body) \
        DR_DEFINE_ANNOTATION_LABELS(annotation, return_type)                \
        return_type __fastcall annotation parameters                        \
        {                                                                   \
            DR_ANNOTATION_FUNCTION(annotation, body)                        \
        }
#else /* GCC or Intel (Unix or Windows) */
/* *************************************************************************************
 * The assembly sequence for this platform begins with absolute bytes "0xeb 0xNN"
 * (jmp +LABEL_REFERENCE_LENGTH) to facilitate an optimized detection algorithm in
 * the DR interpreter. The target of this jump is always the following jump.
 * Each reference to _GLOBAL_OFFSET_TABLE_ is adjusted by the linker to be
 * XIP-relative, and no relocations are generated for the operand.
 */
#    ifdef DYNAMORIO_ANNOTATIONS_X64
/* (mov=8 + bsf/bsr=9) => 0x11 */
#        define LABEL_REFERENCE_LENGTH "0x11"
#        define LABEL_REFERENCE_REGISTER "%rax"
/* Defensive clobber list: only rax is actually clobbered, but all
 * argument-passing registers are included to discourage optimizers from using
 * them prior to the annotation. Note that optimizations should be disabled for
 * files that define annotation functions, so this is just an extra precaution.
 */
#        define ANNOTATION_FUNCTION_CLOBBER_LIST \
            "%rax", "%rcx", "%rdx", "%rsi", "%rdi", "%r8", "%r9"
#        define _CALL_TYPE
#        define ANNOT_LBL(annot, base) #        annot "_label@GOT"
#    else
/* (mov=5 + bsf/bsr=7) => 0xc */
#        define LABEL_REFERENCE_LENGTH "0xc"
#        define LABEL_REFERENCE_REGISTER "%eax"
/* Defensive clobber list: only rax is actually clobbered, but all
 * argument-passing registers are included to discourage optimizers from using
 * them prior to the annotation. Note that optimizations should be disabled for
 * files that define annotation functions, so this is just an extra precaution.
 */
#        define ANNOTATION_FUNCTION_CLOBBER_LIST "%rax", "%rcx", "%rdx"
#        define _CALL_TYPE , fastcall
/* i#2050: i386 ABI forces us to use the base register as an offset into GOT, which is
 * not the case for 64-bit.
 */
#        define ANNOT_LBL(annot, base) #        annot "_label@GOT(%" base ")"
#    endif
#    define DR_ANNOTATION_ATTRIBUTES \
        __attribute__((noinline, visibility("hidden") _CALL_TYPE))
#    define DR_WEAK_DECLARATION __attribute__((weak))
/* GCC extension "__label__" confines the named label to the block scope. */
#    define DR_ANNOTATION_OR_NATIVE(annotation, native_version, ...) \
        ({                                                           \
            __label__ native_run, native_end_marker;                 \
            extern const char *annotation##_label;                   \
            __asm__ volatile goto(".byte 0xeb; .byte " LABEL_REFERENCE_LENGTH "; \
                            mov _GLOBAL_OFFSET_TABLE_,%" LABEL_REFERENCE_REGISTER "; \
                            bsf " ANNOT_LBL(annotation, LABEL_REFERENCE_REGISTER) ", \
                               %" LABEL_REFERENCE_REGISTER "; \
                            jmp %l0;" ::                             \
                                      : LABEL_REFERENCE_REGISTER     \
                                  : native_run);                     \
            annotation(__VA_ARGS__);                                 \
            goto native_end_marker;                                  \
        native_run:                                                  \
            native_version;                                          \
        native_end_marker:;                                          \
        })

#    define DR_DECLARE_ANNOTATION(return_type, annotation, parameters) \
        DR_ANNOTATION_ATTRIBUTES return_type annotation parameters DR_WEAK_DECLARATION
#    define DR_DEFINE_ANNOTATION(return_type, annotation, parameters, body)          \
        EXTERN_C const char *annotation##_label =                                    \
            "dynamorio-annotation:" #return_type ":" #annotation;                    \
        DR_ANNOTATION_ATTRIBUTES return_type annotation parameters                   \
        {                                                                            \
            __label__ native_run, native_end_marker;                                 \
            extern const char *annotation##_label;                                   \
            __asm__ volatile goto(".byte 0xeb; .byte " LABEL_REFERENCE_LENGTH "; \
                               mov _GLOBAL_OFFSET_TABLE_,%" LABEL_REFERENCE_REGISTER \
                                  "; \
                               bsr " ANNOT_LBL(annotation,                           \
                                               LABEL_REFERENCE_REGISTER) ", \
                                  %" LABEL_REFERENCE_REGISTER "; \
                               jmp %l0;" ::                                          \
                                      : ANNOTATION_FUNCTION_CLOBBER_LIST             \
                                  : native_run);                                     \
            goto native_end_marker;                                                  \
        native_run:                                                                  \
            body;                                                                    \
        native_end_marker:;                                                          \
        }
#endif

#define DR_ANNOTATION(annotation, ...) DR_ANNOTATION_OR_NATIVE(annotation, , __VA_ARGS__)

#endif
