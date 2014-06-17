#include "test_mode_annotations.h"

#pragma auto_inline(off)

DR_DEFINE_ANNOTATION(void, test_annotation_init_mode) (unsigned int mode)
{
#ifndef _MSC_VER
    __asm__ volatile ("");
#endif
}

DR_DEFINE_ANNOTATION(void, test_annotation_init_context) (unsigned int id,
    const char *name, unsigned int initial_mode)
{
#ifndef _MSC_VER
    __asm__ volatile ("");
#endif
}

DR_DEFINE_ANNOTATION(void, test_annotation_set_mode) (unsigned int context_id,
    unsigned int mode)
{
#ifndef _MSC_VER
    __asm__ volatile ("");
#endif
}

#pragma auto_inline(on)
