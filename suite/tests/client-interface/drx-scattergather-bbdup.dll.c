/* **********************************************************
 * Copyright (c) 2019-2023 Google, Inc.  All rights reserved.
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

/* Tests drx_expand_scatter_gather(). */

#include "dr_api.h"
#include "client_tools.h"
#include "drmgr.h"
#include "drreg.h"
#include "drx.h"
#include "drbbdup.h"
#include "drx-scattergather-shared.h"
#include <limits.h>

static uint64 global_sg_count;

static void
event_exit(void)
{
    drbbdup_status_t res = drbbdup_exit();
    CHECK(res == DRBBDUP_SUCCESS, "drbbdup exit failed");
    drx_exit();
    drreg_exit();
    drmgr_exit();
    dr_fprintf(STDERR, "event_exit, %d scatter/gather instructions\n", global_sg_count);
}

static void
inscount(uint num_instrs)
{
    /* We assume the test is single threaded hence no race. */
    global_sg_count += num_instrs;
}

/* Global, because the markers will be in a different app2app list after breaking up
 * scatter/gather into separate basic blocks during expansion.
 */
static app_pc mask_clobber_test_avx512_gather_pc = (app_pc)INT_MAX;
static app_pc mask_update_test_avx512_gather_pc = (app_pc)INT_MAX;
static app_pc mask_clobber_test_avx512_scatter_pc = (app_pc)INT_MAX;
static app_pc mask_update_test_avx512_scatter_pc = (app_pc)INT_MAX;
static app_pc mask_update_test_avx2_gather_pc = (app_pc)INT_MAX;

static ptr_int_t instru_mode;
enum {
    INSTRU_MODE_EXPAND = 0,
    INSTRU_MODE_NOP = 1,
};

static uintptr_t
event_bb_setup(void *drbbdup_ctx, void *drcontext, void *tag, instrlist_t *bb,
               bool *enable_dups, bool *enable_dynamic_handling, void *user_data)
{
    *enable_dups = true;
    drbbdup_status_t res;
    res = drbbdup_register_case_encoding(drbbdup_ctx, INSTRU_MODE_NOP);
    CHECK(res == DRBBDUP_SUCCESS, "register case failed");
    *enable_dynamic_handling = false;
    return INSTRU_MODE_EXPAND;
}

static void
event_bb_retrieve_mode(void *drcontext, void *tag, instrlist_t *bb, instr_t *where,
                       void *user_data, void *orig_analysis_data)
{
    /* Nop. */
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      instr_t *where, bool for_trace, bool translating,
                      void *orig_analysis_data, void *user_data)
{
    uint num_instrs;
    if (user_data == NULL)
        return DR_EMIT_DEFAULT;
    num_instrs = *(uint *)user_data;
    bool first = false;
    if (drbbdup_is_first_instr(drcontext, instr, &first) != DRBBDUP_SUCCESS || !first)
        return DR_EMIT_DEFAULT;
    dr_insert_clean_call(drcontext, bb, where, (void *)inscount, false /* save fpstate */,
                         1, OPND_CREATE_INT32(num_instrs));
    /* We stress test the non-recreate path. */
    return DR_EMIT_DEFAULT; // NOCHECK STORE_TRANSLATIONS;
}

