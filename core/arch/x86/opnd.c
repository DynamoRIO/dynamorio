/* **********************************************************
 * Copyright (c) 2011-2014 Google, Inc.  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
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
/* Copyright (c) 2000-2001 Hewlett-Packard Company */

/* file "opnd_shared.c" -- IR opnd utilities */

#include "../globals.h"
#include "instr.h"

enum {
    FLOAT_ZERO    = 0x00000000,
    FLOAT_ONE     = 0x3f800000,
    FLOAT_LOG2_10 = 0x40549a78,
    FLOAT_LOG2_E  = 0x3fb8aa3b,
    FLOAT_PI      = 0x40490fdb,
    FLOAT_LOG10_2 = 0x3e9a209a,
    FLOAT_LOGE_2  = 0x3f317218,
};

uint
opnd_immed_float_arch(uint opcode)
{
    switch (opcode) {
    case OP_fldz:    return FLOAT_ZERO;
    case OP_fld1:    return FLOAT_ONE;
    case OP_fldl2t:  return FLOAT_LOG2_10;
    case OP_fldl2e:  return FLOAT_LOG2_E;
    case OP_fldpi:   return FLOAT_PI;
    case OP_fldlg2:  return FLOAT_LOG10_2;
    case OP_fldln2:  return FLOAT_LOGE_2;
    case OP_ftst:    return FLOAT_ZERO;
    default:
       CLIENT_ASSERT(false, "invalid float opc");
       return FLOAT_ZERO;
    }
}

DR_API
bool
reg_is_stolen(reg_id_t reg)
{
    return false;
}


#ifndef STANDALONE_DECODER
/****************************************************************************/

opnd_t
opnd_create_sized_tls_slot(int offs, opnd_size_t size)
{
    /* We do not request disp_short_addr or force_full_disp, letting
     * encode_base_disp() choose whether to use the 0x67 addr prefix
     * (assuming offs is small).
     */
    return opnd_create_far_base_disp(SEG_TLS, REG_NULL, REG_NULL, 0, offs, size);
}

#endif /* !STANDALONE_DECODER */
/****************************************************************************/
