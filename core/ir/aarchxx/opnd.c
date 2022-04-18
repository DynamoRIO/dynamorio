/* **********************************************************
 * Copyright (c) 2014-2015 Google, Inc.  All rights reserved.
 * Copyright (c) 2016 ARM Limited. All rights reserved.
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

#include "../globals.h"
#include "instr.h"
#include "arch.h"

reg_id_t dr_reg_stolen = DR_REG_NULL;

uint
opnd_immed_float_arch(uint opcode)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1551, i#1569 */
    return 0;
}

DR_API
bool
reg_is_stolen(reg_id_t reg)
{
    if (dr_reg_fixer[reg] == dr_reg_stolen && dr_reg_fixer[reg] != DR_REG_NULL)
        return true;
    return false;
}

int
opnd_get_reg_dcontext_offs(reg_id_t reg)
{
#ifdef AARCH64
    if (DR_REG_X0 <= reg && reg <= DR_REG_X30)
        return R0_OFFSET + (R1_OFFSET - R0_OFFSET) * (reg - DR_REG_X0);
    if (DR_REG_W0 <= reg && reg <= DR_REG_W30)
        return R0_OFFSET + (R1_OFFSET - R0_OFFSET) * (reg - DR_REG_W0);
    if (reg == DR_REG_XSP || reg == DR_REG_WSP)
        return XSP_OFFSET;
    CLIENT_ASSERT(false, "opnd_get_reg_dcontext_offs: invalid reg");
    return -1;
#else
    switch (reg) {
    case DR_REG_R0: return R0_OFFSET;
    case DR_REG_R1: return R1_OFFSET;
    case DR_REG_R2: return R2_OFFSET;
    case DR_REG_R3: return R3_OFFSET;
    case DR_REG_R4: return R4_OFFSET;
    case DR_REG_R5: return R5_OFFSET;
    case DR_REG_R6: return R6_OFFSET;
    case DR_REG_R7: return R7_OFFSET;
    case DR_REG_R8: return R8_OFFSET;
    case DR_REG_R9: return R9_OFFSET;
    case DR_REG_R10: return R10_OFFSET;
    case DR_REG_R11: return R11_OFFSET;
    case DR_REG_R12: return R12_OFFSET;
    case DR_REG_R13: return R13_OFFSET;
    case DR_REG_R14: return R14_OFFSET;
    case DR_REG_R15: return PC_OFFSET;
    default: CLIENT_ASSERT(false, "opnd_get_reg_dcontext_offs: invalid reg"); return -1;
    }
#endif
}

#ifndef STANDALONE_DECODER

opnd_t
opnd_create_sized_tls_slot(int offs, opnd_size_t size)
{
    return opnd_create_base_disp(dr_reg_stolen, REG_NULL, 0, offs, size);
}

#endif /* !STANDALONE_DECODER */
