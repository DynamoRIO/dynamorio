/* **********************************************************
 * Copyright (c) 2010-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2000-2001 Hewlett-Packard Company */

/* file "emit_utils.c"
 * The Pentium processors maintain cache consistency in hardware, so we don't
 * worry about getting stale cache entries.
 */

#include "../globals.h"
#include "../fragment.h"
#include "../fcache.h"
#include "arch.h"
#include "instr.h"
#include "instr_create_shared.h"
#include "instrlist.h"
#include "instrument.h" /* for dr_insert_call() */

#include "decode_private.h"

#define PRE instrlist_meta_preinsert
#define APP instrlist_meta_append

/***************************************************************************/
/*                               EXIT STUB                                 */
/***************************************************************************/

/*
direct branch exit_stub:
   5x8  mov   %xax, xax_offs(&dcontext) or tls
   <we used to support PROFILE_LINKCOUNT with a counter inc here but no more>
   5x10 mov   &linkstub, %xax
    5   jmp   target addr

indirect branch exit_stub (only used if -indirect_stubs):
   6x9  mov   %xbx, xbx_offs(&dcontext) or tls
   5x11 mov   &linkstub, %xbx
    5   jmp   indirect_branch_lookup

indirect branches use xbx so that the flags can be saved into xax using
the lahf instruction!
xref PR 249775 on lahf support on x64.

also see emit_inline_ibl_stub() below

*/

/* macro to make it easier to get offsets of fields that are in
 * a different memory space with self-protection
 */
#define UNPROT_OFFS(dcontext, offs)                                            \
    (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask)                      \
         ? (((ptr_uint_t)((dcontext)->upcontext.separate_upcontext)) + (offs)) \
         : (((ptr_uint_t)(dcontext)) + (offs)))

/* The write that inserts the relative target is done atomically so this
 * function is safe with respect to a thread executing the code containing
 * this target, presuming that the code in both the before and after states
 * is valid, and that [pc, pc+4) does not cross a cache line.
 * For x64 this routine only works for 32-bit reachability.  If further
 * reach is needed the caller must use indirection.  Xref PR 215395.
 */
byte *
insert_relative_target(byte *pc, cache_pc target, bool hot_patch)
{
    /* insert 4-byte pc-relative offset from the beginning of the next instruction
     */
    int value = (int)(ptr_int_t)(target - pc - 4);
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_int(target - pc - 4)));
    ATOMIC_4BYTE_WRITE(vmcode_get_writable_addr(pc), value, hot_patch);
    pc += 4;
    return pc;
}

byte *
insert_relative_jump(byte *pc, cache_pc target, bool hot_patch)
{
    int value;
    ASSERT(pc != NULL);
    *(vmcode_get_writable_addr(pc)) = JMP_OPCODE;
    pc++;

    /* test that we aren't crossing a cache line boundary */
    CHECK_JMP_TARGET_ALIGNMENT(pc, 4, hot_patch);
    /* We don't need to be atomic, so don't use insert_relative_target. */
    value = (int)(ptr_int_t)(target - pc - 4);
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_int(target - pc - 4)));
    *(int *)(vmcode_get_writable_addr(pc)) = value;
    pc += 4;
    return pc;
}

bool
exit_cti_reaches_target(dcontext_t *dcontext, fragment_t *f, linkstub_t *l,
                        cache_pc target_pc)
{
    /* Our reachability model assumes cache is all self-reachable */
    return true;
}

void
patch_stub(fragment_t *f, cache_pc stub_pc, cache_pc target_pc, cache_pc target_prefix_pc,
           bool hot_patch)
{
    /* x86 doesn't use this approach to linking */
    ASSERT_NOT_REACHED();
}

bool
stub_is_patched(dcontext_t *dcontext, fragment_t *f, cache_pc stub_pc)
{
    /* x86 doesn't use this approach to linking */
    return false;
}

void
unpatch_stub(dcontext_t *dcontext, fragment_t *f, cache_pc stub_pc, bool hot_patch)
{
    /* x86 doesn't use this approach to linking: nothing to do */
}

/* Patch the (direct) branch at branch_pc so it branches to target_pc
 * The write that actually patches the branch is done atomically so this
 * function is safe with respect to a thread executing this branch presuming
 * that both the before and after targets are valid and that [pc, pc+4) does
 * not cross a cache line.
 */
void
patch_branch(dr_isa_mode_t isa_mode, cache_pc branch_pc, cache_pc target_pc,
             bool hot_patch)
{
    cache_pc byte_ptr = exit_cti_disp_pc(branch_pc);
    insert_relative_target(byte_ptr, target_pc, hot_patch);
}

/* Checks patchable exit cti for proper alignment for patching. If it's
 * properly aligned returns 0, else returns the number of bytes it would
 * need to be forward shifted to be properly aligned */
uint
patchable_exit_cti_align_offs(dcontext_t *dcontext, instr_t *inst, cache_pc pc)
{
    /* all our exit cti's currently use 4 byte offsets */
    /* FIXME : would be better to use a instr_is_cti_long or some such
     * also should check for addr16 flag (we shouldn't have any prefixes) */
    ASSERT((instr_is_cti(inst) && !instr_is_cti_short(inst) &&
            !TESTANY(~(PREFIX_JCC_TAKEN | PREFIX_JCC_NOT_TAKEN | PREFIX_PRED_MASK),
                     instr_get_prefixes(inst))) ||
           instr_is_cti_short_rewrite(inst, NULL));
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(
        ALIGN_SHIFT_SIZE(pc + instr_length(dcontext, inst) - CTI_PATCH_SIZE,
                         CTI_PATCH_SIZE, PAD_JMPS_ALIGNMENT))));
    return (uint)ALIGN_SHIFT_SIZE(pc + instr_length(dcontext, inst) - CTI_PATCH_SIZE,
                                  CTI_PATCH_SIZE, PAD_JMPS_ALIGNMENT);
}

/* make sure to keep in sync w/ instr_raw_is_tls_spill() */
static cache_pc
insert_spill_or_restore(dcontext_t *dcontext, cache_pc pc, uint flags, bool spill,
                        bool shared, reg_id_t reg, ushort tls_offs, uint dc_offs,
                        bool require_addr16)
{
    DEBUG_DECLARE(cache_pc start_pc;)
    pc = vmcode_get_writable_addr(pc);
    IF_DEBUG(start_pc = pc);
    byte opcode = ((reg == REG_XAX) ? (spill ? MOV_XAX2MEM_OPCODE : MOV_MEM2XAX_OPCODE)
                                    : (spill ? MOV_REG2MEM_OPCODE : MOV_MEM2REG_OPCODE));
#ifdef X64
    /* for x64, shared and private fragments all use tls, even for 32-bit code */
    shared = true;
    if (!FRAG_IS_32(flags)) {
        /* mov %rbx, gs:os_tls_offset(tls_offs) */
        if (reg == REG_XAX) {
            /* shorter to use 0xa3 w/ addr32 prefix than 0x89/0x8b w/ sib byte */
            /* FIXME: PR 209709: test perf and remove if outweighs space */
            *pc = ADDR_PREFIX_OPCODE;
            pc++;
        }
        *pc = TLS_SEG_OPCODE;
        pc++;
        *pc = REX_PREFIX_BASE_OPCODE | REX_PREFIX_W_OPFLAG;
        pc++;
        *pc = opcode;
        pc++;
        if (reg != REG_XAX) {
            /* 0x1c for rbx, 0x0c for rcx, 0x04 for rax */
            *pc = MODRM_BYTE(0 /*mod*/, reg_get_bits(reg), 4 /*rm*/);
            pc++;
            *pc = SIB_DISP32;
            pc++; /* sib byte: abs addr */
        }
        *((uint *)pc) = (uint)os_tls_offset(tls_offs);
        pc += 4;
    } else
#endif /* X64 */
        if (shared) {
            /* mov %ebx, fs:os_tls_offset(tls_offs) */
            /* trying hard to keep the size of the stub 5 for eax, 6 else */
            /* FIXME: case 5231 when staying on trace space is better,
             * when going through this to the IBL routine speed asks for
             * not adding the prefix.
             */
            bool addr16 = (require_addr16 || use_addr_prefix_on_short_disp());
            if (addr16) {
                *pc = ADDR_PREFIX_OPCODE;
                pc++;
            }
            *pc = TLS_SEG_OPCODE;
            pc++;
            *pc = opcode;
            pc++;
            if (reg != REG_XAX) {
                /* 0x1e for ebx, 0x0e for ecx, 0x06 for eax
                 * w/o addr16 those are 0x1d, 0x0d, 0x05
                 */
                *pc = MODRM_BYTE(0 /*mod*/, reg_get_bits(reg), addr16 ? 6 : 5 /*rm*/);
                pc++;
            }
            if (addr16) {
                *((ushort *)pc) = os_tls_offset(tls_offs);
                pc += 2;
            } else {
                *((uint *)pc) = os_tls_offset(tls_offs);
                pc += 4;
            }
        } else {
            /* mov %ebx,((int)&dcontext)+dc_offs */
            *pc = opcode;
            pc++;
            if (reg != REG_XAX) {
                /* 0x1d for ebx, 0x0d for ecx, 0x05 for eax */
                *pc = MODRM_BYTE(0 /*mod*/, reg_get_bits(reg), 5 /*rm*/);
                pc++;
            }
            IF_X64(ASSERT_NOT_IMPLEMENTED(false));
            *((uint *)pc) = (uint)(ptr_uint_t)UNPROT_OFFS(dcontext, dc_offs);
            pc += 4;
        }
    ASSERT(IF_X64_ELSE(false, !shared) ||
           (pc - start_pc) ==
               (reg == REG_XAX ? SIZE_MOV_XAX_TO_TLS(flags, require_addr16)
                               : SIZE_MOV_XBX_TO_TLS(flags, require_addr16)));
    ASSERT(IF_X64_ELSE(false, !shared) || !spill || reg == REG_XAX ||
           instr_raw_is_tls_spill(start_pc, reg, tls_offs));
    return vmcode_get_executable_addr(pc);
}

/* instr_raw_is_tls_spill() matches the exact sequence of bytes inserted here */
static byte *
insert_jmp_to_ibl(byte *pc, fragment_t *f, linkstub_t *l, cache_pc exit_target,
                  dcontext_t *dcontext)
{
#ifdef WINDOWS
    bool spill_xbx_to_fs = FRAG_DB_SHARED(f->flags) ||
        (is_shared_syscall_routine(dcontext, exit_target) &&
         DYNAMO_OPTION(shared_fragment_shared_syscalls));
#else
    bool spill_xbx_to_fs = FRAG_DB_SHARED(f->flags);
#endif
    ASSERT(linkstub_owned_by_fragment(dcontext, f, l));
    /* we use XBX to hold the linkstub pointer for IBL routines,
       note that direct stubs use XAX for linkstub pointer */
#ifdef WINDOWS
    if (INTERNAL_OPTION(shared_syscalls_fastpath) &&
        is_shared_syscall_routine(dcontext, exit_target)) {
        /* jmp <exit_target> */
        pc = insert_relative_jump(pc, exit_target, NOT_HOT_PATCHABLE);
        return pc;
    } else
#endif
        pc = insert_spill_or_restore(dcontext, pc, f->flags, true /*spill*/,
                                     spill_xbx_to_fs, REG_XBX, INDIRECT_STUB_SPILL_SLOT,
                                     XBX_OFFSET, true);

    /* Switch to the writable view for the raw stores below. */
    pc = vmcode_get_writable_addr(pc);
    /* mov $linkstub_ptr,%xbx */
#ifdef X64
    if (!FRAG_IS_32(f->flags)) {
        *pc = REX_PREFIX_BASE_OPCODE | REX_PREFIX_W_OPFLAG;
        pc++;
    }
#endif
    *pc = MOV_IMM2XBX_OPCODE;
    pc++;
#ifdef WINDOWS
    if (DYNAMO_OPTION(shared_syscalls) &&
        is_shared_syscall_routine(dcontext, exit_target)) {
        /* FIXME We could reduce mem usage by not allocating a linkstub for
         * this exit since it's never referenced.
         */
        LOG(THREAD, LOG_LINKS, 4, "\tF%d using %s shared syscalls link stub\n", f->id,
            TEST(FRAG_IS_TRACE, f->flags) ? "trace" : "bb");
        l = (linkstub_t *)(TEST(FRAG_IS_TRACE, f->flags)
                               ? get_shared_syscalls_trace_linkstub()
                               : get_shared_syscalls_bb_linkstub());
    }
#endif
    if (TEST(FRAG_COARSE_GRAIN, f->flags)) {
        /* FIXME: once we separate these we should switch to 15-byte w/
         * store-to-mem instead of in a spilled xbx, to use same
         * slots as coarse direct stubs
         */
        /* There is no linkstub_t so we store source tag instead */
        *((ptr_uint_t *)pc) = (ptr_uint_t)f->tag;
        pc += sizeof(f->tag);
        /* FIXME: once we separate the indirect stubs out we will need
         * a 15-byte stub .  For that we should simply store the
         * source cti directly into a tls slot.  For now though we inline
         * the stubs and spill xbx.
         */
    } else {
        *((ptr_uint_t *)pc) = (ptr_uint_t)l;
        pc += sizeof(l);
    }
    pc = vmcode_get_executable_addr(pc);

    /* jmp <exit_target> */
    pc = insert_relative_jump(pc, exit_target, NOT_HOT_PATCHABLE);
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
    instr_t *inst;
    uint offset = 0;
    int first_patch_offset = -1;
    uint start_shift = 0;
    /* if emitting prefix_size isn't set up yet */
    cache_pc starting_pc = f->start_pc + fragment_prefix_size(f->flags);
    ASSERT(emitting || f->prefix_size == fragment_prefix_size(f->flags));

    ASSERT(PAD_FRAGMENT_JMPS(f->flags)); /* shouldn't call this otherwise */

    for (inst = instrlist_first(ilist); inst != NULL; inst = instr_get_next(inst)) {
        /* don't support non exit cti patchable instructions yet */
        ASSERT_NOT_IMPLEMENTED(!TEST(INSTR_HOT_PATCHABLE, inst->flags));
        if (instr_is_exit_cti(inst)) {
            /* see if we need to be able to patch this instruction */
            if (is_exit_cti_patchable(dcontext, inst, f->flags)) {
                /* See if we are crossing a cache line.  Offset is the start of
                 * the current instr.
                 */
                uint nop_length =
                    patchable_exit_cti_align_offs(dcontext, inst, starting_pc + offset);
                LOG(THREAD, LOG_INTERP, 4, "%s: F%d @" PFX " cti shift needed: %d\n",
                    __FUNCTION__, f->id, starting_pc + offset, nop_length);
                if (first_patch_offset < 0)
                    first_patch_offset = offset;
                if (nop_length > 0) {
                    /* crosses cache line, nop pad */
                    /* instead of inserting a nop, shift the starting pc if
                     * we are within 1 cache line of the first patchable offset
                     * (this covers the case of a conditional branch which
                     * mangles to two patchable exits and is still safe since
                     * they are less then a cache line apart) */
                    if (PAD_JMPS_SHIFT_START(f->flags) &&
                        offset + instr_length(dcontext, inst) - first_patch_offset <
                            PAD_JMPS_ALIGNMENT) {
                        ASSERT(start_shift == 0); /* should only shift once */
                        start_shift = nop_length;
                        /* adjust the starting_pc, all previously checked
                         * instructions should be fine since we are still
                         * within the same cache line as the first patchable
                         * offset */
                        starting_pc += nop_length;
                    } else {
                        instr_t *nop_inst = INSTR_CREATE_nopNbyte(dcontext, nop_length);
#ifdef X64
                        if (FRAG_IS_32(f->flags)) {
                            instr_set_x86_mode(nop_inst, true /*x86*/);
                            instr_shrink_to_32_bits(nop_inst);
                        }
#endif
                        LOG(THREAD, LOG_INTERP, 4,
                            "Marking exit branch as having nop padding\n");
                        instr_branch_set_padded(inst, true);
                        instrlist_preinsert(ilist, inst, nop_inst);
                        /* sanity check */
                        ASSERT((int)nop_length == instr_length(dcontext, nop_inst));
                        if (emitting) {
                            /* fixup offsets */
                            nop_inst->offset = offset;
                            /* only inc stats for emitting, not for recreating */
                            STATS_PAD_JMPS_ADD(f->flags, num_nops, 1);
                            STATS_PAD_JMPS_ADD(f->flags, nop_bytes, nop_length);
                        }
                        /* set translation whether emitting or not */
                        instr_set_translation(nop_inst, instr_get_translation(inst));
                        instr_set_our_mangling(nop_inst, true);
                        offset += nop_length;
                    }
                    /* sanity check that we fixed the alignment */
                    ASSERT(patchable_exit_cti_align_offs(dcontext, inst,
                                                         starting_pc + offset) == 0);
                } else {
                    DOSTATS({
                        /* only inc stats for emitting, not for recreating */
                        if (emitting)
                            STATS_PAD_JMPS_ADD(f->flags, num_no_pad_exits, 1);
                    });
                }
            }
        }
        if (emitting)
            inst->offset = offset; /* used by instr_encode */
        offset += instr_length(dcontext, inst);
    }
    return start_shift;
}

static cache_pc
insert_save_xax(dcontext_t *dcontext, cache_pc pc, uint flags, bool shared,
                ushort tls_offs, bool require_addr16)
{
    return insert_spill_or_restore(dcontext, pc, flags, true /*spill*/, shared, REG_XAX,
                                   tls_offs, XAX_OFFSET, require_addr16);
}

/* restore xax in a stub or a fragment prefix */
static cache_pc
insert_restore_xax(dcontext_t *dcontext, cache_pc pc, uint flags, bool shared,
                   ushort tls_offs, bool require_addr16)
{
    return insert_spill_or_restore(dcontext, pc, flags, false /*restore*/, shared,
                                   REG_XAX, tls_offs, XAX_OFFSET, require_addr16);
}

/* for the hashtable lookup inlined into exit stubs, the
 * lookup routine is encoded earlier into a template,
 * (in the routine emit_inline_ibl_stub(), below)
 * which we copy here and fix up the linkstub ptr for.
 * when the hashtable changes, the mask and table are
 * updated in update_indirect_exit_stub(), below.
 */
