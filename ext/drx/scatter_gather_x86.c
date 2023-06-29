/* **********************************************************
 * Copyright (c) 2013-2023 Google, Inc.   All rights reserved.
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

/* DynamoRio eXtension utilities: Support for expanding x86 scatter and gather
 * instructions.
 */

#include "dr_api.h"
#include "drx.h"
#include "hashtable.h"
#include "../ext_utils.h"
#include <stddef.h> /* for offsetof */
#include <string.h>

/* We use drmgr but only internally.  A user of drx will end up loading in
 * the drmgr library, but it won't affect the user's code.
 */
#include "drmgr.h"

#include "drreg.h"

#ifdef UNIX
#    ifdef LINUX
#        include "../../core/unix/include/syscall.h"
#    else
#        include <sys/syscall.h>
#    endif
#    include <signal.h> /* SIGKILL */
#endif

#include <limits.h>

#ifdef DEBUG
#    define ASSERT(x, msg) DR_ASSERT_MSG(x, msg)
#    define IF_DEBUG(x) x
#else
#    define ASSERT(x, msg) /* nothing */
#    define IF_DEBUG(x)    /* nothing */
#endif                     /* DEBUG */

#define XMM_REG_SIZE 16
#define YMM_REG_SIZE 32
#define ZMM_REG_SIZE 64
/* For simplicity, we use the largest alignment required by the opcodes
 * returned by get_mov_scratch_mm_opcode_and_size.
 */
#define MM_ALIGNMENT 64

#define MAX(x, y) ((x) >= (y) ? (x) : (y))

#ifdef WINDOWS
#    define IF_WINDOWS_ELSE(x, y) (x)
#else
#    define IF_WINDOWS_ELSE(x, y) (y)
#endif

#define MINSERT instrlist_meta_preinsert
/* For inserting an app instruction, which must have a translation ("xl8") field. */
#define PREXL8 instrlist_preinsert

#define VERBOSE 0

static int tls_idx;
typedef struct _per_thread_t {
    void *scratch_mm_spill_slot;
    void *scratch_mm_spill_slot_aligned;
} per_thread_t;
static per_thread_t init_pt;

static int drx_scatter_gather_expanded;

static bool
drx_event_restore_state(void *drcontext, bool restore_memory,
                        dr_restore_state_info_t *info);

static per_thread_t *
get_tls_data(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    /* Support use during init (i#2910). */
    if (pt == NULL)
        return &init_pt;
    return pt;
}

static void
get_mov_scratch_mm_opcode_and_size(int *opcode_out, opnd_size_t *opnd_size_out)
{
    uint opcode;
    opnd_size_t opnd_size;
    /* We use same opcodes as used by fcache enter/return. */
    if (proc_avx512_enabled()) {
        /* ZMM enabled. */
        opcode = OP_vmovaps; /* Requires 64-byte alignment. */
        opnd_size = OPSZ_64;
    } else {
        /* YMM enabled. */
        ASSERT(proc_avx_enabled(), "Scatter/gather instrs not available");
        opcode = OP_vmovdqa; /* Requires 32-byte alignment. */
        opnd_size = OPSZ_32;
    }
    if (opcode_out != NULL)
        *opcode_out = opcode;
    if (opnd_size_out != NULL)
        *opnd_size_out = opnd_size;
}

static void
drx_thread_init(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)dr_thread_alloc(drcontext, sizeof(*pt));
    opnd_size_t mm_opsz;
    get_mov_scratch_mm_opcode_and_size(NULL, &mm_opsz);
    pt->scratch_mm_spill_slot =
        dr_thread_alloc(drcontext, opnd_size_in_bytes(mm_opsz) + (MM_ALIGNMENT - 1));
    pt->scratch_mm_spill_slot_aligned =
        (void *)ALIGN_FORWARD(pt->scratch_mm_spill_slot, MM_ALIGNMENT);
    drmgr_set_tls_field(drcontext, tls_idx, (void *)pt);
}

static void
drx_thread_exit(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    opnd_size_t mm_opsz;
    get_mov_scratch_mm_opcode_and_size(NULL, &mm_opsz);
    dr_thread_free(drcontext, pt->scratch_mm_spill_slot,
                   opnd_size_in_bytes(mm_opsz) + (MM_ALIGNMENT - 1));
    dr_thread_free(drcontext, pt, sizeof(*pt));
}

bool
drx_scatter_gather_init()
{
    drmgr_priority_t fault_priority = { sizeof(fault_priority),
                                        DRMGR_PRIORITY_NAME_DRX_FAULT, NULL, NULL,
                                        DRMGR_PRIORITY_FAULT_DRX };

    if (!drmgr_register_restore_state_ex_event_ex(drx_event_restore_state,
                                                  &fault_priority))
        return false;
    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1)
        return false;
    if (!drmgr_register_thread_init_event(drx_thread_init) ||
        !drmgr_register_thread_exit_event(drx_thread_exit))
        return false;

    return true;
}

void
drx_scatter_gather_exit()
{
    drmgr_unregister_tls_field(tls_idx);
}

typedef struct _scatter_gather_info_t {
    bool is_evex;
    bool is_load;
    opnd_size_t scalar_index_size;
    opnd_size_t scalar_value_size;
    opnd_size_t scatter_gather_size;
    reg_id_t mask_reg;
    reg_id_t base_reg;
    reg_id_t index_reg;
    union {
        reg_id_t gather_dst_reg;
        reg_id_t scatter_src_reg;
    };
    int disp;
    int scale;
} scatter_gather_info_t;

static void
get_scatter_gather_info(instr_t *instr, scatter_gather_info_t *sg_info)
{
    /* We detect whether the instruction is EVEX by looking at its potential mask
     * operand.
     */
    opnd_t dst0 = instr_get_dst(instr, 0);
    opnd_t src0 = instr_get_src(instr, 0);
    opnd_t src1 = instr_get_src(instr, 1);
    sg_info->is_evex = opnd_is_reg(src0) && reg_is_opmask(opnd_get_reg(src0));
    sg_info->mask_reg = sg_info->is_evex ? opnd_get_reg(src0) : opnd_get_reg(src1);
    ASSERT(!sg_info->is_evex ||
               (opnd_get_reg(instr_get_dst(instr, 1)) == opnd_get_reg(src0)),
           "Invalid gather instruction.");
    int opc = instr_get_opcode(instr);
    opnd_t memopnd;
    switch (opc) {
    case OP_vgatherdpd:
        sg_info->scalar_index_size = OPSZ_4;
        sg_info->scalar_value_size = OPSZ_8;
        sg_info->is_load = true;
        break;
    case OP_vgatherqpd:
        sg_info->scalar_index_size = OPSZ_8;
        sg_info->scalar_value_size = OPSZ_8;
        sg_info->is_load = true;
        break;
    case OP_vgatherdps:
        sg_info->scalar_index_size = OPSZ_4;
        sg_info->scalar_value_size = OPSZ_4;
        sg_info->is_load = true;
        break;
    case OP_vgatherqps:
        sg_info->scalar_index_size = OPSZ_8;
        sg_info->scalar_value_size = OPSZ_4;
        sg_info->is_load = true;
        break;
    case OP_vpgatherdd:
        sg_info->scalar_index_size = OPSZ_4;
        sg_info->scalar_value_size = OPSZ_4;
        sg_info->is_load = true;
        break;
    case OP_vpgatherqd:
        sg_info->scalar_index_size = OPSZ_8;
        sg_info->scalar_value_size = OPSZ_4;
        sg_info->is_load = true;
        break;
    case OP_vpgatherdq:
        sg_info->scalar_index_size = OPSZ_4;
        sg_info->scalar_value_size = OPSZ_8;
        sg_info->is_load = true;
        break;
    case OP_vpgatherqq:
        sg_info->scalar_index_size = OPSZ_8;
        sg_info->scalar_value_size = OPSZ_8;
        sg_info->is_load = true;
        break;
    case OP_vscatterdpd:
        sg_info->scalar_index_size = OPSZ_4;
        sg_info->scalar_value_size = OPSZ_8;
        sg_info->is_load = false;
        break;
    case OP_vscatterqpd:
        sg_info->scalar_index_size = OPSZ_8;
        sg_info->scalar_value_size = OPSZ_8;
        sg_info->is_load = false;
        break;
    case OP_vscatterdps:
        sg_info->scalar_index_size = OPSZ_4;
        sg_info->scalar_value_size = OPSZ_4;
        sg_info->is_load = false;
        break;
    case OP_vscatterqps:
        sg_info->scalar_index_size = OPSZ_8;
        sg_info->scalar_value_size = OPSZ_4;
        sg_info->is_load = false;
        break;
    case OP_vpscatterdd:
        sg_info->scalar_index_size = OPSZ_4;
        sg_info->scalar_value_size = OPSZ_4;
        sg_info->is_load = false;
        break;
    case OP_vpscatterqd:
        sg_info->scalar_index_size = OPSZ_8;
        sg_info->scalar_value_size = OPSZ_4;
        sg_info->is_load = false;
        break;
    case OP_vpscatterdq:
        sg_info->scalar_index_size = OPSZ_4;
        sg_info->scalar_value_size = OPSZ_8;
        sg_info->is_load = false;
        break;
    case OP_vpscatterqq:
        sg_info->scalar_index_size = OPSZ_8;
        sg_info->scalar_value_size = OPSZ_8;
        sg_info->is_load = false;
        break;
    default:
        ASSERT(false, "Incorrect opcode.");
        memopnd = opnd_create_null();
        break;
    }
    if (sg_info->is_load) {
        sg_info->scatter_gather_size = opnd_get_size(dst0);
        sg_info->gather_dst_reg = opnd_get_reg(dst0);
        memopnd = sg_info->is_evex ? src1 : src0;
    } else {
        sg_info->scatter_gather_size = opnd_get_size(src1);
        sg_info->scatter_src_reg = opnd_get_reg(src1);
        memopnd = dst0;
    }
    sg_info->index_reg = opnd_get_index(memopnd);
    sg_info->base_reg = opnd_get_base(memopnd);
    sg_info->disp = opnd_get_disp(memopnd);
    sg_info->scale = opnd_get_scale(memopnd);
}

static bool
expand_gather_insert_scalar(void *drcontext, instrlist_t *bb, instr_t *sg_instr, int el,
                            scatter_gather_info_t *sg_info, reg_id_t simd_reg,
                            reg_id_t scalar_reg, reg_id_t scratch_xmm, bool is_avx512,
                            app_pc orig_app_pc)
{
    /* Used by both AVX2 and AVX-512. */
    ASSERT(instr_is_gather(sg_instr), "Internal error: only gather instructions.");
    reg_id_t simd_reg_zmm = reg_resize_to_opsz(simd_reg, OPSZ_64);
    reg_id_t simd_reg_ymm = reg_resize_to_opsz(simd_reg, OPSZ_32);
    uint scalar_value_bytes = opnd_size_in_bytes(sg_info->scalar_value_size);
    int scalarxmmimm = el * scalar_value_bytes / XMM_REG_SIZE;
    if (is_avx512) {
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_vextracti32x4_mask(
                             drcontext, opnd_create_reg(scratch_xmm),
                             opnd_create_reg(DR_REG_K0),
                             opnd_create_immed_int(scalarxmmimm, OPSZ_1),
                             opnd_create_reg(simd_reg_zmm)),
                         orig_app_pc));
    } else {
        PREXL8(bb, sg_instr,
               INSTR_XL8(
                   INSTR_CREATE_vextracti128(drcontext, opnd_create_reg(scratch_xmm),
                                             opnd_create_reg(simd_reg_ymm),
                                             opnd_create_immed_int(scalarxmmimm, OPSZ_1)),
                   orig_app_pc));
    }
    if (sg_info->scalar_value_size == OPSZ_4) {
        PREXL8(
            bb, sg_instr,
            INSTR_XL8(
                INSTR_CREATE_vpinsrd(
                    drcontext, opnd_create_reg(scratch_xmm), opnd_create_reg(scratch_xmm),
                    opnd_create_reg(IF_X64_ELSE(reg_64_to_32(scalar_reg), scalar_reg)),
                    opnd_create_immed_int((el * scalar_value_bytes) % XMM_REG_SIZE /
                                              opnd_size_in_bytes(OPSZ_4),
                                          OPSZ_1)),
                orig_app_pc));
    } else if (sg_info->scalar_value_size == OPSZ_8) {
        ASSERT(reg_is_64bit(scalar_reg),
               "The qword index versions are unsupported in 32-bit mode.");
        PREXL8(
            bb, sg_instr,
            INSTR_XL8(INSTR_CREATE_vpinsrq(
                          drcontext, opnd_create_reg(scratch_xmm),
                          opnd_create_reg(scratch_xmm), opnd_create_reg(scalar_reg),
                          opnd_create_immed_int((el * scalar_value_bytes) % XMM_REG_SIZE /
                                                    opnd_size_in_bytes(OPSZ_8),
                                                OPSZ_1)),
                      orig_app_pc));

    } else {
        ASSERT(false, "Unexpected index size.");
    }
    if (is_avx512) {
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_vinserti32x4_mask(
                             drcontext, opnd_create_reg(simd_reg_zmm),
                             opnd_create_reg(DR_REG_K0),
                             opnd_create_immed_int(scalarxmmimm, OPSZ_1),
                             opnd_create_reg(simd_reg_zmm), opnd_create_reg(scratch_xmm)),
                         orig_app_pc));
    } else {
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_vinserti128(
                             drcontext, opnd_create_reg(simd_reg_ymm),
                             opnd_create_reg(simd_reg_ymm), opnd_create_reg(scratch_xmm),
                             opnd_create_immed_int(scalarxmmimm, OPSZ_1)),
                         orig_app_pc));
    }
    return true;
}

