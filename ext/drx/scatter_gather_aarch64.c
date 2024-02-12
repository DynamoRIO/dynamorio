/* **********************************************************
 * Copyright (c) 2013-2023 Google, Inc.   All rights reserved.
 * Copyright (c) 2023      Arm Limited.   All rights reserved.
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

/* DynamoRio eXtension utilities: Support for expanding AArch64 scatter and gather
 * instructions.
 */

#include "dr_api.h"
#include "drx.h"
#include "drmgr.h"
#include "drreg.h"
#include "../ext_utils.h"
#include "scatter_gather_shared.h"

#include <stddef.h> /* for offsetof */

/* Control printing of verbose debugging messages. */
#define VERBOSE 0

typedef struct _per_thread_t {
    /* TODO i#3844: drreg does not support spilling predicate/vector regs yet,
     * so we do it ourselves.
     */

    void *scratch_pred_spill_slots;       /* Storage for spilled predicate registers. */
    size_t scratch_pred_spill_slots_size; /* Size of scratch_pred_spill_slots in bytes. */

    void *scratch_vector_spill_slots;       /* Storage for spilled vector registers. */
    size_t scratch_vector_spill_slots_size; /* Size of scratch_vector_spill_slots in
                                               bytes. */

    void *scratch_vector_spill_slots_aligned; /* Aligned ptr inside
                                                scratch_vector_spill_slots to save/restore
                                                spilled Z vector registers. */
} per_thread_t;

/* Track the state of manual spill slots for SVE registers.
 * This corresponds to the spill slot storage in per_thread_t.
 */
typedef struct _spill_slot_state_t {
#define NUM_PRED_SLOTS 2
    reg_id_t pred_slots[NUM_PRED_SLOTS];

#define NUM_VECTOR_SLOTS 1
    reg_id_t vector_slots[NUM_VECTOR_SLOTS];
} spill_slot_state_t;

void
init_spill_slot_state(DR_PARAM_OUT spill_slot_state_t *spill_slot_state)
{
    for (size_t i = 0; i < NUM_PRED_SLOTS; i++)
        spill_slot_state->pred_slots[i] = DR_REG_NULL;

    for (size_t i = 0; i < NUM_VECTOR_SLOTS; i++)
        spill_slot_state->vector_slots[i] = DR_REG_NULL;
}

void
drx_scatter_gather_thread_init(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)dr_thread_alloc(drcontext, sizeof(*pt));

    const uint vl_bytes = proc_get_vector_length_bytes();
    const uint pl_bytes = vl_bytes / 8; /* Predicate register size */

    /*
     * The instructions we use to load/store the spilled predicate register require
     * the base address to be aligned to 2 bytes:
     *     LDR <Pt>, [<Xn|SP>{, #<imm>, MUL VL}]
     *     STR <Pt>, [<Xn|SP>{, #<imm>, MUL VL}]
     * and dr_thread_alloc() guarantees allocated memory is aligned to the pointer size
     * (8 bytes) so we shouldn't have to do any further alignment.
     */
    static const size_t predicate_alignment_bytes = 2;
    pt->scratch_pred_spill_slots_size = pl_bytes * NUM_PRED_SLOTS;

    pt->scratch_pred_spill_slots =
        dr_thread_alloc(drcontext, pt->scratch_pred_spill_slots_size);
    DR_ASSERT_MSG(ALIGNED(pt->scratch_pred_spill_slots, predicate_alignment_bytes),
                  "scratch_pred_spill_slots is misaligned");

    /*
     * The scalable vector versions of LDR/STR require 16 byte alignment so we have to
     * over-allocate and get an aligned pointer inside the allocated memory.
     */
    static const size_t vector_alignment_bytes = 16;
    pt->scratch_vector_spill_slots_size =
        (vl_bytes * NUM_VECTOR_SLOTS) + (vector_alignment_bytes - 1);

    pt->scratch_vector_spill_slots =
        dr_thread_alloc(drcontext, pt->scratch_vector_spill_slots_size);
    pt->scratch_vector_spill_slots_aligned =
        (void *)ALIGN_FORWARD(pt->scratch_vector_spill_slots, vector_alignment_bytes);

    drmgr_set_tls_field(drcontext, drx_scatter_gather_tls_idx, (void *)pt);
}

void
drx_scatter_gather_thread_exit(void *drcontext)
{
    per_thread_t *pt =
        (per_thread_t *)drmgr_get_tls_field(drcontext, drx_scatter_gather_tls_idx);
    dr_thread_free(drcontext, pt->scratch_pred_spill_slots,
                   pt->scratch_pred_spill_slots_size);
    dr_thread_free(drcontext, pt->scratch_vector_spill_slots,
                   pt->scratch_vector_spill_slots_size);
    dr_thread_free(drcontext, pt, sizeof(*pt));
}

static void
get_scatter_gather_info(instr_t *instr, DR_PARAM_OUT scatter_gather_info_t *sg_info)
{
    DR_ASSERT_MSG(instr_is_scatter(instr) || instr_is_gather(instr),
                  "Instruction must be scatter or gather.");

    opnd_t dst0 = instr_get_dst(instr, 0);
    opnd_t src0 = instr_get_src(instr, 0);

    opnd_t memopnd;
    if (instr_is_scatter(instr)) {
        sg_info->is_load = false;
        sg_info->scatter_src_reg = opnd_get_reg(src0);
        sg_info->element_size = opnd_get_vector_element_size(src0);
        memopnd = dst0;
        sg_info->mask_reg = opnd_get_reg(instr_get_src(instr, instr_num_srcs(instr) - 1));
    } else {
        sg_info->is_load = true;
        sg_info->gather_dst_reg = opnd_get_reg(dst0);
        sg_info->element_size = opnd_get_vector_element_size(dst0);
        memopnd = src0;
        sg_info->mask_reg = opnd_get_reg(instr_get_src(instr, 1));
    }

    sg_info->base_reg = opnd_get_base(memopnd);
    sg_info->index_reg = opnd_get_index(memopnd);

    sg_info->disp = opnd_get_disp(memopnd);
    sg_info->extend =
        opnd_get_index_extend(memopnd, &sg_info->scaled, &sg_info->extend_amount);

    sg_info->scalar_value_size = opnd_get_size(memopnd);

    switch (instr_get_opcode(instr)) {
#define DRX_CASE(op, _reg_count, _is_scalar_value_signed, _faulting_behavior)           \
    case OP_##op:                                                                       \
        sg_info->reg_count = _reg_count;                                                \
        sg_info->is_scalar_value_signed = _is_scalar_value_signed;                      \
        sg_info->is_replicating = false;                                                \
        sg_info->faulting_behavior = _faulting_behavior;                                \
        /* The size of the vector in memory is:                                         \
         *     number_of_elements = (reg_count * vector_length) / element_size          \
         *     size = number_of_elements * value_size                                   \
         *          = (reg_count * vector_length / element_size) * value_size           \
         *          = (reg_count * vector_length * value_size) / element_size           \
         */                                                                             \
        sg_info->scatter_gather_size =                                                  \
            opnd_size_from_bytes((sg_info->reg_count * proc_get_vector_length_bytes() * \
                                  opnd_size_in_bytes(sg_info->scalar_value_size)) /     \
                                 opnd_size_in_bytes(sg_info->element_size));            \
        break

        DRX_CASE(ld1b, 1, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld1h, 1, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld1w, 1, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld1d, 1, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld1sb, 1, true, DRX_NORMAL_FAULTING);
        DRX_CASE(ld1sh, 1, true, DRX_NORMAL_FAULTING);
        DRX_CASE(ld1sw, 1, true, DRX_NORMAL_FAULTING);

        DRX_CASE(ldff1b, 1, false, DRX_FIRST_FAULTING);
        DRX_CASE(ldff1h, 1, false, DRX_FIRST_FAULTING);
        DRX_CASE(ldff1w, 1, false, DRX_FIRST_FAULTING);
        DRX_CASE(ldff1d, 1, false, DRX_FIRST_FAULTING);
        DRX_CASE(ldff1sb, 1, true, DRX_FIRST_FAULTING);
        DRX_CASE(ldff1sh, 1, true, DRX_FIRST_FAULTING);
        DRX_CASE(ldff1sw, 1, true, DRX_FIRST_FAULTING);

        DRX_CASE(ldnf1b, 1, false, DRX_NON_FAULTING);
        DRX_CASE(ldnf1h, 1, false, DRX_NON_FAULTING);
        DRX_CASE(ldnf1w, 1, false, DRX_NON_FAULTING);
        DRX_CASE(ldnf1d, 1, false, DRX_NON_FAULTING);
        DRX_CASE(ldnf1sb, 1, true, DRX_NON_FAULTING);
        DRX_CASE(ldnf1sh, 1, true, DRX_NON_FAULTING);
        DRX_CASE(ldnf1sw, 1, true, DRX_NON_FAULTING);

        DRX_CASE(ldnt1b, 1, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ldnt1h, 1, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ldnt1w, 1, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ldnt1d, 1, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ldnt1sb, 1, true, DRX_NORMAL_FAULTING);
        DRX_CASE(ldnt1sh, 1, true, DRX_NORMAL_FAULTING);
        DRX_CASE(ldnt1sw, 1, true, DRX_NORMAL_FAULTING);

        DRX_CASE(st1b, 1, false, DRX_NORMAL_FAULTING);
        DRX_CASE(st1h, 1, false, DRX_NORMAL_FAULTING);
        DRX_CASE(st1w, 1, false, DRX_NORMAL_FAULTING);
        DRX_CASE(st1d, 1, false, DRX_NORMAL_FAULTING);

        DRX_CASE(stnt1b, 1, false, DRX_NORMAL_FAULTING);
        DRX_CASE(stnt1h, 1, false, DRX_NORMAL_FAULTING);
        DRX_CASE(stnt1w, 1, false, DRX_NORMAL_FAULTING);
        DRX_CASE(stnt1d, 1, false, DRX_NORMAL_FAULTING);

        DRX_CASE(ld2b, 2, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld2h, 2, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld2w, 2, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld2d, 2, false, DRX_NORMAL_FAULTING);

        DRX_CASE(st2b, 2, false, DRX_NORMAL_FAULTING);
        DRX_CASE(st2h, 2, false, DRX_NORMAL_FAULTING);
        DRX_CASE(st2w, 2, false, DRX_NORMAL_FAULTING);
        DRX_CASE(st2d, 2, false, DRX_NORMAL_FAULTING);

        DRX_CASE(ld3b, 3, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld3h, 3, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld3w, 3, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld3d, 3, false, DRX_NORMAL_FAULTING);

        DRX_CASE(st3b, 3, false, DRX_NORMAL_FAULTING);
        DRX_CASE(st3h, 3, false, DRX_NORMAL_FAULTING);
        DRX_CASE(st3w, 3, false, DRX_NORMAL_FAULTING);
        DRX_CASE(st3d, 3, false, DRX_NORMAL_FAULTING);

        DRX_CASE(ld4b, 4, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld4h, 4, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld4w, 4, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld4d, 4, false, DRX_NORMAL_FAULTING);

        DRX_CASE(st4b, 4, false, DRX_NORMAL_FAULTING);
        DRX_CASE(st4h, 4, false, DRX_NORMAL_FAULTING);
        DRX_CASE(st4w, 4, false, DRX_NORMAL_FAULTING);
        DRX_CASE(st4d, 4, false, DRX_NORMAL_FAULTING);
#define DRX_CASE_REP(op, loaded_vector_size)               \
    case OP_##op:                                          \
        sg_info->reg_count = 1;                            \
        sg_info->scatter_gather_size = loaded_vector_size; \
        sg_info->is_scalar_value_signed = false;           \
        sg_info->is_replicating = true;                    \
        sg_info->faulting_behavior = DRX_NORMAL_FAULTING;  \
        break

        DRX_CASE_REP(ld1rob, OPSZ_32);

        DRX_CASE_REP(ld1rqb, OPSZ_16);
        DRX_CASE_REP(ld1rqh, OPSZ_16);
        DRX_CASE_REP(ld1rqw, OPSZ_16);
        DRX_CASE_REP(ld1rqd, OPSZ_16);
#undef DRX_CASE
#undef DRX_CASE_REP

    default: DR_ASSERT_MSG(false, "Invalid scatter/gather instruction");
    }

    DR_ASSERT(sg_info->mask_reg >= DR_REG_P0 && sg_info->mask_reg <= DR_REG_P15);
}

