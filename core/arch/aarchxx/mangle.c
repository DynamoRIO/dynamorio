/* **********************************************************
 * Copyright (c) 2014-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2016 ARM Limited. All rights reserved.
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
 */

#include "../globals.h"
#include "arch.h"
#include "instr_create_shared.h"
#include "instrument.h" /* instrlist_meta_preinsert */
#include "../clean_call_opt.h"
#include "disassemble.h"

/* Make code more readable by shortening long lines.
 * We mark everything we add as non-app instr.
 */
#define POST instrlist_meta_postinsert
#define PRE instrlist_meta_preinsert

/* For ARM and AArch64, we always use TLS and never use hardcoded
 * dcontext (xref USE_SHARED_GENCODE_ALWAYS() and -private_ib_in_tls).
 * Thus we use instr_create_{save_to,restore_from}_tls() directly.
 */

#ifdef AARCH64
/* Defined in aarch64.asm. */
void
icache_op_ic_ivau_asm(void);
void
icache_op_isb_asm(void);

typedef struct ALIGN_VAR(16) _icache_op_struct_t {
    /* This flag is set if any icache lines have been invalidated. */
    unsigned int flag;
    /* The lower half of the address of "lock" must be non-zero as we want to
     * acquire the lock using only two free registers and STXR Ws, Wt, [Xn]
     * requires s != t and s != n, so we use t == n. With this ordering of the
     * members alignment guarantees that bit 2 of the address of "lock" is set.
     */
    unsigned int lock;
    /* The icache line size. This is discovered using the system register
     * ctr_el0 and will be (1 << (2 + n)) with 0 <= n < 16.
     */
    size_t linesize;
    /* If these are equal then no icache lines have been invalidated. Otherwise
     * they are both aligned to the icache line size and describe a set of
     * consecutive icache lines (which could wrap around the top of memory).
     */
    void *begin, *end;
    /* Some space to spill registers. */
    ptr_uint_t spill[2];
} icache_op_struct_t;

/* Used in aarch64.asm. */
icache_op_struct_t icache_op_struct;
#endif

void
mangle_arch_init(void)
{
#ifdef AARCH64
    /* Check address of "lock" is unaligned. See comment in icache_op_struct_t. */
    ASSERT(!ALIGNED(&icache_op_struct.lock, 16));
#endif
}

void
insert_clear_eflags(dcontext_t *dcontext, clean_call_info_t *cci, instrlist_t *ilist,
                    instr_t *instr)
{
    /* On ARM/AArch64 no known calling convention requires any of the
     * flags to be zero on entry to a function, so there is nothing to do.
     */
}

#ifdef AARCH64
/* Maximum positive immediate offset for STP/LDP with 64 bit registers. */
#    define MAX_STP_OFFSET 504
/* Maximum positive immediate offset for SVE STR/LDR with Z/P registers. */
#    define MAX_SVE_STR_OFFSET 255

/* Creates a memory reference for registers saved/restored to memory. */
static opnd_t
create_base_disp_for_save_restore(uint base_reg, bool is_single_reg, reg_type_t rtype,
                                  uint num_saved, callee_info_t *ci)
{
    /* opzs depends on the kind of register and whether a single register or
     * a pair of registers is saved/restored using stp/ldp.
     */
    uint opsz = OPSZ_NA;
    uint offset = 0;
    switch (rtype) {
    case GPR_REG_TYPE:
        opsz = is_single_reg ? OPSZ_8 : OPSZ_16;
        offset = num_saved * sizeof(reg_t);
        break;
    case SIMD_REG_TYPE:
        opsz = is_single_reg ? OPSZ_16 : OPSZ_32;
        offset = num_saved * 16;
        break;
    case SVE_ZREG_TYPE:
        opsz = opnd_size_from_bytes(proc_get_vector_length_bytes());
        offset = num_saved * proc_get_vector_length_bytes();
        break;
    case SVE_PREG_TYPE:
        opsz = opnd_size_from_bytes(proc_get_vector_length_bytes() / 8);
        offset = num_saved * (proc_get_vector_length_bytes() / 8);
        break;
    default: ASSERT_NOT_REACHED();
    }
    return opnd_create_base_disp(base_reg, DR_REG_NULL, 0, offset, opsz);
}

static instr_t *
create_load_or_store_instr(dcontext_t *dcontext, reg_id_t reg, opnd_t mem, bool save)
{
    if (save) {
        return INSTR_CREATE_str(dcontext, mem, opnd_create_reg(reg));
    }
    return INSTR_CREATE_ldr(dcontext, opnd_create_reg(reg), mem);
}

/* Creates code to save or restore GPR or SIMD registers to memory starting at
 * base_reg. Uses stp/ldp to save/restore as many register pairs to memory as possible
 * and uses a single str/ldp for the last register in case the number of registers
 * is odd. Optionally takes reg_skip into account.
 */
static void
insert_save_or_restore_gpr_simd_registers(
    dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr, bool *reg_skip,
    reg_id_t base_reg, reg_id_t first_reg, bool save, reg_type_t rtype,
    opnd_t (*get_mem_opnd)(uint base_reg, bool is_single_reg, reg_type_t rtype,
                           uint num_saved, callee_info_t *ci),
    callee_info_t *ci)
{
    ASSERT(rtype == GPR_REG_TYPE || rtype == SIMD_REG_TYPE);

    uint i, reg1 = UINT_MAX,
            num_regs = (rtype == GPR_REG_TYPE) ? 30 : MCXT_NUM_SIMD_SVE_SLOTS;
    uint saved_regs = 0;
    instr_t *new_instr;
    /* Use stp/ldp to save/restore as many register pairs to memory, skipping
     * registers according to reg_skip.
     */
    for (i = 0; i < num_regs; i += 1) {
        if (reg_skip != NULL && reg_skip[i])
            continue;

        if (reg1 == UINT_MAX)
            reg1 = i;
        else {
            opnd_t mem1 =
                get_mem_opnd(base_reg, /*is_single_reg=*/false, rtype,
                             /* When creating save/restore instructions
                              * for inlining, we need the register id
                              * to compute the address.
                              */
                             ci != NULL ? first_reg + (reg_id_t)reg1 : saved_regs, ci);

            uint disp = opnd_get_disp(mem1);
            /* We cannot use STP/LDP if the immediate offset is too big. */
            if (disp > MAX_STP_OFFSET) {
                PRE(ilist, instr,
                    create_load_or_store_instr(dcontext, first_reg + reg1, mem1, save));

                opnd_t mem2 =
                    get_mem_opnd(base_reg, /*is_single_reg=*/false, rtype,
                                 /* When creating save/restore instructions
                                  * for inlining, we need the register id
                                  * to compute the address.
                                  */
                                 ci != NULL ? first_reg + (reg_id_t)i : saved_regs, ci);

                PRE(ilist, instr,
                    create_load_or_store_instr(dcontext, first_reg + i, mem2, save));
            } else {
                if (save) {
                    new_instr = INSTR_CREATE_stp(dcontext, mem1,
                                                 opnd_create_reg(first_reg + reg1),
                                                 opnd_create_reg(first_reg + i));
                } else {
                    new_instr =
                        INSTR_CREATE_ldp(dcontext, opnd_create_reg(first_reg + reg1),
                                         opnd_create_reg(first_reg + i), mem1);
                }
                PRE(ilist, instr, new_instr);
            }
            reg1 = UINT_MAX;
            saved_regs += 2;
        }
    }

    /* Use str/ldr to save/restore last single register to memory if the number
     * of registers to save/restore is odd.
     */
    if (reg1 != UINT_MAX) {
        opnd_t mem =
            get_mem_opnd(base_reg, /*is_single_reg=*/true, rtype,
                         ci != NULL ? first_reg + (reg_id_t)reg1 : saved_regs, ci);
        PRE(ilist, instr,
            create_load_or_store_instr(dcontext, first_reg + reg1, mem, save));
    }
}

static void
insert_save_or_restore_svep_registers(
    dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr, bool *reg_skip,
    reg_id_t base_reg, bool save,
    opnd_t (*get_mem_opnd)(uint base_reg, bool is_single_reg, reg_type_t rtype,
                           uint num_saved, callee_info_t *ci),
    callee_info_t *ci)
{
    uint i, saved_regs = 0;
    for (i = 0; i < MCXT_NUM_SVEP_SLOTS; i++) {
        if (reg_skip != NULL && reg_skip[MCXT_NUM_SIMD_SVE_SLOTS + i])
            continue;

        opnd_t mem =
            get_mem_opnd(base_reg, /*is_single_reg=*/true, SVE_PREG_TYPE, saved_regs, ci);
        /* disp should never be greater than MAX_SVE_STR_OFFSET because it
         * is the immediate multiplied by the current vector register size
         * in bytes: STR <Pn>, [<Xn|SP>{, #<imm>, MUL VL}] and we only go up
         * num_regs registers.
         */
        ASSERT(opnd_get_disp(mem) / proc_get_vector_length_bytes() <= MAX_SVE_STR_OFFSET);
        PRE(ilist, instr, create_load_or_store_instr(dcontext, DR_REG_P0 + i, mem, save));
        saved_regs++;
    }
}
static void
insert_save_or_restore_sve_registers(
    dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr, bool *reg_skip,
    reg_id_t base_reg, reg_id_t first_reg, bool save, reg_type_t rtype,
    opnd_t (*get_mem_opnd)(uint base_reg, bool is_single_reg, reg_type_t rtype,
                           uint num_saved, callee_info_t *ci),
    callee_info_t *ci)
{
    ASSERT(rtype == SVE_ZREG_TYPE);
    ASSERT(first_reg == DR_REG_Z0);
    ASSERT(MCXT_NUM_FFR_SLOTS == 1);

    // SVE Z registers
    uint i, saved_regs = 0;
    for (i = 0; i < MCXT_NUM_SIMD_SVE_SLOTS; i++) {
        if (reg_skip != NULL && reg_skip[i])
            continue;

        opnd_t mem =
            get_mem_opnd(base_reg, /*is_single_reg=*/true, SVE_ZREG_TYPE, saved_regs, ci);
        /* disp should never be greater than MAX_SVE_STR_OFFSET because it
         * is the immediate multiplied by the current vector register size
         * in bytes: STR <Zt>, [<Xn|SP>{, #<imm>, MUL VL}] and we only go up
         * MCXT_NUM_SIMD_SVE_SLOTS registers.
         */
        ASSERT(opnd_get_disp(mem) / proc_get_vector_length_bytes() <= MAX_SVE_STR_OFFSET);
        PRE(ilist, instr, create_load_or_store_instr(dcontext, DR_REG_Z0 + i, mem, save));
        saved_regs++;
    }

    /* add base_reg, base_reg, #(SVE register offset) */
    PRE(ilist, instr,
        XINST_CREATE_add(dcontext, opnd_create_reg(base_reg),
                         OPND_CREATE_INT16(MCXT_NUM_SIMD_SVE_SLOTS * sizeof(dr_simd_t))));

    /* The FFR register cannot be loaded directly into the base as the ld/str register has
     * to be a predicate.  Which means that the FFR saving has to be after the predicates,
     * and vice versa when loading.
     *
     * Save Seq:
     * - Save preds
     * - Save FFR to P15
     * - Store P15 to x0 (offset 16 to skip past preds)
     *
     * Load Seq:
     * - Read x0 to P15 (offset 16 to skip past preds)
     * - Write P15 to FFR
     * - Restore preds
     */
    const bool handle_ffr =
        reg_skip == NULL || !reg_skip[MCXT_NUM_SIMD_SVE_SLOTS + MCXT_NUM_SVEP_SLOTS];
    // SVE P and FFR registers.
    if (save) {
        insert_save_or_restore_svep_registers(dcontext, ilist, instr, reg_skip, base_reg,
                                              save, get_mem_opnd, ci);

        if (handle_ffr) {
            PRE(ilist, instr,
                INSTR_CREATE_rdffr_sve(
                    dcontext, opnd_create_reg_element_vector(DR_REG_P15, OPSZ_1)));
            opnd_t mem =
                get_mem_opnd(base_reg, /*is_single_reg=*/true, SVE_PREG_TYPE, 16, ci);
            PRE(ilist, instr,
                create_load_or_store_instr(dcontext, DR_REG_P15, mem, save));
        }
    } else {
        if (handle_ffr) {
            opnd_t mem =
                get_mem_opnd(base_reg, /*is_single_reg=*/true, SVE_PREG_TYPE, 16, ci);
            PRE(ilist, instr,
                create_load_or_store_instr(dcontext, DR_REG_P15, mem, save));
            PRE(ilist, instr,
                INSTR_CREATE_wrffr_sve(
                    dcontext, opnd_create_reg_element_vector(DR_REG_P15, OPSZ_1)));
        }

        insert_save_or_restore_svep_registers(dcontext, ilist, instr, reg_skip, base_reg,
                                              save, get_mem_opnd, ci);
    }
}

static void
insert_save_or_restore_registers(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                                 bool *reg_skip, reg_id_t base_reg, reg_id_t first_reg,
                                 bool save, reg_type_t rtype,
                                 opnd_t (*get_mem_opnd)(uint base_reg, bool is_single_reg,
                                                        reg_type_t rtype, uint num_saved,
                                                        callee_info_t *ci),
                                 callee_info_t *ci)
{
    switch (rtype) {
    case GPR_REG_TYPE:
    case SIMD_REG_TYPE:
        insert_save_or_restore_gpr_simd_registers(dcontext, ilist, instr, reg_skip,
                                                  base_reg, first_reg, save, rtype,
                                                  get_mem_opnd, ci);
        break;
    case SVE_ZREG_TYPE:
        insert_save_or_restore_sve_registers(dcontext, ilist, instr, reg_skip, base_reg,
                                             first_reg, save, rtype, get_mem_opnd, ci);
        break;
    case SVE_PREG_TYPE:
        /* SVE Z, P and FFR registers are saved/restored sequentially in
         * insert_save_or_restore_sve_registers(). At this top level call layer
         * we use SVE_ZREG_TYPE to indicate all of SVE register bank.
         */
        CLIENT_ASSERT(false,
                      "internal error, use SVE_ZREG_TYPE for top level save/restore of "
                      "SVE registers.");
    default: ASSERT_NOT_REACHED();
    }
}

static void
insert_save_registers(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                      bool *reg_skip, reg_id_t base_reg, reg_id_t first_reg,
                      reg_type_t rtype)
{
    insert_save_or_restore_registers(dcontext, ilist, instr, reg_skip, base_reg,
                                     first_reg, true /* save */, rtype,
                                     create_base_disp_for_save_restore, NULL);
}

static void
insert_restore_registers(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                         bool *reg_skip, reg_id_t base_reg, reg_id_t first_reg,
                         reg_type_t rtype)
{
    insert_save_or_restore_registers(dcontext, ilist, instr, reg_skip, base_reg,
                                     first_reg, false /* restore */, rtype,
                                     create_base_disp_for_save_restore, NULL);
}

static opnd_t
inline_get_mem_opnd(uint base_reg, bool is_single_reg, reg_type_t rtype, uint reg_id,
                    callee_info_t *ci)
{
    return callee_info_slot_opnd(ci, SLOT_REG, reg_id);
}

void
insert_save_inline_registers(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                             bool *reg_skip, reg_id_t first_reg, reg_type_t rtype,
                             void *ci)
{
    insert_save_or_restore_registers(dcontext, ilist, instr, reg_skip, 0, first_reg,
                                     true /* save */, rtype, inline_get_mem_opnd,
                                     (callee_info_t *)ci);
}

void
insert_restore_inline_registers(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                                bool *reg_skip, reg_id_t first_reg, reg_type_t rtype,
                                void *ci)
{
    insert_save_or_restore_registers(dcontext, ilist, instr, reg_skip, 0, first_reg,
                                     false /* restore */, rtype, inline_get_mem_opnd,
                                     (callee_info_t *)ci);
}

#endif

/* Pushes not only the GPRs but also simd regs, xip, and xflags, in
 * priv_mcontext_t order.
 * The current stack pointer alignment should be passed.  Use 1 if
 * unknown (NOT 0).
 * Returns the amount of data pushed.  Does NOT fix up the xsp value pushed
 * to be the value prior to any pushes for x64 as no caller needs that
 * currently (they all build a priv_mcontext_t and have to do further xsp
 * fixups anyway).
 * Does NOT push the app's value of the stolen register.
 * If scratch is REG_NULL, spills a register for scratch space.
 */
uint
insert_push_all_registers(dcontext_t *dcontext, clean_call_info_t *cci,
                          instrlist_t *ilist, instr_t *instr, uint alignment,
                          opnd_t push_pc,
                          reg_id_t scratch /*optional*/
                              _IF_AARCH64(bool out_of_line))
{
    uint dstack_offs = 0;

    if (cci == NULL)
        cci = &default_clean_call_info;
    if (cci->preserve_mcontext || cci->num_simd_skip != proc_num_simd_registers()) {
        /* FIXME i#1551: once we add skipping of regs, need to keep shape here.
         * Also, num_opmask_skip is not applicable to ARM/AArch64.
         */
    }
    /* FIXME i#1551: once we have cci->num_simd_skip, skip this if possible */
#ifdef AARCH64
    ASSERT(proc_num_simd_registers() ==
           (MCXT_NUM_SIMD_SVE_SLOTS +
            (proc_has_feature(FEATURE_SVE) ? (MCXT_NUM_SVEP_SLOTS + MCXT_NUM_FFR_SLOTS)
                                           : 0)));

    /* X0 is used to hold the stack pointer. */
    cci->reg_skip[DR_REG_X0 - DR_REG_START_GPR] = false;
    /* X1 and X2 are used to save and restore the status and control registers. */
    cci->reg_skip[DR_REG_X1 - DR_REG_START_GPR] = false;
    cci->reg_skip[DR_REG_X2 - DR_REG_START_GPR] = false;
    /* X11 is used to calculate the target address of the clean call. */
    cci->reg_skip[DR_REG_X11 - DR_REG_START_GPR] = false;

    /* For out-of-line clean calls, the stack pointer is adjusted before jumping
     * to this code.
     */
    if (!out_of_line) {
        /* sub sp, sp, #clean_call_switch_stack_size */
        PRE(ilist, instr,
            XINST_CREATE_sub(dcontext, opnd_create_reg(DR_REG_SP),
                             OPND_CREATE_INT16(get_clean_call_switch_stack_size())));
    }

    /* Push GPRs. */
    insert_save_registers(dcontext, ilist, instr, cci->reg_skip, DR_REG_SP, DR_REG_X0,
                          GPR_REG_TYPE);

    dstack_offs += 32 * XSP_SZ;

    /* mov x0, sp (add %sp $0x0000 lsl $0x00 -> %x0) */
    PRE(ilist, instr,
        XINST_CREATE_move(dcontext, opnd_create_reg(DR_REG_X0),
                          opnd_create_reg(DR_REG_SP)));

    /* For out-of-line clean calls, X30 is saved before jumping to this code,
     * because it is used for the return address.
     */
    if (!out_of_line) {
        /* stp x30, x0, [sp, #x30_offset] */
        PRE(ilist, instr,
            INSTR_CREATE_stp(dcontext,
                             opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0,
                                                   REG_OFFSET(DR_REG_X30), OPSZ_16),
                             opnd_create_reg(DR_REG_X30), opnd_create_reg(DR_REG_X0)));
    }

    /* add x0, x0, #dstack_offs (add %x0 $0x0100 lsl $0x00 -> %x0) */
    PRE(ilist, instr,
        XINST_CREATE_add(dcontext, opnd_create_reg(DR_REG_X0),
                         OPND_CREATE_INT16(dstack_offs)));

    /* save the push_pc operand to the priv_mcontext_t.pc field */
    if (!(cci->skip_save_flags)) {
        if (opnd_is_immed_int(push_pc)) {
            /* movz   $0x0000 lsl $0x00 -> %x1 */
            PRE(ilist, instr,
                XINST_CREATE_load_int(dcontext, opnd_create_reg(DR_REG_X1), push_pc));
        } else {
            ASSERT(opnd_is_reg(push_pc));
            reg_id_t push_pc_reg = opnd_get_reg(push_pc);
            /* push_pc opnd is already pushed on the stack */
            /* ldr x1, [sp, #push_pc_offset] */
            PRE(ilist, instr,
                INSTR_CREATE_ldr(dcontext, opnd_create_reg(DR_REG_X1),
                                 OPND_CREATE_MEM64(DR_REG_SP, REG_OFFSET(push_pc_reg))));
        }

        /* str x1, [sp, #dstack_offset] (str %x1 -> +0x0100(%sp)[8byte]) */
        PRE(ilist, instr,
            INSTR_CREATE_str(dcontext, OPND_CREATE_MEM64(DR_REG_SP, dstack_offs),
                             opnd_create_reg(DR_REG_X1)));
    }

    dstack_offs += XSP_SZ;

    /* Save flag values using x1, x2. */
    /* mrs x1, nzcv (mrs %nzcv -> %x1)
     */
    PRE(ilist, instr,
        INSTR_CREATE_mrs(dcontext, opnd_create_reg(DR_REG_X1),
                         opnd_create_reg(DR_REG_NZCV)));
    /* mrs x2, fpcr (mrs %fpcr -> %x2) */
    PRE(ilist, instr,
        INSTR_CREATE_mrs(dcontext, opnd_create_reg(DR_REG_X2),
                         opnd_create_reg(DR_REG_FPCR)));
    /* stp w1, w2, [x0, #8] (stp %w1 %w2 -> +0x08(%x0)[8byte]) */
    PRE(ilist, instr,
        INSTR_CREATE_stp(dcontext, OPND_CREATE_MEM64(DR_REG_X0, 8),
                         opnd_create_reg(DR_REG_W1), opnd_create_reg(DR_REG_W2)));

    /* mrs x1, fpsr (mrs %fpsr -> %x1) */
    PRE(ilist, instr,
        INSTR_CREATE_mrs(dcontext, opnd_create_reg(DR_REG_X1),
                         opnd_create_reg(DR_REG_FPSR)));
    /* str w1, [x0, #16] (str %w1 -> +0x10(%x0)[4byte]) */
    PRE(ilist, instr,
        INSTR_CREATE_str(dcontext, OPND_CREATE_MEM32(DR_REG_X0, 16),
                         opnd_create_reg(DR_REG_W1)));

    /* The three flag registers take 12 bytes. */
    dstack_offs += 12;

    /* The SIMD register data is 16-byte-aligned. */
    dstack_offs = ALIGN_FORWARD(dstack_offs, 16);

    /* add x0, x0, #(dstack_offs - prev_dstack_offs) */
    PRE(ilist, instr,
        XINST_CREATE_add(dcontext, opnd_create_reg(DR_REG_X0),
                         OPND_CREATE_INT16(dstack_offs - 32 * XSP_SZ)));

    if (proc_has_feature(FEATURE_SVE)) {
        /* Save the SVE regs */
        insert_save_registers(dcontext, ilist, instr, cci->simd_skip, DR_REG_X0,
                              DR_REG_Z0, SVE_ZREG_TYPE);
    } else {
        /* Save the SIMD registers. */
        insert_save_registers(dcontext, ilist, instr, cci->simd_skip, DR_REG_X0,
                              DR_REG_Q0, SIMD_REG_TYPE);
    }

    dstack_offs += MCXT_NUM_SIMD_SLOTS * sizeof(dr_simd_t);

    /* Restore the registers we used. */
    /* ldp x0, x1, [sp] */
    PRE(ilist, instr,
        INSTR_CREATE_ldp(dcontext, opnd_create_reg(DR_REG_X0), opnd_create_reg(DR_REG_X1),
                         opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0, 0, OPSZ_16)));
    /* ldr x2, [sp, #x2_offset] */
    PRE(ilist, instr,
        INSTR_CREATE_ldr(dcontext, opnd_create_reg(DR_REG_X2),
                         opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0,
                                               REG_OFFSET(DR_REG_X2), OPSZ_8)));
