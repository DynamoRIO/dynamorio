/* **********************************************************
 * Copyright (c) 2013-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2002-2010 VMware, Inc.  All rights reserved.
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
/* Copyright (c) 2002-2003 Massachusetts Institute of Technology */

/* optimize.c
 *
 * routines to support optimization of traces
 * (old offline optimization stuff is in mangle.c)
 */

#include "../globals.h" /* just to disable warning C4206 about an empty file */

#ifdef INTERNAL /* around whole file */

#    include "../globals.h"
#    include "arch.h"
#    include "instr.h"
#    include "instr_create_shared.h"
#    include "instrlist.h"
#    include "decode.h"
#    include "decode_fast.h"
/* XXX i#1551: eliminate PREFIX_{DATA,ADDR} refs and then remove this include */
#    include "x86/decode_private.h"
#    include "../fragment.h"
#    include "disassemble.h"
#    include "proc.h"

/* IMPORTANT INSTRUCTIONS FOR WRITING OPTIMIZATIONS:
 *
 * 1) can assume that all instructions are fully decoded -- that is,
 *    instr_operands_valid(instr) will return true
 * 2) optimizations MUST BE DETERMINISTIC!  they are re-executed to
 *    reconstruct the Pc (and in the future the rest of the machine state,
 *    hopefully) on exceptions/signals
 */

/****************************************************************************/
/* forward declarations */
/* if these are useful elsewhere, un-static them */

/* optimizations */
static void
instr_counts(dcontext_t *dcontext, app_pc tag, instrlist_t *trace, bool pre);
static void
constant_propagation(dcontext_t *dcontext, app_pc tag, instrlist_t *trace);
static void
call_return_matching(dcontext_t *dcontext, app_pc tag, instrlist_t *trace);
static void
remove_unnecessary_zeroing(dcontext_t *dcontext, app_pc tag, instrlist_t *trace);
static void
stack_adjust_combiner(dcontext_t *dcontext, app_pc tag, instrlist_t *trace);

static void
prefetch_optimize_trace(dcontext_t *dcontext, app_pc tag, instrlist_t *trace);
void
remove_redundant_loads(dcontext_t *dcontext, app_pc tag, instrlist_t *trace);

static void
peephole_optimize(dcontext_t *dcontext, app_pc tag, instrlist_t *trace);

static void
identify_for_loop(dcontext_t *dcontext, app_pc tag, instrlist_t *trace);
static void
unroll_loops(dcontext_t *dcontext, app_pc tag, instrlist_t *trace);

/* utility routines */
bool
is_store_to_ecxoff(dcontext_t *dcontext, instr_t *inst);
bool
is_load_from_ecxoff(dcontext_t *dcontext, instr_t *inst);
bool
opnd_is_constant_address(opnd_t address);
static bool
is_zeroing_instr(instr_t *inst);
static bool
replace_inc_with_add(dcontext_t *dcontext, instr_t *inst, instrlist_t *trace);
static bool
safe_write(instr_t *mem_writer);
static bool
instruction_affects_mem_access(instr_t *instr, opnd_t mem_access);
static bool
is_dead_register(reg_id_t reg, instr_t *where);
static reg_id_t
find_dead_register_across_instrs(instr_t *start, instr_t *end);
static bool
is_nop(instr_t *inst);
void
remove_inst(dcontext_t *dcontext, instrlist_t *ilist, instr_t *inst);

/* for using a 24 entry bool array to represent some property about
 * about normal registers and sub register (eax -> dl) */
/* propagates the value into all sub registers, doesn't propagate up
 * into enclosing registers, index value is checked for bounds */
static void
propagate_down(bool *reg_rep, int index, bool value);
/* checks the 24 entry array and returns true if it and all sub
 * registers of the index are true and 0 <= index < 24 */
static bool
check_down(bool *reg_rep, int index);

/* given a cbr, finds the previous instr that writes the flag the cbr reads */
static instr_t *
get_decision_instr(instr_t *jmp);

/* exported utility routines */
void
d_r_loginst(dcontext_t *dcontext, uint level, instr_t *instr, const char *string);
void
d_r_logopnd(dcontext_t *dcontext, uint level, opnd_t opnd, const char *string);
void
d_r_logtrace(dcontext_t *dcontext, uint level, instrlist_t *trace, const char *string);

/****************************************************************************/
/* main routine */

void
optimize_trace(dcontext_t *dcontext, app_pc tag, instrlist_t *trace)
{
    /* we have un-truncation-check 32-bit casts for opnd_get_immed_int(), for
     * one thing, here and in loadtoconst.c */
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));

    /* FIXME: this routine is of course not in its final form
     * we are still playing with different optimizations
     */

    /* all opts want to expand all bundles and many want cti info including instr_t
     * targets, so we go ahead and do that up front
     */
    instrlist_decode_cti(dcontext, trace);

#    ifdef DEBUG
    LOG(THREAD, LOG_OPTS, 3, "\noptimize_trace ******************\n");
    LOG(THREAD, LOG_OPTS, 3, "\nbefore optimization:\n");

    if (d_r_stats->loglevel >= 3 && (d_r_stats->logmask & LOG_OPTS) != 0)
        instrlist_disassemble(dcontext, tag, trace, THREAD);

#    endif

    if (dynamo_options.instr_counts) {
        instr_counts(dcontext, tag, trace, true);
    }

    if (dynamo_options.call_return_matching) {
        call_return_matching(dcontext, tag, trace);
    }

    if (dynamo_options.unroll_loops) {
        unroll_loops(dcontext, tag, trace);
    }

    if (dynamo_options.vectorize) {
        identify_for_loop(dcontext, tag, trace);
    }

    if (dynamo_options.prefetch) {
        prefetch_optimize_trace(dcontext, tag, trace);
    }

    if (dynamo_options.rlr) {
        remove_redundant_loads(dcontext, tag, trace);
    }

    if (dynamo_options.remove_unnecessary_zeroing) {
        remove_unnecessary_zeroing(dcontext, tag, trace);
    }

    if (dynamo_options.constant_prop) {
        constant_propagation(dcontext, tag, trace);
    }

    if (dynamo_options.remove_dead_code) {
        remove_dead_code(dcontext, tag, trace);
    }

    if (dynamo_options.stack_adjust) {
        stack_adjust_combiner(dcontext, tag, trace);
    }

    if (dynamo_options.peephole) {
        peephole_optimize(dcontext, tag, trace);
    }

    if (dynamo_options.instr_counts) {
        instr_counts(dcontext, tag, trace, false);
    }

#    ifdef DEBUG
    LOG(THREAD, LOG_OPTS, 3, "\nafter optimization:\n");
    if (d_r_stats->loglevel >= 3 && (d_r_stats->logmask & LOG_OPTS) != 0)
        instrlist_disassemble(dcontext, tag, trace, THREAD);
#    endif
}

#    ifdef DEBUG
static struct {
    /* rlr */
    uint loads_removed_from_loads;
    uint loads_removed_from_stores;
    uint ctis_in_load_removal;
    int reg_overwritten;
    int val_saved_in_dead_reg;
    uint loads_examined;
    /* inc->add */
    int incs_examined;
    int incs_replaced;
    /* unrolling */
    int loops_unrolled;
    // spill_xmm
    int vals_spilled_to_xmm;
    int loads_replaced_by_xmm;
    int xmm_saves_to_mem;
    int stores_replaced_by_xmm;
    int xmm_restored_from_memory;
    int xmm_mov_to_dead_reg;
    int loadstore_combos_replaced_by_xmm;
    int xmm_traces;
    int mmx_traces;
    /* constant propagation */
    int num_instrs_simplified;
    int num_fail_simplified;
    int num_opnds_simplified;
    int num_const_add_const_mem;
    int num_jmps_simplified;
    int num_jecxz_instrs_removed;
    /* remove dead loads */
    int dead_loads_removed;
    /* remove unnecssary XOR zeroing */
    int xors_removed;
    /* stack adjustment combining */
    int num_stack_adjust_removed;
    /* instr_counts */
    int pre_num_instrs_seen;
    int pre_num_jmps_seen;
    int post_num_instrs_seen;
    int post_num_jmps_seen;
    /* call return matching */
    int num_returns_removed;
    int num_return_instrs_removed;
} opt_stats_t;

/*this function is called when dynamo exits. prints stats for any optimization
  that wants to keep them in opt_stats_t and put appropriate code below*/

void
print_optimization_stats()
{
    if (dynamo_options.rlr) {
        uint top, bottom;
        LOG(GLOBAL, LOG_OPTS, 1, "%u loads examined for rlr\n",
            opt_stats_t.loads_examined);
        divide_uint64_print(opt_stats_t.loads_removed_from_stores +
                                opt_stats_t.loads_removed_from_loads,
                            opt_stats_t.loads_examined, true, 2, &top, &bottom);
        LOG(GLOBAL, LOG_OPTS, 1, "%u.%.2u%% of examined loads removed\n", top, bottom);
        divide_uint64_print(opt_stats_t.ctis_in_load_removal,
                            opt_stats_t.loads_removed_from_loads +
                                opt_stats_t.loads_removed_from_stores,
                            false, 4, &top, &bottom);
        LOG(GLOBAL, LOG_OPTS, 1,
            "%u loads removed from loads\n"
            "%u loads removed from stores\n"
            "%u ctis traversed.  %u.%.4u ctis / load\n",
            opt_stats_t.loads_removed_from_loads, opt_stats_t.loads_removed_from_stores,
            opt_stats_t.ctis_in_load_removal, top, bottom);
        LOG(GLOBAL, LOG_OPTS, 1, "%d rlr's had problems because a reg. was overwritten\n",
            opt_stats_t.reg_overwritten);
        LOG(GLOBAL, LOG_OPTS, 1,
            "%d rlr's were saved by using a dead register to save value\n",
            opt_stats_t.val_saved_in_dead_reg);
    }
    if (dynamo_options.peephole && proc_get_family() == FAMILY_PENTIUM_4) {
        LOG(GLOBAL, LOG_OPTS, 1, "%d inc/dec examined, %d replaced with add/sub\n",
            opt_stats_t.incs_examined, opt_stats_t.incs_replaced);
    }
    if (dynamo_options.unroll_loops) {
        LOG(GLOBAL, LOG_OPTS, 1, "%d loops unrolled\n", opt_stats_t.loops_unrolled);
    }

    if (dynamo_options.call_return_matching) {
        LOG(GLOBAL, LOG_OPTS, 1, "Call Return Matching - stats\n");
        LOG(GLOBAL, LOG_OPTS, 1, "   %d returns removed\n",
            opt_stats_t.num_returns_removed);
        LOG(GLOBAL, LOG_OPTS, 1, "   %d return instrs removed\n",
            opt_stats_t.num_return_instrs_removed);
    }

    if (dynamo_options.constant_prop) {
        LOG(GLOBAL, LOG_OPTS, 1, "Constant Prop - stats\n");
        LOG(GLOBAL, LOG_OPTS, 1, "   %d operands simplified\n",
            opt_stats_t.num_opnds_simplified);
        LOG(GLOBAL, LOG_OPTS, 1,
            "   %d constant loads from immutable memory discoverd (included in operands "
            "simplified)\n",
            opt_stats_t.num_const_add_const_mem);
        LOG(GLOBAL, LOG_OPTS, 1, "   %d instructions simplified\n",
            opt_stats_t.num_instrs_simplified);
        LOG(GLOBAL, LOG_OPTS, 1, "   %d instructions failed simplification\n",
            opt_stats_t.num_fail_simplified);
        LOG(GLOBAL, LOG_OPTS, 1, "   %d jmps removed\n", opt_stats_t.num_jmps_simplified);
        LOG(GLOBAL, LOG_OPTS, 1,
            "   %d jecxz related instrs removed, (6 per jecxz instr)\n",
            opt_stats_t.num_jecxz_instrs_removed);
    }

    if (dynamo_options.remove_unnecessary_zeroing) {
        LOG(GLOBAL, LOG_OPTS, 1, "%d unnecessary zeroing instances removed\n",
            opt_stats_t.xors_removed);
    }

    if (dynamo_options.stack_adjust) {
        LOG(GLOBAL, LOG_OPTS, 1, "Stack Adjustment Combiner - stats\n");
        LOG(GLOBAL, LOG_OPTS, 1, "   %d stack adjustments removed\n",
            opt_stats_t.num_stack_adjust_removed);
    }

    if (dynamo_options.remove_dead_code) {
        LOG(GLOBAL, LOG_OPTS, 1, "Dead Code Elimination - stats\n");
        LOG(GLOBAL, LOG_OPTS, 1, "   %d dead instructions removed\n",
            opt_stats_t.dead_loads_removed);
    }

    if (dynamo_options.instr_counts) {
        LOG(GLOBAL, LOG_OPTS, 1, "Prior to optimizations\n");
        LOG(GLOBAL, LOG_OPTS, 1, "     %d instrs in traces\n",
            opt_stats_t.pre_num_instrs_seen);
        LOG(GLOBAL, LOG_OPTS, 1, "     %d jmps (cbr) in traces\n",
            opt_stats_t.pre_num_jmps_seen);
        LOG(GLOBAL, LOG_OPTS, 1, "After optimizations\n");
        LOG(GLOBAL, LOG_OPTS, 1, "     %d instrs in traces\n",
            opt_stats_t.post_num_instrs_seen);
        LOG(GLOBAL, LOG_OPTS, 1, "     %d jmps (cbr) in traces\n",
            opt_stats_t.post_num_jmps_seen);
    }
}
#    endif

/****************************************************************************/

/* op1 and op2 are both memory references */
static bool
generate_antialias_check(dcontext_t *dcontext, instrlist_t *pre_loop, opnd_t op1,
                         opnd_t op2)
{
    /* basic idea: "lea op1 !overlap lea op2" */
    opnd_t lea1, lea2;
    if (opnd_same(op1, op2))
        return false; /* guaranteed alias */
    if (!opnd_defines_use(op1, op2))
        return true; /* guaranteed non-alias */
    /* FIXME: get pre-loop values of registers */
    /* FIXME: get unroll factor -- pass to opnd_defines_use too */
    /* assumption: ebx and ecx are saved at start of pre_loop, restored at end
     * FIXME: op1/op2 may use ebx/ecx!
     */
    lea1 = op1;
    opnd_set_size(&lea1, OPSZ_lea);
    lea2 = op2;
    opnd_set_size(&lea2, OPSZ_lea);
    instrlist_append(pre_loop,
                     INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_EBX), lea1));
    instrlist_append(pre_loop,
                     INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_ECX), lea2));
    instrlist_append(
        pre_loop,
        INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_EBX), opnd_create_reg(REG_ECX)));
    return true;
}

#    define MAX_INDUCTION_VARS 8
#    define MAX_LCDS 32
#    define MAX_INVARIANTS 32

static void
identify_for_loop(dcontext_t *dcontext, app_pc tag, instrlist_t *trace)
{
    instr_t *inst, *branch, *check;
    opnd_t opnd;
    int i, j, k;
    instr_t *induction_var[MAX_INDUCTION_VARS];
    int num_induction_vars = 0;
    opnd_t lcd[MAX_LCDS];
    int num_lcds = 0;
    opnd_t invariant[MAX_INVARIANTS];
    int num_invariants = 0;
    instrlist_t pre_loop;
    instrlist_init(&pre_loop);

    /* FIXME: what about loops with cbr at top and ubr at bottom? */

    /* FIXME: for now, we only look for single-basic-block traces */

    LOG(THREAD, LOG_OPTS, 3, "identify_for_loop: examining trace with tag " PFX "\n",
        tag);
    /* first, make sure we end with a conditional branch (followed by uncond.
     * for fall-through)
     */
    inst = instrlist_last(trace);
    if (!instr_is_ubr(inst))
        return;
    branch = instr_get_prev(inst);
    if (!instr_is_cbr(branch))
        return;
    /* now look for self-loop */
    if (opnd_get_pc(instr_get_target(branch)) != tag)
        return;

#    ifdef DEBUG
    LOG(THREAD, LOG_OPTS, 1,
        "\nidentify_for_loop: found whole-trace self-loop: tag " PFX "\n", tag);
    if ((d_r_stats->logmask & LOG_OPTS) != 0)
        instrlist_disassemble(dcontext, tag, trace, THREAD);
#    endif

    /* FIXME: for each pair of read/write and write/write: insert pre-loop check to
     * ensure no aliases
     */

    /* make a pass looking for lcds and induction variables
     *   only look at scalars -- ignore memory references, we deal with them
     *   separately later
     * also make sure there's only one exit
     */
    for (inst = instrlist_first(trace); inst != branch; inst = instr_get_next(inst)) {
        /* for now do not allow exits in middle */
        if (instr_is_exit_cti(inst)) {
            LOG(THREAD, LOG_OPTS, 1, "internal exit found, giving up\n");
            return;
        }
        /* loop-carried dependence: a read with no writes prior in loop but
         *   with a write following in loop
         * loop invariant: a read with no writes anywhere in loop
         * FIXME: better to store dependence info somehow, or to make passes
         * through instrlist whenever need info?
         */
        d_r_loginst(dcontext, 1, inst, "considering");
        for (i = 0; i < instr_num_srcs(inst); i++) {
            opnd = instr_get_src(inst, i);
            /* ignore immeds and memory references */
            if (opnd_is_immed(opnd) || opnd_is_memory_reference(opnd))
                continue;
            for (check = instrlist_first(trace); check != inst;
                 check = instr_get_next(check)) {
                for (j = 0; j < instr_num_dsts(check); j++) {
                    if (opnd_defines_use(instr_get_dst(check, j), opnd)) {
                        /* write prior to read: no lcd */
                        d_r_loginst(dcontext, 1, check,
                                    "\twrite prior to read -> no lcd");
                        goto no_lcd;
                    }
                }
            }
            for (check = inst; check != branch; check = instr_get_next(check)) {
                for (j = 0; j < instr_num_dsts(check); j++) {
                    if (opnd_defines_use(instr_get_dst(check, j), opnd)) {
                        /* write following read: lcd */
                        goto has_lcd;
                    }
                }
            }
            /* no writes: loop invariant! */
            d_r_logopnd(dcontext, 1, opnd, "\tloop invariant");
            invariant[num_invariants] = opnd;
            num_invariants++;
            if (num_invariants >= MAX_INVARIANTS) {
                LOG(THREAD, LOG_OPTS, 1, "too many invariants, giving up\n");
                return;
            }
        }
        d_r_loginst(dcontext, 1, inst, "\tfell off end -> no lcd");
    no_lcd:
        continue;
    has_lcd:
        d_r_loginst(dcontext, 1, inst, "\tfound a loop-carried dependence");
        /* find basic induction variables: i = i + constant
         * FIXME: consider loop invariants as well as immeds as constants
         * FIXME: only consider inc,dec,add,sub?
         * FIXME: look for dependent induction vars too: j = i + constant
         */
        /* assumption: immediate operands are always 1st source */
        if (instr_get_opcode(inst) == OP_inc || instr_get_opcode(inst) == OP_dec ||
            (instr_num_srcs(inst) == 2 && instr_num_dsts(inst) == 1 &&
             opnd_is_immed_int(instr_get_src(inst, 0)) &&
             opnd_same(instr_get_src(inst, 1), instr_get_dst(inst, 0)))) {
            d_r_loginst(dcontext, 1, inst, "\t\tfound induction variable");
            induction_var[num_induction_vars] = inst;
            num_induction_vars++;
            if (num_induction_vars >= MAX_INDUCTION_VARS) {
                LOG(THREAD, LOG_OPTS, 1, "too many induction vars, giving up\n");
                return;
            }
        } else {
            /* not an induction variable, but may be ok if lcd operand
             * is based on induction var values
             */
            lcd[num_lcds] = opnd;
            num_lcds++;
            if (num_lcds >= MAX_LCDS) {
                LOG(THREAD, LOG_OPTS, 1, "too many lcds, giving up\n");
                return;
            }
        }
    }

    LOG(THREAD, LOG_OPTS, 1, "now looking through lcds for ones we can't handle\n");
    /* it's ok for an lcd to read a value kept in an induction var
     * or a loop invariant
     */
    for (i = 0; i < num_lcds; i++) {
        if (opnd_is_reg(lcd[i])) {
            for (j = 0; j < num_induction_vars; j++) {
                if (opnd_same(lcd[i], instr_get_dst(induction_var[j], 0)))
                    goto next_lcd;
            }
            for (j = 0; j < num_invariants; j++) {
                if (opnd_same(lcd[i], invariant[j]))
                    goto next_lcd;
            }
        } else if (opnd_is_memory_reference(lcd[i])) {
            for (j = 0; j < opnd_num_regs_used(lcd[i]); j++) {
                opnd = opnd_create_reg(opnd_get_reg_used(lcd[i], j));
                for (k = 0; k < num_induction_vars; k++) {
                    if (opnd_same(opnd, instr_get_dst(induction_var[k], 0)))
                        break;
                }
                if (k == num_induction_vars) {
                    for (k = 0; k < num_invariants; k++) {
                        if (opnd_same(opnd, invariant[k]))
                            break;
                    }
                    if (k == num_invariants)
                        goto lcd_bad;
                }
            }
        } else
            ASSERT_NOT_REACHED();
    next_lcd:
        d_r_logopnd(dcontext, 1, lcd[i], "\tlcd read is induction var value, so it's ok");
        continue;
    lcd_bad:
        d_r_logopnd(dcontext, 1, lcd[i],
                    "\tlcd read is not induction var value, giving up");
        return;
    }

    /* now look at loop termination test */
    inst = get_decision_instr(branch);
    d_r_loginst(dcontext, 1, inst, "looking at decision instr");
    /* test must involve only induction vars and constants */
    for (i = 0; i < instr_num_srcs(inst); i++) {
        opnd = instr_get_src(inst, i);
        if (!opnd_is_immed(opnd)) {
            for (j = 0; j < num_induction_vars; j++) {
                if (opnd_same(opnd, instr_get_dst(induction_var[j], 0)))
                    break;
            }
            if (j == num_induction_vars) {
                d_r_loginst(dcontext, 1, inst,
                            "\tloop termination test not just consts & inductions!");
                return;
            }
        }
    }

    LOG(THREAD, LOG_OPTS, 1, "now looking at memory references\n");
    for (inst = instrlist_first(trace); inst != branch; inst = instr_get_next(inst)) {
        /* for each store, generate pre-loop checks to ensure no overlap with
         * any other store or read
         */
        d_r_loginst(dcontext, 1, inst, "considering");
        for (i = 0; i < instr_num_dsts(inst); i++) {
            opnd = instr_get_dst(inst, i);
            if (!opnd_is_memory_reference(opnd))
                continue;
            for (check = instrlist_first(trace); check != branch;
                 check = instr_get_next(check)) {
                for (j = 0; j < instr_num_dsts(check); j++) {
                    if (check == inst && j == i)
                        continue;
                    if (opnd_is_memory_reference(instr_get_dst(check, j))) {
                        /* disambiguate these writes */
                        d_r_logopnd(dcontext, 1, instr_get_dst(check, j),
                                    "\tgenerating checks");
                        if (!generate_antialias_check(dcontext, &pre_loop, opnd,
                                                      instr_get_dst(check, j))) {
                            d_r_loginst(dcontext, 1, inst,
                                        "unavoidable alias, giving up");
                            return;
                        }
                    }
                }
            }
        }
    }
    if (instrlist_first(&pre_loop) != NULL) {
        /* if we generated any tests, they assume we have two registers:
         * save two registers at start, then restore at end, of pre_loop
         * FIXME: what about eflags?
         */
        instrlist_prepend(&pre_loop,
                          instr_create_save_to_dcontext(dcontext, REG_EBX, XBX_OFFSET));
        instrlist_prepend(&pre_loop,
                          instr_create_save_to_dcontext(dcontext, REG_ECX, XCX_OFFSET));
        instrlist_append(
            &pre_loop, instr_create_restore_from_dcontext(dcontext, REG_ECX, XCX_OFFSET));
        instrlist_append(
            &pre_loop, instr_create_restore_from_dcontext(dcontext, REG_EBX, XBX_OFFSET));
    }

    LOG(THREAD, LOG_OPTS, 1, "loop has passed all tests so far!\n");
#    ifdef DEBUG
    LOG(THREAD, LOG_OPTS, 1, "pre-loop checks are:\n");
    if (d_r_stats->loglevel >= 1 && (d_r_stats->logmask & LOG_OPTS) != 0)
        instrlist_disassemble(dcontext, tag, &pre_loop, THREAD);
#    endif

    /* now look for "load, arithop, store" pattern
     * THIS IS A HACK -- just want to identify loop in mmx.c
     */
    inst = instrlist_first(trace);
    if (instr_get_opcode(inst) != OP_mov_ld) {
        LOG(THREAD, LOG_OPTS, 1, "1st instr not a load, aborting\n");
        return;
    }
    inst = instr_get_next(inst);
    if (instr_get_opcode(inst) != OP_add) {
        LOG(THREAD, LOG_OPTS, 1, "2nd instr not an add, aborting\n");
        return;
    }
    inst = instr_get_next(inst);
    if (instr_get_opcode(inst) != OP_mov_st) {
        LOG(THREAD, LOG_OPTS, 1, "3rd instr not a store, aborting\n");
        return;
    }
    LOG(THREAD, LOG_OPTS, 1, "found 'load, arithop, store' pattern!\n");
    for (check = instr_get_next(inst); check != branch; check = instr_get_next(check)) {
        for (j = 0; j < num_induction_vars; j++) {
            if (induction_var[j] == check)
                break;
        }
        if (j == num_induction_vars) {
            d_r_loginst(dcontext, 1, check, "non-induction var is present");
            return;
        }
    }
    LOG(THREAD, LOG_OPTS, 1, "vectorizing\n");

    /* prior to unrolling, replace inc with add */
    for (i = 0; i < num_induction_vars; i++) {
        int opcode = instr_get_opcode(induction_var[i]);
        if (opcode == OP_inc || opcode == OP_dec) {
            inst = instr_get_prev(induction_var[i]);
            if (replace_inc_with_add(dcontext, induction_var[i], trace)) {
                /* orig induction var instr_t was destroyed, get new copy */
                if (inst == NULL)
                    induction_var[i] = instrlist_first(trace);
                else
                    induction_var[i] = instr_get_next(inst);
            } else {
                d_r_loginst(dcontext, 1, induction_var[i],
                            "couldn't replace inc w/ add b/c of eflags\n");
                /* FIXME: undo earlier inc->adds */
                return;
            }
        }
    }

    /********** unroll loop **********/
#    if 0
    do {
        body using i;
        i += inc;
    } while (i < max);

becomes

/*
pre-loop to get alignment is tricky when replacing sideline...need to
measure memory addresses, can't just go on induction var...for
now do not have a pre-loop and do unaligned simd
          offs = i % inc;
          while (i % (inc * unrollfactor) != 0) {
              body using i;
              i += inc;
          }
*/
        do {
            body using i;
            body using i+inc;
            i += (inc * unrollfactor);
        } while (i < max - (inc * unrollfactor) + 1);
    while (i < max) {
        body using i;
        i += inc;
    }
#    endif
#    if 0
    /* First do pre-alignment loop
     * to do mod: idiv divides edx:eax by r/m32, puts quotient
     *     in eax, remainder in edx
     *   cdq sign-extends eax into edx:eax
     *   so x mod y is:
     *     mov x,%eax
     *     cdq
     *     idiv y
     * answer is now in %edx!  */
    inst = instrlist_first(trace);
    instrlist_preinsert(trace, inst,
                        instr_create_save_to_dcontext(dcontext, REG_EAX, XAX_OFFSET));
    instrlist_preinsert(trace, inst,
                        instr_create_save_to_dcontext(dcontext, REG_EDX, XDX_OFFSET));
    /* FIXME: get termination index var & lower/upper bound
     * hardcoding to mmx.c loop for now
     */
    instrlist_preinsert(trace, inst,
                        INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_EAX), opnd_create_reg(REG_ECX)));
    instrlist_preinsert(trace, inst, INSTR_CREATE_cdq(dcontext));
    /* can't pass immed to idiv so have to grab register */
    instrlist_preinsert(trace, inst,
                        instr_create_save_to_dcontext(dcontext, REG_EBX, XBX_OFFSET));
    instrlist_preinsert(trace, inst,
                        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_EBX),
                                             OPND_CREATE_INT32(unroll_factor)));
    instrlist_preinsert(trace, inst,
                        INSTR_CREATE_idiv_v(dcontext, opnd_create_reg(REG_EBX)));
    instrlist_preinsert(trace, inst,
                        instr_create_restore_from_dcontext(dcontext, REG_EBX, XBX_OFFSET));
    /* mod is now in edx */
    /* FIXME:
     *   test edx, edx
     *   restore eax and edx
     *   jz unrolled_loop
     *   <body of loop>
     *   jmp top of pre loop
     * unrolled_loop:
     *   <body of loop> X unroll factor except:
     *   test: "< bound" => "< bound - unrollfactor + 1"
     *   ivar: "ivar += k" => "ivar += unrollfactor * k"
     */
    instrlist_preinsert(trace, inst,
                        instr_create_restore_from_dcontext(dcontext, REG_EDX, XDX_OFFSET));
    instrlist_preinsert(trace, inst,
                        instr_create_restore_from_dcontext(dcontext, REG_EAX, XAX_OFFSET));
