/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
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

/* file "instrlist.c" */

#include "../globals.h"
#include "instrlist.h"
#include "instr.h"
#include "decode.h"
#include "arch.h"

#ifdef DEBUG
/* case 10450: give messages to clients */
#    undef ASSERT /* N.B.: if have issues w/ DYNAMO_OPTION, re-instate */
#    undef ASSERT_TRUNCATE
#    undef ASSERT_BITFIELD_TRUNCATE
#    undef ASSERT_NOT_REACHED
#    define ASSERT DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#    define ASSERT_TRUNCATE DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#    define ASSERT_BITFIELD_TRUNCATE DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#    define ASSERT_NOT_REACHED DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#endif

/* returns an empty instrlist_t object */
instrlist_t *
instrlist_create(void *drcontext)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instrlist_t *ilist =
        (instrlist_t *)heap_alloc(dcontext, sizeof(instrlist_t) HEAPACCT(ACCT_IR));
    CLIENT_ASSERT(ilist != NULL, "instrlist_create: allocation error");
    instrlist_init(ilist);
    return ilist;
}

/* initializes an instrlist_t object */
void
instrlist_init(instrlist_t *ilist)
{
    CLIENT_ASSERT(ilist != NULL, "instrlist_create: NULL parameter");
    ilist->first = ilist->last = NULL;
    ilist->flags = 0; /* no flags set */
    ilist->translation_target = NULL;
    ilist->fall_through_bb = NULL;
#ifdef ARM
    ilist->auto_pred = DR_PRED_NONE;
#endif
}

/* frees the instrlist_t object */
void
instrlist_destroy(void *drcontext, instrlist_t *ilist)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(ilist->first == NULL && ilist->last == NULL,
                  "instrlist_destroy: list not empty");
    heap_free(dcontext, ilist, sizeof(instrlist_t) HEAPACCT(ACCT_IR));
}

/* frees the Instrs in the instrlist_t */
void
instrlist_clear(void *drcontext, instrlist_t *ilist)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
#ifdef ARM
    /* XXX i#4680: Reset encode state to avoid dangling pointers. */
    if (instrlist_first(ilist) != NULL &&
        instr_get_isa_mode(instrlist_first(ilist)) == DR_ISA_ARM_THUMB)
        encode_reset_it_block(dcontext);
#endif
    instr_t *instr;
    while (NULL != (instr = instrlist_first(ilist))) {
        instrlist_remove(ilist, instr);
        instr_destroy(dcontext, instr);
    }
}

/* frees the Instrs in the instrlist_t and the instrlist_t object itself */
void
instrlist_clear_and_destroy(void *drcontext, instrlist_t *ilist)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instrlist_clear(dcontext, ilist);
    instrlist_destroy(dcontext, ilist);
}

/* Specifies the fall-through target of a basic block if its last
 * instruction is a conditional branch instruction.
 * It can only be called in basic block building event callbacks
 * and has NO EFFECT in other cases, e.g. trace.
 */
bool
instrlist_set_fall_through_target(instrlist_t *bb, app_pc tgt)
{
    bb->fall_through_bb = tgt;
    return true;
}

app_pc
instrlist_get_fall_through_target(instrlist_t *bb)
{
    return bb->fall_through_bb;
}

/* Specifies the return target of a basic block if its last
 * instruction is a call instruction.
 * It can only be called in basic block building event callbacks
 * and has NO EFFECT in other cases.
 */
bool
instrlist_set_return_target(instrlist_t *bb, app_pc tgt)
{
    bb->fall_through_bb = tgt;
    return true;
}

app_pc
instrlist_get_return_target(instrlist_t *bb)
{
    return bb->fall_through_bb;
}

/* All future Instrs inserted into ilist that do not have raw bits
 * will have instr_set_translation called with pc as the target
 */
void
instrlist_set_translation_target(instrlist_t *ilist, app_pc pc)
{
    ilist->translation_target = pc;
}

app_pc
instrlist_get_translation_target(instrlist_t *ilist)
{
    return ilist->translation_target;
}

void
instrlist_set_our_mangling(instrlist_t *ilist, bool ours)
{
    if (ours)
        ilist->flags |= INSTR_OUR_MANGLING;
    else
        ilist->flags &= ~INSTR_OUR_MANGLING;
}

