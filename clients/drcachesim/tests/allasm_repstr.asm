 /* **********************************************************
 * Copyright (c) 2021-2022 Google, Inc.  All rights reserved.
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

/* This is a statically-linked app. */
.text
.globl _start
.type _start, @function
        .align   8
_start:
        // Align stack pointer to cache line.
        and      rsp, -16

        // Test a zero-iter rep string loop.
        mov      ecx, 0
        lea      esi, bye_str
        lea      edi, hello_str
        cld
        rep      movsd

        // Test a rep string loop.
        mov      ecx, 5
        lea      esi, bye_str
        lea      edi, hello_str
        cld
        rep      movsb

        // Test page-spanning accesses.
        lea      rcx, page_str
        // Somehow the GNU assembler 2.38 is adding the load size (4 here) to
        // whatever displacement is listed (!!!), so these end up as -3 and -1.
        mov      eax, DWORD [-7+rcx]
        mov      eax, DWORD [-5+rcx]

        // Print a message in a loop for testing tracing windows.
        mov      ebx, 10          // Loop count.
repeat:
        mov      rdi, 2           // stderr
        lea      rsi, hello_str
        mov      rdx, 13          // sizeof(hello_str)
        mov      eax, 1           // SYS_write
        syscall
        dec      ebx
        cmp      ebx, 0
        jnz      repeat

        // Exit.
        mov      rdi, 0           // exit code
        mov      eax, 231         // SYS_exit_group
        syscall

        .data
        .align   8
hello_str:
        .string  "Hello world!\n"
bye_str:
        .string  "Adios\n"
        // Push .data onto a 2nd page to test page-spanning accesses.
        // We assume 4K pages here.
        .align   4096
page_str:
        .word    0
