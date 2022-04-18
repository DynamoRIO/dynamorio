/* **********************************************************
 * Copyright (c) 2011-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2001-2008 VMware, Inc.  All rights reserved.
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
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2001 Hewlett-Packard Company */

/* file "disassemble.h" */

#ifndef DISASSEMBLE_H
#define DISASSEMBLE_H

#include "../globals.h"
#include "disassemble_api.h"

/* for printing to buffers */
#define MAX_OPND_DIS_SZ 64 /* gets long w/ ibl target names */
/* Long examples:
 * "<RAW>  <raw 0x00007f85922c0877-0x00007f85922c0882 == 48 63 f8 48 89 d6 b8 05 00
 * ...>" "lock cmpxchg %rcx <rel> 0x000007fefd1a2728[8byte] %rax -> <rel> "
 * "0x000007fefd1a2728[8byte] %rax "
 */
#define MAX_INSTR_DIS_SZ 196
/* Here's a pretty long one,
 * "  0x00007f859277d63a  48 83 05 4e 63 21 00 add    $0x0000000000000001 <rel> "
 * "0x00007f8592993990 -> <rel> 0x00007f8592993990 \n                     01 "
 * For ARM:
 * " 8ca90aa1   vstm.hi %s0 %s1 %s2 %s3 %s4 %s5 %s6 %s7 %s8 %s9 %s10 %s11 %s12 %s13
 * %s14 "
 * "%s15 %s16 %s17 %s18 %s19 %s20 %s21 %s22 %s23 %s24 %s25 %s26 %s27 %s28 %s29 %s30
 * %s31 "
 * "%r9 -> (%r9)[124byte]"
 */
#define MAX_PC_DIS_SZ 228

void
disassemble_options_init(void);

DR_UNS_API /* deprecated from interface */
    /**
     * Decodes and then prints the instruction at address \p pc to file \p outfile.
     * Prior to the instruction the address and raw bytes of the instruction
     * are printed.
     * The default is to use DR's custom syntax (see disassemble_set_syntax()).
     * Returns the address of the subsequent instruction, or a guess if the instruction
     * at \p pc is invalid.
     */
    byte *
    disassemble_with_bytes(dcontext_t *dcontext, byte *pc, file_t outfile);

#endif /* DISASSEMBLE_H */
