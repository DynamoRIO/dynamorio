/* ****************************************************
 * Copyright (c) 2026 Arm Limited  All rights reserved.
 * ***************************************************/

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
 * * Neither the name of Arm Limited nor the names of its contributors may be
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

#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "client_tools.h"

#include "selfmod-state-shared.h"

static reg_id_t reg = DR_REG_NULL;
static bool found_test_store;

static bool
is_test_add(instr_t *instr)
{
    if (instr == NULL || instr_get_opcode(instr) != OP_add ||
        instr_num_dsts(instr) != 1 || instr_num_srcs(instr) != 4)
        return false;

    const opnd_t dst = instr_get_dst(instr, 0);
    const opnd_t src0 = instr_get_src(instr, 0);
    const opnd_t src1 = instr_get_src(instr, 1);
    const opnd_t src2 = instr_get_src(instr, 2);
    const opnd_t src3 = instr_get_src(instr, 3);

    return opnd_is_reg(dst) && opnd_get_reg(dst) == ADD_DST_DR_REG && opnd_is_reg(src0) &&
        opnd_get_reg(src0) == ADD_SRC_DR_REG && opnd_is_immed_int(src1) &&
        opnd_get_immed_int(src1) == 7 && opnd_is_immed_int(src2) &&
        opnd_get_immed_int(src2) == DR_SHIFT_LSL && opnd_is_immed_int(src3) &&
        opnd_get_immed_int(src3) == 0;
}

static bool
is_test_store(instr_t *instr)
{
    if (instr == NULL || instr_get_opcode(instr) != OP_str ||
        instr_num_dsts(instr) != 1 || instr_num_srcs(instr) != 1)
        return false;

    const opnd_t dst = instr_get_dst(instr, 0);
    const opnd_t src = instr_get_src(instr, 0);
    return opnd_is_base_disp(dst) && opnd_get_base(dst) == STR_ADDR_DR_REG &&
        opnd_get_index(dst) == DR_REG_NULL && opnd_get_disp(dst) == 0 &&
        opnd_is_reg(src) && opnd_get_reg(src) == STR_SRC_DR_REG &&
        is_test_add(instr_get_next_app(instr));
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      bool for_trace, bool translating, void *user_data)
{
    if (reg != DR_REG_NULL && instr_is_app(instr)) {
        if (drreg_unreserve_register(drcontext, bb, instr, reg) != DRREG_SUCCESS)
            CHECK(false, "failed to unreserve register");
        reg = DR_REG_NULL;
    }

    if (!instr_is_app(instr) || !is_test_store(instr))
        return DR_EMIT_DEFAULT;

    drvector_t allowed;
    drreg_init_and_fill_vector(&allowed, false);
    drreg_set_vector_entry(&allowed, ADD_SRC_DR_REG, true);
    if (drreg_reserve_register(drcontext, bb, instr, &allowed, &reg) != DRREG_SUCCESS)
        CHECK(false, "failed to reserve clobber register");
    drvector_delete(&allowed);

    /* Insert a meta instruction that clobbers the add src register. */
    instrlist_meta_preinsert(bb, instr,
                             XINST_CREATE_load_int(drcontext,
                                                   opnd_create_reg(ADD_SRC_DR_REG),
                                                   OPND_CREATE_INT16(0x55)));
    found_test_store = true;
    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
    CHECK(found_test_store, "failed to find test store");
    if (!drmgr_unregister_bb_insertion_event(event_app_instruction) ||
        drreg_exit() != DRREG_SUCCESS)
        CHECK(false, "exit failed");
    drmgr_exit();
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    drreg_options_t ops = { sizeof(ops), 1 /*max slots needed*/, false };
    if (!drmgr_init())
        CHECK(false, "drmgr init failed");
    if (drreg_init(&ops) != DRREG_SUCCESS)
        CHECK(false, "drreg init failed");
    drmgr_register_exit_event(event_exit);
    if (!drmgr_register_bb_instrumentation_event(NULL, event_app_instruction, NULL))
        CHECK(false, "bb registration failed");
}
