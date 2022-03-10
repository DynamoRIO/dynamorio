/* **********************************************************
 * Copyright (c) 2015-2022 Google, Inc.  All rights reserved.
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

/* Tests the drbbdup extension's interactions with emulation. */

#include "dr_api.h"
#include "client_tools.h"
#include "drmgr.h"
#include "drbbdup.h"
#include "drutil.h"
#include "drreg.h"
#include <string.h>

typedef struct _per_block_t {
    bool saw_first;
    bool saw_last;
} per_block_t;

/* Assume single threaded. */
static uintptr_t encode_val = 3;

#ifdef X86
static bool saw_movs;
static bool saw_zero_iter_rep_string;
#endif
static bool has_rest_of_block_emul;

static dr_emit_flags_t
app2app_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
              bool translating)
{
#if VERBOSE
    /* Useful for debugging. */
    instrlist_disassemble(drcontext, tag, bb, STDERR);
#endif

    has_rest_of_block_emul = false;

    /* Test drutil rep string expansion interacting with drbbdup. */
    bool expanded;
    if (!drutil_expand_rep_string_ex(drcontext, bb, &expanded, NULL))
        DR_ASSERT(false);
    if (expanded) {
        /* We can't overlap ours with drutil's so bow out. */
        has_rest_of_block_emul = true;
        return DR_EMIT_DEFAULT;
    }
    /* Test handling of DR_EMULATE_REST_OF_BLOCK by inserting it in the middle
     * of a multi-instr block and ensuring drbbdup doesn't let it leak through into
     * the start of subsequent cloned blocks.
     * We want to leave some blocks unchanged though to test drbbdup's own
     * emulation labels.
     *
     * XXX i#5390: We should also test the last instr being emulated: not sure drbbdup
     * will do the right thing there if it's a "special" instr.
     */
    static int bb_count;
    if (++bb_count % 2 == 0)
        return DR_EMIT_DEFAULT;
    instr_t *mid = instrlist_last_app(bb);
    if (mid == NULL || mid == instrlist_first_app(bb))
        return DR_EMIT_DEFAULT;
    mid = instr_get_prev_app(mid);
    if (mid == NULL || mid == instrlist_first_app(bb))
        return DR_EMIT_DEFAULT;
    emulated_instr_t emulated_instr;
    emulated_instr.size = sizeof(emulated_instr);
    emulated_instr.pc = instr_get_app_pc(mid);
    emulated_instr.instr = instr_clone(drcontext, mid);
    emulated_instr.flags = DR_EMULATE_REST_OF_BLOCK;
    drmgr_insert_emulation_start(drcontext, bb, mid, &emulated_instr);
    /* XXX i#5400: We'd like to pass a user_data to the instrument event but drbbdup
     * doesn't support that; instead we rely on being single-threaded.
     */
    has_rest_of_block_emul = true;

    return DR_EMIT_DEFAULT;
}

static uintptr_t
set_up_bb_dups(void *drbbdup_ctx, void *drcontext, void *tag, instrlist_t *bb,
               bool *enable_dups, bool *enable_dynamic_handling, void *user_data)
{
    drbbdup_status_t res;

    CHECK(enable_dups != NULL, "should not be NULL");
    CHECK(enable_dynamic_handling != NULL, "should not be NULL");

    res = drbbdup_register_case_encoding(drbbdup_ctx, 1);
    CHECK(res == DRBBDUP_SUCCESS, "failed to register case 1");
    res = drbbdup_register_case_encoding(drbbdup_ctx, 2);
    CHECK(res == DRBBDUP_SUCCESS, "failed to register case 2");

    *enable_dups = true;
    *enable_dynamic_handling = false; /* disable dynamic handling */

    return 0; /* return default case */
}

#ifdef X86
/* Assumes it is only called inside a rep string expansion. */
static void
look_for_zero_iters(reg_t xcx)
{
    if (xcx == 0)
        saw_zero_iter_rep_string = true;
}
#endif

