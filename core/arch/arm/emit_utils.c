/* **********************************************************
 * Copyright (c) 2014-2021 Google, Inc.  All rights reserved.
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
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* file "emit_utils.c"
 * The ARM processors does not maintain cache consistency in hardware,
 * so we need be careful about getting stale cache entries.
 */

#include "../globals.h"
#include "arch.h"
#include "instr.h"
#include "instr_create_shared.h"
#include "instrlist.h"
#include "instrument.h" /* for dr_insert_call() */

/* shorten code generation lines */
#define APP instrlist_meta_append
#define PRE instrlist_meta_preinsert
#define OPREG opnd_create_reg

/***************************************************************************/
/*                               EXIT STUB                                 */
/***************************************************************************/

/* We use two approaches to linking based on whether we can reach the
 * target from the exit cti:
 *
 *     Unlinked:
 *         b stub
 *       stub:
 *         str r0, [r10, #r0-slot]
 *         movw r0, #bottom-half-&linkstub
 *         movt r0, #top-half-&linkstub
 *         ldr pc, [r10, #fcache-return-offs]
 *         <ptr-sized slot>
 *
 *     Linked, target < 32MB away (or < 1MB for T32 cbr):
 *         b target
 *       stub:
 *         str r0, [r10, #r0-slot]
 *         movw r0, #bottom-half-&linkstub
 *         movt r0, #top-half-&linkstub
 *         ldr pc, [r10, #fcache-return-offs]
 *         <ptr-sized slot>
 *
 *     Linked, target > 32MB away (or > 1MB for T32 cbr):
 *         b stub
 *       stub:
 *         ldr pc, [pc + 12 or 14]
 *         movw r0, #bottom-half-&linkstub
 *         movt r0, #top-half-&linkstub
 *         ldr pc, [r10, #fcache-return-offs]
 *         <target>
 *
 * i#1906: the addresses from which data is loaded into the PC must be
 * 4-byte-aligned.  We arrange this by padding the body of a Thumb fragment
 * to ensure the stubs start on a 4-byte alignment.
 *
 * XXX i#1611: improve on this by allowing load-into-PC exit ctis,
 * which would give us back -indirect_stubs and -cbr_single_stub.
 *
 * XXX: we could move T32 b.cc into IT block to reach 16MB instead of 1MB.
 */

byte *
insert_relative_target(byte *pc, cache_pc target, bool hot_patch)
{
    /* FIXME i#1551: NYI on ARM.
     * We may want to refactor the calling code to remove this and only
     * use patch_branch().
     */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

byte *
insert_relative_jump(byte *pc, cache_pc target, bool hot_patch)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

static byte *
insert_spill_reg(byte *pc, fragment_t *f, reg_id_t src)
{
    ushort slot;
    ASSERT(src >= DR_REG_R0 && src <= DR_REG_R4);
    slot = TLS_REG0_SLOT + (src - DR_REG_R0) * sizeof(reg_t);
    /* str src, [r10, #src-slot] */
    if (FRAG_IS_THUMB(f->flags)) {
        *(ushort *)pc = 0xf8c0 | (dr_reg_stolen - DR_REG_R0);
        pc += THUMB_SHORT_INSTR_SIZE;
        *(ushort *)pc = ((src - DR_REG_R0) << 12) | slot;
        pc += THUMB_SHORT_INSTR_SIZE;
    } else {
        *(uint *)pc = 0xe5800000 | ((src - DR_REG_R0) << 12) |
            ((dr_reg_stolen - DR_REG_R0) << 16) | slot;
        pc += ARM_INSTR_SIZE;
    }
    return pc;
}

static byte *
insert_ldr_tls_to_pc(byte *pc, uint frag_flags, uint offs)
{
    /* ldr pc, [r10, #offs] */
    ASSERT(ALIGNED(offs, PC_LOAD_ADDR_ALIGN)); /* unpredictable unless aligned: i#1906 */
    if (FRAG_IS_THUMB(frag_flags)) {
        *(ushort *)pc = 0xf8d0 | (dr_reg_stolen - DR_REG_R0);
        pc += THUMB_SHORT_INSTR_SIZE;
        *(ushort *)pc = 0xf000 | offs;
        return pc + THUMB_SHORT_INSTR_SIZE;
    } else {
        *(uint *)pc = 0xe590f000 | ((dr_reg_stolen - DR_REG_R0) << 16) | offs;
        return pc + ARM_INSTR_SIZE;
    }
}

static byte *
insert_mov_linkstub(byte *pc, fragment_t *f, linkstub_t *l, reg_id_t dst)
{
    ptr_int_t ls = (ptr_int_t)l;
    if (FRAG_IS_THUMB(f->flags)) {
        /* movw dst, #bottom-half-&linkstub */
        *(ushort *)pc = 0xf240 | ((ls & 0xf000) >> 12) | ((ls & 0x0800) >> 1);
        pc += THUMB_SHORT_INSTR_SIZE;
        *(ushort *)pc = ((ls & 0x0700) << 4) | ((dst - DR_REG_R0) << 8) | (ls & 0xff);
        pc += THUMB_SHORT_INSTR_SIZE;
        /* movt dst, #top-half-&linkstub */
        *(ushort *)pc = 0xf2c0 | ((ls & 0xf0000000) >> 28) | ((ls & 0x08000000) >> 17);
        pc += THUMB_SHORT_INSTR_SIZE;
        *(ushort *)pc = ((ls & 0x07000000) >> 12) | ((dst - DR_REG_R0) << 8) |
            ((ls & 0xff0000) >> 16);
        pc += THUMB_SHORT_INSTR_SIZE;
    } else {
        /* movw dst, #bottom-half-&linkstub */
        *(uint *)pc =
            0xe3000000 | ((ls & 0xf000) << 4) | ((dst - DR_REG_R0) << 12) | (ls & 0xfff);
        pc += ARM_INSTR_SIZE;
        /* movt dst, #top-half-&linkstub */
        *(uint *)pc = 0xe3400000 | ((ls & 0xf0000000) >> 12) | ((dst - DR_REG_R0) << 12) |
            ((ls & 0x0fff0000) >> 16);
        pc += ARM_INSTR_SIZE;
    }
    return pc;
}

/* inserts any nop padding needed to ensure patchable branch offsets don't
 * cross cache line boundaries.  If emitting sets the offset field of all
 * instructions, else sets the translation for the added nops (for
 * recreating). If emitting and -pad_jmps_shift_{bb,trace} returns the number
 * of bytes to shift the start_pc by (this avoids putting a nop before the
 * first exit cti) else returns 0.
 */
uint
nop_pad_ilist(dcontext_t *dcontext, fragment_t *f, instrlist_t *ilist, bool emitting)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
    return 0;
}

