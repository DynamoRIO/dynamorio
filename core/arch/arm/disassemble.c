/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
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

/* disassemble.c -- printing of ARM instructions */

#include "../globals.h"
#include "arch.h"
#include "instr.h"
#include "decode.h"
#include "decode_private.h"
#include "disassemble.h"
#include <string.h>

#if defined(INTERNAL) || defined(DEBUG) || defined(CLIENT_INTERFACE)

#ifdef DEBUG
/* case 10450: give messages to clients */
/* we can't undef ASSERT b/c of DYNAMO_OPTION */
# undef ASSERT_TRUNCATE
# undef ASSERT_BITFIELD_TRUNCATE
# undef ASSERT_NOT_REACHED
# define ASSERT_TRUNCATE DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
# define ASSERT_BITFIELD_TRUNCATE DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
# define ASSERT_NOT_REACHED DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#endif


bool
opnd_disassemble_noimplicit(char *buf, size_t bufsz, size_t *sofar INOUT,
                            dcontext_t *dcontext, instr_t *instr,
                            byte optype, opnd_t opnd, bool prev, bool multiple_encodings)
{
    /* FIXME i#1551: NYI */
    CLIENT_ASSERT(false, "NYI");
    switch (optype) {
    default:
        CLIENT_ASSERT(false, "missing decode type"); /* catch any missing types */
    }
    return false;
}

const char *
instr_opcode_name_arch(instr_t *instr, const instr_info_t *info)
{
    return NULL;
}

const char *
instr_opcode_name_suffix_arch(instr_t *instr)
{
    return NULL;
}

void
print_instr_prefixes(dcontext_t *dcontext, instr_t *instr,
                     char *buf, size_t bufsz, size_t *sofar INOUT)
{
    return;
}

int
print_opcode_suffix(instr_t *instr, char *buf, size_t bufsz, size_t *sofar INOUT)
{
    /* FIXME i#1551: NYI: print predicate */
    CLIENT_ASSERT(false, "NYI");
    return 0;
}

#endif /* INTERNAL || CLIENT_INTERFACE */