void
instrlist_set_auto_predicate(instrlist_t *ilist, dr_pred_type_t pred)
{
#ifdef ARM
    ilist->auto_pred = pred;
#endif
}

dr_pred_type_t
instrlist_get_auto_predicate(instrlist_t *ilist)
{
#ifdef ARM
    return ilist->auto_pred;
#else
    return DR_PRED_NONE;
#endif
}

bool
instrlist_get_our_mangling(instrlist_t *ilist)
{
    return TEST(INSTR_OUR_MANGLING, ilist->flags);
}

/* returns the first inst in the list */
instr_t *
instrlist_first(instrlist_t *ilist)
{
    return ilist->first;
}

/* returns the first app (non-meta) inst in the list */
instr_t *
instrlist_first_app(instrlist_t *ilist)
{
    instr_t *first = ilist->first;

    if (first == NULL)
        return NULL;
    if (instr_is_app(first))
        return first;

    return instr_get_next_app(first);
}

instr_t *
instrlist_first_nonlabel(instrlist_t *ilist)
{
    instr_t *first = ilist->first;
    while (first != NULL && instr_is_label(first))
        first = instr_get_next(first);
    return first;
}

/* returns the last inst in the list */
instr_t *
instrlist_last(instrlist_t *ilist)
{
    return ilist->last;
}

instr_t *
instrlist_last_app(instrlist_t *ilist)
{
    instr_t *last = ilist->last;
    if (last == NULL)
        return NULL;
    if (instr_is_app(last))
        return last;
    return instr_get_prev_app(last);
}

void
instrlist_cut(instrlist_t *ilist, instr_t *cut_point)
{
    CLIENT_ASSERT(cut_point != NULL, "instrlist_cut: instr cut point should not be NULL");
    instr_t *last_instr = instr_get_prev(cut_point);
    if (last_instr != NULL)
        instr_set_next(last_instr, NULL);
    instr_set_prev(cut_point, NULL);
    ilist->last = last_instr;
}

static inline void
check_translation(instrlist_t *ilist, instr_t *inst)
{
    if (ilist->translation_target != NULL && instr_get_translation(inst) == NULL) {
        instr_set_translation(inst, ilist->translation_target);
    }
    if (instrlist_get_our_mangling(ilist))
        instr_set_our_mangling(inst, true);
#ifdef ARM
    if (instr_is_meta(inst)) {
        dr_pred_type_t auto_pred = ilist->auto_pred;
        if (instr_predicate_is_cond(auto_pred)) {
            CLIENT_ASSERT(!instr_is_cti(inst), "auto-predication does not support cti's");
            CLIENT_ASSERT(
                !TESTANY(EFLAGS_WRITE_NZCV,
                         instr_get_arith_flags(inst, DR_QUERY_INCLUDE_COND_SRCS)),
                "cannot auto predicate a meta-inst that writes to NZCV");
            if (!instr_is_predicated(inst))
                instr_set_predicate(inst, auto_pred);
        }
    }
#endif
}

/* appends inst to the list ("inst" can be a chain of insts) */
void
instrlist_append(instrlist_t *ilist, instr_t *inst)
{
    instr_t *top = inst;
    instr_t *bot;

    CLIENT_ASSERT(instr_get_prev(inst) == NULL,
                  "instrlist_append: cannot add middle of list");
    check_translation(ilist, inst);
    while (instr_get_next(inst)) {
        inst = instr_get_next(inst);
        check_translation(ilist, inst);
    }
    bot = inst;
    if (ilist->last) {
        instr_set_next(ilist->last, top);
        instr_set_prev(top, ilist->last);
        ilist->last = bot;
    } else {
        ilist->first = top;
        ilist->last = bot;
    }
}

/* prepends inst to the list ("inst" can be a chain of insts) */
void
instrlist_prepend(instrlist_t *ilist, instr_t *inst)
{
    instr_t *top = inst;
    instr_t *bot;

    CLIENT_ASSERT(instr_get_prev(inst) == NULL,
                  "instrlist_prepend: cannot add middle of list");
    check_translation(ilist, inst);
    while (instr_get_next(inst)) {
        inst = instr_get_next(inst);
        check_translation(ilist, inst);
    }
    bot = inst;
    if (ilist->first) {
        instr_set_next(bot, ilist->first);
        instr_set_prev(ilist->first, bot);
        ilist->first = top;
    } else {
        ilist->first = top;
        ilist->last = bot;
    }
}

