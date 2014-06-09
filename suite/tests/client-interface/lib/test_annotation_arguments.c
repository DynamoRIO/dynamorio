#include "test_annotation_arguments.h"

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
# ifndef __LP64__
__attribute__ ((fastcall))
# endif
void
test_annotation_eight_args(unsigned int a, unsigned int b,
    unsigned int c, unsigned int d, unsigned int e, unsigned int f,
    unsigned int g, unsigned int h)
{
}

# ifndef __LP64__
__attribute__ ((fastcall))
# endif
void
test_annotation_nine_args(unsigned int a, unsigned int b,
    unsigned int c, unsigned int d, unsigned int e, unsigned int f,
    unsigned int g, unsigned int h, unsigned int i)
{
}

# ifndef __LP64__
__attribute__ ((fastcall))
# endif
void
test_annotation_ten_args(unsigned int a, unsigned int b,
    unsigned int c, unsigned int d, unsigned int e, unsigned int f,
    unsigned int g, unsigned int h, unsigned int i, unsigned int j)
{
}
#endif

#pragma auto_inline(on)
