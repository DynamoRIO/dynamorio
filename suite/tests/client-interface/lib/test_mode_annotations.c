#include "test_mode_annotations.h"

#pragma auto_inline(off)

#ifdef _MSC_VER
__declspec(dllexport) void __fastcall
test_annotation_init_mode(unsigned int mode)
{
}

__declspec(dllexport) void __fastcall
test_annotation_init_context(unsigned int id, const char *mode, unsigned int initial_mode)
{
}

__declspec(dllexport) void __fastcall
test_annotation_set_mode(unsigned int context_id, unsigned int mode)
{
}
#else
# ifndef __LP64__
__attribute__ ((fastcall))
# endif
void
test_annotation_init_mode(unsigned int mode)
{
}

# ifndef __LP64__
__attribute__ ((fastcall))
# endif
void
test_annotation_init_context(unsigned int id, const char *name, unsigned int initial_mode)
{
}

# ifndef __LP64__
__attribute__ ((fastcall))
# endif
void
test_annotation_set_mode(unsigned int context_id, unsigned int mode)
{
}
#endif

#pragma auto_inline(on)
