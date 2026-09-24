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

static app_pc test_store_pc;
static bool found_test_store;

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      bool for_trace, bool translating, void *user_data)
{
    if (!instr_is_app(instr) || instr_get_app_pc(instr) != test_store_pc)
        return DR_EMIT_DEFAULT;

    CHECK(instr_get_opcode(instr) == OP_str, "test store has unexpected opcode");

    /* Spill and clobber the add src register before the store instruction so that it
     * does not have its app value when the store faults.
     */

    instr_t *add_instr = instr_get_next_app(instr);
    CHECK(instr_get_opcode(add_instr) == OP_add,
          "test store is not follwed by the expected add.");
    CHECK(instr_num_srcs(add_instr) >= 1, "add instr has wrong number of srcs.");
    opnd_t add_src = instr_get_src(add_instr, 0);
    CHECK(opnd_is_reg(add_src), "add src 0 is not a register.");
    reg_id_t clobber_reg = opnd_get_reg(add_src);

    drvector_t allowed;
    reg_id_t reg = DR_REG_NULL;
    drreg_init_and_fill_vector(&allowed, false);
    drreg_set_vector_entry(&allowed, clobber_reg, true);
    if (drreg_reserve_register(drcontext, bb, instr, &allowed, &reg) != DRREG_SUCCESS) {
        CHECK(false, "failed to reserve clobber register");
    }
    ASSERT(reg == clobber_reg);
    drvector_delete(&allowed);

    /* Insert a meta instruction that clobbers the add src register. */
    static const uint poison_value = 0x55;
    instrlist_meta_preinsert(bb, instr,
                             XINST_CREATE_load_int(drcontext,
                                                   opnd_create_reg(clobber_reg),
                                                   OPND_CREATE_INT16(poison_value)));

    /* Now restore the app value after the store. */
    if (drreg_unreserve_register(drcontext, bb, instr_get_next_app(instr), reg) !=
        DRREG_SUCCESS) {
        CHECK(false, "failed to unreserve register");
    }

    found_test_store = true;
    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
    CHECK(found_test_store, "failed to find test store");
    if (!drmgr_unregister_bb_insertion_event(event_app_instruction) ||
        drreg_exit() != DRREG_SUCCESS) {
        CHECK(false, "exit failed");
    }
    drmgr_exit();
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    drreg_options_t ops = { sizeof(ops), 1 /*max slots needed*/, false };
    module_data_t *module = dr_get_main_module();
    CHECK(module != NULL, "failed to find main module");
    test_store_pc = (app_pc)dr_get_proc_address(module->handle, "codemod_state_writer");
    CHECK(test_store_pc != NULL, "failed to find codemod_state_writer");
    dr_free_module_data(module);

    if (!drmgr_init()) {
        CHECK(false, "drmgr init failed");
    }
    if (drreg_init(&ops) != DRREG_SUCCESS) {
        CHECK(false, "drreg init failed");
    }
    drmgr_register_exit_event(event_exit);
    if (!drmgr_register_bb_instrumentation_event(NULL, event_app_instruction, NULL)) {
        CHECK(false, "bb registration failed");
    }
}
