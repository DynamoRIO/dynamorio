 /* **********************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
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

/* This is a statically-linked app.
 * Be sure to link with "--thumb-entry _start" to get the LSB set to 1.
 * Note: I'm using //-style comments below for easy transition to and from
 * @-style comments for native ARM assembly.
 */
.global _start

_start:
        mov      r3, #4
1:
        mov      r0, #1            // stdout
        ldr      r1, =hello
        mov      r2, #13           // sizeof(hello)
        mov      r7, #4            // SYS_write
        svc      0
        sub      r3, r3, #1
        cmp      r3, #0
        bne      1b

// loop with linkable (no syscall -> shared) branches
        mov      r3, #100
2:
        sub      r3, r3, #1
        b        separate_bb
separate_bb:
        cmp      r3, #0
        bne      2b

// direct call and "bx lr" return:
        ldr      r0, =hello
        mov      r1, #13           // sizeof(hello)
        bl       _print

// indirect direct call and "pop pc" return:
        ldr      r0, =hello
        mov      r1, #13           // sizeof(hello)
        adr      r2, _print_pop_pc
        blx      r2

// indirect jumps
        sub      pc, #4
        add      r2, pc, #4
        push     {r1-r2}
        pop      {r1,pc}

        mov      r0, #0
        cmp      r0, #0
        addne    pc, r10, pc

// test dr_reg_stolen mangling optimization
        mov      r10, pc
        mov      pc, r10
        mov      r10, sp
        mov      r3, r10
// exit
        mov      r0, #1            // stdout
        ldr      r1, =alldone
        mov      r2, #9            // sizeof(alldone)
        mov      r7, #4            // SYS_write
        svc      0
        mov      r0, #0            // exit code
        mov      r7, #1            // SYS_exit
        svc      0

// functions
_print:
        mov      r2, r1            // size of string
        mov      r1, r0            // string to print
        mov      r0, #1            // stdout
        mov      r7, #4            // SYS_write
        svc      0
        bx       lr

_print_pop_pc:
        push     {r4-r8,lr}
        mov      r2, r1            // size of string
        mov      r1, r0            // string to print
        mov      r0, #1            // stdout
        mov      r7, #4            // SYS_write
        svc      0
        pop      {r4-r8,pc}

        .data
hello:
        .ascii   "Hello world!\n"
alldone:
        .ascii   "All done\n"
