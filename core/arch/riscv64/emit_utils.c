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

/* TODO i#3544: Think of a better way to represent CSR in the IR, maybe as registers? */
/* Number of the fcsr register. */
#define FCSR 0x003

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
            machine_cache_sync(stub_pc, stub_pc + 4, /*flush_icache=*/true);
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
    /* If stub_pc is not aligned to 4 bytes, the first instruction will be c.nop, see
     * insert_exit_stub_other_flags(). */
    stub_pc = ALIGNED(stub_pc, 4) ? stub_pc : stub_pc + 2;
    /* At any time, at most one patching strategy will be in effect: the one for
     * intermediate fragments or the one for far fragments.
     */
    if (stub_is_patched_for_intermediate_fragment_link(dcontext, stub_pc)) {
        /* Restore the sd a0, offs(reg_stolen), see insert_exit_stub_other_flags().
         * Format of the sd instruction:
         *  | imm[11:5] |  rs2  |  rs1  |011| imm[4:0] |0100011|
         *  ^   31-25   ^ 24-20 ^ 19-15 ^   ^   11-7   ^
         */
        ASSERT(TLS_REG0_SLOT <= (2 << 11) - 1);
        *(uint *)vmcode_get_writable_addr(stub_pc) =
            (0x3023 | TLS_REG0_SLOT >> 5 << 25 | (DR_REG_A0 - DR_REG_ZERO) << 20 |
             (dr_reg_stolen - DR_REG_ZERO) << 15 | (TLS_REG0_SLOT & 0x1f) << 7);
        if (hot_patch)
            machine_cache_sync(stub_pc, stub_pc + 4, /*flush_icache=*/true);
    } else if (stub_is_patched_for_far_fragment_link(dcontext, f, stub_pc)) {
        /* Restore the data slot to fcache return address. */
        /* RISCV64 uses shared gencode. So, fcache_return routine address should be the
         * same, no matter which thread creates/unpatches the stub.
         */
        ASSERT(fcache_return_routine(dcontext) == fcache_return_routine(GLOBAL_DCONTEXT));
        /* We set hot_patch to false as we are not modifying code. */
        ATOMIC_8BYTE_ALIGNED_WRITE(get_target_pc_slot(f, stub_pc),
                                   (ptr_uint_t)fcache_return_routine(dcontext),
                                   /*hotpatch=*/false);
    }
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
    cache_pc cti = EXIT_CTI_PC(f, l);
    if (!EXIT_HAS_STUB(l->flags, f->flags))
        return NULL;
    if (decode_raw_is_jmp(dcontext, cti))
        return decode_raw_jmp_target(dcontext, cti);

    /* FIXME: i#3544: In trace, we might have direct branch to indirect linkstubs. */

    /* There should be no other types of branch to linkstubs. */
    ASSERT_NOT_REACHED();
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
    /* FIXME i#3544: coarse-grain NYI on RISCV64 */
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
    ASSERT(f->prefix_size == 0);
    /* Always use prefix on RISCV64 as there is no load to PC. */
    byte *write_start = vmcode_get_writable_addr(f->start_pc);
    byte *pc = write_start;

    instrlist_t ilist;
    instrlist_init(&ilist);

    APP(&ilist,
        INSTR_CREATE_ld(
            dcontext, opnd_create_reg(DR_REG_A0),
            opnd_create_base_disp(dr_reg_stolen, DR_REG_NULL, 0, TLS_REG0_SLOT, OPSZ_8)));
    APP(&ilist,
        INSTR_CREATE_ld(
            dcontext, opnd_create_reg(DR_REG_A1),
            opnd_create_base_disp(dr_reg_stolen, DR_REG_NULL, 0, TLS_REG1_SLOT, OPSZ_8)));

    pc = instrlist_encode(dcontext, &ilist, pc, false);
    instrlist_clear(dcontext, &ilist);

    f->prefix_size = (byte)(pc - write_start);
    ASSERT(f->prefix_size == fragment_prefix_size(f->flags));
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
    APP(ilist, RESTORE_FROM_DC(dcontext, DR_REG_A0, XFLAGS_OFFSET));
    APP(ilist,
        INSTR_CREATE_csrrw(dcontext, opnd_create_reg(DR_REG_X0),
                           opnd_create_reg(DR_REG_A0),
                           opnd_create_immed_int(FCSR, OPSZ_12b)));
}

