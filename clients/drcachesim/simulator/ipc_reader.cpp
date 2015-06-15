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

#include <assert.h>
#include "memref.h"
#include "ipc_reader.h"
#include "utils.h"

#define BOOLS_MATCH(b1, b2) (!!(b1) == !!(b2))

ipc_reader_t::ipc_reader_t()
{
    // Following typical stream iterator convention, the default constructor
    // produces an EOF object.
    at_eof = true;
}

ipc_reader_t::ipc_reader_t(const char *ipc_name) :
    pipe(ipc_name)
{
    at_eof = true;
}

bool
ipc_reader_t::init()
{
    at_eof = false;
    if (!pipe.create() ||
        !pipe.open_for_read())
        return false;
    pipe.maximize_buffer();
    ++*this;
    return true;
}

ipc_reader_t::~ipc_reader_t()
{
    pipe.close();
    pipe.destroy();
}

memref_t&
ipc_reader_t::operator*()
{
    return cur;
}

bool
ipc_reader_t::operator==(const ipc_reader_t& rhs)
{
    return BOOLS_MATCH(rhs.at_eof, at_eof);
}

bool
ipc_reader_t::operator!=(const ipc_reader_t& rhs)
{
    return !BOOLS_MATCH(rhs.at_eof, at_eof);
}

reader_t
ipc_reader_t::operator++(int)
{
    ipc_reader_t tmp = *this;
    ++*this;
    return tmp;
}

reader_t&
ipc_reader_t::operator++()
{
    // XXX: we probably want to read a big chunk of data here and
    // parcel it out via an internal array, but we start out simple by
    // reading one entry at a time.  We'll measure where the
    // bottlenecks are before optimizing.

    // If we ever switch to separate IPC buffers per application thread,
    // we'd do the merging and timestamp ordering here.

    trace_entry_t entry;
    // We bail if we get a partial read, or EOF, or any error.
    while (true) {
        if (pipe.read(&entry, sizeof(entry)) < (ssize_t)sizeof(entry)) { // blocking read
            at_eof = true;
            break;
        } else {
            bool have_memref = false;
            switch (entry.type) {
            case TRACE_TYPE_READ:
            case TRACE_TYPE_WRITE:
            case TRACE_TYPE_PREFETCH:
                have_memref = true;
                cur.tid = cur_tid;
                cur.type = entry.type;
                cur.size = entry.size;
                cur.addr = entry.addr;
                break;
            case TRACE_TYPE_INSTR:
                // FIXME i#1703: NYI.
                // It's also not yet decided how to handle the PC for a mem ref
                // vs an instr fetch: who we have a PC field?
                break;
            case TRACE_TYPE_FLUSH:
                // FIXME i#1703: NYI
                break;
            case TRACE_TYPE_THREAD:
                cur_tid = (memref_tid_t) entry.addr;
                break;
            default:
                ERROR("Unknown trace entry type %d\n", entry.type);
                assert(false);
                at_eof = true; // bail
                break;
            }
            if (have_memref)
                break;
        }
    }

    return *this;
}