static void
analyze_case(void *drcontext, void *tag, instrlist_t *bb, uintptr_t encoding,
             void *user_data, void *orig_analysis_data, void **analysis_data)
{
    per_block_t *per_block =
        (per_block_t *)dr_thread_alloc(drcontext, sizeof(per_block_t));
    memset(per_block, 0, sizeof(*per_block));
    *analysis_data = per_block;
}

static void
destroy_case_analysis(void *drcontext, uintptr_t encoding, void *user_data,
                      void *orig_analysis_data, void *analysis_data)
{
    per_block_t *per_block = (per_block_t *)analysis_data;
    CHECK(per_block->saw_first, "failed to see first instr");
    /* If we added a rest-of-block emul it will hide the last instr. */
    CHECK(per_block->saw_last || has_rest_of_block_emul, "failed to see last instr");
    dr_thread_free(drcontext, analysis_data, sizeof(per_block_t));
}

static dr_emit_flags_t
instrument_instr(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                 instr_t *where, bool for_trace, bool translating, uintptr_t encoding,
                 void *user_data, void *orig_analysis_data, void *analysis_data)
{
    bool is_first, is_last;
    drbbdup_status_t res;
    per_block_t *per_block = (per_block_t *)analysis_data;

    res = drbbdup_is_first_instr(drcontext, instr, &is_first);
    CHECK(res == DRBBDUP_SUCCESS, "failed to check whether instr is first");
    if (is_first) {
        per_block->saw_first = true;
        /* Ensure DR_EMULATE_REST_OF_BLOCK didn't leak through. */
        const emulated_instr_t *emul_info;
        if (drmgr_in_emulation_region(drcontext, &emul_info)) {
            /* It might be the rep string, which sets DR_EMULATE_INSTR_ONLY.
             * XXX: We could try to pass a flag: but drbbdup won't let
             * us pass from app2app; we'd need TLS or a global.
             */
            CHECK(!TEST(DR_EMULATE_REST_OF_BLOCK, emul_info->flags) ||
                      TEST(DR_EMULATE_INSTR_ONLY, emul_info->flags),
                  "DR_EMULATE_REST_OF_BLOCK leaked through!");
        }
    }

#ifdef X86
    /* Cause drutil's expanded "rep movs" to crash if drbbdup sets
     * DRREG_IGNORE_CONTROL_FLOW (i#5398).  We do not spill rcx until after the
     * rcx==0 path jumps over the OP_movs, such that drreg's restore before the
     * OP_loop writes the wrong value.
     */
    if (instr_get_opcode(instr) == OP_movs) {
        saw_movs = true;
        reg_id_t reg_clobber;
        drvector_t rvec;
        drreg_init_and_fill_vector(&rvec, false);
        drreg_set_vector_entry(&rvec, DR_REG_XCX, true);
        if (drreg_reserve_register(drcontext, bb, where, &rvec, &reg_clobber) !=
            DRREG_SUCCESS)
            CHECK(false, "failed to reserve scratch register\n");
        drvector_delete(&rvec);
        instrlist_meta_preinsert(bb, where,
                                 XINST_CREATE_load_int(drcontext,
                                                       opnd_create_reg(reg_clobber),
                                                       OPND_CREATE_INT32(-1)));
        if (drreg_unreserve_register(drcontext, bb, where, reg_clobber) != DRREG_SUCCESS)
            CHECK(false, "failed to unreserve scratch register\n");
    }
    if (instr_get_opcode(instr) == OP_jecxz) {
        /* Ensure our test case includes a zero-iter rep string. */
        dr_insert_clean_call(drcontext, bb, where, look_for_zero_iters, false, 1,
                             opnd_create_reg(DR_REG_XCX));
    }
#endif

    /* Ensure the drmgr emulation API works for a final special instr where "instr" !=
     * "where".  The emulation shows up on the label prior to the "where" for the last
     * instr so we can't compare emul to instr; instead we make sure the emul returned
     * is never a jump to a label.
     */
    instr_t *instr_fetch = drmgr_orig_app_instr_for_fetch(drcontext);
    CHECK(instr_fetch == NULL || !instr_is_ubr(instr_fetch) ||
              !opnd_is_instr(instr_get_target(instr_fetch)),
          "app instr should never be jump to label");

    /* Ensure the drmgr emulation API works for the final special instr in the
     * final case (as well as other cases).
     */
    instr_t *instr_operands = drmgr_orig_app_instr_for_operands(drcontext);
#if VERBOSE
    /* Useful for debugging */
    dr_fprintf(STDERR, "%s: emul fetch=", __FUNCTION__);
    if (instr_fetch == NULL)
        dr_fprintf(STDERR, "<null>", __FUNCTION__);
    else
        instr_disassemble(drcontext, instr_fetch, STDERR);
    dr_fprintf(STDERR, "  op=", __FUNCTION__);
    if (instr_operands == NULL)
        dr_fprintf(STDERR, "<null>", __FUNCTION__);
    else
        instr_disassemble(drcontext, instr_operands, STDERR);
    dr_fprintf(STDERR, "  instr=", __FUNCTION__);
    instr_disassemble(drcontext, instr, STDERR);
    dr_fprintf(STDERR, "  where=", __FUNCTION__);
    instr_disassemble(drcontext, where, STDERR);
    dr_fprintf(STDERR, "\n");
#endif
    CHECK(instr_fetch != NULL || instr_operands != NULL || has_rest_of_block_emul ||
              (instr_get_prev(where) != NULL &&
               drmgr_is_emulation_start(instr_get_prev(where))),
          "emul error");

    res = drbbdup_is_last_instr(drcontext, instr, &is_last);
    CHECK(res == DRBBDUP_SUCCESS, "failed to check whether instr is last");
    if (is_last) {
        per_block->saw_last = true;
        if (!has_rest_of_block_emul) {
            CHECK(instr_fetch != NULL || !instr_is_app(instr),
                  "last instr hidden from emul");
            CHECK(instr_operands != NULL || !instr_is_app(instr),
                  "last instr hidden from emul");
        }
    }

    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
#ifdef X86
    CHECK(saw_movs, "test case missing OP_movs");
    CHECK(saw_zero_iter_rep_string, "test case missing zero-iter rep string");
#endif

    drbbdup_status_t res = drbbdup_exit();
    CHECK(res == DRBBDUP_SUCCESS, "drbbdup exit failed");

    bool app2app_res = drmgr_unregister_bb_app2app_event(app2app_event);
    CHECK(app2app_res, "failed to unregister app2app event");

    drmgr_exit();
    drutil_exit();
    if (drreg_exit() != DRREG_SUCCESS)
        CHECK(false, "drreg_exit failed");

    dr_fprintf(STDERR, "Success\n");
}

DR_EXPORT void
dr_init(client_id_t id)
{
    drreg_options_t ops = { sizeof(ops), 1, false };
    if (!drmgr_init() || !drutil_init() || drreg_init(&ops) != DRREG_SUCCESS)
        CHECK(false, "library init failed");

    drbbdup_options_t opts = { 0 };
    opts.struct_size = sizeof(drbbdup_options_t);
    opts.set_up_bb_dups = set_up_bb_dups;
    opts.analyze_case = analyze_case;
    opts.destroy_case_analysis = destroy_case_analysis;
    opts.instrument_instr_ex = instrument_instr;
    opts.runtime_case_opnd = OPND_CREATE_ABSMEM(&encode_val, OPSZ_PTR);
    opts.non_default_case_limit = 3;

    drbbdup_status_t res = drbbdup_init(&opts);
    CHECK(res == DRBBDUP_SUCCESS, "drbbdup init failed");
    dr_register_exit_event(event_exit);

    bool app2app_res = drmgr_register_bb_app2app_event(app2app_event, NULL);
    CHECK(app2app_res, "app2app failed");
}