static byte *
insert_inlined_ibl(dcontext_t *dcontext, fragment_t *f, linkstub_t *l, byte *pc,
                   cache_pc unlinked_exit_target, uint flags)
{
    ibl_code_t *ibl_code =
        get_ibl_routine_code(dcontext, extract_branchtype(l->flags), f->flags);
    byte *start_pc = pc;
    cache_pc linked_exit_target = get_linked_entry(dcontext, unlinked_exit_target);

    /* PR 248207: haven't updated the inlining to be x64-compliant yet */
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));

    ASSERT(linkstub_owned_by_fragment(dcontext, f, l));
    ASSERT(ibl_code->ibl_head_is_inlined);
    ASSERT(EXIT_HAS_STUB(l->flags, f->flags));
    memcpy(start_pc, ibl_code->inline_ibl_stub_template, ibl_code->inline_stub_length);

    /* exit should be unlinked initially */
    patch_branch(FRAG_ISA_MODE(f->flags), EXIT_CTI_PC(f, l),
                 start_pc + ibl_code->inline_unlink_offs, NOT_HOT_PATCHABLE);

    if (DYNAMO_OPTION(indirect_stubs)) {
        /* fixup linked/unlinked targets */
        if (DYNAMO_OPTION(atomic_inlined_linking)) {
            insert_relative_target(start_pc + ibl_code->inline_linkedjmp_offs,
                                   linked_exit_target, NOT_HOT_PATCHABLE);
            insert_relative_target(start_pc + ibl_code->inline_unlinkedjmp_offs,
                                   unlinked_exit_target, NOT_HOT_PATCHABLE);
        } else {
            insert_relative_target(start_pc + ibl_code->inline_linkedjmp_offs,
                                   unlinked_exit_target, NOT_HOT_PATCHABLE);
        }
        /* set the linkstub ptr */
        pc = start_pc + ibl_code->inline_linkstub_first_offs;
        IF_X64(ASSERT_NOT_IMPLEMENTED(false));
        *((uint *)vmcode_get_writable_addr(pc)) = (uint)(ptr_uint_t)l;
        if (DYNAMO_OPTION(atomic_inlined_linking)) {
            pc = start_pc + ibl_code->inline_linkstub_second_offs;
            IF_X64(ASSERT_NOT_IMPLEMENTED(false));
            *((uint *)vmcode_get_writable_addr(pc)) = (uint)(ptr_uint_t)l;
        }
    } else {
        insert_relative_target(start_pc + ibl_code->inline_linkedjmp_offs,
                               linked_exit_target, NOT_HOT_PATCHABLE);
        insert_relative_target(
            start_pc + ibl_code->inline_unlink_offs +
                1 /* skip jmp opcode: see emit_inline_ibl_stub FIXME */,
            unlinked_exit_target, NOT_HOT_PATCHABLE);
    }

    return start_pc + ibl_code->inline_stub_length;
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
    cache_pc exit_target;
    bool indirect = false;
    bool can_inline = true;
    ASSERT(linkstub_owned_by_fragment(dcontext, f, l));

    /* select the correct exit target */
    if (LINKSTUB_DIRECT(l_flags)) {
        if (TEST(FRAG_COARSE_GRAIN, f->flags)) {
            /* need to target the fcache return prefix */
            exit_target = fcache_return_coarse_prefix(stub_pc, NULL);
            ASSERT(exit_target != NULL);
        } else
            exit_target = get_direct_exit_target(dcontext, f->flags);
    } else {
        ASSERT(LINKSTUB_INDIRECT(l_flags));
        /* caller shouldn't call us if no stub */
        ASSERT(EXIT_HAS_STUB(l_flags, f->flags));
        if (TEST(FRAG_COARSE_GRAIN, f->flags)) {
            /* need to target the ibl prefix */
            exit_target =
                get_coarse_ibl_prefix(dcontext, stub_pc, extract_branchtype(l_flags));
            ASSERT(exit_target != NULL);
        } else {
            /* initially, stub should be unlinked */
            exit_target = get_unlinked_entry(dcontext, EXIT_TARGET_TAG(dcontext, f, l));
        }
        indirect = true;
#ifdef WINDOWS
        can_inline = (exit_target != unlinked_shared_syscall_routine(dcontext));
#endif
        if (can_inline) {
            ibl_code_t *ibl_code =
                get_ibl_routine_code(dcontext, extract_branchtype(l_flags), f->flags);
            if (!ibl_code->ibl_head_is_inlined)
                can_inline = false;
        }
    }

    if (indirect && can_inline) {
        pc = insert_inlined_ibl(dcontext, f, l, pc, exit_target, f->flags);
        IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_int(pc - stub_pc)));
        return (int)(pc - stub_pc);
    }

    if (indirect) {
        pc = insert_jmp_to_ibl(pc, f, l, exit_target, dcontext);
    } else if (TEST(FRAG_COARSE_GRAIN, f->flags)) {
        /* This is an entrance stub.  It may be executed even when linked,
         * so we store target info to memory instead of a register.
         * The exact bytes used here are assumed by entrance_stub_target_tag().
         */
#ifdef X64
        if (!FRAG_IS_32(f->flags)) {
            app_pc tgt = EXIT_TARGET_TAG(dcontext, f, l);
            /* Both entrance_stub_target_tag() and coarse_indirect_stub_jmp_target()
             * assume that the addr prefix is present for 32-bit but not 64-bit.
             */
            pc = vmcode_get_writable_addr(pc);
            /* since we have no 8-byte-immed-to-memory, we split into two pieces */
            *pc = TLS_SEG_OPCODE;
            pc++;
            *pc = MOV_IMM2MEM_OPCODE;
            pc++;
            *pc = MODRM_BYTE(0 /*mod*/, 0 /*reg*/, 4 /*rm*/);
            pc++; /* => no base, w/ sib */
            *pc = SIB_DISP32;
            pc++; /* just disp32 */
            /* low 32 bits */
            *((uint *)pc) = (uint)os_tls_offset(DIRECT_STUB_SPILL_SLOT);
            pc += 4;
            *((uint *)pc) = (uint)(ptr_uint_t)tgt;
            pc += 4;

            *pc = TLS_SEG_OPCODE;
            pc++;
            *pc = MOV_IMM2MEM_OPCODE;
            pc++;
            *pc = MODRM_BYTE(0 /*mod*/, 0 /*reg*/, 4 /*rm*/);
            pc++; /* => no base, w/ sib */
            *pc = SIB_DISP32;
            pc++; /* just disp32 */
            /* high 32 bits */
            *((uint *)pc) = 4 + (uint)os_tls_offset(DIRECT_STUB_SPILL_SLOT);
            pc += 4;
            *((uint *)pc) = (uint)(((ptr_uint_t)tgt) >> 32);
            pc += 4;
            pc = vmcode_get_executable_addr(pc);
        } else {
#endif /* X64 */
            /* We must be at or below 15 bytes so we require addr16.
             * Both entrance_stub_target_tag() and coarse_indirect_stub_jmp_target()
             * assume that the addr prefix is present for 32-bit but not 64-bit.
             */
            pc = vmcode_get_writable_addr(pc);
            /* addr16 mov <target>, fs:<dir-stub-spill> */
            /* FIXME: PR 209709: test perf and remove if outweighs space */
            *pc = ADDR_PREFIX_OPCODE;
            pc++;
            *pc = TLS_SEG_OPCODE;
            pc++;
            *pc = MOV_IMM2MEM_OPCODE;
            pc++;
            *pc = MODRM16_DISP16;
            pc++;
            *((ushort *)pc) = os_tls_offset(DIRECT_STUB_SPILL_SLOT);
            pc += 2;
            *((uint *)pc) = (uint)(ptr_uint_t)EXIT_TARGET_TAG(dcontext, f, l);
            pc += 4;
            pc = vmcode_get_executable_addr(pc);
#ifdef X64
        }
#endif /* X64 */
        /* jmp to exit target */
        pc = insert_relative_jump(pc, exit_target, NOT_HOT_PATCHABLE);
    } else {
        /* direct branch */

        /* we use XAX to hold the linkstub pointer before we get to fcache_return,
           note that indirect stubs use XBX for linkstub pointer */
        pc = insert_save_xax(dcontext, pc, f->flags, FRAG_DB_SHARED(f->flags),
                             DIRECT_STUB_SPILL_SLOT, true);

        /* mov $linkstub_ptr,%xax */
#ifdef X64
        if (FRAG_IS_32(f->flags)) {
            /* XXX i#829: we only support stubs in the low 4GB which is ok for
             * WOW64 mixed-mode but long-term for 64-bit flexibility (i#774) we
             * may need to store the other half of the pointer somewhere
             */
            uint l_uint;
            ASSERT_TRUNCATE(l_uint, uint, (ptr_uint_t)l);
            l_uint = (uint)(ptr_uint_t)l;
            *(vmcode_get_writable_addr(pc)) = MOV_IMM2XAX_OPCODE;
            pc++;
            *((uint *)vmcode_get_writable_addr(pc)) = l_uint;
            pc += sizeof(l_uint);
        } else {
            *(vmcode_get_writable_addr(pc)) =
                REX_PREFIX_BASE_OPCODE | REX_PREFIX_W_OPFLAG;
            pc++;
#endif
            /* shared w/ 32-bit and 64-bit !FRAG_IS_32 */
            *(vmcode_get_writable_addr(pc)) = MOV_IMM2XAX_OPCODE;
            pc++;
            *((ptr_uint_t *)vmcode_get_writable_addr(pc)) = (ptr_uint_t)l;
            pc += sizeof(l);

#ifdef X64
        }
#endif
        /* jmp to exit target */
        pc = insert_relative_jump(pc, exit_target, NOT_HOT_PATCHABLE);
    }

    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_int(pc - stub_pc)));
    return (int)(pc - stub_pc);
}

cache_pc
exit_cti_disp_pc(cache_pc branch_pc)
{
    cache_pc byte_ptr = branch_pc;
    byte opcode = *byte_ptr;
    uint length = 0;

    if (opcode == RAW_PREFIX_jcc_taken || opcode == RAW_PREFIX_jcc_not_taken) {
        length++;
        byte_ptr++;
        opcode = *byte_ptr;
        /* branch hints are only valid with jcc instrs, and if present on
         * other ctis we strip them out during mangling (i#435)
         */
        ASSERT(opcode == RAW_OPCODE_jcc_byte1);
    }
    if (opcode == ADDR_PREFIX_OPCODE) { /* used w/ jecxz/loop* */
        length++;
        byte_ptr++;
        opcode = *byte_ptr;
    }

    if (opcode >= RAW_OPCODE_loop_start && opcode <= RAW_OPCODE_loop_end) {
        /* assume that this is a mangled jcxz/loop*
         * target pc is in last 4 bytes of "9-byte instruction"
         */
        length += CTI_SHORT_REWRITE_LENGTH;
    } else if (opcode == RAW_OPCODE_jcc_byte1) {
        /* 2-byte opcode, 6-byte instruction, except for branch hint */
        ASSERT(*(byte_ptr + 1) >= RAW_OPCODE_jcc_byte2_start &&
               *(byte_ptr + 1) <= RAW_OPCODE_jcc_byte2_end);
        length += CBR_LONG_LENGTH;
    } else {
        /* 1-byte opcode, 5-byte instruction */
#ifdef HOT_PATCHING_INTERFACE
        ASSERT(opcode == RAW_OPCODE_jmp || opcode == RAW_OPCODE_call);
#else
        ASSERT(opcode == RAW_OPCODE_jmp);
#endif
        length += JMP_LONG_LENGTH;
    }
    return branch_pc + length - 4; /* disp is 4 even on x64 */
}

/* NOTE : for inlined indirect branches linking is !NOT! atomic with respect
 * to a thread executing in the cache unless using the atomic_inlined_linking
 * option (unlike unlinking)
 */
void
link_indirect_exit_arch(dcontext_t *dcontext, fragment_t *f, linkstub_t *l,
                        bool hot_patch, app_pc target_tag)
{
    uint stub_size;
    app_pc exit_target, cur_target, pc;
    /* w/ indirect exits now having their stub pcs computed based
     * on the cti targets, we must calculate them at a consistent
     * state (we do have multi-stage modifications for inlined stubs)
     */
    byte *stub_pc = (byte *)EXIT_STUB_PC(dcontext, f, l);

    if (DYNAMO_OPTION(indirect_stubs)) {
        /* go to start of 5-byte jump instruction at end of exit stub */
        stub_size = exit_stub_size(dcontext, target_tag, f->flags);
        pc = stub_pc + stub_size - 5;
    } else {
        /* cti goes straight to ibl, and must be a jmp, not jcc,
         * except for -unsafe_ignore_eflags_trace stay-on-trace cmp,jne
         */
        pc = EXIT_CTI_PC(f, l);
        /* for x64, or -unsafe_ignore_eflags_trace, a trace may have a jne to the stub */
        if (*pc == JNE_OPCODE_1) {
            ASSERT(TEST(FRAG_IS_TRACE, f->flags));
#ifndef X64
            ASSERT(INTERNAL_OPTION(unsafe_ignore_eflags_trace));
#endif
            /* FIXME: share this code w/ common path below: 1 opcode byte vs 2 */
            /* get absolute address of target */
            cur_target = (app_pc)PC_RELATIVE_TARGET(pc + 2);
            exit_target = get_linked_entry(dcontext, cur_target);
            pc += 2; /* skip jne opcode */
            pc = insert_relative_target(pc, exit_target, hot_patch);
            return;
        } else {
            ASSERT(*pc == JMP_OPCODE);
        }
    }
    /* get absolute address of target */
    cur_target = (app_pc)PC_RELATIVE_TARGET(pc + 1);
    exit_target = get_linked_entry(dcontext, cur_target);
    pc++; /* skip jmp opcode */
    pc = insert_relative_target(pc, exit_target, hot_patch);
}

cache_pc
indirect_linkstub_stub_pc(dcontext_t *dcontext, fragment_t *f, linkstub_t *l)
{
    cache_pc cti = EXIT_CTI_PC(f, l);
    /* decode the cti: it should be a relative jmp to the stub */
    cache_pc stub;
    if (!EXIT_HAS_STUB(l->flags, f->flags))
        return NULL;
    /* for x64, or -unsafe_ignore_eflags_trace, a trace may have a jne to the stub */
    if (*cti == JNE_OPCODE_1) {
        ASSERT(TEST(FRAG_IS_TRACE, f->flags));
#ifndef X64
        ASSERT(INTERNAL_OPTION(unsafe_ignore_eflags_trace));
#endif
        stub = (cache_pc)PC_RELATIVE_TARGET(cti + 2 /*opcode bytes*/);
    } else {
        if (*cti == JMP_OPCODE) {
            stub = (cache_pc)PC_RELATIVE_TARGET(cti + 1 /*opcode byte*/);
        } else {
            /* case 6532/10987: frozen coarse has no jmp to stub */
            ASSERT(TEST(FRAG_COARSE_GRAIN, f->flags));
            ASSERT(coarse_is_indirect_stub(cti));
            stub = cti;
        }
    }
    ASSERT(stub >= cti && (stub - cti) <= MAX_FRAGMENT_SIZE);
    if (!TEST(LINK_LINKED, l->flags)) {
        /* the unlink target is not always the start of the stub */
        stub -= linkstub_unlink_entry_offset(dcontext, f, l);
        /* FIXME: for -no_indirect_stubs we could point exit cti directly
         * at unlink ibl routine if we could find the stub target for
         * linking here...should consider storing stub pc for ind exits
         * for that case to save 5 bytes in the inlined stub
         */
    }
    return stub;
}

/* since we now support branch hints on long cbrs, we need to do a little
 * decoding to find their length
 */
cache_pc
cbr_fallthrough_exit_cti(cache_pc prev_cti_pc)
{
    if (*prev_cti_pc == RAW_PREFIX_jcc_taken || *prev_cti_pc == RAW_PREFIX_jcc_not_taken)
        prev_cti_pc++;
    return (prev_cti_pc + CBR_LONG_LENGTH);
}

/* This is an atomic operation with respect to a thread executing in the
 * cache (barring ifdef NATIVE_RETURN, which is now removed), for
 * inlined indirect exits the
 * unlinked path of the ibl routine detects the race condition between the
 * two patching writes and handles it appropriately unless using the
 * atomic_inlined_linking option in which case there is only one patching
 * write (since tail is duplicated) */
void
unlink_indirect_exit(dcontext_t *dcontext, fragment_t *f, linkstub_t *l)
{
    uint stub_size;
    cache_pc exit_target, cur_target;
    app_pc target_tag = EXIT_TARGET_TAG(dcontext, f, l);
    byte *pc;
    ibl_code_t *ibl_code = NULL;
    /* w/ indirect exits now having their stub pcs computed based
     * on the cti targets, we must calculate them at a consistent
     * state (we do have multi-stage modifications for inlined stubs)
     */
    byte *stub_pc = (byte *)EXIT_STUB_PC(dcontext, f, l);
    ASSERT(!TEST(FRAG_COARSE_GRAIN, f->flags));
    ASSERT(linkstub_owned_by_fragment(dcontext, f, l));
    ASSERT(LINKSTUB_INDIRECT(l->flags));
    /* target is always the same, so if it's already unlinked, this is a nop */
    if (!TEST(LINK_LINKED, l->flags))
        return;

#ifdef WINDOWS
    if (!is_shared_syscall_routine(dcontext, target_tag)) {
#endif
        ibl_code = get_ibl_routine_code(dcontext, extract_branchtype(l->flags), f->flags);
#ifdef WINDOWS
    }
#endif

    if ((!DYNAMO_OPTION(atomic_inlined_linking) && DYNAMO_OPTION(indirect_stubs)) ||
#ifdef WINDOWS
        target_tag ==
            shared_syscall_routine_ex(
                dcontext _IF_X64(FRAGMENT_GENCODE_MODE(f->flags))) ||
#endif
        /* FIXME: for -no_indirect_stubs and inlined ibl, we'd like to directly
         * target the unlinked ibl entry but we don't yet -- see FIXME in
         * emit_inline_ibl_stub()
         */
        !ibl_code->ibl_head_is_inlined) {

        if (DYNAMO_OPTION(indirect_stubs)) {
            /* go to start of 5-byte jump instruction at end of exit stub */
            stub_size = exit_stub_size(dcontext, target_tag, f->flags);
            pc = stub_pc + stub_size - 5;
        } else {
            /* cti goes straight to ibl, and must be a jmp, not jcc */
            pc = EXIT_CTI_PC(f, l);
            /* for x64, or -unsafe_ignore_eflags_trace, a trace may have a jne */
            if (*pc == JNE_OPCODE_1) {
                ASSERT(TEST(FRAG_IS_TRACE, f->flags));
#ifndef X64
                ASSERT(INTERNAL_OPTION(unsafe_ignore_eflags_trace));
#endif
                pc++; /* 2-byte opcode, skip 1st here */
            } else
                ASSERT(*pc == JMP_OPCODE);
        }
        cur_target = (cache_pc)PC_RELATIVE_TARGET(pc + 1);
        exit_target = get_unlinked_entry(dcontext, cur_target);
        pc++; /* skip jmp opcode */
        pc = insert_relative_target(pc, exit_target, HOT_PATCHABLE);
    }

    /* To maintain atomicity with respect to executing thread, must unlink
     * the ending jmp (above) first so that the unlinked path can detect the
     * race condition case */
#ifdef WINDOWS
    /* faster than is_shared_syscall_routine() since only linked target can get here
     * yet inconsistent
     */
    if (target_tag !=
        shared_syscall_routine_ex(dcontext _IF_X64(FRAGMENT_GENCODE_MODE(f->flags)))) {
#endif
        /* need to make branch target the unlink entry point inside exit stub */
        if (ibl_code->ibl_head_is_inlined) {
            cache_pc target = stub_pc;
            /* now add offset of unlinked entry */
            target += ibl_code->inline_unlink_offs;
            patch_branch(FRAG_ISA_MODE(f->flags), EXIT_CTI_PC(f, l), target,
                         HOT_PATCHABLE);
        }
#ifdef WINDOWS
    }
#endif
}

