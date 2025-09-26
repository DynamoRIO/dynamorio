/* **********************************************************
 * Copyright (c) 2025 Google, Inc.  All rights reserved.
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
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
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

/*
 * Standalone syscall record trimming tool.
 *
 * Usage:
 * drsyscall_record_trimmer -input_file <input syscall record file>
 *                          -output_file <output trimmed syscall record file>
 *                          -trim_after_timestamp
 *                            <timestamp in microseconds since Jan 1, 1601>
 *                          -trim_before_timestamp
 *                            <timestamp in microseconds since Jan 1, 1601>
 *
 * Each syscall starts with a DRSYS_SYSCALL_NUMBER_TIMESTAMP record and ends
 * with a DRSYS_RECORD_END_TIMESTAMP record (exception: exit_group has no end record).
 *
 * To prevent partial syscall records in the output file, only the timestamp
 * of the DRSYS_SYSCALL_NUMBER_TIMESTAMP record is used for trimming decisions.
 *
 * If trim_before_timestamp falls within the syscall boundary (between start/end),
 * all its records are filtered out.
 * If trim_after_timestamp falls within the syscall boundary, all its records are kept in
 * the output file.
 *
 */
#include <fcntl.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dr_api.h"
#include "dr_tools.h"
#include "droption.h"
#include "drsyscall_record_lib.h"

namespace {

using ::dynamorio::droption::droption_parser_t;
using ::dynamorio::droption::DROPTION_SCOPE_ALL;
using ::dynamorio::droption::DROPTION_SCOPE_FRONTEND;
using ::dynamorio::droption::droption_t;

droption_t<std::string> op_input_file(DROPTION_SCOPE_FRONTEND, "input_file", "",
                                      "Input syscall record file",
                                      "Input syscall record file");
droption_t<std::string> op_output_file(DROPTION_SCOPE_FRONTEND, "output_file", "",
                                       "Output trimmed syscall record file",
                                       "Output trimmed syscall record file");
droption_t<uint64_t> op_trim_after_timestamp(
    DROPTION_SCOPE_FRONTEND, "trim_after_timestamp", UINT64_MAX,
    "Trim syscall records started until this timestamp (in us).",
    "Remove all syscall records started until this timestamp (in us).");
droption_t<uint64_t> op_trim_before_timestamp(
    DROPTION_SCOPE_FRONTEND, "trim_before_timestamp", 0,
    "Trim syscall records started after this timestamp (in us).",
    "Remove all syscall records started after this timestamp (in us). "
    "drsyscall_record_viewer can be used to read the syscall record file and retrieve "
    "the specfic timestamps for trimming the syscall record file.");

int output_file = INVALID_FILE;
int record_file = INVALID_FILE;
uint64_t trim_after_timestamp = UINT64_MAX;
uint64_t trim_before_timestamp = 0;

static size_t
read_func(char *buffer, size_t size)
{
    return dr_read_file(record_file, buffer, size);
}

static bool
write_record_if_not_filtered(char *buffer, size_t size, const char *type,
                             uint64_t timestamp)
{
    if (timestamp < trim_before_timestamp) {
        return true;
    }
    if (timestamp > trim_after_timestamp) {
        return false;
    }
    const ssize_t wrote = dr_write_file(output_file, buffer, size);
    if (wrote != static_cast<ssize_t>(size)) {
        std::cerr << "wrote " << wrote << " bytes instead of " << size
                  << " bytes for type " << type << "\n";
        return false;
    }
    return true;
}

static bool
record_cb(syscall_record_t *record, char *buffer, size_t size)
{
    static uint64_t current_record_timestamp = 0;
    switch (record->type) {
    case DRSYS_RECORD_END_DEPRECATED:
    case DRSYS_SYSCALL_NUMBER_DEPRECATED:
        std::cerr << "Syscall record type DRSYS_RECORD_END_DEPRECATED "
                     "and DRSYS_SYSCALL_NUMBER_DEPRECATED are not supported.\n";
        return false;
    case DRSYS_SYSCALL_NUMBER_TIMESTAMP:
        current_record_timestamp = record->syscall_number_timestamp.timestamp;
        return write_record_if_not_filtered((char *)record, sizeof(*record),
                                            "DRSYS_SYSCALL_NUMBER_TIMESTAMP",
                                            current_record_timestamp);
    case DRSYS_PRECALL_PARAM:
    case DRSYS_POSTCALL_PARAM:
        return write_record_if_not_filtered((char *)record, sizeof(*record),
                                            (record->type == DRSYS_PRECALL_PARAM
                                                 ? "DRSYS_PRECALL_PARAM"
                                                 : "DRSYS_POSTCALL_PARAM"),
                                            current_record_timestamp);
    case DRSYS_MEMORY_CONTENT:
        if (!write_record_if_not_filtered((char *)record, sizeof(*record),
                                          "DRSYS_MEMORY_CONTENT record",
                                          current_record_timestamp)) {
            return false;
        }
        return write_record_if_not_filtered(buffer, size, "DRSYS_MEMORY_CONTENT content",
                                            current_record_timestamp);
    case DRSYS_RETURN_VALUE:
        return write_record_if_not_filtered((char *)record, sizeof(*record),
                                            "DRSYS_RETURN_VALUE",
                                            current_record_timestamp);
    case DRSYS_RECORD_END_TIMESTAMP:
        if (!write_record_if_not_filtered((char *)record, sizeof(*record),
                                          "DRSYS_RECORD_END_TIMESTAMP",
                                          current_record_timestamp)) {
            return false;
        }
        return true;
    default: std::cerr << "unknown record type " << record->type << "\n"; return false;
    }
    return true;
}

} // namespace

int
main(int argc, const char *argv[])
{
    dr_standalone_init();
    int last_index;
    std::string parse_err;
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_FRONTEND, argc, argv, &parse_err,
                                       &last_index)) {
        std::cerr << "Usage error: " << parse_err << "\nUsage:\n " << argv[0]
                  << "\nOptions:\n"
                  << droption_parser_t::usage_short(DROPTION_SCOPE_ALL);
        return 1;
    }

    if (op_input_file.get_value().empty()) {
        std::cerr << "missing input file name.\n";
        return 1;
    }
    if (op_output_file.get_value().empty()) {
        std::cerr << "missing output file name.\n";
        return 1;
    }
    record_file = dr_open_file(op_input_file.get_value().c_str(), DR_FILE_READ);
    if (record_file == INVALID_FILE) {
        std::cerr << "failed to open " << op_input_file.get_value() << "\n";
        return 1;
    }
    output_file =
        dr_open_file(op_output_file.get_value().c_str(), DR_FILE_WRITE_OVERWRITE);
    if (output_file == INVALID_FILE) {
        std::cerr << "failed to open " << op_output_file.get_value() << "\n";
        return 1;
    }
    trim_after_timestamp = op_trim_after_timestamp.get_value();
    trim_before_timestamp = op_trim_before_timestamp.get_value();

    const bool success = drsyscall_iterate_records(read_func, record_cb);
    dr_close_file(output_file);
    dr_close_file(record_file);
    if (!success) {
        std::cerr << "failed to iterate syscall records\n";
    }
    return 0;
}