#else
    /* vstmdb always does writeback */
    PRE(ilist, instr,
        INSTR_CREATE_vstmdb(dcontext, OPND_CREATE_MEMLIST(DR_REG_SP), SIMD_REG_LIST_LEN,
                            SIMD_REG_LIST_16_31));
    PRE(ilist, instr,
        INSTR_CREATE_vstmdb(dcontext, OPND_CREATE_MEMLIST(DR_REG_SP), SIMD_REG_LIST_LEN,
                            SIMD_REG_LIST_0_15));

    dstack_offs += proc_num_simd_registers() * sizeof(dr_simd_t);
    ASSERT(proc_num_simd_registers() == MCXT_NUM_SIMD_SLOTS);

    /* pc and aflags */
    if (cci->skip_save_flags) {
        /* even if we skip flag saves we want to keep mcontext shape */
        int offs_beyond_xmm = 2 * XSP_SZ;
        dstack_offs += offs_beyond_xmm;
        PRE(ilist, instr,
            XINST_CREATE_sub(dcontext, opnd_create_reg(DR_REG_SP),
                             OPND_CREATE_INT(offs_beyond_xmm)));
    } else {
        uint slot = TLS_REG0_SLOT;
        bool spill = scratch == REG_NULL;
        if (spill) {
            scratch = DR_REG_R0;
            if (opnd_is_reg(push_pc) && opnd_get_reg(push_pc) == scratch) {
                scratch = DR_REG_R1;
                slot = TLS_REG1_SLOT;
            }
        }
        /* XXX: actually, r0 was just used as scratch for swapping stack
         * via dcontext, so an optimization opportunity exists to avoid
         * that restore and the re-spill here.
         */
        if (spill)
            PRE(ilist, instr, instr_create_save_to_tls(dcontext, scratch, slot));
        PRE(ilist, instr,
            INSTR_CREATE_mrs(dcontext, opnd_create_reg(scratch),
                             opnd_create_reg(DR_REG_CPSR)));
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(scratch)));
        dstack_offs += XSP_SZ;
        if (opnd_is_immed_int(push_pc)) {
            PRE(ilist, instr,
                XINST_CREATE_load_int(dcontext, opnd_create_reg(scratch), push_pc));
            PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(scratch)));
        } else {
            ASSERT(opnd_is_reg(push_pc));
            PRE(ilist, instr, INSTR_CREATE_push(dcontext, push_pc));
        }
        if (spill)
            PRE(ilist, instr, instr_create_restore_from_tls(dcontext, scratch, slot));
        dstack_offs += XSP_SZ;
    }

    /* We rely on dr_get_mcontext_priv() to fill in the app's stolen reg value
     * and sp value.
     */
    if (dr_get_isa_mode(dcontext) == DR_ISA_ARM_THUMB) {
        /* We can't use sp with stm */
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(DR_REG_LR)));
        /* We can't push sp w/ writeback, and in fact dr_get_mcontext() gets
         * sp from the stack swap so we can leave this empty.
         */
        PRE(ilist, instr,
            XINST_CREATE_sub(dcontext, opnd_create_reg(DR_REG_SP),
                             OPND_CREATE_INT(XSP_SZ)));
        PRE(ilist, instr,
            INSTR_CREATE_stmdb_wb(dcontext, OPND_CREATE_MEMLIST(DR_REG_SP),
                                  DR_REG_LIST_LENGTH_T32, DR_REG_LIST_T32));
    } else {
        PRE(ilist, instr,
            INSTR_CREATE_stmdb_wb(dcontext, OPND_CREATE_MEMLIST(DR_REG_SP),
                                  DR_REG_LIST_LENGTH_ARM, DR_REG_LIST_ARM));
    }
    dstack_offs += 15 * XSP_SZ;

    /* Make dstack_offs 8-byte algined, as we only accounted for 17 4-byte slots. */
    dstack_offs += XSP_SZ;
    ASSERT(cci->skip_save_flags || cci->num_simd_skip != 0 || cci->num_regs_skip != 0 ||
           dstack_offs == (uint)get_clean_call_switch_stack_size());
#endif
    return dstack_offs;
}

/* User should pass the alignment from insert_push_all_registers: i.e., the
 * alignment at the end of all the popping, not the alignment prior to
 * the popping.
 */
void
insert_pop_all_registers(dcontext_t *dcontext, clean_call_info_t *cci, instrlist_t *ilist,
                         instr_t *instr, uint alignment _IF_AARCH64(bool out_of_line))
{
    if (cci == NULL)
        cci = &default_clean_call_info;
#ifdef AARCH64
    uint current_offs;
    /* mov x0, sp */
    PRE(ilist, instr,
        XINST_CREATE_move(dcontext, opnd_create_reg(DR_REG_X0),
                          opnd_create_reg(DR_REG_SP)));

    current_offs =
        get_clean_call_switch_stack_size() - (MCXT_NUM_SIMD_SLOTS * sizeof(dr_simd_t));

    /* add x0, x0, current_offs */
    PRE(ilist, instr,
        XINST_CREATE_add(dcontext, opnd_create_reg(DR_REG_X0),
                         OPND_CREATE_INT32(current_offs)));

    if (proc_has_feature(FEATURE_SVE)) {
        /* Restore the SVE regs */
        insert_restore_registers(dcontext, ilist, instr, cci->simd_skip, DR_REG_X0,
                                 DR_REG_Z0, SVE_ZREG_TYPE);
    } else {
        /* Restore the SIMD registers. */
        insert_restore_registers(dcontext, ilist, instr, cci->simd_skip, DR_REG_X0,
                                 DR_REG_Q0, SIMD_REG_TYPE);
    }

    /* mov x0, sp */
    PRE(ilist, instr,
        XINST_CREATE_move(dcontext, opnd_create_reg(DR_REG_X0),
                          opnd_create_reg(DR_REG_SP)));

    /* point x0 to push_pc field */
    current_offs = (32 * XSP_SZ);

    /* add x0, x0, #gpr_size */
    PRE(ilist, instr,
        XINST_CREATE_add(dcontext, opnd_create_reg(DR_REG_X0),
                         OPND_CREATE_INT32(current_offs)));

    /* load pc and flags */
    if (!(cci->skip_save_flags)) {
        /* ldp w1, w2, [x0, #8] */
        PRE(ilist, instr,
            INSTR_CREATE_ldp(dcontext, opnd_create_reg(DR_REG_W1),
                             opnd_create_reg(DR_REG_W2),
                             OPND_CREATE_MEM64(DR_REG_X0, 8)));
        /* msr nzcv, x1 */
        PRE(ilist, instr,
            INSTR_CREATE_msr(dcontext, opnd_create_reg(DR_REG_NZCV),
                             opnd_create_reg(DR_REG_X1)));
        /* msr fpcr, x2 */
        PRE(ilist, instr,
            INSTR_CREATE_msr(dcontext, opnd_create_reg(DR_REG_FPCR),
                             opnd_create_reg(DR_REG_X2)));

        /* ldr w1, [x0, #16] */
        PRE(ilist, instr,
            INSTR_CREATE_ldr(dcontext, opnd_create_reg(DR_REG_W1),
                             OPND_CREATE_MEM32(DR_REG_X0, 16)));

        /* msr fpsr, x1 */
        PRE(ilist, instr,
            INSTR_CREATE_msr(dcontext, opnd_create_reg(DR_REG_FPSR),
                             opnd_create_reg(DR_REG_X1)));
    }

    /* Pop GPRs */
    insert_restore_registers(dcontext, ilist, instr, cci->reg_skip, DR_REG_SP, DR_REG_X0,
                             GPR_REG_TYPE);

    /* For out-of-line clean calls, X30 is restored after jumping back from this
     * code, because it is used for the return address.
     */
    if (!out_of_line) {
        /* Recover x30 */
        /* ldr x30, [sp, #x30_offset] */
        PRE(ilist, instr,
            INSTR_CREATE_ldr(dcontext, opnd_create_reg(DR_REG_X30),
                             OPND_CREATE_MEM64(DR_REG_SP, REG_OFFSET(DR_REG_X30))));
        PRE(ilist, instr,
            XINST_CREATE_add(dcontext, opnd_create_reg(DR_REG_SP),
                             OPND_CREATE_INT16(get_clean_call_switch_stack_size())));
    }

#else
    /* We rely on dr_set_mcontext_priv() to set the app's stolen reg value,
     * and the stack swap to set the sp value: we assume the stolen reg on
     * the stack still has our TLS base in it.
     */
    /* We can't use sp with ldm for Thumb, and we don't want to write sp for ARM. */
    PRE(ilist, instr,
        INSTR_CREATE_ldm_wb(dcontext, OPND_CREATE_MEMLIST(DR_REG_SP),
                            DR_REG_LIST_LENGTH_T32, DR_REG_LIST_T32));
    /* We don't want the sp value */
    PRE(ilist, instr,
        XINST_CREATE_add(dcontext, opnd_create_reg(DR_REG_SP), OPND_CREATE_INT(XSP_SZ)));
    PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(DR_REG_LR)));

    /* pc and aflags */
    if (cci->skip_save_flags) {
        /* even if we skip flag saves we still keep mcontext shape */
        int offs_beyond_xmm = 2 * XSP_SZ;
        PRE(ilist, instr,
            XINST_CREATE_add(dcontext, opnd_create_reg(DR_REG_SP),
                             OPND_CREATE_INT(offs_beyond_xmm)));
    } else {
        reg_id_t scratch = DR_REG_R0;
        uint slot = TLS_REG0_SLOT;
        /* just throw pc slot away */
        PRE(ilist, instr,
            XINST_CREATE_add(dcontext, opnd_create_reg(DR_REG_SP),
                             OPND_CREATE_INT(XSP_SZ)));
        PRE(ilist, instr, instr_create_save_to_tls(dcontext, scratch, slot));
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(scratch)));
        PRE(ilist, instr,
            INSTR_CREATE_msr(dcontext, opnd_create_reg(DR_REG_CPSR),
                             OPND_CREATE_INT_MSR_NZCVQG(), opnd_create_reg(scratch)));
        PRE(ilist, instr, instr_create_restore_from_tls(dcontext, scratch, slot));
    }
    /* FIXME i#1551: once we have cci->num_simd_skip, skip this if possible */
    PRE(ilist, instr,
        INSTR_CREATE_vldm_wb(dcontext, OPND_CREATE_MEMLIST(DR_REG_SP), SIMD_REG_LIST_LEN,
                             SIMD_REG_LIST_0_15));
    PRE(ilist, instr,
        INSTR_CREATE_vldm_wb(dcontext, OPND_CREATE_MEMLIST(DR_REG_SP), SIMD_REG_LIST_LEN,
                             SIMD_REG_LIST_16_31));
#endif
}

#ifndef AARCH64
reg_id_t
shrink_reg_for_param(reg_id_t regular, opnd_t arg)
{
    return regular;
}
#endif /* !AARCH64 */

/* Return true if opnd is a register, but not XSP, or immediate zero on AArch64. */
static bool
opnd_is_reglike(opnd_t opnd)
{
    return ((opnd_is_reg(opnd) && opnd_get_reg(opnd) != DR_REG_XSP)
                IF_X64(|| (opnd_is_immed_int(opnd) && opnd_get_immed_int(opnd) == 0)));
}

uint
insert_parameter_preparation(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                             bool clean_call, uint num_args, opnd_t *args)
{
    uint num_regs = num_args < NUM_REGPARM ? num_args : NUM_REGPARM;
    signed char regs[NUM_REGPARM];
    int usecount[NUM_REGPARM];
    ptr_int_t stack_inc = 0;
    uint i, j;

    /* We expect every arg to be an immediate integer, a full-size register,
     * or a simple memory reference (NYI).
     */
    for (i = 0; i < num_args; i++) {
        CLIENT_ASSERT(opnd_is_immed_int((args[i])) ||
                          (opnd_is_reg(args[i]) &&
                           reg_get_size(opnd_get_reg(args[i])) == OPSZ_PTR) ||
                          opnd_is_base_disp(args[i]),
                      "insert_parameter_preparation: bad argument type");
        ASSERT_NOT_IMPLEMENTED(!opnd_is_base_disp(args[i])); /* FIXME i#2210 */
    }

    /* The strategy here is to first set up the arguments that can be set up
     * without using a temporary register: stack arguments that are registers and
     * register arguments that are not involved in a cycle. When this has been done,
     * the value in the link register (LR) will be dead, so we can use LR as a
     * temporary for setting up the remaining arguments.
     */

    /* Set up stack arguments that are registers (not SP) or zero (on AArch64). */
    if (num_args > NUM_REGPARM) {
        uint n = num_args - NUM_REGPARM;
        /* On both ARM and AArch64 the stack pointer is kept (2 * XSP_SZ)-aligned. */
        stack_inc = ALIGN_FORWARD(n, 2) * XSP_SZ;
#ifdef AARCH64
        for (i = 0; i < n; i += 2) {
            opnd_t *arg0 = &args[NUM_REGPARM + i];
            opnd_t *arg1 = i + 1 < n ? &args[NUM_REGPARM + i + 1] : NULL;
            if (i == 0) {
                if (i + 1 < n && opnd_is_reglike(*arg1)) {
                    /* stp x(...), x(...), [sp, #-(stack_inc)]! */
                    PRE(ilist, instr,
                        instr_create_2dst_4src(
                            dcontext, OP_stp,
                            opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, -stack_inc,
                                                  OPSZ_16),
                            opnd_create_reg(DR_REG_XSP),
                            opnd_is_reg(*arg0) ? *arg0 : opnd_create_reg(DR_REG_XZR),
                            opnd_is_reg(*arg1) ? *arg1 : opnd_create_reg(DR_REG_XZR),
                            opnd_create_reg(DR_REG_XSP),
                            opnd_create_immed_int(-stack_inc, OPSZ_PTR)));
                } else if (opnd_is_reglike(*arg0)) {
                    /* str x(...), [sp, #-(stack_inc)]! */
                    PRE(ilist, instr,
                        instr_create_2dst_3src(
                            dcontext, OP_str,
                            opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, -stack_inc,
                                                  OPSZ_PTR),
                            opnd_create_reg(DR_REG_XSP),
                            opnd_is_reg(*arg0) ? *arg0 : opnd_create_reg(DR_REG_XZR),
                            opnd_create_reg(DR_REG_XSP),
                            opnd_create_immed_int(-stack_inc, OPSZ_PTR)));
                } else {
                    /* sub sp, sp, #(stack_inc) */
                    PRE(ilist, instr,
                        INSTR_CREATE_sub(dcontext, opnd_create_reg(DR_REG_XSP),
                                         opnd_create_reg(DR_REG_XSP),
                                         OPND_CREATE_INT32(stack_inc)));
                }
            } else if (opnd_is_reglike(*arg0)) {
                if (i + 1 < n && opnd_is_reglike(*arg1)) {
                    /* stp x(...), x(...), [sp, #(i * XSP_SZ)] */
                    PRE(ilist, instr,
                        instr_create_1dst_2src(
                            dcontext, OP_stp,
                            opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, i * XSP_SZ,
                                                  OPSZ_16),
                            opnd_is_reg(*arg0) ? *arg0 : opnd_create_reg(DR_REG_XZR),
                            opnd_is_reg(*arg1) ? *arg1 : opnd_create_reg(DR_REG_XZR)));
                } else {
                    /* str x(...), [sp, #(i * XSP_SZ)] */
                    PRE(ilist, instr,
                        instr_create_1dst_1src(
                            dcontext, OP_str,
                            opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, i * XSP_SZ,
                                                  OPSZ_PTR),
                            opnd_is_reg(*arg0) ? *arg0 : opnd_create_reg(DR_REG_XZR)));
                }
            } else if (i + 1 < n && opnd_is_reglike(*arg1)) {
                /* str x(...), [sp, #((i + 1) * XSP_SZ)] */
                PRE(ilist, instr,
                    instr_create_1dst_1src(
                        dcontext, OP_str,
                        opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0,
                                              (i + 1) * XSP_SZ, OPSZ_PTR),
                        opnd_is_reg(*arg1) ? *arg1 : opnd_create_reg(DR_REG_XZR)));
            }
        }
#else /* ARM */
        /* XXX: We could use OP_stm here, but with lots of awkward corner cases. */
        PRE(ilist, instr,
            INSTR_CREATE_sub(dcontext, opnd_create_reg(DR_REG_XSP),
                             opnd_create_reg(DR_REG_XSP), OPND_CREATE_INT32(stack_inc)));
        for (i = 0; i < n; i++) {
            opnd_t arg = args[NUM_REGPARM + i];
            if (opnd_is_reglike(arg)) {
                /* str r(...), [sp, #(i * XSP_SZ)] */
                PRE(ilist, instr,
                    XINST_CREATE_store(dcontext,
                                       opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0,
                                                             i * XSP_SZ, OPSZ_PTR),
                                       arg));
            }
        }
#endif
    }

    /* Initialise regs[], which encodes the contents of parameter registers.
     * A non-negative value x means d_r_regparms[x];
     * -1 means an immediate integer;
     * -2 means a non-parameter register.
     */
    for (i = 0; i < num_regs; i++) {
        if (opnd_is_immed_int(args[i]))
            regs[i] = -1;
        else {
            reg_id_t reg = opnd_get_reg(args[i]);
            regs[i] = -2;
            for (j = 0; j < NUM_REGPARM; j++) {
                if (reg == d_r_regparms[j]) {
                    regs[i] = j;
                    break;
                }
            }
        }
    }

    /* Initialise usecount[]: how many other registers use the value in a reg. */
    for (i = 0; i < num_regs; i++)
        usecount[i] = 0;
    for (i = 0; i < num_regs; i++) {
        if (regs[i] >= 0 && regs[i] != i)
            ++usecount[regs[i]];
    }

    /* Set up register arguments that are not part of a cycle. */
    {
        bool changed;
        do {
            changed = false;
            for (i = 0; i < num_regs; i++) {
                if (regs[i] == i || usecount[i] != 0)
                    continue;
                if (regs[i] == -1) {
                    insert_mov_immed_ptrsz(dcontext, opnd_get_immed_int(args[i]),
                                           opnd_create_reg(d_r_regparms[i]), ilist, instr,
                                           NULL, NULL);
                } else if (regs[i] == -2 && opnd_get_reg(args[i]) == DR_REG_XSP) {
                    /* XXX: We could record which register has been set to the SP to
                     * avoid repeating this load if several arguments are set to SP.
                     */
                    insert_get_mcontext_base(dcontext, ilist, instr, d_r_regparms[i]);
                    PRE(ilist, instr,
                        instr_create_restore_from_dc_via_reg(
                            dcontext, d_r_regparms[i], d_r_regparms[i], XSP_OFFSET));
                } else {
                    PRE(ilist, instr,
                        XINST_CREATE_move(dcontext, opnd_create_reg(d_r_regparms[i]),
                                          args[i]));
                    if (regs[i] != -2)
                        --usecount[regs[i]];
                }
                regs[i] = i;
                changed = true;
            }
        } while (changed);
    }

    /* From now on it is safe to use LR as a temporary. */

    /* Set up register arguments that are in cycles. A rotation of n values is
     * realised with (n + 1) moves.
     */
    for (;;) {
        int first, tmp;
        for (i = 0; i < num_regs; i++) {
            if (regs[i] != i)
                break;
        }
        if (i >= num_regs)
            break;
        first = i;
        PRE(ilist, instr,
            XINST_CREATE_move(dcontext, opnd_create_reg(DR_REG_LR),
                              opnd_create_reg(d_r_regparms[i])));
        do {
            tmp = regs[i];
            ASSERT(0 <= tmp && tmp < num_regs);
            PRE(ilist, instr,
                XINST_CREATE_move(dcontext, opnd_create_reg(d_r_regparms[i]),
                                  tmp == first ? opnd_create_reg(DR_REG_LR)
                                               : opnd_create_reg(d_r_regparms[tmp])));
            regs[i] = i;
            i = tmp;
        } while (tmp != first);
    }

    /* Set up stack arguments that are (non-zero) constants or SP. */
    for (i = NUM_REGPARM; i < num_args; i++) {
        uint off = (i - NUM_REGPARM) * XSP_SZ;
        opnd_t arg = args[i];
        if (!opnd_is_reglike(arg)) {
            if (opnd_is_reg(arg)) {
                ASSERT(opnd_get_reg(arg) == DR_REG_XSP);
                insert_get_mcontext_base(dcontext, ilist, instr, DR_REG_LR);
                PRE(ilist, instr,
                    instr_create_restore_from_dc_via_reg(dcontext, DR_REG_LR, DR_REG_LR,
                                                         XSP_OFFSET));
            } else {
                ASSERT(opnd_is_immed_int(arg));
                insert_mov_immed_ptrsz(dcontext, opnd_get_immed_int(arg),
                                       opnd_create_reg(DR_REG_LR), ilist, instr, NULL,
                                       NULL);
            }
            PRE(ilist, instr,
                XINST_CREATE_store(
                    dcontext,
                    opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, off, OPSZ_PTR),
                    opnd_create_reg(DR_REG_LR)));
        }
    }

    return (uint)stack_inc;
}

bool
insert_reachable_cti(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                     byte *encode_pc, byte *target, bool jmp, bool returns, bool precise,
                     reg_id_t scratch, instr_t **inlined_tgt_instr)
{
    ASSERT(scratch != REG_NULL); /* required */
    /* load target into scratch register */
    insert_mov_immed_ptrsz(dcontext,
                           (ptr_int_t)PC_AS_JMP_TGT(dr_get_isa_mode(dcontext), target),
                           opnd_create_reg(scratch), ilist, where, NULL, NULL);
    /* even if a call and not a jmp, we can skip this if it doesn't return */
    if (!jmp && returns) {
        PRE(ilist, where, XINST_CREATE_call_reg(dcontext, opnd_create_reg(scratch)));
    } else {
        PRE(ilist, where, XINST_CREATE_jump_reg(dcontext, opnd_create_reg(scratch)));
    }
    return false /* an ind branch */;
}

