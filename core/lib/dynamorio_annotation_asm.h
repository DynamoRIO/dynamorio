#ifndef _DYNAMORIO_ANNOTATION_ASM_H_
#define _DYNAMORIO_ANNOTATION_ASM_H_ 1

#include <stddef.h>

#ifndef DYNAMORIO_ANNOTATIONS_X64
# ifdef _MSC_VER
#  ifdef _WIN64
#   define DR_ANNOTATIONS_X64 1
#  endif
# else
#  ifdef __LP64__
#   define DR_ANNOTATIONS_X64 1
#  endif
# endif
#endif

#ifdef DYNAMORIO_ANNOTATIONS_X64
# define DYNAMORIO_ANNOTATION_MAGIC_NUMBER 0xaaaabbbbccccddddULL
#else
# define DYNAMORIO_ANNOTATION_MAGIC_NUMBER 0xaabbccddUL
#endif

#ifdef _MSC_VER
# pragma intrinsic(_AddressOfReturnAddress)

# ifdef _WIN64
#  define MAGIC_NUMBER 0xaaaabbbbccccddddULL
#  define RETURN_ALL_BITS 0xffffffffffffffffULL
# else
#  define MAGIC_NUMBER 0xaabbccddUL
#  define RETURN_ALL_BITS 0xffffffffUL
# endif

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
    __asm__ volatile goto ("jmp %l1; \
                            movq %0,%%rax; \
                            bsf "#annotation"_name@GOTPCREL(%%rip),%%rax;" \
                           : \
                           : "i"(DYNAMORIO_ANNOTATION_MAGIC_NUMBER) \
                           : "%rax" \
                           : jump_to); \
    annotation(__VA_ARGS__); \
    jump_to:; \
})
#  define DR_ANNOTATION_EXPRESSION(annotation, ...) \
({ \
    __label__ jump_to; \
    extern const char *annotation##_name; \
    __asm__ volatile goto ("jmp %l1; \
                            movq %0,%%rax; \
                            bsr "#annotation"_name@GOTPCREL(%%rip),%%rax;" \
                           : \
                           : "i"(DYNAMORIO_ANNOTATION_MAGIC_NUMBER) \
                           : "%rax" \
                           : jump_to); \
    jump_to: \
    annotation(__VA_ARGS__); \
})
# else
#  define DR_ANNOTATION(annotation, ...) __extension__ \
({ \
    __label__ jump_to: \
    extern const char *annotation##_name; \
    __asm__ volatile goto ("jmp %l1; \
                            mov %0,%%eax; \
                            mov $_GLOBAL_OFFSET_TABLE_,%%eax; \
                            bsf "#annotation"_name@GOT,%%eax;" \
                           : \
                           : "i"(DYNAMORIO_ANNOTATION_MAGIC_NUMBER) \
                           : "%eax" \
                           : jump_to); \
    annotation(__VA_ARGS__); \
    jump_to:; \
})
#  define DR_ANNOTATION_EXPRESSION(annotation, ...) __extension__ \
({ \
    __label__ jump_to; \
    extern const char *annotation##_name; \
    __asm__ volatile goto ("jmp %l1; \
                            mov %0,%%eax; \
                            mov $_GLOBAL_OFFSET_TABLE_,%%eax; \
                            bsr "#annotation"_name@GOT,%%eax;" \
                           : \
                           : "i"(DYNAMORIO_ANNOTATION_MAGIC_NUMBER) \
                           : "%eax" \
                           : jump_to); \
    jump_to: \
    annotation(__VA_ARGS__); \
})
# endif
#endif

#ifdef _MSC_VER
# define DR_WEAK_DECLARATION
# define DR_DECLARE_ANNOTATION(annotation) \
    static __inline void annotation##_tag() \
    { \
        extern const char *annotation##_name; \
        _m_prefetch(annotation##_name); \
    } \
    void __fastcall annotation
# define DR_DECLARE_ANNOTATION_EXPRESSION(return_type, annotation) \
    static __inline uintptr_t annotation##_tag() \
    { \
        extern const char *annotation##_name; \
        _m_prefetch(annotation##_name); \
        return 0; \
    } \
    return_type __fastcall annotation
#else
# define DR_WEAK_DECLARATION __attribute__ ((weak))
# ifdef DYNAMORIO_ANNOTATIONS_X64
#  define DR_DECLARE_ANNOTATION(return_type, annotation) \
    __attribute__((noinline, visibility("hidden"))) return_type annotation
# else
#  define DR_DECLARE_ANNOTATION(return_type, annotation) \
    __attribute__((noinline, fastcall, visibility("hidden"))) return_type annotation
# endif
# define DR_DECLARE_ANNOTATION_EXPRESSION DR_DECLARE_ANNOTATION
#endif

#ifdef _MSC_VER
# define DR_DEFINE_ANNOTATION(return_type, annotation, ...) \
    const char *annotation##_name = "dynamorio-annotation:"#annotation; \
    return_type __fastcall annotation
#else
# ifdef DYNAMORIO_ANNOTATIONS_X64
#  define DR_DEFINE_ANNOTATION(return_type, annotation) \
    const char *annotation##_name = #annotation; \
    __attribute__((noinline, visibility("hidden"))) return_type annotation
# else
#  define DR_DEFINE_ANNOTATION(return_type, annotation) \
    const char *annotation##_name = #annotation; \
    __attribute__((noinline, fastcall, visibility("hidden"))) return_type annotation
# endif
#endif

#endif

