/* **********************************************************
 * Copyright (c) 2000-2008 VMware, Inc.  All rights reserved.
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

/* file "instrlist.h" */

#ifndef _INSTRLIST_H_
#define _INSTRLIST_H_ 1

struct _instr_list_t {
    instr_t *first;
    instr_t *last;
    int flags;
    app_pc translation_target;
#ifdef CLIENT_INTERFACE
    /* i#620: provide API for setting fall-throught/return target in bb */
    /* XXX: can this be unioned with traslation_target for saving space?
     * looks no, as traslation_target will be used in mangle and trace,
     * which conflicts with our checks in trace and return address mangling. 
     * XXX: There are several possible ways to implement i#620, for example,
     * adding a dr_register_bb_event() OUT param. 
     * However, we do here to avoid breaking backward compatibility
     */
    app_pc fall_through_bb;
#endif /* CLIENT_INTERFACE */
}; /* instrlist_t */

/* DR_API EXPORT TOFILE dr_ir_instrlist.h */
/* DR_API EXPORT BEGIN */
/****************************************************************************
 * INSTRLIST ROUTINES
 */
/**
 * @file dr_ir_instrlist.h
 * @brief Functions to create and manipulate lists of instructions.
 */
/* DR_API EXPORT END */

/* basic instrlist_t functions */

DR_API
/** Returns an initialized instrlist_t allocated on the thread-local heap. */
instrlist_t*
instrlist_create(dcontext_t *dcontext);

DR_API
/** Initializes \p ilist. */
void 
instrlist_init(instrlist_t *ilist);

DR_API
/** Deallocates the thread-local heap storage for \p ilist. */
void 
instrlist_destroy(dcontext_t *dcontext, instrlist_t *ilist);

DR_API
/** Frees the instructions in \p ilist. */
void
instrlist_clear(dcontext_t *dcontext, instrlist_t *ilist);

DR_API
/** Destroys the instructions in \p ilist and destroys the instrlist_t object itself. */
void
instrlist_clear_and_destroy(dcontext_t *dcontext, instrlist_t *ilist);

DR_API
/** 
 * All future instructions inserted into \p ilist that do not have raw bits
 * will have instr_set_translation() called with \p pc as the target.
 * This is a convenience routine to make it easy to have the same
 * code generate non-translation and translation instructions, and it does
 * not try to enforce that all instructions have translations (e.g.,
 * some could be inserted via instr_set_next()).
 */
void
instrlist_set_translation_target(instrlist_t *ilist, app_pc pc);

DR_API
/** Returns the translation target, or NULL if none is set. */
app_pc
instrlist_get_translation_target(instrlist_t *ilist);

/* not exported: for PR 267260 */
void
instrlist_set_our_mangling(instrlist_t *ilist, bool ours);

bool
instrlist_get_our_mangling(instrlist_t *ilist);

DR_API
/** Returns the first instr_t in \p ilist. */
instr_t*
instrlist_first(instrlist_t *ilist);

DR_API
/** Returns the last instr_t in \p ilist. */
instr_t*
instrlist_last(instrlist_t *ilist);

DR_API
/** Adds \p instr to the end of \p ilist. */
void 
instrlist_append(instrlist_t *ilist, instr_t *instr);

DR_API
/** Adds \p instr to the front of \p ilist. */
void 
instrlist_prepend(instrlist_t *ilist, instr_t *instr);


DR_API
/** 
 * Allocates a new instrlist_t and for each instr_t in \p old allocates
 * a new instr_t using instr_clone to produce a complete copy of \p old.
 * Each operand that is opnd_is_instr() has its target updated
 * to point to the corresponding instr_t in the new instrlist_t
 * (this routine assumes that all such targets are contained within \p old,
 * and may fault otherwise).
 */
instrlist_t*
instrlist_clone(dcontext_t *dcontext, instrlist_t *old);


/* Adds every instr_t in prependee to the front of ilist (maintaining
 * the original order).
 * Then calls instrlist_destroy on prependee.
 * FIXME: get rid of this?
 */
void 
instrlist_prepend_instrlist(dcontext_t *dcontext, instrlist_t *ilist,
                            instrlist_t *prependee);

/* Adds every instr_t in appendee to the end of ilist (maintaining
 * the original order).
 * Then calls instrlist_destroy on appendee.
 * FIXME: get rid of this?
 */
void 
instrlist_append_instrlist(dcontext_t *dcontext, instrlist_t *ilist,
                           instrlist_t *appendee);

/* instrlist_t functions focusing on a paricular instr_t
 * For insert_before and insert_after, if the instr is NULL, either 
 * instrlist_append or instrlist_prepend will be called
 */

DR_API
/** Inserts \p instr into \p ilist prior to \p where. */
void   
instrlist_preinsert(instrlist_t *ilist, instr_t *where, instr_t *instr);

DR_API
/** Inserts \p instr into \p ilist after \p where. */
void   
instrlist_postinsert(instrlist_t *ilist, instr_t *where, instr_t *instr);

DR_API
/** Replaces \p oldinst with \p newinst in \p ilist (does not destroy \p oldinst). */
instr_t*
instrlist_replace(instrlist_t *ilist, instr_t *oldinst, instr_t *newinst);

DR_API
/** Removes (does not destroy) \p instr from \p ilist. */
void   
instrlist_remove(instrlist_t *ilist, instr_t *instr);

# ifdef CLIENT_INTERFACE
DR_API
/**
 * Specifies the fall-through target of a basic block if its last 
 * instruction is a conditional branch instruction. 
 * It can only be called in basic block building event callbacks 
 * when the \p for_trace parameter is false,
 * and has NO EFFECT in other cases.
 */
bool
instrlist_set_fall_through_target(instrlist_t *bb, app_pc tgt);

/* Get the fall-through target of a basic block if it is set
 * by a client, return NULL otherwise. 
 */
app_pc
instrlist_get_fall_through_target(instrlist_t *bb);

DR_API
/**
 * Specifies the return target of a basic block if its last 
 * instruction is a call instruction. 
 * It can only be called in basic block building event callbacks
 * when the \p for_trace parameter is false,
 * and has NO EFFECT in other cases.
 */
bool
instrlist_set_return_target(instrlist_t *bb, app_pc tgt);

/* Get the return target of a basic block if it is set by a client,
 * return NULL otherwise. 
 */
app_pc
instrlist_get_return_target(instrlist_t *bb);
# endif /* CLIENT_INTERFACE */

#endif /* _INSTRLIST_H_ */
