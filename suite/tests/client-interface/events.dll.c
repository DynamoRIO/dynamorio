/* **********************************************************
 * Copyright (c) 2008-2009 VMware, Inc.  All rights reserved.
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

/*
 * API regression test that registers for all supported event callbacks
 * (except the nudge and security violation callback)
 */

#include "dr_api.h"
#ifdef LINUX
# include <signal.h>
#endif

static
void exit_event1(void)
{
    dr_printf("exit event 1\n");
    if (!dr_unregister_exit_event(exit_event1))
        dr_printf("unregister failed!\n");
}

static
void exit_event2(void)
{
    dr_printf("exit event 2\n");
    if (!dr_unregister_exit_event(exit_event2))
        dr_printf("unregister failed!\n");
}

static
void thread_init_event1(void *drcontext)
{
    dr_printf("thread init event 1\n");
    if (!dr_unregister_thread_init_event(thread_init_event1))
        dr_printf("unregister failed!\n");
}
static
void thread_init_event2(void *drcontext)
{
    dr_printf("thread init event 2\n");
    if (!dr_unregister_thread_init_event(thread_init_event2))
        dr_printf("unregister failed!\n");
}

static
void thread_exit_event1(void *drcontext)
{
    dr_printf("thread exit event 1\n");
    if (!dr_unregister_thread_exit_event(thread_exit_event1))
        dr_printf("unregister failed!\n");
}

static
void thread_exit_event2(void *drcontext)
{
    dr_printf("thread exit event 2\n");
    if (!dr_unregister_thread_exit_event(thread_exit_event2))
        dr_printf("unregister failed!\n");
}

#ifdef LINUX
static
void fork_init_event1(void *drcontext)
{
    dr_printf("fork init event 1\n");
    if (!dr_unregister_fork_init_event(fork_init_event1))
        dr_printf("unregister failed!\n");
}
static
void fork_init_event2(void *drcontext)
{
    dr_printf("fork init event 2\n");
    if (!dr_unregister_fork_init_event(fork_init_event2))
        dr_printf("unregister failed!\n");
}

#endif
static
dr_emit_flags_t bb_event1(void *dcontext, void *tag, instrlist_t *bb,
                          bool for_trace, bool translating)
{
    dr_printf("bb event 1\n");
    if (!dr_unregister_bb_event(bb_event1))
        dr_printf("unregister failed!\n");
    return DR_EMIT_DEFAULT;
}

static
dr_emit_flags_t bb_event2(void *dcontext, void *tag, instrlist_t *bb,
                          bool for_trace, bool translating)
{
    dr_printf("bb event 2\n");
    if (!dr_unregister_bb_event(bb_event2))
        dr_printf("unregister failed!\n");
    return DR_EMIT_DEFAULT;
}

static
dr_emit_flags_t trace_event1(void *dcontext, void *tag, instrlist_t *trace,
                             bool translating)
{
    dr_printf("trace event 1\n");
    if (!dr_unregister_trace_event(trace_event1))
        dr_printf("unregister failed!\n");
    return DR_EMIT_DEFAULT;
}

static
dr_emit_flags_t trace_event2(void *dcontext, void *tag, instrlist_t *trace,
                             bool translating)
{
    dr_printf("trace event 2\n");
    if (!dr_unregister_trace_event(trace_event2))
        dr_printf("unregister failed!\n");
    return DR_EMIT_DEFAULT;
}

static
dr_custom_trace_action_t end_trace_event1(void *dcontext, void *trace_tag, void *next_tag)
{
    dr_printf("end trace event 1\n");
    if (!dr_unregister_end_trace_event(end_trace_event1))
        dr_printf("unregister failed!\n");
    return 0;
}

static
dr_custom_trace_action_t end_trace_event2(void *dcontext, void *trace_tag, void *next_tag)
{
    dr_printf("end trace event 2\n");
    if (!dr_unregister_end_trace_event(end_trace_event2))
        dr_printf("unregister failed!\n");
    return 0;
}

