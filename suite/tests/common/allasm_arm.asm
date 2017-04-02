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
 * Note: I'm using //-style comments below for easy transition to and from
 * @-style comments for native ARM assembly.
 */
.global _start

        .align   6
_start:
        bic      sp, sp, #63       // align stack pointer to cache line
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

// prefetch
        .word    0xf4dff080         // pli [pc, #128] miss
        .word    0xf4dff080         // pli [pc, #128] hit
        .word    0xf5dff080         // pld [pc, #128] miss
        .word    0xf5dff080         // pld [pc, #128] hit

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

// direct call to test cache flush
        bl       _flush

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

        mov      r10, pc
        bx       r10

// test dr_reg_stolen mangling optimization
        mov      r10, pc
        mov      pc, r10
        mov      r10, sp
        mov      r3, r10
        push     {r0-r15}
        add      sp, sp, #16
        pop      {r4-r11}
        add      sp, sp, #16
        cmp      r10, #0
        movne    r3, r9
        moveq    r3, r10
        movne    r9, r3
        moveq    r10, r3

// test various SIMD cases
        mov      r7, sp
        vld3.8   {d10-d12}, [r7]!
        vmull.u16 q10, d24, d16
        vldm     r7!, {d3}
        vld4.32  {d17[],d19[],d21[],d23[]}, [r7 :128], r12
        vsri.64  d28, d15, #1
        vsli.32  d27, d0, #31
        vqmovun.s32 d4, q8
        vqshlu.s64 d3, d9, #13

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

_flush:
        mov      r0, #0            // start
        mov      r1, #0
        movt     r1, #1            // end
        mov      r2, #0
        mov      r7, #2
        movt     r7, #15           // SYS_cacheflush
        svc      0
        bx       lr

        .data
        .align   6
hello:
        .ascii   "Hello world!\n"
alldone:
        .ascii   "All done\n"