int
insert_out_of_line_context_switch(dcontext_t *dcontext, instrlist_t *ilist,
                                  instr_t *instr, bool save, byte *encode_pc)
{
#ifdef AARCH64
    if (save) {
        /* Reserve stack space to push the context. We do it here instead of
         * in insert_push_all_registers, so we can save the original value
         * of X30 on the stack before it is changed by the BL (branch & link)
         * to the clean call save routine in the code cache.
         *
         * sub sp, sp, #clean_call_switch_stack_size
         */
        PRE(ilist, instr,
            XINST_CREATE_sub(dcontext, opnd_create_reg(DR_REG_SP),
                             OPND_CREATE_INT16(get_clean_call_switch_stack_size())));

        /* str x30, [sp, #x30_offset]
         *
         * We have to save the original value of x30 before using BLR to jump
         * to the save code, because BLR will modify x30. The original value of
         * x30 is restored after the returning from the save/restore functions below.
         */
        PRE(ilist, instr,
            INSTR_CREATE_str(dcontext,
                             opnd_create_base_disp(DR_REG_SP, DR_REG_NULL, 0,
                                                   REG_OFFSET(DR_REG_X30), OPSZ_8),
                             opnd_create_reg(DR_REG_X30)));
    }

    insert_mov_immed_ptrsz(
        dcontext,
        (long)(save ? get_clean_call_save(dcontext) : get_clean_call_restore(dcontext)),
        opnd_create_reg(DR_REG_X30), ilist, instr, NULL, NULL);
    PRE(ilist, instr, INSTR_CREATE_blr(dcontext, opnd_create_reg(DR_REG_X30)));

    /* Restore original value of X30, which was changed by BLR.
     *
     * ldr x30, [sp, #x30_offset]
     */
    PRE(ilist, instr,
        INSTR_CREATE_ldr(dcontext, opnd_create_reg(DR_REG_X30),
                         OPND_CREATE_MEM64(DR_REG_SP, REG_OFFSET(DR_REG_X30))));

    if (!save) {
        /* add sp, sp, #clean_call_switch_stack_size */
        PRE(ilist, instr,
            XINST_CREATE_add(dcontext, opnd_create_reg(DR_REG_SP),
                             OPND_CREATE_INT16(get_clean_call_switch_stack_size())));
    }

    return get_clean_call_switch_stack_size();
#else
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1621: NYI on AArch32. */
    return 0;
#endif
}

/*###########################################################################
 *###########################################################################
 *
 * MANGLING ROUTINES
 */

/* forward declaration */
static void
mangle_stolen_reg(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                  instr_t *next_instr, bool instr_to_be_removed);

#ifndef AARCH64

/* i#1662 optimization: we try to pick the same scratch register during
 * mangling to provide more opportunities for optimization,
 * xref insert_save_to_tls_if_necessary().
 *
 * Returns the prev reg restore instruction.
 */
static instr_t *
find_prior_scratch_reg_restore(dcontext_t *dcontext, instr_t *instr, reg_id_t *prior_reg)
{
    instr_t *prev = instr_get_prev(instr);
    bool tls, spill;

    ASSERT(prior_reg != NULL);
    *prior_reg = REG_NULL;
    if (INTERNAL_OPTION(opt_mangle) == 0)
        return NULL;
    while (prev != NULL &&
           /* We can eliminate the restore/respill pair only if they are executed
            * together, so only our own mangling label instruction is allowed in
            * between.
            */
           instr_is_label(prev) && instr_is_our_mangling(prev))
        prev = instr_get_prev(prev);
    if (prev != NULL &&
        instr_is_DR_reg_spill_or_restore(dcontext, prev, &tls, &spill, prior_reg, NULL)) {
        if (tls && !spill && *prior_reg >= SCRATCH_REG0 && *prior_reg <= SCRATCH_REG_LAST)
            return prev;
    }
    *prior_reg = REG_NULL;
    return NULL;
}

#endif /* !AARCH64 */

/* optimized spill: only if not immediately spilled already */
static void
insert_save_to_tls_if_necessary(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                                reg_id_t reg, ushort slot)
{
#ifdef AARCH64
    /* FIXME i#1569: not yet optimized */
    PRE(ilist, where, instr_create_save_to_tls(dcontext, reg, slot));
#else
    instr_t *prev;
    reg_id_t prior_reg;
    DEBUG_DECLARE(bool tls;)
    DEBUG_DECLARE(bool spill;)

    /* this routine is only called for non-mbr mangling */
    STATS_INC(non_mbr_spills);
    prev = find_prior_scratch_reg_restore(dcontext, where, &prior_reg);
    if (INTERNAL_OPTION(opt_mangle) > 0 && prev != NULL && prior_reg == reg) {
        /* XXX: This looks only for dr_reg_stolen. */
        ASSERT(instr_is_DR_reg_spill_or_restore(dcontext, prev, &tls, &spill, &prior_reg,
                                                NULL) &&
               tls && !spill && prior_reg == reg);
        /* remove the redundant restore-spill pair */
        instrlist_remove(ilist, prev);
        instr_destroy(dcontext, prev);
        STATS_INC(non_mbr_respill_avoided);
    } else {
        PRE(ilist, where, instr_create_save_to_tls(dcontext, reg, slot));
    }
#endif
}

#ifndef AARCH64

/* If instr is inside an IT block, removes it from the block and
 * leaves it as an isolated (un-encodable) predicated instr, with any
 * other instrs from the same block made to be legal on both sides by
 * modifying and adding new OP_it instrs as necessary, which are marked
 * as app instrs.
 * Returns a new next_instr.
 */
static instr_t *
mangle_remove_from_it_block(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    instr_t *prev, *it;
    uint prior, count;
    if (instr_get_isa_mode(instr) != DR_ISA_ARM_THUMB || !instr_is_predicated(instr))
        return instr_get_next(instr); /* nothing to do */
    for (prior = 0, prev = instr_get_prev(instr); prev != NULL;
         prior++, prev = instr_get_prev(prev)) {
        if (instr_get_opcode(prev) == OP_it)
            break;
        ASSERT(instr_is_predicated(prev));
    }
    ASSERT(prev != NULL);
    it = prev;
    count = instr_it_block_get_count(it);
    ASSERT(count > prior && count <= IT_BLOCK_MAX_INSTRS);
    if (prior > 0) {
        instrlist_preinsert(ilist, it,
                            instr_it_block_create(
                                dcontext, instr_it_block_get_pred(it, 0),
                                prior > 1 ? instr_it_block_get_pred(it, 1) : DR_PRED_NONE,
                                prior > 2 ? instr_it_block_get_pred(it, 2) : DR_PRED_NONE,
                                DR_PRED_NONE));
        count -= prior;
    }
    count--; /* this instr */
    if (count > 0) {
        instrlist_postinsert(
            ilist, instr,
            instr_it_block_create(
                dcontext, instr_it_block_get_pred(it, prior + 1),
                count > 1 ? instr_it_block_get_pred(it, prior + 2) : DR_PRED_NONE,
                count > 2 ? instr_it_block_get_pred(it, prior + 3) : DR_PRED_NONE,
                DR_PRED_NONE));
    }
    /* It is now safe to remove the original OP_it instr */
    instrlist_remove(ilist, it);
    instr_destroy(dcontext, it);
    DOLOG(5, LOG_INTERP, {
        LOG(THREAD, LOG_INTERP, 4, "bb ilist after removing from IT block:\n");
        instrlist_disassemble(dcontext, NULL, ilist, THREAD);
    });
    return instr_get_next(instr);
}

/* Adds enough OP_it instrs to ensure that each predicated instr in [start, end)
 * (open-ended, so pass NULL to go to the final instr in ilist) is inside an IT
 * block and is thus legally encodable.  Marks the OP_it instrs as app instrs.
 */
int
reinstate_it_blocks(dcontext_t *dcontext, instrlist_t *ilist, instr_t *start,
                    instr_t *end)
{
    instr_t *instr, *block_start = NULL;
    app_pc block_xl8 = NULL;
    int res = 0;
    uint it_count = 0, block_count = 0;
    dr_pred_type_t block_pred[IT_BLOCK_MAX_INSTRS];
    for (instr = start; instr != NULL && instr != end; instr = instr_get_next(instr)) {
        bool instr_predicated = instr_is_predicated(instr) &&
            /* A label instruction may be used as a cti target, so we stop
             * the IT block on label instructions.
             */
            !instr_is_label(instr) &&
            /* Do not put OP_b exit cti into block: patch_branch can't handle */
            instr_get_opcode(instr) != OP_b && instr_get_opcode(instr) != OP_b_short;
        if (block_start != NULL) {
            bool matches = true;
            ASSERT(block_count < IT_BLOCK_MAX_INSTRS);
            if (instr_predicated) {
                if (instr_get_predicate(instr) != block_pred[0] &&
                    instr_get_predicate(instr) != instr_invert_predicate(block_pred[0]))
                    matches = false;
                else
                    block_pred[block_count++] = instr_get_predicate(instr);
            }
            if (!matches || !instr_predicated || block_count == IT_BLOCK_MAX_INSTRS ||
                /* i#1702: a cti must end the IT-block */
                instr_is_cti(instr)) {
                res++;
                instrlist_preinsert(
                    ilist, block_start,
                    INSTR_XL8(instr_it_block_create(
                                  dcontext, block_pred[0],
                                  block_count > 1 ? block_pred[1] : DR_PRED_NONE,
                                  block_count > 2 ? block_pred[2] : DR_PRED_NONE,
                                  block_count > 3 ? block_pred[3] : DR_PRED_NONE),
                              block_xl8));
                block_start = NULL;
                if (instr_predicated && matches)
                    continue;
            } else
                continue;
        }
        /* Skip existing IT blocks.
         * XXX: merge w/ adjacent blocks.
         */
        if (it_count > 0)
            it_count--;
        else if (instr_get_opcode(instr) == OP_it)
            it_count = instr_it_block_get_count(instr);
        else if (instr_predicated) {
            instr_t *app;
            block_start = instr;
            block_pred[0] = instr_get_predicate(instr);
            block_count = 1;
            /* XXX i#1695: we want the xl8 to be the original app IT instr, if
             * it existed, as using the first instr inside the block will not
             * work on relocation.  Should we insert labels to keep that info
             * when we remove IT instrs?
             */
            for (app = instr; app != NULL && instr_get_app_pc(app) == NULL;
                 app = instr_get_next(app))
                /*nothing*/;
            if (app != NULL)
                block_xl8 = instr_get_app_pc(app);
            else
                block_xl8 = NULL;
        }
    }
    if (block_start != NULL) {
        res++;
        instrlist_preinsert(ilist, block_start,
                            INSTR_XL8(instr_it_block_create(
                                          dcontext, block_pred[0],
                                          block_count > 1 ? block_pred[1] : DR_PRED_NONE,
                                          block_count > 2 ? block_pred[2] : DR_PRED_NONE,
                                          block_count > 3 ? block_pred[3] : DR_PRED_NONE),
                                      block_xl8));
    }
    return res;
}

static void
mangle_reinstate_it_blocks(dcontext_t *dcontext, instrlist_t *ilist, instr_t *start,
                           instr_t *end)
{
    if (dr_get_isa_mode(dcontext) != DR_ISA_ARM_THUMB)
        return; /* nothing to do */
    reinstate_it_blocks(dcontext, ilist, start, end);
    DOLOG(5, LOG_INTERP, {
        LOG(THREAD, LOG_INTERP, 4, "bb ilist after reinstating IT blocks:\n");
        instrlist_disassemble(dcontext, NULL, ilist, THREAD);
    });
}

#endif /* !AARCH64 */

/* This is *not* a hot-patchable patch: i.e., it is subject to races. */
void
patch_mov_immed_arch(dcontext_t *dcontext, ptr_int_t val, byte *pc, instr_t *first,
                     instr_t *last)
{
#ifdef AARCH64
    uint *write_pc = (uint *)vmcode_get_writable_addr(pc);
    ASSERT(first != NULL && last != NULL);
    /* We expect OP_movz followed by up to 3 OP_movk. */
    ASSERT(instr_get_opcode(first) == OP_movz && opnd_is_reg(instr_get_dst(first, 0)));
    reg_id_t dst_reg = opnd_get_reg(instr_get_dst(first, 0));
    int instr_count = 1;
    for (instr_t *inst = instr_get_next(first); inst != NULL;
         inst = instr_get_next(inst)) {
        ++instr_count;
        ASSERT(instr_get_opcode(inst) == OP_movk && opnd_is_reg(instr_get_dst(inst, 0)));
        if (inst == last)
            break;
    }
    uint *end_pc = insert_mov_imm(write_pc, dst_reg, val);
    ASSERT(end_pc - write_pc <= instr_count);
    while (end_pc - write_pc < instr_count) {
        *end_pc = RAW_NOP_INST;
        ++end_pc;
    }
#else
    ASSERT_NOT_IMPLEMENTED(false); /* TODO i#1551: NYI */
#endif
}

/* Used for fault translation */
bool
instr_check_xsp_mangling(dcontext_t *dcontext, instr_t *inst, int *xsp_adjust)
{
    ASSERT(xsp_adjust != NULL);
    /* No current ARM/AArch64 mangling splits an atomic push/pop into emulated pieces:
     * the OP_ldm/OP_stm splits shouldn't need special translation handling.
     */
    return false;
}

void
mangle_syscall_arch(dcontext_t *dcontext, instrlist_t *ilist, uint flags, instr_t *instr,
                    instr_t *next_instr)
{
    /* inlined conditional system call mangling is not supported */
    ASSERT(!instr_is_predicated(instr));

    /* Shared routine already checked method, handled INSTR_NI_SYSCALL*,
     * and inserted the signal barrier and non-auto-restart nop.
     * If we get here, we're dealing with an ignorable syscall.
     */

    /* We assume that the stolen register will, in effect, be neither
     * read nor written by a system call as it is above the highest
     * register used for the syscall arguments or number. This assumption
     * currently seems to be valid on arm/arm64 Linux, which only writes the
     * return value (with system calls that return). When other kernels are
     * supported it may be necessary to move the stolen register value to a
     * safer register (one that is "callee-saved" and not used by the gateway
     * mechanism) before the system call, and restore it afterwards.
     */
    ASSERT(DR_REG_STOLEN_MIN > DR_REG_SYSNUM);
}

#ifdef UNIX
/* Inserts code to handle clone into ilist.
 * instr is the syscall instr itself.
 * Assumes that instructions exist beyond instr in ilist.
 */
void
mangle_insert_clone_code(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    /*    svc 0
     *    cbnz r0, parent
     *    jmp new_thread_dynamo_start
     *  parent:
     *    <post system call, etc.>
     */
    instr_t *in = instr_get_next(instr);
    instr_t *parent = INSTR_CREATE_label(dcontext);
    ASSERT(in != NULL);
    PRE(ilist, in,
        INSTR_CREATE_cbnz(dcontext, opnd_create_instr(parent),
                          opnd_create_reg(DR_REG_R0)));
    insert_reachable_cti(dcontext, ilist, in, vmcode_get_start(),
                         (byte *)get_new_thread_start(dcontext), true /*jmp*/,
                         false /*!returns*/, false /*!precise*/, DR_REG_R0 /*scratch*/,
                         NULL);
    instr_set_meta(instr_get_prev(in));
    PRE(ilist, in, parent);
}
#endif /* UNIX */

void
mangle_interrupt(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                 instr_t *next_instr)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1551, i#1569 */
}

#ifndef AARCH64

/* Adds a mov of the fall-through address into IBL_TARGET_REG, predicated
 * with the inverse of instr's predicate.
 * The caller must call mangle_reinstate_it_blocks() in Thumb mode afterward
 * in order to make for legal encodings.
 */
static void
mangle_add_predicated_fall_through(dcontext_t *dcontext, instrlist_t *ilist,
                                   instr_t *instr, instr_t *next_instr,
                                   instr_t *mangle_start)
{
    /* Our approach is to simply add a move-immediate of the fallthrough
     * address under the inverted predicate.  This is much simpler to
     * implement than adding a new kind of indirect branch ("conditional
     * indirect") and plumbing it through all the optimized emit and link
     * code (in particular, cbr stub sharing and other complex features).
     */
    dr_pred_type_t pred = instr_get_predicate(instr);
    ptr_int_t fall_through = get_call_return_address(dcontext, ilist, instr);
    instr_t *first, *last;
    ASSERT(instr_is_predicated(instr)); /* caller should check */

    /* Mark the taken mangling as predicated.  We are starting after our r2
     * spill.  It gets complex w/ interactions with mangle_stolen_reg() (b/c
     * we aren't starting far enough back) so we bail for that.
     * For mangle_pc_read(), we simply don't predicate the restore (b/c
     * we aren't predicating the save).
     */
    if (!instr_uses_reg(instr, dr_reg_stolen)) {
        instr_t *prev = instr_get_next(mangle_start);
        for (; prev != next_instr; prev = instr_get_next(prev)) {
            if (instr_is_app(prev) ||
                !instr_is_DR_reg_spill_or_restore(dcontext, prev, NULL, NULL, NULL, NULL))
                instr_set_predicate(prev, pred);
        }
    }

    insert_mov_immed_ptrsz(
        dcontext,
        (ptr_int_t)PC_AS_JMP_TGT(instr_get_isa_mode(instr), (app_pc)fall_through),
        opnd_create_reg(IBL_TARGET_REG), ilist, next_instr, &first, &last);
    for (;; first = instr_get_next(first)) {
        instr_set_predicate(first, instr_invert_predicate(pred));
        if (last == NULL || first == last)
            break;
    }
}

static inline bool
app_instr_is_in_it_block(dcontext_t *dcontext, instr_t *instr)
{
    ASSERT(instr_is_app(instr));
    return (instr_get_isa_mode(instr) == DR_ISA_ARM_THUMB && instr_is_predicated(instr));
}

#endif /* !AARCH64 */

instr_t *
mangle_direct_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                   instr_t *next_instr, bool mangle_calls, uint flags)
{
#ifdef AARCH64
    ptr_int_t retaddr;

    ASSERT(instr_get_opcode(instr) == OP_bl);
    ASSERT(opnd_is_pc(instr_get_target(instr)));
    retaddr = get_call_return_address(dcontext, ilist, instr);
    insert_mov_immed_ptrsz(dcontext, retaddr, opnd_create_reg(DR_REG_X30), ilist, instr,
                           NULL, NULL);
    instrlist_remove(ilist, instr); /* remove OP_bl */
    instr_destroy(dcontext, instr);
    return next_instr;
#else
    /* Strategy: replace OP_bl with 2-step mov immed into lr + OP_b */
    ptr_uint_t retaddr;
    uint opc = instr_get_opcode(instr);
    ptr_int_t target;
    instr_t *first, *last;
    bool in_it = app_instr_is_in_it_block(dcontext, instr);
    instr_t *bound_start = INSTR_CREATE_label(dcontext);
    if (in_it) {
        /* split instr off from its IT block for easier mangling (we reinstate later) */
        next_instr = mangle_remove_from_it_block(dcontext, ilist, instr);
    }
    PRE(ilist, instr, bound_start);
    ASSERT(opc == OP_bl || opc == OP_blx);
    ASSERT(opnd_is_pc(instr_get_target(instr)));
    target = (ptr_int_t)opnd_get_pc(instr_get_target(instr));
    retaddr = get_call_return_address(dcontext, ilist, instr);
    insert_mov_immed_ptrsz(
        dcontext, (ptr_int_t)PC_AS_JMP_TGT(instr_get_isa_mode(instr), (app_pc)retaddr),
        opnd_create_reg(DR_REG_LR), ilist, instr, &first, &last);
    if (opc == OP_bl) {
        /* OP_blx predication is handled below */
        if (instr_is_predicated(instr)) {
            for (;; first = instr_get_next(first)) {
                instr_set_predicate(first, instr_get_predicate(instr));
                if (last == NULL || first == last)
                    break;
            }
            /* Add exit cti for taken direction b/c we're removing the OP_bl */
            instrlist_preinsert(
                ilist, instr,
                INSTR_PRED(XINST_CREATE_jump(dcontext, opnd_create_pc((app_pc)target)),
                           instr_get_predicate(instr)));
        }
    } else {
        /* Unfortunately while there is OP_blx with an immed, OP_bx requires
         * indirection through a register.  We thus need to swap modes separately,
         * but our ISA doesn't support mixing modes in one fragment, making
         * a local "blx next_instr" not easy.  We have two potential solutions:
         *   A) Implement far linking through stub's "ldr pc, [pc + 8]" and use
         *      it for blx.  We need to implement that anyway for reachability,
         *      but as it's not implemented yet, I'm going w/ B) for now.
         *   B) Pretend this is an indirect branch and use the ibl.
         *      This is slower so XXX i#1612: switch to A once we have far links.
         */
        if (instr_get_isa_mode(instr) == DR_ISA_ARM_A32)
            target = (ptr_int_t)PC_AS_JMP_TGT(DR_ISA_ARM_THUMB, (app_pc)target);
        PRE(ilist, instr,
            instr_create_save_to_tls(dcontext, IBL_TARGET_REG, IBL_TARGET_SLOT));
        insert_mov_immed_ptrsz(dcontext, target, opnd_create_reg(IBL_TARGET_REG), ilist,
                               instr, NULL, NULL);
        if (instr_is_predicated(instr)) {
            mangle_add_predicated_fall_through(dcontext, ilist, instr, next_instr,
                                               bound_start);
            ASSERT(in_it || instr_get_isa_mode(instr) != DR_ISA_ARM_THUMB);
        }
    }
    /* remove OP_bl (final added jmp already targets the callee) or OP_blx */
    instrlist_remove(ilist, instr);
    instr_destroy(dcontext, instr);
    if (in_it)
        mangle_reinstate_it_blocks(dcontext, ilist, bound_start, next_instr);
    return next_instr;
#endif
}

