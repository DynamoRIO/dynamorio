/* **********************************************************
 * Copyright (c) 2010-2023 Google, Inc.  All rights reserved.
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
 * translate.c - fault translation
 */

#include "../globals.h"
#include "../link.h"
#include "../fragment.h"

#include "arch.h"
#include "instr.h"
#include "instr_create_shared.h"
#include "decode.h"
#include "decode_fast.h"
#include "../fcache.h"
#include "proc.h"
#include "instrument.h"

#if defined(DEBUG) || defined(INTERNAL)
#    include "disassemble.h"
#endif

/***************************************************************************
 * FAULT TRANSLATION
 *
 * Current status:
 * After PR 214962, PR 267260, PR 263407, PR 268372, and PR 267764/i398, we
 * properly translate indirect branch mangling and client modifications.
 * FIXME: However, we still do not properly translate for:
 * - PR 303413: properly translate native_exec and windows sysenter mangling faults
 * - PR 208037/i#399: flushed fragments (need -safe_translate_flushed)
 * - PR 213251: hot patch fragments (b/c nudge can change whether patched =>
 *     should store translations for all hot patch fragments)
 * - i#400/PR 372021: restore eflags if within window of ibl or trace-cmp eflags-are-dead
 * - i#751: fault translation has not been tested for x86_to_x64
 */

typedef struct _translate_walk_t {
    /* The context we're translating */
    priv_mcontext_t *mc;
    /* The code cache span of the containing fragment */
    byte *start_cache;
    byte *end_cache;
    /* PR 263407: Track registers spilled since the last cti, for
     * restoring indirect branch and rip-rel spills. UINT_MAX means
     * nothing recorded, otherwise holds offset of spill in local
     * spill space.
     */
    uint reg_spill_offs[REG_SPILL_NUM];
    bool reg_tls[REG_SPILL_NUM];
    /* PR 267260: Track our own mangle-inserted pushes and pops, for
     * restoring state in the middle of our indirect branch mangling.
     * This is the adjustment in the forward direction.
     */
    int xsp_adjust;
    /* Track whether we've seen an instr for which we can't relocate */
    bool unsupported_mangle;
    /* Are we currently in a mangle region */
    bool in_mangle_region;
    /* Are we currently in a mangle region's epilogue */
    bool in_mangle_region_epilogue;
    /* What is the translation target of the current mangle region */
    app_pc translation;
    /* Are we inside a clean call? */
    bool in_clean_call;
} translate_walk_t;

static void
translate_walk_init(translate_walk_t *walk, byte *start_cache, byte *end_cache,
                    priv_mcontext_t *mc)
{
    memset(walk, 0, sizeof(*walk));
    walk->mc = mc;
    walk->start_cache = start_cache;
    walk->end_cache = end_cache;
    for (int r = 0; r < REG_SPILL_NUM; r++)
        walk->reg_spill_offs[r] = UINT_MAX;
}

#ifdef UNIX
static inline bool
instr_is_inline_syscall_jmp(dcontext_t *dcontext, instr_t *inst)
{
    if (!instr_is_our_mangling(inst))
        return false;
        /* Not bothering to check whether there's a nearby syscall instr:
         * any label-targeting short jump should be fine to ignore.
         */
#    ifdef X86
    return (instr_get_opcode(inst) == OP_jmp_short &&
            opnd_is_instr(instr_get_target(inst)));
#    elif defined(AARCH64)
    return (instr_get_opcode(inst) == OP_b && opnd_is_instr(instr_get_target(inst)));
#    elif defined(ARM)
    return ((instr_get_opcode(inst) == OP_b_short ||
             /* A32 uses a regular jump */
             instr_get_opcode(inst) == OP_b) &&
            opnd_is_instr(instr_get_target(inst)));
#    else
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
#    endif /* X86/ARM */
}

static inline bool
instr_is_seg_ref_load(dcontext_t *dcontext, instr_t *inst)
{
#    ifdef X86
    /* This won't fault but we don't want "unsupported mangle instr" message. */
    if (!instr_is_our_mangling(inst))
        return false;
    /* Look for the load of either segment base */
    if (instr_is_tls_restore(inst, REG_NULL /*don't care*/,
                             os_tls_offset(os_get_app_tls_base_offset(SEG_FS))) ||
        instr_is_tls_restore(inst, REG_NULL /*don't care*/,
                             os_tls_offset(os_get_app_tls_base_offset(SEG_GS))))
        return true;
    /* Look for the lea */
    if (instr_get_opcode(inst) == OP_lea) {
        opnd_t mem = instr_get_src(inst, 0);
        if (opnd_get_scale(mem) == 1 &&
            opnd_get_index(mem) == opnd_get_reg(instr_get_dst(inst, 0)))
            return true;
    }
#    endif /* X86 */
    return false;
}

static inline bool
instr_is_rseq_mangling(dcontext_t *dcontext, instr_t *inst)
{
#    ifdef LINUX
    /* This won't fault but we don't want it marked as unsupported. */
    if (!instr_is_our_mangling(inst))
        return false;
    if (vmvector_empty(d_r_rseq_areas))
        return false;
    /* XXX: Keep this consistent with mangle_rseq_* in mangle_shared.c. */
    if (instr_get_opcode(inst) ==
            IF_X86_ELSE(OP_mov_ld, IF_RISCV64_ELSE(OP_lw, OP_ldr)) &&
        opnd_is_reg(instr_get_dst(inst, 0)) &&
        opnd_is_base_disp(instr_get_src(inst, 0))) {
        reg_id_t dst = opnd_get_reg(instr_get_dst(inst, 0));
        opnd_t memref = instr_get_src(inst, 0);
        int disp = opnd_get_disp(memref);
        if (reg_is_gpr(dst) && reg_is_pointer_sized(dst) &&
            opnd_get_index(memref) == DR_REG_NULL &&
            disp ==
                offsetof(dcontext_t, rseq_entry_state) +
                    sizeof(reg_t) * (dst - DR_REG_START_GPR))
            return true;
    } else if (instr_get_opcode(inst) ==
                   IF_X86_ELSE(OP_mov_st, IF_RISCV64_ELSE(OP_sw, OP_str)) &&
               opnd_is_reg(instr_get_src(inst, 0)) &&
               opnd_is_base_disp(instr_get_dst(inst, 0))) {
        reg_id_t dst = opnd_get_reg(instr_get_src(inst, 0));
        opnd_t memref = instr_get_dst(inst, 0);
        int disp = opnd_get_disp(memref);
        if (reg_is_gpr(dst) && reg_is_pointer_sized(dst) &&
            opnd_get_index(memref) == DR_REG_NULL &&
            disp ==
                offsetof(dcontext_t, rseq_entry_state) +
                    sizeof(reg_t) * (dst - DR_REG_START_GPR))
            return true;
    }
#        ifdef AARCH64
    if (instr_get_opcode(inst) == OP_mrs &&
        opnd_get_reg(instr_get_src(inst, 0)) == LIB_SEG_TLS)
        return true;
    if (instr_get_opcode(inst) == OP_movz || instr_get_opcode(inst) == OP_movk)
        return true;
    if (instr_get_opcode(inst) == OP_strh && opnd_is_base_disp(instr_get_dst(inst, 0)) &&
        opnd_get_disp(instr_get_dst(inst, 0)) == EXIT_REASON_OFFSET)
        return true;
    if (instr_get_opcode(inst) == OP_str && opnd_is_base_disp(instr_get_dst(inst, 0)) &&
        opnd_get_disp(instr_get_dst(inst, 0)) == rseq_get_tls_ptr_offset())
        return true;
#        endif
#    endif
    return false;
}
#endif /* UNIX */

#if defined(X86) && defined(UNIX)
static bool
instr_is_segment_mangling(dcontext_t *dcontext, instr_t *instr)
{
    if (!instr_is_our_mangling(instr))
        return false;
    /* Look for mangle_mov_seg() patterns. */
    int opc = instr_get_opcode(instr);
    if (opc == OP_nop) /* Write to seg. */
        return true;
    if (opc == OP_mov_ld || opc == OP_movzx) {
        opnd_t op_fs = opnd_create_sized_tls_slot(
            os_tls_offset(os_get_app_tls_reg_offset(SEG_FS)), OPSZ_2);
        opnd_t op_gs = opnd_create_sized_tls_slot(
            os_tls_offset(os_get_app_tls_reg_offset(SEG_GS)), OPSZ_2);
        return opnd_same(op_fs, instr_get_src(instr, 0)) ||
            opnd_same(op_gs, instr_get_src(instr, 0));
    }
    /* XXX: For mangle_seg_ref(), it could be any far memory operand, so we would
     * want to look at the prior instr?  No special translation is needed, but
     * we want to avoid being labeled as an unsupported mangle instr.
     */
    return false;
}
#endif

#ifdef ARM
static bool
instr_is_mov_PC_immed(dcontext_t *dcontext, instr_t *inst)
{
    if (!instr_is_our_mangling(inst))
        return false;
    return (instr_get_opcode(inst) == OP_movw || instr_get_opcode(inst) == OP_movt);
}
#endif

static bool
instr_is_load_mcontext_base(instr_t *inst)
{
    if (instr_get_opcode(inst) != OP_load || !opnd_is_base_disp(instr_get_src(inst, 0)))
        return false;
    return opnd_get_disp(instr_get_src(inst, 0)) ==
        os_tls_offset((ushort)TLS_DCONTEXT_SLOT);
}

#ifdef X86

/* FIXME i#3329: add support for ARM/AArch64. */

static bool
translate_walk_enters_mangling_epilogue(dcontext_t *tdcontext, instr_t *inst,
                                        translate_walk_t *walk)
{
    return !walk->in_mangle_region_epilogue && instr_is_our_mangling_epilogue(inst);
}

static bool
translate_walk_exits_mangling_epilogue(dcontext_t *tdcontext, instr_t *inst,
                                       translate_walk_t *walk)
{
    return walk->in_mangle_region_epilogue && !instr_is_our_mangling_epilogue(inst);
}
#endif

static void
translate_walk_track_pre_instr(dcontext_t *tdcontext, instr_t *inst,
                               translate_walk_t *walk)
{
    /* Two mangle regions can be adjacent: distinguish by translation field */
    if (walk->in_mangle_region &&
        /* On AArchXX, we spill registers across an app instr, so go solely on xl8 */
        (IF_X86(!instr_is_our_mangling(inst) ||)
         /* handle adjacent mangle regions */
         IF_X86(translate_walk_exits_mangling_epilogue(tdcontext, inst, walk) ||)
         /* Entering the mangling region's epilogue can have different xl8 */
         (IF_X86(!translate_walk_enters_mangling_epilogue(tdcontext, inst, walk) &&)
              instr_get_translation(inst) != walk->translation))) {
        LOG(THREAD_GET, LOG_INTERP, 4, "%s: from one mangle region to another\n",
            __FUNCTION__);
        /* We assume our manglings are local and contiguous: once out of a
         * mangling region, we're good to go again.
         */
        walk->in_mangle_region = false;
        walk->in_mangle_region_epilogue = false;
        walk->unsupported_mangle = false;
        walk->xsp_adjust = 0;
        for (reg_id_t r = 0; r < REG_SPILL_NUM; r++) {
#ifndef AARCHXX
            /* we should have seen a restore for every spill, unless at
             * fragment-ending jump to ibl, which shouldn't come here
             */
            ASSERT(walk->reg_spill_offs[r] == UINT_MAX);
            walk->reg_spill_offs[r] = UINT_MAX; /* be paranoid */
#else
            /* On AArchXX we do spill registers across app instrs and mangle
             * regions, though right now only the following routines do this:
             * - mangle_stolen_reg()
             * - mangle_gpr_list_read()
             * - mangle_reads_thread_register()
             * Each of these cases is a tls restore, and we assert as much.
             */
            DOCHECK(1, {
                if (walk->reg_spill_offs[r] != UINT_MAX) {
                    instr_t *curr;
                    bool spill_or_restore = false;
                    reg_id_t reg;
                    bool spill;
                    bool spill_tls;
                    for (curr = inst; curr != NULL; curr = instr_get_next(curr)) {
                        spill_or_restore = instr_is_DR_reg_spill_or_restore(
                            tdcontext, curr, &spill_tls, &spill, &reg, NULL);
                        if (spill_or_restore && r == reg - REG_START_SPILL)
                            break;
                    }
                    ASSERT(spill_or_restore && r == reg - REG_START_SPILL && !spill &&
                           spill_tls);
                }
            });
#endif
        }
    }
}

