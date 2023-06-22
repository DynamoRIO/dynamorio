/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
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

/* encode_shared.c -- cross-platform encodingn routines */

#include "../globals.h"
#include "arch.h"
#include "instr.h"
#include "decode.h"
#include "disassemble.h"
#include "decode_fast.h"
#include "decode_private.h"

#ifdef DEBUG
/* case 10450: give messages to clients */
/* we can't undef ASSERT b/c of DYNAMO_OPTION */
#    undef ASSERT_TRUNCATE
#    undef ASSERT_BITFIELD_TRUNCATE
#    undef ASSERT_NOT_REACHED
#    define ASSERT_TRUNCATE DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#    define ASSERT_BITFIELD_TRUNCATE DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#    define ASSERT_NOT_REACHED DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#endif

/* In arch-specific file */
bool
encoding_possible(decode_info_t *di, instr_t *in, const instr_info_t *ii);

void
decode_info_init_for_instr(decode_info_t *di, instr_t *instr);

byte *
instr_encode_arch(dcontext_t *dcontext, instr_t *instr, byte *copy_pc, byte *final_pc,
                  bool check_reachable,
                  bool *has_instr_opnds /*OUT OPTIONAL*/
                      _IF_DEBUG(bool assert_reachable));

#if defined(AARCH64) || defined(RISCV64)
/* exported
 */
bool
instr_is_encoding_possible(instr_t *instr)
{
    decode_info_t di;

    return encoding_possible(&di, instr, NULL);
}
#else
/* exported, looks at all possible instr_info_t templates
 */
bool
instr_is_encoding_possible(instr_t *instr)
{
    const instr_info_t *info = get_encoding_info(instr);
    return (info != NULL);
}
#endif

/* looks at all possible instr_info_t templates, returns first match
 * returns NULL if no encoding is possible
 */
const instr_info_t *
get_encoding_info(instr_t *instr)
{
    const instr_info_t *info = instr_get_instr_info(instr);
    decode_info_t di;
    decode_info_init_for_instr(&di, instr);
    IF_AARCHXX(di.check_reachable = false;)

    while (!encoding_possible(&di, instr, info)) {
        info = get_next_instr_info(info);
        /* stop when hit end of list or when hit extra operand tables (OP_CONTD) */
        if (info == NULL || info->opcode == OP_CONTD) {
            return NULL;
        }
    }
    return info;
}

/* completely ignores reachability and predication failures */
byte *
instr_encode_ignore_reachability(dcontext_t *dcontext, instr_t *instr, byte *pc)
{
    return instr_encode_arch(dcontext, instr, pc, pc, false, NULL _IF_DEBUG(false));
}

/* just like instr_encode but doesn't assert on reachability or predication failures */
byte *
instr_encode_check_reachability(dcontext_t *dcontext, instr_t *instr, byte *pc,
                                bool *has_instr_opnds /*OUT OPTIONAL*/)
{
    return instr_encode_arch(dcontext, instr, pc, pc, true,
                             has_instr_opnds _IF_DEBUG(false));
}

byte *
instr_encode_to_copy(void *drcontext, instr_t *instr, byte *copy_pc, byte *final_pc)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    return instr_encode_arch(dcontext, instr, copy_pc, final_pc, true,
                             NULL _IF_DEBUG(true));
}

byte *
instr_encode(void *drcontext, instr_t *instr, byte *pc)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    return instr_encode_to_copy(dcontext, instr, pc, pc);
}
