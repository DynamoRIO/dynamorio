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
#ifndef _DRSYSCALL_RECORD_LIB_H_
#define _DRSYSCALL_RECORD_LIB_H_ 1
#include <stdio.h>
#include <unistd.h>

#include "dr_api.h"
#include "drsyscall.h"
#include "drsyscall_record.h"

/**
 * A user provided function to read syscall records. Returns the number of bytes read.
 * Returns 0 if there are no more bytes.
 */
typedef size_t (*drsyscall_record_read_t)(DR_PARAM_IN char *buffer,
                                          DR_PARAM_IN size_t size);

/**
 * A user provided function to write syscall records. Returns the number of bytes
 * written. For performance optimization, the function should implement buffering
 * to write records in bulk.
 */
typedef size_t (*drsyscall_record_write_t)(DR_PARAM_IN char *buffer,
                                           DR_PARAM_IN size_t size);

/**
 * Callback function to invoke for each syscall record. For record type
 * #DRSYS_MEMORY_CONTENT, \p buffer points to the beginning of the buffer and
 * \p size is the size of the buffer. \p buffer is NULL and \p size is 0 for
 * other types.
 *
 * Returns true to continue, false to stop.
 */
typedef bool (*drsyscall_iter_record_cb_t)(DR_PARAM_IN syscall_record_t *record,
                                           DR_PARAM_IN char *buffer,
                                           DR_PARAM_IN size_t size);

DR_EXPORT
/**
 * Dynamically iterates over all syscall records.
 *
 * @param[in] read_func  A user provided function to read syscall records.
 * @param[in] record_cb  The callback to invoke for each syscall record.
 *
 * \return true when read_func returns 0 or record_cb returns false. Return false if an
 * error occurs.
 */
bool
drsyscall_iterate_records(DR_PARAM_IN drsyscall_record_read_t read_func,
                          DR_PARAM_IN drsyscall_iter_record_cb_t record_cb);

DR_EXPORT
/**
 * Write a #syscall_record_t of type #DRSYS_PRECALL_PARAM or #DRSYS_PRECALL_PARAM
 * based on \p arg.
 *
 * @param[in] write_func  A user provided function to write syscall record.
 * @param[in] arg         System call parameter or memory region.
 *
 * \return the actual number of bytes written, or -1 if an error occurs.
 */
int
drsyscall_write_param_record(DR_PARAM_IN drsyscall_record_write_t write_func,
                             DR_PARAM_IN drsys_arg_t *arg);

DR_EXPORT
/**
 * Write a #syscall_record_t of type #DRSYS_MEMORY_CONTENT record_file based on \p
 * arg.
 *
 * @param[in] write_func  A user provided function to write syscall record.
 * @param[in] arg         System call parameter or memory region.
 *
 * @return the actual number of bytes written, or -1 if an error occurs.
 */
int
drsyscall_write_memarg_record(DR_PARAM_IN drsyscall_record_write_t write_func,
                              DR_PARAM_IN drsys_arg_t *arg);

DR_EXPORT
/**
 * Write a #syscall_record_t of type #DRSYS_SYSCALL_NUMBER based on \p
 * sysnum.
 *
 * @param[in] write_func  A user provided function to write syscall record.
 * @param[in] sysnum      The system call number.
 *
 * @return the actual number of bytes written.
 *
 * \deprecated drsyscall_write_syscall_number_timestamp_record() should be used
 * instead.
 */
int
drsyscall_write_syscall_number_record(DR_PARAM_IN drsyscall_record_write_t write_func,
                                      DR_PARAM_IN int sysnum);

DR_EXPORT
/**
 * Write a #syscall_record_t of type #DRSYS_RECORD_END based on \p sysnum.
 *
 * @param[in] write_func  A user provided function to write syscall record.
 * @param[in] sysnum      The system call number.
 *
 * @return the actual number of bytes written.
 *
 * \deprecated drsyscall_write_syscall_end_timestamp_record() should be used
 * instead.
 */
int
drsyscall_write_syscall_end_record(DR_PARAM_IN drsyscall_record_write_t write_func,
                                   DR_PARAM_IN int sysnum);

DR_EXPORT
/**
 * Write a #syscall_record_t of type #DRSYS_SYSCALL_NUMBER_TIMESTAMP based on \p
 * sysnum and \p timestamp.
 *
 * @param[in] write_func  A user provided function to write syscall record.
 * @param[in] sysnum      The system call number.
 * @param[in] timestamp   The timestamp of the beginning of the syscall.
 *
 * @return the actual number of bytes written.
 */
int
drsyscall_write_syscall_number_timestamp_record(
    DR_PARAM_IN drsyscall_record_write_t write_func, DR_PARAM_IN drsys_sysnum_t sysnum,
    DR_PARAM_IN uint64_t timestamp);

DR_EXPORT
/**
 * Write a #syscall_record_t of type #DRSYS_RECORD_END_TIMESTAMP based on \p
 * sysnum and \p timestamp.
 *
 * @param[in] write_func  A user provided function to write syscall record.
 * @param[in] sysnum      The system call number.
 * @param[in] timestamp   The timestamp of the end of the syscall.
 *
 * @return the actual number of bytes written.
 */
int
drsyscall_write_syscall_end_timestamp_record(
    DR_PARAM_IN drsyscall_record_write_t write_func, DR_PARAM_IN drsys_sysnum_t sysnum,
    DR_PARAM_IN uint64_t timestamp);

#endif /* _DRSYSCALL_RECORD_LIB_H_ */
