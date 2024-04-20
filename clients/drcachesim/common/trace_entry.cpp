/* **********************************************************
 * Copyright (c) 2015-2023 Google, Inc.  All rights reserved.
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

#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

const char *const trace_type_names[] = {
    "read",
    "write",
    "prefetch",
    "prefetch_read_l1",
    "prefetch_read_l2",
    "prefetch_read_l3",
    "prefetchnta",
    "prefetch_read",
    "prefetch_write",
    "prefetch_instr",
    "instr",
    "direct_jump",
    "indirect_jump",
    "conditional_jump",
    "direct_call",
    "indirect_call",
    "return",
    "instr_bundle",
    "instr_flush",
    "instr_flush_end",
    "data_flush",
    "data_flush_end",
    "thread",
    "thread_exit",
    "pid",
    "header",
    "footer",
    "hw prefetch",
    "marker",
    "non-fetched instr",
    "maybe-fetched instr",
    "sysenter",
    "prefetch_read_l1_nt",
    "prefetch_read_l2_nt",
    "prefetch_read_l3_nt",
    "prefetch_instr_l1",
    "prefetch_instr_l1_nt",
    "prefetch_instr_l2",
    "prefetch_instr_l2_nt",
    "prefetch_instr_l3",
    "prefetch_instr_l3_nt",
    "prefetch_write_l1",
    "prefetch_write_l1_nt",
    "prefetch_write_l2",
    "prefetch_write_l2_nt",
    "prefetch_write_l3",
    "prefetch_write_l3_nt",
    "encoding",
    "taken_jump",
    "untaken_jump",
};

const char *const trace_version_names[] = {
    "<unknown>",
    "<unknown>",
    "trace_entry_version_no_kernel_pc",
    "trace_entry_version_kernel_pc",
    "trace_entry_version_encodings",
    "trace_entry_version_branch_info",
    "trace_entry_version_frequent_timestamps",
};

const char *const trace_marker_names[] = {
    "trace_marker_type_kernel_event",
    "trace_marker_type_kernel_xfer",
    "trace_marker_type_timestamp",
    "trace_marker_type_cpu_id",
    "trace_marker_type_func_id",
    "trace_marker_type_func_retaddr",
    "trace_marker_type_func_arg",
    "trace_marker_type_func_retval",
    "trace_marker_type_split_value",
    "trace_marker_type_filetype",
    "trace_marker_type_cache_line_size",
    "trace_marker_type_instruction_count",
    "trace_marker_type_version",
    "trace_marker_type_rseq_abort",
    "trace_marker_type_window_id",
    "trace_marker_type_physical_address",
    "trace_marker_type_physical_address_not_available",
    "trace_marker_type_virtual_address",
    "trace_marker_type_page_size",
    "trace_marker_type_syscall_idx",
    "trace_marker_type_chunk_instr_count",
    "trace_marker_type_chunk_footer",
    "trace_marker_type_record_ordinal",
    "trace_marker_type_filter_endpoint",
    "trace_marker_type_rseq_entry",
    "trace_marker_type_syscall",
    "trace_marker_type_maybe_blocking_syscall",
    "trace_marker_type_syscall_trace_start",
    "trace_marker_type_syscall_trace_end",
    "trace_marker_type_branch_target",
    "trace_marker_type_syscall_failed",
    "trace_marker_type_direct_thread_switch",
    "trace_marker_type_core_wait",
    "trace_marker_type_core_idle",
    "trace_marker_type_context_switch_start",
    "trace_marker_type_context_switch_end",
    "trace_marker_type_vector_length",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_unused",
    "trace_marker_type_reserved_end",
};

} // namespace drmemtrace
} // namespace dynamorio