static void
translate_walk_track_post_instr(dcontext_t *tdcontext, instr_t *inst,
                                translate_walk_t *walk)
{
    reg_id_t reg, r;
    bool spill, spill_tls;

    if (instr_is_label(inst)) {
        if (instr_get_note(inst) == (void *)DR_NOTE_CALL_SEQUENCE_START)
            walk->in_clean_call = true;
        else if (instr_get_note(inst) == (void *)DR_NOTE_CALL_SEQUENCE_END)
            walk->in_clean_call = false;
    }
    if (instr_is_our_mangling(inst)) {
        if (!walk->in_mangle_region) {
            walk->in_mangle_region = true;
            walk->translation = instr_get_translation(inst);
            LOG(THREAD_GET, LOG_INTERP, 4, "%s: entering mangle region xl8=" PFX "\n",
                __FUNCTION__, walk->translation);
        } else if (IF_X86_ELSE(
                       translate_walk_enters_mangling_epilogue(tdcontext, inst, walk),
                       false)) {
            walk->in_mangle_region_epilogue = true;
            walk->translation = instr_get_translation(inst);
            LOG(THREAD_GET, LOG_INTERP, 4,
                "%s: entering mangle region epilogue xl8=" PFX "\n", __FUNCTION__,
                walk->translation);
        } else
            ASSERT(walk->translation == instr_get_translation(inst));
        /* We recognize a clean call by explicit labels or flags.
         * We do not track any stack or spills: we assume we will only
         * fault on an argument that references app memory, in which case
         * we restore to the priv_mcontext_t on the stack.
         */
        if (walk->in_clean_call) {
            DOLOG(4, LOG_INTERP, {
                d_r_loginst(get_thread_private_dcontext(), 4, inst,
                            "\tin clean call arg region");
            });
            return;
        }
        /* PR 263407: track register values that we've spilled.  We assume
         * that spilling to non-canonical slots only happens in ibl or
         * context switch code: never in app code mangling.  Since a client
         * might add ctis (non-linear code) and its own spills, we track
         * register spills only within our own mangling code (for
         * post-mangling traces (PR 306163) we require that the client
         * handle all translation if it modifies our mangling regions:
         * we'll provide a query routine instr_is_DR_mangling()): our
         * spills are all local anyway, except
         * for selfmod, which we hardcode rep-string support for (non-linear code
         * isn't handled by general reg scan).  Our trace cmp is the only
         * instance (besides selfmod) where we have a cti in our mangling,
         * but it doesn't affect our linearity assumption.  We assume we
         * have no entry points in between a spill and a restore.  Our
         * mangling goes in last (for regular bbs and traces; see
         * comment above for post-mangling traces), and so for local
         * spills like rip-rel and ind branches this is fine.
         */
#if defined(RISCV64)
        ASSERT_NOT_IMPLEMENTED(false);
#endif
        if (instr_is_cti(inst) &&
#ifdef X86
            /* Do not reset for a trace-cmp jecxz or jmp (32-bit) or
             * jne (64-bit), since ecx needs to be restored (won't
             * fault, but for thread relocation)
             */
            ((instr_get_opcode(inst) != OP_jecxz && instr_get_opcode(inst) != OP_jmp &&
              /* x64 trace cmp uses jne for exit */
              instr_get_opcode(inst) != OP_jne) ||
             /* Rather than check for trace, just ignore exit jumps, which
              * won't mess up linearity here. For stored translation info we
              * don't have meta-flags so we can't use instr_is_exit_cti(). */
             ((instr_get_opcode(inst) == OP_jmp ||
               /* x64 trace cmp uses jne for exit */
               instr_get_opcode(inst) == OP_jne) &&
              (!opnd_is_pc(instr_get_target(inst)) ||
               (opnd_get_pc(instr_get_target(inst)) >= walk->start_cache &&
                opnd_get_pc(instr_get_target(inst)) < walk->end_cache))))
#elif defined(RISCV64)
            /* FIXME i#3544: Not implemented */
            false
#else
            /* Do not reset for cbnz/bne in ldstex mangling, nor for the b after strex. */
            !(instr_get_opcode(inst) == OP_cbnz ||
              (instr_get_opcode(inst) == OP_b &&
               (instr_get_prev(inst) != NULL &&
                instr_get_opcode(instr_get_prev(inst)) == OP_subs)) ||
              (instr_get_opcode(inst) == OP_b &&
               (instr_get_prev(inst) != NULL &&
                instr_is_exclusive_store(instr_get_prev(inst)))))
#endif
        ) {
            /* FIXME i#1551: add ARM version of the series of trace cti checks above */
            IF_ARM(ASSERT_NOT_IMPLEMENTED(DYNAMO_OPTION(disable_traces)));
            /* FIXME i#3544: Implement traces */
            IF_RISCV64(ASSERT_NOT_IMPLEMENTED(DYNAMO_OPTION(disable_traces)));
            /* reset for non-exit non-trace-jecxz cti (i.e., selfmod cti) */
            LOG(THREAD_GET, LOG_INTERP, 4, "\treset spills on cti\n");
            for (r = 0; r < REG_SPILL_NUM; r++)
                walk->reg_spill_offs[r] = UINT_MAX;
        }
        uint offs = UINT_MAX;
        if (instr_is_DR_reg_spill_or_restore(tdcontext, inst, &spill_tls, &spill, &reg,
                                             &offs)) {
            r = reg - REG_START_SPILL;
            ASSERT(r < REG_SPILL_NUM);
            IF_ARM({
                /* Ignore the spill of r0 into TLS for syscall restart
                 * XXX: we're assuming it's immediately prior to the syscall.
                 */
                if (instr_get_next(inst) != NULL &&
                    instr_is_syscall(instr_get_next(inst)))
                    spill = false;
            });
            /* if a restore whose spill was before a cti, ignore */
            if (spill || walk->reg_spill_offs[r] != UINT_MAX) {
                /* Ensure restores and spills are properly paired up, but we do
                 * allow for redundant spills.
                 */
                ASSERT(spill || (!spill && walk->reg_spill_offs[r] != UINT_MAX));
                ASSERT(spill || walk->reg_tls[r] == spill_tls);
                if (spill) {
                    ASSERT(offs != UINT_MAX);
                    walk->reg_spill_offs[r] = offs;
                } else {
                    walk->reg_spill_offs[r] = UINT_MAX;
                }
                walk->reg_tls[r] = spill_tls;
                LOG(THREAD_GET, LOG_INTERP, 4, "\tspill update: %s %s %s offs=%u\n",
                    spill ? "spill" : "restore", spill_tls ? "tls" : "mcontext",
                    reg_names[reg], offs);
            }
        }
#ifdef AARCHXX
        else if (instr_is_stolen_reg_move(inst, &spill, &reg) ||
                 /* Accessing the stolen reg TLS slot does not satisfy the
                  * instr_is_DR_reg_spill_or_restore() check above b/c it's not
                  * a regular spill slot per reg_spill_tls_offs.
                  * We assume it does not need tracking: restore_stolen_register()
                  * is all we need as the window where we've swapped regs is just
                  * one app instr w/ no mangling or instru between.
                  */
                 instr_is_tls_restore(inst, dr_reg_stolen, TLS_REG_STOLEN_SLOT) ||
                 /* The store has the swapped register as the base. */
                 (instr_get_opcode(inst) == OP_store &&
                  opnd_get_reg(instr_get_src(inst, 0)) == dr_reg_stolen &&
                  opnd_get_disp(instr_get_dst(inst, 0)) ==
                      os_tls_offset(TLS_REG_STOLEN_SLOT))) {
            /* do nothing */
            LOG(THREAD_GET, LOG_INTERP, 4, "%s: stolen reg move\n", __FUNCTION__);
        }
#endif
        /* PR 267260: Track our own mangle-inserted pushes and pops, for
         * restoring state on an app fault in the middle of our indirect
         * branch mangling.  We only need to support instrs added up until
         * the last one that could have an app fault, as we can fail when
         * called to translate for thread relocation: thus we ignore
         * syscall mangling.
         *
         * The main scenarios are:
         *
         * 1) call*: "spill ecx; mov->ecx; push retaddr":
         *    ecx restore handled above
         * 2) far direct call: "push cs; push retaddr"
         *    if fail on 2nd push need to undo 1st push
         * 3) far call*: "spill ecx; tgt->ecx; push cs; push retaddr"
         *    if fail on 1st push, restore ecx (above); 2nd push, also undo 1st push
         * 4) iret: "pop eip; pop cs; pop eflags; (pop rsp; pop ss)"
         *    if fail on non-initial pop, undo earlier pops
         * 5) lret: "pop eip; pop cs"
         *    if fail on non-initial pop, undo earlier pops
         *
         * FIXME: some of these push/pops are simulated (we simply adjust
         * esp or do nothing), so we're not truly fault-transparent.
         */
        else if (instr_check_xsp_mangling(tdcontext, inst, &walk->xsp_adjust)) {
            /* walk->xsp_adjust is now adjusted */
        } else if (instr_is_trace_cmp(tdcontext, inst)) {
            /* nothing to do */
            /* We don't support restoring a fault in the middle, but we
             * identify here to avoid "unsupported mangle instr" message
             */
        } else if (instr_is_load_mcontext_base(inst)) {
            LOG(THREAD_GET, LOG_INTERP, 4, "\tmcontext base load\n");
            /* nothing to do */
        }
#ifdef UNIX
        else if (instr_is_inline_syscall_jmp(tdcontext, inst)) {
            /* nothing to do */
        } else if (instr_is_seg_ref_load(tdcontext, inst)) {
            /* nothing to do */
        } else if (instr_is_rseq_mangling(tdcontext, inst)) {
            /* nothing to do */
        }
#endif
#if defined(X86) && defined(UNIX)
        else if (instr_is_segment_mangling(tdcontext, inst)) {
            /* nothing to do */
        }
#endif
#ifdef ARM
        else if (instr_is_mov_PC_immed(tdcontext, inst)) {
            /* nothing to do */
        }
#endif
#ifdef AARCHXX
        else if (instr_is_ldstex_mangling(tdcontext, inst)) {
            /* nothing to do */
        }
#endif
        /* Single step mangling adds a nop. */
        else if (instr_is_nop(inst)) {
            /* nothing to do */
        } else if (instr_is_app(inst)) {
            /* To have reg spill+restore in the same mangle region, we mark
             * the (modified) app instr for rip-rel and for segment mangling as
             * "our mangling".  There's nothing specific to do for it.
             */
        }
        /* We do not support restoring state at arbitrary points for thread
         * relocation (a performance issue, not a correctness one): if not a
         * spill, restore, push, or pop, we will not properly translate.
         * For an exit jmp for a simple ret we could relocate: but better not to
         * for a call, since we've modified the stack w/ a push, so we fail on
         * all exit jmps.
         */
        else {
            /* XXX: Maybe this should be a full SYSLOG since it can lead to
             * translation failure.
             */
            /* TODO i#5069 There are unsupported mangle instrs on AArch64
             * that this function is yet not able to recognise.
             */
            DOLOG(2, LOG_INTERP,
                  d_r_loginst(get_thread_private_dcontext(), 2, inst,
                              "unsupported mangle instr"););
            walk->unsupported_mangle = true;
        }
    }
}

