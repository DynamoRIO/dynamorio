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
#include "instr_create_shared.h"
#include "instrlist.h"
#include "instrument.h"
#include "arch.h"

#define APP instrlist_meta_append
#define PRE instrlist_meta_preinsert

#define RAW_NOP_INST 0x00000013
#define RAW_NOP_INST_SZ 4

#define RAW_C_NOP_INST 0x0001
#define RAW_C_NOP_INST_SZ 2

#define RAW_JR_A1_INST 0x58067

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

/* Returns writable addr for the target_pc data slot of the given stub. The slot starts at
 * the 8-byte aligned region in the 16-byte slot reserved in the stub.
 */
static ptr_uint_t *
get_target_pc_slot(fragment_t *f, cache_pc stub_pc)
{
    return (ptr_uint_t *)ALIGN_FORWARD(
        vmcode_get_writable_addr(stub_pc + DIRECT_EXIT_STUB_SIZE(f->flags) -
                                 DIRECT_EXIT_STUB_DATA_SZ),
        8);
}

/* Emit code for the exit stub at stub_pc. Return the size of the emitted code in bytes.
 * This routine assumes that the caller will take care of any cache synchronization
 * necessary. The stub is unlinked initially, except coarse grain indirect exits, which
 * are always linked.
 */
int
insert_exit_stub_other_flags(dcontext_t *dcontext, fragment_t *f, linkstub_t *l,
                             cache_pc stub_pc, ushort l_flags)
{
    instrlist_t ilist;
    instrlist_init(&ilist);

    ushort *write_stub_pc = (ushort *)vmcode_get_writable_addr(stub_pc);
    /* Declare pc as ushort to help handling of C extension instructions. */
    ushort *pc = write_stub_pc, *new_pc;
    uint num_nops_needed = 0;
    uint max_instrs = 0;
    uint remainder = (uint64)pc & 0x3;

    /* Insert a c.nop at top for non-aligned stub_pc, so instructions after are all
     * aligned. */
    if (remainder != 0) {
        ASSERT(remainder == 2);
        *pc++ = RAW_C_NOP_INST;
    }

    /* FIXME i#3544: coarse-grain NYI on RISCV64 */
    ASSERT_NOT_IMPLEMENTED(!TEST(FRAG_COARSE_GRAIN, f->flags));

    if (LINKSTUB_DIRECT(l_flags)) {
        APP(&ilist,
            INSTR_CREATE_sd(dcontext, OPND_CREATE_MEMPTR(dr_reg_stolen, TLS_REG0_SLOT),
                            opnd_create_reg(DR_REG_A0)));
        max_instrs++;
        APP(&ilist,
            INSTR_CREATE_sd(dcontext, OPND_CREATE_MEMPTR(dr_reg_stolen, TLS_REG1_SLOT),
                            opnd_create_reg(DR_REG_A1)));
        max_instrs++;

        /* Insert an anchor for the subsequent insert_mov_immed_ptrsz() call. */
        instr_t *nop = INSTR_CREATE_addi(dcontext, opnd_create_reg(DR_REG_X0),
                                         opnd_create_reg(DR_REG_X0),
                                         opnd_create_immed_int(0, OPSZ_12b));
        APP(&ilist, nop);

        insert_mov_immed_ptrsz(dcontext, (ptr_int_t)l, opnd_create_reg(DR_REG_A0), &ilist,
                               nop, NULL, NULL);
        /* Up to 8 instructions will be generated, see mov64(). */
        max_instrs += 8;

        instrlist_remove(&ilist, nop);
        instr_destroy(dcontext, nop);

        new_pc = (ushort *)instrlist_encode(dcontext, &ilist, (byte *)pc, false);
        instrlist_clear(dcontext, &ilist);

        /* We can use INSTR_CREATE_auipc() here, but it's easier to use a raw value.
         * Now, A1 holds the current pc - RISCV64_INSTR_SIZE.
         */
        *(uint *)new_pc = 0x00000597; /* auipc a1, 0x0 */
        new_pc += 2;
        max_instrs++;

        ptr_uint_t *target_pc_slot = get_target_pc_slot(f, stub_pc);
        ASSERT(new_pc < (ushort *)target_pc_slot);
        uint target_pc_slot_offs =
            (byte *)target_pc_slot - (byte *)new_pc + RISCV64_INSTR_SIZE;

        instrlist_init(&ilist);

        /* Now, A1 holds the address of fcache_return routine. */
        APP(&ilist,
            INSTR_CREATE_ld(dcontext, opnd_create_reg(DR_REG_A1),
                            OPND_CREATE_MEMPTR(DR_REG_A1, target_pc_slot_offs)));
        max_instrs++;

        APP(&ilist, XINST_CREATE_jump_reg(dcontext, opnd_create_reg(DR_REG_A1)));
        max_instrs++;

        new_pc = (ushort *)instrlist_encode(dcontext, &ilist, (byte *)new_pc, false);

        num_nops_needed = max_instrs - ((uint *)new_pc - (uint *)pc);
        pc = new_pc;

        /* Fill up with NOPs, depending on how many instructions we needed to move
         * the immediate into a register. Ideally we would skip adding NOPs, but
         * lots of places expect the stub size to be fixed.
         */
        for (uint j = 0; j < num_nops_needed; j++) {
            *(uint *)pc = RAW_NOP_INST;
            pc += 2;
        }

        /* The final slot is a data slot, which will hold the address of either
         * the fcache-return routine or the linked fragment. We reserve 16 bytes
         * and use the 8-byte aligned region of 8 bytes within it.
         */
        ASSERT((byte *)pc == (byte *)target_pc_slot ||
               (byte *)pc + 2 == (byte *)target_pc_slot ||
               (byte *)pc + 4 == (byte *)target_pc_slot ||
               (byte *)pc + 6 == (byte *)target_pc_slot);
        pc += (DIRECT_EXIT_STUB_DATA_SZ - remainder) / sizeof(ushort);

        /* We start off with the fcache-return routine address in the slot.
         * RISCV64 uses shared gencode. So, fcache_return routine address should be
         * same, no matter which thread creates/unpatches the stub.
         */
        ASSERT(fcache_return_routine(dcontext) == fcache_return_routine(GLOBAL_DCONTEXT));
        *target_pc_slot = (ptr_uint_t)fcache_return_routine(dcontext);
        ASSERT((ptr_int_t)((byte *)pc - (byte *)write_stub_pc) ==
               DIRECT_EXIT_STUB_SIZE(l_flags));
    } else {
        /* Stub starts out unlinked. */
        cache_pc exit_target =
            get_unlinked_entry(dcontext, EXIT_TARGET_TAG(dcontext, f, l));
        APP(&ilist,
            INSTR_CREATE_sd(dcontext, OPND_CREATE_MEMPTR(dr_reg_stolen, TLS_REG0_SLOT),
                            opnd_create_reg(DR_REG_A0)));
        max_instrs++;
        APP(&ilist,
            INSTR_CREATE_sd(dcontext, OPND_CREATE_MEMPTR(dr_reg_stolen, TLS_REG1_SLOT),
                            opnd_create_reg(DR_REG_A1)));
        max_instrs++;

        instr_t *next_instr = INSTR_CREATE_ld(
            dcontext, opnd_create_reg(DR_REG_A1),
            OPND_CREATE_MEMPTR(dr_reg_stolen,
                               get_ibl_entry_tls_offs(dcontext, exit_target)));
        APP(&ilist, next_instr);
        max_instrs++;
        insert_mov_immed_ptrsz(dcontext, (ptr_int_t)l, opnd_create_reg(DR_REG_A0), &ilist,
                               next_instr, NULL, NULL);
        /* Up to 8 instructions will be generated, see mov64(). */
        max_instrs += 8;

        APP(&ilist, XINST_CREATE_jump_reg(dcontext, opnd_create_reg(DR_REG_A1)));
        max_instrs++;

        new_pc = (ushort *)instrlist_encode(dcontext, &ilist, (byte *)pc, false);

        num_nops_needed = max_instrs - ((uint *)new_pc - (uint *)pc);
        pc = new_pc;

        /* Fill up with NOPs, depending on how many instructions we needed to move
         * the immediate into a register. Ideally we would skip adding NOPs, but
         * lots of places expect the stub size to be fixed.
         */
        for (uint j = 0; j < num_nops_needed; j++) {
            *(uint *)pc = RAW_NOP_INST;
            pc += 2;
        }

        *pc++ = RAW_C_NOP_INST;
        if (remainder == 0)
            *pc++ = RAW_C_NOP_INST;
    }
    instrlist_clear(dcontext, &ilist);

    return (int)((byte *)pc - (byte *)write_stub_pc);
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
    else if ((enc & 0x3) == 0x1 && ((ushort)enc >> 13) >= 0x6) /* C.BEQZ, C.BNEZ */
        return (off < 0x100);
    else if ((enc & 0x3) == 0x1 && ((ushort)enc >> 13) == 0x5) /* C.J */
        return (off < 0x800);
    else
        ASSERT(false);
    return false;
}