size_t
get_fcache_return_tls_offs(dcontext_t *dcontext, uint flags)
{
    /* ARM always uses shared gencode so we ignore FRAG_DB_SHARED(flags) */
    if (TEST(FRAG_COARSE_GRAIN, flags)) {
        /* FIXME i#1575: coarse-grain NYI on ARM */
        ASSERT_NOT_IMPLEMENTED(false);
        return 0;
    } else {
        /* FIXME i#1551: add Thumb support: ARM vs Thumb gencode */
        return TLS_FCACHE_RETURN_SLOT;
    }
}

/* Emit code for the exit stub at stub_pc.  Return the size of the
 * emitted code in bytes.  This routine assumes that the caller will
 * take care of any cache synchronization necessary (though none is
 * necessary on the Pentium).
 * The stub is unlinked initially, except coarse grain indirect exits,
 * which are always linked.
 */
int
insert_exit_stub_other_flags(dcontext_t *dcontext, fragment_t *f, linkstub_t *l,
                             cache_pc stub_pc, ushort l_flags)
{
    byte *pc = (byte *)stub_pc;
    /* FIXME i#1575: coarse-grain NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(!TEST(FRAG_COARSE_GRAIN, f->flags));
    /* XXX: should we use our IR and encoder instead?  Then we could
     * share code with emit_do_syscall(), though at a perf cost.
     */
    if (LINKSTUB_DIRECT(l_flags)) {
        /* XXX: we can shrink from 16 bytes to 12 if we keep &linkstub as
         * data at the end of the stub and use a pc-rel load instead of the 2
         * mov-immed instrs (followed by the same ldr into pc):
         *    ldr r0 [pc]
         *    ldr pc, [r10, #fcache-return-offs]
         *    <&linkstub>
         * However, that may incur dcache misses w/ separate icache.
         * Another idea is to spill lr instead of r0 and use "bl fcache_return"
         * (again with &linkstub as data), though it has reachability problems.
         */
        /* str r0, [r10, #r0-slot] */
        pc = insert_spill_reg(pc, f, DR_REG_R0);
        /* movw dst, #bottom-half-&linkstub */
        /* movt dst, #top-half-&linkstub */
        pc = insert_mov_linkstub(pc, f, l, DR_REG_R0);
        /* ldr pc, [r10, #fcache-return-offs] */
        pc = insert_ldr_tls_to_pc(pc, f->flags,
                                  get_fcache_return_tls_offs(dcontext, f->flags));
        /* The final slot is a data slot only used if the target is far away. */
        pc += sizeof(app_pc);
    } else {
        /* stub starts out unlinked */
        cache_pc exit_target =
            get_unlinked_entry(dcontext, EXIT_TARGET_TAG(dcontext, f, l));
        /* str r1, [r10, #r1-slot] */
        pc = insert_spill_reg(pc, f, DR_REG_R1);
        /* movw dst, #bottom-half-&linkstub */
        /* movt dst, #top-half-&linkstub */
        pc = insert_mov_linkstub(pc, f, l, DR_REG_R1);
        /* ldr pc, [r10, #ibl-offs] */
        pc = insert_ldr_tls_to_pc(pc, f->flags,
                                  get_ibl_entry_tls_offs(dcontext, exit_target));
    }
    return (int)(pc - stub_pc);
}

bool
exit_cti_reaches_target(dcontext_t *dcontext, fragment_t *f, linkstub_t *l,
                        cache_pc target_pc)
{
    cache_pc stub_pc = (cache_pc)EXIT_STUB_PC(dcontext, f, l);
    uint bits;
    ptr_int_t disp = (target_pc - stub_pc);
    ptr_uint_t mask;
    if (FRAG_IS_THUMB(f->flags)) {
        byte *branch_pc = EXIT_CTI_PC(f, l);
        if (((*(branch_pc + 3)) & 0xd0) == 0x90) {
            /* Unconditional OP_b: 24 bits x2 */
            bits = 25;
        } else {
            /* Conditional OP_b: 20 bits x2 */
            bits = 21;
        }
    } else {
        /* 24 bits x4 */
        bits = 26;
    }
    mask = ~((1 << (bits - 1)) - 1);
    if (disp >= 0) {
        return (disp & mask) == 0;
    } else {
        return (disp & mask) == mask;
    }
}

void
patch_stub(fragment_t *f, cache_pc stub_pc, cache_pc target_pc, cache_pc target_prefix_pc,
           bool hot_patch)
{
    /* For far-away targets, we branch to the stub and use an
     * indirect branch from there:
     *        b stub
     *      stub:
     *        ldr pc, [pc + 12 or 14]
     *        movw r0, #bottom-half-&linkstub
     *        movt r0, #top-half-&linkstub
     *        ldr pc, [r10, #fcache-return-offs]
     *        <target>
     */
    /* Write target to stub's data slot */
    *(app_pc *)(stub_pc + DIRECT_EXIT_STUB_SIZE(f->flags) - DIRECT_EXIT_STUB_DATA_SZ) =
        PC_AS_JMP_TGT(FRAG_ISA_MODE(f->flags), target_pc);
    /* Clobber 1st instr of stub w/ "ldr pc, [pc + 12]" */
    if (FRAG_IS_THUMB(f->flags)) {
        uint word1 = 0xf8d0 | (DR_REG_PC - DR_REG_R0);
        /* All instrs are 4 bytes, so cur pc == start of next instr, so we have to
         * skip 3 instrs:
         */
        cache_pc tgt = stub_pc + DIRECT_EXIT_STUB_INSTR_COUNT * THUMB_LONG_INSTR_SIZE;
        uint offs = tgt - decode_cur_pc(stub_pc, FRAG_ISA_MODE(f->flags), OP_ldr, NULL);
        uint word2 = 0xf000 | offs;
        ASSERT(ALIGNED(tgt, PC_LOAD_ADDR_ALIGN)); /* unpredictable unless: i#1906 */
        /* We assume this is atomic */
        *(uint *)stub_pc = (word2 << 16) | word1; /* little-endian */
    } else {
        /* We assume this is atomic */
        *(uint *)stub_pc = 0xe590f000 | ((DR_REG_PC - DR_REG_R0) << 16) |
            /* Like for Thumb except cur pc is +8 which skips 2nd instr */
            ((DIRECT_EXIT_STUB_INSTR_COUNT - 2) * ARM_INSTR_SIZE);
    }
    if (hot_patch)
        machine_cache_sync(stub_pc, stub_pc + ARM_INSTR_SIZE, true);
}

