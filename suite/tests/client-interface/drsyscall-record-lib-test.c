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

#include "configure.h"
#include "dr_api.h"
#include "drsyscall_record.h"
#include "drsyscall_record_lib.h"
#include "syscall.h"

// There are ten syscall records for write: one DRSYS_SYSCALL_NUMBER_TIMESTAMP record,
// three DRSYS_PRECALL_PARAM records, one DRSYS_MEMORY_CONTENT record, three
// DRSYS_POSTCALL_PARAM records, one DRSYS_RETURN_VALUE record, and one
// DRSYS_RECORD_END_TIMESTAMP record.
// To align the DRSYS_RETURN_VALUE record to end exactly at the end of
// the drsyscall_iterate_records() buffer, we substract the size of nine
// syscall records from DRSYSCALL_ITERATE_RECORDS_BUFFER_SIZE.
// This test verifies the case when a syscall record ends exactly at the end of the
// drsyscall_iterate_records() buffer.
#define TOTAL_SYSCALL_RECORDS 10
#define WRITE_BUFFER_SIZE                    \
    (DRSYSCALL_ITERATE_RECORDS_BUFFER_SIZE - \
     (TOTAL_SYSCALL_RECORDS - 1) * sizeof(syscall_record_t))
#define SYSCALL_RECORD_BUFFER_SIZE \
    (DRSYSCALL_ITERATE_RECORDS_BUFFER_SIZE + sizeof(syscall_record_t))
#define FILE_DESCRIPTOR 2
#define BUFFER_CHAR '0'

char syscall_record_read_buffer[SYSCALL_RECORD_BUFFER_SIZE];
char syscall_record_write_buffer[SYSCALL_RECORD_BUFFER_SIZE];

static size_t
write_syscall_record(void *drcontext, char *buf, size_t size)
{
    static int next_record_offset = 0;
    static int syscall_record_count = 0;
    DR_ASSERT(next_record_offset + size <= SYSCALL_RECORD_BUFFER_SIZE);
    memcpy(&syscall_record_write_buffer[next_record_offset], buf, size);
    next_record_offset += size;
    return size;
}

static size_t
read_syscall_record(char *buffer, size_t size)
{
    static int current_offset = 0;
    DR_ASSERT(current_offset <= SYSCALL_RECORD_BUFFER_SIZE);
    if (current_offset == SYSCALL_RECORD_BUFFER_SIZE) {
        return 0;
    }
    memcpy(buffer, &syscall_record_write_buffer[current_offset], size);
    current_offset += size;
    return size;
}

static bool
read_syscall_record_cb(syscall_record_t *record, char *buffer, size_t size)
{
    static int next_record_offset = 0;
    switch (record->type) {
    case DRSYS_SYSCALL_NUMBER_TIMESTAMP:
    case DRSYS_PRECALL_PARAM:
    case DRSYS_POSTCALL_PARAM:
    case DRSYS_RETURN_VALUE:
    case DRSYS_RECORD_END_TIMESTAMP:
        DR_ASSERT(next_record_offset + sizeof(syscall_record_t) <=
                  SYSCALL_RECORD_BUFFER_SIZE);
        memcpy(&syscall_record_read_buffer[next_record_offset], (char *)record,
               sizeof(syscall_record_t));
        next_record_offset += sizeof(syscall_record_t);
        break;
    case DRSYS_MEMORY_CONTENT:
        DR_ASSERT(size == WRITE_BUFFER_SIZE);
        for (int index = 0; index < size; ++index) {
            DR_ASSERT(buffer[index] == BUFFER_CHAR);
        }
        DR_ASSERT(next_record_offset + sizeof(syscall_record_t) <=
                  SYSCALL_RECORD_BUFFER_SIZE);
        memcpy(&syscall_record_read_buffer[next_record_offset], (char *)record,
               sizeof(syscall_record_t));
        next_record_offset += sizeof(syscall_record_t);
        DR_ASSERT(next_record_offset + size <= SYSCALL_RECORD_BUFFER_SIZE);
        memcpy(&syscall_record_read_buffer[next_record_offset], buffer, size);
        next_record_offset += size;
        break;
    default: DR_ASSERT(true);
    }
    return true;
}

static void
write_syscall_write_records()
{
    uint64_t timestamp = 0;
    drsys_sysnum_t sysnum_write;
    sysnum_write.number = SYS_write;
    drsyscall_write_syscall_number_timestamp_record(GLOBAL_DCONTEXT, write_syscall_record,
                                                    sysnum_write, timestamp);

    drsys_arg_t arg0;
    arg0.valid = true;
    arg0.ordinal = 0;
    arg0.pre = true;
    arg0.value64 = FILE_DESCRIPTOR;
    drsyscall_write_param_record(GLOBAL_DCONTEXT, write_syscall_record, &arg0);

    char write_buffer[WRITE_BUFFER_SIZE];
    memset(write_buffer, BUFFER_CHAR, WRITE_BUFFER_SIZE);

    drsys_arg_t arg1;
    arg1.valid = true;
    arg1.ordinal = 0;
    arg1.pre = true;
    // Cast the write_buffer to uintptr_t to address cast from pointer to integer of
    // different size compiler error for 32-bit systems.
    const uintptr_t buffer_address = (uintptr_t)write_buffer;
    arg1.value64 = (uint64)buffer_address;
    drsyscall_write_param_record(GLOBAL_DCONTEXT, write_syscall_record, &arg1);

    drsys_arg_t arg2;
    arg2.valid = true;
    arg2.ordinal = 0;
    arg2.pre = true;
    arg2.value64 = WRITE_BUFFER_SIZE;
    drsyscall_write_param_record(GLOBAL_DCONTEXT, write_syscall_record, &arg2);

    drsys_arg_t mem_arg;
    mem_arg.valid = true;
    mem_arg.mode = DRSYS_PARAM_IN;
    mem_arg.ordinal = 0;
    mem_arg.pre = true;
    mem_arg.start_addr = write_buffer;
    mem_arg.size = WRITE_BUFFER_SIZE;
    drsyscall_write_memarg_record(GLOBAL_DCONTEXT, write_syscall_record, &mem_arg);

    arg0.pre = false;
    drsyscall_write_param_record(GLOBAL_DCONTEXT, write_syscall_record, &arg0);
    arg1.pre = false;
    drsyscall_write_param_record(GLOBAL_DCONTEXT, write_syscall_record, &arg1);
    arg2.pre = false;
    drsyscall_write_param_record(GLOBAL_DCONTEXT, write_syscall_record, &arg2);

    drsys_arg_t return_arg;
    return_arg.valid = true;
    return_arg.ordinal = -1;
    return_arg.pre = false;
    return_arg.value64 = WRITE_BUFFER_SIZE;
    drsyscall_write_param_record(GLOBAL_DCONTEXT, write_syscall_record, &return_arg);

    drsyscall_write_syscall_end_timestamp_record(GLOBAL_DCONTEXT, write_syscall_record,
                                                 sysnum_write, ++timestamp);
}

int
main(int argc, char *argv[])
{
    write_syscall_write_records();
    drsyscall_iterate_records(read_syscall_record, read_syscall_record_cb);
    DR_ASSERT(memcmp(syscall_record_write_buffer, syscall_record_read_buffer,
                     SYSCALL_RECORD_BUFFER_SIZE) == 0);
    dr_printf("done");
}