#    endif

    /* HACK: focus on mmx.c sample loop */
    inst = instrlist_first(trace);
    opnd = instr_get_src(inst, 0);
    ASSERT(opnd_is_memory_reference(opnd));
    opnd_set_size(&opnd, OPSZ_8);
    check = INSTR_CREATE_movq(dcontext, opnd_create_reg(REG_MM0), opnd);
    d_r_loginst(dcontext, 1, inst, "replacing this");
    d_r_loginst(dcontext, 1, check, "\twith this");
    replace_inst(dcontext, trace, inst, check);

    inst = instr_get_next(check);
    opnd = instr_get_src(inst, 0);
    ASSERT(opnd_is_memory_reference(opnd));
    opnd_set_size(&opnd, OPSZ_8);
    check = INSTR_CREATE_paddd(dcontext, opnd_create_reg(REG_MM0), opnd);
    d_r_loginst(dcontext, 1, inst, "replacing this");
    d_r_loginst(dcontext, 1, check, "\twith this");
    replace_inst(dcontext, trace, inst, check);

    inst = instr_get_next(check);
    opnd = instr_get_dst(inst, 0);
    ASSERT(opnd_is_memory_reference(opnd));
    opnd_set_size(&opnd, OPSZ_8);
    check = INSTR_CREATE_movq(dcontext, opnd, opnd_create_reg(REG_MM0));
    d_r_loginst(dcontext, 1, inst, "replacing this");
    d_r_loginst(dcontext, 1, check, "\twith this");
    replace_inst(dcontext, trace, inst, check);

    /* now make induction vars do X unroll duty */
    for (i = 0; i < num_induction_vars; i++) {
        if (instr_get_opcode(induction_var[i]) == OP_inc ||
            instr_get_opcode(induction_var[i]) == OP_dec) {
            /* couldn't convert to add/sub, so duplicate */
            instrlist_preinsert(trace, induction_var[i],
                                instr_clone(dcontext, induction_var[i]));
        } else {
            opnd = instr_get_src(induction_var[i], 0);
            ASSERT(opnd_is_immed_int(opnd));
            opnd =
                opnd_create_immed_int(opnd_get_immed_int(opnd) * 2, opnd_get_size(opnd));
            instr_set_src(induction_var[i], 0, opnd);
        }
    }

#    ifdef DEBUG
    LOG(THREAD, LOG_OPTS, 1, "\nfinal version of trace:\n");
    if ((d_r_stats->logmask & LOG_OPTS) != 0)
        instrlist_disassemble(dcontext, tag, trace, THREAD);
#    endif
}

/****************************************************************************/

/* WARNING: this optimization inserts intra-trace loops that are not
 * considered exit cti's, so they cannot be unlinked/relinked, nor does
 * linkcount profiling work properly on them.
 * We need to figure out our official stance on support for this kind
 * of thing in optimized traces.
 */
static void
unroll_loops(dcontext_t *dcontext, app_pc tag, instrlist_t *trace)
{
    instr_t *inst, *temp;
    instr_t *branch, *decision, *cmp, *recovery_cmp, *final_jmp;
    int unroll_factor, cmp_vs, i;
    bool counting_up;
    opnd_t cmp_const;
    uint eflags_6 = 0;

    /* FIXME: what about loops with cbr at top and ubr at bottom? */

    LOG(THREAD, LOG_OPTS, 3, "unroll loop: examining trace with tag " PFX "\n", tag);
    /* first, make sure we end with a conditional branch (followed by uncond.
     * for fall-through)
     */
    final_jmp = instrlist_last(trace);
    if (!instr_is_ubr(final_jmp))
        return;
    branch = instr_get_prev(final_jmp);
    if (!instr_is_cbr(branch))
        return;
    /* now look for self-loop */
    if (opnd_get_pc(instr_get_target(branch)) != tag)
        return;

        /* eflags: for simplicity require that eflags be written before read
         * only need to look at arith flags
         */
#    ifdef DEBUG
    LOG(THREAD, LOG_OPTS, 3, "\nunroll loop -- checking eflags on:\n");
    if (d_r_stats->loglevel >= 3 && (d_r_stats->logmask & LOG_OPTS) != 0)
        instrlist_disassemble(dcontext, tag, trace, THREAD);
#    endif
    for (inst = instrlist_first(trace); inst != branch; inst = instr_get_next(inst)) {
        uint eflags = instr_get_arith_flags(inst, DR_QUERY_DEFAULT);
        if (eflags != 0) {
            if ((eflags & EFLAGS_READ_6) != 0) {
                if ((eflags_6 | (eflags & EFLAGS_READ_6)) != eflags_6) {
                    /* we're reading a flag that has not been written yet */
                    d_r_loginst(dcontext, 3, inst,
                                "reads flag before writing it, giving up");
                    return;
                }
            } else if ((eflags & EFLAGS_WRITE_6) != 0) {
                /* store the flags we're writing, but as read bits */
                eflags_6 |= EFLAGS_WRITE_TO_READ((eflags & EFLAGS_WRITE_6));
                /* check against read flags (we've shifted them): */
                if ((eflags_6 & EFLAGS_READ_6) == EFLAGS_READ_6) {
                    break; /* all written before read */
                }
            }
        }
    }
    /* if get here, eflags are written before read -- and we assume that
     * our cmp checks below will ensure that exiting the trace will not
     * have different eflags behavior than the unrolled loop
     * FIXME: I'm not certain of this
     */

#    ifdef DEBUG
    LOG(THREAD, LOG_OPTS, 3, "\nunroll loop: found whole-trace self-loop: tag " PFX "\n",
        tag);
    if (d_r_stats->loglevel >= 3 && (d_r_stats->logmask & LOG_OPTS) != 0)
        instrlist_disassemble(dcontext, tag, trace, THREAD);
#    endif

    /********** unroll loop **********/
    /*
      do {
      body using i;
      i += inc;
      } while (i < max);

      becomes (assuming no eflags problems):

      while (i < max - (inc * (unrollfactor-1))) {
      body using i;
      body using i+inc;
      body using i+(inc*2);
      ...
      body using i+(inc*(unrollfactor-1));
      i += (inc * unrollfactor);
      }
      while (i < max) {
      body using i;
      i += inc;
      }
    */

    /* FIXME: determine best unroll factor, somehow */
    unroll_factor = 2;

    /* see if we can get the branch into a state that can
     * have its bounds changed: "cmp var, immed"
     */
    decision = get_decision_instr(branch);
    if (decision == NULL) {
        LOG(THREAD, LOG_OPTS, 3, "can't find decision instr\n");
        return;
    }
    d_r_loginst(dcontext, 3, decision, "decision instr");

    if (instr_get_opcode(decision) == OP_cmp) {
        int opcode = instr_get_opcode(branch);
        switch (opcode) {
        case OP_jb: counting_up = true; break;
        case OP_jnb: counting_up = false; break;
        case OP_jbe: counting_up = true; break;
        case OP_jnbe: counting_up = false; break;
        case OP_jl: counting_up = true; break;
        case OP_jnl: counting_up = false; break;
        case OP_jle: counting_up = true; break;
        case OP_jnle: counting_up = false; break;
        case OP_js: counting_up = true; break;
        case OP_jns: counting_up = false; break;
        default:
            d_r_loginst(dcontext, 3, branch, "cannot handle decision branch");
            return;
        }
    } else if (instr_get_opcode(decision) == OP_inc ||
               instr_get_opcode(decision) == OP_add ||
               instr_get_opcode(decision) == OP_adc)
        counting_up = true;
    else if (instr_get_opcode(decision) == OP_dec || instr_get_opcode(decision) == OP_sub)
        counting_up = false;
    else {
        LOG(THREAD, LOG_OPTS, 3, "can't figure out direction of loop index var\n");
        return;
    }

    if (instr_get_opcode(decision) == OP_cmp) {
        cmp = decision;
    } else {
        /* common loop type: ends with "dec var, jns" */
        if (instr_get_opcode(decision) == OP_inc ||
            instr_get_opcode(decision) == OP_dec) {
            opnd_t cmp_var;
            int opcode;
            cmp_var = instr_get_dst(decision, 0);
            if (instr_get_opcode(branch) == OP_jns) {
                /* jump if non-negative */
                cmp_const = OPND_CREATE_INT8(0);
                opcode = OP_jge;
            } else if (instr_get_opcode(branch) == OP_js) {
                /* jump if negative */
                cmp_const = OPND_CREATE_INT8(0);
                opcode = OP_jl;
            } else {
                d_r_loginst(dcontext, 3, branch, "can't handle loop branch");
                return;
            }
            cmp = INSTR_CREATE_cmp(dcontext, cmp_var, cmp_const);
            instrlist_preinsert(trace, branch, cmp);
            temp = INSTR_CREATE_jcc(dcontext, opcode, instr_get_target(branch));
            replace_inst(dcontext, trace, branch, temp);
            branch = temp;

            /* replace with add/sub for easy stride changing
             * if we fail, give up, not because we can't inc twice, but
             * because of eflags concerns
             */
            if (!replace_inc_with_add(dcontext, decision, trace)) {
                d_r_loginst(dcontext, 3, decision, "couldn't replace with add/sub");
                return;
            }
        } else {
            /* give up -- if add cases in future, remember to deal w/ eflags */
            d_r_loginst(dcontext, 3, decision, "can't handle loop branch decision");
            return;
        }
    }
    /* FIXME: detect loop invariants, and allow them as constants
     * requires adding extra instructions to compute bounds
     */
    if (!opnd_is_immed_int(instr_get_src(cmp, 1))) {
        d_r_loginst(dcontext, 3, cmp, "cmp is not vs. constant");
        return;
    }

    /* make recovery loop */
    recovery_cmp = instr_clone(dcontext, cmp);
    instrlist_preinsert(trace, final_jmp, recovery_cmp);
    temp = instr_clone(dcontext, branch);
    instr_invert_cbr(temp);
    instr_set_target(temp, instr_get_target(final_jmp));
    instrlist_preinsert(trace, final_jmp, temp);
    for (inst = instrlist_first(trace); inst != cmp; inst = instr_get_next(inst)) {
        instrlist_preinsert(trace, final_jmp, instr_clone(dcontext, inst));
    }
    /* now change final jmp to loop to recovery loop check */
    instr_set_target(final_jmp, opnd_create_instr(recovery_cmp));

    /* unroll:
     * duplicate every instruction up to cmp at end
     */
    temp = instr_get_prev(cmp);
    for (i = 1; i < unroll_factor; i++) {
        for (inst = instrlist_first(trace); inst != cmp; inst = instr_get_next(inst)) {
            instrlist_preinsert(trace, cmp, instr_clone(dcontext, inst));
            if (inst == temp) /* avoid infinite loop */
                break;
        }
    }

    /* now switch unrolled loop from do-while to while */

    /* put jcc up front */
    temp = instr_clone(dcontext, branch);
    instr_invert_cbr(temp);
    instr_set_target(temp, opnd_create_instr(recovery_cmp));
    instrlist_prepend(trace, temp);

    /* now stick cmp in front of it */
    cmp_vs = (int)opnd_get_immed_int(instr_get_src(cmp, 1));
    if (counting_up)
        cmp_vs -= (unroll_factor - 1);
    else
        cmp_vs += (unroll_factor - 1);
    if (cmp_vs >= -128 && cmp_vs <= 127)
        cmp_const = OPND_CREATE_INT8(cmp_vs);
    else
        cmp_const = OPND_CREATE_INT32(cmp_vs);
    instrlist_prepend(trace,
                      INSTR_CREATE_cmp(dcontext, instr_get_src(cmp, 0), cmp_const));

    /* now change end-of-unrolled-loop jcc to be a jmp to top cmp */
    instr_set_opcode(branch, OP_jmp);
    instr_set_target(branch, opnd_create_instr(instrlist_first(trace)));

    /* remove end-of-unrolled-loop cmp */
    instrlist_remove(trace, cmp);
    instr_destroy(dcontext, cmp);

    /* control flow is all set, now combine induction var updates
     * FIXME: NOT DONE YET
     */

#    ifdef DEBUG
    opt_stats_t.loops_unrolled++;
    LOG(THREAD, LOG_OPTS, 3, "\nfinal version of unrolled trace:\n");
    if (d_r_stats->loglevel >= 3 && (d_r_stats->logmask & LOG_OPTS) != 0)
        instrlist_disassemble(dcontext, tag, trace, THREAD);
#    endif
}

/***************************************************************************/
/* from Tim */
/* a simple non-optimization that counts the number of instructions processed */
/* maby extend it later to gather more statistics, size distribution          */
/* op distribution, etc. if ever desired */

static void
instr_counts(dcontext_t *dcontext, app_pc tag, instrlist_t *trace, bool pre)
{
#    ifdef DEBUG
    instr_t *inst;
    int jmps = 0;
    int instrs = 0;
    for (inst = instrlist_first(trace); inst != NULL; inst = instr_get_next(inst)) {
        instrs++;
        if (instr_is_cbr(inst))
            jmps++;
    }
    if (pre) {
        LOG(THREAD, LOG_OPTS, 2,
            "Prior to optimization\n     %d instrs in trace\n     %d jmps exiting "
            "trace\n",
            instrs, jmps);
        opt_stats_t.pre_num_instrs_seen += instrs;
        opt_stats_t.pre_num_jmps_seen += jmps;
    } else {
        LOG(THREAD, LOG_OPTS, 2,
            "After optimization\n     %d instrs in trace\n     %d jmps exiting trace\n",
            instrs, jmps);
        opt_stats_t.post_num_instrs_seen += instrs;
        opt_stats_t.post_num_jmps_seen += jmps;
    }
#    endif
}

/***************************************************************************/
/* from Tim */
/* Constant Propagation */

/* utility structures */
#    define PS_VALID_VAL 0x01
#    define PS_LVALID_VAL 0x02 /* high and low parts, only used for regs */
#    define PS_HVALID_VAL 0x04
#    define PS_KEEP 0x80

#    define NUM_CONSTANT_ADDRESS 24
#    define NUM_STACK_SLOTS 24

static int cp_global_aggr;
static int cp_local_aggr;

/* holds the current state of the propagation */
typedef struct _prop_state_t {
    dcontext_t *dcontext;
    instrlist_t *trace;
    instr_t *hint;
    byte reg_state[8];
    int reg_vals[8];
    /* constant address */
    int addresses[NUM_CONSTANT_ADDRESS];
    int address_vals[NUM_CONSTANT_ADDRESS];
    byte address_state[NUM_CONSTANT_ADDRESS];

    /* stack */
    int cur_scope;

    int stack_offsets_ebp[NUM_STACK_SLOTS];
    int stack_vals[NUM_STACK_SLOTS];

    int stack_scope[NUM_STACK_SLOTS];
    byte stack_address_state[NUM_STACK_SLOTS];
    /* add esp offsets in the future ? */
    bool lost_scope_count;
} prop_state_t;