/* Get the number of elements per register in a scatter/gather instruction. */
static uint
get_number_of_elements(const scatter_gather_info_t *sg_info)
{
    const uint bytes_transferred_per_register =
        opnd_size_in_bytes(sg_info->scatter_gather_size) / sg_info->reg_count;
    return bytes_transferred_per_register /
        opnd_size_in_bytes(sg_info->scalar_value_size);
}

/* Get the nth register in a multi-register range.
 * For example:
 *   get_register_at_index(DR_REG_Z0, 0) -> DR_REG_0
 *   get_register_at_index(DR_REG_Z0, 1) -> DR_REG_1
 *   get_register_at_index(DR_REG_Z0, 2) -> DR_REG_2
 *   get_register_at_index(DR_REG_Z0, 3) -> DR_REG_3
 *   get_register_at_index(DR_REG_Z30, 0) -> DR_REG_30
 *   get_register_at_index(DR_REG_Z30, 1) -> DR_REG_31
 *   get_register_at_index(DR_REG_Z30, 2) -> DR_REG_0
 *   get_register_at_index(DR_REG_Z30, 3) -> DR_REG_1
 */
static reg_id_t
get_register_at_index(reg_id_t start, uint index)
{

    return (((start - DR_REG_Z0) + index) % DR_NUM_SIMD_VECTOR_REGS) + DR_REG_Z0;
}

/* Gather together some variables commonly used in the expansion functions to make them
 * easier to pass around.
 */
typedef struct {
    void *drcontext;
    instrlist_t *bb;    /* The basic block to write the expanded sequence to. */
    instr_t *sg_instr;  /* The instruction we are expanding. */
    app_pc orig_app_pc; /* The PC of instruction being expanded. */
} sg_emit_context_t;

#define EMIT(sg_context, op, ...)                                                        \
    instrlist_preinsert(sg_context->bb, sg_context->sg_instr,                            \
                        INSTR_XL8(INSTR_CREATE_##op(sg_context->drcontext, __VA_ARGS__), \
                                  sg_context->orig_app_pc))

/* Emit code to clear all inactive elements of a load's destination registers */
static void
emit_clear_inactive_dst_elements(const sg_emit_context_t *sg_context,
                                 const scatter_gather_info_t *sg_info,
                                 reg_id_t scratch_pred)
{
    DR_ASSERT(sg_info->is_load);

    for (uint reg_index = 0; reg_index < sg_info->reg_count; reg_index++) {
        const reg_id_t vector_dst =
            get_register_at_index(sg_info->gather_dst_reg, reg_index);

        if ((sg_info->base_reg == vector_dst) || (sg_info->index_reg == vector_dst)) {
            /* The dst register is also the base/index register so we need to preserve
             * the value of the active elements so we can use them in the address
             * calculation. We do this by CPYing a 0 value into the dst register using
             * the inverse of the mask_reg as the governing predicate.
             */

            /* ptrue    scratch_pred.b */
            EMIT(sg_context, ptrue_sve,
                 opnd_create_reg_element_vector(scratch_pred, OPSZ_1),
                 opnd_create_immed_pred_constr(DR_PRED_CONSTR_ALL));

            /* not      scratch_pred.b, scratch_pred/z, mask_reg.b */
            EMIT(sg_context, not_sve_pred_b,
                 opnd_create_reg_element_vector(scratch_pred, OPSZ_1),
                 opnd_create_predicate_reg(scratch_pred, false),
                 opnd_create_reg_element_vector(sg_info->mask_reg, OPSZ_1));

            /* cpy      vector_dst.element_size, scratch_pred/m, #0, lsl #0 */
            EMIT(sg_context, cpy_sve_shift_pred,
                 opnd_create_reg_element_vector(vector_dst, sg_info->element_size),
                 opnd_create_predicate_reg(scratch_pred, true), OPND_CREATE_INT8(0),
                 opnd_create_immed_uint(0, OPSZ_1b));
        } else {
            /* We don't care about any values in the dst register so zero the whole
             * thing.
             */

            /* dup      vector_dst.size, #0, lsl #0 */
            EMIT(sg_context, dup_sve_shift,
                 opnd_create_reg_element_vector(vector_dst, sg_info->element_size),
                 OPND_CREATE_INT8(0), opnd_create_immed_uint(0, OPSZ_1b));
        }
    }
}

static void
emit_init_active_element_loop(const sg_emit_context_t *sg_context,
                              const scatter_gather_info_t *sg_info, reg_id_t scratch_pred)
{
    /* pfalse   scratch_pred.b */
    EMIT(sg_context, pfalse_sve, opnd_create_reg_element_vector(scratch_pred, OPSZ_1));
}

static void
emit_advance_to_next_active_element(const sg_emit_context_t *sg_context,
                                    const scatter_gather_info_t *sg_info,
                                    reg_id_t scratch_pred, instr_t *end_label)
{

    /* pnext scratch_pred.element_size, mask_reg, scratch_pred.element_size */
    EMIT(sg_context, pnext_sve,
         opnd_create_reg_element_vector(scratch_pred, sg_info->element_size),
         opnd_create_reg(sg_info->mask_reg));

    /* b.none   end */
    instrlist_preinsert(
        sg_context->bb, sg_context->sg_instr,
        INSTR_XL8(INSTR_PRED(INSTR_CREATE_bcond(sg_context->drcontext,
                                                opnd_create_instr(end_label)),
                             DR_PRED_SVE_NONE),
                  sg_context->orig_app_pc));
}

/* Emit code to move the element value from the element active in element_mask from
 * the vector src_reg to the scalar dst_reg.
 */
