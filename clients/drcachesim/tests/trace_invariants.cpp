/* **********************************************************
 * Copyright (c) 2017 Google, Inc.  All rights reserved.
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
{
    memset(&prev_instr, 0, sizeof(prev_instr));
    memset(&prev_marker, 0, sizeof(prev_marker));
}

trace_invariants_t::~trace_invariants_t()
{
}

bool
trace_invariants_t::process_memref(const memref_t &memref)
{
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
        // Invariant: offline traces guarantee that a branch target must immediately
        // follow the branch w/ no intervening trace switch.
        if (knob_offline && type_is_instr_branch(prev_instr.instr.type)) {
            assert(prev_instr.instr.tid == memref.instr.tid ||
                   // For limited-window traces a thread might exit after a branch.
                   thread_exited[prev_instr.instr.tid]);
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
                // Kernel-mediated.
                (prev_marker.instr.tid == memref.instr.tid &&
                 (prev_marker.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT ||
                  prev_marker.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_XFER)) ||
                prev_instr.instr.type == TRACE_TYPE_INSTR_SYSENTER);
        }
        prev_instr = memref;
    }
    if (memref.marker.type == TRACE_TYPE_MARKER) {
        if (knob_verbose >= 3) {
            std::cerr << "::" << memref.data.pid << ":" << memref.data.tid << ":: "
                      << "marker type " << memref.marker.marker_type << " value "
                      << memref.marker.marker_value << "\n";
        }
        prev_marker = memref;
        // Clear prev_instr to avoid a branch-gap failure above for things like
        // wow64 call* NtContinue syscall.
        memset(&prev_instr, 0, sizeof(prev_instr));
    }
    return true;
}

bool
trace_invariants_t::print_results()
{
    std::cerr << "Trace invariant checks passed\n";
    return true;
}
