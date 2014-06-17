#include "dynamorio_annotations.h"

#pragma auto_inline(off)

DR_DEFINE_ANNOTATION(char, dynamorio_annotate_running_on_dynamorio) ()
{
#ifndef _MSC_VER
    __asm__ volatile ("");
#endif
    return 0;
}

#pragma auto_inline(on)