/*******************************************************************************
 * COARSE-GRAIN FRAGMENT SUPPORT
 */

cache_pc
entrance_stub_jmp(cache_pc stub)
{
#ifdef X64
    if (*stub == 0x65)
        return (stub + STUB_COARSE_DIRECT_SIZE64 - JMP_LONG_LENGTH);
        /* else, 32-bit stub */
#endif
    return (stub + STUB_COARSE_DIRECT_SIZE32 - JMP_LONG_LENGTH);
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
    bool res = false;
    /* FIXME: case 10334: pass in info and if non-NULL avoid lookup here? */
    coarse_info_t *info = get_stub_coarse_info(stub);
    if (info != NULL) {
        res = ALIGNED(stub, coarse_stub_alignment(info)) &&
            *entrance_stub_jmp(stub) == JMP_OPCODE;
        DOCHECK(1, {
            if (res) {
                cache_pc tgt = entrance_stub_jmp_target(stub);
                ASSERT(!in_fcache(stub));
                ASSERT(tgt == trace_head_return_coarse_prefix(stub, info) ||
                       tgt == fcache_return_coarse_prefix(stub, info) ||
                       /* another fragment */
                       in_fcache(tgt));
            }
        });
    }
    return res;
}

/*###########################################################################
 *
 * fragment_t Prefixes
 *
 * Two types: indirect branch target, which restores eflags and xcx, and
 * normal prefix, which just restores xcx
 */
#define IBL_EFLAGS_IN_TLS() (IF_X64_ELSE(true, SHARED_IB_TARGETS()))

/* Indirect Branch Target Prefix
 * We have 3 different prefixes: one if we don't need to restore eflags, one
 * if we need to restore just using sahf, and one if we also need to restore
 * the overflow flag OF.
 *
 * FIXME: currently we cache-align the prefix, not the normal
 * entry point...if prefix gets much longer, might want to add
 * nops to get normal entry cache-aligned?
 */

/* for now all ibl targets must use same scratch locations: tls or not, no mixture */

#ifdef X86
#    define RESTORE_XAX_PREFIX(flags)                                   \
        ((FRAG_IS_X86_TO_X64(flags) &&                                  \
          IF_X64_ELSE(DYNAMO_OPTION(x86_to_x64_ibl_opt), false))        \
             ? SIZE64_MOV_R8_TO_XAX                                     \
             : (IBL_EFLAGS_IN_TLS() ? SIZE_MOV_XAX_TO_TLS(flags, false) \
                                    : SIZE32_MOV_XAX_TO_ABS))
#    define PREFIX_BASE(flags) \
        (RESTORE_XAX_PREFIX(flags) + FRAGMENT_BASE_PREFIX_SIZE(flags))
#else
/* FIXME i#1551: implement ibl and prefix support */
#    define RESTORE_XAX_PREFIX(flags) (ASSERT_NOT_IMPLEMENTED(false), 0)
#    define PREFIX_BASE(flags) (ASSERT_NOT_IMPLEMENTED(false), 0)
#endif

int
fragment_ibt_prefix_size(uint flags)
{
    bool use_eflags_restore = TEST(FRAG_IS_TRACE, flags)
        ? !DYNAMO_OPTION(trace_single_restore_prefix)
        : !DYNAMO_OPTION(bb_single_restore_prefix);
    /* The common case is !INTERNAL_OPTION(unsafe_ignore_eflags*) so
     * PREFIX_BASE(flags) is defined accordingly, and we subtract from it to
     * get the correct value when the option is on.
     */
    if (INTERNAL_OPTION(unsafe_ignore_eflags_prefix)) {
        if (INTERNAL_OPTION(unsafe_ignore_eflags_ibl)) {
            ASSERT(PREFIX_BASE(flags) - RESTORE_XAX_PREFIX(flags) >= 0);
            return PREFIX_BASE(flags) - RESTORE_XAX_PREFIX(flags);
        } else {
            /* still need to restore xax, just don't restore eflags */
            return PREFIX_BASE(flags);
        }
    }
    if (!use_eflags_restore)
        return PREFIX_BASE(flags) - RESTORE_XAX_PREFIX(flags);
    if (TEST(FRAG_WRITES_EFLAGS_6, flags)) /* no flag restoration needed */
        return PREFIX_BASE(flags);
    else if (TEST(FRAG_WRITES_EFLAGS_OF, flags)) /* no OF restoration needed */
        return (PREFIX_BASE(flags) + PREFIX_SIZE_FIVE_EFLAGS);
    else { /* must restore all 6 flags */
        if (INTERNAL_OPTION(unsafe_ignore_overflow)) {
            /* do not restore OF */
            return (PREFIX_BASE(flags) + PREFIX_SIZE_FIVE_EFLAGS);
        } else {
            return (PREFIX_BASE(flags) + PREFIX_SIZE_RESTORE_OF +
                    PREFIX_SIZE_FIVE_EFLAGS);
        }
    }
}

/* See SAVE_TO_DC_OR_TLS() in mangle.c for the save xcx code */
static cache_pc
insert_restore_xcx(dcontext_t *dcontext, cache_pc pc, uint flags, bool require_addr16)
{
    /* shared fragment prefixes use tls, private use mcontext
     * this works b/c the shared ibl copies the app xcx to both places!
     * private_ib_in_tls option makes all prefixes use tls
     */
    return insert_spill_or_restore(dcontext, pc, flags, false /*restore*/,
                                   XCX_IN_TLS(flags), REG_XCX, MANGLE_XCX_SPILL_SLOT,
                                   XCX_OFFSET, require_addr16);
}

static cache_pc
insert_restore_register(dcontext_t *dcontext, fragment_t *f, cache_pc pc, reg_id_t reg)
{
    ASSERT(reg == REG_XAX || reg == REG_XCX);
#ifdef X64
    if (FRAG_IS_X86_TO_X64(f->flags) && DYNAMO_OPTION(x86_to_x64_ibl_opt)) {
        /* in x86_to_x64 mode, rax was spilled to r8 and rcx was spilled to r9
         * to restore rax:  49 8b c0   mov %r8 -> %rax
         * to restore rcx:  49 8b c9   mov %r9 -> %rcx
         */
        pc = vmcode_get_writable_addr(pc);
        *pc = REX_PREFIX_BASE_OPCODE | REX_PREFIX_W_OPFLAG | REX_PREFIX_B_OPFLAG;
        pc++;
        *pc = MOV_MEM2REG_OPCODE;
        pc++;
        *pc = MODRM_BYTE(3 /*mod*/, reg_get_bits(reg),
                         reg_get_bits((reg == REG_XAX) ? REG_R8 : REG_R9));
        pc++;
        pc = vmcode_get_executable_addr(pc);
    } else {
#endif
        pc = (reg == REG_XAX)
            ? insert_restore_xax(dcontext, pc, f->flags, IBL_EFLAGS_IN_TLS(),
                                 PREFIX_XAX_SPILL_SLOT, false)
            : insert_restore_xcx(dcontext, pc, f->flags, false);
#ifdef X64
    }
#endif
    return pc;
}

void
insert_fragment_prefix(dcontext_t *dcontext, fragment_t *f)
{
    byte *pc = (byte *)f->start_pc;
    bool insert_eflags_xax_restore = TEST(FRAG_IS_TRACE, f->flags)
        ? !DYNAMO_OPTION(trace_single_restore_prefix)
        : !DYNAMO_OPTION(bb_single_restore_prefix);
    ASSERT(f->prefix_size == 0); /* shouldn't be any prefixes yet */

    if (use_ibt_prefix(f->flags)) {
        if ((!INTERNAL_OPTION(unsafe_ignore_eflags_prefix) ||
             !INTERNAL_OPTION(unsafe_ignore_eflags_ibl)) &&
            insert_eflags_xax_restore) {
            if (!INTERNAL_OPTION(unsafe_ignore_eflags_prefix) &&
                !TEST(FRAG_WRITES_EFLAGS_6, f->flags)) {
                if (!TEST(FRAG_WRITES_EFLAGS_OF, f->flags) &&
                    !INTERNAL_OPTION(unsafe_ignore_overflow)) {
                    DEBUG_DECLARE(byte *restore_of_prefix_pc = pc;)
                    /* must restore OF
                     * we did a seto on %al, so we restore OF by adding 0x7f to
                     * %al (7f not ff b/c add only sets OF for signed operands,
                     * sets CF for uint)
                     */
                    STATS_INC(num_oflag_prefix_restore);

                    pc = vmcode_get_writable_addr(pc);
                    /* 04 7f   add $0x7f,%al */
                    *pc = ADD_AL_OPCODE;
                    pc++;
                    *pc = 0x7f;
                    pc++;
                    pc = vmcode_get_executable_addr(pc);

                    ASSERT(pc - restore_of_prefix_pc == PREFIX_SIZE_RESTORE_OF);
                }

                /* restore other 5 flags w/ sahf */
                *vmcode_get_writable_addr(pc) = SAHF_OPCODE;
                pc++;
                ASSERT(PREFIX_SIZE_FIVE_EFLAGS == 1);
            }
            /* restore xax */
            pc = insert_restore_register(dcontext, f, pc, REG_XAX);
        }

        pc = insert_restore_register(dcontext, f, pc, REG_XCX);

        /* set normal entry point to be next pc */
        ASSERT_TRUNCATE(f->prefix_size, byte, ((cache_pc)pc) - f->start_pc);
        f->prefix_size = (byte)(((cache_pc)pc) - f->start_pc);
    } else {
        if (dynamo_options.bb_prefixes) {
            pc = insert_restore_register(dcontext, f, pc, REG_XCX);

            /* set normal entry point to be next pc */
            ASSERT_TRUNCATE(f->prefix_size, byte, ((cache_pc)pc) - f->start_pc);
            f->prefix_size = (byte)(((cache_pc)pc) - f->start_pc);
        } /* else, no prefix */
    }
    /* make sure emitted size matches size we requested */
    ASSERT(f->prefix_size == fragment_prefix_size(f->flags));
}

/***************************************************************************/
/*             THREAD-PRIVATE/SHARED ROUTINE GENERATION                    */
/***************************************************************************/

/* helper functions for emit_fcache_enter_common  */
#ifdef X64
#    ifdef WINDOWS
#        define OPND_ARG1 opnd_create_reg(REG_RCX)
#    else
#        define OPND_ARG1 opnd_create_reg(REG_RDI)
#    endif /* Win/Unix */
#else
#    define OPND_ARG1 OPND_CREATE_MEM32(REG_ESP, 4)
#endif /* 64/32-bit */

void
append_fcache_enter_prologue(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
#ifdef UNIX
    instr_t *no_signals = INSTR_CREATE_label(dcontext);
#endif
    if (!absolute) {
        /* grab gen routine's parameter dcontext and put it into edi */
#ifdef UNIX
        /* first, save callee-saved reg in case we return for a signal */
        APP(ilist,
            XINST_CREATE_move(dcontext, opnd_create_reg(REG_XAX),
                              opnd_create_reg(REG_DCXT)));
#endif
        APP(ilist, XINST_CREATE_load(dcontext, opnd_create_reg(REG_DCXT), OPND_ARG1));
        if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
            APP(ilist, RESTORE_FROM_DC(dcontext, REG_DCXT_PROT, PROT_OFFS));
    }
#ifdef UNIX
    APP(ilist,
        XINST_CREATE_cmp(dcontext,
                         OPND_DC_FIELD(absolute, dcontext, OPSZ_1, SIGPENDING_OFFSET),
                         OPND_CREATE_INT8(0)));
    APP(ilist, INSTR_CREATE_jcc(dcontext, OP_jle, opnd_create_instr(no_signals)));
    if (!absolute) {
        /* restore callee-saved reg */
        APP(ilist,
            XINST_CREATE_move(dcontext, opnd_create_reg(REG_DCXT),
                              opnd_create_reg(REG_XAX)));
    }
    APP(ilist, XINST_CREATE_return(dcontext));
    APP(ilist, no_signals);
#endif
}

/*  # append instructions to call exit_dr_hook
 *  if (EXIT_DR_HOOK != NULL && !dcontext->ignore_enterexit)
 *    if (!absolute)
 *      push    %xdi
 *      push    %xsi
 *    else
 *      # support for skipping the hook
 *      RESTORE_FROM_UPCONTEXT ignore_enterexit_OFFSET,%edi
 *      cmpl    %edi,0
 *      jnz     post_hook
 *    endif
 *    call  EXIT_DR_HOOK # for x64 windows, reserve 32 bytes stack space for call
 *    if (!absolute)
 *      pop    %xsi
 *      pop    %xdi
 *    endif
 *  endif
 * post_hook:
 */
void
append_call_exit_dr_hook(dcontext_t *dcontext, instrlist_t *ilist, bool absolute,
                         bool shared)
{
    instr_t *post_hook = INSTR_CREATE_label(dcontext);
    if (EXIT_DR_HOOK != NULL) {
        /* if absolute, don't bother to save any regs around the call */
        if (!absolute) {
            /* save xdi and xsi around call.
             * for x64, they're supposed to be callee-saved on windows,
             * but not linux (though we could move to r12-r15 on linux
             * instead of pushing them).
             */
            APP(ilist, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_XDI)));
            APP(ilist, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_XSI)));
        }
#ifdef WINDOWS
        else {
            /* For thread-private (used for syscalls), don't call if
             * dcontext->ignore_enterexit.  This is a perf hit to check: could
             * instead have a space hit via a separate routine.  This is only
             * needed right now for NtSuspendThread handling (see case 4942).
             */
            APP(ilist, RESTORE_FROM_DC(dcontext, REG_EDI, IGNORE_ENTEREXIT_OFFSET));
            /* P4 opt guide says to use test to cmp reg with 0: shorter instr */
            APP(ilist,
                INSTR_CREATE_test(dcontext, opnd_create_reg(REG_EDI),
                                  opnd_create_reg(REG_EDI)));
            APP(ilist, INSTR_CREATE_jcc(dcontext, OP_jnz, opnd_create_instr(post_hook)));
        }
#endif
        /* make sure to use dr_insert_call() rather than a raw OP_call instr,
         * since x64 windows requires 32 bytes of stack space even w/ no args,
         * and we don't want anyone clobbering our pushed registers!
         */
        dr_insert_call((void *)dcontext, ilist, NULL /*append*/, (void *)EXIT_DR_HOOK, 0);
        if (!absolute) {
            /* save edi and esi around call */
            APP(ilist, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_XSI)));
            APP(ilist, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_XDI)));
        }
    }
    APP(ilist, post_hook /*label*/);
}

/* append instructions to restore xflags
 *  # restore the original register state
 *  RESTORE_FROM_UPCONTEXT xflags_OFFSET,%xax
 *  push  %xax
 *  popf        # restore eflags temporarily using dstack
 */
void
append_restore_xflags(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    APP(ilist, RESTORE_FROM_DC(dcontext, SCRATCH_REG0, XFLAGS_OFFSET));
    APP(ilist, INSTR_CREATE_push(dcontext, opnd_create_reg(SCRATCH_REG0)));
    /* restore eflags temporarily using dstack */
    APP(ilist, INSTR_CREATE_RAW_popf(dcontext));
}

/* append instructions to restore extension registers like xmm
 *  if preserve_xmm_caller_saved
 *    RESTORE_FROM_UPCONTEXT xmm_OFFSET+0*16,%xmm0
 *    RESTORE_FROM_UPCONTEXT xmm_OFFSET+1*16,%xmm1
 *    RESTORE_FROM_UPCONTEXT xmm_OFFSET+2*16,%xmm2
 *    RESTORE_FROM_UPCONTEXT xmm_OFFSET+3*16,%xmm3
 *    RESTORE_FROM_UPCONTEXT xmm_OFFSET+4*16,%xmm4
 *    RESTORE_FROM_UPCONTEXT xmm_OFFSET+5*16,%xmm5
 *    RESTORE_FROM_UPCONTEXT xmm_OFFSET+6*16,%xmm6  # 32-bit Linux
 *    RESTORE_FROM_UPCONTEXT xmm_OFFSET+7*16,%xmm7  # 32-bit Linux
 *  endif
 */
