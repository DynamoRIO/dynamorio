/* **********************************************************
 * Copyright (c) 2017-2018 Google, Inc.  All rights reserved.
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

#include "directory_iterator.h"
#include "utils.h"
#include "dr_frontend.h"

// Following typical stream iterator convention, the default constructor
// produces an EOF object.
directory_iterator_t::directory_iterator_t(const std::string &directory)
{
#ifdef UNIX
    dir = opendir(directory.c_str());
    if (dir == nullptr) {
        error_descr = "Failed to access directory.";
        return;
    }
    ++*this;
#else
    find = INVALID_HANDLE_VALUE;
    // Append \*
    _snprintf(path, BUFFER_SIZE_ELEMENTS(path), "%s\\*", directory.c_str());
    NULL_TERMINATE_BUFFER(path);
    if (drfront_char_to_tchar(path, wdir, BUFFER_SIZE_ELEMENTS(wdir)) !=
        DRFRONT_SUCCESS) {
        error_descr = "Failed to convert from utf-8 to utf-16";
        return;
    }
    path[0] = '\0';
    ++*this;
#endif
    at_eof = false;
}

directory_iterator_t::~directory_iterator_t()
{
#ifdef UNIX
    if (dir != nullptr)
        closedir(dir);
#else
    if (find != INVALID_HANDLE_VALUE)
        FindClose(find);
#endif
}

// Work around clang-format bug: no newline after return type for single-char operator.
// clang-format off
const std::string &
directory_iterator_t::operator*()
// clang-format on
{
    return cur_file;
}

directory_iterator_t &
directory_iterator_t::operator++()
{
#ifdef UNIX
    ent = readdir(dir);
    if (ent == nullptr)
        at_eof = true;
    else
        cur_file = ent->d_name;
#else
    if (find == INVALID_HANDLE_VALUE) {
        find = FindFirstFileW(wdir, &data);
        if (find == INVALID_HANDLE_VALUE) {
            error_descr = "Failed to list directory";
            at_eof = true;
            return *this;
        }
    } else if (!FindNextFile(find, &data)) {
        at_eof = true;
        return *this;
    }
    while (TESTANY(data.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY)) {
        if (!FindNextFile(find, &data)) {
            at_eof = true;
            return *this;
        }
    }
    if (drfront_tchar_to_char(data.cFileName, path, BUFFER_SIZE_ELEMENTS(path)) !=
        DRFRONT_SUCCESS) {
        error_descr = "Failed to convert from utf-16 to utf-8";
        at_eof = true;
        return *this;
    }
    cur_file = path;
#endif
    return *this;
}

bool
directory_iterator_t::is_directory(const std::string &path)
{
    bool is_dir;
    drfront_status_t res = drfront_dir_exists(path.c_str(), &is_dir);
    return res == DRFRONT_SUCCESS && is_dir;
}

bool
directory_iterator_t::create_directory(const std::string &path)
{
    drfront_status_t res = drfront_create_dir(path.c_str());
    return res == DRFRONT_SUCCESS;
}
