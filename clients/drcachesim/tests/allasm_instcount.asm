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

/* Test for i#7769: Instruction counting should exclude REP emulation overhead.
 * This minimal test has ~9 fetched instructions including one REP with multiple iterations.
 * Without the fix, REP emulation overhead causes instruction counts to be inflated.
 */
.text
.globl _start
.type _start, @function
        .align   8
_start:
    // Align stack (instruction 1)
    and     rsp, -16

    // Simple setup (instructions 2-5)
    xor     eax, eax         // instruction 2
    mov     ecx, 1           // instruction 3
    lea     rdi, dst_data    // instruction 4

    // Dummy block 1 with jump
    push    rbx              // instruction 5
    mov     rbx, 0x1234      // instruction 6
    pop     rbx              // instruction 7
    jmp     dummy2           // instruction 8 - control flow!

dummy2:
    // Dummy block 2
    push    rcx              // instruction 9
    xor     rcx, rcx         // instruction 10
    pop     rcx              // instruction 11
    jmp     rep_section      // instruction 12 - control flow!

rep_section:
    // REP instruction
    rep     stosb            // instruction 13

    xor     ebx, ebx         // instruction 14
    xor     edx, edx         // instruction 15
    jmp     exit_block       // instruction 16 - control flow!

exit_block:
    // Exit
    mov     rdi, 0           // instruction 17
    mov     eax, 231         // instruction 18: SYS_exit_group
    syscall                  // instruction 19

.data
    .align  8
dst_data:
    .space  16