instr_t *
mangle_indirect_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                     instr_t *next_instr, bool mangle_calls, uint flags)
{
#ifdef AARCH64
    int opc = instr_get_opcode(instr);
    ASSERT(instr_is_call_indirect(instr));
    PRE(ilist, instr,
        instr_create_save_to_tls(dcontext, IBL_TARGET_REG, IBL_TARGET_SLOT));
    ASSERT(opnd_is_reg(instr_get_target(instr)));
    if (opnd_same(instr_get_target(instr), opnd_create_reg(dr_reg_stolen))) {
        /* if the target reg is dr_reg_stolen, the app value is in TLS */
        PRE(ilist, instr,
            instr_create_restore_from_tls(dcontext, IBL_TARGET_REG, TLS_REG_STOLEN_SLOT));
    } else {
        PRE(ilist, instr,
            XINST_CREATE_move(dcontext, opnd_create_reg(IBL_TARGET_REG),
                              instr_get_target(instr)));
    }
    switch (opc) {
    case OP_blraa:
    case OP_blrab:
    case OP_blraaz:
    case OP_blrabz:
        PRE(ilist, instr, INSTR_CREATE_xpaci(dcontext, opnd_create_reg(IBL_TARGET_REG)));
    }
    insert_mov_immed_ptrsz(dcontext, get_call_return_address(dcontext, ilist, instr),
                           opnd_create_reg(DR_REG_X30), ilist, next_instr, NULL, NULL);
    instrlist_remove(ilist, instr); /* remove OP_blr */
    instr_destroy(dcontext, instr);
    return next_instr;
#else
    ptr_uint_t retaddr;
    bool in_it = app_instr_is_in_it_block(dcontext, instr);
    instr_t *bound_start = INSTR_CREATE_label(dcontext);
    if (in_it) {
        /* split instr off from its IT block for easier mangling (we reinstate later) */
        next_instr = mangle_remove_from_it_block(dcontext, ilist, instr);
    }
    PRE(ilist, instr,
        instr_create_save_to_tls(dcontext, IBL_TARGET_REG, IBL_TARGET_SLOT));
    /* We need the spill to be unconditional so start pred processing here */
    PRE(ilist, instr, bound_start);

    if (!opnd_same(instr_get_target(instr), opnd_create_reg(IBL_TARGET_REG))) {
        if (opnd_same(instr_get_target(instr), opnd_create_reg(dr_reg_stolen))) {
            /* if the target reg is dr_reg_stolen, the app value is in TLS */
            PRE(ilist, instr,
                instr_create_restore_from_tls(dcontext, IBL_TARGET_REG,
                                              TLS_REG_STOLEN_SLOT));
        } else {
            PRE(ilist, instr,
                XINST_CREATE_move(dcontext, opnd_create_reg(IBL_TARGET_REG),
                                  instr_get_target(instr)));
        }
    }
    retaddr = get_call_return_address(dcontext, ilist, instr);
    insert_mov_immed_ptrsz(
        dcontext, (ptr_int_t)PC_AS_JMP_TGT(instr_get_isa_mode(instr), (app_pc)retaddr),
        opnd_create_reg(DR_REG_LR), ilist, instr, NULL, NULL);

    if (instr_is_predicated(instr)) {
        mangle_add_predicated_fall_through(dcontext, ilist, instr, next_instr,
                                           bound_start);
        ASSERT(in_it || instr_get_isa_mode(instr) != DR_ISA_ARM_THUMB);
    }
    /* remove OP_blx_ind (final added jmp already targets the callee) */
    instrlist_remove(ilist, instr);
    instr_destroy(dcontext, instr);
    if (in_it)
        mangle_reinstate_it_blocks(dcontext, ilist, bound_start, next_instr);
    return next_instr;
#endif
}

void
mangle_return(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
              instr_t *next_instr, uint flags)
{
    /* The mangling is identical */
    mangle_indirect_jump(dcontext, ilist, instr, next_instr, flags);
}

instr_t *
mangle_indirect_jump(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                     instr_t *next_instr, uint flags)
{
    int opc = instr_get_opcode(instr);
#ifdef AARCH64
    ASSERT((instr_branch_type(instr) == (LINK_INDIRECT | LINK_JMP)) ||
           (instr_branch_type(instr) == (LINK_INDIRECT | LINK_RETURN)));
    PRE(ilist, instr,
        instr_create_save_to_tls(dcontext, IBL_TARGET_REG, IBL_TARGET_SLOT));
    opnd_t target = instr_get_target(instr);
    ASSERT(opnd_is_reg(target));

    if (opnd_same(target, opnd_create_reg(dr_reg_stolen))) {
        /* if the target reg is dr_reg_stolen, the app value is in TLS */
        PRE(ilist, instr,
            instr_create_restore_from_tls(dcontext, IBL_TARGET_REG, TLS_REG_STOLEN_SLOT));
    } else {
        PRE(ilist, instr,
            XINST_CREATE_move(dcontext, opnd_create_reg(IBL_TARGET_REG), target));
    }

    switch (opc) {
    case OP_retaa:
    case OP_retab:
    case OP_braa:
    case OP_brab:
    case OP_braaz:
    case OP_brabz:
        PRE(ilist, instr, INSTR_CREATE_xpaci(dcontext, opnd_create_reg(IBL_TARGET_REG)));
    }

    instrlist_remove(ilist, instr); /* remove OP_br or OP_ret */
    instr_destroy(dcontext, instr);
    return next_instr;
#else
    bool remove_instr = false;
    dr_isa_mode_t isa_mode = instr_get_isa_mode(instr);
    bool in_it = app_instr_is_in_it_block(dcontext, instr);
    instr_t *bound_start = INSTR_CREATE_label(dcontext);
    if (in_it) {
        /* split instr off from its IT block for easier mangling (we reinstate later) */
        next_instr = mangle_remove_from_it_block(dcontext, ilist, instr);
    }
    PRE(ilist, instr,
        instr_create_save_to_tls(dcontext, IBL_TARGET_REG, IBL_TARGET_SLOT));
    /* We need the spill to be unconditional so start pred processing here */
    PRE(ilist, instr, bound_start);
    /* Most gpr_list writes are handled by mangle_gpr_list_write() by extracting
     * a single "ldr pc" instr out for mangling here, except simple instructions
     * like "pop pc". Xref mangle_gpr_list_write() for details.
     */
    if (instr_writes_gpr_list(instr)) {
        opnd_t memop = instr_get_src(instr, 0);
        /* must be simple cases like "pop pc" */
        ASSERT(opnd_is_base_disp(memop));
        ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_PC);
        /* FIXME i#1551: on A32, ldm* can have only one reg in the reglist,
         * i.e., "ldm r10, {pc}" is valid, so we should check dr_reg_stolen usage.
         */
        ASSERT_NOT_IMPLEMENTED(!opnd_uses_reg(memop, dr_reg_stolen));
        opnd_set_size(&memop, OPSZ_VAR_REGLIST);
        instr_set_src(instr, 0, memop);
        instr_set_dst(instr, 0, opnd_create_reg(IBL_TARGET_REG));
        /* We target only the typical return instructions: multi-pop here */
        if (TEST(INSTR_CLOBBER_RETADDR, instr->flags) && opc == OP_ldmia) {
            bool writeback = instr_num_srcs(instr) > 1;
            if (writeback) {
                opnd_set_disp(&memop, -sizeof(void *));
                opnd_set_size(&memop, OPSZ_PTR);
                /* We do not support writing a passed-in value as it would require
                 * spilling another reg.  We write the only non-retaddr-guaranteed
                 * reg we have, our stolen reg.
                 */
                POST(ilist, instr,
                     XINST_CREATE_store(dcontext, memop, opnd_create_reg(dr_reg_stolen)));
            } /* else not a pop */
        }
    } else if (opc == OP_bx || opc == OP_bxj) {
        ASSERT(opnd_is_reg(instr_get_target(instr)));
        if (opnd_same(instr_get_target(instr), opnd_create_reg(dr_reg_stolen))) {
            /* if the target reg is dr_reg_stolen, the app value is in TLS */
            PRE(ilist, instr,
                instr_create_restore_from_tls(dcontext, IBL_TARGET_REG,
                                              TLS_REG_STOLEN_SLOT));
        } else {
            PRE(ilist, instr,
                XINST_CREATE_move(dcontext, opnd_create_reg(IBL_TARGET_REG),
                                  instr_get_target(instr)));
        }
        /* remove the bx */
        remove_instr = true;
    } else if (opc == OP_tbb || opc == OP_tbh) {
        /* XXX: should we add add dr_insert_get_mbr_branch_target() for use
         * internally and by clients?  OP_tb{b,h} break our assumptions of the target
         * simply being stored as an absolute address at the memory operand location.
         * Instead, these are pc-relative: pc += memval*2.  However, it's non-trivial
         * to add that, as it requires duplicating all this mangling code.  Really
         * clients should use dr_insert_mbr_instrumentation(), and instr_get_target()
         * isn't that useful for mbrs.
         */
        ptr_int_t cur_pc = (ptr_int_t)decode_cur_pc(
            instr_get_raw_bits(instr), instr_get_isa_mode(instr), opc, instr);
        /* for case like tbh [pc, r10, lsl, #1] */
        if (instr_uses_reg(instr, dr_reg_stolen))
            mangle_stolen_reg(dcontext, ilist, instr, instr_get_next(instr), false);

        if (opc == OP_tbb) {
            PRE(ilist, instr,
                INSTR_CREATE_ldrb(dcontext, opnd_create_reg(IBL_TARGET_REG),
                                  instr_get_src(instr, 0)));
        } else {
            PRE(ilist, instr,
                INSTR_CREATE_ldrh(dcontext, opnd_create_reg(IBL_TARGET_REG),
                                  instr_get_src(instr, 0)));
        }
        PRE(ilist, instr,
            INSTR_CREATE_lsl(dcontext, opnd_create_reg(IBL_TARGET_REG),
                             opnd_create_reg(IBL_TARGET_REG), OPND_CREATE_INT(1)));
        /* Rather than steal another register and using movw,movt to put the pc
         * into it, we split the add up into 4 pieces.
         * Even if the memref is pc-relative, this is still faster than sharing
         * the pc from mangle_rel_addr() if we have mangle_rel_addr() use r2
         * as the scratch reg.
         * XXX: arrange for that to happen, when we refactor the ind br vs PC
         * and stolen reg mangling, if memref doesn't already use r2.
         */
        if (opc == OP_tbb) {
            /* One byte x2 won't touch the top half, so we use a movt to add: */
            PRE(ilist, instr,
                INSTR_CREATE_movt(dcontext, opnd_create_reg(IBL_TARGET_REG),
                                  OPND_CREATE_INT((cur_pc & 0xffff0000) >> 16)));
        } else {
            PRE(ilist, instr,
                XINST_CREATE_add(dcontext, opnd_create_reg(IBL_TARGET_REG),
                                 OPND_CREATE_INT(cur_pc & 0xff000000)));
            PRE(ilist, instr,
                XINST_CREATE_add(dcontext, opnd_create_reg(IBL_TARGET_REG),
                                 OPND_CREATE_INT(cur_pc & 0x00ff0000)));
        }
        PRE(ilist, instr,
            XINST_CREATE_add(dcontext, opnd_create_reg(IBL_TARGET_REG),
                             OPND_CREATE_INT(cur_pc & 0x0000ff00)));
        PRE(ilist, instr,
            XINST_CREATE_add(dcontext, opnd_create_reg(IBL_TARGET_REG),
                             /* These do not switch modes so we set LSB */
                             OPND_CREATE_INT((cur_pc & 0x000000ff) | 0x1)));
        /* remove the instr */
        remove_instr = true;
    } else if (opc == OP_rfe || opc == OP_rfedb || opc == OP_rfeda || opc == OP_rfeib ||
               opc == OP_eret) {
        /* FIXME i#1551: NYI on ARM */
        ASSERT_NOT_IMPLEMENTED(false);
    } else {
        /* Explicitly writes just the pc */
        uint i;
        bool found_pc;
        instr_t *immed_next = instr_get_next(instr);
        /* XXX: can anything (non-OP_ldm) have r2 as an additional dst? */
        ASSERT_NOT_IMPLEMENTED(
            !instr_writes_to_reg(instr, IBL_TARGET_REG, DR_QUERY_INCLUDE_ALL));
        for (i = 0; i < instr_num_dsts(instr); i++) {
            if (opnd_is_reg(instr_get_dst(instr, i)) &&
                opnd_get_reg(instr_get_dst(instr, i)) == DR_REG_PC) {
                found_pc = true;
                instr_set_dst(instr, i, opnd_create_reg(IBL_TARGET_REG));
                break;
            }
        }
        ASSERT(found_pc);
        if (isa_mode == DR_ISA_ARM_THUMB &&
            (instr_get_opcode(instr) == OP_mov || instr_get_opcode(instr) == OP_add)) {
            /* Some Thumb write-to-PC instructions (OP_add and OP_mov) are simple
             * non-mode-changing branches, so we set LSB to 1.
             */
            opnd_t src = opnd_create_reg(IBL_TARGET_REG);
            if (instr_get_opcode(instr) == OP_mov && !instr_is_predicated(instr)) {
                /* Optimization: we can replace the mov */
                src = instr_get_src(instr, 0);
                remove_instr = true;
            }
            if (instr_get_opcode(instr) == OP_add) {
                /* We need to add shift immeds: easiest to create a new add (i#1919) */
                PRE(ilist, instr,
                    INSTR_CREATE_add(dcontext, instr_get_dst(instr, 0),
                                     instr_get_src(instr, 0), instr_get_src(instr, 1)));
                remove_instr = true;
            }
            /* We want this before any mangle_rel_addr mangling */
            POST(ilist, instr,
                 INSTR_CREATE_orr(dcontext, opnd_create_reg(IBL_TARGET_REG), src,
                                  OPND_CREATE_INT(1)));
        }
        if (instr_uses_reg(instr, dr_reg_stolen)) {
            /* Stolen register mangling must happen after orr instr
             * inserted above but before any mangle_rel_addr mangling.
             */
            mangle_stolen_reg(dcontext, ilist, instr, immed_next, remove_instr);
        }
        /* We target only the typical return instructions: single pop here */
        if (TEST(INSTR_CLOBBER_RETADDR, instr->flags) && opc == OP_ldr) {
            bool writeback = instr_num_srcs(instr) > 1;
            if (writeback && opnd_is_immed_int(instr_get_src(instr, 1))) {
                opnd_t memop = instr_get_src(instr, 0);
                opnd_set_disp(&memop, -opnd_get_immed_int(instr_get_src(instr, 1)));
                /* See above: we just write our stolen reg value */
                POST(ilist, instr,
                     XINST_CREATE_store(dcontext, memop, opnd_create_reg(dr_reg_stolen)));
            } /* else not a pop */
        }
    }
    if (instr_is_predicated(instr)) {
        mangle_add_predicated_fall_through(dcontext, ilist, instr, next_instr,
                                           bound_start);
        ASSERT(in_it || isa_mode != DR_ISA_ARM_THUMB);
    }
    if (remove_instr) {
        instrlist_remove(ilist, instr);
        instr_destroy(dcontext, instr);
    }
    if (in_it)
        mangle_reinstate_it_blocks(dcontext, ilist, bound_start, next_instr);
    return next_instr;
#endif
}

/* Local single-instr-window scratch reg picker.  Only considers r0-r5, so the caller
 * must split up any GPR reg list first.  Assumes we only care about instrs that read
 * or write regs outside of r0-r5, so we'll only fail on instrs that can access 7
 * GPR's, and again caller should split those up.
 *
 * For some use case (e.g., mangle stolen reg), the scratch reg will be
 * used across the app instr, so we cannot pick a dead reg.
 *
 * Returns REG_NULL if fail to find a scratch reg.
 */
static reg_id_t
pick_scratch_reg(dcontext_t *dcontext, instr_t *instr, reg_id_t do_not_pick_a,
                 reg_id_t do_not_pick_b, reg_id_t do_not_pick_c, bool dead_reg_ok,
                 ushort *scratch_slot OUT, bool *should_restore OUT)
{
    reg_id_t reg;
    ushort slot = 0;
    if (should_restore != NULL)
        *should_restore = true;

#ifndef AARCH64 /* FIXME i#1569: not yet optimized */
    if (find_prior_scratch_reg_restore(dcontext, instr, &reg) != NULL &&
        reg != REG_NULL && !instr_uses_reg(instr, reg) &&
        !reg_overlap(reg, do_not_pick_a) && !reg_overlap(reg, do_not_pick_b) &&
        !reg_overlap(reg, do_not_pick_c) &&
        /* Ensure no conflict in scratch regs for PC or stolen reg
         * mangling vs ind br mangling.  We can't just check for mbr b/c
         * of OP_blx.
         */
        (!instr_is_cti(instr) || reg != IBL_TARGET_REG)) {
        ASSERT(reg >= SCRATCH_REG0 && reg <= SCRATCH_REG_LAST);
        slot = TLS_REG0_SLOT + sizeof(reg_t) * (reg - SCRATCH_REG0);
        DOLOG(4, LOG_INTERP, {
            dcontext_t *dcontext = get_thread_private_dcontext();
            LOG(THREAD, LOG_INTERP, 4, "use last scratch reg %s\n", reg_names[reg]);
        });
    } else
#endif
        reg = REG_NULL;

    if (reg == REG_NULL) {
        for (reg = SCRATCH_REG0, slot = TLS_REG0_SLOT; reg <= SCRATCH_REG_LAST;
             reg++, slot += sizeof(reg_t)) {
            if (!instr_uses_reg(instr, reg) && !reg_overlap(reg, do_not_pick_a) &&
                !reg_overlap(reg, do_not_pick_b) && !reg_overlap(reg, do_not_pick_c) &&
                /* not pick  IBL_TARGET_REG if instr is a cti */
                (!instr_is_cti(instr) || reg != IBL_TARGET_REG))
                break;
        }
    }
    /* We can only try to pick a dead register if the scratch reg usage
     * allows so (e.g., not across the app instr).
     */
    if (reg > SCRATCH_REG_LAST && dead_reg_ok) {
        /* Likely OP_ldm.  We'll have to pick a dead reg (non-ideal b/c a fault
         * could come in: i#400).
         */
        for (reg = SCRATCH_REG0, slot = TLS_REG0_SLOT; reg <= SCRATCH_REG_LAST;
             reg++, slot += sizeof(reg_t)) {
            if (!instr_reads_from_reg(instr, reg, DR_QUERY_INCLUDE_ALL) &&
                !reg_overlap(reg, do_not_pick_a) && !reg_overlap(reg, do_not_pick_b) &&
                !reg_overlap(reg, do_not_pick_c) &&
                /* Ensure no conflict vs ind br mangling */
                (!instr_is_cti(instr) || reg != IBL_TARGET_REG))
                break;
        }
        if (reg <= SCRATCH_REG_LAST && should_restore != NULL)
            *should_restore = false;
    }
    if (reg > SCRATCH_REG_LAST) {
        /* As a last resort, use a higher reg with a lower slot.  This may seem
         * confusing but the translation spill walk code handles it.
         */
        for (reg = SCRATCH_REG0, slot = TLS_REG0_SLOT; reg <= SCRATCH_REG_LAST;
             reg++, slot += sizeof(reg_t)) {
            if (!reg_overlap(reg, do_not_pick_a) && !reg_overlap(reg, do_not_pick_b) &&
                !reg_overlap(reg, do_not_pick_c) &&
                /* not pick  IBL_TARGET_REG if instr is a cti */
                (!instr_is_cti(instr) || reg != IBL_TARGET_REG))
                break;
        }
        /* We have a slot.  Now pick a reg. */
        for (reg = SCRATCH_REG_LAST + 1; reg <= DR_REG_STOP_GPR; reg++) {
            if (!instr_uses_reg(instr, reg) && !reg_overlap(reg, do_not_pick_a) &&
                !reg_overlap(reg, do_not_pick_b) && !reg_overlap(reg, do_not_pick_c) &&
                /* not pick  IBL_TARGET_REG if instr is a cti */
                (!instr_is_cti(instr) || reg != IBL_TARGET_REG))
                break;
        }
    }
    /* Only OP_stm could read all 6 of our scratch regs and also read or write
     * the PC or stolen reg (OP_smlal{b,t}{b,t} can read 4 GPR's but not a 4th),
     * and it's not allowed to have PC as a base reg (it's "unpredictable" at
     * least).  For stolen reg as base, we should split it up before calling here.
     */
    if (reg > DR_REG_STOP_GPR)
        reg = REG_NULL;
    if (scratch_slot != NULL)
        *scratch_slot = slot;
    return reg;
}

/* Should return NULL if it destroys "instr". */
instr_t *
mangle_rel_addr(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                instr_t *next_instr)
{
#ifdef AARCH64
    uint opc = instr_get_opcode(instr);
    opnd_t dst = instr_get_dst(instr, 0);
    opnd_t src = instr_get_src(instr, 0);
    app_pc tgt;
    ASSERT(opc == OP_adr || opc == OP_adrp || opc == OP_ldr || opc == OP_ldrsw);
    ASSERT(instr_has_rel_addr_reference(instr));
    instr_get_rel_addr_target(instr, &tgt);
    ASSERT(opnd_is_reg(dst));
    ASSERT(opnd_is_rel_addr(src));
    ASSERT(opnd_get_addr(src) == tgt);

    if (instr_uses_reg(instr, dr_reg_stolen)) {
        dst = opnd_create_reg(reg_resize_to_opsz(DR_REG_X0, opnd_get_size(dst)));
        PRE(ilist, next_instr,
            instr_create_save_to_tls(dcontext, DR_REG_X0, TLS_REG0_SLOT));
    }

    if ((opc == OP_ldr || opc == OP_ldrsw) && reg_is_gpr(opnd_get_reg(dst))) {
        reg_id_t xreg = reg_to_pointer_sized(opnd_get_reg(dst));
        insert_mov_immed_ptrsz(dcontext, (ptr_int_t)tgt, opnd_create_reg(xreg), ilist,
                               next_instr, NULL, NULL);
        PRE(ilist, next_instr,
            instr_create_1dst_1src(
                dcontext, opc, dst,
                opnd_create_base_disp(xreg, REG_NULL, 0, 0, opnd_get_size(src))));
    } else if (opc == OP_ldr) {
        PRE(ilist, instr, instr_create_save_to_tls(dcontext, DR_REG_X0, TLS_REG0_SLOT));
        insert_mov_immed_ptrsz(dcontext, (ptr_int_t)tgt, opnd_create_reg(DR_REG_X0),
                               ilist, next_instr, NULL, NULL);
        PRE(ilist, next_instr,
            XINST_CREATE_load(
                dcontext, dst,
                opnd_create_base_disp(DR_REG_X0, REG_NULL, 0, 0, opnd_get_size(dst))));
        PRE(ilist, next_instr,
            instr_create_restore_from_tls(dcontext, DR_REG_X0, TLS_REG0_SLOT));
    } else {
        /* OP_adr, OP_adrp */
        insert_mov_immed_ptrsz(dcontext, (ptr_int_t)tgt, dst, ilist, next_instr, NULL,
                               NULL);
    }

    if (instr_uses_reg(instr, dr_reg_stolen)) {
        PRE(ilist, next_instr,
            instr_create_save_to_tls(dcontext, DR_REG_X0, TLS_REG_STOLEN_SLOT));
        PRE(ilist, next_instr,
            instr_create_restore_from_tls(dcontext, DR_REG_X0, TLS_REG0_SLOT));
    }

    instrlist_remove(ilist, instr);
    instr_destroy(dcontext, instr);
    return NULL;
#else
    /* Compute the value of r15==pc for orig app instr */
    ptr_int_t r15 =
        (ptr_int_t)decode_cur_pc(instr_get_raw_bits(instr), instr_get_isa_mode(instr),
                                 instr_get_opcode(instr), instr);
    opnd_t mem_op;
    ushort slot;
    bool should_restore;
    reg_id_t reg = pick_scratch_reg(dcontext, instr, DR_REG_NULL, DR_REG_NULL,
                                    DR_REG_NULL, true, &slot, &should_restore);
    opnd_t new_op;
    dr_shift_type_t shift_type;
    uint shift_amt, disp;
    bool store = instr_writes_memory(instr);
    bool in_it = app_instr_is_in_it_block(dcontext, instr);
    instr_t *bound_start = INSTR_CREATE_label(dcontext);
    if (in_it) {
        /* split instr off from its IT block for easier mangling (we reinstate later) */
        next_instr = mangle_remove_from_it_block(dcontext, ilist, instr);
    }
    PRE(ilist, instr, bound_start);

    ASSERT(instr_has_rel_addr_reference(instr));
    /* Manual says "unpredicatable" if PC is base of ldm/stm */
    ASSERT(!instr_reads_gpr_list(instr) && !instr_writes_gpr_list(instr));
    ASSERT(reg != REG_NULL);
    if (store) {
        mem_op = instr_get_dst(instr, 0);
    } else {
        mem_op = instr_get_src(instr, 0);
    }
    ASSERT(opnd_is_base_disp(mem_op));
    ASSERT(opnd_get_base(mem_op) == DR_REG_PC);

    disp = opnd_get_disp(mem_op);
    /* For Thumb, there is a special-cased subtract from PC with a 12-bit immed that
     * has no analogue with a non-PC base.
     */
    if (instr_get_isa_mode(instr) == DR_ISA_ARM_THUMB &&
        TEST(DR_OPND_NEGATED, opnd_get_flags(mem_op)) && disp >= 256) {
        /* Apply the disp now */
        r15 -= disp;
        disp = 0;
    }

    insert_save_to_tls_if_necessary(dcontext, ilist, instr, reg, slot);
    insert_mov_immed_ptrsz(dcontext, r15, opnd_create_reg(reg), ilist, instr, NULL, NULL);

    shift_type = opnd_get_index_shift(mem_op, &shift_amt);
    new_op =
        opnd_create_base_disp_arm(reg, opnd_get_index(mem_op), shift_type, shift_amt,
                                  disp, opnd_get_flags(mem_op), opnd_get_size(mem_op));
    if (store) {
        instr_set_dst(instr, 0, new_op);
    } else {
        instr_set_src(instr, 0, new_op);
    }

    if (should_restore)
        PRE(ilist, next_instr, instr_create_restore_from_tls(dcontext, reg, slot));

    if (in_it) {
        /* XXX: we could mark our mangling as predicated in some cases,
         * like mangle_add_predicated_fall_through() does.
         */
        mangle_reinstate_it_blocks(dcontext, ilist, bound_start, next_instr);
    }
    return next_instr;
#endif
}