void
append_restore_simd_reg(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    /* No processor will support AVX-512 but no SSE/AVX. */
    ASSERT(preserve_xmm_caller_saved() || !ZMM_ENABLED());
    if (!preserve_xmm_caller_saved())
        return;
    /* PR 264138: we must preserve xmm0-5 if on a 64-bit kernel.
     * Rather than try and optimize we save/restore on every cxt
     * sw.  The xmm field is aligned, so we can use movdqa/movaps,
     * though movdqu is stated to be as fast as movdqa when aligned:
     * but if so, why have two versions?  Is it only loads and not stores
     * for which that is true?  => PR 266305.
     * It's not clear that movdqa is any faster (and its opcode is longer):
     * movdqa and movaps are listed as the same latency and throughput in
     * the AMD optimization guide.  Yet examples of fast memcpy online seem
     * to use movdqa when sse2 is available.
     * Note that mov[au]p[sd] and movdq[au] are functionally equivalent.
     */
    /* FIXME i#438: once have SandyBridge processor need to measure
     * cost of vmovdqu and whether worth arranging 32-byte alignment
     */
    int i;
    uint opcode = move_mm_reg_opcode(true /*align16*/, true /*align32*/);
    ASSERT(proc_has_feature(FEATURE_SSE));
    instr_t *post_restore = NULL;
    instr_t *pre_avx512_restore = NULL;
    if (ZMM_ENABLED()) {
        post_restore = INSTR_CREATE_label(dcontext);
        pre_avx512_restore = INSTR_CREATE_label(dcontext);
        APP(ilist,
            INSTR_CREATE_cmp(
                dcontext,
                OPND_CREATE_ABSMEM(
                    vmcode_get_executable_addr((byte *)d_r_avx512_code_in_use), OPSZ_1),
                OPND_CREATE_INT8(0)));
        APP(ilist,
            INSTR_CREATE_jcc(dcontext, OP_jnz, opnd_create_instr(pre_avx512_restore)));
    }
    for (i = 0; i < proc_num_simd_sse_avx_saved(); i++) {
        APP(ilist,
            instr_create_1dst_1src(dcontext, opcode,
                                   opnd_create_reg(REG_SAVED_XMM0 + (reg_id_t)i),
                                   OPND_DC_FIELD(absolute, dcontext, OPSZ_SAVED_XMM,
                                                 SIMD_OFFSET + i * MCXT_SIMD_SLOT_SIZE)));
    }
    if (ZMM_ENABLED()) {
        APP(ilist, INSTR_CREATE_jmp(dcontext, opnd_create_instr(post_restore)));
        APP(ilist, pre_avx512_restore /*label*/);
        uint opcode_avx512 = move_mm_avx512_reg_opcode(true /*align64*/);
        for (i = 0; i < proc_num_simd_registers(); i++) {
            APP(ilist,
                instr_create_1dst_2src(
                    dcontext, opcode_avx512,
                    opnd_create_reg(DR_REG_START_ZMM + (reg_id_t)i),
                    opnd_create_reg(DR_REG_K0),
                    OPND_DC_FIELD(absolute, dcontext, OPSZ_SAVED_ZMM,
                                  SIMD_OFFSET + i * MCXT_SIMD_SLOT_SIZE)));
        }
        for (i = 0; i < proc_num_opmask_registers(); i++) {
            APP(ilist,
                instr_create_1dst_1src(
                    dcontext, proc_has_feature(FEATURE_AVX512BW) ? OP_kmovq : OP_kmovw,
                    opnd_create_reg(DR_REG_START_OPMASK + (reg_id_t)i),
                    OPND_DC_FIELD(absolute, dcontext, OPSZ_SAVED_OPMASK,
                                  OPMASK_OFFSET + i * OPMASK_AVX512BW_REG_SIZE)));
        }
        APP(ilist, post_restore /*label*/);
    }
}

/* append instructions to restore general purpose registers
 *  ifdef X64
 *    RESTORE_FROM_UPCONTEXT r8_OFFSET,%r8
 *    RESTORE_FROM_UPCONTEXT r9_OFFSET,%r9
 *    RESTORE_FROM_UPCONTEXT r10_OFFSET,%r10
 *    RESTORE_FROM_UPCONTEXT r11_OFFSET,%r11
 *    RESTORE_FROM_UPCONTEXT r12_OFFSET,%r12
 *    RESTORE_FROM_UPCONTEXT r13_OFFSET,%r13
 *    RESTORE_FROM_UPCONTEXT r14_OFFSET,%r14
 *    RESTORE_FROM_UPCONTEXT r15_OFFSET,%r15
 *  endif
 *    RESTORE_FROM_UPCONTEXT xax_OFFSET,%xax
 *    RESTORE_FROM_UPCONTEXT xbx_OFFSET,%xbx
 *    RESTORE_FROM_UPCONTEXT xcx_OFFSET,%xcx
 *    RESTORE_FROM_UPCONTEXT xdx_OFFSET,%xdx
 *  if (absolute || !TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
 *    RESTORE_FROM_UPCONTEXT xdx_OFFSET,%xdx
 *  if (absolute || !TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
 *    RESTORE_FROM_UPCONTEXT xsi_OFFSET,%xsi
 *  endif
 *  if (absolute || TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
 *    RESTORE_FROM_UPCONTEXT xdi_OFFSET,%xdi
 *  endif
 *    RESTORE_FROM_UPCONTEXT xbp_OFFSET,%xbp
 *    RESTORE_FROM_UPCONTEXT xsp_OFFSET,%xsp
 *  if (!absolute)
 *    if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
 *      RESTORE_FROM_UPCONTEXT xsi_OFFSET,%xsi
 *    else
 *      RESTORE_FROM_UPCONTEXT xdi_OFFSET,%xdi
 *    endif
 *  endif
 */
void
append_restore_gpr(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
#ifdef X64
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_R8, R8_OFFSET));
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_R9, R9_OFFSET));
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_R10, R10_OFFSET));
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_R11, R11_OFFSET));
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_R12, R12_OFFSET));
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_R13, R13_OFFSET));
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_R14, R14_OFFSET));
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_R15, R15_OFFSET));
#endif
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_XAX, XAX_OFFSET));
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_XBX, XBX_OFFSET));
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_XCX, XCX_OFFSET));
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_XDX, XDX_OFFSET));
    /* must restore esi last */
    if (absolute || !TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
        APP(ilist, RESTORE_FROM_DC(dcontext, REG_XSI, XSI_OFFSET));
    /* must restore edi last */
    if (absolute || TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
        APP(ilist, RESTORE_FROM_DC(dcontext, REG_XDI, XDI_OFFSET));
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_XBP, XBP_OFFSET));
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_XSP, XSP_OFFSET));
    /* must restore esi last */
    if (!absolute) {
        if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
            APP(ilist, RESTORE_FROM_DC(dcontext, REG_XSI, XSI_OFFSET));
        else
            APP(ilist, RESTORE_FROM_DC(dcontext, REG_XDI, XDI_OFFSET));
    }
}

/* helper functions for append_fcache_return_common */

/* append instructions to save gpr
 *
 * if (!absolute)
 *   # get xax and xdi into their real slots, via xbx
 *   SAVE_TO_UPCONTEXT %xbx,xbx_OFFSET
 *   mov    fs:xax_OFFSET,%xbx
 *   SAVE_TO_UPCONTEXT %xbx,xax_OFFSET
 *   mov    fs:xdx_OFFSET,%xbx
 *   SAVE_TO_UPCONTEXT %xbx,xdi_OFFSET
 *  endif
 *
 *  # save the current register state to dcontext's mcontext
 *  # xax already in context
 *
 *  if (absolute)
 *    SAVE_TO_UPCONTEXT %xbx,xbx_OFFSET
 *  endif
 *    SAVE_TO_UPCONTEXT %xcx,xcx_OFFSET
 *    SAVE_TO_UPCONTEXT %xdx,xdx_OFFSET
 *  if (absolute || !TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
 *    SAVE_TO_UPCONTEXT %xsi,xsi_OFFSET
 *  endif
 *
 *  # on X86
 *  if (absolute)
 *    SAVE_TO_UPCONTEXT %xdi,xdi_OFFSET
 *  endif
 *    SAVE_TO_UPCONTEXT %xbp,xbp_OFFSET
 *    SAVE_TO_UPCONTEXT %xsp,xsp_OFFSET
 *  ifdef X64
 *    SAVE_TO_UPCONTEXT %r8,r8_OFFSET
 *    SAVE_TO_UPCONTEXT %r9,r9_OFFSET
 *    SAVE_TO_UPCONTEXT %r10,r10_OFFSET
 *    SAVE_TO_UPCONTEXT %r11,r11_OFFSET
 *    SAVE_TO_UPCONTEXT %r12,r12_OFFSET
 *    SAVE_TO_UPCONTEXT %r13,r13_OFFSET
 *    SAVE_TO_UPCONTEXT %r14,r14_OFFSET
 *    SAVE_TO_UPCONTEXT %r15,r15_OFFSET
 *  endif
 */
void
append_save_gpr(dcontext_t *dcontext, instrlist_t *ilist, bool ibl_end, bool absolute,
                generated_code_t *code, linkstub_t *linkstub, bool coarse_info)
{
    if (!absolute) {
        /* get xax and xdi from TLS into their real slots, via xbx */
        APP(ilist, SAVE_TO_DC(dcontext, REG_XBX, XBX_OFFSET));
        APP(ilist, RESTORE_FROM_TLS(dcontext, REG_XBX, DIRECT_STUB_SPILL_SLOT));
        if (linkstub != NULL) {
            /* app xax is still in  %xax, src info is in %xcx, while target pc
             * is now in %xbx
             */
            APP(ilist, SAVE_TO_DC(dcontext, REG_XAX, XAX_OFFSET));
            APP(ilist, SAVE_TO_DC(dcontext, REG_XBX, NEXT_TAG_OFFSET));
            APP(ilist,
                INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XAX),
                                     OPND_CREATE_INTPTR((ptr_int_t)linkstub)));
            if (coarse_info) {
                APP(ilist, SAVE_TO_DC(dcontext, REG_XCX, COARSE_DIR_EXIT_OFFSET));
#ifdef X64
                /* XXX: there are a few ways to perhaps make this a little
                 * cleaner: maybe a restore_indirect_branch_spill() or sthg,
                 * and IBL_REG to indirect xcx.
                 */
                if (GENCODE_IS_X86_TO_X64(code->gencode_mode) &&
                    DYNAMO_OPTION(x86_to_x64_ibl_opt))
                    APP(ilist, RESTORE_FROM_REG(dcontext, REG_XCX, REG_R9));
                else {
#endif /* X64 */
                    APP(ilist,
                        RESTORE_FROM_TLS(dcontext, REG_XCX, MANGLE_XCX_SPILL_SLOT));
#ifdef X64
                }
#endif
            }
        } else {
            APP(ilist, SAVE_TO_DC(dcontext, REG_XBX, XAX_OFFSET));
        }
        APP(ilist, RESTORE_FROM_TLS(dcontext, REG_XBX, DCONTEXT_BASE_SPILL_SLOT));
        APP(ilist, SAVE_TO_DC(dcontext, REG_XBX, XDI_OFFSET));
    }

    /* save the current register state to context->regs
     * xax already in context
     */
    if (!ibl_end) {
        /* for ibl_end, xbx and xcx are already in their dcontext slots */
        if (absolute) /* else xbx saved above */
            APP(ilist, SAVE_TO_DC(dcontext, REG_XBX, XBX_OFFSET));
        APP(ilist, SAVE_TO_DC(dcontext, REG_XCX, XCX_OFFSET));
    }
    APP(ilist, SAVE_TO_DC(dcontext, REG_XDX, XDX_OFFSET));
    if (absolute || !TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
        APP(ilist, SAVE_TO_DC(dcontext, REG_XSI, XSI_OFFSET));
    if (absolute) /* else xdi saved above */
        APP(ilist, SAVE_TO_DC(dcontext, REG_XDI, XDI_OFFSET));
    APP(ilist, SAVE_TO_DC(dcontext, REG_XBP, XBP_OFFSET));
    APP(ilist, SAVE_TO_DC(dcontext, REG_XSP, XSP_OFFSET));
#ifdef X64
    APP(ilist, SAVE_TO_DC(dcontext, REG_R8, R8_OFFSET));
    APP(ilist, SAVE_TO_DC(dcontext, REG_R9, R9_OFFSET));
    APP(ilist, SAVE_TO_DC(dcontext, REG_R10, R10_OFFSET));
    APP(ilist, SAVE_TO_DC(dcontext, REG_R11, R11_OFFSET));
    APP(ilist, SAVE_TO_DC(dcontext, REG_R12, R12_OFFSET));
    APP(ilist, SAVE_TO_DC(dcontext, REG_R13, R13_OFFSET));
    APP(ilist, SAVE_TO_DC(dcontext, REG_R14, R14_OFFSET));
    APP(ilist, SAVE_TO_DC(dcontext, REG_R15, R15_OFFSET));
#endif /* X64 */
}

/* append instructions to save extension registers
 *  if preserve_xmm_caller_saved
 *    SAVE_TO_UPCONTEXT %xmm0,xmm_OFFSET+0*16
 *    SAVE_TO_UPCONTEXT %xmm1,xmm_OFFSET+1*16
 *    SAVE_TO_UPCONTEXT %xmm2,xmm_OFFSET+2*16
 *    SAVE_TO_UPCONTEXT %xmm3,xmm_OFFSET+3*16
 *    SAVE_TO_UPCONTEXT %xmm4,xmm_OFFSET+4*16
 *    SAVE_TO_UPCONTEXT %xmm5,xmm_OFFSET+5*16
 *    SAVE_TO_UPCONTEXT %xmm6,xmm_OFFSET+6*16  # 32-bit Linux
 *    SAVE_TO_UPCONTEXT %xmm7,xmm_OFFSET+7*16  # 32-bit Linux
 *  endif
 */
void
append_save_simd_reg(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    /* No processor will support AVX-512 but no SSE/AVX. */
    ASSERT(preserve_xmm_caller_saved() || !ZMM_ENABLED());
    if (!preserve_xmm_caller_saved())
        return;
    /* PR 264138: we must preserve xmm0-5 if on a 64-bit kernel.
     * Rather than try and optimize we save/restore on every cxt
     * sw.  The xmm field is aligned, so we can use movdqa/movaps,
     * though movdqu is stated to be as fast as movdqa when aligned:
     * but if so, why have two versions?  Is it only loads and not stores
     * for which that is true?  => PR 266305.
     * It's not clear that movdqa is any faster (and its opcode is longer):
     * movdqa and movaps are listed as the same latency and throughput in
     * the AMD optimization guide.  Yet examples of fast memcpy online seem
     * to use movdqa when sse2 is available.
     * Note that mov[au]p[sd] and movdq[au] are functionally equivalent.
     */
    /* FIXME i#438: once have SandyBridge processor need to measure
     * cost of vmovdqu and whether worth arranging 32-byte alignment
     */
    int i;
    uint opcode = move_mm_reg_opcode(true /*align16*/, true /*align32*/);
    ASSERT(proc_has_feature(FEATURE_SSE));
    instr_t *post_save = NULL;
    instr_t *pre_avx512_save = NULL;
    if (ZMM_ENABLED()) {
        post_save = INSTR_CREATE_label(dcontext);
        pre_avx512_save = INSTR_CREATE_label(dcontext);
        APP(ilist,
            INSTR_CREATE_cmp(
                dcontext,
                OPND_CREATE_ABSMEM(
                    vmcode_get_executable_addr((byte *)d_r_avx512_code_in_use), OPSZ_1),
                OPND_CREATE_INT8(0)));
        APP(ilist,
            INSTR_CREATE_jcc(dcontext, OP_jnz, opnd_create_instr(pre_avx512_save)));
    }
    for (i = 0; i < proc_num_simd_sse_avx_saved(); i++) {
        APP(ilist,
            instr_create_1dst_1src(dcontext, opcode,
                                   OPND_DC_FIELD(absolute, dcontext, OPSZ_SAVED_XMM,
                                                 SIMD_OFFSET + i * MCXT_SIMD_SLOT_SIZE),
                                   opnd_create_reg(REG_SAVED_XMM0 + (reg_id_t)i)));
    }
    if (ZMM_ENABLED()) {
        APP(ilist, INSTR_CREATE_jmp(dcontext, opnd_create_instr(post_save)));
        APP(ilist, pre_avx512_save /*label*/);
        uint opcode_avx512 = move_mm_avx512_reg_opcode(true /*align64*/);
        for (i = 0; i < proc_num_simd_registers(); i++) {
            APP(ilist,
                instr_create_1dst_2src(
                    dcontext, opcode_avx512,
                    OPND_DC_FIELD(absolute, dcontext, OPSZ_SAVED_ZMM,
                                  SIMD_OFFSET + i * MCXT_SIMD_SLOT_SIZE),
                    opnd_create_reg(DR_REG_K0),
                    opnd_create_reg(DR_REG_START_ZMM + (reg_id_t)i)));
        }
        for (i = 0; i < proc_num_opmask_registers(); i++) {
            APP(ilist,
                instr_create_1dst_1src(
                    dcontext, proc_has_feature(FEATURE_AVX512BW) ? OP_kmovq : OP_kmovw,
                    OPND_DC_FIELD(absolute, dcontext, OPSZ_SAVED_OPMASK,
                                  OPMASK_OFFSET + i * OPMASK_AVX512BW_REG_SIZE),
                    opnd_create_reg(DR_REG_START_OPMASK + (reg_id_t)i)));
        }
        APP(ilist, post_save /*label*/);
    }
}

/* append instructions to save xflags and clear it
 *  # now save eflags -- too hard to do without a stack on X86!
 *  pushf           # push eflags on stack
 *  pop     %xbx    # grab eflags value
 *  SAVE_TO_UPCONTEXT %xbx,xflags_OFFSET # save eflags value
 *
 *  # clear eflags now to avoid app's eflags messing up our ENTER_DR_HOOK
 *  # FIXME: this won't work at CPL0 if we ever run there!
 *  push  0
 *  popf
 */
void
append_save_clear_xflags(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    reg_id_t reg = IF_X86_ELSE(REG_XBX, DR_REG_R1);
#ifdef X86
    APP(ilist, INSTR_CREATE_RAW_pushf(dcontext));
    APP(ilist, INSTR_CREATE_pop(dcontext, opnd_create_reg(reg)));
#elif defined(ARM)
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
#endif /* X86/ARM */
    APP(ilist, SAVE_TO_DC(dcontext, reg, XFLAGS_OFFSET));

    /* clear eflags now to avoid app's eflags (namely an app std)
     * messing up our ENTER_DR_HOOK
     */
#ifdef X86
    /* on x64 a push immed is sign-extended to 64-bit */
    /* XXX i#1147: can we clear DF and IF only? */
    APP(ilist, INSTR_CREATE_push_imm(dcontext, OPND_CREATE_INT8(0)));
    APP(ilist, INSTR_CREATE_RAW_popf(dcontext));
#elif defined(ARM)
    /* i#1551: do nothing on ARM, no DF or IF on ARM */
#endif /* X86/ARM */
}

/* append instructions to call enter_dr_hooks
 * # X86 only
 *  if (ENTER_DR_HOOK != NULL && !dcontext->ignore_enterexit)
 *      # don't bother to save any registers around call except for xax
 *      # and xcx, which holds next_tag
 *      push    %xcx
 *    if (!absolute)
 *      push    %xdi
 *      push    %xsi
 *    endif
 *      push    %xax
 *    if (absolute)
 *      # support for skipping the hook (note: 32-bits even on x64)
 *      RESTORE_FROM_UPCONTEXT ignore_enterexit_OFFSET,%edi
 *      cmp     %edi,0
 *      jnz     post_hook
 *    endif
 *    # for x64 windows, reserve 32 bytes stack space for call prior to call
 *    call    ENTER_DR_HOOK
 *   post_hook:
 *    pop     %xax
 *    if (!absolute)
 *      pop     %xsi
 *      pop     %xdi
 *    endif
 *      pop     %xcx
 *  endif
 */
