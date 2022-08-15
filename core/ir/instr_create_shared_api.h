/* **********************************************************
 * Copyright (c) 2011-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2002-2010 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2002-2003 Massachusetts Institute of Technology */

#ifndef _DR_IR_MACROS_H_
#define _DR_IR_MACROS_H_ 1

/**
 * @file dr_ir_macros.h
 * @brief Cross-platform instruction creation convenience macros.
 */

#ifndef DYNAMORIO_INTERNAL
#    ifdef X86
#        include "dr_ir_macros_x86.h"
#    elif defined(AARCH64)
#        include "dr_ir_macros_aarch64.h"
#    elif defined(ARM)
#        include "dr_ir_macros_arm.h"
#    elif defined(RISCV64)
#        include "dr_ir_macros_riscv64.h"
#    endif
#    include "dr_ir_opnd.h"
#    include "dr_ir_instr.h"
#    include "dr_ir_utils.h"
#endif
#include <limits.h> /* For SCHAR_MAX, SCHAR_MIN. */

/**
 * Set the translation field for an instruction. For example:
 * instr_t *pushf_instr = INSTR_XL8(INSTR_CREATE_pushf(drcontext), addr);
 */
#define INSTR_XL8(instr_ptr, app_addr) instr_set_translation((instr_ptr), (app_addr))

/**
 * Set the predication value for an instruction.
 */
#define INSTR_PRED(instr_ptr, pred) instr_set_predicate((instr_ptr), (pred))

/**
 * Set an encoding hint for an instruction.
 */
#define INSTR_ENCODING_HINT(instr_ptr, hint) instr_set_encoding_hint((instr_ptr), (hint))

/* operand convenience routines for common cases */
/** Create a base+disp 8-byte operand. */
#define OPND_CREATE_MEM64(base_reg, disp) \
    opnd_create_base_disp(base_reg, DR_REG_NULL, 0, disp, OPSZ_8)
/** Create a base+disp 4-byte operand. */
#define OPND_CREATE_MEM32(base_reg, disp) \
    opnd_create_base_disp(base_reg, DR_REG_NULL, 0, disp, OPSZ_4)
/** Create a base+disp 2-byte operand. */
#define OPND_CREATE_MEM16(base_reg, disp) \
    opnd_create_base_disp(base_reg, DR_REG_NULL, 0, disp, OPSZ_2)
/** Create a base+disp 1-byte operand. */
#define OPND_CREATE_MEM8(base_reg, disp) \
    opnd_create_base_disp(base_reg, DR_REG_NULL, 0, disp, OPSZ_1)
#ifdef X64
/** Create a base+disp pointer-sized operand. */
#    define OPND_CREATE_MEMPTR OPND_CREATE_MEM64
#else
/** Create a base+disp pointer-sized operand. */
#    define OPND_CREATE_MEMPTR OPND_CREATE_MEM32
#endif

#ifdef X64
/**
 * Create an 8-byte immediate integer operand.
 * \note This is only relevant for x86: for ARM where immediate sizes are
 * ignored, simply use OPND_CREATE_INT().
 */
#    define OPND_CREATE_INT64(val) opnd_create_immed_int((ptr_int_t)(val), OPSZ_8)
/**
 * Create a pointer-sized immediate integer operand.
 * \note This is only relevant for x86: for ARM where immediate sizes are
 * ignored, simply use OPND_CREATE_INT().
 */
#    define OPND_CREATE_INTPTR OPND_CREATE_INT64
#else
/**
 * Create a pointer-sized immediate integer operand.
 * \note This is only relevant for x86: for ARM where immediate sizes are
 * ignored, simply use OPND_CREATE_INT().
 */
#    define OPND_CREATE_INTPTR OPND_CREATE_INT32
#endif
/**
 * Create a 4-byte immediate integer operand.
 * \note This is only relevant for x86: for ARM where immediate sizes are
 * ignored, simply use OPND_CREATE_INT().
 */
#define OPND_CREATE_INT32(val) opnd_create_immed_int((ptr_int_t)(val), OPSZ_4)
/**
 * Create a 2-byte immediate integer operand.
 * \note This is only relevant for x86: for ARM where immediate sizes are
 * ignored, simply use OPND_CREATE_INT().
 */
#define OPND_CREATE_INT16(val) opnd_create_immed_int((ptr_int_t)(val), OPSZ_2)
/**
 * Create a 1-byte immediate integer operand.
 * \note This is only relevant for x86: for ARM where immediate sizes are
 * ignored, simply use OPND_CREATE_INT().
 */
#define OPND_CREATE_INT8(val) opnd_create_immed_int((ptr_int_t)(val), OPSZ_1)
/**
 * Create a 1-byte immediate integer operand if val will fit, else create a 4-byte
 * immediate integer operand.
 * \note This is only relevant for x86: for ARM where immediate sizes are
 * ignored, simply use OPND_CREATE_INT().
 */
#define OPND_CREATE_INT_32OR8(val)                                               \
    ((val) <= SCHAR_MAX && (ptr_int_t)(val) >= SCHAR_MIN ? OPND_CREATE_INT8(val) \
                                                         : OPND_CREATE_INT32(val))
/**
 * Create a 1-byte immediate interger operand if val will fit, else create a 2-byte
 * immediate integer operand.
 * \note This is only relevant for x86: for ARM where immediate sizes are
 * ignored, simply use OPND_CREATE_INT().
 */
#define OPND_CREATE_INT_16OR8(val)                                               \
    ((val) <= SCHAR_MAX && (ptr_int_t)(val) >= SCHAR_MIN ? OPND_CREATE_INT8(val) \
                                                         : OPND_CREATE_INT16(val))

/**
 * Creates an instr_t with opcode OP_LABEL.  An OP_LABEL instruction can be used as a
 * jump or call instr_t target, and when emitted it will take no space in the
 * resulting machine code.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 */
#define INSTR_CREATE_label(dc) instr_create_0dst_0src((dc), OP_LABEL)

#endif /* _DR_IR_MACROS_H_ */
