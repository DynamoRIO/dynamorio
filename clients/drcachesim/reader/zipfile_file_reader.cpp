/* **********************************************************
 * Copyright (c) 2017-2022 Google, Inc.  All rights reserved.
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

#include "zipfile_file_reader.h"
#include <inttypes.h>

// We use minizip, which is in the contrib/minizip directory in the zlib
// sources.  The docs are the header files:
// https://github.com/madler/zlib/blob/master/contrib/minizip/unzip.h

/* clang-format off */ /* (make vera++ newline-after-type check happy) */
template <>
/* clang-format on */
file_reader_t<zipfile_reader_t>::~file_reader_t<zipfile_reader_t>()
{
    for (auto &zipfile : input_files_)
        unzClose(zipfile.file);
    delete[] thread_eof_;
}

template <>
bool
file_reader_t<zipfile_reader_t>::open_single_file(const std::string &path)
{
    unzFile file = unzOpen(path.c_str());
    if (file == nullptr)
        return false;
    input_files_.emplace_back(file);
    if (unzGoToFirstFile(file) != UNZ_OK || unzOpenCurrentFile(file) != UNZ_OK)
        return false;
    VPRINT(this, 1, "Opened input file %s\n", path.c_str());
    return true;
}

template <>
bool
file_reader_t<zipfile_reader_t>::read_next_thread_entry(size_t thread_index,
                                                        OUT trace_entry_t *entry,
                                                        OUT bool *eof)
{
    *eof = false;
    zipfile_reader_t *zipfile = &input_files_[thread_index];
    if (zipfile->cur_buf >= zipfile->max_buf) {
        int num_read =
            unzReadCurrentFile(zipfile->file, zipfile->buf, sizeof(zipfile->buf));
        if (num_read == 0) {
#ifdef DEBUG
            if (verbosity_ >= 3) {
                char name[128];
                name[0] = '\0'; /* Just in case. */
                // This call is expensive if we do it every time.
                unzGetCurrentFileInfo64(zipfile->file, nullptr, name, sizeof(name),
                                        nullptr, 0, nullptr, 0);
                VPRINT(this, 3,
                       "Thread #%zd hit end of component %s; opening next component\n",
                       thread_index, name);
            }
#endif
            // read_next_entry() stores the last entry into entry_copy_.
            if ((entry_copy_.type != TRACE_TYPE_MARKER ||
                 entry_copy_.size != TRACE_MARKER_TYPE_CHUNK_FOOTER) &&
                entry_copy_.type != TRACE_TYPE_FOOTER) {
                VPRINT(this, 1, "Chunk is missing footer: truncation detected\n");
                return false;
            }
            if (unzCloseCurrentFile(zipfile->file) != UNZ_OK)
                return false;
            int res = unzGoToNextFile(zipfile->file);
            if (res != UNZ_OK) {
                if (res == UNZ_END_OF_LIST_OF_FILE) {
                    VPRINT(this, 2, "Thread #%zd hit EOF\n", thread_index);
                    *eof = true;
                }
                return false;
            }
            if (unzOpenCurrentFile(zipfile->file) != UNZ_OK)
                return false;
            num_read =
                unzReadCurrentFile(zipfile->file, zipfile->buf, sizeof(zipfile->buf));
        }
        if (num_read < static_cast<int>(sizeof(*entry))) {
            VPRINT(this, 1, "Thread #%zd failed to read: returned %d\n", thread_index,
                   num_read);
            return false;
        }
        zipfile->cur_buf = zipfile->buf;
        zipfile->max_buf = zipfile->buf + (num_read / sizeof(*zipfile->max_buf));
    }
    *entry = *zipfile->cur_buf;
    ++zipfile->cur_buf;
    VPRINT(this, 4, "Read from thread #%zd: type=%d, size=%d, addr=%zu\n", thread_index,
           entry->type, entry->size, entry->addr);
    return true;
}

template <>
bool
file_reader_t<zipfile_reader_t>::is_complete()
{
    // We could call unzeof() but we need the thread index.
    // XXX: We should remove or change this interface.  It is not used now.
    return false;
}