bool
append_call_enter_dr_hook(dcontext_t *dcontext, instrlist_t *ilist, bool ibl_end,
                          bool absolute)
{
    bool instr_target = false;
    if (ENTER_DR_HOOK != NULL) {
        /* xax is only reg we need to save around the call.
         * we could move to a callee-saved register instead of pushing.
         */
        instr_t *post_hook = INSTR_CREATE_label(dcontext);
        if (ibl_end) {
            /* also save xcx, which holds next_tag */
            APP(ilist, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_XCX)));
        }
        if (!absolute) {
            /* save xdi and xsi around call */
            APP(ilist, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_XDI)));
            APP(ilist, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_XSI)));
        }
        APP(ilist, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_XAX)));
#ifdef WINDOWS
        if (absolute) {
            /* For thread-private (used for syscalls), don't call if
             * dcontext->ignore_enterexit.  This is a perf hit to check: could
             * instead have a space hit via a separate routine.  This is only
             * needed right now for NtSuspendThread handling (see case 4942).
             */
            APP(ilist, RESTORE_FROM_DC(dcontext, REG_EDI, IGNORE_ENTEREXIT_OFFSET));
            /* P4 opt guide says to use test to cmp reg with 0: shorter instr */
            APP(ilist,
                INSTR_CREATE_test(dcontext, opnd_create_reg(REG_EDI),
                                  opnd_create_reg(REG_EDI)));
            APP(ilist, INSTR_CREATE_jcc(dcontext, OP_jnz, opnd_create_instr(post_hook)));
            instr_target = true;
        }
#endif /* WINDOWS */
        /* make sure to use dr_insert_call() rather than a raw OP_call instr,
         * since x64 windows requires 32 bytes of stack space even w/ no args,
         * and we don't want anyone clobbering our pushed registers!
         */
        dr_insert_call((void *)dcontext, ilist, NULL /*append*/, (void *)ENTER_DR_HOOK,
                       0);
        APP(ilist, post_hook /*label*/);
        APP(ilist, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_XAX)));
        if (!absolute) {
            /* save xdi and xsi around call */
            APP(ilist, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_XSI)));
            APP(ilist, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_XDI)));
        }
        if (ibl_end) {
            APP(ilist, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_XCX)));

            /* now we can store next tag */
            APP(ilist, SAVE_TO_DC(dcontext, REG_XCX, NEXT_TAG_OFFSET));
        }
    }
    return instr_target;
}

/* saves the eflags
 * uses the xax slot, either in TLS
 * memory if tls is true; else using mcontext accessed using absolute address if
 * absolute is true, else off xdi.
 * MUST NOT clobber xax between this call and the restore call!
 */
void
insert_save_eflags(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where, uint flags,
                   bool tls, bool absolute _IF_X64(bool x86_to_x64_ibl_opt))
{
    /* no support for absolute addresses on x64: we always use tls/reg */
    IF_X64(ASSERT_NOT_IMPLEMENTED(!absolute));

    if (TEST(FRAG_WRITES_EFLAGS_6, flags)) /* no flag save needed */
        return;
    /* save the flags */
    /*>>>    SAVE_TO_TLS/UPCONTEXT %xax,xax_tls_slot/xax_OFFSET  */
    /*>>>    lahf                                                */
    /*>>>    seto        %al                                     */
    /* for shared ibl targets we put eflags in tls -- else, we use mcontext,
     * either absolute address or indirected via xdi as specified by absolute param
     */
    if (IF_X64_ELSE(x86_to_x64_ibl_opt, false)) {
        /* in x86_to_x64, spill rax to r8 */
        PRE(ilist, where, SAVE_TO_REG(dcontext, REG_XAX, REG_R8));
    } else if (tls) {
        /* We need to save this in an easy location for the prefixes
         * to restore from.  FIXME: This can be much more streamlined
         * if TLS_SLOT_SCRATCH1 was the XAX spill slot for everyone
         */
        /* FIXME: since the prefixes are trying to be smart now
         * based on shared/privateness of the fragment, we also need
         * to know what would the target do if shared.
         */
        /*>>>    SAVE_TO_TLS %xax,xax_tls_slot                       */
        PRE(ilist, where, SAVE_TO_TLS(dcontext, REG_XAX, PREFIX_XAX_SPILL_SLOT));
    } else {
        /*>>>    SAVE_TO_UPCONTEXT %xax,xax_OFFSET                       */
        PRE(ilist, where, SAVE_TO_DC(dcontext, REG_XAX, XAX_OFFSET));
    }
    PRE(ilist, where, INSTR_CREATE_lahf(dcontext));
    if (!TEST(FRAG_WRITES_EFLAGS_OF, flags) &&
        !INTERNAL_OPTION(unsafe_ignore_overflow)) { /* OF needs saving */
        /* Move OF flags into the OF flag spill slot. */
        PRE(ilist, where, INSTR_CREATE_setcc(dcontext, OP_seto, opnd_create_reg(REG_AL)));
    }
}

/* restores eflags from xax and the xax app value from the xax slot, either in
 * TLS memory if tls is true; else using mcontext accessed using absolute
 * address if absolute is true, else off xdi.
 * also restores xax
 */
void
insert_restore_eflags(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                      uint flags, bool tls,
                      bool absolute _IF_X64(bool x86_to_x64_ibl_opt))
{
    /* no support for absolute addresses on x64: we always use tls/reg */
    IF_X64(ASSERT_NOT_IMPLEMENTED(!absolute));

    if (TEST(FRAG_WRITES_EFLAGS_6, flags)) /* no flag save was done */
        return;
    if (!TEST(FRAG_WRITES_EFLAGS_OF, flags) && /* OF was saved */
        !INTERNAL_OPTION(unsafe_ignore_overflow)) {
        /* restore OF using add that overflows and sets OF
         * if OF was on when we did seto
         */
        PRE(ilist, where,
            INSTR_CREATE_add(dcontext, opnd_create_reg(REG_AL), OPND_CREATE_INT8(0x7f)));
    }
    /* restore other 5 flags (still in xax at this point) */
    /*>>>    sahf                                                    */
    PRE(ilist, where, INSTR_CREATE_sahf(dcontext));
    /* restore xax */
    if (IF_X64_ELSE(x86_to_x64_ibl_opt, false)) {
        PRE(ilist, where, RESTORE_FROM_REG(dcontext, REG_XAX, REG_R8));
    } else if (tls) {
        PRE(ilist, where, RESTORE_FROM_TLS(dcontext, REG_XAX, PREFIX_XAX_SPILL_SLOT));
    } else {
        /*>>>    RESTORE_FROM_UPCONTEXT xax_OFFSET,%xax                  */
        PRE(ilist, where, RESTORE_FROM_DC(dcontext, REG_XAX, XAX_OFFSET));
    }
}

/*******************************************************************************/

#define GET_IB_FTABLE(ibl_code, target_trace_table, field)                 \
    (GET_IBL_TARGET_TABLE((ibl_code)->branch_type, (target_trace_table)) + \
     offsetof(ibl_table_t, field))

#define HASHLOOKUP_TAG_OFFS (offsetof(fragment_entry_t, tag_fragment))
#define HASHLOOKUP_START_PC_OFFS (offsetof(fragment_entry_t, start_pc_fragment))

/* When inline_ibl_head, this emits the inlined lookup for the exit stub.
 *   Only assumption is that xcx = effective address of indirect branch
 * Else, this emits the top of the shared lookup routine, which assumes:
 *   1) xbx = &linkstub
 *   2) xcx = effective address of indirect branch
 * Assumes that a jne_short is sufficient to reach miss_tgt.
 * Returns pointers to three instructions, for use in calculating offsets
 * and in pointing jmps inside the ibl head.
 * It's fine to pass NULL if you're not interested in them.
 */
void
append_ibl_head(dcontext_t *dcontext, instrlist_t *ilist, ibl_code_t *ibl_code,
                patch_list_t *patch, instr_t **fragment_found, instr_t **compare_tag_inst,
                instr_t **post_eflags_save, opnd_t miss_tgt, bool miss_8bit,
                bool target_trace_table, bool inline_ibl_head)
{
    instr_t *mask, *table = NULL, *compare_tag = NULL, *after_linkcount;
    opnd_t mask_opnd;
    bool absolute = !ibl_code->thread_shared_routine;
    bool table_in_tls = SHARED_IB_TARGETS() &&
        (target_trace_table || SHARED_BB_ONLY_IB_TARGETS()) &&
        DYNAMO_OPTION(ibl_table_in_tls);
    uint hash_to_address_factor;
    /* Use TLS only for spilling app state -- registers & flags */
    bool only_spill_state_in_tls = !absolute && !table_in_tls;
    IF_X64(bool x86_to_x64_ibl_opt =
               ibl_code->x86_to_x64_mode && DYNAMO_OPTION(x86_to_x64_ibl_opt);)

    /* no support for absolute addresses on x64: we always use tls/reg */
    IF_X64(ASSERT_NOT_IMPLEMENTED(!absolute));

#ifndef X64
    /* For x64 we need this after the cmp post-eflags entry; for x86, it's
     * needed before for thread-private eflags save.
     */
    if (only_spill_state_in_tls) {
        /* grab dcontext in XDI for thread shared routine */
        insert_shared_get_dcontext(dcontext, ilist, NULL, true /* save xdi to scratch */);
    }
#endif
    if (!INTERNAL_OPTION(unsafe_ignore_eflags_ibl)) {
        /* There are ways to generate IBL that doesn't touch the EFLAGS -- see
         * case 7169. We're not using any of those techniques, so we save the
         * flags.
         */
        insert_save_eflags(dcontext, ilist, NULL, 0, IBL_EFLAGS_IN_TLS(),
                           absolute _IF_X64(x86_to_x64_ibl_opt));
    }
    /* PR 245832: x64 trace cmp saves flags so we need an entry point post-flags-save */
    if (post_eflags_save != NULL) {
        *post_eflags_save = INSTR_CREATE_label(dcontext);
        APP(ilist, *post_eflags_save);
    }
#ifdef X64
    /* See comments above */
    if (only_spill_state_in_tls) {
        /* grab dcontext in XDI for thread shared routine */
        insert_shared_get_dcontext(dcontext, ilist, NULL, true /* save xdi to scratch */);
    }
#endif
    if (IF_X64_ELSE(x86_to_x64_ibl_opt, false)) {
        after_linkcount = SAVE_TO_REG(dcontext, SCRATCH_REG1, REG_R10);
    } else if (inline_ibl_head || !DYNAMO_OPTION(indirect_stubs)) {
        /*>>>    SAVE_TO_UPCONTEXT %xbx,xbx_OFFSET                       */
        if (absolute)
            after_linkcount = SAVE_TO_DC(dcontext, SCRATCH_REG1, SCRATCH_REG1_OFFS);
        else
            after_linkcount = SAVE_TO_TLS(dcontext, SCRATCH_REG1, TLS_REG1_SLOT);
    } else {
        /* create scratch register: re-use xbx, it holds linkstub ptr,
         * don't need to restore it on hit!  save to **xdi** slot so as
         * to not overwrite linkstub ptr */
        /*>>>    SAVE_TO_UPCONTEXT %xbx,xdi_OFFSET                       */
        if (absolute)
            after_linkcount = SAVE_TO_DC(dcontext, SCRATCH_REG1, SCRATCH_REG5_OFFS);
        else if (table_in_tls) /* xdx is the free slot for tls */
            after_linkcount = SAVE_TO_TLS(dcontext, SCRATCH_REG1, TLS_REG3_SLOT);
        else /* the xdx slot already holds %xdi so use the mcontext */
            after_linkcount = SAVE_TO_DC(dcontext, SCRATCH_REG1, SCRATCH_REG5_OFFS);
    }
    APP(ilist, after_linkcount);
    if (ibl_code->thread_shared_routine && !DYNAMO_OPTION(private_ib_in_tls)) {
        /* copy app xcx currently in tls slot into mcontext slot, so that we
         * can work with both tls and mcontext prefixes
         * do not need this if using all-tls (private_ib_in_tls option)
         */
        /* xbx is now dead, just saved it */
#ifdef X64
        if (x86_to_x64_ibl_opt)
            APP(ilist, RESTORE_FROM_REG(dcontext, SCRATCH_REG1, REG_R9));
        else
#endif
            APP(ilist, RESTORE_FROM_TLS(dcontext, SCRATCH_REG1, MANGLE_XCX_SPILL_SLOT));
        APP(ilist, SAVE_TO_DC(dcontext, SCRATCH_REG1, SCRATCH_REG2_OFFS));
    }
    /* make a copy of the tag for hashing
     * keep original in xbx, hash will be in xcx
     *>>>    mov     %xcx,%xbx                                       */
    APP(ilist,
        XINST_CREATE_load(dcontext, opnd_create_reg(SCRATCH_REG1),
                          opnd_create_reg(SCRATCH_REG2)));

    if (only_spill_state_in_tls) {
        /* grab the per_thread_t into XDI - can't use SAVE_TO_DC after this */
        /* >>> mov  %xdi, fragment_field(%xdi) */
        /*   TODO:  make this an 8bit offset, currently it is a 32bit one
             8b bf 94 00 00 00    mov    0x94(%xdi) -> %xdi
        */
        APP(ilist,
            XINST_CREATE_load(
                dcontext, opnd_create_reg(SCRATCH_REG5),
                OPND_DC_FIELD(absolute, dcontext, OPSZ_PTR, FRAGMENT_FIELD_OFFSET)));
        /* TODO: should have a flag that SAVE_TO_DC can ASSERT(valid_DC_in_reg) */
    }
    /* hash function = (tag & mask) */
    if (!absolute && table_in_tls) {
        /* mask is in tls */
        mask_opnd = OPND_TLS_FIELD(TLS_MASK_SLOT(ibl_code->branch_type));
    } else if (!absolute) {
        ASSERT(only_spill_state_in_tls);
        /* this is an offset in per_thread_t so should fit in 32 bits */
        IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_int(
            GET_IB_FTABLE(ibl_code, target_trace_table, hash_mask))));
        mask_opnd = opnd_create_base_disp(
            SCRATCH_REG5, REG_NULL, 0,
            (int)GET_IB_FTABLE(ibl_code, target_trace_table, hash_mask), OPSZ_PTR);
    } else {
        /* mask not created yet, use 0x3fff for now
         * if we did need to support an immediate for x64, we could
         * just use the lower 32 bits and let them be sign-extended.
         *>>>    andl    $0x3fff,%xcx                                    */
        mask_opnd = opnd_create_immed_int(0x3fff, OPSZ_4);
    }
    mask = INSTR_CREATE_and(dcontext, opnd_create_reg(SCRATCH_REG2), mask_opnd);
    APP(ilist, mask);
    if (absolute) {
        add_patch_entry(
            patch, mask, PATCH_PER_THREAD,
            (ptr_uint_t)GET_IB_FTABLE(ibl_code, target_trace_table, hash_mask));
    }

    /* load from lookup hash table tag and start_pc */
    /* simply   cmp     BOGUS_HASH_TABLE(,%xcx,8),%xcx   # tag */
    /*          jne     next_fragment    */
    /*          jmp     *FRAGMENT_START_PC_OFFS(4,%xdx,3)# pc */
    /* or better yet */
    /*  lea     BOGUS_HASH_TABLE(,%xcx,8),%xcx   # xcx  = &lookuptable[hash]    */
    /*  cmp     HASHLOOKUP_TAG_OFFS(%xcx),%xbx   # tag           _cache line 1_ */
    /*  jne     next_fragment    */
    /*  jmp     *HASHLOOKUP_START_PC_OFFS(%xcx)  # pc            _cache line 1_ */

    /* *>>>    lea    BOGUS_HASH_TABLE(,%xcx,8),%xcx
       not created yet, use 0 */

    if (only_spill_state_in_tls) {
        /* grab the corresponding table or lookuptable for trace into XDI */
        /* >>> mov  %xdi, lookuptable(%xdi) */
        /* 8b 7f 40             mov    0x40(%xdi) -> %xdi
         */

        instr_t *table_in_xdi;
        /* this is an offset in per_thread_t so should fit in 32 bits */
        IF_X64(ASSERT(
            CHECK_TRUNCATE_TYPE_int(GET_IB_FTABLE(ibl_code, target_trace_table, table))));
        table_in_xdi = XINST_CREATE_load(
            dcontext, opnd_create_reg(SCRATCH_REG5),
            opnd_create_base_disp(SCRATCH_REG5, REG_NULL, 0,
                                  (int)GET_IB_FTABLE(ibl_code, target_trace_table, table),
                                  OPSZ_PTR));
        /* lookuptable can still be reloaded from XDI later at sentinel_check */
        APP(ilist, table_in_xdi);
    }

    if (absolute) {
        ASSERT(sizeof(fragment_entry_t) == 8); /* x64 not supported */
        if (HASHTABLE_IBL_OFFSET(ibl_code->branch_type) <= IBL_HASH_FUNC_OFFSET_MAX) {
            /* multiply by 16,8,4,2 or 1 respectively when we offset 0,1,2,3,4 bits */
            IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(
                sizeof(fragment_entry_t) /
                (size_t)(1 << HASHTABLE_IBL_OFFSET(ibl_code->branch_type)))));
            hash_to_address_factor = (uint)sizeof(fragment_entry_t) /
                (1 << HASHTABLE_IBL_OFFSET(ibl_code->branch_type));
        } else {
            /* FIXME: we'll need to shift right a few more bits */
            /* >>>   shrl  factor-3, %xcx */
            ASSERT_NOT_IMPLEMENTED(false);
            hash_to_address_factor = 1;
        }
        /* FIXME: there is no good way to ASSERT that the table we're looking up
         * is using the correct hash_mask_offset
         */

        /* FIXME: case 4893: three ADD's are faster than one LEA - if IBL
         * head is not inlined we may want to try that advice
         */
        /* FIXME: case 4893 when hash_mask_offset==3 we can use a better encoding
         * since we don't need an index register we can switch to a non-SIB encoding
         * so that instead of 7 bytes we have 6 byte encoding going through the fast
         * decoder
         * 8d 0c 0d 5039721c   lea     xcx,[1c723950+xcx]   ; currently
         * 8d 89 __ 5039721c   lea     xcx,[xcx+0x1c723950] ; shorter
         */
        table =
            INSTR_CREATE_lea(dcontext, opnd_create_reg(SCRATCH_REG2),
                             opnd_create_base_disp(REG_NULL, SCRATCH_REG2,
                                                   hash_to_address_factor, 0, OPSZ_lea));
        add_patch_entry(patch, table, PATCH_PER_THREAD,
                        GET_IB_FTABLE(ibl_code, target_trace_table, table));
        APP(ilist, table);
    } else { /* !absolute && (table_in_tls  || only_spill_state_in_tls) */
        /* we have the base added separately already, so we skip the lea and
         * use faster and smaller add sequences for our shift
         */
        uint i;
        opnd_t table_opnd;

        ASSERT_NOT_IMPLEMENTED(HASHTABLE_IBL_OFFSET(ibl_code->branch_type) <=
                               IBL_HASH_FUNC_OFFSET_MAX);
        /* are 4 adds faster than 1 lea, for x64? */
        for (i = IBL_HASH_FUNC_OFFSET_MAX;
             i > HASHTABLE_IBL_OFFSET(ibl_code->branch_type); i--) {
            APP(ilist,
                INSTR_CREATE_add(dcontext, opnd_create_reg(SCRATCH_REG2),
                                 opnd_create_reg(SCRATCH_REG2)));
        }
        /* we separately add the lookuptable base since we'd need an extra register
         * to do it in combination with the shift:
         *    add   fs:lookuptable,%xcx -> %xcx
         * or if the table addr is in %xdi:
         *    add   %xdi,%xcx -> %xcx
         */
        table_opnd = table_in_tls
            ? OPND_TLS_FIELD(TLS_TABLE_SLOT(ibl_code->branch_type)) /* addr in TLS */
            : opnd_create_reg(SCRATCH_REG5) /* addr in %xdi */;
        APP(ilist, INSTR_CREATE_add(dcontext, opnd_create_reg(SCRATCH_REG2), table_opnd));
    }

    /* compare tags, empty slot is not 0, instead is a constant frag w/ tag 0
     *>>>    cmp     HASHLOOKUP_TAG_OFFS(%xcx),%xbx                    */
    compare_tag =
        INSTR_CREATE_cmp(dcontext, OPND_CREATE_MEMPTR(SCRATCH_REG2, HASHLOOKUP_TAG_OFFS),
                         opnd_create_reg(SCRATCH_REG1));
    APP(ilist, compare_tag);

    /*>>>    jne     next_fragment                                   */
    if (miss_8bit)
        APP(ilist, INSTR_CREATE_jcc(dcontext, OP_jne_short, miss_tgt));
    else
        APP(ilist, INSTR_CREATE_jcc(dcontext, OP_jne, miss_tgt));