static bool
translate_walk_good_state(dcontext_t *tdcontext, translate_walk_t *walk,
                          app_pc translate_pc)
{
    return (!walk->unsupported_mangle ||
            /* If we're at the instr AFTER the mangle region, or at an instruction
             * in the mangled region's EPILOGUE, we're ok.
             */
            (walk->in_mangle_region && translate_pc != walk->translation));
}

static void
translate_walk_restore(dcontext_t *tdcontext, translate_walk_t *walk, instr_t *inst,
                       app_pc translate_pc)
{
    reg_id_t r;

    if (IF_X86_ELSE(translate_walk_enters_mangling_epilogue(tdcontext, inst, walk),
                    false)) {
        /* We handle only simple symmetric one-spill/one-restore mangling cases
         * when xl8 inst addresses in mangling epilogue. Everything else is
         * currently not supported. In this case, the restore routine here acts
         * as if it was emulating the epilogue instructions, because we xl8 the
         * PC post-app instruction. This is semantically different from restoring
         * the state pre-app instruction, as this routine originally intended.
         * This works, because only the simple spill-restore mangle case is
         * supported (xref i#3307). For more complex cases, this should get factored
         * out into a separate routine that walks the epilogue and advances the state
         * accordingly.
         */
        LOG(THREAD_GET, LOG_INTERP, 2,
            "\ttranslation " PFX " is in mangling epilogue " PFX
            " checking for simple symmetric mangling case\n",
            translate_pc, walk->translation);
        DOCHECK(1, {
            bool spill_seen = false;
            for (r = 0; r < REG_SPILL_NUM; r++) {
                if (walk->reg_spill_offs[r] != UINT_MAX) {
                    ASSERT_NOT_IMPLEMENTED(!spill_seen);
                    spill_seen = true;
                }
            }
            bool tls;
            bool spill;
            uint offs;
            if (instr_is_reg_spill_or_restore(tdcontext, inst, &tls, &spill, NULL,
                                              &offs)) {
                ASSERT_NOT_IMPLEMENTED(!spill);
            } else if (!tls || offs == -1 ||
                       offs != os_tls_offset((ushort)MANGLE_RIPREL_SPILL_SLOT)) {
                /* Riprel mangling can put arbitrary registers into
                 * MANGLE_RIPREL_SPILL_SLOT and as such is not recognized as regular
                 * spill/restore by instr_is_reg_spill_or_restore. Either way, we don't
                 * support cases that are more complex than one spill and restore in this
                 * context if instruction was part of mangling epilogue.
                 */
                ASSERT_NOT_IMPLEMENTED(false);
            }
            DOCHECK(1, {
                /* Enforcing here what mangling needs to obey.  We can, however,
                 * have a rip-rel mangled push/pop, for which our post-instr xl8 is fine
                 * w/o restoring anything about the stack.
                 */
                instr_t instr;
                instr_init(tdcontext, &instr);
                ASSERT(walk->translation < translate_pc);
                app_pc npc = decode(tdcontext, walk->translation, &instr);
                ASSERT(npc != NULL && instr_valid(&instr));
                IF_X86(int opc = instr_get_opcode(&instr);)
                ASSERT_NOT_IMPLEMENTED(
                    walk->xsp_adjust ==
                    0 IF_X86(|| opc == OP_push || opc == OP_push_imm || opc == OP_pop));
                instr_free(tdcontext, &instr);
            });
        });
    }

    /* PR 263407: restore register values that are currently in spill slots
     * for ind branches or rip-rel mangling.
     * FIXME: for rip-rel loads, we may have clobbered the destination
     * already, and won't be able to restore it: but that's a minor issue.
     */
    for (r = 0; r < REG_SPILL_NUM; r++) {
        if (walk->reg_spill_offs[r] != UINT_MAX) {
            reg_id_t reg = r + REG_START_SPILL;
            reg_t value;
            if (walk->reg_tls[r]) {
                value =
                    *(reg_t *)(((byte *)&tdcontext->local_state->spill_space) +
                               os_local_state_offset((ushort)walk->reg_spill_offs[r]));
            } else {
                value = reg_get_value_priv(reg, get_mcontext(tdcontext));
            }
            LOG(THREAD_GET, LOG_INTERP, 2, "\trestoring spilled %s to " PFX "\n",
                reg_names[reg], value);
            STATS_INC(recreate_spill_restores);
            reg_set_value_priv(reg, walk->mc, value);
        }
    }

    if (translate_pc !=
        walk->translation IF_X86(
            &&!translate_walk_enters_mangling_epilogue(tdcontext, inst, walk))) {
        /* When we walk we update only each instr we pass.  If we're
         * now sitting at the instr AFTER the mangle region, we do
         * NOT want to adjust xsp, since we're not translating to
         * before that instr.  We should not have any outstanding spills.
         */
        LOG(THREAD_GET, LOG_INTERP, 2,
            "\ttranslation " PFX " is post-walk " PFX " so not fixing xsp\n",
            translate_pc, walk->translation);
    } else {
        /* PR 267260: Restore stack-adjust mangling of ctis.
         * FIXME: we do NOT undo writes to the stack, so we're not completely
         * transparent.  If we ever do restore memory, we'll want to pass in
         * the restore_memory param.
         */
        if (walk->xsp_adjust != 0) {
            walk->mc->xsp -= walk->xsp_adjust; /* negate to undo */
            LOG(THREAD_GET, LOG_INTERP, 2, "\tundoing push/pop by %d: xsp now " PFX "\n",
                walk->xsp_adjust, walk->mc->xsp);
        }
    }
}

static void
translate_restore_clean_call(dcontext_t *tdcontext, translate_walk_t *walk)
{
    /* We restore to the priv_mcontext_t that was pushed on the stack.
     * FIXME i#4219: This is not safe: see comment below.
     */
    LOG(THREAD_GET, LOG_INTERP, 2, "\ttranslating clean call arg crash\n");
    dr_get_mcontext_priv(tdcontext, NULL, walk->mc);
    /* walk->mc->pc will be fixed up by caller */

    /* PR 306410: up to caller to shift signal or SEH frame from dstack
     * to app stack.  We naturally do that already for linux b/c we always
     * have an alternate signal handling stack, but for Windows it takes
     * extra work.
     */
}

app_pc
translate_restore_special_cases(dcontext_t *dcontext, app_pc pc)
{
#ifdef LINUX
    app_pc handler;
    if (rseq_get_region_info(pc, NULL, NULL, &handler, NULL, NULL)) {
        LOG(THREAD_GET, LOG_INTERP, 2,
            "recreate_app: moving " PFX " inside rseq region to handler " PFX "\n", pc,
            handler);
        /* Remember the original for translate_last_direct_translation. */
        dcontext->client_data->last_special_xl8 = pc;
        return handler;
    }
    dcontext->client_data->last_special_xl8 = NULL;
#endif
    return pc;
}

app_pc
translate_last_direct_translation(dcontext_t *dcontext, app_pc pc)
{
#ifdef LINUX
    app_pc handler;
    if (dcontext->client_data->last_special_xl8 != NULL &&
        rseq_get_region_info(dcontext->client_data->last_special_xl8, NULL, NULL,
                             &handler, NULL, NULL) &&
        pc == handler)
        return dcontext->client_data->last_special_xl8;
#endif
    return pc;
}

void
translate_clear_last_direct_translation(dcontext_t *dcontext)
{
#ifdef LINUX
    dcontext->client_data->last_special_xl8 = NULL;
#endif
}

/* Returns a success code, but makes a best effort regardless.
 * If just_pc is true, only recreates pc.
 * Modifies mc with the recreated state.
 * The caller must ensure tdcontext remains valid.
 */