static void
set_stack_val(prop_state_t *state, int disp, int val, byte flags)
{
#    ifdef DEBUG
    dcontext_t *dcontext = state->dcontext;
#    endif
    bool cont = true;
    int i;
    for (i = 0; (i < NUM_STACK_SLOTS) && cont; i++) {
        if (state->stack_address_state[i] == 0) {
            state->stack_offsets_ebp[i] = disp;
            state->stack_vals[i] = val;
            state->stack_scope[i] = state->cur_scope;
            state->stack_address_state[i] = flags;
            cont = false;
        }
    }
    if (cont) {
        LOG(THREAD, LOG_OPTS, 3, "stack cache overflow\n");
        i = disp % NUM_STACK_SLOTS;
        ASSERT(i > 0 && i < NUM_STACK_SLOTS);
        state->stack_offsets_ebp[i] = disp;
        state->stack_vals[i] = val;
        state->stack_scope[i] = state->cur_scope;
        state->stack_address_state[i] = flags;
    }

    LOG(THREAD, LOG_OPTS, 3,
        " stack cache add: " PFX "  val " PFX "  scope depth %d flags 0x%02x\n", disp,
        val, state->cur_scope, flags);
}

static void
update_stack_val(prop_state_t *state, int disp, int val)
{
#    ifdef DEBUG
    dcontext_t *dcontext = state->dcontext;
#    endif
    bool cont = true;
    int i;
    for (i = 0; i < NUM_STACK_SLOTS && cont; i++) {
        if (state->stack_offsets_ebp[i] == disp &&
            state->stack_scope[i] == state->cur_scope) {
            state->stack_vals[i] = val;
            state->stack_address_state[i] |= PS_VALID_VAL;
            cont = false;
            LOG(THREAD, LOG_OPTS, 3,
                " stack cache update disp " PFX " to value " PFX " at stack depth %d\n",
                disp, val, state->cur_scope);
        }
    }
    if (cp_local_aggr > 2) {
        if (cont)
            set_stack_val(state, disp, val, PS_VALID_VAL);
    }
}

#    if 0 /* not used */
static bool
check_stack_val(prop_state_t *state, int disp, int val, bool valid)
{
    int i;
    for (i = 0; i < NUM_STACK_SLOTS; i++) {
        if (state->stack_offsets_ebp[i] == disp &&
            state->stack_scope[i] == state->cur_scope) {
            if ((state->stack_address_state[i] & PS_VALID_VAL) != 0) {
                if (valid)
                    ASSERT(state->stack_vals[i] == val);
                return false;
            }
            return valid;
        }
    }
    return true;
}
#    endif

static void
clear_stack_val(prop_state_t *state, int address)
{
#    ifdef DEBUG
    dcontext_t *dcontext = state->dcontext;
#    endif
    int i;
    bool cont = true;
    for (i = 0; i < NUM_STACK_SLOTS && cont; i++) {
        if (state->stack_offsets_ebp[i] == address &&
            state->stack_scope[i] == state->cur_scope) {
            state->stack_address_state[i] &= PS_KEEP;
            cont = false;
            LOG(THREAD, LOG_OPTS, 3,
                " load constant cache cleared: disp " PFX " stack depth %d \n", address,
                state->cur_scope);
        }
    }
}

/* adds an address value pair to the constant address cache */
static void
set_address_val(prop_state_t *state, int address, int val, byte flags)
{
#    ifdef DEBUG
    dcontext_t *dcontext = state->dcontext;
#    endif
    bool cont = true;
    int i;
    for (i = 0; (i < NUM_CONSTANT_ADDRESS) && cont; i++) {
        if (state->address_state[i] == 0) {
            state->addresses[i] = address;
            state->address_vals[i] = val;
            state->address_state[i] = flags;
            cont = false;
        }
    }
    if (cont) {
        LOG(THREAD, LOG_OPTS, 3, "constant address cache overflow\n");
        i = address % NUM_CONSTANT_ADDRESS;
        ASSERT(i > 0 && i < NUM_CONSTANT_ADDRESS);
        state->addresses[i] = address;
        state->address_vals[i] = val;
        state->address_state[i] = flags;
    }
    LOG(THREAD, LOG_OPTS, 3,
        " load const cache add: " PFX "  val " PFX "  flags 0x%02x\n", address, val,
        flags);
}

/* updates and address value pair in the constant address cache if the address is
 * already there, else adds it */
static void
update_address_val(prop_state_t *state, int address, int val)
{
#    ifdef DEBUG
    dcontext_t *dcontext = state->dcontext;
#    endif
    bool cont = true;
    int i;
    for (i = 0; i < NUM_CONSTANT_ADDRESS; i++) {
        if (state->addresses[i] == address) {
            state->address_vals[i] = val;
            state->address_state[i] |= PS_VALID_VAL;
            cont = false;
            LOG(THREAD, LOG_OPTS, 3, " load const cache update: " PFX "  val " PFX "\n",
                address, val);
        }
    }
    if (cp_global_aggr > 2) {
        if (cont)
            set_address_val(state, address, val, PS_VALID_VAL);
    }
}

/* removes the address from the constant address cache */
static void
clear_address_val(prop_state_t *state, int address)
{
#    ifdef DEBUG
    dcontext_t *dcontext = state->dcontext;
#    endif
    int i;
    for (i = 0; i < NUM_CONSTANT_ADDRESS; i++) {
        if (state->addresses[i] == address) {
            state->address_state[i] &= PS_KEEP;
            LOG(THREAD, LOG_OPTS, 3, " load constant cache cleared: " PFX "\n", address);
        }
    }
}

/* gets the value of a const address to an immutable region in memory
 * assumes that const_add_const_mem has already been called on this
 * and returned true
 */

static int
get_immutable_value(opnd_t address, prop_state_t *state, int size)
{
    int result;

    switch (size) {
    case OPSZ_1: {
        char *ptr_byte = ((char *)(ptr_int_t)opnd_get_disp(address));
        IF_X64(ASSERT_NOT_IMPLEMENTED(false));
        result = *ptr_byte;
        break;
    }
    case OPSZ_2: {
        short *ptr_byte = ((short *)(ptr_int_t)opnd_get_disp(address));
        IF_X64(ASSERT_NOT_IMPLEMENTED(false));
        result = *ptr_byte;
        break;
    }
    case OPSZ_4: {
        int *ptr_byte = ((int *)(ptr_int_t)opnd_get_disp(address));
        IF_X64(ASSERT_NOT_IMPLEMENTED(false));
        result = *ptr_byte;
        break;
    }
    default: {
        /* can't handle size, log is usually quadwords for floats */
#    ifdef DEBUG
        dcontext_t *dcontext = state->dcontext;
        d_r_logopnd(state->dcontext, 3, address,
                    "Couldn't handle size in get_immutable_value");
        LOG(THREAD, LOG_OPTS, 3, "Address size was %d\n", size);
#    endif
        /* should never get here, since const_address_const_mem should fail */
        result = 0;
        ASSERT_NOT_REACHED();
    }
    }

    return result;
}

/* returns true if the opnd is a stack  address (ebp)
 * i.e. is memory access with ebp as reg base and null as index reg */
static bool
opnd_is_stack_address(opnd_t address)
{
    return (opnd_is_near_base_disp(address) && (opnd_get_base(address) == REG_EBP) &&
            (opnd_get_index(address) == REG_NULL));
}

/* true if addresses (which must be a constant address) is an
 * immutable region of memory */
static bool
const_address_const_mem(opnd_t address, prop_state_t *state, bool prefix_data)
{
    bool success = false;
    int size = opnd_get_size(address);
    d_r_logopnd(state->dcontext, 3, address, " checking const address const mem\n");
    if (size == OPSZ_4_short2) {
        if (prefix_data)
            size = OPSZ_2;
        else
            size = OPSZ_4;
    }
    if (size != OPSZ_1 && size != OPSZ_2 && size != OPSZ_4) {
        /* can't handle size, is usually quadwords for floats */
#    ifdef DEBUG
        dcontext_t *dcontext = state->dcontext;
        d_r_logopnd(state->dcontext, 3, address,
                    "Couldn't handle size in const_address_const_mem");
        LOG(THREAD, LOG_OPTS, 3, "Address size was %d\n", size);
#    endif
        return false;
    }

    /* FIXME : is is_execuatable always right here? */
    /* i.e. is it going to be true, forever, that this location isn't writable */
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));
    if (cp_global_aggr > 1 &&
        is_executable_address((app_pc)(ptr_int_t)opnd_get_disp(address)))
        success = true;

    return success;
}

/* takes an opnd and returns a simplified version, simplifies address and
 * regs based on the information in propState
 */
static opnd_t
propagate_address(opnd_t old, prop_state_t *state)
{
    reg_id_t base_reg, index_reg, seg;
    int disp, scale;
    opnd_size_t size;
    bool modified;

    if (!opnd_is_memory_reference(old))
        return old;
    IF_X64(ASSERT_NOT_IMPLEMENTED(false)); /* rel and abs mem refs NYI */
    /* tries to simplify the address calculation with propagated values */
    base_reg = opnd_get_base(old) - REG_START_32;
    disp = opnd_get_disp(old);
    index_reg = opnd_get_index(old) - REG_START_32;
    scale = opnd_get_scale(old);
    seg = REG_NULL;
    size = opnd_get_size(old);
    modified = false;

    if (opnd_is_far_base_disp(old)) {
        seg = opnd_get_segment(old);
    }

    if (index_reg < 8 /* rules out underflow */ &&
        ((state->reg_state[index_reg] & PS_VALID_VAL) != 0)) {

        disp += (state->reg_vals[index_reg] * scale);
        index_reg = REG_NULL;
        modified = true;
    } else {
        index_reg += REG_START_32;
    }

    if (base_reg < 8 /* rules out underflow */ &&
        ((state->reg_state[base_reg] & PS_VALID_VAL) != 0)) {

        disp += state->reg_vals[base_reg];
        /* don't think this is necessary  *******FIXME*************
           if ((seg == REG_NULL) && ((base_reg + REG_START_32 == REG_ESP) ||
           (base_reg + REG_START_32 == REG_EBP))) {
           seg = SEG_SS;
           }
        */
        base_reg = REG_NULL;
        modified = true;
    } else {
        base_reg += REG_START_32;
    }

    if (!modified)
        return old;

    if (seg == REG_NULL) {
        /* return base disp */
        return opnd_create_base_disp(base_reg, index_reg, scale, disp, size);
    }

    /* return far base disp */
    return opnd_create_far_base_disp(seg, base_reg, index_reg, scale, disp, size);
}

/* attempts to simplify the opnd with propagated information */

static opnd_t
propagate_opnd(opnd_t old, prop_state_t *state, bool prefix_data)
{

    reg_id_t reg;
    int immed, disp, i;
    opnd_size_t size = OPSZ_NA;
#    ifdef DEBUG
    dcontext_t *dcontext = state->dcontext;
#    endif

    if (opnd_is_reg(old)) {
        reg = opnd_get_reg(old) - REG_START_32;
        if (reg < 8 /* rules out underflow */) {
            if ((state->reg_state[reg] & PS_VALID_VAL) != 0) {
                immed = state->reg_vals[reg];
                return opnd_create_immed_int(immed, OPSZ_4);
            } else
                return old;
        }
        reg = opnd_get_reg(old) - REG_START_16;
        if (reg < 8 /* rules out underflow */) {
            if ((state->reg_state[reg] & PS_VALID_VAL) != 0 ||
                ((state->reg_state[reg] & PS_LVALID_VAL) != 0 &&
                 (state->reg_state[reg] & PS_HVALID_VAL) != 0)) {
                /* mask out top part of register */
                immed = state->reg_vals[reg] & 0x0000ffff;
                /* sign extend if negative */
                if ((immed & 0x00008000) != 0)
                    immed |= 0xffff0000;
                return opnd_create_immed_int(immed, OPSZ_2);
            } else
                return old;
        }
        reg = opnd_get_reg(old) - REG_START_8;
        if (reg < 4 /* rules out underflow */ &&
            ((state->reg_state[reg] & PS_VALID_VAL) != 0 ||
             (state->reg_state[reg] & PS_LVALID_VAL) != 0)) {
            /* is low part */
            /* mask out top part of register */
            immed = state->reg_vals[reg] & 0x000000ff;
            /* sign extend if negative */
            if ((immed & 0x00000080) != 0)
                immed |= 0xffffff00;
            return opnd_create_immed_int(immed, OPSZ_1);
        }
        reg -= 4;
        if (reg < 4 /* rules out underflow */ &&
            ((state->reg_state[reg] & PS_VALID_VAL) != 0 ||
             (state->reg_state[reg] & PS_HVALID_VAL) != 0)) {
            /* is high part */
            /* mask out part of register */
            immed = state->reg_vals[reg] & 0x0000ff00;
            /* shift */
            immed = immed >> 8;
            /* sign extend if negative */
            if ((immed & 0x00000080) != 0)
                immed |= 0xffffff00;
            return opnd_create_immed_int(immed, OPSZ_1);
        }
        return old;
    }

    old = propagate_address(old, state);

    /* if address, get size */
    if (opnd_is_memory_reference(old)) {
        size = opnd_get_size(old);
        /* handle variable size */
        if (size == OPSZ_4_short2) {
            if (prefix_data)
                size = OPSZ_2;
            else
                size = OPSZ_4;
        }
    }

    if (opnd_is_stack_address(old) && cp_local_aggr > 0) {
        // check stack value
        disp = opnd_get_disp(old);
        for (i = 0; i < NUM_STACK_SLOTS; i++) {
            if (state->stack_offsets_ebp[i] == disp &&
                state->cur_scope == state->stack_scope[i] &&
                (state->stack_address_state[i] & PS_VALID_VAL) != 0) {
                LOG(THREAD, LOG_OPTS, 3, "  at stack depth %d ", state->cur_scope);
                d_r_logopnd(state->dcontext, 3, old, " found cached stack address");
                immed = state->stack_vals[i];
                return opnd_create_immed_int(immed, size);
            }
        }
    }

    if (opnd_is_constant_address(old) && cp_global_aggr > 0) {
        if (const_address_const_mem(old, state, prefix_data)) {
#    ifdef DEBUG
            d_r_logopnd(state->dcontext, 2, old, " found const address const mem\n");
            opt_stats_t.num_const_add_const_mem++;
#    endif
            immed = get_immutable_value(old, state, size);
            return opnd_create_immed_int(immed, size);
        } else {
            // check for constant address
            disp = opnd_get_disp(old);
            for (i = 0; i < NUM_CONSTANT_ADDRESS; i++) {
                if (state->addresses[i] == disp &&
                    (state->address_state[i] & PS_VALID_VAL) != 0) {
                    d_r_logopnd(state->dcontext, 3, old,
                                " found cached constant address\n");
                    immed = state->address_vals[i];
                    return opnd_create_immed_int(immed, size);
                }
            }
        }
    }
    return old;
}

/* checks an eflags and eflags_valid to see if a particular flag flag is valid
 * and has appropriate value */
static bool
check_flag_val(uint flag, int val, uint eflags, uint eflags_valid)
{
    if ((eflags_valid & flag) != 0) {
        if (((val == 1) && ((flag & eflags) != 0)) ||
            ((val == 0) && ((flag & eflags) == 0))) {
            return true;
        }
    }
    return false;
}

/* checks and eflags and an eflags_valid and checks to see that the two given flags
 * are both valid and set either same (if same is true) or different (if same is false) */
static bool
compare_flag_vals(uint flag1, uint flag2, bool same, uint eflags, uint eflags_valid)
{
    if (((eflags_valid & flag1) != 0) && ((eflags_valid & flag2) != 0)) {
        if ((same && (((flag1 & eflags) != 0) == ((flag2 & eflags) != 0))) ||
            ((!same) && (((flag1 & eflags) != 0) != ((flag2 & eflags) != 0)))) {
            return true;
        }
    }
    return false;
}

/* returns true if, given the passed in flag information the jump
 * will never be taken */
static bool
removable_jmp(instr_t *inst, uint eflags, uint eflags_valid)
{
    int opcode = instr_get_opcode(inst);
    switch (opcode) {
    case OP_jno_short:
    case OP_jno: {
        return check_flag_val(EFLAGS_READ_OF, 1, eflags, eflags_valid);
    }
    case OP_jo_short:
    case OP_jo: {
        return check_flag_val(EFLAGS_READ_OF, 0, eflags, eflags_valid);
    }
    case OP_jnb_short:
    case OP_jnb: {
        return check_flag_val(EFLAGS_READ_CF, 1, eflags, eflags_valid);
    }
    case OP_jb_short:
    case OP_jb: {
        return check_flag_val(EFLAGS_READ_CF, 0, eflags, eflags_valid);
    }
    case OP_jnz_short:
    case OP_jnz: {
        return check_flag_val(EFLAGS_READ_ZF, 1, eflags, eflags_valid);
    }
    case OP_jz_short:
    case OP_jz: {
        return check_flag_val(EFLAGS_READ_ZF, 0, eflags, eflags_valid);
    }
    case OP_jnbe_short:
    case OP_jnbe: {
        return (check_flag_val(EFLAGS_READ_CF, 1, eflags, eflags_valid) ||
                check_flag_val(EFLAGS_READ_ZF, 1, eflags, eflags_valid));
    }
    case OP_jbe_short:
    case OP_jbe: {
        return (check_flag_val(EFLAGS_READ_CF, 0, eflags, eflags_valid) &&
                check_flag_val(EFLAGS_READ_ZF, 0, eflags, eflags_valid));
    }
    case OP_jns_short:
    case OP_jns: {
        return check_flag_val(EFLAGS_READ_SF, 1, eflags, eflags_valid);
    }
    case OP_js_short:
    case OP_js: {
        return check_flag_val(EFLAGS_READ_SF, 0, eflags, eflags_valid);
    }
    case OP_jnp_short:
    case OP_jnp: {
        return check_flag_val(EFLAGS_READ_PF, 1, eflags, eflags_valid);
    }
    case OP_jp_short:
    case OP_jp: {
        return check_flag_val(EFLAGS_READ_PF, 0, eflags, eflags_valid);
    }
    case OP_jnl_short:
    case OP_jnl: {
        return compare_flag_vals(EFLAGS_READ_SF, EFLAGS_READ_OF, false, eflags,
                                 eflags_valid);
    }
    case OP_jl_short:
    case OP_jl: {
        return compare_flag_vals(EFLAGS_READ_SF, EFLAGS_READ_OF, true, eflags,
                                 eflags_valid);
    }
    case OP_jnle_short:
    case OP_jnle: {
        return (check_flag_val(EFLAGS_READ_ZF, 1, eflags, eflags_valid) ||
                compare_flag_vals(EFLAGS_READ_SF, EFLAGS_READ_OF, false, eflags,
                                  eflags_valid));
    }
    case OP_jle_short:
    case OP_jle: {
        return (check_flag_val(EFLAGS_READ_ZF, 0, eflags, eflags_valid) &&
                compare_flag_vals(EFLAGS_READ_SF, EFLAGS_READ_OF, true, eflags,
                                  eflags_valid));
    }
    }
    return false;
}

/* takes in an eflags, eflags_valid and eflags_invalid and propagates the information
 * forward simplifying instructions and eliminating jumps where possible, returns false
 * if it reaches a non simplifiable instruction depends on the any of the eflags_valid
 * or eflags_invalid before all flags in valid and invalid are overwritten by instructions
 */
