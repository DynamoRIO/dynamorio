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
#include <map>
#include "ipc_reader.h"
#include "../common/memref.h"
#include "../common/utils.h"

#ifdef VERBOSE
# include <iostream>
#endif

#define BOOLS_MATCH(b1, b2) (!!(b1) == !!(b2))

ipc_reader_t::ipc_reader_t()
{
    // Following typical stream iterator convention, the default constructor
    // produces an EOF object.
    at_eof = true;
}

ipc_reader_t::ipc_reader_t(const char *ipc_name) :
    pipe(ipc_name), cur_tid(0), cur_pid(0), cur_pc(0), bundle_idx(0)
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

const memref_t&
ipc_reader_t::operator*()
{
    return cur_ref;
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
    // If we ever switch to separate IPC buffers per application thread,
    // we'd do the merging and timestamp ordering here.

    // We bail if we get a partial read, or EOF, or any error.
    while (true) {
        if (bundle_idx == 0/*not in instr bundle*/) {
            if (cur_buf >= end_buf) {
                ssize_t sz = pipe.read(buf, sizeof(buf)); // blocking read
                if (sz < 0 || sz % sizeof(*end_buf) != 0) {
                    at_eof = true;
                    break;
                }
                cur_buf = buf;
                end_buf = buf + (sz / sizeof(*end_buf));
            } else
                cur_buf++;
        }
        if (cur_buf < end_buf) {
#ifdef VERBOSE
            std::cerr << "RECV: " << cur_buf->type << " sz=" << cur_buf->size <<
                " addr=" << (void *)cur_buf->addr << std::endl;
#endif
            bool have_memref = false;
            switch (cur_buf->type) {
            case TRACE_TYPE_READ:
            case TRACE_TYPE_WRITE:
            case TRACE_TYPE_PREFETCH:
            case TRACE_TYPE_PREFETCHT0:
            case TRACE_TYPE_PREFETCHT1:
            case TRACE_TYPE_PREFETCHT2:
            case TRACE_TYPE_PREFETCHNTA:
            case TRACE_TYPE_PREFETCH_READ:
            case TRACE_TYPE_PREFETCH_WRITE:
            case TRACE_TYPE_PREFETCH_INSTR:
                have_memref = true;
                cur_ref.pid = cur_pid;
                cur_ref.tid = cur_tid;
                cur_ref.type = cur_buf->type;
                cur_ref.size = cur_buf->size;
                cur_ref.addr = cur_buf->addr;
                // The trace stream always has the instr fetch first, which we
                // use to obtain the PC for subsequent data references.
                cur_ref.pc = cur_pc;
                break;
            case TRACE_TYPE_INSTR:
                have_memref = true;
                cur_ref.pid = cur_pid;
                cur_ref.tid = cur_tid;
                cur_ref.type = cur_buf->type;
                cur_ref.size = cur_buf->size;
                cur_pc = cur_buf->addr;
                cur_ref.addr = cur_pc;
                cur_ref.pc = cur_pc;
                next_pc = cur_pc + cur_ref.size;
                break;
            case TRACE_TYPE_INSTR_BUNDLE:
                have_memref = true;
                // The trace stream always has the instr fetch first, which we
                // use to compute the starting PC for the subsequent instructions.
                cur_ref.size = cur_buf->length[bundle_idx++];
                cur_pc = next_pc;
                cur_ref.pc = cur_pc;
                cur_ref.addr = cur_pc;
                next_pc = cur_pc + cur_ref.size;
                // cur_buf->size stores the number of instrs in this bundle
                assert(cur_buf->size <= sizeof(cur_buf->length));
                if (bundle_idx == cur_buf->size)
                    bundle_idx = 0;
                break;
            case TRACE_TYPE_INSTR_FLUSH:
            case TRACE_TYPE_DATA_FLUSH:
                cur_ref.pid = cur_pid;
                cur_ref.tid = cur_tid;
                cur_ref.type = cur_buf->type;
                cur_ref.size = cur_buf->size;
                cur_ref.addr = cur_buf->addr;
                if (cur_ref.size != 0)
                    have_memref = true;
                break;
            case TRACE_TYPE_INSTR_FLUSH_END:
            case TRACE_TYPE_DATA_FLUSH_END:
                cur_ref.size = cur_buf->addr - cur_ref.addr;
                have_memref = true;
                break;
            case TRACE_TYPE_THREAD:
                cur_tid = (memref_tid_t) cur_buf->addr;
                cur_pid = tid2pid[cur_tid];
                break;
            case TRACE_TYPE_THREAD_EXIT:
                cur_tid = (memref_tid_t) cur_buf->addr;
                cur_pid = tid2pid[cur_tid];
                // We do pass this to the caller but only some fields are valid:
                cur_ref.pid = cur_pid;
                cur_ref.tid = cur_tid;
                cur_ref.type = cur_buf->type;
                have_memref = true;
                break;
            case TRACE_TYPE_PID:
                // We do want to replace, in case of tid reuse.
                tid2pid[cur_tid] = (memref_pid_t) cur_buf->addr;
                break;
            default:
                ERROR("Unknown trace entry type %d\n", cur_buf->type);
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
