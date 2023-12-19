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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* A mock reader that iterates over a vector of trace_entry_t, for tests. */

#ifndef _MOCK_READER_H_
#define _MOCK_READER_H_ 1

#include <vector>

#include "reader.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

// A mock reader that iterates over a vector of records.
class mock_reader_t : public reader_t {
public:
    mock_reader_t() = default;
    explicit mock_reader_t(const std::vector<trace_entry_t> &trace)
        : trace_(trace)
    {
        verbosity_ = 3;
    }
    bool
    init() override
    {
        at_eof_ = false;
        ++*this;
        return true;
    }
    trace_entry_t *
    read_next_entry() override
    {
        trace_entry_t *entry = read_queued_entry();
        if (entry != nullptr)
            return entry;
        ++index_;
        if (index_ >= static_cast<int>(trace_.size())) {
            at_eof_ = true;
            return nullptr;
        }
        return &trace_[index_];
    }
    std::string
    get_stream_name() const override
    {
        return "";
    }

private:
    std::vector<trace_entry_t> trace_;
    int index_ = -1;
};

static inline trace_entry_t
make_memref(addr_t addr, trace_type_t type = TRACE_TYPE_READ, unsigned short size = 1)
{
    trace_entry_t entry;
    entry.type = static_cast<unsigned short>(type);
    entry.size = size;
    entry.addr = addr;
    return entry;
}

static inline trace_entry_t
make_instr(addr_t pc, trace_type_t type = TRACE_TYPE_INSTR, unsigned short size = 1)
{
    trace_entry_t entry;
    entry.type = static_cast<unsigned short>(type);
    entry.size = size;
    entry.addr = pc;
    return entry;
}

static inline trace_entry_t
make_exit(memref_tid_t tid)
{
    trace_entry_t entry;
    entry.type = TRACE_TYPE_THREAD_EXIT;
    entry.addr = static_cast<addr_t>(tid);
    return entry;
}

static inline trace_entry_t
make_header(int version)
{
    trace_entry_t entry;
    entry.type = TRACE_TYPE_HEADER;
    entry.addr = version;
    return entry;
}

static inline trace_entry_t
make_footer()
{
    trace_entry_t entry;
    entry.type = TRACE_TYPE_FOOTER;
    return entry;
}

static inline trace_entry_t
make_version(int version)
{
    trace_entry_t entry;
    entry.type = TRACE_TYPE_MARKER;
    entry.size = TRACE_MARKER_TYPE_VERSION;
    entry.addr = version;
    return entry;
}

static inline trace_entry_t
make_thread(memref_tid_t tid)
{
    trace_entry_t entry;
    entry.type = TRACE_TYPE_THREAD;
    entry.addr = static_cast<addr_t>(tid);
    return entry;
}

static inline trace_entry_t
make_pid(memref_pid_t pid)
{
    trace_entry_t entry;
    entry.type = TRACE_TYPE_PID;
    entry.addr = static_cast<addr_t>(pid);
    return entry;
}

static inline trace_entry_t
make_timestamp(uint64_t timestamp)
{
    trace_entry_t entry;
    entry.type = TRACE_TYPE_MARKER;
    entry.size = TRACE_MARKER_TYPE_TIMESTAMP;
    entry.addr = static_cast<addr_t>(timestamp);
    return entry;
}

static inline trace_entry_t
make_marker(trace_marker_type_t type, uintptr_t value)
{
    trace_entry_t entry;
    entry.type = TRACE_TYPE_MARKER;
    entry.size = static_cast<unsigned short>(type);
    entry.addr = value;
    return entry;
}

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _MOCK_READER_H_ */