static bool
do_forward_check_eflags(instr_t *inst, uint eflags, uint eflags_valid,
                        uint eflags_invalid, prop_state_t *state)
{
#    ifdef DEBUG
    dcontext_t *dcontext = state->dcontext;
#    endif
    instr_t *next_inst, *temp = NULL;
    uint eflags_check;
    int opcode;
    uint temp_ef;
    bool replace = false;
    ;
    if ((eflags_valid == 0) && (eflags_invalid == 0))
        return true;
    for (inst = instr_get_next(inst); inst != NULL; inst = next_inst) {
        next_inst = instr_get_next(inst);
        d_r_loginst(state->dcontext, 3, inst, " flag checking ");
        while ((inst != NULL) && instr_is_cti(inst)) {
            LOG(THREAD, LOG_OPTS, 3,
                "attempting to remove, eflags = " PFX " eflags valid = " PFX "\n", eflags,
                eflags_valid);
            if (removable_jmp(inst, eflags, eflags_valid)) {
#    ifdef DEBUG
                opt_stats_t.num_jmps_simplified++;
                d_r_loginst(state->dcontext, 3, inst, " removing this jmp ");
#    endif
                remove_inst(state->dcontext, state->trace, inst);
                inst = next_inst;
                next_inst = instr_get_next(inst);
            } else {
                if (INTERNAL_OPTION(unsafe_ignore_eflags_trace) &&
                    instr_get_opcode(inst) == OP_jecxz)
                    return true;
                LOG(THREAD, LOG_OPTS, 3, "forward eflags check failed (1)\n");
                return false;
            }
        }
        if ((inst == NULL) || instr_is_interrupt(inst) || instr_is_call(inst)) {
            LOG(THREAD, LOG_OPTS, 3, "forward eflags check failed (2)\n");
            return false;
        }

        // probably move to own method later if expanded to others
        // FIXME cmov's other setcc's cmc
        // don't bother to change to xor for zeroing, is not more efficient for 1 byte
        /* TODO : add set[n]be set[n]l set[n]le, skip p since never used and might not
         * be setting it right
         */
        opcode = instr_get_opcode(inst);
        if (((opcode == OP_seto) || (opcode == OP_setno)) &&
            ((eflags_valid & EFLAGS_READ_OF) != 0)) {
            if (((eflags & EFLAGS_READ_OF) != 0 && opcode == OP_seto) ||
                ((eflags & EFLAGS_READ_OF) == 0 && opcode == OP_setno)) {
                temp = INSTR_CREATE_mov_st(state->dcontext, instr_get_dst(inst, 0),
                                           OPND_CREATE_INT8(1));
            } else {
                temp = INSTR_CREATE_mov_st(state->dcontext, instr_get_dst(inst, 0),
                                           OPND_CREATE_INT8(0));
            }
            replace = true;
        }
        if (((opcode == OP_setz) || (opcode == OP_setnz)) &&
            ((eflags_valid & EFLAGS_READ_ZF) != 0)) {
            if (((eflags & EFLAGS_READ_ZF) != 0 && opcode == OP_setz) ||
                ((eflags & EFLAGS_READ_ZF) == 0 && opcode == OP_setnz)) {
                temp = INSTR_CREATE_mov_st(state->dcontext, instr_get_dst(inst, 0),
                                           OPND_CREATE_INT8(1));
            } else {
                temp = INSTR_CREATE_mov_st(state->dcontext, instr_get_dst(inst, 0),
                                           OPND_CREATE_INT8(0));
            }
            replace = true;
        }
        if (((opcode == OP_setb) || (opcode == OP_setnb)) &&
            ((eflags_valid & EFLAGS_READ_CF) != 0)) {
            if (((eflags & EFLAGS_READ_CF) != 0 && opcode == OP_setb) ||
                ((eflags & EFLAGS_READ_CF) == 0 && opcode == OP_setnb)) {
                temp = INSTR_CREATE_mov_st(state->dcontext, instr_get_dst(inst, 0),
                                           OPND_CREATE_INT8(1));
            } else {
                temp = INSTR_CREATE_mov_st(state->dcontext, instr_get_dst(inst, 0),
                                           OPND_CREATE_INT8(0));
            }
            replace = true;
        }
        if (((opcode == OP_sets) || (opcode == OP_setns)) &&
            ((eflags_valid & EFLAGS_READ_SF) != 0)) {
            if (((eflags & EFLAGS_READ_SF) != 0 && opcode == OP_sets) ||
                ((eflags & EFLAGS_READ_SF) == 0 && opcode == OP_setns)) {
                temp = INSTR_CREATE_mov_st(state->dcontext, instr_get_dst(inst, 0),
                                           OPND_CREATE_INT8(1));
            } else {
                temp = INSTR_CREATE_mov_st(state->dcontext, instr_get_dst(inst, 0),
                                           OPND_CREATE_INT8(0));
            }
            replace = true;
        }

        temp_ef = EFLAGS_READ_SF | EFLAGS_READ_ZF | EFLAGS_READ_AF | EFLAGS_READ_PF |
            EFLAGS_READ_CF;
        if ((opcode == OP_lahf) && ((eflags_valid & temp_ef) == temp_ef)) {
            temp_ef = 0x02;
            if ((eflags & EFLAGS_READ_CF) != 0)
                temp_ef = temp_ef | 0x01;
            if ((eflags & EFLAGS_READ_PF) != 0)
                temp_ef = temp_ef | 0x04;
            if ((eflags & EFLAGS_READ_AF) != 0)
                temp_ef = temp_ef | 0x10;
            if ((eflags & EFLAGS_READ_ZF) != 0)
                temp_ef = temp_ef | 0x40;
            // have to sign extend so the create immed int turns out right
            if ((eflags & EFLAGS_READ_SF) != 0)
                temp_ef = temp_ef | 0xffffff80;
            LOG(THREAD, LOG_OPTS, 3, "lahf val  %d  " PFX "\n", temp_ef, temp_ef);
            temp = INSTR_CREATE_mov_imm(state->dcontext, instr_get_dst(inst, 0),
                                        OPND_CREATE_INT8(temp_ef));
            replace = true;
        }

        if (replace) {
#    ifdef DEBUG
            opt_stats_t.num_instrs_simplified++;
            d_r_loginst(state->dcontext, 3, inst, " old instruction");
            d_r_loginst(state->dcontext, 3, temp, " simplified to  ");
#    endif
            replace_inst(state->dcontext, state->trace, inst, temp);
            inst = temp;
            replace = false;
        }

        eflags_check = instr_get_eflags(inst, DR_QUERY_DEFAULT);
        if (((eflags_invalid & eflags_check) != 0) ||
            ((eflags_valid & eflags_check) != 0)) {
            d_r_loginst(state->dcontext, 3, inst, " uses eflags!");
            LOG(THREAD, LOG_OPTS, 3,
                "forward eflags check failed(3)  " PFX "   " PFX "  " PFX "\n",
                eflags_valid, eflags_invalid, eflags_check);
            return false;
        }
        eflags_invalid =
            eflags_invalid & (~(EFLAGS_WRITE_TO_READ(eflags_check & EFLAGS_WRITE_ALL)));
        eflags_valid =
            eflags_valid & (~(EFLAGS_WRITE_TO_READ(eflags_check & EFLAGS_WRITE_ALL)));
        if ((eflags_valid == 0) && (eflags_invalid == 0))
            return true;
    }
    LOG(THREAD, LOG_OPTS, 3, "forward eflags check failed(4)\n");
    return false;
}

/* looks at the eflags of the instr passed in and checks to see if there
 * is any dependency on the eflags written, gives up at CTI's
 * return true if no dependency found
 */
static bool
forward_check_eflags(instr_t *inst, prop_state_t *state)
{
    return do_forward_check_eflags(
        inst, 0, 0,
        EFLAGS_WRITE_TO_READ(instr_get_eflags(inst, DR_QUERY_DEFAULT) & EFLAGS_WRITE_ALL),
        state);
}

static instr_t *
make_imm_store(prop_state_t *state, instr_t *inst, int value)
{
    return INSTR_CREATE_mov_st(state->dcontext, instr_get_dst(inst, 0),
                               OPND_CREATE_INT32(value));
}

/* replaces inst with a mov imm of value to the same dst */
static instr_t *
make_to_imm_store(instr_t *inst, int value, prop_state_t *state)
{
    instr_t *replacement;
    opnd_t dst = instr_get_dst(inst, 0);
    dcontext_t *dcontext = state->dcontext;

    if (value == 0 && opnd_is_reg(dst)) {
        replacement = INSTR_CREATE_xor(dcontext, dst, dst);
        if (instr_get_prefix_flag(inst, PREFIX_DATA)) {
            instr_set_prefix_flag(replacement, PREFIX_DATA);
            LOG(THREAD, LOG_OPTS, 3, "carrying data prefix over %d\n",
                instr_get_prefixes(inst));
        }
        if (do_forward_check_eflags(
                inst, 0, 0,
                EFLAGS_WRITE_TO_READ(instr_get_eflags(replacement, DR_QUERY_DEFAULT)),
                state)) {
            /* check size prefixes, ignore lock and addr prefix */
            replace_inst(dcontext, state->trace, inst, replacement);
            return replacement;
        } else {
            d_r_loginst(dcontext, 3, inst,
                        " unable to simplify move zero to xor, e-flags check failed ");
            instr_destroy(dcontext, replacement);
        }
    }

    /* is always creating the right sized imm? */
    replacement = INSTR_CREATE_mov_st(state->dcontext, dst,
                                      opnd_create_immed_int(value, opnd_get_size(dst)));
    /* handle prefixes, imm->reg (data) imm->mem (data & addr) */
    if (instr_get_prefix_flag(inst, PREFIX_DATA)) {
        instr_set_prefix_flag(replacement, PREFIX_DATA);
        LOG(THREAD, LOG_OPTS, 3, "carrying data prefix over %d\n",
            instr_get_prefixes(inst));
    }
    if (instr_get_prefix_flag(inst, PREFIX_ADDR) && opnd_is_memory_reference(dst)) {
        instr_set_prefix_flag(replacement, PREFIX_ADDR);
        LOG(THREAD, LOG_OPTS, 3, "carrying addr prefix over %d\n",
            instr_get_prefixes(inst));
    }
    replace_inst(dcontext, state->trace, inst, replacement);
    return replacement;
}

static instr_t *
make_to_nop(prop_state_t *state, instr_t *inst, const char *pre, const char *post,
            const char *fail)
{
    instr_t *backup;
    if (forward_check_eflags(inst, state)) {
        d_r_loginst(state->dcontext, 3, inst, pre);
        backup = INSTR_CREATE_nop(state->dcontext);
        replace_inst(state->dcontext, state->trace, inst, backup);
        d_r_loginst(state->dcontext, 3, backup, post);
        return backup;
    } else {
        d_r_loginst(state->dcontext, 3, inst, fail);
        return inst;
    }
}

/* uses < 0 as short for if top bit is set */
/* calculates zf pf sf flags from some result immed */
static int
calculate_zf_pf_sf(int immed)
{
    int result = 0;
    int i;
    bool parity = true;
    if (immed == 0)
        result = result | EFLAGS_READ_ZF;
    if (immed < 0)
        result = result | EFLAGS_READ_SF;
    for (i = 0; i < 8; i++) {
        if (((immed >> i) & 0x1) != 0)
            parity = !parity;
    }
    if (parity)
        result = result | EFLAGS_READ_PF;
    return result;
}

/* simplifies an instruction where possible */
/* NOTE that at this point all subsized arguments have been sign extended */
/* if op takes subsize note signextend (movzx and shifts for ex.) must */
/* explicitly check the size of the immed */
static instr_t *
prop_simplify(instr_t *inst, prop_state_t *state)
{
    instr_t *replacement;
    int opcode, num_src, num_dst, immed1, immed2, immed3 = 0, i;
    opnd_t temp_opnd;
    uint eflags, eflags_valid, eflags_invalid;
    dcontext_t *dcontext = state->dcontext;

    num_src = instr_num_srcs(inst);
    num_dst = instr_num_dsts(inst);
    opcode = instr_get_opcode(inst);

    if (opcode == OP_lea) {
        temp_opnd = instr_get_src(inst, 0);
        if (opnd_is_constant_address(temp_opnd)) {
            inst = make_to_imm_store(inst, opnd_get_disp(temp_opnd), state);
        }
        return inst;
    }

    if ((num_src == 1) && (num_dst == 1) && opnd_is_immed_int(instr_get_src(inst, 0))) {
        immed1 = (int)opnd_get_immed_int(instr_get_src(inst, 0));
        switch (opcode) {
            // movsx bsf bsr
        case OP_mov_st:
        case OP_mov_ld: {
            inst = make_to_imm_store(inst, immed1, state);
            break;
        }
        case OP_movzx: {
            if (opnd_get_size(instr_get_src(inst, 0)) == OPSZ_1) {
                immed3 = immed1 & 0x000000ff;
            } else {
                immed3 = immed1 & 0x0000ffff;
            }
            inst = make_to_imm_store(inst, immed3, state);
            break;
        }
        case OP_movsx: {
            /* already sign extended */
            immed3 = immed1;
            inst = make_to_imm_store(inst, immed3, state);
            break;
        }
        case OP_bswap: {
            immed3 = (((immed1 << 24) & 0xff000000) | ((immed1 << 8) & 0x00ff0000) |
                      ((immed1 >> 8) & 0x0000ff00) | ((immed1 >> 24) & 0x000000ff));
            inst = make_to_imm_store(inst, immed3, state);
            break;
        }
        case OP_not: {
            immed3 = ~immed1;
            inst = make_to_imm_store(inst, immed3, state);
            break;
        }
        case OP_neg: {
            immed3 = -immed1;
            inst = make_to_imm_store(inst, immed3, state);
            break;
        }
        case OP_inc: {
            immed3 = immed1 + 1;
            eflags = calculate_zf_pf_sf(immed3);
            eflags_valid = EFLAGS_READ_ZF | EFLAGS_READ_SF | EFLAGS_READ_PF;
            eflags_invalid = instr_get_eflags(inst, DR_QUERY_DEFAULT) & EFLAGS_READ_ALL &
                (~eflags_valid);
            if (do_forward_check_eflags(inst, eflags, eflags_valid, eflags_invalid,
                                        state))
                inst = make_to_imm_store(inst, immed3, state);
            else
                state->hint = make_imm_store(state, inst, immed3);
            break;
        }
        case OP_dec: {
            immed3 = immed1 - 1;
            eflags = calculate_zf_pf_sf(immed3);
            eflags_valid = EFLAGS_READ_ZF | EFLAGS_READ_SF | EFLAGS_READ_PF;
            eflags_invalid = instr_get_eflags(inst, DR_QUERY_DEFAULT) & EFLAGS_READ_ALL &
                (~eflags_valid);
            if (do_forward_check_eflags(inst, eflags, eflags_valid, eflags_invalid,
                                        state))
                inst = make_to_imm_store(inst, immed3, state);
            else
                state->hint = make_imm_store(state, inst, immed3);
            break;
        }
        default: {
            // unable to simplify instruction
        }
        }
        return inst;
    }

    if ((num_src == 2) && opnd_is_immed_int(instr_get_src(inst, 1)) &&
        !opnd_is_immed_int(instr_get_src(inst, 0))) {
        immed1 = (int)opnd_get_immed_int(instr_get_src(inst, 1));
        if (opcode == OP_cmp || opcode == OP_test) {
            temp_opnd = instr_get_src(inst, 1);
            if ((immed1 <= 127) && (immed1 >= -128)) {
                opnd_set_size(&temp_opnd, OPSZ_1);
                instr_set_src(inst, 1, temp_opnd);
            }
        }
        /* jecxz hack, Should only match our indirect branch handling thing */
        if (opcode == OP_jecxz) {
            if (immed1 == 0) {
                /* NOTE : this hardcodes indirect branch stuff */
                instr_t *inst2, *inst3;
                LOG(THREAD, LOG_OPTS, 3,
                    "Found removable jeczx inst noping it and removing 2 prev, and next "
                    "three instructions\n");
                replacement = INSTR_CREATE_nop(state->dcontext);
                replace_inst(state->dcontext, state->trace, inst, replacement);
                inst = replacement;

                /* remove control flow after jecxz */
                inst2 = instr_get_next(inst);
                d_r_loginst(dcontext, 3, inst2, "removing ");
                ASSERT(instr_get_opcode(inst2) == OP_lea &&
                       opnd_get_reg(instr_get_dst(inst2, 0)) == REG_ECX);
                instrlist_remove(state->trace, inst2);
                inst2 = instr_get_next(inst);
                d_r_loginst(dcontext, 3, inst2, "removing ");
                ASSERT(instr_get_opcode(inst2) == OP_jmp);
                instrlist_remove(state->trace, inst2);

                /* remove prev inst */
                inst2 = instr_get_prev(inst);
                inst3 = instr_get_prev(inst2);
                if (instr_get_opcode(inst2) == OP_nop ||
                    ((instr_get_opcode(inst2) == OP_mov_imm ||
                      instr_get_opcode(inst2) == OP_mov_st || is_zeroing_instr(inst2)) &&
                     opnd_get_reg(instr_get_dst(inst2, 0)) == REG_ECX)) {
                    d_r_loginst(dcontext, 3, inst2, "removing ");
                    instrlist_remove(state->trace, inst2);
                } else {
                    d_r_loginst(
                        dcontext, 1, inst2,
                        "ERROR : unexpected instruction in constant prop indirect "
                        "branch removal (1) aborting");
                    return inst;
                }
                inst2 = inst3;
                /* three possiblities at this point */
                /* is lea or pop from return = shouldn't happen, well at */
                /* not till we start propagating stack vals, in which case */
                /* maby we can ignore since -call_return matching will get */
                /* is push from indirect call, move imm->reg, save to ecxoff*/
                /* is mov imm->reg, save to ecxoff */
                /* !save to ecxoff might be noped, as might any of the other */
                /* prev. by constant prop, asserts are fragile, remove them? */
                if (instr_get_opcode(inst2) == OP_pop ||
                    instr_get_opcode(inst2) == OP_lea) {
                    d_r_loginst(dcontext, 3, inst2,
                                "ERROR : found what looks like a call return in jecxz "
                                "removal, but we can't find those yet!! aborting");
                    return inst;
                }
                if (instr_get_opcode(inst2) == OP_push_imm) {
                    d_r_loginst(dcontext, 3, inst2, "skipping ");
                    inst2 = instr_get_prev(inst2);
                }
                inst3 = instr_get_prev(inst2);
                if (instr_get_opcode(inst2) == OP_nop ||
                    ((instr_get_opcode(inst2) == OP_mov_imm ||
                      instr_get_opcode(inst2) == OP_mov_st || is_zeroing_instr(inst2)) &&
                     opnd_get_reg(instr_get_dst(inst2, 0)) == REG_ECX)) {
                    d_r_loginst(dcontext, 3, inst2, "removing ");
                    instrlist_remove(state->trace, inst2);
                } else {
                    d_r_loginst(
                        dcontext, 1, inst2,
                        "ERROR : unexpected instruction in constant prop indirect "
                        "branch removal (2) aborting");
                    return inst;
                }
                inst2 = inst3;
                if (instr_get_opcode(inst2) == OP_nop ||
                    is_store_to_ecxoff(dcontext, inst2)) {
                    d_r_loginst(dcontext, 3, inst2, "removing ");
                    instrlist_remove(state->trace, inst2);
                } else {
                    d_r_loginst(
                        dcontext, 1, inst2,
                        "ERROR : unexpected instruction in constant prop indirect "
                        "branch removal (3) aborting");
                    return inst;
                }

                /* remove post inst */
                /* some op may have removed this already so check to be sure */
                inst2 = instr_get_next(inst);
                if (is_load_from_ecxoff(dcontext, inst2)) {
                    d_r_loginst(dcontext, 3, inst2, "removing ");
                    instrlist_remove(state->trace, inst2);
                } else {
                    d_r_loginst(
                        dcontext, 1, inst2,
                        "ERROR : unexpected instruction in constant prop indirect "
                        "branch removal (a), This could be very bad");
                }

#    ifdef DEBUG
                opt_stats_t.num_jmps_simplified++;
                opt_stats_t.num_instrs_simplified++;
                opt_stats_t.num_jecxz_instrs_removed += 6;
#    endif
            } else {
                d_r_loginst(
                    dcontext, 1, inst,
                    "ERROR : Constant prop predicts indirect branch exit from trace "
                    "always taken! If this is part of a reconstruct for exception "
                    "state then the pc calculated is going to be wrong, if it isn't "
                    "then something is broken regarding constant prop");
            }
        }
        return inst;
    }

    if ((num_src == 2) && opnd_is_immed_int(instr_get_src(inst, 0))) {
        immed1 = (int)opnd_get_immed_int(instr_get_src(inst, 0));

        if (!opnd_is_immed_int(instr_get_src(inst, 1))) {
            switch (opcode) {
            case OP_sub:
            case OP_add:
            case OP_or:
            case OP_and:
            case OP_xor: {
                temp_opnd = instr_get_src(inst, 0);
                if ((immed1 <= 127) && (immed1 >= -128)) {
                    opnd_set_size(&temp_opnd, OPSZ_1);
                    instr_set_src(inst, 0, temp_opnd);
                }
                break;
            }
            case OP_test: {
                temp_opnd = instr_get_src(inst, 0);
                if ((immed1 <= 127) && (immed1 >= -128))
                    opnd_set_size(&temp_opnd, OPSZ_1);
                instr_set_src(inst, 0, instr_get_src(inst, 1));
                instr_set_src(inst, 1, temp_opnd);
                break;
            }
            case OP_push: {
                instr_set_opcode(inst, OP_push_imm);
                break;
            }
            }
            return inst;
        } else {
            immed2 = (int)opnd_get_immed_int(instr_get_src(inst, 1));
            switch (num_dst) {
            case 0: {
                switch (opcode) {
                case OP_test: {
                    immed3 = immed1 & immed2;
                    eflags_valid = EFLAGS_READ_CF | EFLAGS_READ_PF | EFLAGS_READ_ZF |
                        EFLAGS_READ_SF | EFLAGS_READ_OF | EFLAGS_READ_AF;
                    // technically AF is undefined, but since no one
                    // should be relying on it we can set it to zero
                    eflags = calculate_zf_pf_sf(immed3);
                    if (do_forward_check_eflags(inst, eflags, eflags_valid, 0, state)) {
                        replacement = INSTR_CREATE_nop(state->dcontext);
                        replace_inst(dcontext, state->trace, inst, replacement);
                        inst = replacement;
                    }
                    break;
                }
                case OP_cmp: {
                    // FIXME of and sf and af correct? FIXME!!
                    immed3 = immed1 - immed2;
                    eflags = calculate_zf_pf_sf(immed3);
                    if (((uint)immed1) < ((uint)immed2)) {
                        eflags = eflags | EFLAGS_READ_CF;
                    }
                    if (((immed1 >= immed2) && ((eflags & EFLAGS_READ_CF) != 0)) ||
                        ((immed1 < immed2) && ((eflags & EFLAGS_READ_CF) == 0))) {
                        eflags = eflags | EFLAGS_READ_OF;
                    }
                    if ((immed1 & 0x7) < (immed2 & 0x7))
                        eflags = eflags | EFLAGS_READ_AF;
                    eflags_valid = EFLAGS_READ_CF | EFLAGS_READ_PF | EFLAGS_READ_ZF |
                        EFLAGS_READ_SF | EFLAGS_READ_OF | EFLAGS_READ_AF;
                    if (do_forward_check_eflags(inst, eflags, eflags_valid, 0, state)) {
                        replacement = INSTR_CREATE_nop(state->dcontext);
                        replace_inst(state->dcontext, state->trace, inst, replacement);
                        inst = replacement;
                    }
                    break;
                }
                default: {
                    // couldn't handle
                }
                }
                break;
            }
            case 1: {
                /* TODO: maby explicitly find some flags? */
                bool replace = true;
                int size;
                switch (opcode) {
                case OP_add: immed3 = immed2 + immed1; break;
                case OP_sub: immed3 = immed2 - immed1; break;
                case OP_or: immed3 = immed2 | immed1; break;
                case OP_and: immed3 = immed2 & immed1; break;
                case OP_xor: immed3 = immed2 ^ immed1; break;
                case OP_shl:
                    /* same as OP_sal */
                    size = opnd_get_size(instr_get_src(inst, 1));
                    immed3 = immed2 << (immed1 & 0x1F);
                    /* adjust for size */
                    if (size == OPSZ_1) {
                        if ((immed3 & 0x00000080) != 0)
                            immed3 |= 0xffffff00;
                        else
                            immed3 &= 0x000000ff;
                    } else if (size == OPSZ_2) {
                        if ((immed3 & 0x00008000) != 0)
                            immed3 |= 0xffff0000;
                        else
                            immed3 &= 0x0000ffff;
                    } else if (size != OPSZ_4)
                        replace = false;
                    break;
                case OP_sar: {
                    bool neg = immed2 < 0;
                    size = opnd_get_size(instr_get_src(inst, 1));
                    if (size == OPSZ_1) {
                        immed3 = immed2 >> (immed1 & 0x1f);
                        if (neg)
                            immed3 |= 0xffffff00;
                        else
                            immed3 &= 0x000000ff;
                    } else if (size == OPSZ_2) {
                        immed3 = immed2 >> (immed1 & 0x1f);
                        if (neg)
                            immed3 |= 0xffff0000;
                        else
                            immed3 &= 0x0000ffff;
                    } else if (size == OPSZ_4) {
                        immed3 = immed2;
                        if (neg) {
                            for (i = 0; i < (immed1 & 0x1F); i++)
                                immed3 = (immed3 >> 1) | 0x80000000;
                        } else {
                            immed3 = immed3 >> (immed1 & 0x1f);
                        }
                    } else
                        replace = false;
                    break;
                }
                case OP_shr:
                    size = opnd_get_size(instr_get_src(inst, 1));
                    if (immed1 == 0 || immed2 == 0)
                        immed3 = immed2;
                    else {
                        if (size == OPSZ_1) {
                            immed3 = (immed2 & 0x000000ff) >> (immed1 & 0x1f);
                        } else if (size == OPSZ_2) {
                            immed3 = (immed2 & 0x0000ffff) >> (immed1 & 0x1f);
                        } else if (size == OPSZ_4) {
                            if (immed2 > 0) {
                                immed3 = immed2 >> (immed1 & 0x1f);
                            } else {
                                immed3 = immed2;
                                for (i = 0; i < (immed1 & 0x1F); i++)
                                    immed3 = (immed3 >> 1) & 0x7fffffff;
                            }
                        } else
                            replace = false;
                    }
                    break;
                    /* TODO : rotates, keep size issues in mind */
                case OP_ror:
                case OP_rol:
                default:
                    replace = false;
                    /* can't handle this instruction */
                }
                if (!replace)
                    break;
                if (forward_check_eflags(inst, state))
                    inst = make_to_imm_store(inst, immed3, state);
                else
                    state->hint = make_imm_store(state, inst, immed3);
                break;
            }
            case 2: {
                // mul divide xchg xadd
            }
            default: {
                // unable to simlify this instruction
            }
            }
        }
        return inst;
    }

    // cpuid

    return inst;
}

