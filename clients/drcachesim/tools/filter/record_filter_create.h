/* **********************************************************
 * Copyright (c) 2024 Google, Inc.  All rights reserved.
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

#ifndef _RECORD_FILTER_CREATE_H_
#define _RECORD_FILTER_CREATE_H_ 1

#include "analysis_tool.h"

namespace dynamorio {
namespace drmemtrace {

/**
 * @file drmemtrace/record_filter_create.h
 * @brief DrMemtrace record filter trace analysis tool creation.
 */

/**
 * Creates a record analysis tool that filters the #trace_entry_t records of an offline
 * trace. Streams through each shard independenty and parallelly, and writes the
 * filtered version to the output directory with the same base name. Serial mode is not
 * yet supported. The options specify the filter(s) to employ.
 *
 * @param[in] output_dir  The destination directory for the new filtered trace.
 * @param[in] stop_timestamp  Disables filtering (outputs everything) once a timestamp
 *   equal to or greater than this value is seen.
 * @param[in] cache_filter_size  Enables a data cache filter with the given size in
 *   bytes with 64-byte lines and a direct mapped LRU cache.
 * @param[in] remove_trace_types  A comma-separated list of integers of #trace_type_t
 *   types to remove.
 * @param[in] remove_marker_types  A comma-separated list of integers of
 *   #trace_marker_type_t marker types to remove.
 * @param[in] trim_before_timestamp  Trim records from the trace's initial timestamp
 *   up to its first timestamp whose value is greater or equal to this parameter.
 * @param[in] trim_after_timestamp  Trim records after the trace's first timestamp
 *   whose value is greater than this parameter.
 * @param[in] trim_before_instr  Trim records from the trace's initial timestamp up to
 * the first timestamp that follows the instruction ordinal specified by this parameter.
 * @param[in] trim_after_instr  Trim records after the trace's first timestamp that
 * follows the instruction ordinal specified by this parameter.
 * @param[in] encodings2regdeps  If true, converts instruction encodings from the real ISA
 *   of the input trace to the #DR_ISA_REGDEPS synthetic ISA.
 * @param[in] keep_func_ids  A comma-separated list of integers representing the
 *   function IDs related to #TRACE_MARKER_TYPE_FUNC_ID (and _ARG, _RETVAL, _RETADDR)
 *   markers to preserve in the trace, while removing all other function markers.
 * @param[in] modify_marker_value A list of comma-separated pairs of integers representing
 *   <TRACE_MARKER_TYPE_, new_value> to modify the value of all listed TRACE_MARKER_TYPE_
 *   in the trace with their corresponding new_value.
 * @param[in] verbose  Verbosity level for notifications.
 */
record_analysis_tool_t *
record_filter_tool_create(const std::string &output_dir, uint64_t stop_timestamp,
                          int cache_filter_size, const std::string &remove_trace_types,
                          const std::string &remove_marker_types,
                          uint64_t trim_before_timestamp, uint64_t trim_after_timestamp,
                          uint64_t trim_before_instr, uint64_t trim_after_instr,
                          bool encodings2regdeps, const std::string &keep_func_ids,
                          const std::string &modify_marker_value, unsigned int verbose);

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _RECORD_FILTER_CREATE_H_ */