static void
emit_extract_current_element(const sg_emit_context_t *sg_context,
                             const scatter_gather_info_t *sg_info, reg_id_t dst_reg,
                             reg_id_t element_mask, reg_id_t src_reg)
{
    DR_ASSERT(reg_is_z(src_reg));
    DR_ASSERT(!reg_is_z(dst_reg));

    /* lastb    dst_reg, element_mask, src_reg.element_size */
    EMIT(sg_context, lastb_sve_scalar, opnd_create_reg(dst_reg),
         opnd_create_reg(element_mask),
         opnd_create_reg_element_vector(src_reg, sg_info->element_size));
}

/* Emit code to move the value from the scalar src_reg to the element of the vector
 * dst_reg active in element_mask.
 */
static void
emit_insert_current_element(const sg_emit_context_t *sg_context,
                            const scatter_gather_info_t *sg_info, reg_id_t dst_reg,
                            reg_id_t element_mask, reg_id_t src_reg)
{
    DR_ASSERT(!reg_is_z(src_reg));
    DR_ASSERT(reg_is_z(dst_reg));

    /* cpy      dst_reg.element_size, element_mask/m, src_reg */
    EMIT(sg_context, cpy_sve_pred,
         opnd_create_reg_element_vector(dst_reg, sg_info->element_size),
         opnd_create_predicate_reg(element_mask, /*merging=*/true),
         opnd_create_reg(reg_resize_to_opsz(src_reg, sg_info->element_size)));
}

static void
emit_scalar_load_or_store(const sg_emit_context_t *sg_context,
                          const scatter_gather_info_t *sg_info, reg_id_t base_reg,
                          reg_id_t index_reg, reg_id_t src_or_dst)
{
    opnd_t mem = opnd_create_base_disp_shift_aarch64(
        base_reg, index_reg, sg_info->extend, sg_info->scaled, sg_info->disp, /*flags=*/0,
        sg_info->scalar_value_size, sg_info->extend_amount);

#define CREATE(op, ...) INSTR_CREATE_##op(sg_context->drcontext, __VA_ARGS__)

    instr_t *ld_st_instr = NULL;
    if (sg_info->is_load) {
        /* ldr[bh]  scratch_gpr, [mem] */
        if (sg_info->is_scalar_value_signed) {
            const reg_id_t dst_wx = reg_resize_to_opsz(src_or_dst, sg_info->element_size);
            switch (sg_info->scalar_value_size) {
            case OPSZ_1: ld_st_instr = CREATE(ldrsb, opnd_create_reg(dst_wx), mem); break;
            case OPSZ_2: ld_st_instr = CREATE(ldrsh, opnd_create_reg(dst_wx), mem); break;
            case OPSZ_4: ld_st_instr = CREATE(ldrsw, opnd_create_reg(dst_wx), mem); break;
            }
        } else {
            const reg_id_t dst_x = src_or_dst;
            const reg_id_t dst_w = reg_resize_to_opsz(dst_x, OPSZ_4);
            switch (sg_info->scalar_value_size) {
            case OPSZ_1: ld_st_instr = CREATE(ldrb, opnd_create_reg(dst_w), mem); break;
            case OPSZ_2: ld_st_instr = CREATE(ldrh, opnd_create_reg(dst_w), mem); break;
            case OPSZ_4: ld_st_instr = CREATE(ldr, opnd_create_reg(dst_w), mem); break;
            case OPSZ_8: ld_st_instr = CREATE(ldr, opnd_create_reg(dst_x), mem); break;
            }
        }
    } else {
        DR_ASSERT_MSG(!sg_info->is_scalar_value_signed,
                      "Invalid scatter_gather_info_t data");
        const reg_id_t src_x = src_or_dst;
        const reg_id_t src_w = reg_resize_to_opsz(src_or_dst, OPSZ_4);

        /* str[bh]  src, [mem] */
        switch (sg_info->scalar_value_size) {
        case OPSZ_1: ld_st_instr = CREATE(strb, mem, opnd_create_reg(src_w)); break;
        case OPSZ_2: ld_st_instr = CREATE(strh, mem, opnd_create_reg(src_w)); break;
        case OPSZ_4: ld_st_instr = CREATE(str, mem, opnd_create_reg(src_w)); break;
        case OPSZ_8: ld_st_instr = CREATE(str, mem, opnd_create_reg(src_x)); break;
        }
    }
#undef CREATE

    DR_ASSERT_MSG(ld_st_instr != NULL, "Invalid scatter_gather_info_t data");

    if (ld_st_instr != NULL)
        scatter_gather_tag_expanded_ld_st(ld_st_instr);

    instrlist_preinsert(sg_context->bb, sg_context->sg_instr,
                        INSTR_XL8(ld_st_instr, sg_context->orig_app_pc));
}

static void
emit_load_store_current_element(const sg_emit_context_t *sg_context,
                                const scatter_gather_info_t *sg_info,
                                reg_id_t scratch_pred, reg_id_t base_reg,
                                reg_id_t index_reg, reg_id_t src_or_dst,
                                uint vector_reg_index)
{
    DR_ASSERT(vector_reg_index < sg_info->reg_count);

    if (!sg_info->is_load) {
        /* Copy the current active element of the vector src reg to scalar_src_or_dst
         */
        emit_extract_current_element(
            sg_context, sg_info, src_or_dst, scratch_pred,
            get_register_at_index(sg_info->scatter_src_reg, vector_reg_index));
    }

    /* Perform the scalar load/store for this element */
    emit_scalar_load_or_store(sg_context, sg_info, base_reg, index_reg, src_or_dst);

    if (sg_info->is_load) {
        /* Copy the loaded value into the current element of the vector dst reg */
        emit_insert_current_element(
            sg_context, sg_info,
            get_register_at_index(sg_info->gather_dst_reg, vector_reg_index),
            scratch_pred, src_or_dst);
    }
}

/*
 * Emit code to expand a scatter or gather instruction into a series of equivalent scalar
 * loads or stores.
 *
 * Scalar+vector:
 * These instructions either have scalar+vector memory operands or the form:
 *     [<Xn|SP>, <Zm>.<Ts>{, <mod>}]
 * where addresses to load/store each element are calculated by adding a base address
 * from the scalar register Xn, to an offset read from the corresponding element of the
 * vector index register Zm.
 * Before being the index value is optionally modified according to a modifier <mod>.
 * The valid modifiers depend on the instruction, but they include:
 *     lsl #<n> (left shift by n)
 *     sxtw #<n> (sign extend and left shift by n)
 *     uxtw #<n> (zero extend and left shift by n)
 *
 * or vector+immediate memory operands or the form:
 *     [<Zn>.<Ts>{, #<imm>}]
 * where addresses to load/store each element are calculated by adding an immediate offset
 * to a base address read from the corresponding element of the vector base register Zn.
 *
 * The emitted code roughly implements this algorithm:
 *     if (is_load)
 *         clear_inactive_elements(dst);
 *     for (e=first_active_element();
 *          active_elements_remain();
 *          e = next_active_element()) {
 *         if (is_load) {
 *             dst[e] = scalar_load(base, offsets[e], mod);
 *         } else {
 *             scalar_store(src[e], base, offsets[e], mod);
 *         }
 *     }
 * except we unroll the loop. Without unrolling the loop drmemtrace's instrumentation
 * would be repeated every iteration and give incorrect ifetch statistics.
 * (See i#4948 for more details)
 *
 * For example
 *     ld1d   (%x0,%z26.d,lsl #3)[32byte] %p1/z -> %z27.d
 * with a 256-bit vector length expands to:
 *
 * clear_inactive_elements:
 *       dup    $0x00 lsl $0x00 -> %z27.d       ; Clear dst register
 *       pfalse  -> %p0.b
 * handle_active_elements:
 *       pnext  %p1 %p0.d -> %p0.d              ; p0 = mask indicating first active
 *                                              ;      element of p1
 *                                              ; NOTE: This is the first *active*
 *                                              ; element which may or may not be
 *                                              ; element 0.
 *       b.none end                             ; if (no more active elements) goto end
 *       lastb  %p0 %z26.d -> %x1               ; extract offset for the current element
 *       ldr    (%x0,%x1,lsl #3)[8byte] -> %x1  ; perform the scalar load
 *       cpy    %p0/m %x1 -> %z27.d             ; cpy loaded value to dst element
 *       pnext  %p1 %p0.d -> %p0.d              ; Find the second active element (if any)
 *       b.none end
 *       lastb  %p0 %z26.d -> %x1
 *       ldr    (%x0,%x1,lsl #3)[8byte] -> %x1
 *       cpy    %p0/m %x1 -> %z27.d
 *       pnext  %p1 %p0.d -> %p0.d              ; Find the third active element (if any)
 *       b.none end
 *       lastb  %p0 %z26.d -> %x1
 *       ldr    (%x0,%x1,lsl #3)[8byte] -> %x1
 *       cpy    %p0/m %x1 -> %z27.d
 *       pnext  %p1 %p0.d -> %p0.d              ; Find the fourth active element (if any)
 *       b.none end
 *       lastb  %p0 %z26.d -> %x1
 *       ldr    (%x0,%x1,lsl #3)[8byte] -> %x1
 *       cpy    %p0/m %x1 -> %z27.d
 *   end:
 *       ...
 *
 * Vector+immediate:
 *
 * These instructions either have scalar+vector memory operands or the form:
 *     [<Zn>.<Ts>{, #<imm>}]
 * where addresses to load/store each element are calculated by adding an immediate offset
 * to a base address read from the corresponding element of the vector base register Zn.
 *
 * The emitted code roughly implements this algorithm:
 *     if (is_load)
 *         clear_inactive_elements(dst);
 *     for (e=first_active_element();
 *          active_elements_remain();
 *          e = next_active_element()) {
 *         if (is_load) {
 *             dst[e] = scalar_load(base[e], imm);
 *         } else {
 *             scalar_store(src[e], base[e], imm);
 *         }
 *     }
 * except we unroll the loop. Without unrolling the loop drmemtrace's instrumentation
 * would be repeated every iteration and give incorrect ifetch statistics.
 * (See i#4948 for more details)
 *
 * For example
 *     st1h   %z7.d %p2 -> +0x3e(%z23.d)[8byte]
 * with a 256-bit vector length expands to:
 *
 *       pfalse  -> %p0.b
 * handle_active_elements:
 *       pnext  %p2 %p0.d -> %p0.d              ; p0 = mask indicating first active
 *                                              ;      element of p1
 *                                              ; NOTE: This is the first *active*
 *                                              ; element which may or may not be
 *                                              ; element 0.
 *       b.none end                             ; if (no more active elements) goto end
 *       lastb  %p0 %z23.d -> %x0               ; extract offset for the current element
 *       lastb  %p0 %z7.d -> %x1                ; extract current element from src reg
 *       strh   %w1 -> +0x3e(%x0)[2byte]        ; perform the scalar store
 *       pnext  %p2 %p0.d -> %p0.d              ; Find the second active element (if any)
 *       b.none end
 *       lastb  %p0 %z23.d -> %x0
 *       lastb  %p0 %z7.d -> %x1
 *       strh   %w1 -> +0x3e(%x0)[2byte]
 *       pnext  %p2 %p0.d -> %p0.d              ; Find the thrid active element (if any)
 *       b.none end
 *       lastb  %p0 %z23.d -> %x0
 *       lastb  %p0 %z7.d -> %x1
 *       strh   %w1 -> +0x3e(%x0)[2byte]
 *       pnext  %p2 %p0.d -> %p0.d              ; Find the fourth active element (if any)
 *       b.none end
 *       lastb  %p0 %z23.d -> %x0
 *       lastb  %p0 %z7.d -> %x1
 *       strh   %w1 -> +0x3e(%x0)[2byte]
 *   end:
 *       ...
 *
 * This function is also used in the expansion of predicated contiguous load/stores so it
 * needs to be able to handle multi-register operations, even though there are not any
 * multi-register scatter/gather instructions.
 */
