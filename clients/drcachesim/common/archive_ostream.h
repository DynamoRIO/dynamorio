/* **********************************************************
 * Copyright (c) 2022-2023 Google, Inc.  All rights reserved.
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

/* archive_ostream_t: a wrapper around std::ostream which additionally
 * provides archive functionality to combine multiple components inside one
 * larger component (typically a file).
 */

#ifndef _ARCHIVE_OSTREAM_H_
#define _ARCHIVE_OSTREAM_H_ 1

#include <fstream>

namespace dynamorio {
namespace drmemtrace {

class archive_ostream_t : public std::ostream {
public:
    explicit archive_ostream_t(std::basic_streambuf<char, std::char_traits<char>> *buf)
        : std::ostream(buf)
    {
    }
    // Closes any currently open component and opens a new one.  Future writes are
    // appended to the new component.  Returns an empty string on success or a non-empty
    // error description on failure.
    virtual std::string
    open_new_component(const std::string &name) = 0;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _ARCHIVE_OSTREAM_H_ */
