/* **********************************************************
 * Copyright (c) 2012 Google, Inc.  All rights reserved.
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

#ifndef _NATIVE_EXEC_H_
#define _NATIVE_EXEC_H_ 1

#include "globals.h"
#include "module_shared.h"

extern vm_area_vector_t *native_exec_areas;

void
native_exec_module_load(module_area_t *ma, bool at_map);
void
native_exec_module_unload(module_area_t *ma);

void
native_exec_init(void);
void
native_exec_exit(void);

bool
is_native_pc(app_pc pc);

/* Gets called on every call into a native module. */
void
call_to_native(app_pc *sp);

/* Gets called on every return to a native module. */
void
return_to_native(void);

/* Gets called on every cross-module call out of a native module. */
void
native_module_callout(priv_mcontext_t *mc, app_pc target);

/* The following prototypes are implemented by various object file formats.  For
 * now we assume a single object file format per platform.
 */

void
native_module_init(void);
void
native_module_exit(void);

void
native_module_hook(module_area_t *ma, bool at_map);
void
native_module_unhook(module_area_t *ma);

/* Update next_tag with the real app return address. */
void
interpret_back_from_native(dcontext_t *dcontext);

/* Put back the native return addresses that we swapped to maintain control.  We
 * do this when detaching.  If we're coordinating with the app, then we could do
 * this before the app takes a stack trace.  Returns whether or not there were
 * any native retaddrs.
 */
void
put_back_native_retaddrs(dcontext_t *dcontext);

/* Return if this pc is one of the back_from_native return stubs.  Try to make
 * this a single predictable branch.
 */
static inline bool
native_exec_is_back_from_native(app_pc pc)
{
    ptr_uint_t diff = (ptr_uint_t)pc - (ptr_uint_t)back_from_native_retstubs;
    return (diff < MAX_NATIVE_RETSTACK * BACK_FROM_NATIVE_RETSTUB_SIZE);
}

#endif /* _NATIVE_EXEC_H_ */