static void
expand_scatter_gather(const sg_emit_context_t *sg_context,
                      const scatter_gather_info_t *sg_info, reg_id_t scalar_base,
                      reg_id_t scalar_index, reg_id_t scalar_src_or_dst,
                      reg_id_t scratch_pred)
{
    DR_ASSERT_MSG(
        !reg_is_z(scalar_base),
        "expand_scatter_gather: scalar_base register must be scalar X register");
    DR_ASSERT_MSG(
        !reg_is_z(scalar_index),
        "expand_scatter_gather: scalar_index register must be scalar X register");
    DR_ASSERT_MSG(!reg_is_z(scalar_index),
                  "expand_scatter_gather: scalar_index must be scalar X register");
    DR_ASSERT_MSG(!reg_is_z(scalar_src_or_dst),
                  "expand_scatter_gather: scalar_src_or_dst must be scalar X register");

    DR_ASSERT_MSG((scalar_src_or_dst != scalar_index) || sg_info->is_load,
                  "expand_scatter_gather: scalar_src and scalar_index registers must "
                  "not alias");

    const uint no_of_elements = get_number_of_elements(sg_info);

    if (sg_info->is_load) {
        /* First we deal with the inactive elements. Gather loads are always zeroing
         * so we need to set all inactive elements to 0.
         */
        emit_clear_inactive_dst_elements(sg_context, sg_info, scratch_pred);
    }

    emit_init_active_element_loop(sg_context, sg_info, scratch_pred);

    instr_t *end_label = INSTR_CREATE_label(sg_context->drcontext);

    for (uint i = 0; i < no_of_elements; i++) {

        /* Advance scratch_pred to the next active element in sg_info->mask_reg, or branch
         * to end_label if there are no more active elements.
         */
        emit_advance_to_next_active_element(sg_context, sg_info, scratch_pred, end_label);

        if (reg_is_z(sg_info->base_reg)) {
            /* Copy the current active element of the vector base reg to scalar_base */
            emit_extract_current_element(sg_context, sg_info, scalar_base, scratch_pred,
                                         sg_info->base_reg);
        }

        if (reg_is_z(sg_info->index_reg)) {
            /* Copy the current active element of the vector index reg to scalar_index */
            emit_extract_current_element(sg_context, sg_info, scalar_index, scratch_pred,
                                         sg_info->index_reg);
        }

        emit_load_store_current_element(sg_context, sg_info, scratch_pred, scalar_base,
                                        scalar_index, scalar_src_or_dst, 0);

        for (uint reg_index = 1; reg_index < sg_info->reg_count; reg_index++) {
            /* Increment the index value so the memory operand for the scalar
             * load/store we emit below points to the value for the next register.
             */
            EMIT(sg_context, add, opnd_create_reg(scalar_index),
                 opnd_create_reg(scalar_index), OPND_CREATE_INT(1));

            emit_load_store_current_element(sg_context, sg_info, scratch_pred,
                                            scalar_base, scalar_index, scalar_src_or_dst,
                                            reg_index);
        }
    }

    instrlist_meta_preinsert(sg_context->bb, sg_context->sg_instr, end_label);
}

/*
 * Predicated contiguous loads and stores.
 *
 * These instructions have memory operands of the form:
 *     [<Xn|SP>, <Xm>{, lsl #amount}] (scalar+scalar)
 * or
 *     [<Xn|SP>{, #imm, mul vl}] (scalar+immediate)
 *
 * The memory operands of these instructions essentially work like scalar memory operands.
 * Xn contains the base address to which we add an index either from the register Xm or an
 * immediate value. That gives the address to load/store for element 0 of the vector and
 * successive elements are loaded from/stored to successive addresses in memory.
 * Essentially, the address for each element e is calculated as:
 *
 *     base + index + (e * scalar_value_size)
 *
 * Contiguous accesses are expanded in a similar way to scalar+vector scatter/gather
 * accesses (see expand_scatter_gather() for details) with an extra step at the beginning.
 *
 * When we expand a scatter/gather instruction we use the pnext instruction to iterate
 * over the active elements in the governing predicate. The loop essentially works like
 * this:
 *
 *     mask = [0, 0, 0, ...]; // All elements start inactive.
 *     while (1) {
 *         mask = pnext(governing_predicate, mask);
 *         if (no_element_is_active(mask))
 *             break;
 *         ...
 *     }
 *
 * The key thing here is that our loop variable isn't an index we are incrementing, its
 * a 1-bit mask we are left-shifting.
 *
 * This works well for the true scatter/gather instructions because we can use the mask
 * to extract the current element from the vector index or base register to a scalar
 * register which we can use in a scalar load/store using the lastb instruction.
 *
 * Contiguous accesses don't have a vector we can extract from, so we need to create one.
 * Essentially we transform the contiguous operation into a scalar+vector scatter/gather
 * operation and expand that.
 * We do this by calculating the element 0 address and using that as the new base, and
 * generating a vector of element numbers to use as the vector index.
 *
 *     new_base = base + index
 *     new_indices = [0, 1, 2, 3, ...]
 *
 * Now each address can be calculated as:
 *
 *     new_base + (extract_active_element(new_indices, mask) * scalar_value_size)
 *
 * which can be expanded the same way as a regular scalar+vector scatter/gather operation.
 */