void
patch_stub(fragment_t *f, cache_pc stub_pc, cache_pc target_pc, cache_pc target_prefix_pc,
           bool hot_patch)
{
    ptr_int_t off = (ptr_int_t)target_pc - (ptr_int_t)stub_pc;
    if (off < 0x100000 && off > (ptr_int_t)0xFFFFFFFFFFF00000L) {
        /* target_pc is a near fragment. We can get there with a J (OP_jal, 21-bit signed
         * immediate offset).
         */
        ASSERT(((off << (64 - 21)) >> (64 - 21)) == off);

        /* Format of the J-type instruction:
         * |   31    |30       21|   20    |19        12|11   7|6      0|
         * | imm[20] | imm[10:1] | imm[11] | imm[19:12] |  rd  | opcode |
         *  ^------------------------------------------^
         */
        *(uint *)vmcode_get_writable_addr(stub_pc) = 0x6f | (((off >> 20) & 1) << 31) |
            (((off >> 1) & 0x3ff) << 21) | (((off >> 11) & 1) << 20) |
            (((off >> 12) & 0xff) << 12);
        if (hot_patch)
            machine_cache_sync(stub_pc, stub_pc + 4, true);
        return;
    }
    /* target_pc is a far fragment. We must use an indirect branch. Note that the indirect
     * branch needs to be to the fragment prefix, as we need to restore the clobbered
     * regs.
     */
    /* We set hot_patch to false as we are not modifying code. */
    ATOMIC_8BYTE_ALIGNED_WRITE(get_target_pc_slot(f, stub_pc),
                               (ptr_uint_t)target_prefix_pc,
                               /*hot_patch=*/false);
}

