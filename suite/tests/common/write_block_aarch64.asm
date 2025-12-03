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
        .global  _start

        .align   6
_start:
        mov      w0, #2            // stderr
        adr      x1, block
        mov      w2, #8012         // sizeof(block)
        mov      w8, #64           // SYS_write
        svc      #0
        mov      w0, #0            // status
        mov      w8, #94           // SYS_exit_group
        svc      #0

        .data
        .align   6
block:
        .fill   8011, 1, 0x30     // Create 8011 '0' characters
        .string "\n"
