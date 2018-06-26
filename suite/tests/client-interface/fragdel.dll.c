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

static void *mutex = NULL;
static int deletions = 0;

app_pc start = NULL;
app_pc end = NULL;

static bool
my_strcmp(const char *in1, const char *in2)
{
    if (in1 == NULL || in2 == NULL)
        return false;

    while (*in1 == *in2) {
        if (*in1 == '\0')
            return true;
        in1++;
        in2++;
    }
    return false;
}

static void
delete_fragment(void *drcontext, app_pc tag)
{
    dr_mutex_lock(mutex);
    deletions++;
    dr_mutex_unlock(mutex);

    dr_delete_fragment(drcontext, tag);
}

#define MINSERT instrlist_meta_preinsert

static dr_emit_flags_t
bb_event(void *drcontext, app_pc tag, instrlist_t *bb, bool for_trace, bool translating)
{
    if (tag >= start && tag < end) {
        instr_t *instr = instrlist_first(bb);

        dr_prepare_for_call(drcontext, bb, instr);

        MINSERT(bb, instr,
                INSTR_CREATE_push_imm(drcontext, OPND_CREATE_INT32((ptr_uint_t)tag)));
        MINSERT(
            bb, instr,
            INSTR_CREATE_push_imm(drcontext, OPND_CREATE_INT32((ptr_uint_t)drcontext)));
        MINSERT(bb, instr,
                INSTR_CREATE_call(drcontext, opnd_create_pc((void *)delete_fragment)));

        dr_cleanup_after_call(drcontext, bb, instr, 8);
    }
    return DR_EMIT_DEFAULT;
}

static void
exit_event(void)
{
    dr_mutex_exit(mutex);
    if (deletions > 10000)
        dr_fprintf(STDERR, "deleted > 10k fragments\n");
    else
        dr_fprintf(STDERR, "deleted %d fragments\n", deletions);
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    module_iterator_t *iter;

    dr_register_bb_event(bb_event);
    dr_register_exit_event(exit_event);

    mutex = dr_mutex_init();

    iter = module_iterator_start();
    while (module_iterator_hasnext(iter)) {
        module_data_t *data = module_iterator_next(iter);
        if (my_strcmp(module_name(data), "fragdel.exe")) {
            start = data->start;
            end = data->end;
            break;
        }
    }
    module_iterator_stop(iter);
}
