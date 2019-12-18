/* **********************************************************
 * Copyright (c) 2012-2019 Google, Inc.  All rights reserved.
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
#include "arch_exports.h"
#include "instr.h"
#include "instrlist.h"
#include "decode_fast.h"
#include "monitor.h"

#define PRE instrlist_preinsert
/* list of native_exec module regions
 */
vm_area_vector_t *native_exec_areas;

static const app_pc retstub_start = (app_pc)back_from_native_retstubs;
#ifdef DEBUG
static const app_pc retstub_end = (app_pc)back_from_native_retstubs_end;
#endif

void
native_exec_init(void)
{
    native_module_init();
    if (!DYNAMO_OPTION(native_exec) || DYNAMO_OPTION(thin_client))
        return;
    VMVECTOR_ALLOC_VECTOR(native_exec_areas, GLOBAL_DCONTEXT, VECTOR_SHARED,
                          native_exec_areas);
    DOCHECK(CHKLVL_ASSERTS, {
        /* i#2124: we work around a bug in clang by using a local var. */
        app_pc local_start = retstub_start;
        ASSERT(retstub_end ==
               local_start + MAX_NATIVE_RETSTACK * BACK_FROM_NATIVE_RETSTUB_SIZE);
    });
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
is_dr_native_pc(app_pc pc)
{
#ifdef DR_APP_EXPORTS
    if (pc ==
        (app_pc)dr_app_running_under_dynamorio IF_LINUX(
            || pc == (app_pc)dr_app_handle_mbr_target))
        return true;
#endif /* DR_APP_EXPORTS */
    return false;
}

bool
is_native_pc(app_pc pc)
{
    return vmvector_overlap(native_exec_areas, pc, pc + 1);
}

bool
is_stay_native_pc(app_pc pc)
{
    /* only used for native exec */
    ASSERT(DYNAMO_OPTION(native_exec) && !vmvector_empty(native_exec_areas));
    return (is_dr_native_pc(pc) || is_native_pc(pc));
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
        LOG(THREAD_GET, LOG_INTERP | LOG_VMAREAS, 4,
            "on_native_exec_list: module %s vs default list %s\n",
            (modname == NULL ? "null" : modname),
            DYNAMO_OPTION(native_exec_default_list));
        onlist = check_filter(DYNAMO_OPTION(native_exec_default_list), modname);
        string_option_read_unlock();
    }
    if (!onlist && !IS_STRING_OPTION_EMPTY(native_exec_list)) {
        string_option_read_lock();
        LOG(THREAD_GET, LOG_INTERP | LOG_VMAREAS, 4,
            "on_native_exec_list: module %s vs append list %s\n",
            modname == NULL ? "null" : modname, DYNAMO_OPTION(native_exec_list));
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
    if (DYNAMO_OPTION(native_exec) && name != NULL && on_native_exec_list(name)) {
        LOG(GLOBAL, LOG_INTERP | LOG_VMAREAS, 1, "module %s is on native_exec list\n",
            name);
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
    bool is_native;
    if (!DYNAMO_OPTION(native_exec))
        return;
    is_native = check_and_mark_native_exec(ma, true /*add*/);
    if (is_native && DYNAMO_OPTION(native_exec_retakeover))
        native_module_hook(ma, at_map);
}

void
native_exec_module_unload(module_area_t *ma)
{
    bool is_native;
    if (!DYNAMO_OPTION(native_exec))
        return;
    is_native = check_and_mark_native_exec(ma, false /*!add*/);
    if (DYNAMO_OPTION(native_exec_retakeover)) {
        if (is_native)
            native_module_unhook(ma);
#ifdef UNIX
        else
            native_module_nonnative_mod_unload(ma);
#endif
    }
}

/* Clean call called on every fcache to native transition.  Turns on and off
 * asynch handling and updates some state.  Called from native bbs built by
 * build_native_exec_bb() in arch/interp.c.
 *
 * N.B.: all the actions of this routine are mirrored in insert_enter_native(),
 * so any changes here should be mirrored there.
 */
static void
entering_native(dcontext_t *dcontext)
{
    /* we need to match dr_app_stop() so we pop the kstack */
    KSTOP_NOT_MATCHING(dispatch_num_exits);
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
    /* FIXME i#2375: for -native_exec_opt on UNIX we need to update the gencode
     * to do what os_thread_{,not_}under_dynamo() and os_thread_re_take_over() do.
     */
    if (IF_WINDOWS_ELSE(true, !DYNAMO_OPTION(native_exec_opt)))
        dynamo_thread_not_under_dynamo(dcontext);
    /* XXX: setting same var that set_asynch_interception is! */
    dcontext->thread_record->under_dynamo_control = false;

    ASSERT(!is_building_trace(dcontext));
    set_last_exit(dcontext, (linkstub_t *)get_native_exec_linkstub());
    /* now we're in app! */
    dcontext->whereami = DR_WHERE_APP;
    SYSLOG_INTERNAL_WARNING_ONCE("entered at least one module natively");
    STATS_INC(num_native_module_enter);
}

/* We replace the actual return target on the app stack with a stub pc
 * so that the control transfer back to code cache or DR after native module
 * returns.
 */
static bool
prepare_return_from_native_via_stub(dcontext_t *dcontext, app_pc *app_sp)
{
#ifdef UNIX
    app_pc stub_pc;
    ASSERT(!is_native_pc(*app_sp));
    /* i#1238-c#4: the inline asm stub does not support kstats, so we
     * only support it when native_exec_opt is on, which turns kstats off.
     */
    if (!DYNAMO_OPTION(native_exec_opt))
        return false;
    stub_pc = native_module_get_ret_stub(dcontext, *app_sp);
    if (stub_pc == NULL)
        return false;
    *app_sp = stub_pc;
    return true;
#endif
    return false;
}

static void
prepare_return_from_native_via_stack(dcontext_t *dcontext, app_pc *app_sp)
{
    uint i;
    ASSERT(!is_native_pc(*app_sp));
    /* Push the retaddr and stack location onto our stack.  The current entry
     * should be free and we should have enough space.
     * XXX: it would be nice to abort in a release build, but this can be perf
     * critical.
     */
    i = dcontext->native_retstack_cur;
    ASSERT(i < MAX_NATIVE_RETSTACK);
    dcontext->native_retstack[i].retaddr = *app_sp;
    dcontext->native_retstack[i].retloc = (app_pc)app_sp;
    dcontext->native_retstack_cur = i + 1;
    LOG(THREAD, LOG_ASYNCH, 2, "%s: app ra=" PFX ", sp=" PFX ", level=%d\n", *app_sp,
        app_sp, i);
    /* i#978: We use a different return stub for every nested call to native
     * code.  Each stub pushes a different index into the retstack.  We could
     * use the SP at return time to try to find the app's return address, but
     * because of ret imm8 instructions, that's not robust.
     */
    *app_sp = retstub_start + i * BACK_FROM_NATIVE_RETSTUB_SIZE;
}

void
call_to_native(app_pc *app_sp)
{
    dcontext_t *dcontext;

    ENTERING_DR();
    dcontext = get_thread_private_dcontext();
    ASSERT(dcontext != NULL);
    /* i#1090: If the return address is also in a native module, then leave it
     * alone.  This happens on:
     * - native call
     * - native call tail_caller@plt
     * - non-native jmp native@plt      # TOS is native PC: don't swap
     * - native ret                     # should stay native
     * XXX: Doing a vmvector binary search on every call to native is expensive.
     */
    if (!is_native_pc(*app_sp)) {
        /* We try to use stub for fast return-from-native handling, if fails
         * (e.g., on Windows or optimization disabled), fall back to use the stack.
         */
        if (!prepare_return_from_native_via_stub(dcontext, app_sp))
            prepare_return_from_native_via_stack(dcontext, app_sp);
    }
    LOG(THREAD, LOG_ASYNCH, 1, "!!!! Entering module NATIVELY, retaddr=" PFX "\n\n",
        *app_sp);
    entering_native(dcontext);
    EXITING_DR();
}

/* N.B.: all the actions of this routine are mirrored in insert_return_to_native(),
 * so any changes here should be mirrored there.
 */
void
return_to_native(void)
{
    dcontext_t *dcontext;
    ENTERING_DR();
    dcontext = get_thread_private_dcontext();
    ASSERT(dcontext != NULL);
    entering_native(dcontext);
    EXITING_DR();
}

/* Re-enters DR at the target PC.  Used on returns back from native modules and
 * calls out of native modules.  Inverse of entering_native().
 */
static void
back_from_native_common(dcontext_t *dcontext, priv_mcontext_t *mc, app_pc target)
{
    /* ASSUMPTION: was native entire time, don't need to initialize dcontext
     * or anything, and next_tag is still there!
     */
    ASSERT(dcontext->whereami == DR_WHERE_APP);
    ASSERT(dcontext->last_exit == get_native_exec_linkstub());
    ASSERT(!is_native_pc(target));
    dcontext->next_tag = target;
    /* tell d_r_dispatch() why we're coming there */
    dcontext->whereami = DR_WHERE_FCACHE;
    /* FIXME i#2375: for -native_exec_opt on UNIX we need to update the gencode
     * to do what os_thread_{,not_}under_dynamo() and os_thread_re_take_over() do.
     */
    if (IF_WINDOWS_ELSE(true, !DYNAMO_OPTION(native_exec_opt)))
        dynamo_thread_under_dynamo(dcontext);
    /* XXX: setting same var that set_asynch_interception is! */
    dcontext->thread_record->under_dynamo_control = true;

    *get_mcontext(dcontext) = *mc;
    /* clear pc */
    get_mcontext(dcontext)->pc = 0;

    DOLOG(2, LOG_TOP, {
        byte *cur_esp;
        GET_STACK_PTR(cur_esp);
        LOG(THREAD, LOG_TOP, 2,
            "%s: next_tag=" PFX ", cur xsp=" PFX ", mc->xsp=" PFX "\n", __FUNCTION__,
            dcontext->next_tag, cur_esp, mc->xsp);
    });

    call_switch_stack(dcontext, dcontext->dstack, (void (*)(void *))d_r_dispatch,
                      NULL /*not on d_r_initstack*/, false /*shouldn't return*/);
    ASSERT_NOT_REACHED();
}

/* Pops all return address pairs off the native return stack up to and including
 * retidx.  Returns the return address corresponding to retidx.  This assumes
 * that the app is only doing unwinding, and not re-entering frames after
 * returning past them.
 */
static app_pc
pop_retaddr_for_index(dcontext_t *dcontext, int retidx, app_pc xsp)
{
    ASSERT(dcontext != NULL);
    ASSERT(retidx >= 0 && retidx < MAX_NATIVE_RETSTACK &&
           (uint)retidx < dcontext->native_retstack_cur);
    DOCHECK(CHKLVL_ASSERTS, {
        /* Because of ret imm8 instrs, we can't assert that the current xsp is
         * one slot off from the xsp after the call.  We can assert that it's
         * within 256 bytes, though.
         */
        app_pc retloc = dcontext->native_retstack[retidx].retloc;
        ASSERT(xsp >= retloc && xsp <= retloc + 256 + sizeof(void *) &&
               "failed to find current sp in native_retstack");
    });
    /* Not zeroing out the [retidx:cur] range for performance. */
    dcontext->native_retstack_cur = retidx;
    return dcontext->native_retstack[retidx].retaddr;
}

/* Re-enters DR after a call to a native module returns.  Called from the asm
 * routine back_from_native() in x86.asm.
 */
void
return_from_native(priv_mcontext_t *mc)
{
    dcontext_t *dcontext;
    app_pc target;
    int retidx;
    ENTERING_DR();
    dcontext = get_thread_private_dcontext();
    if (dcontext == NULL) {
        os_thread_re_take_over();
        dcontext = get_thread_private_dcontext();
    }
    ASSERT(dcontext != NULL);
    SYSLOG_INTERNAL_WARNING_ONCE("returned from at least one native module");
    retidx = native_get_retstack_idx(mc);
    target = pop_retaddr_for_index(dcontext, retidx, (app_pc)mc->xsp);
    ASSERT(!is_native_pc(target) &&
           "shouldn't return from native to native PC (i#1090?)");
    LOG(THREAD, LOG_ASYNCH, 1, "\n!!!! Returned from NATIVE module to " PFX "\n", target);
    back_from_native_common(dcontext, mc, target); /* noreturn */
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
    if (dcontext == NULL) {
        os_thread_re_take_over();
        dcontext = get_thread_private_dcontext();
    }
    ASSERT(dcontext != NULL);
    ASSERT(DYNAMO_OPTION(native_exec_retakeover));
    LOG(THREAD, LOG_ASYNCH, 4, "%s: cross-module call to %p\n", __FUNCTION__, target);
    back_from_native_common(dcontext, mc, target);
    ASSERT_NOT_REACHED();
}

/* Update next_tag with the real app return address.  next_tag should currently
 * be equal to a return stub PC.  We compute the offset of the stub, and then
 * divide by the length of each stub to get the index into the return stub.
 */
void
interpret_back_from_native(dcontext_t *dcontext)
{
    app_pc xsp = (app_pc)get_mcontext(dcontext)->xsp;
    ptr_int_t offset = dcontext->next_tag - retstub_start;
    int retidx;
    ASSERT(native_exec_is_back_from_native(dcontext->next_tag));
    ASSERT(offset % BACK_FROM_NATIVE_RETSTUB_SIZE == 0);
    retidx = (int)offset / BACK_FROM_NATIVE_RETSTUB_SIZE;
    dcontext->next_tag = pop_retaddr_for_index(dcontext, retidx, xsp);
    LOG(THREAD, LOG_ASYNCH, 2,
        "%s: tried to interpret back_from_native, "
        "interpreting retaddr " PFX " instead\n",
        __FUNCTION__, dcontext->next_tag);
    ASSERT(!is_native_pc(dcontext->next_tag));
}

void
put_back_native_retaddrs(dcontext_t *dcontext)
{
    retaddr_and_retloc_t *retstack = dcontext->native_retstack;
    uint i;
    ASSERT(dcontext->native_retstack_cur < MAX_NATIVE_RETSTACK);
    for (i = 0; i < dcontext->native_retstack_cur; i++) {
        app_pc *retloc = (app_pc *)retstack[i].retloc;
        ASSERT(*retloc >= retstub_start && *retloc < retstub_end);
        LOG(THREAD, LOG_ASYNCH, 2, "%s: writing " PFX " over " PFX " @" PFX "\n",
            __FUNCTION__, retstack[i].retaddr, *retloc, retloc);
        *retloc = retstack[i].retaddr;
    }
    dcontext->native_retstack_cur = 0;
#ifdef HOT_PATCHING_INTERFACE
    /* In hotp_only mode, a thread can be !under_dynamo_control
     * and have no native_exec_retloc.  For hotp_only, there
     * should be no need to restore a return value on the stack
     * as the thread has been native from the start and not
     * half-way through as it would in the regular hot patching
     * mode, i.e., with the code cache.  See case 7681.
     */
    if (i == 0) {
        ASSERT(DYNAMO_OPTION(hotp_only));
    } else {
        ASSERT(!DYNAMO_OPTION(hotp_only));
    }
#endif
}