/*
 * Emit code to expand a scalar+scalar predicated contiguous load or store into a series
 * of equivalent scalar loads and stores.
 * These instructions have memory operands of the form:
 *     [<Xn|SP>, <Xm>{, lsl #amount}]
 *
 * The emitted code roughly implements this algorithm:
 *     new_base = base + (index lsl #amount)
 *     offsets = [i*reg_count for i in range(reg_count)]
 *     if (is_load)
 *         clear_inactive_elements(dst);
 *     for (e=first_active_element();
 *          active_elements_remain();
 *          e = next_active_element()) {
 *         first_reg_offset = offsets[e]=
 *         if (is_load) {
 *             for (i=0; i < reg_count; i++) {
 *                 dsts[(z1 + i) % 32][e] = scalar_load(new_base, first_reg_offset + i,
 *                                                      mod);
 *             }
 *         } else {
 *             for (i=0; i < reg_count; i++) {
 *                 scalar_store(srcs[(z1 + i) % 32][e], new_base, first_reg_offset + i,
 *                              mod);
 *             }
 *         }
 *     }
 * except we unroll the loops. Without unrolling the loop drmemtrace's instrumentation
 * would be repeated every iteration and give incorrect ifetch statistics.
 * (See i#4948 for more details)
 *
 * For example
 *     ld2h   (%x0,%x1,lsl #1)[64byte] %p6/z -> %z12.h %z13.h
 * with a 256-bit vector length expands to:
 *
 *       add    %x0 %x1 uxtx $0x01 -> %x4       ; Calculate new base
 *       index  $0x00 $0x02 -> %z0.h            ; Initialize index vector
 * clear_inactive_elements:
 *       dup    $0x00 lsl $0x00 -> %z12.h      ; Clear destination registers
 *       dup    $0x00 lsl $0x00 -> %z13.h
 *       pfalse  -> %p0.b
 * handle_active_elements:
 *       pnext  %p6 %p0.h -> %p0.h              ; p0 = mask indicating first active
 *                                              ;      element of p1
 *                                              ; NOTE: This is the first *active*
 *                                              ; element which may or may not be
 *                                              ; element 0.
 *       b.none end                             ; if (no more active elements) goto end
 *       lastb  %p0 %z0.h -> %x2                ; extract offset for the current element
 *       ldrh   (%x4,%x2,lsl #1)[2byte] -> %w3  ; scalar load for 1st dst register
 *       cpy    %p0/m %w3 -> %z12.h             ; cpy loaded value to dst element
 *       add    %x2 $0x01 lsl $0x0 -> %x2       ; increment index
 *       ldrh   (%x4,%x2,lsl #1)[2byte] -> %w3  ; scalar load for 2st dst register
 *       cpy    %p0/m %w3 -> %z13.h             ; cpy loaded value to dst element
 *       pnext  %p6 %p0.h -> %p0.h              ; Find the second active element (if any)
 *       b.none end
 *       lastb  %p0 %z0.h -> %x2
 *       ldrh   (%x4,%x2,lsl #1)[2byte] -> %w3
 *       cpy    %p0/m %w3 -> %z12.h
 *       add    %x2 $0x01 lsl $0x0 -> %x2
 *       ldrh   (%x4,%x2,lsl #1)[2byte] -> %w3
 *       cpy    %p0/m %w3 -> %z13.h
 *       ...                                    ; Repeat for all elements
 *   end:
 *       ...
 */
static void
expand_scalar_plus_scalar(const sg_emit_context_t *sg_context,
                          const scatter_gather_info_t *sg_info, reg_id_t new_base,
                          reg_id_t scalar_index, reg_id_t scalar_src_or_dst,
                          reg_id_t scratch_pred, reg_id_t governing_pred,
                          reg_id_t scratch_vec)
{
    DR_ASSERT_MSG(
        !reg_is_z(sg_info->base_reg),
        "expand_scalar_plus_scalar: base_reg register must be scalar X register");
    DR_ASSERT_MSG(
        !reg_is_z(sg_info->index_reg),
        "expand_scalar_plus_scalar: index_reg register must be scalar X register");
    DR_ASSERT_MSG(!reg_is_z(scalar_index),
                  "expand_scalar_plus_scalar: scalar_index must be scalar X register");
    DR_ASSERT_MSG(
        !reg_is_z(scalar_src_or_dst),
        "expand_scalar_plus_scalar: scalar_src_or_dst must be scalar X register");

    /* Calculate the new base address in new_base.
     * Note that we can't use drutil_insert_get_mem_addr() here because we don't want the
     * BSD licensed drx to have a dependency on the LGPL licensed drutil.
     */

    /* add      new_base, base_reg, index_reg, extend #extend_amount */
    EMIT(sg_context, add_extend, opnd_create_reg(new_base),
         opnd_create_reg(sg_info->base_reg), opnd_create_reg(sg_info->index_reg),
         OPND_CREATE_INT(sg_info->extend), OPND_CREATE_INT(sg_info->extend_amount));

    /* Populate the new vector index register, starting at 0 and incrementing by the
     * number of values which are accessed per-index. This is one value per register
     * accessed so the increment is the same as sg_info->regcount.
     */

    /* index    scratch_vec.element_size, #0, #reg_count */
    EMIT(sg_context, index_sve,
         opnd_create_reg_element_vector(scratch_vec, sg_info->element_size),
         /*starting value=*/opnd_create_immed_int(0, OPSZ_5b),
         /*increment=*/opnd_create_immed_int(sg_info->reg_count, OPSZ_5b));

    /* Create a new scatter_gather_info_t with the updated registers. */
    scatter_gather_info_t mod_sg_info = *sg_info;
    mod_sg_info.base_reg = new_base;
    mod_sg_info.index_reg = scratch_vec;
    mod_sg_info.disp = 0;
    mod_sg_info.mask_reg = governing_pred;

    /* Note that mod_sg_info might not describe a valid SVE instruction.
     * For example if we are expanding:
     *     ld1h z31.h, p0/z, [x0, x1, lsl #1]
     * The mod_sg_info might look like a theoretical instruction:
     *     ld1h z31.h, p0/z, [x2, z0.h, lsl #1]
     * which is not a valid SVE instruction (scatter/gather instructions only support
     * S and D element sizes).
     * It doesn't matter that this theoretical instruction does not exist;
     * expand_scatter_gather() is able to generate a sequence of valid instructions that
     * carry out the described operation correctly anyway.
     */

    /* Expand the instruction as if it were a scalar+vector scatter/gather instruction */

    expand_scatter_gather(sg_context, &mod_sg_info, new_base, scalar_index,
                          scalar_src_or_dst, scratch_pred);
}

/*
 * Emit code to expand a scalar+immediate predicated contiguous load or store into a
 * series of equivalent scalar loads and stores.
 * These instructions have memory operands of the form:
 *     [<Xn|SP>{, #imm, mul vl}]
 *
 * The emitted code roughly implements this algorithm:
 *     new_base = base + (imm * vl)
 *     offsets = [i*reg_count for i in range(reg_count)]
 *     if (is_load)
 *         clear_inactive_elements(dst);
 *     for (e=first_active_element();
 *          active_elements_remain();
 *          e = next_active_element()) {
 *         first_reg_offset = offsets[e]=
 *         if (is_load) {
 *             for (i=0; i < reg_count; i++) {
 *                 dsts[(z1 + i) % 32][e] = scalar_load(new_base, first_reg_offset + i,
 *                                                      mod);
 *             }
 *         } else {
 *             for (i=0; i < reg_count; i++) {
 *                 scalar_store(srcs[(z1 + i) % 32][e], new_base, first_reg_offset + i,
 *                              mod);
 *             }
 *         }
 *     }
 * except we unroll the loops. Without unrolling the loop drmemtrace's instrumentation
 * would be repeated every iteration and give incorrect ifetch statistics.
 * (See i#4948 for more details)
 *
 * For example
 *     st4b   %z24.b %z25.b %z26.b %z27.b %p3 -> -0x0280(%x0)[128byte]
 * with a 256-bit vector length expands to:
 *
 *       sub    %x0 $0x0280 lsl $0x0 -> %x3     : Calculate new base
 *       index  $0x00 $0x04 -> %z0.b            ; Initialize index vector
 *       pfalse  -> %p0.b
 * handle_active_elements:
 *       pnext  %p3 %p0.b -> %p0.b              ; p0 = mask indicating first active
 *                                              ;      element of p1
 *                                              ; NOTE: This is the first *active*
 *                                              ; element which may or may not be
 *                                              ; element 0.
 *       b.none end                             ; if (no more active elements) goto end
 *       lastb  %p0 %z0.b -> %x1                ; extract offset for the current element
 *       lastb  %p0 %z24.b -> %x2               ; extract value from the first src reg
 *       strb   %w2 -> (%x3,%x1,uxtw #0)[1byte] ; scalar store for first value
 *       add    %x1 $0x01 lsl $0x00 -> %x1      ; increment index
 *       lastb  %p0 %z25.b -> %x2               ; extract value from the second src reg
 *       strb   %w2 -> (%x3,%x1,uxtw #0)[1byte] ; scalar store for second value
 *       add    %x1 $0x01 lsl $0x00 -> %x1      ; increment index
 *       lastb  %p0 %z26.b -> %x2               ; extract value from the third src reg
 *       strb   %w2 -> (%x3,%x1,uxtw #0)[1byte] ; scalar store for third value
 *       add    %x1 $0x01 lsl $0x00 -> %x1      ; increment index
 *       lastb  %p0 %z27.b -> %x2               ; extract value from the fourth src reg
 *       strb   %w2 -> (%x3,%x1,uxtw #0)[1byte] ; scalar store for fourth value
 *       pnext  %p3 %p0.b -> %p0.b              ; Find the second active element (if any)
 *       b.none end
 *       lastb  %p0 %z0.b -> %x1
 *       lastb  %p0 %z24.b -> %x2
 *       strb   %w2 -> (%x3,%x1,uxtw #0)[1byte]
 *       add    %x1 $0x01 lsl $0x00 -> %x1
 *       lastb  %p0 %z25.b -> %x2
 *       strb   %w2 -> (%x3,%x1,uxtw #0)[1byte]
 *       add    %x1 $0x01 lsl $0x00 -> %x1
 *       lastb  %p0 %z26.b -> %x2
 *       strb   %w2 -> (%x3,%x1,uxtw #0)[1byte]
 *       add    %x1 $0x01 lsl $0x00 -> %x1
 *       lastb  %p0 %z27.b -> %x2
 *       strb   %w2 -> (%x3,%x1,uxtw #0)[1byte]
 *       ...                                    ; Repeat for all elements
 *   end:
 *       ...
 */
