/* ******************************************************
 * Copyright (c) 2015-2021 Google, Inc.  All rights reserved.
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

/* This client provides handlers for hypothetical annotations. There are 3 execution modes
 * for this client, which can be selected via command line argument:
 *
 *   - default (fast decoding): no argument
 *   - full decoding: "full-decode"
 *   - truncation: "truncate@#", where "#" is a single digit 1-9 indicating the maximum
 *                 number of app instructions that remain in each bb after truncation.
 */

#include "dr_api.h"
#include "client_tools.h"
#include "dr_ir_opnd.h"
#include "dr_annotation.h"
#include "drmgr.h"
#include "drreg.h"
#include <string.h> /* memset */
#ifdef WINDOWS
#    pragma warning(disable : 4100) /* unreferenced formal parameter */
#endif

/* Distinguishes client output from app output */
#define PRINT(s) dr_fprintf(STDERR, "      <" s ">\n");
#define PRINTF(s, ...) dr_fprintf(STDERR, "      <" s ">\n", __VA_ARGS__);

static client_id_t client_id;

static uint bb_truncation_length;

static void
test_two_args(int a, int b)
{
    PRINTF("test_two_args(): %d, %d", a, b);
}

static void
test_three_args(int a, int b, int c)
{
    PRINTF("test_three_args(): %d * %d * %d = %d", a, b, c, a * b * c);

    dr_annotation_set_return_value(a * b * c);
}

static void
test_eight_args(uint a, uint b, uint c, uint d, uint e, uint f, uint g, uint h)
{
    PRINTF("Test many args: "
           "a=%d, b=%d, c=%d, d=%d, e=%d, f=%d, g=%d, h=%d",
           a, b, c, d, e, f, g, h);
}

static void
test_nine_args(uint a, uint b, uint c, uint d, uint e, uint f, uint g, uint h, uint i)
{
    PRINTF("Test many args: "
           "a=%d, b=%d, c=%d, d=%d, e=%d, f=%d, g=%d, h=%d, i=%d",
           a, b, c, d, e, f, g, h, i);
}

static void
test_ten_args(uint a, uint b, uint c, uint d, uint e, uint f, uint g, uint h, uint i,
              uint j)
{
    PRINTF("Test many args: "
           "a=%d, b=%d, c=%d, d=%d, e=%d, f=%d, g=%d, h=%d, i=%d, j=%d",
           a, b, c, d, e, f, g, h, i, j);
}

static dr_emit_flags_t
bb_event_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating, OUT void **user_data)
{
    *(ptr_uint_t *)user_data = 0;
    for (instr_t *inst = instrlist_first(bb); inst != NULL; inst = instr_get_next(inst)) {
        if (instr_is_label(inst) &&
            (ptr_uint_t)instr_get_note(inst) == DR_NOTE_ANNOTATION) {
            *(ptr_uint_t *)user_data = 1;
            break;
        }
    }
    return DR_EMIT_DEFAULT;
}

