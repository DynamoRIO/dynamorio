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

/* directory_iterator: provides an iterator for walking files in a directory.
 */

#ifndef _DIRECTORY_ITERATOR_H_
#define _DIRECTORY_ITERATOR_H_ 1

#include <assert.h>

#include <iterator>
#include <string>
#ifdef WINDOWS
#    define UNICODE
#    define _UNICODE
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#else
#    include <dirent.h> /* opendir, readdir */
#endif

#include "utils.h"

namespace dynamorio {
namespace drmemtrace {

// Iterates over files: skips sub-directories.
// Returns the basenames of the files (i.e., not absolute paths).
// This class is not thread-safe.
class directory_iterator_t : public std::iterator<std::input_iterator_tag, std::string> {
public:
    directory_iterator_t()
    {
    }
    virtual ~directory_iterator_t();

    explicit directory_iterator_t(const std::string &directory);

    std::string
    error_string() const
    {
        return error_descr_;
    }

    virtual const std::string &
    operator*();

    // To avoid double-dispatch (requires listing all derived types in the base here)
    // and RTTI in trying to get the right operators called for subclasses, we
    // instead directly check at_eof here.  If we end up needing to run code
    // and a bool field is not enough we can change this to invoke a virtual
    // method is_at_eof().
    virtual bool
    operator==(const directory_iterator_t &rhs) const
    {
        return BOOLS_MATCH(at_eof_, rhs.at_eof_);
    }
    virtual bool
    operator!=(const directory_iterator_t &rhs) const
    {
        return !BOOLS_MATCH(at_eof_, rhs.at_eof_);
    }

    virtual directory_iterator_t &
    operator++();

    virtual bool
    operator!()
    {
        return at_eof_;
    }

    // We do not bother to support the post-increment operator.

    // Static cross-platform utility functions.
    static bool
    is_directory(const std::string &path);
    // Recursively creates all sub-directories.
    static bool
    create_directory(const std::string &path);

private:
    bool at_eof_ = true;
    std::string error_descr_;
    std::string cur_file_;
#ifdef WINDOWS
    HANDLE find_ = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW data_;
    TCHAR wdir_[MAX_PATH];
    char path_[MAX_PATH];
#else
    DIR *dir_ = nullptr;
    struct dirent *ent_ = nullptr;
#endif
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _DIRECTORY_ITERATOR_H_ */
