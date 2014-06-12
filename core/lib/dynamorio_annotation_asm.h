#ifndef _DYNAMORIO_ANNOTATION_ASM_H_
#define _DYNAMORIO_ANNOTATION_ASM_H_ 1

#define DYNAMORIO_ANNOTATION_MAGIC_NUMBER 0xaaaabbbbccccddddULL

#define PASTE1(x, y, z) x##y##z
#define PASTE(x, y, z) PASTE1(x, y, z)

#define DR_ANNOTATION_STATEMENT(annotation, ...) __extension__ \
({ \
    extern const char *annotation##_name; \
    static const char **annotation##_name_ptr = &annotation##_name; \
    __asm__ volatile goto ("jmp %l2; \
                   movq %0,%%rax; \
                   bsf %1,%%rax;" \
                  : \
                  : "i"(DYNAMORIO_ANNOTATION_MAGIC_NUMBER), "m"(annotation##_name_ptr) \
                  : "%rax" \
                  : PASTE(annotation##_,__LINE__,_label)); \
    annotation(__VA_ARGS__); \
    PASTE(annotation##_,__LINE__,_label):; \
})

#define DR_ANNOTATION_EXPRESSION(annotation, ...) __extension__ \
({ \
    extern const char *annotation##_name; \
    static const char **annotation##_name_ptr = &annotation##_name; \
    __asm__ volatile goto ("jmp %l2; \
                   movq %0,%%rax; \
                   bsr %1,%%rax;" \
                  : \
                  : "i"(DYNAMORIO_ANNOTATION_MAGIC_NUMBER), "m"(annotation##_name_ptr) \
                  : "%rax" \
                  : PASTE(annotation##_,__LINE__,_label)); \
    PASTE(annotation##_,__LINE__,_label):; \
    annotation(__VA_ARGS__); \
})

#ifdef _MSC_VER
# define DR_WEAK_DECLARATION
# define DR_DECLARE_ANNOTATION(return_type, annotation) \
  __declspec(dllexport) return_type __fastcall annotation
#else
# define DR_WEAK_DECLARATION __attribute__ ((weak))
# ifdef __LP64__
#  define DR_DECLARE_ANNOTATION(return_type, annotation) \
    __attribute__((noinline, visibility("hidden"))) return_type annotation
# else
#  define DR_DECLARE_ANNOTATION(return_type, annotation) \
    __attribute__((noinline, fastcall, visibility("hidden"))) return_type annotation
# endif
#endif


#ifdef _MSC_VER
#  define DR_DEFINE_ANNOTATION(return_type, annotation) \
  __declspec(dllexport) return_type __fastcall annotation
#else
# ifdef __LP64__
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

