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

DR_EXPORT
/**
 * Reads system call records from \p record_file and outputs the content to \p
 * output_stream. Any errors encountered during this process are reported to the
 * \p error_stream.
 * @return 0 on success, or -1 if an error occurs.
 */
int
drsyscall_read_record_file(DR_PARAM_IN FILE *output_stream,
                           DR_PARAM_IN FILE *error_stream, DR_PARAM_IN int record_file);

DR_EXPORT
/**
 * Write a #syscall_record_t of type #DRSYS_PRECALL_PARAM or #DRSYS_PRECALL_PARAM to \p
 * record_file based on \p arg.
 * @return the actual number of bytes written, or -1 if an error occues.
 */
int
drsyscall_write_param_record(DR_PARAM_IN int record_file, DR_PARAM_IN drsys_arg_t *arg);

DR_EXPORT
/**
 * Write a #syscall_record_t of type #DRSYS_MEMORY_CONTENT to \p record_file based on \p
 * arg.
 * @return the actual number of bytes written, or -1 if an error occues.
 */
int
drsyscall_write_memarg_record(DR_PARAM_IN int record_file, DR_PARAM_IN drsys_arg_t *arg);

DR_EXPORT
/**
 * Write a #syscall_record_t of type #DRSYS_SYSCALL_NUMBER to \p record_file based on \p
 * arg.
 * @return the actual number of bytes written.
 */
int
drsyscall_write_syscall_number_record(DR_PARAM_IN int record_file,
                                      DR_PARAM_IN int sysnum);

DR_EXPORT
/**
 * Write a #syscall_record_t of type #DRSYS_RECORD_END to \p record_file based on \p arg.
 * @return the actual number of bytes written.
 */
int
drsyscall_write_syscall_end_record(DR_PARAM_IN int record_file, DR_PARAM_IN int sysnum);

#endif /* _DRSYSCALL_RECORD_LIB_H_ */
