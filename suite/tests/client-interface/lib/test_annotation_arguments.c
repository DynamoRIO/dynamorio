#include "test_annotation_arguments.h"
#include <stdio.h>

#ifdef DR_ANNOTATION_EXPORT
# undef DR_ANNOTATION_EXPORT
#endif

#ifdef _MSC_VER
#else
# ifdef __LP64__
#  define DR_ANNOTATION_EXPORT __attribute__((visibility("default")))
# else
#  define DR_ANNOTATION_EXPORT __attribute__((fastcall, visibility("default")))
# endif
#endif

#pragma auto_inline(off)

#ifdef _MSC_VER
__declspec(dllexport) void __fastcall
test_annotation_eight_args(unsigned int a, unsigned int b,
    unsigned int c, unsigned int d, unsigned int e, unsigned int f,
    unsigned int g, unsigned int h)
{
}

__declspec(dllexport) void __fastcall
test_annotation_nine_args(unsigned int a, unsigned int b,
    unsigned int c, unsigned int d, unsigned int e, unsigned int f,
    unsigned int g, unsigned int h, unsigned int i)
{
}

__declspec(dllexport) void __fastcall
test_annotation_ten_args(unsigned int a, unsigned int b,
    unsigned int c, unsigned int d, unsigned int e, unsigned int f,
    unsigned int g, unsigned int h, unsigned int i, unsigned int j)
{
}
#else
DR_ANNOTATION_EXPORT
void
test_annotation_eight_args(unsigned int a, unsigned int b,
    unsigned int c, unsigned int d, unsigned int e, unsigned int f,
    unsigned int g, unsigned int h)
{
}

DR_ANNOTATION_EXPORT
void
test_annotation_nine_args(unsigned int a, unsigned int b,
    unsigned int c, unsigned int d, unsigned int e, unsigned int f,
    unsigned int g, unsigned int h, unsigned int i)
{
}

DR_ANNOTATION_EXPORT
void
test_annotation_ten_args(unsigned int a, unsigned int b,
    unsigned int c, unsigned int d, unsigned int e, unsigned int f,
    unsigned int g, unsigned int h, unsigned int i, unsigned int j)
{
}
#endif

#undef DR_ANNOTATION_EXPORT

#pragma auto_inline(on)
