/* **********************************************************
 * Copyright (c) 2017-2023 Google, Inc.  All rights reserved.
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

namespace dynamorio {
namespace drmemtrace {

// We use minizip, which is in the contrib/minizip directory in the zlib
// sources.  The docs are the header files:
// https://github.com/madler/zlib/blob/master/contrib/minizip/unzip.h

/* clang-format off */ /* (make vera++ newline-after-type check happy) */
template <>
/* clang-format on */
file_reader_t<zipfile_reader_t>::file_reader_t()
{
    input_file_.file = nullptr;
}

/* clang-format off */ /* (make vera++ newline-after-type check happy) */
template <>
/* clang-format on */
file_reader_t<zipfile_reader_t>::~file_reader_t<zipfile_reader_t>()
{
    if (input_file_.file != nullptr) {
        unzClose(input_file_.file);
        input_file_.file = nullptr;
    }
}

template <>
bool
file_reader_t<zipfile_reader_t>::open_single_file(const std::string &path)
{
    unzFile file = unzOpen(path.c_str());
    if (file == nullptr)
        return false;
    input_file_ = zipfile_reader_t(file);
    if (unzGoToFirstFile(file) != UNZ_OK || unzOpenCurrentFile(file) != UNZ_OK)
        return false;
    VPRINT(this, 1, "Opened input file %s\n", path.c_str());
    return true;
}

template <>
trace_entry_t *
file_reader_t<zipfile_reader_t>::read_next_entry()
{
    trace_entry_t *from_queue = read_queued_entry();
    if (from_queue != nullptr)
        return from_queue;
    zipfile_reader_t *zipfile = &input_file_;
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
                VPRINT(this, 3, "Hit end of component %s; opening next component\n",
                       name);
            }
#endif
            // read_next_entry() stored the last-read entry into entry_copy_.
            if ((entry_copy_.type != TRACE_TYPE_MARKER ||
                 entry_copy_.size != TRACE_MARKER_TYPE_CHUNK_FOOTER) &&
                entry_copy_.type != TRACE_TYPE_FOOTER) {
                VPRINT(this, 1, "Chunk is missing footer: truncation detected\n");
                return nullptr;
            }
            if (unzCloseCurrentFile(zipfile->file) != UNZ_OK)
                return nullptr;
            int res = unzGoToNextFile(zipfile->file);
            if (res != UNZ_OK) {
                if (res == UNZ_END_OF_LIST_OF_FILE) {
                    VPRINT(this, 2, "Hit EOF\n");
                    at_eof_ = true;
                }
                return nullptr;
            }
            if (unzOpenCurrentFile(zipfile->file) != UNZ_OK)
                return nullptr;
            num_read =
                unzReadCurrentFile(zipfile->file, zipfile->buf, sizeof(zipfile->buf));
        }
        if (num_read < static_cast<int>(sizeof(entry_copy_))) {
            VPRINT(this, 1, "Failed to read: returned %d\n", num_read);
            return nullptr;
        }
        zipfile->cur_buf = zipfile->buf;
        zipfile->max_buf = zipfile->buf + (num_read / sizeof(*zipfile->max_buf));
    }
    entry_copy_ = *zipfile->cur_buf;
    ++zipfile->cur_buf;
    VPRINT(this, 5, "Read: type=%s (%d), size=%d, addr=%zu\n",
           trace_type_names[entry_copy_.type], entry_copy_.type, entry_copy_.size,
           entry_copy_.addr);
    return &entry_copy_;
}

template <>
reader_t &
file_reader_t<zipfile_reader_t>::skip_instructions(uint64_t instruction_count)
{
    if (instruction_count == 0)
        return *this;
    VPRINT(this, 2, "Skipping %" PRIi64 " instrs\n", instruction_count);
    if (!pre_skip_instructions())
        return *this;
    if (chunk_instr_count_ == 0) {
        VPRINT(this, 1, "Failed to record chunk instr count\n");
        at_eof_ = true;
        return *this;
    }
    zipfile_reader_t *zipfile = &input_file_;
    // We assume our unzGoToNextFile loop is plenty performant and we don't need to
    // know the chunk names to use with a single unzLocateFile.
    uint64_t stop_count = cur_instr_count_ + instruction_count + 1;
    VPRINT(this, 2,
           "stop=%" PRIi64 " cur=%" PRIi64 " chunk=%" PRIi64 " est=%" PRIi64 "\n",
           stop_count, cur_instr_count_, chunk_instr_count_,
           cur_instr_count_ +
               (chunk_instr_count_ - (cur_instr_count_ % chunk_instr_count_)));
    // First, quickly skip over chunks to reach the chunk containing the target.
    while (cur_instr_count_ +
               (chunk_instr_count_ - (cur_instr_count_ % chunk_instr_count_)) <
           stop_count) {
        if (unzCloseCurrentFile(zipfile->file) != UNZ_OK) {
            VPRINT(this, 1, "Failed to close zip subfile\n");
            at_eof_ = true;
            return *this;
        }
        int res = unzGoToNextFile(zipfile->file);
        if (res != UNZ_OK) {
            if (res == UNZ_END_OF_LIST_OF_FILE)
                VPRINT(this, 2, "Hit EOF\n");
            else
                VPRINT(this, 2, "Failed to go to next zip subfile\n");
            at_eof_ = true;
            return *this;
        }
        if (unzOpenCurrentFile(zipfile->file) != UNZ_OK) {
            VPRINT(this, 1, "Failed to open zip subfile\n");
            at_eof_ = true;
            return *this;
        }
        cur_instr_count_ += chunk_instr_count_ - (cur_instr_count_ % chunk_instr_count_);
        VPRINT(this, 2, "At %" PRIi64 " instrs at start of new chunk\n",
               cur_instr_count_);
        VPRINT(this, 2,
               "zip chunk stop=%" PRIi64 " cur=%" PRIi64 " chunk=%" PRIi64
               " end-of-chunk=%" PRIi64 "\n",
               stop_count, cur_instr_count_, chunk_instr_count_,
               cur_instr_count_ +
                   (chunk_instr_count_ - (cur_instr_count_ % chunk_instr_count_)));
        // Clear cached data from the prior chunk.
        zipfile->cur_buf = zipfile->max_buf;
    }
    // Now do a linear walk the rest of the way, remembering timestamps (we have
    // duplicated timestamps at the start of the chunk to cover any skipped in
    // the fast chunk jumps we just did).
    // Subtract 1 to pass the target instr itself.
    return skip_instructions_with_timestamp(stop_count - 1);
}

} // namespace drmemtrace
} // namespace dynamorio
