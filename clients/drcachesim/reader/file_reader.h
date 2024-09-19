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

/* file_reader: obtains memory streams from DR clients running in
 * application processes and presents them via an iterator interface
 * to the cache simulator.
 */

#ifndef _FILE_READER_H_
#define _FILE_READER_H_ 1

#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include <fstream>
#include <queue>
#include <string>
#include <vector>

#include "directory_iterator.h"
#include "memref.h"
#include "reader.h"
#include "trace_entry.h"
#include "utils.h"

namespace dynamorio {
namespace drmemtrace {

#ifndef ZHEX64_FORMAT_STRING
/* We avoid dr_defines.h to keep this code separated and simpler for using with
 * external code.
 */
#    ifdef WINDOWS
#        define ZHEX64_FORMAT_STRING "%016I64x"
#    else
#        define ZHEX64_FORMAT_STRING "%" PRIx64
#    endif
#endif

/**
 * We templatize on the file type itself for specializing for compression and
 * other different types.  An alternative would be to require a std::istream
 * interface and add gzip_istream_t (paralleling gzip_ostream_t used for
 * raw2trace).
 */
template <typename T> class file_reader_t : public reader_t {
public:
    file_reader_t();
    file_reader_t(const std::string &path, int verbosity = 0)
        : reader_t(verbosity, "[file_reader]")
        , input_path_(path)
    {
        online_ = false;
    }
    virtual ~file_reader_t();
    bool
    init() override
    {
        at_eof_ = false;
        if (!open_input_file())
            return false;
        ++*this;
        return true;
    }

    std::string
    get_stream_name() const override
    {
        size_t ind = input_path_.find_last_of(DIRSEP);
        if (ind == std::string::npos)
            return input_path_;
        return input_path_.substr(ind + 1);
    }

protected:
    trace_entry_t *
    read_next_entry() override;

    virtual bool
    open_single_file(const std::string &path);

    virtual bool
    open_input_file()
    {
        if (!open_single_file(input_path_)) {
            ERRMSG("Failed to open %s\n", input_path_.c_str());
            return false;
        }

        // First read the tid and pid entries which precede any timestamps.
        // We hand out the tid to the output on every thread switch, and the pid
        // the very first time for the thread.
        trace_entry_t *entry;
        trace_entry_t header = {}, pid = {}, tid = {};
        entry = read_next_entry();
        if (entry == nullptr || entry->type != TRACE_TYPE_HEADER) {
            ERRMSG("Invalid header\n");
            return false;
        }
        header = *entry;
        // We can handle the older version 1 as well which simply omits the
        // early marker with the arch tag, and version 2 which only differs wrt
        // TRACE_MARKER_TYPE_KERNEL_EVENT.
        if (entry->addr > TRACE_ENTRY_VERSION) {
            ERRMSG("Cannot handle version #%zu (expect version <= #%u)\n", entry->addr,
                   TRACE_ENTRY_VERSION);
            return false;
        }
        // Read the meta entries until we hit the pid.
        // We want to pass the tid+pid to the reader *before* any markers,
        // even though markers can precede the tid+pid in the file, in particular
        // for legacy traces.
        std::queue<trace_entry_t> marker_queue;
        while ((entry = read_next_entry()) != nullptr) {
            if (entry->type == TRACE_TYPE_PID) {
                // We assume the pid entry is after the tid.
                pid = *entry;
                break;
            } else if (entry->type == TRACE_TYPE_THREAD)
                tid = *entry;
            else if (entry->type == TRACE_TYPE_MARKER)
                marker_queue.push(*entry);
            else {
                ERRMSG("Unexpected trace sequence\n");
                return false;
            }
        }
        VPRINT(this, 2, "Read header: ver=%zu, pid=%zu, tid=%zu\n", header.addr, pid.addr,
               tid.addr);
        // The reader expects us to own the header and pass the tid as
        // the first entry.
        queue_.push(tid);
        queue_.push(pid);
        while (!marker_queue.empty()) {
            queue_.push(marker_queue.front());
            marker_queue.pop();
        }
        return true;
    }

    // Provided so that instantiations can specialize.
    reader_t &
    skip_instructions(uint64_t instruction_count) override
    {
        return reader_t::skip_instructions(instruction_count);
    }

    // Protected for access by mock_file_reader_t.
    T input_file_;

private:
    std::string input_path_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _FILE_READER_H_ */