static bool
expand_avx512_gather_insert_scalar_value(void *drcontext, instrlist_t *bb,
                                         instr_t *sg_instr, int el,
                                         scatter_gather_info_t *sg_info,
                                         reg_id_t scalar_value_reg, reg_id_t scratch_xmm,
                                         app_pc orig_app_pc)
{
    return expand_gather_insert_scalar(drcontext, bb, sg_instr, el, sg_info,
                                       sg_info->gather_dst_reg, scalar_value_reg,
                                       scratch_xmm, true /* AVX-512 */, orig_app_pc);
}

static bool
expand_avx2_gather_insert_scalar_value(void *drcontext, instrlist_t *bb,
                                       instr_t *sg_instr, int el,
                                       scatter_gather_info_t *sg_info,
                                       reg_id_t scalar_value_reg, reg_id_t scratch_xmm,
                                       app_pc orig_app_pc)
{
    return expand_gather_insert_scalar(drcontext, bb, sg_instr, el, sg_info,
                                       sg_info->gather_dst_reg, scalar_value_reg,
                                       scratch_xmm, false /* AVX2 */, orig_app_pc);
}

static bool
expand_avx2_gather_insert_scalar_mask(void *drcontext, instrlist_t *bb, instr_t *sg_instr,
                                      int el, scatter_gather_info_t *sg_info,
                                      reg_id_t scalar_index_reg, reg_id_t scratch_xmm,
                                      app_pc orig_app_pc)
{
    return expand_gather_insert_scalar(drcontext, bb, sg_instr, el, sg_info,
                                       sg_info->mask_reg, scalar_index_reg, scratch_xmm,
                                       false /* AVX2 */, orig_app_pc);
}

static bool
expand_scatter_gather_extract_scalar(void *drcontext, instrlist_t *bb, instr_t *sg_instr,
                                     int el, scatter_gather_info_t *sg_info,
                                     opnd_size_t scalar_size, uint scalar_bytes,
                                     reg_id_t from_simd_reg, reg_id_t scratch_xmm,
                                     reg_id_t scratch_reg, bool is_avx512,
                                     app_pc orig_app_pc)
{
    reg_id_t from_simd_reg_zmm = reg_resize_to_opsz(from_simd_reg, OPSZ_64);
    reg_id_t from_simd_reg_ymm = reg_resize_to_opsz(from_simd_reg, OPSZ_32);
    int scalarxmmimm = el * scalar_bytes / XMM_REG_SIZE;
    if (is_avx512) {
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_vextracti32x4_mask(
                             drcontext, opnd_create_reg(scratch_xmm),
                             opnd_create_reg(DR_REG_K0),
                             opnd_create_immed_int(scalarxmmimm, OPSZ_1),
                             opnd_create_reg(from_simd_reg_zmm)),
                         orig_app_pc));
    } else {
        PREXL8(bb, sg_instr,
               INSTR_XL8(
                   INSTR_CREATE_vextracti128(drcontext, opnd_create_reg(scratch_xmm),
                                             opnd_create_reg(from_simd_reg_ymm),
                                             opnd_create_immed_int(scalarxmmimm, OPSZ_1)),
                   orig_app_pc));
    }
    if (scalar_size == OPSZ_4) {
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_vpextrd(
                             drcontext,
                             opnd_create_reg(
                                 IF_X64_ELSE(reg_64_to_32(scratch_reg), scratch_reg)),
                             opnd_create_reg(scratch_xmm),
                             opnd_create_immed_int((el * scalar_bytes) % XMM_REG_SIZE /
                                                       opnd_size_in_bytes(OPSZ_4),
                                                   OPSZ_1)),
                         orig_app_pc));
    } else if (scalar_size == OPSZ_8) {
        ASSERT(reg_is_64bit(scratch_reg),
               "The qword index versions are unsupported in 32-bit mode.");
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_vpextrq(
                             drcontext, opnd_create_reg(scratch_reg),
                             opnd_create_reg(scratch_xmm),
                             opnd_create_immed_int((el * scalar_bytes) % XMM_REG_SIZE /
                                                       opnd_size_in_bytes(OPSZ_8),
                                                   OPSZ_1)),
                         orig_app_pc));
    } else {
        ASSERT(false, "Unexpected scalar size.");
        return false;
    }
    return true;
}

static bool
expand_avx512_scatter_extract_scalar_value(void *drcontext, instrlist_t *bb,
                                           instr_t *sg_instr, int el,
                                           scatter_gather_info_t *sg_info,
                                           reg_id_t scratch_xmm, reg_id_t scratch_reg,
                                           app_pc orig_app_pc)
{
    return expand_scatter_gather_extract_scalar(
        drcontext, bb, sg_instr, el, sg_info, sg_info->scalar_value_size,
        opnd_size_in_bytes(sg_info->scalar_value_size), sg_info->scatter_src_reg,
        scratch_xmm, scratch_reg, true /* AVX-512 */, orig_app_pc);
}

static bool
expand_avx512_scatter_gather_extract_scalar_index(void *drcontext, instrlist_t *bb,
                                                  instr_t *sg_instr, int el,
                                                  scatter_gather_info_t *sg_info,
                                                  reg_id_t scratch_xmm,
                                                  reg_id_t scratch_reg,
                                                  app_pc orig_app_pc)
{
    return expand_scatter_gather_extract_scalar(
        drcontext, bb, sg_instr, el, sg_info, sg_info->scalar_index_size,
        opnd_size_in_bytes(sg_info->scalar_index_size), sg_info->index_reg, scratch_xmm,
        scratch_reg, true /* AVX-512 */, orig_app_pc);
}

static bool
expand_avx2_gather_extract_scalar_index(void *drcontext, instrlist_t *bb,
                                        instr_t *sg_instr, int el,
                                        scatter_gather_info_t *sg_info,
                                        reg_id_t scratch_xmm, reg_id_t scratch_reg,
                                        app_pc orig_app_pc)
{
    return expand_scatter_gather_extract_scalar(
        drcontext, bb, sg_instr, el, sg_info, sg_info->scalar_index_size,
        opnd_size_in_bytes(sg_info->scalar_index_size), sg_info->index_reg, scratch_xmm,
        scratch_reg, false /* AVX2 */, orig_app_pc);
}

static bool
expand_avx512_scatter_gather_update_mask(void *drcontext, instrlist_t *bb,
                                         instr_t *sg_instr, int el,
                                         scatter_gather_info_t *sg_info,
                                         reg_id_t scratch_reg, app_pc orig_app_pc,
                                         drvector_t *allowed)
{
    reg_id_t save_mask_reg;
    PREXL8(bb, sg_instr,
           INSTR_XL8(INSTR_CREATE_mov_imm(drcontext,
                                          opnd_create_reg(IF_X64_ELSE(
                                              reg_64_to_32(scratch_reg), scratch_reg)),
                                          OPND_CREATE_INT32(1 << el)),
                     orig_app_pc));
    if (drreg_reserve_register(drcontext, bb, sg_instr, allowed, &save_mask_reg) !=
        DRREG_SUCCESS)
        return false;
    /* The scratch k register we're using here is always k0, because it is never
     * used for scatter/gather.
     */
    MINSERT(bb, sg_instr,
            INSTR_CREATE_kmovw(
                drcontext,
                opnd_create_reg(IF_X64_ELSE(reg_64_to_32(save_mask_reg), save_mask_reg)),
                opnd_create_reg(DR_REG_K0)));
    PREXL8(bb, sg_instr,
           INSTR_XL8(INSTR_CREATE_kmovw(drcontext, opnd_create_reg(DR_REG_K0),
                                        opnd_create_reg(IF_X64_ELSE(
                                            reg_64_to_32(scratch_reg), scratch_reg))),
                     orig_app_pc));
    PREXL8(bb, sg_instr,
           INSTR_XL8(INSTR_CREATE_kandnw(drcontext, opnd_create_reg(sg_info->mask_reg),
                                         opnd_create_reg(DR_REG_K0),
                                         opnd_create_reg(sg_info->mask_reg)),
                     orig_app_pc));
    MINSERT(bb, sg_instr,
            INSTR_CREATE_kmovw(drcontext, opnd_create_reg(DR_REG_K0),
                               opnd_create_reg(IF_X64_ELSE(reg_64_to_32(save_mask_reg),
                                                           save_mask_reg))));
    if (drreg_unreserve_register(drcontext, bb, sg_instr, save_mask_reg) !=
        DRREG_SUCCESS) {
        ASSERT(false, "drreg_unreserve_register should not fail");
        return false;
    }
    return true;
}

static bool
expand_avx2_gather_update_mask(void *drcontext, instrlist_t *bb, instr_t *sg_instr,
                               int el, scatter_gather_info_t *sg_info,
                               reg_id_t scratch_xmm, reg_id_t scratch_reg,
                               app_pc orig_app_pc)
{
    /* The width of the mask element and data element is identical per definition of the
     * instruction.
     */
    if (sg_info->scalar_value_size == OPSZ_4) {
        PREXL8(
            bb, sg_instr,
            INSTR_XL8(
                INSTR_CREATE_xor(
                    drcontext,
                    opnd_create_reg(IF_X64_ELSE(reg_64_to_32(scratch_reg), scratch_reg)),
                    opnd_create_reg(IF_X64_ELSE(reg_64_to_32(scratch_reg), scratch_reg))),
                orig_app_pc));
    } else if (sg_info->scalar_value_size == OPSZ_8) {
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_xor(drcontext, opnd_create_reg(scratch_reg),
                                          opnd_create_reg(scratch_reg)),
                         orig_app_pc));
    }
    reg_id_t null_index_reg = scratch_reg;
    if (!expand_avx2_gather_insert_scalar_mask(drcontext, bb, sg_instr, el, sg_info,
                                               null_index_reg, scratch_xmm, orig_app_pc))
        return false;
    return true;
}

static bool
expand_avx2_gather_make_test(void *drcontext, instrlist_t *bb, instr_t *sg_instr, int el,
                             scatter_gather_info_t *sg_info, reg_id_t scratch_xmm,
                             reg_id_t scratch_reg, instr_t *skip_label,
                             app_pc orig_app_pc)
{
    /* The width of the mask element and data element is identical per definition of the
     * instruction.
     */
    expand_scatter_gather_extract_scalar(
        drcontext, bb, sg_instr, el, sg_info, sg_info->scalar_value_size,
        opnd_size_in_bytes(sg_info->scalar_value_size), sg_info->mask_reg, scratch_xmm,
        scratch_reg, false /* AVX2 */, orig_app_pc);
    if (sg_info->scalar_value_size == OPSZ_4) {
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_shr(drcontext,
                                          opnd_create_reg(IF_X64_ELSE(
                                              reg_64_to_32(scratch_reg), scratch_reg)),
                                          OPND_CREATE_INT8(31)),
                         orig_app_pc));
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_and(drcontext,
                                          opnd_create_reg(IF_X64_ELSE(
                                              reg_64_to_32(scratch_reg), scratch_reg)),
                                          OPND_CREATE_INT32(1)),
                         orig_app_pc));
    } else if (sg_info->scalar_value_size == OPSZ_8) {
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_shr(drcontext, opnd_create_reg(scratch_reg),
                                          OPND_CREATE_INT8(63)),
                         orig_app_pc));
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_and(drcontext, opnd_create_reg(scratch_reg),
                                          OPND_CREATE_INT32(1)),
                         orig_app_pc));
    }
    PREXL8(bb, sg_instr,
           INSTR_XL8(INSTR_CREATE_jcc(drcontext, OP_jz, opnd_create_instr(skip_label)),
                     orig_app_pc));
    return true;
}

static bool
expand_avx512_scatter_gather_make_test(void *drcontext, instrlist_t *bb,
                                       instr_t *sg_instr, int el,
                                       scatter_gather_info_t *sg_info,
                                       reg_id_t scratch_reg, instr_t *skip_label,
                                       app_pc orig_app_pc)
{
    PREXL8(bb, sg_instr,
           INSTR_XL8(INSTR_CREATE_kmovw(drcontext,
                                        opnd_create_reg(IF_X64_ELSE(
                                            reg_64_to_32(scratch_reg), scratch_reg)),
                                        opnd_create_reg(sg_info->mask_reg)),
                     orig_app_pc));
    PREXL8(bb, sg_instr,
           INSTR_XL8(INSTR_CREATE_test(drcontext,
                                       opnd_create_reg(IF_X64_ELSE(
                                           reg_64_to_32(scratch_reg), scratch_reg)),
                                       OPND_CREATE_INT32(1 << el)),
                     orig_app_pc));
    PREXL8(bb, sg_instr,
           INSTR_XL8(INSTR_CREATE_jcc(drcontext, OP_jz, opnd_create_instr(skip_label)),
                     orig_app_pc));
    return true;
}

