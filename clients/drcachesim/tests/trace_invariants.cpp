/* **********************************************************
 * Copyright (c) 2017-2020 Google, Inc.  All rights reserved.
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

#include "trace_invariants.h"
#include <assert.h>
#include <iostream>
#include <string.h>

trace_invariants_t::trace_invariants_t(bool offline, unsigned int verbose)
    : knob_offline_(offline)
    , knob_verbose_(verbose)
    , instrs_until_interrupt_(-1)
    , memrefs_until_interrupt_(-1)
    , app_handler_pc_(0)
{
    memset(&prev_instr_, 0, sizeof(prev_instr_));
    memset(&pre_signal_instr_, 0, sizeof(pre_signal_instr_));
    memset(&prev_xfer_marker_, 0, sizeof(prev_xfer_marker_));
    memset(&prev_entry_, 0, sizeof(prev_entry_));
    memset(&prev_prev_entry_, 0, sizeof(prev_prev_entry_));
}

trace_invariants_t::~trace_invariants_t()
{
}

bool
trace_invariants_t::process_memref(const memref_t &memref)
{
    // Check conditions specific to the signal_invariants app, where it
    // has annotations in prefetch instructions telling us how many instrs
    // and/or memrefs until a signal should arrive.
    if ((instrs_until_interrupt_ == 0 && memrefs_until_interrupt_ == -1) ||
        (instrs_until_interrupt_ == -1 && memrefs_until_interrupt_ == 0) ||
        (instrs_until_interrupt_ == 0 && memrefs_until_interrupt_ == 0)) {
        assert((memref.marker.type == TRACE_TYPE_MARKER &&
                memref.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT) ||
               // TODO i#3937: Online instr bundles currently violate this.
               !knob_offline_);
        instrs_until_interrupt_ = -1;
        memrefs_until_interrupt_ = -1;
    }
    if (memrefs_until_interrupt_ >= 0 &&
        (memref.data.type == TRACE_TYPE_READ || memref.data.type == TRACE_TYPE_WRITE)) {
        assert(memrefs_until_interrupt_ != 0);
        --memrefs_until_interrupt_;
    }
    // Check that the signal delivery marker is immediately followed by the
    // app's signal handler, which we have annotated with "prefetcht0 [1]".
    if (memref.data.type == TRACE_TYPE_PREFETCHT0 && memref.data.addr == 1) {
        assert(type_is_instr(prev_entry_.instr.type) &&
               prev_prev_entry_.marker.type == TRACE_TYPE_MARKER &&
               prev_xfer_marker_.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT);
        app_handler_pc_ = prev_entry_.instr.addr;
    }

    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_CACHE_LINE_SIZE) {
        found_cache_line_size_marker_[memref.marker.tid] = true;
    }

    if (memref.exit.type == TRACE_TYPE_THREAD_EXIT) {
        assert(found_cache_line_size_marker_[memref.exit.tid]);
        thread_exited_[memref.exit.tid] = true;
    }
    if (type_is_instr(memref.instr.type) ||
        memref.instr.type == TRACE_TYPE_PREFETCH_INSTR ||
        memref.instr.type == TRACE_TYPE_INSTR_NO_FETCH) {
        if (knob_verbose_ >= 3) {
            std::cerr << "::" << memref.data.pid << ":" << memref.data.tid << ":: "
                      << " @" << (void *)memref.instr.addr
                      << ((memref.instr.type == TRACE_TYPE_INSTR_NO_FETCH)
                              ? " non-fetched"
                              : "")
                      << " instr x" << memref.instr.size << "\n";
        }
        assert(instrs_until_interrupt_ != 0);
        if (instrs_until_interrupt_ > 0)
            --instrs_until_interrupt_;
        // Invariant: offline traces guarantee that a branch target must immediately
        // follow the branch w/ no intervening trace switch.
        if (knob_offline_ && type_is_instr_branch(prev_instr_.instr.type)) {
            assert(
                prev_instr_.instr.tid == memref.instr.tid ||
                // For limited-window traces a thread might exit after a branch.
                thread_exited_[prev_instr_.instr.tid] ||
                // The invariant is relaxed for a signal.
                (prev_xfer_marker_.instr.tid == prev_instr_.instr.tid &&
                 prev_xfer_marker_.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT));
        }
        // Invariant: non-explicit control flow (i.e., kernel-mediated) is indicated
        // by markers.
        if (prev_instr_.instr.addr != 0 /*first*/ &&
            prev_instr_.instr.tid == memref.instr.tid &&
            !type_is_instr_branch(prev_instr_.instr.type)) {
            assert( // Regular fall-through.
                (prev_instr_.instr.addr + prev_instr_.instr.size == memref.instr.addr) ||
                // String loop.
                (prev_instr_.instr.addr == memref.instr.addr &&
                 memref.instr.type == TRACE_TYPE_INSTR_NO_FETCH) ||
                // Kernel-mediated, but we can't tell if we had a thread swap.
                (prev_xfer_marker_.instr.tid != 0 &&
                 (prev_xfer_marker_.instr.tid != memref.instr.tid ||
                  prev_xfer_marker_.marker.marker_type ==
                      TRACE_MARKER_TYPE_KERNEL_EVENT ||
                  prev_xfer_marker_.marker.marker_type ==
                      TRACE_MARKER_TYPE_KERNEL_XFER)) ||
                prev_instr_.instr.type == TRACE_TYPE_INSTR_SYSENTER);
            // XXX: If we had instr decoding we could check direct branch targets
            // and look for gaps after branches.
        }