bool
stub_is_patched(dcontext_t *dcontext, fragment_t *f, cache_pc stub_pc)
{
    if (FRAG_IS_THUMB(f->flags)) {
        return ((*stub_pc) & 0xf) == (DR_REG_PC - DR_REG_R0);
    } else {
        return (*(stub_pc + 2) & 0xf) == (DR_REG_PC - DR_REG_R0);
    }
}

void
unpatch_stub(dcontext_t *dcontext, fragment_t *f, cache_pc stub_pc, bool hot_patch)
{
    /* XXX: we're called even for a near link, so try to avoid any writes or flushes */
    if (FRAG_IS_THUMB(f->flags)) {
        if (*stub_pc == dr_reg_stolen - DR_REG_R0)
            return; /* nothing to do */
    } else {
        if (*(stub_pc + 2) == dr_reg_stolen - DR_REG_R0)
            return; /* nothing to do */
    }
    insert_spill_reg(stub_pc, f, DR_REG_R0);
    if (hot_patch)
        machine_cache_sync(stub_pc, stub_pc + ARM_INSTR_SIZE, true);
}

void
patch_branch(dr_isa_mode_t isa_mode, cache_pc branch_pc, cache_pc target_pc,
             bool hot_patch)
{
    if (isa_mode == DR_ISA_ARM_A32) {
        if (((*(branch_pc + 3)) & 0xf) == 0xa) {
            /* OP_b with 3-byte immed that's stored as >>2 */
            uint val = (*(uint *)branch_pc) & 0xff000000;
            int disp = target_pc - decode_cur_pc(branch_pc, isa_mode, OP_b, NULL);
            ASSERT(ALIGNED(disp, ARM_INSTR_SIZE));
            ASSERT(disp < 0x4000000 && disp >= -32 * 1024 * 1024); /* 26-bit max */
            val |= ((disp >> 2) & 0xffffff);
            *(uint *)branch_pc = val;
            if (hot_patch)
                machine_cache_sync(branch_pc, branch_pc + ARM_INSTR_SIZE, true);
            return;
        }
    } else if (isa_mode == DR_ISA_ARM_THUMB) {
        /* Remember that we have 2 little-endian shorts */
        if (((*(branch_pc + 1)) & 0xf0) == 0xf0 &&
            /* Match uncond and cond OP_b */
            ((*(branch_pc + 3)) & 0xc0) == 0x80) {
            if (((*(branch_pc + 3)) & 0xd0) == 0x90) {
                /* Unconditional OP_b: 3-byte immed that's stored, split up, as >>1 */
                encode_raw_jmp(isa_mode, target_pc, branch_pc, branch_pc);
            } else {
                /* Conditional OP_b: 20-bit immed */
                /* First, get the non-immed bits */
                ushort valA = (*(ushort *)branch_pc) & 0xfbc0;
                ushort valB = (*(ushort *)(branch_pc + 2)) & 0xd000;
                int disp = target_pc - decode_cur_pc(branch_pc, isa_mode, OP_b, NULL);
                ASSERT(ALIGNED(disp, THUMB_SHORT_INSTR_SIZE));
                ASSERT(disp < 0x100000 && disp >= -1 * 1024 * 1024); /* 21-bit max */
                /* A10,B11,B13,A5:0,B10:0 x2 */
                /* XXX: share with encoder's TYPE_J_b26_b11_b13_b16_b0 */
                valB |= (disp >> 1) & 0x7ff;        /* B10:0 */
                valA |= (disp >> 12) & 0x3f;        /* A5:0 */
                valB |= ((disp >> 18) & 0x1) << 13; /* B13 */
                valB |= ((disp >> 19) & 0x1) << 11; /* B11 */
                valA |= ((disp >> 20) & 0x1) << 10; /* A10 */
                *(ushort *)branch_pc = valA;
                *(ushort *)(branch_pc + 2) = valB;
            }
            if (hot_patch)
                machine_cache_sync(branch_pc, branch_pc + THUMB_LONG_INSTR_SIZE, true);
            return;
        } else {
            dcontext_t *dcontext;
            dr_isa_mode_t old_mode;
            /* Normally instr_is_cti_short_rewrite() gets the isa mode from an instr_t
             * param, but we're passing NULL.  Rather than change all its callers
             * to have to pass in an isa mode we set it here.
             * XXX: we're duplicating work in instr_is_cti_short_rewrite().
             */
            dcontext = get_thread_private_dcontext();
            dr_set_isa_mode(dcontext, isa_mode, &old_mode);
            if (instr_is_cti_short_rewrite(NULL, branch_pc)) {
                encode_raw_jmp(isa_mode, target_pc, branch_pc + CTI_SHORT_REWRITE_B_OFFS,
                               branch_pc + CTI_SHORT_REWRITE_B_OFFS);
                if (hot_patch) {
                    machine_cache_sync(branch_pc + CTI_SHORT_REWRITE_B_OFFS,
                                       branch_pc + CTI_SHORT_REWRITE_B_OFFS +
                                           THUMB_LONG_INSTR_SIZE,
                                       true);
                }
                dr_set_isa_mode(dcontext, old_mode, NULL);
                return;
            }
            dr_set_isa_mode(dcontext, old_mode, NULL);
        }
    }
    /* FIXME i#1569: add AArch64 support */
    ASSERT_NOT_IMPLEMENTED(false);
}

uint
patchable_exit_cti_align_offs(dcontext_t *dcontext, instr_t *inst, cache_pc pc)
{
    return 0; /* always aligned */
}

