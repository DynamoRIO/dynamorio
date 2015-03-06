/* **********************************************************
 * Copyright (c) 2014-2015 Google, Inc.  All rights reserved.
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

/* Test that the annotation inclusion policy  */
dr_emit_flags_t
bb_event_truncate(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating)
{
    bool found_annotation = false;
    instr_t *instr = instrlist_first(bb), *annotation_label = NULL;

    while (instr != NULL) {
        if ((ptr_uint_t) instr_get_note(instr) == DR_NOTE_ANNOTATION) {
            annotation_label = instr;
            instr = instr_get_next(instr);
            break;
        }
        instr = instr_get_next(instr);
    }

    if (annotation_label != NULL) {
        instr_t *next;
        while (instr != NULL) {
            next = instr_get_next(instr);
            if (!instr_is_meta(instr)) {
                if (instr_is_cbr(instr)) {
                    ASSERT(instrlist_last(bb) == instr);
                    instrlist_set_fall_through_target(bb, instr_get_branch_target_pc(instr));
                    break;
                }
                instrlist_remove(bb, instr);
                instr_destroy(drcontext, instr);
            }
            instr = next;
        }

        /* put the annotation label at the end of the bb to invoke the inclusion policy */
        instrlist_remove(bb, annotation_label);
        instrlist_append(bb, annotation_label);
    }

    return DR_EMIT_DEFAULT;
}

DR_EXPORT
void dr_init(client_id_t id)
{
    dr_fprintf(STDERR, "vg-annot-retarget placeholder\n");

    dr_register_bb_event(bb_event_truncate);
}
