/* **********************************************************
 * Copyright (c) 2017-2021 Google, Inc.  All rights reserved.
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
#include <iostream>
#include <string.h>

trace_invariants_t::trace_invariants_t(bool offline, unsigned int verbose,
                                       std::string test_name)
    : knob_offline_(offline)
    , knob_verbose_(verbose)
    , knob_test_name_(test_name)
    , app_handler_pc_(0)
{
    memset(&prev_interleaved_instr_, 0, sizeof(prev_interleaved_instr_));
}

trace_invariants_t::~trace_invariants_t()
{
}

void
trace_invariants_t::report_if_false(bool condition, const std::string &message)
{
    if (!condition) {
        std::cerr << "Trace invariant failure: " << message << "\n";
        abort();
    }
}

bool
trace_invariants_t::process_memref(const memref_t &memref)
{
    if (prev_instr_.find(memref.data.tid) == prev_instr_.end()) {
#ifdef UNIX
        instrs_until_interrupt_[memref.data.tid] = -1;
        memrefs_until_interrupt_[memref.data.tid] = -1;
        prev_entry_[memref.data.tid] = {};
        prev_prev_entry_[memref.data.tid] = {};
#endif
        prev_instr_[memref.data.tid] = {};
        prev_xfer_marker_[memref.data.tid] = {};
    }
#ifdef UNIX
    // Check conditions specific to the signal_invariants app, where it
    // has annotations in prefetch instructions telling us how many instrs
    // and/or memrefs until a signal should arrive.
    if ((instrs_until_interrupt_[memref.data.tid] == 0 &&
         memrefs_until_interrupt_[memref.data.tid] == -1) ||
        (instrs_until_interrupt_[memref.data.tid] == -1 &&
         memrefs_until_interrupt_[memref.data.tid] == 0) ||
        (instrs_until_interrupt_[memref.data.tid] == 0 &&
         memrefs_until_interrupt_[memref.data.tid] == 0)) {
        report_if_false((memref.marker.type == TRACE_TYPE_MARKER &&
                         memref.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT) ||
                            // TODO i#3937: Online instr bundles currently violate this.
                            !knob_offline_,
                        "Interruption marker mis-placed");
        instrs_until_interrupt_[memref.data.tid] = -1;
        memrefs_until_interrupt_[memref.data.tid] = -1;
    }
    if (memrefs_until_interrupt_[memref.data.tid] >= 0 &&
        (memref.data.type == TRACE_TYPE_READ || memref.data.type == TRACE_TYPE_WRITE)) {
        report_if_false(memrefs_until_interrupt_[memref.data.tid] != 0,
                        "Interruption marker too late");
        --memrefs_until_interrupt_[memref.data.tid];
    }
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        prev_entry_[memref.data.tid].marker.type == TRACE_TYPE_MARKER &&
        prev_entry_[memref.data.tid].marker.marker_type == TRACE_MARKER_TYPE_RSEQ_ABORT) {
        // The rseq marker must be immediately prior to the kernel event marker.
        report_if_false(memref.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT,
                        "Rseq marker not immediately prior to kernel marker");
    }
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_RSEQ_ABORT) {
        // Check that the rseq final instruction was not executed: that raw2trace
        // rolled it back.
        report_if_false(memref.marker.marker_value !=
                            prev_instr_[memref.data.tid].instr.addr,
                        "Rseq post-abort instruction not rolled back");
    }
    // Check that the signal delivery marker is immediately followed by the
    // app's signal handler, which we have annotated with "prefetcht0 [1]".
    if (memref.data.type == TRACE_TYPE_PREFETCHT0 && memref.data.addr == 1) {
        report_if_false(type_is_instr(prev_entry_[memref.data.tid].instr.type) &&
                            prev_prev_entry_[memref.data.tid].marker.type ==
                                TRACE_TYPE_MARKER &&
                            prev_xfer_marker_[memref.data.tid].marker.marker_type ==
                                TRACE_MARKER_TYPE_KERNEL_EVENT,
                        "Signal handler not immediately after signal marker");
        app_handler_pc_ = prev_entry_[memref.data.tid].instr.addr;
    }
#endif

    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_FILETYPE) {
        file_type_ = static_cast<offline_file_type_t>(memref.marker.marker_value);
    }
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_INSTRUCTION_COUNT) {
        found_instr_count_marker_[memref.marker.tid] = true;
        report_if_false(memref.marker.marker_value >=
                            last_instr_count_marker_[memref.marker.tid],
                        "Instr count markers not increasing");
        last_instr_count_marker_[memref.marker.tid] = memref.marker.marker_value;
    }
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_CACHE_LINE_SIZE) {
        found_cache_line_size_marker_[memref.marker.tid] = true;
    }

    if (memref.exit.type == TRACE_TYPE_THREAD_EXIT) {
        report_if_false(!TESTALL(OFFLINE_FILE_TYPE_FILTERED, file_type_) ||
                            found_instr_count_marker_[memref.exit.tid],
                        "Missing instr count markers");
        report_if_false(found_cache_line_size_marker_[memref.exit.tid],
                        "Missing cache line marker");
        if (knob_test_name_ == "filter_asm_instr_count") {
            static constexpr int ASM_INSTR_COUNT = 133;
            report_if_false(last_instr_count_marker_[memref.exit.tid] == ASM_INSTR_COUNT,
                            "Incorrect instr count marker value");
        }
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
#ifdef UNIX
        report_if_false(instrs_until_interrupt_[memref.data.tid] != 0,
                        "Interruption marker too late");
        if (instrs_until_interrupt_[memref.data.tid] > 0)
            --instrs_until_interrupt_[memref.data.tid];
#endif
        // Invariant: offline traces guarantee that a branch target must immediately
        // follow the branch w/ no intervening thread switch.
        if (knob_offline_ && type_is_instr_branch(prev_interleaved_instr_.instr.type)) {
            report_if_false(
                prev_interleaved_instr_.instr.tid == memref.instr.tid ||
                    // For limited-window traces a thread might exit after a branch.
                    thread_exited_[prev_interleaved_instr_.instr.tid] ||
                    // The invariant is relaxed for a signal.
                    // prev_xfer_marker_ is cleared on an instr, so if set to
                    // non-zero it means it is immediately prior, in between
                    // prev_interleaved_instr_ and memref.
                    (prev_xfer_marker_[prev_interleaved_instr_.instr.tid].instr.tid ==
                         prev_interleaved_instr_.instr.tid &&
                     prev_xfer_marker_[prev_interleaved_instr_.instr.tid]
                             .marker.marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT),
                "Branch target not immediately after branch");
        }
        // Invariant: non-explicit control flow (i.e., kernel-mediated) is indicated
        // by markers.
        // We cache the prev_instr_ hash lookup to avoid large slowdowns on Windows.
        memref_t *prev_instr_ptr = prev_interleaved_instr_.instr.tid == memref.instr.tid
            ? &prev_interleaved_instr_
            : &prev_instr_[memref.data.tid];
        if (prev_instr_ptr->instr.addr != 0 /*first*/ &&
            !type_is_instr_branch(prev_instr_ptr->instr.type)) {
            report_if_false( // Filtered.
                TESTALL(OFFLINE_FILE_TYPE_FILTERED, file_type_) ||
                    // Regular fall-through.
                    (prev_instr_ptr->instr.addr + prev_instr_ptr->instr.size ==
                     memref.instr.addr) ||
                    // String loop.
                    (prev_instr_ptr->instr.addr == memref.instr.addr &&
                     (memref.instr.type == TRACE_TYPE_INSTR_NO_FETCH ||
                      // Online incorrectly marks the 1st string instr across a thread
                      // switch as fetched.
                      // TODO i#4915, #4948: Eliminate non-fetched and remove the
                      // underlying instrs altogether, which would fix this for us.
                      (!knob_offline_ &&
                       prev_interleaved_instr_.instr.tid != memref.instr.tid))) ||
                    // Kernel-mediated, but we can't tell if we had a thread swap.
                    (prev_xfer_marker_[memref.data.tid].instr.tid != 0 &&
                     (prev_xfer_marker_[memref.data.tid].marker.marker_type ==
                          TRACE_MARKER_TYPE_KERNEL_EVENT ||
                      prev_xfer_marker_[memref.data.tid].marker.marker_type ==
                          TRACE_MARKER_TYPE_KERNEL_XFER)) ||
                    prev_instr_ptr->instr.type == TRACE_TYPE_INSTR_SYSENTER,
                "Non-explicit control flow has no marker");
            // XXX: If we had instr decoding we could check direct branch targets
            // and look for gaps after branches.
        }