template <>
bool
file_reader_t<zipfile_reader_t>::skip_thread_instructions(size_t thread_index,
                                                          uint64_t instruction_count,
                                                          OUT bool *eof)
{
    if (instruction_count == 0)
        return true;
    VPRINT(this, 2, "Thread #%zd skipping %" PRIi64 " instrs\n", thread_index,
           instruction_count);
    trace_entry_t timestamp = {};
    trace_entry_t cpu = {};
    const memref_t &memref = **this;
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP) {
        timestamp = entry_copy_;
    }
    zipfile_reader_t *zipfile = &input_files_[thread_index];
    // We assume our unzGoToNextFile loop is plenty performant and we don't need to
    // know the chunk names to use with a single unzLocateFile.
    uint64_t stop_count_ = cur_instr_count_ + instruction_count + 1;
    VPRINT(this, 2,
           "stop=%" PRIi64 " cur=%" PRIi64 " chunk=%" PRIi64 " est=%" PRIi64 "\n",
           stop_count_, cur_instr_count_, chunk_instr_count_,
           cur_instr_count_ +
               (chunk_instr_count_ - (cur_instr_count_ % chunk_instr_count_)));
    // First, quickly skip over chunks to reach the chunk containing the target.
    while (cur_instr_count_ +
               (chunk_instr_count_ - (cur_instr_count_ % chunk_instr_count_)) <
           stop_count_) {
        if (unzCloseCurrentFile(zipfile->file) != UNZ_OK)
            return false;
        int res = unzGoToNextFile(zipfile->file);
        if (res != UNZ_OK) {
            if (res == UNZ_END_OF_LIST_OF_FILE) {
                VPRINT(this, 2, "Thread #%zd hit EOF\n", thread_index);
                *eof = true;
            }
            return false;
        }
        if (unzOpenCurrentFile(zipfile->file) != UNZ_OK)
            return false;
        cur_instr_count_ += chunk_instr_count_ - (cur_instr_count_ % chunk_instr_count_);
        VPRINT(this, 2, "Thread #%zd at %" PRIi64 " instrs at start of new chunk\n",
               thread_index, cur_instr_count_);
        VPRINT(this, 2,
               "zip chunk stop=%" PRIi64 " cur=%" PRIi64 " chunk=%" PRIi64
               " end-of-chunk=%" PRIi64 "\n",
               stop_count_, cur_instr_count_, chunk_instr_count_,
               cur_instr_count_ +
                   (chunk_instr_count_ - (cur_instr_count_ % chunk_instr_count_)));
        // Clear cached data from the prior chunk.
        zipfile->cur_buf = zipfile->max_buf;
    }
    // We have to linearly walk the last mile.
    // We need to present a timestamp+cpu so we reset this field so process_input_entry()
    // will not skip the first pair in this new chunk.
    last_timestamp_instr_count_ = cur_instr_count_;
    while (cur_instr_count_ < stop_count_) { // End condition is never reached.
        if (!read_next_thread_entry(thread_index, &entry_copy_, eof))
            return false;
        // We need to pass up memrefs for the final skipped instr, but we don't
        // want to process_input_entry() on the first unskipped instr so we can
        // insert the timestamp+cpu first.
        if (cur_instr_count_ + 1 == stop_count_ &&
            type_is_instr(static_cast<trace_type_t>(entry_copy_.type)))
            break;
        // Update core state.
        input_entry_ = &entry_copy_;
        if (!process_input_entry())
            continue;
        const memref_t &memref = **this;
        if (memref.marker.type == TRACE_TYPE_MARKER) {
            if (memref.marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP)
                timestamp = entry_copy_;
            else if (memref.marker.marker_type == TRACE_MARKER_TYPE_CPU_ID)
                cpu = entry_copy_;
        }
        // TODO i#5538: Have raw2trace insert a record ordinal marker at chunk entry
        // and use it here to udpate the memtrace_stream_t.
    }
    if (timestamp.type == TRACE_TYPE_MARKER && cpu.type == TRACE_TYPE_MARKER) {
        // Insert the two markers.
        // TODO i#5538: These end up with record ordinals that belong to different
        // records in the unskipped trace: we should instead not print them out
        // at all, somehow.
        trace_entry_t instr = entry_copy_;
        entry_copy_ = timestamp;
        process_input_entry();
        queues_[thread_index].push(cpu);
        queues_[thread_index].push(instr);
    } else {
        // We missed the markers somehow; fall back to just process the instr.
        // TODO i#5538: For skipping from the middle we need to have the
        // base reader cache the last timestamp,cpu.
        VPRINT(this, 1, "Skip failed to find both timestamp and cpu\n");
        process_input_entry();
    }
    return true;
}
