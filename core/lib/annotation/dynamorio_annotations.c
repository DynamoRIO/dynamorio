#include "dynamorio_annotations.h"

#pragma auto_inline(off)

#ifdef _MSC_VER
__declspec(dllexport) bool __fastcall
dynamorio_annotate_running_on_dynamorio()
{
    return false;
}
#else
# ifndef __LP64__
__attribute__ ((fastcall))
# endif
bool
dynamorio_annotate_running_on_dynamorio()
{
    return false;
}
#endif

#pragma auto_inline(on)

