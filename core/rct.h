/* **********************************************************
 * Copyright (c) 2004-2008 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2004-2007 Determina Corp. */

/*
 * rct.h - RCT policy exports
 */

#ifndef RCT_H
#define RCT_H

enum {
    RCT_CACHE_EXEMPT_NONE = 0,
    RCT_CACHE_EXEMPT_MODULES = 1,
    RCT_CACHE_EXEMPT_ALL = 2, /* including DGC */
};

#ifdef RCT_IND_BRANCH
/* public */
int
rct_ind_branch_check(dcontext_t *dcontext, app_pc target_addr, app_pc src_addr);

bool
rct_check_ref_and_add(dcontext_t *dcontext, app_pc ref, app_pc referto_start,
                      app_pc referto_end _IF_DEBUG(app_pc addr)
                          _IF_DEBUG(bool *known_ref /* OPTIONAL OUT */));

uint
find_address_references(dcontext_t *dcontext, app_pc text_start, app_pc text_end,
                        app_pc referto_start, app_pc referto_end);
uint
invalidate_ind_branch_target_range(dcontext_t *dcontext, app_pc text_start,
                                   app_pc text_end);

void
rct_known_targets_init(void);
void
rct_init(void);
void
rct_exit(void);

/* private routines for rct.c and fragment.c */

/* FIXME: once we move the needed pieces out into fragment.h then
 * these routines can be moved from fragment.c and be static in rct.c
 */

#    include "fragment.h"

/* returns NULL if not found */
app_pc
rct_ind_branch_target_lookup(dcontext_t *dcontext, app_pc tag);

bool
rct_add_valid_ind_branch_target(dcontext_t *dcontext, app_pc tag);

void
rct_flush_ind_branch_target_entry(dcontext_t *dcontext, app_pc tag);

bool
rct_is_exported_function(app_pc tag); /* in module.c */

#    ifdef X64
/* in module.c */
bool
rct_add_rip_rel_addr(dcontext_t *dcontext, app_pc tgt _IF_DEBUG(app_pc src));
#    endif

/* exported for ASSERT's to verify it's held */
extern mutex_t rct_module_lock;

#endif /* RCT_IND_BRANCH */

#endif /* RCT_H */