#ifdef UNIX
        // Ensure signal handlers return to the interruption point.
        if (prev_xfer_marker_[memref.data.tid].marker.marker_type ==
            TRACE_MARKER_TYPE_KERNEL_XFER) {
            report_if_false(
                ((memref.instr.addr == prev_xfer_int_pc_[memref.data.tid].top() ||
                  // DR hands us a different address for sysenter than the
                  // resumption point.
                  pre_signal_instr_[memref.data.tid].top().instr.type ==
                      TRACE_TYPE_INSTR_SYSENTER) &&
                 (memref.instr.addr ==
                      pre_signal_instr_[memref.data.tid].top().instr.addr ||
                  // Asynch will go to the subsequent instr.
                  memref.instr.addr ==
                      pre_signal_instr_[memref.data.tid].top().instr.addr +
                          pre_signal_instr_[memref.data.tid].top().instr.size ||
                  // Too hard to figure out branch targets.  We have the
                  // prev_xfer_int_pc_ though.
                  type_is_instr_branch(
                      pre_signal_instr_[memref.data.tid].top().instr.type) ||
                  pre_signal_instr_[memref.data.tid].top().instr.type ==
                      TRACE_TYPE_INSTR_SYSENTER)) ||
                    // Nested signal.  XXX: This only works for our annotated test
                    // signal_invariants.
                    memref.instr.addr == app_handler_pc_ ||
                    // Marker for rseq abort handler.  Not as unique as a prefetch, but
                    // we need an instruction and not a data type.
                    memref.instr.type == TRACE_TYPE_INSTR_DIRECT_JUMP,
                "Signal handler return point incorrect");
            // We assume paired signal entry-exit (so no longjmp and no rseq
            // inside signal handlers).
            prev_xfer_int_pc_[memref.data.tid].pop();
            pre_signal_instr_[memref.data.tid].pop();
        }
