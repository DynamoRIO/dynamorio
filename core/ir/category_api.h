/* **********************************************************
 * Copyright (c) 2023 Google, Inc.  All rights reserved.
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

#ifndef _DR_IR_CATEGORIES_H_
#define _DR_IR_CATEGORIES_H_ 1

/****************************************************************************
 * CATEGORIES
 */
/**
 * @file dr_ir_categories.h
 * @brief Instruction category constants.
 */

/** This enum corresponds the instruction category */
typedef enum _dr_instr_category_type_t {
    DR_INSTR_CATEGORY_UNCATEGORIZED = 0x0, /**< Uncategorised. */
    DR_INSTR_CATEGORY_MATH_INT = 0x1,      /**< Integer arithmetic operations */
    DR_INSTR_CATEGORY_MATH_FLOAT = 0x2,    /**< Floating-Point arithmetic operations */
    DR_INSTR_CATEGORY_LOAD = 0x4,          /**< Loads */
    DR_INSTR_CATEGORY_STORE = 0x8,         /**< Stores */
    DR_INSTR_CATEGORY_BRANCH = 0x10,       /**< Branches */
    DR_INSTR_CATEGORY_SIMD = 0x20,         /**< Operations with vector registers (SIMD) */
    DR_INSTR_CATEGORY_OTHER = 0x40         /**< Other types of instructions */
} dr_instr_category_type_t;

#endif /* _DR_IR_CATEGORIES_H_ */