static bool
expand_avx512_scatter_store_scalar_value(void *drcontext, instrlist_t *bb,
                                         instr_t *sg_instr,
                                         scatter_gather_info_t *sg_info,
                                         reg_id_t scalar_index_reg,
                                         reg_id_t scalar_value_reg, app_pc orig_app_pc)
{
    if (sg_info->base_reg == IF_X64_ELSE(DR_REG_RAX, DR_REG_EAX)) {
        /* We need the app's base register value. If it's xax, then it may be used to
         * store flags by drreg.
         */
        drreg_get_app_value(drcontext, bb, sg_instr, sg_info->base_reg,
                            sg_info->base_reg);
    }
#ifdef X64
    if (sg_info->scalar_index_size == OPSZ_4) {
        PREXL8(bb, sg_instr,
               INSTR_XL8(
                   INSTR_CREATE_movsxd(drcontext, opnd_create_reg(scalar_index_reg),
                                       opnd_create_reg(reg_64_to_32(scalar_index_reg))),
                   orig_app_pc));
    }
#endif
    if (sg_info->scalar_value_size == OPSZ_4) {
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_mov_st(
                             drcontext,
                             opnd_create_base_disp(sg_info->base_reg, scalar_index_reg,
                                                   sg_info->scale, sg_info->disp, OPSZ_4),
                             opnd_create_reg(IF_X64_ELSE(reg_64_to_32(scalar_value_reg),
                                                         scalar_value_reg))),
                         orig_app_pc));
    } else if (sg_info->scalar_value_size == OPSZ_8) {
        ASSERT(reg_is_64bit(scalar_index_reg),
               "Internal error: scratch index register not 64-bit.");
        ASSERT(reg_is_64bit(scalar_value_reg),
               "Internal error: scratch value register not 64-bit.");
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_mov_st(
                             drcontext,
                             opnd_create_base_disp(sg_info->base_reg, scalar_index_reg,
                                                   sg_info->scale, sg_info->disp, OPSZ_8),
                             opnd_create_reg(scalar_value_reg)),
                         orig_app_pc));
    } else {
        ASSERT(false, "Unexpected index size.");
        return false;
    }
    return true;
}

static bool
expand_gather_load_scalar_value(void *drcontext, instrlist_t *bb, instr_t *sg_instr,
                                scatter_gather_info_t *sg_info, reg_id_t scalar_index_reg,
                                app_pc orig_app_pc)
{
    if (sg_info->base_reg == IF_X64_ELSE(DR_REG_RAX, DR_REG_EAX)) {
        /* We need the app's base register value. If it's xax, then it may be used to
         * store flags by drreg.
         */
        drreg_get_app_value(drcontext, bb, sg_instr, sg_info->base_reg,
                            sg_info->base_reg);
    }
#ifdef X64
    if (sg_info->scalar_index_size == OPSZ_4) {
        PREXL8(bb, sg_instr,
               INSTR_XL8(
                   INSTR_CREATE_movsxd(drcontext, opnd_create_reg(scalar_index_reg),
                                       opnd_create_reg(reg_64_to_32(scalar_index_reg))),
                   orig_app_pc));
    }
#endif
    if (sg_info->scalar_value_size == OPSZ_4) {
        PREXL8(
            bb, sg_instr,
            INSTR_XL8(INSTR_CREATE_mov_ld(
                          drcontext,
                          opnd_create_reg(IF_X64_ELSE(reg_64_to_32(scalar_index_reg),
                                                      scalar_index_reg)),
                          opnd_create_base_disp(sg_info->base_reg, scalar_index_reg,
                                                sg_info->scale, sg_info->disp, OPSZ_4)),
                      orig_app_pc));
    } else if (sg_info->scalar_value_size == OPSZ_8) {
        ASSERT(reg_is_64bit(scalar_index_reg),
               "Internal error: scratch register not 64-bit.");
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(scalar_index_reg),
                                             opnd_create_base_disp(
                                                 sg_info->base_reg, scalar_index_reg,
                                                 sg_info->scale, sg_info->disp, OPSZ_8)),
                         orig_app_pc));
    } else {
        ASSERT(false, "Unexpected index size.");
        return false;
    }
    return true;
}

/*****************************************************************************************
 * drx_expand_scatter_gather()
 *
 * The function expands scatter and gather instructions to a sequence of equivalent
 * scalar operations. Gather instructions are expanded into a sequence of mask register
 * bit tests, extracting the index value, a scalar load, inserting the scalar value into
 * the destination simd register, and mask register bit updates. Scatter instructions
 * are similarly expanded into a sequence, but deploy a scalar store. Registers spilled
 * and restored by drreg are not illustrated in the sequence below.
 *
 * ------------------------------------------------------------------------------
 * AVX2 vpgatherdd, vgatherdps, vpgatherdq, vgatherdpd, vpgatherqd, vgatherqps, |
 * vpgatherqq, vgatherqpd:                                                      |
 * ------------------------------------------------------------------------------
 *
 * vpgatherdd (%rax,%ymm1,4)[4byte] %ymm2 -> %ymm0 %ymm2 sequence laid out here,
 * others are similar:
 *
 * Extract mask dword. qword versions use vpextrq:
 *   vextracti128   %ymm2 $0x00 -> %xmm3
 *   vpextrd        %xmm3 $0x00 -> %ecx
 * Test mask bit:
 *   shr            $0x0000001f %ecx -> %ecx
 *   and            $0x00000001 %ecx -> %ecx
 * Skip element if mask not set:
 *   jz             <skip0>
 * Extract index dword. qword versions use vpextrq:
 *   vextracti128   %ymm1 $0x00 -> %xmm3
 *   vpextrd        %xmm3 $0x00 -> %ecx
 * Restore app's base register value (may not be present):
 *   mov            %rax -> %gs:0x00000090[8byte]
 *   mov            %gs:0x00000098[8byte] -> %rax
 * Load scalar value:
 *   mov            (%rax,%rcx,4)[4byte] -> %ecx
 * Insert scalar value in destination register:
 *   vextracti128   %ymm0 $0x00 -> %xmm3
 *   vpinsrd        %xmm3 %ecx $0x00 -> %xmm3
 *   vinserti128    %ymm0 %xmm3 $0x00 -> %ymm0
 * Set mask dword to zero:
 *   xor            %ecx %ecx -> %ecx
 *   vextracti128   %ymm2 $0x00 -> %xmm3
 *   vpinsrd        %xmm3 %ecx $0x00 -> %xmm3
 *   vinserti128    %ymm2 %xmm3 $0x00 -> %ymm2
 *   skip0:
 * Do the same as above for the next element:
 *   vextracti128   %ymm2 $0x00 -> %xmm3
 *   vpextrd        %xmm3 $0x01 -> %ecx
 *   shr            $0x0000001f %ecx -> %ecx
 *   and            $0x00000001 %ecx -> %ecx
 *   jz             <skip1>
 *   vextracti128   %ymm1 $0x00 -> %xmm3
 *   vpextrd        %xmm3 $0x01 -> %ecx
 *   mov            (%rax,%rcx,4)[4byte] -> %ecx
 *   vextracti128   %ymm0 $0x00 -> %xmm3
 *   vpinsrd        %xmm3 %ecx $0x01 -> %xmm3
 *   vinserti128    %ymm0 %xmm3 $0x00 -> %ymm0
 *   xor            %ecx %ecx -> %ecx
 *   vextracti128   %ymm2 $0x00 -> %xmm3
 *   vpinsrd        %xmm3 %ecx $0x01 -> %xmm3
 *   vinserti128    %ymm2 %xmm3 $0x00 -> %ymm2
 *   skip1:
 *   [..]
 * Do the same as above for the last element:
 *   vextracti128   %ymm2 $0x01 -> %xmm3
 *   vpextrd        %xmm3 $0x03 -> %ecx
 *   shr            $0x0000001f %ecx -> %ecx
 *   and            $0x00000001 %ecx -> %ecx
 *   jz             <skip7>
 *   vextracti128   %ymm1 $0x01 -> %xmm3
 *   vpextrd        %xmm3 $0x03 -> %ecx
 *   mov            (%rax,%rcx,4)[4byte] -> %ecx
 *   vextracti128   %ymm0 $0x01 -> %xmm3
 *   vpinsrd        %xmm3 %ecx $0x03 -> %xmm3
 *   vinserti128    %ymm0 %xmm3 $0x01 -> %ymm0
 *   xor            %ecx %ecx -> %ecx
 *   vextracti128   %ymm2 $0x01 -> %xmm3
 *   vpinsrd        %xmm3 %ecx $0x03 -> %xmm3
 *   vinserti128    %ymm2 %xmm3 $0x01 -> %ymm2
 *   skip7:
 * Finally, clear the entire mask register, even
 * the parts that are not used as a mask:
 *   vpxor          %ymm2 %ymm2 -> %ymm2
 *
 * ---------------------------------------------------------------------------------
 * AVX-512 vpgatherdd, vgatherdps, vpgatherdq, vgatherdpd, vpgatherqd, vgatherqps, |
 * vpgatherqq, vgatherqpd:                                                         |
 * ---------------------------------------------------------------------------------
 *
 * vpgatherdd {%k1} (%rax,%zmm1,4)[4byte] -> %zmm0 %k1 sequence laid out here,
 * others are similar:
 *
 * Extract mask bit:
 *   kmovw           %k1 -> %ecx
 * Test mask bit:
 *   test            %ecx $0x00000001
 * Skip element if mask not set:
 *   jz              <skip0>
 * Extract index dword. qword versions use vpextrq:
 *   vextracti32x4   {%k0} $0x00 %zmm1 -> %xmm2
 *   vpextrd         %xmm2 $0x00 -> %ecx
 * Restore app's base register value (may not be present):
 *   mov             %rax -> %gs:0x00000090[8byte]
 *   mov             %gs:0x00000098[8byte] -> %rax
 * Load scalar value:
 *   mov             (%rax,%rcx,4)[4byte] -> %ecx
 * Insert scalar value in destination register:
 *   vextracti32x4   {%k0} $0x00 %zmm0 -> %xmm2
 *   vpinsrd         %xmm2 %ecx $0x00 -> %xmm2
 *   vinserti32x4    {%k0} $0x00 %zmm0 %xmm2 -> %zmm0
 * Set mask bit to zero:
 *   mov             $0x00000001 -> %ecx
 * %k0 is saved to a gpr here, while the gpr
 * is managed by drreg. This is not further
 * layed out in this example.
 *   kmovw           %ecx -> %k0
 *   kandnw          %k0 %k1 -> %k1
 * It is not illustrated that %k0 is restored here.
 *   skip0:
 * Do the same as above for the next element:
 *   kmovw           %k1 -> %ecx
 *   test            %ecx $0x00000002
 *   jz              <skip1>
 *   vextracti32x4   {%k0} $0x00 %zmm1 -> %xmm2
 *   vpextrd         %xmm2 $0x01 -> %ecx
 *   mov             (%rax,%rcx,4)[4byte] -> %ecx
 *   vextracti32x4   {%k0} $0x00 %zmm0 -> %xmm2
 *   vpinsrd         %xmm2 %ecx $0x01 -> %xmm2
 *   vinserti32x4    {%k0} $0x00 %zmm0 %xmm2 -> %zmm0
 *   mov             $0x00000002 -> %ecx
 *   kmovw           %ecx -> %k0
 *   kandnw          %k0 %k1 -> %k1
 *   skip1:
 *   [..]
 * Do the same as above for the last element:
 *   kmovw           %k1 -> %ecx
 *   test            %ecx $0x00008000
 *   jz              <skip15>
 *   vextracti32x4   {%k0} $0x03 %zmm1 -> %xmm2
 *   vpextrd         %xmm2 $0x03 -> %ecx
 *   mov             (%rax,%rcx,4)[4byte] -> %ecx
 *   vextracti32x4   {%k0} $0x03 %zmm0 -> %xmm2
 *   vpinsrd         %xmm2 %ecx $0x03 -> %xmm2
 *   vinserti32x4    {%k0} $0x03 %zmm0 %xmm2 -> %zmm0
 *   mov             $0x00008000 -> %ecx
 *   kmovw           %ecx -> %k0
 *   kandnw          %k0 %k1 -> %k1
 *   skip15:
 * Finally, clear the entire mask register, even
 * the parts that are not used as a mask:
 *   kxorq           %k1 %k1 -> %k1
 *
 * --------------------------------------------------------------------------
 * AVX-512 vpscatterdd, vscatterdps, vpscatterdq, vscatterdpd, vpscatterqd, |
 * vscatterqps, vpscatterqq, vscatterqpd:                                   |
 * --------------------------------------------------------------------------
 *
 * vpscatterdd {%k1} %zmm0 -> (%rcx,%zmm1,4)[4byte] %k1 sequence laid out here,
 * others are similar:
 *
 * Extract mask bit:
 *   kmovw           %k1 -> %edx
 * Test mask bit:
 *   test            %edx $0x00000001
 * Skip element if mask not set:
 *   jz              <skip0>
 * Extract index dword. qword versions use vpextrq:
 *   vextracti32x4   {%k0} $0x00 %zmm1 -> %xmm2
 *   vpextrd         %xmm2 $0x00 -> %edx
 * Extract scalar value dword. qword versions use vpextrq:
 *   vextracti32x4   {%k0} $0x00 %zmm0 -> %xmm2
 *   vpextrd         %xmm2 $0x00 -> %ebx
 * Store scalar value:
 *   mov             %ebx -> (%rcx,%rdx,4)[4byte]
 * Set mask bit to zero:
 *   mov             $0x00000001 -> %edx
 *   kmovw           %edx -> %k0
 *   kandnw          %k0 %k1 -> %k1
 *   skip0:
 * Do the same as above for the next element:
 *   kmovw           %k1 -> %edx
 *   test            %edx $0x00000002
 *   jz              <skip1>
 *   vextracti32x4   {%k0} $0x00 %zmm1 -> %xmm2
 *   vpextrd         %xmm2 $0x01 -> %edx
 *   vextracti32x4   {%k0} $0x00 %zmm0 -> %xmm2
 *   vpextrd         %xmm2 $0x01 -> %ebx
 *   mov             %ebx -> (%rcx,%rdx,4)[4byte]
 *   mov             $0x00000002 -> %edx
 *   kmovw           %edx -> %k0
 *   kandnw          %k0 %k1 -> %k1
 *   skip1:
 *   [..]
 * Do the same as above for the last element:
 *   kmovw           %k1 -> %edx
 *   test            %edx $0x00008000
 *   jz              <skip15>
 *   vextracti32x4   {%k0} $0x03 %zmm1 -> %xmm2
 *   vpextrd         %xmm2 $0x03 -> %edx
 *   vextracti32x4   {%k0} $0x03 %zmm0 -> %xmm2
 *   vpextrd         %xmm2 $0x03 -> %ebx
 *   mov             %ebx -> (%rcx,%rdx,4)[4byte]
 *   mov             $0x00008000 -> %edx
 *   kmovw           %edx -> %k0
 *   kandnw          %k0 %k1 -> %k1
 *   skip15:
 * Finally, clear the entire mask register, even
 * the parts that are not used as a mask:
 *   kxorq           %k1 %k1 -> %k1
 *
 * For more design details see the following document:
 * https://dynamorio.org/page_scatter_gather_emulation.html
 */
