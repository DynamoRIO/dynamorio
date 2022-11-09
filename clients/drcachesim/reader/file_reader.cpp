/* **********************************************************
 * Copyright (c) 2016-2020 Google, Inc.  All rights reserved.
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

#include <fstream>
#include "file_reader.h"

void
destroy_file_reader_common(std::vector<std::ifstream *> &input_files, bool *thread_eof)
{
    for (auto fstream : input_files) {
        delete fstream;
    }
    delete[] thread_eof;
}

bool
open_single_file_common(const std::string &path,
                        std::vector<std::ifstream *> &input_files)
{
    auto fstream = new std::ifstream(path, std::ifstream::binary);
    if (!*fstream)
        return false;
    input_files.push_back(fstream);
    return true;
}

bool
read_next_thread_entry_common(size_t thread_index, OUT trace_entry_t *entry,
                              OUT bool *eof, std::vector<std::ifstream *> &input_files)
{
    if (!input_files[thread_index]->read((char *)entry, sizeof(*entry))) {
        *eof = input_files[thread_index]->eof();
        return false;
    }
    return true;
}

bool
is_complete_common(std::vector<std::ifstream *> &input_files, std::string &input_path,
                   std::vector<std::string> &input_path_list, trace_entry_t &entry_copy)
{
    // FIXME i#3230: this code is in the middle of refactoring for split thread files.
    // We support the is_complete() call from analyzer_multi for a single file for now,
    // but may have to abandon this function altogether since gzFile doesn't support it.
    bool opened_temporarily = false;
    if (input_files.empty()) {
        // Supporting analyzer_multi calling before init() for a single legacy file.
        opened_temporarily = true;
        if (!input_path_list.empty() || input_path.empty() ||
            directory_iterator_t::is_directory(input_path))
            return false; // Not supported.
        if (!open_single_file_common(input_path, input_files))
            return false;
    }
    bool res = false;
    for (auto fstream : input_files) {
        res = false;
        std::streampos pos = fstream->tellg();
        fstream->seekg(-(int)sizeof(trace_entry_t), fstream->end);
        // Avoid reaching eof b/c we can't seek away from it.
        if (fstream->read((char *)&entry_copy.type, sizeof(entry_copy.type)) &&
            entry_copy.type == TRACE_TYPE_FOOTER)
            res = true;
        fstream->seekg(pos);
        if (!res)
            break;
    }
    if (opened_temporarily) {
        // Put things back for init().
        for (auto fstream : input_files)
            delete fstream;
        input_files.clear();
    }
    return res;
}

/* clang-format off */ /* (make vera++ newline-after-type check happy) */
template <>
/* clang-format on */
file_reader_tmpl_t<std::ifstream *, memref_t>::~file_reader_tmpl_t()
{
    destroy_file_reader_common(input_files_, thread_eof_);
}

template <>
bool
file_reader_tmpl_t<std::ifstream *, memref_t>::open_single_file(const std::string &path)
{
    if (!open_single_file_common(path, input_files_))
        return false;

    VPRINT(this, 1, "Opened input file %s\n", path.c_str());
    return true;
}

template <>
bool
file_reader_tmpl_t<std::ifstream *, memref_t>::read_next_thread_entry(
    size_t thread_index, OUT trace_entry_t *entry, OUT bool *eof)
{
    if (!read_next_thread_entry_common(thread_index, entry, eof, input_files_))
        return false;

    VPRINT(this, 4, "Read from thread #%zd file: type=%d, size=%d, addr=%zu\n",
           thread_index, entry->type, entry->size, entry->addr);
    return true;
}

template <>
bool
file_reader_tmpl_t<std::ifstream *, memref_t>::is_complete()
{
    return is_complete_common(input_files_, input_path_, input_path_list_, entry_copy_);
}

/* clang-format off */ /* (make vera++ newline-after-type check happy) */
template <>
/* clang-format on */
file_reader_tmpl_t<std::ifstream *, trace_entry_t>::~file_reader_tmpl_t()
{
    destroy_file_reader_common(input_files_, thread_eof_);
}

template <>
bool
file_reader_tmpl_t<std::ifstream *, trace_entry_t>::open_single_file(
    const std::string &path)
{
    if (!open_single_file_common(path, input_files_))
        return false;

    VPRINT(this, 1, "Opened input file %s\n", path.c_str());
    return true;
}

template <>
bool
file_reader_tmpl_t<std::ifstream *, trace_entry_t>::read_next_thread_entry(
    size_t thread_index, OUT trace_entry_t *entry, OUT bool *eof)
{
    if (!read_next_thread_entry_common(thread_index, entry, eof, input_files_))
        return false;

    VPRINT(this, 4, "Read from thread #%zd file: type=%d, size=%d, addr=%zu\n",
           thread_index, entry->type, entry->size, entry->addr);
    return true;
}

template <>
bool
file_reader_tmpl_t<std::ifstream *, trace_entry_t>::is_complete()
{
    return is_complete_common(input_files_, input_path_, input_path_list_, entry_copy_);
}

template class file_reader_tmpl_t<std::ifstream *, memref_t>;
template class file_reader_tmpl_t<std::ifstream *, trace_entry_t>;
