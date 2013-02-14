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
#include "monitor.h"

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

/* Clean call called on every fcache to native transition.  Turns on and off
 * asynch handling and updates some state.  Called from native bbs built by
 * build_native_exec_bb() in x86/interp.c.
 */
void
entering_native(void)
{
    dcontext_t *dcontext;
    ENTERING_DR();
    dcontext = get_thread_private_dcontext();
    ASSERT(dcontext != NULL);
#ifdef WINDOWS
    /* turn off asynch interception for this thread while native
     * FIXME: what if callbacks and apcs are destined for other modules?
     * should instead run dispatcher under DR every time, if going to native dll
     * will go native then?  have issues w/ missing the cb ret, though...
     * N.B.: if allow some asynch, have to find another place to store the real
     * return addr (currently in next_tag)
     *
     * We can't revert memory prots, since other threads are under DR
     * control, but we do handle our-fault write faults in native threads.
     */
    set_asynch_interception(dcontext->owning_thread, false);
#endif
    /* FIXME: setting same var that set_asynch_interception is! */
    dcontext->thread_record->under_dynamo_control = false;

    /* if we were building a trace, kill it */
    if (is_building_trace(dcontext)) {
        LOG(THREAD, LOG_ASYNCH, 2, "entering_native: squashing old trace\n");
        trace_abort(dcontext);
    }
    set_last_exit(dcontext, (linkstub_t *) get_native_exec_linkstub());
    /* now we're in app! */
    dcontext->whereami = WHERE_APP;
    SYSLOG_INTERNAL_WARNING_ONCE("entered at least one module natively");
    LOG(THREAD, LOG_ASYNCH, 1, "!!!! Entering module NATIVELY, retaddr="PFX"\n\n",
        dcontext->native_exec_retval);
    STATS_INC(num_native_module_enter);
    EXITING_DR();
}

/* Re-enters DR at the target PC.  Used on returns back from native modules and
 * calls out of native modules.  Inverse of entering_native().
 */
static void
back_from_native_C(dcontext_t *dcontext, priv_mcontext_t *mc, app_pc target)
{
    /* ASSUMPTION: was native entire time, don't need to initialize dcontext
     * or anything, and next_tag is still there!
     */
    ASSERT(dcontext->whereami == WHERE_APP);
    ASSERT(dcontext->last_exit == get_native_exec_linkstub());
    dcontext->next_tag = target;
    /* tell dispatch() why we're coming there */
    dcontext->whereami = WHERE_FCACHE;
#ifdef WINDOWS
    /* asynch back on */
    set_asynch_interception(dcontext->owning_thread, true);
#endif
    /* XXX: setting same var that set_asynch_interception is! */
    dcontext->thread_record->under_dynamo_control = true;

    *get_mcontext(dcontext) = *mc;
    /* clear pc */
    get_mcontext(dcontext)->pc = 0;

    call_switch_stack(dcontext, dcontext->dstack, dispatch,
                      false/*not on initstack*/, false/*shouldn't return*/);
    ASSERT_NOT_REACHED();
}

/* Re-enters DR after a call to a native module returns.  Called from the asm
 * routine back_from_native() in x86.asm.
 */
void
return_from_native(priv_mcontext_t *mc)
{
    dcontext_t *dcontext;
    app_pc target;
    ENTERING_DR();
    dcontext = get_thread_private_dcontext();
    ASSERT(dcontext != NULL);
    LOG(THREAD, LOG_ASYNCH, 1, "\n!!!! Returned from NATIVE module to "PFX"\n",
        dcontext->native_exec_retval);
    SYSLOG_INTERNAL_WARNING_ONCE("returned from at least one native module");
    ASSERT(dcontext->native_exec_retval != NULL);
    ASSERT(dcontext->native_exec_retloc != NULL);
    target = dcontext->native_exec_retval;
    dcontext->native_exec_retval = NULL;
    dcontext->native_exec_retloc = NULL;
    back_from_native_C(dcontext, mc, target); /* noreturn */
    ASSERT_NOT_REACHED();
}

/* Re-enters DR on calls from native modules to non-native modules.  Called from
 * x86.asm.
 */
void
native_module_callout(priv_mcontext_t *mc, app_pc target)
{
    dcontext_t *dcontext;
    ENTERING_DR();
    dcontext = get_thread_private_dcontext();
    ASSERT(dcontext != NULL);
    ASSERT(DYNAMO_OPTION(native_exec_retakeover));
    LOG(THREAD, LOG_ASYNCH, 4, "%s: cross-module call to %p\n",
        __FUNCTION__, target);
    back_from_native_C(dcontext, mc, target);
    ASSERT_NOT_REACHED();
}