/* initializes all the trace constant stuff and add */
static void
get_trace_constant(prop_state_t *state)
{
    /* can add all dynamo addresses here, they are never aliased so always */
    /* safe to optimize, but takes up space in our cache, with new jump code */
    /* probably only use exc so just put it in, and maby eax to since is fav */
    /* when need to store flags/pass arg, can always add more location later */
    /* probably cleaner way of getting addresses but who cares for now */
    set_address_val(
        state, opnd_get_disp(opnd_create_dcontext_field(state->dcontext, XCX_OFFSET)), 0,
        PS_KEEP);
    set_address_val(
        state, opnd_get_disp(opnd_create_dcontext_field(state->dcontext, XAX_OFFSET)), 0,
        PS_KEEP);
}

/* updates the prop state as appropriate */
static instr_t *
update_prop_state(prop_state_t *state, instr_t *inst, bool intrace)
{
    int opcode = instr_get_opcode(inst);
    int num_dst = instr_num_dsts(inst);
    bool is_zeroing = is_zeroing_instr(inst);
    dcontext_t *dcontext = state->dcontext;
    instr_t *backup;
    opnd_t opnd;
    int val, i;
    reg_id_t reg;
    if (is_zeroing ||
        ((opcode == OP_mov_imm || opcode == OP_mov_st) &&
         opnd_is_immed_int(instr_get_src(inst, 0)))) {
        if (is_zeroing)
            val = 0;
        else
            val = (int)opnd_get_immed_int(instr_get_src(inst, 0));
        opnd = instr_get_dst(inst, 0);
        if (opnd_is_reg(opnd)) {
            reg = opnd_get_reg(opnd) - REG_START_32;
            if (reg < 8 /* rules out underflow */) {
                /* if resetting to same value then just nop the instruction */
                if (intrace && (state->reg_state[reg] & PS_VALID_VAL) != 0 &&
                    state->reg_vals[reg] == val) {
                    inst = make_to_nop(state, inst,
                                       " register already set to val, simplify ", " to ",
                                       " register already set to val, but unable to "
                                       "simplify due to eflags");
                } else {
                    state->reg_state[reg] = PS_VALID_VAL;
                    state->reg_vals[reg] = val;
                }
            } else {
                reg = opnd_get_reg(opnd) - REG_START_16;
                if (reg < 8 /* rules out underflow */) {
                    /* if resetting to same value then just nop the instruction */
                    if (intrace &&
                        ((state->reg_state[reg] & PS_VALID_VAL) != 0 ||
                         ((state->reg_state[reg] & PS_LVALID_VAL) != 0 &&
                          (state->reg_state[reg] & PS_HVALID_VAL) != 0)) &&
                        (state->reg_vals[reg] & 0x0000ffff) == (val & 0x0000ffff)) {
                        inst =
                            make_to_nop(state, inst,
                                        " register already set to val, simplify ", " to ",
                                        " register already set to val, but unable to "
                                        "simplify due to eflags");
                    } else {
                        state->reg_state[reg] |= PS_LVALID_VAL | PS_HVALID_VAL;
                        state->reg_vals[reg] =
                            (state->reg_vals[reg] & 0xffff0000) | (val & 0x0000ffff);
                    }
                } else {
                    reg = opnd_get_reg(opnd) - REG_START_8;
                    if (reg < 4 /* rules out underflow */) {
                        /* if resetting to same value then just nop the instruction */
                        if (intrace &&
                            ((state->reg_state[reg] & PS_VALID_VAL) != 0 ||
                             (state->reg_state[reg] & PS_LVALID_VAL) != 0) &&
                            (state->reg_vals[reg] & 0x000000ff) == (val & 0x000000ff)) {
                            inst = make_to_nop(state, inst,
                                               " register already set to val, simplify ",
                                               " to ",
                                               " register already set to val, but unable "
                                               "to simplify due to eflags");
                        } else {
                            state->reg_state[reg] |= PS_LVALID_VAL;
                            state->reg_vals[reg] =
                                (state->reg_vals[reg] & 0xffffff00) | (val & 0x000000ff);
                        }
                    } else {
                        reg -= 4;
                        if (reg < 4 /* rules out underflow */) {
                            /* if resetting to same value then just nop the instruction */
                            if (intrace &&
                                ((state->reg_state[reg] & PS_VALID_VAL) != 0 ||
                                 (state->reg_state[reg] & PS_HVALID_VAL) != 0) &&
                                (state->reg_vals[reg] & 0x0000ff00) ==
                                    ((val << 8) & 0x0000ff00)) {
                                inst = make_to_nop(
                                    state, inst,
                                    " register already set to val, simplify ", " to ",
                                    " register already set to val, but unable to "
                                    "simplify due to eflags");
                            } else {
                                state->reg_state[reg] |= PS_HVALID_VAL;
                                state->reg_vals[reg] =
                                    (state->reg_vals[reg] & 0xffff00ff) |
                                    ((val << 8) & 0x0000ff00);
                            }
                        } else {
                            /* just in case */
                            for (i = 0; i < 8; i++) {
                                if (instr_writes_to_reg(inst, REG_START_32 + (reg_id_t)i,
                                                        DR_QUERY_DEFAULT)) {
                                    state->reg_state[i] = 0;
                                }
                            }
                        }
                    }
                }
            }
        } else {
            // do constant addresses
            if (opnd_is_constant_address(opnd) && cp_global_aggr > 0) {
                int disp = opnd_get_disp(opnd);
                for (i = 0; i < NUM_CONSTANT_ADDRESS; i++) {
                    if (state->addresses[i] == disp && state->address_vals[i] == val &&
                        (state->address_state[i] & PS_VALID_VAL) != 0) {
                        d_r_loginst(dcontext, 3, inst,
                                    " mem location already set to val, simplify ");
                        backup = INSTR_CREATE_nop(dcontext);
                        replace_inst(dcontext, state->trace, inst, backup);
                        d_r_loginst(dcontext, 3, backup, " to ");
                        inst = backup;
                    }
                }
                update_address_val(state, disp, val);
            }

            // do stack vals

            if (opnd_is_stack_address(opnd) && cp_local_aggr > 0) {
                int disp = opnd_get_disp(opnd);
                for (i = 0; i < NUM_STACK_SLOTS; i++) {
                    if (state->stack_offsets_ebp[i] == disp &&
                        state->stack_vals[i] == val &&
                        (state->stack_address_state[i] & PS_VALID_VAL) != 0 &&
                        state->stack_scope[i] == state->cur_scope) {
                        d_r_loginst(dcontext, 3, inst,
                                    " mem location already set to val, simplify ");
                        backup = INSTR_CREATE_nop(dcontext);
                        replace_inst(dcontext, state->trace, inst, backup);
                        d_r_loginst(dcontext, 3, backup, " to ");
                        inst = backup;
                    }
                }

                update_stack_val(state, disp, val);
            }
        }
    } else {
        // call and int
        if (instr_is_interrupt(inst) || instr_is_call(inst)) {
            // can assume mem addresses not touched??
            // shouldn't be going to app code for call at least
            for (i = 0; i < 8; i++)
                state->reg_state[i] = 0;
        }
        // update for regs written to, actually if xh then don't need to
        // invalidate xl and vice versa, but to much work to check for that probably
        // unlikely occurrence
        for (i = 0; i < 8; i++) {
            if (instr_writes_to_reg(inst, REG_START_32 + (reg_id_t)i, DR_QUERY_DEFAULT)) {
                state->reg_state[i] = 0;
            }
        }
        // update mem caches
        for (i = 0; i < num_dst; i++) {
            opnd = instr_get_dst(inst, i);
            if (opnd_is_constant_address(opnd) && cp_global_aggr > 0) {
                clear_address_val(state, opnd_get_disp(opnd));
            }
        }
        // update stack cahes
        for (i = 0; i < num_dst; i++) {
            opnd = instr_get_dst(inst, i);
            if (opnd_is_stack_address(opnd) && cp_local_aggr > 0) {
                clear_stack_val(state, opnd_get_disp(opnd));
            }
        }
    }
    return inst;
}

typedef struct _two_entry_list_element_t {
    int disp;
    int scope;
    struct _two_entry_list_element_t *next;
} two_entry_list_element_t;

typedef struct _start_list_element_t {
    int endscope;
    two_entry_list_element_t *next;
} start_list_element_t;

/*************************************************************************/

/*This track the scopes. The number indicates the depth of the nested
  scopes. Also checks for stack constant instructions */
instr_t *
handle_stack(prop_state_t *state, instr_t *inst)
{
    dcontext_t *dcontext = state->dcontext;
    int i;
    if (instr_get_opcode(inst) == OP_enter ||
        ((instr_get_opcode(inst) == OP_mov_st || instr_get_opcode(inst) == OP_mov_ld) &&
         opnd_is_reg(instr_get_src(inst, 0)) &&
         opnd_get_reg(instr_get_src(inst, 0)) == REG_ESP &&
         opnd_is_reg(instr_get_dst(inst, 0)) &&
         opnd_get_reg(instr_get_dst(inst, 0)) == REG_EBP)) {
        state->cur_scope++;
        LOG(THREAD, LOG_OPTS, 3, "Adjust scope up to %d\n", state->cur_scope);
        return inst;
    }
    if (instr_get_opcode(inst) == OP_leave ||
        (instr_get_opcode(inst) == OP_pop && opnd_is_reg(instr_get_dst(inst, 0)) &&
         opnd_get_reg(instr_get_dst(inst, 0)) == REG_EBP)) {
        state->cur_scope--;

        for (i = 0; i < NUM_STACK_SLOTS; i++) {
            if (state->stack_scope[i] > state->cur_scope &&
                state->stack_address_state[i] != 0) {
                state->stack_address_state[i] = 0;
            }
        }
        LOG(THREAD, LOG_OPTS, 3, "Adjust scope down to %d\n", state->cur_scope);
        return inst;
    }
    if (instr_writes_to_reg(inst, REG_EBP, DR_QUERY_DEFAULT)) {
        d_r_loginst(dcontext, 2, inst, "Lost stack scope count");
        state->lost_scope_count = true;
        for (i = 0; i < NUM_STACK_SLOTS; i++) {
            state->stack_address_state[i] = 0;
        }
    }
    return inst;
}

/* FIXME : (could affect correctness at higher levels of optimization)
 * constant address and various operand sizes, at level 1 what if we write/read
 * a 16bit value, we'll actually do 32bit, hard to detect since ?believe? will
 * both be OPSZ_4_short2, look at prefix?  Similarly at level 2 we don't do any size
 * checking at all for constant address, what if is a byte of existing etc.
 * Ah, just trust the programmer, these are all addresses from him anyways
 * we can trust that he won't do anything that weird with them
 * FIXME :  more robust matching for the dynamorio stack call hints, the pattern
 * matching at the moment is somewhat birttle, though should fail gracefully
 * TODO : (doesn't affect correctness only effectiveness)
 * Easy
 *   reverse cmp arg. if one constant and can flip any dependat jmps? will have to
 *      either check target or assume normal eflags
 *   if write to xh then don't need to invalidate xl, right now invalidate all
 *      probably not worth to effort to fix, is pretty rare occurrence where it
 *      matters
 *   handle more setcc cmovcc instrs in do_forward_eflags_check, probably
 *      not worth it, they're almost never used
 *   have more instrs in simplify figure out their eflags, usually not worth it
 *   handle more instrs in simplify, (already have all the most common)
 * Hard
 *   size issues
 *   any floating point stuff? probably not feasible or worthwhile
 */

/* performs constant prop, loops through all the instruction updating the
 * prop state for each one, propagating information collected so far into
 * opnds and and calling simplify on the results */
static void
constant_propagation(dcontext_t *dcontext, app_pc tag, instrlist_t *trace)
{
    prop_state_t state;
    int num_src, num_dst, i;
    opnd_t opnd, prop_opnd;
    instr_t *inst, *backup;

    /* FIXME: this is a data race!
     * and why set this for every trace?  options are static!
     * have some kind of optimize_init to set these
     */
    cp_global_aggr = dynamo_options.constant_prop % 10;
    cp_local_aggr = (dynamo_options.constant_prop - cp_global_aggr) / 10;

    LOG(THREAD, LOG_OPTS, 3,
        "starting constant prop, global aggresiveness %d local aggresiveness %d\n",
        cp_global_aggr, cp_local_aggr);
    DOLOG(4, LOG_OPTS, { instrlist_disassemble(dcontext, tag, trace, THREAD); });

    state.dcontext = dcontext;
    state.trace = trace;
    state.hint = NULL;
    state.lost_scope_count = false;

    state.cur_scope = 0;
    for (i = 0; i < NUM_CONSTANT_ADDRESS; i++)
        state.address_state[i] = 0;
    for (i = 0; i < NUM_STACK_SLOTS; i++)
        state.stack_address_state[i] = 0;

    for (i = 0; i < 8; i++)
        state.reg_state[i] = 0;

    get_trace_constant(&state);

    for (inst = instrlist_first(trace); inst != NULL; inst = instr_get_next(inst)) {
        /* backup in case results turns out to be unencodable */
        backup = NULL;

        inst = handle_stack(&state, inst);
        ASSERT(inst != NULL);

        /* propagate to sources */
        num_src = instr_num_srcs(inst);

        d_r_loginst(dcontext, 3, inst, " checking");

        for (i = 0; i < num_src; i++) {
            opnd = instr_get_src(inst, i);

            if (instr_get_opcode(inst) == OP_lea)
                prop_opnd = propagate_address(opnd, &state);
            else {
                prop_opnd = propagate_opnd(opnd, &state,
                                           instr_get_prefix_flag(inst, PREFIX_DATA));
            }
            if (!opnd_same(opnd, prop_opnd)) {
#    ifdef DEBUG
                opt_stats_t.num_opnds_simplified++;
                d_r_logopnd(dcontext, 3, opnd, " old operand");
                d_r_logopnd(dcontext, 3, prop_opnd, " simplified to  ");
#    endif
                if (backup == NULL)
                    backup = instr_clone(dcontext, inst);
                instr_set_src(inst, i, prop_opnd);
            }
        }
        // propagate to dsts, just simplify addresses
        num_dst = instr_num_dsts(inst);
        for (i = 0; i < num_dst; i++) {
            opnd = instr_get_dst(inst, i);
            prop_opnd = propagate_address(opnd, &state);
            if (!opnd_same(opnd, prop_opnd)) {
#    ifdef DEBUG
                opt_stats_t.num_opnds_simplified++;
                d_r_logopnd(dcontext, 3, opnd, " old operand");
                d_r_logopnd(dcontext, 3, prop_opnd, " simplified to  ");
#    endif
                if (backup == NULL)
                    backup = instr_clone(dcontext, inst);
                instr_set_dst(inst, i, prop_opnd);
            }
        }

        /* if acutally propgated any info in, attempt to simplify */
        if (backup != NULL) {
            inst = prop_simplify(inst, &state);
            if (instr_is_encoding_possible(inst)) {
#    ifdef DEBUG
                opt_stats_t.num_instrs_simplified++;
                d_r_loginst(dcontext, 3, backup, " old instruction");
                d_r_loginst(dcontext, 3, inst, " simplified to  ");
#    endif
                instr_destroy(dcontext, backup);
                backup = NULL;
            } else {
#    ifdef DEBUG
                opt_stats_t.num_fail_simplified++;
                d_r_loginst(dcontext, 3, backup, " was unable to simplify ");
                d_r_loginst(dcontext, 3, inst, " to this ");
#    endif
                replace_inst(dcontext, trace, inst, backup);
                inst = backup;
                backup = NULL;
            }
        }

        // update prop state
        if (state.hint == NULL)
            inst = update_prop_state(&state, inst, true);
        else {
            d_r_loginst(
                dcontext, 3, state.hint,
                " using hint instruction instead of actual to update prop state ");
            update_prop_state(&state, state.hint, false);
            instr_destroy(dcontext, state.hint);
            state.hint = NULL;
        }
    }

#    ifdef DEBUG
    LOG(THREAD, LOG_OPTS, 3, "done constant prop\n");
    if (d_r_stats->loglevel >= 3 && (d_r_stats->logmask & LOG_OPTS) != 0)
        instrlist_disassemble(dcontext, tag, trace, THREAD);
#    endif
}

/***************************************************************************/
/* from Tim */
/* remove unnecsary zeroing */
/* removes unnecesary zeroing
 * This may not always be faster since
 * the pentium 4 hardware (and maby earlier versions too) recognizes
 * xor zeroring specially and uses it to break the false dependences
 *
 * ignores effects on flags, should perhaps consider them for correctness
 * but one would not expect a program to use the flags set zeroing a register
 * for a conditional jmp
 *
 * should also catch the adobe case where we for ex.
 * xor zero eax, load into ah, use eax, xor zero eax, load into ah ...
 *
 * relies to some degree on the enum in instr.h
 */
static void
remove_unnecessary_zeroing(dcontext_t *dcontext, app_pc tag, instrlist_t *trace)
{
    instr_t *inst, *next_inst;
    int i, cur_reg, num_dsts;
    /* keeps track if actually necessary to mark off dst of non zeroing instructions */
    bool check_dsts;
    bool zeroed[24];
    opnd_t dst;
    check_dsts = false;
    for (i = 0; i < 24; i++) {
        zeroed[i] = false;
    }
    for (inst = instrlist_first(trace); inst != NULL; inst = next_inst) {
        next_inst = instr_get_next(inst);
        if (is_zeroing_instr(inst)) {
            cur_reg = opnd_get_reg(instr_get_dst(inst, 0)) - REG_START_32;
            /* if zeroed (and also of all sub registers) then kill the inst
             * otherwise mark reg and sub regs as zeroed  */
            if (check_down(zeroed, cur_reg)) {
                /* is ok to remove instruction reg and subregs already zero */
#    ifdef DEBUG
                d_r_loginst(dcontext, 3, inst, "unnecsary xor removed ");
                opt_stats_t.xors_removed++;
#    endif
                remove_inst(dcontext, trace, inst);
            } else {
                propagate_down(zeroed, cur_reg, true);
                check_dsts = true;
            }
        } else {
            /* non-zeroing instruction, check for registers being written
             * and mark them non-zero if necessary */
            if (check_dsts) {
                num_dsts = instr_num_dsts(inst);
                for (i = 0; i < num_dsts; i++) {
                    dst = instr_get_dst(inst, i);
                    if (opnd_is_reg(dst)) {
                        cur_reg = opnd_get_reg(dst) - REG_START_32;
                        propagate_down(zeroed, cur_reg, false);
                    }
                }
                check_dsts = false;
                for (i = 0; i < 24; i++)
                    check_dsts |= zeroed[i];
            }
        }
    }
}

/****************************************************************************/
/* from Tim */
/* removes dead code
 * removes some nops (that use dead registers) but not all
 * relies to some degree on the ordering of enum reg in instr.h
 */

#    define NUM_ADD_CACHE 16
#    define ADD_KEEP 0x01
#    define ADD_DEAD 0x02

static int dc_global_aggr;
static int dc_local_aggr;

static void
add_address(dcontext_t *dcontext, int address, byte flag, int *adds, byte *flags)
{
    bool cont = true;
    int i;
    for (i = 0; (i < NUM_ADD_CACHE) && cont; i++) {
        if (adds[i] == 0) {
            adds[i] = address;
            flags[i] = flag;
            cont = false;
        }
    }
    if (cont) {
        LOG(THREAD, LOG_OPTS, 3, "constant address cache overflow\n");
        i = address % NUM_ADD_CACHE;
        adds[i] = address;
        flags[i] = flag;
    }
    LOG(THREAD, LOG_OPTS, 3, " load const cache add: " PFX "  flags 0x%02x\n", address,
        flag);
}

static bool
address_is_dead(dcontext_t *dcontext, int address, int *adds, byte *flags)
{
    int i = 0;
    for (; i < NUM_ADD_CACHE; i++)
        if (adds[i] == address && (flags[i] & ADD_DEAD) != 0)
            return true;
    return false;
}

