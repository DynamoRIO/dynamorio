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

#define SVE_MAX_VECTOR_LENGTH_BITS 2048
#define SVE_MAX_VECTOR_LENGTH_BYTES (SVE_MAX_VECTOR_LENGTH_BITS / 8)
#define SVE_VECTOR_ALIGNMENT_BYTES 16
#define SVE_VECTOR_SPILL_SLOT_SIZE \
    SVE_MAX_VECTOR_LENGTH_BYTES + (SVE_VECTOR_ALIGNMENT_BYTES - 1)

#define SVE_MAX_PREDICATE_LENGTH_BITS (SVE_MAX_VECTOR_LENGTH_BITS / 8)
#define SVE_MAX_PREDICATE_LENGTH_BYTES (SVE_MAX_PREDICATE_LENGTH_BITS / 8)
#define SVE_PREDICATE_ALIGNMENT_BYTES 2
#define SVE_PREDICATE_SPILL_SLOT_SIZE SVE_MAX_PREDICATE_LENGTH_BYTES

typedef struct _per_thread_t {
    void *scratch_pred_spill_slot; /* Storage for spilled predicate register. */
} per_thread_t;

void
drx_scatter_gather_thread_init(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)dr_thread_alloc(drcontext, sizeof(*pt));

    /*
     * The instructions we use to load/store the spilled predicate register require
     * the base address to be aligned to 2 bytes:
     *     LDR <Pt>, [<Xn|SP>{, #<imm>, MUL VL}]
     *     STR <Pt>, [<Xn|SP>{, #<imm>, MUL VL}]
     * and dr_thread_alloc() guarantees allocated memory is aligned to the pointer size
     * (8 bytes) so we shouldn't have to do any further alignment.
     */
    pt->scratch_pred_spill_slot =
        dr_thread_alloc(drcontext, SVE_PREDICATE_SPILL_SLOT_SIZE);
    DR_ASSERT_MSG(ALIGNED(pt->scratch_pred_spill_slot, SVE_PREDICATE_ALIGNMENT_BYTES),
                  "scratch_pred_spill_slot is misaligned");

    drmgr_set_tls_field(drcontext, drx_scatter_gather_tls_idx, (void *)pt);
}

void
drx_scatter_gather_thread_exit(void *drcontext)
{
    per_thread_t *pt =
        (per_thread_t *)drmgr_get_tls_field(drcontext, drx_scatter_gather_tls_idx);
    dr_thread_free(drcontext, pt->scratch_pred_spill_slot, SVE_PREDICATE_SPILL_SLOT_SIZE);
    dr_thread_free(drcontext, pt, sizeof(*pt));
}