static void
expand_scalar_plus_immediate(const sg_emit_context_t *sg_context,
                             const scatter_gather_info_t *sg_info, reg_id_t new_base,
                             reg_id_t scalar_index, reg_id_t scalar_src_or_dst,
                             reg_id_t scratch_pred, reg_id_t governing_pred,
                             reg_id_t scratch_vec)
{
    DR_ASSERT_MSG(
        !reg_is_z(sg_info->base_reg),
        "expand_scalar_plus_vector: base_reg register must be scalar X register");
    DR_ASSERT_MSG(
        !reg_is_z(sg_info->index_reg),
        "expand_scalar_plus_vector: index_reg register must be scalar X register");
    DR_ASSERT_MSG(!reg_is_z(scalar_index),
                  "expand_scalar_plus_vector: scalar_index must be scalar X register");
    DR_ASSERT_MSG(
        !reg_is_z(scalar_src_or_dst),
        "expand_scalar_plus_vector: scalar_src_or_dst must be scalar X register");

    /* Calculate the new base address in new_base.
     * Note that we can't use drutil_insert_get_mem_addr() here because we don't want the
     * BSD licensed drx to have a dependency on the LGPL licensed drutil.
     */

    DR_ASSERT(!sg_info->scaled);
    DR_ASSERT(sg_info->extend_amount == 0);

    if (sg_info->disp == 0) {
        /* The displacement is 0 so the original base register already contains the
         * base of the contiguous memory region.
         */
        new_base = sg_info->base_reg;
    } else if (sg_info->disp > 0) {
        /* add      new_base, base_reg, #disp */
        EMIT(sg_context, add, opnd_create_reg(new_base),
             opnd_create_reg(sg_info->base_reg), OPND_CREATE_INT(sg_info->disp));
    } else {
        /* sub      new_base, base_reg, #-disp */
        EMIT(sg_context, sub, opnd_create_reg(new_base),
             opnd_create_reg(sg_info->base_reg), OPND_CREATE_INT(-sg_info->disp));
    }

    /* Populate the new vector index register, starting at 0 and incrementing by the
     * number of values which are accessed per-index. This is one value per register
     * accessed so the increment is the same as sg_info->regcount.
     */

    /* index    scratch_vec.element_size, #0, #reg_count */
    EMIT(sg_context, index_sve,
         opnd_create_reg_element_vector(scratch_vec, sg_info->element_size),
         /*starting value=*/opnd_create_immed_int(0, OPSZ_5b),
         /*increment=*/opnd_create_immed_int(sg_info->reg_count, OPSZ_5b));

    /* Create a new scatter_gather_info_t with the updated registers. */
    scatter_gather_info_t mod_sg_info = *sg_info;
    mod_sg_info.base_reg = new_base;
    mod_sg_info.index_reg = scratch_vec;
    mod_sg_info.disp = 0;
    mod_sg_info.mask_reg = governing_pred;
    mod_sg_info.scaled = true;
    mod_sg_info.extend_amount = opnd_size_to_shift_amount(sg_info->scalar_value_size);
    mod_sg_info.extend = DR_EXTEND_UXTW;

    /* Note that mod_sg_info might not describe a valid SVE instruction.
     * For example if we are expanding:
     *     ld1h z31.h, p0/z, [x0, x1, lsl #1]
     * The mod_sg_info might look like a theoretical instruction:
     *     ld1h z31.h, p0/z, [x2, z0.h, lsl #1]
     * which is not a valid SVE instruction (scatter/gather instructions only support
     * S and D element sizes).
     * It doesn't matter that this theoretical instruction does not exist;
     * expand_scatter_gather() is able to generate a sequence of valid instructions that
     * carry out the described operation correctly anyway.
     */

    /* Expand the instruction as if it were a scalar+vector scatter/gather instruction */

    expand_scatter_gather(sg_context, &mod_sg_info, new_base, scalar_index,
                          scalar_src_or_dst, scratch_pred);
}

/* This instruction loads a fixed size 16-byte vector which is replicated to
 * all quadword elements of the destination register.
 *
 * If the hardware vector length is also 16-bytes (128-bit) then this is the same as a
 * regular predicated contiguous ld1[bhsd], but if the vector length is larger we need
 * to emit code to do the replicating.
 *
 * For example
 *     ld1rqd (%x0,%x1,lsl #3)[16byte] %p2/z -> %z31.d
 * with a 256-bit vector length expands to:
 *
 * setup:
 *       ptrue  VL16 -> %p0.b                  ; p0 = 0b00000000000000001111111111111111
 *       and    %p2/z %p2.b %p0.b -> %p1.b     ; Use p0 to mask the governing predicate p2
 *       add    %x0 %x1 uxtx $0x03 -> %x3      ; Calculate new base address
 *       index  $0x00 $0x01 -> %z0.d           ; Initialize vector index
 *       dup    $0x00 lsl $0x00 -> %z31.d      ; Clear destination register
 * handle_active_elements:
 *       pfalse  -> %p0.b                      ; Initialize loop variable
 *       pnext  %p1 %p0.d -> %p0.d             ; p0 = mask indicating first active
 *                                             ;      element of p1
 *                                             ; NOTE: This is the first *active*
 *                                             ; element which may or may not be
 *                                             ; element 0.
 *       b.none end                            ; if (no more active elements) goto end
 *       lastb  %p0 %z0.d -> %x2               ; extract offset for the current element
 *       ldr    (%x3,%x2,lsl #3)[8byte] -> %x2 ; perform the scalar load
 *       cpy    %p0/m %x2 -> %z31.d            ; cpy loaded value to dst element
 *       pnext  %p1 %p0.d -> %p0.d             ; Find the second active element (if any)
 *       b.eq   end
 *       lastb  %p0 %z0.d -> %x2
 *       ldr    (%x3,%x2,lsl #3)[8byte] -> %x2
 *       cpy    %p0/m %x2 -> %z31.d
 * end:
 *       dup    %z31.q $0x00 -> %z31.q         ; Copy quadword (16-byte) element 0 to
 *                                             ; all elements of dst register.
 */
static void
expand_replicating(const sg_emit_context_t *sg_context,
                   const scatter_gather_info_t *sg_info, reg_id_t new_base,
                   reg_id_t scalar_index, reg_id_t scalar_src_or_dst,
                   reg_id_t scratch_pred, reg_id_t governing_pred, reg_id_t scratch_vec)
{
    DR_ASSERT(sg_info->is_replicating);
    DR_ASSERT(sg_info->is_load);
    if (proc_get_vector_length_bytes() > 16) {
        /* Only the bottom 16 bits of the governing predicate register are used so we
         * need to mask out any higher bits than that.
         */
        DR_ASSERT(sg_info->scatter_gather_size == OPSZ_16);

        /* Set scratch_pred to a value with the first 16 elements active */
        /* ptrue    scratch_pred.b, vl16 */
        EMIT(sg_context, ptrue_sve, opnd_create_reg_element_vector(scratch_pred, OPSZ_1),
             opnd_create_immed_pred_constr(DR_PRED_CONSTR_VL16));

        /* Create a new governing predicate by applying the mask we created in
         * scratch_pred to the instruction's mask_reg.
         */

        /* and      governing_pred.b, mask_reg/z, mask_reg.b, scratch_pred.b */
        EMIT(sg_context, and_sve_pred_b,
             opnd_create_reg_element_vector(governing_pred, OPSZ_1),
             opnd_create_predicate_reg(sg_info->mask_reg, /*merging=*/false),
             opnd_create_reg_element_vector(sg_info->mask_reg, OPSZ_1),
             opnd_create_reg_element_vector(scratch_pred, OPSZ_1));
    }

    if (sg_info->index_reg == DR_REG_NULL) {
        expand_scalar_plus_immediate(sg_context, sg_info, new_base, scalar_index,
                                     scalar_src_or_dst, scratch_pred, governing_pred,
                                     scratch_vec);
    } else {
        expand_scalar_plus_scalar(sg_context, sg_info, new_base, scalar_index,
                                  scalar_src_or_dst, scratch_pred, governing_pred,
                                  scratch_vec);
    }

    if (proc_get_vector_length_bytes() > 16) {
        /* All supported replicating loads load a 16-byte vector. */
        DR_ASSERT(sg_info->scatter_gather_size == OPSZ_16);

        /* Replicate the first quadword element (16 bytes) to the other elements in the
         * vector.
         */

        /* dup gather_dst.q, gather_dst.q[0]*/
        EMIT(sg_context, dup_sve_idx,
             opnd_create_reg_element_vector(sg_info->gather_dst_reg, OPSZ_16),
             opnd_create_reg_element_vector(sg_info->gather_dst_reg, OPSZ_16),
             opnd_create_immed_uint(0, OPSZ_2b));
    }
}

#undef EMIT

/* Spill a scratch predicate or vector register.
 * TODO i#3844: drreg does not support spilling predicate regs yet, so we do it
 * ourselves.
 * When that support is available, this function can be replaced with a drreg API call.
 */
