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

#include "dr_api.h"
#include "drsyscall_record_lib.h"

int record_file;

static size_t
read_func(char *buffer, size_t size)
{
    return read(record_file, buffer, size);
}

static bool
record_cb(syscall_record_t *record, char *buffer, size_t size)
{
    switch (record->type) {
    case DRSYS_SYSCALL_NUMBER_DEPRECATED:
        dr_printf("syscall: %d\n", record->syscall_number);
        break;
    case DRSYS_PRECALL_PARAM:
    case DRSYS_POSTCALL_PARAM:
        dr_printf("%s-syscall ordinal %d, value " PIFX "\n",
                  (record->type == DRSYS_PRECALL_PARAM ? "pre" : "post"),
                  record->param.ordinal, record->param.value);
        break;
    case DRSYS_MEMORY_CONTENT:
        dr_printf("memory content address " PFX ", size " PIFX "\n    ",
                  record->content.address, record->content.size);
        size_t count = 0;
        for (char *ptr = buffer; count < size; ++count, ++ptr) {
            dr_printf("%02x", *ptr);
            if ((count + 1) % 16 == 0) {
                dr_printf("\n    ");
                continue;
            }
            if ((count + 1) % 4 == 0) {
                dr_printf(" ");
            }
        }
        dr_printf("\n");
        break;
    case DRSYS_RETURN_VALUE:
        dr_printf("return value " PIFX "\n", record->return_value);
        break;
    case DRSYS_RECORD_END_DEPRECATED:
        dr_printf("syscall end: %d\n", record->syscall_number);
        break;
    case DRSYS_SYSCALL_NUMBER_TIMESTAMP:
        dr_printf("syscall: %d, timestamp: %" INT64_FORMAT_CODE "\n",
                  record->syscall_number_timestamp.syscall_number.number,
                  record->syscall_number_timestamp.timestamp);
        break;
    case DRSYS_RECORD_END_TIMESTAMP:
        dr_printf("syscall end: %d, timestamp: %" INT64_FORMAT_CODE "\n",
                  record->syscall_number_timestamp.syscall_number.number,
                  record->syscall_number_timestamp.timestamp);
        break;
    default: dr_printf("unknown record type %d\n", record->type); return false;
    }
    return true;
}

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        dr_printf("The name of the syscall record file is required.\n");
        return -1;
    }

    record_file = open(argv[1], O_RDONLY);
    if (record_file == -1) {
        dr_printf("unable to open file %s\n", argv[1]);
        return -1;
    }

    drsyscall_iterate_records(read_func, record_cb);
}
