#ifndef _TEST_MODE_ANNOTATIONS_H_
#define _TEST_MODE_ANNOTATIONS_H_ 1

#include "annotation/dynamorio_annotation_asm.h"

#define TEST_ANNOTATION_INIT_MODE(mode) \
    DR_ANNOTATION_STATEMENT(test_annotation_init_mode, mode)

#define TEST_ANNOTATION_INIT_CONTEXT(id, name, mode) \
    DR_ANNOTATION_STATEMENT(test_annotation_init_context, id, name, mode)

#define TEST_ANNOTATION_SET_MODE(context_id, mode) \
    DR_ANNOTATION_STATEMENT(test_annotation_set_mode, context_id, mode)

#ifdef __cplusplus
extern "C" {
#endif

DR_DECLARE_ANNOTATION(void, test_annotation_init_mode) (unsigned int mode)
    DR_WEAK_DECLARATION;

DR_DECLARE_ANNOTATION(void, test_annotation_init_context) (unsigned int id,
    const char *name, unsigned int initial_mode) DR_WEAK_DECLARATION;

DR_DECLARE_ANNOTATION(void, test_annotation_set_mode) (unsigned int context_id,
    unsigned int mode) DR_WEAK_DECLARATION;

#ifdef __cplusplus
}
#endif

#endif