reg_id_t
reserve_sve_register(void *drcontext, instrlist_t *bb, instr_t *where,
                     reg_id_t scratch_gpr0, reg_id_t min_register, reg_id_t max_register,
                     size_t slot_tls_offset, opnd_size_t reg_size, uint slot_num,
                     reg_id_t *already_allocated_regs, uint num_already_allocated)
{
    /* Search the instruction for an unused register we will use as a temp. */
    reg_id_t reg;
    for (reg = min_register; reg <= max_register; ++reg) {
        if (!instr_uses_reg(where, reg)) {
            bool reg_already_allocated = false;
            for (uint i = 0; !reg_already_allocated && i < num_already_allocated; i++) {
                reg_already_allocated = already_allocated_regs[i] == reg;
            }
            if (!reg_already_allocated)
                break;
        }
    }
    DR_ASSERT(!instr_uses_reg(where, reg));

    drmgr_insert_read_tls_field(drcontext, drx_scatter_gather_tls_idx, bb, where,
                                scratch_gpr0);

    /* ldr scratch_gpr0, [scratch_gpr0, #slot_tls_offset] */
    instrlist_meta_preinsert(
        bb, where,
        INSTR_CREATE_ldr(drcontext, opnd_create_reg(scratch_gpr0),
                         OPND_CREATE_MEMPTR(scratch_gpr0, slot_tls_offset)));

    /* str reg, [scratch_gpr0, #slot_num, mul vl] */
    instrlist_meta_preinsert(
        bb, where,
        INSTR_CREATE_str(drcontext,
                         opnd_create_base_disp(
                             scratch_gpr0, DR_REG_NULL, /*scale=*/0,
                             /*disp=*/slot_num * opnd_size_in_bytes(reg_size), reg_size),
                         opnd_create_reg(reg)));

    return reg;
}

reg_id_t
reserve_pred_register(void *drcontext, instrlist_t *bb, instr_t *where,
                      reg_id_t scratch_gpr0, spill_slot_state_t *slot_state)
{
    uint slot;
    for (slot = 0; slot < NUM_PRED_SLOTS; slot++) {
        if (slot_state->pred_slots[slot] == DR_REG_NULL) {
            break;
        }
    }
    DR_ASSERT(slot_state->pred_slots[slot] == DR_REG_NULL);

    /* Some instructions require the predicate to be in the range p0 - p7. This includes
     * LASTB which we use to extract elements from the vector register.
     */
    const reg_id_t reg =
        reserve_sve_register(drcontext, bb, where, scratch_gpr0, DR_REG_P0, DR_REG_P7,
                             offsetof(per_thread_t, scratch_pred_spill_slots),
                             opnd_size_from_bytes(proc_get_vector_length_bytes() / 8),
                             slot, slot_state->pred_slots, slot);

    slot_state->pred_slots[slot] = reg;
    return reg;
}

reg_id_t
reserve_vector_register(void *drcontext, instrlist_t *bb, instr_t *where,
                        reg_id_t scratch_gpr0, spill_slot_state_t *slot_state)
{
    uint slot;
    for (slot = 0; slot < NUM_VECTOR_SLOTS; slot++) {
        if (slot_state->vector_slots[slot] == DR_REG_NULL) {
            break;
        }
    }
    DR_ASSERT(slot_state->vector_slots[slot] == DR_REG_NULL);

    const reg_id_t reg =
        reserve_sve_register(drcontext, bb, where, scratch_gpr0, DR_REG_Z0, DR_REG_Z31,
                             offsetof(per_thread_t, scratch_vector_spill_slots_aligned),
                             opnd_size_from_bytes(proc_get_vector_length_bytes()), slot,
                             slot_state->vector_slots, slot);

    slot_state->vector_slots[slot] = reg;
    return reg;
}

/* Restore the scratch predicate reg.
 * TODO i#3844: drreg does not support spilling predicate regs yet, so we do it
 * ourselves.
 * When that support is available, this funcion can be replaced with a drreg API call.
 */
void
unreserve_sve_register(void *drcontext, instrlist_t *bb, instr_t *where,
                       reg_id_t scratch_gpr0, reg_id_t reg, size_t slot_tls_offset,
                       opnd_size_t reg_size, uint slot_num)
{
    drmgr_insert_read_tls_field(drcontext, drx_scatter_gather_tls_idx, bb, where,
                                scratch_gpr0);

    /* ldr scratch_gpr0, [scratch_gpr0, #slot_tls_offset] */
    instrlist_meta_preinsert(
        bb, where,
        INSTR_CREATE_ldr(drcontext, opnd_create_reg(scratch_gpr0),
                         OPND_CREATE_MEMPTR(scratch_gpr0, slot_tls_offset)));

    /* ldr reg, [scratch_gpr0, #slot_num, mul vl] */
    instrlist_meta_preinsert(
        bb, where,
        INSTR_CREATE_ldr(
            drcontext, opnd_create_reg(reg),
            opnd_create_base_disp(scratch_gpr0, DR_REG_NULL, /*scale=*/0,
                                  /*disp=*/slot_num * opnd_size_in_bytes(reg_size),
                                  reg_size)));
}

void
unreserve_pred_register(void *drcontext, instrlist_t *bb, instr_t *where,
                        reg_id_t scratch_gpr0, reg_id_t scratch_pred,
                        spill_slot_state_t *slot_state)
{
    uint slot;
    for (slot = 0; slot < NUM_PRED_SLOTS; slot++) {
        if (slot_state->pred_slots[slot] == scratch_pred) {
            break;
        }
    }
    DR_ASSERT(slot_state->pred_slots[slot] == scratch_pred);

    unreserve_sve_register(drcontext, bb, where, scratch_gpr0, scratch_pred,
                           offsetof(per_thread_t, scratch_pred_spill_slots),
                           opnd_size_from_bytes(proc_get_vector_length_bytes() / 8),
                           slot);

    slot_state->pred_slots[slot] = DR_REG_NULL;
}

void
unreserve_vector_register(void *drcontext, instrlist_t *bb, instr_t *where,
                          reg_id_t scratch_gpr0, reg_id_t scratch_vec,
                          spill_slot_state_t *slot_state)
{
    uint slot;
    for (slot = 0; slot < NUM_VECTOR_SLOTS; slot++) {
        if (slot_state->vector_slots[slot] == scratch_vec) {
            break;
        }
    }
    DR_ASSERT(slot_state->vector_slots[slot] == scratch_vec);

    unreserve_sve_register(drcontext, bb, where, scratch_gpr0, scratch_vec,
                           offsetof(per_thread_t, scratch_vector_spill_slots_aligned),
                           opnd_size_from_bytes(proc_get_vector_length_bytes()), slot);

    slot_state->vector_slots[slot] = DR_REG_NULL;
}

/*****************************************************************************************
 * drx_expand_scatter_gather()
 *
 * The function expands scatter and gather instructions to a sequence of equivalent
 * scalar operations.
 */
