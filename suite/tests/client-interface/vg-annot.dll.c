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
#include <string.h>

typedef struct _test_stats_t {
    uint num_bytes_made_defined;
    uint num_define_memory_requests;
    uint num_bbs_truncated;
    uint num_instructions_truncated;
} test_stats_t;

static test_stats_t test_stats;

#ifdef WINDOWS
static app_pc *skip_truncation = NULL;
#endif

static ptr_uint_t
handle_running_on_valgrind(dr_vg_client_request_t *request)
{
    return 1;
}

static ptr_uint_t
handle_make_mem_defined_if_addressable(dr_vg_client_request_t *request)
{
    dr_printf("Make %d bytes defined if addressable.\n", request->args[1]);

    test_stats.num_bytes_made_defined += request->args[1];
    test_stats.num_define_memory_requests++;

    return 0;
}

/* Avoid truncating this block in Windows because an (unrelated) error occurs. */
#ifdef WINDOWS
static void
event_module_load(void *drcontext, const module_data_t *info, bool loaded)
{
    if ((info->names.module_name != NULL) &&
        (strcmp("ntdll.dll", info->names.module_name) == 0)) {
        *skip_truncation = (app_pc) dr_get_proc_address(info->handle,
                                                        "KiUserExceptionDispatcher");
    }
}
#endif

dr_emit_flags_t
empty_bb_event(void *drcontext, void *tag, instrlist_t *bb,
               bool for_trace, bool translating)
{
    return DR_EMIT_DEFAULT;
}

dr_emit_flags_t
bb_event_truncate(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating)
{
    bool truncated = false;
    instr_t *prev, *truncate_marker = instrlist_first(bb), *instr = instrlist_last(bb);

    if (instr_get_next(truncate_marker) != NULL)
        truncate_marker = instr_get_next(truncate_marker);

#ifdef WINDOWS
    if (dr_fragment_app_pc(tag) == *skip_truncation)
        return DR_EMIT_DEFAULT;
#endif

    while ((instr != NULL) && (instr != truncate_marker)) { // && !instr_ok_to_mangle(instr)) {
        prev = instr_get_prev(instr);
        instrlist_remove(bb, instr);
        instr_destroy(drcontext, instr);
        test_stats.num_instructions_truncated++;
        truncated = true;
        instr = prev;
    }
    /*
    if ((instr != NULL) && (instr != truncate_marker)) {
        instrlist_remove(bb, instr);
        instr_destroy(drcontext, instr);
        test_stats.num_instructions_truncated++;
        truncated = true;
    }
    */

    if (truncated)
        test_stats.num_bbs_truncated++;

    return DR_EMIT_DEFAULT;
}

void exit_event(void)
{
    if (test_stats.num_instructions_truncated > 0) {
        dr_printf("Truncated a total of %d instructions from %d basic blocks.\n",
                  test_stats.num_instructions_truncated, test_stats.num_bbs_truncated);
    }
    dr_printf("Received %d 'define memory' requests for a total of %d bytes.\n",
              test_stats.num_define_memory_requests, test_stats.num_bytes_made_defined);

#ifdef WINDOWS
    dr_global_free(skip_truncation, sizeof(app_pc));
#endif
}

DR_EXPORT
void dr_init(client_id_t id)
{
    const char *options = dr_get_options(id);

    /* This client supports 3 modes:
     *   - default: allows fast decoding of app instructions by not registering a bb event
     *   - full-decode: registers a bb event to enable full decoding of app instructions
     *   - truncate: registers a bb event that truncates basic blocks to max length 2
     */
    if (strcmp(options, "full-decode") == 0) {
        dr_printf("Init vg-annot with full decoding.\n");
        dr_register_bb_event(empty_bb_event);
    } else if (strcmp(options, "truncate") == 0) {
        dr_printf("Init vg-annot with bb truncation.\n");
        dr_register_bb_event(bb_event_truncate);
    } else {
        dr_printf("Init vg-annot with fast decoding.\n");
    }

    memset(&test_stats, 0, sizeof(test_stats_t));

#ifdef WINDOWS
    skip_truncation = dr_global_alloc(sizeof(app_pc));
    *skip_truncation = NULL;

    dr_register_module_load_event(event_module_load);
#endif
    dr_register_exit_event(exit_event);

    dr_annotation_register_valgrind(DR_VG_ID__RUNNING_ON_VALGRIND,
                                    handle_running_on_valgrind);

    dr_annotation_register_valgrind(DR_VG_ID__MAKE_MEM_DEFINED_IF_ADDRESSABLE,
                                    handle_make_mem_defined_if_addressable);
}