static bool
stub_is_patched_for_intermediate_fragment_link(dcontext_t *dcontext, cache_pc stub_pc)
{
    uint enc;
    ATOMIC_4BYTE_ALIGNED_READ(stub_pc, &enc);
    return (enc & 0xfff) == 0x6f; /* J (OP_jal) */
}

static bool
stub_is_patched_for_far_fragment_link(dcontext_t *dcontext, fragment_t *f,
                                      cache_pc stub_pc)
{
    ptr_uint_t target_pc;
    ATOMIC_8BYTE_ALIGNED_READ(get_target_pc_slot(f, stub_pc), &target_pc);
    return target_pc != (ptr_uint_t)fcache_return_routine(dcontext);
}

bool
stub_is_patched(dcontext_t *dcontext, fragment_t *f, cache_pc stub_pc)
{
    /* If stub_pc is not aligned to 4 bytes, the first instruction will be c.nop, see
     * insert_exit_stub_other_flags(). */
    stub_pc = ALIGNED(stub_pc, 4) ? stub_pc : stub_pc + 2;
    return stub_is_patched_for_intermediate_fragment_link(dcontext, stub_pc) ||
        stub_is_patched_for_far_fragment_link(dcontext, f, stub_pc);
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
    } else if ((enc & 0x3) == 0x1 && ((ushort)enc >> 13) >= 0x6) { /* C.BEQZ, C.BNEZ */
        ASSERT(((off << (64 - 9)) >> (64 - 9)) == off);

        /* Format of the CB-type instruction:
         * |15 13|12        10|9   7|6              2|1      0|
         * | ... | imm[8|4:3] | ... | imm[7:6|2:1|5] | opcode |
         *        ^----------^       ^--------------^
         */
        *(ushort *)pc_writable =
            (ushort)((enc & 0xe383) | (((off >> 8) & 1) << 12) |
                     (((off >> 3) & 3) << 10) | (((off >> 6) & 3) << 5) |
                     (((off >> 1) & 3) << 3) | (((off >> 5) & 1) << 2));
    } else if ((enc & 0x3) == 0x1 && ((ushort)enc >> 13) == 0x5) { /* C.J */
        ASSERT(((off << (64 - 12)) >> (64 - 12)) == off);

        /* Decode the immediate field of the CJ-type format as a pc-relative offset:
         * |15 13|12                      2|1      0|
         * | ... | [11|4|9:8|10|6|7|3:1|5] | opcode |
         *        ^-----------------------^
         */
        *(ushort *)pc_writable =
            (ushort)((enc & 0xe003) | (((off >> 11) & 1) << 12) |
                     (((off >> 4) & 1) << 11) | (((off >> 8) & 3) << 9) |
                     (((off >> 10) & 1) << 8) | (((off >> 6) & 1) << 7) |
                     (((off >> 7) & 1) << 6) | (((off >> 1) & 7) << 3) |
                     (((off >> 5) & 1) << 2));
    } else
        ASSERT(false);
    if (hot_patch)
        machine_cache_sync(branch_pc, branch_pc + 4, true);
    return;
}

