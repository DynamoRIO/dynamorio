/* **********************************************************
 * Copyright (c) 2016-2020 Google, Inc.  All rights reserved.
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
    return cur_ref_;
}

reader_t &
reader_t::operator++()
{
    // We bail if we get a partial read, or EOF, or any error.
    while (true) {
        if (bundle_idx_ == 0 /*not in instr bundle*/)
            input_entry_ = read_next_entry();
        if (input_entry_ == NULL) {
            if (!at_eof_) {
                ERRMSG("Trace is truncated\n");
                assert(false);
                at_eof_ = true; // bail
            }
            break;
        }
        if (input_entry_->type == TRACE_TYPE_FOOTER) {
            VPRINT(this, 2, "At thread EOF\n");
            // We've already presented the thread exit entry to the analyzer.
            continue;
        }
        VPRINT(this, 4, "RECV: type=%d, size=%d, addr=0x%zx\n", input_entry_->type,
               input_entry_->size, input_entry_->addr);
        bool have_memref = false;
        switch (input_entry_->type) {
        case TRACE_TYPE_READ:
        case TRACE_TYPE_WRITE:
        case TRACE_TYPE_PREFETCH:
        case TRACE_TYPE_PREFETCH_READ_L1:
        case TRACE_TYPE_PREFETCH_READ_L2:
        case TRACE_TYPE_PREFETCH_READ_L3:
        case TRACE_TYPE_PREFETCHNTA:
        case TRACE_TYPE_PREFETCH_READ:
        case TRACE_TYPE_PREFETCH_WRITE:
        case TRACE_TYPE_PREFETCH_INSTR:
        case TRACE_TYPE_PREFETCH_READ_L1_NT:
        case TRACE_TYPE_PREFETCH_READ_L2_NT:
        case TRACE_TYPE_PREFETCH_READ_L3_NT:
        case TRACE_TYPE_PREFETCH_INSTR_L1:
        case TRACE_TYPE_PREFETCH_INSTR_L1_NT:
        case TRACE_TYPE_PREFETCH_INSTR_L2:
        case TRACE_TYPE_PREFETCH_INSTR_L2_NT:
        case TRACE_TYPE_PREFETCH_INSTR_L3:
        case TRACE_TYPE_PREFETCH_INSTR_L3_NT:
        case TRACE_TYPE_PREFETCH_WRITE_L1:
        case TRACE_TYPE_PREFETCH_WRITE_L1_NT:
        case TRACE_TYPE_PREFETCH_WRITE_L2:
        case TRACE_TYPE_PREFETCH_WRITE_L2_NT:
        case TRACE_TYPE_PREFETCH_WRITE_L3:
        case TRACE_TYPE_PREFETCH_WRITE_L3_NT:
            have_memref = true;
            assert(cur_tid_ != 0 && cur_pid_ != 0);
            cur_ref_.data.pid = cur_pid_;
            cur_ref_.data.tid = cur_tid_;
            cur_ref_.data.type = (trace_type_t)input_entry_->type;
            cur_ref_.data.size = input_entry_->size;
            cur_ref_.data.addr = input_entry_->addr;
            // The trace stream always has the instr fetch first, which we
            // use to obtain the PC for subsequent data references.
            cur_ref_.data.pc = cur_pc_;
            break;
        case TRACE_TYPE_INSTR_MAYBE_FETCH:
            // While offline traces can convert rep string per-iter instrs into
            // no-fetch entries, online can't w/o extra work, so we do the work
            // here:
            if (prev_instr_addr_ == input_entry_->addr)
                input_entry_->type = TRACE_TYPE_INSTR_NO_FETCH;
            else
                input_entry_->type = TRACE_TYPE_INSTR;
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
            assert(cur_tid_ != 0 && cur_pid_ != 0);
            if (input_entry_->size == 0) {
                // Just an entry to tell us the PC of the subsequent memref,
                // used with -L0_filter where we don't reliably have icache
                // entries prior to data entries.
                cur_pc_ = input_entry_->addr;
            } else {
                have_memref = true;
                cur_ref_.instr.pid = cur_pid_;
                cur_ref_.instr.tid = cur_tid_;
                cur_ref_.instr.type = (trace_type_t)input_entry_->type;
                cur_ref_.instr.size = input_entry_->size;
                cur_pc_ = input_entry_->addr;
                cur_ref_.instr.addr = cur_pc_;
                next_pc_ = cur_pc_ + cur_ref_.instr.size;
                prev_instr_addr_ = input_entry_->addr;
            }
            break;
        case TRACE_TYPE_INSTR_BUNDLE:
            have_memref = true;
            // The trace stream always has the instr fetch first, which we
            // use to compute the starting PC for the subsequent instructions.
            if (!(type_is_instr(cur_ref_.instr.type) ||
                  cur_ref_.instr.type == TRACE_TYPE_INSTR_NO_FETCH)) {
                // XXX i#3320: Diagnostics to track down the elusive remaining case of
                // this assert on Appveyor.  We'll remove and replace with just the
                // assert once we have a fix.
                ERRMSG("Invalid trace entry type %d before a bundle\n",
                       cur_ref_.instr.type);
                assert(type_is_instr(cur_ref_.instr.type) ||
                       cur_ref_.instr.type == TRACE_TYPE_INSTR_NO_FETCH);
            }
            cur_ref_.instr.size = input_entry_->length[bundle_idx_++];
            cur_pc_ = next_pc_;
            cur_ref_.instr.addr = cur_pc_;
            next_pc_ = cur_pc_ + cur_ref_.instr.size;
            // input_entry_->size stores the number of instrs in this bundle
            assert(input_entry_->size <= sizeof(input_entry_->length));
            if (bundle_idx_ == input_entry_->size)
                bundle_idx_ = 0;
            break;
        case TRACE_TYPE_INSTR_FLUSH:
        case TRACE_TYPE_DATA_FLUSH:
            assert(cur_tid_ != 0 && cur_pid_ != 0);
            cur_ref_.flush.pid = cur_pid_;
            cur_ref_.flush.tid = cur_tid_;
            cur_ref_.flush.type = (trace_type_t)input_entry_->type;
            cur_ref_.flush.size = input_entry_->size;
            cur_ref_.flush.addr = input_entry_->addr;
            if (cur_ref_.flush.size != 0)
                have_memref = true;
            break;
        case TRACE_TYPE_INSTR_FLUSH_END:
        case TRACE_TYPE_DATA_FLUSH_END:
            cur_ref_.flush.size = input_entry_->addr - cur_ref_.flush.addr;
            have_memref = true;
            break;
        case TRACE_TYPE_THREAD:
            cur_tid_ = (memref_tid_t)input_entry_->addr;
            // tid2pid might not be filled in yet: if so, we expect a
            // TRACE_TYPE_PID entry right after this one, and later asserts
            // will complain if it wasn't there.
            cur_pid_ = tid2pid_[cur_tid_];
            break;
        case TRACE_TYPE_THREAD_EXIT:
            cur_tid_ = (memref_tid_t)input_entry_->addr;
            cur_pid_ = tid2pid_[cur_tid_];
            assert(cur_tid_ != 0 && cur_pid_ != 0);
            // We do pass this to the caller but only some fields are valid:
            cur_ref_.exit.pid = cur_pid_;
            cur_ref_.exit.tid = cur_tid_;
            cur_ref_.exit.type = (trace_type_t)input_entry_->type;
            have_memref = true;
            break;
        case TRACE_TYPE_PID:
            cur_pid_ = (memref_pid_t)input_entry_->addr;
            // We do want to replace, in case of tid reuse.
            tid2pid_[cur_tid_] = cur_pid_;
            break;
        case TRACE_TYPE_MARKER:
            have_memref = true;
            cur_ref_.marker.type = (trace_type_t)input_entry_->type;
            assert((cur_tid_ != 0 && cur_pid_ != 0) ||
                   input_entry_->size == TRACE_MARKER_TYPE_FILETYPE);
            cur_ref_.marker.pid = cur_pid_;
            cur_ref_.marker.tid = cur_tid_;
            cur_ref_.marker.marker_type = (trace_marker_type_t)input_entry_->size;
            cur_ref_.marker.marker_value = input_entry_->addr;
            break;
        default:
            ERRMSG("Unknown trace entry type %d\n", input_entry_->type);
            assert(false);
            at_eof_ = true; // bail
            break;
        }
        if (have_memref)
            break;
    }

    return *this;
}
