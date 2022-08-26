/* **********************************************************
 * Copyright (c) 2013-2014 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Intercepts module transitions for native execution for Mach-O modules.
 * FIXME i#1287: all of this is NYI!
 */

#include "../globals.h"
#include "../native_exec.h"
#include "module.h"
#include "module_private.h"

void
native_module_hook(module_area_t *ma, bool at_map)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1287: implement for MacOS */
}

void
native_module_unhook(module_area_t *ma)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1287: implement for MacOS */
}

/* Our replacement for _dl_fixup.
 */
void *
dynamorio_dl_fixup(void *l_map, uint reloc_arg)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1287: implement for MacOS */
    return NULL;
}

void
native_module_init(void)
{
    /* FIXME i#1287: implement for MacOS */
}

void
native_module_exit(void)
{
    /* FIXME i#1287: implement for MacOS */
}

/* called on unloading a non-native module */
void
native_module_nonnative_mod_unload(module_area_t *ma)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1287: implement for MacOS */
}

/* get (create if not exist) a ret_stub for the return target: tgt */
app_pc
native_module_get_ret_stub(dcontext_t *dcontext, app_pc tgt)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1287: implement for MacOS */
    return NULL;
}

/* xref i#1247: clean call right before dl_runtime_resolve return */
void
native_module_at_runtime_resolve_ret(app_pc xsp, int ret_imm)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1287: implement for MacOS */
}

bool
native_exec_replace_next_tag(dcontext_t *dcontext)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1287: implement for MacOS */
    return false;
}