/* inserts "inst" before "where" ("inst" can be a chain of insts) */
void
instrlist_preinsert(instrlist_t *ilist, instr_t *where, instr_t *inst)
{
    instr_t *whereprev;
    instr_t *top = inst;
    instr_t *bot;

    if (where == NULL) {
        /* if where is NULL there is no inst to send for a "before" */
        instrlist_append(ilist, inst);
        return;
    }

    CLIENT_ASSERT(where != NULL, "instrlist_preinsert: where cannot be NULL");
    CLIENT_ASSERT(instr_get_prev(inst) == NULL,
                  "instrlist_preinsert: cannot add middle of list");
    whereprev = instr_get_prev(where);
    check_translation(ilist, inst);
    while (instr_get_next(inst)) {
        inst = instr_get_next(inst);
        check_translation(ilist, inst);
    }
    bot = inst;
    if (whereprev) {
        instr_set_next(whereprev, top);
        instr_set_prev(top, whereprev);
    } else {
        ilist->first = top;
    }
    instr_set_next(bot, where);
    instr_set_prev(where, bot);
}

/* inserts "inst" after "where" ("inst" can be a chain of insts) */
void
instrlist_postinsert(instrlist_t *ilist, instr_t *where, instr_t *inst)
{
    instr_t *wherenext;
    instr_t *top = inst;
    instr_t *bot;

    if (where == NULL) {
        /* if where is NULL there is no inst to send for an "after" */
        instrlist_prepend(ilist, inst);
        return;
    }

    CLIENT_ASSERT(where != NULL, "instrlist_postinsert: where cannot be NULL");
    CLIENT_ASSERT(instr_get_prev(inst) == NULL,
                  "instrlist_postinsert: cannot add middle of list");

    wherenext = instr_get_next(where);
    check_translation(ilist, inst);
    while (instr_get_next(inst)) {
        inst = instr_get_next(inst);
        check_translation(ilist, inst);
    }
    bot = inst;
    instr_set_next(where, top);
    instr_set_prev(top, where);
    if (wherenext) {
        instr_set_next(bot, wherenext);
        instr_set_prev(wherenext, bot);
    } else {
        ilist->last = bot;
    }
}

/* replace oldinst with newinst, remove oldinst from ilist, and return oldinst
   (newinst can be a chain of insts) */
instr_t *
instrlist_replace(instrlist_t *ilist, instr_t *oldinst, instr_t *newinst)
{
    instr_t *where;

    CLIENT_ASSERT(oldinst != NULL, "instrlist_replace: oldinst cannot be NULL");
    CLIENT_ASSERT(instr_get_prev(newinst) == NULL,
                  "instrlist_replace: cannot add middle of list");
    where = instr_get_prev(oldinst);
    instrlist_remove(ilist, oldinst);
    if (where)
        instrlist_postinsert(ilist, where, newinst);
    else
        instrlist_prepend(ilist, newinst);

    return oldinst;
}

/* removes "inst" from the instrlist_t it currently belongs to */
void
instrlist_remove(instrlist_t *ilist, instr_t *inst)
{
    if (instr_get_prev(inst))
        instr_set_next(instr_get_prev(inst), instr_get_next(inst));
    else
        ilist->first = instr_get_next(inst);

    if (instr_get_next(inst))
        instr_set_prev(instr_get_next(inst), instr_get_prev(inst));
    else
        ilist->last = instr_get_prev(inst);

    instr_set_prev(inst, NULL);
    instr_set_next(inst, NULL);
}

