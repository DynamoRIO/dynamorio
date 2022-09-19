/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2008-2010 VMware, Inc.  All rights reserved.
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
#include "client_tools.h"

#ifdef UNIX
#    include <signal.h>
#    include <string.h>
#endif

#ifdef WINDOWS
#    pragma warning(disable : 4100) // unreferenced formal parameter
#    pragma warning(disable : 4127) // conditional expression is constant
#endif

/* We compile this test as C and C++ with different target names. */
#ifdef __cplusplus
#    define EVENTS "events_cpp"
#else
#    define EVENTS "events"
#endif

void *mutex;

enum event_seq {
    EVENT_MODULE_LOAD_1 = 0,
    EVENT_MODULE_LOAD_2,
    EVENT_THREAD_INIT_1,
    EVENT_THREAD_INIT_2,
    EVENT_BB_1,
    EVENT_BB_2,
    EVENT_END_TRACE_1,
    EVENT_END_TRACE_2,
    EVENT_TRACE_1,
    EVENT_TRACE_2,
    EVENT_DELETE_1,
    EVENT_DELETE_2,
    EVENT_FILTER_SYSCALL_1,
    EVENT_FILTER_SYSCALL_2,
    EVENT_PRE_SYSCALL_1,
    EVENT_PRE_SYSCALL_2,
    EVENT_POST_SYSCALL_1,
    EVENT_POST_SYSCALL_2,
    EVENT_KERNEL_XFER_1,
    EVENT_KERNEL_XFER_2,
    EVENT_MODULE_UNLOAD_1,
    EVENT_MODULE_UNLOAD_2,
    EVENT_THREAD_EXIT_1,
    EVENT_THREAD_EXIT_2,
    EVENT_FORK_INIT_1,
    EVENT_FORK_INIT_2,
    EVENT_SIGNAL_1,
    EVENT_SIGNAL_2,
    EVENT_EXCEPTION_1,
    EVENT_EXCEPTION_2,
    EVENT_RESTORE_STATE_1,
    EVENT_RESTORE_STATE_2,
    EVENT_RESTORE_STATE_EX_1,
    EVENT_RESTORE_STATE_EX_2,
    EVENT_last,
};

const char *const name[EVENT_last] = {
    "module load event 1",
    "module load event 2",
    "thread init event 1",
    "thread init event 2",
    "bb event 1",
    "bb event 2",
    "end trace event 1",
    "end trace event 2",
    "trace event 1",
    "trace event 2",
    "delete event 1",
    "delete event 2",
    "filter syscall event 1",
    "filter syscall event 2",
    "pre syscall event 1",
    "pre syscall event 2",
    "post syscall event 1",
    "post syscall event 2",
    "kernel xfer event 1",
    "kernel xfer event 2",
    "module unload event 1",
    "module unload event 2",
    "thread exit event 1",
    "thread exit event 2",
    "fork init event 1",
    "fork init event 2",
    "signal event 1",
    "signal event 2",
    "exception event 1",
    "exception event 2",
    "restore state event 1",
    "restore state event 2",
    "restore state ex event 1",
    "restore state ex event 2",
};

int counts[EVENT_last];

static void
inc_count_first(int first, int second)
{
    dr_mutex_lock(mutex);
    if (counts[second] == 0)
        dr_fprintf(STDERR, "%s is called before %s\n", name[first], name[second]);
    counts[first]++;
    dr_mutex_unlock(mutex);
}

static void
inc_count_second(int second)
{
    dr_mutex_lock(mutex);
    counts[second]++;
    dr_mutex_unlock(mutex);
}

static void
check_result(void)
{
    int i;
    for (i = 0; i < EVENT_last; i++) {
        if (counts[i] == 0)
            continue;
        if (counts[i] == 1)
            dr_fprintf(STDERR, "%s is called 1 time\n", name[i]);
        else
            dr_fprintf(STDERR, "%s is called >1 time\n", name[i]);
        dr_flush_file(STDOUT);
    }
}

static void
low_on_memory_event()
{
    /* Do nothing. Testing only register and unregister functions. */
}

