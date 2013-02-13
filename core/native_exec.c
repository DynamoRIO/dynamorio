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

/* Cross-platform logic for executing parts of the app natively alongside DR's
 * code cache.
 *
 * At Determina, native exec was used primarily to avoid security violation
 * false positives in JITs.  For instrumentation clients, it can offer improved
 * performance when dealing with libraries that don't need to be instrumented.
 * However, we cannot guarantee that we won't lose control or violate
 * transparency.
 */

#include "native_exec.h"
#include "../globals.h"
#include "../vmareas.h"
#include "../module_shared.h"
#include "../instrlist.h"
#include "arch_exports.h"
#include "instr.h"
#include "decode_fast.h"

/* list of native_exec module regions 
 */
vm_area_vector_t *native_exec_areas;

void
native_exec_init(void)
{
    native_module_init();
    if (!DYNAMO_OPTION(native_exec) || DYNAMO_OPTION(thin_client))
        return;
    VMVECTOR_ALLOC_VECTOR(native_exec_areas, GLOBAL_DCONTEXT, VECTOR_SHARED,
                          native_exec_areas);
}

void
native_exec_exit(void)
{
    native_module_exit();
    if (native_exec_areas == NULL)
        return;
    vmvector_delete_vector(GLOBAL_DCONTEXT, native_exec_areas);
    native_exec_areas = NULL;
}

static bool
on_native_exec_list(const char *modname)
{
    bool onlist = false;

    ASSERT(!DYNAMO_OPTION(thin_client));
    if (!DYNAMO_OPTION(native_exec))
        return false;

    if (!IS_STRING_OPTION_EMPTY(native_exec_default_list)) {
        string_option_read_lock();
        LOG(THREAD_GET, LOG_INTERP|LOG_VMAREAS, 4,
            "on_native_exec_list: module %s vs default list %s\n",
            (modname == NULL ? "null" : modname),
            DYNAMO_OPTION(native_exec_default_list));
        onlist = check_filter(DYNAMO_OPTION(native_exec_default_list), modname);
        string_option_read_unlock();
    }
    if (!onlist &&
        !IS_STRING_OPTION_EMPTY(native_exec_list)) {
        string_option_read_lock();
        LOG(THREAD_GET, LOG_INTERP|LOG_VMAREAS, 4,
            "on_native_exec_list: module %s vs append list %s\n",
            modname==NULL?"null":modname, DYNAMO_OPTION(native_exec_list));
        onlist = check_filter(DYNAMO_OPTION(native_exec_list), modname);
        string_option_read_unlock();
    }
    return onlist;
}

static bool
check_and_mark_native_exec(module_area_t *ma, bool add)
{
    bool is_native = false;
    const char *name = GET_MODULE_NAME(&ma->names);
    ASSERT(os_get_module_info_locked());
    if (DYNAMO_OPTION(native_exec) && name != NULL &&
        on_native_exec_list(name)) {
        LOG(GLOBAL, LOG_INTERP|LOG_VMAREAS, 1,
            "module %s is on native_exec list\n", name);
        is_native = true;
    }

    if (add && is_native) {
        RSTATS_INC(num_native_module_loads);
        vmvector_add(native_exec_areas, ma->start, ma->end, NULL);
    } else if (!add) {
        /* If we're removing and it's native, it should be on there already.  If
         * it's not native, then it shouldn't be present, but we'll remove
         * whatever is there.
         */
        DEBUG_DECLARE(bool present =)
            vmvector_remove(native_exec_areas, ma->start, ma->end);
        ASSERT_CURIOSITY((is_native && present) || (!is_native && !present));
    }
    return is_native;
}

void
native_exec_module_load(module_area_t *ma, bool at_map)
{
    bool is_native = check_and_mark_native_exec(ma, true/*add*/);
    if (is_native && DYNAMO_OPTION(native_exec_retakeover))
        native_module_hook(ma, at_map);
}

void
native_exec_module_unload(module_area_t *ma)
{
    bool is_native = check_and_mark_native_exec(ma, false/*!add*/);
    if (is_native && DYNAMO_OPTION(native_exec_retakeover))
        native_module_unhook(ma);
}

void
native_module_transition(priv_mcontext_t *mc, app_pc target)
{
    LOG(THREAD_GET, 6, LOG_LOADER, "cross-module call to %p\n", target);
}