#ifdef UNIX
        // Ensure signal handlers return to the interruption point.
        if (prev_xfer_marker_.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_XFER) {
            assert(memref.instr.tid != pre_signal_instr_.instr.tid ||
                   memref.instr.addr == pre_signal_instr_.instr.addr ||
                   // Asynch will go to the subsequent instr.
                   memref.instr.addr ==
                       pre_signal_instr_.instr.addr + pre_signal_instr_.instr.size ||
                   // Nested signal.  XXX: This only works for our annotated test
                   // signal_invariants.
                   memref.instr.addr == app_handler_pc_ ||
                   // Marker for rseq abort handler.  Not as unique as a prefetch, but
                   // we need an instruction and not a data type.
                   memref.instr.type == TRACE_TYPE_INSTR_DIRECT_JUMP ||
                   // Too hard to figure out branch targets.
                   type_is_instr_branch(pre_signal_instr_.instr.type) ||
                   pre_signal_instr_.instr.type == TRACE_TYPE_INSTR_SYSENTER);
        }
#endif
        prev_instr_ = memref;
        // Clear prev_xfer_marker_ on an instr (not a memref which could come between an
        // instr and a kernel-mediated far-away instr) to ensure it's *immediately*
        // prior (i#3937).
        memset(&prev_xfer_marker_, 0, sizeof(prev_xfer_marker_));
    }
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        // Ignore timestamp, etc. markers which show up a signal delivery boundaries
        // b/c the tracer does a buffer flush there.
        (memref.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT ||
         memref.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_XFER)) {
        if (knob_verbose_ >= 3) {
            std::cerr << "::" << memref.data.pid << ":" << memref.data.tid << ":: "
                      << "marker type " << memref.marker.marker_type << " value "
                      << memref.marker.marker_value << "\n";
        }
        if (memref.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT &&
            // Give up on back-to-back signals.
            prev_xfer_marker_.marker.marker_type != TRACE_MARKER_TYPE_KERNEL_XFER)
            pre_signal_instr_ = prev_instr_;
        prev_xfer_marker_ = memref;
    }

    // Look for annotations where signal_invariants.c and rseq.c pass info to us on what
    // to check for.  We assume the app does not have prefetch instrs w/ low addresses.
    if (memref.data.type == TRACE_TYPE_PREFETCHT2 && memref.data.addr < 1024) {
        instrs_until_interrupt_ = static_cast<int>(memref.data.addr);
    }
    if (memref.data.type == TRACE_TYPE_PREFETCHT1 && memref.data.addr < 1024) {
        memrefs_until_interrupt_ = static_cast<int>(memref.data.addr);
    }

    prev_prev_entry_ = prev_entry_;
    prev_entry_ = memref;
    return true;
}

bool
trace_invariants_t::print_results()
{
    std::cerr << "Trace invariant checks passed\n";
    return true;
}
