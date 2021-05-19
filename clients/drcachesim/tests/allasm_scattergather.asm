 /* **********************************************************
 * Copyright (c) 2021 Google, Inc.  All rights reserved.
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

/* This is a statically-linked app.
 * XXX i#2985, i#3844: We intentionally use high-numbered xmm registers here,
 * to avoid conflict with the xmm spill reg selection logic which is very naive
 * as of today. Maybe replace with lower-numbered xmm registers when drreg
 * support for xmm spills is complete.
 */
.text
.globl _start
.type _start, @function
        .align   8
_start:
        // Align stack pointer to cache line.
        and      rsp, -16

        // Load data into xmm10.
        // We embed data in instrs to avoid adding extra loads.
        mov      eax, 0xdead
        vpinsrd  xmm10, xmm10, eax, 0x00
        mov      eax, 0xbeef
        vpinsrd  xmm10, xmm10, eax, 0x01
        mov      eax, 0x8bad
        vpinsrd  xmm10, xmm10, eax, 0x02
        mov      eax, 0xf00d
        vpinsrd  xmm10, xmm10, eax, 0x03

        // Load indices into xmm11.
        mov      eax, 0x01
        vpinsrd  xmm11, xmm11, eax, 0x00
        mov      eax, 0x03
        vpinsrd  xmm11, xmm11, eax, 0x01
        mov      eax, 0x00
        vpinsrd  xmm11, xmm11, eax, 0x02
        mov      eax, 0x02
        vpinsrd  xmm11, xmm11, eax, 0x03

#ifdef __AVX512F__
        // Scatter xmm10 data to arr, skipping element at index 1 in xmm10.
        mov      ebx, 0xd
        kmovw    k1, ebx
        vpscatterdd [arr + xmm11*4]{k1}, xmm10
#else
        // Emulate scatter instr if not available.
        mov      dword ptr [arr], 0x00008bad
        mov      dword ptr [arr + 0x04*1], 0x0000dead
        mov      dword ptr [arr + 0x04*2], 0x0000f00d
#endif

        // Gather arr data into xmm12, skipping index 2 at xmm12,
        // using same indices as scatter.
        pcmpeqd  xmm13, xmm13
        xor      eax, eax
        vpinsrd  xmm13, xmm13, eax, 0x02
        vpgatherdd xmm12, [arr + xmm11*4], xmm13

        // Compare xmm10 and xmm12.
        // Only first and last elements should be equal.
        mov      eax, 0x00
        vpinsrd  xmm10, xmm10, eax, 0x01
        vpinsrd  xmm10, xmm10, eax, 0x02
        cmpss    xmm10, xmm12, 0

        // Print comparison result.
        jne      incorrect
        lea      rsi, correct_str
        mov      rdx, 8           // sizeof(correct_str)
        jmp      done_cmp
incorrect:
        lea      rsi, incorrect_str
        mov      rdx, 10          // sizeof(incorrect_str)
done_cmp:
        mov      rdi, 1           // stdout
        mov      eax, 1           // SYS_write
        syscall

        // Print end message.
        mov      rdi, 1           // stdout
        lea      rsi, hello_str
        mov      rdx, 13          // sizeof(hello_str)
        mov      eax, 1           // SYS_write
        syscall

        // Exit.
        mov      rdi, 0           // exit code
        mov      eax, 231         // SYS_exit_group
        syscall

        .data
        .align   8
hello_str:
        .string  "Hello world!\n"
correct_str:
        .string  "Correct\n"
incorrect_str:
        .string  "Incorrect\n"
arr:
        .zero    16
