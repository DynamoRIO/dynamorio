/* ******************************************************************************
 * Copyright (c) 2014-2016 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL Google, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* file "cleancallopt.c" */

#include "../globals.h"
#include "arch.h"
#include "instrument.h"
#include "../hashtable.h"
#include "disassemble.h"
#include "instr_create.h"

#ifdef CLIENT_INTERFACE

static void
callee_info_init(callee_info_t *ci)
{
    memset(ci, 0, sizeof(*ci));
    ci->bailout = true;
    /* to be conservative */
    ci->has_locals   = true;
    ci->write_aflags = true;
    ci->read_aflags  = true;
    ci->tls_used     = true;
}

void
clean_call_opt_init(void)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(INTERNAL_OPTION(opt_cleancall) == 0);
    callee_info_init(&default_callee_info);
}

void
clean_call_opt_exit(void)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(INTERNAL_OPTION(opt_cleancall) == 0);
}

bool
analyze_clean_call(dcontext_t *dcontext, clean_call_info_t *cci, instr_t *where,
                   void *callee, bool save_fpstate, bool always_out_of_line,
                   uint num_args, opnd_t *args)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(INTERNAL_OPTION(opt_cleancall) == 0);
    clean_call_info_init(cci, callee, save_fpstate, num_args);
    return false;
}

void
insert_inline_clean_call(dcontext_t *dcontext, clean_call_info_t *cci,
                         instrlist_t *ilist, instr_t *where, opnd_t *args)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_REACHED();
}
#endif /* CLIENT_INTERFACE */