#ifdef X64
    if (ibl_code->x86_mode) {
        /* Currently we're using the x64 table, so we have to ensure the top
         * bits are 0 before we declare it a match (xref PR 283895).
         */
        APP(ilist,
            INSTR_CREATE_cmp(dcontext,
                             OPND_CREATE_MEM32(SCRATCH_REG2, HASHLOOKUP_TAG_OFFS + 4),
                             OPND_CREATE_INT32(0)));
        if (miss_8bit)
            APP(ilist, INSTR_CREATE_jcc(dcontext, OP_jne_short, miss_tgt));
        else
            APP(ilist, INSTR_CREATE_jcc(dcontext, OP_jne, miss_tgt));
    }
#endif

#define HEAD_START_PC_OFFS HASHLOOKUP_START_PC_OFFS
    append_ibl_found(dcontext, ilist, ibl_code, patch, HEAD_START_PC_OFFS, false,
                     only_spill_state_in_tls,
                     target_trace_table ? DYNAMO_OPTION(trace_single_restore_prefix)
                                        : DYNAMO_OPTION(bb_single_restore_prefix),
                     fragment_found);
#undef HEAD_START_PC_OFFS

    if (compare_tag_inst != NULL)
        *compare_tag_inst = compare_tag;
}

/* create the inlined ibl exit stub template
 *
hit path (shared_syscall remains as before):
  if (!INTERNAL_OPTION(unsafe_ignore_eflags_ibl)) {
  | 5   movl  %eax,eax_OFFSET
  | 1   lahf
  | 3   seto  %al
  }
    6   movl  %ebx, ebx_offs(&dcontext)
    2   movl  %ecx,%ebx                 # tag in ecx, hash will be in ebx
    6   andl  $0x3fff,%ecx              # hash the tag
    7   movl  ftable(,%ecx,4),%ecx      # ecx = ftable[hash]
        # empty slot is not 0, instead is a constant frag w/ tag 0
    2   cmpl  FRAGMENT_TAG_OFFS(%ecx),%ebx
    2   jne   miss # if !DYNAMO_OPTION(indirect_stubs), jne ibl
    6   movl  ebx_offs(&dcontext),%ebx
    3   jmp   *FRAGMENT_START_PC_OFFS(%ecx)
unlinked entry point into stub:
 if (!DYNAMO_OPTION(indirect_stubs)) {
     5  jmp   unlinked_ib_lookup  # we can eliminate this if we store stub pc
 } else {
  if (DYNAMO_OPTION(atomic_inlined_linking)) {
        # duplicate miss path so linking can be atomic
    10  movl  &linkstub, edi_offs(&dcontext)
    5   jmp   unlinked_ib_lookup
  } else {
        # set flag in ecx (bottom byte = 0x1) so that unlinked path can
        # detect race condition during unlinking
    6   movl  %ecx, ebx_offs(&dcontext)
    2   movb  $0x1, %ecx
  }
miss:
    10  movl  &linkstub, edi_offs(&dcontext)
    5   jmp   indirect_branch_lookup/(if !atomic_inlined_linking)unlinked_ib_lookup
 }
*/
byte *
emit_inline_ibl_stub(dcontext_t *dcontext, byte *pc, ibl_code_t *ibl_code,
                     bool target_trace_table)
{
    /* careful -- we're called in middle of setting up code fields, so don't go
     * reading any without making sure they're initialized first
     */
    instrlist_t ilist;
    instr_t *miss = NULL, *unlink, *after_unlink = NULL;

    patch_list_t *patch = &ibl_code->ibl_stub_patch;
    byte *unlinked_ibl_pc = ibl_code->unlinked_ibl_entry;
    byte *linked_ibl_pc = ibl_code->indirect_branch_lookup_routine;

    bool absolute = !ibl_code->thread_shared_routine;

    /* PR 248207: haven't updated the inlining to be x64-compliant yet.
     * Keep in mind PR 257963: trace inline cmp needs separate entry. */
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));
    /* no support for absolute addresses on x64: we always use tls/reg */
    IF_X64(ASSERT_NOT_IMPLEMENTED(!absolute));

    ibl_code->inline_ibl_stub_template = pc;
    ibl_code->ibl_head_is_inlined = true;

    /* initialize the ilist and the patch list */
    instrlist_init(&ilist);
    /* FIXME: for !absolute need to optimize to PATCH_TYPE_INDIRECT_FS */
    init_patch_list(patch, absolute ? PATCH_TYPE_ABSOLUTE : PATCH_TYPE_INDIRECT_XDI);

    /*
        <head>
unlinked entry point into stub:
  if (DYNAMO_OPTION(atomic_inlined_linking)) {
        # duplicate miss path so linking can be atomic
    10  movl  &linkstub, edi_offs(&dcontext)
    5   jmp   unlinked_ib_lookup
  } else {
        # set flag in ecx (bottom byte = 0x1) so that unlinked path can
        # detect race condition during unlinking
    6   movl  %ecx, ebx_offs(&dcontext)
    2   movb  $0x1, %ecx
  }
miss:
    10  movl  &linkstub, edi_offs(&dcontext)
    5   jmp   indirect_branch_lookup/(if !atomic_inlined_linking)unlinked_ib_lookup
    */

    if (DYNAMO_OPTION(indirect_stubs)) {
        if (absolute) {
            miss = XINST_CREATE_store(
                dcontext, opnd_create_dcontext_field(dcontext, SCRATCH_REG5_OFFS),
                OPND_CREATE_INT32(0));
        } else {
            miss = XINST_CREATE_store(dcontext, OPND_TLS_FIELD(TLS_REG3_SLOT),
                                      OPND_CREATE_INT32(0));
        }
        append_ibl_head(dcontext, &ilist, ibl_code, patch, NULL, NULL, NULL,
                        opnd_create_instr(miss), true /*miss can have 8-bit offs*/,
                        target_trace_table, true /* inline of course */);

        /*>>>    SAVE_TO_UPCONTEXT %ebx,ebx_OFFSET                       */
        /*>>>    SAVE_TO_UPCONTEXT &linkstub,edx_OFFSET                  */
        /*>>>    jmp     unlinked_ib_lookup                              */
        if (DYNAMO_OPTION(atomic_inlined_linking)) {
            if (absolute) {
                unlink = SAVE_TO_DC(dcontext, SCRATCH_REG1, SCRATCH_REG1_OFFS);
                after_unlink = XINST_CREATE_store(
                    dcontext, opnd_create_dcontext_field(dcontext, SCRATCH_REG5_OFFS),
                    OPND_CREATE_INT32(0));
            } else {
                unlink = SAVE_TO_TLS(dcontext, SCRATCH_REG1, TLS_REG1_SLOT);
                after_unlink = XINST_CREATE_store(dcontext, OPND_TLS_FIELD(TLS_REG3_SLOT),
                                                  OPND_CREATE_INT32(0));
            }
            APP(&ilist, unlink);
            APP(&ilist, after_unlink);
            APP(&ilist, INSTR_CREATE_jmp(dcontext, opnd_create_pc(unlinked_ibl_pc)));
        } else {
            if (absolute)
                unlink = SAVE_TO_DC(dcontext, SCRATCH_REG2, SCRATCH_REG1_OFFS);
            else
                unlink = SAVE_TO_TLS(dcontext, SCRATCH_REG2, TLS_REG1_SLOT);
            APP(&ilist, unlink);
            APP(&ilist,
                XINST_CREATE_load_int(dcontext, opnd_create_reg(REG_CL),
                                      OPND_CREATE_INT8(1)));
        }
        APP(&ilist, miss);
        APP(&ilist,
            INSTR_CREATE_jmp(dcontext,
                             opnd_create_pc(DYNAMO_OPTION(atomic_inlined_linking)
                                                ? linked_ibl_pc
                                                : unlinked_ibl_pc)));

        add_patch_marker(patch, unlink, PATCH_UINT_SIZED /* pc relative */,
                         0 /* beginning of instruction */,
                         (ptr_uint_t *)&ibl_code->inline_unlink_offs);

        if (DYNAMO_OPTION(atomic_inlined_linking)) {
            add_patch_marker(patch, after_unlink, PATCH_UINT_SIZED /* pc relative */,
                             -4 /* grab last 4 bytes of instructions */,
                             (ptr_uint_t *)&ibl_code->inline_linkstub_second_offs);
            add_patch_marker(patch, instr_get_prev(miss),
                             PATCH_UINT_SIZED /* pc relative */, 1 /* skip jmp opcode */,
                             (ptr_uint_t *)&ibl_code->inline_unlinkedjmp_offs);
        }
        add_patch_marker(patch, miss, PATCH_UINT_SIZED /* pc relative */,
                         -4 /* grab offsets that are last 4 bytes of instructions */,
                         (ptr_uint_t *)&ibl_code->inline_linkstub_first_offs);
        add_patch_marker(patch, instrlist_last(&ilist),
                         PATCH_UINT_SIZED /* pc relative */, 1 /* skip jmp opcode */,
                         (ptr_uint_t *)&ibl_code->inline_linkedjmp_offs);
    } else {
        instr_t *cmp;
        append_ibl_head(dcontext, &ilist, ibl_code, patch, NULL, &cmp, NULL,
                        opnd_create_pc(linked_ibl_pc), false /*miss needs 32-bit offs*/,
                        target_trace_table, true /* inline of course */);
        /* FIXME: we'd like to not have this jmp at all and instead have the cti
         * go to the inlined stub when linked and straight to the unlinked ibl
         * entry when unlinked but we haven't put in the support in the link
         * routines (they all assume they can find the unlinked from the current
         * target in a certain manner).
         */
        unlink = INSTR_CREATE_jmp(dcontext, opnd_create_pc(unlinked_ibl_pc));
        APP(&ilist, unlink);
        /* FIXME: w/ private traces and htable stats we have a patch entry
         * inserted inside app_ibl_head (in append_ibl_found) at a later instr
         * than the miss instr.  To fix, we must either put the miss patch point
         * in the middle of the array and shift it over to keep it sorted, or
         * enable patch-encode to handle out-of-order entries (we could mark
         * this with a flag).
         */
#ifdef HASHTABLE_STATISTICS
        ASSERT_NOT_IMPLEMENTED(!absolute || !INTERNAL_OPTION(hashtable_ibl_stats));
#endif
        /* FIXME: cleaner to have append_ibl_head pass back miss instr */
        add_patch_marker(patch, instr_get_next(cmp), PATCH_UINT_SIZED /* pc relative */,
                         2 /* skip jne opcode */,
                         (ptr_uint_t *)&ibl_code->inline_linkedjmp_offs);
        /* FIXME: we would add a patch for inline_unlinkedjmp_offs at unlink+1,
         * but encode_with_patch_list asserts, wanting 1 patch per instr, in order
         */
        add_patch_marker(patch, unlink, PATCH_UINT_SIZED /* pc relative */,
                         0 /* beginning of instruction */,
                         (ptr_uint_t *)&ibl_code->inline_unlink_offs);
    }

    ibl_code->inline_stub_length = encode_with_patch_list(dcontext, patch, &ilist, pc);

    /* free the instrlist_t elements */
    instrlist_clear(dcontext, &ilist);
    return pc + ibl_code->inline_stub_length;
}

/* FIXME: case 5232 where this should really be smart -
 * for now always using jmp rel32 with statistics
 *
 * Use with caution where jmp_short would really work in release - no
 * ASSERTs to help you.
 */
#ifdef HASHTABLE_STATISTICS
#    define INSTR_CREATE_jmp_smart INSTR_CREATE_jmp
#else
#    define INSTR_CREATE_jmp_smart INSTR_CREATE_jmp_short
#endif

