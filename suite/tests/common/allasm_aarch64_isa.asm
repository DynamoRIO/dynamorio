/* **********************************************************
 * Copyright (c) 2023 Google, Inc.  All rights reserved.
 * Copyright (c) 2016 ARM Limited. All rights reserved.
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
 * * Neither the name of ARM Limited nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL ARM LIMITED OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

// AArch64 assembler test program.

// This test program is intended to help with the development of
// Dynamic Binary Instrumentation (DBI) and similar systems.
// Facilities are tested in a reasonable order for implementing them.
// It is assumed that non-branch instructions that make no reference
// to the PC are straightforward and do not require special testing.
// The code is written for Linux but should be easily portable to
// other operating systems. It has only been tested on little-endian
// systems but might work on a big-endian system. Directives and
// relocations are for the GNU assembler, with C preprocessor.
// Typical command to assemble and link:
//   gcc -g -nostartfiles -nodefaultlibs -xassembler-with-cpp THIS_FILE
// Typical usage is to compare the output produced on the system
// under development with the output produced on a trusted system.

// AArch64 user-space registers:
//
// PC, X0-X30, SP, V0-V31, NZCV, FPCR, FPSR, TPIDR_EL0, and
// TPIDRRO_EL0 (not used by Linux).

// A64 instructions that need special treatment by DBI:
//
// Branches:
//     B       label
//     BL      label
//     B.cond  label
//     CBNZ    Wt|Xt, label
//     CBZ     Wt|Xt, label
//     TBNZ    Wt|Xt, #uimm6, label
//     TBZ     Wt|Xt, #uimm6, label
//     BLR     Xn
//     BR      Xn
//     RET     {Xn}
//
// Other instructions that read the PC:
//     ADR     Xd, label
//     ADRP    Xd, label
//     LDR     Wt|Xt|St|Dt|Qt, label
//     LDRSW   Xt, label
//     PRFM    prfop, label
//
// System calls:
//     SVC     #imm
//
// Also, any load/store exclusive may need special treatment as
// instrumentation might clear the exclusive monitor and make
// the store fail every time, causing an infinite loop.

// Customise these definitions for the system under test.
// Each of xm, xn, xs must be a different X register, not X30.
// Set xs to a register that is treated differently and therefore requires
// more thorough testing. Set xm and xn to ordinary, non-problematic registers.
#define xm x11
#define xn x12
#define xs x28

        // Set up the stack and stack pointer.
        // On Linux, nothing needs to be done.
        .macro  init_stack
        .endm

        // Detect execution on path that should not be executed.
        .macro  error
        .inst   0 // unallocated encoding; on Linux: SIGILL, Illegal instruction
        .endm

        // Write to output, X1 bytes, starting at X0.
        // The last character written will always be '\n'.
        // All registers may be changed.
        // On Linux, we use the system call "write".
        .macro  write
        mov     x2, x1
        mov     x1, x0
        mov     w0, #1  // stdout
        mov     w8, #64 // SYS_write
        svc     #0
        .endm

        // Exit.
        // On Linux, we use the system call "exit_group".
        .macro  exit
        mov     w0, #0  // status
        mov     w8, #94 // SYS_exit_group
        svc     #0
        .endm

        // Move a 64-bit symbol value into an X register.
        .macro  mov64 xreg, constant
        movz    \xreg, #:abs_g3:\constant
        movk    \xreg, #:abs_g2_nc:\constant
        movk    \xreg, #:abs_g1_nc:\constant
        movk    \xreg, #:abs_g0_nc:\constant
        .endm

        // Write n-byte string from X0, adding '\n'.
        .macro  printn c
        mov     w1, #'\n'
        strb    w1, [x0, #\c]
        mov     x1, #(\c + 1)
        write
        .endm

        // Add the character c at X0, advancing X0. Uses X1.
        .macro  addch c
        mov     w1, #\c
        strb    w1, [x0], #1
        .endm

        // Execute some branches, using registers xa and xb.
        .macro  branch xa, xb
        mov     \xa, #100
        mov64   \xb, ll2_\@
ll1_\@:
        br      \xb
ll2_\@:
        sub     \xa, \xa, #1
        cbnz    \xa, ll1_\@
        .endm

str_line1:
        .ascii  "line1\n"
        .equ    strlen_line1, . - str_line1
str_line2:
        .ascii  "line2\n"
        .equ    strlen_line2, . - str_line2
digits:
        .ascii  "0123456789abcdef" // 16 hex digits

        .align  3
str_stack:
        .ascii  "Stack is usable\n" // exactly 16 bytes
str_end:
        .ascii  "END of the TEST\n" // exactly 16 bytes
str_x0_x1:
        .ascii  "abcd(X0-X1)vwxyz" // exactly 16 bytes
str_x2_x3:
        .ascii  "ABCD(X2-X3)VWXYZ" // exactly 16 bytes
str_x30_sp:
        .ascii  "0123(X30-SP)6789" // exactly 16 bytes
str_tbnz_tbz:
        .ascii  "abc(TBNZ,TBZ)xyz" // exactly 16 bytes

        // The following 796 bytes specify a complete register state,
        // loaded by load_state. Parts of it are also used separately.
str_regstate:
str_xregs:
        // Exactly 256 bytes:
        .ascii  "ab(x0)cdef(x1)ghij(x2)klmn(x3)op"
        .ascii  "qr(x4)stuv(x5)wxyz(x6)ABCD(x7)EF"
        .ascii  "GH(x8)IJKL(x9)MNOP(x10)QRS(x11)T"
        .ascii  "UV(x12)WXY(x13)Zab(x14)cde(x15)f"
        .ascii  "gh(x16)ijk(x17)lmn(x18)opq(x19)r"
        .ascii  "st(x20)uvw(x21)xyz(x22)ABC(x23)D"
        .ascii  "EF(x24)GHI(x25)JKL(x26)MNO(x27)P"
        .ascii  "QR(x28)STU(x29)VWX(x30)YZa(sp)bc"
str_vregs:
        // Exactly 512 bytes:
        .ascii  "abcdef(v0)ghijklmnopqr(v1)stuvwx"
        .ascii  "yzABCD(v2)EFGHIJKLMNOP(v3)QRSTUV"
        .ascii  "WXYZab(v4)cdefghijklmn(v5)opqrst"
        .ascii  "uvwxyz(v6)ABCDEFGHIJKL(v7)MNOPQR"
        .ascii  "STUVWX(v8)YZabcdefghij(v9)klmnop"
        .ascii  "qrstuv(v10)wxyzABCDEFG(v11)HIJKL"
        .ascii  "MNOPQR(v12)STUVWXYZabc(v13)defgh"
        .ascii  "ijklmn(v14)opqrstuvwxy(v15)zABCD"
        .ascii  "EFGHIJ(v16)KLMNOPQRSTU(v17)VWXYZ"
        .ascii  "abcdef(v18)ghijklmnopq(v19)rstuv"
        .ascii  "wxyzAB(v20)CDEFGHIJKLM(v21)NOPQR"
        .ascii  "STUVWX(v22)YZabcdefghi(v23)jklmn"
        .ascii  "opqrst(v24)uvwxyzABCDE(v25)FGHIJ"
        .ascii  "KLMNOP(v26)QRSTUVWXYZa(v27)bcdef"
        .ascii  "ghijkl(v28)mnopqrstuvw(v29)xyzAB"
        .ascii  "CDEFGH(v30)IJKLMNOPQRS(v31)TUVWX"

        .ascii  "<-[PC]->" // exactly 8 bytes
str_tpidr_el0:
        .ascii  "tpidrel0" // exactly 8 bytes
        .4byte  0x05400000 // FPCR
        .4byte  0x08000095 // FPSR
        .4byte  0x90000000 // NZCV

        .align  2
        .global _start
        .type   _start, %function
_start:
        init_stack

        // Check we can write.
        mov64   x0, str_line1
        mov     x1, #strlen_line1
        write
        mov64   x0, str_line2
        mov     x1, #strlen_line2
        write

        // Check stack is usable.
        sub     sp, sp, #16
        mov64   x0, str_stack
        ldp     x0, x1, [x0]
        stp     x0, x1, [sp]
        mov     x0, sp
        mov     x1, #16
        write
        add     sp, sp, #16

        // We put this message on the stack now and print it out at the end
        // so we can detect a failure to restore the stack pointer correctly.
        sub     sp, sp, #16
        mov64   x0, str_end
        ldp     x0, x1, [x0]
        stp     x0, x1, [sp]

        // Check BR (register/indirect branch).
        sub     sp, sp, #32
        stp     xzr, xzr, [sp, #16]
        mov     x0, #0
        mov     x1, #0
        mov64   x2, 1f
        mov     x3, #0
        br      x2
        error
1:
        mov     w0, #'B'
        mov     w1, #'R'
        strb    w0, [sp, #16]
        strb    w1, [sp, #17]
        add     x0, sp, #16
        printn  2
        add     sp, sp, #32

        // Check B (immediate/direct branch, forwards).
        sub     sp, sp, #32
        stp     xzr, xzr, [sp, #16]
        mov     x0, #0
        mov     x1, #0
        b       1f
        error
1:
        mov     w0, #'B'
        mov     w1, #'f'
        strb    w0, [sp, #16]
        strb    w1, [sp, #17]
        add     x0, sp, #16
        printn  2
        add     sp, sp, #32

        // Check B (immediate/direct branch, backwards).
        sub     sp, sp, #32
        stp     xzr, xzr, [sp, #16]
        b       2f
        error
1:
        mov     w0, #'B'
        mov     w1, #'b'
        strb    w0, [sp, #16]
        strb    w1, [sp, #17]
        add     x0, sp, #16
        printn  2
        b       3f
        error
2:
        mov     x0, #0
        mov     x1, #0
        b       1b
        error
3:
        add     sp, sp, #32

        // Checking the maximum range of a B would require a big program.
        // One could, however, uncomment each of these two instructions in
        // turn and check the faulting address as reported by a debugger.
        //b       . + 0x7fffffc // maximum-range branch forwards
        //b       . - 0x8000000 // maximum-range branch backwards

        // Check registers X0 and X1 are preserved across branches.
        sub     sp, sp, #48
        stp     xzr, xzr, [sp, #16]
        mov64   x0, str_x0_x1
        ldp     x0, x1, [x0]
        mov     x2, #0
        mov64   x3, 1f
        br      x3
        error
1:
        b       2f
        error
2:
        stp     x0, x1, [sp, #16]
        add     x0, sp, #16
        printn  16
        add     sp, sp, #48

        // Check registers X2 and X3 are preserved across branches.
        sub     sp, sp, #48
        stp     xzr, xzr, [sp, #16]
        mov64   x2, str_x2_x3
        ldp     x2, x3, [x2]
        mov64   x0, 1f
        mov     x1, #0
        br      x0
        error
1:
        b       2f
        error
2:
        stp     x2, x3, [sp, 16]
        add     x0, sp, #16
        printn  16
        add     sp, sp, #48

        // Check registers X30 and SP are preserved across branches.
        sub     sp, sp, #48
        mov     x3, sp // save SP
        stp     xzr, xzr, [sp, #16]
        mov64   x30, str_x30_sp
        ldp     x30, x0, [x30]
        mov     sp, x0
        mov     x0, #0
        mov64   x1, 1f
        mov     x2, #0
        br      x1
        error
1:
        b       2f
        error
2:
        mov     x0, sp
        mov     sp, x3 // restore SP
        stp     x30, x0, [sp, #16]
        add     x0, sp, #16
        printn  16
        add     sp, sp, #48

        // Check CBNZ and CBZ.
        sub     sp, sp, #32
        add     x0, sp, #16
        addch   'C'
        addch   'B'
        addch   ':'
        addch   'a'
        mov     x1, #0x100000000
        cbnz    w1, 5f // CBNZ not taken
        addch   'b'
        mov     x2, #0x8000000000000000
        cbnz    x2, 2f // CBNZ forwards
        error
1:
        addch   'd'
        mov     x1, #0x8000000000000000
        cbz     x1, 5f // CBZ not taken
        addch   'e'
        mov     x2, #0x100000000
        cbz     w2, 4f // CBZ forwards
        error
2:
        addch   'c'
        mov     x3, #0x100000000
        cbnz    x3, 1b // CBNZ backwards
        error
3:
        addch   'g'
        mov     x1, #0
        cbz     x1, 5f // CBZ forwards
        error
4:
        addch   'f'
        mov     x3, #0x100000000
        cbz     w3, 3b // CBZ backwards
        error
5:
        addch   'h'
        addch   '\n'
        add     x1, sp, #16
        sub     x1, x0, x1
        add     x0, sp, #16
        write
        add     sp, sp, #32

        // Check repeated execution of the same block: B first.
        sub     sp, sp, #32
        add     x0, sp, #16
        addch   'L'
        addch   '1'
        addch   ':'
        mov     w1, #0 // Initialise counter.
        b       2f

1:
        // Start of loop. We branch here with a B the first time.
        mov     w2, #'0'
        add     w2, w2, w1
        strb    w2, [x0], #1
        add     w1, w1, #1
2:
        // Exit from loop if we have reached 10.
        sub     w2, w1, #10
        cbz     w2, 4f
        // Is counter odd or even?
        and     w2, w1, #1
        cbnz    w2, 3f
        // Branch back with B.
        b       1b
3:
        // Branch back with BR.
        mov64   x3, 1b
        br      x3
4:
        // End of loop.
        addch   '\n'
        add     x1, sp, #16
        sub     x1, x0, x1
        add     x0, sp, #16
        write
        add     sp, sp, #32

        // Check repeated execution of the same block: BR first.
        sub     sp, sp, #32
        add     x0, sp, #16
        addch   'L'
        addch   '2'
        addch   ':'
        mov     w1, #0 // Initialise counter.
        b       2f

1:
        // Start of loop. We branch here with a BR the first time.
        mov     w2, #'0'
        add     w2, w2, w1
        strb    w2, [x0], #1
        add     w1, w1, #1
2:
        // Exit from loop if we have reached 10.
        sub     w2, w1, #10
        cbz     w2, 4f
        // Is counter odd or even?
        and     w2, w1, #1
        cbz     w2, 3f
        // Branch back with B.
        b       1b
3:
        // Branch back with BR.
        mov64   x3, 1b
        br      x3
4:
        // End of loop.
        addch   '\n'
        add     x1, sp, #16
        sub     x1, x0, x1
        add     x0, sp, #16
        write
        add     sp, sp, #32

        // Check BL (immediate/direct call).
        // The encoding differs from B by only one bit so there is no
        // need to check branching forwards and backwards.
        mov     x0, #1
        mov     x1, #2
        mov     x2, #4
        mov     x3, #8
        mov     x30, #0
        bl      2f
1:
        error
2:
        eor     x0, x0, #1
        eor     x1, x1, #2
        eor     x2, x2, #4
        eor     x3, x3, #8
        orr     x0, x0, x1
        orr     x0, x0, x2
        orr     x0, x0, x3
        mov64   x1, 1b
        eor     x30, x30, x1
        orr     x0, x0, x30
        cbz     x0, 3f
        error
3:

        // Check RET (return, architecturally identical to BR).
        mov     x0, #1
        mov64   x1, 1f
        mov     x2, #4
        mov     x3, #8
        mov     x30, #16
        ret     x1
        error
1:
        eor     x0, x0, #1
        eor     x2, x2, #4
        eor     x3, x3, #8
        eor     x30, x30, #16
        orr     x0, x0, x2
        orr     x0, x0, x3
        orr     x0, x0, x30
        mov64   x2, 1b
        eor     x1, x1, x2
        orr     x0, x0, x1
        cbz     x0, 2f
        error
2:

        // Check BLR (register/indirect call).
        mov     x0, #1
        mov     x1, #2
        mov64   x2, 2f
        mov     x3, #8
        mov     x30, #0
        blr     x2
1:
        error
2:
        eor     x0, x0, #1
        eor     x1, x1, #2
        eor     x3, x3, #8
        orr     x0, x0, x1
        orr     x0, x0, x3
        mov64   x1, 2b
        eor     x2, x2, x1
        orr     x0, x0, x2
        mov64   x1, 1b
        eor     x30, x30, x1
        orr     x0, x0, x30
        cbz     x0, 3f
        error
3:

        // Check BLR X30 (a special case).
        mov     x0, #1
        mov     x1, #2
        mov     x2, #4
        mov     x3, #8
        mov64   x30, 2f
        blr     x30
1:
        error
2:
        eor     x0, x0, #1
        eor     x1, x1, #2
        eor     x2, x2, #4
        eor     x3, x3, #8
        orr     x0, x0, x1
        orr     x0, x0, x2
        orr     x0, x0, x3
        mov64   x1, 1b
        eor     x30, x30, x1
        orr     x0, x0, x30
        cbz     x0, 3f
        error
3:

        // Check repeated use of function.
        sub     sp, sp, #32
        b       1f
        error
func1:
        add     w0, w0, #1
        ret
        error
1:
        mov     w0, #'0'
        mov64   x1, func1
        mov64   x2, func2
        bl      func1
        blr     x1
        bl      func1
        blr     x1
        bl      func1
        blr     x2
        bl      func2
        blr     x2
        bl      func2
        mov     w1, #'f'
        mov     w2, #':'
        strb    w1, [sp, #16]
        strb    w2, [sp, #17]
        strb    w0, [sp, #18]
        add     x0, sp, #16
        printn  3
        b       2f
        error
func2:
        add     w0, w0, #1
        ret
        error
2:
        add     sp, sp, #32

        mov     x29, #0
        // We can now fearlessly use functions, each with its own stack frame.
        bl      check_tbnz_tbz
        bl      check_bcond
        bl      check_adr
        bl      check_adrp
        bl      check_regs_preserved
        bl      check_literals
        bl      check_special_reg
        bl      check_ldxr

        // Write final message, str_end, which should still be on the stack.
        mov     x0, sp
        mov     x1, #16
        write

        exit

        // Check TBNZ, TBZ.
check_tbnz_tbz:
        stp     x29, x30, [sp, #-48]!
        mov     x29, sp
        stp     xzr, xzr, [sp, #16]
        mov64   x0, str_tbnz_tbz
        ldp     x0, x1, [x0]
        mov     x2, #0
        mov     x3, #0
        mov     w30, #65
1:
        sub     w30, w30, #1
        cbz     w30, 3f
        ror     x0, x0, #1
        ror     x1, x1, #1
        ror     x2, x2, #1
        ror     x3, x3, #1
        tbnz    x0, #11, 2f
        orr     x2, x2, #1
2:
        tbz     x1, #19, 1b
        orr     x3, x3, #1
        b       1b
3:
        mvn     x2, x2
        ror     x2, x2, #53
        ror     x3, x3, #45
        stp     x2, x3, [sp, #16]
        add     x0, sp, #16
        printn  16
        ldp     x29, x30, [sp], #48
        ret

        // Check B.cond.
        .macro  bcond cond, c1, c2
        stp     xzr, xzr, [sp]
        str     xzr, [sp, #16]
        mov     x0, sp
        addch   \c1
        addch   \c2
        addch   ':'
        mov     w3, #0
1:
        lsl     x1, x3, #28
        msr     nzcv, x1
        mov     w1, #'1'
        b.\cond 2f
        mov     w1, #'0'
2:
        strb    w1, [x0], #1
        add     w3, w3, #1
        sub     w1, w3, #16
        cbnz    w1, 1b
        addch   '\n'
        mov     x1, sp
        sub     x1, x0, x1
        mov     x0, sp
        write
        .endm
check_bcond:
        sub     sp, sp, #32
        bcond   eq, 'e', 'q'
        bcond   ne, 'n', 'e'
        bcond   cs, 'c', 's'
        bcond   cc, 'c', 'c'
        bcond   mi, 'm', 'i'
        bcond   pl, 'p', 'l'
        bcond   vs, 'v', 's'
        bcond   vc, 'v', 'c'
        bcond   hi, 'h', 'i'
        bcond   ls, 'l', 's'
        bcond   ge, 'g', 'e'
        bcond   lt, 'l', 't'
        bcond   gt, 'g', 't'
        bcond   le, 'l', 'e'
        bcond   al, 'a', 'l'
        bcond   nv, 'n', 'v'
        add     sp, sp, #32
        ret

        // Check ADR.
check_adr:
1:
        adr     x1, . + 0xfffff
2:
        adr     x2, . - 0x100000
        mov64   x0, 1b + 0xfffff
        eor     x1, x1, x0
        mov64   x0, 2b - 0x100000
        eor     x2, x2, x0
        orr     x0, x1, x2
        cbz     x0, 3f
        error
3:
        ret

        // Check ADRP.
check_adrp:
1:
        adrp    x1, . + 0xfffff000
2:
        adrp    x2, . - 0x100000000
        mov64   x0, 1b + 0xfffff000
        eor     x1, x1, x0
        mov64   x0, 2b - 0x100000000
        eor     x2, x2, x0
        orr     x0, x1, x2
        bic     x0, x0, 0xfff
        cbz     x0, 3f
        error
3:
        ret

check_regs_preserved:
        sub     sp, sp, #1024
        stp     x29, x30, [sp, #-16]!
        mov     x29, sp
        // Check X0-X30 and SP are preserved across branches.
        // Initialise X0-X15.
        mov64   x16, str_xregs
        ldp     x0, x1, [x16], #16
        ldp     x2, x3, [x16], #16
        ldp     x4, x5, [x16], #16
        ldp     x6, x7, [x16], #16
        ldp     x8, x9, [x16], #16
        ldp     x10, x11, [x16], #16
        ldp     x12, x13, [x16], #16
        ldp     x14, x15, [x16], #16
        branch  x21, x22
        // Save X0-X15 to stack.
        add     x16, sp, #32
        stp     x0, x1, [x16], #16
        stp     x2, x3, [x16], #16
        stp     x4, x5, [x16], #16
        stp     x6, x7, [x16], #16
        stp     x8, x9, [x16], #16
        stp     x10, x11, [x16], #16
        stp     x12, x13, [x16], #16
        stp     x14, x15, [x16], #16
        // Initialise X16-X30 and SP.
        mov64   x0, str_xregs + 128
        ldp     x16, x17, [x0], #16
        ldp     x18, x19, [x0], #16
        ldp     x20, x21, [x0], #16
        ldp     x22, x23, [x0], #16
        ldp     x24, x25, [x0], #16
        ldp     x26, x27, [x0], #16
        ldp     x28, x29, [x0], #16
        ldp     x30, x1, [x0], #16
        mov     x15, sp // save SP
        mov     sp, x1
        branch  x11, x12
        // Save X16-X30 and SP to stack.
        add     x0, x15, #(32 + 128)
        stp     x16, x17, [x0], #16
        stp     x18, x19, [x0], #16
        stp     x20, x21, [x0], #16
        stp     x22, x23, [x0], #16
        stp     x24, x25, [x0], #16
        stp     x26, x27, [x0], #16
        stp     x28, x29, [x0], #16
        mov     x1, sp
        mov     sp, x15 // restore SP
        stp     x30, x1, [x0], #16
        // Print data from stack.
        add     x0, sp, #32
        mov     x1, #8
        mov     x2, #32
        bl      print_lines

        // Check V0-V31 are preserved across branches.
        mov64   x0, str_vregs
        ld1     {v0.2d-v3.2d}, [x0], #64
        ld1     {v4.2d-v7.2d}, [x0], #64
        ld1     {v8.2d-v11.2d}, [x0], #64
        ld1     {v12.2d-v15.2d}, [x0], #64
        ld1     {v16.2d-v19.2d}, [x0], #64
        ld1     {v20.2d-v23.2d}, [x0], #64
        ld1     {v24.2d-v27.2d}, [x0], #64
        ld1     {v28.2d-v31.2d}, [x0], #64
        branch  x1, x2
        add     x0, sp, #32
        st1     {v0.2d-v3.2d}, [x0], #64
        st1     {v4.2d-v7.2d}, [x0], #64
        st1     {v8.2d-v11.2d}, [x0], #64
        st1     {v12.2d-v15.2d}, [x0], #64
        st1     {v16.2d-v19.2d}, [x0], #64
        st1     {v20.2d-v23.2d}, [x0], #64
        st1     {v24.2d-v27.2d}, [x0], #64
        st1     {v28.2d-v31.2d}, [x0], #64
        // Print data from stack.
        add     x0, sp, #32
        mov     x1, #16
        mov     x2, #32
        bl      print_lines

        // Check NZCV is preserved across branches.
        mov     w0, #0
1:
        lsl     w1, w0, #28
        msr     nzcv, x1
        branch  x1, x2
        mrs     x2, nzcv
        eor     x2, x2, x0, lsl #28
        cbz     x2, 2f
        error
2:
        add     w0, w0, #1
        sub     w1, w0, #16
        cbnz    w1, 1b

        // Check FPCR and FPSR are preserved across branches.

        // Save original values of FPCR and FPSR.
        mrs     x1, fpcr
        mrs     x2, fpsr
        stp     x1, x2, [sp, #80]

        // Set FPCR and FPSR to zero, read them back and print.
        add     x0, sp, #32
        addch   'F'
        addch   'P'
        addch   '0'
        addch   ' '
        mov     x3, xzr
        msr     fpcr, x3
        msr     fpsr, x3
        branch  x1, x2
        mrs     x1, fpcr
        mrs     x2, fpsr
        str     x2, [sp, #64]
        bl      hex16
        addch   ' '
        ldr     x1, [sp, #64]
        bl      hex16
        addch   '\n'
        add     x1, sp, #32
        sub     x1, x0, x1
        add     x0, sp, #32
        write

        // Set some FPCR and FPSR bits to one, read them back and print.
        // We set only the bits that are available on all/most implementations.
        add     x0, sp, #32
        addch   'F'
        addch   'P'
        addch   '1'
        addch   ' '
        mov     x3, #0x07c00000 // AHP, DN, FZ; RMode
        msr     fpcr, x3
        mov     x3, #0x08000000 // QC
        movk    x3, #0x0000009f // IDC, IXC; UFC, OFC, DZC, IOC
        msr     fpsr, x3
        branch  x1, x2
        mrs     x1, fpcr
        mrs     x2, fpsr
        str     x2, [sp, #64]
        bl      hex16
        addch   ' '
        ldr     x1, [sp, #64]
        bl      hex16
        addch   '\n'
        add     x1, sp, #32
        sub     x1, x0, x1
        add     x0, sp, #32
        write

        // Restore original values of FPCR and FPSR.
        ldp     x1, x2, [sp, #80]
        msr     fpcr, x1
        msr     fpsr, x2

        // Check TPIDR_EL0 is preserved across branches.
        mrs     x0, tpidr_el0
        str     x0, [sp, #32]
        mov64   x0, str_tpidr_el0
        ldr     x0, [x0]
        msr     tpidr_el0, x0
        mov     x0, #0
        branch  x1, x2
        mrs     x0, tpidr_el0
        str     x0, [sp, #16]
        add     x0, sp, #16
        printn  8
        ldr     x0, [sp, #32]
        msr     tpidr_el0, x0

        ldp     x29, x30, [sp], #16
        add     sp, sp, #1024
        ret

        // Check literal loads. We assume ADR is working.
        // Compare literal with non-literal loads to avoid endianness issues.

        .align  4
data_s_b:
        .4byte  0xabcd1234
data_d_b:
        .8byte  0x1234567890abcdef
data_q_b:
        .8byte  0x1122334455667788, 0x9900aabbccddeeff
        .align  2

check_literals:
1:
        // LDR Wt, label
        adr     x0, data_s_b
        ldr     w0, [x0]
        ldr     w1, data_s_b
        eor     x2, x0, x1
        cbz     x2, 1f
        error
1:
        adr     x0, data_s_f
        ldr     w0, [x0]
        ldr     w1, data_s_f
        eor     x2, x0, x1
        cbz     x2, 1f
        error
1:
        // LDRSW Wt, label
        adr     x0, data_s_b
        ldrsw   x0, [x0]
        ldrsw   x1, data_s_b
        eor     x2, x0, x1
        cbz     x2, 1f
        error
1:
        adr     x0, data_s_f
        ldrsw   x0, [x0]
        ldrsw   x1, data_s_f
        eor     x2, x0, x1
        cbz     x2, 1f
        error
1:
        // LDR Xt, label
        adr     x0, data_d_b
        ldr     x0, [x0]
        ldr     x1, data_d_b
        eor     x2, x0, x1
        cbz     x2, 1f
        error
1:
        adr     x0, data_d_f
        ldr     x0, [x0]
        ldr     x1, data_d_f
        eor     x2, x0, x1
        cbz     x2, 1f
        error
1:
        // LDR St, label
        adr     x0, data_s_b
        ldr     s0, [x0]
        mov     w0, v0.s[0]
        ldr     s1, data_s_b
        mov     w1, v1.s[0]
        eor     x2, x0, x1
        cbz     x2, 1f
        error
1:
        adr     x0, data_s_f
        ldr     s0, [x0]
        mov     w0, v0.s[0]
        ldr     s1, data_s_f
        mov     w1, v1.s[0]
        eor     x2, x0, x1
        cbz     x2, 1f
        error
1:
        // LDR Dt, label
        adr     x0, data_d_b
        ldr     d0, [x0]
        mov     x0, v0.d[0]
        ldr     d1, data_d_b
        mov     x1, v1.d[0]
        eor     x2, x0, x1
        cbz     x2, 1f
        error
1:
        adr     x0, data_d_f
        ldr     d0, [x0]
        mov     x0, v0.d[0]
        ldr     d1, data_d_f
        mov     x1, v1.d[0]
        eor     x2, x0, x1
        cbz     x2, 1f
        error
        // LDR Qt, label
        adr     x0, data_q_b
        ldr     q0, [x0]
        mov     x0, v0.d[0]
        mov     x1, v0.d[1]
        ldr     q1, data_q_b
        mov     x2, v1.d[0]
        mov     x3, v1.d[1]
        eor     x0, x0, x2
        eor     x1, x1, x3
        orr     x0, x0, x1
        cbz     x0, 1f
        error
1:
        adr     x0, data_q_f
        ldr     q0, [x0]
        mov     x0, v0.d[0]
        mov     x1, v0.d[1]
        ldr     q1, data_q_f
        mov     x2, v1.d[0]
        mov     x3, v1.d[1]
        eor     x0, x0, x2
        eor     x1, x1, x3
        orr     x0, x0, x1
        cbz     x0, 1f
        error
1:
        ret

        .align  4
data_s_f:
        .4byte  0xaabbccdd
data_d_f:
        .8byte  0xabcdef0123456789
data_q_f:
        .8byte  0xaabbccddeeff0011, 0x2233445566778899
        .align  2

        // Check use of "special" register (xs).
check_special_reg:
        stp     x29, x30, [sp, #-16]!
        mov     x29, sp
        sub     sp, sp, #800
        // Load register state.
        adr     x0, str_regstate
        bl      load_state
        ldp     x0, x1, [x0]
        bl      check_special_reg_loop
        // Save register state.
        stp     x0, x1, [sp]
        mov     x0, sp
        bl      save_state
        // Fix up X30, SP, PC.
        adr     x0, str_regstate
        ldp     x1, x2, [x0, #240]
        stp     x1, x2, [sp, #240]
        ldr     x1, [x0, #768]
        str     x1, [sp, #768]
        // Print register state.
        mov     x0, sp
        bl      print_state
        add     sp, sp, #800
        ldp     x29, x30, [sp], #16
        ret

        // We execute these tests in a loop in case the test system uses
        // optimisations after a certain number of repeats.
check_special_reg_loop:
        stp     xm, xn, [sp, #-32]!
        stp     xs, x30, [sp, #16]
        mov     xm, #1200
1:
        bl      xs_readwrite
        bl      xs_cbranches
        bl      xs_rbranches
        bl      xs_adr
        bl      xs_ldr
        sub     xm, xm, #1
        cbnz    xm, 1b
        ldp     xs, x30, [sp, #16]
        ldp     xm, xn, [sp], #32
        ret

        // Test instructions that read and write xs.
xs_readwrite:
        mov     xn, #3
        mul     xs, xn, xn // write xs
        mul     xn, xs, xs // read xs
        sub     xn, xn, #81
        cbz     xn, 1f
        error
1:
        mov     xs, #5
        mul     xs, xs, xs // read and write xs
        sub     xn, xs, #25
        cbz     xn, 2f
        error
2:
        ret

        // Test conditional branches using xs.
xs_cbranches:
        // Set xs to zero in non-obvious way.
        bic     xs, xs, #0xffff
        mul     xs, xs, xs
        mul     xs, xs, xs
        cbz     xs, 2f
1:
        error
2:
        add     xs, xs, #1 // xs = 1
        cbz     xs, 1b
        tbz     xs, #1, 4f
3:
        error
4:
        add     xs, xs, #1 // xs = 2
        tbz     xs, #1, 3b
        ret

        // Test register/indirect branches using xs.
xs_rbranches:
        str     x30, [sp, #-16]!
        mov64   xs, 2f
        blr     xs
1:
        error
2:
        mov64   xn, 1b
        eor     xn, xn, x30
        cbnz    xn, 1b
        mov64   xs, 3f
        br      xs
        error
3:
        mov64   xs, 4f
        ret     xs
        error
4:
        ldr     x30, [sp], #16
        ret

        // Test ADR/ADRP using xs.
xs_adr:
        adr     xs, xs_adr + 0x1234
        mov64   xn, xs_adr + 0x1234
        eor     xn, xn, xs
        cbz     xn, 1f
        error
1:
        adrp    xs, xs_adr + 0x12345678
        mov64   xn, xs_adr + 0x12345678
        bic     xn, xn, #0xfff
        eor     xn, xn, xs
        cbz     xn, 2f
        error
2:
        ret

        // Test literal loads using xs.
xs_ldr:
        ldr     xs, xs_ldr_data
        adr     xn, xs_ldr_data
        ldr     xn, [xn]
        eor     xn, xn, xs
        cbz     xn, 1f
        error
1:
        ret
xs_ldr_data:
        .4byte  0xabc12345

        // Check load/store exclusive.
check_ldxr:
        sub     sp, sp, #16
        str     x30, [sp, #8]
1:
        ldxr    xn, [sp]
        add     xn, xn, #1
        stxr    w30, xn, [sp]
        cbnz    w30, 1b
        ldr     x30, [sp, #8]
        add     sp, sp, #16
        ret

        // Load register state except X0-X1, X30, SP, PC from struct at X0.
        // Order of registers is: X0-X30, SP, V0-V31, PC, TPIDR_EL0,
        // FPCR, FPSR, NZCV. Total of 798 bytes.
load_state:
        ldr     x8, [x0, #776]
        ldr     w9, [x0, #784]
        ldr     w10, [x0, #788]
        ldr     w11, [x0, #792]
        msr     tpidr_el0, x8
        msr     fpcr, x9
        msr     fpsr, x10
        msr     nzcv, x11
        ldp     x2, x3, [x0, #16]
        ldp     x4, x5, [x0, #32]
        ldp     x6, x7, [x0, #48]
        ldp     x8, x9, [x0, #64]
        ldp     x10, x11, [x0, #80]
        ldp     x12, x13, [x0, #96]
        ldp     x14, x15, [x0, #112]
        ldp     x16, x17, [x0, #128]
        ldp     x18, x19, [x0, #144]
        ldp     x20, x21, [x0, #160]
        ldp     x22, x23, [x0, #176]
        ldp     x24, x25, [x0, #192]
        ldp     x26, x27, [x0, #208]
        ldp     x28, x29, [x0, #224]
        add     x0, x0, #256
        ld1     {v0.2d-v3.2d}, [x0], #64
        ld1     {v4.2d-v7.2d}, [x0], #64
        ld1     {v8.2d-v11.2d}, [x0], #64
        ld1     {v12.2d-v15.2d}, [x0], #64
        ld1     {v16.2d-v19.2d}, [x0], #64
        ld1     {v20.2d-v23.2d}, [x0], #64
        ld1     {v24.2d-v27.2d}, [x0], #64
        ld1     {v28.2d-v31.2d}, [x0], #64
        sub     x0, x0, #768
        ret

        // Save register state except X0-X1, X30, SP, PC to struct at X0.
        // See load_state, above.
save_state:
        stp     x2, x3, [x0, #16]
        stp     x4, x5, [x0, #32]
        stp     x6, x7, [x0, #48]
        stp     x8, x9, [x0, #64]
        stp     x10, x11, [x0, #80]
        stp     x12, x13, [x0, #96]
        stp     x14, x15, [x0, #112]
        stp     x16, x17, [x0, #128]
        stp     x18, x19, [x0, #144]
        stp     x20, x21, [x0, #160]
        stp     x22, x23, [x0, #176]
        stp     x24, x25, [x0, #192]
        stp     x26, x27, [x0, #208]
        stp     x28, x29, [x0, #224]
        add     x8, x0, #256
        mrs     x11, tpidr_el0
        mrs     x12, fpcr
        mrs     x13, fpsr
        mrs     x14, nzcv
        st1     {v0.2d-v3.2d}, [x8], #64
        st1     {v4.2d-v7.2d}, [x8], #64
        st1     {v8.2d-v11.2d}, [x8], #64
        st1     {v12.2d-v15.2d}, [x8], #64
        st1     {v16.2d-v19.2d}, [x8], #64
        st1     {v20.2d-v23.2d}, [x8], #64
        st1     {v24.2d-v27.2d}, [x8], #64
        st1     {v28.2d-v31.2d}, [x8], #64
        str     x11, [x8, #8]
        stp     w12, w13, [x8, #16]
        str     w14, [x8, #24]
        ret

        // Print register state from struct at X0.
        // FPCR, FPSR and NZCV are printed as hex; other registers should contain
        // printable characters.
print_state:
        stp     x29, x30, [sp, #-48]!
        mov     x29, sp
        mov     x19, x0
        // Print X0-X31.
        mov     x1, #8
        mov     x2, #32
        bl      print_lines
        // Print V0-V31.
        add     x0, x19, #256
        mov     x1, #16
        mov     x2, #32
        bl      print_lines
        // Print PC and TPIDR_EL0.
        add     x0, x19, #768
        mov     x1, #8
        mov     x2, #2
        bl      print_lines
        // Print FPCR, FPSR, NZCV.
        add     x0, sp, #16
        ldr     w1, [x19, #784] // FPCR
        bl      hex16
        add     x0, sp, #16
        printn  16
        add     x0, sp, #16
        ldr     w1, [x19, #788] // FPSR
        bl      hex16
        add     x0, sp, #16
        printn  16
        add     x0, sp, #16
        ldr     w1, [x19, #792] // NZCV
        bl      hex16
        add     x0, sp, #16
        printn  16
        ldp     x29, x30, [sp], #48
        ret

        // Output data starting at X0 in lines with X1 bytes per line,
        // X2 lines. The data is temporarily overwritten with '\n',
        // as required.
print_lines:
        stp     x29, x30, [sp, #-48]!
        mov     x29, sp
1:
        ldrb    w3, [x0, x1]
        mov     w30, #'\n'
        strb    w30, [x0, x1]
        stp     x0, x1, [sp, #16]
        stp     x2, x3, [sp, #32]
        add     x1, x1, #1
        write
        ldp     x0, x1, [sp, #16]
        ldp     x2, x3, [sp, #32]
        strb    w3, [x0, x1]
        add     x0, x0, x1
        sub     x2, x2, #1
        cbnz    x2, 1b
        ldp     x29, x30, [sp], #48
        ret

        // Convert X1 to hex and append at X0.
hex16:
        stp     x29, x30, [sp, #-16]!
        mov     x29, sp
        mov64   x2, digits
        mov     w3, #16
1:
        lsr     x30, x1, #60
        ldrb    w30, [x2, x30]
        strb    w30, [x0], #1
        lsl     x1, x1, #4
        sub     w3, w3, #1
        cbnz    w3, 1b
        ldp     x29, x30, [sp], #16
        ret
