/* **********************************************************
 * Copyright (c) 2015-2016 Google, Inc.  All rights reserved.
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
#include "drsyms.h"
#include "drmgr.h"
#include <string.h>

static app_pc func_pc = NULL;

void static mbr_instru_test(app_pc __attribute__((unused)) caller, app_pc callee)
{
    if (func_pc && callee == func_pc)
        dr_printf("Call to test_func\n");
}

dr_emit_flags_t static bb_event(void *drcontext, void __attribute__((unused)) * tag,
                                instrlist_t *bb, instr_t *instr,
                                bool __attribute__((unused)) for_trace,
                                bool __attribute__((unused)) translating,
                                void __attribute__((unused)) * user_data)
{
    if (instr_is_app(instr) && instr_is_call_indirect(instr)) {
        dr_insert_mbr_instrumentation(drcontext, bb, instr, &mbr_instru_test,
                                      SPILL_SLOT_1);
    }
    return DR_EMIT_DEFAULT;
}

bool static search_test_func(drsym_info_t *info,
                             drsym_error_t __attribute__((unused)) status, void *start)
{
    if (strcmp(info->name, "test_func") == 0)
        func_pc = start + info->start_offs;
    return true;
}

void static load_event(void __attribute__((unused)) * drcontext,
                       const module_data_t *info, bool __attribute__((unused)) loaded)
{
    drsym_enumerate_symbols_ex(info->full_path, &search_test_func, sizeof(drsym_info_t),
                               info->start, DRSYM_DEMANGLE_FULL);
    drsym_free_resources(info->full_path);
}

void static exit_event(void)
{
    drsym_exit();
    drmgr_exit();
}

DR_EXPORT void
dr_client_main(client_id_t __attribute__((unused)) id, int __attribute__((unused)) argc,
               const char __attribute__((unused)) * argv[])
{
    drsym_init(0);
    drmgr_init();
    dr_register_exit_event(&exit_event);
    drmgr_register_module_load_event(&load_event);
    drmgr_register_bb_instrumentation_event(NULL, &bb_event, NULL);
}
