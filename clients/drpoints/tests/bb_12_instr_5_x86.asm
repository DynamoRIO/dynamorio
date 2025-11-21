// This program executes 12 basic blocks of 5 instructions each one after the other.

.text
.global _start

// Use Intel syntax.
.intel_syntax noprefix

// Block 0. Program Start.
_start:
    nop
    nop
    nop
    nop
    jmp _block1  // Jump to next block.

// Block 1.
_block1:
    mov rax, 1
    nop
    nop
    nop
    jmp _block2  // Jump to next block.

// Block 2.
_block2:
    mov rbx, 2
    add rax, rbx
    nop
    nop
    jmp _block3  // Jump to next block.

// Block 3.
_block3:
    mov rcx, 3
    add rax, rcx
    nop
    nop
    jmp _block4  // Jump to next block.

// Block 4.
_block4:
    mov rdx, 4
    add rax, rdx
    nop
    nop
    jmp _block5  // Jump to next block.

// Block 5.
_block5:
    mov rsi, 5
    add rax, rsi
    nop
    nop
    jmp _block6  // Jump to next block.

// Block 6.
_block6:
    mov r8, 6
    add rax, r8
    nop
    nop
    jmp _block7  // Jump to next block.

// Block 7.
_block7:
    mov r9, 7
    add rax, r9
    nop
    nop
    jmp _block8  // Jump to next block.

// Block 8.
_block8:
    mov r10, 8
    add rax, r10
    nop
    nop
    jmp _block9  // Jump to next block.

// Block 9.
_block9:
    mov r11, 9
    add rax, r11
    nop
    nop
    jmp _block10 // Jump to next block.

// Block 10.
_block10:
    mov r12, 10
    add rax, r12
    nop
    nop
    jmp _exit    // Jump to next block.

// Block 11. Program Exit.
_exit:
    nop
    nop
    mov rax, 60  // syscall number for exit
    xor rdi, rdi // exit code 0
    syscall