bool
drx_expand_scatter_gather(void *drcontext, instrlist_t *bb, OUT bool *expanded)
{
    instr_t *instr, *next_instr, *first_app = NULL;
    bool delete_rest = false;

    if (expanded != NULL)
        *expanded = false;
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_APP2APP) {
        return false;
    }

    /* Make each scatter or gather instruction be in their own basic block.
     * TODO i#3837: cross-platform code like the following bb splitting can be shared
     * with other architectures in the future.
     */
    for (instr = instrlist_first(bb); instr != NULL; instr = next_instr) {
        next_instr = instr_get_next(instr);
        if (delete_rest) {
            instrlist_remove(bb, instr);
            instr_destroy(drcontext, instr);
        } else if (instr_is_app(instr)) {
            if (first_app == NULL)
                first_app = instr;
            if (instr_is_gather(instr) || instr_is_scatter(instr)) {
                delete_rest = true;
                if (instr != first_app) {
                    instrlist_remove(bb, instr);
                    instr_destroy(drcontext, instr);
                }
            }
        }
    }
    if (first_app == NULL)
        return true;
    if (!instr_is_gather(first_app) && !instr_is_scatter(first_app))
        return true;

    /* We want to avoid spill slot conflicts with later instrumentation passes. */
    drreg_status_t res_bb_props =
        drreg_set_bb_properties(drcontext, DRREG_HANDLE_MULTI_PHASE_SLOT_RESERVATIONS);
    DR_ASSERT(res_bb_props == DRREG_SUCCESS);

    dr_atomic_store32(&drx_scatter_gather_expanded, 1);

    instr_t *sg_instr = first_app;
    scatter_gather_info_t sg_info;
    bool res = false;
    /* XXX: we may want to make this function public, as it may be useful to clients. */
    get_scatter_gather_info(sg_instr, &sg_info);
#ifndef X64
    if (sg_info.scalar_index_size == OPSZ_8 || sg_info.scalar_value_size == OPSZ_8) {
        /* FIXME i#2985: we do not yet support expansion of the qword index and value
         * scatter/gather versions in 32-bit mode.
         */
        return false;
    }
#endif
    uint no_of_elements = opnd_size_in_bytes(sg_info.scatter_gather_size) /
        MAX(opnd_size_in_bytes(sg_info.scalar_index_size),
            opnd_size_in_bytes(sg_info.scalar_value_size));
    reg_id_t scratch_reg0 = DR_REG_INVALID, scratch_reg1 = DR_REG_INVALID;
    drvector_t allowed;
    drreg_init_and_fill_vector(&allowed, true);
    /* We need the scratch registers and base register app's value to be available at the
     * same time. Do not use.
     */
    drreg_set_vector_entry(&allowed, sg_info.base_reg, false);
    if (drreg_reserve_aflags(drcontext, bb, sg_instr) != DRREG_SUCCESS)
        goto drx_expand_scatter_gather_exit;
    if (drreg_reserve_register(drcontext, bb, sg_instr, &allowed, &scratch_reg0) !=
        DRREG_SUCCESS)
        goto drx_expand_scatter_gather_exit;
    if (instr_is_scatter(sg_instr)) {
        if (drreg_reserve_register(drcontext, bb, sg_instr, &allowed, &scratch_reg1) !=
            DRREG_SUCCESS)
            goto drx_expand_scatter_gather_exit;
    }
    app_pc orig_app_pc = instr_get_app_pc(sg_instr);
    reg_id_t scratch_xmm;
    /* Search the instruction for an unused xmm register we will use as a temp.
     * Modify scatter-gather tests if the criteria for picking the scratch xmm changes.
     */
    for (scratch_xmm = DR_REG_START_XMM; scratch_xmm <= DR_REG_STOP_XMM; ++scratch_xmm) {
        if ((sg_info.is_evex ||
             scratch_xmm != reg_resize_to_opsz(sg_info.mask_reg, OPSZ_16)) &&
            scratch_xmm != reg_resize_to_opsz(sg_info.index_reg, OPSZ_16) &&
            /* redundant with scatter_src_reg */
            scratch_xmm != reg_resize_to_opsz(sg_info.gather_dst_reg, OPSZ_16))
            break;
    }
    /* Spill the scratch mm reg. We spill the largest reg corresponding to scratch_xmm
     * that is supported by the system. This is required because mov-ing a part of a
     * ymm/zmm reg zeroes the remaining automatically. So we need to save the complete
     * ymm/zmm reg and not just the lower xmm bits.
     * TODO i#3844: drreg does not support spilling mm regs yet, so we do it ourselves.
     * When that support is available, replace the following with the required drreg API
     * calls.
     */
    int mov_scratch_mm_opcode;
    opnd_size_t mov_scratch_mm_opnd_sz;
    reg_id_t scratch_mm;
    get_mov_scratch_mm_opcode_and_size(&mov_scratch_mm_opcode, &mov_scratch_mm_opnd_sz);
    scratch_mm = reg_resize_to_opsz(scratch_xmm, mov_scratch_mm_opnd_sz);

    drmgr_insert_read_tls_field(drcontext, tls_idx, bb, sg_instr, scratch_reg0);
    instrlist_meta_preinsert(
        bb, sg_instr,
        INSTR_CREATE_mov_ld(
            drcontext, opnd_create_reg(scratch_reg0),
            OPND_CREATE_MEMPTR(scratch_reg0,
                               offsetof(per_thread_t, scratch_mm_spill_slot_aligned))));

    if (mov_scratch_mm_opnd_sz == OPSZ_64) {
        instrlist_meta_preinsert(
            bb, sg_instr,
            instr_create_1dst_2src(drcontext, mov_scratch_mm_opcode,
                                   opnd_create_base_disp(scratch_reg0, DR_REG_NULL, 0, 0,
                                                         mov_scratch_mm_opnd_sz),
                                   /* k0 denotes unmasked operation. */
                                   opnd_create_reg(DR_REG_K0),
                                   opnd_create_reg(scratch_mm)));
    } else {
        instrlist_meta_preinsert(
            bb, sg_instr,
            instr_create_1dst_1src(drcontext, mov_scratch_mm_opcode,
                                   opnd_create_base_disp(scratch_reg0, DR_REG_NULL, 0, 0,
                                                         mov_scratch_mm_opnd_sz),
                                   opnd_create_reg(scratch_mm)));
    }

    emulated_instr_t emulated_instr;
    emulated_instr.size = sizeof(emulated_instr);
    emulated_instr.pc = instr_get_app_pc(sg_instr);
    emulated_instr.instr = sg_instr;
    /* Tools should instrument the data operations in the sequence. */
    emulated_instr.flags = DR_EMULATE_INSTR_ONLY;
    drmgr_insert_emulation_start(drcontext, bb, sg_instr, &emulated_instr);

    if (sg_info.is_evex) {
        if (/* AVX-512 */ instr_is_gather(sg_instr)) {
            for (uint el = 0; el < no_of_elements; ++el) {
                instr_t *skip_label = INSTR_CREATE_label(drcontext);
                if (!expand_avx512_scatter_gather_make_test(drcontext, bb, sg_instr, el,
                                                            &sg_info, scratch_reg0,
                                                            skip_label, orig_app_pc))
                    goto drx_expand_scatter_gather_exit;
                if (!expand_avx512_scatter_gather_extract_scalar_index(
                        drcontext, bb, sg_instr, el, &sg_info, scratch_xmm, scratch_reg0,
                        orig_app_pc))
                    goto drx_expand_scatter_gather_exit;
                reg_id_t scalar_index_reg = scratch_reg0;
                if (!expand_gather_load_scalar_value(drcontext, bb, sg_instr, &sg_info,
                                                     scalar_index_reg, orig_app_pc))
                    goto drx_expand_scatter_gather_exit;
                reg_id_t scalar_value_reg = scratch_reg0;
                if (!expand_avx512_gather_insert_scalar_value(drcontext, bb, sg_instr, el,
                                                              &sg_info, scalar_value_reg,
                                                              scratch_xmm, orig_app_pc))
                    goto drx_expand_scatter_gather_exit;
                if (!expand_avx512_scatter_gather_update_mask(drcontext, bb, sg_instr, el,
                                                              &sg_info, scratch_reg0,
                                                              orig_app_pc, &allowed))
                    goto drx_expand_scatter_gather_exit;
                MINSERT(bb, sg_instr, skip_label);
            }
        } else /* AVX-512 instr_is_scatter(sg_instr) */ {
            for (uint el = 0; el < no_of_elements; ++el) {
                instr_t *skip_label = INSTR_CREATE_label(drcontext);
                expand_avx512_scatter_gather_make_test(drcontext, bb, sg_instr, el,
                                                       &sg_info, scratch_reg0, skip_label,
                                                       orig_app_pc);
                if (!expand_avx512_scatter_gather_extract_scalar_index(
                        drcontext, bb, sg_instr, el, &sg_info, scratch_xmm, scratch_reg0,
                        orig_app_pc))
                    goto drx_expand_scatter_gather_exit;
                reg_id_t scalar_index_reg = scratch_reg0;
                reg_id_t scalar_value_reg = scratch_reg1;
                if (!expand_avx512_scatter_extract_scalar_value(
                        drcontext, bb, sg_instr, el, &sg_info, scratch_xmm,
                        scalar_value_reg, orig_app_pc))
                    goto drx_expand_scatter_gather_exit;
                if (!expand_avx512_scatter_store_scalar_value(
                        drcontext, bb, sg_instr, &sg_info, scalar_index_reg,
                        scalar_value_reg, orig_app_pc))
                    goto drx_expand_scatter_gather_exit;
                if (!expand_avx512_scatter_gather_update_mask(drcontext, bb, sg_instr, el,
                                                              &sg_info, scratch_reg0,
                                                              orig_app_pc, &allowed))
                    goto drx_expand_scatter_gather_exit;
                MINSERT(bb, sg_instr, skip_label);
            }
        }
        /* The mask register is zeroed completely when instruction finishes. */
        if (proc_has_feature(FEATURE_AVX512BW)) {
            PREXL8(
                bb, sg_instr,
                INSTR_XL8(INSTR_CREATE_kxorq(drcontext, opnd_create_reg(sg_info.mask_reg),
                                             opnd_create_reg(sg_info.mask_reg),
                                             opnd_create_reg(sg_info.mask_reg)),
                          orig_app_pc));
        } else {
            PREXL8(
                bb, sg_instr,
                INSTR_XL8(INSTR_CREATE_kxorw(drcontext, opnd_create_reg(sg_info.mask_reg),
                                             opnd_create_reg(sg_info.mask_reg),
                                             opnd_create_reg(sg_info.mask_reg)),
                          orig_app_pc));
        }
    } else {
        /* AVX2 instr_is_gather(sg_instr) */
        for (uint el = 0; el < no_of_elements; ++el) {
            instr_t *skip_label = INSTR_CREATE_label(drcontext);
            if (!expand_avx2_gather_make_test(drcontext, bb, sg_instr, el, &sg_info,
                                              scratch_xmm, scratch_reg0, skip_label,
                                              orig_app_pc))
                goto drx_expand_scatter_gather_exit;
            if (!expand_avx2_gather_extract_scalar_index(drcontext, bb, sg_instr, el,
                                                         &sg_info, scratch_xmm,
                                                         scratch_reg0, orig_app_pc))
                goto drx_expand_scatter_gather_exit;
            reg_id_t scalar_index_reg = scratch_reg0;
            if (!expand_gather_load_scalar_value(drcontext, bb, sg_instr, &sg_info,
                                                 scalar_index_reg, orig_app_pc))
                goto drx_expand_scatter_gather_exit;
            reg_id_t scalar_value_reg = scratch_reg0;
            if (!expand_avx2_gather_insert_scalar_value(drcontext, bb, sg_instr, el,
                                                        &sg_info, scalar_value_reg,
                                                        scratch_xmm, orig_app_pc))
                goto drx_expand_scatter_gather_exit;
            if (!expand_avx2_gather_update_mask(drcontext, bb, sg_instr, el, &sg_info,
                                                scratch_xmm, scratch_reg0, orig_app_pc))
                goto drx_expand_scatter_gather_exit;
            MINSERT(bb, sg_instr, skip_label);
        }
        /* The mask register is zeroed completely when instruction finishes. */
        PREXL8(bb, sg_instr,
               INSTR_XL8(INSTR_CREATE_vpxor(drcontext, opnd_create_reg(sg_info.mask_reg),
                                            opnd_create_reg(sg_info.mask_reg),
                                            opnd_create_reg(sg_info.mask_reg)),
                         orig_app_pc));
    }
    /* Restore the scratch xmm. */
    drmgr_insert_read_tls_field(drcontext, tls_idx, bb, sg_instr, scratch_reg0);
    instrlist_meta_preinsert(
        bb, sg_instr,
        INSTR_CREATE_mov_ld(
            drcontext, opnd_create_reg(scratch_reg0),
            OPND_CREATE_MEMPTR(scratch_reg0,
                               offsetof(per_thread_t, scratch_mm_spill_slot_aligned))));
    if (mov_scratch_mm_opnd_sz == OPSZ_64) {
        instrlist_meta_preinsert(
            bb, sg_instr,
            instr_create_1dst_2src(drcontext, mov_scratch_mm_opcode,
                                   opnd_create_reg(scratch_mm),
                                   opnd_create_reg(DR_REG_K0),
                                   opnd_create_base_disp(scratch_reg0, DR_REG_NULL, 0, 0,
                                                         mov_scratch_mm_opnd_sz)));
    } else {
        instrlist_meta_preinsert(
            bb, sg_instr,
            instr_create_1dst_1src(drcontext, mov_scratch_mm_opcode,
                                   opnd_create_reg(scratch_mm),
                                   opnd_create_base_disp(scratch_reg0, DR_REG_NULL, 0, 0,
                                                         mov_scratch_mm_opnd_sz)));
    }
    ASSERT(scratch_reg0 != scratch_reg1,
           "Internal error: scratch registers must be different");
    if (drreg_unreserve_register(drcontext, bb, sg_instr, scratch_reg0) !=
        DRREG_SUCCESS) {
        ASSERT(false, "drreg_unreserve_register should not fail");
        goto drx_expand_scatter_gather_exit;
    }
    if (instr_is_scatter(sg_instr)) {
        if (drreg_unreserve_register(drcontext, bb, sg_instr, scratch_reg1) !=
            DRREG_SUCCESS) {
            ASSERT(false, "drreg_unreserve_register should not fail");
            goto drx_expand_scatter_gather_exit;
        }
    }
    if (drreg_unreserve_aflags(drcontext, bb, sg_instr) != DRREG_SUCCESS)
        goto drx_expand_scatter_gather_exit;