/* Use THREAD_GET instead of THREAD so log messages go to calling thread */
static recreate_success_t
recreate_app_state_from_info(dcontext_t *tdcontext, const translation_info_t *info,
                             byte *start_cache, byte *end_cache, priv_mcontext_t *mc,
                             bool just_pc _IF_DEBUG(uint flags))
{
    byte *answer = NULL;
    byte *cpc, *prev_cpc;
    cache_pc target_cache = mc->pc;
    uint i;
    bool contig = true, ours = false, in_clean_call = false;
    recreate_success_t res = (just_pc ? RECREATE_SUCCESS_PC : RECREATE_SUCCESS_STATE);
    instr_t instr;
    translate_walk_t walk;
    translate_walk_init(&walk, start_cache, end_cache, mc);
    instr_init(tdcontext, &instr);

    ASSERT(info != NULL);
    ASSERT(end_cache >= start_cache);

    LOG(THREAD_GET, LOG_INTERP, 3,
        "recreate_app : looking for " PFX " in frag @ " PFX " (tag " PFX ")\n",
        target_cache, start_cache, info->translation[0].app);
    DOLOG(3, LOG_INTERP, { translation_info_print(info, start_cache, THREAD_GET); });

    /* Strategy: walk through cache instrs, updating current app translation
     * as we go along from the info table.  The table records only
     * translations at change points and must interpolate between them, using
     * either a stride of 0 if the previous translation entry is marked
     * "identical" or a stride equal to the instruction length as we decode
     * from the cache if the previous entry is !identical=="contiguous".
     */

    cpc = start_cache;
    ASSERT(cpc - start_cache == info->translation[0].cache_offs);
    i = 0;
    while (cpc < end_cache) {
        LOG(THREAD_GET, LOG_INTERP, 5, "cache pc " PFX " vs " PFX "\n", cpc,
            target_cache);
        /* we can go beyond the end of the table: then use the last point */
        if (i < info->num_entries &&
            cpc - start_cache >= info->translation[i].cache_offs) {
            /* We hit a change point: new app translation target */
            answer = info->translation[i].app;
            contig = !TEST(TRANSLATE_IDENTICAL, info->translation[i].flags);
            ours = TEST(TRANSLATE_OUR_MANGLING, info->translation[i].flags);
            in_clean_call = TEST(TRANSLATE_CLEAN_CALL, info->translation[i].flags);
            i++;
        }

        if (cpc >= target_cache) {
            /* we found the target to translate */
            ASSERT(cpc == target_cache);
            if (cpc > target_cache) { /* in debug will hit assert 1st */
                LOG(THREAD_GET, LOG_INTERP, 2,
                    "recreate_app -- WARNING: cache pc " PFX " != " PFX "\n", cpc,
                    target_cache);
                res = RECREATE_FAILURE; /* try to restore, but return false */
            }
            break;
        }

        /* PR 263407/PR 268372: we need to decode to instr level to track register
         * values that we've spilled, and watch for ctis.  So far we don't need
         * enough to justify a full decode_fragment().
         */
        instr_reset(tdcontext, &instr);
        prev_cpc = cpc;
        cpc = decode(tdcontext, cpc, &instr);
        if (cpc == NULL) {
            LOG(THREAD_GET, LOG_INTERP, 2,
                "recreate_app -- failed to decode cache pc " PFX "\n", cpc);
            ASSERT_NOT_REACHED();
            instr_free(tdcontext, &instr);
            return RECREATE_FAILURE;
        }
        instr_set_our_mangling(&instr, ours);
        /* Sets the translation so that spilled registers can be restored. */
        instr_set_translation(&instr, answer);
        translate_walk_track_pre_instr(tdcontext, &instr, &walk);
        translate_walk_track_post_instr(tdcontext, &instr, &walk);
        /* We directly set this field rather than inserting synthetic labels. */
        walk.in_clean_call = in_clean_call;

        /* advance translation by the stride: either instr length or 0 */
        if (contig)
            answer += (cpc - prev_cpc);
        /* else, answer stays put */
    }
    /* should always find xlation */
    ASSERT(cpc < end_cache);
    instr_free(tdcontext, &instr);

    if (answer == NULL || !translate_walk_good_state(tdcontext, &walk, answer)) {
        /* PR 214962: we're either in client meta-code (NULL translation) or
         * post-app-fault in our own manglings: we shouldn't get an app
         * fault in either case, so it's ok to fail, and neither is a safe
         * spot for thread relocation.  For client meta-code we could split
         * synch view (since we can get the app state consistent, just not
         * the client state) from synch relocate, but that would require
         * synchall re-architecting and may not be a noticeable perf win
         * (should spend enough time at syscalls that will hit safe spot in
         * reasonable time).
         */
        /* PR 302951: our clean calls do show up here and have full state.
         * FIXME i#4219: Actually we do *not* always have full state: for asynch
         * xl8 we could be before setup or after teardown of the mcontext on the
         * dstack, and with leaner clean calls we might not have the full mcontext.
         */
        if (answer == NULL && walk.in_clean_call)
            translate_restore_clean_call(tdcontext, &walk);
        else
            res = RECREATE_SUCCESS_PC; /* failed on full state, but pc good */
        /* should only happen for thread synch, not a fault */
        DOCHECK(1, {
            if (!(res == RECREATE_SUCCESS_STATE /* clean call */ ||
                  tdcontext != get_thread_private_dcontext() ||
                  INTERNAL_OPTION(stress_recreate_pc) ||
                  /* we can currently fail for flushed code (PR 208037/i#399)
                   * (and hotpatch, native_exec, and sysenter: but too rare to check) */
                  TEST(FRAG_SELFMOD_SANDBOXED, flags) || TEST(FRAG_WAS_DELETED, flags))) {
                CLIENT_ASSERT(false,
                              "meta-instr faulted?  must set translation"
                              " field and handle fault!");
            }
        });
        if (answer == NULL) {
            /* use next instr's translation.  skip any further meta-instrs regions. */
            for (; i < info->num_entries; i++) {
                if (info->translation[i].app != NULL)
                    break;
            }
            ASSERT(i < info->num_entries);
            if (i < info->num_entries)
                answer = info->translation[i].app;
            ;
            ASSERT(answer != NULL);
        }
    }

    if (!just_pc)
        translate_walk_restore(tdcontext, &walk, &instr, answer);
    answer = translate_restore_special_cases(tdcontext, answer);
    LOG(THREAD_GET, LOG_INTERP, 2, "recreate_app -- found ok pc " PFX "\n", answer);
    mc->pc = answer;
    return res;
}

/* Returns a success code, but makes a best effort regardless.
 * If just_pc is true, only recreates pc.
 * Modifies mc with the recreated state.
 * The caller must ensure tdcontext remains valid.
 */
/* Use THREAD_GET instead of THREAD so log messages go to calling thread */
static recreate_success_t
recreate_app_state_from_ilist(dcontext_t *tdcontext, instrlist_t *ilist, byte *start_app,
                              byte *start_cache, byte *end_cache, priv_mcontext_t *mc,
                              bool just_pc, uint flags)
{
    byte *answer = NULL;
    byte *cpc, *prev_bytes;
    instr_t *inst, *prev_ok;
    cache_pc target_cache = mc->pc;
    recreate_success_t res = (just_pc ? RECREATE_SUCCESS_PC : RECREATE_SUCCESS_STATE);
    translate_walk_t walk;

    LOG(THREAD_GET, LOG_INTERP, 3,
        "recreate_app : looking for " PFX " in frag @ " PFX " (tag " PFX ")\n",
        target_cache, start_cache, start_app);

    DOLOG(5, LOG_INTERP, { instrlist_disassemble(tdcontext, 0, ilist, THREAD_GET); });

    /* walk ilist, incrementing cache pc by each instr's length until
     * cache pc equals target, then look at original address of
     * current instr, which is set by routines in mangle except for
     * cti_short_rewrite.
     */
    cpc = start_cache;
    /* since asking for the length will encode to a buffer, we cannot
     * walk backwards at all.  thus we keep track of the previous instr
     * with valid original bytes.
     */
    prev_ok = NULL;
    prev_bytes = NULL;

    translate_walk_init(&walk, start_cache, end_cache, mc);

    for (inst = instrlist_first(ilist); inst; inst = instr_get_next(inst)) {
        int len = instr_length(tdcontext, inst);

        /* All we care about is that we are not going to skip over a
         * bundle of app instructions.
         */
        ASSERT(!instr_is_level_0(inst));

        /* Case 4531, 4344: raw instructions being up-decoded can have
         * their translation fields clobbered so we don't want any of those.
         * (We used to have raw jecxz and nop instrs.)  But we do have cases
         * of !instr_operands_valid() (rseq signature instr-as-data; or
         * if the bb associated with this instr was hot patched, then
         * the inserted raw instructions can trigger this assert).
         */

        /* PR 332437: skip label instrs.  Nobody should expect setting
         * a label's translation field to have any effect, and we
         * don't need to explicitly split our mangling regions at
         * labels so no reason to call translate_walk_track().
         *
         * We also skip all other length 0 instrs.  That would
         * include un-encodable instrs, which we wouldn't have output,
         * and so we should skip here in case the very next instr that we
         * did encode had the real fault.
         */
        if (len == 0)
            continue;

        /* note this will be exercised for all instructions up to the answer */

        translate_walk_track_pre_instr(tdcontext, inst, &walk);

        LOG(THREAD_GET, LOG_INTERP, 5, "cache pc " PFX " vs " PFX "\n", cpc,
            target_cache);
        if (cpc + len > target_cache && instr_is_cti_short_rewrite(inst, cpc)) {
            /* The target is inside the short-cti bundle.  Everything should be fine:
             * there are no state changes inside.
             */
            LOG(THREAD_GET, LOG_INTERP, 3,
                "recreate_app -- target is inside short-cti bundle %p-%p\n", cpc,
                cpc + len);
            cpc = target_cache;
        }
        if (cpc >= target_cache) {
            if (cpc > target_cache) {
                if (cpc == start_cache) {
                    /* Prefix instructions are not added to recreate_fragment_ilist()
                     * FIXME: we should do so, and then we can at least restore
                     * our spills, just in case.
                     */
                    LOG(THREAD_GET, LOG_INTERP, 2,
                        "recreate_app -- cache pc " PFX " != " PFX ", "
                        "assuming a prefix instruction\n",
                        cpc, target_cache);
                    res = RECREATE_SUCCESS_PC; /* failed on full state, but pc good */
                    /* Should only happen for thread synch, not a fault. Checking whether
                     * tdcontext is the same as this thread's private dcontext is a weak
                     * indicator of xl8 due to a fault. */
                    ASSERT_CURIOSITY(tdcontext != get_thread_private_dcontext() ||
                                     INTERNAL_OPTION(stress_recreate_pc) ||
                                     tdcontext->client_data->is_translating);
                } else {
                    LOG(THREAD_GET, LOG_INTERP, 2,
                        "recreate_app -- WARNING: cache pc " PFX " != " PFX ", "
                        "probably prefix instruction\n",
                        cpc, target_cache);
                    res = RECREATE_FAILURE; /* try to restore, but return false */
                }
            }
            if (instr_get_translation(inst) == NULL) {
                /* Clients are supposed to leave their meta instrs with
                 * NULL translations.  (DR may hit this assert for
                 * -optimize but we need to fix that by setting translation
                 * for all our optimizations.)  We assume we will never
                 * get an app fault here, so we fail if asked for full state
                 * since although we can get full app state we can't relocate
                 * in the middle of client meta code.
                 */
                ASSERT(instr_is_meta(inst));
                /* PR 302951: our clean calls do show up here and have full state.
                 * FIXME i#4219: This is not safe: see comment above.
                 */
                if (walk.in_clean_call)
                    translate_restore_clean_call(tdcontext, &walk);
                else
                    res = RECREATE_SUCCESS_PC; /* failed on full state, but pc good */
                /* should only happen for thread synch, not a fault */
                DOCHECK(1, {
                    if (!(instr_is_our_mangling(inst) /* PR 302951 */ ||
                          tdcontext != get_thread_private_dcontext() ||
                          INTERNAL_OPTION(stress_recreate_pc) ||
                          tdcontext->client_data->is_translating)) {
                        CLIENT_ASSERT(false,
                                      "meta-instr faulted?  must set translation "
                                      "field and handle fault!");
                    }
                });
                if (prev_ok == NULL) {
                    answer = start_app;
                    LOG(THREAD_GET, LOG_INTERP, 2,
                        "recreate_app -- WARNING: guessing start pc " PFX "\n", answer);
                } else {
                    answer = prev_bytes;
                    LOG(THREAD_GET, LOG_INTERP, 2,
                        "recreate_app -- WARNING: guessing after prev "
                        "translation (pc " PFX ")\n",
                        answer);
                    DOLOG(2, LOG_INTERP,
                          d_r_loginst(get_thread_private_dcontext(), 2, prev_ok,
                                      "\tprev instr"););
                }
            } else {
                answer = instr_get_translation(inst);
                if (translate_walk_good_state(tdcontext, &walk, answer)) {
                    LOG(THREAD_GET, LOG_INTERP, 2,
                        "recreate_app -- found valid state pc " PFX "\n", answer);
                } else {
                    LOG(THREAD_GET, LOG_INTERP, 2,
                        "recreate_app -- invalid state: unsup=%d in-mangle=%d xl8=%p "
                        "walk=%p\n",
                        walk.unsupported_mangle, walk.in_mangle_region, answer,
                        walk.translation);
#ifdef X86
                    int op = instr_get_opcode(inst);
                    if (TEST(FRAG_SELFMOD_SANDBOXED, flags) &&
                        (op == OP_rep_ins || op == OP_rep_movs || op == OP_rep_stos)) {
                        /* i#398: xl8 selfmod: rep string instrs have xbx spilled in
                         * thread-private slot.  We assume no other selfmod mangling
                         * has a reg spilled at time of app instr execution.
                         */
                        if (!just_pc) {
                            walk.mc->xbx = get_mcontext(tdcontext)->xbx;
                            LOG(THREAD_GET, LOG_INTERP, 2,
                                "\trestoring spilled xbx to " PFX "\n", walk.mc->xbx);
                            STATS_INC(recreate_spill_restores);
                        }
                        LOG(THREAD_GET, LOG_INTERP, 2,
                            "recreate_app -- found valid state pc " PFX "\n", answer);
                    } else
#endif /* X86 */
                    {
                        res = RECREATE_SUCCESS_PC; /* failed on full state, but pc good */
                        /* should only happen for thread synch, not a fault */
                        ASSERT(tdcontext != get_thread_private_dcontext() ||
                               INTERNAL_OPTION(stress_recreate_pc) ||
                               tdcontext->client_data->is_translating ||
                               /* we can currently fail for flushed code (PR 208037)
                                * (and hotpatch, native_exec, and sysenter: but too
                                * rare to check) */
                               TEST(FRAG_SELFMOD_SANDBOXED, flags) ||
                               TEST(FRAG_WAS_DELETED, flags));
                        LOG(THREAD_GET, LOG_INTERP, 2,
                            "recreate_app -- not able to fully recreate "
                            "context, pc is in added instruction from mangling\n");
                    }
                }
            }
            if (!just_pc)
                translate_walk_restore(tdcontext, &walk, inst, answer);
            answer = translate_restore_special_cases(tdcontext, answer);
            LOG(THREAD_GET, LOG_INTERP, 2, "recreate_app -- found ok pc " PFX "\n",
                answer);
            mc->pc = answer;
            return res;
        }
        /* we only use translation pointers, never just raw bit pointers */
        if (instr_get_translation(inst) != NULL) {
            prev_ok = inst;
            DOLOG(4, LOG_INTERP,
                  d_r_loginst(get_thread_private_dcontext(), 4, prev_ok, "\tok instr"););
            prev_bytes = instr_get_translation(inst);
            if (instr_is_app(inst)) {
                /* we really want the pc after the translation target since we'll
                 * use this if we pass up the target without hitting it:
                 * unless this is a meta instr in which case we assume the
                 * real instr is ahead (FIXME: there could be cases where
                 * we want the opposite: how know?)
                 */
                /* FIXME: do we need to check for readability first?
                 * in normal usage all translation targets should have been decoded
                 * already while building the bb ilist
                 */
                prev_bytes = decode_next_pc(tdcontext, prev_bytes);
            }
        }

        translate_walk_track_post_instr(tdcontext, inst, &walk);

        cpc += len;
    }

    /* ERROR! */
    LOG(THREAD_GET, LOG_INTERP, 1,
        "ERROR: recreate_app : looking for " PFX " in frag @ " PFX " "
        "(tag " PFX ")\n",
        target_cache, start_cache, start_app);
    DOLOG(1, LOG_INTERP, { instrlist_disassemble(tdcontext, 0, ilist, THREAD_GET); });
    ASSERT_NOT_REACHED();
    if (just_pc) {
        /* just guess */
        answer = translate_restore_special_cases(tdcontext, answer);
        mc->pc = answer;
    }
    return RECREATE_FAILURE;
}

