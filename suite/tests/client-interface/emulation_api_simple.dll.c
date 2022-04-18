/* **********************************************************
 * Copyright (c) 2021-2022 Google, Inc.   All rights reserved.
 * Copyright (c) 2018 ARM Limited. All rights reserved.
 * **********************************************************
 *
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
 * * Neither the name of ARM Limited nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL ARM LIMITED OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 */

/* A simple client replacing 'dst = src0 & src1' with
 * 'dst = !(!src0 | !src1)' to sanity test two core emulation API functions:
 * - drmgr_insert_emulation_start()
 * - drmgr_insert_emulation_end()
 *
 * Note: This emulation client is for AArch64 and x86_64 only:
 * XXX i#3173 Improve testing of emulation API functions
 */

#include "dr_api.h"
#include "drmgr.h"
#include "client_tools.h"
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define PRE instrlist_preinsert

/* These are atomically incremented for precise counts. */
static int count_emulated_fully;
static int count_emulated_partly;
static int count_record_instr_orig;
static int count_record_instr_unchanged;
static int count_record_data_orig;
static int count_record_data_derived;
static int count_record_data_unchanged;

static ptr_uint_t derived_marker;

/* Event handlers. */
static void
event_exit(void);

static dr_emit_flags_t
event_instruction_change(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                         bool translating);
static dr_emit_flags_t
event_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
               bool translating, OUT void **user_data);

static dr_emit_flags_t
event_insertion(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                bool for_trace, bool translating, void *user_data);

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    if (!drmgr_init())
        DR_ASSERT(false);

    dr_register_exit_event(event_exit);

    if (!drmgr_register_bb_app2app_event(event_instruction_change, NULL))
        DR_ASSERT(false);
    if (!drmgr_register_bb_instrumentation_event(event_analysis, event_insertion, NULL))
        DR_ASSERT(false);
    derived_marker = drmgr_reserve_note_range(1);
    DR_ASSERT(derived_marker != DRMGR_NOTE_NONE);
}

static void
event_exit(void)
{
    DR_ASSERT(dr_atomic_load32(&count_emulated_fully) > 0);
    DR_ASSERT(dr_atomic_load32(&count_emulated_partly) > 0);
    DR_ASSERT(dr_atomic_load32(&count_record_instr_orig) > 0);
    DR_ASSERT(dr_atomic_load32(&count_record_instr_unchanged) > 0);
    DR_ASSERT(dr_atomic_load32(&count_record_data_orig) > 0);
    DR_ASSERT(dr_atomic_load32(&count_record_data_derived) > 0);
    DR_ASSERT(dr_atomic_load32(&count_record_data_unchanged) > 0);
#if VERBOSE
    dr_fprintf(STDERR, "Found and emulated %d instructions fully, %d partly\n",
               dr_atomic_load32(&count_emulated_fully),
               dr_atomic_load32(&count_emulated_partly));
#endif
    if (!drmgr_unregister_bb_app2app_event(event_instruction_change) ||
        !drmgr_unregister_bb_instrumentation_event(event_analysis))
        DR_ASSERT(false);
    drmgr_exit();
}

static bool
should_fully_emulate_instr(instr_t *instr)
{
    /* We replace 'dst = src0 & src1' with 'dst = !(!src0 | !src1)'. We only
     * transform AND instructions with unshifted, 64 bit register operands.
     */
    if (instr_get_opcode(instr) != OP_and)
        return false;
    opnd_t src0 = instr_get_src(instr, 0);
#ifdef AARCH64
    if (instr_num_srcs(instr) != 4 || opnd_get_size(src0) != OPSZ_8 ||
        opnd_get_immed_int(instr_get_src(instr, 3)) != 0)
        return false;
#elif defined(X86_64)
    opnd_t dst = instr_get_dst(instr, 0);
    opnd_t src1 = instr_get_src(instr, 1);
    if (!opnd_same(src1, dst) || !opnd_is_reg(src0) || opnd_get_size(src0) != OPSZ_8)
        return false;
#else
#    error Architecture not supported.
#endif
    return true;
}

