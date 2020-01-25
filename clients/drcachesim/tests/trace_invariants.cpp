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
    : knob_offline(offline)
    , knob_verbose(verbose)
    , instrs_until_interrupt(-1)
    , memrefs_until_interrupt(-1)
    , app_handler_pc(0)
{
    memset(&prev_instr, 0, sizeof(prev_instr));
    memset(&pre_signal_instr, 0, sizeof(pre_signal_instr));
    memset(&prev_xfer_marker, 0, sizeof(prev_xfer_marker));
    memset(&prev_entry, 0, sizeof(prev_entry));
    memset(&prev_prev_entry, 0, sizeof(prev_prev_entry));
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
    if ((instrs_until_interrupt == 0 && memrefs_until_interrupt == -1) ||
        (instrs_until_interrupt == -1 && memrefs_until_interrupt == 0) ||
        (instrs_until_interrupt == 0 && memrefs_until_interrupt == 0)) {
        assert((memref.marker.type == TRACE_TYPE_MARKER &&
                memref.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT) ||
               // TODO i#3937: Online instr bundles currently violate this.
               !knob_offline);
        instrs_until_interrupt = -1;
        memrefs_until_interrupt = -1;
    }
    if (memrefs_until_interrupt >= 0 &&
        (memref.data.type == TRACE_TYPE_READ || memref.data.type == TRACE_TYPE_WRITE)) {
        assert(memrefs_until_interrupt != 0);
        --memrefs_until_interrupt;
    }
    // Check that the signal delivery marker is immediately followed by the
    // app's signal handler, which we have annotated with "prefetcht0 [1]".
    if (memref.data.type == TRACE_TYPE_PREFETCHT0 && memref.data.addr == 1) {
        assert(type_is_instr(prev_entry.instr.type) &&
               prev_prev_entry.marker.type == TRACE_TYPE_MARKER &&
               prev_xfer_marker.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT);
        app_handler_pc = prev_entry.instr.addr;
    }

    if (memref.exit.type == TRACE_TYPE_THREAD_EXIT)
        thread_exited[memref.exit.tid] = true;
    if (type_is_instr(memref.instr.type) ||
        memref.instr.type == TRACE_TYPE_PREFETCH_INSTR ||
        memref.instr.type == TRACE_TYPE_INSTR_NO_FETCH) {
        if (knob_verbose >= 3) {
            std::cerr << "::" << memref.data.pid << ":" << memref.data.tid << ":: "
                      << " @" << (void *)memref.instr.addr
                      << ((memref.instr.type == TRACE_TYPE_INSTR_NO_FETCH)
                              ? " non-fetched"
                              : "")
                      << " instr x" << memref.instr.size << "\n";
        }
        assert(instrs_until_interrupt != 0);
        if (instrs_until_interrupt > 0)
            --instrs_until_interrupt;
        // Invariant: offline traces guarantee that a branch target must immediately
        // follow the branch w/ no intervening trace switch.
        if (knob_offline && type_is_instr_branch(prev_instr.instr.type)) {
            assert(
                prev_instr.instr.tid == memref.instr.tid ||
                // For limited-window traces a thread might exit after a branch.
                thread_exited[prev_instr.instr.tid] ||
                // The invariant is relaxed for a signal.
                (prev_xfer_marker.instr.tid == prev_instr.instr.tid &&
                 prev_xfer_marker.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT));
        }
        // Invariant: non-explicit control flow (i.e., kernel-mediated) is indicated
        // by markers.
        if (prev_instr.instr.addr != 0 /*first*/ &&
            prev_instr.instr.tid == memref.instr.tid &&
            !type_is_instr_branch(prev_instr.instr.type)) {
            assert( // Regular fall-through.
                (prev_instr.instr.addr + prev_instr.instr.size == memref.instr.addr) ||
                // String loop.
                (prev_instr.instr.addr == memref.instr.addr &&
                 memref.instr.type == TRACE_TYPE_INSTR_NO_FETCH) ||
                // Kernel-mediated, but we can't tell if we had a thread swap.
                (prev_xfer_marker.instr.tid != 0 &&
                 (prev_xfer_marker.instr.tid != memref.instr.tid ||
                  prev_xfer_marker.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT ||
                  prev_xfer_marker.marker.marker_type ==
                      TRACE_MARKER_TYPE_KERNEL_XFER)) ||
                prev_instr.instr.type == TRACE_TYPE_INSTR_SYSENTER);
            // XXX: If we had instr decoding we could check direct branch targets
            // and look for gaps after branches.
        }
#ifdef UNIX
        // Ensure signal handlers return to the interruption point.
        if (prev_xfer_marker.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_XFER) {
            assert(memref.instr.tid != pre_signal_instr.instr.tid ||
                   memref.instr.addr == pre_signal_instr.instr.addr ||
                   // Asynch will go to the subsequent instr.
                   memref.instr.addr ==
                       pre_signal_instr.instr.addr + pre_signal_instr.instr.size ||
                   // Nested signal.  XXX: This only works for our annotated test
                   // signal_invariants.
                   memref.instr.addr == app_handler_pc ||
                   // Marker for rseq abort handler.  Not as unique as a prefetch, but
                   // we need an instruction and not a data type.
                   memref.instr.type == TRACE_TYPE_INSTR_DIRECT_JUMP ||
                   // Too hard to figure out branch targets.
                   type_is_instr_branch(pre_signal_instr.instr.type) ||
                   pre_signal_instr.instr.type == TRACE_TYPE_INSTR_SYSENTER);
        }
#endif
        prev_instr = memref;
        // Clear prev_xfer_marker on an instr (not a memref which could come between an
        // instr and a kernel-mediated far-away instr) to ensure it's *immediately*
        // prior (i#3937).
        memset(&prev_xfer_marker, 0, sizeof(prev_xfer_marker));
    }
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        // Ignore timestamp, etc. markers which show up a signal delivery boundaries
        // b/c the tracer does a buffer flush there.
        (memref.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT ||
         memref.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_XFER)) {
        if (knob_verbose >= 3) {
            std::cerr << "::" << memref.data.pid << ":" << memref.data.tid << ":: "
                      << "marker type " << memref.marker.marker_type << " value "
                      << memref.marker.marker_value << "\n";
        }
        if (memref.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT &&
            // Give up on back-to-back signals.
            prev_xfer_marker.marker.marker_type != TRACE_MARKER_TYPE_KERNEL_XFER)
            pre_signal_instr = prev_instr;
        prev_xfer_marker = memref;
    }

    // Look for annotations where signal_invariants.c and rseq.c pass info to us on what
    // to check for.  We assume the app does not have prefetch instrs w/ low addresses.
    if (memref.data.type == TRACE_TYPE_PREFETCHT2 && memref.data.addr < 1024) {
        instrs_until_interrupt = static_cast<int>(memref.data.addr);
    }
    if (memref.data.type == TRACE_TYPE_PREFETCHT1 && memref.data.addr < 1024) {
        memrefs_until_interrupt = static_cast<int>(memref.data.addr);
    }

    prev_prev_entry = prev_entry;
    prev_entry = memref;
    return true;
}

bool
trace_invariants_t::print_results()
{
    std::cerr << "Trace invariant checks passed\n";
    return true;
}