static
void delete_event1(void *dcontext, void *tag)
{
    dr_printf("delete event 1\n");
    if (!dr_unregister_delete_event(delete_event1))
        dr_printf("unregister failed!\n");
}
static
void delete_event2(void *dcontext, void *tag)
{
    dr_printf("delete event 2\n");
    if (!dr_unregister_delete_event(delete_event2))
        dr_printf("unregister failed!\n");
}

static
void module_load_event1(void *drcontext, const module_data_t *info, bool loaded)
{
    dr_printf("module load event 1\n");
    if (!dr_unregister_module_load_event(module_load_event1))
        dr_printf("unregister failed!\n");
}

static
void module_load_event2(void *drcontext, const module_data_t *info, bool loaded)
{
    dr_printf("module load event 2\n");
    if (!dr_unregister_module_load_event(module_load_event2))
        dr_printf("unregister failed!\n");
}

static
void module_unload_event1(void *drcontext, const module_data_t *info)
{
    dr_printf("module unload event 1\n");
    if (!dr_unregister_module_unload_event(module_unload_event1))
        dr_printf("unregister failed!\n");
}

static
void module_unload_event2(void *drcontext, const module_data_t *info)
{
    dr_printf("module unload event 2\n");
    if (!dr_unregister_module_unload_event(module_unload_event2))
        dr_printf("unregister failed!\n");
}

static
bool pre_syscall_event1(void *drcontext, int sysnum)
{
    dr_printf("pre syscall event 1\n");
    if (!dr_unregister_pre_syscall_event(pre_syscall_event1))
        dr_printf("unregister failed!\n");
    return true;
}

static
bool pre_syscall_event2(void *drcontext, int sysnum)
{
    dr_printf("pre syscall event 2\n");
    if (!dr_unregister_pre_syscall_event(pre_syscall_event2))
        dr_printf("unregister failed!\n");
    return true;
}

static
void post_syscall_event1(void *drcontext, int sysnum)
{
    dr_printf("post syscall event 1\n");
    if (!dr_unregister_post_syscall_event(post_syscall_event1))
        dr_printf("unregister failed!\n");
}

static
void post_syscall_event2(void *drcontext, int sysnum)
{
    dr_printf("post syscall event 2\n");
    if (!dr_unregister_post_syscall_event(post_syscall_event2))
        dr_printf("unregister failed!\n");
}

static
bool filter_syscall_event1(void *drcontext, int sysnum)
{
    dr_printf("filter syscall event 1\n");
    if (!dr_unregister_filter_syscall_event(filter_syscall_event1))
        dr_printf("unregister failed!\n");
    return true;
}

static
bool filter_syscall_event2(void *drcontext, int sysnum)
{
    dr_printf("filter syscall event 2\n");
    if (!dr_unregister_filter_syscall_event(filter_syscall_event2))
        dr_printf("unregister failed!\n");
    return true;
}


#ifdef WINDOWS
static
void exception_event_redirect(void *dcontext, dr_exception_t *excpt)
{
    app_pc addr;
    dr_mcontext_t mcontext;
    module_data_t *data = dr_lookup_module_by_name("client.events.exe");
    dr_printf("exception event redirect\n");
    if (data == NULL) {
        dr_printf("couldn't find events.exe module\n");
        return;
    }
    addr = (app_pc)dr_get_proc_address(data->handle, "redirect");
    dr_free_module_data(data);
    mcontext = excpt->mcontext;
    mcontext.pc = addr;
    if (addr == NULL) {
        dr_printf("Couldn't find function redirect in events.exe\n");
        return;
    }
    dr_redirect_execution(&mcontext, 0);
    dr_printf("should not be reached, dr_redirect_execution() should not return\n");
}

static
void exception_event1(void *dcontext, dr_exception_t *excpt)
{
    if (excpt->record->ExceptionCode == STATUS_ACCESS_VIOLATION)
        dr_printf("exception event 1\n");

    if (!dr_unregister_exception_event(exception_event1))
        dr_printf("unregister failed!\n");

    /* ensure we get our deletion events */
    dr_flush_region(excpt->record->ExceptionAddress, 1);
}

