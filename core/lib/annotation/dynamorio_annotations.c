#include "dynamorio_annotations.h"

#pragma auto_inline(off)

#ifdef _MSC_VER
__declspec(dllexport) char __fastcall
dynamorio_annotate_running_on_dynamorio()
{
    return 0;
}
#else
# ifndef __LP64__
__attribute__ ((fastcall))
# endif
char
dynamorio_annotate_running_on_dynamorio()
{
    return 0;
}
#endif

#pragma auto_inline(on)

