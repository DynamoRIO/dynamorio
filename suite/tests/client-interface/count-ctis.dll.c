/* **********************************************************
 * Copyright (c) 2007-2010 VMware, Inc.  All rights reserved.
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

#include "dr_api.h"
#include "client_tools.h"

#define MINSERT instrlist_meta_preinsert

uint num_call_dir = 0;
uint num_call_ind = 0;
uint num_jump_dir = 0;
uint num_jump_ind = 0;
uint num_br_cond = 0;
uint num_ret = 0;

static void
at_call_dir(app_pc src, app_pc dst)
{
    num_call_dir++;
}

static void
at_call_ind(app_pc src, app_pc dst)
{
    num_call_ind++;
}

static void
at_jump_dir(app_pc src, app_pc dst)
{
    num_jump_dir++;
}

static void
at_jump_ind(app_pc src, app_pc dst)
{
    num_jump_ind++;
}

static void
at_br_cond(app_pc src, app_pc dst, int taken)
{
    num_br_cond++;
}

static void
at_br_cond_ex(app_pc inst_addr, app_pc targ_addr, app_pc fall_addr, int taken)
{
    void *drcontext = dr_get_current_drcontext();
    if (fall_addr != decode_next_pc(drcontext, inst_addr)) {
        dr_fprintf(STDERR, "ERROR: wrong fall-through addr: " PFX " vs " PFX "\n",
                   fall_addr, decode_next_pc(drcontext, inst_addr));
    }
}

static void
at_ret(app_pc src, app_pc dst)
{
    num_ret++;
}

static dr_emit_flags_t
bb_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr, *next_instr;

    for (instr = instrlist_first(bb); instr != NULL; instr = next_instr) {
        next_instr = instr_get_next(instr);

        if (instr_is_cti(instr)) {

            if (instr_is_ubr(instr)) {
                dr_insert_ubr_instrumentation(drcontext, bb, instr, at_jump_dir);
            }

            else if (instr_is_call_direct(instr)) {
                dr_insert_call_instrumentation(drcontext, bb, instr, at_call_dir);
            }

            else if (instr_is_call_indirect(instr)) {
                dr_insert_mbr_instrumentation(drcontext, bb, instr, at_call_ind,
                                              SPILL_SLOT_1);
            }

            else if (instr_is_return(instr)) {
                dr_insert_mbr_instrumentation(drcontext, bb, instr, at_ret, SPILL_SLOT_1);
            }

            else if (instr_is_mbr(instr)) {
                dr_insert_mbr_instrumentation(drcontext, bb, instr, at_jump_ind,
                                              SPILL_SLOT_1);
            }

            else if (instr_is_cbr(instr)) {
                dr_insert_cbr_instrumentation(drcontext, bb, instr, at_br_cond);
                dr_insert_cbr_instrumentation_ex(drcontext, bb, instr, at_br_cond_ex,
                                                 opnd_create_null());
            }

            else {
                ASSERT(false);
            }
        }
    }
    return DR_EMIT_DEFAULT;
}

static void
check(uint count, const char *str)
{
    dr_fprintf(STDERR, "%s... ", str);
    /* We assume every types of cti are executed at least twice. */
    if (count > 1) {
        dr_fprintf(STDERR, "yes\n");
    } else {
        dr_fprintf(STDERR, "no\n");
    }
}

void
exit_event(void)
{
    check(num_call_dir, "direct calls");
    check(num_call_ind, "indirect calls");
    check(num_jump_dir, "direct jumps");
    check(num_jump_ind, "indirect jumps");
    check(num_br_cond, "conditional branches");
    check(num_ret, "returns");
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    dr_register_bb_event(bb_event);
    dr_register_exit_event(exit_event);
}