cache_pc
exit_cti_disp_pc(cache_pc branch_pc)
{
    /* FIXME i#1551: NYI on ARM.
     * We may want to refactor the calling code to remove this and only
     * use patch_branch().
     */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

void
link_indirect_exit_arch(dcontext_t *dcontext, fragment_t *f, linkstub_t *l,
                        bool hot_patch, app_pc target_tag)
{
    byte *stub_pc = (byte *)EXIT_STUB_PC(dcontext, f, l);
    byte *pc;
    cache_pc exit_target;
    ibl_type_t ibl_type = { 0 };
    DEBUG_DECLARE(bool is_ibl =)
    get_ibl_routine_type_ex(dcontext, target_tag, &ibl_type);
    ASSERT(is_ibl);
    if (IS_IBL_LINKED(ibl_type.link_state))
        exit_target = target_tag;
    else
        exit_target = get_linked_entry(dcontext, target_tag);
    /* We want to patch the final instr.  For Thumb it's wide. */
    ASSERT_NOT_IMPLEMENTED(DYNAMO_OPTION(indirect_stubs));
    pc = stub_pc + exit_stub_size(dcontext, target_tag, f->flags) - ARM_INSTR_SIZE;
    /* ldr pc, [r10, #ibl-offs] */
    insert_ldr_tls_to_pc(pc, f->flags, get_ibl_entry_tls_offs(dcontext, exit_target));
    /* XXX: since we need a syscall to sync, we should start out linked */
    if (hot_patch)
        machine_cache_sync(pc, pc + ARM_INSTR_SIZE, true);
}

cache_pc
indirect_linkstub_stub_pc(dcontext_t *dcontext, fragment_t *f, linkstub_t *l)
{
    cache_pc cti = EXIT_CTI_PC(f, l);
    cache_pc tgt;
    dr_isa_mode_t old_mode;
    if (!EXIT_HAS_STUB(l->flags, f->flags))
        return NULL;
    dr_set_isa_mode(dcontext, FRAG_ISA_MODE(f->flags), &old_mode);
    ASSERT(decode_raw_is_jmp(dcontext, cti));
    tgt = decode_raw_jmp_target(dcontext, cti);
    dr_set_isa_mode(dcontext, old_mode, NULL);
    return tgt;
}

cache_pc
cbr_fallthrough_exit_cti(cache_pc prev_cti_pc)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

/* This is an atomic operation with respect to a thread executing in the
 * cache (barring ifdef NATIVE_RETURN, which is now removed), for
 * inlined indirect exits the
 * unlinked path of the ibl routine detects the race condition between the
 * two patching writes and handles it appropriately unless using the
 * atomic_inlined_linking option in which case there is only one patching
 * write (since tail is duplicated)
 */
void
unlink_indirect_exit(dcontext_t *dcontext, fragment_t *f, linkstub_t *l)
{
    byte *stub_pc = (byte *)EXIT_STUB_PC(dcontext, f, l);
    byte *pc;
    cache_pc exit_target;
    ibl_code_t *ibl_code = NULL;
    ASSERT(linkstub_owned_by_fragment(dcontext, f, l));
    ASSERT(LINKSTUB_INDIRECT(l->flags));
    /* target is always the same, so if it's already unlinked, this is a nop */
    if (!TEST(LINK_LINKED, l->flags))
        return;
    ibl_code = get_ibl_routine_code(dcontext, extract_branchtype(l->flags), f->flags);
    exit_target = ibl_code->unlinked_ibl_entry;
    /* We want to patch the final instr.  For Thumb it's wide. */
    ASSERT_NOT_IMPLEMENTED(DYNAMO_OPTION(indirect_stubs));
    pc = stub_pc +
        exit_stub_size(dcontext, ibl_code->indirect_branch_lookup_routine, f->flags) -
        ARM_INSTR_SIZE;
    /* ldr pc, [r10, #ibl-offs] */
    insert_ldr_tls_to_pc(pc, f->flags, get_ibl_entry_tls_offs(dcontext, exit_target));
    machine_cache_sync(pc, pc + ARM_INSTR_SIZE, true);
}

/*******************************************************************************
 * COARSE-GRAIN FRAGMENT SUPPORT
 */

cache_pc
entrance_stub_jmp(cache_pc stub)
{
    /* FIXME i#1575: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

/* Returns whether stub is an entrance stub as opposed to a fragment
 * or a coarse indirect stub.  FIXME: if we separate coarse indirect
 * stubs from bodies we'll need to put them somewhere else, or fix up
 * decode_fragment() to be able to distinguish them in some other way
 * like first instruction tls slot.
 */
bool
coarse_is_entrance_stub(cache_pc stub)
{
    /* FIXME i#1575: coarse-grain NYI on ARM */
    return false;
}

/*###########################################################################
 *
 * fragment_t Prefixes
 *
 * Two types: indirect branch target, which restores eflags and xcx, and
 * normal prefix, which just restores xcx
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
    byte *pc = (byte *)f->start_pc;
    ASSERT(f->prefix_size == 0);
    if (use_ibt_prefix(f->flags)) {
        if (FRAG_IS_THUMB(f->flags)) {
            /* ldr r0, [r10, #r0-slot] */
            *(ushort *)pc = 0xf8d0 | (dr_reg_stolen - DR_REG_R0);
            pc += THUMB_SHORT_INSTR_SIZE;
            *(ushort *)pc = TLS_REG0_SLOT;
            pc += THUMB_SHORT_INSTR_SIZE;
        } else {
            /* ldr r0, [r10, #r0-slot] */
            *(uint *)pc =
                0xe5900000 | ((dr_reg_stolen - DR_REG_R0) << 16) | TLS_REG0_SLOT;
            pc += ARM_INSTR_SIZE;
        }
    }
    f->prefix_size = (byte)(((cache_pc)pc) - f->start_pc);
    /* make sure emitted size matches size we requested */
    ASSERT(f->prefix_size == fragment_prefix_size(f->flags));
}

/***************************************************************************/
/*             THREAD-PRIVATE/SHARED ROUTINE GENERATION                    */
/***************************************************************************/

/* helper functions for emit_fcache_enter_common */

void
append_call_exit_dr_hook(dcontext_t *dcontext, instrlist_t *ilist, bool absolute,
                         bool shared)
{
    /* i#1551: DR_HOOK is not supported on ARM */
    ASSERT_NOT_IMPLEMENTED(EXIT_DR_HOOK == NULL);
}

void
append_restore_xflags(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    APP(ilist, RESTORE_FROM_DC(dcontext, SCRATCH_REG0, XFLAGS_OFFSET));
    APP(ilist,
        INSTR_CREATE_msr(dcontext, opnd_create_reg(DR_REG_CPSR),
                         OPND_CREATE_INT_MSR_NZCVQG(), opnd_create_reg(SCRATCH_REG0)));
}

/* dcontext is in REG_DCXT. other registers can be used as scratch.
 */