static void
address_set_dead(dcontext_t *dcontext, int address, int *adds, byte *flags, bool dead)
{
    int i = 0;
    for (; i < NUM_ADD_CACHE; i++) {
        if (adds[i] == address) {
            if (dead)
                flags[i] |= ADD_DEAD;
            else
                flags[i] &= (~ADD_DEAD);
            return;
        }
    }
    if (dc_global_aggr > 2) {
        if (dead)
            add_address(dcontext, address, ADD_DEAD, adds, flags);
    }
}

static void
add_init(dcontext_t *dcontext, int *addresses, byte *flags)
{
    /* can add all dynamo addresses here, they are never aliased so always */
    /* safe to optimize, but takes up space in our cache, with new jump code */
    /* probably only use exc so just put it in, and maby eax to since is fav */
    /* when need to store flags/pass arg, can always add more location later */
    /* probably cleaner way of getting addresses but who cares for now */
    add_address(dcontext, opnd_get_disp(opnd_create_dcontext_field(dcontext, XCX_OFFSET)),
                ADD_KEEP, addresses, flags);
    add_address(dcontext, opnd_get_disp(opnd_create_dcontext_field(dcontext, XAX_OFFSET)),
                ADD_KEEP, addresses, flags);
}

#    if 0 /* not used */
static void
add_stack_address(dcontext_t *dcontext, int address, byte flag, int scope, int *adds, byte *flags, int *scopes)
{
    bool cont = true;
    int i;
    for (i = 0; (i < NUM_STACK_SLOTS) && cont; i++) {
        if (adds[i] == 0) {
            adds[i] = address;
            scopes[i] = scope;
            flags[i] = flag;
            cont = false;
        }
    }
    if (cont) {
        LOG(THREAD, LOG_OPTS, 3, "constant address cache overflow\n");
        i = address % NUM_STACK_SLOTS;
        adds[i] = address;
        flags[i] = flag;
        scopes[i] = scope;
    }
    LOG(THREAD, LOG_OPTS, 3, " stack const cache add: "PFX"  flags 0x%02x\n", address, flag);
}
#    endif

static bool
stack_address_is_dead(dcontext_t *dcontext, int address, int scope, int *adds,
                      byte *flags, int *scopes)
{
    int i;
    for (i = 0; i < NUM_STACK_SLOTS; i++) {
        if (adds[i] == address && scopes[i] == scope && (flags[i] & ADD_DEAD) != 0) {
            return true;
        }
    }
    return false;
}

static void
stack_address_set_dead(dcontext_t *dcontext, int address, int scope, int *adds,
                       byte *flags, int *scopes, bool dead)
{
    int i;
    for (i = 0; i < NUM_STACK_SLOTS; i++) {
        if (adds[i] == address && scopes[i] == scope && flags[i] != 0) {
            if (dead)
                flags[i] |= ADD_DEAD;
            else
                flags[i] &= (~ADD_DEAD);
            return;
        }
    }
}

void
remove_dead_code(dcontext_t *dcontext, app_pc tag, instrlist_t *trace)
{
    instr_t *inst, *prev_inst, *ecx_load;
    opnd_t dst, src;
    bool free[24];
    int addresses[NUM_ADD_CACHE];
    byte address_state[NUM_ADD_CACHE];

    int stack_scope[NUM_STACK_SLOTS];
    int stack_offsets_ebp[NUM_STACK_SLOTS];
    byte stack_state[NUM_STACK_SLOTS];
    int scope = 0; /* good as any default */

    int i, j, dst_reg, opcode, num_dsts, num_srcs, src_reg;
    uint eflags;
    bool killinst;
    bool kill_ecx_load = false;

    dc_global_aggr = dynamo_options.remove_dead_code % 10;
    dc_local_aggr = (dynamo_options.remove_dead_code - dc_global_aggr) / 10;
#    ifdef DEBUG
    LOG(THREAD, LOG_OPTS, 3,
        "removing dead loads, global aggressiveness %d local aggressiveness %d\n",
        dc_global_aggr, dc_local_aggr);
    if (d_r_stats->loglevel >= 4 && (d_r_stats->logmask & LOG_OPTS) != 0)
        instrlist_disassemble(dcontext, tag, trace, THREAD);
#    endif
    /* initialize */

    eflags = EFLAGS_READ_ALL;
    for (i = 0; i < 24; i++)
        free[i] = false;
    for (i = 0; i < NUM_ADD_CACHE; i++) {
        addresses[i] = 0;
        address_state[i] = 0;
    }
    for (i = 0; i < NUM_STACK_SLOTS; i++) {
        stack_offsets_ebp[i] = 0;
        stack_scope[i] = 0;
        stack_state[i] = 0;
    }
    add_init(dcontext, addresses, address_state);
    ecx_load = NULL;

    /* main loop runs from bottom of trace to top */
    for (inst = instrlist_last(trace); inst != NULL; inst = prev_inst) {
        prev_inst = instr_get_prev(inst);
        d_r_loginst(dcontext, 3, inst, "remove_dead_code working on:");
        if (instr_is_cti(inst) || instr_is_interrupt(inst)) {
            /* perhaps to a bit of multi-trace search here to see if really
             * necessary to mark all flags and regs as live when hit cti? */
            eflags = EFLAGS_READ_ALL;
            for (i = 0; i < 24; i++)
                free[i] = false;
            for (i = 0; i < NUM_ADD_CACHE; i++) {
                if ((address_state[i] & ADD_KEEP) != 0)
                    address_state[i] &= (~ADD_DEAD);
                else {
                    address_state[i] = 0;
                    addresses[i] = 0;
                }
            }
            for (i = 0; i < NUM_STACK_SLOTS; i++) {
                if ((stack_state[i] & ADD_KEEP) != 0)
                    stack_state[i] &= (~ADD_DEAD);
                else {
                    stack_state[i] = 0;
                    stack_offsets_ebp[i] = 0;
                    stack_scope[i] = 0;
                }
            }
            ecx_load = NULL;
            /* skip over the bit of control flow in indirect branch */
            if (instr_get_opcode(inst) == OP_jmp && prev_inst != NULL &&
                instr_get_prev(prev_inst) != NULL &&
                instr_get_opcode(instr_get_prev(inst)) == OP_jecxz)
                prev_inst = instr_get_prev(instr_get_prev(prev_inst));
        } else {
            // default to removing instruction, then see if we need it
            killinst = true;
            opcode = instr_get_opcode(inst);
            num_dsts = instr_num_dsts(inst);
            num_srcs = instr_num_srcs(inst);

            /* This track the scopes. The number indicates the depth of the
             * nested scopes. If it is 0, then we are in the original scope
             * and stack optimization can be done.
             */

            if (opcode == OP_leave ||
                (opcode == OP_pop && opnd_is_reg(instr_get_dst(inst, 0)) &&
                 opnd_get_reg(instr_get_dst(inst, 0)) == REG_EBP)) {
                scope++;
                LOG(THREAD, LOG_OPTS, 3, "cur scope + to %d\n", scope);
            } else {
                if (opcode == OP_enter ||
                    ((opcode == OP_mov_st || opcode == OP_mov_ld) &&
                     opnd_is_reg(instr_get_src(inst, 0)) &&
                     opnd_get_reg(instr_get_src(inst, 0)) == REG_ESP &&
                     opnd_is_reg(instr_get_dst(inst, 0)) &&
                     opnd_get_reg(instr_get_dst(inst, 0)) == REG_EBP)) {
                    scope--;
                    LOG(THREAD, LOG_OPTS, 3, "cur scope - to %d\n", scope);
                    for (i = 0; i < NUM_STACK_SLOTS; i++) {
                        if ((stack_scope[i] > scope) && (stack_state[i] != 0)) {
                            stack_state[i] = 0;
                        }
                    }
                } else {
                    if (instr_writes_to_reg(inst, REG_EBP, DR_QUERY_DEFAULT)) {
                        LOG(THREAD, LOG_OPTS, 2,
                            "dead code lost count of scope nesting, clearing cache\n");
                        for (i = 0; i < NUM_STACK_SLOTS; i++)
                            stack_state[i] = 0;
                    }
                }
            }

            /* only eliminate instructions that have a destination or are known to
             * eliminable */
            /* believe? that any instr with at least 1 dst has no other effects beside
             * that */
            /* dst and eflags.  Allow test, cmp, sahf to be killed */
            killinst = killinst &&
                !((num_dsts == 0) && (opcode != OP_sahf) && (opcode != OP_cmp) &&
                  (opcode != OP_test));
            /* check that all destinations are dead, (also not mem etc.) */
            for (i = 0; (i < num_dsts) && killinst; i++) {
                dst = instr_get_dst(inst, i);
                if (opnd_is_reg(dst)) {
                    dst_reg = opnd_get_reg(dst) - REG_START_32;
                    killinst = killinst && check_down(free, dst_reg);
                } else {
                    if (opnd_is_constant_address(dst)) {
                        if (ecx_load != NULL && is_store_to_ecxoff(dcontext, inst)) {
                            killinst = true;
                            if (kill_ecx_load) {
#    ifdef DEBUG
                                opt_stats_t.dead_loads_removed++;
                                d_r_loginst(dcontext, 3, ecx_load, "removed dead code ");
#    endif
                                remove_inst(dcontext, trace, ecx_load);
                            }
                        } else {
                            killinst = killinst &&
                                address_is_dead(dcontext, opnd_get_disp(dst), addresses,
                                                address_state);
                        }
                    } else {
                        if (opnd_is_stack_address(dst)) {
                            killinst = killinst &&
                                stack_address_is_dead(dcontext, opnd_get_disp(dst), scope,
                                                      stack_offsets_ebp, stack_state,
                                                      stack_scope);
                        } else {
                            killinst = false;
                        }
                    }
                }
            }
            /* check flags if might still be killable */
            killinst = killinst &&
                ((EFLAGS_WRITE_TO_READ(instr_get_eflags(inst, DR_QUERY_DEFAULT) &
                                       EFLAGS_WRITE_ALL) &
                  eflags) == 0);
            /* always kill if nop */
            killinst = killinst || is_nop(inst);
            /* check ecx load */
            if (is_load_from_ecxoff(dcontext, inst)) {
                ecx_load = inst;
                kill_ecx_load = !killinst;
            }
            if (killinst) {
                /* delete the instruction */
#    ifdef DEBUG
                opt_stats_t.dead_loads_removed++;
                d_r_loginst(dcontext, 3, inst, "removed dead code ");
#    endif
                remove_inst(dcontext, trace, inst);
            } else {
                /* can't be killed so add dependencies */
                /* add flag constraints */
                eflags &= ~EFLAGS_WRITE_TO_READ(instr_get_eflags(inst, DR_QUERY_DEFAULT) &
                                                EFLAGS_WRITE_ALL);
                eflags |= instr_get_eflags(inst, DR_QUERY_DEFAULT) & EFLAGS_READ_ALL;
                // mark destinations as free
                for (i = 0; i < num_dsts; i++) {
                    dst = instr_get_dst(inst, i);
                    if (opnd_is_reg(dst)) {
                        /* mark dst reg and sub regs as free */
                        dst_reg = opnd_get_reg(dst) - REG_START_32;
                        propagate_down(free, dst_reg, true);
                    } else {
                        if (opnd_is_constant_address(dst)) {
                            address_set_dead(dcontext, opnd_get_disp(dst), addresses,
                                             address_state, true);
                        } else {
                            if (opnd_is_stack_address(dst)) {
                                stack_address_set_dead(dcontext, opnd_get_disp(dst),
                                                       scope, stack_offsets_ebp,
                                                       stack_state, stack_scope, true);
                            }
                            /* reg used in address mark as unfree */
                            for (j = opnd_num_regs_used(dst) - 1; j >= 0; j--) {
                                dst_reg = opnd_get_reg_used(dst, j) - REG_START_32;
                                propagate_down(free, dst_reg, false);
                            }
                        }
                    }
                }
                // don't propagate srcs if zeroing instr
                // mark sources as needed
                if (!is_zeroing_instr(inst)) {
                    for (i = 0; i < num_srcs; i++) {
                        /* mark src regs and sub regs not free */
                        src = instr_get_src(inst, i);
                        if (opnd_is_constant_address(src)) {
                            address_set_dead(dcontext, opnd_get_disp(src), addresses,
                                             address_state, false);
                        } else {
                            if (opnd_is_stack_address(src)) {
                                stack_address_set_dead(dcontext, opnd_get_disp(src),
                                                       scope, stack_offsets_ebp,
                                                       stack_state, stack_scope, false);
                            }
                            for (j = opnd_num_regs_used(src) - 1; j >= 0; j--) {
                                src_reg = opnd_get_reg_used(src, j) - REG_START_32;
                                propagate_down(free, src_reg, false);
                            }
                        }
                    }
                }
            }
        }
    }
#    ifdef DEBUG
    LOG(THREAD, LOG_OPTS, 3, "done removing dead code\n");
    if (d_r_stats->loglevel >= 4 && (d_r_stats->logmask & LOG_OPTS) != 0)
        instrlist_disassemble(dcontext, tag, trace, THREAD);
#    endif
}

/***************************************************************************/
/* from Tim */
/* attempts to combine multiple adjustments of the esp register into a
 * single adjustment, might eventually be useful for locations, or as a
 * tool for inlining, but is inspired by ocaml and tinyvm code which have
 * a lot of manipulation of the stack without much actual use of it once
 * the other passes have finished
 *
 * Most esp manipulation is the result of inlined calls
 * in general have to worry about amount of space allocated on the stack etc.
 */

static bool
is_stack_adjustment(instr_t *inst)
{
    int opcode = instr_get_opcode(inst);
    return (((opcode == OP_add || opcode == OP_sub) &&
             opnd_is_reg(instr_get_dst(inst, 0)) &&
             opnd_get_reg(instr_get_dst(inst, 0)) == REG_ESP &&
             opnd_is_immed_int(instr_get_src(inst, 0))) ||

            (opcode == OP_lea && opnd_get_reg(instr_get_dst(inst, 0)) == REG_ESP &&
             ((opnd_get_base(instr_get_src(inst, 0)) == REG_ESP &&
               opnd_get_index(instr_get_src(inst, 0)) == REG_NULL) ||
              (opnd_get_base(instr_get_src(inst, 0)) == REG_NULL &&
               opnd_get_index(instr_get_src(inst, 0)) == REG_ESP &&
               opnd_get_scale(instr_get_src(inst, 0)) == 1))));
}

static int
get_stack_adjustment(instr_t *inst)
{
    int opcode = instr_get_opcode(inst);
    if (opcode == OP_add)
        return (int)opnd_get_immed_int(instr_get_src(inst, 0));
    if (opcode == OP_sub)
        return -(int)opnd_get_immed_int(instr_get_src(inst, 0));
    if (opcode == OP_lea)
        return opnd_get_disp(instr_get_src(inst, 0));
    else
        return -1;
}

static void
set_stack_adjustment(instr_t *inst, int adjust)
{
    int opcode = instr_get_opcode(inst);
    opnd_t temp_opnd;
    if (opcode == OP_lea) {
        instr_set_src(inst, 0,
                      opnd_create_base_disp(REG_ESP, REG_NULL, 0, adjust, OPSZ_lea));
        return;
    }
    if (opcode == OP_sub)
        adjust = -adjust;
    if (adjust < -128 || adjust > 127)
        temp_opnd = OPND_CREATE_INT32(adjust);
    else
        temp_opnd = OPND_CREATE_INT8(adjust);
    instr_set_src(inst, 0, temp_opnd);
}

static void
stack_adjust_combiner(dcontext_t *dcontext, app_pc tag, instrlist_t *trace)
{
    instr_t *inst, *next, *first_adjust, *last_adjust;
    int opcode, adj, max_off = 0, cur_off = 0, first_off = 0;
#    ifdef DEBUG
    LOG(THREAD, LOG_OPTS, 3, "combining stack adjustments\n");
    if (d_r_stats->loglevel >= 4 && (d_r_stats->logmask & LOG_OPTS) != 0)
        instrlist_disassemble(dcontext, tag, trace, THREAD);
#    endif
    last_adjust = NULL;
    first_adjust = NULL;
    for (inst = instrlist_first(trace); inst != NULL; inst = next) {
        next = instr_get_next(inst); /* in case we destroy inst */
#    ifdef DEBUG
        instr_decode(dcontext, inst);
        d_r_loginst(dcontext, 3, inst, "stack adjust considering");
#    endif
        if (first_adjust == NULL) {
            if (is_stack_adjustment(inst)) {
                first_adjust = inst;
                first_off = get_stack_adjustment(inst);
                cur_off = first_off;
                LOG(THREAD, LOG_OPTS, 3, "  found starting stack adjust, offset %d\n",
                    cur_off);
                max_off = 1000000; /* something large */
            }
        } else {
            /* see if we can fold in another adjustment */
            if (is_stack_adjustment(inst)) {
                adj = get_stack_adjustment(inst);
                cur_off += adj;
                LOG(THREAD, LOG_OPTS, 3,
                    "  found stack adjust adjust by: %d, current offset now: %d\n", adj,
                    cur_off);
                if (last_adjust != NULL) {
#    ifdef DEBUG
                    d_r_loginst(dcontext, 3, last_adjust,
                                "  removing old last adjustment");
                    opt_stats_t.num_stack_adjust_removed++;
#    endif
                    ASSERT(cur_off % 4 == 0);
                    remove_inst(dcontext, trace, last_adjust);
                    last_adjust = inst;
                } else {
                    last_adjust = inst;
                }
                continue;
            }
            opcode = instr_get_opcode(inst);
            /* if instr depends on ESP or leaves trace must restore */
            /* include interrupt and call, though may not be necessary */
            /* could mangle pushes and pops instead of restoring, is */
            /* helpfull?, check for store to ecx_off, might mangle indirect */
            /* macro's by inserting a clean up instruction */
            if (!instr_uses_reg(inst, REG_ESP) && !instr_is_cti(inst) &&
                !instr_is_interrupt(inst) && !instr_is_call(inst)) {
                /* skip writes to constant address, presume that they will never be stack
                 */
                if ((opcode == OP_mov_st || opcode == OP_mov_imm) &&
                    opnd_is_constant_address(instr_get_dst(inst, 0))) {
                    LOG(THREAD, LOG_OPTS, 3, "store to constant mem, skipping\n");
                    continue;
                }
                if (instr_writes_memory(inst)) {
                    /* could be write to the stack, since can't tell in general (aliases)
                     */
                    /* make sure if the cur_off is negative (allocating space on the
                     * stack) */
                    /* we don't eventually set the offset of the last adjust to reserve
                     * less */
                    /* space then that */
                    LOG(THREAD, LOG_OPTS, 3, "write to memory");
                    if (cur_off < max_off) {
                        LOG(THREAD, LOG_OPTS, 3,
                            "\ncurrent offset %d, less than max offset %d, setting max "
                            "offset to current offset\n",
                            cur_off, max_off);
                        max_off = cur_off;
                    }
                }
                LOG(THREAD, LOG_OPTS, 3, "skipping\n");
                continue;
            }
            /* fixing up */
            LOG(THREAD, LOG_OPTS, 3, "reached stopping point, clean up\n");
            if (max_off < cur_off) {
                LOG(THREAD, LOG_OPTS, 3,
                    "  max offset is less than current off set, set and fix\n");
                /* need to be sure to allocate enough space on stack at beginning */
                d_r_loginst(dcontext, 3, first_adjust, "  replacing initial adjustment");
                set_stack_adjustment(first_adjust, max_off);
                d_r_loginst(dcontext, 3, first_adjust, "  with");
                ASSERT(max_off % 4 == 0);
                /* protect eflags use lea */
                d_r_loginst(dcontext, 3, last_adjust, "  replacing last adjustment");
                set_stack_adjustment(last_adjust, cur_off - max_off);
                d_r_loginst(dcontext, 3, last_adjust, "  with");
                ASSERT((cur_off - max_off) % 4 == 0);
            } else {
                if (cur_off == 0) {
                    /* remove initial and last adjustment */
#    ifdef DEBUG
                    opt_stats_t.num_stack_adjust_removed++;
                    d_r_loginst(dcontext, 3, first_adjust,
                                "  curent offset = 0 removing initial adjustment");
                    if (last_adjust != NULL) {
                        opt_stats_t.num_stack_adjust_removed++;
                        d_r_loginst(dcontext, 3, last_adjust,
                                    "  curent offset = 0 removing last adjustment");
                    }
#    endif
                    remove_inst(dcontext, trace, first_adjust);
                    if (last_adjust != NULL)
                        remove_inst(dcontext, trace, last_adjust);
                } else {
                    /* change adjustment if necessary */
                    if (first_off != cur_off) {
                        LOG(THREAD, LOG_OPTS, 3,
                            "  current offset %d, initial offset %d\n", cur_off,
                            first_off);
                        d_r_loginst(dcontext, 3, first_adjust,
                                    "  replacing initial adjustment");
                        set_stack_adjustment(first_adjust, cur_off);
                        d_r_loginst(dcontext, 3, first_adjust, "  with");
                        ASSERT(cur_off % 4 == 0);
                    } else {
                        LOG(THREAD, LOG_OPTS, 3,
                            "  current offset = last offset = %d, no change needed\n",
                            cur_off);
                    }
                    /* remove last adjust */
                    if (last_adjust != NULL) {
#    ifdef DEBUG
                        opt_stats_t.num_stack_adjust_removed++;
                        d_r_loginst(dcontext, 3, last_adjust,
                                    "  removing last adjustment");
#    endif
                        remove_inst(dcontext, trace, last_adjust);
                    }
                }
            }
            last_adjust = NULL;
            first_adjust = NULL;
        }
    }
#    ifdef DEBUG
    LOG(THREAD, LOG_OPTS, 3, "done combining stack adjustments\n");
    if (d_r_stats->loglevel >= 3 && (d_r_stats->logmask & LOG_OPTS) != 0)
        instrlist_disassemble(dcontext, tag, trace, THREAD);
#    endif
}

/****************************************************************************/
/* call return matching, attempts to match calls with their corresponding
 * returns, may not always be safe
 */

