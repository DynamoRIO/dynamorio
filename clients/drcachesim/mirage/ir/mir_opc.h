#ifndef __MIR_OP_H_
#define __MIR_OP_H

// This file defines a reduced set of operations for the Mirage IR

typedef enum {

    // NULL OPERATION
    MIR_OP_NULL,
    
    // REGISTER OPERATION

    // Arithmetic

    MIR_OP_ADD,  // reg-reg add
    // MIR_OP_ADDI, // reg-imm add

    MIR_OP_SUB,  // reg-reg sub
    // MIR_OP_SUBI, // reg-imm sub

    MIR_OP_MUL,  // reg-reg mul
    // MIR_OP_MULI, // reg-imm mul

    MIR_OP_DIV,  // reg-reg div
    // MIR_OP_DIVI, // reg-imm div

    MIR_OP_DIVU, // reg-reg unsigned-div
    // MIR_OP_DIVUI,// reg-imm unsigned-div

    MIR_OP_REM,  // reg-reg rem
    // MIR_OP_REMI, // reg-imm rem

    // MIR_OP_REMU, // reg-reg unsigned-rem
    // MIR_OP_REMUI,// reg-imm unsigned-rem

    // Bitwise

    MIR_OP_AND,  // reg-reg and
    // MIR_OP_ANDI, // reg-imm and

    MIR_OP_OR,   // reg-reg or
    // MIR_OP_ORI,  // reg-imm or

    MIR_OP_XOR,  // reg-reg xor
    // MIR_OP_XORI, // reg-imm xor

    // Memory

    MIR_OP_LD8,  // load 8-bit
    MIR_OP_LD16, // load 16-bit
    MIR_OP_LD32, // load 32-bit
    MIR_OP_LD64, // load 64-bit

    MIR_OP_ST8,  // store 8-bit
    MIR_OP_ST16, // store 16-bit
    MIR_OP_ST32, // store 32-bit
    MIR_OP_ST64, // store 64-bit

} mir_opc_t;

static const char* mir_opc_str[] = {
    "NULL",
    "ADD",
    "SUB",
    "MUL",
    "DIV",
    "DIVU",
    "REM",
    "REMU",
    "AND",
    "OR",
    "XOR",
    "LD8",
    "LD16",
    "LD32",
    "LD64",
    "ST8",
    "ST16",
    "ST32",
    "ST64",
};

inline const char* mir_opc_to_str(mir_opc_t op){
    // directly cast to string, not as a pointer
    return mir_opc_str[op];
}

#endif // __MIR_OP_H_