#ifndef AARCH64

/* mangle simple pc read, pc read in gpr_list is handled in mangle_gpr_list_read */
static void
mangle_pc_read(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
               instr_t *next_instr)
{
    ushort slot;
    bool should_restore;
    reg_id_t reg = pick_scratch_reg(dcontext, instr, DR_REG_NULL, DR_REG_NULL,
                                    DR_REG_NULL, true, &slot, &should_restore);
    ptr_int_t app_r15 =
        (ptr_int_t)decode_cur_pc(instr_get_raw_bits(instr), instr_get_isa_mode(instr),
                                 instr_get_opcode(instr), instr);
    int i;

    ASSERT(reg != REG_NULL);
    ASSERT(!instr_is_meta(instr) &&
           instr_reads_from_reg(instr, DR_REG_PC, DR_QUERY_INCLUDE_ALL));

    insert_save_to_tls_if_necessary(dcontext, ilist, instr, reg, slot);
    insert_mov_immed_ptrsz(dcontext, app_r15, opnd_create_reg(reg), ilist, instr, NULL,
                           NULL);
    for (i = 0; i < instr_num_srcs(instr); i++) {
        if (opnd_uses_reg(instr_get_src(instr, i), DR_REG_PC)) {
            /* A memref should have been mangled already in mangle_rel_addr */
            opnd_t orig = instr_get_src(instr, i);
            ASSERT(opnd_is_reg(orig));
            instr_set_src(
                instr, i,
                opnd_create_reg_ex(reg, opnd_get_size(orig), opnd_get_flags(orig)));
        }
    }
    if (should_restore)
        PRE(ilist, next_instr, instr_create_restore_from_tls(dcontext, reg, slot));
}

#endif /* !AARCH64 */

/* save tls_base from dr_reg_stolen to reg and load app value to dr_reg_stolen */
static void
restore_app_value_to_stolen_reg(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                                reg_id_t reg, ushort slot)
{
    insert_save_to_tls_if_necessary(dcontext, ilist, instr, reg, slot);
    /* This precise opcode (OP_orr) is checked for in instr_is_stolen_reg_move(). */
    PRE(ilist, instr,
        XINST_CREATE_move(dcontext, opnd_create_reg(reg),
                          opnd_create_reg(dr_reg_stolen)));
    /* We always read the app value to make sure we write back
     * the correct value in the case of predicated execution.
     */
    /* load the app value if the dr_reg_stolen might be read
     * or it is not always be written.
     */
    if (instr_reads_from_reg(instr, dr_reg_stolen, DR_QUERY_DEFAULT) ||
        !instr_writes_to_exact_reg(instr, dr_reg_stolen, DR_QUERY_DEFAULT)) {
        PRE(ilist, instr,
            instr_create_restore_from_tls(dcontext, dr_reg_stolen, TLS_REG_STOLEN_SLOT));
    } else {
        DOLOG(4, LOG_INTERP, {
            LOG(THREAD, LOG_INTERP, 4, "skip restore stolen reg app value for: ");
            instr_disassemble(dcontext, instr, THREAD);
            LOG(THREAD, LOG_INTERP, 4, "\n");
        });
    }
}

/* store app value from dr_reg_stolen to slot if writback is true and
 * restore tls_base from reg back to dr_reg_stolen
 */
static void
restore_tls_base_to_stolen_reg(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                               instr_t *next_instr, reg_id_t reg, ushort slot)
{
    /* store app val back if it might be written  */
    if (instr_writes_to_reg(instr, dr_reg_stolen, DR_QUERY_INCLUDE_COND_DSTS)) {
        PRE(ilist, next_instr,
            XINST_CREATE_store(dcontext,
                               opnd_create_base_disp(reg, REG_NULL, 0,
                                                     os_tls_offset(TLS_REG_STOLEN_SLOT),
                                                     OPSZ_PTR),
                               opnd_create_reg(dr_reg_stolen)));
    } else {
        DOLOG(4, LOG_INTERP, {
            LOG(THREAD, LOG_INTERP, 4, "skip save stolen reg app value for: ");
            instr_disassemble(dcontext, instr, THREAD);
            LOG(THREAD, LOG_INTERP, 4, "\n");
        });
    }
    /* restore stolen reg from spill reg */
    /* This precise opcode (OP_orr) is checked for in instr_is_stolen_reg_move(). */
    PRE(ilist, next_instr,
        XINST_CREATE_move(dcontext, opnd_create_reg(dr_reg_stolen),
                          opnd_create_reg(reg)));
}

/* Mangle simple dr_reg_stolen access.
 * dr_reg_stolen in gpr_list is handled in mangle_gpr_list_{read/write}.
 *
 * Because this routine switches the register that hold DR's TLS base,
 * it should be called after all other mangling routines that perform
 * reg save/restore.
 */
static void
mangle_stolen_reg(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                  instr_t *next_instr, bool instr_to_be_removed)
{
    ushort slot;
    bool should_restore;
    reg_id_t tmp;

    /* Our stolen reg model is to expose to the client.  We assume that any
     * meta instrs using it are using it as TLS.
     */
    ASSERT(!instr_is_meta(instr) && instr_uses_reg(instr, dr_reg_stolen));

#ifndef AARCH64 /* FIXME i#1569: recognise "move" on AArch64 */
    /* optimization, convert simple mov to ldr/str:
     * - "mov r0  -> r10"  ==> "str r0 -> [r10_slot]"
     * - "mov r10 -> r0"   ==> "ldr [r10_slot] -> r0"
     */
    if (instr_get_opcode(instr) == OP_mov && opnd_is_reg(instr_get_src(instr, 0))) {
        opnd_t opnd;
        ASSERT(instr_num_srcs(instr) == 1 && instr_num_dsts(instr) == 1);
        ASSERT(opnd_is_reg(instr_get_dst(instr, 0)));
        /* mov rx -> rx, do nothing */
        if (opnd_same(instr_get_src(instr, 0), instr_get_dst(instr, 0)))
            return;
        /* this optimization changes the original instr, so it is only applied
         * if instr_to_be_removed is false
         */
        if (!instr_to_be_removed) {
            opnd = opnd_create_tls_slot(os_tls_offset(TLS_REG_STOLEN_SLOT));
            if (opnd_get_reg(instr_get_src(instr, 0)) == dr_reg_stolen) {
                /* mov r10 -> rx, convert to a ldr */
                instr_set_opcode(instr, OP_ldr);
                instr_set_src(instr, 0, opnd);
                return;
            } else {
                ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == dr_reg_stolen);
                /* mov rx -> r10, convert to a str */
                instr_set_opcode(instr, OP_str);
                instr_set_dst(instr, 0, opnd);
                return;
            }
            ASSERT_NOT_REACHED();
        }
    }
#endif

    /* move stolen reg value into tmp reg for app instr execution */
    tmp = pick_scratch_reg(dcontext, instr, DR_REG_NULL, DR_REG_NULL, DR_REG_NULL, false,
                           &slot, &should_restore);
    ASSERT(tmp != REG_NULL);
    restore_app_value_to_stolen_reg(dcontext, ilist, instr, tmp, slot);

    /* -- app instr executes here -- */

    /* restore tls_base back to dr_reg_stolen */
    restore_tls_base_to_stolen_reg(dcontext, ilist, instr, next_instr, tmp, slot);
    /* restore tmp if necessary */
    if (should_restore)
        PRE(ilist, next_instr, instr_create_restore_from_tls(dcontext, tmp, slot));
}

/* replace thread register read instruction with a TLS load instr */
instr_t *
mangle_reads_thread_register(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                             instr_t *next_instr)
{
#ifdef AARCH64
    reg_id_t reg = opnd_get_reg(instr_get_dst(instr, 0));
    ASSERT(instr->opcode == OP_mrs);
    if (reg != dr_reg_stolen) {
        PRE(ilist, instr,
            instr_create_restore_from_tls(dcontext, reg,
                                          os_get_app_tls_base_offset(TLS_REG_LIB)));
    } else {
        PRE(ilist, instr, instr_create_save_to_tls(dcontext, DR_REG_X0, TLS_REG0_SLOT));
        PRE(ilist, instr,
            instr_create_restore_from_tls(dcontext, DR_REG_X0,
                                          os_get_app_tls_base_offset(TLS_REG_LIB)));
        PRE(ilist, instr,
            instr_create_save_to_tls(dcontext, DR_REG_X0, TLS_REG_STOLEN_SLOT));
        PRE(ilist, instr,
            instr_create_restore_from_tls(dcontext, DR_REG_X0, TLS_REG0_SLOT));
    }
    instrlist_remove(ilist, instr);
    instr_destroy(dcontext, instr);
    return next_instr;
#else
    opnd_t opnd;
    reg_id_t reg;
    bool in_it = app_instr_is_in_it_block(dcontext, instr);
    instr_t *bound_start = INSTR_CREATE_label(dcontext);
    if (in_it) {
        /* split instr off from its IT block for easier mangling (we reinstate later) */
        next_instr = mangle_remove_from_it_block(dcontext, ilist, instr);
    }
    PRE(ilist, instr, bound_start);
    ASSERT(!instr_is_meta(instr) && instr_reads_thread_register(instr));
    reg = opnd_get_reg(instr_get_dst(instr, 0));
    ASSERT(reg_is_gpr(reg) && opnd_get_size(instr_get_dst(instr, 0)) == OPSZ_PTR);
    /* convert mrc to load */
    opnd = opnd_create_sized_tls_slot(
        os_tls_offset(os_get_app_tls_base_offset(TLS_REG_LIB)), OPSZ_PTR);
    instr_remove_srcs(dcontext, instr, 1, instr_num_srcs(instr));
    instr_set_src(instr, 0, opnd);
    instr_set_opcode(instr, OP_ldr);
    ASSERT(reg != DR_REG_PC);
    /* special case: dst reg is dr_reg_stolen */
    if (reg == dr_reg_stolen) {
        instr_t *immed_nexti;
        /* we do not mangle r10 in [r10, disp], but need save r10 after execution,
         * so we cannot use mangle_stolen_reg.
         */
        insert_save_to_tls_if_necessary(dcontext, ilist, instr, SCRATCH_REG0,
                                        TLS_REG0_SLOT);
        PRE(ilist, instr,
            INSTR_CREATE_mov(dcontext, opnd_create_reg(SCRATCH_REG0),
                             opnd_create_reg(dr_reg_stolen)));

        /* -- "ldr r10, [r10, disp]" executes here -- */

        immed_nexti = instr_get_next(instr);
        restore_tls_base_to_stolen_reg(dcontext, ilist, instr, immed_nexti, SCRATCH_REG0,
                                       TLS_REG0_SLOT);
        PRE(ilist, immed_nexti,
            instr_create_restore_from_tls(dcontext, SCRATCH_REG0, TLS_REG0_SLOT));
    }
    if (in_it)
        mangle_reinstate_it_blocks(dcontext, ilist, bound_start, next_instr);
    return next_instr;
#endif
}

#ifdef AARCH64
instr_t *
mangle_writes_thread_register(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                              instr_t *next_instr)
{
    reg_id_t reg = opnd_get_reg(instr_get_src(instr, 0));
    ASSERT(instr->opcode == OP_msr);
    if (reg != dr_reg_stolen) {
        PRE(ilist, instr,
            instr_create_save_to_tls(dcontext, reg,
                                     os_get_app_tls_base_offset(TLS_REG_LIB)));
    } else {
        PRE(ilist, instr, instr_create_save_to_tls(dcontext, DR_REG_X0, TLS_REG0_SLOT));
        PRE(ilist, instr,
            instr_create_restore_from_tls(dcontext, DR_REG_X0, TLS_REG_STOLEN_SLOT));
        PRE(ilist, instr,
            instr_create_save_to_tls(dcontext, DR_REG_X0,
                                     os_get_app_tls_base_offset(TLS_REG_LIB)));
        PRE(ilist, instr,
            instr_create_restore_from_tls(dcontext, DR_REG_X0, TLS_REG0_SLOT));
    }
    instrlist_remove(ilist, instr);
    instr_destroy(dcontext, instr);
    return next_instr;
}
#endif

#ifndef AARCH64

static void
store_reg_to_memlist(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                     instr_t *next_instr,
                     reg_id_t base_reg,   /* reg holding memlist base */
                     ushort app_val_slot, /* slot holding app value */
                     reg_id_t tmp_reg,    /* scratch reg */
                     reg_id_t fix_reg,    /* reg to be fixed up */
                     uint fix_reg_idx)
{
    bool writeback = instr_num_dsts(instr) > 1;
    uint num_srcs = instr_num_srcs(instr);
    int offs;
    instr_t *store;

    switch (instr_get_opcode(instr)) {
    case OP_stmia:
        if (writeback)
            offs = -((num_srcs - 1 /*writeback*/ - fix_reg_idx) * sizeof(reg_t));
        else
            offs = fix_reg_idx * sizeof(reg_t);
        break;
    case OP_stmda:
        if (writeback)
            offs = (fix_reg_idx + 1) * sizeof(reg_t);
        else
            offs = -((num_srcs - fix_reg_idx - 1) * sizeof(reg_t));
        break;
    case OP_stmdb:
        if (writeback)
            offs = fix_reg_idx * sizeof(reg_t);
        else
            offs = -((num_srcs - fix_reg_idx) * sizeof(reg_t));
        break;
    case OP_stmib:
        if (writeback)
            offs = -((num_srcs - 1 /*writeback*/ - fix_reg_idx - 1) * sizeof(reg_t));
        else
            offs = (fix_reg_idx + 1) * sizeof(reg_t);
        break;
    default: offs = 0; ASSERT_NOT_REACHED();
    }

    /* load proper value into spill reg */
    if (fix_reg == DR_REG_PC) {
        ptr_int_t app_r15 =
            (ptr_int_t)decode_cur_pc(instr_get_raw_bits(instr), instr_get_isa_mode(instr),
                                     instr_get_opcode(instr), instr);
        insert_mov_immed_ptrsz(dcontext, app_r15, opnd_create_reg(tmp_reg), ilist,
                               next_instr, NULL, NULL);
    } else {
        /* load from app_val_slot */
        PRE(ilist, next_instr,
            instr_create_restore_from_tls(dcontext, tmp_reg, app_val_slot));
    }

    /* store to proper location */
    store = XINST_CREATE_store(
        dcontext, opnd_create_base_disp(base_reg, REG_NULL, 0, offs, OPSZ_PTR),
        opnd_create_reg(tmp_reg));
    /* we must use the same predicate to avoid crashing here when original didn't run */
    instr_set_predicate(store, instr_get_predicate(instr));
    /* app instr, not meta */
    instr_set_translation(store, instr_get_translation(instr));
    instrlist_preinsert(ilist, next_instr, store);
}

/* mangle dr_reg_stolen or pc read in a reglist store (i.e., stm).
 * Approach: fix up memory slot w/ app value after the store.
 */
static void
mangle_gpr_list_read(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                     instr_t *next_instr)
{
    reg_id_t spill_regs[2] = { DR_REG_R0, DR_REG_R1 };
    reg_id_t spill_slots[2] = { TLS_REG0_SLOT, TLS_REG1_SLOT };
    /* regs that need fix up in the memory slots */
    reg_id_t fix_regs[2] = { DR_REG_PC, dr_reg_stolen };
    bool reg_found[2] = { false, false };
    uint reg_pos[2]; /* position of those fix_regs in reglist  */
    uint i, j, num_srcs = instr_num_srcs(instr);
    bool writeback = instr_num_dsts(instr) > 1;
    bool stolen_reg_is_base = false;
    opnd_t memop = instr_get_dst(instr, 0);

    ASSERT(dr_reg_stolen != spill_regs[0] && dr_reg_stolen != spill_regs[1]);

    /* check base reg */
    /* base reg cannot be PC, so could only be dr_reg_stolen */
    if (opnd_uses_reg(memop, dr_reg_stolen)) {
        stolen_reg_is_base = true;
        restore_app_value_to_stolen_reg(dcontext, ilist, instr, spill_regs[0],
                                        spill_slots[0]);
        /* We do not need fix up memory slot for dr_reg_stolen since it holds
         * app value now, but we may need fix up the slot for spill_regs[0].
         */
        fix_regs[1] = spill_regs[0];
    }

    /* -- app instr executes here -- */

    /* restore dr_reg_stolen if used as base */
    if (stolen_reg_is_base) {
        ASSERT(fix_regs[1] == spill_regs[0]);
        ASSERT(opnd_uses_reg(memop, dr_reg_stolen));
        /* restore dr_reg_stolen from spill_regs[0] */
        restore_tls_base_to_stolen_reg(dcontext, ilist, instr,
                                       /* XXX: we must restore tls base right after instr
                                        * for other TLS usage, so we use instr_get_next
                                        * instead of next_instr.
                                        */
                                       instr_get_next(instr), spill_regs[0],
                                       spill_slots[0]);
        /* do not restore spill_reg[0] as we may use it as scratch reg later */
    }

    /* fix up memory slot w/ app value after the store */
    for (i = 0; i < (writeback ? (num_srcs - 1) : num_srcs); i++) {
        reg_id_t reg;
        ASSERT(opnd_is_reg(instr_get_src(instr, i)));
        reg = opnd_get_reg(instr_get_src(instr, i));
        for (j = 0; j < 2; j++) {
            if (reg == fix_regs[j]) {
                reg_found[j] = true;
                reg_pos[j] = i;
            }
        }
    }

    if (reg_found[0] || reg_found[1]) {
        ushort app_val_slot; /* slot holding app value */
        reg_id_t base_reg;
        reg_id_t scratch = spill_regs[1];
        if (stolen_reg_is_base) {
            /* dr_reg_stolen is used as the base in the app, but it is holding
             * TLS base, so we now put dr_reg_stolen app value into spill_regs[0]
             * to use it as the base instead.
             */
            ASSERT(fix_regs[1] == spill_regs[0]);
            app_val_slot = spill_slots[0];
            base_reg = spill_regs[0];
            PRE(ilist, next_instr,
                instr_create_restore_from_tls(dcontext, spill_regs[0],
                                              TLS_REG_STOLEN_SLOT));
        } else {
            ASSERT(fix_regs[1] == dr_reg_stolen);
            app_val_slot = TLS_REG_STOLEN_SLOT;
            base_reg = opnd_get_base(memop);
            if (opnd_uses_reg(memop, scratch)) {
                /* We know !stolen_reg_is_base so we can use r0 as scratch instead
                 * and not have any conflicts.  We keep same TLS slot.
                 */
                scratch = spill_regs[0];
            }
        }
        ASSERT(!opnd_uses_reg(memop, scratch));

        /* save spill reg */
        insert_save_to_tls_if_necessary(dcontext, ilist, next_instr, scratch,
                                        spill_slots[1]);

        /* fixup the slot in memlist */
        for (i = 0; i < 2; i++) {
            if (reg_found[i]) {
                store_reg_to_memlist(dcontext, ilist, instr, next_instr, base_reg,
                                     app_val_slot, scratch, fix_regs[i], reg_pos[i]);
            }
        }

        /* restore spill reg */
        PRE(ilist, next_instr,
            instr_create_restore_from_tls(dcontext, scratch, spill_slots[1]));
    }

    if (stolen_reg_is_base) {
        ASSERT(fix_regs[1] == spill_regs[0]);
        PRE(ilist, next_instr,
            instr_create_restore_from_tls(dcontext, spill_regs[0], spill_slots[0]));
    }
}

/* We normalize a ldm{ia,ib,da,db} instruction to a sequence of instructions:
 * 1. adjust base
 * 2. ldr r0 [base]  # optional split for getting a scratch reg
 * 3. ldmia
 * 4. adjust base
 * 5. ldr pc [base, disp]
 */