dr_emit_flags_t
bb_event_insert(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                bool for_trace, bool translating, void *user_data)
{
    if ((ptr_uint_t)user_data == 0)
        return DR_EMIT_DEFAULT;

    const reg_id_t regs[] = {
    /* Put non-app values in all the annotation parameter registers. */
#ifdef X64
#    ifdef UNIX
        DR_REG_XDI, DR_REG_XSI, DR_REG_XDX, DR_REG_XCX, DR_REG_R8, DR_REG_R9
#    else
        DR_REG_XCX, DR_REG_XDX, DR_REG_R8, DR_REG_R9
#    endif
#else
        DR_REG_XDX, DR_REG_XCX
#endif
    };
    /* Test i#5118: not having app values in regs at annotation call point. */
    if (drmgr_is_first_nonlabel_instr(drcontext, inst)) {
        drvector_t allowed;
        drreg_init_and_fill_vector(&allowed, false);
        /* For each param register, reserve it for the whole bb and set it to zero. */
        for (size_t i = 0; i < sizeof(regs) / sizeof(regs[0]); ++i) {
            drreg_set_vector_entry(&allowed, regs[i], true);
            reg_id_t got;
            drreg_status_t res =
                drreg_reserve_register(drcontext, bb, inst, &allowed, &got);
            ASSERT(res == DRREG_SUCCESS && got == regs[i]);
            instrlist_meta_preinsert(bb, inst,
                                     XINST_CREATE_sub_s(drcontext,
                                                        opnd_create_reg(regs[i]),
                                                        opnd_create_reg(regs[i])));
            drreg_set_vector_entry(&allowed, regs[i], false);
        }
        drvector_delete(&allowed);
    }
    if (drmgr_is_last_instr(drcontext, inst)) {
        for (size_t i = 0; i < sizeof(regs) / sizeof(regs[0]); ++i) {
            drreg_status_t res = drreg_unreserve_register(drcontext, bb, inst, regs[i]);
            ASSERT(res == DRREG_SUCCESS);
        }
    }

    return DR_EMIT_DEFAULT;
}

/* Truncates every basic block to the length specified in the CL option (see dr_init) */
dr_emit_flags_t
bb_event_truncate(void *drcontext, void *, instrlist_t *bb, bool, bool)
{
    uint app_instruction_count = 0;
    instr_t *next, *instr = instrlist_first(bb);

    while (instr != NULL) {
        next = instr_get_next(instr);
        if (!instr_is_meta(instr)) {
            if (app_instruction_count == bb_truncation_length) {
                instrlist_remove(bb, instr);
                instr_destroy(drcontext, instr);
            } else {
                app_instruction_count++;
            }
        }
        instr = next;
    }

    return DR_EMIT_DEFAULT;
}

/* Convenience function for registering annotation handlers */
static void
register_call(const char *annotation, void *target, uint num_args)
{
    dr_annotation_register_call(annotation, target, false, num_args,
                                DR_ANNOTATION_CALL_TYPE_FASTCALL);
}

static void
event_exit(void)
{
    if (!drmgr_unregister_bb_instrumentation_event(bb_event_analysis) ||
        drreg_exit() != DRREG_SUCCESS)
        PRINT("exit failed");
    drmgr_exit();
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
#ifdef WINDOWS
    dr_enable_console_printing();
#endif

    client_id = id;

    /* XXX: should use droption */
    if (argc > 1 && strcmp(argv[1], "full-decode") == 0) {
        PRINT("Init annotation test client with full decoding");
        drreg_options_t ops = { sizeof(ops), 2 /*max slots needed*/, false };
        if (!drmgr_init() || drreg_init(&ops) != DRREG_SUCCESS ||
            !drmgr_register_bb_instrumentation_event(bb_event_analysis, bb_event_insert,
                                                     NULL))
            PRINT("init failed");
        dr_register_exit_event(event_exit);
    } else if (argc > 1 && strlen(argv[1]) >= 8 && strncmp(argv[1], "truncate", 8) == 0) {
        bb_truncation_length = (argv[1][9] - '0'); /* format is "truncate@n" (0<n<10) */
        ASSERT(bb_truncation_length < 10 && bb_truncation_length > 0);
        PRINT("Init annotation test client with bb truncation");
        /* We deliberately test without drmgr for this case. */
#undef dr_register_bb_event
        dr_register_bb_event(bb_event_truncate);
    } else {
        /* We again do not use drmgr here to ensure we have fast decoding. */
        PRINT("Init annotation test client with fast decoding");
    }

    register_call("test_annotation_two_args", (void *)test_two_args, 2);
    register_call("test_annotation_three_args", (void *)test_three_args, 3);
    register_call("test_annotation_eight_args", (void *)test_eight_args, 8);
    register_call("test_annotation_nine_args", (void *)test_nine_args, 9);
    register_call("test_annotation_ten_args", (void *)test_ten_args, 10);
}
