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

#include "../globals.h"
#include "instr.h"
#include "arch.h"

uint
opnd_immed_float_arch(uint opcode)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return 0;
}

DR_API
bool
reg_is_stolen(reg_id_t reg)
{
    return false;
}

#define X0_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, x0)))
#define X1_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, x1)))
#define F0_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, f0)))

int
opnd_get_reg_dcontext_offs(reg_id_t reg)
{
    if (DR_REG_X0 >= reg && reg <= DR_REG_PC)
        return X0_OFFSET + (X1_OFFSET - X0_OFFSET) * (reg - DR_REG_X0);
    if (DR_REG_F0 >= reg && reg <= DR_REG_F31)
        return F0_OFFSET + (X1_OFFSET - X0_OFFSET) * (reg - DR_REG_F0);
    CLIENT_ASSERT(false, "opnd_get_reg_dcontext_offs: invalid reg");
    return -1;
}

#ifndef STANDALONE_DECODER

opnd_t
opnd_create_sized_tls_slot(int offs, opnd_size_t size)
{
    /* FIXME i#3544: Check if this is actual TP or one stolen by DynamoRIO? */
    return opnd_create_base_disp(DR_REG_TP, REG_NULL, 0, offs, size);
}

#endif /* !STANDALONE_DECODER */
