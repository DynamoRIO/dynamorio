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

#include <string>
#include <windows.h>
#include "named_pipe.h"

namespace dynamorio {
namespace drmemtrace {

#define MAX_NAME_LEN 256 // From CreateNamedPipe docs.
#define ALLOC_UNIT (64 * 1025)
#define OUT_BUFSZ (16 * ALLOC_UNIT)
#define IN_BUFSZ OUT_BUFSZ

named_pipe_t::named_pipe_t()
    : fd_(INVALID_HANDLE_VALUE)
{
    // Empty.
}

named_pipe_t::named_pipe_t(const char *name)
    : fd_(INVALID_HANDLE_VALUE)
{
    set_name(name);
}

named_pipe_t::~named_pipe_t()
{
    if (fd_ != INVALID_HANDLE_VALUE)
        close();
}

bool
named_pipe_t::set_name(const char *name)
{
    if (fd_ == INVALID_HANDLE_VALUE) {
        pipe_name_ = std::string("\\\\.\\pipe\\") + name;
        // The total is limited to 256 chars.
        if (pipe_name_.size() > MAX_NAME_LEN)
            pipe_name_.resize(MAX_NAME_LEN);
        return true;
    }
    return false;
}

std::string
named_pipe_t::get_name() const
{
    return pipe_name_;
}

bool
named_pipe_t::create()
{
    fd_ = CreateNamedPipeA(pipe_name_.c_str(), PIPE_ACCESS_INBOUND,
                           PIPE_TYPE_BYTE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES,
                           OUT_BUFSZ, IN_BUFSZ, 0, NULL);
    return (fd_ != INVALID_HANDLE_VALUE);
}

bool
named_pipe_t::destroy()
{
    return close();
}

bool
named_pipe_t::open_for_read()
{
    if (fd_ == INVALID_HANDLE_VALUE) {
        fd_ = CreateFileA(pipe_name_.c_str(), GENERIC_READ,
                          FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        return (fd_ != INVALID_HANDLE_VALUE);
    } else {
        // This may block.
        BOOL res = ConnectNamedPipe(fd_, NULL);
        return res == TRUE;
    }
}

bool
named_pipe_t::open_for_write()
{
    if (fd_ == INVALID_HANDLE_VALUE) {
        fd_ = CreateFileA(pipe_name_.c_str(), GENERIC_WRITE,
                          FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        // FIXME i#1727: support multiple processes.  We get ERROR_PIPE_BUSY if
        // a 2nd process connects to the same pipe instance, so we need an array
        // of instances (we'll have to set a maximum) and to use overlapped i/o
        // and have read() wait on an event for each pipe instance (or, use a
        // separate thread per app process, but that significantly changes our
        // design).
        return (fd_ != INVALID_HANDLE_VALUE);
    } else {
        // This may block.
        BOOL res = ConnectNamedPipe(fd_, NULL);
        return res == TRUE;
    }
}

bool
named_pipe_t::close()
{
    BOOL res = CloseHandle(fd_);
    return res == TRUE;
}

bool
named_pipe_t::maximize_buffer()
{
    // We specified the buffer sizes in create().
    return true;
}

ssize_t
named_pipe_t::read(void *buf OUT, size_t sz)
{
    DWORD actual;
    BOOL res = ReadFile(fd_, buf, (DWORD)sz, &actual, NULL);
    if (!res)
        return -1;
    else
        return actual;
}

ssize_t
named_pipe_t::write(const void *buf IN, size_t sz)
{
    DWORD actual;
    BOOL res = WriteFile(fd_, buf, (DWORD)sz, &actual, NULL);
    if (!res)
        return -1;
    else
        return actual;
}

const ssize_t
named_pipe_t::get_atomic_write_size() const
{
    // FIXME i#1727: what's the atomic pipe write limit?
    return 512; // POSIX.1-2001 limit
}

} // namespace drmemtrace
} // namespace dynamorio
