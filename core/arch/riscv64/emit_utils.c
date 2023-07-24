/* **********************************************************
 * Copyright (c) 2022 Rivos, Inc.  All rights reserved.
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
 * * Neither the name of Rivos, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL RIVOS, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include <stdint.h>

#include "../globals.h"
#include "arch.h"

#define RAW_NOP_INST 0x00000013
#define RAW_NOP_INST_SZ 4

#define RAW_C_NOP_INST 0x0001
#define RAW_C_NOP_INST_SZ 2

/***************************************************************************/
/*                               EXIT STUB                                 */
/***************************************************************************/

byte *
insert_relative_target(byte *pc, cache_pc target, bool hot_patch)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

byte *
insert_relative_jump(byte *pc, cache_pc target, bool hot_patch)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

uint
nop_pad_ilist(dcontext_t *dcontext, fragment_t *f, instrlist_t *ilist, bool emitting)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return 0;
}

int
insert_exit_stub_other_flags(dcontext_t *dcontext, fragment_t *f, linkstub_t *l,
                             cache_pc stub_pc, ushort l_flags)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return 0;
}

bool
exit_cti_reaches_target(dcontext_t *dcontext, fragment_t *f, linkstub_t *l,
                        cache_pc target_pc)
{
    cache_pc branch_pc = EXIT_CTI_PC(f, l);
    /* Compute offset as unsigned, modulo arithmetic. */
    ptr_uint_t off = (ptr_uint_t)target_pc - (ptr_uint_t)branch_pc;
    uint enc = *(uint *)branch_pc;
    ASSERT(ALIGNED(branch_pc, 2) && ALIGNED(target_pc, 2));

    if ((enc & 0x7f) == 0x63 &&
        /* BEQ, BNE */
        (((enc >> 12) & 0x7) <= 0x1 ||
         /* BLT, BGE, BLTU, BGEU */
         ((enc >> 12) & 0x7) >= 0x4))
        return (off < 0x1000);
    else if ((enc & 0x7f) == 0x6f) /* JAL */
        return (off < 0x100000);
    /* TODO: i#3544: Add support for compressed instructions.  */
    ASSERT(false);
    return false;
}

void
patch_stub(fragment_t *f, cache_pc stub_pc, cache_pc target_pc, cache_pc target_prefix_pc,
           bool hot_patch)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return;
}

bool
stub_is_patched(dcontext_t *dcontext, fragment_t *f, cache_pc stub_pc)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

void
unpatch_stub(dcontext_t *dcontext, fragment_t *f, cache_pc stub_pc, bool hot_patch)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

void
patch_branch(dr_isa_mode_t isa_mode, cache_pc branch_pc, cache_pc target_pc,
             bool hot_patch)
{
    /* Compute offset as unsigned, modulo arithmetic. */
    ptr_int_t off = (ptr_uint_t)target_pc - (ptr_uint_t)branch_pc;
    uint *pc_writable = (uint *)vmcode_get_writable_addr(branch_pc);
    uint enc = *pc_writable;
    ASSERT(ALIGNED(branch_pc, 2) && ALIGNED(target_pc, 2));
    if ((enc & 0x7f) == 0x63 &&
        /* BEQ, BNE */
        (((enc >> 12) & 0x7) <= 0x1 ||
         /* BLT, BGE, BLTU, BGEU */
         ((enc >> 12) & 0x7) >= 0x4)) {
        ASSERT(((off << (64 - 13)) >> (64 - 13)) == off);

        /* Format of the B-type instruction:
         * |  31   |30     25|24   20|19   15|14    12|11     8|   7   |6      0|
         * |imm[12]|imm[10:5]|  rs2  |  rs1  | funct3 |imm[4:1]|imm[11]| opcode |
         *  ^---------------^                          ^--------------^
         */
        *pc_writable = (enc & 0x1fff07f) | (((off >> 12) & 1) << 31) |
            (((off >> 5) & 63) << 25) | (((off >> 1) & 15) << 8) |
            (((off >> 11) & 1) << 7);
    } else if ((enc & 0xfff) == 0x6f) { /* J */
        ASSERT(((off << (64 - 21)) >> (64 - 21)) == off);

        /* Format of the J-type instruction:
         * |   31    |30       21|   20    |19        12|11   7|6      0|
         * | imm[20] | imm[10:1] | imm[11] | imm[19:12] |  rd  | opcode |
         *  ^------------------------------------------^
         */
        *pc_writable = 0x6f | (((off >> 20) & 1) << 31) | (((off >> 1) & 0x3ff) << 21) |
            (((off >> 11) & 1) << 20) | (((off >> 12) & 0xff) << 12);
    } else
        /* TODO: i#3544: Add support for compressed instructions.  */
        ASSERT(false);
    if (hot_patch)
        machine_cache_sync(branch_pc, branch_pc + 4, true);
    return;
}

