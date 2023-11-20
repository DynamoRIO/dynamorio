/* **********************************************************
 * Copyright (c) 2016-2023 Google, Inc.  All rights reserved.
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

#include "raw2trace_shared.h"

#include <cstddef>
#include <string>
#include <sstream>
#include <iostream>

#include "drmemtrace.h"
#include "trace_entry.h"
#include "utils.h"

namespace dynamorio {
namespace drmemtrace {

bool
trace_metadata_reader_t::is_thread_start(const offline_entry_t *entry,
                                         DR_PARAM_OUT std::string *error,
                                         DR_PARAM_OUT int *version,
                                         DR_PARAM_OUT offline_file_type_t *file_type)
{
    *error = "";
    if (entry->extended.type != OFFLINE_TYPE_EXTENDED ||
        (entry->extended.ext != OFFLINE_EXT_TYPE_HEADER_DEPRECATED &&
         entry->extended.ext != OFFLINE_EXT_TYPE_HEADER)) {
        return false;
    }
    int ver;
    offline_file_type_t type;
    if (entry->extended.ext == OFFLINE_EXT_TYPE_HEADER_DEPRECATED) {
        ver = static_cast<int>(entry->extended.valueA);
        type = static_cast<offline_file_type_t>(entry->extended.valueB);
        if (ver >= OFFLINE_FILE_VERSION_HEADER_FIELDS_SWAP) {
            return false;
        }
    } else {
        ver = static_cast<int>(entry->extended.valueB);
        type = static_cast<offline_file_type_t>(entry->extended.valueA);
        if (ver < OFFLINE_FILE_VERSION_HEADER_FIELDS_SWAP) {
            return false;
        }
    }
    type = static_cast<offline_file_type_t>(static_cast<int>(type) |
                                            OFFLINE_FILE_TYPE_ENCODINGS);
    if (version != nullptr)
        *version = ver;
    if (file_type != nullptr)
        *file_type = type;
    if (ver < OFFLINE_FILE_VERSION_OLDEST_SUPPORTED || ver > OFFLINE_FILE_VERSION) {
        std::stringstream ss;
        ss << "Version mismatch: found " << ver << " but we require between "
           << OFFLINE_FILE_VERSION_OLDEST_SUPPORTED << " and " << OFFLINE_FILE_VERSION;
        *error = ss.str();
        return false;
    }
    if (TESTANY(OFFLINE_FILE_TYPE_ARCH_ALL, type) &&
        !TESTANY(build_target_arch_type(), type)) {
        std::stringstream ss;
        ss << "Architecture mismatch: trace recorded on " << trace_arch_string(type)
           << " but tools built for " << trace_arch_string(build_target_arch_type());
        *error = ss.str();
        return false;
    }
    return true;
}

std::string
trace_metadata_reader_t::check_entry_thread_start(const offline_entry_t *entry)
{
    std::string error;
    if (is_thread_start(entry, &error, nullptr, nullptr))
        return "";
    if (error.empty())
        return "Thread log file is corrupted: missing version entry";
    return error;
}

drmemtrace_status_t
drmemtrace_get_timestamp_from_offline_trace(const void *trace, size_t trace_size,
                                            DR_PARAM_OUT uint64 *timestamp)
{
    if (trace == nullptr || timestamp == nullptr)
        return DRMEMTRACE_ERROR_INVALID_PARAMETER;

    const offline_entry_t *offline_entries =
        reinterpret_cast<const offline_entry_t *>(trace);
    size_t size = trace_size / sizeof(offline_entry_t);
    if (size < 1)
        return DRMEMTRACE_ERROR_INVALID_PARAMETER;

    std::string error;
    if (!trace_metadata_reader_t::is_thread_start(offline_entries, &error, nullptr,
                                                  nullptr) &&
        !error.empty())
        return DRMEMTRACE_ERROR_INVALID_PARAMETER;
    size_t timestamp_pos = 0;
    while (timestamp_pos < size &&
           offline_entries[timestamp_pos].timestamp.type != OFFLINE_TYPE_TIMESTAMP) {
        if (timestamp_pos > 15) // Something is wrong if we've gone this far.
            return DRMEMTRACE_ERROR_INVALID_PARAMETER;
        // We only expect header-type entries.
        int type = offline_entries[timestamp_pos].tid.type;
        if (type != OFFLINE_TYPE_THREAD && type != OFFLINE_TYPE_PID &&
            type != OFFLINE_TYPE_EXTENDED)
            return DRMEMTRACE_ERROR_INVALID_PARAMETER;
        ++timestamp_pos;
    }
    if (timestamp_pos == size)
        return DRMEMTRACE_ERROR_INVALID_PARAMETER;

    *timestamp = offline_entries[timestamp_pos].timestamp.usec;
    return DRMEMTRACE_SUCCESS;
}

} // namespace drmemtrace
} // namespace dynamorio
