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
#include <unistd.h>

#include "drsyscall_record.h"

int
main(int argc, char *argv[])
{
    int record_file = open(argv[1], O_RDONLY);
    if (record_file == -1) {
        fprintf(stderr, "unable to open file %s\n", argv[1]);
        return -1;
    }
    syscall_record_t record = {};
    while (read(record_file, &record, sizeof(record)) == sizeof(record)) {
        switch (record.type) {
        case DRSYS_SYSCALL_NUMBER:
            fprintf(stdout, "syscall: %d\n", record.syscall_number);
            break;
        case DRSYS_PRECALL_PARAM:
        case DRSYS_POSTCALL_PARAM:
            fprintf(stdout, "%s-syscall ordinal %d, value 0x" HEX64_FORMAT_STRING "\n",
                    (record.type == DRSYS_PRECALL_PARAM ? "pre" : "post"),
                    record.param.ordinal, record.param.value);
            break;
        case DRSYS_MEMORY_CONTENT:
            fprintf(stdout, "memory content address " PFX ", size " PIFX "\n    ",
                    record.content.address, record.content.size);
            uint32_t buffer;
            size_t count = 0;
            for (; count + sizeof(buffer) <= record.content.size;
                 count += sizeof(buffer)) {
                if (read(record_file, &buffer, sizeof(buffer)) != sizeof(buffer)) {
                    fprintf(stderr,
                            "failed to read " PIFX " bytes from the record file.\n",
                            sizeof(buffer));
                    return -1;
                }
                fprintf(stdout, "%08x ", buffer);
                if ((count + 4) % 16 == 0) {
                    fprintf(stdout, "\n    ");
                }
            }
            if (count < record.content.size) {
                buffer = 0;
                if (read(record_file, &buffer, record.content.size - count) !=
                    record.content.size - count) {
                    fprintf(stderr,
                            "failed to read " PIFX " bytes from the record file.\n",
                            record.content.size - count);
                    return -1;
                }
                fprintf(stdout, "%08x", buffer);
            }
            fprintf(stdout, "\n");
            break;
        case DRSYS_RETURN_VALUE:
            fprintf(stdout, "return value 0x" HEX64_FORMAT_STRING "\n",
                    record.return_value);
            break;
        case DRSYS_RECORD_END:
            fprintf(stdout, "syscall end: %d\n", record.syscall_number);
            break;
        default: fprintf(stderr, "unknown record type %d\n", record.type); return -1;
        }
    }
    return 0;
}
