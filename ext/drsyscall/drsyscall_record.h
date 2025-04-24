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

#include <stdint.h>
#include "dr_api.h"

typedef enum {
    DRSYS_SYSCALL_NUMBER = 1,
    DRSYS_PRECALL_PARAM,
    DRSYS_POSTCALL_PARAM,
    DRSYS_MEMORY_CONTENT,
    DRSYS_RETURN_VALUE,
    DRSYS_RECORD_END,
} syscall_record_type_t;

/**
 * To enable #syscall_record_t to be default initialized reliably, a byte array is defined
 * with the same length as the largest member of the union.
 */
#define SYSCALL_RECORD_UNION_SIZE_BYTES (sizeof(uint8_t *) + sizeof(size_t))

typedef struct syscall_record_t_ {
    // type is one of syscall_record_type_t.
    uint16_t type;
    union {
        // The _raw_bytes entry is for initialization purposes and must be first in
        // this list.  A byte array is used for initialization rather than an existing
        // struct to avoid incomplete initialization due to padding or alignment
        // constraints within a struct.  This array is not intended to be used for
        // syscall_record_t access.
        uint8_t _raw_bytes[SYSCALL_RECORD_UNION_SIZE_BYTES]; /**< Do not use: for init
                                                                only. */
        // syscall_number is used when the type is DRSYS_SYSCALL_NUMBER or
        // DRSYS_RECORD_END.
        uint16_t syscall_number;
        // param is used for type DRSYS_PRECALL_PARAM and DRSYS_POSTCALL_PARAM.
        struct {
            uint16_t ordinal;
            reg_t value;
        } param;
        // content is used for type DRSYS_MEMORY_CONTENT.
        struct {
            uint8_t *address;
            size_t size;
        } content;
        // return_value is used for type DRSYS_RETURN_VALUE.
        reg_t return_value;
    };
} __attribute__((__packed__)) syscall_record_t;
