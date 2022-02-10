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

#include "directory_iterator.h"
#include "utils.h"
#include "dr_frontend.h"

// Following typical stream iterator convention, the default constructor
// produces an EOF object.
directory_iterator_t::directory_iterator_t(const std::string &directory)
{
#ifdef UNIX
    dir_ = opendir(directory.c_str());
    if (dir_ == nullptr) {
        error_descr_ = "Failed to access directory.";
        return;
    }
    ++*this;
#else
    find_ = INVALID_HANDLE_VALUE;
    // Append \*
    _snprintf(path_, BUFFER_SIZE_ELEMENTS(path_), "%s\\*", directory.c_str());
    NULL_TERMINATE_BUFFER(path_);
    if (drfront_char_to_tchar(path_, wdir_, BUFFER_SIZE_ELEMENTS(wdir_)) !=
        DRFRONT_SUCCESS) {
        error_descr_ = "Failed to convert from utf-8 to utf-16";
        return;
    }
    path_[0] = '\0';
    ++*this;
#endif
    at_eof_ = false;
}

directory_iterator_t::~directory_iterator_t()
{
#ifdef UNIX
    if (dir_ != nullptr)
        closedir(dir_);
#else
    if (find_ != INVALID_HANDLE_VALUE)
        FindClose(find_);
#endif
}

// Work around clang-format bug: no newline after return type for single-char operator.
// clang-format off
const std::string &
directory_iterator_t::operator*()
// clang-format on
{
    return cur_file_;
}

directory_iterator_t &
directory_iterator_t::operator++()
{
#ifdef UNIX
    ent_ = readdir(dir_);
    if (ent_ == nullptr)
        at_eof_ = true;
    else
        cur_file_ = ent_->d_name;
#else
    if (find_ == INVALID_HANDLE_VALUE) {
        find_ = FindFirstFileW(wdir_, &data_);
        if (find_ == INVALID_HANDLE_VALUE) {
            error_descr_ = "Failed to list directory";
            at_eof_ = true;
            return *this;
        }
    } else if (!FindNextFile(find_, &data_)) {
        at_eof_ = true;
        return *this;
    }
    while (TESTANY(data_.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY)) {
        if (!FindNextFile(find_, &data_)) {
            at_eof_ = true;
            return *this;
        }
    }
    if (drfront_tchar_to_char(data_.cFileName, path_, BUFFER_SIZE_ELEMENTS(path_)) !=
        DRFRONT_SUCCESS) {
        error_descr_ = "Failed to convert from utf-16 to utf-8";
        at_eof_ = true;
        return *this;
    }
    cur_file_ = path_;
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
