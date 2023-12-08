/* **********************************************************
 * Copyright (c) 2013-2023 Google, Inc.   All rights reserved.
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

/* DynamoRio eXtension utilities */

#include "dr_api.h"
#include "drmgr.h"
#include "drx.h"

#include "scatter_gather_shared.h"

int drx_scatter_gather_tls_idx;

/*
 * Split a basic block at the first scatter/gather app instruction found.
 *
 * If the first app instruction in bb is a scatter/gather instruction, all following
 * instructions will be removed so that bb just contains the scatter/gather instruction.
 *
 * If the first app instruction in bb is not a scatter/gather instruction, all
 * instructions up until the first scatter/gather instruction will be left. The
 * scatter/gather instruction and any following instructions will be removed from bb and
 * the function will return false.
 *
 * If there are no scatter/gather instructions in bb, it will be unchanged and the
 * function will return false.
 */
bool
scatter_gather_split_bb(void *drcontext, instrlist_t *bb, DR_PARAM_OUT instr_t **sg_instr)
{
    instr_t *instr, *next_instr, *first_app = NULL;
    bool delete_rest = false;
    bool first_app_is_scatter_gather = false;

    for (instr = instrlist_first(bb); instr != NULL; instr = next_instr) {
        next_instr = instr_get_next(instr);
        if (delete_rest) {
            instrlist_remove(bb, instr);
            instr_destroy(drcontext, instr);
        } else if (instr_is_app(instr)) {
            if (first_app == NULL)
                first_app = instr;
            if (instr_is_gather(instr) || instr_is_scatter(instr)) {
                delete_rest = true;
                if (instr == first_app) {
                    first_app_is_scatter_gather = true;
                } else {
                    instrlist_remove(bb, instr);
                    instr_destroy(drcontext, instr);
                }
            }
        }
    }

    if (first_app_is_scatter_gather && (sg_instr != NULL)) {
        *sg_instr = first_app;
    }

    return first_app_is_scatter_gather;
}

/* These architecture specific functions are defined in scatter_gather_${ARCH_NAME}.c */
void
drx_scatter_gather_thread_exit(void *drcontext);

void
drx_scatter_gather_thread_exit(void *drcontext);

bool
drx_scatter_gather_restore_state(void *drcontext, dr_restore_state_info_t *info,
                                 instr_t *sg_inst);

int drx_scatter_gather_expanded = 0;

void
drx_mark_scatter_gather_expanded(void)
{
    dr_atomic_store32(&drx_scatter_gather_expanded, 1);
}

static bool
drx_event_restore_state(void *drcontext, bool restore_memory,
                        dr_restore_state_info_t *info)
{
    instr_t inst;
    bool success = true;
    if (info->fragment_info.cache_start_pc == NULL)
        return true; /* fault not in cache */
    if (dr_atomic_load32(&drx_scatter_gather_expanded) == 0) {
        /* Nothing to do if nobody had never called expand_scatter_gather() before. */
        return true;
    }
    if (!info->fragment_info.app_code_consistent) {
        /* Can't verify application code.
         * XXX i#2985: is it better to keep searching?
         */
        return true;
    }
    instr_init(drcontext, &inst);
    byte *pc = decode(drcontext, dr_fragment_app_pc(info->fragment_info.tag), &inst);
    if (pc != NULL) {
        if (instr_is_gather(&inst) || instr_is_scatter(&inst)) {
            success = success && drx_scatter_gather_restore_state(drcontext, info, &inst);
        }
    }
    instr_free(drcontext, &inst);
    return success;
}

/* Reserved note range values */
enum {
    SG_NOTE_EXPANDED_LD_ST,
    SG_NOTE_COUNT,
};

static ptr_uint_t note_base;
#define NOTE_VAL(enum_val) ((void *)(ptr_int_t)(note_base + (enum_val)))

bool
drx_scatter_gather_init()
{
    drmgr_priority_t fault_priority = { sizeof(fault_priority),
                                        DRMGR_PRIORITY_NAME_DRX_FAULT, NULL, NULL,
                                        DRMGR_PRIORITY_FAULT_DRX };

    if (!drmgr_register_restore_state_ex_event_ex(drx_event_restore_state,
                                                  &fault_priority))
        return false;

    drx_scatter_gather_tls_idx = drmgr_register_tls_field();
    if (drx_scatter_gather_tls_idx == -1)
        return false;

    if (!drmgr_register_thread_init_event(drx_scatter_gather_thread_init) ||
        !drmgr_register_thread_exit_event(drx_scatter_gather_thread_exit))
        return false;

    note_base = drmgr_reserve_note_range(SG_NOTE_COUNT);
    DR_ASSERT_MSG(note_base != DRMGR_NOTE_NONE, "failed to reserve note range");
    if (note_base == DRMGR_NOTE_NONE)
        return false;

    return true;
}

void
drx_scatter_gather_exit()
{
    drmgr_unregister_tls_field(drx_scatter_gather_tls_idx);
}

bool
scatter_gather_is_expanded_ld_st(instr_t *instr)
{
    return instr_get_note(instr) == NOTE_VAL(SG_NOTE_EXPANDED_LD_ST);
}

void
scatter_gather_tag_expanded_ld_st(instr_t *instr)
{
    instr_set_note(instr, NOTE_VAL(SG_NOTE_EXPANDED_LD_ST));
}