void
append_restore_simd_reg(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    /* s16–s31 (d8–d15, q4–q7) are callee-saved, but we save them to be safe */
    APP(ilist,
        INSTR_CREATE_add(dcontext, opnd_create_reg(DR_REG_R1), opnd_create_reg(REG_DCXT),
                         OPND_CREATE_INT(offsetof(priv_mcontext_t, simd))));
    APP(ilist,
        INSTR_CREATE_vldm_wb(dcontext, OPND_CREATE_MEMLIST(DR_REG_R1), SIMD_REG_LIST_LEN,
                             SIMD_REG_LIST_0_15));
    APP(ilist,
        INSTR_CREATE_vldm_wb(dcontext, OPND_CREATE_MEMLIST(DR_REG_R1), SIMD_REG_LIST_LEN,
                             SIMD_REG_LIST_16_31));
}

/* Append instructions to restore gpr on fcache enter, to be executed
 * right before jump to fcache target.
 * - dcontext is in REG_DCXT
 * - DR's tls base is in dr_reg_stolen
 * - all other registers can be used as scratch, and we are using R0.
 */
void
append_restore_gpr(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    dr_isa_mode_t isa_mode = dr_get_isa_mode(dcontext);
    /* FIXME i#1573: NYI on ARM with SELFPROT_DCONTEXT */
    ASSERT_NOT_IMPLEMENTED(!TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask));
    ASSERT(dr_reg_stolen != SCRATCH_REG0);
    /* store stolen reg value into TLS slot */
    APP(ilist, RESTORE_FROM_DC(dcontext, SCRATCH_REG0, REG_OFFSET(dr_reg_stolen)));
    APP(ilist, SAVE_TO_TLS(dcontext, SCRATCH_REG0, TLS_REG_STOLEN_SLOT));

    /* Save DR's tls base into mcontext for the ldm later.
     * XXX: we just want to remove the stolen reg from the reg list,
     * so instead of having this extra store, we should provide a help
     * function to create the reg list.
     * This means that the mcontext stolen reg slot holds DR's base instead of
     * the app's value while we're in the cache, which can be confusing: but we have
     * to get the official value from TLS on signal and other transitions anyway,
     * and DR's base makes it easier to spot bugs than a prior app value.
     */
    APP(ilist, SAVE_TO_DC(dcontext, dr_reg_stolen, REG_OFFSET(dr_reg_stolen)));
    /* prepare for ldm */
    if (R0_OFFSET != 0) {
        APP(ilist,
            INSTR_CREATE_add(dcontext, opnd_create_reg(REG_DCXT),
                             opnd_create_reg(REG_DCXT), OPND_CREATE_INT(R0_OFFSET)));
    }
    /* load all regs from mcontext */
    if (isa_mode == DR_ISA_ARM_THUMB) {
        /* We can't use sp with ldm */
        APP(ilist,
            INSTR_CREATE_ldr(
                dcontext, opnd_create_reg(DR_REG_SP),
                OPND_CREATE_MEM32(REG_DCXT, sizeof(void *) * DR_REG_LIST_LENGTH_T32)));
        APP(ilist,
            INSTR_CREATE_ldr(
                dcontext, opnd_create_reg(DR_REG_LR),
                OPND_CREATE_MEM32(REG_DCXT,
                                  sizeof(void *) * (1 + DR_REG_LIST_LENGTH_T32))));
        APP(ilist,
            INSTR_CREATE_ldm(dcontext, OPND_CREATE_MEMLIST(REG_DCXT),
                             DR_REG_LIST_LENGTH_T32, DR_REG_LIST_T32));
    } else {
        APP(ilist,
            INSTR_CREATE_ldm(dcontext, OPND_CREATE_MEMLIST(REG_DCXT),
                             DR_REG_LIST_LENGTH_ARM, DR_REG_LIST_ARM));
    }
}

/* helper functions for append_fcache_return_common */

/* Append instructions to save gpr on fcache return, called after
 * append_fcache_return_prologue.
 * Assuming the execution comes from an exit stub,
 * dcontext base is held in REG_DCXT, and exit stub in r0.
 * - store all registers into dcontext's mcontext
 * - restore REG_DCXT app value from TLS slot to mcontext
 * - restore dr_reg_stolen app value from TLS slot to mcontext
 */
void
append_save_gpr(dcontext_t *dcontext, instrlist_t *ilist, bool ibl_end, bool absolute,
                generated_code_t *code, linkstub_t *linkstub, bool coarse_info)
{
    dr_isa_mode_t isa_mode = dr_get_isa_mode(dcontext);
    ASSERT_NOT_IMPLEMENTED(!absolute &&
                           !TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask));
    if (R0_OFFSET != 0) {
        APP(ilist,
            INSTR_CREATE_add(dcontext, opnd_create_reg(REG_DCXT),
                             opnd_create_reg(REG_DCXT), OPND_CREATE_INT(R0_OFFSET)));
    }
    /* save current register state to dcontext's mcontext, some are in TLS */
    if (isa_mode == DR_ISA_ARM_THUMB) {
        /* We can't use sp with stm */
        APP(ilist,
            INSTR_CREATE_stm(dcontext, OPND_CREATE_MEMLIST(REG_DCXT),
                             DR_REG_LIST_LENGTH_T32, DR_REG_LIST_T32));
        APP(ilist,
            INSTR_CREATE_str(
                dcontext,
                OPND_CREATE_MEM32(REG_DCXT, sizeof(void *) * DR_REG_LIST_LENGTH_T32),
                opnd_create_reg(DR_REG_SP)));
        APP(ilist,
            INSTR_CREATE_str(dcontext,
                             OPND_CREATE_MEM32(
                                 REG_DCXT, sizeof(void *) * (1 + DR_REG_LIST_LENGTH_T32)),
                             opnd_create_reg(DR_REG_LR)));
    } else {
        APP(ilist,
            INSTR_CREATE_stm(dcontext, OPND_CREATE_MEMLIST(REG_DCXT),
                             DR_REG_LIST_LENGTH_ARM, DR_REG_LIST_ARM));
    }

    /* app's r0 was spilled to DIRECT_STUB_SPILL_SLOT by exit stub */
    APP(ilist, RESTORE_FROM_TLS(dcontext, SCRATCH_REG1, DIRECT_STUB_SPILL_SLOT));
    if (linkstub != NULL) {
        /* FIXME i#1575: NYI for coarse-grain stub */
        ASSERT_NOT_IMPLEMENTED(false);
    } else {
        APP(ilist, SAVE_TO_DC(dcontext, SCRATCH_REG1, R0_OFFSET));
    }
    /* REG_DCXT's app value is stored in DCONTEXT_BASE_SPILL_SLOT by
     * append_prepare_fcache_return, copy it to mcontext.
     */
    APP(ilist, RESTORE_FROM_TLS(dcontext, SCRATCH_REG1, DCONTEXT_BASE_SPILL_SLOT));
    APP(ilist, SAVE_TO_DC(dcontext, SCRATCH_REG1, REG_DCXT_OFFS));
    /* dr_reg_stolen's app value is always stored in the TLS spill slot,
     * and we restore its value back to mcontext on fcache return.
     */
    APP(ilist, RESTORE_FROM_TLS(dcontext, SCRATCH_REG1, TLS_REG_STOLEN_SLOT));
    APP(ilist, SAVE_TO_DC(dcontext, SCRATCH_REG1, REG_OFFSET(dr_reg_stolen)));
}

