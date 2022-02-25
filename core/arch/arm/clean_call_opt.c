/* ******************************************************************************
 * Copyright (c) 2014-2021 Google, Inc.  All rights reserved.
 * ******************************************************************************/

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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL Google, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* file "clean_call_opt.c" */

#include "../globals.h"
#include "arch.h"

void
analyze_callee_regs_usage(dcontext_t *dcontext, callee_info_t *ci)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#2094: NYI on ARM */
}

void
analyze_callee_save_reg(dcontext_t *dcontext, callee_info_t *ci)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#2094: NYI on ARM */
}

void
analyze_callee_tls(dcontext_t *dcontext, callee_info_t *ci)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#2094: NYI on ARM */
}

app_pc
check_callee_instr_level2(dcontext_t *dcontext, callee_info_t *ci, app_pc next_pc,
                          app_pc cur_pc, app_pc tgt_pc)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#2094: NYI on ARM */
    return NULL;
}

bool
check_callee_ilist_inline(dcontext_t *dcontext, callee_info_t *ci)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#2094: NYI on ARM */
    return false;
}

void
analyze_clean_call_aflags(dcontext_t *dcontext, clean_call_info_t *cci, instr_t *where)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#2094: NYI on ARM */
}

void
insert_inline_reg_save(dcontext_t *dcontext, clean_call_info_t *cci, instrlist_t *ilist,
                       instr_t *where, opnd_t *args)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#2094: NYI on ARM */
}

void
insert_inline_reg_restore(dcontext_t *dcontext, clean_call_info_t *cci,
                          instrlist_t *ilist, instr_t *where)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#2094: NYI on ARM */
}

void
insert_inline_arg_setup(dcontext_t *dcontext, clean_call_info_t *cci, instrlist_t *ilist,
                        instr_t *where, opnd_t *args)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#2094: NYI on ARM */
}
