/* **********************************************************
 * Copyright (c) 2025 ARM Limited. All rights reserved.
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

/* This file is included in two places:
 * - by asm_offsets.c, which checks at compile time that values are correct;
 * - by asm_offsets.h, which defines the values for use in .asm files.
 *
 * XXX: The repetition of each offset/size value in this file is not ideal
 * but the alternatives seem worse:
 * - generate the header file with a C program at build time;
 * - require a developer to update two files when adding a value.
 */

#if !defined(OFFSET) || !defined(SIZE)
#    error Define OFFSET and SIZE before including asm_offsetsx.h!
#endif

OFFSET(dcontext_t, dstack, 0x9f8)
#define dcontext_t_OFFSET_dstack 0x9f8
OFFSET(dcontext_t, is_exiting, 0xa00)
#define dcontext_t_OFFSET_is_exiting 0xa00

OFFSET(icache_op_struct_t, flag, 0)
#define icache_op_struct_t_OFFSET_flag 0
OFFSET(icache_op_struct_t, lock, 4)
#define icache_op_struct_t_OFFSET_lock 4
OFFSET(icache_op_struct_t, linesize, 8)
#define icache_op_struct_t_OFFSET_linesize 8
OFFSET(icache_op_struct_t, begin, 16)
#define icache_op_struct_t_OFFSET_begin 16
OFFSET(icache_op_struct_t, end, 24)
#define icache_op_struct_t_OFFSET_end 24
OFFSET(icache_op_struct_t, spill, 32)
#define icache_op_struct_t_OFFSET_spill 32

OFFSET(priv_mcontext_t, simd, 288)
#define priv_mcontext_t_OFFSET_simd 288
SIZE(priv_mcontext_t, 2480)
#define priv_mcontext_t_SIZE 2480

OFFSET(spill_state_t, r0, 0)
#define spill_state_t_OFFSET_r0 0
OFFSET(spill_state_t, r1, 8)
#define spill_state_t_OFFSET_r1 8
OFFSET(spill_state_t, r2, 16)
#define spill_state_t_OFFSET_r2 16
OFFSET(spill_state_t, r3, 24)
#define spill_state_t_OFFSET_r3 24
OFFSET(spill_state_t, r4, 32)
#define spill_state_t_OFFSET_r4 32
OFFSET(spill_state_t, r5, 40)
#define spill_state_t_OFFSET_r5 40
OFFSET(spill_state_t, fcache_return, 64)
#define spill_state_t_OFFSET_fcache_return 64

#if !defined(ANDROID)
OFFSET(struct tlsdesc_t, arg, 8)
#    define struct_tlsdesc_t_OFFSET_arg 8
#endif
