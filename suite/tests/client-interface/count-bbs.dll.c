/* ******************************************************************************
 * Copyright (c) 2010 Massachusetts Institute of Technology  All rights reserved.
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

#include "dr_api.h"

static uint64 bbcnt0 = 0;
static uint64 bbcnt0_fp = 0;
static uint64 bbcnt1 = 0;
static uint64 bbcnt4 = 0;

static void
bbcount4(reg_t r1, reg_t r2, reg_t r3, reg_t r4)
{
    bbcnt4++;
}

static void
bbcount1(reg_t r1)
{
    bbcnt1++;
}

static void
bbcount0_fp()
{
    bbcnt0_fp++;
}

static void
bbcount0()
{
    bbcnt0++;
}

static dr_emit_flags_t
bb_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr;

    instr = instrlist_first(bb);

    dr_insert_clean_call(drcontext, bb, instr, bbcount0, false, 0);
    dr_insert_clean_call(drcontext, bb, instr, bbcount0_fp, true, 0);
    dr_insert_clean_call(drcontext, bb, instr, bbcount1, false, 1,
                         opnd_create_reg(REG_XAX));
    dr_insert_clean_call(drcontext, bb, instr, bbcount4, false, 4,
                         opnd_create_reg(REG_XAX), opnd_create_reg(REG_XBX),
                         opnd_create_reg(REG_XCX), opnd_create_reg(REG_XDX));
    return DR_EMIT_DEFAULT;
}

static void
check(uint64 count, const char *str)
{
    dr_fprintf(STDERR, "%s... ", str);
    if (bbcnt0 == count) {
        dr_fprintf(STDERR, "yes\n");
    } else {
        dr_fprintf(STDERR, "no\n");
    }
}

void
exit_event(void)
{
    check(bbcnt0_fp, "bbcount0_fp");
    check(bbcnt1, "bbcount1");
    check(bbcnt4, "bbcount4");
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    dr_register_bb_event(bb_event);
    dr_register_exit_event(exit_event);
}