static void
emulate_fully(void *drcontext, instrlist_t *bb, instr_t *instr)
{
    dr_atomic_add32_return_sum(&count_emulated_fully, 1);

    opnd_t dst = instr_get_dst(instr, 0);
    opnd_t src0 = instr_get_src(instr, 0);
    opnd_t src1 = instr_get_src(instr, 1);
    app_pc raw_instr_pc = instr_get_app_pc(instr);

    /* Insert emulation label to signal start of emulation code sequence. The
     * label is loaded with data about the instruction being emulated for use by
     * an observational client.
     */
    emulated_instr_t emulated_instr;
    emulated_instr.size = sizeof(emulated_instr);
    emulated_instr.pc = raw_instr_pc;
    emulated_instr.instr = instr;
    emulated_instr.flags = 0;
    drmgr_insert_emulation_start(drcontext, bb, instr, &emulated_instr);
    instr_t *start_derived = INSTR_CREATE_label(drcontext);
    instrlist_meta_preinsert(bb, instr, start_derived);

#ifdef AARCH64
    dr_save_reg(drcontext, bb, instr, DR_REG_X26, SPILL_SLOT_1);
    dr_save_reg(drcontext, bb, instr, DR_REG_X27, SPILL_SLOT_2);

    opnd_t scratch0 = opnd_create_reg(DR_REG_X26);
    opnd_t scratch1 = opnd_create_reg(DR_REG_X27);

    /* scratch0 = !src0
     * XXX i#2440 AArch64 missing INSTR_CREATE macros (INSTR_CREATE_orn)
     */
    PRE(bb, instr,
        INSTR_XL8(instr_create_1dst_4src(drcontext, OP_orn, scratch0,
                                         opnd_create_reg(DR_REG_XZR), src0,
                                         OPND_CREATE_LSL(), OPND_CREATE_INT(0)),
                  raw_instr_pc));

    /* scratch1 = !src1
     * XXX i#2440 AArch64 missing INSTR_CREATE macros (INSTR_CREATE_orn)
     */
    PRE(bb, instr,
        INSTR_XL8(instr_create_1dst_4src(drcontext, OP_orn, scratch1,
                                         opnd_create_reg(DR_REG_XZR), src1,
                                         OPND_CREATE_LSL(), OPND_CREATE_INT(0)),
                  raw_instr_pc));

    /* scratch0 = scratch0 | scratch1
     * XXX i#2440 AArch64 missing INSTR_CREATE macros (INSTR_CREATE_orr)
     */
    PRE(bb, instr,
        INSTR_XL8(instr_create_1dst_4src(drcontext, OP_orr, scratch0, scratch0, scratch1,
                                         OPND_CREATE_LSL(), OPND_CREATE_INT(0)),
                  raw_instr_pc));

    /* dst = !scratch0
     * XXX i#2440 AArch64 missing INSTR_CREATE macros (INSTR_CREATE_orn)
     */
    PRE(bb, instr,
        INSTR_XL8(instr_create_1dst_4src(drcontext, OP_orn, dst,
                                         opnd_create_reg(DR_REG_XZR), scratch0,
                                         OPND_CREATE_LSL(), OPND_CREATE_INT(0)),
                  raw_instr_pc));

    dr_restore_reg(drcontext, bb, instr, DR_REG_X26, SPILL_SLOT_1);
    dr_restore_reg(drcontext, bb, instr, DR_REG_X27, SPILL_SLOT_2);
#elif defined(X86_64)
    /*  and rax, rdx
     * =>
     *  not rax
     *  <spill rdx>
     *  not rdx
     *  or rax, rdx
     *  <restore rdx>
     *  not rax
     *  test rax, rax   (to set SF,ZF,PF and clear OF,CF)
     */
    PRE(bb, instr, INSTR_XL8(INSTR_CREATE_not(drcontext, dst), raw_instr_pc));
    dr_save_reg(drcontext, bb, instr, opnd_get_reg(src0), SPILL_SLOT_1);
    PRE(bb, instr, INSTR_XL8(INSTR_CREATE_not(drcontext, src0), raw_instr_pc));
    PRE(bb, instr, INSTR_XL8(INSTR_CREATE_or(drcontext, dst, src0), raw_instr_pc));
    dr_restore_reg(drcontext, bb, instr, opnd_get_reg(src0), SPILL_SLOT_1);
    PRE(bb, instr, INSTR_XL8(INSTR_CREATE_not(drcontext, dst), raw_instr_pc));
    PRE(bb, instr, INSTR_XL8(INSTR_CREATE_test(drcontext, dst, dst), raw_instr_pc));
#else
#    error Architecture not supported.
#endif
    for (instr_t *added = start_derived; added != instr; added = instr_get_next(added)) {
        instr_set_note(added, (void *)derived_marker);
    }

    /* Insert emulation label to signal end of emulation code sequence and
     * remove the instruction being emulated from the basic block.
     */
    drmgr_insert_emulation_end(drcontext, bb, instr);
    instrlist_remove(bb, instr);
}

