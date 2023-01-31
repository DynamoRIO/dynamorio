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
 * application processes and presents them via an interator interface
 * to the cache simulator.
 */

#ifndef _FILE_READER_H_
#define _FILE_READER_H_ 1

#include <inttypes.h>
#include <string.h>
#include <fstream>
#include <queue>
#include <vector>
#include "reader.h"
#include "memref.h"
#include "directory_iterator.h"
#include "trace_entry.h"
#include "utils.h"

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
    file_reader_t()
    {
        online_ = false;
    }
    file_reader_t(const std::string &path, int verbosity = 0)
        : reader_t(verbosity, "[file_reader]")
        , input_path_(path)
    {
        online_ = false;
    }
    explicit file_reader_t(const std::vector<std::string> &path_list, int verbosity = 0)
        : reader_t(verbosity, "[file_reader]")
        , input_path_list_(path_list)
    {
        online_ = false;
    }
    virtual ~file_reader_t();
    bool
    init() override
    {
        at_eof_ = false;
        if (!open_input_files())
            return false;
        ++*this;
        return true;
    }

    virtual bool
    is_complete();

    std::string
    get_stream_name() const override
    {
        size_t ind = input_path_.find_last_of(DIRSEP);
        if (ind == std::string::npos)
            return input_path_;
        return input_path_.substr(ind + 1);
    }

    reader_t &
    skip_instructions(uint64_t instruction_count) override
    {
        if (instruction_count == 0)
            return *this;
        if (input_files_.size() > 1) {
            // TODO i#5538: For fast thread-interleaved (whether serial here or the
            // forthcoming per-cpu iteration) we need to read in the schedule file(s)
            // that raw2trace writes out so that we can compute how far to separately
            // fast-skip in each interleaved thread by calling the per-thread version.
            // We'll also need to update the memref pid+tid state since we're not
            // repeating top headers in every thread after a skip.  For now this is a
            // slow linear walk.
            return reader_t::skip_instructions(instruction_count);
        }
        // If the user asks to skip from the very start, we still need to find the chunk
        // count marker and drain the header queue and populate the stream header values.
        // XXX: We assume the page size is the final header; it is complex to wait for
        // the timestamp as we don't want to read it yet.
        while (page_size_ == 0) {
            input_entry_ = read_next_entry();
            process_input_entry();
        }
        if (!queues_[0].empty())
            ERRMSG("Failed to drain header queue\n");
        bool eof = false;
        if (!skip_thread_instructions(0, instruction_count, &eof) || eof)
            at_eof_ = true;
        return *this;
    }