uint
patchable_exit_cti_align_offs(dcontext_t *dcontext, instr_t *inst, cache_pc pc)
{
    return 0; /* Always aligned. */
}

cache_pc
exit_cti_disp_pc(cache_pc branch_pc)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

/* Skips nop instructions backwards until the first `jr a1` instruction is found. */
static uint *
get_stub_branch(uint *val)
{
    ushort *pc = (ushort *)val;
    /* Skip c.nop/nop instructions backwards. */
    while (*pc == RAW_C_NOP_INST || *(uint *)pc == RAW_NOP_INST) {
        /* We're looking for `jr a1`, its upper 16 bits are 0x5, not 0x1 (RAW_C_NOP_INST),
         * so this is safe to do. */
        if (*(pc - 1) == RAW_C_NOP_INST)
            pc--;
        else
            pc -= 2;
    }
    /* The first non-NOP instruction must be the branch. */
    ASSERT(*(uint *)pc == RAW_JR_A1_INST);
    return (uint *)pc;
}

void
link_indirect_exit_arch(dcontext_t *dcontext, fragment_t *f, linkstub_t *l,
                        bool hot_patch, app_pc target_tag)
{
    byte *stub_pc = (byte *)EXIT_STUB_PC(dcontext, f, l);
    uint *pc;
    cache_pc exit_target;
    ibl_type_t ibl_type = { 0 };
    DEBUG_DECLARE(bool is_ibl =)
    get_ibl_routine_type_ex(dcontext, target_tag, &ibl_type);
    ASSERT(is_ibl);
    if (IS_IBL_LINKED(ibl_type.link_state))
        exit_target = target_tag;
    else
        exit_target = get_linked_entry(dcontext, target_tag);

    /* Set pc to the last instruction in the stub.
     * See insert_exit_stub_other_flags(), the last instruction in indirect exit stub will
     * always be a c.nop.
     */
    pc = (uint *)(stub_pc + exit_stub_size(dcontext, target_tag, f->flags) -
                  RISCV64_INSTR_COMPRESSED_SIZE);
    pc = get_stub_branch(pc) - 1;

    ASSERT(get_ibl_entry_tls_offs(dcontext, exit_target) <= (2 << 11) - 1);
    /* Format of the ld instruction:
        | imm[11:0] |  rs1  |011|  rd  |0000011|
        ^   31-20   ^ 19-15 ^   ^ 11-7 ^
     */
    /* ld a1, offs(reg_stolen) */
    *(uint *)vmcode_get_writable_addr((byte *)pc) = 0x3003 |
        get_ibl_entry_tls_offs(dcontext, exit_target) << 20 |
        (dr_reg_stolen - DR_REG_ZERO) << 15 | (DR_REG_A1 - DR_REG_ZERO) << 7;

    if (hot_patch)
        machine_cache_sync(pc, pc + 1, true);
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
