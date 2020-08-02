/* ******************************************************************************
 * Copyright (c) 2010-2020 Google, Inc.  All rights reserved.
 * Copyright (c) 2010 Massachusetts Institute of Technology  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
 * ******************************************************************************/

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

/* file "ir_utils.c": multi-instruction manipulation shared between the core
 * and drdecode.
 */

#include "../globals.h"
#include "ir_utils.h"

void
insert_mov_immed_ptrsz(dcontext_t *dcontext, ptr_int_t val, opnd_t dst,
                       instrlist_t *ilist, instr_t *instr, OUT instr_t **first,
                       OUT instr_t **last)
{
    insert_mov_immed_arch(dcontext, NULL, NULL, val, dst, ilist, instr, first, last);
}

void
insert_mov_instr_addr(dcontext_t *dcontext, instr_t *src, byte *encode_estimate,
                      opnd_t dst, instrlist_t *ilist, instr_t *instr, OUT instr_t **first,
                      OUT instr_t **last)
{
    insert_mov_immed_arch(dcontext, src, encode_estimate, 0, dst, ilist, instr, first,
                          last);
}

void
insert_push_immed_ptrsz(dcontext_t *dcontext, ptr_int_t val, instrlist_t *ilist,
                        instr_t *instr, OUT instr_t **first, OUT instr_t **last)
{
    insert_push_immed_arch(dcontext, NULL, NULL, val, ilist, instr, first, last);
}

void
insert_push_instr_addr(dcontext_t *dcontext, instr_t *src_inst, byte *encode_estimate,
                       instrlist_t *ilist, instr_t *instr, OUT instr_t **first,
                       OUT instr_t **last)
{
    insert_push_immed_arch(dcontext, src_inst, encode_estimate, 0, ilist, instr, first,
                           last);
}
