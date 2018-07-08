/* ******************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
 * ******************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef _TEST_ANNOTATION_ARGUMENTS_H_
#define _TEST_ANNOTATION_ARGUMENTS_H_ 1

#include "dr_annotations_asm.h"

#define TEST_ANNOTATION_TWO_ARGS(a, b, native_version) \
    DR_ANNOTATION_OR_NATIVE(test_annotation_two_args, native_version, a, b)

#define TEST_ANNOTATION_THREE_ARGS(a, b, c) test_annotation_three_args(a, b, c)

#define TEST_ANNOTATION_EIGHT_ARGS(a, b, c, d, e, f, g, h) \
    DR_ANNOTATION(test_annotation_eight_args, a, b, c, d, e, f, g, h)

#define TEST_ANNOTATION_NINE_ARGS(a, b, c, d, e, f, g, h, i) \
    DR_ANNOTATION(test_annotation_nine_args, a, b, c, d, e, f, g, h, i)

#define TEST_ANNOTATION_TEN_ARGS(a, b, c, d, e, f, g, h, i, j) \
    DR_ANNOTATION(test_annotation_ten_args, a, b, c, d, e, f, g, h, i, j)

#ifdef __cplusplus
extern "C" {
#endif

DR_DECLARE_ANNOTATION(void, test_annotation_two_args, (unsigned int a, unsigned int b));

DR_DECLARE_ANNOTATION(int, test_annotation_three_args,
                      (unsigned int a, unsigned int b, unsigned int c));

DR_DECLARE_ANNOTATION(void, test_annotation_eight_args,
                      (unsigned int a, unsigned int b, unsigned int c, unsigned int d,
                       unsigned int e, unsigned int f, unsigned int g, unsigned int h));

DR_DECLARE_ANNOTATION(void, test_annotation_nine_args,
                      (unsigned int a, unsigned int b, unsigned int c, unsigned int d,
                       unsigned int e, unsigned int f, unsigned int g, unsigned int h,
                       unsigned int i));

DR_DECLARE_ANNOTATION(void, test_annotation_ten_args,
                      (unsigned int a, unsigned int b, unsigned int c, unsigned int d,
                       unsigned int e, unsigned int f, unsigned int g, unsigned int h,
                       unsigned int i, unsigned int j));

#ifdef __cplusplus
}
#endif

#endif