/* We perform same-data different-instr emulation to test DR_EMULATE_INSTR_ONLY. */
static bool
should_partly_emulate_instr(instr_t *instr)
{
#ifdef AARCH64
    /* We replace "ldr reg, [base]" with "ldr reg, [base, XZR, LSL #3]". */
    if (instr_get_opcode(instr) != OP_ldr || instr_num_srcs(instr) != 1 ||
        instr_num_dsts(instr) != 1)
        return false;
    opnd_t dst = instr_get_dst(instr, 0);
    opnd_t src = instr_get_src(instr, 0);
    if (!opnd_is_reg(dst) || !opnd_is_near_base_disp(src))
        return false;
    if (opnd_get_base(src) == DR_REG_NULL || opnd_get_index(src) != DR_REG_NULL ||
        opnd_get_base(src) == DR_REG_XSP || opnd_get_disp(src) != 0)
        return false;
#elif defined(X86_64)
    /* We replace "mov reg, disp(base)" with "mov reg, disp(,index,1)". */
    if (instr_get_opcode(instr) != OP_mov_ld || instr_num_srcs(instr) != 1 ||
        instr_num_dsts(instr) != 1)
        return false;
    opnd_t dst = instr_get_dst(instr, 0);
    opnd_t src = instr_get_src(instr, 0);
    if (!opnd_is_reg(dst) || !opnd_is_near_base_disp(src))
        return false;
    if (opnd_get_base(src) == DR_REG_NULL || opnd_get_index(src) != DR_REG_NULL ||
        opnd_get_base(src) == DR_REG_XSP)
        return false;
#else
#    error Architecture not supported.
#endif
    return true;
}

static void
emulate_partly(void *drcontext, instrlist_t *bb, instr_t *instr)
{
    dr_atomic_add32_return_sum(&count_emulated_partly, 1);

    app_pc raw_instr_pc = instr_get_app_pc(instr);

    emulated_instr_t emulated_instr;
    emulated_instr.size = sizeof(emulated_instr);
    emulated_instr.pc = raw_instr_pc;
    emulated_instr.instr = instr;
    emulated_instr.flags = DR_EMULATE_INSTR_ONLY;
    drmgr_insert_emulation_start(drcontext, bb, instr, &emulated_instr);
    instr_t *start_derived = INSTR_CREATE_label(drcontext);
    instrlist_meta_preinsert(bb, instr, start_derived);

#ifdef AARCH64
    /*  ldr reg, [base]
     * =>
     *  ldr reg, [base, XZR, LSL #3]
     */
    opnd_t dst = instr_get_dst(instr, 0);
    opnd_t src = instr_get_src(instr, 0);
    PRE(bb, instr,
        INSTR_XL8(INSTR_CREATE_ldr(drcontext, dst,
                                   opnd_create_base_disp_aarch64(
                                       opnd_get_base(src), DR_REG_XZR, DR_EXTEND_UXTX,
                                       true, opnd_get_disp(src), 0, opnd_get_size(src))),
                  raw_instr_pc));
#elif defined(X86_64)
    /*  mov reg, disp(base)
     * =>
     *  mov reg, disp(,index,1)
     */
    opnd_t dst = instr_get_dst(instr, 0);
    opnd_t src = instr_get_src(instr, 0);
    PRE(bb, instr,
        INSTR_XL8(INSTR_CREATE_mov_ld(
                      drcontext, dst,
                      opnd_create_base_disp(DR_REG_NULL, opnd_get_base(src), 1,
                                            opnd_get_disp(src), opnd_get_size(src))),
                  raw_instr_pc));
#else
#    error Architecture not supported.
#endif
    for (instr_t *added = start_derived; added != instr; added = instr_get_next(added)) {
        instr_set_note(added, (void *)derived_marker);
    }

    /* Insert emulation label to signal end of emulation code sequence and
     * remove the instruction being emulated from the basic block.
     */
    drmgr_insert_emulation_end(drcontext, bb, instr);
    instrlist_remove(bb, instr);
}

static dr_emit_flags_t
event_instruction_change(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                         bool translating)
{
    instr_t *instr, *next_instr;

    for (instr = instrlist_first_app(bb); instr != NULL; instr = next_instr) {
        /* We're deleting some instrs, so get the next first. */
        next_instr = instr_get_next_app(instr);

        if (should_fully_emulate_instr(instr))
            emulate_fully(drcontext, bb, instr);
        if (should_partly_emulate_instr(instr))
            emulate_partly(drcontext, bb, instr);
    }

    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
               bool translating, OUT void **user_data)
{
    bool in_emulation = false;
    for (instr_t *instr = instrlist_first(bb); instr != NULL;
         instr = instr_get_next(instr)) {
        if (drmgr_is_emulation_start(instr)) {
            emulated_instr_t emulated_instr = {
                0,
            };
            emulated_instr.size = sizeof(emulated_instr);
            CHECK(drmgr_get_emulated_instr_data(instr, &emulated_instr),
                  "drmgr_get_emulated_instr_data() failed");
            in_emulation = true;
            continue;
        }
        /* drmgr_in_emulation_region() only works in the insertion phase so we
         * can't compare here.
         */
        if (drmgr_is_emulation_end(instr)) {
            in_emulation = false;
        }
    }
    return DR_EMIT_DEFAULT;
}

