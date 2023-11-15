/* **********************************************************
 * Copyright (c) 2016-2023 Google, Inc.  All rights reserved.
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

/* Functions and structs extracted from raw2trace.h for sharing with the tracer.
 */

#ifndef _RAW2TRACE_SHARED_H_
#define _RAW2TRACE_SHARED_H_ 1

/**
 * @file drmemtrace/raw2trace_shared.h
 * @brief DrMemtrace routines and structs shared between raw2trace and tracer.
 */

#include "dr_api.h"
#include "drmemtrace.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

#define OUTFILE_SUFFIX "raw"
#ifdef BUILD_PT_POST_PROCESSOR
#    define OUTFILE_SUFFIX_PT "raw.pt"
#endif
#ifdef HAS_ZLIB
#    define OUTFILE_SUFFIX_GZ "raw.gz"
#    define OUTFILE_SUFFIX_ZLIB "raw.zlib"
#endif
#ifdef HAS_SNAPPY
#    define OUTFILE_SUFFIX_SZ "raw.sz"
#endif
#ifdef HAS_LZ4
#    define OUTFILE_SUFFIX_LZ4 "raw.lz4"
#endif
#define OUTFILE_SUBDIR "raw"
#define WINDOW_SUBDIR_PREFIX "window"
#define WINDOW_SUBDIR_FORMAT "window.%04zd" /* ptr_int_t is the window number type. */
#define WINDOW_SUBDIR_FIRST "window.0000"
#define TRACE_SUBDIR "trace"

/**
 * Functions for decoding and verifying raw memtrace data headers.
 */
struct trace_metadata_reader_t {
    static bool
    is_thread_start(const offline_entry_t *entry, DR_PARAM_OUT std::string *error,
                    DR_PARAM_OUT int *version,
                    DR_PARAM_OUT offline_file_type_t *file_type);
    static std::string
    check_entry_thread_start(const offline_entry_t *entry);
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _RAW2TRACE_SHARED_H_ */