/*
# indirect_branch_lookup
#.If the lookup succeeds, control jumps to the fcache target; otherwise
# it sets up for and jumps to fcache_return.

# when we unlink an indirect branch we go through the cleanup part of
# this lookup routine that takes us straight to fcache_return.

# We assume dynamo is NOT in trace creation mode (which would require
# going back to dynamo here).  We assume that when a fragment is
# unlinked its indirect branch exit stubs are redirected to the
# unlinked_* labels below.  Note that even if you did come in here in
# trace creation mode, and we didn't go back to dynamo here, the
# current trace would have ended now (b/c next fragment is a trace),
# so we'd end up possibly adding erroneous fragments to the end of
# the trace but the indirect branch check would ensure they were never
# executed.

# N.B.: a number of optimizations of the miss path are possible by making
# it separate from the unlink path
*/
/* Must have a valid fcache return pc prior to calling this function! */
byte *
emit_indirect_branch_lookup(dcontext_t *dcontext, generated_code_t *code, byte *pc,
                            byte *fcache_return_pc, bool target_trace_table,
                            bool inline_ibl_head, ibl_code_t *ibl_code /* IN/OUT */)
{
    instrlist_t ilist;
    instr_t *fragment_not_found, *unlinked = INSTR_CREATE_label(dcontext);
    instr_t *target_delete_entry;
    patch_list_t *patch = &ibl_code->ibl_patch;
    bool absolute = !ibl_code->thread_shared_routine;
    bool table_in_tls = SHARED_IB_TARGETS() &&
        (target_trace_table || SHARED_BB_ONLY_IB_TARGETS()) &&
        DYNAMO_OPTION(ibl_table_in_tls);
    /* Use TLS only for spilling app state -- registers & flags */
    bool only_spill_state_in_tls = !absolute && !table_in_tls;
#ifdef HASHTABLE_STATISTICS
    /* save app XDI since inc routine uses it */
    bool save_xdi = !absolute && table_in_tls;
#endif
    instr_t *fragment_found;
    instr_t *compare_tag = NULL;
    instr_t *sentinel_check;
    /* for IBL_COARSE_SHARED and !DYNAMO_OPTION(indirect_stubs) */
    const linkstub_t *linkstub = NULL;
    IF_X64(bool x86_to_x64_ibl_opt =
               ibl_code->x86_to_x64_mode && DYNAMO_OPTION(x86_to_x64_ibl_opt);)

    instr_t *next_fragment_nochasing =
        INSTR_CREATE_cmp(dcontext, OPND_CREATE_MEMPTR(SCRATCH_REG2, HASHLOOKUP_TAG_OFFS),
                         OPND_CREATE_INT8(0));

    /* no support for absolute addresses on x64: we always use tls/reg */
    IF_X64(ASSERT_NOT_IMPLEMENTED(!absolute));

    if (ibl_code->source_fragment_type == IBL_COARSE_SHARED ||
        !DYNAMO_OPTION(indirect_stubs)) {
        linkstub = get_ibl_sourceless_linkstub(ibltype_to_linktype(ibl_code->branch_type),
                                               IBL_FRAG_FLAGS(ibl_code));
    }

    /* When the target_delete_entry is reached, all registers contain
     * app state, except for those restored in a prefix. We need to massage
     * the state so that it looks like the fragment_not_found -- IBL miss -
     * path, so we need to restore %xbx. See more on the target_delete_entry
     * below, where the instr is added to the ilist.
     */
#ifdef X64
    if (x86_to_x64_ibl_opt) {
        target_delete_entry = SAVE_TO_REG(dcontext, SCRATCH_REG1, REG_R10);
    } else {
#endif
        target_delete_entry = absolute
            ? instr_create_save_to_dcontext(dcontext, SCRATCH_REG1, SCRATCH_REG1_OFFS)
            : SAVE_TO_TLS(dcontext, SCRATCH_REG1, INDIRECT_STUB_SPILL_SLOT);
#ifdef X64
    }
#endif

    fragment_not_found = XINST_CREATE_load(dcontext, opnd_create_reg(SCRATCH_REG2),
                                           opnd_create_reg(SCRATCH_REG1));

    /* initialize the ilist */
    instrlist_init(&ilist);
    init_patch_list(patch, absolute ? PATCH_TYPE_ABSOLUTE : PATCH_TYPE_INDIRECT_XDI);

    LOG(THREAD, LOG_EMIT, 3,
        "emit_indirect_branch_lookup: pc=" PFX " fcache_return_pc=" PFX "\n"
        "target_trace_table=%d inline_ibl_head=%d absolute=%d\n",
        pc, fcache_return_pc, target_trace_table, inline_ibl_head, absolute);

    if (inline_ibl_head) {
        /* entry point is next_fragment, expects
         * 1) xbx = effective address of indirect branch
         * 2) xcx = &fragment
         * 3) xdx_slot = &linkstub
         */
    } else {
        /* entry point: expects
         * 1) xbx = &linkstub if DYNAMO_OPTION(indirect_stubs),
         *          or src tag if DYNAMO_OPTION(coarse_units)
         * 2) xcx = effective address of indirect branch
         */
#ifdef X64
        instr_t *trace_cmp_entry;
#endif
        append_ibl_head(dcontext, &ilist, ibl_code, patch, &fragment_found, &compare_tag,
                        IF_X64_ELSE(&trace_cmp_entry, NULL),
                        opnd_create_instr(next_fragment_nochasing),
                        true /*miss can have 8-bit offs*/, target_trace_table,
                        inline_ibl_head);
#ifdef X64
        if (IS_IBL_TRACE(ibl_code->source_fragment_type) &&
            !GENCODE_IS_X86(code->gencode_mode)) {
            /* if -unsafe_ignore_eflags_ibl this will equal regular entry */
            add_patch_marker(patch, trace_cmp_entry, PATCH_ASSEMBLE_ABSOLUTE,
                             0 /* beginning of instruction */,
                             (ptr_uint_t *)&ibl_code->trace_cmp_entry);
        }
#endif
    }
    /* next_fragment_nochasing: */
    /*>>>    cmp     $0, HASHLOOKUP_TAG_OFFS(%xcx)                   */
    APP(&ilist, next_fragment_nochasing);

    /* forward reference to sentinel_check */
    if (INTERNAL_OPTION(ibl_sentinel_check)) {
        /* sentinel_check: */
        /* check if at table end sentinel */
        /* One solution would be to compare xcx to
         * &lookuptable[ftable->capacity-1] (sentinel) while it would
         * work great for thread private IBL routines where we can
         * hardcode the address.
         *  >>>a)   cmp     %xcx, HASHLOOKUP_SENTINEL_ADDR
                                  ;; &lookuptable[ftable->capacity-1] (sentinel)
         * For shared routines currently we'd need to walk a few
         * pointers - we could just put that one TLS to avoid pointer
         * chasing.  Yet if we are to have even one extra memory load
         * anyways, it is easier to just store a special start_pc to
         * compare instead
         *  >>>b)   cmp     4x8(%xcx), HASHLOOKUP_SENTINEL_PC
         * Where the expectation is that null_fragment=(0,0) while
         * sentinel_fragment=(0,1) For simplicity we just use b) even
         * in private IBL routines.
         */
        /* we can use 8-bit immed, will be sign-expanded before cmp */
        ASSERT((int)(ptr_int_t)HASHLOOKUP_SENTINEL_START_PC <= INT8_MAX &&
               (int)(ptr_int_t)HASHLOOKUP_SENTINEL_START_PC >= INT8_MIN);
        sentinel_check = INSTR_CREATE_cmp(
            dcontext, OPND_CREATE_MEMPTR(SCRATCH_REG2, HASHLOOKUP_START_PC_OFFS),
            OPND_CREATE_INT8((int)(ptr_int_t)HASHLOOKUP_SENTINEL_START_PC));
    } else {
        /* sentinel handled in C code
         * just exit back to d_r_dispatch
         */
        sentinel_check = fragment_not_found;
    }

    /*>>>    je      sentinel_check                                  */
    /* FIXME: je_short ends up not reaching target for shared inline! */
    APP(&ilist,
        INSTR_CREATE_jcc(dcontext, OP_je_short, opnd_create_instr(sentinel_check)));

    /* For open address hashing xcx = &lookuptable[h]; to get &lt[h+1] just add 8x16
     *   add xcx, 8x16  # no wrap around check, instead rely on a nulltag sentinel entry
     * alternative method of rehashing xbx+4x8 or without checks is also not efficient
     */
#ifdef HASHTABLE_STATISTICS
    if (INTERNAL_OPTION(hashtable_ibl_stats)) {
        if (save_xdi)
            APP(&ilist, SAVE_TO_TLS(dcontext, SCRATCH_REG5, HTABLE_STATS_SPILL_SLOT));
        append_increment_counter(dcontext, &ilist, ibl_code, patch, REG_NULL,
                                 HASHLOOKUP_STAT_OFFS(collision),
                                 REG_NULL); /* no registers dead */
        if (save_xdi) {
            APP(&ilist,
                RESTORE_FROM_TLS(dcontext, SCRATCH_REG5, HTABLE_STATS_SPILL_SLOT));
        }
    }
#endif
    APP(&ilist,
        INSTR_CREATE_lea(dcontext, opnd_create_reg(SCRATCH_REG2),
                         opnd_create_base_disp(SCRATCH_REG2, REG_NULL, 0,
                                               sizeof(fragment_entry_t), OPSZ_lea)));

    if (inline_ibl_head) {
        compare_tag = INSTR_CREATE_cmp(
            dcontext, OPND_CREATE_MEMPTR(SCRATCH_REG2, HASHLOOKUP_TAG_OFFS),
            opnd_create_reg(SCRATCH_REG1));
        APP(&ilist, compare_tag);

        /*  TODO: check whether the static predictor can help here */
        /*  P4OG:2-18 "Use prefix 3E (DS) for taken and 2E (CS) for not taken cbr"
         *  (DS == PREFIX_DATA)
         */
        APP(&ilist,
            INSTR_CREATE_jcc(dcontext, OP_jne_short,
                             opnd_create_instr(next_fragment_nochasing)));

        append_ibl_found(dcontext, &ilist, ibl_code, patch, HASHLOOKUP_START_PC_OFFS,
                         true, only_spill_state_in_tls,
                         target_trace_table ? DYNAMO_OPTION(trace_single_restore_prefix)
                                            : DYNAMO_OPTION(bb_single_restore_prefix),
                         NULL);
    } else {
        /* case 5232: use INSTR_CREATE_jmp_smart,
         * since release builds can use a short jump
         */
        APP(&ilist, INSTR_CREATE_jmp_smart(dcontext, opnd_create_instr(compare_tag)));
    }

    if (INTERNAL_OPTION(ibl_sentinel_check)) {
        /* check if at table end sentinel */
        APP(&ilist, sentinel_check);

        /* not found, if not at end of table sentinel fragment  */
        /*>>>    jne      fragment_not_found                        */
        APP(&ilist,
            INSTR_CREATE_jcc(dcontext, OP_jne_short,
                             opnd_create_instr(fragment_not_found)));

#ifdef HASHTABLE_STATISTICS
        if (INTERNAL_OPTION(hashtable_ibl_stats)) {
            if (save_xdi)
                APP(&ilist, SAVE_TO_TLS(dcontext, SCRATCH_REG5, HTABLE_STATS_SPILL_SLOT));
            append_increment_counter(dcontext, &ilist, ibl_code, patch, REG_NULL,
                                     HASHLOOKUP_STAT_OFFS(overwrap),
                                     REG_NULL); /* no registers dead */
            if (save_xdi) {
                APP(&ilist,
                    RESTORE_FROM_TLS(dcontext, SCRATCH_REG5, HTABLE_STATS_SPILL_SLOT));
            }
        }
#endif

        /* should overwrap to beginning of table, if at end of table sentinel */
        /* for private table */
        /*>>>    mov     BOGUS_HASH_TABLE -> %xcx  ; &lookuptable[0]   */

        /* for shared table - table address should still be preserved in XDI
         *        mov    %xdi -> %xcx        ; xdi == &lookuptable[0]
         */
        if (absolute) {
            /* lookuptable is a patchable immediate */
            enum { BOGUS_HASH_TABLE = 0xabcdabcd };
            instr_t *table = INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(SCRATCH_REG2),
                                                  OPND_CREATE_INT32(BOGUS_HASH_TABLE));
            add_patch_entry(patch, table, PATCH_PER_THREAD,
                            GET_IB_FTABLE(ibl_code, target_trace_table, table));
            APP(&ilist, table);
        } else if (table_in_tls) {
            /* grab lookuptable from tls */
            APP(&ilist,
                RESTORE_FROM_TLS(dcontext, SCRATCH_REG2,
                                 TLS_TABLE_SLOT(ibl_code->branch_type)));
        } else {
#ifdef HASHTABLE_STATISTICS
            if (INTERNAL_OPTION(hashtable_ibl_stats)) {
                /* The hash stats inc routine clobbers XDI so we need to reload
                 * it and then reload per_thread_t* and then the table*. */
                insert_shared_get_dcontext(dcontext, &ilist, NULL, false);
                APP(&ilist,
                    XINST_CREATE_load(dcontext, opnd_create_reg(SCRATCH_REG5),
                                      OPND_DC_FIELD(absolute, dcontext, OPSZ_PTR,
                                                    FRAGMENT_FIELD_OFFSET)));
                /* We could load directly into XCX but since hash stats are on,
                 * we assume that this isn't a performance-sensitive run and
                 * opt for code simplicity by rematerializing XDI.
                 */
                /* this is an offset in per_thread_t so should fit in 32 bits */
                IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_int(
                    GET_IB_FTABLE(ibl_code, target_trace_table, table))));
                APP(&ilist,
                    XINST_CREATE_load(
                        dcontext, opnd_create_reg(SCRATCH_REG5),
                        opnd_create_base_disp(
                            SCRATCH_REG5, REG_NULL, 0,
                            (int)GET_IB_FTABLE(ibl_code, target_trace_table, table),
                            OPSZ_PTR)));
            }
#endif
            /* XDI should still point to lookuptable[0] */
            APP(&ilist,
                XINST_CREATE_load(dcontext, opnd_create_reg(SCRATCH_REG2),
                                  opnd_create_reg(SCRATCH_REG5)));
        }

        /*>>>    jmp    compare_tag                                 */
        /* FIXME: should fit in a jmp_short for release builds */
        /* case 5232: use INSTR_CREATE_jmp_smart here */
        APP(&ilist, INSTR_CREATE_jmp(dcontext, opnd_create_instr(compare_tag)));
    }

    /* there is no fall-through through here: we insert separate entry points here */

#ifdef X64
    if (IS_IBL_TRACE(ibl_code->source_fragment_type) &&
        !GENCODE_IS_X86(code->gencode_mode)) {
        if (INTERNAL_OPTION(unsafe_ignore_eflags_trace)) { /*==unsafe_ignore_eflags_ibl */
            /* trace_cmp link and unlink entries are identical to regular */
            add_patch_marker(patch, unlinked, PATCH_ASSEMBLE_ABSOLUTE,
                             0 /* beginning of instruction */,
                             (ptr_uint_t *)&ibl_code->trace_cmp_unlinked);
        } else if (inline_ibl_head) {
            /* For inlining we can't reuse the eflags restore below, so
             * we insert our own
             */
            instr_t *trace_cmp_unlinked = INSTR_CREATE_label(dcontext);
            APP(&ilist, trace_cmp_unlinked);
            add_patch_marker(patch, trace_cmp_unlinked, PATCH_ASSEMBLE_ABSOLUTE,
                             0 /* beginning of instruction */,
                             (ptr_uint_t *)&ibl_code->trace_cmp_unlinked);
            insert_restore_eflags(dcontext, &ilist, NULL, 0, true /*tls*/, false /*!abs*/,
                                  x86_to_x64_ibl_opt);
            APP(&ilist, INSTR_CREATE_jmp(dcontext, opnd_create_instr(unlinked)));
        }
    }
#endif

    /****************************************************************************
     * target_delete_entry
     */

    /*>>>  target_delete_entry:                                     */
    /* This entry point aids in atomic hashtable deletion. A        */
    /* hashtable entry's start_pc_fragment is redirected to here    */
    /* when the entry's fragment is being deleted. It's a prefix to */
    /* the fragment_not_found path and so leads to a cache exit.    */
    /* The regular not_found path skips over these instructions,    */
    /* directly to the fragment_not_found entry.                    */
    /*                                                              */
    /* If coming from a shared ibl, xbx is NOT in the mcontext, which our
     * miss path restore assumes -- so we put it there now.  If coming from
     * a private ibl or a no-prefix-target ibl, this is simply a redundant
     * store.  Xref case 4649.
     */
    /*>>>    SAVE_TO_UPCONTEXT %xbx,xbx_OFFSET                       */
    APP(&ilist, target_delete_entry);

    if (linkstub == NULL) {
        /* If coming from an inlined ibl, the linkstub was not stored, so
         * we use a special linkstub_t in the last_exit "slot" (xdi / tls xdx) for any
         * source (xref case 4635).
         * Rare enough that should be ok, and everyone, including trace building,
         * can handle it.  Although w/ an unknown last_exit the trace builder has to
         * assume the final exit was taken, that's only bad when ending in a cbr,
         * and when that's the case won't end up here (have to have -inline_bb_ibl
         * to get here, since we only add bbs to traces).
         */
#ifdef X64
        /* xbx is now dead so we can use it */
        APP(&ilist,
            INSTR_CREATE_mov_imm(
                dcontext, opnd_create_reg(SCRATCH_REG1),
                OPND_CREATE_INTPTR((ptr_int_t)get_ibl_deleted_linkstub())));
#endif
        if (absolute) {
            APP(&ilist,
                instr_create_save_immed32_to_dcontext(
                    dcontext, (int)(ptr_int_t)get_ibl_deleted_linkstub(),
                    SCRATCH_REG5_OFFS));
        } else if (table_in_tls) {
            APP(&ilist,
                XINST_CREATE_store(
                    dcontext, OPND_TLS_FIELD(TLS_REG3_SLOT),
                    IF_X64_ELSE(
                        opnd_create_reg(SCRATCH_REG1),
                        OPND_CREATE_INTPTR((ptr_int_t)get_ibl_deleted_linkstub()))));
        } else {
            /* doesn't touch xbx */
            insert_shared_get_dcontext(dcontext, &ilist, NULL, true);
            APP(&ilist,
                XINST_CREATE_store(
                    dcontext,
                    OPND_DC_FIELD(absolute, dcontext, OPSZ_PTR, SCRATCH_REG5_OFFS),
                    IF_X64_ELSE(
                        opnd_create_reg(SCRATCH_REG1),
                        OPND_CREATE_INTPTR((ptr_int_t)get_ibl_deleted_linkstub()))));
            insert_shared_restore_dcontext_reg(dcontext, &ilist, NULL);
        }
    } /* else later will fill in fake linkstub anyway.
       * FIXME: for -no_indirect_stubs, is this source of add_ibl curiosities on IIS?
       * but one at least was a post-syscall!
       */

    /* load the tag value from the table ptr in xcx into xbx, so    */
    /* that it gets shuffled into xcx by the following instruction  */
    /*>>>    mov     (%xcx), %xbx                                   */
    if (!ibl_use_target_prefix(ibl_code)) {
        /* case 9688: for no-prefix-target ibl we stored the tag in xax slot
         * in the hit path.  we also restored eflags already.
         */
        if (absolute) {
            APP(&ilist, RESTORE_FROM_DC(dcontext, SCRATCH_REG1, SCRATCH_REG0_OFFS));
        } else {
            APP(&ilist, RESTORE_FROM_TLS(dcontext, SCRATCH_REG1, DIRECT_STUB_SPILL_SLOT));
        }
        if (!INTERNAL_OPTION(unsafe_ignore_eflags_ibl)) {
            /* save flags so we can re-use miss path below */
            insert_save_eflags(dcontext, &ilist, NULL, 0, IBL_EFLAGS_IN_TLS(),
                               absolute _IF_X64(x86_to_x64_ibl_opt));
        }
    } else {
        APP(&ilist,
            XINST_CREATE_load(dcontext, opnd_create_reg(SCRATCH_REG1),
                              OPND_CREATE_MEMPTR(SCRATCH_REG2, FRAGMENT_TAG_OFFS)));
    }

    /* Add to the patch list right away; hashtable stats could be added
     * further later, so if we don't add now the patch ordering becomes
     * confused.
     */
    add_patch_marker(patch, target_delete_entry, PATCH_ASSEMBLE_ABSOLUTE,
                     0 /* beginning of instruction */,
                     (ptr_uint_t *)&ibl_code->target_delete_entry);

    /****************************************************************************
     * fragment_not_found
     */

    /* put target back in xcx to match regular unlinked path,
     * the unlinked inlined indirect branch race condition case
     * also comes here (if !atomic_inlined_linking) */
    /*>>>  fragment_not_found:
     *>>>    mov     %xbx, %xcx                                      */
    APP(&ilist, fragment_not_found);

    /* This counter will also get the unlink inline indirect branch race
     * condition cases (if !atomic_inlined_linking), but that should almost
     * never happen so don't worry about it screwing up the count */
#ifdef HASHTABLE_STATISTICS
    if (INTERNAL_OPTION(hashtable_ibl_stats)) {
        if (save_xdi)
            APP(&ilist, SAVE_TO_TLS(dcontext, SCRATCH_REG5, HTABLE_STATS_SPILL_SLOT));
        append_increment_counter(dcontext, &ilist, ibl_code, patch, REG_NULL,
                                 HASHLOOKUP_STAT_OFFS(miss), SCRATCH_REG1); /* xbx dead */
        if (save_xdi) {
            APP(&ilist,
                RESTORE_FROM_TLS(dcontext, SCRATCH_REG5, HTABLE_STATS_SPILL_SLOT));
        }
    }
