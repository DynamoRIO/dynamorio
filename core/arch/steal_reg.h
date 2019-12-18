/* **********************************************************
 * Copyright (c) 2019 Google, Inc.  All rights reserved.
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

/* file "x86/steal_reg.h" */

#ifndef X86_STEAL_REG_H
#define X86_STEAL_REG_H

/* This header file declares the tables and functions that help decode
   x86 instructions so that we can steal edi for our own uses. */

void
steal_reg(dcontext_t *dcontext, instr_t *instr, instrlist_t *ilist);
void
restore_state(dcontext_t *dcontext, instr_t *instr, instrlist_t *ilist);

/* We use the instrlist_t flags field to signify when the application's
 * value in ebx is in dcontext and ebx actually contains the
 * application's edi value (true when bit 0 of flags is asserted).  As
 * you can see in the .c file, we're pretty conservative with this
 * optimization.  We're really only concerned with the stretches where
 * edi is involved in a calculation.  As such, whenever an instruction
 * doesn't use edi, we place dcontext->xbx back in ebx.  We also keep
 * track of when the edi value in ebx matches dcontext->xdi (true when
 * bit 1 of flags is asserted) so that we don't have to dump ebx into
 * dcontext->xdi unnecessarily. */
#define EDI_VAL_IN_MEM 0
#define EDI_VAL_IN_EBX 1
#define EDI_VAL_IN_EBX_AND_MEM 3
/* XXX: We now store other flags in instrlist_t.flags.  The steal reg code needs
 * to be updated for that.  However, it's been unused for so long perhaps it should
 * just be deleted.
 */
#define STEAL_REG_ILIST_FLAGS (EDI_VAL_IN_MEM | EDI_VAL_IN_EBX | EDI_VAL_IN_EBX_AND_MEM)

#endif /* X86_STEAL_REG_H */
