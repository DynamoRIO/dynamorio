/* **********************************************************
 * Copyright (c) 2011-2024 Google, Inc.  All rights reserved.
 * Copyright (c) 2001-2010 VMware, Inc.  All rights reserved.
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
/* Copyright (c) 2001 Hewlett-Packard Company */

/* decode.c -- a decoder for DR synthetic IR */

#include "encode.h"
#include "decode.h"

#include "../globals.h"
#include "instr_api.h"

#define CATEGORY_BITS 22
#define FLAGS_BITS 2
#define NUM_OPND_BITS 4

void
decode_from_synth(dcontext_t *dcontext, byte *encoded_instr, instr_t *instr)
{
    uint encoding = 0;
    uint shift = 0;

    /*
     * Copy encoded_instr in a uint for easy retrieving of values.
     */
    memcpy(&encoding, encoded_instr, sizeof(uint));

    /*
     * Decode synthetic opcode as instruction category.
     */
    uint category_mask = (1U << CATEGORY_BITS) - 1;
    uint category = encoding & category_mask;
    instr_set_category(instr, category);
    shift += CATEGORY_BITS;

    /*
     * Decode flags.
     */
    // Commented to avoid unused variable error.
    // uint flags_mask = ((1U << FLAGS_BITS) - 1) << shift;
    // uint flags = encoding & flags_mask;
    // TOFIX: set all arithmetic flags of instr_t?
    shift += FLAGS_BITS;

    /*
     * Decode number of source operands.
     */
    uint num_srcs_mask = ((1U << NUM_OPND_BITS) - 1) << shift;
    uint num_srcs = encoding & num_srcs_mask;
    shift += NUM_OPND_BITS;

    /*
     * Decode number of destination operands.
     */
    uint num_dsts_mask = ((1U << NUM_OPND_BITS) - 1) << shift;
    uint num_dsts = encoding & num_dsts_mask;

    instr_set_num_opnds(dcontext, instr, num_dsts, num_srcs);

    return;
}
