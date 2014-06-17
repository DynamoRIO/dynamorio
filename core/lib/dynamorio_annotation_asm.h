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

#ifdef _MSC_VER
# pragma intrinsic(_AddressOfReturnAddress)

/*
1. no relos in GOT ref (eip-relative)
2. _AddressOfReturnAddress may fail in managed code (very rare)
   a. annotation will still execute correctly, w/o crash
3. configure_DynamoRIO_annotations @make/DynamoRIOConfig.cmake.in (see #572)
4. Change names to dr_annotation*
5. Move samples to the wiki somewhere
*/

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
#else
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
/* bsr "#annotation"_name@GOTPCREL(%%rip),%%rax;" \ */

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
# define DR_DECLARE_ANNOTATION(annotation) \
    static void annotation##_tag() \
    { \
        extern const char *annotation##_name; \
        _m_prefetch(annotation##_name); \
    } \
    void __fastcall annotation
# define DR_DECLARE_ANNOTATION_EXPRESSION(return_type, annotation) \
    static uintptr_t annotation##_tag() \
    { \
        extern const char *annotation##_name; \
        _m_prefetch(annotation##_name); \
        return 0; \
    } \
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

