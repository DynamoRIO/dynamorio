/* **********************************************************
 * Copyright (c) 2016-2024 Google, Inc.  All rights reserved.
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

#include <list>

#include "dr_api.h"
#include "drmemtrace.h"
#include "reader.h"
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
#define TRACE_CHUNK_PREFIX "chunk."

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

// We need to determine the memref_t record count for inserting a marker with
// that count at the start of each chunk.
class memref_counter_t : public reader_t {
public:
    bool
    init() override
    {
        return true;
    }
    trace_entry_t *
    read_next_entry() override
    {
        return nullptr;
    };
    std::string
    get_stream_name() const override
    {
        return "";
    }
    int
    entry_memref_count(const trace_entry_t *entry)
    {
        // Mirror file_reader_t::open_input_file().
        // In particular, we need to skip TRACE_TYPE_HEADER and to pass the
        // tid and pid to the reader before the 2 markers in front of them.
        if (!saw_pid_) {
            if (entry->type == TRACE_TYPE_HEADER)
                return 0;
            else if (entry->type == TRACE_TYPE_THREAD) {
                list_.push_front(*entry);
                return 0;
            } else if (entry->type != TRACE_TYPE_PID) {
                list_.push_back(*entry);
                return 0;
            }
            saw_pid_ = true;
            auto it = list_.begin();
            ++it;
            list_.insert(it, *entry);
            int count = 0;
            for (auto &next : list_) {
                input_entry_ = &next;
                if (process_input_entry())
                    ++count;
            }
            return count;
        }
        if (entry->type == TRACE_TYPE_FOOTER)
            return 0;
        input_entry_ = const_cast<trace_entry_t *>(entry);
        return process_input_entry() ? 1 : 0;
    }
    unsigned char *
    get_decode_pc(addr_t orig_pc)
    {
        if (encodings_.find(orig_pc) == encodings_.end())
            return nullptr;
        return encodings_[orig_pc].bits;
    }
    void
    set_core_sharded(bool core_sharded)
    {
        core_sharded_ = core_sharded;
    }

private:
    bool saw_pid_ = false;
    std::list<trace_entry_t> list_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _RAW2TRACE_SHARED_H_ */
