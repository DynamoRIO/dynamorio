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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dr_api.h"
#include "drsyscall_record_lib.h"

int output_file = -1;
int record_file = -1;
uint64_t trim_after_timestamp = UINT64_MAX;
uint64_t trim_before_timestamp = 0;

static int
usage(char *command)
{
    dr_printf("\
Usage: %s\n\
drsyscall_record_trimmer \n\
    -i <input syscall record file> \n\
    -o <output trimmed syscall recode file> \n\
    -a <trim syscall records started after timestamp(us)> \n\
    -b <trim syscall records started before timestamp(us)> \n",
              command);
    return 0;
}

static size_t
read_func(char *buffer, size_t size)
{
    return read(record_file, buffer, size);
}

static bool
write_record(char *buffer, size_t size, const char *type, uint64_t timestamp)
{
    if (timestamp < trim_before_timestamp) {
        return true;
    }
    if (timestamp > trim_after_timestamp) {
        return false;
    }
    const ssize_t wrote = write(output_file, buffer, size);
    if (wrote != size) {
        dr_printf("wrote %ld bytes instead of %ld bytes for %s.", wrote, size, type);
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
        dr_printf("Syscall record type DRSYS_RECORD_END_DEPRECATED "
                  "and DRSYS_SYSCALL_NUMBER_DEPRECATED are not supported.");
        return false;
    case DRSYS_SYSCALL_NUMBER_TIMESTAMP:
        current_record_timestamp = record->syscall_number_timestamp.timestamp;
        return write_record((char *)record, sizeof(*record),
                            "DRSYS_SYSCALL_NUMBER_TIMESTAMP", current_record_timestamp);
    case DRSYS_PRECALL_PARAM:
    case DRSYS_POSTCALL_PARAM:
        return write_record((char *)record, sizeof(*record),
                            (record->type == DRSYS_PRECALL_PARAM
                                 ? "DRSYS_PRECALL_PARAM"
                                 : "DRSYS_POSTCALL_PARAM"),
                            current_record_timestamp);
    case DRSYS_MEMORY_CONTENT:
        if (!write_record((char *)record, sizeof(*record), "DRSYS_MEMORY_CONTENT record",
                          current_record_timestamp)) {
            return false;
        }
        return write_record(buffer, size, "DRSYS_MEMORY_CONTENT content",
                            current_record_timestamp);
    case DRSYS_RETURN_VALUE:
        return write_record((char *)record, sizeof(*record), "DRSYS_RETURN_VALUE",
                            current_record_timestamp);
    case DRSYS_RECORD_END_TIMESTAMP:
        if (!write_record((char *)record, sizeof(*record), "DRSYS_RECORD_END_TIMESTAMP",
                          current_record_timestamp)) {
            return false;
        }
        current_record_timestamp = record->syscall_number_timestamp.timestamp;
        return true;
    default: dr_printf("unknown record type %d\n", record->type); return false;
    }
    return true;
}

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        usage(argv[0]);
        return -1;
    }

    int arg_index = 1;
    while (arg_index < argc) {
        if (!strncmp(argv[arg_index], "--help", 6)) {
            usage(argv[0]);
            return 0;
        }
        if (!strncmp(argv[arg_index], "-i", 2)) {
            record_file = open(argv[++arg_index], O_RDONLY);
            if (record_file == -1) {
                dr_printf("unable to open input file %s.\n", argv[arg_index]);
                return -1;
            }

        } else if (!strncmp(argv[arg_index], "-o", 2)) {
            output_file =
                open(argv[++arg_index], O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
            if (output_file == -1) {
                dr_printf("unable to open output file %s.\n", "blah");
                return -1;
            }
        } else if (!strncmp(argv[arg_index], "-a", 2)) {
            char *end_ptr;
            trim_after_timestamp = strtoull(argv[++arg_index], &end_ptr, /*base=*/10);
            if (end_ptr == NULL) {
                dr_printf("invalid trim after timestamp: %s.\n", argv[arg_index]);
                return -1;
            }
        } else if (!strncmp(argv[arg_index], "-b", 2)) {
            char *end_ptr;
            trim_before_timestamp = strtoull(argv[++arg_index], &end_ptr, /*base=*/10);
            if (end_ptr == NULL) {
                dr_printf("invalid trim before timestamp: %s.\n", argv[arg_index]);
                return -1;
            }
        }
        ++arg_index;
    }

    if (record_file == -1) {
        dr_printf("missing input file name\n");
        return -1;
    }
    if (output_file == -1) {
        dr_printf("missing output file name\n");
        return -1;
    }

    drsyscall_iterate_records(read_func, record_cb);

    close(output_file);
    close(record_file);
    return 0;
}