static void
get_scatter_gather_info(instr_t *instr, OUT scatter_gather_info_t *sg_info)
{
    DR_ASSERT_MSG(instr_is_scatter(instr) || instr_is_gather(instr),
                  "Instruction must be scatter or gather.");

    opnd_t dst0 = instr_get_dst(instr, 0);
    opnd_t src0 = instr_get_src(instr, 0);
    sg_info->mask_reg = opnd_get_reg(instr_get_src(instr, 1));

    opnd_t memopnd;
    if (instr_is_scatter(instr)) {
        sg_info->is_load = false;
        sg_info->scatter_src_reg = opnd_get_reg(src0);
        sg_info->element_size = opnd_get_vector_element_size(src0);
        memopnd = dst0;
    } else {
        sg_info->is_load = true;
        sg_info->gather_dst_reg = opnd_get_reg(dst0);
        sg_info->element_size = opnd_get_vector_element_size(dst0);
        memopnd = src0;
    }

    sg_info->base_reg = opnd_get_base(memopnd);
    sg_info->index_reg = opnd_get_index(memopnd);

    sg_info->disp = opnd_get_disp(memopnd);
    sg_info->extend =
        opnd_get_index_extend(memopnd, &sg_info->scaled, &sg_info->extend_amount);

    sg_info->scatter_gather_size = opnd_get_size(memopnd);

    switch (instr_get_opcode(instr)) {
#define DRX_CASE(op, _reg_count, value_size, value_signed, _faulting_behavior) \
    case OP_##op:                                                              \
        sg_info->reg_count = _reg_count;                                       \
        sg_info->scalar_value_size = value_size;                               \
        sg_info->is_scalar_value_signed = value_signed;                        \
        sg_info->faulting_behavior = _faulting_behavior;                       \
        break

        DRX_CASE(ld1b, 1, OPSZ_1, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld1h, 1, OPSZ_2, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld1w, 1, OPSZ_4, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld1d, 1, OPSZ_8, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld1sb, 1, OPSZ_1, true, DRX_NORMAL_FAULTING);
        DRX_CASE(ld1sh, 1, OPSZ_2, true, DRX_NORMAL_FAULTING);
        DRX_CASE(ld1sw, 1, OPSZ_4, true, DRX_NORMAL_FAULTING);

        DRX_CASE(ldff1b, 1, OPSZ_1, false, DRX_FIRST_FAULTING);
        DRX_CASE(ldff1h, 1, OPSZ_2, false, DRX_FIRST_FAULTING);
        DRX_CASE(ldff1w, 1, OPSZ_4, false, DRX_FIRST_FAULTING);
        DRX_CASE(ldff1d, 1, OPSZ_8, false, DRX_FIRST_FAULTING);
        DRX_CASE(ldff1sb, 1, OPSZ_1, true, DRX_FIRST_FAULTING);
        DRX_CASE(ldff1sh, 1, OPSZ_2, true, DRX_FIRST_FAULTING);
        DRX_CASE(ldff1sw, 1, OPSZ_4, true, DRX_FIRST_FAULTING);

        DRX_CASE(ldnf1b, 1, OPSZ_1, false, DRX_NON_FAULTING);
        DRX_CASE(ldnf1h, 1, OPSZ_2, false, DRX_NON_FAULTING);
        DRX_CASE(ldnf1w, 1, OPSZ_4, false, DRX_NON_FAULTING);
        DRX_CASE(ldnf1d, 1, OPSZ_8, false, DRX_NON_FAULTING);
        DRX_CASE(ldnf1sb, 1, OPSZ_1, true, DRX_NON_FAULTING);
        DRX_CASE(ldnf1sh, 1, OPSZ_2, true, DRX_NON_FAULTING);
        DRX_CASE(ldnf1sw, 1, OPSZ_4, true, DRX_NON_FAULTING);

        DRX_CASE(ldnt1b, 1, OPSZ_1, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ldnt1h, 1, OPSZ_2, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ldnt1w, 1, OPSZ_4, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ldnt1d, 1, OPSZ_8, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ldnt1sb, 1, OPSZ_1, true, DRX_NORMAL_FAULTING);
        DRX_CASE(ldnt1sh, 1, OPSZ_2, true, DRX_NORMAL_FAULTING);
        DRX_CASE(ldnt1sw, 1, OPSZ_4, true, DRX_NORMAL_FAULTING);

        DRX_CASE(st1b, 1, OPSZ_1, false, DRX_NORMAL_FAULTING);
        DRX_CASE(st1h, 1, OPSZ_2, false, DRX_NORMAL_FAULTING);
        DRX_CASE(st1w, 1, OPSZ_4, false, DRX_NORMAL_FAULTING);
        DRX_CASE(st1d, 1, OPSZ_8, false, DRX_NORMAL_FAULTING);

        DRX_CASE(stnt1b, 1, OPSZ_1, false, DRX_NORMAL_FAULTING);
        DRX_CASE(stnt1h, 1, OPSZ_2, false, DRX_NORMAL_FAULTING);
        DRX_CASE(stnt1w, 1, OPSZ_4, false, DRX_NORMAL_FAULTING);
        DRX_CASE(stnt1d, 1, OPSZ_8, false, DRX_NORMAL_FAULTING);

        DRX_CASE(ld2b, 2, OPSZ_1, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld2h, 2, OPSZ_2, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld2w, 2, OPSZ_4, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld2d, 2, OPSZ_8, false, DRX_NORMAL_FAULTING);

        DRX_CASE(st2b, 2, OPSZ_1, false, DRX_NORMAL_FAULTING);
        DRX_CASE(st2h, 2, OPSZ_2, false, DRX_NORMAL_FAULTING);
        DRX_CASE(st2w, 2, OPSZ_4, false, DRX_NORMAL_FAULTING);
        DRX_CASE(st2d, 2, OPSZ_8, false, DRX_NORMAL_FAULTING);

        DRX_CASE(ld3b, 3, OPSZ_1, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld3h, 3, OPSZ_2, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld3w, 3, OPSZ_4, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld3d, 3, OPSZ_8, false, DRX_NORMAL_FAULTING);

        DRX_CASE(st3b, 3, OPSZ_1, false, DRX_NORMAL_FAULTING);
        DRX_CASE(st3h, 3, OPSZ_2, false, DRX_NORMAL_FAULTING);
        DRX_CASE(st3w, 3, OPSZ_4, false, DRX_NORMAL_FAULTING);
        DRX_CASE(st3d, 3, OPSZ_8, false, DRX_NORMAL_FAULTING);

        DRX_CASE(ld4b, 4, OPSZ_1, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld4h, 4, OPSZ_2, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld4w, 4, OPSZ_4, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld4d, 4, OPSZ_8, false, DRX_NORMAL_FAULTING);

        DRX_CASE(st4b, 4, OPSZ_1, false, DRX_NORMAL_FAULTING);
        DRX_CASE(st4h, 4, OPSZ_2, false, DRX_NORMAL_FAULTING);
        DRX_CASE(st4w, 4, OPSZ_4, false, DRX_NORMAL_FAULTING);
        DRX_CASE(st4d, 4, OPSZ_8, false, DRX_NORMAL_FAULTING);

        DRX_CASE(ld1rob, 1, OPSZ_1, false, DRX_NORMAL_FAULTING);

        DRX_CASE(ld1rqb, 1, OPSZ_1, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld1rqh, 1, OPSZ_2, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld1rqw, 1, OPSZ_4, false, DRX_NORMAL_FAULTING);
        DRX_CASE(ld1rqd, 1, OPSZ_8, false, DRX_NORMAL_FAULTING);
#undef DRX_CASE

    default: DR_ASSERT_MSG(false, "Invalid scatter/gather instruction");
    }
}

