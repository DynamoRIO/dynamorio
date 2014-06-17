#ifndef _DYNAMORIO_ANNOTATIONS_H_
#define _DYNAMORIO_ANNOTATIONS_H_ 1

#include "annotation/dynamorio_annotation_asm.h"

#define DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO() \
    DR_ANNOTATION_EXPRESSION(dynamorio_annotate_running_on_dynamorio)

#ifdef __cplusplus
extern "C" {
#endif

#pragma auto_inline(off)

DR_DECLARE_ANNOTATION_EXPRESSION(char, dynamorio_annotate_running_on_dynamorio) ()
    DR_WEAK_DECLARATION;

#pragma auto_inline(on)

#ifdef __cplusplus
}
#endif

#endif
