/* **********************************************************
 * Copyright (c) 2012-2022 Google, Inc.  All rights reserved.
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

/*
 * emit.c - fragment code generation routines
 */

#include "globals.h"
#include "link.h"
#include "fragment.h"
#include "fcache.h"
#include "proc.h"
#include "instrlist.h"
#include "emit.h"
#include "instrlist.h"
#include "instr.h"
#include "instr_create_shared.h"
#include "monitor.h"
#include "translate.h"

#ifdef DEBUG
#    include "decode_fast.h" /* for decode_next_pc for stress_recreate_pc */
#endif

#define STATS_FCACHE_ADD(flags, stat, val)                  \
    DOSTATS({                                               \
        if (TEST(FRAG_SHARED, (flags))) {                   \
            if (IN_TRACE_CACHE(flags))                      \
                STATS_ADD(fcache_shared_trace_##stat, val); \
            else                                            \
                STATS_ADD(fcache_shared_bb_##stat, val);    \
        } else if (IN_TRACE_CACHE(flags))                   \
            STATS_ADD(fcache_trace_##stat, val);            \
        else                                                \
            STATS_ADD(fcache_bb_##stat, val);               \
    })

#ifdef INTERNAL
/* case 4344 - verify we can recreate app pc in fragment, returns the pc of
 * the last instruction in the body of f */
static cache_pc
get_last_fragment_body_instr_pc(dcontext_t *dcontext, fragment_t *f)
{
    cache_pc body_last_inst_pc;
    linkstub_t *l;

    /* Assumption : the last exit stub exit cti is the last instruction in the
     * body.  PR 215217 enforces this for clients as well. */
    l = FRAGMENT_EXIT_STUBS(f);
    /* never called on future fragments, so a stub should exist */
    while (!LINKSTUB_FINAL(l))
        l = LINKSTUB_NEXT_EXIT(l);

    body_last_inst_pc = EXIT_CTI_PC(f, l);
    return body_last_inst_pc;
}

void
stress_test_recreate(dcontext_t *dcontext, fragment_t *f, instrlist_t *ilist)
{
    cache_pc body_end_pc = get_last_fragment_body_instr_pc(dcontext, f);
    app_pc recreated_pc;

    LOG(THREAD, LOG_MONITOR, 2,
        "Testing recreating Fragment %d for tag " PFX " at " PFX "\n", f->id, f->tag,
        f->start_pc);

    DOCHECK(2, {
        /* Visualize translation info if it were to be recorded for every
         * fragment, not just deleted ones -- for debugging only.  But we run
         * the info-creation code at checklevel 2 as a sanity check.
         */
        translation_info_t *info = record_translation_info(dcontext, f, NULL);
        DOLOG(3, LOG_INTERP, { translation_info_print(info, f->start_pc, THREAD); });
        translation_info_free(dcontext, info);
        /* handy reference of app code and fragment -- only 1st part of trace though */
        LOG(THREAD, LOG_INTERP, 3,
            "Re-printing app bb and cache disasm for convenience:\n");
        DOLOG(3, LOG_INTERP, { disassemble_app_bb(dcontext, f->tag, THREAD); });
        DOLOG(3, LOG_INTERP, { disassemble_fragment(dcontext, f, false); });
    });

    DOCHECK(2, {
        /* Translate them all.
         * Useful when verifying manually, o/w we just ensure no asserts or crashes.
         */
        cache_pc cpc = f->start_pc;
        while (cpc <= body_end_pc) {
            recreated_pc = recreate_app_pc(dcontext, cpc, NULL /*for full test*/);
            LOG(THREAD, LOG_MONITOR, 2, "\ttranslated cache " PFX " => app " PFX "\n",
                cpc, recreated_pc);
            cpc = decode_next_pc(dcontext, cpc);
        }
    });

    recreated_pc = recreate_app_pc(dcontext, body_end_pc, NULL /*for full test*/);
    /* FIXME: figure out how to test each instruction, while knowing the app state */
    LOG(THREAD, LOG_MONITOR, 2, "Testing recreating Fragment #%d recreated_pc=" PFX "\n",
        GLOBAL_STAT(num_fragments), recreated_pc);

    ASSERT(recreated_pc != NULL);

    if (INTERNAL_OPTION(stress_recreate_state) && ilist != NULL)
        stress_test_recreate_state(dcontext, f, ilist);
}
#endif /* INTERNAL */

/* here instead of link.c b/c link.c doesn't deal w/ Instrs */
bool
final_exit_shares_prev_stub(dcontext_t *dcontext, instrlist_t *ilist, uint frag_flags)
{
    /* if a cbr is final exit pair, should they share a stub? */
    if (INTERNAL_OPTION(cbr_single_stub) && !TEST(FRAG_COARSE_GRAIN, frag_flags)) {
        /* don't need to expand since is_exit_cti will rule out level 0 */
        instr_t *inst = instrlist_last(ilist);
        /* FIXME: we could support code after the last cti (this is ubr so
         * would be out-of-line code) or between cbr and ubr but for
         * simplicity of identifying exits for traces we don't
         */
        if (instr_is_exit_cti(inst) && instr_is_ubr(inst)) {
            /* don't need to expand since is_exit_cti will rule out level 0 */
            instr_t *prev_cti = instr_get_prev(inst);
            if (prev_cti != NULL &&
                instr_is_exit_cti(prev_cti)
                /* cti_loop is fine since cti points to loop instr, enabling
                 * our disambiguation to know which state to look at */
                && instr_is_cbr(prev_cti)
                /* no separate freeing */
                && ((TEST(FRAG_SHARED, frag_flags) &&
                     !DYNAMO_OPTION(unsafe_free_shared_stubs)) ||
                    (!TEST(FRAG_SHARED, frag_flags) &&
                     !DYNAMO_OPTION(free_private_stubs)))) {
                return true;
            }
        }
    }
    return false;
}

/* Walks ilist and f's linkstubs, setting each linkstub_t's fields appropriately
 * for the corresponding exit cti in ilist.
 * If emit is true, also encodes each instr in ilist to f's cache slot,
 * increments stats for new fragments, and returns the final pc after all encodings.
 */
cache_pc
set_linkstub_fields(dcontext_t *dcontext, fragment_t *f, instrlist_t *ilist,
                    uint num_direct_stubs, uint num_indirect_stubs, bool emit)
{
    uint i;
    bool frag_offs_at_end;
    linkstub_t *l;
    cache_pc pc;
    instr_t *inst;
    app_pc target;
    DEBUG_DECLARE(instr_t *prev_cti = NULL;)

    pc = FCACHE_ENTRY_PC(f);
    l = FRAGMENT_EXIT_STUBS(f);
    i = 0;
    frag_offs_at_end =
        linkstub_frag_offs_at_end(f->flags, num_direct_stubs, num_indirect_stubs);
    for (inst = instrlist_first(ilist); inst; inst = instr_get_next(inst)) {
        if (instr_is_exit_cti(inst)) {
            /* l is currently zeroed out but otherwise uninitialized
             * stub starts out as unlinked and never-been-linked
             */
            ASSERT(l->flags == 0);
            i++;
            if (i == num_direct_stubs + num_indirect_stubs) {
                /* set final flag */
                l->flags |= LINK_END_OF_LIST;
            }
            if (frag_offs_at_end)
                l->flags |= LINK_FRAG_OFFS_AT_END;

            DODEBUG({
                if (emit && is_exit_cti_patchable(dcontext, inst, f->flags)) {
                    uint off = patchable_exit_cti_align_offs(dcontext, inst, pc);
                    if (off > 0) {
                        ASSERT(!PAD_FRAGMENT_JMPS(f->flags));
                        STATS_PAD_JMPS_ADD(f->flags, unaligned_exits, 1);
                        STATS_PAD_JMPS_ADD(f->flags, unaligned_exit_bytes, off);
                    }
                }
            });
            /* An alternative way of testing this is to match with
             * is_return_lookup_routine() whenever we get that */
            /* FIXME: doing the above is much easier now and it is more reliable
             * than expecting the branch type flags to propagate through
             */
            l->flags |= instr_exit_branch_type(inst);

            target = instr_get_branch_target_pc(inst);

            if (is_indirect_branch_lookup_routine(dcontext, (cache_pc)target)) {
                ASSERT(IF_WINDOWS_ELSE_0(is_shared_syscall_routine(dcontext, target)) ||
                       is_ibl_routine_type(
                           dcontext, (cache_pc)target,
                           extract_branchtype((ushort)instr_exit_branch_type(inst))));
                /* this is a mangled form of an original indirect
                 * branch or is a mangled form of an indirect branch
                 * to a real native pc out of the fragment
                 */
                l->flags |= LINK_INDIRECT;
                ASSERT(!LINKSTUB_DIRECT(l->flags));
                ASSERT(!LINKSTUB_NORMAL_DIRECT(l->flags));
                ASSERT(!LINKSTUB_CBR_FALLTHROUGH(l->flags));
                ASSERT(LINKSTUB_INDIRECT(l->flags));
            } else {
                DOSTATS({
                    if (emit) {
                        if (PTR_UINT_ABS(target - f->tag) > SHRT_MAX) {
                            if (num_indirect_stubs == 0 && num_direct_stubs == 2 &&
                                i == 2)
                                STATS_INC(num_bb_fallthru_far);
                            STATS_INC(num_bb_exit_tgt_far);
                        } else {
                            if (num_indirect_stubs == 0 && num_direct_stubs == 2 &&
                                i == 2)
                                STATS_INC(num_bb_fallthru_near);
                            STATS_INC(num_bb_exit_tgt_near);
                        }
                    }
                });

                if (LINKSTUB_FINAL(l) &&
                    use_cbr_fallthrough_short(f->flags, num_direct_stubs,
                                              num_indirect_stubs)) {
                    /* this is how we mark a cbr fallthrough, w/ both
                     * LINK_DIRECT and LINK_INDIRECT
                     */
                    l->flags |= LINK_DIRECT | LINK_INDIRECT;
                    /* ensure our macros are in synch */
                    ASSERT(LINKSTUB_DIRECT(l->flags));
                    ASSERT(!LINKSTUB_NORMAL_DIRECT(l->flags));
                    ASSERT(LINKSTUB_CBR_FALLTHROUGH(l->flags));
                    ASSERT(!LINKSTUB_INDIRECT(l->flags));
                    DOSTATS({
                        if (emit)
                            STATS_INC(num_bb_cbr_fallthru_shrink);
                    });
                    ASSERT(prev_cti != NULL && instr_is_cbr(prev_cti));
                    /* should always qualify for single stub */
                    ASSERT(!INTERNAL_OPTION(cbr_single_stub) ||
                           /* FIXME: this duplicates calc of final_cbr_single_stub
                            * bool cached in emit_fragment_common()
                            */
                           (inst == instrlist_last(ilist) &&
                            final_exit_shares_prev_stub(dcontext, ilist, f->flags)));
                } else {
                    direct_linkstub_t *dl = (direct_linkstub_t *)l;
                    l->flags |= LINK_DIRECT;
                    /* ensure our macros are in synch */
                    ASSERT(LINKSTUB_DIRECT(l->flags));
                    ASSERT(LINKSTUB_NORMAL_DIRECT(l->flags));
                    ASSERT(!LINKSTUB_CBR_FALLTHROUGH(l->flags));
                    ASSERT(!LINKSTUB_INDIRECT(l->flags));
                    dl->target_tag = target;
                }
            }

            if (should_separate_stub(dcontext, target, f->flags))
                l->flags |= LINK_SEPARATE_STUB;

            /* FIXME: we don't yet support !emit ctis: need to avoid patching
             * the cti when emit the exit stub */
            ASSERT_NOT_IMPLEMENTED(!emit || instr_ok_to_emit(inst));

            if (LINKSTUB_CBR_FALLTHROUGH(l->flags)) {
                /* target is indicated via cti_offset */
                ASSERT_TRUNCATE(l->cti_offset, short, target - f->tag);
                l->cti_offset = (ushort) /* really a short */ (target - f->tag);
            } else {
                ASSERT_TRUNCATE(l->cti_offset, ushort, pc - f->start_pc);
                l->cti_offset = (ushort)(pc - f->start_pc);
            }

            DOCHECK(1, {
                /* ensure LINK_ flags were transferred via instr_exit_branch_type */
                if (instr_branch_special_exit(inst)) {
                    ASSERT(!LINKSTUB_INDIRECT(l->flags) &&
                           TEST(LINK_SPECIAL_EXIT, l->flags));
                }
                if (instr_branch_is_padded(inst)) {
                    ASSERT(TEST(LINK_PADDED, l->flags));
                }
            });

            if (!EXIT_HAS_STUB(l->flags, f->flags)) {
                /* exit cti points straight at ibl routine */
                instr_set_branch_target_pc(inst, get_unlinked_entry(dcontext, target));
            } else {
                /* HACK: set the branch target pc in inst to be its own pc- this ensures
                   that instr_encode will not fail due to address span problems- the
                   correct target (to the exit stub) will get patched in when the
                   exit stub corresponding to this exit branch is emitted later */
                instr_set_branch_target_pc(inst, pc);
            }
            /* PR 267260/PR 214962: keep this exit cti marked */
            instr_set_our_mangling(inst, true);

            LOG(THREAD, LOG_EMIT,
                dcontext == GLOBAL_DCONTEXT || dcontext->in_opnd_disassemble ? 5U : 3U,
                "exit_branch_type=0x%x target=" PFX " l->flags=0x%x\n",
                instr_exit_branch_type(inst), target, l->flags);

            DOCHECK(1, {
                if (TEST(FRAG_COARSE_GRAIN, f->flags)) {
                    ASSERT(!frag_offs_at_end);
                    /* FIXME: indirect stubs should be separated
                     * eventually, but right now no good place to put them
                     * so keeping inline
                     */
                    ASSERT(LINKSTUB_INDIRECT(l->flags) ||
                           TEST(LINK_SEPARATE_STUB, l->flags));
                }
            });

            /* traversal depends on flags being set */
            ASSERT(l->flags != 0);
            ASSERT(i <= num_direct_stubs + num_indirect_stubs);
            l = LINKSTUB_NEXT_EXIT(l);
            DODEBUG({ prev_cti = inst; });
        } /* exit cti */
        if (instr_ok_to_emit(inst)) {
            if (emit) {
                pc = instr_encode_to_copy(dcontext, inst, vmcode_get_writable_addr(pc),
                                          pc);
                ASSERT(pc != NULL);
                pc = vmcode_get_executable_addr(pc);
            } else {
                pc += instr_length(dcontext, inst);
            }
        }
    }
    return pc;
}

/* Emits code for ilist into the fcache, returns created fragment.
 * It is not added to the fragment table, nor is it linked!
 */
static fragment_t *
emit_fragment_common(dcontext_t *dcontext, app_pc tag, instrlist_t *ilist, uint flags,
                     void *vmlist, bool link_fragment, bool add_to_htable,
                     fragment_t *replace_fragment)
{
    fragment_t *f;
    instr_t *inst;
    cache_pc pc = 0;
    app_pc target;
    linkstub_t *l;
    uint len;
    uint offset = 0;
    uint copy_sz = 0;
    uint extra_jmp_padding_body = 0;
    uint extra_jmp_padding_stubs = 0;
    uint last_pad_offset = 0;
    uint num_direct_stubs = 0;
    uint num_indirect_stubs = 0;
    uint stub_size_total = 0; /* those in fcache w/ fragment */
    bool final_cbr_single_stub = false;
    byte *prev_stub_pc = NULL;
    uint stub_size = 0;
    bool no_stub = false;
    dr_isa_mode_t isa_mode;
    uint mode_flags;

    KSTART(emit);
    /* we do entire cache b/c links may touch many units
     * FIXME: change to lazier version triggered by segfaults or something?
     */
    SELF_PROTECT_CACHE(dcontext, NULL, WRITABLE);

    /* ensure some higher-level lock is held if f is shared */
    ASSERT(!TEST(FRAG_SHARED, flags) || INTERNAL_OPTION(single_thread_in_DR) ||
           !USE_BB_BUILDING_LOCK() || OWN_MUTEX(&bb_building_lock) ||
           OWN_MUTEX(&trace_building_lock));

    /* 1st walk through instr list:
     * -- determine body size and number of exit stubs required;
     * -- if not padding jmps sets offsets as well
     */
    ASSERT(instrlist_first(ilist) != NULL);
    isa_mode = instr_get_isa_mode(instrlist_first(ilist));
#ifdef ARM
    /* XXX i#1734: reset encdoe state to avoid any stale encode state
     * or dangling pointer.
     */
    if (isa_mode == DR_ISA_ARM_THUMB)
        encode_reset_it_block(dcontext);
#endif
    mode_flags = frag_flags_from_isa_mode(isa_mode);
    if (mode_flags != 0)
        flags |= mode_flags;
#if defined(X86) && defined(X64)
    else if (dr_get_isa_mode(dcontext) == DR_ISA_IA32)
        flags |= FRAG_X86_TO_X64;
#endif
    for (inst = instrlist_first(ilist); inst; inst = instr_get_next(inst)) {
        /* Since decode_fragment needs to be able to decode from the code
         * cache, we require that each fragment has a single mode
         * (xref PR 278329)
         */
        IF_X64(CLIENT_ASSERT(instr_get_isa_mode(inst) == isa_mode,
                             "single fragment cannot mix x86 and x64 modes"));
        if (!PAD_FRAGMENT_JMPS(flags)) {
            /* We're going to skip the 2nd pass; save the offset for instr_encode. */
            inst->offset = offset;
        }
        if (instr_ok_to_emit(inst))
            offset += instr_length(dcontext, inst);
        ASSERT_NOT_IMPLEMENTED(!TEST(INSTR_HOT_PATCHABLE, inst->flags));
        if (instr_is_exit_cti(inst)) {
            target = instr_get_branch_target_pc(inst);
            len = exit_stub_size(dcontext, (cache_pc)target, flags);
            if (PAD_FRAGMENT_JMPS(flags) && instr_ok_to_emit(inst)) {
                /* Most exits have only a single patchable jmp (is difficult
                 * to handle all the races for more then one). Exceptions are
                 * usually where you have to patch the jmp in the body as well
                 * as in the stub and include inlined_indirect (without
                 * -atomic_inlined_linking) or TRACE_HEAD_CACHE_INCR.  All of these
                 * have issues with atomically linking/unlinking. Inlined
                 * indirect has special support for unlinking (but not linking
                 * hence can't use inlined_ibl on shared frags without
                 * -atomic_inlined_linking, but is otherwise ok).  I suspect
                 * the other two exceptions are ok as well in practice (just
                 * racy as to whether the trace head count gets incremented or
                 * the custom code is executed or we exit cache unnecessarily).
                 */
                if (is_exit_cti_patchable(dcontext, inst, flags)) {
                    if (last_pad_offset == 0 ||
                        !WITHIN_PAD_REGION(last_pad_offset, offset)) {
                        last_pad_offset = offset - CTI_PATCH_OFFSET;
                        extra_jmp_padding_body += MAX_PAD_SIZE;
                    }
                }
                if (is_exit_cti_stub_patchable(dcontext, inst, flags)) {
                    extra_jmp_padding_stubs += MAX_PAD_SIZE;
                }
            }
            if (is_indirect_branch_lookup_routine(dcontext, target)) {
                num_indirect_stubs++;
                STATS_INC(num_indirect_exit_stubs);
                LOG(THREAD, LOG_EMIT, 3, "emit_fragment: %s use ibl <" PFX ">\n",
                    TEST(FRAG_IS_TRACE, flags) ? "trace" : "bb", target);
                stub_size_total += len;
                STATS_FCACHE_ADD(flags, indirect_stubs, len);
            } else {
                num_direct_stubs++;
                STATS_INC(num_direct_exit_stubs);

                /* if a cbr is final exit pair, should they share a stub? */
                if (INTERNAL_OPTION(cbr_single_stub) && inst == instrlist_last(ilist) &&
                    final_exit_shares_prev_stub(dcontext, ilist, flags)) {
                    final_cbr_single_stub = true;
                    STATS_INC(num_cbr_single_stub);
                } else if (!should_separate_stub(dcontext, target, flags)) {
                    stub_size_total += len;
                    STATS_FCACHE_ADD(flags, direct_stubs, len);
                } else /* ensure have cti to jmp to separate stub! */
                    ASSERT(instr_ok_to_emit(inst));
            }
        }
    }

#ifdef ARM
    /* i#1906: we must 4-align the start of direct stubs */
    if (num_direct_stubs > 0) {
        if (!ALIGNED(offset, PC_LOAD_ADDR_ALIGN)) {
            extra_jmp_padding_body += 2;
            instrlist_append(ilist, INSTR_CREATE_nop(dcontext));
            ASSERT(instr_length(dcontext, instrlist_last(ilist)) == 2);
            instrlist_last(ilist)->offset = offset;
        }
        ASSERT(ALIGNED(offset + extra_jmp_padding_body, PC_LOAD_ADDR_ALIGN));
    }
#endif

    DOSTATS({
        if (!TEST(FRAG_IS_TRACE, flags)) {
            if (num_indirect_stubs > 0) {
                if (num_indirect_stubs == 1 && num_direct_stubs == 0)
                    STATS_INC(num_bb_one_indirect_exit);
                else /* funny bb w/ mixture of ind and dir exits */
                    STATS_INC(num_bb_indirect_extra_exits);
            } else {
                if (num_direct_stubs == 1)
                    STATS_INC(num_bb_one_direct_exit);
                else if (num_direct_stubs == 2)
                    STATS_INC(num_bb_two_direct_exits);
                else
                    STATS_INC(num_bb_many_direct_exits);
            }
            if (TEST(FRAG_HAS_DIRECT_CTI, flags))
                STATS_INC(num_bb_has_elided);
            if (linkstub_frag_offs_at_end(flags, num_direct_stubs, num_indirect_stubs))
                STATS_INC(num_bb_fragment_offset);
        }
    });

    STATS_PAD_JMPS_ADD(flags, body_bytes, extra_jmp_padding_body);
    STATS_PAD_JMPS_ADD(flags, stub_bytes, extra_jmp_padding_stubs);

    STATS_FCACHE_ADD(flags, bodies, offset);
    STATS_FCACHE_ADD(flags, prefixes, fragment_prefix_size(flags));

    if (TEST(FRAG_SELFMOD_SANDBOXED, flags)) {
        /* We need a copy of the original app code at bottom of
         * fragment.  We count it as part of the fragment body size,
         * and use a size field stored at the very end (whose storage
         * is also included in the fragment body size) to distinguish
         * the real body from the selfmod copy (storing it there
         * rather than in fragment_t to save space in the common case).
         */
        /* assume contiguous bb */
        app_pc end_bb_pc;
        ASSERT((flags & FRAG_HAS_DIRECT_CTI) == 0);
        /* FIXME PR 215217: a client may have truncated or otherwise changed
         * the code, but we assume no new code has been added.  Thus, checking
         * the original full range can only result in a false positive selfmod
         * event, which is a performance issue only.
         */
        end_bb_pc = find_app_bb_end(dcontext, tag, flags);
        ASSERT(end_bb_pc > tag);
        ASSERT_TRUNCATE(copy_sz, uint, (ptr_uint_t)(end_bb_pc - tag) + sizeof(uint));
        copy_sz = (uint)(end_bb_pc - tag) + sizeof(uint);
        /* ensure this doesn't push fragment size over limit */
        ASSERT(offset + copy_sz <= MAX_FRAGMENT_SIZE);
        offset += copy_sz;
        STATS_FCACHE_ADD(flags, selfmod_copy, copy_sz);
    }

    /* create a new fragment_t, or fill in the emit wrapper for coarse-grain */
    /* FIXME : don't worry too much about whether padding should be requested in
     * the stub or body argument, fragment_create doesn't distinguish between
     * the two */
    f = fragment_create(dcontext, tag, offset + extra_jmp_padding_body, num_direct_stubs,
                        num_indirect_stubs, stub_size_total + extra_jmp_padding_stubs,
                        flags);
    ASSERT(f != NULL);
    DOSTATS({
        /* PR 217008: avoid gcc warning from truncation assert in XSTATS_TRACK_MAX:
         * "comparison is always true due to limited range of data type"
         * to turn off have to turn off Wextra; hopefully future gcc
         * will have patch out there for Walways-true. */
        int tmp_size = f->size;
        STATS_TRACK_MAX(max_fragment_requested_size, tmp_size);
    });

    if (PAD_FRAGMENT_JMPS(flags)) {
        uint start_shift;
        /* 2nd (pad_jmps) walk through instr list:
         * -- record offset of each instr from start of fragment body.
         * -- insert any nops needed for patching alignment
         * recreate needs to do this too, so we use a shared routine */
        start_shift = nop_pad_ilist(dcontext, f, ilist, true /* emitting, set offset */);
        fcache_shift_start_pc(dcontext, f, start_shift);
    }

    /* emit prefix */
    insert_fragment_prefix(dcontext, f);

    /* 3rd walk through instr list: (2nd if -no_pad_jmps)
     * -- initialize and set fields in link stub for each exit cti;
     * -- emit each instr into the fragment.
     */
    pc = set_linkstub_fields(dcontext, f, ilist, num_direct_stubs, num_indirect_stubs,
                             true /*encode each instr*/);
    /* pc should now be pointing to the beginning of the first exit stub */

    /* emit the exit stub code */
    for (l = FRAGMENT_EXIT_STUBS(f); l; l = LINKSTUB_NEXT_EXIT(l)) {
        if (TEST(FRAG_COARSE_GRAIN, flags) && LINKSTUB_DIRECT(l->flags)) {
            /* Coarse-grain fragments do not have direct exit stubs.
             * Instead they have entrance stubs, created when linking.
             */
            continue;
        }

        if (!EXIT_HAS_STUB(l->flags, flags)) {
            /* there is no stub */
            continue;
        }

        if (final_cbr_single_stub && LINKSTUB_FINAL(l)) {
            no_stub = true;
            if (!TEST(LINK_SEPARATE_STUB, l->flags)) {
                /* still need to patch the cti, so set pc back to prev stub pc */
                pc = prev_stub_pc;
            }
            LOG(THREAD, LOG_EMIT, 3, "final exit sharing prev exit's stub @ " PFX "\n",
                prev_stub_pc);
        }

        if (TEST(LINK_SEPARATE_STUB, l->flags)) {
            if (no_stub) {
                if (LINKSTUB_NORMAL_DIRECT(l->flags)) {
                    direct_linkstub_t *dl = (direct_linkstub_t *)l;
                    dl->stub_pc = prev_stub_pc;
                } else {
                    ASSERT(LINKSTUB_CBR_FALLTHROUGH(l->flags));
                    /* stub pc computation should return prev pc */
                    ASSERT(EXIT_STUB_PC(dcontext, f, l) == prev_stub_pc);
                }
            } else {
                separate_stub_create(dcontext, f, l);
            }
            prev_stub_pc = EXIT_STUB_PC(dcontext, f, l);
            ASSERT(prev_stub_pc != NULL);
            /* pointing at start of stub is the unlink entry */
            ASSERT(linkstub_unlink_entry_offset(dcontext, f, l) == 0);
            patch_branch(FRAG_ISA_MODE(f->flags), EXIT_CTI_PC(f, l),
                         EXIT_STUB_PC(dcontext, f, l), false);
            continue;
        }

        ASSERT(EXIT_HAS_LOCAL_STUB(l->flags, flags));

        if (PAD_FRAGMENT_JMPS(flags)) {
            pc = pad_for_exitstub_alignment(dcontext, l, f, pc);
        }

        if (LINKSTUB_NORMAL_DIRECT(l->flags)) {
            direct_linkstub_t *dl = (direct_linkstub_t *)l;
            dl->stub_pc = pc;
        }
        /* relocate the exit branch target so it takes to the unlink
         * entry to the stub
         */
        patch_branch(FRAG_ISA_MODE(f->flags), EXIT_CTI_PC(f, l),
                     pc + linkstub_unlink_entry_offset(dcontext, f, l), false);
        LOG(THREAD, LOG_EMIT, 3,
            "Exit cti " PFX " is targeting " PFX " + 0x%x => " PFX "\n",
            EXIT_CTI_PC(f, l), pc, linkstub_unlink_entry_offset(dcontext, f, l),
            pc + linkstub_unlink_entry_offset(dcontext, f, l));

        DODEBUG({
            uint shift = bytes_for_exitstub_alignment(dcontext, l, f, pc);
            if (shift > 0) {
                ASSERT(!PAD_FRAGMENT_JMPS(flags));
                STATS_PAD_JMPS_ADD(flags, unaligned_stubs, 1);
                STATS_PAD_JMPS_ADD(flags, unaligned_stubs_bytes, shift);
            }
        });

        /* insert an exit stub */
        prev_stub_pc = pc;
        if (!no_stub)
            stub_size = insert_exit_stub(dcontext, f, l, pc);
        /* note that we don't do proactive linking here since it may
         * depend on whether this is a trace fragment, which is marked
         * by the caller, who is responsible for calling link_new_fragment
         */

        /* if no_stub we assume stub_size is still what it was for prev stub,
         * and yes we do need to adjust it back to the end of the single stub
         */
        pc += stub_size;
    }

    ASSERT(pc - f->start_pc <= f->size);

    /* Give back extra space to fcache */
    STATS_PAD_JMPS_ADD(flags, excess_bytes, f->size - (pc - f->start_pc) - copy_sz);
    if (PAD_FRAGMENT_JMPS(flags) && INTERNAL_OPTION(pad_jmps_return_excess_padding) &&
        f->size - (pc - f->start_pc) - copy_sz > 0) {
        /* will adjust size, must call before we copy the selfmod since we
         * break abstraction by putting the copy space in the fcache
         * extra field and fcache needs to read/modify the fields */
        fcache_return_extra_space(dcontext, f, f->size - (pc - f->start_pc) - copy_sz);
    }

    if (TEST(FRAG_SELFMOD_SANDBOXED, flags)) {
        /* put copy of the original app code at bottom of fragment */
        cache_pc copy_pc;

        ASSERT(f->size > copy_sz);
        copy_pc = f->start_pc + f->size - copy_sz;
        ASSERT(copy_pc == pc ||
               (PAD_FRAGMENT_JMPS(flags) &&
                !INTERNAL_OPTION(pad_jmps_return_excess_padding)));
        /* size is stored at the end, but included in copy_sz */
        memcpy(vmcode_get_writable_addr(copy_pc), tag, copy_sz - sizeof(uint));
        *((uint *)vmcode_get_writable_addr(copy_pc + copy_sz - sizeof(uint))) = copy_sz;
        /* count copy as part of fragment */
        pc = copy_pc + copy_sz;
    }

    ASSERT(pc - f->start_pc <= f->size);
    STATS_TRACK_MAX(max_fragment_size, pc - f->start_pc);
    STATS_PAD_JMPS_ADD(flags, sum_fragment_bytes_ever, pc - f->start_pc);

    /* if we don't give the extra space back to fcache, need to nop out the
     * rest of the memory to avoid problems with shifting fcache pointers */
    if (PAD_FRAGMENT_JMPS(flags) && !INTERNAL_OPTION(pad_jmps_return_excess_padding)) {
        /* these can never be reached, but will be decoded by shift
         * fcache pointers */
        SET_TO_NOPS(dr_get_isa_mode(dcontext), pc, f->size - (pc - f->start_pc));
    } else {
        ASSERT(f->size - (pc - f->start_pc) == 0);
    }

    /* finalize the fragment
     * that means filling in all offsets, etc. that weren't known at
     * instrlist building time
     */
#ifdef PROFILE_RDTSC
    if (dynamo_options.profile_times)
        finalize_profile_call(dcontext, f);
#endif
#ifdef CHECK_RETURNS_SSE2
    finalize_return_check(dcontext, f);
#endif
    if ((flags & FRAG_IS_TRACE) != 0) {
        /* trace-only finalization */
#ifdef SIDELINE
        if (dynamo_options.sideline) {
            finalize_sideline_prefix(dcontext, f);
        }
#endif
    } else {
        /* bb-only finalization */
    }
    mangle_finalize(dcontext, ilist, f);

    /* add fragment to vm area lists */
    vm_area_add_fragment(dcontext, f, vmlist);

    /* store translation info, if requested */
    if (TEST(FRAG_HAS_TRANSLATION_INFO, f->flags)) {
        ASSERT(!TEST(FRAG_COARSE_GRAIN, f->flags));
        fragment_record_translation_info(dcontext, f, ilist);
    }

    /* if necessary, i-cache sync */
    machine_cache_sync((void *)f->start_pc, (void *)(f->start_pc + f->size), true);

    /* Future removal and replacement w/ the real fragment must be atomic
     * wrt linking, so we hold the change_linking_lock across both (xref
     * case 5474).
     * We must grab the change_linking_lock even for private fragments
     * if we have any shared fragments in the picture, to make atomic
     * our future fragment additions and removals and the associated
     * fragment and future fragment lookups.
     * Optimization: we could do away with this and try to only
     * grab it when a private fragment needs to create a shared
     * future, redoing our lookup with the lock held.
     */
    if (link_fragment || add_to_htable)
        SHARED_RECURSIVE_LOCK(acquire, change_linking_lock);

    if (link_fragment) {
        /* link BEFORE adding to the hashtable, to reduce races, though we
         * should be able to handle them :)
         */
        if (replace_fragment)
            shift_links_to_new_fragment(dcontext, replace_fragment, f);
        else
            link_new_fragment(dcontext, f);
    }

    if (add_to_htable) {
        if (TEST(FRAG_COARSE_GRAIN, f->flags)) {
            /* added in link_new_fragment */
        } else
            fragment_add(dcontext, f);

        DOCHECK(1, {
            if (TEST(FRAG_SHARED, flags))
                ASSERT(fragment_lookup_future(dcontext, tag) == NULL);
            else
                ASSERT(fragment_lookup_private_future(dcontext, tag) == NULL);
        });
    }

    if (link_fragment || add_to_htable)
        SHARED_RECURSIVE_LOCK(release, change_linking_lock);

    SELF_PROTECT_CACHE(dcontext, NULL, READONLY);

    KSTOP(emit);
    return f;
}

/* Emits code for ilist into the fcache, returns the created fragment.
 * Does not add the fragment to the ftable, leaving it as an "invisible"
 * fragment.  This means it is the caller's responsibility to ensure
 * it is properly disposed of when done with.
 * The fragment is also not linked, to give the caller more flexibility.
 */
fragment_t *
emit_invisible_fragment(dcontext_t *dcontext, app_pc tag, instrlist_t *ilist, uint flags,
                        void *vmlist)
{
    return emit_fragment_common(
        dcontext, tag, ilist, flags, vmlist, false /* don't link: up to caller */,
        false /* don't add: it's invisible! */, NULL /* not replacing */);
}

/* Emits code for ilist into the fcache, returns the created
 * fragment.  Adds the fragment to the fragment hashtable and
 * links it as a new fragment.
 */
fragment_t *
emit_fragment(dcontext_t *dcontext, app_pc tag, instrlist_t *ilist, uint flags,
              void *vmlist, bool link)
{
    return emit_fragment_common(dcontext, tag, ilist, flags, vmlist, link,
                                true /* add to htable */, NULL /* not replacing */);
}

/* Emits code for ilist into the fcache, returns the created
 * fragment.  Adds the fragment to the fragment hashtable and
 * links it as a new fragment.
 */
fragment_t *
emit_fragment_ex(dcontext_t *dcontext, app_pc tag, instrlist_t *ilist, uint flags,
                 void *vmlist, bool link, bool visible)
{
    return emit_fragment_common(dcontext, tag, ilist, flags, vmlist, link, visible,
                                NULL /* not replacing */);
}

/* Emits code for ilist into the fcache, returns the created
 * fragment.  Adds the fragment to the fragment hashtable and
 * links it as a new fragment by subsuming replace's links.
 */
fragment_t *
emit_fragment_as_replacement(dcontext_t *dcontext, app_pc tag, instrlist_t *ilist,
                             uint flags, void *vmlist, fragment_t *replace)
{
    return emit_fragment_common(dcontext, tag, ilist, flags, vmlist,
                                true /* link it up */, true /* add to htable */,
                                replace /* replace this fragment */);
}
