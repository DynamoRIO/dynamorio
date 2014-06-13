#include "dynamorio_annotations.h"

#pragma auto_inline(off)

DR_DEFINE_ANNOTATION(char, dynamorio_annotate_running_on_dynamorio) ()
{
    __asm__ volatile ("");
    return 0;
}

#pragma auto_inline(on)

