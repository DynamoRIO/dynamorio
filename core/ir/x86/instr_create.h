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

#ifndef _INSTR_CREATE_H_
#define _INSTR_CREATE_H_ 1

/* Non-exported instr create macros. */

/* Convenience routine for nop of certain size.
 * Note that Intel now recommends a different set of multi-byte nops,
 * but we stick with these as our tools (mainly windbg) don't understand
 * the OP_nop_modrm encoding (though should work on PPro+).
 * \param dc The void * dcontext used to allocate memory for the instr_t.
 * \param n  The number of bytes in the encoding of the nop.
 */
#define INSTR_CREATE_nopNbyte(dc, n) instr_create_nbyte_nop(dc, n, false)

/* convenience routines for when you only need raw bits */
#define INSTR_CREATE_RAW_pushf(dc) instr_create_raw_1byte(dc, 0x9c)
#define INSTR_CREATE_RAW_popf(dc) instr_create_raw_1byte(dc, 0x9d)
#define INSTR_CREATE_RAW_pusha(dc) instr_create_raw_1byte(dc, 0x60)
#define INSTR_CREATE_RAW_popa(dc) instr_create_raw_1byte(dc, 0x61)
#define INSTR_CREATE_RAW_nop(dc) instr_create_raw_1byte(dc, 0x90)
#define INSTR_CREATE_RAW_nop1byte(dc) INSTR_CREATE_RAW_nop(dc)
#ifdef X64
#    define INSTR_CREATE_RAW_nop2byte(dc) instr_create_raw_2bytes(dc, 0x66, 0x90)
#    define INSTR_CREATE_RAW_nop3byte(dc) instr_create_raw_3bytes(dc, 0x48, 0x8d, 0x3f)
#else
#    define INSTR_CREATE_RAW_nop2byte(dc) instr_create_raw_2bytes(dc, 0x8b, 0xff)
#    define INSTR_CREATE_RAW_nop3byte(dc) instr_create_raw_3bytes(dc, 0x8d, 0x7f, 0x00)
#endif
#define INSTR_CREATE_RAW_nopNbyte(dc, n) instr_create_nbyte_nop(dc, n, true)

#endif /* _INSTR_CREATE_H_ */
