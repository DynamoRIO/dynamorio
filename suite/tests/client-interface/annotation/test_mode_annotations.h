/* ******************************************************
 * Copyright (c) 2014-2020 Google, Inc.  All rights reserved.
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

#ifndef _TEST_MODE_ANNOTATIONS_H_
#define _TEST_MODE_ANNOTATIONS_H_ 1

#include "dr_annotations_asm.h"

#define TEST_ANNOTATION_INIT_MODE(mode) DR_ANNOTATION(test_annotation_init_mode, mode)

#define TEST_ANNOTATION_INIT_CONTEXT(id, name, mode) \
    DR_ANNOTATION(test_annotation_init_context, id, name, mode)

#define TEST_ANNOTATION_GET_MODE(context_id) test_annotation_get_mode(context_id)

#define TEST_ANNOTATION_SET_MODE(context_id, mode, native_version) \
    DR_ANNOTATION_OR_NATIVE(test_annotation_set_mode, native_version, context_id, mode)

#define TEST_ANNOTATION_GET_PC() DR_ANNOTATION(test_annotation_get_pc)

#define TEST_ANNOTATION_GET_CLIENT_VERSION() test_annotation_get_client_version()

#define TEST_ANNOTATION_ROTATE_VALGRIND_HANDLER(phase) \
    DR_ANNOTATION_OR_NATIVE(test_annotation_rotate_valgrind_handler, , phase)

#ifdef __cplusplus
extern "C" {
#endif

DR_DECLARE_ANNOTATION(void, test_annotation_init_mode, (unsigned int mode));

DR_DECLARE_ANNOTATION(void, test_annotation_init_context,
                      (unsigned int id, const char *name, unsigned int initial_mode));

DR_DECLARE_ANNOTATION(unsigned int, test_annotation_get_mode, (unsigned int context_id));

DR_DECLARE_ANNOTATION(void, test_annotation_set_mode,
                      (unsigned int context_id, unsigned int mode));

DR_DECLARE_ANNOTATION(void, test_annotation_get_pc, (void));

DR_DECLARE_ANNOTATION(const char *, test_annotation_get_client_version, (void));

DR_DECLARE_ANNOTATION(void, test_annotation_rotate_valgrind_handler, (int phase));

#ifdef __cplusplus
}
#endif

#endif
