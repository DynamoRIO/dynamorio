/* **********************************************************
 * Copyright (c) 2016 Google, Inc.  All rights reserved.
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
#include "reader.h"
#include "../common/memref.h"
#include "../common/utils.h"

#ifdef VERBOSE
# include <iostream>
#endif

// Following typical stream iterator convention, the default constructor
// produces an EOF object.
reader_t::reader_t() : at_eof(true), input_entry(NULL), cur_tid(0), cur_pid(0),
                       cur_pc(0), bundle_idx(0)
{
    /* Empty. */
}

const memref_t&
reader_t::operator*()
{
    return cur_ref;
}

reader_t&
reader_t::operator++()
{
    // We bail if we get a partial read, or EOF, or any error.
    while (true) {
        if (bundle_idx == 0/*not in instr bundle*/)
            input_entry = read_next_entry();
        if (input_entry == NULL) {
#ifdef VERBOSE
            std::cerr << "EOF" << std::endl;
#endif
            at_eof = true;
            break;
        }
#ifdef VERBOSE
        std::cerr << "RECV: " << input_entry->type << " sz=" << input_entry->size <<
            " addr=" << (void *)input_entry->addr << std::endl;
#endif
        bool have_memref = false;
        switch (input_entry->type) {
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
            cur_ref.type = input_entry->type;
            cur_ref.size = input_entry->size;
            cur_ref.addr = input_entry->addr;
            // The trace stream always has the instr fetch first, which we
            // use to obtain the PC for subsequent data references.
            cur_ref.pc = cur_pc;
            break;
        case TRACE_TYPE_INSTR:
            have_memref = true;
            cur_ref.pid = cur_pid;
            cur_ref.tid = cur_tid;
            cur_ref.type = input_entry->type;
            cur_ref.size = input_entry->size;
            cur_pc = input_entry->addr;
            cur_ref.addr = cur_pc;
            cur_ref.pc = cur_pc;
            next_pc = cur_pc + cur_ref.size;
            break;
        case TRACE_TYPE_INSTR_BUNDLE:
            have_memref = true;
            // The trace stream always has the instr fetch first, which we
            // use to compute the starting PC for the subsequent instructions.
            cur_ref.size = input_entry->length[bundle_idx++];
            cur_pc = next_pc;
            cur_ref.pc = cur_pc;
            cur_ref.addr = cur_pc;
            next_pc = cur_pc + cur_ref.size;
            // input_entry->size stores the number of instrs in this bundle
            assert(input_entry->size <= sizeof(input_entry->length));
            if (bundle_idx == input_entry->size)
                bundle_idx = 0;
            break;
        case TRACE_TYPE_INSTR_FLUSH:
        case TRACE_TYPE_DATA_FLUSH:
            cur_ref.pid = cur_pid;
            cur_ref.tid = cur_tid;
            cur_ref.type = input_entry->type;
            cur_ref.size = input_entry->size;
            cur_ref.addr = input_entry->addr;
            if (cur_ref.size != 0)
                have_memref = true;
            break;
        case TRACE_TYPE_INSTR_FLUSH_END:
        case TRACE_TYPE_DATA_FLUSH_END:
            cur_ref.size = input_entry->addr - cur_ref.addr;
            have_memref = true;
            break;
        case TRACE_TYPE_THREAD:
            cur_tid = (memref_tid_t) input_entry->addr;
            cur_pid = tid2pid[cur_tid];
            break;
        case TRACE_TYPE_THREAD_EXIT:
            cur_tid = (memref_tid_t) input_entry->addr;
            cur_pid = tid2pid[cur_tid];
            // We do pass this to the caller but only some fields are valid:
            cur_ref.pid = cur_pid;
            cur_ref.tid = cur_tid;
            cur_ref.type = input_entry->type;
            have_memref = true;
            break;
        case TRACE_TYPE_PID:
            // We do want to replace, in case of tid reuse.
            tid2pid[cur_tid] = (memref_pid_t) input_entry->addr;
            break;
        default:
            ERROR("Unknown trace entry type %d\n", input_entry->type);
            assert(false);
            at_eof = true; // bail
            break;
        }
        if (have_memref)
            break;
    }

    return *this;
}
