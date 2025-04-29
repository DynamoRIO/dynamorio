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

#include <stdio.h>
#include <unistd.h>

#include "drsyscall.h"
#include "drsyscall_record.h"

DR_EXPORT
int
drsyscall_read_record_file(DR_PARAM_IN FILE *output_stream,
                           DR_PARAM_IN FILE *error_stream, DR_PARAM_IN int record_file)
{
    syscall_record_t record = {};
    while (read(record_file, &record, sizeof(record)) == sizeof(record)) {
        switch (record.type) {
        case DRSYS_SYSCALL_NUMBER:
            fprintf(output_stream, "syscall: %d\n", record.syscall_number);
            break;
        case DRSYS_PRECALL_PARAM:
        case DRSYS_POSTCALL_PARAM:
            fprintf(output_stream, "%s-syscall ordinal %d, value " PIFX "\n",
                    (record.type == DRSYS_PRECALL_PARAM ? "pre" : "post"),
                    record.param.ordinal, record.param.value);
            break;
        case DRSYS_MEMORY_CONTENT:
            fprintf(output_stream, "memory content address " PFX ", size " PIFX "\n    ",
                    record.content.address, record.content.size);
            char *buffer = (char *)malloc(record.content.size);
            if (buffer == NULL) {
                fprintf(error_stream,
                        "failed to allocate buffer to read the record file.\n");
                return -1;
            }
            if (read(record_file, buffer, record.content.size) != record.content.size) {
                fprintf(error_stream,
                        "failed to read " PIFX " bytes from the record file.\n",
                        sizeof(buffer));
                return -1;
            }
            char *ptr = buffer;
            for (size_t count = 0; count < record.content.size; ++count, ++ptr) {
                fprintf(output_stream, "%02x", *ptr);
                if ((count + 1) % 16 == 0) {
                    fprintf(output_stream, "\n    ");
                    continue;
                }
                if ((count + 1) % 4 == 0) {
                    fprintf(output_stream, " ");
                }
            }
            free(buffer);
            fprintf(output_stream, "\n");
            break;
        case DRSYS_RETURN_VALUE:
            fprintf(output_stream, "return value " PIFX "\n", record.return_value);
            break;
        case DRSYS_RECORD_END:
            fprintf(output_stream, "syscall end: %d\n", record.syscall_number);
            break;
        default:
            fprintf(error_stream, "unknown record type %d\n", record.type);
            return -1;
        }
    }
    return 0;
}

DR_EXPORT
int
drsyscall_write_param_record(DR_PARAM_IN int record_file, DR_PARAM_IN drsys_arg_t *arg)
{
    if (!arg->valid) {
        return 0;
    }

    // Ordinal is set to -1 for return value.
    if (arg->ordinal == -1) {
        if (arg->pre) {
            // Ordinal should not be -1 for pre-syscall.
            return -1;
        }
        syscall_record_t record = {};
        record.type = DRSYS_RETURN_VALUE;
        record.return_value = arg->value64;
        return write(record_file, &record, sizeof(record));
    }
    syscall_record_t record = {};
    record.type = arg->pre ? DRSYS_PRECALL_PARAM : DRSYS_POSTCALL_PARAM;
    record.param.ordinal = arg->ordinal;
    record.param.value = arg->value64;
    return write(record_file, &record, sizeof(record));
}

DR_EXPORT
int
drsyscall_write_memarg_record(DR_PARAM_IN int record_file, DR_PARAM_IN drsys_arg_t *arg)
{
    if (!arg->valid) {
        return 0;
    }

    if ((arg->pre && arg->mode & DRSYS_PARAM_IN) ||
        (!arg->pre && arg->mode & DRSYS_PARAM_OUT)) {
        syscall_record_t record = {};
        record.type = DRSYS_MEMORY_CONTENT;
        record.content.address = arg->start_addr;
        record.content.size = arg->size;
        if (write(record_file, &record, sizeof(record)) != sizeof(record)) {
            return -1;
        }
        return write(record_file, arg->start_addr, arg->size);
    }
    return 0;
}

DR_EXPORT
int
drsyscall_write_syscall_number_record(DR_PARAM_IN int record_file, DR_PARAM_IN int sysnum)
{
    syscall_record_t record = {};
    record.type = DRSYS_SYSCALL_NUMBER;
    record.syscall_number = sysnum;
    return write(record_file, &record, sizeof(record));
}

DR_EXPORT
int
drsyscall_write_syscall_end_record(DR_PARAM_IN int record_file, DR_PARAM_IN int sysnum)
{
    syscall_record_t record = {};
    record.type = DRSYS_RECORD_END;
    record.syscall_number = sysnum;
    return write(record_file, &record, sizeof(record));
}