instrlist_t *
instrlist_clone(void *drcontext, instrlist_t *old)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *inst, *copy;
    instrlist_t *newlist = instrlist_create(dcontext);

    inst = instrlist_first(old);
    while (inst != NULL) {
        copy = instr_clone(dcontext, inst);
        /* to copy instr targets we temporarily clobber note field */
        instr_set_note(inst, (void *)copy);
        instrlist_append(newlist, copy);
        inst = instr_get_next(inst);
    }

    /* Fix up instr src if it is an instr and restore note field */
    /* Note: we do not allows instruction update code cache,
     * which is very dangerous.
     * So we do not support instr as dst opnd and won't fix up here if any.
     */
    for (inst = instrlist_first(old), copy = instrlist_first(newlist);
         inst != NULL && copy != NULL;
         inst = instr_get_next(inst), copy = instr_get_next(copy)) {
        int i;
        for (i = 0; i < inst->num_srcs; i++) {
            instr_t *tgt;
            opnd_t op = instr_get_src(copy, i);
            if (!opnd_is_instr(op))
                continue;
            CLIENT_ASSERT(opnd_get_instr(op) != NULL,
                          "instrlist_clone: NULL instr operand");
            tgt = (instr_t *)instr_get_note(opnd_get_instr(op));
            CLIENT_ASSERT(tgt != NULL, "instrlist_clone: operand instr not in instrlist");
            if (opnd_is_far_instr(op)) {
                instr_set_src(copy, i,
                              opnd_create_far_instr(opnd_get_segment_selector(op), tgt));
            } else
                instr_set_src(copy, i, opnd_create_instr(tgt));
        }
    }
    for (inst = instrlist_first(old), copy = instrlist_first(newlist);
         inst != NULL && copy != NULL;
         inst = instr_get_next(inst), copy = instr_get_next(copy)) {
        /* restore note field */
        instr_set_note(inst, instr_get_note(copy));
    }
    newlist->fall_through_bb = old->fall_through_bb;
    return newlist;
}

/*
 * puts a whole list (prependee) onto the front of ilist
 * frees prependee when done because it will contain nothing useful
 */

void
instrlist_prepend_instrlist(dcontext_t *dcontext, instrlist_t *ilist,
                            instrlist_t *prependee)
{
    instr_t *first = instrlist_first(prependee);
    if (!first)
        return;
    instrlist_prepend(ilist, first);
    instrlist_init(prependee);
    instrlist_destroy(dcontext, prependee);
}

void
instrlist_append_instrlist(dcontext_t *dcontext, instrlist_t *ilist,
                           instrlist_t *appendee)
{
    instr_t *first = instrlist_first(appendee);
    if (!first)
        return;
    instrlist_append(ilist, first);
    instrlist_init(appendee);
    instrlist_destroy(dcontext, appendee);
}

/* If has_instr_jmp_targets is true, this routine trashes the note field
 * of each instr_t to store the offset in order to properly encode
 * the relative pc for an instr_t jump target
 */
byte *
instrlist_encode_to_copy(void *drcontext, instrlist_t *ilist, byte *copy_pc,
                         byte *final_pc, byte *max_pc, bool has_instr_jmp_targets)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    instr_t *inst;
    size_t len = 0;
#ifdef ARM
    /* XXX i#1734: reset encode state to avoid any stale encode state
     * or dangling pointer.
     */
    if (instr_get_isa_mode(instrlist_first(ilist)) == DR_ISA_ARM_THUMB)
        encode_reset_it_block(dcontext);
#endif
    /* Do an extra pass over the instrlist so we can determine if an instr opnd
     * was erroneously used with has_instr_jmp_targets = false.
     */
    DOCHECK(2, {
        if (!has_instr_jmp_targets) {
            for (inst = instrlist_first(ilist); inst; inst = instr_get_next(inst)) {
                if (TEST(INSTR_OPERANDS_VALID, (inst)->flags)) {
                    int i;
                    for (i = 0; i < instr_num_srcs(inst); ++i) {
                        CLIENT_ASSERT(!opnd_is_instr(instr_get_src(inst, i)),
                                      "has_instr_jmp_targets was unset "
                                      "but an instr opnd was found");
                    }
                }
            }
        }
    });
    if (has_instr_jmp_targets || max_pc != NULL) {
        /* Must first compute offset and total length. */
        for (inst = instrlist_first(ilist); inst; inst = instr_get_next(inst)) {
            if (has_instr_jmp_targets)
                inst->offset = len;
            len += instr_length(dcontext, inst);
        }
    }
    if (max_pc != NULL &&
        (copy_pc + len > max_pc || POINTER_OVERFLOW_ON_ADD(copy_pc, len)))
        return NULL;
    for (inst = instrlist_first(ilist); inst != NULL; inst = instr_get_next(inst)) {
        byte *pc = instr_encode_to_copy(dcontext, inst, copy_pc, final_pc);
        if (pc == NULL)
            return NULL;
        final_pc += pc - copy_pc;
        copy_pc = pc;
    }
    return copy_pc;
}