uint
patchable_exit_cti_align_offs(dcontext_t *dcontext, instr_t *inst, cache_pc pc)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return 0;
}

cache_pc
exit_cti_disp_pc(cache_pc branch_pc)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

void
link_indirect_exit_arch(dcontext_t *dcontext, fragment_t *f, linkstub_t *l,
                        bool hot_patch, app_pc target_tag)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

cache_pc
indirect_linkstub_stub_pc(dcontext_t *dcontext, fragment_t *f, linkstub_t *l)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

cache_pc
cbr_fallthrough_exit_cti(cache_pc prev_cti_pc)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
}

void
unlink_indirect_exit(dcontext_t *dcontext, fragment_t *f, linkstub_t *l)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

/*******************************************************************************
 * COARSE-GRAIN FRAGMENT SUPPORT
 */

cache_pc
entrance_stub_jmp(cache_pc stub)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

bool
coarse_is_entrance_stub(cache_pc stub)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

/*###########################################################################
 *
 * fragment_t Prefixes
 */

int
fragment_ibt_prefix_size(uint flags)
{
    /* Nothing extra for ibt as we don't have flags to restore */
    return FRAGMENT_BASE_PREFIX_SIZE(flags);
}

void
insert_fragment_prefix(dcontext_t *dcontext, fragment_t *f)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

/***************************************************************************/
/*             THREAD-PRIVATE/SHARED ROUTINE GENERATION                    */
/***************************************************************************/

void
append_call_exit_dr_hook(dcontext_t *dcontext, instrlist_t *ilist, bool absolute,
                         bool shared)
{
    ASSERT_NOT_IMPLEMENTED(EXIT_DR_HOOK == NULL);
}

void
append_restore_xflags(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

/* dcontext is in REG_DCXT; other registers can be used as scratch.
 */
void
append_restore_simd_reg(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

void
append_restore_gpr(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

void
append_save_gpr(dcontext_t *dcontext, instrlist_t *ilist, bool ibl_end, bool absolute,
                generated_code_t *code, linkstub_t *linkstub, bool coarse_info)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

/* dcontext base is held in REG_DCXT, and exit stub in X0.
 * GPR's are already saved.
 */
void
append_save_simd_reg(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

/* Scratch reg0 is holding exit stub. */
void
append_save_clear_xflags(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

bool
append_call_enter_dr_hook(dcontext_t *dcontext, instrlist_t *ilist, bool ibl_end,
                          bool absolute)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

void
insert_save_eflags(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where, uint flags,
                   bool tls, bool absolute)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

void
insert_restore_eflags(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                      uint flags, bool tls, bool absolute)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

byte *
emit_inline_ibl_stub(dcontext_t *dcontext, byte *pc, ibl_code_t *ibl_code,
                     bool target_trace_table)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

byte *
emit_indirect_branch_lookup(dcontext_t *dc, generated_code_t *code, byte *pc,
                            byte *fcache_return_pc, bool target_trace_table,
                            bool inline_ibl_head, ibl_code_t *ibl_code /* IN/OUT */)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

void
relink_special_ibl_xfer(dcontext_t *dcontext, int index,
                        ibl_entry_point_type_t entry_type, ibl_branch_type_t ibl_type)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

bool
fill_with_nops(dr_isa_mode_t isa_mode, byte *addr, size_t size)
{
    /* FIXME i#3544: We need to detect if C-extension is available and use
     * the appropriate NOP encoding.
     */
    const size_t nop_sz = RAW_C_NOP_INST_SZ;
    byte *pc;

    if (!ALIGNED(addr, nop_sz) || !ALIGNED(addr + size, nop_sz)) {
        ASSERT_NOT_REACHED();
        return false;
    }
    /* Little endian is assumed here as everywhere else. */
    for (pc = addr; pc < addr + size; pc += nop_sz) {
        if (nop_sz == RAW_C_NOP_INST_SZ)
            *(uint16_t *)pc = RAW_C_NOP_INST; /* c.nop */
        else
            *(uint32_t *)pc = RAW_NOP_INST; /* nop */
    }
    return true;
}

void
append_fcache_enter_prologue(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}