protected:
    bool
    read_next_thread_entry(size_t thread_index, OUT trace_entry_t *entry,
                           OUT bool *eof) override;

    virtual bool
    open_single_file(const std::string &path);

    virtual bool
    open_input_files()
    {
        if (!input_path_list_.empty()) {
            for (const std::string &path : input_path_list_) {
                if (!open_single_file(path)) {
                    ERRMSG("Failed to open %s\n", path.c_str());
                    return false;
                }
            }
        } else if (directory_iterator_t::is_directory(input_path_)) {
            VPRINT(this, 1, "Iterating directory %s\n", input_path_.c_str());
            directory_iterator_t end;
            directory_iterator_t iter(input_path_);
            if (!iter) {
                ERRMSG("Failed to list directory %s: %s", input_path_.c_str(),
                       iter.error_string().c_str());
                return false;
            }
            for (; iter != end; ++iter) {
                std::string fname = *iter;
                if (fname == "." || fname == ".." ||
                    starts_with(fname, DRMEMTRACE_SERIAL_SCHEDULE_FILENAME) ||
                    fname == DRMEMTRACE_CPU_SCHEDULE_FILENAME)
                    continue;
                // Skip the auxiliary files.
                if (fname == DRMEMTRACE_MODULE_LIST_FILENAME ||
                    fname == DRMEMTRACE_FUNCTION_LIST_FILENAME ||
                    fname == DRMEMTRACE_ENCODING_FILENAME)
                    continue;
                VPRINT(this, 2, "Found file %s\n", fname.c_str());
                if (!open_single_file(input_path_ + DIRSEP + fname)) {
                    ERRMSG("Failed to open %s\n", fname.c_str());
                    return false;
                }
            }
        } else {
            if (!open_single_file(input_path_)) {
                ERRMSG("Failed to open %s\n", input_path_.c_str());
                return false;
            }
        }
        if (input_files_.empty()) {
            ERRMSG("No thread files found.");
            return false;
        }

        thread_count_ = input_files_.size();
        queues_.resize(input_files_.size());
        tids_.resize(input_files_.size());
        timestamps_.resize(input_files_.size());
        times_.resize(input_files_.size(), 0);
        // We can't take the address of a vector<bool> element so we use a raw array.
        thread_eof_ = new bool[input_files_.size()];
        memset(thread_eof_, 0, input_files_.size() * sizeof(*thread_eof_));

        // First read the tid and pid entries which precede any timestamps_.
        // We hand out the tid to the output on every thread switch, and the pid
        // the very first time for the thread.
        trace_entry_t header, next, pid = {};
        for (index_ = 0; index_ < input_files_.size(); ++index_) {
            if (!read_next_thread_entry(index_, &header, &thread_eof_[index_]) ||
                header.type != TRACE_TYPE_HEADER) {
                ERRMSG("Invalid header for input file #%zu\n", index_);
                return false;
            }
            // We can handle the older version 1 as well which simply omits the
            // early marker with the arch tag, and version 2 which only differs wrt
            // TRACE_MARKER_TYPE_KERNEL_EVENT..
            if (header.addr > TRACE_ENTRY_VERSION) {
                ERRMSG(
                    "Cannot handle version #%zu (expect version <= #%u) for input file "
                    "#%zu\n",
                    header.addr, TRACE_ENTRY_VERSION, index_);
                return false;
            }
            // Read the meta entries until we hit the pid.
            // We want to pass the tid+pid to the reader *before* any markers,
            // even though 2 markers preced the tid+pid in the file.
            std::queue<trace_entry_t> marker_queue;
            while (read_next_thread_entry(index_, &next, &thread_eof_[index_])) {
                if (next.type == TRACE_TYPE_PID) {
                    // We assume the pid entry is the last, right before the timestamp.
                    pid = next;
                    break;
                } else if (next.type == TRACE_TYPE_THREAD)
                    tids_[index_] = next;
                else if (next.type == TRACE_TYPE_MARKER)
                    marker_queue.push(next);
                else {
                    ERRMSG("Unexpected trace sequence for input file #%zu\n", index_);
                    return false;
                }
            }
            VPRINT(this, 2, "Read thread #%zd header: ver=%zu, pid=%zu, tid=%zu\n",
                   index_, header.addr, pid.addr, tids_[index_].addr);
            // The reader expects us to own the header and pass the tid as
            // the first entry.
            queues_[index_].push(tids_[index_]);
            queues_[index_].push(pid);
            while (!marker_queue.empty()) {
                queues_[index_].push(marker_queue.front());
                marker_queue.pop();
            }
        }
        index_ = input_files_.size();

        return true;
    }

    trace_entry_t *
    read_next_entry() override
    {
        // We read the thread files simultaneously in lockstep and merge them into
        // a single interleaved stream in timestamp order.
        // When a thread file runs out we leave its times_[] entry as 0 and its file at
        // eof.
        while (thread_count_ > 0) {
            if (index_ >= input_files_.size()) {
                // Pick the next thread by looking for the smallest timestamp.
                uint64_t min_time = 0xffffffffffffffff;
                size_t next_index = 0;
                for (size_t i = 0; i < times_.size(); ++i) {
                    if (times_[i] == 0 && !thread_eof_[i]) {
                        if (!read_next_thread_entry(i, &timestamps_[i],
                                                    &thread_eof_[i])) {
                            ERRMSG("Failed to read from input file #%zu\n", i);
                            return nullptr;
                        }
                        if (timestamps_[i].type != TRACE_TYPE_MARKER &&
                            timestamps_[i].size != TRACE_MARKER_TYPE_TIMESTAMP) {
                            ERRMSG("Missing timestamp entry in input file #%zu\n", i);
                            return nullptr;
                        }
                        times_[i] = timestamps_[i].addr;
                        VPRINT(this, 3,
                               "Thread #%zu timestamp is @0x" ZHEX64_FORMAT_STRING "\n",
                               i, times_[i]);
                    }
                    if (times_[i] != 0 && times_[i] < min_time) {
                        min_time = times_[i];
                        next_index = i;
                    }
                }
                VPRINT(this, 2,
                       "Next thread in timestamp order is #%zu @0x" ZHEX64_FORMAT_STRING
                       "\n",
                       next_index, times_[next_index]);
                index_ = next_index;
                times_[index_] = 0; // Read from file for this thread's next timestamp.
                // If the queue is not empty, it should contain the initial tid;pid.
                if ((queues_[index_].empty() ||
                     queues_[index_].front().type != TRACE_TYPE_THREAD) &&
                    // For a single thread (or already-interleaved file) we do not need
                    // thread entries before each timestamp.
                    input_files_.size() > 1)
                    queues_[index_].push(tids_[index_]);
                queues_[index_].push(timestamps_[index_]);
            }
            if (!queues_[index_].empty()) {
                entry_copy_ = queues_[index_].front();
                queues_[index_].pop();
                return &entry_copy_;
            }
            VPRINT(this, 4, "About to read thread #%zu\n", index_);
            if (!read_next_thread_entry(index_, &entry_copy_, &thread_eof_[index_])) {
                if (thread_eof_[index_]) {
                    VPRINT(this, 2, "Thread #%zu at eof\n", index_);
                    --thread_count_;
                    if (thread_count_ == 0) {
                        VPRINT(this, 2, "All threads at eof\n");
                        at_eof_ = true;
                        break;
                    }
                    times_[index_] = 0;
                    index_ = input_files_.size(); // Request thread scan.
                    continue;
                } else {
                    ERRMSG("Failed to read from input file #%zu\n", index_);
                    return nullptr;
                }
            }
            if (entry_copy_.type == TRACE_TYPE_MARKER &&
                entry_copy_.size == TRACE_MARKER_TYPE_TIMESTAMP) {
                VPRINT(this, 3, "Thread #%zu timestamp 0x" ZHEX64_FORMAT_STRING "\n",
                       index_, (uint64_t)entry_copy_.addr);
                times_[index_] = entry_copy_.addr;
                timestamps_[index_] = entry_copy_;
                index_ = input_files_.size(); // Request thread scan.
                continue;
            }
            return &entry_copy_;
        }
        return nullptr;
    }

    virtual bool
    skip_thread_instructions(size_t thread_index, uint64_t instruction_count,
                             OUT bool *eof)
    {
        // Default implementation for file types that have no fast seeking and must do a
        // linear walk.
        uint64_t stop_count_ = cur_instr_count_ + instruction_count + 1;
        while (cur_instr_count_ < stop_count_) {
            if (!read_next_thread_entry(thread_index, &entry_copy_, eof))
                return false;
            // Update core state.
            input_entry_ = &entry_copy_;
            process_input_entry();
            // TODO i#5538: Remember the last timestamp+cpu and insert it; share
            // code with the zipfile reader.
        }
        return true;
    }

    // Protected for access by mock_file_reader_t.
    std::vector<T> input_files_;

private:
    std::string input_path_;
    std::vector<std::string> input_path_list_;
    trace_entry_t entry_copy_;
    // The current thread we're processing is "index".  If it's set to input_files_.size()
    // that means we need to pick a new thread.
    size_t index_;
    size_t thread_count_;
    std::vector<std::queue<trace_entry_t>> queues_;
    std::vector<trace_entry_t> tids_;
    std::vector<trace_entry_t> timestamps_;
    std::vector<uint64_t> times_;
    bool *thread_eof_ = nullptr;
};

#endif /* _FILE_READER_H_ */
