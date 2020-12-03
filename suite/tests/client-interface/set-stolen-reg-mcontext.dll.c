/* **********************************************************
 * Copyright (c) 2020 Google, Inc.  All rights reserved.
 * Copyright (c) 2020 Arm Limited   All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "dr_api.h"

// Random value to detect later.
static ptr_int_t test_value = 7;

static void
check_stolen_reg_restore()
{
    fprintf(stderr, "check_stolen_reg_restore entered\n");

    void *drcontext = dr_get_current_drcontext();

    fprintf(stderr, "test value = %ld\n", test_value);

    dr_mcontext_t mc;
    mc.size = sizeof(mc);
    mc.flags = DR_MC_ALL;

    mc.r28 = 0;

    fprintf(stderr, "fetching TLS\n");

    bool ok = dr_get_mcontext(drcontext, &mc);

    fprintf(stderr, "mc->stolen_reg after = %ld\n", mc.r28);

    fprintf(stderr, "check_stolen_reg_restore returning\n");
}

static void
check_stolen_reg_spill()
{
    fprintf(stderr, "check_stolen_reg_spill entered\n");

    void *drcontext = dr_get_current_drcontext();

    fprintf(stderr, "test value = %ld\n", test_value);

    fprintf(stderr, "setting TLS\n");

    dr_mcontext_t mc;
    mc.size = sizeof(mc);
    mc.flags = DR_MC_ALL;

    mc.r28 = test_value;

    dr_set_mcontext(drcontext, &mc);

    fprintf(stderr, "check_stolen_reg_spill returning\n");
}

static dr_emit_flags_t
bb_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr, *next_instr;

    for (instr = instrlist_first(bb); instr != NULL; instr = next_instr) {
        next_instr = instr_get_next(instr);

        reg_t imm1 = 0;
        if (instr_is_mov_constant(instr, &imm1) && opnd_is_reg(instr_get_dst(instr, 0)) &&
            opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_R27 && imm1 == 1) {

            dr_insert_clean_call(drcontext, bb, instr, (void *)check_stolen_reg_spill,
                                 false /*fpstate */, 0);

            dr_insert_clean_call(drcontext, bb, instr, (void *)check_stolen_reg_restore,
                                 false /*fpstate */, 0);
        }
    }

    return DR_EMIT_DEFAULT;
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    dr_register_bb_event(bb_event);
}