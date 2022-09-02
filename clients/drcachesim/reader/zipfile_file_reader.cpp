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

// We use minizip, which is in the contrib/minizip directory in the zlib
// sources.  The docs are the header files:
// https://github.com/madler/zlib/blob/master/contrib/minizip/unzip.h

/* clang-format off */ /* (make vera++ newline-after-type check happy) */
template <>
/* clang-format on */
file_reader_t<unzFile>::~file_reader_t<unzFile>()
{
    for (auto file : input_files_)
        unzClose(file);
    delete[] thread_eof_;
}

template <>
bool
file_reader_t<unzFile>::open_single_file(const std::string &path)
{
    unzFile file = unzOpen(path.c_str());
    if (file == nullptr)
        return false;
    input_files_.push_back(file);
    if (unzGoToFirstFile(file) != UNZ_OK || unzOpenCurrentFile(file) != UNZ_OK)
        return false;
    VPRINT(this, 1, "Opened input file %s\n", path.c_str());
    return true;
}

template <>
bool
file_reader_t<unzFile>::read_next_thread_entry(size_t thread_index,
                                               OUT trace_entry_t *entry, OUT bool *eof)
{
    *eof = false;
    // TODO i#5538: The reading performance for .zip files seems to be up to 60%
    // slower than .gz files, which is unexpected since both use zlib and the checksum
    // and header differences make zlib normally faster than gzip.
    // Do we need to buffer ourselves here by reading many entries at once?
    int num_read = unzReadCurrentFile(input_files_[thread_index], entry, sizeof(*entry));
    if (num_read == 0) {
#ifdef DEBUG
        if (verbosity_ >= 3) {
            char name[128];
            name[0] = '\0'; /* Just in case. */
            // This call is expensive if we do it every time.
            unzGetCurrentFileInfo64(input_files_[thread_index], nullptr, name,
                                    sizeof(name), nullptr, 0, nullptr, 0);
            VPRINT(this, 3,
                   "Thread #%zd hit end of component %s; opening next component\n",
                   thread_index, name);
        }
#endif
        // read_next_entry() stores the last entry into entry_copy_.
        if ((entry_copy_.type != TRACE_TYPE_MARKER ||
             entry_copy_.size != TRACE_MARKER_TYPE_CHUNK_FOOTER) &&
            entry_copy_.type != TRACE_TYPE_FOOTER) {
            VPRINT(this, 1, "Chunk is missing footer: truncation detection\n");
            return false;
        }
        if (unzCloseCurrentFile(input_files_[thread_index]) != UNZ_OK)
            return false;
        int res = unzGoToNextFile(input_files_[thread_index]);
        if (res != UNZ_OK) {
            if (res == UNZ_END_OF_LIST_OF_FILE) {
                VPRINT(this, 2, "Thread #%zd hit EOF\n", thread_index);
                *eof = true;
            }
            return false;
        }
        if (unzOpenCurrentFile(input_files_[thread_index]) != UNZ_OK)
            return false;
        num_read = unzReadCurrentFile(input_files_[thread_index], entry, sizeof(*entry));
    }
    if (num_read < static_cast<int>(sizeof(*entry))) {
        VPRINT(this, 1, "Thread #%zd failed to read: returned %d\n", thread_index,
               num_read);
        return false;
    }
    VPRINT(this, 4, "Read from thread #%zd: type=%d, size=%d, addr=%zu\n", thread_index,
           entry->type, entry->size, entry->addr);
    return true;
}

template <>
bool
file_reader_t<unzFile>::is_complete()
{
    // We could call unzeof() but we need the thread index.
    // XXX: We should remove or change this interface.  It is not used now.
    return false;
}

// TODO i#5538: Implement seeking via unzLocateFile.