/* dcontext base is held in REG_DCXT, and exit stub in r0.
 * GPR's are already saved.
 */
void
append_save_simd_reg(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    /* s16–s31 (d8–d15, q4–q7) are callee-saved, but we save them to be safe */
    APP(ilist,
        INSTR_CREATE_add(dcontext, opnd_create_reg(DR_REG_R1), opnd_create_reg(REG_DCXT),
                         OPND_CREATE_INT(offsetof(priv_mcontext_t, simd))));
    APP(ilist,
        INSTR_CREATE_vstm_wb(dcontext, OPND_CREATE_MEMLIST(DR_REG_R1), SIMD_REG_LIST_LEN,
                             SIMD_REG_LIST_0_15));
    APP(ilist,
        INSTR_CREATE_vstm_wb(dcontext, OPND_CREATE_MEMLIST(DR_REG_R1), SIMD_REG_LIST_LEN,
                             SIMD_REG_LIST_16_31));
}

/* scratch reg0 is holding exit stub */
void
append_save_clear_xflags(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    APP(ilist,
        INSTR_CREATE_mrs(dcontext, opnd_create_reg(SCRATCH_REG1),
                         opnd_create_reg(DR_REG_CPSR)));
    APP(ilist, SAVE_TO_DC(dcontext, SCRATCH_REG1, XFLAGS_OFFSET));
    /* There is no DF on ARM, so we do not need clear xflags. */
}

bool
append_call_enter_dr_hook(dcontext_t *dcontext, instrlist_t *ilist, bool ibl_end,
                          bool absolute)
{
    /* i#1551: DR_HOOK is not supported on ARM */
    ASSERT_NOT_IMPLEMENTED(EXIT_DR_HOOK == NULL);
    return false;
}

void
insert_save_eflags(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where, uint flags,
                   bool tls, bool absolute)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
}

void
insert_restore_eflags(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                      uint flags, bool tls, bool absolute)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
}

/* create the inlined ibl exit stub template */
byte *
emit_inline_ibl_stub(dcontext_t *dcontext, byte *pc, ibl_code_t *ibl_code,
                     bool target_trace_table)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
    return pc;
}

static void
insert_mode_change_handling(dcontext_t *dc, instrlist_t *ilist, instr_t *where,
                            reg_id_t addr_reg, reg_id_t scratch1, reg_id_t scratch2)
{
    /* Check LSB for mode changes: store the new mode in the dcontext.
     * XXX i#1551: to avoid this store every single time even when there's no
     * mode change, we'd need to generate separate thumb and arm IBL versions.
     * We'd still need to check LSB and branch.
     * Unfortunately it's hard to not do this in the IBL and instead back in DR:
     * what about signal handler, other places who decode?
     */
    ASSERT_NOT_IMPLEMENTED(!TEST(SELFPROT_DCONTEXT, DYNAMO_OPTION(protect_mask)));
    PRE(ilist, where, instr_create_restore_from_tls(dc, scratch2, TLS_DCONTEXT_SLOT));
    /* Get LSB from target address */
    PRE(ilist, where,
        INSTR_CREATE_and(dc, OPREG(scratch1), OPREG(addr_reg), OPND_CREATE_INT(1)));
    /* Get right enum value. d_r_arch_init() ensures A32 + 1 == Thumb. */
    PRE(ilist, where,
        INSTR_CREATE_add(dc, OPREG(scratch1), OPREG(scratch1),
                         OPND_CREATE_INT(DR_ISA_ARM_A32)));
    PRE(ilist, where,
        XINST_CREATE_store(
            dc, OPND_CREATE_MEM32(scratch2, (int)offsetof(dcontext_t, isa_mode)),
            OPREG(scratch1)));
    /* Now clear the bit for the table lookup */
    PRE(ilist, where,
        INSTR_CREATE_bic(dc, OPREG(addr_reg), OPREG(addr_reg), OPND_CREATE_INT(0x1)));
}

/* XXX: ideally we'd share the high-level and use XINST_CREATE or _arch routines
 * to fill in pieces like flag saving.  However, the ibl generation code for x86
 * is so complex that this needs a bunch of refactoring and likely removing support
 * for certain options before it becomes a reasonable task.  For now we go with
 * a separate lean routine that supports very few options.  Once we start filling
 * in hashtable stats we should consider refactoring and sharing.
 */
