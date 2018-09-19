/* **********************************************************
 * Copyright (c) 2018 ARM Limited. All rights reserved.
 * **********************************************************
 *
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
 *
 */

/* A simple AArch64 client replacing 'dst = src0 & src1' with
 * 'dst = !(!src0 | !src1)' to sanity test two core emulation API functions:
 * - drmgr_insert_emulation_start()
 * - drmgr_insert_emulation_end()
 *
 * Note this emulation client is for AArch64 only:
 * XXX i#3173 Improve testing of emulation API functions
 */

#include "dr_api.h"
#include "drmgr.h"
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

/* Events handlers */
static void
event_exit(void);

static dr_emit_flags_t
event_instruction_change(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                         bool translating);

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    if (!drmgr_init())
        DR_ASSERT(false);

    dr_register_exit_event(event_exit);

    if (!drmgr_register_bb_app2app_event(event_instruction_change, NULL))
        DR_ASSERT(false);
}

static void
event_exit(void)
{
    if (!drmgr_unregister_bb_app2app_event(event_instruction_change))
        DR_ASSERT(false);
    drmgr_exit();
}

static dr_emit_flags_t
event_instruction_change(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                         bool translating)
{
    instr_t *instr, *next_instr;

    for (instr = instrlist_first_app(bb); instr != NULL; instr = next_instr) {
        /* We're deleting some instrs, so get the next first. */
        next_instr = instr_get_next_app(instr);

        /* We replace 'dst = src0 & src1' with 'dst = !(!src0 | !src1)'. We only
         * transform AND instructions with unshifted, 64 bit register operands. */
        if (instr_get_opcode(instr) != OP_and)
            continue;
        opnd_t dst = instr_get_dst(instr, 0);
        opnd_t src0 = instr_get_src(instr, 0);
        opnd_t src1 = instr_get_src(instr, 1);

        if (instr_num_srcs(instr) != 4 || opnd_get_size(src0) != OPSZ_8 ||
            opnd_get_immed_int(instr_get_src(instr, 3)) != 0)
            continue;

        app_pc raw_instr_pc = instr_get_app_pc(instr);

        /* Insert emulation label to signal start of emulation code sequence. The
         * label is loaded with data about the instruction being emulated for use by
         * an observational client.
         */
        emulated_instr_t emulated_instr;
        emulated_instr.size = sizeof(emulated_instr);
        emulated_instr.pc = raw_instr_pc;
        emulated_instr.instr = instr;
        drmgr_insert_emulation_start(drcontext, bb, instr, &emulated_instr);

        dr_save_reg(drcontext, bb, instr, DR_REG_X26, SPILL_SLOT_1);
        dr_save_reg(drcontext, bb, instr, DR_REG_X27, SPILL_SLOT_2);

        opnd_t scratch0 = opnd_create_reg(DR_REG_X26);
        opnd_t scratch1 = opnd_create_reg(DR_REG_X27);

        /* scratch0 = !src0
         * XXX i#2440 AArch64 missing INSTR_CREATE macros (INSTR_CREATE_orn)
         */
        instr_t *not0 = instr_create_1dst_4src(drcontext, OP_orn, scratch0,
                                               opnd_create_reg(DR_REG_XZR), src0,
                                               OPND_CREATE_LSL(), OPND_CREATE_INT(0));
        instrlist_preinsert(bb, instr, instr_set_translation(not0, raw_instr_pc));

        /* scratch1 = !src1
         * XXX i#2440 AArch64 missing INSTR_CREATE macros (INSTR_CREATE_orn)
         */
        instr_t *not1 = instr_create_1dst_4src(drcontext, OP_orn, scratch1,
                                               opnd_create_reg(DR_REG_XZR), src1,
                                               OPND_CREATE_LSL(), OPND_CREATE_INT(0));
        instrlist_preinsert(bb, instr, instr_set_translation(not1, raw_instr_pc));

        /* scratch0 = scratch0 | scratch1
         * XXX i#2440 AArch64 missing INSTR_CREATE macros (INSTR_CREATE_orr)
         */
        instr_t *or_i =
            instr_create_1dst_4src(drcontext, OP_orr, scratch0, scratch0, scratch1,
                                   OPND_CREATE_LSL(), OPND_CREATE_INT(0));
        instrlist_preinsert(bb, instr, instr_set_translation(or_i, raw_instr_pc));

        /* dst = !scratch0
         * XXX i#2440 AArch64 missing INSTR_CREATE macros (INSTR_CREATE_orn)
         */
        instr_t *not2 =
            instr_create_1dst_4src(drcontext, OP_orn, dst, opnd_create_reg(DR_REG_XZR),
                                   scratch0, OPND_CREATE_LSL(), OPND_CREATE_INT(0));
        instrlist_preinsert(bb, instr, instr_set_translation(not2, raw_instr_pc));

        dr_restore_reg(drcontext, bb, instr, DR_REG_X26, SPILL_SLOT_1);
        dr_restore_reg(drcontext, bb, instr, DR_REG_X27, SPILL_SLOT_2);

        /* Insert emulation label to signal end of emulation code sequence and
         * remove the instruction being emulated from the basic block.
         */
        drmgr_insert_emulation_end(drcontext, bb, instr);
        instrlist_remove(bb, instr);
    }

    return DR_EMIT_DEFAULT;
}