static void
record_instr_fetch_orig(instr_t *instr)
{
    DR_ASSERT(should_fully_emulate_instr(instr) || should_partly_emulate_instr(instr));
    DR_ASSERT((ptr_uint_t)instr_get_note(instr) != derived_marker);
    dr_atomic_add32_return_sum(&count_record_instr_orig, 1);
    dr_atomic_add32_return_sum(&count_record_instr_unchanged, 1);
    dr_atomic_add32_return_sum(&count_record_data_orig, 1);
    dr_atomic_add32_return_sum(&count_record_data_derived, 1);
    dr_atomic_add32_return_sum(&count_record_data_unchanged, 1);
}

static void
record_instr_fetch_unchanged(instr_t *instr)
{
    DR_ASSERT(!should_partly_emulate_instr(instr) && !should_fully_emulate_instr(instr));
    DR_ASSERT((ptr_uint_t)instr_get_note(instr) != derived_marker);
}

static void
record_data_addresses_orig(instr_t *instr)
{
    DR_ASSERT(should_fully_emulate_instr(instr) && !should_partly_emulate_instr(instr));
    DR_ASSERT((ptr_uint_t)instr_get_note(instr) != derived_marker);
}

static void
record_data_addresses_derived(instr_t *instr)
{
    /* For the "partly" case, we should *not* see the original instr
     * here but instead the replacement.
     */
    DR_ASSERT(!should_partly_emulate_instr(instr) && !should_fully_emulate_instr(instr));
    DR_ASSERT((ptr_uint_t)instr_get_note(instr) == derived_marker);
}

static void
record_data_addresses_unchanged(instr_t *instr)
{
    DR_ASSERT(!should_partly_emulate_instr(instr) && !should_fully_emulate_instr(instr));
    DR_ASSERT((ptr_uint_t)instr_get_note(instr) != derived_marker);
}

static dr_emit_flags_t
event_insertion(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                bool for_trace, bool translating, void *user_data)
{
    /* Use the recommended emulation instrumentation code pattern: */
    const emulated_instr_t *emulation;
    if (drmgr_in_emulation_region(drcontext, &emulation)) {
        if (TEST(DR_EMULATE_IS_FIRST_INSTR, emulation->flags)) {
            DR_ASSERT(drmgr_orig_app_instr_for_fetch(drcontext) == emulation->instr);
            record_instr_fetch_orig(emulation->instr);
            if (!TEST(DR_EMULATE_INSTR_ONLY, emulation->flags)) {
                DR_ASSERT(drmgr_orig_app_instr_for_operands(drcontext) ==
                          emulation->instr);
                record_data_addresses_orig(emulation->instr);
            }
        } else {
            /* Else skip further instr fetches until outside emulation region. */
            DR_ASSERT(drmgr_orig_app_instr_for_fetch(drcontext) == NULL);
        }
        if (instr_is_app(inst) && TEST(DR_EMULATE_INSTR_ONLY, emulation->flags)) {
            DR_ASSERT(drmgr_orig_app_instr_for_operands(drcontext) == inst);
            record_data_addresses_derived(inst);
        } else if (!TEST(DR_EMULATE_IS_FIRST_INSTR, emulation->flags) ||
                   TEST(DR_EMULATE_INSTR_ONLY, emulation->flags)) {
            DR_ASSERT(drmgr_orig_app_instr_for_operands(drcontext) == NULL);
        }
    } else if (instr_is_app(inst)) {
        DR_ASSERT(drmgr_orig_app_instr_for_fetch(drcontext) == inst);
        record_instr_fetch_unchanged(inst);
        DR_ASSERT(drmgr_orig_app_instr_for_operands(drcontext) == inst);
        record_data_addresses_unchanged(inst);
    } else {
        DR_ASSERT(drmgr_orig_app_instr_for_fetch(drcontext) == NULL);
        DR_ASSERT(drmgr_orig_app_instr_for_operands(drcontext) == NULL);
    }
    return DR_EMIT_DEFAULT;
}
