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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
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

/*
 * This test is designed to test the boundary condition when a syscall record ends exactly
 * at the end of the buffer for drsyscall_iterate_records. The current buffer size for
 * drsyscall_iterate_records is 8192. A write syscall has ten sycall recrods: one
 * DRSYS_SYSCALL_NUMBER_TIMESTAMP record, three DRSYS_PRECALL_PARAM records, one
 * DRSYS_MEMORY_CONTENT record, three DRSYS_POSTCALL_PARAM records, one DRSYS_RETURN_VALUE
 * record, and one DRSYS_RECORD_END_TIMESTAMP record. Each record has 18 bytes. To align
 * the end of the DRSYS_RECORD_END_TIMESTAMP at the end of the drsyscall_iterate_records
 * buffer, we call write with size 8192 - 10 * 18 = 8012 bytes.
 */
/* This is a statically-linked app. */
.text
.globl _start
.type _start, @function

        .align   8
_start:
        and      rsp, -16         // align stack pointer to cache line
        mov      rdi, 2           // stderr
        lea      rsi, block
        mov      rdx, 8012        // sizeof(block)
        mov      eax, 1           // SYS_write
        syscall
        mov      rdi, 0           // exit code
        mov      eax, 231         // SYS_exit_group
        syscall

        .data
        .align   8
block:
        .fill   8011, 1, 0x30     // Create 8011 '0' characters
        .string "\n"
