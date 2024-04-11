/* **********************************************************
 * Copyright (c) 2024 Google, Inc.  All rights reserved.
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

#include "record_view.h"

#include <inttypes.h>
#include <stdint.h>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef HAS_ZLIB
#    include "common/gzip_ostream.h"
#endif
#ifdef HAS_ZIP
#    include "common/zipfile_ostream.h"
#endif
#include "memref.h"
#include "memtrace_stream.h"
#include "raw2trace_shared.h"
#include "trace_entry.h"
#include "utils.h"

#undef VPRINT
#ifdef DEBUG
#    define VPRINT(reader, level, ...)                            \
        do {                                                      \
            if ((reader)->verbosity_ >= (level)) {                \
                fprintf(stderr, "%s ", (reader)->output_prefix_); \
                fprintf(stderr, __VA_ARGS__);                     \
            }                                                     \
        } while (0)
// clang-format off
#    define UNUSED(x) /* nothing */
// clang-format on
#else
#    define VPRINT(reader, level, ...) /* nothing */
#    define UNUSED(x) ((void)(x))
#endif

namespace dynamorio {
namespace drmemtrace {

record_analysis_tool_t *
record_view_tool_create(void)
{
    return new dynamorio::drmemtrace::record_view_t();
}

record_view_t::record_view_t()
{
}

record_view_t::~record_view_t()
{
}

bool
record_view_t::parallel_shard_supported()
{
    return false;
}

std::string
record_view_t::initialize_shard_type(shard_type_t shard_type)
{
    return "";
}

void *
record_view_t::parallel_shard_init_stream(int shard_index, void *worker_data,
                                          memtrace_stream_t *shard_stream)
{
    return shard_stream;
}

bool
record_view_t::parallel_shard_exit(void *shard_data)
{
    return true;
}

std::string
record_view_t::parallel_shard_error(void *shard_data)
{
    return "";
}

bool
record_view_t::parallel_shard_memref(void *shard_data, const trace_entry_t &entry)
{
    const char *trace_type_name = trace_type_names[entry.type];
    std::cerr << std::string(trace_type_name) << "\n";

    return true;
}

bool
record_view_t::process_memref(const trace_entry_t &entry)
{
    return parallel_shard_memref(NULL, entry);
}

bool
record_view_t::print_results()
{
    return true;
}

} // namespace drmemtrace
} // namespace dynamorio