static dr_emit_flags_t
event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating, void **user_data)
{
    uint num_sg_instrs = 0;
    bool is_emulation = false;
    for (instr_t *instr = instrlist_first(bb); instr != NULL;
         instr = instr_get_next(instr)) {
        if (instr_is_gather(instr) || instr_is_scatter(instr)) {
            /* FIXME i#2985: some scatter/gather instructions will not get expanded in
             * 32-bit mode.
             */
            IF_X64(dr_fprintf(STDERR, "Unexpected scatter or gather instruction\n"));
        }
        if (drmgr_is_emulation_start(instr)) {
            emulated_instr_t emulated_instr;
            emulated_instr.size = sizeof(emulated_instr);
            CHECK(drmgr_get_emulated_instr_data(instr, &emulated_instr),
                  "drmgr_get_emulated_instr_data() failed");
            if (instr_is_gather(emulated_instr.instr) ||
                instr_is_scatter(emulated_instr.instr))
                num_sg_instrs++;
            is_emulation = true;
            continue;
        }
        if (drmgr_is_emulation_end(instr)) {
            is_emulation = false;
            continue;
        }
        if (is_emulation)
            continue;
        if (!instr_is_app(instr))
            continue;
    }
    *user_data = (uint *)dr_thread_alloc(drcontext, sizeof(uint));
    uint *num_instr_data = (uint *)*user_data;
    *num_instr_data = num_sg_instrs;
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb_analyze_case(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                      bool translating, uintptr_t mode, void *user_data,
                      void *orig_analysis_data, void **analysis_data)
{
    if (mode == INSTRU_MODE_NOP) {
        return DR_EMIT_DEFAULT;
    } else if (mode == INSTRU_MODE_EXPAND) {
        return event_bb_analysis(drcontext, tag, bb, for_trace, translating,
                                 analysis_data);
    } else {
        DR_ASSERT(false);
        return DR_EMIT_DEFAULT;
    }
}

static void
event_bb_analyze_case_cleanup(void *drcontext, uintptr_t mode, void *user_data,
                              void *orig_analysis_data, void *analysis_data)
{
    if (mode == INSTRU_MODE_NOP) {
        /* No cleanup needed. */
    } else if (mode == INSTRU_MODE_EXPAND) {
        dr_thread_free(drcontext, analysis_data, sizeof(uint));
    } else
        DR_ASSERT(false);
}

static dr_emit_flags_t
event_app_instruction_case(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                           instr_t *where, bool for_trace, bool translating,
                           uintptr_t mode, void *user_data, void *orig_analysis_data,
                           void *analysis_data)
{
    if (mode == INSTRU_MODE_NOP) {
        return DR_EMIT_DEFAULT;
    }
    if (mode == INSTRU_MODE_EXPAND) {
        return event_app_instruction(drcontext, tag, bb, instr, where, for_trace,
                                     translating, orig_analysis_data, analysis_data);
    } else
        DR_ASSERT(false);
    return DR_EMIT_DEFAULT;
}

static byte *
search_for_next_scatter_or_gather_pc_impl(void *drcontext, instr_t *start_instr,
                                          bool search_for_gather)
{
    byte *pc = instr_get_app_pc(start_instr);
    instr_t temp_instr;
    instr_init(drcontext, &temp_instr);
    /* This relies heavily on the exact test app's behavior, as well as
     * the scatter/gather expansion's code layout.
     */
    int instr_count = 0;
    while (true) {
        instr_reset(drcontext, &temp_instr);
        byte *next_pc = decode(drcontext, pc, &temp_instr);
        CHECK(next_pc != NULL,
              "Everything should be decodable in the test until a "
              "scatter or gather instruction will be found.");
        CHECK(!instr_is_cti(&temp_instr), "unexpected cti instruction when decoding");
        if (search_for_gather && instr_is_gather(&temp_instr)) {
            break;
        } else if (!search_for_gather && instr_is_scatter(&temp_instr)) {
            break;
        }
        pc = next_pc;
        const int INSTRUCTIONS_OFF_MARKERS = 5;
        if (instr_count++ > INSTRUCTIONS_OFF_MARKERS)
            return NULL;
    }
    instr_free(drcontext, &temp_instr);
    return pc;
}

static byte *
search_for_next_scatter_pc(void *drcontext, instr_t *start_instr)
{
    return search_for_next_scatter_or_gather_pc_impl(drcontext, start_instr, false);
}

static byte *
search_for_next_gather_pc(void *drcontext, instr_t *start_instr)
{
    return search_for_next_scatter_or_gather_pc_impl(drcontext, start_instr, true);
}

static dr_emit_flags_t
event_bb_app2app(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                 bool translating)
{
    instr_t *instr;
    bool expanded = false;
    bool scatter_gather_present = false;
    ptr_int_t val;

    for (instr = instrlist_first_app(bb); instr != NULL;
         instr = instr_get_next_app(instr)) {
        if (instr_is_gather(instr)) {
            scatter_gather_present = true;
        } else if (instr_is_scatter(instr)) {
            scatter_gather_present = true;
        } else if (instr_is_mov_constant(instr, &val) &&
                   val == TEST_AVX512_GATHER_MASK_CLOBBER_MARKER) {
            instr_t *next_instr = instr_get_next(instr);
            if (next_instr != NULL) {
                if (instr_is_mov_constant(next_instr, &val) &&
                    val == TEST_AVX512_GATHER_MASK_CLOBBER_MARKER) {
                    /* We're searching for the next gather instruction that will be
                     * expanded. We will use its pc to identify the corner case
                     * instructions where we will inject a ud2 after gather expansion.
                     */
                    CHECK(mask_clobber_test_avx512_gather_pc == (app_pc)INT_MAX,
                          "unexpected gather instruction pc");
                    mask_clobber_test_avx512_gather_pc =
                        search_for_next_gather_pc(drcontext, next_instr);
                }
            }
        } else if (instr_is_mov_constant(instr, &val) &&
                   val == TEST_AVX512_SCATTER_MASK_CLOBBER_MARKER) {
            instr_t *next_instr = instr_get_next(instr);
            if (next_instr != NULL) {
                if (instr_is_mov_constant(next_instr, &val) &&
                    val == TEST_AVX512_SCATTER_MASK_CLOBBER_MARKER) {
                    /* Same as above, but for scatter case. */
                    CHECK(mask_clobber_test_avx512_scatter_pc == (app_pc)INT_MAX,
                          "unexpected scatter instruction pc");
                    mask_clobber_test_avx512_scatter_pc =
                        search_for_next_scatter_pc(drcontext, next_instr);
                }
            }
        } else if (instr_is_mov_constant(instr, &val) &&
                   val == TEST_AVX512_GATHER_MASK_UPDATE_MARKER) {
            instr_t *next_instr = instr_get_next(instr);
            if (next_instr != NULL) {
                if (instr_is_mov_constant(next_instr, &val) &&
                    val == TEST_AVX512_GATHER_MASK_UPDATE_MARKER) {
                    /* Same comment as above. */
                    CHECK(mask_update_test_avx512_gather_pc == (app_pc)INT_MAX,
                          "unexpected gather instruction pc");
                    mask_update_test_avx512_gather_pc =
                        search_for_next_gather_pc(drcontext, next_instr);
                }
            }
        } else if (instr_is_mov_constant(instr, &val) &&
                   val == TEST_AVX512_SCATTER_MASK_UPDATE_MARKER) {
            instr_t *next_instr = instr_get_next(instr);
            if (next_instr != NULL) {
                if (instr_is_mov_constant(next_instr, &val) &&
                    val == TEST_AVX512_SCATTER_MASK_UPDATE_MARKER) {
                    /* Same comment as above. */
                    CHECK(mask_update_test_avx512_scatter_pc == (app_pc)INT_MAX,
                          "unexpected scatter instruction pc");
                    mask_update_test_avx512_scatter_pc =
                        search_for_next_scatter_pc(drcontext, next_instr);
                }
            }
        } else if (instr_is_mov_constant(instr, &val) &&
                   val == TEST_AVX2_GATHER_MASK_UPDATE_MARKER) {
            instr_t *next_instr = instr_get_next(instr);
            if (next_instr != NULL) {
                if (instr_is_mov_constant(next_instr, &val) &&
                    val == TEST_AVX2_GATHER_MASK_UPDATE_MARKER) {
                    /* Same comment as above. */
                    CHECK(mask_update_test_avx2_gather_pc == (app_pc)INT_MAX,
                          "unexpected gather instruction pc");
                    mask_update_test_avx2_gather_pc =
                        search_for_next_gather_pc(drcontext, next_instr);
                }
            }
        }
    }
    bool expansion_ok = drx_expand_scatter_gather(drcontext, bb, &expanded);
    if (!expansion_ok) {
        /* XXX i#2985: The qword versions of scatter/gather are unsupported
         * in 32-bit mode.
         */
        IF_X64(CHECK(false, "drx_expand_scatter_gather() failed"));
    }
    CHECK((scatter_gather_present IF_X64(&&expanded)) || (expansion_ok && !expanded),
          "drx_expand_scatter_gather() bad OUT values");
    for (instr = instrlist_first(bb); instr != NULL; instr = instr_get_next(instr)) {
        if (instr_get_opcode(instr) == OP_kandnw &&
            (instr_get_app_pc(instr) == mask_clobber_test_avx512_gather_pc ||
             instr_get_app_pc(instr) == mask_clobber_test_avx512_scatter_pc)) {
            /* We've found the clobber case of the scatter/gather sequence that clobbers
             * the k0 mask register. Then we're inserting a ud2 app instruction right
             * after it, so we will SIGILL and the value will be tested in the app's
             * signal handler. We will be here twice: one time during bb creation, and
             * another time when translating. After that, the app itself will longjmp to
             * the next subtest and we will neither recreate this code nor translate it.
             */
            instrlist_postinsert(
                bb, instr,
                INSTR_XL8(INSTR_CREATE_ud2(drcontext),
                          /* It's guaranteed by the test that there will be a next
                           * app instruction, because the emulated sequence consists
                           * of 16 mask updates, and this is just the first.
                           */
                          instr_get_app_pc(instr_get_next_app(instr))));
            /* We don't need to do anything else. */
            break;
        } else if (instr_get_opcode(instr) == OP_kandnw &&
                   (instr_get_app_pc(instr) == mask_update_test_avx512_gather_pc ||
                    instr_get_app_pc(instr) == mask_update_test_avx512_scatter_pc)) {
            /* Same as above, but this time, we inject the ud2 right before the mask
             * update.
             */
            instrlist_preinsert(bb, instr,
                                INSTR_XL8(INSTR_CREATE_ud2(drcontext),
                                          /* It's again guaranteed by the test that there
                                           * will be a next app instruction.
                                           */
                                          instr_get_app_pc(instr_get_next_app(instr))));
        } else if (instr_is_mov(instr) && instr_reads_memory(instr) &&
                   (instr_get_app_pc(instr) == mask_update_test_avx2_gather_pc)) {
            instrlist_postinsert(bb, instr,
                                 INSTR_XL8(INSTR_CREATE_ud2(drcontext),
                                           /* It's again guaranteed by the test that there
                                            * will be a next app instruction.
                                            */
                                           instr_get_app_pc(instr_get_next_app(instr))));
            /* We don't need to do anything else. */
            break;
        }
    }
    return DR_EMIT_DEFAULT;
}

DR_EXPORT void
dr_init(client_id_t id)
{
    drmgr_priority_t priority = { sizeof(priority), "drx-scattergather-bbdup", NULL, NULL,
                                  0 };
    drreg_options_t ops = { sizeof(ops), 2 /*max slots needed*/, false };
    drreg_status_t res;
    bool ok = drmgr_init();
    CHECK(ok, "drmgr_init failed");
    ok = drx_init();
    CHECK(ok, "drx_init failed");
    res = drreg_init(&ops);
    CHECK(res == DRREG_SUCCESS, "drreg_init failed");
    dr_register_exit_event(event_exit);

    drbbdup_options_t opts = {
        sizeof(opts),
    };
    opts.struct_size = sizeof(drbbdup_options_t);
    opts.set_up_bb_dups = event_bb_setup;
    opts.insert_encode = event_bb_retrieve_mode;
    opts.analyze_case_ex = event_bb_analyze_case;
    opts.destroy_case_analysis = event_bb_analyze_case_cleanup;
    opts.instrument_instr_ex = event_app_instruction_case;
    opts.runtime_case_opnd = OPND_CREATE_ABSMEM(&instru_mode, OPSZ_PTR);
    opts.atomic_load_encoding = true;
    opts.non_default_case_limit = 1;

    drbbdup_status_t bbdup_res = drbbdup_init(&opts);
    CHECK(bbdup_res == DRBBDUP_SUCCESS, "drbbdup init failed");

    drmgr_priority_t pri_pre_bbdup = { sizeof(drmgr_priority_t),
                                       "drx-scattergather-bbdup-app2app", NULL, NULL,
                                       DRMGR_PRIORITY_APP2APP_DRBBDUP - 1 };
    ok = drmgr_register_bb_app2app_event(event_bb_app2app, &pri_pre_bbdup);
    CHECK(ok, "drmgr register app2app failed");
}
