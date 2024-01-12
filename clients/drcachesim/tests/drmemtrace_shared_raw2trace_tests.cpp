/* **********************************************************
 * Copyright (c) 2023 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Unit tests for drmemtrace APIs implemented by shared raw2trace code. */

#include "dr_api.h"
#include "drmemtrace.h"
#include "trace_entry.h"
#include "raw2trace_shared.h"

namespace dynamorio {
namespace drmemtrace {

offline_entry_t
make_header(int version = OFFLINE_FILE_VERSION,
            offline_file_type_t file_type = static_cast<offline_file_type_t>(
                OFFLINE_FILE_TYPE_DEFAULT | OFFLINE_FILE_TYPE_ENCODINGS |
                OFFLINE_FILE_TYPE_SYSCALL_NUMBERS))
{
    offline_entry_t entry;
    entry.extended.type = OFFLINE_TYPE_EXTENDED;
    entry.extended.ext = OFFLINE_EXT_TYPE_HEADER;
    entry.extended.valueA = file_type;
    entry.extended.valueB = version;
    return entry;
}

offline_entry_t
make_deprecated_header(int version, offline_file_type_t file_type)
{
    offline_entry_t entry;
    entry.extended.type = OFFLINE_TYPE_EXTENDED;
    entry.extended.ext = OFFLINE_EXT_TYPE_HEADER_DEPRECATED;
    entry.extended.valueA = version;
    entry.extended.valueB = file_type;
    return entry;
}

offline_entry_t
make_timestamp(uint64_t value)
{
    offline_entry_t entry;
    entry.timestamp.type = OFFLINE_TYPE_TIMESTAMP;
    entry.timestamp.usec = value;
    return entry;
}

bool
test_get_timestamp()
{
    offline_entry_t only_timestamp[1] = { make_timestamp(456) };
    uint64 timestamp = 0;
    if (drmemtrace_get_timestamp_from_offline_trace(only_timestamp,
                                                    BUFFER_SIZE_BYTES(only_timestamp),
                                                    &timestamp) != DRMEMTRACE_SUCCESS)
        return false;
    if (timestamp != 456)
        return false;
    offline_entry_t header_and_timestamp[2] = { make_header(), make_timestamp(123) };
    if (drmemtrace_get_timestamp_from_offline_trace(
            header_and_timestamp, BUFFER_SIZE_BYTES(header_and_timestamp), &timestamp) !=
        DRMEMTRACE_SUCCESS)
        return false;
    return timestamp == 123;
}

bool
test_check_entry_thread_start()
{
    offline_entry_t header = make_header();
    if (trace_metadata_reader_t::check_entry_thread_start(&header) != "")
        return false;
    offline_entry_t timestamp = make_timestamp(1);
    if (trace_metadata_reader_t::check_entry_thread_start(&timestamp) == "")
        return false;
    return true;
}

bool
test_is_thread_start()
{
    int expected_version = OFFLINE_FILE_VERSION;
    offline_file_type_t expected_type = static_cast<offline_file_type_t>(
        OFFLINE_FILE_TYPE_SYSCALL_NUMBERS | OFFLINE_FILE_TYPE_ENCODINGS);
    offline_entry_t header = make_header(expected_version, expected_type);

    int version;
    offline_file_type_t type;
    std::string error;
    if (!trace_metadata_reader_t::is_thread_start(&header, &error, &version, &type) ||
        error != "")
        return false;
    if (!(version == expected_version && type == expected_type))
        return false;

    expected_version = OFFLINE_FILE_VERSION_KERNEL_INT_PC;
    expected_type = static_cast<offline_file_type_t>(OFFLINE_FILE_TYPE_DEFAULT |
                                                     OFFLINE_FILE_TYPE_ENCODINGS);
    offline_entry_t deprecated_header =
        make_deprecated_header(expected_version, expected_type);

    if (!trace_metadata_reader_t::is_thread_start(&deprecated_header, &error, &version,
                                                  &type) ||
        error != "")
        return false;
    if (!(version == expected_version && type == expected_type))
        return false;
    return true;
}

int
test_main(int argc, const char *argv[])
{
    if (!test_get_timestamp() || !test_check_entry_thread_start() ||
        !test_is_thread_start())
        return 1;
    fprintf(stderr, "Success\n");
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
