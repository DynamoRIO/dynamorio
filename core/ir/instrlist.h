/* **********************************************************
 * Copyright (c) 2014-2021 Google, Inc.  All rights reserved.
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

#include "instr.h"
#include "instrlist_api.h"

struct _instrlist_t {
    instr_t *first;
    instr_t *last;
    int flags;
    app_pc translation_target;
    /* i#620: provide API for setting fall-throught/return target in bb */
    /* XXX: can this be unioned with traslation_target for saving space?
     * looks no, as traslation_target will be used in mangle and trace,
     * which conflicts with our checks in trace and return address mangling.
     * XXX: There are several possible ways to implement i#620, for example,
     * adding a dr_register_bb_event() OUT param.
     * However, we do here to avoid breaking backward compatibility
     */
    app_pc fall_through_bb;
#ifdef ARM
    dr_pred_type_t auto_pred;
#endif /* ARM */
};     /* instrlist_t */

/* basic instrlist_t functions */

bool
instrlist_get_our_mangling(instrlist_t *ilist);

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

/* Get the fall-through target of a basic block if it is set
 * by a client, return NULL otherwise.
 */
app_pc
instrlist_get_fall_through_target(instrlist_t *bb);

/* Get the return target of a basic block if it is set by a client,
 * return NULL otherwise.
 */
app_pc
instrlist_get_return_target(instrlist_t *bb);

#endif /* _INSTRLIST_H_ */