static
void exception_event2(void *dcontext, dr_exception_t *excpt)
{    
    if (excpt->record->ExceptionCode == STATUS_ACCESS_VIOLATION)
        dr_printf("exception event 2\n");
    
    dr_register_exception_event(exception_event_redirect);

    if (!dr_unregister_exception_event(exception_event2))
        dr_printf("unregister failed!\n");
}
#else
static
dr_signal_action_t signal_event1(void *dcontext, dr_siginfo_t *info)
{
    dr_printf("signal event 1 sig=%d\n", info->sig);

    if (info->sig == SIGUSR2)
        return DR_SIGNAL_SUPPRESS;
    else if (info->sig == SIGURG) {
        if (!dr_unregister_signal_event(signal_event1))
            dr_printf("unregister failed!\n");
        return DR_SIGNAL_BYPASS;
    }
    return DR_SIGNAL_DELIVER;
}

static
dr_signal_action_t signal_event2(void *dcontext, dr_siginfo_t *info)
{    
    dr_printf("signal event 2 sig=%d\n", info->sig);
    
    if (!dr_unregister_signal_event(signal_event2))
        dr_printf("unregister failed!\n");

    return DR_SIGNAL_DELIVER;
}
#endif /* WINDOWS */

static void
restore_state_event1(void *drcontext, void *tag, dr_mcontext_t *mcontext,
                     bool restore_memory, bool app_code_consistent)
{
    dr_printf("in restore_state event 1\n");

    if (!dr_unregister_restore_state_event(restore_state_event1))
        dr_printf("unregister failed!\n");
}

static void
restore_state_event2(void *drcontext, void *tag, dr_mcontext_t *mcontext,
                     bool restore_memory, bool app_code_consistent)
{
    dr_printf("in restore_state event 2\n");

    if (!dr_unregister_restore_state_event(restore_state_event2))
        dr_printf("unregister failed!\n");
}

DR_EXPORT
void dr_init(client_id_t id)
{
    /* FIXME: we should test the nudge events as well, but that
     * would require some extra stuff our testing infrastructure
     * doesn't currently support.
     */

    dr_register_exit_event(exit_event1);
    dr_register_exit_event(exit_event2);
    dr_register_thread_init_event(thread_init_event1);
    dr_register_thread_init_event(thread_init_event2);
    dr_register_thread_exit_event(thread_exit_event1);
    dr_register_thread_exit_event(thread_exit_event2);
#ifdef LINUX
    dr_register_fork_init_event(fork_init_event1);
    dr_register_fork_init_event(fork_init_event2);
#endif
    dr_register_bb_event(bb_event1);
    dr_register_bb_event(bb_event2);
    dr_register_trace_event(trace_event1);
    dr_register_trace_event(trace_event2);
    dr_register_end_trace_event(end_trace_event1);
    dr_register_end_trace_event(end_trace_event2);
    dr_register_delete_event(delete_event1);
    dr_register_delete_event(delete_event2);
    dr_register_restore_state_event(restore_state_event1);
    dr_register_restore_state_event(restore_state_event2);
    dr_register_module_load_event(module_load_event1);
    dr_register_module_load_event(module_load_event2);
    dr_register_module_unload_event(module_unload_event1);
    dr_register_module_unload_event(module_unload_event2);
    dr_register_pre_syscall_event(pre_syscall_event1);
    dr_register_pre_syscall_event(pre_syscall_event2);
    dr_register_post_syscall_event(post_syscall_event1);
    dr_register_post_syscall_event(post_syscall_event2);
    dr_register_filter_syscall_event(filter_syscall_event1);
    dr_register_filter_syscall_event(filter_syscall_event2);
#ifdef WINDOWS
    dr_register_exception_event(exception_event1);
    dr_register_exception_event(exception_event2);
#else
    dr_register_signal_event(signal_event1);
    dr_register_signal_event(signal_event2);
#endif
}