bool
drx_expand_scatter_gather(void *drcontext, instrlist_t *bb, DR_PARAM_OUT bool *expanded)
{
    if (expanded != NULL)
        *expanded = false;

    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_APP2APP)
        return false;

    instr_t *sg_instr = NULL;
    if (!scatter_gather_split_bb(drcontext, bb, &sg_instr)) {
        /* bb did not begin with a scatter/gather instruction. If there were any
         * scatter/gather instructions that were not at the beginning, they have been
         * split out of the bb and we will be called again later to handle them.
         */
        return true;
    }
    DR_ASSERT(sg_instr != NULL);

    scatter_gather_info_t sg_info;
    bool res = false;
    get_scatter_gather_info(sg_instr, &sg_info);

    /* Filter out instructions which are not yet supported.
     * We return true with *expanded=false here to indicate that no error occurred but
     * we didn't expand any instructions. This matches the behaviour of this function
     * for architectures with no scatter/gather expansion support.
     */
    if (sg_info.faulting_behavior != DRX_NORMAL_FAULTING) {
        /* TODO i#5036: Add support for first-fault and non-fault accesses. */
        return true;
    }

    const bool is_contiguous =
        !(reg_is_z(sg_info.base_reg) || reg_is_z(sg_info.index_reg));

    /* We want to avoid spill slot conflicts with later instrumentation passes. */
    drreg_status_t res_bb_props =
        drreg_set_bb_properties(drcontext, DRREG_HANDLE_MULTI_PHASE_SLOT_RESERVATIONS);
    DR_ASSERT(res_bb_props == DRREG_SUCCESS);

    /* Tell drx_event_restore_state() that an expansion has occurred. */
    drx_mark_scatter_gather_expanded();

    drvector_t allowed;
    drreg_init_and_fill_vector(&allowed, true);

    /* We need the scratch registers and base/index register app's value to be available
     * at the same time. Do not use.
     */
    if (!reg_is_z(sg_info.base_reg))
        drreg_set_vector_entry(&allowed, sg_info.base_reg, false);
    if (!reg_is_z(sg_info.index_reg))
        drreg_set_vector_entry(&allowed, sg_info.index_reg, false);

    if (drreg_reserve_aflags(drcontext, bb, sg_instr) != DRREG_SUCCESS)
        goto drx_expand_scatter_gather_exit;

    /* Used as a scratch register when we reserve/unreserve vector/predicate registers,
     * and used as the scalar_index_or_base register in the expansion.
     */
    reg_id_t scratch_gpr = DR_REG_INVALID;
    if (drreg_reserve_register(drcontext, bb, sg_instr, &allowed, &scratch_gpr) !=
        DRREG_SUCCESS)
        goto drx_expand_scatter_gather_exit;

    reg_id_t scalar_src_or_dst = DR_REG_INVALID;
    if (!sg_info.is_load || (is_contiguous && sg_info.reg_count > 1)) {
        if (drreg_reserve_register(drcontext, bb, sg_instr, &allowed,
                                   &scalar_src_or_dst) != DRREG_SUCCESS)
            goto drx_expand_scatter_gather_exit;
    } else {
        /* The scalar_dst and scalar_index/base registers are not not needed at the same
         * time for single register loads so we can use the same register for both.
         */
        scalar_src_or_dst = scratch_gpr;
    }

    reg_id_t contiguous_new_base = DR_REG_INVALID;
    if (is_contiguous &&
        drreg_reserve_register(drcontext, bb, sg_instr, &allowed, &contiguous_new_base) !=
            DRREG_SUCCESS)
        goto drx_expand_scatter_gather_exit;

    spill_slot_state_t spill_slot_state;
    init_spill_slot_state(&spill_slot_state);

    const reg_id_t scratch_pred =
        reserve_pred_register(drcontext, bb, sg_instr, scratch_gpr, &spill_slot_state);

    reg_id_t scratch_vec = DR_REG_INVALID;
    if (is_contiguous) {
        /* This is a contiguous predicated access which requires an extra scratch Z
         * register. */
        scratch_vec = reserve_vector_register(drcontext, bb, sg_instr, scratch_gpr,
                                              &spill_slot_state);
    }

    reg_id_t governing_pred = sg_info.mask_reg;
    if (sg_info.is_replicating && proc_get_vector_length_bytes() > 16) {
        governing_pred = reserve_pred_register(drcontext, bb, sg_instr, scratch_gpr,
                                               &spill_slot_state);
    }

    const app_pc orig_app_pc = instr_get_app_pc(sg_instr);

    emulated_instr_t emulated_instr;
    emulated_instr.size = sizeof(emulated_instr);
    emulated_instr.pc = instr_get_app_pc(sg_instr);
    emulated_instr.instr = sg_instr;
    /* Tools should instrument the data operations in the sequence. */
    emulated_instr.flags = DR_EMULATE_INSTR_ONLY;
    drmgr_insert_emulation_start(drcontext, bb, sg_instr, &emulated_instr);

    const sg_emit_context_t sg_context = {
        drcontext,
        bb,
        sg_instr,
        orig_app_pc,
    };

    if (sg_info.is_replicating) {
        expand_replicating(&sg_context, &sg_info, contiguous_new_base, scratch_gpr,
                           scalar_src_or_dst, scratch_pred, governing_pred, scratch_vec);
    } else if (is_contiguous) {
        /* scalar+scalar or scalar+immediate predicated contiguous access */

        if (sg_info.index_reg == DR_REG_NULL) {
            expand_scalar_plus_immediate(&sg_context, &sg_info, contiguous_new_base,
                                         scratch_gpr, scalar_src_or_dst, scratch_pred,
                                         governing_pred, scratch_vec);
        } else {
            expand_scalar_plus_scalar(&sg_context, &sg_info, contiguous_new_base,
                                      scratch_gpr, scalar_src_or_dst, scratch_pred,
                                      governing_pred, scratch_vec);
        }
    } else {
        /* scalar+vector, vector+immediate, or vector+scalar scatter/gather */
        reg_id_t scalar_base;
        reg_id_t scalar_index;
        if (reg_is_z(sg_info.index_reg)) {
            scalar_base = sg_info.base_reg;
            scalar_index = scratch_gpr;
        } else {
            scalar_base = scratch_gpr;
            scalar_index = sg_info.index_reg;
        }

        expand_scatter_gather(&sg_context, &sg_info, scalar_base, scalar_index,
                              scalar_src_or_dst, scratch_pred);
    }

    drmgr_insert_emulation_end(drcontext, bb, sg_instr);

    for (uint i = 0; i < NUM_VECTOR_SLOTS; i++) {
        const reg_id_t reg = spill_slot_state.vector_slots[i];
        if (reg != DR_REG_NULL) {
            unreserve_vector_register(drcontext, bb, sg_instr, scratch_gpr, reg,
                                      &spill_slot_state);
        }
    }

    for (uint i = 0; i < NUM_PRED_SLOTS; i++) {
        const reg_id_t reg = spill_slot_state.pred_slots[i];
        if (reg != DR_REG_NULL) {
            unreserve_pred_register(drcontext, bb, sg_instr, scratch_gpr, reg,
                                    &spill_slot_state);
        }
    }

    if (drreg_unreserve_register(drcontext, bb, sg_instr, scratch_gpr) != DRREG_SUCCESS) {
        DR_ASSERT_MSG(false, "drreg_unreserve_register should not fail");
        goto drx_expand_scatter_gather_exit;
    }

    if (scalar_src_or_dst != scratch_gpr &&
        drreg_unreserve_register(drcontext, bb, sg_instr, scalar_src_or_dst) !=
            DRREG_SUCCESS) {
        DR_ASSERT_MSG(false, "drreg_unreserve_register should not fail");
        goto drx_expand_scatter_gather_exit;
    }

    if (contiguous_new_base != DR_REG_INVALID &&
        drreg_unreserve_register(drcontext, bb, sg_instr, contiguous_new_base) !=
            DRREG_SUCCESS) {
        DR_ASSERT_MSG(false, "drreg_unreserve_register should not fail");
        goto drx_expand_scatter_gather_exit;
    }

    if (drreg_unreserve_aflags(drcontext, bb, sg_instr) != DRREG_SUCCESS)
        goto drx_expand_scatter_gather_exit;

#if VERBOSE
    dr_fprintf(STDERR, "\tVector length = %u bytes\n", proc_get_vector_length_bytes());
    dr_print_instr(drcontext, STDERR, sg_instr, "\tThe instruction\n");
#endif

    /* Remove and destroy the original scatter/gather. */
    instrlist_remove(bb, sg_instr);
#if VERBOSE
    dr_fprintf(STDERR, "\twas expanded to the following sequence:\n");
    for (instr_t *instr = instrlist_first(bb); instr != NULL;
         instr = instr_get_next(instr)) {
        dr_print_instr(drcontext, STDERR, instr, "");
    }
#endif

    if (expanded != NULL)
        *expanded = true;
    res = true;

drx_expand_scatter_gather_exit:
    drvector_delete(&allowed);
    return res;
}

bool
drx_scatter_gather_restore_state(void *drcontext, dr_restore_state_info_t *info,
                                 instr_t *sg_inst)
{
    /* If this function is called, we know that a fault occurred on an instruction in a
     * fragment which expands a scatter/gather instruction, but we don't know whether
     * the instruction that faulted was one of the expansion loads or stores emitted by
     * drx_expand_scatter_gather(), or part of instrumentation added later by a client.
     *
     * If a scatter/gather expansion instruction faults we need to treat it as if the
     * expanded scatter/gather instruction had faulted and set the register state as
     * appropriate for the expanded instruction. This isn't implemented yet so we hit
     * an assert below.
     *
     * Previously this function would always assert but this causes a problem with
     * clients (such as memval_simple) that use drx_buf (or similar) which uses faulting
     * stores to manage the trace buffer.
     * Until we implement proper state restoration we need to filter out faults that
     * don't come from scatter/gather expansion instructions and pass them on to the
     * client to handle, otherwise we can get spurious failures with clients like
     * memval_simple.
     */
    if (info->fragment_info.ilist != NULL) {
        byte *pc = info->fragment_info.cache_start_pc;
        for (instr_t *instr = instrlist_first(info->fragment_info.ilist); instr != NULL;
             instr = instr_get_next(instr)) {
            if (pc == info->raw_mcontext->pc && !instr_is_label(instr)) {
                /* Found the faulting instruction */
                if (!scatter_gather_is_expanded_ld_st(instr)) {
                    /* The fault originates from an instruction inserted by a client.
                     * Pass it on for the client to handle.
                     */
                    return true;
                }
                break;
            } else if (pc > info->raw_mcontext->pc) {
                DR_ASSERT_MSG(pc < info->raw_mcontext->pc,
                              "Failed to find faulting instruction");
                return false;
            }
            pc += instr_length(drcontext, instr);
        }
    } else {
        /* The ilist isn't available (see i#3801). We could decode the code cache and use
         * heuristics to determine the origin of the load/store, but right now we just
         * assume that it is an expansion instruction and hit the assert below.
         */
    }

    /* TODO i#6317, i#5036: Restore the scratch predicate register.
     *                      We need to add support for handling SVE state during
     *                      signals first.
     */
    DR_ASSERT_MSG(false, "NYI i#6317 i#5036");
    return false;
}
