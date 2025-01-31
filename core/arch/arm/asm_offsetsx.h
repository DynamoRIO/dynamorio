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

OFFSET(dcontext_t, dstack, 0x174)
#define dcontext_t_OFFSET_dstack 0x174
OFFSET(dcontext_t, is_exiting, 0x178)
#define dcontext_t_OFFSET_is_exiting 0x178

OFFSET(priv_mcontext_t, r0, 0x0)
#define priv_mcontext_t_OFFSET_r0 0x0
OFFSET(priv_mcontext_t, sp, 0x34)
#define priv_mcontext_t_OFFSET_sp 0x34
OFFSET(priv_mcontext_t, lr, 0x38)
#define priv_mcontext_t_OFFSET_lr 0x38
OFFSET(priv_mcontext_t, pc, 0x3c)
#define priv_mcontext_t_OFFSET_pc 0x3c
OFFSET(priv_mcontext_t, cpsr, 0x40)
#define priv_mcontext_t_OFFSET_cpsr 0x40
OFFSET(priv_mcontext_t, simd, 0x48)
#define priv_mcontext_t_OFFSET_simd 0x48
SIZE(priv_mcontext_t, 0x148)
#define priv_mcontext_t_SIZE 0x148
