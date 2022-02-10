/* **********************************************************
 * Copyright (c) 2020 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 * Test of instrumentation in load-store-exclusive-monitor regions.
 */

#include "dr_api.h"
#include "client_tools.h"

static int counter;

/* We have two phases: one with clean calls and another without. */
static bool phase_one = true;

static void
in_region(void)
{
    counter++;
}

static dr_emit_flags_t
bb_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr;
    instr_t *insert_at = NULL;
    int nop_count = 0;
    for (instr = instrlist_first(bb); instr != NULL; instr = instr_get_next(instr)) {
        if (phase_one && instr_is_nop(instr)) {
            ++nop_count;
            if (nop_count == 4) {
                phase_one = false;
                /* We want to re-instrument all the blocks.  We expect new code
                 * before we re-execute the old blocks so a delayed flush is sufficient.
                 */
                dr_delay_flush_region(NULL, ~0UL, 0, NULL);
            }
        } else
            nop_count = 0;
        if (instr_is_exclusive_load(instr)) {
            insert_at = instr_get_next(instr);
#ifdef ARM
            /* TODO i#1698: DR does not yet convert 32-bit pairs. */
            if (instr_get_opcode(instr) == OP_ldaexd ||
                instr_get_opcode(instr) == OP_ldrexd)
                insert_at = NULL;
#endif
            break;
        } else if (instr_is_exclusive_store(instr)) {
            insert_at = instr;
#ifdef ARM
            /* TODO i#1698: DR does not yet convert 32-bit pairs. */
            if (instr_get_opcode(instr) == OP_stlexd ||
                instr_get_opcode(instr) == OP_strexd)
                insert_at = NULL;
#endif
            break;
        }
    }
    if (insert_at != NULL && phase_one) {
        /* Insert enough memory refs in exclusive monitor regions to cause
         * monitor failure every single time.
         * However, this often thwarts single-block optimizations for ldstex2cas,
         * so we have a 2nd phase with no clean calls.
         */
        dr_insert_clean_call(drcontext, bb, insert_at, (void *)in_region,
                             false /*fpstate */, 0);
    }
    return DR_EMIT_DEFAULT;
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    if (dr_get_stolen_reg() != IF_ARM_ELSE(DR_REG_R10, DR_REG_X28)) {
        /* Our test assembly code has these stolen register values hardcoded. */
        dr_fprintf(STDERR,
                   "Default stolen register changed: this test needs to be updated!\n");
    }
    dr_register_bb_event(bb_event);
}
