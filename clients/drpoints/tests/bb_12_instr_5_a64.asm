// This program executes 12 basic blocks of 5 instructions each one after the other.

.globl _start

// Block 0. Program Start.
_start:
    mov x0, #0
    add x0, x0, #1
    mov x1, #10
    add x2, x0, x1
    b block_1           // Jump to next block.

// Block 1.
block_1:
    add x0, x0, #1
    mov x5, #20
    add x6, x0, x5
    eor x4, x0, x1
    b block_2           // Jump to next block.

// Block 2.
block_2:
    add x0, x0, #1
    mov x9, #30
    add x10, x0, x9
    orr x11, x0, x9
    b block_3           // Jump to next block.

// Block 3.
block_3:
    add x0, x0, #1
    mov x2, #40
    sub x3, x0, x2
    and x4, x0, x2
    b block_4           // Jump to next block.

// Block 4.
block_4:
    add x0, x0, #1
    mov x5, #50
    add x6, x0, x5
    bic x7, x0, x5
    b block_5           // Jump to next block.

// Block 5.
block_5:
    add x0, x0, #1
    mov x8, #60
    mul x9, x0, x8
    mov x10, #70
    b block_6           // Jump to next block.

// Block 6.
block_6:
    add x0, x0, #1
    mov x11, #80
    madd x1, x0, x11, x8
    subs x3, x0, x2
    b block_7           // Jump to next block.

// Block 7.
block_7:
    add x0, x0, #1
    csel x4, x0, x2, eq
    mov x5, #90
    add x6, x0, x5
    b block_8           // Jump to next block.

// Block 8.
block_8:
    add x0, x0, #1
    mov x7, #100
    lsr x8, x0, #1
    lsl x9, x0, #2
    b block_9          // Jump to next block.

// Block 9.
block_9:
    add x0, x0, #1
    mov x10, #110
    asr x11, x0, #1
    mov x1, #120
    b block_10          // Jump to next block.

// Block 10.
block_10:
    add x0, x0, #1
    mov x2, #130
    add x3, x0, x2
    mov x4, #140
    b program_exit      // Jump to next block.

// Block 11. Program Exit.
program_exit:
    add x0, x0, #1
    mov x5, #150
    mov x8, #93    // x8 will contain the syscall number for exit (93 on Linux AArch64).
    mov x0, #0     // Set exit status to 0.
    svc #0         // Invoke the system call.
