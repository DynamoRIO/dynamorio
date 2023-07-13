/* **********************************************************
 * Copyright (c) 2015-2023 Google, Inc.  All rights reserved.
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

/* named_pipe: an abstraction over different operating systems of a
 * simple named pipe interface.
 */

#ifndef _NAMED_PIPE_H_
#define _NAMED_PIPE_H_ 1

#include <string>
#ifdef WINDOWS
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#else
#    include <unistd.h> // for ssize_t
#endif

namespace dynamorio {
namespace drmemtrace {

#ifdef WINDOWS
#    ifdef X64
typedef __int64 ssize_t;
#    else
typedef int ssize_t;
#    endif
#endif

#ifndef OUT
#    define OUT // nothing
#endif
#ifndef IN
#    define IN // nothing
#endif

// Usage is as follows:
// + Single caller calls create() up front (and at the end destroy()).
// + Each reader calls open_for_read() (and close() when done).
// + Each writer calls open_for_write() (and close() when done).
class named_pipe_t {
public:
    named_pipe_t();
    bool
    set_name(const char *name);
    std::string
    get_name() const;
    explicit named_pipe_t(const char *name);
    ~named_pipe_t();

    bool
    create();
    bool
    destroy();

    // These may block.
    bool
    open_for_read();
    bool
    open_for_write();

    bool
    close();

    // Increases the pipe's internal buffer to the maximum size.
    bool
    maximize_buffer();

    // Returns < 0 on EOF or an error.
    // On success (or partial read) returns number of bytes read.
    ssize_t
    read(void *buf OUT, size_t sz);

    // Returns < 0 on an error.
    // On success (or partial write) returns number of bytes written.
    ssize_t
    write(const void *buf IN, size_t sz);

#ifdef UNIX
    // On UNIX, rather than calling open_for_{read,write}, The caller
    // can substitute a custom call to SYS_open if desired, using
    // get_pipe_path() and setting the file descriptor in set_fd().
    // XXX i#1716: this should happen automatically in open_for_* and
    // we should not need this workaround.
    const std::string &
    get_pipe_path() const;
    bool
    set_fd(int fd);
#endif

    const ssize_t
    get_atomic_write_size() const;

private:
#ifdef WINDOWS
    HANDLE fd_;
#else
    int fd_;
#endif
    std::string pipe_name_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _NAMED_PIPE_H_ */
