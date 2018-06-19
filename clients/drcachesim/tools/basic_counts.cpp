/* **********************************************************
 * Copyright (c) 2017-2018 Google, Inc.  All rights reserved.
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

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <vector>

#include "basic_counts.h"
#include "../common/utils.h"

const std::string basic_counts_t::TOOL_NAME = "Basic counts tool";

analysis_tool_t *
basic_counts_tool_create(unsigned int verbose)
{
    return new basic_counts_t(verbose);
}

basic_counts_t::basic_counts_t(unsigned int verbose) :
    total_threads(0), total_instrs(0), total_instrs_nofetch(0), total_prefetches(0),
    total_loads(0), total_stores(0), total_sched_markers(0), total_xfer_markers(0),
    total_func_markers(0), total_other_markers(0), knob_verbose(verbose)
{
    // Empty.
}

basic_counts_t::~basic_counts_t()
{
    // Empty.
}

static bool is_func_marker(trace_marker_type_t marker_type)
{
  return marker_type == TRACE_MARKER_TYPE_FUNC_MALLOC_RETADDR ||
         marker_type == TRACE_MARKER_TYPE_FUNC_MALLOC_ARG ||
         marker_type == TRACE_MARKER_TYPE_FUNC_MALLOC_RETVAL;
}

bool
basic_counts_t::process_memref(const memref_t &memref)
{
  if (type_is_instr(memref.instr.type)) {
      ++thread_instrs[memref.instr.tid];
      ++total_instrs;
  } else if (memref.data.type == TRACE_TYPE_INSTR_NO_FETCH) {
      ++thread_instrs_nofetch[memref.instr.tid];
      ++total_instrs_nofetch;
  } else if (type_is_prefetch(memref.data.type)) {
      ++thread_prefetches[memref.data.tid];
      ++total_prefetches;
  } else if (memref.data.type == TRACE_TYPE_READ) {
      ++thread_loads[memref.data.tid];
      ++total_loads;
  } else if (memref.data.type == TRACE_TYPE_WRITE) {
      ++thread_stores[memref.data.tid];
      ++total_stores;
  } else if (memref.marker.type == TRACE_TYPE_MARKER) {
      if (memref.marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP ||
          memref.marker.marker_type == TRACE_MARKER_TYPE_CPU_ID) {
          ++thread_sched_markers[memref.data.tid];
          ++total_sched_markers;
      } else if (memref.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT ||
                 memref.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_XFER) {
          ++thread_xfer_markers[memref.data.tid];
          ++total_xfer_markers;
      } else if (is_func_marker(memref.marker.marker_type)) {
          ++thread_func_markers[memref.data.tid];
          ++total_func_markers;
      } else {
          ++thread_other_markers[memref.data.tid];
          ++total_other_markers;
      }
  } else if (memref.exit.type == TRACE_TYPE_THREAD_EXIT) {
      ++total_threads;
  }
  return true;
}

static bool
cmp_val(const std::pair<memref_tid_t, int_least64_t> &l,
        const std::pair<memref_tid_t, int_least64_t> &r)
{
    return (l.second > r.second);
}

bool
basic_counts_t::print_results() {
    std::cerr << TOOL_NAME << " results:\n";
    std::cerr << "Total counts:\n";
    std::cerr << std::setw(12) << total_instrs << " total (fetched) instructions\n";
    std::cerr << std::setw(12) << total_instrs_nofetch <<
        " total non-fetched instructions\n";
    std::cerr << std::setw(12) << total_prefetches << " total prefetches\n";
    std::cerr << std::setw(12) << total_loads << " total data loads\n";
    std::cerr << std::setw(12) << total_stores << " total data stores\n";
    std::cerr << std::setw(12) << total_threads << " total threads\n";
    std::cerr << std::setw(12) << total_sched_markers << " total scheduling markers\n";
    std::cerr << std::setw(12) << total_xfer_markers << " total transfer markers\n";
    std::cerr << std::setw(12) << total_func_markers << " total function markers\n";
    std::cerr << std::setw(12) << total_other_markers << " total other markers\n";

    // Print the threads sorted by instrs.
    std::vector<std::pair<memref_tid_t, int_least64_t>> sorted(thread_instrs.begin(),
                                                               thread_instrs.end());
    std::sort(sorted.begin(), sorted.end(), cmp_val);
    for (const auto& keyvals : sorted) {
        memref_tid_t tid = keyvals.first;
        std::cerr << "Thread " << tid << " counts:\n";
        std::cerr << std::setw(12) << thread_instrs[tid] << " (fetched) instructions\n";
        std::cerr << std::setw(12) << thread_instrs_nofetch[tid] <<
            " non-fetched instructions\n";
        std::cerr << std::setw(12) << thread_prefetches[tid] << " prefetches\n";
        std::cerr << std::setw(12) << thread_loads[tid] << " data loads\n";
        std::cerr << std::setw(12) << thread_stores[tid] << " data stores\n";
        std::cerr << std::setw(12) << thread_sched_markers[tid] <<
            " scheduling markers\n";
        std::cerr << std::setw(12) << thread_xfer_markers[tid] << " transfer markers\n";
        std::cerr << std::setw(12) << thread_func_markers[tid] << " function markers\n";
        std::cerr << std::setw(12) << thread_other_markers[tid] << " other markers\n";
    }
    return true;
}
