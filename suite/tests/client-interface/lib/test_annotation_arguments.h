#ifndef _TEST_ANNOTATION_ARGUMENTS_H_
#define _TEST_ANNOTATION_ARGUMENTS_H_ 1

#include "annotation/dynamorio_annotation_asm.h"

#define TEST_ANNOTATION_EIGHT_ARGS(a, b, c, d, e, f, g, h) \
    DR_ANNOTATION_STATEMENT(test_annotation_eight_args, a, b, c, d, e, f, g, h)

#define TEST_ANNOTATION_NINE_ARGS(a, b, c, d, e, f, g, h, i) \
    DR_ANNOTATION_STATEMENT(test_annotation_nine_args, a, b, c, d, e, f, g, h, i)

#define TEST_ANNOTATION_TEN_ARGS(a, b, c, d, e, f, g, h, i, j) \
    DR_ANNOTATION_STATEMENT(test_annotation_ten_args, a, b, c, d, e, f, g, h, i, j)

#ifdef __cplusplus
extern "C" {
#endif

DR_DECLARE_ANNOTATION(void, test_annotation_eight_args) (unsigned int a,
    unsigned int b, unsigned int c, unsigned int d, unsigned int e,
    unsigned int f, unsigned int g, unsigned int h)
    DR_WEAK_DECLARATION;

DR_DECLARE_ANNOTATION(void, test_annotation_nine_args) (unsigned int a,
    unsigned int b, unsigned int c, unsigned int d, unsigned int e,
    unsigned int f, unsigned int g, unsigned int h, unsigned int i)
    DR_WEAK_DECLARATION;

DR_DECLARE_ANNOTATION(void, test_annotation_ten_args) (unsigned int a,
    unsigned int b,
    unsigned int c, unsigned int d, unsigned int e, unsigned int f,
    unsigned int g, unsigned int h, unsigned int i, unsigned int j)
    DR_WEAK_DECLARATION;

#ifdef __cplusplus
}
#endif

#endif