/* dcontext is in REG_DCXT; other registers can be used as scratch.
 */
void
append_restore_simd_reg(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    /* No-op. */
}

/* Append instructions to restore gpr on fcache enter, to be executed
 * right before jump to fcache target.
 * - dcontext is in REG_DCXT
 * - DR's tls base is in dr_reg_stolen
 * - all other registers can be used as scratch, and we are using a0.
 */
void
append_restore_gpr(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    APP(ilist, RESTORE_FROM_DC(dcontext, SCRATCH_REG0, REG_OFFSET(dr_reg_stolen)));
    APP(ilist, SAVE_TO_TLS(dcontext, SCRATCH_REG0, TLS_REG_STOLEN_SLOT));

    APP(ilist, RESTORE_FROM_DC(dcontext, SCRATCH_REG0, REG_OFFSET(DR_REG_TP)));
    APP(ilist,
        SAVE_TO_TLS(dcontext, SCRATCH_REG0, os_get_app_tls_base_offset(TLS_REG_LIB)));

    for (int reg = DR_REG_X0 + 1; reg < DR_REG_X0 + 32; reg++) {
        if (reg == REG_DCXT || reg == DR_REG_TP || reg == dr_reg_stolen)
            continue;
        APP(ilist, RESTORE_FROM_DC(dcontext, reg, REG_OFFSET(reg)));
    }
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_DCXT, REG_OFFSET(REG_DCXT)));
}

/* Append instructions to save gpr on fcache return, called after
 * append_fcache_return_prologue.
 * Assuming the execution comes from an exit stub via jr a1,
 * dcontext base is held in REG_DCXT, and exit stub in a0.
 * App's a0 and a1 is stored in TLS_REG0_SLOT and TLS_REG1_SLOT
 * - store all registers into dcontext's mcontext
 * - restore REG_DCXT app value from TLS slot to mcontext
 * - restore dr_reg_stolen app value from TLS slot to mcontext
 */
void
append_save_gpr(dcontext_t *dcontext, instrlist_t *ilist, bool ibl_end, bool absolute,
                generated_code_t *code, linkstub_t *linkstub, bool coarse_info)
{
    if (linkstub != NULL) {
        /* FIXME i#3544: NYI for coarse-grain stub. */
        ASSERT_NOT_IMPLEMENTED(false);
    }

    /* a0 and a1 will always have been saved in TLS slots before executing
     * the code generated here. See, for example:
     * emit_do_syscall_common, emit_indirect_branch_lookup, handle_sigreturn,
     * insert_exit_stub_other_flags, execute_handler_from_{cache,dispatch},
     * transfer_from_sig_handler_to_fcache_return
     */
    for (int reg = DR_REG_X0 + 1; reg < DR_REG_X0 + 32; reg++) {
        if (reg == DR_REG_A0 || reg == DR_REG_A1 || reg == REG_DCXT || reg == DR_REG_TP ||
            reg == dr_reg_stolen)
            continue;
        APP(ilist, SAVE_TO_DC(dcontext, reg, REG_OFFSET(reg)));
    }

    /* We cannot use SCRATCH_REG0 here as a scratch, as it's holding the last_exit,
     * see insert_exit_stub_other_flags() */
    APP(ilist, RESTORE_FROM_TLS(dcontext, SCRATCH_REG1, TLS_REG0_SLOT));
    APP(ilist, SAVE_TO_DC(dcontext, SCRATCH_REG1, REG_OFFSET(DR_REG_A0)));

    APP(ilist, RESTORE_FROM_TLS(dcontext, SCRATCH_REG1, TLS_REG1_SLOT));
    APP(ilist, SAVE_TO_DC(dcontext, SCRATCH_REG1, REG_OFFSET(DR_REG_A1)));

    /* REG_DCXT's app value is stored in DCONTEXT_BASE_SPILL_SLOT by
     * append_prepare_fcache_return, so copy it to mcontext.
     */
    APP(ilist, RESTORE_FROM_TLS(dcontext, SCRATCH_REG1, DCONTEXT_BASE_SPILL_SLOT));
    APP(ilist, SAVE_TO_DC(dcontext, SCRATCH_REG1, REG_DCXT_OFFS));

    /* App values of dr_reg_stolen and tp are always stored in the TLS spill slots,
     * and we restore their values back to mcontext on fcache return.
     */
    APP(ilist, RESTORE_FROM_TLS(dcontext, SCRATCH_REG1, TLS_REG_STOLEN_SLOT));
    APP(ilist, SAVE_TO_DC(dcontext, SCRATCH_REG1, REG_OFFSET(dr_reg_stolen)));

    APP(ilist,
        RESTORE_FROM_TLS(dcontext, SCRATCH_REG1,
                         os_get_app_tls_base_offset(TLS_REG_LIB)));
    APP(ilist, SAVE_TO_DC(dcontext, SCRATCH_REG1, REG_OFFSET(DR_REG_TP)));
}

