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
// 16 means Thumb
.code 16
.syntax unified

_start:
        mov r3, #4
1:
        mov r0, #1            // stdout
        ldr r1, =hello
        mov r2, #13           // sizeof(hello)
        mov r7, #4            // SYS_write
        svc 0
        sub r3, r3, #1
        cmp r3, #0
        bne 1b

// loop with linkable (no syscall -> shared) branches
        mov r3, #100
2:
        sub r3, r3, #1
        b separate_bb
separate_bb:
        cmp r3, #0
        bne 2b

// direct call and "bx lr" return:
        ldr r0, =hello
        mov r1, #13           // sizeof(hello)
        bl _print

// write to stolen reg and read pc at same time
        mov r10, pc

// indirect call and "pop pc" return:
        ldr r0, =hello
        mov r1, #13           // sizeof(hello)
        adr r2, _print_pop_pc
        add r2, r2, #1          // keep it Thumb
        blx r2

// indirect jumps
        // Thumb won't let us do arith ops on the pc
.align 4 // try to make sequence less fragile wrt code changes around it
        add r1, pc, #2        // point at add below, and align (stays Thumb though)
        mov pc, r1
        add r2, pc, #7        // point at the exit code below
        push {r1-r2}
        pop  {r1,pc}

// mode switches
        blx _callee_ARM
        adr r2, _callee_ARM
        blx r2

// stolen reg
        push {r0-r12}
        pop {r0-r12}
        mov r10, sp
        stm r10!, {r0-r9}
        b 4f
4:
        ldm r10!, {r0-r9}
        smlalbb r0, r1, r2, r3  // reads all 4 scratch regs

// IT blocks
        itt hi
        strhi r10, [sp, #-4]
        movhi r10, r0
        b 3f
3:
        itete vs
        movvs r3, r10
        movvc r0, r2
        movvs r1, r3
        movvc r4, r0

        itete vs
        movvs r0, r2
        movvc r3, r10
        movvs r1, r3
        movvc r4, r0

        itete vs
        movvs r0, r2
        movvc r1, r3
        movvs r3, r10
        movvc r4, r0

        itete vs
        movvs r0, r2
        movvc r1, r3
        movvs r4, r0
        movvc r3, r10

// indirect jump in IT block:
        adr r2, _jmp_target
        add r2, r2, #1          // keep it Thumb
        mov r0, #0
        cmp r0, #0
        it ne
        bxne r0
        it eq
        bxeq r2
_jmp_target:

// calls in IT block:
        mov r0, #0
        cmp r0, #0
        it ne
        blne _print
        it ne
        blxne _print

// various SIMD instrs just to stress decoder
        vmov.i32 q0, #0x12
        vmov.i32 q1, #0xab
        movw r0, #0x5678
        movt r0, #0x1234
        vmov.32 d4[0], r0
        vmov.32 d4[1], r0

// some tricky cases that recently hit bugs
        strd  r12, lr, [sp, #-16]!

// indirect jump combined with stolen reg write in IT block:
        adr      r0, _exit
        add      r0, r0, #1          // keep it Thumb
        str      r0, [sp, #-4]
        sub      sp, #36
        mov      r0, #0
        cmp      r0, #0
        ittt     eq
        moveq    r0, r9
        addeq    r0, #36
        ldmiaeq.w sp!, {r4, r5, r6, r7, r8, r9, r10, r11, pc}

// exit
_exit:
        mov r0, #1            // stdout
        ldr r1, =alldone
        mov r2, #9            // sizeof(alldone)
        mov r7, #4            // SYS_write
        svc 0
        mov r0, #0            // exit code
        mov r7, #1            // SYS_exit
        svc 0

// functions
_print:
        mov r2, r1            // size of string
        mov r1, r0            // string to print
        mov r0, #1            // stdout
        mov r7, #4            // SYS_write
        svc 0
        bx lr

_print_pop_pc:
        push {r4-r8,lr}
        mov r2, r1            // size of string
        mov r1, r0            // string to print
        mov r0, #1            // stdout
        mov r7, #4            // SYS_write
        svc 0
        pop {r4-r8,pc}

.align 4
_callee_ARM:
        .word 0xe12fff1e        // bx lr

        .data
hello:
        .ascii "Hello world!\n"
alldone:
        .ascii "All done\n"
