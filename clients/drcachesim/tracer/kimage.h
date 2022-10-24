/* **********************************************************
 * Copyright (c) 2022 Google, Inc.  All rights reserved.
 * **********************************************************/

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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
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

/* kimage.h: kimage is a file that contains all kernel code segments. It is used to decode
 * kernel PT trace. The format of the file is:
 *  ------------------------------------------------
 * | An instance of kimage_hdr_t                    |
 *  ------------------------------------------------
 * | A table of kimage_code_segment_hdr_t instances |
 *  ------------------------------------------------
 * | Binary data for all code segments              |
 *  ------------------------------------------------
 */

#ifndef _KIMAGE_H_
#define _KIMAGE_H_ 1

#include <inttypes.h>

/* This struct type defines the file header that stores all kernel code segments. */
struct kimage_hdr_t {
    /* The number of code segments in kimage. */
    uint64_t code_segments_num;
    /* The offset of the first code segment in . */
    uint64_t code_segments_offset;
};

/* This struct type defines every kernel code segment's header. */
struct kimage_code_segment_hdr_t {
    /* The offset of the code segment in the kimage. */
    uint64_t offset;
    /* The length of the code segment. */
    uint64_t len;
    /* This is the virtuall address that the code segment is loaded to in kernel memory.
     */
    uint64_t vaddr;
};

#endif