#endif
    if (only_spill_state_in_tls) {
        /* get dcontext in register (xdi) */
        insert_shared_get_dcontext(dcontext, &ilist, NULL, false /* xdi is dead */);
    }

    /* for inlining we must restore flags prior to xbx restore: but when not
     * inlining we reverse them so that trace_cmp entry can come in at the restore */
    if (inline_ibl_head) {
        if (!INTERNAL_OPTION(unsafe_ignore_eflags_ibl)) {
            /* restore flags + xax (which we only need so save below works) */
            insert_restore_eflags(dcontext, &ilist, NULL, 0, IBL_EFLAGS_IN_TLS(),
                                  absolute _IF_X64(x86_to_x64_ibl_opt));
        }
        APP(&ilist, unlinked);
    }
    if (DYNAMO_OPTION(indirect_stubs)) {
        /* restore scratch xbx from **xdi / tls xdx ** offset */
        /*>>>    RESTORE_FROM_UPCONTEXT xdi_OFFSET,%xbx                  */
        if (absolute) {
            APP(&ilist, RESTORE_FROM_DC(dcontext, SCRATCH_REG1, SCRATCH_REG5_OFFS));
        } else if (table_in_tls) {
            APP(&ilist, RESTORE_FROM_TLS(dcontext, SCRATCH_REG1, TLS_REG3_SLOT));
        } else {
            ASSERT(only_spill_state_in_tls);
            APP(&ilist, RESTORE_FROM_DC(dcontext, SCRATCH_REG1, SCRATCH_REG5_OFFS));
        }
    } else {
        /* restore xbx */
        if (IF_X64_ELSE(x86_to_x64_ibl_opt, false))
            APP(&ilist, RESTORE_FROM_REG(dcontext, SCRATCH_REG1, REG_R10));
        else if (absolute)
            APP(&ilist, RESTORE_FROM_DC(dcontext, SCRATCH_REG1, SCRATCH_REG1_OFFS));
        else {
            APP(&ilist,
                RESTORE_FROM_TLS(dcontext, SCRATCH_REG1, INDIRECT_STUB_SPILL_SLOT));
        }
    }

    if (only_spill_state_in_tls) {
        insert_shared_restore_dcontext_reg(dcontext, &ilist, NULL);
    }

    if (!inline_ibl_head) {
        /* see above: when not inlining we do eflags restore after xbx restore */
#ifdef X64
        if (IS_IBL_TRACE(ibl_code->source_fragment_type) &&
            !GENCODE_IS_X86(code->gencode_mode) &&
            !INTERNAL_OPTION(unsafe_ignore_eflags_trace)) {
            instr_t *trace_cmp_unlinked = INSTR_CREATE_label(dcontext);
            APP(&ilist, trace_cmp_unlinked);
            add_patch_marker(patch, trace_cmp_unlinked, PATCH_ASSEMBLE_ABSOLUTE,
                             0 /* beginning of instruction */,
                             (ptr_uint_t *)&ibl_code->trace_cmp_unlinked);
        }
#endif
        if (!INTERNAL_OPTION(unsafe_ignore_eflags_ibl)) {
            /* restore flags + xax (which we only need so save below works) */
            insert_restore_eflags(dcontext, &ilist, NULL, 0, IBL_EFLAGS_IN_TLS(),
                                  absolute _IF_X64(x86_to_x64_ibl_opt));
        }
        APP(&ilist, unlinked);
    }

    if (!absolute) {
        insert_shared_get_dcontext(dcontext, &ilist, NULL, true /* save register */);
    }
    /* note we are now saving XAX to the dcontext - no matter where it
     * was saved before for saving and restoring eflags.  FIXME: in
     * some incarnations of this routine it is redundant, yet this is
     * the slow path anyways
     */
    APP(&ilist, SAVE_TO_DC(dcontext, SCRATCH_REG0, SCRATCH_REG0_OFFS));

    /* Indirect exit stub: we have app XBX in slot1, and linkstub in XBX */
    /*   fcache_return however is geared for direct exit stubs which uses XAX */
    /*   app XBX is properly restored  */
    /*   XAX gets the linkstub_ptr */
    /*   app XAX is saved in slot1 */
    /* FIXME: this all can be cleaned up at the cost of an extra byte in
       direct exit stubs to use XBX */

    if (ibl_code->source_fragment_type == IBL_COARSE_SHARED) {
        /* Coarse-grain uses the src tag plus sourceless but type-containing
         * fake linkstubs.  Here we put the src from xbx into its special slot.
         */
        ASSERT(DYNAMO_OPTION(coarse_units));
        APP(&ilist, SAVE_TO_DC(dcontext, SCRATCH_REG1, COARSE_IB_SRC_OFFSET));
        ASSERT(linkstub != NULL);
    }

    if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask)) {
        /* in order to write next_tag we must unprotect -- and we don't
         * have safe stack yet!  so we duplicate fcache_return code here,
         * but we keep xcx w/ next tag around until we can store it as next_tag.
         */
        /* need to save xax (was never saved before) */
        /*>>>    SAVE_TO_UPCONTEXT %xax,xax_OFFSET                  */
        /* put &linkstub where d_r_dispatch expects it */
        /*>>>    mov     %xbx,%xax                                       */
        if (linkstub == NULL) {
            APP(&ilist,
                XINST_CREATE_load(dcontext, opnd_create_reg(SCRATCH_REG0),
                                  opnd_create_reg(SCRATCH_REG1)));
        } else {
            APP(&ilist,
                INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(SCRATCH_REG0),
                                     OPND_CREATE_INTPTR((ptr_int_t)linkstub)));
        }
        append_fcache_return_common(dcontext, code, &ilist, true /*ibl end*/, absolute,
                                    false /*!shared*/, NULL, false /*no coarse info*/);
    } else {
        /* set up for fcache_return: save xax, put xcx in next_tag, &linkstub in xax */
        /*>>>    SAVE_TO_UPCONTEXT %xax,xax_OFFSET                       */
        /*>>>    SAVE_TO_DCONTEXT %xcx,next_tag_OFFSET                   */
        /*>>>    mov     %xbx,%xax                                       */
        /*>>>    RESTORE_FROM_UPCONTEXT xbx_OFFSET,%xbx                  */
        /*>>>    RESTORE_FROM_UPCONTEXT xcx_OFFSET,%xcx                  */
        /*>>>    jmp     _fcache_return                                  */
        APP(&ilist, SAVE_TO_DC(dcontext, SCRATCH_REG2, NEXT_TAG_OFFSET));

        if (linkstub == NULL) {
            APP(&ilist,
                XINST_CREATE_load(dcontext, opnd_create_reg(SCRATCH_REG0),
                                  opnd_create_reg(SCRATCH_REG1)));
        } else {
            /* there is no exit-specific stub -- we use a generic one here */
            APP(&ilist,
                INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(SCRATCH_REG0),
                                     OPND_CREATE_INTPTR((ptr_int_t)linkstub)));
        }

        if (absolute) {
            if (DYNAMO_OPTION(indirect_stubs))
                APP(&ilist, RESTORE_FROM_DC(dcontext, SCRATCH_REG1, SCRATCH_REG1_OFFS));
            /* else, xbx never spilled */
        } else /* table_in_tls || only_spill_state_in_tls */ {
            if (DYNAMO_OPTION(indirect_stubs)) {
                /* restore XBX from EXIT_STUB_SPILL_SLOT */
                APP(&ilist,
                    RESTORE_FROM_TLS(dcontext, SCRATCH_REG1, INDIRECT_STUB_SPILL_SLOT));
            } /* else, xbx never spilled */
            /* now need to juggle with app XAX to be in DIRECT_STUB_SPILL_SLOT,
             * using XCX
             */
            APP(&ilist, RESTORE_FROM_DC(dcontext, SCRATCH_REG2, SCRATCH_REG0_OFFS));
            APP(&ilist, SAVE_TO_TLS(dcontext, SCRATCH_REG2, DIRECT_STUB_SPILL_SLOT));
            /* DIRECT_STUB_SPILL_SLOT has XAX value as needed for fcache_return_shared */
        }

        /* We need to restore XCX from TLS for shared IBL routines, but from
         * mcontext for private IBL routines (unless private_ib_in_tls is set).
         * For x86_to_x64, we restore XCX from R9.
         */
        if (IF_X64_ELSE(x86_to_x64_ibl_opt, false)) {
            APP(&ilist, RESTORE_FROM_REG(dcontext, SCRATCH_REG2, REG_R9));
        } else if (ibl_code->thread_shared_routine || DYNAMO_OPTION(private_ib_in_tls)) {
            APP(&ilist, RESTORE_FROM_TLS(dcontext, SCRATCH_REG2, MANGLE_XCX_SPILL_SLOT));
        } else {
            APP(&ilist, RESTORE_FROM_DC(dcontext, SCRATCH_REG2, SCRATCH_REG2_OFFS));
        }

        if (!absolute) {
            /* restore from scratch the XDI register even if fcache_return will
             * save it again */
            insert_shared_restore_dcontext_reg(dcontext, &ilist, NULL);
        }
        /* pretending we came from a direct exit stub - linkstub in XAX, all
         * other app registers restored */
        APP(&ilist, INSTR_CREATE_jmp(dcontext, opnd_create_pc(fcache_return_pc)));
    }

    if (inline_ibl_head && !DYNAMO_OPTION(atomic_inlined_linking)) {
        /* #ifdef HASHTABLE_STATISTICS
         * >>> race_condition_inc:
         * >>>   #note that eflags are already saved in this path
         * >>>   <inc_stat>
         * >>>   jmp fragment_not_found
         * #endif
         * >>>   #detect unlink path flag to check for unlinking race condition
         * >>>   #must be eflags safe, they are prob. not saved yet
         * >>> unlinked:
         * >>>   movzx %cl, %xcx
         * >>>   # xcx now holds 1 in the unlink case, and the zero extended
         * >>>   # lower byte of a pointer into the hashtable in the race
         * >>>   # condition case (since our pointers into the hashtable are
         * >>>   # aligned this can't be 1), the loop will jmp if xcx != 1
         * #ifdef HASHTABLE_STATISTICS
         * >>>   loop race_condition_inc #race condition handling path
         * #else
         * >>>   loop fragment_not_found  #race condition handling path
         * #endif
         * >>>   #normal unlink path
         * >>>   RESTORE_FROM_UPCONTEXT xbx_offset, %xcx
         * >>>   SAVE_TO_UPCONTEXT %xbx, xbx_offset
         * >>>   jmp old_unlinked
         */
        instr_t *old_unlinked_target = instr_get_next(unlinked);
        instr_t *race_target = fragment_not_found;
#ifdef HASHTABLE_STATISTICS
        if (INTERNAL_OPTION(hashtable_ibl_stats)) {
            race_target = instrlist_last(&ilist);
            if (save_xdi)
                APP(&ilist, SAVE_TO_TLS(dcontext, SCRATCH_REG5, HTABLE_STATS_SPILL_SLOT));
            append_increment_counter(dcontext, &ilist, ibl_code, patch, REG_NULL,
                                     HASHLOOKUP_STAT_OFFS(race_condition), SCRATCH_REG2);
            if (save_xdi) {
                APP(&ilist,
                    RESTORE_FROM_TLS(dcontext, SCRATCH_REG5, HTABLE_STATS_SPILL_SLOT));
            }
            APP(&ilist,
                INSTR_CREATE_jmp_short(dcontext, opnd_create_instr(fragment_not_found)));
        }
        race_target = instr_get_next(race_target);
#endif
        instrlist_remove(&ilist, unlinked);
        APP(&ilist, unlinked);
        APP(&ilist,
            INSTR_CREATE_movzx(dcontext, opnd_create_reg(SCRATCH_REG2),
                               opnd_create_reg(REG_CL)));
        add_patch_marker(patch, unlinked, PATCH_ASSEMBLE_ABSOLUTE,
                         0 /* beginning of instruction */,
                         (ptr_uint_t *)&ibl_code->unlinked_ibl_entry);
        /* subtract 1 from xcx and jmp if !=0 (race condition case) */
        APP(&ilist, INSTR_CREATE_loop(dcontext, opnd_create_instr(race_target)));
#ifdef HASHTABLE_STATISTICS
        if (INTERNAL_OPTION(hashtable_ibl_stats)) {
            if (save_xdi)
                APP(&ilist, SAVE_TO_TLS(dcontext, SCRATCH_REG5, HTABLE_STATS_SPILL_SLOT));
            append_increment_counter(dcontext, &ilist, ibl_code, patch, REG_NULL,
                                     HASHLOOKUP_STAT_OFFS(unlinked_count), SCRATCH_REG2);
            if (save_xdi) {
                APP(&ilist,
                    RESTORE_FROM_TLS(dcontext, SCRATCH_REG5, HTABLE_STATS_SPILL_SLOT));
            } else if (only_spill_state_in_tls) /* restore app %xdi */
                insert_shared_restore_dcontext_reg(dcontext, &ilist, NULL);
        }
#endif
        if (absolute) {
            APP(&ilist, RESTORE_FROM_DC(dcontext, SCRATCH_REG2, SCRATCH_REG1_OFFS));
            APP(&ilist, SAVE_TO_DC(dcontext, SCRATCH_REG1, SCRATCH_REG1_OFFS));
        } else {
            APP(&ilist, RESTORE_FROM_TLS(dcontext, SCRATCH_REG2, SCRATCH_REG1_OFFS));
            APP(&ilist, SAVE_TO_TLS(dcontext, SCRATCH_REG1, SCRATCH_REG1_OFFS));
        }
        APP(&ilist,
            INSTR_CREATE_jmp_short(dcontext, opnd_create_instr(old_unlinked_target)));
    } else {
        /* get a patch marker at the instruction the label is at: */
#ifdef HASHTABLE_STATISTICS
        if (INTERNAL_OPTION(hashtable_ibl_stats)) {
            instr_t *old_unlinked = instr_get_next(unlinked);
            instrlist_remove(&ilist, unlinked);
            APP(&ilist, unlinked);
            add_patch_marker(patch, unlinked, PATCH_ASSEMBLE_ABSOLUTE,
                             0 /* beginning of instruction */,
                             (ptr_uint_t *)&ibl_code->unlinked_ibl_entry);
            /* FIXME: for x64 -thread_private we enter here from trace,
             * and not from top of ibl, so we must save xdi.  Is this true
             * for all cases of only_spill_state_in_tls with !save_xdi?
             * Maybe should be saved in append_increment_counter's call to
             * insert_shared_get_dcontext() instead.
             */
            if (IF_X64_ELSE(true, save_xdi)) {
                APP(&ilist, SAVE_TO_TLS(dcontext, SCRATCH_REG5, HTABLE_STATS_SPILL_SLOT));
            }
            /* have to save eflags before increment, saving eflags clobbers
             * xax slot, but that should be dead here */
            insert_save_eflags(dcontext, &ilist, NULL, 0, !absolute,
                               absolute _IF_X64(x86_to_x64_ibl_opt));
            append_increment_counter(dcontext, &ilist, ibl_code, patch, REG_NULL,
                                     HASHLOOKUP_STAT_OFFS(unlinked_count), SCRATCH_REG1);
            insert_restore_eflags(dcontext, &ilist, NULL, 0, !absolute,
                                  absolute _IF_X64(x86_to_x64_ibl_opt));
            if (IF_X64_ELSE(true, save_xdi)) {
                APP(&ilist,
                    RESTORE_FROM_TLS(dcontext, SCRATCH_REG5, HTABLE_STATS_SPILL_SLOT));
            } else if (only_spill_state_in_tls) {
                /* we didn't care that XDI got clobbered since it was spilled
                 * at the entry point into the IBL routine but we do need to
                 * restore app state now.
                 */
                insert_shared_restore_dcontext_reg(dcontext, &ilist, NULL);
            }
            APP(&ilist, INSTR_CREATE_jmp(dcontext, opnd_create_instr(old_unlinked)));
        } else
#endif
        {
            add_patch_marker(patch, unlinked, PATCH_ASSEMBLE_ABSOLUTE,
                             0 /* beginning of instruction */,
                             (ptr_uint_t *)&ibl_code->unlinked_ibl_entry);
        }
    }

#ifdef X64
    if (GENCODE_IS_X86(code->gencode_mode)) {
        /* We currently have x86 code parsing the regular x64 table (PR 283895 covers
         * using an x86 table, for both full correctness and performance: for now
         * we have no way to detect a source in one mode jumping to a target built in
         * another mode w/o a mode switch, but that would be an app error anyway).
         * Rather than complicating the REG_X* defines used above we have a post-pass
         * that shrinks all the registers and all the INTPTR immeds.
         * The other two changes we need are performed up above:
         *   1) cmp top bits to 0 for match
         *   2) no trace_cmp entry points
         * Note that we're punting on PR 283152: we go ahead and clobber the top bits
         * of all our scratch registers.
         */
        instrlist_convert_to_x86(&ilist);
    }
#endif

    ibl_code->ibl_routine_length = encode_with_patch_list(dcontext, patch, &ilist, pc);

    /* free the instrlist_t elements */
    instrlist_clear(dcontext, &ilist);

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
#ifdef X64
        code = SHARED_GENCODE_MATCH_THREAD(dcontext);
#else
        ASSERT(special_ibl_xfer_is_thread_private()); /* else shouldn't be called */
        code = THREAD_GENCODE(dcontext);
#endif
    }
    if (code == NULL) /* shared_code_x86, or thread private that we don't need */
        return;
    ibl_tgt = special_ibl_xfer_tgt(dcontext, code, entry_type, ibl_type);
    ASSERT(code->special_ibl_xfer[index] != NULL);
    pc = (code->special_ibl_xfer[index] + code->special_ibl_unlink_offs[index] +
          1 /*jmp opcode*/);

    protect_generated_code(code, WRITABLE);
    insert_relative_target(pc, ibl_tgt, code->thread_shared /*hot patch*/);
    protect_generated_code(code, READONLY);
}

bool
fill_with_nops(dr_isa_mode_t isa_mode, byte *addr, size_t size)
{
    /* Xref AMD Software Optimization Guide for AMD Family 15h Processors, document
     * #47414, section 5.8 "Code Padding with Operand-Size Override and Multibyte
     * NOP".
     * For compatibility with Intel case 10 and 11 are left out.
     * Xref Intel, see Vol. 2B 4-167 "Table 4-12. Recommended Multi-Byte Sequence of NOP
     * Instruction".
     */
    switch (size) {
    case 1: memcpy(addr, "\x90", 1); break;
    case 2: memcpy(addr, "\x66\x90", 2); break;
    case 3: memcpy(addr, "\x0f\x1f\x00", 3); break;
    case 4: memcpy(addr, "\x0f\x1f\x40\x00", 4); break;
    case 5: memcpy(addr, "\x0f\x1f\x44\x00\x00", 5); break;
    case 6: memcpy(addr, "\x66\x0f\x1f\x44\x00\x00", 6); break;
    case 7: memcpy(addr, "\x0f\x1f\x80\x00\x00\x00\x00", 7); break;
    case 8: memcpy(addr, "\x0f\x1f\x84\x00\x00\x00\x00\x00", 8); break;
    case 9: memcpy(addr, "\x66\x0f\x1f\x84\x00\x00\x00\x00\x00", 9); break;
    default: memset(addr, 0x90, size);
    }
    return true;
}

/* If code_buf points to a jmp rel32 returns true and returns the target of
 * the jmp in jmp_target as if was located at app_loc. */
bool
is_jmp_rel32(byte *code_buf, app_pc app_loc, app_pc *jmp_target /* OUT */)
{
    if (*code_buf == JMP_OPCODE) {
        if (jmp_target != NULL) {
            *jmp_target = app_loc + JMP_LONG_LENGTH + *(int *)(code_buf + 1);
        }
        return true;
    }
    return false;
}

/* If code_buf points to a jmp rel8 returns true and returns the target of
 * the jmp in jmp_target as if was located at app_loc. */
bool
is_jmp_rel8(byte *code_buf, app_pc app_loc, app_pc *jmp_target /* OUT */)
{
    if (*code_buf == JMP_SHORT_OPCODE) {
        if (jmp_target != NULL) {
            *jmp_target = app_loc + JMP_SHORT_LENGTH + *(char *)(code_buf + 1);
        }
        return true;
    }
    return false;
}
