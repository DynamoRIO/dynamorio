/* **********************************************************
 * Copyright (c) 2016-2018 Google, Inc.  All rights reserved.
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
#include "reader.h"
#include "../common/memref.h"
#include "../common/utils.h"

// Work around clang-format bug: no newline after return type for single-char operator.
// clang-format off
const memref_t &
reader_t::operator*()
// clang-format on
{
    return cur_ref;
}

reader_t &
reader_t::operator++()
{
    // We bail if we get a partial read, or EOF, or any error.
    while (true) {
        if (bundle_idx == 0 /*not in instr bundle*/)
            input_entry = read_next_entry();
        if (input_entry == NULL) {
            if (!at_eof) {
                ERRMSG("Trace is truncated\n");
                assert(false);
                at_eof = true; // bail
            }
            break;
        }
        if (input_entry->type == TRACE_TYPE_FOOTER) {
            VPRINT(this, 2, "At thread EOF\n");
            // We've already presented the thread exit entry to the analyzer.
            continue;
        }
        VPRINT(this, 4, "RECV: type=%d, size=%d, addr=%zd\n", input_entry->type,
               input_entry->size, input_entry->addr);
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
            assert(cur_tid != 0 && cur_pid != 0);
            cur_ref.data.pid = cur_pid;
            cur_ref.data.tid = cur_tid;
            cur_ref.data.type = (trace_type_t)input_entry->type;
            cur_ref.data.size = input_entry->size;
            cur_ref.data.addr = input_entry->addr;
            // The trace stream always has the instr fetch first, which we
            // use to obtain the PC for subsequent data references.
            cur_ref.data.pc = cur_pc;
            break;
        case TRACE_TYPE_INSTR_MAYBE_FETCH:
            // While offline traces can convert rep string per-iter instrs into
            // no-fetch entries, online can't w/o extra work, so we do the work
            // here:
            if (prev_instr_addr == input_entry->addr)
                input_entry->type = TRACE_TYPE_INSTR_NO_FETCH;
            else
                input_entry->type = TRACE_TYPE_INSTR;
            ANNOTATE_FALLTHROUGH;
        case TRACE_TYPE_INSTR:
        case TRACE_TYPE_INSTR_DIRECT_JUMP:
        case TRACE_TYPE_INSTR_INDIRECT_JUMP:
        case TRACE_TYPE_INSTR_CONDITIONAL_JUMP:
        case TRACE_TYPE_INSTR_DIRECT_CALL:
        case TRACE_TYPE_INSTR_INDIRECT_CALL:
        case TRACE_TYPE_INSTR_RETURN:
        case TRACE_TYPE_INSTR_SYSENTER:
        case TRACE_TYPE_INSTR_NO_FETCH:
            assert(cur_tid != 0 && cur_pid != 0);
            if (input_entry->size == 0) {
                // Just an entry to tell us the PC of the subsequent memref,
                // used with -L0_filter where we don't reliably have icache
                // entries prior to data entries.
                cur_pc = input_entry->addr;
            } else {
                have_memref = true;
                cur_ref.instr.pid = cur_pid;
                cur_ref.instr.tid = cur_tid;
                cur_ref.instr.type = (trace_type_t)input_entry->type;
                cur_ref.instr.size = input_entry->size;
                cur_pc = input_entry->addr;
                cur_ref.instr.addr = cur_pc;
                next_pc = cur_pc + cur_ref.instr.size;
                prev_instr_addr = input_entry->addr;
            }
            break;
        case TRACE_TYPE_INSTR_BUNDLE:
            have_memref = true;
            // The trace stream always has the instr fetch first, which we
            // use to compute the starting PC for the subsequent instructions.
            assert(type_is_instr(cur_ref.instr.type));
            cur_ref.instr.size = input_entry->length[bundle_idx++];
            cur_pc = next_pc;
            cur_ref.instr.addr = cur_pc;
            next_pc = cur_pc + cur_ref.instr.size;
            // input_entry->size stores the number of instrs in this bundle
            assert(input_entry->size <= sizeof(input_entry->length));
            if (bundle_idx == input_entry->size)
                bundle_idx = 0;
            break;
        case TRACE_TYPE_INSTR_FLUSH:
        case TRACE_TYPE_DATA_FLUSH:
            assert(cur_tid != 0 && cur_pid != 0);
            cur_ref.flush.pid = cur_pid;
            cur_ref.flush.tid = cur_tid;
            cur_ref.flush.type = (trace_type_t)input_entry->type;
            cur_ref.flush.size = input_entry->size;
            cur_ref.flush.addr = input_entry->addr;
            if (cur_ref.flush.size != 0)
                have_memref = true;
            break;
        case TRACE_TYPE_INSTR_FLUSH_END:
        case TRACE_TYPE_DATA_FLUSH_END:
            cur_ref.flush.size = input_entry->addr - cur_ref.flush.addr;
            have_memref = true;
            break;
        case TRACE_TYPE_THREAD:
            cur_tid = (memref_tid_t)input_entry->addr;
            // tid2pid might not be filled in yet: if so, we expect a
            // TRACE_TYPE_PID entry right after this one, and later asserts
            // will complain if it wasn't there.
            cur_pid = tid2pid[cur_tid];
            break;
        case TRACE_TYPE_THREAD_EXIT:
            cur_tid = (memref_tid_t)input_entry->addr;
            cur_pid = tid2pid[cur_tid];
            assert(cur_tid != 0 && cur_pid != 0);
            // We do pass this to the caller but only some fields are valid:
            cur_ref.exit.pid = cur_pid;
            cur_ref.exit.tid = cur_tid;
            cur_ref.exit.type = (trace_type_t)input_entry->type;
            have_memref = true;
            break;
        case TRACE_TYPE_PID:
            cur_pid = (memref_pid_t)input_entry->addr;
            // We do want to replace, in case of tid reuse.
            tid2pid[cur_tid] = cur_pid;
            break;
        case TRACE_TYPE_MARKER:
            have_memref = true;
            cur_ref.marker.type = (trace_type_t)input_entry->type;
            assert(cur_tid != 0 && cur_pid != 0);
            cur_ref.marker.pid = cur_pid;
            cur_ref.marker.tid = cur_tid;
            cur_ref.marker.marker_type = (trace_marker_type_t)input_entry->size;
            cur_ref.marker.marker_value = input_entry->addr;
            break;
        default:
            ERRMSG("Unknown trace entry type %d\n", input_entry->type);
            assert(false);
            at_eof = true; // bail
            break;
        }
        if (have_memref)
            break;
    }

    return *this;
}