byte *
instrlist_encode(void *drcontext, instrlist_t *ilist, byte *pc,
                 bool has_instr_jmp_targets)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    return instrlist_encode_to_copy(dcontext, ilist, pc, pc, NULL, has_instr_jmp_targets);
}

DR_API
/* Inserts inst as a non-application instruction into ilist prior to "where" */
void
instrlist_meta_preinsert(instrlist_t *ilist, instr_t *where, instr_t *inst)
{
    instr_set_meta(inst);
    instrlist_preinsert(ilist, where, inst);
}

DR_API
/* Inserts inst as a non-application instruction into ilist after "where" */
void
instrlist_meta_postinsert(instrlist_t *ilist, instr_t *where, instr_t *inst)
{
    instr_set_meta(inst);
    instrlist_postinsert(ilist, where, inst);
}

DR_API
/* Inserts inst as a non-application instruction onto the end of ilist */
void
instrlist_meta_append(instrlist_t *ilist, instr_t *inst)
{
    instr_set_meta(inst);
    instrlist_append(ilist, inst);
}

DR_API
/* Create instructions for storing pointer-size integer val to dst,
 * and then insert them into ilist prior to where.
 * The "first" and "last" created instructions are returned.
 */
void
instrlist_insert_mov_immed_ptrsz(void *drcontext, ptr_int_t val, opnd_t dst,
                                 instrlist_t *ilist, instr_t *where, OUT instr_t **first,
                                 OUT instr_t **last)
{
    CLIENT_ASSERT(opnd_get_size(dst) == OPSZ_PTR, "wrong dst size");
    insert_mov_immed_ptrsz((dcontext_t *)drcontext, val, dst, ilist, where, first, last);
}

DR_API
/* Create instructions for pushing pointer-size integer val on the stack,
 * and then insert them into ilist prior to where.
 * The "first" and "last" created instructions are returned.
 */
void
instrlist_insert_push_immed_ptrsz(void *drcontext, ptr_int_t val, instrlist_t *ilist,
                                  instr_t *where, OUT instr_t **first, OUT instr_t **last)
{
    insert_push_immed_ptrsz((dcontext_t *)drcontext, val, ilist, where, first, last);
}

DR_API
void
instrlist_insert_mov_instr_addr(void *drcontext, instr_t *src_inst, byte *encode_pc,
                                opnd_t dst, instrlist_t *ilist, instr_t *where,
                                OUT instr_t **first, OUT instr_t **last)
{
    CLIENT_ASSERT(opnd_get_size(dst) == OPSZ_PTR, "wrong dst size");
    if (encode_pc == NULL) {
#ifdef STANDALONE_DECODER
        /* Maybe we should fail? */
#else
        /* Pass highest code cache address.
         * XXX: unless we're beyond the reservation!  Would still be reachable
         * from rest of vmcode, but might be higher than vmcode_get_end()!
         */
        encode_pc = vmcode_get_end();
#endif
    }
    insert_mov_instr_addr((dcontext_t *)drcontext, src_inst, encode_pc, dst, ilist, where,
                          first, last);
}

DR_API
void
instrlist_insert_push_instr_addr(void *drcontext, instr_t *src_inst, byte *encode_pc,
                                 instrlist_t *ilist, instr_t *where, OUT instr_t **first,
                                 OUT instr_t **last)
{
    if (encode_pc == NULL) {
#ifdef STANDALONE_DECODER
        /* Maybe we should fail? */
#else
        /* Pass highest code cache address.
         * XXX: unless we're beyond the reservation!  Would still be reachable
         * from rest of vmcode, but might be higher than vmcode_get_end()!
         */
        encode_pc = vmcode_get_end();
#endif
    }
    insert_push_instr_addr((dcontext_t *)drcontext, src_inst, encode_pc, ilist, where,
                           first, last);
}