#endif
        prev_interleaved_instr_ = memref;
        // These 2 hash assignments cause a 2.5x slowdown for this test on Windows.
        // We have as many other hash lookups as we can under UNIX.
        // We could try to only update on a tid change to further reduce overhead.
        prev_instr_[memref.data.tid] = memref;
        // Clear prev_xfer_marker_ on an instr (not a memref which could come between an
        // instr and a kernel-mediated far-away instr) to ensure it's *immediately*
        // prior (i#3937).
        prev_xfer_marker_[memref.data.tid] = {};
    }
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        // Ignore timestamp, etc. markers which show up a signal delivery boundaries
        // b/c the tracer does a buffer flush there.
        (memref.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT ||
         memref.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_XFER)) {
        if (knob_verbose_ >= 3) {
            std::cerr << "::" << memref.data.pid << ":" << memref.data.tid << ":: "
                      << "marker type " << memref.marker.marker_type << " value 0x"
                      << std::hex << memref.marker.marker_value << std::dec << "\n";
        }
#ifdef UNIX
        if (memref.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT)
            prev_xfer_int_pc_[memref.data.tid].push(memref.marker.marker_value);
        report_if_false(memref.marker.marker_value != 0,
                        "Kernel event marker value missing");
        if (memref.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT &&
            // Give up on back-to-back signals.
            prev_xfer_marker_[memref.data.tid].marker.marker_type !=
                TRACE_MARKER_TYPE_KERNEL_XFER)
            pre_signal_instr_[memref.data.tid].push(prev_instr_[memref.data.tid]);
#endif
        prev_xfer_marker_[memref.data.tid] = memref;
    }

#ifdef UNIX
    // Look for annotations where signal_invariants.c and rseq.c pass info to us on what
    // to check for.  We assume the app does not have prefetch instrs w/ low addresses.
    if (memref.data.type == TRACE_TYPE_PREFETCHT2 && memref.data.addr < 1024) {
        instrs_until_interrupt_[memref.data.tid] = static_cast<int>(memref.data.addr);
    }
    if (memref.data.type == TRACE_TYPE_PREFETCHT1 && memref.data.addr < 1024) {
        memrefs_until_interrupt_[memref.data.tid] = static_cast<int>(memref.data.addr);
    }

    prev_prev_entry_[memref.data.tid] = prev_entry_[memref.data.tid];
    prev_entry_[memref.data.tid] = memref;
#endif

    return true;
}

bool
trace_invariants_t::print_results()
{
    std::cerr << "Trace invariant checks passed\n";
    return true;
}
