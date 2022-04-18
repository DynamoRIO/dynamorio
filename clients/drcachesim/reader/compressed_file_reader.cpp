/* **********************************************************
 * Copyright (c) 2017-2020 Google, Inc.  All rights reserved.
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

#include "compressed_file_reader.h"

/* clang-format off */ /* (make vera++ newline-after-type check happy) */
template <>
/* clang-format on */
file_reader_t<gzFile>::~file_reader_t<gzFile>()
{
    for (auto file : input_files_)
        gzclose(file);
    delete[] thread_eof_;
}

template <>
bool
file_reader_t<gzFile>::open_single_file(const std::string &path)
{
    gzFile file = gzopen(path.c_str(), "rb");
    if (file == nullptr)
        return false;
    VPRINT(this, 1, "Opened input file %s\n", path.c_str());
    input_files_.push_back(file);
    return true;
}

template <>
bool
file_reader_t<gzFile>::read_next_thread_entry(size_t thread_index,
                                              OUT trace_entry_t *entry, OUT bool *eof)
{
    int len = gzread(input_files_[thread_index], (char *)entry, sizeof(*entry));
    // Returns less than asked-for for end of file, or –1 for error.
    if (len < (int)sizeof(*entry)) {
        *eof = (len >= 0);
        return false;
    }
    VPRINT(this, 4, "Read from thread #%zd file: type=%d, size=%d, addr=%zu\n",
           thread_index, entry->type, entry->size, entry->addr);
    return true;
}

template <>
bool
file_reader_t<gzFile>::is_complete()
{
    // The gzip reading interface does not support seeking to SEEK_END so there
    // is no efficient way to read the footer.
    // We could have the trace file writer seek back and set a bit at the start.
    // Currently we are forced to not use this function.
    // XXX: Should we just remove this interface, then?
    return false;
}