static void
exit_event1(void)
{

    if (!dr_unregister_low_on_memory_event(low_on_memory_event))
        dr_fprintf(STDERR, "unregister failed!\n");
    dr_fprintf(STDERR, "exit event 1\n");
    dr_flush_file(STDOUT);

    if (!dr_unregister_exit_event(exit_event1))
        dr_fprintf(STDERR, "unregister failed!\n");
    check_result();
    dr_mutex_destroy(mutex);
}

static void
exit_event2(void)
{
    dr_fprintf(STDERR, "exit event 2\n");
    dr_flush_file(STDOUT);

    if (!dr_unregister_exit_event(exit_event2))
        dr_fprintf(STDERR, "unregister failed!\n");
}

static void
thread_init_event1(void *drcontext)
{
    inc_count_first(EVENT_THREAD_INIT_1, EVENT_THREAD_INIT_2);
    if (!dr_unregister_thread_init_event(thread_init_event1))
        dr_fprintf(STDERR, "unregister failed!\n");
}
static void
thread_init_event2(void *drcontext)
{
    inc_count_second(EVENT_THREAD_INIT_2);
    if (!dr_unregister_thread_init_event(thread_init_event2))
        dr_fprintf(STDERR, "unregister failed!\n");
}

static void
thread_exit_event1(void *drcontext)
{
    inc_count_first(EVENT_THREAD_EXIT_1, EVENT_THREAD_EXIT_2);
    if (!dr_unregister_thread_exit_event(thread_exit_event1))
        dr_fprintf(STDERR, "unregister failed!\n");
}

static void
thread_exit_event2(void *drcontext)
{
    inc_count_second(EVENT_THREAD_EXIT_2);
    if (!dr_unregister_thread_exit_event(thread_exit_event2))
        dr_fprintf(STDERR, "unregister failed!\n");
}

#ifdef UNIX
static void
fork_init_event1(void *drcontext)
{
    inc_count_first(EVENT_FORK_INIT_1, EVENT_FORK_INIT_2);
    if (!dr_unregister_fork_init_event(fork_init_event1))
        dr_fprintf(STDERR, "unregister failed!\n");
}
static void
fork_init_event2(void *drcontext)
{
    int i;
    dr_mutex_lock(mutex);
    for (i = 0; i < EVENT_last; i++)
        counts[i] = 0;
    counts[EVENT_FORK_INIT_2]++;
    dr_mutex_unlock(mutex);
    if (!dr_unregister_fork_init_event(fork_init_event2))
        dr_fprintf(STDERR, "unregister failed!\n");
}

