/* **********************************************************
 * Copyright (c) 2016-2018 Google, Inc.  All rights reserved.
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

#include <string.h>
#include <fstream>
#include <queue>
#include <vector>
#include "reader.h"
#include "memref.h"
#include "directory_iterator.h"
#include "trace_entry.h"

#ifndef ZHEX64_FORMAT_STRING
/* We avoid dr_defines.h to keep this code separated and simpler for using with
 * external code.
 */
#    ifdef WINDOWS
#        define ZHEX64_FORMAT_STRING "%016I64x"
#    else
#        if defined(__i386__) || defined(__arm__)
#            define ZHEX64_FORMAT_STRING "%016llx"
#        else
#            define ZHEX64_FORMAT_STRING "%016lx"
#        endif
#    endif
#endif

/* We templatize on the file type itself for specializing for compression and
 * other different types.  An alternative would be to require a std::istream
 * interface and add gzip_istream_t (paralleling gzip_ostream_t used for
 * raw2trace).
 */
template <typename T> class file_reader_t : public reader_t {
public:
    file_reader_t()
    {
    }
    file_reader_t(const std::string &path, int verbosity_in = 0)
        : reader_t(verbosity_in, "[file_reader]")
        , input_path(path)
    {
    }
    explicit file_reader_t(const std::vector<std::string> &path_list,
                           int verbosity_in = 0)
        : reader_t(verbosity_in, "[file_reader]")
        , input_path_list(path_list)
    {
    }
    virtual ~file_reader_t();
    virtual bool
    init()
    {
        at_eof = false;
        if (!open_input_files())
            return false;
        ++*this;
        return true;
    }

    virtual bool
    is_complete();

protected:
    virtual bool
    read_next_thread_entry(size_t thread_index, OUT trace_entry_t *entry, OUT bool *eof);

    virtual bool
    open_single_file(const std::string &path);

    virtual bool
    open_input_files()
    {
        if (!input_path_list.empty()) {
            for (std::string path : input_path_list) {
                if (!open_single_file(path)) {
                    ERRMSG("Failed to open %s\n", path.c_str());
                    return false;
                }
            }
        } else if (directory_iterator_t::is_directory(input_path)) {
            VPRINT(this, 1, "Iterating directory %s\n", input_path.c_str());
            directory_iterator_t end;
            directory_iterator_t iter(input_path);
            if (!iter) {
                ERRMSG("Failed to list directory %s: %s", input_path.c_str(),
                       iter.error_string().c_str());
                return false;
            }
            for (; iter != end; ++iter) {
                std::string fname = *iter;
                if (fname == "." || fname == "..")
                    continue;
                VPRINT(this, 2, "Found file %s\n", fname.c_str());
                if (!open_single_file(input_path + DIRSEP + fname)) {
                    ERRMSG("Failed to open %s\n", fname.c_str());
                    return false;
                }
            }
        } else {
            if (!open_single_file(input_path)) {
                ERRMSG("Failed to open %s\n", input_path.c_str());
                return false;
            }
        }
        if (input_files.empty()) {
            ERRMSG("No thread files found.");
            return false;
        }

        thread_count = input_files.size();
        queues.resize(input_files.size());
        tids.resize(input_files.size());
        timestamps.resize(input_files.size());
        times.resize(input_files.size(), 0);
        // We can't take the address of a vector<bool> element so we use a raw array.
        thread_eof = new bool[input_files.size()];
        memset(thread_eof, 0, input_files.size() * sizeof(*thread_eof));

        // First read the tid and pid entries which precede any timestamps.
        // We hand out the tid to the output on every thread switch, and the pid
        // the very first time for the thread.
        trace_entry_t header, pid;
        for (index = 0; index < input_files.size(); ++index) {
            if (!read_next_thread_entry(index, &header, &thread_eof[index]) ||
                !read_next_thread_entry(index, &tids[index], &thread_eof[index]) ||
                !read_next_thread_entry(index, &pid, &thread_eof[index])) {
                ERRMSG("Failed to read header fields from input file #%zu\n", index);
                return false;
            }
            VPRINT(this, 2, "Read thread #%zd header: ver=%zu, pid=%zu, tid=%zu\n", index,
                   header.addr, pid.addr, tids[index].addr);
            if (header.type != TRACE_TYPE_HEADER || header.addr != TRACE_ENTRY_VERSION ||
                pid.type != TRACE_TYPE_PID || tids[index].type != TRACE_TYPE_THREAD) {
                ERRMSG("Invalid header for input file #%zu\n", index);
                return false;
            }
            // The reader expects us to own the header and pass the tid as
            // the first entry.
            queues[index].push(tids[index]);
            queues[index].push(pid);
        }
        index = input_files.size();

        return true;
    }

    virtual trace_entry_t *
    read_next_entry()
    {
        // We read the thread files simultaneously in lockstep and merge them into
        // a single interleaved stream in timestamp order.
        // When a thread file runs out we leave its times[] entry as 0 and its file at
        // eof.
        while (thread_count > 0) {
            if (index >= input_files.size()) {
                // Pick the next thread by looking for the smallest timestamp.
                uint64_t min_time = 0xffffffffffffffff;
                size_t next_index = 0;
                for (size_t i = 0; i < times.size(); ++i) {
                    if (times[i] == 0 && !thread_eof[i]) {
                        if (!read_next_thread_entry(i, &timestamps[i], &thread_eof[i])) {
                            ERRMSG("Failed to read from input file #%zu\n", i);
                            return nullptr;
                        }
                        if (timestamps[i].type != TRACE_TYPE_MARKER &&
                            timestamps[i].size != TRACE_MARKER_TYPE_TIMESTAMP) {
                            ERRMSG("Missing timestamp entry in input file #%zu\n", i);
                            return nullptr;
                        }
                        times[i] = timestamps[i].addr;
                        VPRINT(this, 3,
                               "Thread #%zu timestamp is @0x" ZHEX64_FORMAT_STRING "\n",
                               i, times[i]);
                    }
                    if (times[i] != 0 && times[i] < min_time) {
                        min_time = times[i];
                        next_index = i;
                    }
                }
                VPRINT(this, 2,
                       "Next thread in timestamp order is #%zu @0x" ZHEX64_FORMAT_STRING
                       "\n",
                       next_index, times[next_index]);
                index = next_index;
                times[index] = 0; // Read from file for this thread's next timestamp.
                // If the queue is not empty, it should contain the initial tid;pid.
                if ((queues[index].empty() ||
                     queues[index].front().type != TRACE_TYPE_THREAD) &&
                    // For a single thread (or already-interleaved file) we do not need
                    // thread entries before each timestamp.
                    input_files.size() > 1)
                    queues[index].push(tids[index]);
                queues[index].push(timestamps[index]);
            }
            if (!queues[index].empty()) {
                entry_copy = queues[index].front();
                queues[index].pop();
                return &entry_copy;
            }
            VPRINT(this, 4, "About to read thread #%zu\n", index);
            if (!read_next_thread_entry(index, &entry_copy, &thread_eof[index])) {
                if (thread_eof[index]) {
                    VPRINT(this, 2, "Thread #%zu at eof\n", index);
                    --thread_count;
                    if (thread_count == 0) {
                        VPRINT(this, 2, "All threads at eof\n");
                        at_eof = true;
                        break;
                    }
                    times[index] = 0;
                    index = input_files.size(); // Request thread scan.
                    continue;
                } else {
                    ERRMSG("Failed to read from input file #%zu\n", index);
                    return nullptr;
                }
            }
            if (entry_copy.type == TRACE_TYPE_MARKER &&
                entry_copy.size == TRACE_MARKER_TYPE_TIMESTAMP) {
                VPRINT(this, 3, "Thread #%zu timestamp 0x" ZHEX64_FORMAT_STRING "\n",
                       index, (uint64_t)entry_copy.addr);
                times[index] = entry_copy.addr;
                timestamps[index] = entry_copy;
                index = input_files.size(); // Request thread scan.
                continue;
            }
            return &entry_copy;
        }
        return nullptr;
    }

private:
    std::string input_path;
    std::vector<std::string> input_path_list;
    std::vector<T> input_files;
    trace_entry_t entry_copy;
    // The current thread we're processing is "index".  If it's set to input_files.size()
    // that means we need to pick a new thread.
    size_t index;
    size_t thread_count;
    std::vector<std::queue<trace_entry_t>> queues;
    std::vector<trace_entry_t> tids;
    std::vector<trace_entry_t> timestamps;
    std::vector<uint64_t> times;
    bool *thread_eof = nullptr;
};

#endif /* _FILE_READER_H_ */
