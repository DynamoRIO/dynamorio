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

#ifndef _DRSYSCALL_RECORD_H_
#    define _DRSYSCALL_RECORD_H_ 1

#    include <stdint.h>
#    include "dr_api.h"

/**************************************************
 * TOP-LEVEL ROUTINES
 */
/**
 * @file drsyscall_record.h
 * @brief Header for Dr. Syscall system call records.
 */

// XXX i#7472: Move definitions of START_PACKED_STRUCTURE and
// END_PACKED_STRUCTURE to core.
#    ifdef WINDOWS
/* Use special C99 operator _Pragma to generate a pragma from a macro */
#        if _MSC_VER <= 1200
#            define ACTUAL_PRAGMA(p) _Pragma(#        p)
#        else
#            define ACTUAL_PRAGMA(p) __pragma(p)
#        endif
/* Usage: if planning to typedef, that must be done separately, as MSVC will
 * not take _pragma after typedef.
 */
#        define START_PACKED_STRUCTURE ACTUAL_PRAGMA(pack(push, 1))
#        define END_PACKED_STRUCTURE ACTUAL_PRAGMA(pack(pop))
#    else                              /* UNIX */
#        define START_PACKED_STRUCTURE /* nothing */
#        define END_PACKED_STRUCTURE __attribute__((__packed__))
#    endif

/** The type of the syscall record. */
typedef enum {
    /** Start of a syscall. */
    DRSYS_SYSCALL_NUMBER_DEPRECATED = 1,
    /** Pre-syscall parameter.*/
    DRSYS_PRECALL_PARAM,
    /** Post-syscall parameter. */
    DRSYS_POSTCALL_PARAM,
    /** Memory address, size, and content. */
    DRSYS_MEMORY_CONTENT,
    /** Return value of the syscall. */
    DRSYS_RETURN_VALUE,
    /** End of a syscall. */
    DRSYS_RECORD_END_DEPRECATED,
    /** Start of a syscall with a timestamp. */
    DRSYS_SYSCALL_NUMBER_TIMESTAMP,
    /** End of a syscall with a timestamp. */
    DRSYS_RECORD_END_TIMESTAMP,
} syscall_record_type_t;

/**
 * To enable #syscall_record_t to be default initialized reliably, a byte array is defined
 * with the same length as the largest member of the union.
 */
#    define SYSCALL_RECORD_UNION_SIZE_BYTES (sizeof(uint8_t *) + sizeof(size_t))

/**
 * Describes a system call number, parameter, memory region, or the return
 * value.
 */
START_PACKED_STRUCTURE
typedef struct syscall_record_t_ {
    // type is one of syscall_record_type_t.
    uint16_t type;
    union {
        /**
         * The _raw_bytes entry is for initialization purposes and must be first in
         * this list. A byte array is used for initialization rather than an existing
         * struct to avoid incomplete initialization due to padding or alignment
         * constraints within a struct. This array is not intended to be used.
         */
        uint8_t _raw_bytes[SYSCALL_RECORD_UNION_SIZE_BYTES];
        /**
         * The syscall number. It is used for type
         * #DRSYS_SYSCALL_NUMBER_DEPRECATED or
         * #DRSYS_RECORD_END_DEPRECATED.
         */
        uint16_t syscall_number;
        START_PACKED_STRUCTURE
        /**
         * The parameter of a syscall. It is used for type #DRSYS_PRECALL_PARAM and
         * #DRSYS_POSTCALL_PARAM.
         */
        struct {
            /** The ordinal of the parameter. Set to -1 for a return value. */
            uint16_t ordinal;
            /** The value of the parameter. */
            reg_t value;
        } END_PACKED_STRUCTURE param;
        START_PACKED_STRUCTURE
        /**
         * The memory address and the size of a syscall parameter. It is used for
         * type #DRSYS_MEMORY_CONTENT. */
        struct {
            /** The address of the memory region. */
            uint8_t *address;
            /** The size of the memory region. */
            size_t size;
        } END_PACKED_STRUCTURE content;
        /** The return value of the syscall. It is used for type #DRSYS_RETURN_VALUE. */
        reg_t return_value;
        START_PACKED_STRUCTURE
        /**
         * The syscall number and a timestamp. It is used for type
         * #DRSYS_SYSCALL_NUMBER_TIMESTAMP and #DRSYS_RECORD_END_TIMESTAMP.
         */
        struct {
            /**
             * The syscall number.
             */
            uint16_t syscall_number;
            /**
             * The timestamp marks the beginning of the syscall for
             * #DRSYS_SYSCALL_NUMBER_TIMESTAMP, and the end of the
             * syscall for #DRSYS_RECORD_END_TIMESTAMP.
             */
            uint64_t timestamp;
        } END_PACKED_STRUCTURE syscall_number_timestamp;
    };
} END_PACKED_STRUCTURE syscall_record_t;

#endif /* _DRSYSCALL_RECORD_H_ */