static void
normalize_ldm_instr(dcontext_t *dcontext, instr_t *instr, /* ldm */
                    instr_t **pre_ldm_adjust, instr_t **pre_ldm_ldr,
                    instr_t **post_ldm_adjust, instr_t **ldr_pc)
{
    int opcode = instr_get_opcode(instr);
    reg_id_t base = opnd_get_base(instr_get_src(instr, 0));
    bool writeback = instr_num_srcs(instr) > 1;
    bool write_pc = instr_writes_to_reg(instr, DR_REG_PC, DR_QUERY_INCLUDE_ALL);
    bool use_pop_pc = false;
    uint num_dsts = instr_num_dsts(instr);
    int memsz = sizeof(reg_t) * (writeback ? (num_dsts - 1) : num_dsts);
    int adjust_pre = 0, adjust_post = 0, ldr_pc_disp = 0;
    dr_pred_type_t pred = instr_get_predicate(instr);
    app_pc pc = get_app_instr_xl8(instr);

    /* FIXME i#1551: NYI on case like "ldm r10, {r10, pc}": if base reg
     * is clobbered, "ldr pc [base, disp]" will use wrong base value.
     * It seems the only solution is load the target value first and store
     * it into some TLS slot for later "ldr pc".
     */
    ASSERT_NOT_IMPLEMENTED(!(write_pc && !writeback &&
                             /* base reg is in the reglist */
                             instr_writes_to_reg(instr, base, DR_QUERY_INCLUDE_ALL)));

    ASSERT(pre_ldm_adjust != NULL && pre_ldm_ldr != NULL && post_ldm_adjust != NULL &&
           ldr_pc != NULL);
    *pre_ldm_adjust = NULL;
    *pre_ldm_ldr = NULL;
    *post_ldm_adjust = NULL;
    *ldr_pc = NULL;

    if (opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_PC) {
        /* special case like "pop pc" in T32.16, do nothing */
        ASSERT(write_pc && memsz == sizeof(reg_t));
        return;
    }

    /* using an example to better understand the code below:
     * - ldm{*} r0{!}, {r1-r4}    ==> ldmia  r0{!}, {r1-r4}
     * - ldm{*} r0{!}, {r1-r3,pc} ==> ldmia  r0{!}, {r1-r3,pc}
     */
    switch (opcode) {
    case OP_ldmia:
        /* ldmia r0,  {r1-r4}:     r0: X->X,      read [X, X+0x10)
         * ldmia r0!, {r1-r4}:     r0: X->X+0x10, read [X, X+0x10)
         * ldmia r0,  {r1-r3,pc}:  r0: X->X,      read [X, X+0xc), [X+0xc, X+0x10)
         * ldmia r0!, {r1-r3,pc}:  r0: X->X+0x10, read [X, X+0xc), [X+0xc, X+0x10)
         */
        adjust_pre = 0;
        if (write_pc) {
            /* we take pc out of reglist, so need post ldm adjust if w/ writeback */
            if (writeback) {
                /* use "pop pc" instead of "ldr pc" to avoid beyond TOS access */
                if (base == DR_REG_SP) {
                    use_pop_pc = true;
                    adjust_post = 0;
                    ldr_pc_disp = 0;
                } else {
                    adjust_post = sizeof(reg_t);
                    ldr_pc_disp = -sizeof(reg_t);
                }
            } else {
                adjust_post = 0;
                ldr_pc_disp = memsz - sizeof(reg_t);
            }
        } else {
            adjust_post = 0;
        }
        break;
    case OP_ldmda:
        /* ldmda r0,  {r1-r4}:     r0: X->X,      read [X-0xc, X+0x4)
         * ldmda r0!, {r1-r4}:     r0: X->X-0x10, read [X-0xc, X+0x4)
         * ldmda r0,  {r1-r3,pc}:  r0: X->X,      read [X-0xc, X), [X, X+0x4)
         * ldmda r0!, {r1-r3,pc}:  r0: X->X-0x10, read [X-0xc, X), [X, X+0x4)
         */
        adjust_pre = -memsz + sizeof(reg_t);
        if (write_pc) {
            if (writeback) {
                adjust_post = -memsz;
                ldr_pc_disp = memsz + sizeof(reg_t);
            } else {
                /* XXX: optimize, add writeback to skip post ldm adjust */
                adjust_post = -adjust_pre;
                ldr_pc_disp = 0;
            }
        } else {
            if (writeback) {
                adjust_post = -memsz - sizeof(reg_t);
            } else {
                adjust_post = -adjust_pre;
            }
        }
        break;
    case OP_ldmdb:
        /* ldmdb r0,  {r1-r4}:     r0: X->X,      read [X-0x10, X)
         * ldmdb r0!, {r1-r4}:     r0: X->X-0x10, read [X-0x10, X)
         * ldmdb r0,  {r1-r3,pc}:  r0: X->X,      read [X-0x10, X-0x4), [X-0x4, X)
         * ldmdb r0!, {r1-r3,pc}:  r0: X->X-0x10, read [X-0x10, X-0x4), [X-0x4, X)
         */
        adjust_pre = -memsz;
        if (write_pc) {
            if (writeback) {
                adjust_post = -(memsz - sizeof(reg_t));
                ldr_pc_disp = memsz - sizeof(reg_t);
            } else {
                adjust_post = -adjust_pre;
                ldr_pc_disp = -sizeof(reg_t);
            }
        } else {
            if (writeback) {
                /* XXX: optimize, remove writeback to avoid post ldm adjust */
                adjust_post = adjust_pre;
            } else {
                /* XXX: optimize, add writeback to avoid post ldm adjust */
                adjust_post = -adjust_pre;
            }
        }
        break;
    case OP_ldmib:
        /* ldmib r0,  {r1-r4}:     r0: X->X,      read [X+4, X+0x14)
         * ldmib r0!, {r1-r4}:     r0: X->X+0x10, read [X+4, X+0x14)
         * ldmib r0,  {r1-r3,pc}:  r0: X->X,      read [X+4, X+0x10), [X+0x10, X+0x14)
         * ldmib r0!, {r1-r3,pc}:  r0: X->X+0x10, read [X+4, X+0x10), [X+0x10, X+0x14)
         */
        adjust_pre = sizeof(reg_t);
        if (write_pc) {
            if (writeback) {
                adjust_post = 0;
                ldr_pc_disp = 0;
            } else {
                adjust_post = -adjust_pre;
                ldr_pc_disp = memsz;
            }
        } else {
            if (writeback)
                adjust_post = -sizeof(reg_t);
            else
                adjust_post = -adjust_pre;
        }
        break;
    default: ASSERT_NOT_REACHED();
    }

    if (instr_uses_reg(instr, dr_reg_stolen) &&
        pick_scratch_reg(dcontext, instr, DR_REG_NULL, DR_REG_NULL, DR_REG_NULL, false,
                         NULL, NULL) == REG_NULL) {
        /* We need split the ldm.
         * We need a scratch reg from r0-r3, so by splitting the bottom reg we're
         * guaranteed to get one.  And since cti uses r2 it works out there.
         */
        adjust_pre += sizeof(reg_t);
        /* adjust base back if base won't be over-written, e.g.,:
         * ldm (%r10)[16byte] -> %r0 %r1 %r2 %r3
         */
        if (!instr_writes_to_reg(instr, base, DR_QUERY_INCLUDE_ALL))
            adjust_post -= sizeof(reg_t);
        /* pre_ldm_adjust makes sure that the base reg points to the start address of
         * the ldmia memory, so we know the slot to be load is at [base, -4].
         */
        *pre_ldm_ldr = XINST_CREATE_load(dcontext, instr_get_dst(instr, 0),
                                         OPND_CREATE_MEMPTR(base, -sizeof(reg_t)));
        /* We remove the reg from reglist later after removing pc from reglist,
         * so it won't mess up the index when removing pc.
         */
        instr_set_predicate(*pre_ldm_ldr, pred);
        instr_set_translation(*pre_ldm_ldr, pc);
    }

    if (adjust_pre != 0) {
        *pre_ldm_adjust = adjust_pre > 0
            ? XINST_CREATE_add(dcontext, opnd_create_reg(base),
                               OPND_CREATE_INT(adjust_pre))
            : XINST_CREATE_sub(dcontext, opnd_create_reg(base),
                               OPND_CREATE_INT(-adjust_pre));
        instr_set_predicate(*pre_ldm_adjust, pred);
        instr_set_translation(*pre_ldm_adjust, pc);
    }

    if (write_pc) {
        instr_remove_dsts(dcontext, instr, writeback ? num_dsts - 2 : num_dsts - 1,
                          writeback ? num_dsts - 1 : num_dsts);
    }
    if (*pre_ldm_ldr != NULL)
        instr_remove_dsts(dcontext, instr, 0, 1);

    /* check how many registers left in the reglist */
    ASSERT(instr_num_dsts(instr) != (writeback ? 1 : 0));
    if (instr_num_dsts(instr) == (writeback ? 2 : 1)) {
        /* only one reg is left in the reglist, convert it to ldr */
        instr_set_opcode(instr, OP_ldr);
        instr_set_src(instr, 0, OPND_CREATE_MEMPTR(base, 0));
        if (writeback) {
            adjust_post += sizeof(reg_t);
            instr_remove_srcs(dcontext, instr, 1, 2);
            instr_remove_dsts(dcontext, instr, 1, 2);
        }
    } else {
        instr_set_opcode(instr, OP_ldmia);
        instr_set_src(instr, 0, OPND_CREATE_MEMLIST(base));
    }

    /* post ldm base register adjustment */
    if (!writeback && instr_writes_to_reg(instr, base, DR_QUERY_INCLUDE_ALL)) {
        /* if the base reg is in the reglist, we do not need to post adjust */
        adjust_post = 0;
    }
    if (adjust_post != 0) {
        *post_ldm_adjust = adjust_post > 0
            ? XINST_CREATE_add(dcontext, opnd_create_reg(base),
                               OPND_CREATE_INT(adjust_post))
            : XINST_CREATE_sub(dcontext, opnd_create_reg(base),
                               OPND_CREATE_INT(-adjust_post));
        instr_set_predicate(*post_ldm_adjust, pred);
        instr_set_translation(*post_ldm_adjust, pc);
    }

    /* post ldm load-pc */
    if (write_pc) {
        if (use_pop_pc) {
            ASSERT(ldr_pc_disp == 0 && base == DR_REG_SP && writeback);
            /* we use pop_list to generate A32.T16 (2-byte) code in Thumb mode */
            *ldr_pc = INSTR_CREATE_pop_list(dcontext, 1, opnd_create_reg(DR_REG_PC));
        } else {
            *ldr_pc = XINST_CREATE_load(dcontext, opnd_create_reg(DR_REG_PC),
                                        OPND_CREATE_MEMPTR(base, ldr_pc_disp));
        }
        instr_set_predicate(*ldr_pc, pred);
        instr_set_translation(*ldr_pc, pc);
        if (TEST(INSTR_CLOBBER_RETADDR, instr->flags))
            (*ldr_pc)->flags |= INSTR_CLOBBER_RETADDR;
    }
}

/* Mangling reglist write is complex: ldm{ia,ib,da,db} w/ and w/o writeback.
 * One possible solution is to split the ldm into multiple ldm instructions.
 * However it has several challenges, for examples:
 * - we need additional base reg adjust instr for ldm w/o writeback
 *   as ldm does not have disp for the memlist,
 * - we need different execution order of split-ldms for ldmia and ldmdb,
 * - ldmib/ldmda add additional complexity,
 * - we still need a "ldr pc" if it writes to pc
 * - etc.
 *
 * Another solution is to convert them into a squence of ldr with base reg
 * adjustments, which may cause large runtime overhead.
 *
 * Our approach is to convert any gpr_list write instrucition into five parts:
 * 1. base reg adjustment
 * 2. ldr r0 [base]   # optional split for getting a scratch reg
 * 3. ldmia base, {reglist}
 * 4. base reg adjustment
 * 5. ldr pc, [base, offset]
 * and mangle each separately.
 */
static instr_t *
mangle_gpr_list_write(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                      instr_t *next_instr)
{
    instr_t *pre_ldm_adjust, *pre_ldm_ldr, *post_ldm_adjust, *ldr_pc;

    ASSERT(!instr_is_meta(instr) && instr_writes_gpr_list(instr));

    /* convert ldm{*} instr to a sequence of instructions */
    normalize_ldm_instr(dcontext, instr, &pre_ldm_adjust, &pre_ldm_ldr, &post_ldm_adjust,
                        &ldr_pc);

    /* pc cannot be used as the base in ldm, so now we only care dr_reg_stolen */
    if (pre_ldm_adjust != NULL) {
        instrlist_preinsert(ilist, instr, pre_ldm_adjust); /* non-meta */
        if (instr_uses_reg(pre_ldm_adjust, dr_reg_stolen)) {
            mangle_stolen_reg(dcontext, ilist, pre_ldm_adjust,
                              /* dr_reg_stolen must be restored right after */
                              instr_get_next(pre_ldm_adjust), false);
        }
    }
    if (pre_ldm_ldr != NULL) {
        /* special case: ldm r0, {r0-rx}, separate ldr r0, [r0] clobbers base r0 */
        if (opnd_get_reg(instr_get_dst(pre_ldm_ldr, 0)) == SCRATCH_REG0 &&
            opnd_get_base(instr_get_src(pre_ldm_ldr, 0)) == SCRATCH_REG0) {
            instr_t *mov;
            /* save the r1 for possible context restore on signal */
            insert_save_to_tls_if_necessary(dcontext, ilist, instr, SCRATCH_REG1,
                                            TLS_REG1_SLOT);
            /* mov r0 => r1, */
            mov = XINST_CREATE_move(dcontext, opnd_create_reg(SCRATCH_REG1),
                                    opnd_create_reg(SCRATCH_REG0));
            instr_set_predicate(mov, instr_get_predicate(instr));
            PRE(ilist, instr, mov);
            /* We will only come to here iff instr is "ldm r0, {r0-rx}",
             * otherwise we will be able to pick a scratch reg without split.
             * Thus the first dst reg must be r1 after split and the base is r0.
             * Now we change "ldm r0, {r1-rx}" to "ldm r1, {r1-rx}".
             */
            ASSERT(opnd_get_reg(instr_get_dst(instr, 0)) == SCRATCH_REG1 &&
                   opnd_get_base(instr_get_src(instr, 0)) == SCRATCH_REG0);
            instr_set_src(instr, 0, OPND_CREATE_MEMLIST(SCRATCH_REG1));
        }

        instrlist_preinsert(ilist, instr, pre_ldm_ldr); /* non-meta */

        if (instr_uses_reg(pre_ldm_ldr, dr_reg_stolen)) {
            mangle_stolen_reg(dcontext, ilist, pre_ldm_ldr,
                              /* dr_reg_stolen must be restored right after */
                              instr_get_next(pre_ldm_ldr), false);
        }
    }

    if (instr_uses_reg(instr, dr_reg_stolen)) {
        /* dr_reg_stolen must be restored right after instr */
        mangle_stolen_reg(dcontext, ilist, instr, instr_get_next(instr), false);
    }

    if (post_ldm_adjust != NULL) {
        instrlist_preinsert(ilist, next_instr, post_ldm_adjust);
        if (instr_uses_reg(post_ldm_adjust, dr_reg_stolen)) {
            mangle_stolen_reg(dcontext, ilist, post_ldm_adjust,
                              /* dr_reg_stolen must be restored right after */
                              instr_get_next(post_ldm_adjust), false);
        }
    }

    if (ldr_pc != NULL) {
        /* we leave ldr_pc to mangle_indirect_jump */
        instrlist_preinsert(ilist, next_instr, ldr_pc);
        next_instr = ldr_pc;
    }
    return next_instr;
}

#endif /* !AARCH64 */

#ifdef AARCH64
/* We mangle a conditional branch that uses the stolen register like this:
 *
 *     cbz   x28, target     # x28 is stolen register
 * =>
 *     str   x0, [x28]       # spill x0
 *     ldr   x0, [x28, #32]  # x28 in memory loaded to x0
 *     cbnz  x0, fall
 *     ldr   x0, [x28]       # restore x0 (original branch taken)
 *     b     target
 * fall:
 *     ldr   x0, [x28]       # restore x0 (original branch not taken)
 *
 * The CBNZ will need special handling when we decode from the cache for
 * traces (i#1668).
 */
static void
mangle_cbr_stolen_reg(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                      instr_t *next_instr)
{
    instr_t *fall = INSTR_CREATE_label(dcontext);
    int opcode = instr_get_opcode(instr);
    reg_id_t reg = DR_REG_X0;
    ushort slot = TLS_REG0_SLOT;
    opnd_t opnd;

    PRE(ilist, instr, instr_create_save_to_tls(dcontext, reg, slot));
    PRE(ilist, instr, instr_create_restore_from_tls(dcontext, reg, TLS_REG_STOLEN_SLOT));
    switch (opcode) {
    case OP_cbnz:
    case OP_cbz:
        opnd = instr_get_src(instr, 1);
        opnd = opnd_create_reg(reg_resize_to_opsz(reg, opnd_get_size(opnd)));
        PRE(ilist, instr,
            instr_create_0dst_2src(dcontext, (opcode == OP_cbz ? OP_cbnz : OP_cbz),
                                   opnd_create_instr(fall), opnd));
        break;
    case OP_tbnz:
    case OP_tbz:
        PRE(ilist, instr,
            instr_create_0dst_3src(dcontext, (opcode == OP_tbz ? OP_tbnz : OP_tbz),
                                   opnd_create_instr(fall), opnd_create_reg(reg),
                                   instr_get_src(instr, 2)));
        break;
    default: ASSERT_NOT_REACHED();
    }
    PRE(ilist, instr, instr_create_restore_from_tls(dcontext, reg, slot));

    /* Replace original instruction with unconditional branch. */
    opnd = instr_get_src(instr, 0);
    instr_reset(dcontext, instr);
    instr_set_opcode(instr, OP_b);
    instr_set_num_opnds(dcontext, instr, 0, 1);
    instr_set_src(instr, 0, opnd);
    instr_set_translation(instr, instrlist_get_translation_target(ilist));

    PRE(ilist, next_instr, fall);
    PRE(ilist, next_instr, instr_create_restore_from_tls(dcontext, reg, slot));
}
#endif /* AARCH64 */

/* On ARM, we need mangle app instr accessing registers pc and dr_reg_stolen.
 * We use this centralized mangling routine here to handle complex issues with
 * more efficient mangling code.
 */
instr_t *
mangle_special_registers(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                         instr_t *next_instr)
{
#ifdef AARCH64
    if (!instr_uses_reg(instr, dr_reg_stolen))
        return next_instr;
    if (instr_is_cbr(instr))
        mangle_cbr_stolen_reg(dcontext, ilist, instr, instr_get_next(instr));
    else if (!instr_is_mbr(instr))
        mangle_stolen_reg(dcontext, ilist, instr, instr_get_next(instr), false);
    return next_instr;
#else
    bool finished = false;
    bool in_it =
        instr_get_isa_mode(instr) == DR_ISA_ARM_THUMB && instr_is_predicated(instr);
    instr_t *bound_start = NULL, *bound_end = next_instr;
    if (in_it) {
        /* split instr off from its IT block for easier mangling (we reinstate later) */
        next_instr = mangle_remove_from_it_block(dcontext, ilist, instr);
        /* We do NOT want the next_instr from mangle_gpr_list_write(), which can
         * point at the split-off OP_ldr of pc: but we need to go past that.
         */
        bound_end = next_instr;
        bound_start = INSTR_CREATE_label(dcontext);
        PRE(ilist, instr, bound_start);
    }

    /* FIXME i#1551: for indirect branch mangling, we first mangle the instr here
     * for possible pc read and dr_reg_stolen read/write,
     * and leave pc write mangling later in mangle_indirect_jump, which is
     * error-prone and inefficient.
     * We should split the mangling and only mangle non-ind-branch instructions
     * here and leave mbr instruction mangling to mangle_indirect_jump.
     */
    /* special handling reglist read */
    if (instr_reads_gpr_list(instr)) {
        mangle_gpr_list_read(dcontext, ilist, instr, next_instr);
        finished = true;
    }

    /* special handling reglist write */
    if (!finished && instr_writes_gpr_list(instr)) {
        next_instr = mangle_gpr_list_write(dcontext, ilist, instr, next_instr);
        finished = true;
    }

    if (!finished && instr_reads_from_reg(instr, DR_REG_PC, DR_QUERY_INCLUDE_ALL))
        mangle_pc_read(dcontext, ilist, instr, next_instr);

    /* mangle_stolen_reg must happen after mangle_pc_read to avoid reg conflict */
    if (!finished && instr_uses_reg(instr, dr_reg_stolen) && !instr_is_mbr(instr))
        mangle_stolen_reg(dcontext, ilist, instr, instr_get_next(instr), false);

    if (in_it) {
        mangle_reinstate_it_blocks(dcontext, ilist, bound_start, bound_end);
    }
    return next_instr;
#endif
}

void
float_pc_update(dcontext_t *dcontext)
{
    /* FIXME i#1551, i#1569: NYI on ARM */
    ASSERT_NOT_REACHED();
}

