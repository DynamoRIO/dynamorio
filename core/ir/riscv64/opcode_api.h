/* **********************************************************
 * Copyright (c) 2022 Rivos, Inc.  All rights reserved.
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
 * * Neither the name of Rivos, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL RIVOS, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef _DR_IR_OPCODES_RISCV64_H_
#define _DR_IR_OPCODES_RISCV64_H_ 1

/****************************************************************************
 * OPCODES
 */
/**
 * @file dr_ir_opcodes_arm.h
 * @brief Instruction opcode constants for RISC-V.
 */
/** Opcode constants for use in the instr_t data structure. */
enum {
    /*   0 */ OP_INVALID,
    /* NULL, */ /**< INVALID opcode */
    /*   1 */ OP_UNDECODED,
    /* NULL, */ /**< UNDECODED opcode */
    /*   2 */ OP_CONTD,
    /* NULL, */ /**< CONTD opcode */
    /*   3 */ OP_LABEL,
    /* NULL, */ /**< LABEL opcode */

    /* FIXME i#3544: Put real opcodes here and make sure ordering matches HW. */
    OP_ecall, /**< RISC-V ecall opcode. */
    OP_l,     /**< RISC-V integer load opcode. Width is encoded in operand sizes. */
    OP_s,     /**< RISC-V integer store opcode. Width is encoded in operand sizes. */
    OP_add,   /**< RISC-V add opcode. */
    OP_addi,  /**< RISC-V addi opcode. */
    OP_sub,   /**< RISC-V sub opcode. */
    OP_mul,   /**< RISC-V mul opcode. */
    OP_mulu,  /**< RISC-V mulu opcode. */
    OP_div,   /**< RISC-V div opcode. */
    OP_divu,  /**< RISC-V divu opcode. */
    OP_jal,   /**< RISC-V jal opcode. */
    OP_jalr,  /**< RISC-V jalr opcode. */
    OP_beq,   /**< RISC-V beq opcode. */
    OP_bne,   /**< RISC-V bne opcode. */
    OP_blt,   /**< RISC-V blt opcode. */
    OP_bltu,  /**< RISC-V bltu opcode. */
    OP_bge,   /**< RISC-V bge opcode. */
    OP_bgeu,  /**< RISC-V bgeu opcode. */
    OP_hint,  /**< RISC-V hint opcode. */

#if defined(RISCV_ISA_F) || defined(RISCV_ISA_D)
    OP_fl, /**< RISC-V FP load opcode. Width is encoded in operand sizes. */
    OP_fs, /**< RISC-V FP store opcode. Width is encoded in operand sizes. */
#endif

    OP_AFTER_LAST,
    /* FIXME i#3544: Replace OP_hint with a real opcode. */
    OP_FIRST = OP_hint,          /**< First real opcode. */
    OP_LAST = OP_AFTER_LAST - 1, /**< Last real opcode. */
};

/* alternative names */
#define OP_jmp OP_hint       /**< Platform-independent opcode name for jump. */
#define OP_jmp_short OP_hint /**< Platform-independent opcode name for short jump. */
#define OP_load OP_hint      /**< Platform-independent opcode name for load. */
#define OP_store OP_hint     /**< Platform-independent opcode name for store. */

#endif /* _DR_IR_OPCODES_RISCV64_H_ */
