#include "test_mode_annotations.h"

#pragma auto_inline(off)

DR_DEFINE_ANNOTATION(void, test_annotation_init_mode) (unsigned int mode)
{
    __asm__ volatile ("");
}

DR_DEFINE_ANNOTATION(void, test_annotation_init_context) (unsigned int id,
    const char *name, unsigned int initial_mode)
{
    __asm__ volatile ("");
}

DR_DEFINE_ANNOTATION(void, test_annotation_set_mode) (unsigned int context_id,
    unsigned int mode)
{
    __asm__ volatile ("");
}

#pragma auto_inline(on)