#ifdef AARCH64
instr_t *
mangle_icache_op(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                 instr_t *next_instr, app_pc pc)
{
    int opc = instr_get_opcode(instr);
    if (opc == OP_ic_ivau) {
        reg_id_t xt = opnd_get_base(instr_get_src(instr, 0));
        /* ic ivau, xT is replaced with: */
        PRE(ilist, instr, /* stp x0, x30, [x28] */
            INSTR_CREATE_stp(
                dcontext,
                opnd_create_base_disp(dr_reg_stolen, DR_REG_NULL, 0, 0, OPSZ_16),
                opnd_create_reg(DR_REG_X0), opnd_create_reg(DR_REG_X30)));
        insert_mov_immed_arch(dcontext, NULL, NULL, (ptr_int_t)pc,
                              opnd_create_reg(DR_REG_X30), ilist, instr, NULL, NULL);
        if (xt == dr_reg_stolen) {
            PRE(ilist, instr, /* ldr x0, [x28, #32] */
                instr_create_restore_from_tls(dcontext, DR_REG_X0, TLS_REG_STOLEN_SLOT));
        }
        PRE(ilist, instr, /* stp xT, x30, [x28, #16] */
            INSTR_CREATE_stp(
                dcontext,
                opnd_create_base_disp(dr_reg_stolen, DR_REG_NULL, 0, 16, OPSZ_16),
                opnd_create_reg(xt == dr_reg_stolen ? DR_REG_X0 : xt),
                opnd_create_reg(DR_REG_X30)));
#    ifdef DR_HOST_NOT_TARGET
        /* We built all our asm code for the host, but here we need it for the target.
         * We have to ifdef it out to separate.  Xref i#1684.
         */
        ASSERT_NOT_REACHED();
#    else
        insert_mov_immed_arch(dcontext, NULL, NULL, (ptr_int_t)icache_op_ic_ivau_asm,
                              opnd_create_reg(DR_REG_X30), ilist, instr, NULL, NULL);
#    endif
        PRE(ilist, instr, /* mov x0, x28 */
            XINST_CREATE_move(dcontext, opnd_create_reg(DR_REG_X0),
                              opnd_create_reg(dr_reg_stolen)));
        PRE(ilist, instr, /* blr x30 */
            INSTR_CREATE_blr(dcontext, opnd_create_reg(DR_REG_X30)));
        PRE(ilist, instr, /* ldp x0, x30, [x28] */
            INSTR_CREATE_ldp(
                dcontext, opnd_create_reg(DR_REG_X0), opnd_create_reg(DR_REG_X30),
                opnd_create_base_disp(dr_reg_stolen, DR_REG_NULL, 0, 0, OPSZ_16)));
        /* Remove original instruction. */
        instrlist_remove(ilist, instr);
        instr_destroy(dcontext, instr);
    } else if (opc == OP_isb) {
        instr_t *label = INSTR_CREATE_label(dcontext);
        instr = next_instr;
        /* isb is followed by: */
        PRE(ilist, instr, /* str x0, [x28] */
            instr_create_save_to_tls(dcontext, DR_REG_X0, TLS_REG0_SLOT));
        insert_mov_immed_arch(dcontext, NULL, NULL, (ptr_int_t)&icache_op_struct.flag,
                              opnd_create_reg(DR_REG_X0), ilist, instr, NULL, NULL);
        PRE(ilist, instr, /* ldr w0, [x0] */
            XINST_CREATE_load(
                dcontext, opnd_create_reg(DR_REG_W0),
                opnd_create_base_disp(DR_REG_X0, DR_REG_NULL, 0, 0, OPSZ_4)));
        PRE(ilist, instr, /* cbz ... */
            INSTR_CREATE_cbz(dcontext, opnd_create_instr(label),
                             opnd_create_reg(DR_REG_W0)));
        PRE(ilist, instr, /* stp x1, x2, [x28, #8] */
            INSTR_CREATE_stp(
                dcontext,
                opnd_create_base_disp(dr_reg_stolen, DR_REG_NULL, 0, 8, OPSZ_16),
                opnd_create_reg(DR_REG_X1), opnd_create_reg(DR_REG_X2)));
#    ifdef DR_HOST_NOT_TARGET
        /* We built all our asm code for the host, but here we need it for the target.
         * We have to ifdef it out to separate.  Xref i#1684.
         */
        ASSERT_NOT_REACHED();
#    else
        insert_mov_immed_arch(dcontext, NULL, NULL, (ptr_int_t)icache_op_isb_asm,
                              opnd_create_reg(DR_REG_X2), ilist, instr, NULL, NULL);
#    endif
        insert_mov_immed_arch(dcontext, NULL, NULL, (ptr_int_t)pc,
                              opnd_create_reg(DR_REG_X1), ilist, instr, NULL, NULL);
        PRE(ilist, instr, /* mov x0, x28 */
            XINST_CREATE_move(dcontext, opnd_create_reg(DR_REG_X0),
                              opnd_create_reg(dr_reg_stolen)));
        PRE(ilist, instr, /* br x2 */
            INSTR_CREATE_br(dcontext, opnd_create_reg(DR_REG_X2)));
        PRE(ilist, instr, label);
        PRE(ilist, instr, /* ldr x0, [x28] */
            instr_create_restore_from_tls(dcontext, DR_REG_X0, TLS_REG0_SLOT));
        /* Leave original instruction. */
    } else
        ASSERT_NOT_REACHED();
    return next_instr;
}
#endif

/***************************************************************************
 * Exclusive load/store mangling.
 * See the design doc at https://dynamorio.org/page_ldstex.html
 * for background information.
 */

static instr_t *
create_ld_from_ldex(dcontext_t *dcontext, instr_t *ldex)
{
    opnd_t regop = instr_get_dst(ldex, 0);
    opnd_t memop = instr_get_src(ldex, 0);
    /* TODO i#1698: Preserve ARM predication and add tests. */
    switch (instr_get_opcode(ldex)) {
#ifdef AARCH64
    case OP_ldaxp:
        /* There is no OP_ldap so we engage the monitor.  Should we add CLREX? */
        return INSTR_CREATE_ldaxp(dcontext, regop, instr_get_dst(ldex, 1), memop);
    case OP_ldxp: return INSTR_CREATE_ldp(dcontext, regop, instr_get_dst(ldex, 1), memop);
    case OP_ldaxr: return INSTR_CREATE_ldar(dcontext, regop, memop);
    case OP_ldxr: return INSTR_CREATE_ldr(dcontext, regop, memop);
    case OP_ldaxrb: return INSTR_CREATE_ldarb(dcontext, regop, memop);
    case OP_ldxrb: return INSTR_CREATE_ldrb(dcontext, regop, memop);
    case OP_ldaxrh: return INSTR_CREATE_ldarh(dcontext, regop, memop);
    case OP_ldxrh: return INSTR_CREATE_ldrh(dcontext, regop, memop);
#else
    case OP_ldaexd:
        /* There is no OP_ldad so we engage the monitor.  Should we add CLREX? */
        return INSTR_CREATE_ldaexd(dcontext, regop, instr_get_dst(ldex, 1), memop);
    case OP_ldrexd:
        return INSTR_CREATE_ldrd(dcontext, regop, instr_get_dst(ldex, 1), memop);
    case OP_ldaex: return INSTR_CREATE_lda(dcontext, regop, memop);
    case OP_ldrex: return INSTR_CREATE_ldr(dcontext, regop, memop);
    case OP_ldaexb: return INSTR_CREATE_ldab(dcontext, regop, memop);
    case OP_ldrexb: return INSTR_CREATE_ldrb(dcontext, regop, memop);
    case OP_ldaexh: return INSTR_CREATE_ldah(dcontext, regop, memop);
    case OP_ldrexh: return INSTR_CREATE_ldrh(dcontext, regop, memop);
#endif
    default: ASSERT_NOT_REACHED();
    }
    return NULL;
}

static instr_t *
create_ldax_from_stex(dcontext_t *dcontext, instr_t *strex, reg_id_t *dest_reg INOUT,
                      /* For a pair, we need a caller-set-up scratch reg for the 2nd. */
                      reg_id_t dest_reg2,
                      /* Whether to merge a pair of 4-bytes into one 8-byte. */
                      bool merge_pair)
{
    /* It is challenging to know whether to use an acquire or regular load opcode
     * because we do not know what the original load opcode was, especially for
     * situations like dr_prepopulate_cache() where we have no dynamic information
     * and for cases of two different load opcodes sharing the same store..
     * Our solution is to always use an acquire load, which won't affect correctness,
     * on processors where it is supported.
     */
#ifdef ARM
    /* Ideally we'd read ID_ISAR2 but we can't at EL0.  We assume no v7 processor
     * has acquire support.  We could record whether we've ever seen any acquire
     * opcodes and flush if we see one.
     */
    bool acquire_supported = proc_get_architecture() >= 8;
#endif
    opnd_t memop = instr_get_dst(strex, 0);
    /* We can't assume the stored reg equals the prior loaded-into reg, so we have to
     * write to a scratch or dead register.  We assume the dest reg is dead (but xref
     * i#400 noted below on faults): having the value or base register equal it is
     * disallowed (at least, it's unpredictable behavior: we assume that us
     * clobbering it falls under possible unpredictable results, though that's
     * probably not true if we fault the base).
     * For a pair, we rely on comparing this first value first, as the compare
     * result writes the same store result register.
     */
    opnd_size_t opsz = opnd_get_size(instr_get_src(strex, 0));
    /* The store dest reg could equal a load dest reg, in which case the caller must
     * pass us a scratch reg.
     */
    if (*dest_reg == DR_REG_NULL)
        *dest_reg = reg_resize_to_opsz(opnd_get_reg(instr_get_dst(strex, 1)), opsz);
    else
        *dest_reg = reg_resize_to_opsz(*dest_reg, opsz);
    opnd_t regop = opnd_create_reg(*dest_reg);
    /* TODO i#1698: Preserve ARM predication and add tests. */
    switch (instr_get_opcode(strex)) {
#ifdef AARCH64
    case OP_stlxp:
    case OP_stxp:
        /* We treat A64 pair-4byte as single-8byte to handle ldxr;stxp. */
        if (merge_pair) {
            ASSERT(opsz == OPSZ_4);
            *dest_reg = reg_resize_to_opsz(*dest_reg, OPSZ_8);
            return INSTR_CREATE_ldaxr(dcontext, opnd_create_reg(*dest_reg), memop);
        } else {
            return INSTR_CREATE_ldaxp(
                dcontext, regop, opnd_create_reg(reg_resize_to_opsz(dest_reg2, opsz)),
                memop);
        }
    case OP_stlxr:
    case OP_stxr: return INSTR_CREATE_ldaxr(dcontext, regop, memop);
    case OP_stlxrb:
    case OP_stxrb: return INSTR_CREATE_ldaxrb(dcontext, regop, memop);
    case OP_stlxrh:
    case OP_stxrh: return INSTR_CREATE_ldaxrh(dcontext, regop, memop);
#else
    case OP_stlexd:
    case OP_strexd:
        /* TODO i#1698: ARM register pairs must be <even,even+1> which we are
         * certainly not guaranteeing today.  This will take some effort to
         * arrange scratch registers wrt the app's strex usage.  For now we bail
         * in the caller.
         */
        return acquire_supported
            ? INSTR_CREATE_ldaexd(dcontext, regop,
                                  opnd_create_reg(reg_resize_to_opsz(dest_reg2, opsz)),
                                  memop)
            : INSTR_CREATE_ldrexd(dcontext, regop,
                                  opnd_create_reg(reg_resize_to_opsz(dest_reg2, opsz)),
                                  memop);
    case OP_stlex:
    case OP_strex:
        return acquire_supported ? INSTR_CREATE_ldaex(dcontext, regop, memop)
                                 : INSTR_CREATE_ldrex(dcontext, regop, memop);
    case OP_stlexb:
    case OP_strexb:
        return acquire_supported ? INSTR_CREATE_ldaexb(dcontext, regop, memop)
                                 : INSTR_CREATE_ldrexb(dcontext, regop, memop);
    case OP_stlexh:
    case OP_strexh:
        return acquire_supported ? INSTR_CREATE_ldaexh(dcontext, regop, memop)
                                 : INSTR_CREATE_ldrexh(dcontext, regop, memop);
#endif
    default: ASSERT_NOT_REACHED();
    }
    return NULL;
}

bool
instr_is_ldstex_mangling(dcontext_t *dcontext, instr_t *inst)
{
    if (!instr_is_our_mangling(inst))
        return false;
    /* XXX: Should we update instr_is_DR_reg_spill_or_restore() to handle OP_stp?
     * But which reg would it return?  We would also need to update for a
     * swapped stolen reg.  For now doing a custom check.
     */
    opnd_t memop = opnd_create_null();
    if (instr_get_opcode(inst) == OP_str ||
        instr_get_opcode(inst) == IF_ARM_ELSE(OP_strd, OP_stp))
        memop = instr_get_dst(inst, 0);
    else if (instr_get_opcode(inst) == OP_ldr)
        memop = instr_get_src(inst, 0);
    if (opnd_is_base_disp(memop) && opnd_get_index(memop) == DR_REG_NULL) {
        /* We can't check that the base is the stolen reg b/c we may have swapped. */
        uint offs = opnd_get_disp(memop);
        if (offs >= TLS_LDSTEX_ADDR_SLOT &&
            offs <= IF_ARM_ELSE(TLS_LDSTEX_FLAGS_SLOT, TLS_LDSTEX_SIZE_SLOT))
            return true;
    }
    /* Look for loading the stolen reg into a swapped reg, which is unique to
     * ldstex mangling.
     */
    if (instr_is_tls_restore(inst, DR_REG_NULL, TLS_REG_STOLEN_SLOT))
        return true;
    ptr_int_t val;
    if (instr_get_opcode(inst) == OP_sub || instr_get_opcode(inst) == OP_subs ||
        IF_ARM(instr_get_opcode(inst) == OP_mrs || instr_get_opcode(inst) == OP_msr ||)
        /* Size should be between 1 (OP_stxrb) and 16 (OP_stxp). */
        (instr_is_mov_constant(inst, &val) && val >= 1 && val <= 16) ||
        instr_get_opcode(inst) == OP_cbnz || instr_get_opcode(inst) == OP_b ||
        instr_get_opcode(inst) == OP_clrex || instr_is_exclusive_load(inst) ||
        instr_is_exclusive_store(inst))
        return true;
    return false;
}

static bool
is_cbnz_available(dcontext_t *dcontext, reg_id_t reg_strex_dst)
{
#ifdef ARM
    return dr_get_isa_mode(dcontext) == DR_ISA_ARM_THUMB && reg_strex_dst <= DR_REG_R7;
#else
    return true;
#endif
}

/* Inserts a jump to label_target if regop1 != regop2, also setting
 * resop to the result.  Avoids flags usage for is_cbnz_available();
 * otherwise the caller must have spilled the flags.
 */
static void
insert_compare_and_jump_not_equal(dcontext_t *dcontext, instrlist_t *ilist,
                                  instr_t *where, opnd_t resop, opnd_t srcop1,
                                  opnd_t srcop2, instr_t *label_target)
{
#ifdef ARM
    if (!is_cbnz_available(dcontext, opnd_get_reg(resop))) {
        PRE(ilist, where, INSTR_CREATE_subs(dcontext, resop, srcop1, srcop2));
        PRE(ilist, where,
            instr_set_predicate(
                XINST_CREATE_jump(dcontext, opnd_create_instr(label_target)),
                DR_PRED_NE));
        return;
    }
#endif
    PRE(ilist, where, INSTR_CREATE_sub(dcontext, resop, srcop1, srcop2));
    PRE(ilist, where,
        INSTR_CREATE_cbnz(dcontext, opnd_create_instr(label_target), resop));
}

static instr_t *
mangle_exclusive_load(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                      instr_t *next_instr)
{
    ASSERT(instr_is_exclusive_load(instr));
#ifdef ARM
    /* TODO i#1698: Preserve ARM predication and add tests.  For now we bail. */
    if (instr_is_predicated(instr)) {
        SYSLOG_INTERNAL_WARNING_ONCE(
            "Not converting predicated exclusive load: NYI i#1698");
        return NULL;
    }
    /* TODO i#1698: Pairs on ARM need consecutive registers.  Our scratch + dead reg
     * usage does not yet support that for our synthetic load.  For now we bail.
     */
    bool is_pair = instr_num_dsts(instr) > 1;
    if (is_pair) {
        SYSLOG_INTERNAL_WARNING_ONCE(
            "Not converting exclusive load-pair to compare-and-swap: NYI i#1698");
        return NULL;
    }
#endif
    LOG(THREAD, LOG_INTERP, 4, "Converting exclusive load @%p to regular\n",
        get_app_instr_xl8(instr));
    RSTATS_INC(num_ldex2cas);
    /* TODO i#1698: Preserve ARM predication and add tests. */
    /* Analyze for stex_in_same_block *before* replacing the stolen reg, so that
     * analysis of intervening instrs is looking at the right regs.
     */
    reg_id_t value_reg = reg_to_pointer_sized(opnd_get_reg(instr_get_dst(instr, 0)));
    reg_id_t value2_reg = DR_REG_NULL;
    if (instr_num_dsts(instr) == 2)
        value2_reg = reg_to_pointer_sized(opnd_get_reg(instr_get_dst(instr, 1)));
    ASSERT(opnd_is_base_disp(instr_get_src(instr, 0)) &&
           opnd_get_index(instr_get_src(instr, 0)) == DR_REG_NULL &&
           opnd_get_disp(instr_get_src(instr, 0)) == 0);
    reg_id_t base_reg = opnd_get_base(instr_get_src(instr, 0));
    /* No need to save the attributes of the ldex if its paired stex is in the same
     * block with no changes to the address or value in between (and we can check
     * the size statically).
     */
    bool stex_in_same_block = false;
    for (instr_t *in = instr_get_next(instr); in != NULL; in = instr_get_next(in)) {
        /* Bail on optimized mangling if followed by another load or a clear of the
         * monitor before the store, since both invalidate this load's monitor.
         */
        if (instr_is_app(in) &&
            (instr_is_exclusive_load(in) || instr_get_opcode(in) == OP_clrex))
            break;
        if (instr_is_app(in) && instr_is_exclusive_store(in)) {
            /* Warn on a mismatched pair. */
            if (opnd_get_size(instr_get_dst(in, 0)) !=
                opnd_get_size(instr_get_src(instr, 0))) {
                /* See comment below about CONSTRAINED UNPREDICTABLE. */
                SYSLOG_INTERNAL_WARNING_ONCE(
                    "Encountered mismatched-size ldex-stex pair: behavior may not "
                    "exactly match CONSTRAINED UNPREDICTABLE hardware");
            }
            if (opnd_get_size(instr_get_dst(in, 0)) ==
                    opnd_get_size(instr_get_src(instr, 0)) &&
                /* Bail on one side being a pair of 4-byte and the other a single 8-byte:
                 * too complicated for the optimized form.
                 */
                opnd_get_size(instr_get_src(in, 0)) ==
                    opnd_get_size(instr_get_dst(instr, 0)) &&
                opnd_get_base(instr_get_dst(in, 0)) == base_reg &&
                /* pick_scratch_reg() only takes 3 conflicts, so we push a pair with the
                 * store res matching a load dest and using the stolen reg to the
                 * unoptimized sequence.  We keep non-stolen-reg cases on the optimized
                 * path because this is relatively common: a store-release pair uses a
                 * monitor because there is no store-release-pair opcode; its load
                 * destinations are discarded, but both cannot be XZR since that is
                 * undefined behavior: so the dead register store result is used.
                 */
                (!instr_uses_reg(in, dr_reg_stolen) ||
                 (reg_to_pointer_sized(opnd_get_reg(instr_get_dst(in, 1))) != value_reg &&
                  reg_to_pointer_sized(opnd_get_reg(instr_get_dst(in, 1))) !=
                      value2_reg)) &&
                /* We bail on optimizing A32 where we have no OP_cbnz and we'd need
                 * to complicate the optimized sequence with a flags spill.
                 */
                is_cbnz_available(dcontext, opnd_get_reg(instr_get_dst(in, 1)))) {
                /* Base and size are the same. */
                stex_in_same_block = true;
                LOG(THREAD, LOG_INTERP, 4,
                    "Using optimized same-block ldex-stex mangling\n");
            }
            break;
        }
        /* Look for changes from app or tool instrs. */
        if (instr_writes_to_reg(in, value_reg, DR_QUERY_INCLUDE_ALL) ||
            (value2_reg != DR_REG_NULL &&
             instr_writes_to_reg(in, value2_reg, DR_QUERY_INCLUDE_ALL)) ||
            instr_writes_to_reg(in, base_reg, DR_QUERY_INCLUDE_ALL)) {
            LOG(THREAD, LOG_INTERP, 4,
                "Value clobbered => not using same-block ldex-stex mangling\n");
            break;
        }
    }
    /* If the ldex uses the stolen reg, we do not swap around it as we normally do,
     * since we have a bunch of TLS refs inside that would then have a non-standard
     * base and confuse translation code.  Instead we change the ldex.
     */
    reg_id_t stolen_swap_reg = DR_REG_NULL;
    ushort swap_slot = 0;
    bool swap_restore = false;
    if (instr_uses_reg(instr, dr_reg_stolen)) {
        if (value_reg == dr_reg_stolen || value2_reg == dr_reg_stolen) {
            /* We can handle the base being the stolen reg: but the values we
             * need to read from these same regs for checks in strex mangling
             * and it gets complex if we have to get yet another scratch reg to
             * get the app value.  It is simpler to bail on the opt in this case.
             */
            stex_in_same_block = false;
        }
        stolen_swap_reg = pick_scratch_reg(dcontext, instr, DR_REG_NULL, DR_REG_NULL,
                                           DR_REG_NULL, false, &swap_slot, &swap_restore);
        if (swap_restore) {
            insert_save_to_tls_if_necessary(dcontext, ilist, instr, stolen_swap_reg,
                                            swap_slot);
        }
        if (instr_reads_from_reg(instr, dr_reg_stolen, DR_QUERY_DEFAULT)) {
            PRE(ilist, instr,
                instr_create_restore_from_tls(dcontext, stolen_swap_reg,
                                              TLS_REG_STOLEN_SLOT));
        }
        instr_replace_reg_resize(instr, dr_reg_stolen, stolen_swap_reg);
        /* Re-acquire registers we may have replaced. */
        value_reg = reg_to_pointer_sized(opnd_get_reg(instr_get_dst(instr, 0)));
        if (instr_num_dsts(instr) == 2)
            value2_reg = reg_to_pointer_sized(opnd_get_reg(instr_get_dst(instr, 1)));
        base_reg = opnd_get_base(instr_get_src(instr, 0));
    }
    ushort xzr_slot = 0, xzr2_slot = 0;
    bool xzr_restore = false, xzr2_restore = false;
    reg_id_t xzr_repl = DR_REG_NULL, xzr2_repl = DR_REG_NULL;
#ifdef AARCH64
    /* If the ldex loads into the zero register, we need to instead get the real
     * value so our compare at the stex will succeed (otherwise we will loop
     * forever: i#5245).  For same-block we statically skip the compare.
     */
    if (!stex_in_same_block && value_reg == DR_REG_XZR) {
        xzr_repl = pick_scratch_reg(dcontext, instr, stolen_swap_reg, DR_REG_NULL,
                                    DR_REG_NULL, true, &xzr_slot, &xzr_restore);
        if (xzr_restore)
            insert_save_to_tls_if_necessary(dcontext, ilist, instr, xzr_repl, xzr_slot);
        opnd_t value_op = instr_get_dst(instr, 0);
        opnd_replace_reg_resize(&value_op, opnd_get_reg(value_op), xzr_repl);
        instr_set_dst(instr, 0, value_op);
        value_reg = xzr_repl;
    }
    if (!stex_in_same_block && value2_reg == DR_REG_XZR) {
        if (value_reg == DR_REG_XZR) {
            /* LDAXP with dest1==dest2 has undefined behavior, but we try to handle it.
             * XXX: I tried to test this but it raises SIGILL on my hardware.
             */
            ASSERT_NOT_TESTED();
        }
        xzr2_repl = pick_scratch_reg(dcontext, instr, stolen_swap_reg, xzr_repl,
                                     DR_REG_NULL, true, &xzr2_slot, &xzr2_restore);
        if (xzr2_restore)
            insert_save_to_tls_if_necessary(dcontext, ilist, instr, xzr2_repl, xzr2_slot);
        opnd_t value2_op = instr_get_dst(instr, 1);
        opnd_replace_reg_resize(&value2_op, opnd_get_reg(value2_op), xzr2_repl);
        instr_set_dst(instr, 1, value2_op);
        value2_reg = xzr2_repl;
    }
#endif
    instr_t *where = instr_get_next(instr);
    if (stex_in_same_block) {
        /* Insert a label so subsequent stex mangling knows the ldex was here. */
        instr_t *marker = INSTR_CREATE_label(dcontext);
        instr_set_note(marker, (void *)DR_NOTE_LDEX);
        dr_instr_label_data_t *label_data = instr_get_label_data_area(marker);
        label_data->data[0] = value_reg;  /* Ruled out dr_reg_stolen above. */
        label_data->data[1] = value2_reg; /* Ruled out dr_reg_stolen above. */
        PRE(ilist, where, marker);
    } else {
        /* Save info on the load for comparison when (if) we hit the store.
         * We insert this *after* the load so we'll have the value loaded.
         */
        /* We need a scratch register. */
        ushort slot;
        bool should_restore;
        reg_id_t scratch = pick_scratch_reg(dcontext, instr, stolen_swap_reg, xzr_repl,
                                            xzr2_repl, true, &slot, &should_restore);
        if (should_restore)
            insert_save_to_tls_if_necessary(dcontext, ilist, where, scratch, slot);
        /* Write the address and value to TLS. */
        /* Pair store requires consecutive register numbers for 32-bit. */
        bool use_pair = IF_AARCH64_ELSE(base_reg != DR_REG_SP, false);
        if (use_pair) {
            PRE(ilist, where,
                XINST_CREATE_store_pair(
                    dcontext,
                    opnd_create_base_disp(dr_reg_stolen, DR_REG_NULL, 0,
                                          TLS_LDSTEX_ADDR_SLOT,
                                          IF_ARM_ELSE(OPSZ_8, OPSZ_16)),
                    opnd_create_reg(base_reg), opnd_create_reg(value_reg)));
        } else {
            /* A64 won't let you use sp as a GPR.  Grrr. */
            if (IF_AARCH64_ELSE(base_reg == DR_REG_SP, false)) {
                PRE(ilist, where,
                    XINST_CREATE_move(dcontext, opnd_create_reg(scratch),
                                      opnd_create_reg(base_reg)));
                PRE(ilist, where,
                    instr_create_save_to_tls(dcontext, scratch, TLS_LDSTEX_ADDR_SLOT));
            } else {
                PRE(ilist, where,
                    instr_create_save_to_tls(dcontext, base_reg, TLS_LDSTEX_ADDR_SLOT));
            }
            PRE(ilist, where,
                instr_create_save_to_tls(dcontext, value_reg, TLS_LDSTEX_VALUE_SLOT));
        }
        /* Write the size to TLS, using a scratch register. */
        PRE(ilist, where,
            XINST_CREATE_load_int(
                dcontext, opnd_create_reg(scratch),
                OPND_CREATE_INT(opnd_get_size(instr_get_src(instr, 0)))));
        /* If a load-pair, write the 2nd value as well. */
        if (instr_num_dsts(instr) == 2) {
            /* For 32-bit, pair store requires consecutive register numbers.
             * XXX: We could store the 2 values at once.
             */
            if (IF_AARCH64_ELSE(opnd_get_size(instr_get_dst(instr, 0)) == OPSZ_PTR,
                                false)) {
                PRE(ilist, where,
                    XINST_CREATE_store_pair(
                        dcontext,
                        opnd_create_base_disp(dr_reg_stolen, DR_REG_NULL, 0,
                                              TLS_LDSTEX_VALUE2_SLOT,
                                              IF_ARM_ELSE(OPSZ_8, OPSZ_16)),
                        opnd_create_reg(value2_reg), opnd_create_reg(scratch)));
            } else {
                /* For A64, we have to treat a pair of 4-bytes as one 8-byte b/c the
                 * strex could be a singleton.
                 */
                if (IF_AARCH64_ELSE(opnd_get_size(instr_get_dst(instr, 0)) == OPSZ_4,
                                    false)) {
                    PRE(ilist, where,
                        XINST_CREATE_store(
                            dcontext,
                            opnd_create_base_disp(dr_reg_stolen, DR_REG_NULL, 0,
                                                  TLS_LDSTEX_VALUE_SLOT + 4, OPSZ_4),
                            instr_get_dst(instr, 1)));
                } else {
                    PRE(ilist, where,
                        instr_create_save_to_tls(dcontext, value2_reg,
                                                 TLS_LDSTEX_VALUE_SLOT));
                }
                PRE(ilist, where,
                    instr_create_save_to_tls(dcontext, scratch, TLS_LDSTEX_SIZE_SLOT));
            }
        } else {
            PRE(ilist, where,
                instr_create_save_to_tls(dcontext, scratch, TLS_LDSTEX_SIZE_SLOT));
        }
        if (should_restore) {
            PRE(ilist, where, instr_create_restore_from_tls(dcontext, scratch, slot));
        }
    }
    /* Replace the exclusive load with a non-exclusive version. */
    instr_t *ld_nonex = create_ld_from_ldex(dcontext, instr);
    instrlist_preinsert(ilist, instr, ld_nonex);
    if (stolen_swap_reg != DR_REG_NULL) {
        if (instr_writes_to_reg(instr, stolen_swap_reg, DR_QUERY_INCLUDE_COND_DSTS)) {
            PRE(ilist, where,
                instr_create_save_to_tls(dcontext, stolen_swap_reg, TLS_REG_STOLEN_SLOT));
        }
        if (swap_restore) {
            PRE(ilist, where,
                instr_create_restore_from_tls(dcontext, stolen_swap_reg, swap_slot));
        }
    }
    if (xzr_restore)
        PRE(ilist, where, instr_create_restore_from_tls(dcontext, xzr_repl, xzr_slot));
    if (xzr2_restore)
        PRE(ilist, where, instr_create_restore_from_tls(dcontext, xzr2_repl, xzr2_slot));
    instrlist_remove(ilist, instr);
    instr_destroy(dcontext, instr);
    return next_instr;
}