#if VERBOSE
    dr_print_instr(drcontext, STDERR, sg_instr, "\tThe instruction\n");
#endif

    drmgr_insert_emulation_end(drcontext, bb, sg_instr);
    /* Remove and destroy the original scatter/gather. */
    instrlist_remove(bb, sg_instr);
#if VERBOSE
    dr_fprintf(STDERR, "\twas expanded to the following sequence:\n");
    for (instr = instrlist_first(bb); instr != NULL; instr = instr_get_next(instr)) {
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

/***************************************************************************
 * RESTORE STATE
 */

/*
 * x86 scatter/gather emulation sequence support
 *
 * The following state machines exist in order to detect restore events that need
 * additional attention by drx in order to fix the application state on top of the
 * fixes that drreg already makes. For the AVX-512 scatter/gather sequences these are
 * instruction windows where a scratch mask is being used, and the windows after
 * each scalar load/store but before the destination mask register update. For AVX2,
 * the scratch mask is an xmm register and will be handled by drreg directly (future
 * update, xref #3844).
 *
 * The state machines allow for instructions like drreg spill/restore and instrumentation
 * in between recognized states. This is an approximation and could be broken in many
 * ways, e.g. by a client adding more than DRX_RESTORE_EVENT_SKIP_UNKNOWN_INSTR_MAX
 * number of instructions as instrumentation, or by altering the emulation sequence's
 * code.
 * TODO i#5005: A more safe way to do this would be along the lines of xref i#3801: if
 * we had instruction lists available, we could see and pass down emulation labels
 * instead of guessing the sequence based on decoding the code cache.
 *
 * AVX-512 gather sequence detection example:
 *
 *         DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_0
 *         vmovups       {%k0} %zmm2 -> (%rcx)[64byte]
 *         DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_1
 *         vextracti32x4 {%k0} $0x00 %zmm1 -> %xmm2
 *         DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_2
 *         vpextrd       %xmm2 $0x00 -> %ecx
 *         DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_3
 *         mov           (%rax,%rcx,4)[4byte] -> %ecx
 *         DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_4
 * (a)     vextracti32x4 {%k0} $0x00 %zmm0 -> %xmm2
 *         DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_5
 * (a)     vpinsrd       %xmm2 %ecx $0x00 -> %xmm2
 *         DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_6
 * (a)     vinserti32x4  {%k0} $0x00 %zmm0 %xmm2 -> %zmm0
 *         DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_7
 * (a)     mov           $0x00000001 -> %ecx
 *         DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_8
 * (a)     kmovw         %k0 -> %edx
 *         DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_9
 * (a)     kmovw         %ecx -> %k0
 *         DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_10
 * (a) (b) kandnw        %k0 %k1 -> %k1
 *         DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_11
 *     (b) kmovw         %edx -> %k0
 *         DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_1
 *
 * (a): The instruction window where the destination mask state hadn't been updated yet.
 * (b): The instruction window where the scratch mask is clobbered w/o support by drreg.
 *
 * AVX-512 scatter sequence detection example:
 *
 *         DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_0
 *         vmovups       {%k0} %zmm2 -> (%rcx)[64byte]
 *         DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_1
 *         vextracti32x4 {%k0} $0x00 %zmm1 -> %xmm2
 *         DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_2
 *         vpextrd       %xmm2 $0x00 -> %edx
 *         DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_3
 *         vextracti32x4 {%k0} $0x00 %zmm0 -> %xmm2
 *         DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_4
 *         vpextrd       %xmm2 $0x00 -> %ebx
 *         DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_5
 *         mov           %ebx -> (%rcx,%rdx,4)[4byte]
 *         DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_6
 * (a)     mov           $0x00000001 -> %edx
 *         DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_7
 * (a)     kmovw         %k0 -> %ebp
 *         DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_8
 * (a)     kmovw         %edx -> %k0
 *         DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_9
 * (a) (b) kandnw        %k0 %k1 -> %k1
 *         DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_10
 *     (b) kmovw         %ebp -> %k0
 *         DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_1
 *
 * (a): The instruction window where the destination mask state hadn't been updated yet.
 * (b): The instruction window where the scratch mask is clobbered w/o support by drreg.
 *
 * AVX2 gather sequence detection example:
 *
 *         DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_0
 *         vmovups       {%k0} %zmm3 -> (%rcx)[64byte]
 *         DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_1
 *         vextracti128  %ymm2 $0x00 -> %xmm3
 *         DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_2
 *         vpextrd       %xmm3 $0x00 -> %ecx
 *         DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_3
 *         mov           (%rax,%rcx,4)[4byte] -> %ecx
 *         DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_4
 * (a)     vextracti128  %ymm0 $0x00 -> %xmm3
 *         DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_5
 * (a)     vpinsrd       %xmm3 %ecx $0x00 -> %xmm3
 *         DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_6
 * (a)     vinserti128   %ymm0 %xmm3 $0x00 -> %ymm0
 *         DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_7
 * (a)     xor           %ecx %ecx -> %ecx
 *         DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_8
 * (a)     vextracti128  %ymm2 $0x00 -> %xmm3
 *         DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_9
 * (a)     vpinsrd       %xmm3 %ecx $0x00 -> %xmm3
 *         DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_10
 * (a)     vinserti128   %ymm2 %xmm3 $0x00 -> %ymm2
 *         DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_1
 *
 * (a): The instruction window where the destination mask state hadn't been updated yet.
 *
 */

#define DRX_RESTORE_EVENT_SKIP_UNKNOWN_INSTR_MAX 32

/* States of the AVX-512 gather detection state machine. */
#define DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_0 0
#define DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_1 1
#define DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_2 2
#define DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_3 3
#define DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_4 4
#define DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_5 5
#define DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_6 6
#define DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_7 7
#define DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_8 8
#define DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_9 9
#define DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_10 10
#define DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_11 11

/* States of the AVX-512 scatter detection state machine. */
#define DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_0 0
#define DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_1 1
#define DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_2 2
#define DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_3 3
#define DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_4 4
#define DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_5 5
#define DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_6 6
#define DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_7 7
#define DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_8 8
#define DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_9 9
#define DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_10 10

/* States of the AVX2 gather detection state machine. */
#define DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_0 0
#define DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_1 1
#define DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_2 2
#define DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_3 3
#define DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_4 4
#define DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_5 5
#define DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_6 6
#define DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_7 7
#define DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_8 8
#define DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_9 9
#define DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_10 10

typedef struct _drx_state_machine_params_t {
    byte *pc;
    byte *prev_pc;
    /* state machine's state */
    int detect_state;
    /* detected start pc of destination mask update */
    byte *restore_dest_mask_start_pc;
    /* detected start pc of scratch mask usage */
    byte *restore_scratch_mask_start_pc;
    /* counter to allow for skipping unknown instructions */
    int skip_unknown_instr_count;
    /* The spilled ymm/zmm register. When the_scratch_xmm is set,
     * it is expected to correspond to this.
     */
    reg_id_t spilled_mm;
    /* detected scratch xmm register for mask update */
    reg_id_t the_scratch_xmm;
    /* detected gpr register that holds the mask update immediate */
    reg_id_t gpr_bit_mask;
    /* detected gpr register that holds the app's mask state */
    reg_id_t gpr_save_scratch_mask;
    /* counter of scalar element in the scatter/gather sequence */
    uint scalar_mask_update_no;
    /* temporary scratch gpr for the AVX-512 scatter value */
    reg_id_t gpr_scratch_index;
    /* temporary scratch gpr for the AVX-512 scatter index */
    reg_id_t gpr_scratch_value;
    instr_t inst;
    dr_restore_state_info_t *info;
    scatter_gather_info_t *sg_info;
} drx_state_machine_params_t;

static void
advance_state(int new_detect_state, drx_state_machine_params_t *params)
{
    params->detect_state = new_detect_state;
    params->skip_unknown_instr_count = 0;
}

/* Advances to state 0 if counter has exceeded threshold, returns otherwise. */
static inline void
skip_unknown_instr_inc(int reset_state, drx_state_machine_params_t *params)
{
    if (params->skip_unknown_instr_count++ >= DRX_RESTORE_EVENT_SKIP_UNKNOWN_INSTR_MAX) {
        advance_state(reset_state, params);
    }
}

static void
restore_spilled_mm_value(void *drcontext, drx_state_machine_params_t *params)
{
    byte mm_val[ZMM_REG_SIZE];
    ASSERT(params->spilled_mm != DR_REG_NULL &&
               (reg_is_strictly_ymm(params->spilled_mm) ||
                reg_is_strictly_zmm(params->spilled_mm)),
           "No spilled ymm/zmm reg recorded");
    memcpy(mm_val, get_tls_data(drcontext)->scratch_mm_spill_slot_aligned,
           reg_is_strictly_ymm(params->spilled_mm) ? YMM_REG_SIZE : ZMM_REG_SIZE);
    reg_set_value_ex(params->spilled_mm, params->info->mcontext, mm_val);
}

/* Run the state machines and decode the code cache. The state machines will search the
 * code for whether the translation pc is in one of the instruction windows that need
 * additional handling by drx in order to restore specific state of the application's mask
 * registers. We consider this sufficiently accurate, but this is still an approximation.
 */
static bool
drx_restore_state_scatter_gather(
    void *drcontext, dr_restore_state_info_t *info, scatter_gather_info_t *sg_info,
    bool (*state_machine_func)(void *drcontext, drx_state_machine_params_t *params))
{
    drx_state_machine_params_t params;
    params.restore_dest_mask_start_pc = NULL;
    params.restore_scratch_mask_start_pc = NULL;
    params.detect_state = 0;
    params.skip_unknown_instr_count = 0;
    params.the_scratch_xmm = DR_REG_NULL;
    params.spilled_mm = DR_REG_NULL;
    params.gpr_bit_mask = DR_REG_NULL;
    params.gpr_save_scratch_mask = DR_REG_NULL;
    params.scalar_mask_update_no = 0;
    params.info = info;
    params.sg_info = sg_info;
    params.pc = params.info->fragment_info.cache_start_pc;
    instr_init(drcontext, &params.inst);
    /* As the state machine is looking for blocks of code that the fault may hit, the 128
     * bytes is a conservative approximation of the block's size, see (a) and (b) above.
     */
    while (params.pc <= params.info->raw_mcontext->pc + 128) {
        instr_reset(drcontext, &params.inst);
        params.prev_pc = params.pc;
        params.pc = decode(drcontext, params.pc, &params.inst);
        if (params.pc == NULL) {
            /* Upon a decoding error we simply give up. */
            break;
        }
        /* If there is a gather or scatter instruction in the code cache, then it is wise
         * to assume that this is not an emulated sequence that we need to examine
         * further.
         */
        if (instr_is_gather(&params.inst))
            break;
        if (instr_is_scatter(&params.inst))
            break;
        dr_log(drcontext, DR_LOG_ALL, 3, "%s @%p state=%d\n", __FUNCTION__,
               params.prev_pc, params.detect_state);
        if ((*state_machine_func)(drcontext, &params))
            break;
    }
    instr_free(drcontext, &params.inst);
    return true;
}

/* Returns true if done, false otherwise. */
static bool
drx_avx2_gather_sequence_state_machine(void *drcontext,
                                       drx_state_machine_params_t *params)
{
    int mov_scratch_mm_opcode;
    opnd_size_t mov_scratch_mm_opnd_sz;
    get_mov_scratch_mm_opcode_and_size(&mov_scratch_mm_opcode, &mov_scratch_mm_opnd_sz);
    uint mov_scratch_mm_opnd_pos = (mov_scratch_mm_opnd_sz == OPSZ_64 ? 1 : 0);
    switch (params->detect_state) {
    case DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_0:
        ASSERT(params->spilled_mm == DR_REG_NULL,
               "Spilled xmm reg must be undetermined yet");
        if (instr_get_opcode(&params->inst) == mov_scratch_mm_opcode &&
            opnd_is_reg(instr_get_src(&params->inst, mov_scratch_mm_opnd_pos)) &&
            (reg_is_strictly_ymm(
                 opnd_get_reg(instr_get_src(&params->inst, mov_scratch_mm_opnd_pos))) ||
             reg_is_strictly_zmm(
                 opnd_get_reg(instr_get_src(&params->inst, mov_scratch_mm_opnd_pos))))) {
            params->spilled_mm =
                opnd_get_reg(instr_get_src(&params->inst, mov_scratch_mm_opnd_pos));
            advance_state(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_1, params);
            break;
        }
        break;
    /* We come back to DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_1 for each
     * scalar load sequence of the expanded gather instr.
     */
    case DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_1:
        if (instr_get_opcode(&params->inst) == OP_vextracti128) {
            opnd_t dst0 = instr_get_dst(&params->inst, 0);
            if (opnd_is_reg(dst0)) {
                reg_id_t tmp_reg = opnd_get_reg(dst0);
                if (!reg_is_strictly_xmm(tmp_reg))
                    break;
                ASSERT(reg_resize_to_opsz(params->spilled_mm, OPSZ_16) == tmp_reg,
                       "Only the spilled xmm should be used as scratch");
                params->the_scratch_xmm = tmp_reg;
                advance_state(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_2, params);
                break;
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_1, params);
        break;
    case DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_2:
        ASSERT(params->the_scratch_xmm != DR_REG_NULL,
               "internal error: expected xmm register to be recorded in state "
               "machine.");
        if ((params->sg_info->scalar_index_size == OPSZ_4 &&
             instr_get_opcode(&params->inst) == OP_vpextrd) ||
            (params->sg_info->scalar_index_size == OPSZ_8 &&
             instr_get_opcode(&params->inst) == OP_vpextrq)) {
            ASSERT(opnd_is_reg(instr_get_src(&params->inst, 0)),
                   "internal error: unexpected instruction format");
            reg_id_t tmp_reg = opnd_get_reg(instr_get_src(&params->inst, 0));
            if (tmp_reg == params->the_scratch_xmm) {
                opnd_t dst0 = instr_get_dst(&params->inst, 0);
                if (opnd_is_reg(dst0) && reg_is_gpr(opnd_get_reg(dst0))) {
                    params->the_scratch_xmm = DR_REG_NULL;
                    params->gpr_scratch_index = opnd_get_reg(dst0);
                    advance_state(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_3, params);
                    break;
                }
            }
        }
        /* Intentionally not else if */
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_1, params);
        break;
    case DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_3:
        if (!instr_is_reg_spill_or_restore(drcontext, &params->inst, NULL, NULL, NULL,
                                           NULL)) {
            if (instr_reads_memory(&params->inst)) {
                opnd_t src0 = instr_get_src(&params->inst, 0);
                if (opnd_is_memory_reference(src0)) {
                    if (opnd_uses_reg(src0, params->gpr_scratch_index)) {
                        opnd_t dst0 = instr_get_dst(&params->inst, 0);
                        if (opnd_is_reg(dst0) && reg_is_gpr(opnd_get_reg(dst0))) {
                            params->restore_dest_mask_start_pc = params->pc;
                            advance_state(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_4,
                                          params);
                            break;
                        }
                    }
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_1, params);
        break;
    case DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_4:
        if (instr_get_opcode(&params->inst) == OP_vextracti128) {
            opnd_t dst0 = instr_get_dst(&params->inst, 0);
            if (opnd_is_reg(dst0)) {
                reg_id_t tmp_reg = opnd_get_reg(dst0);
                if (!reg_is_strictly_xmm(tmp_reg))
                    break;
                ASSERT(reg_resize_to_opsz(params->spilled_mm, OPSZ_16) == tmp_reg,
                       "Only the spilled xmm should be used as scratch");
                params->the_scratch_xmm = tmp_reg;
                advance_state(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_5, params);
                break;
            }
        }
        /* Intentionally not else if */
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_1, params);
        break;
    case DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_5:
        ASSERT(params->the_scratch_xmm != DR_REG_NULL,
               "internal error: expected xmm register to be recorded in state "
               "machine.");
        if ((params->sg_info->scalar_value_size == OPSZ_4 &&
             instr_get_opcode(&params->inst) == OP_vpinsrd) ||
            (params->sg_info->scalar_value_size == OPSZ_8 &&
             instr_get_opcode(&params->inst) == OP_vpinsrq)) {
            ASSERT(opnd_is_reg(instr_get_dst(&params->inst, 0)),
                   "internal error: unexpected instruction format");
            reg_id_t tmp_reg = opnd_get_reg(instr_get_dst(&params->inst, 0));
            if (tmp_reg == params->the_scratch_xmm) {
                params->the_scratch_xmm = DR_REG_NULL;
                advance_state(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_6, params);
                break;
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_1, params);
        break;
    case DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_6:
        if (instr_get_opcode(&params->inst) == OP_vinserti128) {
            ASSERT(opnd_is_reg(instr_get_dst(&params->inst, 0)),
                   "internal error: unexpected instruction format");
            reg_id_t tmp_reg = opnd_get_reg(instr_get_dst(&params->inst, 0));
            if (tmp_reg == params->sg_info->gather_dst_reg) {
                advance_state(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_7, params);
                break;
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_1, params);
        break;
    case DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_7:
        if (instr_get_opcode(&params->inst) == OP_xor) {
            opnd_t dst0 = instr_get_dst(&params->inst, 0);
            opnd_t src0 = instr_get_src(&params->inst, 0);
            opnd_t src1 = instr_get_src(&params->inst, 1);
            if (opnd_is_reg(dst0) && opnd_is_reg(src0) && opnd_is_reg(src1)) {
                reg_id_t reg_dst0 = opnd_get_reg(dst0);
                reg_id_t reg_src0 = opnd_get_reg(src0);
                reg_id_t reg_src1 = opnd_get_reg(src1);
                ASSERT(reg_is_gpr(reg_dst0) && reg_is_gpr(reg_src0) &&
                           reg_is_gpr(reg_src1),
                       "internal error: unexpected instruction format");
                if (reg_dst0 == reg_src0 && reg_src0 == reg_src1) {
                    params->gpr_bit_mask = reg_dst0;
                    advance_state(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_8, params);
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_1, params);
        break;
    case DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_8:
        if (instr_get_opcode(&params->inst) == OP_vextracti128) {
            opnd_t src0 = instr_get_src(&params->inst, 0);
            if (opnd_is_reg(src0)) {
                if (opnd_get_reg(src0) == params->sg_info->mask_reg) {
                    opnd_t dst0 = instr_get_dst(&params->inst, 0);
                    if (opnd_is_reg(dst0)) {
                        reg_id_t tmp_reg = opnd_get_reg(dst0);
                        if (!reg_is_strictly_xmm(tmp_reg))
                            break;
                        ASSERT(reg_resize_to_opsz(params->spilled_mm, OPSZ_16) == tmp_reg,
                               "Only the spilled xmm should be used as scratch");
                        params->the_scratch_xmm = tmp_reg;
                        advance_state(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_9,
                                      params);
                        break;
                    }
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_1, params);
        break;
    case DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_9:
        ASSERT(params->the_scratch_xmm != DR_REG_NULL,
               "internal error: expected xmm register to be recorded in state "
               "machine.");
        if ((params->sg_info->scalar_value_size == OPSZ_4 &&
             instr_get_opcode(&params->inst) == OP_vpinsrd) ||
            (params->sg_info->scalar_value_size == OPSZ_8 &&
             instr_get_opcode(&params->inst) == OP_vpinsrq)) {
            opnd_t src1 = instr_get_src(&params->inst, 1);
            if (opnd_is_reg(src1)) {
                if (opnd_get_reg(src1) == params->gpr_bit_mask) {
                    ASSERT(opnd_is_reg(instr_get_dst(&params->inst, 0)),
                           "internal error: unexpected instruction format");
                    reg_id_t tmp_reg = opnd_get_reg(instr_get_dst(&params->inst, 0));
                    if (tmp_reg == params->the_scratch_xmm) {
                        advance_state(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_10,
                                      params);
                        break;
                    }
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_1, params);
        break;
    case DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_10:
        if (instr_get_opcode(&params->inst) == OP_vinserti128) {
            ASSERT(opnd_is_reg(instr_get_dst(&params->inst, 0)) &&
                       opnd_is_reg(instr_get_src(&params->inst, 0)) &&
                       opnd_is_reg(instr_get_src(&params->inst, 1)),
                   "internal error: unexpected instruction format");
            reg_id_t dst0 = opnd_get_reg(instr_get_dst(&params->inst, 0));
            reg_id_t src0 = opnd_get_reg(instr_get_src(&params->inst, 0));
            reg_id_t src1 = opnd_get_reg(instr_get_src(&params->inst, 1));
            if (src1 == params->the_scratch_xmm) {
                if (src0 == params->sg_info->mask_reg) {
                    if (dst0 == params->sg_info->mask_reg) {
                        /* Check if we are already past the fault point. */
                        if (params->info->raw_mcontext->pc <= params->prev_pc) {
                            if (params->restore_dest_mask_start_pc <=
                                params->info->raw_mcontext->pc) {
                                /* Fix the gather's destination mask here and zero out
                                 * the bit that the emulation sequence hadn't done
                                 * before the fault hit.
                                 */
                                ASSERT(reg_is_strictly_xmm(params->sg_info->mask_reg) ||
                                           reg_is_strictly_ymm(params->sg_info->mask_reg),
                                       "internal error: unexpected instruction format");
                                byte val[YMM_REG_SIZE];
                                if (!reg_get_value_ex(params->sg_info->mask_reg,
                                                      params->info->raw_mcontext, val)) {
                                    ASSERT(false,
                                           "internal error: can't read mcontext's mask "
                                           "value");
                                }
                                uint mask_byte = opnd_size_in_bytes(
                                                     params->sg_info->scalar_index_size) *
                                        (params->scalar_mask_update_no + 1) -
                                    1;
                                val[mask_byte] &= ~(byte)128;
                                reg_set_value_ex(params->sg_info->mask_reg,
                                                 params->info->mcontext, val);
                            }
                            restore_spilled_mm_value(drcontext, params);
                            /* We are done. */
                            return true;
                        }
                        params->scalar_mask_update_no++;
                        uint no_of_elements =
                            opnd_size_in_bytes(params->sg_info->scatter_gather_size) /
                            MAX(opnd_size_in_bytes(params->sg_info->scalar_index_size),
                                opnd_size_in_bytes(params->sg_info->scalar_value_size));
                        if (params->scalar_mask_update_no > no_of_elements) {
                            /* Unlikely that something looks identical to an emulation
                             * sequence for this long, but we safely can return here.
                             */
                            return true;
                        }
                        advance_state(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_1,
                                      params);
                        break;
                    }
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX2_GATHER_EVENT_STATE_1, params);
        break;
    default: ASSERT(false, "internal error: invalid state.");
    }
    return false;
}

/* Returns true if done, false otherwise. */
static bool
drx_avx512_scatter_sequence_state_machine(void *drcontext,
                                          drx_state_machine_params_t *params)
{
    int mov_scratch_mm_opcode;
    opnd_size_t mov_scratch_mm_opnd_sz;
    get_mov_scratch_mm_opcode_and_size(&mov_scratch_mm_opcode, &mov_scratch_mm_opnd_sz);
    uint mov_scratch_mm_opnd_pos = (mov_scratch_mm_opnd_sz == OPSZ_64 ? 1 : 0);
    switch (params->detect_state) {
    case DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_0:
        ASSERT(params->spilled_mm == DR_REG_NULL,
               "Spilled xmm reg must be undetermined yet");
        if (instr_get_opcode(&params->inst) == mov_scratch_mm_opcode &&
            opnd_is_reg(instr_get_src(&params->inst, mov_scratch_mm_opnd_pos)) &&
            (reg_is_strictly_ymm(
                 opnd_get_reg(instr_get_src(&params->inst, mov_scratch_mm_opnd_pos))) ||
             reg_is_strictly_zmm(
                 opnd_get_reg(instr_get_src(&params->inst, mov_scratch_mm_opnd_pos))))) {
            params->spilled_mm =
                opnd_get_reg(instr_get_src(&params->inst, mov_scratch_mm_opnd_pos));
            advance_state(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_1, params);
            break;
        }
        break;
    /* We come back to DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_1 for each
     * scalar store sequence of the expanded scatter instr.
     */
    case DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_1:
        if (instr_get_opcode(&params->inst) == OP_vextracti32x4) {
            opnd_t dst0 = instr_get_dst(&params->inst, 0);
            if (opnd_is_reg(dst0)) {
                reg_id_t tmp_reg = opnd_get_reg(dst0);
                if (!reg_is_strictly_xmm(tmp_reg))
                    break;
                ASSERT(reg_resize_to_opsz(params->spilled_mm, OPSZ_16) == tmp_reg,
                       "Only the spilled xmm should be used as scratch");
                params->the_scratch_xmm = tmp_reg;
                advance_state(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_2, params);
                break;
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_1, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_2:
        ASSERT(params->the_scratch_xmm != DR_REG_NULL,
               "internal error: expected xmm register to be recorded in state "
               "machine.");
        if ((params->sg_info->scalar_index_size == OPSZ_4 &&
             instr_get_opcode(&params->inst) == OP_vpextrd) ||
            (params->sg_info->scalar_index_size == OPSZ_8 &&
             instr_get_opcode(&params->inst) == OP_vpextrq)) {
            ASSERT(opnd_is_reg(instr_get_src(&params->inst, 0)),
                   "internal error: unexpected instruction format");
            reg_id_t tmp_reg = opnd_get_reg(instr_get_src(&params->inst, 0));
            if (tmp_reg == params->the_scratch_xmm) {
                opnd_t dst0 = instr_get_dst(&params->inst, 0);
                if (opnd_is_reg(dst0) && reg_is_gpr(opnd_get_reg(dst0))) {
                    params->the_scratch_xmm = DR_REG_NULL;
                    params->gpr_scratch_index = opnd_get_reg(dst0);
                    advance_state(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_3,
                                  params);
                    break;
                }
            }
        }
        /* Intentionally not else if */
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_1, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_3:
        if (instr_get_opcode(&params->inst) == OP_vextracti32x4) {
            opnd_t dst0 = instr_get_dst(&params->inst, 0);
            if (opnd_is_reg(dst0)) {
                reg_id_t tmp_reg = opnd_get_reg(dst0);
                if (!reg_is_strictly_xmm(tmp_reg))
                    break;
                ASSERT(reg_resize_to_opsz(params->spilled_mm, OPSZ_16) == tmp_reg,
                       "Only the spilled xmm should be used as scratch");
                params->the_scratch_xmm = tmp_reg;
                advance_state(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_4, params);
                break;
            }
        }
        /* Intentionally not else if */
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_1, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_4:
        ASSERT(params->the_scratch_xmm != DR_REG_NULL,
               "internal error: expected xmm register to be recorded in state "
               "machine.");
        if ((params->sg_info->scalar_value_size == OPSZ_4 &&
             instr_get_opcode(&params->inst) == OP_vpextrd) ||
            (params->sg_info->scalar_value_size == OPSZ_8 &&
             instr_get_opcode(&params->inst) == OP_vpextrq)) {
            ASSERT(opnd_is_reg(instr_get_src(&params->inst, 0)),
                   "internal error: unexpected instruction format");
            reg_id_t tmp_reg = opnd_get_reg(instr_get_src(&params->inst, 0));
            if (tmp_reg == params->the_scratch_xmm) {
                opnd_t dst0 = instr_get_dst(&params->inst, 0);
                if (opnd_is_reg(dst0) && reg_is_gpr(opnd_get_reg(dst0))) {
                    params->the_scratch_xmm = DR_REG_NULL;
                    params->gpr_scratch_value = opnd_get_reg(dst0);
                    advance_state(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_5,
                                  params);
                    break;
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_1, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_5: {
        if (!instr_is_reg_spill_or_restore(drcontext, &params->inst, NULL, NULL, NULL,
                                           NULL)) {
            if (instr_writes_memory(&params->inst)) {
                opnd_t dst0 = instr_get_dst(&params->inst, 0);
                if (opnd_is_memory_reference(dst0)) {
                    opnd_t src0 = instr_get_src(&params->inst, 0);
                    if (opnd_is_reg(src0) &&
                        opnd_uses_reg(src0, params->gpr_scratch_value) &&
                        opnd_uses_reg(dst0, params->gpr_scratch_index)) {
                        params->restore_dest_mask_start_pc = params->pc;
                        advance_state(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_6,
                                      params);
                        break;
                    }
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_1, params);
        break;
    }
    case DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_6: {
        ptr_int_t val;
        if (instr_is_mov_constant(&params->inst, &val)) {
            /* If more than one bit is set, this is not what we're looking for. */
            if (val == 0 || (val & (val - 1)) != 0)
                break;
            opnd_t dst0 = instr_get_dst(&params->inst, 0);
            if (opnd_is_reg(dst0)) {
                reg_id_t tmp_gpr = opnd_get_reg(dst0);
                if (reg_is_gpr(tmp_gpr)) {
                    params->gpr_bit_mask = tmp_gpr;
                    advance_state(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_7,
                                  params);
                    break;
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_1, params);
        break;
    }
    case DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_7:
        if (instr_get_opcode(&params->inst) == OP_kmovw) {
            opnd_t src0 = instr_get_src(&params->inst, 0);
            if (opnd_is_reg(src0) && opnd_get_reg(src0) == DR_REG_K0) {
                opnd_t dst0 = instr_get_dst(&params->inst, 0);
                if (opnd_is_reg(dst0)) {
                    reg_id_t tmp_gpr = opnd_get_reg(dst0);
                    if (reg_is_gpr(tmp_gpr)) {
                        params->gpr_save_scratch_mask = tmp_gpr;
                        advance_state(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_8,
                                      params);
                        break;
                    }
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_1, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_8:
        ASSERT(params->gpr_bit_mask != DR_REG_NULL,
               "internal error: expected gpr register to be recorded in state "
               "machine.");
        if (instr_get_opcode(&params->inst) == OP_kmovw) {
            opnd_t src0 = instr_get_src(&params->inst, 0);
            if (opnd_is_reg(src0) && opnd_get_reg(src0) == params->gpr_bit_mask) {
                opnd_t dst0 = instr_get_dst(&params->inst, 0);
                if (opnd_is_reg(dst0) && opnd_get_reg(dst0) == DR_REG_K0) {
                    params->restore_scratch_mask_start_pc = params->pc;
                    advance_state(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_9,
                                  params);
                    break;
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_1, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_9:
        if (instr_get_opcode(&params->inst) == OP_kandnw) {
            opnd_t src0 = instr_get_src(&params->inst, 0);
            opnd_t src1 = instr_get_src(&params->inst, 1);
            opnd_t dst0 = instr_get_dst(&params->inst, 0);
            if (opnd_is_reg(src0) && opnd_get_reg(src0) == DR_REG_K0) {
                if (opnd_is_reg(src1) &&
                    opnd_get_reg(src1) == params->sg_info->mask_reg &&
                    opnd_is_reg(dst0) &&
                    opnd_get_reg(dst0) == params->sg_info->mask_reg) {
                    if (params->restore_dest_mask_start_pc <=
                            params->info->raw_mcontext->pc &&
                        params->info->raw_mcontext->pc <= params->prev_pc) {
                        /* Fix the scatter's destination mask here and zero out
                         * the bit that the emulation sequence hadn't done
                         * before the fault hit.
                         */
                        params->info->mcontext
                            ->opmask[params->sg_info->mask_reg - DR_REG_K0] &=
                            ~(1 << params->scalar_mask_update_no);
                        /* We are not done yet, we have to fix up the scratch
                         * mask as well.
                         */
                    }
                    /* We are counting the scalar load number in the sequence
                     * here.
                     */
                    params->scalar_mask_update_no++;
                    uint no_of_elements =
                        opnd_size_in_bytes(params->sg_info->scatter_gather_size) /
                        MAX(opnd_size_in_bytes(params->sg_info->scalar_index_size),
                            opnd_size_in_bytes(params->sg_info->scalar_value_size));
                    if (params->scalar_mask_update_no > no_of_elements) {
                        /* Unlikely that something looks identical to an emulation
                         * sequence for this long, but we safely can return here.
                         */
                        return true;
                    }
                    advance_state(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_10,
                                  params);
                    break;
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_1, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_10:
        if (instr_get_opcode(&params->inst) == OP_kmovw) {
            opnd_t dst0 = instr_get_dst(&params->inst, 0);
            if (opnd_is_reg(dst0) && opnd_get_reg(dst0) == DR_REG_K0) {
                opnd_t src0 = instr_get_src(&params->inst, 0);
                if (opnd_is_reg(src0)) {
                    if (reg_is_gpr(opnd_get_reg(src0)) &&
                        /* Check if we are already past the fault point. */
                        params->info->raw_mcontext->pc <= params->prev_pc) {
                        if (params->restore_scratch_mask_start_pc <=
                            params->info->raw_mcontext->pc) {
                            /* The scratch mask is always k0. This is hard-coded
                             * in drx. We carefully only update the lowest 16 bits
                             * because the mask was saved with kmovw.
                             */
                            ASSERT(sizeof(params->info->mcontext->opmask[0]) ==
                                       sizeof(long long),
                                   "internal error: unexpected opmask slot size");
                            params->info->mcontext->opmask[0] &= ~0xffffLL;
                            params->info->mcontext->opmask[0] |=
                                reg_get_value(params->gpr_save_scratch_mask,
                                              params->info->raw_mcontext) &
                                0xffff;
                        }
                        restore_spilled_mm_value(drcontext, params);
                        /* We are done. If we did fix up the scatter's destination
                         * mask, this already has happened.
                         */
                        return true;
                    }
                    advance_state(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_1,
                                  params);
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_SCATTER_EVENT_STATE_1, params);
        break;
    default: ASSERT(false, "internal error: invalid state.");
    }
    return false;
}

/* Returns true if done, false otherwise. */
static bool
drx_avx512_gather_sequence_state_machine(void *drcontext,
                                         drx_state_machine_params_t *params)
{
    int mov_scratch_mm_opcode;
    opnd_size_t mov_scratch_mm_opnd_sz;
    get_mov_scratch_mm_opcode_and_size(&mov_scratch_mm_opcode, &mov_scratch_mm_opnd_sz);
    uint mov_scratch_mm_opnd_pos = (mov_scratch_mm_opnd_sz == OPSZ_64 ? 1 : 0);
    switch (params->detect_state) {
    case DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_0:
        ASSERT(params->spilled_mm == DR_REG_NULL,
               "Spilled xmm reg must be undetermined yet");
        if (instr_get_opcode(&params->inst) == mov_scratch_mm_opcode &&
            opnd_is_reg(instr_get_src(&params->inst, mov_scratch_mm_opnd_pos)) &&
            (reg_is_strictly_ymm(
                 opnd_get_reg(instr_get_src(&params->inst, mov_scratch_mm_opnd_pos))) ||
             reg_is_strictly_zmm(
                 opnd_get_reg(instr_get_src(&params->inst, mov_scratch_mm_opnd_pos))))) {
            params->spilled_mm =
                opnd_get_reg(instr_get_src(&params->inst, mov_scratch_mm_opnd_pos));
            advance_state(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_1, params);
            break;
        }
        break;
    /* We come back to DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_1 for each
     * scalar load sequence of the expanded gather instr.
     */
    case DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_1:
        if (instr_get_opcode(&params->inst) == OP_vextracti32x4) {
            opnd_t dst0 = instr_get_dst(&params->inst, 0);
            if (opnd_is_reg(dst0)) {
                reg_id_t tmp_reg = opnd_get_reg(dst0);
                if (!reg_is_strictly_xmm(tmp_reg))
                    break;
                ASSERT(reg_resize_to_opsz(params->spilled_mm, OPSZ_16) == tmp_reg,
                       "Only the spilled xmm should be used as scratch");
                params->the_scratch_xmm = tmp_reg;
                advance_state(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_2, params);
                break;
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_1, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_2:
        ASSERT(params->the_scratch_xmm != DR_REG_NULL,
               "internal error: expected xmm register to be recorded in state "
               "machine.");
        if ((params->sg_info->scalar_index_size == OPSZ_4 &&
             instr_get_opcode(&params->inst) == OP_vpextrd) ||
            (params->sg_info->scalar_index_size == OPSZ_8 &&
             instr_get_opcode(&params->inst) == OP_vpextrq)) {
            ASSERT(opnd_is_reg(instr_get_src(&params->inst, 0)),
                   "internal error: unexpected instruction format");
            reg_id_t tmp_reg = opnd_get_reg(instr_get_src(&params->inst, 0));
            if (tmp_reg == params->the_scratch_xmm) {
                opnd_t dst0 = instr_get_dst(&params->inst, 0);
                if (opnd_is_reg(dst0) && reg_is_gpr(opnd_get_reg(dst0))) {
                    params->the_scratch_xmm = DR_REG_NULL;
                    params->gpr_scratch_index = opnd_get_reg(dst0);
                    advance_state(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_3, params);
                    break;
                }
            }
        }
        /* Intentionally not else if */
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_1, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_3:
        if (!instr_is_reg_spill_or_restore(drcontext, &params->inst, NULL, NULL, NULL,
                                           NULL)) {
            if (instr_reads_memory(&params->inst)) {
                opnd_t src0 = instr_get_src(&params->inst, 0);
                if (opnd_is_memory_reference(src0) &&
                    opnd_uses_reg(src0, params->gpr_scratch_index)) {
                    opnd_t dst0 = instr_get_dst(&params->inst, 0);
                    if (opnd_is_reg(dst0) && reg_is_gpr(opnd_get_reg(dst0))) {
                        params->restore_dest_mask_start_pc = params->pc;
                        advance_state(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_4,
                                      params);
                        break;
                    }
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_1, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_4:
        if (instr_get_opcode(&params->inst) == OP_vextracti32x4) {
            opnd_t dst0 = instr_get_dst(&params->inst, 0);
            if (opnd_is_reg(dst0)) {
                reg_id_t tmp_reg = opnd_get_reg(dst0);
                if (!reg_is_strictly_xmm(tmp_reg))
                    break;
                ASSERT(reg_resize_to_opsz(params->spilled_mm, OPSZ_16) == tmp_reg,
                       "Only the spilled xmm should be used as scratch");
                params->the_scratch_xmm = tmp_reg;
                advance_state(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_5, params);
                break;
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_1, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_5:
        ASSERT(params->the_scratch_xmm != DR_REG_NULL,
               "internal error: expected xmm register to be recorded in state "
               "machine.");
        if ((params->sg_info->scalar_value_size == OPSZ_4 &&
             instr_get_opcode(&params->inst) == OP_vpinsrd) ||
            (params->sg_info->scalar_value_size == OPSZ_8 &&
             instr_get_opcode(&params->inst) == OP_vpinsrq)) {
            ASSERT(opnd_is_reg(instr_get_dst(&params->inst, 0)),
                   "internal error: unexpected instruction format");
            reg_id_t tmp_reg = opnd_get_reg(instr_get_dst(&params->inst, 0));
            if (tmp_reg == params->the_scratch_xmm) {
                advance_state(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_6, params);
                break;
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_1, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_6:
        if (instr_get_opcode(&params->inst) == OP_vinserti32x4) {
            ASSERT(opnd_is_reg(instr_get_dst(&params->inst, 0)),
                   "internal error: unexpected instruction format");
            reg_id_t tmp_reg = opnd_get_reg(instr_get_dst(&params->inst, 0));
            if (tmp_reg == params->sg_info->gather_dst_reg) {
                advance_state(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_7, params);
                break;
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_1, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_7: {
        ptr_int_t val;
        if (instr_is_mov_constant(&params->inst, &val)) {
            /* If more than one bit is set, this is not what we're looking for. */
            if (val == 0 || (val & (val - 1)) != 0)
                break;
            opnd_t dst0 = instr_get_dst(&params->inst, 0);
            if (opnd_is_reg(dst0)) {
                reg_id_t tmp_gpr = opnd_get_reg(dst0);
                if (reg_is_gpr(tmp_gpr)) {
                    params->gpr_bit_mask = tmp_gpr;
                    advance_state(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_8, params);
                    break;
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_1, params);
        break;
    }
    case DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_8:
        if (instr_get_opcode(&params->inst) == OP_kmovw) {
            opnd_t src0 = instr_get_src(&params->inst, 0);
            if (opnd_is_reg(src0) && opnd_get_reg(src0) == DR_REG_K0) {
                opnd_t dst0 = instr_get_dst(&params->inst, 0);
                if (opnd_is_reg(dst0)) {
                    reg_id_t tmp_gpr = opnd_get_reg(dst0);
                    if (reg_is_gpr(tmp_gpr)) {
                        params->gpr_save_scratch_mask = tmp_gpr;
                        advance_state(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_9,
                                      params);
                        break;
                    }
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_1, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_9:
        ASSERT(params->gpr_bit_mask != DR_REG_NULL,
               "internal error: expected gpr register to be recorded in state "
               "machine.");
        if (instr_get_opcode(&params->inst) == OP_kmovw) {
            opnd_t src0 = instr_get_src(&params->inst, 0);
            if (opnd_is_reg(src0) && opnd_get_reg(src0) == params->gpr_bit_mask) {
                opnd_t dst0 = instr_get_dst(&params->inst, 0);
                if (opnd_is_reg(dst0) && opnd_get_reg(dst0) == DR_REG_K0) {
                    params->restore_scratch_mask_start_pc = params->pc;
                    advance_state(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_10,
                                  params);
                    break;
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_1, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_10:
        if (instr_get_opcode(&params->inst) == OP_kandnw) {
            opnd_t src0 = instr_get_src(&params->inst, 0);
            opnd_t src1 = instr_get_src(&params->inst, 1);
            opnd_t dst0 = instr_get_dst(&params->inst, 0);
            if (opnd_is_reg(src0) && opnd_get_reg(src0) == DR_REG_K0) {
                if (opnd_is_reg(src1) &&
                    opnd_get_reg(src1) == params->sg_info->mask_reg) {
                    if (opnd_is_reg(dst0) &&
                        opnd_get_reg(dst0) == params->sg_info->mask_reg) {
                        if (params->restore_dest_mask_start_pc <=
                                params->info->raw_mcontext->pc &&
                            params->info->raw_mcontext->pc <= params->prev_pc) {
                            /* Fix the gather's destination mask here and zero out
                             * the bit that the emulation sequence hadn't done
                             * before the fault hit.
                             */
                            params->info->mcontext
                                ->opmask[params->sg_info->mask_reg - DR_REG_K0] &=
                                ~(1 << params->scalar_mask_update_no);
                            /* We are not done yet, we have to fix up the scratch
                             * mask as well.
                             */
                        }
                        /* We are counting the scalar load number in the sequence
                         * here.
                         */
                        params->scalar_mask_update_no++;
                        uint no_of_elements =
                            opnd_size_in_bytes(params->sg_info->scatter_gather_size) /
                            MAX(opnd_size_in_bytes(params->sg_info->scalar_index_size),
                                opnd_size_in_bytes(params->sg_info->scalar_value_size));
                        if (params->scalar_mask_update_no > no_of_elements) {
                            /* Unlikely that something looks identical to an emulation
                             * sequence for this long, but we safely can return here.
                             */
                            return true;
                        }
                        advance_state(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_11,
                                      params);
                        break;
                    }
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_1, params);
        break;
    case DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_11:
        if (instr_get_opcode(&params->inst) == OP_kmovw) {
            opnd_t dst0 = instr_get_dst(&params->inst, 0);
            if (opnd_is_reg(dst0) && opnd_get_reg(dst0) == DR_REG_K0) {
                opnd_t src0 = instr_get_src(&params->inst, 0);
                if (opnd_is_reg(src0)) {
                    reg_id_t tmp_gpr = opnd_get_reg(src0);
                    if (reg_is_gpr(tmp_gpr)) {
                        /* Check if we are already past the fault point. */
                        if (params->info->raw_mcontext->pc <= params->prev_pc) {
                            if (params->restore_scratch_mask_start_pc <=
                                params->info->raw_mcontext->pc) {
                                /* The scratch mask is always k0. This is hard-coded
                                 * in drx. We carefully only update the lowest 16 bits
                                 * because the mask was saved with kmovw.
                                 */
                                ASSERT(sizeof(params->info->mcontext->opmask[0]) ==
                                           sizeof(long long),
                                       "internal error: unexpected opmask slot size");
                                params->info->mcontext->opmask[0] &= ~0xffffLL;
                                params->info->mcontext->opmask[0] |=
                                    reg_get_value(params->gpr_save_scratch_mask,
                                                  params->info->raw_mcontext) &
                                    0xffff;
                            }
                            restore_spilled_mm_value(drcontext, params);
                            /* We are done. If we did fix up the gather's destination
                             * mask, this already has happened.
                             */
                            return true;
                        }
                    }
                }
            }
        }
        skip_unknown_instr_inc(DRX_DETECT_RESTORE_AVX512_GATHER_EVENT_STATE_1, params);
        break;
    default: ASSERT(false, "internal error: invalid state.");
    }
    return false;
}

static bool
drx_restore_state_for_avx512_gather(void *drcontext, dr_restore_state_info_t *info,
                                    scatter_gather_info_t *sg_info)
{
    return drx_restore_state_scatter_gather(drcontext, info, sg_info,
                                            drx_avx512_gather_sequence_state_machine);
}

static bool
drx_restore_state_for_avx512_scatter(void *drcontext, dr_restore_state_info_t *info,
                                     scatter_gather_info_t *sg_info)
{
    return drx_restore_state_scatter_gather(drcontext, info, sg_info,
                                            drx_avx512_scatter_sequence_state_machine);
}

static bool
drx_restore_state_for_avx2_gather(void *drcontext, dr_restore_state_info_t *info,
                                  scatter_gather_info_t *sg_info)
{
    return drx_restore_state_scatter_gather(drcontext, info, sg_info,
                                            drx_avx2_gather_sequence_state_machine);
}

static bool
drx_event_restore_state(void *drcontext, bool restore_memory,
                        dr_restore_state_info_t *info)
{
    instr_t inst;
    bool success = true;
    if (info->fragment_info.cache_start_pc == NULL)
        return true; /* fault not in cache */
    if (dr_atomic_load32(&drx_scatter_gather_expanded) == 0) {
        /* Nothing to do if nobody had never called expand_scatter_gather() before. */
        return true;
    }
    if (!info->fragment_info.app_code_consistent) {
        /* Can't verify application code.
         * XXX i#2985: is it better to keep searching?
         */
        return true;
    }
    instr_init(drcontext, &inst);
    byte *pc = decode(drcontext, dr_fragment_app_pc(info->fragment_info.tag), &inst);
    if (pc != NULL) {
        scatter_gather_info_t sg_info;
        if (instr_is_gather(&inst)) {
            get_scatter_gather_info(&inst, &sg_info);
            if (sg_info.is_evex) {
                success = success &&
                    drx_restore_state_for_avx512_gather(drcontext, info, &sg_info);
            } else {
                success = success &&
                    drx_restore_state_for_avx2_gather(drcontext, info, &sg_info);
            }
        } else if (instr_is_scatter(&inst)) {
            get_scatter_gather_info(&inst, &sg_info);
            success = success &&
                drx_restore_state_for_avx512_scatter(drcontext, info, &sg_info);
        }
    }
    instr_free(drcontext, &inst);
    return success;
}