static instrlist_t *
recreate_selfmod_ilist(dcontext_t *dcontext, fragment_t *f)
{
    cache_pc selfmod_copy;
    instrlist_t *ilist;
    instr_t *inst;
    ASSERT(TEST(FRAG_SELFMOD_SANDBOXED, f->flags));
    /* If f is selfmod, app code may have changed (we see this w/ code
     * on the stack later flushed w/ os_thread_stack_exit(), though in that
     * case we don't expect it to be executed again), so we do a special
     * recreate from the selfmod copy.
     * Since selfmod is straight-line code we can rebuild from cache and
     * offset each translation entry
     */
    selfmod_copy = FRAGMENT_SELFMOD_COPY_PC(f);
    ASSERT(!TEST(FRAG_IS_TRACE, f->flags));
    ASSERT(!TEST(FRAG_HAS_DIRECT_CTI, f->flags));
    /* We must build our ilist w/o calling check_thread_vm_area(), as it will
     * freak out that we are decoding DR memory.
     */
    /* Be sure to "pretend" the bb is for f->tag, b/c selfmod instru is
     * different based on whether pc's are in low 2GB or not.
     */
    ilist = recreate_bb_ilist(dcontext, selfmod_copy, (byte *)f->tag,
                              /* Be sure to limit the size (i#1441) */
                              selfmod_copy + FRAGMENT_SELFMOD_COPY_CODE_SIZE(f),
                              FRAG_SELFMOD_SANDBOXED, NULL, NULL,
                              false /*don't check vm areas!*/, true /*mangle*/, NULL,
                              true /*call client*/, false /*!for_trace*/);
    ASSERT(ilist != NULL); /* shouldn't fail: our own code is always readable! */
    for (inst = instrlist_first(ilist); inst; inst = instr_get_next(inst)) {
        app_pc app = instr_get_translation(inst);
        if (app != NULL)
            instr_set_translation(inst, app - selfmod_copy + f->tag);
    }
    return ilist;
}

static void
restore_stolen_register(dcontext_t *dcontext, priv_mcontext_t *mcontext)
{
#ifdef AARCHXX
    /* dr_reg_stolen is holding DR's TLS on receiving a signal,
     * so we need put app's reg value into mcontext instead
     */
    LOG(THREAD_GET, LOG_INTERP, 2, "\trestoring stolen register to " PFX "\n",
        dcontext->local_state->spill_space.reg_stolen);
    set_stolen_reg_val(mcontext, dcontext->local_state->spill_space.reg_stolen);
#endif
}

/* The esp in mcontext must either be valid or NULL (if null will be unable to
 * recreate on XP and 03 at vsyscall_after_syscall and on sygate 2k at after syscall).
 * Returns true if successful.  Whether successful or not, attempts to modify
 * mcontext with recreated state. If just_pc only translates the pc
 * (this is more likely to succeed)
 */
/* Use THREAD_GET instead of THREAD so log messages go to calling thread */
/* Also see NOTEs at recreate_app_state() about lock usage, and lack of full stack
 * translation. */