/* dcontext base is held in REG_DCXT, and exit stub in X0.
 * GPR's are already saved.
 */
void
append_save_simd_reg(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    /* No-op. */
}

/* Scratch reg0 is holding exit stub. */
void
append_save_clear_xflags(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    APP(ilist,
        INSTR_CREATE_csrrs(dcontext, opnd_create_reg(DR_REG_A1),
                           opnd_create_reg(DR_REG_X0),
                           opnd_create_immed_int(FCSR, OPSZ_12b)));
    APP(ilist, SAVE_TO_DC(dcontext, DR_REG_A1, XFLAGS_OFFSET));
}

bool
append_call_enter_dr_hook(dcontext_t *dcontext, instrlist_t *ilist, bool ibl_end,
                          bool absolute)
{
    /* i#3544: DR_HOOK is not supported on RISC-V */
    ASSERT_NOT_IMPLEMENTED(EXIT_DR_HOOK == NULL);
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

bool
instr_is_ibl_hit_jump(instr_t *instr)
{
    /* jr a0 */
    return instr_get_opcode(instr) == OP_jalr &&
        opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_X0 &&
        opnd_get_reg(instr_get_target(instr)) == DR_REG_A0;
}

byte *
emit_indirect_branch_lookup(dcontext_t *dc, generated_code_t *code, byte *pc,
                            byte *fcache_return_pc, bool target_trace_table,
                            bool inline_ibl_head, ibl_code_t *ibl_code /* IN/OUT */)
{
    bool absolute = false; /* Used by SAVE_TO_DC. */
    instrlist_t ilist;
    instrlist_init(&ilist);
    patch_list_t *patch = &ibl_code->ibl_patch;
    init_patch_list(patch, PATCH_TYPE_INDIRECT_TLS);

    instr_t *load_tag = INSTR_CREATE_label(dc);
    instr_t *compare_tag = INSTR_CREATE_label(dc);
    instr_t *try_next = INSTR_CREATE_label(dc);
    instr_t *miss = INSTR_CREATE_label(dc);
    instr_t *not_hit = INSTR_CREATE_label(dc);
    instr_t *target_delete_entry = INSTR_CREATE_label(dc);
    instr_t *unlinked = INSTR_CREATE_label(dc);

    /* On entry we expect:
     *     a0: link_stub entry
     *     a1: scratch reg, arrived from jr a1
     *     a2: indirect branch target
     *     TLS_REG0_SLOT: app's a0
     *     TLS_REG1_SLOT: app's a1
     *     TLS_REG2_SLOT: app's a2
     *     TLS_REG3_SLOT: scratch space
     * There are following entries with the same context:
     *     indirect_branch_lookup
     *     unlink_stub_entry
     * target_delete_entry:
     *     a0: scratch
     *     a1: table entry pointer from ibl lookup hit path
     *     a2: app's a2
     *     TLS_REG0_SLOT: app's a0
     *     TLS_REG1_SLOT: app's a1
     *     TLS_REG2_SLOT: app's a2
     * On miss exit we output:
     *     a0: the dcontext->last_exit
     *     a1: jr a1
     *     a2: app's a2
     *     TLS_REG0_SLOT: app's a0 (recovered by fcache_return)
     *     TLS_REG1_SLOT: app's a1 (recovered by fcache_return)
     * On hit exit we output:
     *     a0: fragment_start_pc (points to the fragment prefix)
     *     a1: scratch reg
     *     a2: app's a2
     *     TLS_REG0_SLOT: app's a0 (recovered by fragment_prefix)
     *     TLS_REG1_SLOT: app's a1 (recovered by fragment_prefix)
     */

    /* Spill a0. */
    APP(&ilist, instr_create_save_to_tls(dc, DR_REG_A0, TLS_REG3_SLOT));

    /* Load the hash mask into scratch register a1, which will be used in the hash
     * function.
     */
    APP(&ilist,
        INSTR_CREATE_ld(dc, opnd_create_reg(DR_REG_A1),
                        opnd_create_base_disp(dr_reg_stolen, DR_REG_NULL, 0,
                                              TLS_MASK_SLOT(ibl_code->branch_type),
                                              OPSZ_8)));

    /* Memory barrier for the hash mask. We need a barrier to ensure we see updates
     * properly.
     * fence rw, rw
     */
    APP(&ilist,
        INSTR_CREATE_fence(dc, opnd_create_immed_int(0x3, OPSZ_4b),
                           opnd_create_immed_int(0x3, OPSZ_4b),
                           opnd_create_immed_int(0x0, OPSZ_4b)));

    /* XXX i#6393: Indirect branch lookuptable should have barriers too. */
    /* Load lookup table base into a0. */
    APP(&ilist,
        INSTR_CREATE_ld(dc, opnd_create_reg(DR_REG_A0),
                        opnd_create_base_disp(dr_reg_stolen, DR_REG_NULL, 0,
                                              TLS_TABLE_SLOT(ibl_code->branch_type),
                                              OPSZ_8)));

    /* The hash "function":
     * a1: hash mask
     * a2: indirect branch target
     */
    APP(&ilist,
        INSTR_CREATE_and(dc, opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A1),
                         opnd_create_reg(DR_REG_A2)));

    /* Now, a1 holds the hash table index, use slli+add to get the table entry. */
    ASSERT(4 - HASHTABLE_IBL_OFFSET(ibl_code->branch_type) >= 0);
    if (4 - HASHTABLE_IBL_OFFSET(ibl_code->branch_type) > 0) {
        APP(&ilist,
            INSTR_CREATE_slli(
                dc, opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A1),
                opnd_create_immed_int(4 - HASHTABLE_IBL_OFFSET(ibl_code->branch_type),
                                      OPSZ_6b)));
    }
    APP(&ilist,
        INSTR_CREATE_add(dc, opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A0),
                         opnd_create_reg(DR_REG_A1)));

    /* Jump back from sentinel when wraparound is needed. */
    APP(&ilist, load_tag);

    /* a1: table entry (fragment_entry_t*).
     * Load tag_fragment from fragment_entry_t* into a0.
     */
    APP(&ilist,
        INSTR_CREATE_ld(
            dc, opnd_create_reg(DR_REG_A0),
            OPND_CREATE_MEMPTR(DR_REG_A1, offsetof(fragment_entry_t, tag_fragment))));

    /* Jump back from collision. */
    APP(&ilist, compare_tag);

    /* a0: tag_fragment
     * a2: indirect branch target
     * Did we hit?
     */
    APP(&ilist,
        INSTR_CREATE_beq(dc, opnd_create_instr(not_hit), opnd_create_reg(DR_REG_A0),
                         opnd_create_reg(DR_REG_X0)));
    /* We hit, but did it collide? */
    APP(&ilist,
        INSTR_CREATE_bne(dc, opnd_create_instr(try_next), opnd_create_reg(DR_REG_A0),
                         opnd_create_reg(DR_REG_A2)));

    /* No, so we found the answer.
     * App's original values of a0 and a1 are already in respective TLS slots, and
     * will be restored by the fragment prefix.
     */

    /* Recover app's original a2. */
    APP(&ilist, instr_create_restore_from_tls(dc, DR_REG_A2, TLS_REG2_SLOT));

    /* Load the answer into a0 */
    APP(&ilist,
        INSTR_CREATE_ld(dc, opnd_create_reg(DR_REG_A0),
                        OPND_CREATE_MEMPTR(
                            DR_REG_A1, offsetof(fragment_entry_t, start_pc_fragment))));
    /* jr a0
     * (keep in sync with instr_is_ibl_hit_jump())
     */
    APP(&ilist, XINST_CREATE_jump_reg(dc, opnd_create_reg(DR_REG_A0)));

    APP(&ilist, try_next);

    /* Try next entry, in case of collision. No wraparound check is needed
     * because of the sentinel at the end.
     */
    /* TODO i#3544: Immediate size should be auto-figured-out by the IR. */
    APP(&ilist,
        INSTR_CREATE_addi(dc, opnd_create_reg(DR_REG_A1), opnd_create_reg(DR_REG_A1),
                          opnd_create_immed_int(sizeof(fragment_entry_t), OPSZ_12b)));
    APP(&ilist,
        INSTR_CREATE_ld(
            dc, opnd_create_reg(DR_REG_A0),
            OPND_CREATE_MEMPTR(DR_REG_A1, offsetof(fragment_entry_t, tag_fragment))));

    /* Compare again. */
    APP(&ilist,
        INSTR_CREATE_jal(dc, opnd_create_reg(DR_REG_X0), opnd_create_instr(compare_tag)));

    APP(&ilist, not_hit);

    if (INTERNAL_OPTION(ibl_sentinel_check)) {
        /* Load start_pc from fragment_entry_t* in the hashtable to a0. */
        APP(&ilist,
            XINST_CREATE_load(
                dc, opnd_create_reg(DR_REG_A0),
                OPND_CREATE_MEMPTR(DR_REG_A1,
                                   offsetof(fragment_entry_t, start_pc_fragment))));
        /* To compare with an arbitrary constant we'd need a 4th scratch reg.
         * Instead we rely on the sentinel start PC being 1.
         */
        ASSERT(HASHLOOKUP_SENTINEL_START_PC == (cache_pc)PTR_UINT_1);
        APP(&ilist,
            XINST_CREATE_sub(dc, opnd_create_reg(DR_REG_A0), OPND_CREATE_INT8(1)));
        APP(&ilist,
            INSTR_CREATE_bne(dc, opnd_create_instr(miss), opnd_create_reg(DR_REG_A0),
                             opnd_create_reg(DR_REG_X0)));

        /* Point at the first table slot and then go load and compare its tag */
        APP(&ilist,
            XINST_CREATE_load(dc, opnd_create_reg(DR_REG_A1),
                              OPND_CREATE_MEMPTR(dr_reg_stolen,
                                                 TLS_TABLE_SLOT(ibl_code->branch_type))));
        APP(&ilist,
            INSTR_CREATE_jal(dc, opnd_create_reg(DR_REG_X0),
                             opnd_create_instr(load_tag)));
    }

    /* Target delete entry */
    APP(&ilist, target_delete_entry);
    add_patch_marker(patch, target_delete_entry, PATCH_ASSEMBLE_ABSOLUTE,
                     0 /* beginning of instruction */,
                     (ptr_uint_t *)&ibl_code->target_delete_entry);

    /* Load next_tag from table entry. */
    APP(&ilist,
        INSTR_CREATE_ld(
            dc, opnd_create_reg(DR_REG_A2),
            OPND_CREATE_MEMPTR(DR_REG_A1, offsetof(fragment_entry_t, tag_fragment))));

    /* Store &linkstub_ibl_deleted in a0, instead of last exit linkstub by skipped
     * code below.
     */
    instrlist_insert_mov_immed_ptrsz(dc, (ptr_uint_t)get_ibl_deleted_linkstub(),
                                     opnd_create_reg(DR_REG_A0), &ilist, NULL, NULL,
                                     NULL);
    APP(&ilist,
        INSTR_CREATE_jal(dc, opnd_create_reg(DR_REG_X0), opnd_create_instr(unlinked)));

    APP(&ilist, miss);

    /* Recover the dcontext->last_exit to a0. */
    APP(&ilist, instr_create_restore_from_tls(dc, DR_REG_A0, TLS_REG3_SLOT));

    /* Unlink path: entry from stub. */
    APP(&ilist, unlinked);
    add_patch_marker(patch, unlinked, PATCH_ASSEMBLE_ABSOLUTE,
                     0 /* beginning of instruction */,
                     (ptr_uint_t *)&ibl_code->unlinked_ibl_entry);

    /* Put ib tgt into dcontext->next_tag. */
    insert_shared_get_dcontext(dc, &ilist, NULL, true);
    APP(&ilist, SAVE_TO_DC(dc, DR_REG_A2, NEXT_TAG_OFFSET));
    APP(&ilist, instr_create_restore_from_tls(dc, DR_REG_A5, DCONTEXT_BASE_SPILL_SLOT));
    APP(&ilist, instr_create_restore_from_tls(dc, DR_REG_A2, TLS_REG2_SLOT));

    /* Load the fcache_return into a1. */
    APP(&ilist,
        INSTR_CREATE_ld(dc, opnd_create_reg(DR_REG_A1),
                        OPND_TLS_FIELD(TLS_FCACHE_RETURN_SLOT)));
    /* jr a1 */
    APP(&ilist, XINST_CREATE_jump_reg(dc, opnd_create_reg(DR_REG_A1)));

    ibl_code->ibl_routine_length = encode_with_patch_list(dc, patch, &ilist, pc);
    instrlist_clear(dc, &ilist);
    return pc + ibl_code->ibl_routine_length;
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

