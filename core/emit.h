/* **********************************************************
 * Copyright (c) 2000-2009 VMware, Inc.  All rights reserved.
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
 * emit.h - fragment code generation interfaces
 */

#ifndef _EMIT_H_
#define _EMIT_H_ 1

/* Emits code for ilist into the fcache, returns the created
 * fragment.  Adds the fragment to the fragment hashtable and
 * links it as a new fragment.
 */
fragment_t *
emit_fragment(dcontext_t *dcontext, app_pc tag, instrlist_t *ilist, uint flags,
              void *vmlist, bool link);

fragment_t *
emit_fragment_ex(dcontext_t *dcontext, app_pc tag, instrlist_t *ilist, uint flags,
                 void *vmlist, bool link, bool visible);

/* Emits code for ilist into the fcache, returns the created fragment.
 * Does not add the fragment to the ftable, leaving it as an "invisible"
 * fragment.  This means it is the caller's responsibility to ensure
 * it is properly disposed of when done with.
 * The fragment is also not linked, to give the caller more flexibility.
 */
fragment_t *
emit_invisible_fragment(dcontext_t *dcontext, app_pc tag, instrlist_t *ilist, uint flags,
                        void *vmlist);

fragment_t *
emit_fragment_as_replacement(dcontext_t *dcontext, app_pc tag, instrlist_t *ilist,
                             uint flags, void *vmlist, fragment_t *replace);

#ifdef INTERNAL
void
stress_test_recreate(dcontext_t *dcontext, fragment_t *f, instrlist_t *ilist);
#endif

bool
final_exit_shares_prev_stub(dcontext_t *dcontext, instrlist_t *ilist, uint frag_flags);

/* Walks ilist and f's linkstubs, setting each linkstub_t's fields appropriately
 * for the corresponding exit cti in ilist.
 * If emit is true, also encodes each instr in ilist to f's cache slot,
 * increments stats for new fragments, and returns the final pc after all encodings.
 */
cache_pc
set_linkstub_fields(dcontext_t *dcontext, fragment_t *f, instrlist_t *ilist,
                    uint num_direct_stubs, uint num_indirect_stubs, bool emit);

#endif /* _EMIT_H_ */