static recreate_success_t
recreate_app_state_internal(dcontext_t *tdcontext, priv_mcontext_t *mcontext,
                            bool just_pc, fragment_t *owning_f, bool restore_memory)
{
    recreate_success_t res = (just_pc ? RECREATE_SUCCESS_PC : RECREATE_SUCCESS_STATE);
    dr_mcontext_t xl8_mcontext;
    dr_mcontext_t raw_mcontext;
    dr_mcontext_init(&xl8_mcontext);
    dr_mcontext_init(&raw_mcontext);
#ifdef WINDOWS
    if (get_syscall_method() == SYSCALL_METHOD_SYSENTER &&
        mcontext->pc == vsyscall_after_syscall && mcontext->xsp != 0) {
        ASSERT(get_os_version() >= WINDOWS_VERSION_XP);
        /* case 5441 sygate hack means ret addr to after_syscall will be at
         * esp+4 (esp will point to ret in ntdll.dll) for sysenter */
        /* FIXME - should we check that esp is readable? */
        if (is_after_syscall_address(
                tdcontext,
                *(cache_pc *)(mcontext->xsp +
                              (DYNAMO_OPTION(sygate_sysenter) ? 4 : 0)))) {
            /* no translation needed, ignoring sysenter stack hacks */
            LOG(THREAD_GET, LOG_INTERP | LOG_SYNCH, 2,
                "recreate_app no translation needed (at vsyscall)\n");
            if (!just_pc)
                restore_stolen_register(tdcontext, mcontext);
            if (dr_xl8_hook_exists()) {
                if (!instrument_restore_nonfcache_state_prealloc(
                        tdcontext, restore_memory, mcontext, &xl8_mcontext))
                    return RECREATE_FAILURE;
            }
            return res;
        } else {
            /* this is a dynamo system call! */
            LOG(THREAD_GET, LOG_INTERP | LOG_SYNCH, 2,
                "recreate_app at dynamo system call\n");
            return RECREATE_FAILURE;
        }
    }
#else
    if (get_syscall_method() == SYSCALL_METHOD_SYSENTER &&
        /* Even when the main syscall method is sysenter, we also have a
         * do_int_syscall and do_clone_syscall that use int, so check only
         * the main syscall routine.
         * Note that we don't modify the stack, so once we do sysenter syscalls
         * inlined in the cache (PR 288101) we'll need some mechanism to
         * distinguish those: but for now if a sysenter instruction is used it
         * has to be do_syscall since DR's own syscalls are ints.
         */
        (mcontext->pc == vsyscall_sysenter_return_pc ||
         is_after_main_do_syscall_addr(tdcontext, mcontext->pc) ||
         /* Check for pointing right at sysenter, for i#1145 */
         mcontext->pc + SYSENTER_LENGTH == vsyscall_syscall_end_pc ||
         is_after_main_do_syscall_addr(tdcontext, mcontext->pc + SYSENTER_LENGTH) ||
         /* Check for pointing at the sysenter-restart int 0x80 for i#2659 */
         mcontext->pc + SYSENTER_LENGTH == vsyscall_sysenter_return_pc)) {
        /* If at do_syscall yet not yet in the kernel (or the do_syscall still uses
         * int: i#2005), we need to translate to vsyscall, for detach (i#95).
         */
        if (is_after_main_do_syscall_addr(tdcontext, mcontext->pc)) {
            LOG(THREAD_GET, LOG_INTERP | LOG_SYNCH, 2,
                "recreate_app: from do_syscall " PFX " to vsyscall " PFX "\n",
                mcontext->pc, vsyscall_sysenter_return_pc);
            mcontext->pc = vsyscall_sysenter_return_pc;
        } else if (is_after_main_do_syscall_addr(tdcontext,
                                                 mcontext->pc + SYSENTER_LENGTH)) {
            LOG(THREAD_GET, LOG_INTERP | LOG_SYNCH, 2,
                "recreate_app: from do_syscall " PFX " to vsyscall " PFX "\n",
                mcontext->pc, vsyscall_syscall_end_pc - SYSENTER_LENGTH);
            mcontext->pc = vsyscall_syscall_end_pc - SYSENTER_LENGTH;
        } else {
            LOG(THREAD_GET, LOG_INTERP | LOG_SYNCH, 2,
                "recreate_app: no PC translation needed (at vsyscall)\n");
        }
#    if defined(MACOS) && defined(X86)
        if (!just_pc) {
            LOG(THREAD_GET, LOG_INTERP | LOG_SYNCH, 2,
                "recreate_app: restoring xdx (at sysenter)\n");
            mcontext->xdx = tdcontext->app_xdx;
        }
#    endif
        if (!just_pc)
            restore_stolen_register(tdcontext, mcontext);
        if (dr_xl8_hook_exists()) {
            if (!instrument_restore_nonfcache_state_prealloc(tdcontext, restore_memory,
                                                             mcontext, &xl8_mcontext))
                return RECREATE_FAILURE;
        }
        return res;
    }
#endif
    else if (is_after_syscall_that_rets(tdcontext, mcontext->pc)
             /* Check for pointing right at sysenter, for i#1145 */
             IF_UNIX(||
                     is_after_syscall_that_rets(tdcontext, mcontext->pc + INT_LENGTH))) {
        /* suspended inside kernel at syscall
         * all registers have app values for syscall */
        LOG(THREAD_GET, LOG_INTERP | LOG_SYNCH, 2,
            "recreate_app pc = after_syscall, translating\n");
#ifdef WINDOWS
        if (DYNAMO_OPTION(sygate_int) && get_syscall_method() == SYSCALL_METHOD_INT) {
            if ((app_pc)mcontext->xsp == NULL)
                return RECREATE_FAILURE;
            /* dr system calls will have the same after_syscall address when
             * sygate hack are in effect so need to check top of stack to see
             * if we are returning to dr or do/share syscall (generated
             * routines) */
            if (!in_generated_routine(tdcontext, *(app_pc *)mcontext->xsp)) {
                /* this must be a dynamo system call! */
                LOG(THREAD_GET, LOG_INTERP | LOG_SYNCH, 2,
                    "recreate_app at dynamo system call\n");
                return RECREATE_FAILURE;
            }
            ASSERT(*(app_pc *)mcontext->xsp == after_do_syscall_code(tdcontext) ||
                   *(app_pc *)mcontext->xsp == after_shared_syscall_code(tdcontext));
            if (!just_pc) {
                /* This is an int system call and since for sygate
                 * compatibility we redirect those with a call to an ntdll.dll
                 * int 2e ret 0 we need to pop the stack once to match app. */
                mcontext->xsp += XSP_SZ; /* pop the stack */
            }
        }
#else
        if (is_after_syscall_that_rets(tdcontext, mcontext->pc + INT_LENGTH)) {
            /* i#1145: preserve syscall re-start point */
            mcontext->pc = POST_SYSCALL_PC(tdcontext) - INT_LENGTH;
        } else
#endif
        mcontext->pc = POST_SYSCALL_PC(tdcontext);
        if (!just_pc)
            restore_stolen_register(tdcontext, mcontext);
        if (dr_xl8_hook_exists()) {
            if (!instrument_restore_nonfcache_state_prealloc(tdcontext, restore_memory,
                                                             mcontext, &xl8_mcontext))
                return RECREATE_FAILURE;
        }
        return res;
    } else if (mcontext->pc == get_reset_exit_stub(tdcontext)) {
        LOG(THREAD_GET, LOG_INTERP | LOG_SYNCH, 2,
            "recreate_app at reset exit stub => using next_tag " PFX "\n",
            tdcontext->next_tag);
        /* Context is completely native except the pc and the stolen register. */
        mcontext->pc = tdcontext->next_tag;
        if (!just_pc)
            restore_stolen_register(tdcontext, mcontext);
        if (dr_xl8_hook_exists()) {
            if (!instrument_restore_nonfcache_state_prealloc(tdcontext, restore_memory,
                                                             mcontext, &xl8_mcontext))
                return RECREATE_FAILURE;
        }
        return res;
    } else if (in_generated_routine(tdcontext, mcontext->pc)) {
        LOG(THREAD_GET, LOG_INTERP | LOG_SYNCH, 2,
            "recreate_app state at untranslatable address in "
            "generated routines for thread " TIDFMT "\n",
            tdcontext->owning_thread);
        return RECREATE_FAILURE;
    } else if (in_fcache(mcontext->pc)) {
        /* FIXME: what if pc is in separate direct stub???
         * do we have to read the &l from the stub to find linkstub_t and thus
         * fragment_t owner?
         */
        /* NOTE - only at this point is it safe to grab locks other then the
         * fcache_unit_areas.lock */
        linkstub_t *l;
        cache_pc cti_pc;
        instrlist_t *ilist = NULL;
        fragment_t *f = owning_f;
        bool alloc = false;
        dr_isa_mode_t old_mode;
#ifdef WINDOWS
        bool swap_peb = false;
#endif
        dr_restore_state_info_t client_info;
#ifdef WINDOWS
        /* i#889: restore private PEB/TEB for faithful recreation */
        /* i#1832: swap_peb_pointer() calls is_dynamo_address() in debug build, which
         * acquires dynamo_areas->lock and global_alloc_lock, but this is limited to
         * in_fcache() and thus we should have no deadlock problems on thread synch.
         */
        if (os_using_app_state(tdcontext)) {
            swap_peb_pointer(tdcontext, true /*to priv*/);
            swap_peb = true;
        }
#endif

        /* Rather than storing a mapping table, we re-build the fragment
         * containing the code cache pc whenever we can.  For pending-deletion
         * fragments we can't do that and have to store the info, due to our
         * weak consistency flushing where the app code may have changed
         * before we get here (case 3559).
         */

        /* Check whether we have a fragment w/ stored translations before
         * asking to recreate the ilist
         */
        if (f == NULL)
            f = fragment_pclookup_with_linkstubs(tdcontext, mcontext->pc, &alloc);

        /* If the passed-in fragment is fake, we need to get the linkstubs */
        if (f != NULL && TEST(FRAG_FAKE, f->flags)) {
            ASSERT(TEST(FRAG_COARSE_GRAIN, f->flags));
            f = fragment_recreate_with_linkstubs(tdcontext, f);
            alloc = true;
        }

        /* Whether a bb or trace, this routine will recreate the entire ilist. */
        if (f == NULL) {
            ilist = recreate_fragment_ilist(tdcontext, mcontext->pc, &f, &alloc,
                                            true /*mangle*/, true /*client*/);
        } else if (FRAGMENT_TRANSLATION_INFO(f) == NULL) {
            if (TEST(FRAG_SELFMOD_SANDBOXED, f->flags)) {
                ilist = recreate_selfmod_ilist(tdcontext, f);
            } else {
                /* NULL for pc indicates that f is valid */
                bool new_alloc;
                DEBUG_DECLARE(fragment_t *pre_f = f;)
                ilist = recreate_fragment_ilist(tdcontext, NULL, &f, &new_alloc,
                                                true /*mangle*/, true /*client*/);
                ASSERT(owning_f == NULL || f == owning_f ||
                       (TEST(FRAG_COARSE_GRAIN, owning_f->flags) && f == pre_f));
                ASSERT(!new_alloc);
            }
        }
        if (ilist == NULL && (f == NULL || FRAGMENT_TRANSLATION_INFO(f) == NULL)) {
            /* It is problematic if this routine fails.  Many places assume that
             * recreate_app_pc() will work.
             */
            ASSERT(!INTERNAL_OPTION(safe_translate_flushed));
            res = RECREATE_FAILURE;
            goto recreate_app_state_done;
        }

        LOG(THREAD_GET, LOG_INTERP, 2, "recreate_app : pc is in F%d(" PFX ")%s\n", f->id,
            f->tag, ((f->flags & FRAG_IS_TRACE) != 0) ? " (trace)" : "");

        DOLOG(2, LOG_SYNCH, {
            if (ilist != NULL) {
                LOG(THREAD_GET, LOG_SYNCH, 2, "ilist for recreation:\n");
                instrlist_disassemble(tdcontext, f->tag, ilist, THREAD_GET);
            }
        });

        /* if pc is in an exit stub, we find the corresponding exit instr */
        cti_pc = NULL;
        for (l = FRAGMENT_EXIT_STUBS(f); l; l = LINKSTUB_NEXT_EXIT(l)) {
            if (EXIT_HAS_LOCAL_STUB(l->flags, f->flags)) {
                /* FIXME: as computing the stub pc becomes more expensive,
                 * should perhaps check fragment_body_end_pc() or something
                 * that only does one stub check up front, and then find the
                 * exact stub if pc is beyond the end of the body.
                 */
                if (mcontext->pc < EXIT_STUB_PC(tdcontext, f, l))
                    break;
                cti_pc = EXIT_CTI_PC(f, l);
            }
        }
        if (cti_pc != NULL) {
            /* target is inside an exit stub!
             * new target: the exit cti, not its stub
             */
            if (!just_pc) {
                /* FIXME : translate from exit stub */
                LOG(THREAD_GET, LOG_INTERP | LOG_SYNCH, 2,
                    "recreate_app_helper -- can't full recreate state, pc " PFX " "
                    "is in exit stub\n",
                    mcontext->pc);
                res = RECREATE_SUCCESS_PC; /* failed on full state, but pc good */
                goto recreate_app_state_done;
            }
            LOG(THREAD_GET, LOG_INTERP | LOG_SYNCH, 2,
                "\ttarget " PFX " is inside an exit stub, looking for its cti "
                " " PFX "\n",
                mcontext->pc, cti_pc);
            mcontext->pc = cti_pc;
        }

        /* Recreate in same mode as original fragment */
        DEBUG_DECLARE(bool ok =)
        dr_set_isa_mode(tdcontext, FRAG_ISA_MODE(f->flags), &old_mode);
        ASSERT(ok);

        /* now recreate the state */
        /* keep a copy of the pre-translation state */
        priv_mcontext_to_dr_mcontext(&raw_mcontext, mcontext);
        client_info.raw_mcontext = &raw_mcontext;
        client_info.raw_mcontext_valid = true;
        if (ilist == NULL) {
            ASSERT(f != NULL && FRAGMENT_TRANSLATION_INFO(f) != NULL);
            ASSERT(!TEST(FRAG_WAS_DELETED, f->flags) ||
                   INTERNAL_OPTION(safe_translate_flushed));
            res = recreate_app_state_from_info(
                tdcontext, FRAGMENT_TRANSLATION_INFO(f), (byte *)f->start_pc,
                (byte *)f->start_pc + f->size, mcontext, just_pc _IF_DEBUG(f->flags));
            STATS_INC(recreate_via_stored_info);
        } else {
            res = recreate_app_state_from_ilist(
                tdcontext, ilist, (byte *)f->tag, (byte *)FCACHE_ENTRY_PC(f),
                (byte *)f->start_pc + f->size, mcontext, just_pc, f->flags);
            STATS_INC(recreate_via_app_ilist);
        }
        DEBUG_DECLARE(ok =) dr_set_isa_mode(tdcontext, old_mode, NULL);
        ASSERT(ok);

        if (!just_pc)
            restore_stolen_register(tdcontext, mcontext);
        if (res != RECREATE_FAILURE) {
            /* PR 214962: if the client has a restore callback, invoke it to
             * fix up the state (and pc).
             */
            priv_mcontext_to_dr_mcontext(&xl8_mcontext, mcontext);
            client_info.mcontext = &xl8_mcontext;
            client_info.fragment_info.tag = (void *)f->tag;
            client_info.fragment_info.cache_start_pc = FCACHE_ENTRY_PC(f);
            client_info.fragment_info.is_trace = TEST(FRAG_IS_TRACE, f->flags);
            client_info.fragment_info.app_code_consistent =
                !TESTANY(FRAG_WAS_DELETED | FRAG_SELFMOD_SANDBOXED, f->flags);
            client_info.fragment_info.ilist = ilist;
            /* i#220/PR 480565: client has option of failing the translation */
            if (!instrument_restore_state(tdcontext, restore_memory, &client_info))
                res = RECREATE_FAILURE;
            dr_mcontext_to_priv_mcontext(mcontext, &xl8_mcontext);
        }

    recreate_app_state_done:
        /* free the instrlist_t elements */
        if (ilist != NULL)
            instrlist_clear_and_destroy(tdcontext, ilist);
        if (alloc) {
            ASSERT(f != NULL);
            fragment_free(tdcontext, f);
        }
#ifdef WINDOWS
        if (swap_peb)
            swap_peb_pointer(tdcontext, false /*to app*/);
#endif
        return res;
    } else {
        /* handle any other cases, in DR etc. */
        return RECREATE_FAILURE;
    }
}