/* Having only one thread register tp shared between app and DR, we steal a register for
 * DR's TLS base in the code cache, and store DR's TLS base into a private lib's TLS slot
 * for accessing in C code. On entering the code cache (fcache_enter):
 * - grab gen routine's parameter dcontext and put it into REG_DCXT
 * - check for pending signals
 * - load DR's TLS base into dr_reg_stolen from privlib's TLS
 */
void
append_fcache_enter_prologue(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    ASSERT_NOT_IMPLEMENTED(!absolute &&
                           !TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask));

    instr_t *no_signals = INSTR_CREATE_label(dcontext);

    /* Save callee-saved reg in case we return for a signal. */
    APP(ilist,
        XINST_CREATE_move(dcontext, opnd_create_reg(DR_REG_A1),
                          opnd_create_reg(REG_DCXT)));

    /* Grab gen routine's parameter dcontext and put it into REG_DCXT */
    APP(ilist,
        XINST_CREATE_move(dcontext, opnd_create_reg(REG_DCXT),
                          opnd_create_reg(DR_REG_A0)));
    APP(ilist,
        INSTR_CREATE_lb(dcontext, opnd_create_reg(DR_REG_A2),
                        OPND_DC_FIELD(absolute, dcontext, OPSZ_1, SIGPENDING_OFFSET)));
    APP(ilist,
        INSTR_CREATE_bge(dcontext, opnd_create_instr(no_signals),
                         opnd_create_reg(DR_REG_ZERO), opnd_create_reg(DR_REG_A2)));

    /* Restore callee-saved reg. */
    APP(ilist,
        XINST_CREATE_move(dcontext, opnd_create_reg(REG_DCXT),
                          opnd_create_reg(DR_REG_A1)));

    /* Return back to dispatch if we have pending signals. */
    APP(ilist, XINST_CREATE_jump_reg(dcontext, opnd_create_reg(DR_REG_RA)));
    APP(ilist, no_signals);

    /* Set up stolen reg: load DR's TLS base to dr_reg_stolen. */
    APP(ilist,
        XINST_CREATE_load(dcontext, opnd_create_reg(dr_reg_stolen),
                          OPND_CREATE_MEMPTR(DR_REG_TP, DR_TLS_BASE_OFFSET)));
}