#endif
static dr_emit_flags_t
bb_event1(void *dcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    inc_count_first(EVENT_BB_1, EVENT_BB_2);
    if (!dr_unregister_bb_event(bb_event1))
        dr_fprintf(STDERR, "unregister failed!\n");
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
bb_event2(void *dcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    inc_count_second(EVENT_BB_2);
    if (!dr_unregister_bb_event(bb_event2))
        dr_fprintf(STDERR, "unregister failed!\n");
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
trace_event1(void *dcontext, void *tag, instrlist_t *trace, bool translating)
{
    inc_count_first(EVENT_TRACE_1, EVENT_TRACE_2);
    if (!dr_unregister_trace_event(trace_event1))
        dr_fprintf(STDERR, "unregister failed!\n");
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
trace_event2(void *dcontext, void *tag, instrlist_t *trace, bool translating)
{
    inc_count_second(EVENT_TRACE_2);
    if (!dr_unregister_trace_event(trace_event2))
        dr_fprintf(STDERR, "unregister failed!\n");
    return DR_EMIT_DEFAULT;
}

static dr_custom_trace_action_t
end_trace_event1(void *dcontext, void *trace_tag, void *next_tag)
{
    inc_count_first(EVENT_END_TRACE_1, EVENT_END_TRACE_2);
    if (!dr_unregister_end_trace_event(end_trace_event1))
        dr_fprintf(STDERR, "unregister failed!\n");
    return CUSTOM_TRACE_DR_DECIDES;
}

static dr_custom_trace_action_t
end_trace_event2(void *dcontext, void *trace_tag, void *next_tag)
{
    inc_count_second(EVENT_END_TRACE_2);
    if (!dr_unregister_end_trace_event(end_trace_event2))
        dr_fprintf(STDERR, "unregister failed!\n");
    return CUSTOM_TRACE_DR_DECIDES;
}

static void
delete_event1(void *dcontext, void *tag)
{
    inc_count_first(EVENT_DELETE_1, EVENT_DELETE_2);
    if (!dr_unregister_delete_event(delete_event1))
        dr_fprintf(STDERR, "unregister failed!\n");
}

static void
delete_event2(void *dcontext, void *tag)
{
    inc_count_second(EVENT_DELETE_2);
    if (!dr_unregister_delete_event(delete_event2))
        dr_fprintf(STDERR, "unregister failed!\n");
}

static void
module_load_event_perm(void *drcontext, const module_data_t *info, bool loaded)
{
    /* Test i#138 */
    if (info->full_path == NULL || info->full_path[0] == '\0')
        dr_fprintf(STDERR, "ERROR: full_path empty for %s\n",
                   dr_module_preferred_name(info));
#ifdef WINDOWS
    /* We do not expect \\server-style paths for this test */
    else if (info->full_path[0] == '\\' || info->full_path[1] != ':')
        dr_fprintf(STDERR, "ERROR: full_path is not in DOS format: %s\n",
                   info->full_path);
#else
    else if (info->full_path[0] != '/' && strcmp(info->full_path, "[vdso]") != 0)
        dr_fprintf(STDERR, "ERROR: full_path is not absolute: %s\n", info->full_path);
#endif
}

static void
module_load_event1(void *drcontext, const module_data_t *info, bool loaded)
{
    inc_count_first(EVENT_MODULE_LOAD_1, EVENT_MODULE_LOAD_2);
    if (!dr_unregister_module_load_event(module_load_event1))
        dr_fprintf(STDERR, "unregister failed!\n");
}

static void
module_load_event2(void *drcontext, const module_data_t *info, bool loaded)
{
    inc_count_second(EVENT_MODULE_LOAD_2);
    if (!dr_unregister_module_load_event(module_load_event2))
        dr_fprintf(STDERR, "unregister failed!\n");
}

static void
module_unload_event1(void *drcontext, const module_data_t *info)
{
    inc_count_first(EVENT_MODULE_UNLOAD_1, EVENT_MODULE_UNLOAD_2);
    if (!dr_unregister_module_unload_event(module_unload_event1))
        dr_fprintf(STDERR, "unregister failed!\n");
}

static void
module_unload_event2(void *drcontext, const module_data_t *info)
{
    inc_count_second(EVENT_MODULE_UNLOAD_2);
    if (!dr_unregister_module_unload_event(module_unload_event2))
        dr_fprintf(STDERR, "unregister failed!\n");
}

static bool
pre_syscall_event1(void *drcontext, int sysnum)
{
    inc_count_first(EVENT_PRE_SYSCALL_1, EVENT_PRE_SYSCALL_2);
    if (!dr_unregister_pre_syscall_event(pre_syscall_event1))
        dr_fprintf(STDERR, "unregister failed!\n");
    return true;
}

static bool
pre_syscall_event2(void *drcontext, int sysnum)
{
    inc_count_second(EVENT_PRE_SYSCALL_2);
    if (!dr_unregister_pre_syscall_event(pre_syscall_event2))
        dr_fprintf(STDERR, "unregister failed!\n");
    return true;
}

static void
post_syscall_event1(void *drcontext, int sysnum)
{
    inc_count_first(EVENT_POST_SYSCALL_1, EVENT_POST_SYSCALL_2);
    if (!dr_unregister_post_syscall_event(post_syscall_event1))
        dr_fprintf(STDERR, "unregister failed!\n");
}

static void
post_syscall_event2(void *drcontext, int sysnum)
{
    inc_count_second(EVENT_POST_SYSCALL_2);
    if (!dr_unregister_post_syscall_event(post_syscall_event2))
        dr_fprintf(STDERR, "unregister failed!\n");
}

static bool
filter_syscall_event3(void *drcontext, int sysnum)
{
    return false;
}

static bool
filter_syscall_event1(void *drcontext, int sysnum)
{
    inc_count_first(EVENT_FILTER_SYSCALL_1, EVENT_FILTER_SYSCALL_2);
    if (!dr_unregister_filter_syscall_event(filter_syscall_event1))
        dr_fprintf(STDERR, "unregister failed!\n");
    return true;
}

static bool
filter_syscall_event2(void *drcontext, int sysnum)
{
    inc_count_second(EVENT_FILTER_SYSCALL_2);
    if (!dr_unregister_filter_syscall_event(filter_syscall_event2))
        dr_fprintf(STDERR, "unregister failed!\n");
    /* We register another filter to avoid DR asserting that we have a
     * syscall event and no filter.
     */
    dr_register_filter_syscall_event(filter_syscall_event3);
    return true;
}

static void
kernel_xfer_event1(void *drcontext, const dr_kernel_xfer_info_t *info)
{
    inc_count_first(EVENT_KERNEL_XFER_1, EVENT_KERNEL_XFER_2);
    if (!dr_unregister_kernel_xfer_event(kernel_xfer_event1))
        dr_fprintf(STDERR, "unregister failed!\n");
}

static void
kernel_xfer_event2(void *drcontext, const dr_kernel_xfer_info_t *info)
{
    inc_count_second(EVENT_KERNEL_XFER_2);
    dr_log(drcontext, DR_LOG_ALL, 2, "%s: %d %p to %p sp=%zx\n", __FUNCTION__, info->type,
           info->source_mcontext == NULL ? 0 : info->source_mcontext->pc, info->target_pc,
           info->target_xsp);
    if (info->type == DR_XFER_CLIENT_REDIRECT) {
        /* Test for exception event redirect. */
        ASSERT(info->source_mcontext != NULL);
        dr_mcontext_t mc;
        mc.size = sizeof(mc);
        mc.flags = DR_MC_CONTROL;
        bool ok = dr_get_mcontext(drcontext, &mc);
        ASSERT(ok);
        ASSERT(mc.pc == info->target_pc);
        ASSERT(mc.xsp == info->target_xsp);
        mc.flags = DR_MC_ALL;
        ok = dr_get_mcontext(drcontext, &mc);
        ASSERT(ok);
    }
}

#ifdef WINDOWS
static bool
exception_event_redirect(void *dcontext, dr_exception_t *excpt)
{
    app_pc addr;
    dr_mcontext_t mcontext;
    module_data_t *data = dr_lookup_module_by_name("client." EVENTS ".exe");
    dr_fprintf(STDERR, "exception event redirect\n");
    if (data == NULL) {
        dr_fprintf(STDERR, "couldn't find " EVENTS ".exe module\n");
        return true;
    }
    addr = (app_pc)dr_get_proc_address(data->handle, "redirect");
    dr_free_module_data(data);
    mcontext = *excpt->mcontext;
    mcontext.pc = addr;
    if (addr == NULL) {
        dr_fprintf(STDERR, "Couldn't find function redirect in " EVENTS ".exe\n");
        return true;
    }
#    ifdef X86_64
    /* align properly in case redirect function relies on conventions (i#419) */
    mcontext.xsp = ALIGN_BACKWARD(mcontext.xsp, 16) - sizeof(void *);
#    endif
    dr_redirect_execution(&mcontext);
    dr_fprintf(STDERR,
               "should not be reached, dr_redirect_execution() should not return\n");
    return true;
}

static bool
exception_event1(void *dcontext, dr_exception_t *excpt)
{
    if (excpt->record->ExceptionCode == STATUS_ACCESS_VIOLATION)
        inc_count_first(EVENT_EXCEPTION_1, EVENT_EXCEPTION_2);

    if (!dr_unregister_exception_event(exception_event1))
        dr_fprintf(STDERR, "unregister failed!\n");

    /* ensure we get our deletion events */
    dr_flush_region((app_pc)excpt->record->ExceptionAddress, 1);
    return true;
}

static bool
exception_event2(void *dcontext, dr_exception_t *excpt)
{
    if (excpt->record->ExceptionCode == STATUS_ACCESS_VIOLATION)
        inc_count_second(EVENT_EXCEPTION_2);

    dr_register_exception_event(exception_event_redirect);

    if (!dr_unregister_exception_event(exception_event2))
        dr_fprintf(STDERR, "unregister failed!\n");
    return true;
}
#else
static dr_signal_action_t
signal_event_redirect(void *dcontext, dr_siginfo_t *info)
{
    if (info->sig == SIGSEGV) {
        app_pc addr;
        module_data_t *data = dr_lookup_module_by_name("client." EVENTS);
        dr_fprintf(STDERR, "signal event redirect\n");
        if (data == NULL) {
            dr_fprintf(STDERR, "couldn't find client." EVENTS " module\n");
            return DR_SIGNAL_DELIVER;
        }
        addr = (app_pc)dr_get_proc_address(data->handle, "redirect");
        dr_free_module_data(data);
        if (addr == NULL) {
            dr_fprintf(STDERR, "Couldn't find function redirect in client." EVENTS "\n");
            return DR_SIGNAL_DELIVER;
        }
#    ifdef X86_64
        /* align properly in case redirect function relies on conventions (i#384) */
        info->mcontext->xsp = ALIGN_BACKWARD(info->mcontext->xsp, 16) - sizeof(void *);
#    endif
        info->mcontext->pc = addr;
        return DR_SIGNAL_REDIRECT;
    }
    return DR_SIGNAL_DELIVER;
}

static dr_signal_action_t
signal_event1(void *dcontext, dr_siginfo_t *info)
{
    inc_count_first(EVENT_SIGNAL_1, EVENT_SIGNAL_2);
    if (info->sig == SIGUSR2)
        return DR_SIGNAL_SUPPRESS;
    else if (info->sig == SIGURG) {
        if (!dr_unregister_signal_event(signal_event1))
            dr_fprintf(STDERR, "unregister failed!\n");
        dr_register_signal_event(signal_event_redirect);
        return DR_SIGNAL_BYPASS;
    }
    return DR_SIGNAL_DELIVER;
}

static dr_signal_action_t
signal_event2(void *dcontext, dr_siginfo_t *info)
{
    inc_count_second(EVENT_SIGNAL_2);

    if (!dr_unregister_signal_event(signal_event2))
        dr_fprintf(STDERR, "unregister failed!\n");

    return DR_SIGNAL_DELIVER;
}
#endif /* WINDOWS */

static void
restore_state_event1(void *drcontext, void *tag, dr_mcontext_t *mcontext,
                     bool restore_memory, bool app_code_consistent)
{
    inc_count_first(EVENT_RESTORE_STATE_1, EVENT_RESTORE_STATE_2);

    if (!dr_unregister_restore_state_event(restore_state_event1))
        dr_fprintf(STDERR, "unregister failed!\n");
}

static void
restore_state_event2(void *drcontext, void *tag, dr_mcontext_t *mcontext,
                     bool restore_memory, bool app_code_consistent)
{
    inc_count_second(EVENT_RESTORE_STATE_2);

    if (!dr_unregister_restore_state_event(restore_state_event2))
        dr_fprintf(STDERR, "unregister failed!\n");
}

static bool
restore_state_ex_event1(void *drcontext, bool restore_memory,
                        dr_restore_state_info_t *info)
{
    /* i#488: test bool compatibility */
    bool is_trace = info->fragment_info.is_trace;
    if (sizeof(is_trace) != sizeof(info->fragment_info.is_trace))
        dr_fprintf(STDERR, "bool size incompatibility %d!\n", is_trace /*force ref*/);

    inc_count_first(EVENT_RESTORE_STATE_EX_1, EVENT_RESTORE_STATE_EX_2);

    if (!dr_unregister_restore_state_ex_event(restore_state_ex_event1))
        dr_fprintf(STDERR, "unregister failed!\n");
    return true;
}

static bool
restore_state_ex_event2(void *drcontext, bool restore_memory,
                        dr_restore_state_info_t *info)
{
    inc_count_second(EVENT_RESTORE_STATE_EX_2);

    if (!dr_unregister_restore_state_ex_event(restore_state_ex_event2))
        dr_fprintf(STDERR, "unregister failed!\n");
    return true;
}

static size_t
event_persist_size(void *drcontext, void *perscxt, size_t file_offs, void **user_data OUT)
{
    return 0;
}

static bool
event_persist_patch(void *drcontext, void *perscxt, byte *bb_start, size_t bb_size,
                    void *user_data)
{
    return true;
}

static bool
event_persist(void *drcontext, void *perscxt, file_t fd, void *user_data)
{
    return true;
}

static bool
event_resurrect(void *drcontext, void *perscxt, byte **map INOUT)
{
    return true;
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    /* FIXME: we should test the nudge events as well, but that
     * would require some extra stuff our testing infrastructure
     * doesn't currently support.
     */
    int i;

#ifdef WINDOWS
    dr_os_version_info_t info = {
        sizeof(info),
    };
    if (dr_is_notify_on())
        dr_enable_console_printing();
    /* a sanity check for console printing: no easy way to ensure it really
     * prints to cmd w/o redirecting to a file which then ruins the test so we
     * just make sure it doesn't mess up our broadest test, events.
     */
    if (!dr_get_os_version(&info))
        dr_fprintf(STDERR, "dr_get_os_version failed!\n");
    if (info.build_number == 0 || info.edition[0] == '\0')
        dr_fprintf(STDERR, "dr_get_os_version failed to get new fields\n");
#endif

    for (i = 0; i < EVENT_last; i++)
        counts[i] = 0;
    mutex = dr_mutex_create();
    dr_register_exit_event(exit_event1);
    dr_register_exit_event(exit_event2);
    dr_register_thread_init_event(thread_init_event1);
    dr_register_thread_init_event(thread_init_event2);
    dr_register_thread_exit_event(thread_exit_event1);
    dr_register_thread_exit_event(thread_exit_event2);
#ifdef UNIX
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
    dr_register_restore_state_ex_event(restore_state_ex_event1);
    dr_register_restore_state_ex_event(restore_state_ex_event2);
    dr_register_module_load_event(module_load_event_perm);
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
    dr_register_kernel_xfer_event(kernel_xfer_event1);
    dr_register_kernel_xfer_event(kernel_xfer_event2);
    dr_register_low_on_memory_event(low_on_memory_event);
#ifdef WINDOWS
    dr_register_exception_event(exception_event1);
    dr_register_exception_event(exception_event2);
#else
    dr_register_signal_event(signal_event1);
    dr_register_signal_event(signal_event2);
#endif

    if (!dr_register_persist_ro(event_persist_size, event_persist, event_resurrect))
        dr_fprintf(STDERR, "failed to register for persist ro events");
    if (!dr_register_persist_rx(event_persist_size, event_persist, event_resurrect))
        dr_fprintf(STDERR, "failed to register for persist rx events");
    if (!dr_register_persist_rw(event_persist_size, event_persist, event_resurrect))
        dr_fprintf(STDERR, "failed to register for persist rw events");
    if (!dr_register_persist_patch(event_persist_patch))
        dr_fprintf(STDERR, "failed to register for persist patch event");
    if (!dr_unregister_persist_ro(event_persist_size, event_persist, event_resurrect))
        dr_fprintf(STDERR, "failed to unregister for persist ro events");
    if (!dr_unregister_persist_rx(event_persist_size, event_persist, event_resurrect))
        dr_fprintf(STDERR, "failed to unregister for persist rx events");
    if (!dr_unregister_persist_rw(event_persist_size, event_persist, event_resurrect))
        dr_fprintf(STDERR, "failed to unregister for persist rw events");
    if (!dr_unregister_persist_patch(event_persist_patch))
        dr_fprintf(STDERR, "failed to unregister for persist patch event");

#ifdef LINUX
    /* On Linux, where we have a clear distinction between DR launching the process
     * with zero threads and a later attach where there are threads, make sure
     * the post_attach event return value can be used by clients.
     */
    if (dr_register_post_attach_event(exit_event1))
        dr_fprintf(STDERR, "should fail to register for post-attach event");
    if (dr_unregister_post_attach_event(exit_event1))
        dr_fprintf(STDERR, "should fail to unregister for post-attach event");
#endif
}
