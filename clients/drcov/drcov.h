/* **********************************************************
 * Copyright (c) 2013 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* used by both drcov.c and drcov2lcov.c */
#ifndef _DRCOV_H_
#define _DRCOV_H_ 1

#include "dr_api.h"

/* The bb_entry_t is used by both drcov client and post processing drcov2lcov.
 * It has different sizes and members with and without CBR_COVERAGE.
 * We use different DRCOV_VERSION to make sure the drcov2lcov process the
 * right log file generated from corrsponding drcov client.
 */
#ifdef CBR_COVERAGE
# define DRCOV_VERSION 2
#else
# define DRCOV_VERSION 1
#endif

/* data structure used in drcov.log */
typedef struct _bb_entry_t {
    uint   start;      /* offset of bb start from the image base */
    ushort size;
    ushort mod_id;
#ifdef CBR_COVERAGE
    uint   cbr_tgt;    /* offset of cbr target from the image base */
    bool   trace;
    ushort num_instrs;
#endif
} bb_entry_t;

#endif /* _DRCOV_H_ */