/*
 * Emit code to expand a scalar + vector gather load into a series of equivalent scalar
 * loads.
 * These instructions have memory operands of the form:
 *     [<Xn|SP>, <Zm>.<Ts>{, <mod>}]
 * where addresses to load/store each element are calculated by adding a base address
 * from the scalar register Xn, to an offset read from the corresponding element of the
 * vector index register Zm.
 *
 * if (is_load)
 *     clear_inactive_elements(dst);
 * for (e=first_active_element(); e < num_elements; e =  next_active_element()) {
 *     if (is_load)
 *         dst[e] = scalar_load(base, offsets[e], mod);
 *     else
 *         scalar_store(src[e], base, offsets[e], mod);
 * }

 * For example
 *     ld1d   (%x0,%z26.d,lsl #3)[32byte] %p1/z -> %z27.d
 * expands to:
 *
 *       dup    $0x00 lsl $0x00 -> %z27.d       ; Clear dst register
 *       pfalse  -> %p0.b
 *   loop:
 *       pnext  %p1 %p0.d -> %p0.d              ; p0 = mask indicating next active
 *                                              ;      element of p1
 *       b.none end                             ; if (no more active elements) goto end
 *       lastb  %p0 %z26.d -> %x1               ; extract offset for the current element
 *       ldr    (%x0,%x1,lsl #3)[8byte] -> %x1  ; perform the scalar load
 *       cpy    %p0/m %x1 -> %z27.d             ; cpy loaded value to dst element
 *       b      loop
 *   end:
 *       ...
 *
 * TODO i#5036 Add support for scatter store operations.
 */
