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

/* Keep synched with trace_type_t enum in trace_entry.h.
 */
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

/* Keep synched with trace_version_t enum in trace_entry.h.
 */
const char *const trace_version_names[] = {
    "<unknown>", "<unknown>",   "no_kernel_pc",        "kernel_pc",
    "encodings", "branch_info", "frequent_timestamps",
};

/* Keep synched with trace_marker_type_t enum in trace_entry.h.
 */
const char *const trace_marker_names[] = {
    "marker: kernel xfer",                    /* TRACE_MARKER_TYPE_KERNEL_EVENT */
    "marker: syscall xfer",                   /* TRACE_MARKER_TYPE_KERNEL_XFER */
    "marker: timestamp",                      /* TRACE_MARKER_TYPE_TIMESTAMP */
    "marker: cpu id",                         /* TRACE_MARKER_TYPE_CPU_ID */
    "marker: function",                       /* TRACE_MARKER_TYPE_FUNC_ID */
    "marker: function return address",        /* TRACE_MARKER_TYPE_FUNC_RETADDR */
    "marker: function argument",              /* TRACE_MARKER_TYPE_FUNC_ARG */
    "marker: function return value",          /* TRACE_MARKER_TYPE_FUNC_RETVAL */
    "marker: split value",                    /* TRACE_MARKER_TYPE_SPLIT_VALUE */
    "marker: filetype",                       /* TRACE_MARKER_TYPE_FILETYPE */
    "marker: cache line size",                /* TRACE_MARKER_TYPE_CACHE_LINE_SIZE */
    "marker: instruction count",              /* TRACE_MARKER_TYPE_INSTRUCTION_COUNT */
    "marker: version",                        /* TRACE_MARKER_TYPE_VERSION */
    "marker: rseq abort",                     /* TRACE_MARKER_TYPE_RSEQ_ABORT */
    "marker: window",                         /* TRACE_MARKER_TYPE_WINDOW_ID */
    "marker: physical address",               /* TRACE_MARKER_TYPE_PHYSICAL_ADDRESS */
    "marker: physical address not available", /* TRACE_MARKER_TYPE_PHYSICAL_ADDRESS_NOT_
                                                 AVAILABLE */
    "marker: virtual address",                /* TRACE_MARKER_TYPE_VIRTUAL_ADDRESS */
    "marker: page size",                      /* TRACE_MARKER_TYPE_PAGE_SIZE */
    "marker: system call idx",                /* TRACE_MARKER_TYPE_SYSCALL_IDX */
    "marker: chunk instruction count",        /* TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT */
    "marker: chunk footer",                   /* TRACE_MARKER_TYPE_CHUNK_FOOTER */
    "marker: record ordinal",                 /* TRACE_MARKER_TYPE_RECORD_ORDINAL */
    "marker: filter endpoint",                /* TRACE_MARKER_TYPE_FILTER_ENDPOINT */
    "marker: rseq entry",                     /* TRACE_MARKER_TYPE_RSEQ_ENTRY */
    "marker: system call",                    /* TRACE_MARKER_TYPE_SYSCALL */
    "marker: maybe-blocking system call",  /* TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL */
    "marker: trace start for system call", /* TRACE_MARKER_TYPE_SYSCALL_TRACE_START */
    "marker: trace end for system call",   /* TRACE_MARKER_TYPE_SYSCALL_TRACE_END */
    "marker: indirect branch target",      /* TRACE_MARKER_TYPE_BRANCH_TARGET */
    "marker: system call failed",          /* TRACE_MARKER_TYPE_SYSCALL_FAILED */
    "marker: direct switch to thread",     /* TRACE_MARKER_TYPE_DIRECT_THREAD_SWITCH */
    "marker: wait for another core",       /* TRACE_MARKER_TYPE_CORE_WAIT */
    "marker: core is idle",                /* TRACE_MARKER_TYPE_CORE_IDLE */
    "marker: trace start for context switch", /* TRACE_MARKER_TYPE_CONTEXT_SWITCH_START */
    "marker: trace end for context switch",   /* TRACE_MARKER_TYPE_CONTEXT_SWITCH_END */
    "marker: vector length",                  /* TRACE_MARKER_TYPE_VECTOR_LENGTH */
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: unused",
    "marker: reserved end", /* TRACE_MARKER_TYPE_RESERVED_END */
};

} // namespace drmemtrace
} // namespace dynamorio
