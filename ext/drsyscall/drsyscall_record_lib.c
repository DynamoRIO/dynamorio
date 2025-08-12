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

#include <string.h>

#include "drsyscall.h"
#include "drsyscall_record.h"
#include "drsyscall_record_lib.h"
#include "utils.h"

#define MAX_BUFFER_SIZE 8192

DR_EXPORT
bool
drsyscall_iterate_records(drsyscall_record_read_t read_func,
                          drsyscall_iter_record_cb_t record_cb)
{
    if (read_func == NULL) {
        return false;
    }
    const size_t buffer_size = MAX_BUFFER_SIZE;
    char *buffer = (char *)global_alloc(buffer_size, HEAPSTAT_MISC);
    int offset = 0;
    size_t remaining = 0;
    while (true) {
        if (remaining > 0) {
            // Move the remaining bytes to the beginning of the buffer and read more to
            // fill the buffer.
            memmove(buffer, buffer + offset, remaining);
            remaining += read_func(buffer + remaining, buffer_size - remaining);
            offset = 0;
        } else {
            remaining = read_func(buffer + offset, buffer_size - offset);
            if (remaining == 0) {
                global_free(buffer, buffer_size, HEAPSTAT_MISC);
                return true;
            }
        }
        if (remaining < sizeof(syscall_record_t)) {
            global_free(buffer, buffer_size, HEAPSTAT_MISC);
            return false;
        }
        while (remaining >= sizeof(syscall_record_t)) {
            syscall_record_t *record = (syscall_record_t *)(buffer + offset);
            switch (record->type) {
            case DRSYS_SYSCALL_NUMBER_DEPRECATED:
            case DRSYS_PRECALL_PARAM:
            case DRSYS_POSTCALL_PARAM:
            case DRSYS_RETURN_VALUE:
            case DRSYS_RECORD_END_DEPRECATED:
            case DRSYS_SYSCALL_NUMBER_TIMESTAMP:
            case DRSYS_RECORD_END_TIMESTAMP:
                if (!record_cb(record, NULL, 0)) {
                    global_free(buffer, buffer_size, HEAPSTAT_MISC);
                    return true;
                }
                offset += sizeof(syscall_record_t);
                remaining -= sizeof(syscall_record_t);
                break;
            case DRSYS_MEMORY_CONTENT: {
                const size_t content_size = record->content.size;
                if (remaining >= sizeof(syscall_record_t) + content_size) {
                    if (!record_cb(record, (char *)record + sizeof(syscall_record_t),
                                   content_size)) {
                        return true;
                    }
                    offset += sizeof(syscall_record_t) + content_size;
                    remaining -= sizeof(syscall_record_t) + content_size;
                    break;
                }
                const size_t remaining_bytes =
                    content_size - (remaining - sizeof(syscall_record_t));
                char *content = (char *)global_alloc(content_size, HEAPSTAT_MISC);
                memcpy(content, (char *)record + sizeof(syscall_record_t),
                       remaining - sizeof(syscall_record_t));
                if (read_func(content + remaining - sizeof(syscall_record_t),
                              remaining_bytes) < remaining_bytes) {
                    global_free(content, remaining_bytes, HEAPSTAT_MISC);
                    global_free(buffer, buffer_size, HEAPSTAT_MISC);
                    return false;
                }
                if (!record_cb(record, content, content_size)) {
                    global_free(content, remaining_bytes, HEAPSTAT_MISC);
                    global_free(buffer, buffer_size, HEAPSTAT_MISC);
                    return true;
                }
                global_free(content, content_size, HEAPSTAT_MISC);
                offset = 0;
                remaining = 0;
            } break;
            default: global_free(buffer, buffer_size, HEAPSTAT_MISC); return false;
            }
        }
    }
    global_free(buffer, buffer_size, HEAPSTAT_MISC);
    return true;
}

DR_EXPORT
int
drsyscall_write_param_record(DR_PARAM_IN drsyscall_record_write_t write_func,
                             DR_PARAM_IN drsys_arg_t *arg)
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
        return write_func((char *)&record, sizeof(record));
    }
    syscall_record_t record = {};
    record.type = arg->pre ? DRSYS_PRECALL_PARAM : DRSYS_POSTCALL_PARAM;
    record.param.ordinal = arg->ordinal;
    record.param.value = arg->value64;
    return write_func((char *)&record, sizeof(record));
}

DR_EXPORT
int
drsyscall_write_memarg_record(DR_PARAM_IN drsyscall_record_write_t write_func,
                              DR_PARAM_IN drsys_arg_t *arg)
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
        if (write_func((char *)&record, sizeof(record)) != sizeof(record)) {
            return -1;
        }
        return write_func((char *)arg->start_addr, arg->size);
    }
    return 0;
}

DR_EXPORT
int
drsyscall_write_syscall_number_record(DR_PARAM_IN drsyscall_record_write_t write_func,
                                      DR_PARAM_IN int sysnum)
{
    syscall_record_t record = {};
    record.type = DRSYS_SYSCALL_NUMBER_DEPRECATED;
    record.syscall_number = sysnum;
    return write_func((char *)&record, sizeof(record));
}

DR_EXPORT
int
drsyscall_write_syscall_end_record(DR_PARAM_IN drsyscall_record_write_t write_func,
                                   DR_PARAM_IN int sysnum)
{
    syscall_record_t record = {};
    record.type = DRSYS_RECORD_END_DEPRECATED;
    record.syscall_number = sysnum;
    return write_func((char *)&record, sizeof(record));
}

DR_EXPORT
int
drsyscall_write_syscall_number_timestamp_record(
    DR_PARAM_IN drsyscall_record_write_t write_func, DR_PARAM_IN drsys_sysnum_t sysnum,
    DR_PARAM_IN uint64_t timestamp)
{
    syscall_record_t record = {};
    record.type = DRSYS_SYSCALL_NUMBER_TIMESTAMP;
    record.syscall_number_timestamp.syscall_number = sysnum;
    record.syscall_number_timestamp.timestamp = timestamp;
    return write_func((char *)&record, sizeof(record));
}

DR_EXPORT
int
drsyscall_write_syscall_end_timestamp_record(
    DR_PARAM_IN drsyscall_record_write_t write_func, DR_PARAM_IN drsys_sysnum_t sysnum,
    DR_PARAM_IN uint64_t timestamp)
{
    syscall_record_t record = {};
    record.type = DRSYS_RECORD_END_TIMESTAMP;
    record.syscall_number_timestamp.syscall_number = sysnum;
    record.syscall_number_timestamp.timestamp = timestamp;
    return write_func((char *)&record, sizeof(record));
}
