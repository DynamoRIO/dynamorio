/* ******************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
 * ******************************************************/

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

#include <string.h> /* memset */
#include "dr_api.h"
#include "dr_ir_opnd.h"
#include "dr_annotation.h"

#define MAX_MODE_HISTORY 100

#define PRINT(s) dr_printf("      <"s">\n")
#define PRINTF(s, ...) dr_printf("      <"s">\n", __VA_ARGS__)

static client_id_t client_id;

#ifdef WINDOWS
static app_pc *skip_truncation;
#endif

static void
test_two_args(int a, int b)
{
    PRINTF("test_two_args(): %d, %d", a, b);
}

static int
test_three_args(int a, int b, int c)
{
    PRINTF("test_three_args(): %d * %d * %d = %d", a, b, c, a * b * c);

    RETURN(int, a * b * c);
}

static void
test_eight_args(uint a, uint b, uint c, uint d, uint e, uint f,
                uint g, uint h)
{
    PRINTF("Test many args: "
        "a=%d, b=%d, c=%d, d=%d, e=%d, f=%d, g=%d, h=%d",
        a, b, c, d, e, f, g, h);
}

static void
test_nine_args(uint a, uint b, uint c, uint d, uint e, uint f,
               uint g, uint h, uint i)
{
    PRINTF("Test many args: "
        "a=%d, b=%d, c=%d, d=%d, e=%d, f=%d, g=%d, h=%d, i=%d",
        a, b, c, d, e, f, g, h, i);
}

static void
test_ten_args(uint a, uint b, uint c, uint d, uint e, uint f,
              uint g, uint h, uint i, uint j)
{
    PRINTF("Test many args: "
        "a=%d, b=%d, c=%d, d=%d, e=%d, f=%d, g=%d, h=%d, i=%d, j=%d",
        a, b, c, d, e, f, g, h, i, j);
}

static void
event_module_load(void *, const module_data_t *info, bool)
{
#ifdef WINDOWS // truncating these blocks causes app exceptions (unrelated to annotations)
    if ((info->names.module_name != NULL) &&
        (strcmp("ntdll.dll", info->names.module_name) == 0)) {
        skip_truncation[0] = (app_pc) dr_get_proc_address(info->handle,
                                                          "KiUserExceptionDispatcher");
        skip_truncation[1] = (app_pc) dr_get_proc_address(info->handle,
                                                          "LdrInitializeThunk");
    }
#endif
}

dr_emit_flags_t
empty_bb_event(void *, void *, instrlist_t *, bool, bool)
{
    return DR_EMIT_DEFAULT;
}

dr_emit_flags_t
bb_event_truncate(void *drcontext, void *tag, instrlist_t *bb, bool, bool)
{
    instr_t *prev, *first = instrlist_first(bb), *instr = instrlist_last(bb);

#ifdef WINDOWS
    app_pc fragment = dr_fragment_app_pc(tag);
    if ((fragment == skip_truncation[0]) || (fragment == skip_truncation[1]))
        return DR_EMIT_DEFAULT;
#endif

    while ((first != NULL) && !instr_ok_to_mangle(first))
        first = instr_get_next(first);
    if (first != NULL) {
        while ((instr != NULL) && (instr != first) && !instr_ok_to_mangle(instr)) {
            prev = instr_get_prev(instr);
            instrlist_remove(bb, instr);
            instr_destroy(drcontext, instr);
            instr = prev;
        }
        if ((instr != NULL) && (instr != first)) {
            instrlist_remove(bb, instr);
            instr_destroy(drcontext, instr);
        }
    }
    return DR_EMIT_DEFAULT;
}

static void
register_call(const char *annotation, void *target, uint num_args)
{
    dr_annot_register_call(client_id, annotation, target, false, num_args
                           _IF_NOT_X64(ANNOT_CALL_TYPE_FASTCALL));
}

static void
event_exit(void)
{
#ifdef WINDOWS
    dr_global_free(skip_truncation, 2 * sizeof(app_pc));
#endif
}

DR_EXPORT void
dr_init(client_id_t id)
{
    const char *options = dr_get_options(id);

# ifdef WINDOWS
    dr_enable_console_printing();
# endif

    client_id = id;

    if (strcmp(options, "+bb") == 0) {
        PRINT("Init annotation test client with full decoding");
        dr_register_bb_event(empty_bb_event);
    } else if (strcmp(options, "+b/b") == 0) {
        PRINT("Init annotation test client with bb truncation");
        dr_register_bb_event(bb_event_truncate);
    } else {
        PRINT("Init annotation test client with fast decoding");
    }

#ifdef WINDOWS
    skip_truncation = (app_pc *) dr_global_alloc(2 * sizeof(app_pc));
    memset(skip_truncation, 0, 2 * sizeof(app_pc));
#endif

    dr_register_exit_event(event_exit);
    dr_register_module_load_event(event_module_load);

    register_call("test_annotation_two_args", (void *) test_two_args, 2);
    register_call("test_annotation_three_args", (void *) test_three_args, 3);
    register_call("test_annotation_eight_args", (void *) test_eight_args, 8);
    register_call("test_annotation_nine_args", (void *) test_nine_args, 9);
    register_call("test_annotation_ten_args", (void *) test_ten_args, 10);
}
