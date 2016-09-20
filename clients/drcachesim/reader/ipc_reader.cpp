/* **********************************************************
 * Copyright (c) 2015-2016 Google, Inc.  All rights reserved.
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

#include <assert.h>
#include <map>
#include "ipc_reader.h"
#include "../common/memref.h"
#include "../common/utils.h"

#ifdef VERBOSE
# include <iostream>
#endif

ipc_reader_t::ipc_reader_t()
{
    /* Empty. */
}

ipc_reader_t::ipc_reader_t(const char *ipc_name) :
    pipe(ipc_name)
{
    /* Empty. */
}

bool
ipc_reader_t::init()
{
    at_eof = false;
    if (!pipe.create() ||
        !pipe.open_for_read())
        return false;
    pipe.maximize_buffer();
    cur_buf = buf;
    end_buf = buf;
    ++*this;
    return true;
}

ipc_reader_t::~ipc_reader_t()
{
    pipe.close();
    pipe.destroy();
}

trace_entry_t *
ipc_reader_t::read_next_entry()
{
    ++cur_buf;
    if (cur_buf >= end_buf) {
        ssize_t sz = pipe.read(buf, sizeof(buf)); // blocking read
        if (sz < 0 || sz % sizeof(*end_buf) != 0) {
            return NULL;
        }
        cur_buf = buf;
        end_buf = buf + (sz / sizeof(*end_buf));
    }
    return cur_buf;
}