/* checks to see if the eflags are written before they are read */
static bool
check_eflags_cr(instr_t *inst)
{
    uint eflags = EFLAGS_READ_6;
    uint inst_eflags;
    for (; inst != NULL; inst = instr_get_next(inst)) {
        if (instr_is_cti(inst) || instr_is_interrupt(inst))
            return false;
        inst_eflags = instr_get_eflags(inst, DR_QUERY_DEFAULT);
        if ((eflags & inst_eflags) != 0)
            return false;
        eflags &= ~(EFLAGS_WRITE_TO_READ(inst_eflags));
        if (eflags == 0)
            return true;
    }
    return false;
}

/* removes the return code, pattern matches on our return macro */
static instr_t *
remove_return_no_save_eflags(dcontext_t *dcontext, instrlist_t *trace, instr_t *inst)
{
    instr_t *inst2;
    int to_pop = 4;
    opnd_t replacement;
#    ifdef DEBUG
    opt_stats_t.num_returns_removed++;
    opt_stats_t.num_return_instrs_removed += 4;
#    endif

    inst2 = instr_get_next(inst);
    ASSERT(instr_get_opcode(inst) == OP_mov_st);
    d_r_loginst(dcontext, 3, inst, "removing");
    remove_inst(dcontext, trace, inst);
    inst = inst2;

    inst2 = instr_get_next(inst);
    ASSERT(instr_get_opcode(inst) == OP_pop);
    d_r_loginst(dcontext, 3, inst, "removing");
    remove_inst(dcontext, trace, inst);
    inst = inst2;

    inst2 = instr_get_next(inst);
    if (instr_get_opcode(inst) == OP_lea) {
        /* this popping of the stack, lea */
        to_pop += opnd_get_disp(instr_get_src(inst, 0));
        d_r_loginst(dcontext, 3, inst, "removing");
        remove_inst(dcontext, trace, inst);
        inst = inst2;
#    ifdef DEBUG
        opt_stats_t.num_return_instrs_removed++;
#    endif
    }

    inst2 = instr_get_next(inst);
    ASSERT(instr_get_opcode(inst) == OP_cmp);
    d_r_loginst(dcontext, 3, inst, "removing");
    remove_inst(dcontext, trace, inst);
    inst = inst2;

    inst2 = instr_get_next(inst);
    ASSERT(instr_get_opcode(inst) == OP_jne);
    d_r_loginst(dcontext, 3, inst, "removing");
    remove_inst(dcontext, trace, inst);
    inst = inst2;

    inst2 = instr_get_next(inst);
    ASSERT(instr_get_opcode(inst) == OP_mov_ld);
    d_r_loginst(dcontext, 3, inst, "removing");
    remove_inst(dcontext, trace, inst);
    inst = inst2;

    /* check for add here, is not uncommon to pop off the args after a return, if so */
    /* can save an instruction */
    if ((instr_get_opcode(inst) == OP_add) && opnd_is_reg(instr_get_dst(inst, 0)) &&
        (opnd_get_reg(instr_get_dst(inst, 0)) == REG_ESP) &&
        opnd_is_immed_int(instr_get_src(inst, 0))) {
        to_pop += (int)opnd_get_immed_int(instr_get_src(inst, 0));
#    ifdef DEBUG
        opt_stats_t.num_return_instrs_removed++;
#    endif
        if (to_pop == 0) {
#    ifdef DEBUG
            opt_stats_t.num_return_instrs_removed++;
#    endif
            return inst;
        }
        if ((to_pop <= 127) && (to_pop >= -128)) {
            replacement = OPND_CREATE_INT8(to_pop);
        } else {
            replacement = OPND_CREATE_INT32(to_pop);
        }
        d_r_loginst(dcontext, 3, inst, " updating stack adjustment :");
        instr_set_src(inst, 0, replacement);
        d_r_loginst(dcontext, 3, inst, " to :");
        return inst;
    }
    if ((to_pop <= 127) && (to_pop >= -128)) {
        replacement = OPND_CREATE_INT8(to_pop);
    } else {
        replacement = OPND_CREATE_INT32(to_pop);
    }
    inst2 = INSTR_CREATE_add(dcontext, opnd_create_reg(REG_ESP), replacement);
    d_r_loginst(dcontext, 3, inst2, "adjusting stack");
    instrlist_preinsert(trace, inst, inst2);
    return inst2;
}

/* removes the return code, pattern matches on our return macro */
static instr_t *
remove_return(dcontext_t *dcontext, instrlist_t *trace, instr_t *inst)
{
    if (!INTERNAL_OPTION(unsafe_ignore_eflags_trace)) {
        instr_t *inst2;
        int to_pop = 4;
        opnd_t replacement;
#    ifdef DEBUG
        opt_stats_t.num_returns_removed++;
        opt_stats_t.num_return_instrs_removed += 6;
#    endif

        inst2 = instr_get_next(inst);
        ASSERT(instr_get_opcode(inst) == OP_mov_st);
        d_r_loginst(dcontext, 3, inst, "removing");
        remove_inst(dcontext, trace, inst);
        inst = inst2;

        inst2 = instr_get_next(inst);
        ASSERT(instr_get_opcode(inst) == OP_pop);
        d_r_loginst(dcontext, 3, inst, "removing");
        remove_inst(dcontext, trace, inst);
        inst = inst2;

        inst2 = instr_get_next(inst);
        if (instr_get_opcode(inst2) == OP_lea) {
            /* this popping of the stack, lea */
            to_pop += opnd_get_disp(instr_get_src(inst, 0));
            d_r_loginst(dcontext, 3, inst, "removing");
            remove_inst(dcontext, trace, inst);
            inst = inst2;
#    ifdef DEBUG
            opt_stats_t.num_return_instrs_removed++;
#    endif
        }

        inst2 = instr_get_next(inst);
        ASSERT(instr_get_opcode(inst) == OP_lea);
        d_r_loginst(dcontext, 3, inst, "removing");
        remove_inst(dcontext, trace, inst);
        inst = inst2;

        inst2 = instr_get_next(inst);
        ASSERT(instr_get_opcode(inst) == OP_jecxz);
        d_r_loginst(dcontext, 3, inst, "removing");
        remove_inst(dcontext, trace, inst);
        inst = inst2;

        inst2 = instr_get_next(inst);
        ASSERT(instr_get_opcode(inst) == OP_lea);
        d_r_loginst(dcontext, 3, inst, "removing");
        remove_inst(dcontext, trace, inst);
        inst = inst2;

        inst2 = instr_get_next(inst);
        ASSERT(instr_get_opcode(inst) == OP_jmp);
        d_r_loginst(dcontext, 3, inst, "removing");
        remove_inst(dcontext, trace, inst);
        inst = inst2;

        inst2 = instr_get_next(inst);
        ASSERT(instr_get_opcode(inst) == OP_mov_ld);
        d_r_loginst(dcontext, 3, inst, "removing");
        remove_inst(dcontext, trace, inst);
        inst = inst2;

        /* check for add here, is not uncommon to pop off the args after a return,
         * if so  can save an instruction */
        if ((instr_get_opcode(inst) == OP_add) && opnd_is_reg(instr_get_dst(inst, 0)) &&
            (opnd_get_reg(instr_get_dst(inst, 0)) == REG_ESP) &&
            opnd_is_immed_int(instr_get_src(inst, 0))) {
            to_pop += (int)opnd_get_immed_int(instr_get_src(inst, 0));
#    ifdef DEBUG
            opt_stats_t.num_return_instrs_removed++;
#    endif
            if (to_pop == 0) {
#    ifdef DEBUG
                opt_stats_t.num_return_instrs_removed++;
#    endif
                return inst;
            }
            if ((to_pop <= 127) && (to_pop >= -128)) {
                replacement = OPND_CREATE_INT8(to_pop);
            } else {
                replacement = OPND_CREATE_INT32(to_pop);
            }
            d_r_loginst(dcontext, 3, inst, " updating stack adjustment :");
            instr_set_src(inst, 0, replacement);
            d_r_loginst(dcontext, 3, inst, " to :");
            return inst;
        }
        if (check_eflags_cr(inst)) {
            if ((to_pop <= 127) && (to_pop >= -128)) {
                replacement = OPND_CREATE_INT8(to_pop);
            } else {
                replacement = OPND_CREATE_INT32(to_pop);
            }
            inst2 = INSTR_CREATE_add(dcontext, opnd_create_reg(REG_ESP), replacement);
        } else {
            LOG(THREAD, LOG_OPTS, 3,
                "Forward eflags check failed using lea to adjust stack instead of add");
            inst2 = INSTR_CREATE_lea(
                dcontext, opnd_create_reg(REG_ESP),
                opnd_create_base_disp(REG_ESP, REG_NULL, 0, to_pop, OPSZ_lea));
        }
        d_r_loginst(dcontext, 3, inst2, "adjusting stack");
        instrlist_preinsert(trace, inst, inst2);
        return inst2;
    } else
        return remove_return_no_save_eflags(dcontext, trace, inst);
}

/* for some reason can't get instr_same / opnd_same to work for the below so
 *  just write it out, returns true if the next instruction is likely the pop
 *  of a return */
static bool
is_return(dcontext_t *dcontext, instr_t *inst)
{
    instr_t *pop = instr_get_next(inst);
    if (!INTERNAL_OPTION(unsafe_ignore_eflags_trace)) {
        instr_t *lea, *jecxz;
        if (pop == NULL)
            return false;
        lea = instr_get_next(pop);
        if (lea == NULL)
            return false;
        jecxz = instr_get_next(lea);
        if (jecxz == NULL)
            return false;
        if (instr_get_opcode(jecxz) == OP_lea)
            jecxz = instr_get_next(jecxz);
        return (jecxz != NULL && instr_get_opcode(pop) == OP_pop &&
                opnd_get_reg(instr_get_dst(pop, 0)) == REG_ECX &&
                instr_get_opcode(jecxz) == OP_jecxz);
    } else {
        instr_t *lea, *cmp, *jne;
        if (pop == NULL)
            return false;
        lea = instr_get_next(pop);
        if (lea == NULL)
            return false;
        if (instr_get_opcode(lea) != OP_lea && instr_get_opcode(lea) != OP_cmp)
            return false;
        if (instr_get_opcode(lea) == OP_cmp)
            cmp = lea;
        else
            cmp = instr_get_next(lea);
        if (cmp == NULL)
            return false;
        jne = instr_get_next(cmp);
        return (jne != NULL && instr_get_opcode(pop) == OP_pop &&
                opnd_get_reg(instr_get_dst(pop, 0)) == REG_ECX &&
                instr_get_opcode(cmp) == OP_cmp && instr_get_opcode(jne) == OP_jne);
    }
}

/* checks to see if the address pushed by the push instruction matches the
 * address in the cmp following the pop */
static bool
check_return(dcontext_t *dcontext, instr_t *inst, instr_t *push)
{
    if (!INTERNAL_OPTION(unsafe_ignore_eflags_trace)) {
        instr_t *lea = instr_get_next(inst);
        opnd_t check;
        if (instr_get_opcode(lea) != OP_lea)
            lea = inst;
        check = instr_get_src(lea, 0);
        d_r_logopnd(dcontext, 3, check, "check opnd");
        return (
            opnd_is_near_base_disp(check) &&
            (opnd_get_disp(check) == -(int)opnd_get_immed_int(instr_get_src(push, 0))));
    } else {
        instr_t *cmp = instr_get_next(inst);
        opnd_t check;
        if (instr_get_opcode(cmp) != OP_cmp)
            cmp = inst;
        check = instr_get_src(cmp, 1);
        d_r_logopnd(dcontext, 3, check, "check opnd");
        return (opnd_is_immed_int(check) &&
                opnd_get_immed_int(check) == opnd_get_immed_int(instr_get_src(push, 0)));
    }
}

#    define CALL_RETURN_STACK_SIZE 40

/* attempts to match calls with returns for the purpose of removing the return check
 * instructions */
static void
call_return_matching(dcontext_t *dcontext, app_pc tag, instrlist_t *trace)
{
    instr_t *a[CALL_RETURN_STACK_SIZE];
    instr_t *inst, *next_inst;
    int top, i, opcode;
    top = 0;
#    ifdef DEBUG
    LOG(THREAD, LOG_OPTS, 3, "starting call return matching\n");
    if (d_r_stats->loglevel >= 3 && (d_r_stats->logmask & LOG_OPTS) != 0)
        instrlist_disassemble(dcontext, tag, trace, THREAD);
#    endif
    for (inst = instrlist_first(trace); inst != NULL; inst = next_inst) {
        next_inst = instr_get_next(inst);
        d_r_loginst(dcontext, 3, inst, "checking");
        // look for push_imm from call and add to state
        if (next_inst != NULL) {
            opcode = instr_get_opcode(next_inst);
            if ((opcode == OP_push_imm) &&
                (opnd_get_size(instr_get_src(next_inst, 0)) == OPSZ_4)) {
                inst = next_inst;
                d_r_loginst(dcontext, 3, inst, "found call push");
                if (top < CALL_RETURN_STACK_SIZE) {
                    a[top] = inst;
                    top++;
                } else {
                    LOG(THREAD, LOG_OPTS, 1, "call return matching stack overflow\n");
                    for (i = 1; i < CALL_RETURN_STACK_SIZE; i++) {
                        a[i - 1] = a[i];
                    }
                    a[top - 1] = inst;
                }
            }
        }
        // look for pop from return and remove instruction is possible
        if (is_return(dcontext, inst)) {
            d_r_loginst(dcontext, 3, inst, "found start of return");
            while ((top > 0) &&
                   (!check_return(dcontext, instr_get_next(instr_get_next(inst)),
                                  a[top - 1]))) {
                top--;
                d_r_loginst(dcontext, 3, a[top],
                            "ignoring probable non call push immed on call return stack");
            }
            if (top > 0) {
                d_r_loginst(dcontext, 3, a[top - 1], "corresponding push was");
                LOG(THREAD, LOG_OPTS, 3, "attempting to remove return code\n");
                next_inst = remove_return(dcontext, trace, inst);
                top--;
            } else {
                LOG(THREAD, LOG_OPTS, 3, "call return stack underflow\n");
            }
        }
    }
#    ifdef DEBUG
    LOG(THREAD, LOG_OPTS, 3, "done call return matching\n");
    if (d_r_stats->loglevel >= 3 && (d_r_stats->logmask & LOG_OPTS) != 0)
        instrlist_disassemble(dcontext, tag, trace, THREAD);
#    endif
}

/****************************************************************************/

/* peephole driver
 * so we only walk instrlist once
 * current opts:
 *   p4 only: inc/dec -> add 1/sub 1
 *   leave -> mov ebp,esp; pop ebp
 */
static void
peephole_optimize(dcontext_t *dcontext, app_pc tag, instrlist_t *trace)
{
    bool p4 = (proc_get_family() == FAMILY_PENTIUM_4);
    instr_t *inst, *next_inst;
    int opcode;
    LOG(THREAD, LOG_OPTS, 3, "peephole_optimize\n");
    for (inst = instrlist_first(trace); inst != NULL; inst = next_inst) {
        next_inst = instr_get_next(inst);
        opcode = instr_get_opcode(inst);
        if (p4 && (opcode == OP_inc || opcode == OP_dec)) {
#    ifdef DEBUG
            opt_stats_t.incs_examined++;
            if (replace_inc_with_add(dcontext, inst, trace))
                opt_stats_t.incs_replaced++;
#    else
            replace_inc_with_add(dcontext, inst, trace);
#    endif
        } else if (opcode == OP_leave) {
            /* on Pentium II and later, complex instructions like
             * enter and leave are slower (though smaller) than
             * their simpler components.
             *     leave
             *     =>
             *     movl %ebp,%esp
             *     popl %ebp
             * this makes a difference on microbenchmarks, doesn't
             * seem to show up on spec though
             */
            instrlist_preinsert(trace, inst,
                                INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_ESP),
                                                    opnd_create_reg(REG_EBP)));
            instrlist_preinsert(trace, inst,
                                INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_EBP)));
            instrlist_remove(trace, inst);
            instr_destroy(dcontext, inst);
        }
    }
}

/* replaces inc with add 1, dec with sub 1
 * if cannot replace (eflags constraints), leaves original instruction alone
 * returns true if successful, false if not
 */
static bool
replace_inc_with_add(dcontext_t *dcontext, instr_t *inst, instrlist_t *trace)
{
    instr_t *in;
    uint eflags;
    int opcode = instr_get_opcode(inst);
    bool ok_to_replace = false;

    ASSERT(opcode == OP_inc || opcode == OP_dec);
    LOG(THREAD, LOG_OPTS, 3, "replace_inc_with_add\n");

    /* add/sub writes CF, inc/dec does not, make sure that's ok */
    for (in = inst; in != NULL; in = instr_get_next(in)) {
        eflags = instr_get_eflags(in, DR_QUERY_DEFAULT);
        if ((eflags & EFLAGS_READ_CF) != 0) {
            d_r_loginst(dcontext, 3, in, "reads CF => cannot replace inc with add");
            return false;
        }
        /* if writes but doesn't read, ok */
        if ((eflags & EFLAGS_WRITE_CF) != 0) {
            ok_to_replace = true;
            break;
        }
        /* test is down here b/c we want to look at 1st exit
         * if direct branch, look at top of target
         * N.B.: indirect branches: we'll hit lahf first, which reads CF,
         *   which will stop us from replacing, which is what we want
         */
        if (instr_is_exit_cti(in)) {
            /* FIXME: what if branch is never taken and points to
             * bogus memory, or we walk beyond interrupt or some
             * non-cti that we normally stop at?
             */
            byte *target;
            int noncti_eflags;
            instr_t tinst;
            if (!opnd_is_near_pc(instr_get_target(in)))
                break;
            target = (byte *)opnd_get_pc(instr_get_target(in));
            d_r_loginst(dcontext, 3, in, "looking at target");
            instr_init(dcontext, &tinst);
            do {
                instr_reset(dcontext, &tinst);
                target = decode_cti(dcontext, target, &tinst);
                ASSERT(instr_valid(&tinst));
                noncti_eflags = instr_get_eflags(&tinst, DR_QUERY_DEFAULT);
                if ((noncti_eflags & EFLAGS_READ_CF) != 0) {
                    d_r_loginst(dcontext, 3, in,
                                "reads CF => cannot replace inc with add");
                    return false;
                }
                /* if writes but doesn't read, ok */
                if ((noncti_eflags & EFLAGS_WRITE_CF) != 0) {
                    ok_to_replace = true;
                    break;
                }
                /* stop at 1st cti, don't try to recurse,
                 * common case should hit a writer of CF
                 * prior to 1st cti
                 */
            } while (!instr_is_cti(&tinst));
            instr_free(dcontext, &tinst);
            /* for cbr, exit and fall-through must be ok */
            if (!instr_is_cbr(in))
                break;
            else /* continue for fall-through */
                ok_to_replace = false;
        }
    }
    if (!ok_to_replace) {
        LOG(THREAD, LOG_OPTS, 3, "no write to CF => cannot replace inc with add\n");
        return false;
    }
    if (opcode == OP_inc) {
        LOG(THREAD, LOG_OPTS, 3, "replacing inc with add\n");
        in = INSTR_CREATE_add(dcontext, instr_get_dst(inst, 0), OPND_CREATE_INT8(1));
    } else {
        LOG(THREAD, LOG_OPTS, 3, "replacing dec with sub\n");
        in = INSTR_CREATE_sub(dcontext, instr_get_dst(inst, 0), OPND_CREATE_INT8(1));
    }
    instr_set_prefixes(in, instr_get_prefixes(inst));
    replace_inst(dcontext, trace, inst, in);
    return true;
}