/* Assumes that pc is a pc_recreatable place (i.e. in_fcache(), though could do
 * syscalls with esp, also see FIXME about separate stubs in
 * recreate_app_state_internal()), ASSERTs otherwise.
 * If caller knows which fragment pc belongs to, caller should pass in f
 * to avoid work and avoid lock rank issues as pclookup acquires shared_cache_lock;
 * else, pass in NULL for f.
 * NOTE - If called by a thread other than the tdcontext owner, caller must
 * ensure tdcontext remains valid.  Caller also must ensure that it is safe to
 * allocate memory from tdcontext (for instr routines), i.e. caller owns
 * tdcontext or the owner of tdcontext is suspended.  Also if tdcontext is
 * !couldbelinking then caller must own the thread_initexit_lock in case
 * recreate_fragment_ilist() is called.
 * NOTE - If this function is unable to translate the pc, but the pc is
 * in_fcache() then there is an assert curiosity and the function returns NULL.
 * This can happen only from the pc being in a fragment that is pending
 * deletion (ref case 3559 others).  Most callers don't check the returned
 * value and wouldn't have a way to recover even if they did. FIXME
 */
/* Use THREAD_GET instead of THREAD so log messages go to calling thread */
app_pc
recreate_app_pc(dcontext_t *tdcontext, cache_pc pc, fragment_t *f)
{
    priv_mcontext_t mc;
    recreate_success_t res;

    LOG(THREAD_GET, LOG_INTERP, 2, "recreate_app_pc -- translating from pc=" PFX "\n",
        pc);

    memset(&mc, 0, sizeof(mc)); /* ensures esp is NULL */
    mc.pc = pc;

    res = recreate_app_state_internal(tdcontext, &mc, true, f, false);
    if (res != RECREATE_SUCCESS_PC) {
        ASSERT(res != RECREATE_SUCCESS_STATE); /* shouldn't return that for just_pc */
        ASSERT(in_fcache(pc));                 /* Make sure caller didn't screw up */
        /* We were unable to translate the pc, most likely because the
         * pc is in a fragment that is pending deletion FIXME, most callers
         * aren't able to recover! */
        ASSERT_CURIOSITY(res && "Unable to translate pc");
        mc.pc = NULL;
    }

    LOG(THREAD_GET, LOG_INTERP, 2, "recreate_app_pc -- translation is " PFX "\n", mc.pc);

    return mc.pc;
}

/* Translates the code cache state in mcontext into what it would look like
 * in the original application.
 * If it fails altogether, returns RECREATE_FAILURE, but still provides
 * a best-effort translation.
 * If it fails to restore the full machine state, but does restore the pc,
 * returns RECREATE_SUCCESS_PC.
 * If it successfully restores the full machine state,
 * returns RECREATE_SUCCESS_STATE.  Only for full success does it
 * consider the restore_memory parameter, which, if true, requests restoration
 * of any memory values that were shifted (primarily due to clients) (otherwise,
 * only the passed-in mcontext is modified). If restore_memory is
 * true, the caller should always relocate the translated thread, as
 * it may not execute properly if left at its current location (it
 * could be in the middle of client code in the cache).
 *
 * If caller knows which fragment pc belongs to, caller should pass in f
 * to avoid work and avoid lock rank issues as pclookup acquires shared_cache_lock;
 * else, pass in NULL for f.
 *
 * FIXME: does not undo stack mangling for sysenter
 */
/* NOTE - Can be called with a thread suspended at an arbitrary place by synch
 * routines so must not call mutex_lock (or call a function that does) unless
 * the synch routines have checked that lock.  Currently only fcache_unit_areas.lock
 * is used (for in_fcache in recreate_app_state_internal())
 * (if in_fcache succeeds then assumes other locks won't be a problem).
 * NOTE - If called by a thread other than the tdcontext owner, caller must
 * ensure tdcontext remains valid.  Caller also must ensure that it is safe to
 * allocate memory from tdcontext (for instr routines), i.e. caller owns
 * tdcontext or the owner of tdcontext is suspended.  Also if tdcontext is
 * !couldbelinking then caller must own the thread_initexit_lock in case
 * recreate_fragment_ilist() is called.  We assume that when tdcontext is
 * not the calling thread, this is a thread synch request, and is NOT from
 * an app fault (PR 267260)!
 */
/* Use THREAD_GET instead of THREAD so log messages go to calling thread */
recreate_success_t
recreate_app_state(dcontext_t *tdcontext, priv_mcontext_t *mcontext, bool restore_memory,
                   fragment_t *f)
{
    recreate_success_t res;

#ifdef DEBUG
    if (d_r_stats->loglevel >= 2 && (d_r_stats->logmask & LOG_SYNCH) != 0) {
        LOG(THREAD_GET, LOG_SYNCH, 2, "recreate_app_state -- translating from:\n");
        dump_mcontext(mcontext, THREAD_GET, DUMP_NOT_XML);
    }
#endif

    res = recreate_app_state_internal(tdcontext, mcontext, false, f, restore_memory);

#ifdef DEBUG
    if (res) {
        if (d_r_stats->loglevel >= 2 && (d_r_stats->logmask & LOG_SYNCH) != 0) {
            LOG(THREAD_GET, LOG_SYNCH, 2, "recreate_app_state -- translation is:\n");
            dump_mcontext(mcontext, THREAD_GET, DUMP_NOT_XML);
        }
    } else {
        LOG(THREAD_GET, LOG_SYNCH, 2, "recreate_app_state -- unable to translate\n");
    }
#endif

    return res;
}

static inline uint
translation_info_alloc_size(uint num_entries)
{
    return (sizeof(translation_info_t) + sizeof(translation_entry_t) * num_entries);
}

/* we save space by inlining the array with the struct holding the length */
static translation_info_t *
translation_info_alloc(dcontext_t *dcontext, uint num_entries)
{
    /* we need to use global heap since pending-delete fragments become
     * shared entities
     */
    translation_info_t *info =
        global_heap_alloc(translation_info_alloc_size(num_entries) HEAPACCT(ACCT_OTHER));
    info->num_entries = num_entries;
    return info;
}

void
translation_info_free(dcontext_t *dcontext, translation_info_t *info)
{
    global_heap_free(info,
                     translation_info_alloc_size(info->num_entries) HEAPACCT(ACCT_OTHER));
}

static inline void
set_translation(dcontext_t *dcontext, translation_entry_t **entries, uint *num_entries,
                uint entry, ushort cache_offs, app_pc app, bool identical,
                bool our_mangling, bool in_clean_call)
{
    if (entry >= *num_entries) {
        /* alloc new arrays 2x as big */
        *entries = global_heap_realloc(*entries, *num_entries, *num_entries * 2,
                                       sizeof(translation_entry_t) HEAPACCT(ACCT_OTHER));
        *num_entries *= 2;
    }
    ASSERT(entry < *num_entries);
    (*entries)[entry].cache_offs = cache_offs;
    (*entries)[entry].app = app;
    (*entries)[entry].flags = 0;
    if (identical)
        (*entries)[entry].flags |= TRANSLATE_IDENTICAL;
    if (our_mangling)
        (*entries)[entry].flags |= TRANSLATE_OUR_MANGLING;
    if (in_clean_call)
        (*entries)[entry].flags |= TRANSLATE_CLEAN_CALL;
    LOG(THREAD, LOG_FRAGMENT, 4, "\tset_translation: %d +%5d => " PFX " %s%s%s\n", entry,
        cache_offs, app, identical ? "identical" : "contiguous",
        our_mangling ? " ours" : "", in_clean_call ? " call" : "");
}

void
translation_info_print(const translation_info_t *info, cache_pc start, file_t file)
{
    uint i;
    ASSERT(info != NULL);
    ASSERT(file != INVALID_FILE);
    print_file(file, "translation info " PFX "\n", info);
    for (i = 0; i < info->num_entries; i++) {
        print_file(file, "\t%d +%5d == " PFX " => " PFX " %s%s%s\n", i,
                   info->translation[i].cache_offs,
                   start + info->translation[i].cache_offs, info->translation[i].app,
                   TEST(TRANSLATE_IDENTICAL, info->translation[i].flags) ? "identical"
                                                                         : "contiguous",
                   TEST(TRANSLATE_OUR_MANGLING, info->translation[i].flags) ? " ours"
                                                                            : "",
                   TEST(TRANSLATE_CLEAN_CALL, info->translation[i].flags) ? " call" : "");
    }
}

/* With our weak flushing consistency we must store translation info
 * for any fragment that may outlive its original app code (case
 * 3559).  Here we store actual translation info.  An alternative is
 * to store elided jmp information and a copy of the source memory,
 * but that takes more memory for all but the smallest fragments.  A
 * better alternative is to reliably de-mangle, which would require
 * only elided jmp information.
 */