static void
expand_scalar_plus_vector(void *drcontext, instrlist_t *bb, instr_t *sg_instr,
                          const scatter_gather_info_t *sg_info, reg_id_t scratch_gpr,
                          reg_id_t scratch_pred, app_pc orig_app_pc)
{
#define EMIT(op, ...)    \
    instrlist_preinsert( \
        bb, sg_instr, INSTR_XL8(INSTR_CREATE_##op(drcontext, __VA_ARGS__), orig_app_pc))

    DR_ASSERT_MSG(reg_is_z(sg_info->index_reg), "Index must be a Z register");

    if (sg_info->is_load) {
        /* First we deal with the inactive elements. Gather loads are always zeroing so we
         * need to set all inactive elements to 0.
         */
        if (sg_info->index_reg == sg_info->gather_dst_reg) {
            /* The dst register is also the index register so we need to preserve the
             * value of the active elements so we can use them as offsets. We do this by
             * cpying a 0 value into the dst register using the inverse of the mask_reg as
             * the governing predicate.
             */

            /* ptrue    scratch_pred.b */
            EMIT(ptrue_sve, opnd_create_reg_element_vector(scratch_pred, OPSZ_1),
                 opnd_create_immed_pred_constr(DR_PRED_CONSTR_ALL));

            /* not      scratch_pred.b, scratch_pred/z, mask_reg.b */
            EMIT(not_sve_pred_b, opnd_create_reg_element_vector(scratch_pred, OPSZ_1),
                 opnd_create_predicate_reg(scratch_pred, false),
                 opnd_create_reg_element_vector(sg_info->mask_reg, OPSZ_1));

            /* cpy      gather_dst_reg.element_size, scratch_pred/m, #0, lsl #0 */
            EMIT(cpy_sve_shift_pred,
                 opnd_create_reg_element_vector(sg_info->gather_dst_reg,
                                                sg_info->element_size),
                 opnd_create_predicate_reg(scratch_pred, true), OPND_CREATE_INT8(0),
                 opnd_create_immed_uint(0, OPSZ_1b));
        } else {
            /* We don't care about any values in the dst register so zero the whole thing.
             */

            /* dup      gather_dst_reg.element_size, #0, lsl #0 */
            EMIT(dup_sve_shift,
                 opnd_create_reg_element_vector(sg_info->gather_dst_reg,
                                                sg_info->element_size),
                 OPND_CREATE_INT8(0), opnd_create_immed_uint(0, OPSZ_1b));
        }
    }

    /* pfalse   scratch_pred.b */
    EMIT(pfalse_sve, opnd_create_reg_element_vector(scratch_pred, OPSZ_1));

    instr_t *loop_label = INSTR_CREATE_label(drcontext);
    instr_t *end_label = INSTR_CREATE_label(drcontext);

    instrlist_meta_preinsert(bb, sg_instr, loop_label);

    /* pnext scratch_pred.element_size, mask_reg, scratch_pred.element_size */
    EMIT(pnext_sve, opnd_create_reg_element_vector(scratch_pred, sg_info->element_size),
         opnd_create_reg(sg_info->mask_reg));

    /* b.none   end */
    instrlist_preinsert(
        bb, sg_instr,
        INSTR_XL8(INSTR_PRED(INSTR_CREATE_bcond(drcontext, opnd_create_instr(end_label)),
                             DR_PRED_SVE_NONE),
                  orig_app_pc));

    /* lastb    scratch_gpr, scratch_pred, index_reg.element_size */
    EMIT(lastb_sve_scalar, opnd_create_reg(scratch_gpr), opnd_create_reg(scratch_pred),
         opnd_create_reg_element_vector(sg_info->index_reg, sg_info->element_size));

    if (sg_info->is_load) {
        /* ldr[bh]  scratch_gpr, [base_reg, scratch_gpr, mod #amount] */
        opnd_t mem = opnd_create_base_disp_shift_aarch64(
            sg_info->base_reg, scratch_gpr, sg_info->extend, sg_info->scaled, 0, 0,
            sg_info->scalar_value_size, sg_info->extend_amount);

        if (sg_info->is_scalar_value_signed) {
            const reg_id_t ld_dst =
                reg_resize_to_opsz(scratch_gpr, sg_info->element_size);
            switch (sg_info->scalar_value_size) {
            case OPSZ_1: EMIT(ldrsb, opnd_create_reg(ld_dst), mem); break;
            case OPSZ_2: EMIT(ldrsh, opnd_create_reg(ld_dst), mem); break;
            case OPSZ_4: EMIT(ldrsw, opnd_create_reg(ld_dst), mem); break;
            default: DR_ASSERT_MSG(false, "Invalid scatter_gather_info_t data");
            }
        } else {
            const reg_id_t scratch_gpr_w = reg_resize_to_opsz(scratch_gpr, OPSZ_4);
            switch (sg_info->scalar_value_size) {
            case OPSZ_1: EMIT(ldrb, opnd_create_reg(scratch_gpr_w), mem); break;
            case OPSZ_2: EMIT(ldrh, opnd_create_reg(scratch_gpr_w), mem); break;
            case OPSZ_4: EMIT(ldr, opnd_create_reg(scratch_gpr_w), mem); break;
            case OPSZ_8: EMIT(ldr, opnd_create_reg(scratch_gpr), mem); break;
            default: DR_ASSERT_MSG(false, "Invalid scatter_gather_info_t data");
            }
        }

        /* cpy      gather_dst_reg.element_size, scratch_pred/m, scratch_gpr */
        EMIT(cpy_sve_pred,
             opnd_create_reg_element_vector(sg_info->gather_dst_reg,
                                            sg_info->element_size),
             opnd_create_predicate_reg(scratch_pred, true),
             opnd_create_reg(reg_resize_to_opsz(scratch_gpr, sg_info->element_size)));
    } else {
        DR_ASSERT_MSG(sg_info->is_load, "Stores are not yet supported");
    }

    /* b        loop */
    EMIT(b, opnd_create_instr(loop_label));

    instrlist_meta_preinsert(bb, sg_instr, end_label);

#undef EMIT
}

/* Spill a scratch predicate or vector register.
 * TODO i#3844: drreg does not support spilling predicate regs yet, so we do it
 * ourselves.
 * When that support is available, this function can be replaced with a drreg API call.
 */
reg_id_t
reserve_sve_register(void *drcontext, instrlist_t *bb, instr_t *where,
                     reg_id_t scratch_gpr, reg_id_t min_register, reg_id_t max_register,
                     size_t slot_offset, opnd_size_t reg_size)
{
    /* Search the instruction for an unused register we will use as a temp. */
    reg_id_t reg;
    for (reg = min_register; reg <= max_register; ++reg) {
        if (!instr_uses_reg(where, reg))
            break;
    }

    drmgr_insert_read_tls_field(drcontext, drx_scatter_gather_tls_idx, bb, where,
                                scratch_gpr);

    /* ldr scratch_gpr, [scratch_gpr, #slot_offset] */
    instrlist_meta_preinsert(
        bb, where,
        INSTR_CREATE_ldr(drcontext, opnd_create_reg(scratch_gpr),
                         OPND_CREATE_MEMPTR(scratch_gpr, slot_offset)));

    /* str reg, [scratch_gpr] */
    instrlist_meta_preinsert(
        bb, where,
        INSTR_CREATE_str(drcontext,
                         opnd_create_base_disp(scratch_gpr, DR_REG_NULL, 0, 0, reg_size),
                         opnd_create_reg(reg)));

    return reg;
}

reg_id_t
reserve_pred_register(void *drcontext, instrlist_t *bb, instr_t *where,
                      reg_id_t scratch_gpr)
{
    /* Some instructions require the predicate to be in the range p0 - p7. This includes
     * LASTB which we use to extract elements from the vector register.
     */
    return reserve_sve_register(drcontext, bb, where, scratch_gpr, DR_REG_P0, DR_REG_P7,
                                offsetof(per_thread_t, scratch_pred_spill_slot),
                                opnd_size_from_bytes(proc_get_vector_length_bytes() / 8));
}

/* Restore the scratch predicate reg.
 * TODO i#3844: drreg does not support spilling predicate regs yet, so we do it
 * ourselves.
 * When that support is available, this funcion can be replaced with a drreg API call.
 */
void
unreserve_sve_register(void *drcontext, instrlist_t *bb, instr_t *where,
                       reg_id_t scratch_gpr, reg_id_t reg, size_t slot_offset,
                       opnd_size_t reg_size)
{
    drmgr_insert_read_tls_field(drcontext, drx_scatter_gather_tls_idx, bb, where,
                                scratch_gpr);

    /* ldr scratch_gpr, [scratch_gpr, #slot_offset] */
    instrlist_meta_preinsert(
        bb, where,
        INSTR_CREATE_ldr(drcontext, opnd_create_reg(scratch_gpr),
                         OPND_CREATE_MEMPTR(scratch_gpr, slot_offset)));

    /* ldr reg, [scratch_gpr] */
    instrlist_meta_preinsert(
        bb, where,
        INSTR_CREATE_ldr(
            drcontext, opnd_create_reg(reg),
            opnd_create_base_disp(scratch_gpr, DR_REG_NULL, 0, 0, reg_size)));
}

void
unreserve_pred_register(void *drcontext, instrlist_t *bb, instr_t *where,
                        reg_id_t scratch_gpr, reg_id_t scratch_pred)
{
    unreserve_sve_register(drcontext, bb, where, scratch_gpr, scratch_pred,
                           offsetof(per_thread_t, scratch_pred_spill_slot),
                           opnd_size_from_bytes(proc_get_vector_length_bytes() / 8));
}

/*****************************************************************************************
 * drx_expand_scatter_gather()
 *
 * The function expands scatter and gather instructions to a sequence of equivalent
 * scalar operations.
 */
bool
drx_expand_scatter_gather(void *drcontext, instrlist_t *bb, OUT bool *expanded)
{
    if (expanded != NULL)
        *expanded = false;

    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_APP2APP)
        return false;

    instr_t *sg_instr = NULL;
    if (!scatter_gather_split_bb(drcontext, bb, &sg_instr)) {
        return true;
    }
    DR_ASSERT(sg_instr != NULL);

    scatter_gather_info_t sg_info;
    bool res = false;
    get_scatter_gather_info(sg_instr, &sg_info);

    if (!(sg_info.is_load && reg_is_z(sg_info.index_reg) &&
          sg_info.faulting_behavior == DRX_NORMAL_FAULTING))
        return true;

    /* We want to avoid spill slot conflicts with later instrumentation passes. */
    drreg_status_t res_bb_props =
        drreg_set_bb_properties(drcontext, DRREG_HANDLE_MULTI_PHASE_SLOT_RESERVATIONS);
    DR_ASSERT(res_bb_props == DRREG_SUCCESS);

    /* Tell drx_event_restore_state() that an expansion has occurred. */
    drx_mark_scatter_gather_expanded();

    reg_id_t scratch_gpr = DR_REG_INVALID;
    drvector_t allowed;
    drreg_init_and_fill_vector(&allowed, true);

    /* We need the scratch registers and base register app's value to be available at the
     * same time. Do not use.
     */
    drreg_set_vector_entry(&allowed, sg_info.base_reg, false);

    if (drreg_reserve_aflags(drcontext, bb, sg_instr) != DRREG_SUCCESS)
        goto drx_expand_scatter_gather_exit;
    if (drreg_reserve_register(drcontext, bb, sg_instr, &allowed, &scratch_gpr) !=
        DRREG_SUCCESS)
        goto drx_expand_scatter_gather_exit;

    const reg_id_t scratch_pred =
        reserve_pred_register(drcontext, bb, sg_instr, scratch_gpr);

    const app_pc orig_app_pc = instr_get_app_pc(sg_instr);

    emulated_instr_t emulated_instr;
    emulated_instr.size = sizeof(emulated_instr);
    emulated_instr.pc = instr_get_app_pc(sg_instr);
    emulated_instr.instr = sg_instr;
    /* Tools should instrument the data operations in the sequence. */
    emulated_instr.flags = DR_EMULATE_INSTR_ONLY;
    drmgr_insert_emulation_start(drcontext, bb, sg_instr, &emulated_instr);

    if (sg_info.is_load && reg_is_z(sg_info.index_reg)) {
        /* scalar+vector */
        expand_scalar_plus_vector(drcontext, bb, sg_instr, &sg_info, scratch_gpr,
                                  scratch_pred, orig_app_pc);
    } else {
        /* TODO i#5036
         * Add support for:
         *      Other scatter gather variants:
         *          scalar + vector st1*
         *          vector + immediate ld1/st1*
         *      Predicated contiguous variants:
         *          scalar + immediate ld1/st1*
         *          scalar + scalar ld1/st1*
         *      First fault and non-faulting variants:
         *          ldff1*, ldnf1*
         *      Multi-register variants:
         *          ld2*, ld3*, ld4*,
         *          st2*, st3*, st4*
         */
        goto drx_expand_scatter_gather_exit;
    }

    drmgr_insert_emulation_end(drcontext, bb, sg_instr);

    unreserve_pred_register(drcontext, bb, sg_instr, scratch_gpr, scratch_pred);
    if (drreg_unreserve_register(drcontext, bb, sg_instr, scratch_gpr) != DRREG_SUCCESS) {
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
    DR_ASSERT(instr_is_gather(sg_inst) || instr_is_scatter(sg_inst));
    /* TODO i#5365, i#5036: Restore the scratch predicate register.
     *                      We need to add support for handling SVE state during
     *                      signals first.
     */
    DR_ASSERT_MSG(false, "NYI i#5365 i#5036");
    return false;
}