byte *
emit_indirect_branch_lookup(dcontext_t *dc, generated_code_t *code, byte *pc,
                            byte *fcache_return_pc, bool target_trace_table,
                            bool inline_ibl_head, ibl_code_t *ibl_code /* IN/OUT */)
{
    instrlist_t ilist;
    instr_t *unlinked = INSTR_CREATE_label(dc);
    instr_t *load_tag = INSTR_CREATE_label(dc);
    instr_t *compare_tag = INSTR_CREATE_label(dc);
    instr_t *not_hit = INSTR_CREATE_label(dc);
    instr_t *try_next = INSTR_CREATE_label(dc);
    instr_t *miss = INSTR_CREATE_label(dc);
    instr_t *target_delete_entry = INSTR_CREATE_label(dc);
    patch_list_t *patch = &ibl_code->ibl_patch;
    bool absolute = false; /* XXX: for SAVE_TO_DC: should eliminate it */
    IF_DEBUG(bool table_in_tls = SHARED_IB_TARGETS() &&
                 (target_trace_table || SHARED_BB_ONLY_IB_TARGETS()) &&
                 DYNAMO_OPTION(ibl_table_in_tls);)
    /* FIXME i#1551: non-table_in_tls NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(table_in_tls);
    /* FIXME i#1551: -no_indirect_stubs NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(DYNAMO_OPTION(indirect_stubs));

    instrlist_init(&ilist);
    init_patch_list(patch, PATCH_TYPE_INDIRECT_TLS);

    /* On entry we expect:
     * 1) The app target is in r2 (which is spilled to TLS_REG2_SLOT)
     * 2) The linkstub is in r1 (which is spilled to TLS_REG1_SLOT)
     */

    /* First, get some scratch regs: spill r0, and move r1 to the r3
     * slot as we don't need it if we hit.
     */
    APP(&ilist, instr_create_save_to_tls(dc, DR_REG_R1, TLS_REG3_SLOT));
    APP(&ilist, instr_create_save_to_tls(dc, DR_REG_R0, TLS_REG0_SLOT));

    /* Update dcontext->isa_mode, and then clear LSB of address */
    insert_mode_change_handling(dc, &ilist, NULL, DR_REG_R2, DR_REG_R0, DR_REG_R1);

    /* Now apply the hash, the *8, and add to the table base */
    APP(&ilist,
        INSTR_CREATE_ldr(dc, OPREG(DR_REG_R1),
                         OPND_TLS_FIELD(TLS_MASK_SLOT(ibl_code->branch_type))));
    /* We need the mask load to have Aqcuire semantics to pair with the Release in
     * update_lookuptable_tls() and avoid the reader here seeing a new mask with
     * an old table.
     */
    APP(&ilist, INSTR_CREATE_dmb(dc, OPND_CREATE_INT(DR_DMB_ISHLD)));
    APP(&ilist,
        INSTR_CREATE_and(dc, OPREG(DR_REG_R1), OPREG(DR_REG_R1), OPREG(DR_REG_R2)));
    APP(&ilist,
        INSTR_CREATE_ldr(dc, OPREG(DR_REG_R0),
                         OPND_TLS_FIELD(TLS_TABLE_SLOT(ibl_code->branch_type))));
    ASSERT(sizeof(fragment_entry_t) == 8);
    ASSERT(HASHTABLE_IBL_OFFSET(ibl_code->branch_type) < 3);
    APP(&ilist,
        INSTR_CREATE_add_shimm(
            dc, OPREG(DR_REG_R1), OPREG(DR_REG_R0), OPREG(DR_REG_R1),
            OPND_CREATE_INT(DR_SHIFT_LSL),
            OPND_CREATE_INT(3 - HASHTABLE_IBL_OFFSET(ibl_code->branch_type))));
    /* r1 now holds the fragment_entry_t* in the hashtable */

    /* load tag from fragment_entry_t* in the hashtable to r0 */
    APP(&ilist, load_tag);
    APP(&ilist,
        INSTR_CREATE_ldr(
            dc, OPREG(DR_REG_R0),
            OPND_CREATE_MEMPTR(DR_REG_R1, offsetof(fragment_entry_t, tag_fragment))));
    /* Did we hit? */
    APP(&ilist, compare_tag);
    /* Using OP_cmp requires saving the flags so we instead subtract and then cbz.
     * XXX: if we add stats, cbz might not reach.
     */
    ASSERT_NOT_IMPLEMENTED(dr_get_isa_mode(dc) == DR_ISA_ARM_THUMB);
    APP(&ilist, INSTR_CREATE_cbz(dc, opnd_create_instr(not_hit), OPREG(DR_REG_R0)));
    APP(&ilist,
        INSTR_CREATE_sub(dc, OPREG(DR_REG_R0), OPREG(DR_REG_R0), OPREG(DR_REG_R2)));
    APP(&ilist, INSTR_CREATE_cbnz(dc, opnd_create_instr(try_next), OPREG(DR_REG_R0)));

    /* Hit path */
    /* XXX: add stats via sharing code with x86 */

    /* Save next tag to TLS_REG4_SLOT in case it is needed for the
     * target_delete_entry path.
     * XXX: Instead of using a TLS slot, it will be more performant for the hit path to
     * let the relevant data be passed to the target_delete_entry code using r0 and use
     * load-into-PC for the jump below.
     */
    APP(&ilist, instr_create_save_to_tls(dc, DR_REG_R2, TLS_REG4_SLOT));

    APP(&ilist,
        INSTR_CREATE_ldr(dc, OPREG(DR_REG_R0),
                         OPND_CREATE_MEMPTR(
                             DR_REG_R1, offsetof(fragment_entry_t, start_pc_fragment))));
    APP(&ilist, instr_create_restore_from_tls(dc, DR_REG_R1, TLS_REG1_SLOT));
    APP(&ilist, instr_create_restore_from_tls(dc, DR_REG_R2, TLS_REG2_SLOT));
    APP(&ilist, INSTR_CREATE_bx(dc, OPREG(DR_REG_R0)));

    APP(&ilist, try_next);
    /* Try next entry, in case of collision.  No wraparound check is needed due to
     * the sentinel at the end.
     */
    ASSERT(offsetof(fragment_entry_t, tag_fragment) == 0);
    /* post-index load with write back */
    APP(&ilist,
        INSTR_CREATE_ldr_wbimm(
            dc, OPREG(DR_REG_R0),
            OPND_CREATE_MEMPTR(
                DR_REG_R1,
                (sizeof(fragment_entry_t) + offsetof(fragment_entry_t, tag_fragment))),
            OPND_CREATE_INT(sizeof(fragment_entry_t))));
    APP(&ilist, INSTR_CREATE_b(dc, opnd_create_instr(compare_tag)));

    APP(&ilist, not_hit);
    if (INTERNAL_OPTION(ibl_sentinel_check)) {
        /* load start_pc from fragment_entry_t* in the hashtable to r0 */
        APP(&ilist,
            INSTR_CREATE_ldr(
                dc, OPREG(DR_REG_R0),
                OPND_CREATE_MEMPTR(DR_REG_R1,
                                   offsetof(fragment_entry_t, start_pc_fragment))));
        /* To compare with an arbitrary constant we'd need a 4th scratch reg.
         * Instead we rely on the sentinel start PC being 1.
         */
        ASSERT(HASHLOOKUP_SENTINEL_START_PC == (cache_pc)PTR_UINT_1);
        APP(&ilist,
            INSTR_CREATE_sub(dc, OPREG(DR_REG_R0), OPREG(DR_REG_R0),
                             OPND_CREATE_INT8(1)));
        APP(&ilist, INSTR_CREATE_cbnz(dc, opnd_create_instr(miss), OPREG(DR_REG_R0)));
        /* Point at the first table slot and then go load and compare its tag */
        APP(&ilist,
            INSTR_CREATE_ldr(dc, OPREG(DR_REG_R1),
                             OPND_TLS_FIELD(TLS_TABLE_SLOT(ibl_code->branch_type))));
        APP(&ilist, INSTR_CREATE_b(dc, opnd_create_instr(load_tag)));
    }

    /* Target delete entry.
     * We just executed the hit path, so the app's r1 and r2 values are still in
     * their TLS slots, and &linkstub is still in the r3 slot.
     */
    APP(&ilist, target_delete_entry);
    add_patch_marker(patch, target_delete_entry, PATCH_ASSEMBLE_ABSOLUTE,
                     0 /* beginning of instruction */,
                     (ptr_uint_t *)&ibl_code->target_delete_entry);

    /* Get the next fragment tag from TLS_REG4_SLOT. Note that this already has
     * the LSB cleared, so we jump over the following sequence to avoid redoing.
     */
    APP(&ilist, instr_create_restore_from_tls(dc, DR_REG_R2, TLS_REG4_SLOT));

    /* Save &linkstub_ibl_deleted to TLS_REG3_SLOT; it will be restored to r0 below. */
    instrlist_insert_mov_immed_ptrsz(dc, (ptr_uint_t)get_ibl_deleted_linkstub(),
                                     opnd_create_reg(DR_REG_R1), &ilist, NULL, NULL,
                                     NULL);
    APP(&ilist, instr_create_save_to_tls(dc, DR_REG_R1, TLS_REG3_SLOT));

    APP(&ilist, INSTR_CREATE_b(dc, opnd_create_instr(miss)));

    /* Unlink path: entry from stub */
    APP(&ilist, unlinked);
    add_patch_marker(patch, unlinked, PATCH_ASSEMBLE_ABSOLUTE,
                     0 /* beginning of instruction */,
                     (ptr_uint_t *)&ibl_code->unlinked_ibl_entry);
    /* From stub, we need to save r0 to put the stub into */
    APP(&ilist, instr_create_save_to_tls(dc, DR_REG_R0, TLS_REG0_SLOT));
    /* We need a 2nd scratch for mode changes.  We mirror the linked path. */
    APP(&ilist, instr_create_save_to_tls(dc, DR_REG_R1, TLS_REG3_SLOT));
    /* Update dcontext->isa_mode, and then clear LSB of address */
    insert_mode_change_handling(dc, &ilist, NULL, DR_REG_R2, DR_REG_R0, DR_REG_R1);

    /* Miss path */
    APP(&ilist, miss);
    APP(&ilist, instr_create_restore_from_tls(dc, DR_REG_R1, TLS_REG3_SLOT));
    /* Put &linkstub into r0 for fcache_return */
    APP(&ilist, INSTR_CREATE_mov(dc, OPREG(DR_REG_R0), OPREG(DR_REG_R1)));
    APP(&ilist, instr_create_restore_from_tls(dc, DR_REG_R1, TLS_REG1_SLOT));
    /* Put ib tgt into dcontext->next_tag */
    insert_shared_get_dcontext(dc, &ilist, NULL, true /*save r5*/);
    APP(&ilist, SAVE_TO_DC(dc, DR_REG_R2, NEXT_TAG_OFFSET));
    APP(&ilist, instr_create_restore_from_tls(dc, DR_REG_R5, DCONTEXT_BASE_SPILL_SLOT));
    APP(&ilist, instr_create_restore_from_tls(dc, DR_REG_R2, TLS_REG2_SLOT));
    APP(&ilist,
        INSTR_CREATE_ldr(dc, OPREG(DR_REG_PC),
                         OPND_TLS_FIELD(get_fcache_return_tls_offs(dc, 0))));

    ibl_code->ibl_routine_length = encode_with_patch_list(dc, patch, &ilist, pc);
    instrlist_clear(dc, &ilist);
    return pc + ibl_code->ibl_routine_length;
}