translation_info_t *
record_translation_info(dcontext_t *dcontext, fragment_t *f, instrlist_t *existing_ilist)
{
    translation_entry_t *entries;
    uint num_entries;
    translation_info_t *info = NULL;
    instrlist_t *ilist;
    instr_t *inst;
    uint i;
    uint last_len = 0;
    bool last_contig;
    app_pc last_translation = NULL;
    cache_pc cpc;

    LOG(THREAD, LOG_FRAGMENT, 3, "record_translation_info: F%d(" PFX ")." PFX "\n", f->id,
        f->tag, f->start_pc);

    if (existing_ilist != NULL)
        ilist = existing_ilist;
    else if (TEST(FRAG_SELFMOD_SANDBOXED, f->flags)) {
        ilist = recreate_selfmod_ilist(dcontext, f);
    } else {
        /* Must re-build fragment and record translation info for each instr.
         * Whether a bb or trace, this routine will recreate the entire ilist.
         */
        ilist = recreate_fragment_ilist(dcontext, NULL, &f, NULL, true /*mangle*/,
                                        true /*client*/);
    }
    ASSERT(ilist != NULL);
    DOLOG(3, LOG_FRAGMENT, {
        LOG(THREAD, LOG_FRAGMENT, 3, "ilist for recreation:\n");
        instrlist_disassemble(dcontext, f->tag, ilist, THREAD);
    });

    /* To avoid two passes we do one pass and store into a large-enough array.
     * We then copy the results into a just-right-sized array.  A typical bb
     * requires 2 entries, one for its body of straight-line code and one for
     * the inserted jmp at the end, so we start w/ that to avoid copying in
     * the common case.  FIXME: optimization: instead of every bb requiring a
     * final entry for the inserted jmp, have recreate_ know about it and cut
     * in half the typical storage reqts.
     */
#define NUM_INITIAL_TRANSLATIONS 2
    num_entries = NUM_INITIAL_TRANSLATIONS;
    entries = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, translation_entry_t,
                               NUM_INITIAL_TRANSLATIONS, ACCT_OTHER, PROTECTED);

    i = 0;
    cpc = (byte *)FCACHE_ENTRY_PC(f);
    if (fragment_prefix_size(f->flags) > 0) {
        ASSERT(f->start_pc < cpc);
        set_translation(dcontext, &entries, &num_entries, i, 0, f->tag,
                        true /*identical*/, true /*our mangling*/, false /*!call*/);
        last_translation = f->tag;
        last_contig = false;
        i++;
    } else {
        ASSERT(f->start_pc == cpc);
        last_contig = true; /* we create 1st entry on 1st loop iter */
    }
    bool in_clean_call = false;
    for (inst = instrlist_first(ilist); inst; inst = instr_get_next(inst)) {
        app_pc app = instr_get_translation(inst);
        uint prev_i = i;
        /* Should only be NULL for meta-code added by a client.
         * We preserve the NULL so our translation routines know to not
         * let this be a thread relocation point
         */
        if (instr_is_label(inst)) {
            if (instr_get_note(inst) == (void *)DR_NOTE_CALL_SEQUENCE_START)
                in_clean_call = true;
            else if (instr_get_note(inst) == (void *)DR_NOTE_CALL_SEQUENCE_END)
                in_clean_call = false;
            /* i#739, skip label instr */
            continue;
        }
        /* PR 302951: clean call args are instr_is_our_mangling so no assert for that */
        ASSERT(app != NULL || instr_is_meta(inst));
        /* see whether we need a new entry, or the current stride (contig
         * or identical) holds
         */
        if (last_contig) {
            if ((i == 0 && (app == NULL || instr_is_our_mangling(inst))) ||
                app == last_translation) {
                /* we are now in an identical region */
                /* Our incremental discovery can cause us to add a new
                 * entry of one type that on the next instr we discover
                 * can optimally be recorded as the other type.  Here we
                 * hit an app pc shift whose target needs an identical
                 * entry: so rather than a contig followed by identical,
                 * we can get away with a single identical.
                 * Example: "x x+1 y y", where we use an identical for the
                 * first y instead of the contig that we initially guessed
                 * at b/c we assumed it was an elision.
                 */
                if (i > 0 && entries[i - 1].cache_offs == cpc - last_len - f->start_pc) {
                    /* convert prev contig into identical */
                    ASSERT(!TEST(TRANSLATE_IDENTICAL, entries[i - 1].flags));
                    entries[i - 1].flags |= TRANSLATE_IDENTICAL;
                    LOG(THREAD, LOG_FRAGMENT, 3, "\tchanging %d to identical\n", i - 1);
                } else {
                    set_translation(dcontext, &entries, &num_entries, i,
                                    (ushort)(cpc - f->start_pc), app, true /*identical*/,
                                    instr_is_our_mangling(inst), in_clean_call);
                    i++;
                }
                last_contig = false;
            } else if ((i == 0 && app != NULL && !instr_is_our_mangling(inst)) ||
                       app != last_translation + last_len) {
                /* either 1st loop iter w/ app instr & no prefix, or else probably
                 * a follow-ubr, so create a new contig entry
                 */
                set_translation(dcontext, &entries, &num_entries, i,
                                (ushort)(cpc - f->start_pc), app, false /*contig*/,
                                instr_is_our_mangling(inst), in_clean_call);
                last_contig = true;
                i++;
            } /* else, contig continues */
        } else {
            if (app != last_translation) {
                /* no longer in an identical region */
                ASSERT(i > 0);
                /* If we have translations "x x+1 x+1 x+2 x+3" we can more
                 * efficiently encode with a new contig entry at the 2nd
                 * x+1 rather than an identical entry there followed by a
                 * contig entry for x+2.
                 */
                if (app == last_translation + last_len &&
                    entries[i - 1].cache_offs == cpc - last_len - f->start_pc) {
                    /* convert prev identical into contig */
                    ASSERT(TEST(TRANSLATE_IDENTICAL, entries[i - 1].flags));
                    entries[i - 1].flags &= ~TRANSLATE_IDENTICAL;
                    LOG(THREAD, LOG_FRAGMENT, 3, "\tchanging %d to contig\n", i - 1);
                } else {
                    /* probably a follow-ubr, so create a new contig entry */
                    set_translation(dcontext, &entries, &num_entries, i,
                                    (ushort)(cpc - f->start_pc), app, false /*contig*/,
                                    instr_is_our_mangling(inst), in_clean_call);
                    last_contig = true;
                    i++;
                }
            }
        }
        last_translation = app;

        /* We need to make a new entry if the flags changed. */
        if (i > 0 && i == prev_i &&
            (!BOOLS_MATCH(instr_is_our_mangling(inst),
                          TEST(TRANSLATE_OUR_MANGLING, entries[i - 1].flags)) ||
             !BOOLS_MATCH(in_clean_call,
                          TEST(TRANSLATE_CLEAN_CALL, entries[i - 1].flags)))) {
            /* our manglings are usually identical */
            bool identical = instr_is_our_mangling(inst);
            set_translation(dcontext, &entries, &num_entries, i,
                            (ushort)(cpc - f->start_pc), app, identical,
                            instr_is_our_mangling(inst), in_clean_call);
            last_contig = !identical;
            i++;
        }
        last_len = instr_length(dcontext, inst);
        cpc += last_len;
        ASSERT(CHECK_TRUNCATE_TYPE_ushort(cpc - f->start_pc));
    }
    /* exit stubs can be examined after app code is gone, so we don't need
     * to store any info on them here
     */

    /* free the instrlist_t elements */
    if (existing_ilist == NULL)
        instrlist_clear_and_destroy(dcontext, ilist);

    /* now copy into right-sized array */
    info = translation_info_alloc(dcontext, i);
    memcpy(info->translation, entries, sizeof(translation_entry_t) * i);
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, entries, translation_entry_t, num_entries,
                    ACCT_OTHER, PROTECTED);

    STATS_INC(translations_computed);

    DOLOG(3, LOG_INTERP, { translation_info_print(info, f->start_pc, THREAD); });

    return info;
}

#ifdef INTERNAL
void
stress_test_recreate_state(dcontext_t *dcontext, fragment_t *f, instrlist_t *ilist)
{
    priv_mcontext_t mc;
    bool res;
    cache_pc cpc;
    instr_t *in;
    static const reg_t STRESS_XSP_INIT = 0x08000000; /* arbitrary */
    bool success_so_far = true;
    bool inside_mangle_region = false;
    IF_X86(bool inside_mangle_epilogue = false;)
    uint spill_ibreg_outstanding_offs = UINT_MAX;
    reg_id_t reg;
    bool spill;
    int xsp_adjust = 0;
    app_pc mangle_translation = f->tag;

    LOG(THREAD, LOG_INTERP, 3, "Testing restoring state fragment #%d\n",
        GLOBAL_STAT(num_fragments));

    if (TEST(FRAG_IS_TRACE, f->flags)) {
        /* decode_fragment() does not set the our-mangling bits, nor the
         * translation fields (to distinguish back-to-back mangling
         * regions): not ideal to test using part of what we're testing but
         * better than nothing
         */
        ilist = recreate_fragment_ilist(dcontext, NULL, &f, NULL, true /*mangle*/,
                                        true /*call client*/);
    }

    cpc = FCACHE_ENTRY_PC(f);
    for (in = instrlist_first(ilist); in != NULL;
         cpc += instr_length(dcontext, in), in = instr_get_next(in)) {
        /* PR 267260: we're only testing mangling regions. */
        if (inside_mangle_region &&
            (!instr_is_our_mangling(in) ||
             /* handle adjacent mangle regions */
             IF_X86((inside_mangle_epilogue && !instr_is_our_mangling_epilogue(in)) ||)(
                 TEST(FRAG_IS_TRACE, f->flags) /* we have translation only for traces */
                 && mangle_translation !=
                     instr_get_translation(in)
                         IF_X86(&&!(!inside_mangle_epilogue &&
                                    instr_is_our_mangling_epilogue(in)))))) {
            /* reset */
            LOG(THREAD, LOG_INTERP, 3, "  out of mangling region\n");
            inside_mangle_region = false;
            IF_X86(inside_mangle_epilogue = false;)
            xsp_adjust = 0;
            success_so_far = true;
            spill_ibreg_outstanding_offs = UINT_MAX;
            /* go ahead and fall through and ensure we succeed w/ 0 xsp adjust */
        }

        if (instr_is_our_mangling(in)) {
            if (!inside_mangle_region) {
                inside_mangle_region = true;
                LOG(THREAD, LOG_INTERP, 3, "  entering mangling region\n");
                mangle_translation = instr_get_translation(in);
            } else if (IF_X86_ELSE(!inside_mangle_epilogue &&
                                       instr_is_our_mangling_epilogue(in),
                                   false)) {
                LOG(THREAD, LOG_INTERP, 3, "  entering mangling epilogue\n");
                IF_X86(inside_mangle_epilogue = true;)
            } else {
                ASSERT(!TEST(FRAG_IS_TRACE, f->flags) ||
                       IF_X86(instr_is_our_mangling_epilogue(in) ||)
                               mangle_translation == instr_get_translation(in));
            }

            if (spill_ibreg_outstanding_offs != UINT_MAX) {
                mc.MC_IBL_REG = (reg_t)d_r_get_tls(spill_ibreg_outstanding_offs) + 1;
            } else {
                mc.MC_IBL_REG =
                    (reg_t)d_r_get_tls(os_tls_offset((ushort)IBL_TARGET_SLOT)) + 1;
            }
            mc.xsp = STRESS_XSP_INIT;
            mc.pc = cpc;
            DOLOG(3, LOG_INTERP, {
                LOG(THREAD, LOG_INTERP, 3, "instruction: ");
                instr_disassemble(dcontext, in, THREAD);
                LOG(THREAD, LOG_INTERP, 3, "\n");
            });
            LOG(THREAD, LOG_INTERP, 3, "  restoring cpc=" PFX ", xsp=" PFX "\n", mc.pc,
                mc.xsp);
            res = recreate_app_state(dcontext, &mc, false /*just registers*/, NULL);
            LOG(THREAD, LOG_INTERP, 3,
                "  restored res=%d pc=" PFX ", xsp=" PFX " vs " PFX ", ibreg=" PFX
                " vs " PFX "\n",
                res, mc.pc, mc.xsp, STRESS_XSP_INIT - /*negate*/ xsp_adjust,
                mc.MC_IBL_REG, d_r_get_tls(os_tls_offset((ushort)IBL_TARGET_SLOT)));
            /* We should only have failures at tail end of mangle regions.
             * No instrs after a failing instr should touch app memory.
             */
            ASSERT(success_so_far /* ok to fail */ ||
                   (!res &&
                    (instr_is_DR_reg_spill_or_restore(dcontext, in, NULL, NULL, NULL,
                                                      NULL) ||
                     (!instr_reads_memory(in) && !instr_writes_memory(in)))));

            /* check that xsp and ibreg are adjusted properly */
            ASSERT(mc.xsp == STRESS_XSP_INIT - /*negate*/ xsp_adjust);
            ASSERT(spill_ibreg_outstanding_offs == UINT_MAX ||
                   mc.MC_IBL_REG == (reg_t)d_r_get_tls(spill_ibreg_outstanding_offs));

            if (success_so_far && !res)
                success_so_far = false;
            instr_check_xsp_mangling(dcontext, in, &xsp_adjust);
            if (xsp_adjust != 0)
                LOG(THREAD, LOG_INTERP, 3, "  xsp_adjust=%d\n", xsp_adjust);
            uint offs = UINT_MAX;
            if (instr_is_DR_reg_spill_or_restore(dcontext, in, NULL, &spill, &reg,
                                                 &offs) &&
                reg == IBL_TARGET_REG) {
                if (spill)
                    spill_ibreg_outstanding_offs = offs;
                else
                    spill_ibreg_outstanding_offs = UINT_MAX;
            }
        }
    }
    if (TEST(FRAG_IS_TRACE, f->flags)) {
        instrlist_clear_and_destroy(dcontext, ilist);
    }
}
#endif /* INTERNAL */

/* END TRANSLATION CODE ***********************************************************/
