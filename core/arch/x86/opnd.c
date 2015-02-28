/* **********************************************************
 * Copyright (c) 2011-2015 Google, Inc.  All rights reserved.
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
#include "arch.h"

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

int
opnd_get_reg_dcontext_offs(reg_id_t reg)
{
    uint xmm_index;
    switch (reg) {
#ifdef X86
    case REG_XAX: return XAX_OFFSET;
    case REG_XBX: return XBX_OFFSET;
    case REG_XCX: return XCX_OFFSET;
    case REG_XDX: return XDX_OFFSET;
    case REG_XSP: return XSP_OFFSET;
    case REG_XBP: return XBP_OFFSET;
    case REG_XSI: return XSI_OFFSET;
    case REG_XDI: return XDI_OFFSET;
# ifdef X64
    case REG_EAX: return XAX_OFFSET;
    case REG_EBX: return XBX_OFFSET;
    case REG_ECX: return XCX_OFFSET;
    case REG_EDX: return XDX_OFFSET;
    case REG_ESP: return XSP_OFFSET;
    case REG_EBP: return XBP_OFFSET;
    case REG_ESI: return XSI_OFFSET;
    case REG_EDI: return XDI_OFFSET;
    case REG_R8D: return R8_OFFSET;
    case REG_R9D: return R9_OFFSET;
    case REG_R10D: return R10_OFFSET;
    case REG_R11D: return R11_OFFSET;
    case REG_R12D: return R12_OFFSET;
    case REG_R13D: return R13_OFFSET;
    case REG_R14D: return R14_OFFSET;
    case REG_R15D: return R15_OFFSET;

    case REG_AX: return XAX_OFFSET;
    case REG_BX: return XBX_OFFSET;
    case REG_CX: return XCX_OFFSET;
    case REG_DX: return XDX_OFFSET;
    case REG_SP: return XSP_OFFSET;
    case REG_BP: return XBP_OFFSET;
    case REG_SI: return XSI_OFFSET;
    case REG_DI: return XDI_OFFSET;

    case REG_AL: return XAX_OFFSET;
    case REG_BL: return XBX_OFFSET;
    case REG_CL: return XCX_OFFSET;
    case REG_DL: return XDX_OFFSET;
    case REG_SPL: return XSP_OFFSET;
    case REG_BPL: return XBP_OFFSET;
    case REG_SIL: return XSI_OFFSET;
    case REG_DIL: return XDI_OFFSET;
    case REG_R8L: return R8_OFFSET;
    case REG_R9L: return R9_OFFSET;
    case REG_R10L: return R10_OFFSET;
    case REG_R11L: return R11_OFFSET;
    case REG_R12L: return R12_OFFSET;
    case REG_R13L: return R13_OFFSET;
    case REG_R14L: return R14_OFFSET;
    case REG_R15L: return R15_OFFSET;

    case REG_AH: return XAX_OFFSET;
    case REG_BH: return XBX_OFFSET;
    case REG_CH: return XCX_OFFSET;
    case REG_DH: return XDX_OFFSET;

    case REG_R8: return  R8_OFFSET;
    case REG_R9: return  R9_OFFSET;
    case REG_R10: return R10_OFFSET;
    case REG_R11: return R11_OFFSET;
    case REG_R12: return R12_OFFSET;
    case REG_R13: return R13_OFFSET;
    case REG_R14: return R14_OFFSET;
    case REG_R15: return R15_OFFSET;
# endif
    case DR_REG_XMM0:
    case DR_REG_XMM1:
    case DR_REG_XMM2:
    case DR_REG_XMM3:
    case DR_REG_XMM4:
    case DR_REG_XMM5:
    case DR_REG_XMM6:
    case DR_REG_XMM7:
    case DR_REG_XMM8:
    case DR_REG_XMM9:
    case DR_REG_XMM10:
    case DR_REG_XMM11:
    case DR_REG_XMM12:
    case DR_REG_XMM13:
    case DR_REG_XMM14:
    case DR_REG_XMM15:
        xmm_index = (reg - DR_REG_XMM0);
        return XMM_OFFSET + (0x20 * xmm_index);
#endif
    default: CLIENT_ASSERT(false, "opnd_get_reg_dcontext_offs: invalid reg");
        return -1;
    }
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
