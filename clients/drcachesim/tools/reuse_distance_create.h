/* **********************************************************
 * Copyright (c) 2018 Google, Inc.  All rights reserved.
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

/* reuse-distance tool creation */

#ifndef _REUSE_DISTANCE_CREATE_H_
#define _REUSE_DISTANCE_CREATE_H_ 1

#include "analysis_tool.h"

/**
 * @file drmemtrace/reuse_distance_create.h
 * @brief DrMemtrace reuse distance tool creation.
 */

/**
 * The options for reuse_distance_tool_create().
 * The options are currently documented in \ref sec_drcachesim_ops.
 */
// These options are currently documented in ../common/options.cpp.
struct reuse_distance_knobs_t {
    reuse_distance_knobs_t()
        : line_size(64)
        , report_histogram(false)
        , distance_threshold(100)
        , report_top(10)
        , skip_list_distance(500)
        , verify_skip(false)
        , verbose(0)
    {
    }
    unsigned int line_size;
    bool report_histogram;
    unsigned int distance_threshold;
    unsigned int report_top;
    unsigned int skip_list_distance;
    bool verify_skip;
    unsigned int verbose;
};

/** Creates an analysis tool which computes reuse distance. */
analysis_tool_t *
reuse_distance_tool_create(const reuse_distance_knobs_t &knobs);

#endif /* _REUSE_DISTANCE_CREATE_H_ */