void
relink_special_ibl_xfer(dcontext_t *dcontext, int index,
                        ibl_entry_point_type_t entry_type, ibl_branch_type_t ibl_type)
{
    generated_code_t *code;
    byte *pc, *ibl_tgt;
    if (dcontext == GLOBAL_DCONTEXT) {
        ASSERT(!special_ibl_xfer_is_thread_private()); /* else shouldn't be called */
        code = SHARED_GENCODE_MATCH_THREAD(get_thread_private_dcontext());
    } else {
        ASSERT(special_ibl_xfer_is_thread_private()); /* else shouldn't be called */
        code = THREAD_GENCODE(dcontext);
    }
    if (code == NULL) /* thread private that we don't need */
        return;
    ibl_tgt = special_ibl_xfer_tgt(dcontext, code, entry_type, ibl_type);
    ASSERT(code->special_ibl_xfer[index] != NULL);
    pc = (code->special_ibl_xfer[index] + code->special_ibl_unlink_offs[index]);

    protect_generated_code(code, WRITABLE);
    /* ldr pc, [r10, #ibl-offs] */
    /* Here we assume that our gencode is all Thumb! */
    ASSERT(DEFAULT_ISA_MODE == DR_ISA_ARM_THUMB);
    insert_ldr_tls_to_pc(pc, FRAG_THUMB, get_ibl_entry_tls_offs(dcontext, ibl_tgt));
    machine_cache_sync(pc, pc + THUMB_LONG_INSTR_SIZE, true);
    protect_generated_code(code, READONLY);
}

bool
is_jmp_rel32(byte *code_buf, app_pc app_loc, app_pc *jmp_target /* OUT */)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

bool
is_jmp_rel8(byte *code_buf, app_pc app_loc, app_pc *jmp_target /* OUT */)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

bool
fill_with_nops(dr_isa_mode_t isa_mode, byte *addr, size_t size)
{
    byte *pc = addr;
    size_t align = (isa_mode == DR_ISA_ARM_A32 ? ARM_INSTR_SIZE : THUMB_SHORT_INSTR_SIZE);
    if (!ALIGNED(addr, align) || !ALIGNED(addr + size, align)) {
        ASSERT_NOT_REACHED();
        return false;
    }
    for (pc = addr; pc < addr + size; pc += align) {
        if (isa_mode == DR_ISA_ARM_A32)
            *(uint *)pc = ARM_NOP;
        else
            *(ushort *)pc = THUMB_NOP;
    }
    return true;
}