/****************************************************************************/
/* josh's load removal optimization */
#    define MAX_DIST 40
void
remove_redundant_loads(dcontext_t *dcontext, app_pc tag, instrlist_t *trace)
{
    instr_t *instr, *next_inst, *first_mem_access, *reg_write_checker;
    opnd_t mem_read, orig_reg_opnd = { 0 }; /* FIXME: check */
    reg_id_t orig_reg;
    int dist;
    uint ctis;
    bool removed_from_store = false;
    LOG(THREAD, LOG_OPTS, 3, "entering remove_loads optimization\n");

    for (instr = instrlist_first(trace); instr != NULL; instr = next_inst) {
        next_inst = instr_get_next(instr);

        // ensures that it is an instruction which reads memory
        if (instr_reads_memory(instr)) {
#    ifdef DEBUG
            opt_stats_t.loads_examined++;
#    endif
            if (instr_get_opcode(instr) == OP_mov_ld) {
                mem_read = instr_get_src_mem_access(instr);
            } else if (instr_get_opcode(instr) == OP_add) {
                mem_read = instr_get_src_mem_access(instr);
                if (opnd_same(mem_read, instr_get_dst(instr, 0))) {
                    continue;
                }
            } else {
                continue;
            }
        } else {
            continue;
        }

        /* to simply things for debugging, just worry about cases where the read
           is indirect off the base pointer. this should be removed later */
        if (opnd_get_base(mem_read) != REG_EBP || opnd_get_index(mem_read) != REG_NULL)
            continue;
        LOG(THREAD, LOG_OPTS, 3, "\n");
        d_r_loginst(dcontext, 3, instr, " reads memory, try to eliminate. ");

        // walk backwards to try to find where the memory was written
        for (dist = 0, first_mem_access = instr_get_prev(instr);
             ((dist < MAX_DIST) && (first_mem_access != NULL));
             first_mem_access = instr_get_prev(first_mem_access), dist++) {

            d_r_loginst(dcontext, 3, first_mem_access, "mem_writer walking backwards");

            // if the instr writes to ebp (eventually, any register)
            if (instruction_affects_mem_access(first_mem_access, mem_read)) {
                d_r_loginst(dcontext, 3, first_mem_access,
                            "this instr. probably writes to ebp");
                first_mem_access = NULL;
                break;
            }
            // if its a move that writes to same memory location
            else if (instr_writes_memory(first_mem_access)) {
                opnd_t writeopnd = instr_get_dst(first_mem_access, 0);

                // takes care of push/pop. should i make this if (!pushorpop?)
                if (opnd_is_memory_reference(writeopnd)) {
                    d_r_loginst(dcontext, 3, first_mem_access,
                                "this instr writes to memory");
                    if (opnd_same_address(writeopnd, mem_read)) {

                        // if the writing instruction was a store, then its OK

                        if (instr_get_opcode(first_mem_access) == OP_mov_st) {
                            d_r_loginst(dcontext, 3, first_mem_access,
                                        "this instr writes to same location");
                            orig_reg_opnd = instr_get_src(first_mem_access, 0);
                            removed_from_store = true;
                            if (!opnd_is_reg(orig_reg_opnd)) {
                                d_r_loginst(
                                    dcontext, 3, first_mem_access,
                                    "source isn't a register. can't optimize for now");
                                first_mem_access = NULL;
                            }
                            break;
                        } else { // an add or something else wrote to memory
                            d_r_loginst(
                                dcontext, 3, first_mem_access,
                                "this non-store accesses memory, stop optimization");
                            first_mem_access = NULL;

                            break;
                        }
                    }
                    // check alignment of address (for partial or unaligned writes)
                    // still might be issue with aligned partial writes (data size)
                    // attempted quick fix for gcc bug for CGO paper 1/10/03 Tim
                    // if size/alignment issues really are to blame then a more through
                    // look at the optimization should be made
                    else if (opnd_is_near_base_disp(writeopnd) &&
                             opnd_is_near_base_disp(mem_read) &&
                             (opnd_get_base(mem_read) == opnd_get_base(writeopnd)) &&
                             (opnd_get_index(mem_read) == opnd_get_index(writeopnd))) {
                        int scratch = opnd_get_disp(mem_read) - opnd_get_disp(writeopnd);
                        if ((scratch < 4) && (scratch > -4)) {
                            first_mem_access = NULL;
                            break;
                        }
                    } // end quick fix

                    if (!safe_write(first_mem_access)) {
                        d_r_loginst(dcontext, 3, first_mem_access,
                                    "unsafe write, killing optmization");
                        first_mem_access = NULL;

                        break;
                    }
                }
            }
            // check if the move reads from that location

            else if (instr_reads_memory(first_mem_access)) {
                opnd_t readopnd = instr_get_src(first_mem_access, 0);

                // takes care of push/pop. should i make this if (!pushorpop?)
                if (opnd_is_memory_reference(readopnd)) {
                    d_r_loginst(dcontext, 3, first_mem_access,
                                "this instr reads from memory");
                    if (opnd_same_address(readopnd, mem_read)) {
                        // if the writing instruction was a store, then its OK
                        if (instr_get_opcode(first_mem_access) == OP_mov_ld) {
                            d_r_loginst(dcontext, 3, first_mem_access,
                                        "this instr reads from the same location");
                            orig_reg_opnd = instr_get_dst(first_mem_access, 0);
                            removed_from_store = false;
                            if (!opnd_is_reg(orig_reg_opnd)) {
                                d_r_loginst(
                                    dcontext, 3, first_mem_access,
                                    "dest. isn't a register. can't optimize for now");
                                ASSERT(0);
                                first_mem_access = NULL;
                            }

                            break;
                        }
                    }
                }
            }
        }
        /* if it reached top of trace, or something wrote to ebp */
        if (!first_mem_access) {
            LOG(THREAD, LOG_OPTS, 3,
                "reached top of trace, or an add or other non-move wrote to memory\n");
            continue;
        }
        if (dist >= MAX_DIST) {
            LOG(THREAD, LOG_OPTS, 3, "passed MAX_DIST threshold of %d\n", MAX_DIST);
            continue;
        }

        ASSERT(instr_num_dsts(first_mem_access) == 1 &&
               instr_num_srcs(first_mem_access) == 1);

        ctis = 0; // for stats of how many ctis are traversed
        orig_reg = opnd_get_reg(orig_reg_opnd);
        LOG(THREAD, LOG_OPTS, 3, "original register=%s\n", reg_names[orig_reg]);

        // check here to see if anything overwrites the register holding the value
        for (reg_write_checker = instr_get_next(first_mem_access);
             reg_write_checker != instr;
             reg_write_checker = instr_get_next(reg_write_checker)) {

            d_r_loginst(dcontext, 3, reg_write_checker, "walking forward");
            if (instr_is_cti(reg_write_checker)) {
                d_r_loginst(dcontext, 3, reg_write_checker,
                            "holy shit, load-removal across basic blocks!");
                ctis++;
            }

            // checks if something overwrites the register

            if (instr_writes_to_reg(reg_write_checker, orig_reg, DR_QUERY_DEFAULT)) {
#    ifdef DEBUG
                opt_stats_t.reg_overwritten++;
#    endif
                d_r_loginst(dcontext, 3, reg_write_checker,
                            "original register was overwritten");
                break;
            }
        }

        if (!opnd_is_reg_32bit(orig_reg_opnd) ||
            !opnd_is_reg_32bit(instr_get_dst(instr, 0)))
            continue;

        if (reg_write_checker == instr) {
            /* if reg_write_checker reached instr, then nothing overwrites the
               register, its OK to do optimization */

            // replace load with register read
            bool ok = instr_replace_src_opnd(instr, mem_read, orig_reg_opnd);
            ASSERT(ok);

            // after optimization, got move w/ same src as dst, so remove the instruction
            if (((instr_get_opcode(instr) == OP_mov_st) ||
                 (instr_get_opcode(instr) == OP_mov_ld)) &&
                (opnd_same(instr_get_src(instr, 0), instr_get_dst(instr, 0)))) {
                LOG(THREAD, LOG_OPTS, 3,
                    "replacing mem access with %s made src==dst. removing instr\n",
                    reg_names[orig_reg]);
                instrlist_remove(trace, instr);
                instr_destroy(dcontext, instr);
            } else if (!instr_is_encoding_possible(instr)) {
                d_r_loginst(dcontext, 3, instr,
                            "encoding not possible ;( reverting to orig. instr\n");
                ok = instr_replace_src_opnd(instr, opnd_create_reg(orig_reg), mem_read);
                ASSERT(ok);
            } else {
                // update stats
#    ifdef DEBUG
                opt_stats_t.ctis_in_load_removal += ctis;
#    endif
                if (removed_from_store) {
#    ifdef DEBUG
                    opt_stats_t.loads_removed_from_stores++;
#    endif
                    d_r_loginst(dcontext, 3, instr,
                                "replaced original instr with val from store");
                } else {
#    ifdef DEBUG
                    opt_stats_t.loads_removed_from_loads++;
#    endif
                    d_r_loginst(dcontext, 3, instr,
                                "replaced original instr with val from load");
                }
            }
        } else {
            // the register was overwritten. try to use some other register to hold the
            // value
            reg_id_t dead_reg;
            opnd_t dead_reg_opnd;
            instr_t *copy_to_dead_instr;
            dead_reg = find_dead_register_across_instrs(first_mem_access, instr);
            if (dead_reg != REG_NULL) {

                dead_reg_opnd = opnd_create_reg(dead_reg);
                LOG(THREAD, LOG_OPTS, 3,
                    "looks like %s is free to hold the reg though!\n",
                    reg_names[dead_reg]);
                copy_to_dead_instr =
                    INSTR_CREATE_mov_ld(dcontext, dead_reg_opnd, orig_reg_opnd);
                instrlist_postinsert(trace, first_mem_access, copy_to_dead_instr);
                d_r_loginst(dcontext, 3, copy_to_dead_instr,
                            "inserted this to save val. in dead register");

                instr_replace_src_opnd(instr, mem_read, dead_reg_opnd);
                d_r_loginst(dcontext, 3, instr,
                            "modified this instr to use the new dead register");
#    ifdef DEBUG
                opt_stats_t.val_saved_in_dead_reg++;
                opt_stats_t.ctis_in_load_removal += ctis;
                if (removed_from_store)
                    opt_stats_t.loads_removed_from_stores++;
                else
                    opt_stats_t.loads_removed_from_loads++;
#    endif
            }
        }
    }
    LOG(THREAD, LOG_OPTS, 3, "leaving remove_loads optimization\n");
}

static reg_id_t
find_dead_register_across_instrs(instr_t *start, instr_t *end)
{
    reg_id_t a;
    instr_t *instr = NULL;

    for (a = REG_EAX; a <= REG_EDI; a++) {
        if (is_dead_register(a, start)) {
            for (instr = start; instr != end; instr = instr_get_next(instr)) {
                if (instr_uses_reg(instr, a))
                    break;
            }
        }
        if (instr == end)
            return a;
    }
    return REG_NULL;
}

#    if 0  /* not used */
static int
find_dead_register_across_instrs_H(instr_t *start,instr_t *end)
{

    //This is a huristics. Find the longest dead register. If its more than half a trace - it's good for me.

    int a;
    int reg=REG_NULL;
    instr_t *instr = NULL;
    int reg_dead_count[REG_EDI-REG_EAX+1];
    int count=0;
    int max=0;

    int num_of_instr=0;

    for (instr=start;instr!=end;instr=instr_get_next(instr)) {
        num_of_instr++;
    }


    for (a=REG_EAX;a<=REG_EDI;a++) {
        count=0;
        if (is_dead_register(a,start)) {
            for (instr=start;instr!=end;instr=instr_get_next(instr)) {
                count++;
                if (instr_uses_reg(instr,a)) {
                    reg_dead_count[a]=count;
                    if (count>max) {
                        max=count;
                        reg=a;
                    }
                    break;
                }
            }
        }
        if (instr==end)
            return a;
    }
    if (max>=num_of_instr/2)
        return reg;

    return REG_NULL;
}
#    endif /* #if 0 */

/****************************************************************************/
/* prefetching */

#    define MIN_PREFETCH_DISTANCE 3

static void
prefetch_optimize_trace(dcontext_t *dcontext, app_pc tag, instrlist_t *trace)
{
    instr_t *instr, *loopinstr, *tracewalker, *prefetchinstr;
    opnd_t src_mem_access;
    int distance;

    bool insertprefetch;

    loopinstr = find_next_self_loop(dcontext, tag, instrlist_first(trace));
    if (loopinstr == NULL) {
        return;
    }

    instr = instrlist_first(trace);
    while (instr != NULL) {

        if (instr_reads_memory(instr) && instr_get_opcode(instr) != OP_prefetchnta) {

            src_mem_access = instr_get_src_mem_access(instr);

            // only prefetch if the load is register-indirect
            if (!(opnd_get_base(src_mem_access) || opnd_get_index(src_mem_access)))
                break;

            prefetchinstr = INSTR_CREATE_prefetchnta(
                dcontext,
                opnd_create_base_disp(opnd_get_base(src_mem_access),
                                      opnd_get_index(src_mem_access),
                                      opnd_get_scale(src_mem_access),
                                      opnd_get_disp(src_mem_access), OPSZ_1));

            insertprefetch = true;
            tracewalker = instr->prev; // start walk before instruction to prefetch for
            distance = 0;
            while (1) {
                distance++;
                // if beginning of trace is reached, go back to loop spot
                if (tracewalker == NULL)
                    tracewalker = loopinstr->prev;

                // reached conflicting instruction
                if (instruction_affects_mem_access(tracewalker, src_mem_access))
                    break;

                // came across same prefetch instruction already
                if (instr_same(prefetchinstr, tracewalker)) {
                    insertprefetch = false;
                    break;
                }

                // looped completely around or load is after the loop instr
                if ((tracewalker == instr) || (tracewalker == loopinstr)) {
                    insertprefetch = false;
                    break;
                }
                tracewalker = tracewalker->prev;
            }

            if (insertprefetch &&
                (distance < MIN_PREFETCH_DISTANCE || (tracewalker->next == instr)))
                insertprefetch = false;

            if (insertprefetch) {
#    ifdef DEBUG
                LOG(THREAD, LOG_OPTS, 3, "in trace " PFX " inserting prefetch for:", tag);
                if (d_r_stats->loglevel >= 3)
                    instr_disassemble(dcontext, instr, THREAD);
                LOG(THREAD, LOG_OPTS, 3, " after instruction: ");
                if (d_r_stats->loglevel >= 3)
                    instr_disassemble(dcontext, tracewalker, THREAD);
                LOG(THREAD, LOG_OPTS, 3, "\n");
#    endif
                instrlist_postinsert(trace, tracewalker, prefetchinstr);
            } else
                instr_destroy(dcontext, prefetchinstr);
        }
        instr = instr_get_next(instr);
    }
}

/* Removed josh's attempt at using the SSE2 xmm registers to hold some local vars -
   you can find it in the attic optimize.c 1.95

   The -spill_xmm optimization never speeded anything up - probably because on a
   P4 xmm<->reg operations were three times more expensive than a
   cache hit in memory, on a Pentium M the cost ratio may be reversed
   and the optimization may be worth keeping in mind.
   Of course, transparency and memory aliasing problems don't make it very appealing.
*/

/****************************************************************************/
/* utility routines */

bool
is_store_to_ecxoff(dcontext_t *dcontext, instr_t *inst)
{
    int opcode = instr_get_opcode(inst);
    return ((opcode == OP_mov_imm || opcode == OP_mov_st) &&
            opnd_is_near_base_disp(instr_get_dst(inst, 0)) &&
            opnd_get_disp(instr_get_dst(inst, 0)) ==
                opnd_get_disp(opnd_create_dcontext_field(dcontext, XCX_OFFSET)));
}

bool
is_load_from_ecxoff(dcontext_t *dcontext, instr_t *inst)
{
    return (instr_get_opcode(inst) == OP_mov_ld &&
            opnd_is_near_base_disp(instr_get_src(inst, 0)) &&
            opnd_get_disp(instr_get_src(inst, 0)) ==
                opnd_get_disp(opnd_create_dcontext_field(dcontext, XCX_OFFSET)));
}

/* returns true if the opnd is a constant address
 * i.e. is memory access with null base and index registers */
bool
opnd_is_constant_address(opnd_t address)
{
    return (opnd_is_abs_addr(address) IF_X64(|| opnd_is_rel_addr(address)));
}

/* checks to see if the instr zeros a reg (and does nothing else) */
static bool
is_zeroing_instr(instr_t *inst)
{
    int opcode = instr_get_opcode(inst);
    if (((opcode == OP_xor) || (opcode == OP_pxor) || (opcode == OP_sub)) &&
        opnd_same(instr_get_src(inst, 0), instr_get_src(inst, 1)))
        return true;
    return false;
}

static bool
is_dead_register(reg_id_t reg, instr_t *where)
{
    // something tells me its a bad call to mess with these...
    if (reg == REG_EBP || reg == REG_ESP)
        return false;

    while (!instr_is_cti(where)) {
        if (instr_reg_in_src(where, reg))
            return false;
        else if (instr_writes_to_reg(where, reg, DR_QUERY_DEFAULT))
            return true;
        //! instr_writes_to_reg(...).  probably writing to mem indirectly through reg
        else if (instr_reg_in_dst(where, reg))
            return false;

        where = instr_get_next(where);
    }
    return false;
}

#    if 0 /* not used */
static int
find_dead_register(instr_t *where)
{
    int a;
    for (a=REG_EAX;a<=REG_EDI;a++) {
        if (is_dead_register(a,where))
            return a;
    }
    return REG_NULL;
}

/* checks to see if, starting at inst, the value in reg
 * is dead (is written to before it is next read),
 * could later recurse into other traces/fragments but right now
 * assumes everything is live at a CTI */
static bool
is_dead_reg(dcontext_t *dcontext, instr_t *start_inst, int reg)
{
    instr_t *inst, *next_inst;
    if (reg == REG_NULL)
        return true;
    for (inst = start_inst; inst != NULL; inst=next_inst) {
        next_inst = instr_get_next(inst);
        if (instr_is_cti(inst))
            return false;
        if (instr_reg_in_src(inst, reg))
            return false;
        if (instr_writes_to_reg(inst, reg))
            return true;
        else if (instr_reg_in_dst(inst, reg))
            return false;
    }
    /* in case something goes wrong */
    return false;
}
#    endif

/* replaces old with new and destroys old inst */
void
replace_inst(dcontext_t *dcontext, instrlist_t *ilist, instr_t *old, instr_t *new)
{
    instrlist_preinsert(ilist, old, new);
    instrlist_remove(ilist, old);
    instr_destroy(dcontext, old);
}

/* removes and deystroys inst */
void
remove_inst(dcontext_t *dcontext, instrlist_t *ilist, instr_t *inst)
{
    instrlist_remove(ilist, inst);
    instr_destroy(dcontext, inst);
}

/* checks if instr writes to any registers that mem_access depends on */
static bool
instruction_affects_mem_access(instr_t *instr, opnd_t mem_access)
{
    reg_id_t base, index;
    ASSERT(instr);
    base = opnd_get_base(mem_access);
    if (base != REG_NULL && instr_writes_to_reg(instr, base, DR_QUERY_DEFAULT))
        return true;
    index = opnd_get_index(mem_access);
    if (index != REG_NULL && instr_writes_to_reg(instr, index, DR_QUERY_DEFAULT))
        return true;

    return false;
}

/* a simplistic check to see if a write could overwrite some arbitrary place*/
static bool
safe_write(instr_t *mem_writer)
{
    opnd_t mem_write = instr_get_dst(mem_writer, 0);

    if (!opnd_is_base_disp(mem_write))
        return true;

    else if (opnd_get_base(mem_write) ==
             REG_NULL) // if there's no base, its prob. a constant mem addr
        return true;

    else if (opnd_get_base(mem_write) != REG_EBP || opnd_get_index(mem_write) != REG_NULL)
        return false;

    return true;
}

opnd_t
instr_get_src_mem_access(instr_t *instr)
{
    int a;
    opnd_t curop;

    for (a = 0; a < instr_num_srcs(instr); a++) {
        curop = instr_get_src(instr, a);
        if (opnd_is_memory_reference(curop)) {
            return curop;
        }
    }
    ASSERT_NOT_REACHED();
    curop.kind = NULL_kind;
    return curop;
}

instr_t *
find_next_self_loop(dcontext_t *dcontext, app_pc tag, instr_t *instr)
{
    app_pc target;

    while (instr != NULL) {
        if (instr_is_cbr(instr) || instr_is_ubr(instr)) {
            target = opnd_get_pc(instr_get_target(instr));
            if (target == tag) {
                return instr;
            }
        }

        instr = instr_get_next(instr);
    }
    return NULL;
}

void
replace_self_loop_with_instr(dcontext_t *dcontext, app_pc tag, instrlist_t *trace,
                             instr_t *desiredtargetinstr)
{
    instr_t *top = instrlist_first(trace), *in;
    opnd_t targetop;

    in = top;

    LOG(THREAD, LOG_OPTS, 3,
        "entering replace_self_loop_with_instr looking for tag " PFX ".\n", tag);
    while (in != NULL) {
        if (instr_is_cbr(in) || instr_is_ubr(in)) {
            targetop = instr_get_target(in);
            if (opnd_is_near_pc(targetop) && opnd_get_pc(targetop) == tag) {
                d_r_loginst(dcontext, 3, in, "self_loop (pc target==tag) fixing in");
                instr_set_target(in, opnd_create_instr(desiredtargetinstr));
            } else if (opnd_is_near_instr(targetop) && opnd_get_instr(targetop) == top) {
                d_r_loginst(dcontext, 3, in, "self_loop (inter traget==top)fixing in");
                d_r_logopnd(dcontext, 3, opnd_create_instr(desiredtargetinstr),
                            "self_loop in now points to");
                instr_set_target(in, opnd_create_instr(desiredtargetinstr));
            }
        }
        in = instr_get_next(in);
    }
}

/* given a cbr, finds the previous instr that writes the flag the cbr reads */
static instr_t *
get_decision_instr(instr_t *jmp)
{
    instr_t *inst;
    uint flag_tested = EFLAGS_READ_TO_WRITE(instr_get_eflags(jmp, DR_QUERY_DEFAULT));
    uint eflags;

    ASSERT(instr_is_cbr(jmp));
    inst = instr_get_prev(jmp);
    while (inst != NULL) {
        eflags = instr_get_eflags(inst, DR_QUERY_DEFAULT);
        if ((eflags & flag_tested) != 0) {
            return inst;
        }
        inst = instr_get_prev(inst);
    }
    return NULL;
}

static void
propagate_down(bool *reg_rep, int index, bool value)
{
    if ((index >= 0) && (index < 24)) {
        reg_rep[index] = value;
        if (index < 12) {
            reg_rep[index + 8] = value;
            if (index < 4) {
                reg_rep[index + 16] = value;
                reg_rep[index + 20] = value;
            } else if (index >= 8)
                reg_rep[index + 12] = value;
        }
    }
}

static bool
check_down(bool *reg_rep, int index)
{
    return ((index >= 0) && (index < 24) && reg_rep[index] &&
            ((index >= 12) ||
             (reg_rep[index + 8] && ((index < 8) || reg_rep[index + 12]) &&
              ((index >= 4) || (reg_rep[index + 16] && reg_rep[index + 20])))));
}

/* return true, if this instr is a nop, of one of a class of nops */
/* does not check for all types of nops, since there are many */
/* these seem to be the most common */
static bool
is_nop(instr_t *inst)
{
    int opcode = instr_get_opcode(inst);
    if (opcode == OP_nop)
        return true;
    if ((opcode == OP_mov_ld || opcode == OP_mov_st || opcode == OP_xchg) &&
        opnd_same(instr_get_src(inst, 0), instr_get_dst(inst, 0)))
        return true;
    if (opcode == OP_lea && opnd_get_disp(instr_get_src(inst, 0)) == 0 &&
        ((opnd_get_base(instr_get_src(inst, 0)) == opnd_get_reg(instr_get_dst(inst, 0)) &&
          opnd_get_index(instr_get_src(inst, 0)) == REG_NULL) ||
         (opnd_get_index(instr_get_src(inst, 0)) ==
              opnd_get_reg(instr_get_dst(inst, 0)) &&
          opnd_get_base(instr_get_src(inst, 0)) == REG_NULL &&
          opnd_get_scale(instr_get_src(inst, 0)) == 1)))
        return true;
    // other cases
    return false;
}

#endif /* INTERNAL around whole file */