static instr_t *
mangle_exclusive_store(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                       instr_t *next_instr)
{
    ASSERT(instr_is_exclusive_store(instr));
    bool is_pair = instr_num_srcs(instr) > 1;
#ifdef ARM
    /* TODO i#1698: Preserve ARM predication and add tests.  For now we bail. */
    if (instr_is_predicated(instr)) {
        SYSLOG_INTERNAL_WARNING_ONCE(
            "Not converting predicated exclusive store to compare-and-swap: NYI i#1698");
        return NULL;
    }
    /* TODO i#1698: Pairs on ARM need consecutive registers.  Our scratch + dead reg
     * usage does not yet support that for our synthetic load.  For now we bail.
     */
    if (is_pair) {
        SYSLOG_INTERNAL_WARNING_ONCE(
            "Not converting exclusive store-pair to compare-and-swap: NYI i#1698");
        return NULL;
    }
#endif
    LOG(THREAD, LOG_INTERP, 4, "Converting exclusive store @%p to compare-and-swap\n",
        get_app_instr_xl8(instr));
    RSTATS_INC(num_stex2cas);
    reg_id_t reg_orig_ld_val = DR_REG_NULL;
    reg_id_t reg_orig_ld_val2 = DR_REG_NULL;
    /* Check whether there's a paired prior ldex with nice behavior (no clear in
     * between, no write to the base or value regs, etc.: all checked in
     * mangle_exclusive_load()), indicated by a label.
     */
    bool ldex_in_same_block = false;
    for (instr_t *in = instr_get_prev(instr); in != NULL; in = instr_get_prev(in)) {
        /* The label is always after the matching load. */
        if (instr_is_exclusive_load(in) || instr_is_exclusive_store(in))
            break;
        if (instr_is_label(in) && instr_get_note(in) == (void *)DR_NOTE_LDEX) {
            ldex_in_same_block = true;
            dr_instr_label_data_t *label_data = instr_get_label_data_area(in);
            reg_orig_ld_val = (reg_id_t)label_data->data[0];
            reg_orig_ld_val2 = (reg_id_t)label_data->data[1];
            break;
        }
    }
    reg_id_t stolen_swap_reg = DR_REG_NULL;
    ushort swap_slot = 0;
    bool swap_restore = false;
    /* If the stex uses the stolen reg, we do not swap around it as we normally do,
     * since we have a bunch of TLS refs inside that would then have a non-standard
     * base and confuse translation code.  Instead we change the stex.
     */
    if (instr_uses_reg(instr, dr_reg_stolen)) {
        /* Make sure we don't clobber a prior value reg, if any.
         * Below we make sure our scratch reg is distinct from this swap reg,
         * which fortunately is a separate case from having prior value regs.
         */
        stolen_swap_reg =
            pick_scratch_reg(dcontext, instr, reg_orig_ld_val, reg_orig_ld_val2,
                             DR_REG_NULL, false, &swap_slot, &swap_restore);
        if (swap_restore) {
            insert_save_to_tls_if_necessary(dcontext, ilist, instr, stolen_swap_reg,
                                            swap_slot);
        }
        if (instr_reads_from_reg(instr, dr_reg_stolen, DR_QUERY_DEFAULT)) {
            PRE(ilist, instr,
                instr_create_restore_from_tls(dcontext, stolen_swap_reg,
                                              TLS_REG_STOLEN_SLOT));
        }
        instr_replace_reg_resize(instr, dr_reg_stolen, stolen_swap_reg);
    }

    reg_id_t reg_res = opnd_get_reg(instr_get_dst(instr, 1));
    opnd_t op_res = opnd_create_reg(reg_to_pointer_sized(reg_res));
    ASSERT(opnd_is_base_disp(instr_get_dst(instr, 0)) &&
           opnd_get_index(instr_get_dst(instr, 0)) == DR_REG_NULL &&
           opnd_get_disp(instr_get_dst(instr, 0)) == 0);
    /* We treat non-same-block A64 pair-4byte as single-8byte to handle ldxr;stxp. */
    if (is_pair && !ldex_in_same_block &&
        opnd_get_size(instr_get_src(instr, 1)) == OPSZ_4)
        is_pair = false;
    reg_t reg_base = opnd_get_base(instr_get_dst(instr, 0));
    instr_t *no_match = INSTR_CREATE_label(dcontext);

    /* The store could use 4 different regs (stxp), and we need 2 scratch regs to
     * hold a pair of prior values, plus another for one of the new values (the other
     * we put in the dead status reg): so 7 regs, but only 3 slots.  The stolen reg
     * swap does not add because the store would then use one fewer of the scratch
     * reg range.
     * For our scratch registers used for comparisons, we cannot easily use a dead
     * register because the compares have to write to the store result (so the mismatch
     * case has the right result), and that would clobber a subsequent compare's
     * sources: thus we pass false for dead_reg_ok.  (We do use the dead store result
     * for the first live-value in create_ldax_from_stex() which works out as we
     * compare that one immediately.)
     */
    reg_id_t scratch = DR_REG_NULL, scratch2 = DR_REG_NULL, scratch3 = DR_REG_NULL;
    ushort slot, slot2, slot3;
    bool should_restore = false, should_restore2 = false, should_restore3 = false;
    reg_id_t reg_new_ld_val = DR_REG_NULL;
    reg_id_t reg_new_ld_val2 = DR_REG_NULL;
    bool compare_second_first = false;
    if (ldex_in_same_block) {
        /* We aren't saving the flags so we can only handle Thumb mode with CBNZ. */
        IF_ARM(ASSERT(is_cbnz_available(dcontext, reg_res)));
        if (reg_to_pointer_sized(reg_res) == reg_orig_ld_val ||
            reg_to_pointer_sized(reg_res) == reg_orig_ld_val2) {
            /* We can't use the store res in the synthetic load if it has a value. */
            scratch = pick_scratch_reg(dcontext, instr, stolen_swap_reg, reg_orig_ld_val,
                                       reg_orig_ld_val2,
                                       /*dead_reg_ok=*/false, &slot, &should_restore);
            if (should_restore)
                insert_save_to_tls_if_necessary(dcontext, ilist, instr, scratch, slot);
            reg_new_ld_val = scratch;
            if (reg_to_pointer_sized(reg_res) == reg_orig_ld_val2)
                compare_second_first = true;
        }
        if (is_pair) {
            /* We do need one scratch reg for the value comparison. */
            /* pick_scratch_reg() only takes 3 conflicts, so we push a pair with the
             * store res matching a load dest (scratch != DR_REG_NULL) and using the
             * stolen register (stolen_swap_reg != DR_REG_NULL) to not use
             * ldex_in_same_block.
             */
            ASSERT(stolen_swap_reg == DR_REG_NULL || scratch == DR_REG_NULL);
            reg_id_t swap_or_scratch =
                (stolen_swap_reg == DR_REG_NULL) ? scratch : stolen_swap_reg;
            scratch3 = pick_scratch_reg(dcontext, instr, swap_or_scratch, reg_orig_ld_val,
                                        reg_orig_ld_val2,
                                        /*dead_reg_ok=*/false, &slot3, &should_restore3);
            if (should_restore3)
                insert_save_to_tls_if_necessary(dcontext, ilist, instr, scratch3, slot3);
            reg_new_ld_val2 = scratch3;
        }
    } else {
        ASSERT(reg_orig_ld_val == DR_REG_NULL &&
               reg_orig_ld_val2 ==
                   DR_REG_NULL); /* We only have to avoid stolen_swap_reg. */
        /* We pass false to avoid the status reg, which we ourselves use. */
        scratch = pick_scratch_reg(dcontext, instr, stolen_swap_reg, DR_REG_NULL,
                                   DR_REG_NULL, false, &slot, &should_restore);
        if (should_restore)
            insert_save_to_tls_if_necessary(dcontext, ilist, instr, scratch, slot);
#ifdef ARM
        if (!is_cbnz_available(dcontext, reg_res)) {
            /* We have no CBNZ so we need to preserve the flags. */
            dr_save_arith_flags_to_reg(dcontext, ilist, instr, scratch);
            PRE(ilist, instr,
                instr_create_save_to_tls(dcontext, scratch, TLS_LDSTEX_FLAGS_SLOT));
        }
#endif
        if (is_pair) {
            scratch2 =
                pick_scratch_reg(dcontext, instr, stolen_swap_reg, scratch, DR_REG_NULL,
                                 /*dead_reg_ok=*/false, &slot2, &should_restore2);
            scratch3 =
                pick_scratch_reg(dcontext, instr, stolen_swap_reg, scratch, scratch2,
                                 /*dead_reg_ok=*/false, &slot3, &should_restore3);
            if (should_restore2)
                insert_save_to_tls_if_necessary(dcontext, ilist, instr, scratch2, slot2);
            if (should_restore3)
                insert_save_to_tls_if_necessary(dcontext, ilist, instr, scratch3, slot3);
            reg_new_ld_val2 = scratch3;
        }
        /* Compare address, arranging op_res to show failure on mismatch (though
         * now that we have a stex after no_match for fault fidelity it will set
         * op_res for us; we need a dead/scratch here anyway and op_res fits the bill).
         * XXX i#400: It is possible that the store could fault and the app could
         * examine op_res in the handler: i.e., it's not truly dead.  We do not
         * account for that here.
         */
        PRE(ilist, instr,
            instr_create_restore_from_tls(dcontext, scratch, TLS_LDSTEX_ADDR_SLOT));
        insert_compare_and_jump_not_equal(dcontext, ilist, instr, op_res,
                                          opnd_create_reg(reg_base),
                                          opnd_create_reg(scratch), no_match);

        /* Compare size, arranging op_res to show failure on mismatch.  On some
         * processors, if the stxr's address range is a subset of the ldxp's range, it
         * will succeed, even if the size or base address are not identical.  However,
         * the manual states that this is CONSTRAINED UNPREDICTABLE behavior: B2.9.5 says
         * "software can rely on a LoadExcl / StoreExcl pair to eventually succeed only
         * if the LoadExcl and the StoreExcl have the same transaction size."  Similarly
         * for the target VA and reg count.  Thus, given the complexity of trying to
         * match the actual processor behavior and comparing ranges and whatnot, we're ok
         * with DR enforcing a strict equality, until or unless we see real apps relying
         * on processor quirks.
         */
        PRE(ilist, instr,
            instr_create_restore_from_tls(dcontext, scratch, TLS_LDSTEX_SIZE_SLOT));
        insert_compare_and_jump_not_equal(
            dcontext, ilist, instr, op_res, opnd_create_reg(scratch),
            OPND_CREATE_INT(opnd_get_size(instr_get_dst(instr, 0))), no_match);
    }

    /* Perform the compare-and-swap. */
    if (!ldex_in_same_block) {
        PRE(ilist, instr,
            instr_create_restore_from_tls(dcontext, scratch, TLS_LDSTEX_VALUE_SLOT));
        reg_orig_ld_val = scratch;
        if (is_pair) {
            PRE(ilist, instr,
                instr_create_restore_from_tls(dcontext, scratch2,
                                              TLS_LDSTEX_VALUE2_SLOT));
            reg_orig_ld_val2 = scratch2;
        }
    }
    PRE(ilist, instr,
        create_ldax_from_stex(dcontext, instr, &reg_new_ld_val, reg_new_ld_val2,
                              !is_pair));
    reg_new_ld_val = reg_to_pointer_sized(reg_new_ld_val);
    /* Skip the value comparison if the load discarded via XZR.
     * This is not an optimization, but required to avoid an infinite loop (i#5245).
     * (For !ldex_in_same_block, we handle this when mangling the load.)
     */
    if (is_pair &&
        /* If the 2nd matches the store results, we have to compare it first, since
         * op_res will clobber the value.
         */
        compare_second_first IF_AARCH64(
            &&(!ldex_in_same_block || reg_orig_ld_val2 != DR_REG_XZR))) {
        insert_compare_and_jump_not_equal(dcontext, ilist, instr, op_res,
                                          opnd_create_reg(reg_new_ld_val2),
                                          opnd_create_reg(reg_orig_ld_val2), no_match);
    }
    if (IF_AARCH64_ELSE(!ldex_in_same_block || reg_orig_ld_val != DR_REG_XZR, true)) {
        insert_compare_and_jump_not_equal(dcontext, ilist, instr, op_res,
                                          opnd_create_reg(reg_new_ld_val),
                                          opnd_create_reg(reg_orig_ld_val), no_match);
    }
    if (is_pair &&
        !compare_second_first IF_AARCH64(
            &&(!ldex_in_same_block || reg_orig_ld_val2 != DR_REG_XZR))) {
        insert_compare_and_jump_not_equal(dcontext, ilist, instr, op_res,
                                          opnd_create_reg(reg_new_ld_val2),
                                          opnd_create_reg(reg_orig_ld_val2), no_match);
    }

    /* <---- The original store is here. ----> */

    instr_t *post_store = instr_get_next(instr);
    instr_t *skip_clrex = INSTR_CREATE_label(dcontext);
    PRE(ilist, post_store, XINST_CREATE_jump(dcontext, opnd_create_instr(skip_clrex)));
    PRE(ilist, post_store, no_match);
    /* Clear the monitor, which would happen on a real mismatch. */
    PRE(ilist, post_store, INSTR_CREATE_clrex(dcontext));
    /* Execute the store to duplicate a fault, which happens natively on a mismatch.
     * If we don't do this, the app will likely loop back and might loop forever or
     * might fault incorrectly on the load if its base is now bad.
     */
    PRE(ilist, post_store, instr_clone(dcontext, instr));
    PRE(ilist, post_store, skip_clrex);
    if (should_restore) {
#ifdef ARM
        if (!is_cbnz_available(dcontext, reg_res)) {
            PRE(ilist, post_store,
                instr_create_restore_from_tls(dcontext, scratch, TLS_LDSTEX_FLAGS_SLOT));
            dr_restore_arith_flags_from_reg(dcontext, ilist, post_store, scratch);
        }
#endif
        PRE(ilist, post_store, instr_create_restore_from_tls(dcontext, scratch, slot));
    }
    if (should_restore2) {
        PRE(ilist, post_store, instr_create_restore_from_tls(dcontext, scratch2, slot2));
    }
    if (should_restore3) {
        PRE(ilist, post_store, instr_create_restore_from_tls(dcontext, scratch3, slot3));
    }
    if (stolen_swap_reg != DR_REG_NULL) {
        if (instr_writes_to_reg(instr, stolen_swap_reg, DR_QUERY_INCLUDE_COND_DSTS)) {
            PRE(ilist, post_store,
                instr_create_save_to_tls(dcontext, stolen_swap_reg, TLS_REG_STOLEN_SLOT));
        }
        if (swap_restore) {
            PRE(ilist, post_store,
                instr_create_restore_from_tls(dcontext, stolen_swap_reg, swap_slot));
        }
    }
    return next_instr;
}

/* Returns NULL if it did not instrument and the caller should handle stolen reg, etc. */
instr_t *
mangle_exclusive_monitor_op(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                            instr_t *next_instr)
{
    if (!INTERNAL_OPTION(ldstex2cas))
        return NULL;
    /* For -ldstex2cas we convert exclusive monitor regions into compare-and-swap
     * operations in order to allow regular instrumentation, with the downside of
     * weaker synchronization semantics.
     * See https://dynamorio.org/page_ldstex.html for background and further details.
     * The summary is:
     * + On an exclusive load, save the address, value, opcode, and size,
     *   and convert to a non-exclusive load.
     * + On an exclusive store, if the address, value, or size do not match,
     *   fail the store.  If the opcode does not match: we have complications;
     *   exit to DR.
     *   Then insert a compare-and-swap.
     * + On OP_clrex, clear the saved address.
     */
    if (instr_is_exclusive_load(instr)) {
        return mangle_exclusive_load(dcontext, ilist, instr, next_instr);
    } else if (instr_is_exclusive_store(instr)) {
        return mangle_exclusive_store(dcontext, ilist, instr, next_instr);
    } else if (instr_get_opcode(instr) == OP_clrex) {
        LOG(THREAD, LOG_INTERP, 4, "Clearing fields for CLREX @%p\n",
            get_app_instr_xl8(instr));
        /* Clear the ldstex fields.  Just the addr field should do it.
         * We just need to ensure that "ldxr;clrex;stxr" fails.
         * We rule out same-block handling (which skips the addr comparison)
         * above in ldex handling.
         */
#ifdef AARCH64
        PRE(ilist, instr,
            instr_create_save_to_tls(dcontext, DR_REG_XZR, TLS_LDSTEX_ADDR_SLOT));
#else
        ushort slot;
        bool should_restore;
        reg_id_t scratch = pick_scratch_reg(dcontext, instr, DR_REG_NULL, DR_REG_NULL,
                                            DR_REG_NULL, true, &slot, &should_restore);
        if (should_restore)
            insert_save_to_tls_if_necessary(dcontext, ilist, instr, scratch, slot);
        PRE(ilist, instr,
            XINST_CREATE_load_int(dcontext, opnd_create_reg(scratch),
                                  OPND_CREATE_INT(0)));
        PRE(ilist, instr,
            instr_create_save_to_tls(dcontext, scratch, TLS_LDSTEX_ADDR_SLOT));
        if (should_restore)
            PRE(ilist, instr, instr_create_restore_from_tls(dcontext, scratch, slot));
#endif
    }
    return next_instr;
}

/* END OF MANGLING ROUTINES
 *###########################################################################
 *###########################################################################
 */
