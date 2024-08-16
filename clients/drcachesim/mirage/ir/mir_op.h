#ifndef __MIR_OP_H_
#define __MIR_OP_H

typedef enum {
    // REGISTER OPERATION

    // Arithmetic

    OP_ADD,  // reg-reg add
    OP_ADDI, // reg-imm add

    OP_SUB,  // reg-reg sub
    OP_SUBI, // reg-imm sub

    OP_MUL,  // reg-reg mul
    OP_MULI, // reg-imm mul

    OP_DIV,  // reg-reg div
    OP_DIVI, // reg-imm div

    OP_DIVU, // reg-reg unsigned-div
    OP_DIVUI,// reg-imm unsigned-div

    OP_REM,  // reg-reg rem
    OP_REMI, // reg-imm rem

    OP_REMU, // reg-reg unsigned-rem
    OP_REMUI,// reg-imm unsigned-rem

    // Bitwise

    OP_AND,  // reg-reg and
    OP_ANDI, // reg-imm and

    OP_OR,   // reg-reg or
    OP_ORI,  // reg-imm or

    OP_XOR,  // reg-reg xor
    OP_XORI, // reg-imm xor

    // Memory

    OP_LD8,  // load 8-bit
    OP_LD16, // load 16-bit
    OP_LD32, // load 32-bit
    OP_LD64, // load 64-bit

} mir_op_t;