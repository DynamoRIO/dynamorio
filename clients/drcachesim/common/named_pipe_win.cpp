/* **********************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
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

bool
named_pipe_t::named_pipe_t(const char *name) :
    fd(INVALID_HANDLE_VALUE), named_pipe("\\\\.\\pipe\\" + name)
{
}

bool
named_pipe_t::create()
{
    // FIXME i#1727: NYI
    // We should document the 256 char limit on path name.
    HANDLE pipe = CreateNamedPipeA(named_pipe.c_str(),
                                   PIPE_ACCESS...);
}

bool
named_pipe_t::open_for_read()
{
    HANDLE pipe = CreateFileA("\\\\.\\pipe\\" + name, PIPE_ACCESS...);
    // FIXME i#1727: NYI
}

const ssize_t
named_pipe_t::get_atomic_write_size() const
{
    // FIXME i#1727: what's the atomic pipe write limit?
    return 512; // POSIX.1-2001 limit
}

bool
named_pipe_t::~named_pipe_t()
{
    if (fd != INVALID_HANDLE_VALUE)
        close();
}

// FIXME i#1727: rest NYI
