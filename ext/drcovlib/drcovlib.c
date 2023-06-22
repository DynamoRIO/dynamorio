/* ***************************************************************************
 * Copyright (c) 2012-2023 Google, Inc.  All rights reserved.
 * ***************************************************************************/

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

/* Code Coverage Library
 *
 * Collects information about basic blocks that have been executed.
 * It simply stores the information of basic blocks seen in bb callback event
 * into a table without any instrumentation, and dumps the buffer into log files
 * on thread/process exit.
 *
 * There are pros and cons to creating this coverage library as opposed to other
 * tools using the drcov client straight-up as a 2nd client: DR has support for
 * multiple clients, after all, and drcovlib here is simply writing to a file
 * anyway, more like an end tool than a library that returns raw coverage data.
 * However, making this a library makes it easier to share parsing code for
 * postprocessing tools and makes it easier to export the module table in the
 * future.  Other factors into the decision are whether large tools like
 * Dr. Memory want to use a shared library model, which is required for 2
 * clients and which complicates deployment.  Since Dr. Memory has its own
 * frontend, even a 2-client model still requires special-case support inside
 * the Dr. Memory code base.
 */

#include "dr_api.h"
#include "drmgr.h"
#include "drx.h"
#include "drcovlib.h"
#include "hashtable.h"
#include "drtable.h"
#include "modules.h"
#include "drcovlib_private.h"
#ifdef LINUX
#    include "../../core/unix/include/syscall.h"
#elif defined(UNIX)
#    include <sys/syscall.h>
#endif
#include <limits.h>
#include <string.h>

#define UNKNOWN_MODULE_ID USHRT_MAX

/* This is not exposed: internal use only */
uint verbose;

#define OPTION_MAX_LENGTH MAXIMUM_PATH

static drcovlib_options_t options;
static char logdir[MAXIMUM_PATH];

typedef struct _per_thread_t {
    void *bb_table;
    file_t log;
    char logname[MAXIMUM_PATH];
} per_thread_t;

static per_thread_t *global_data;
static bool drcov_per_thread = false;
static volatile bool go_native;
static int tls_idx = -1;
static int drcovlib_init_count;

/****************************************************************************
 * Utility Functions
 */
static file_t
log_file_create_helper(void *drcontext, const char *suffix, char *buf, size_t buf_els)
{
    file_t log = drx_open_unique_appid_file(
        options.logdir,
        drcontext == NULL ? dr_get_process_id() : dr_get_thread_id(drcontext),
        options.logprefix, suffix,
#ifndef WINDOWS
        DR_FILE_CLOSE_ON_FORK |
#endif
            DR_FILE_ALLOW_LARGE,
        buf, buf_els);
    if (log != INVALID_FILE) {
        dr_log(drcontext, DR_LOG_ALL, 1, "drcov: log file is %s\n", buf);
        NOTIFY(1, "<created log file %s>\n", buf);
    }
    return log;
}

static void
log_file_create(void *drcontext, per_thread_t *data)
{
    data->log =
        log_file_create_helper(drcontext, drcontext == NULL ? "proc.log" : "thd.log",
                               data->logname, BUFFER_SIZE_ELEMENTS(data->logname));
}

/****************************************************************************
 * BB Table Functions
 */

static bool
bb_table_entry_print(ptr_uint_t idx, void *entry, void *iter_data)
{
    per_thread_t *data = iter_data;
    bb_entry_t *bb_entry = (bb_entry_t *)entry;
    dr_fprintf(data->log, "module[%3u]: " PFX ", %3u", bb_entry->mod_id, bb_entry->start,
               bb_entry->size);
    dr_fprintf(data->log, "\n");
    return true; /* continue iteration */
}

static void
bb_table_print(void *drcontext, per_thread_t *data)
{
    ASSERT(data != NULL, "data must not be NULL");
    if (data->log == INVALID_FILE) {
        ASSERT(false, "invalid log file");
        return;
    }
    /* We do not support >32U-bit-max (~4 billion) blocks.
     * drcov2lcov would need a number of changes to support this.
     * We have a debug-build check here.
     */
    ASSERT(drtable_num_entries(data->bb_table) <= UINT_MAX,
           "block count exceeds 32-bit max");
    dr_fprintf(data->log, "BB Table: %u bbs\n",
               (uint)drtable_num_entries(data->bb_table));
    if (TEST(DRCOVLIB_DUMP_AS_TEXT, options.flags)) {
        dr_fprintf(data->log, "module id, start, size:\n");
        drtable_iterate(data->bb_table, data, bb_table_entry_print);
    } else
        drtable_dump_entries(data->bb_table, data->log);
}

static void
bb_table_entry_add(void *drcontext, per_thread_t *data, app_pc start, uint size)
{
    bb_entry_t *bb_entry = drtable_alloc(data->bb_table, 1, NULL);
    uint mod_id;
    app_pc mod_seg_start;
    drcovlib_status_t res =
        drmodtrack_lookup_segment(drcontext, start, &mod_id, &mod_seg_start);
    /* we do not de-duplicate repeated bbs */
    ASSERT(size < USHRT_MAX, "size overflow");
    bb_entry->size = (ushort)size;
    if (res == DRCOVLIB_SUCCESS) {
        ASSERT(mod_id < USHRT_MAX, "module id overflow");
        bb_entry->mod_id = (ushort)mod_id;
        ASSERT(start >= mod_seg_start, "wrong module");
        bb_entry->start = (uint)(start - mod_seg_start);
    } else {
        /* XXX: we just truncate the address, which may have wrong value
         * in x64 arch. It should be ok now since it is an unknown module,
         * which will be ignored in the post-processing.
         * Should be handled for JIT code in the future.
         */
        bb_entry->mod_id = UNKNOWN_MODULE_ID;
        bb_entry->start = (uint)(ptr_uint_t)start;
    }
}

#define INIT_BB_TABLE_ENTRIES 4096
static void *
bb_table_create(bool synch)
{
    return drtable_create(INIT_BB_TABLE_ENTRIES, sizeof(bb_entry_t), 0 /* flags */, synch,
                          NULL);
}

static void
bb_table_destroy(void *table, void *data)
{
    drtable_destroy(table, data);
}

static void
version_print(file_t log)
{
    if (log == INVALID_FILE) {
        /* It is possible that failure on log file creation is caused by the
         * running process not having enough privilege, so this is not a
         * release-build fatal error
         */
        ASSERT(false, "invalid log file");
        return;
    }
    dr_fprintf(log, "DRCOV VERSION: %d\n", DRCOV_VERSION);
    dr_fprintf(log, "DRCOV FLAVOR: %s\n", DRCOV_FLAVOR);
}

static void
dump_drcov_data(void *drcontext, per_thread_t *data)
{
    if (data->log == INVALID_FILE) {
        /* It is possible that failure on log file creation is caused by the
         * running process not having enough privilege, so this is not a
         * release-build fatal error
         */
        ASSERT(false, "invalid log file");
        return;
    }
    version_print(data->log);
    drmodtrack_dump(data->log);
    bb_table_print(drcontext, data);
}

/****************************************************************************
 * Thread/Global Data Creation/Destroy
 */

/* make a copy of global data for pre-thread cache */
static per_thread_t *
thread_data_copy(void *drcontext)
{
    per_thread_t *data;
    ASSERT(drcontext != NULL, "drcontext must not be NULL");
    data = dr_thread_alloc(drcontext, sizeof(*data));
    *data = *global_data;
    return data;
}

static per_thread_t *
thread_data_create(void *drcontext)
{
    per_thread_t *data;
    if (drcontext == NULL) {
        ASSERT(!drcov_per_thread, "drcov_per_thread should not be set");
        data = dr_global_alloc(sizeof(*data));
    } else {
        ASSERT(drcov_per_thread, "drcov_per_thread should be set");
        data = dr_thread_alloc(drcontext, sizeof(*data));
    }
    /* XXX: can we assume bb create event is serialized,
     * if so, no lock is required for bb_table operation.
     */
    data->bb_table = bb_table_create(drcontext == NULL ? true : false);
    log_file_create(drcontext, data);
    return data;
}

static void
thread_data_destroy(void *drcontext, per_thread_t *data)
{
    /* destroy the bb table */
    bb_table_destroy(data->bb_table, data);
    dr_close_file(data->log);
    /* free thread data */
    if (drcontext == NULL) {
        ASSERT(!drcov_per_thread, "drcov_per_thread should not be set");
        dr_global_free(data, sizeof(*data));
    } else {
        ASSERT(drcov_per_thread, "drcov_per_thread is not set");
        dr_thread_free(drcontext, data, sizeof(*data));
    }
}

static void *
global_data_create(void)
{
    return thread_data_create(NULL);
}

static void
global_data_destroy(per_thread_t *data)
{
    thread_data_destroy(NULL, data);
}

/****************************************************************************
 * Event Callbacks
 */

static bool
event_filter_syscall(void *drcontext, int sysnum)
{
#ifdef WINDOWS
    return false;
#else
    return sysnum == SYS_execve;
#endif
}

static bool
event_pre_syscall(void *drcontext, int sysnum)
{
#ifdef UNIX
    if (sysnum == SYS_execve) {
        /* for !drcov_per_thread, the per-thread data is a copy of global data */
        per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
        ASSERT(data != NULL, "data must not be NULL");
        if (!drcov_per_thread)
            drcontext = NULL;
        /* We only dump the data but do not free any memory.
         * XXX: for drcov_per_thread, we only dump the current thread.
         * XXX: We don't handle the syscall failing.
         */
        dump_drcov_data(drcontext, data);
        /* TODO: add execve test.
         * i#1390-c#8: iterate over all the other threads using DR API and dump data.
         * i#1390-c#9: update drcov2lcov to handle multiple dumps in the same file.
         */
    }
#endif
    return true;
}

/* We collect the basic block information including offset from module base,
 * size, and num of instructions, and add it into a basic block table without
 * instrumentation.
 */
static dr_emit_flags_t
event_basic_block_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                           bool translating, OUT void **user_data)
{
    per_thread_t *data;
    instr_t *instr;
    app_pc tag_pc, start_pc, end_pc;

    /* do nothing for translation */
    if (translating)
        return DR_EMIT_DEFAULT;

    data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    /* Collect the number of instructions and the basic block size,
     * assuming the basic block does not have any elision on control
     * transfer instructions, which is true for default options passed
     * to DR but not for -opt_speed.
     */
    /* We separate the tag from the instr pc ranges to handle displaced code
     * such as for the vsyscall hook.
     */
    tag_pc = dr_fragment_app_pc(tag);
    start_pc = instr_get_app_pc(instrlist_first_app(bb));
    end_pc = start_pc; /* for finding the size */
    for (instr = instrlist_first_app(bb); instr != NULL;
         instr = instr_get_next_app(instr)) {
        app_pc pc = instr_get_app_pc(instr);
        int len = instr_length(drcontext, instr);
        /* -opt_speed (elision) is not supported */
        /* For rep str expansion pc may be one back from start pc but equal to the tag. */
        ASSERT(pc != NULL && (pc >= start_pc || pc == tag_pc),
               "-opt_speed is not supported");
        if (pc + len > end_pc)
            end_pc = pc + len;
    }
    /* We allow duplicated basic blocks for the following reasons:
     * 1. Avoids handling issues like code cache consistency, e.g.,
     *    module load/unload, self-modifying code, etc.
     * 2. Avoids the overhead on duplication check.
     * 3. Stores more information on code cache events, e.g., trace building,
     *    repeated bb building, etc.
     * 4. The duplication can be easily handled in a post-processing step,
     *    which is required anyway.
     */
    bb_table_entry_add(drcontext, data, tag_pc, (uint)(end_pc - start_pc));

    if (go_native)
        return DR_EMIT_GO_NATIVE;
    else
        return DR_EMIT_DEFAULT;
}

static void
event_thread_exit(void *drcontext)
{
    per_thread_t *data;

    data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    ASSERT(data != NULL, "data must not be NULL");

    if (drcov_per_thread) {
        dump_drcov_data(drcontext, data);
        thread_data_destroy(drcontext, data);
    } else {
        /* the per-thread data is a copy of global data */
        dr_thread_free(drcontext, data, sizeof(*data));
    }
}

static void
event_thread_init(void *drcontext)
{
    per_thread_t *data;
    static volatile int thread_count;

    if (options.native_until_thread > 0) {
        int local_count = dr_atomic_add32_return_sum(&thread_count, 1);
        NOTIFY(1, "@@@@@@@@@@@@@ new thread #%d " TIDFMT "\n", local_count,
               dr_get_thread_id(drcontext));
        if (go_native && local_count == options.native_until_thread) {
            void **drcontexts = NULL;
            uint num_threads, i;
            go_native = false;
            NOTIFY(1, "thread " TIDFMT " suspending all threads\n",
                   dr_get_thread_id(drcontext));
            if (dr_suspend_all_other_threads_ex(&drcontexts, &num_threads, NULL,
                                                DR_SUSPEND_NATIVE)) {
                NOTIFY(1, "suspended %d threads\n", num_threads);
                for (i = 0; i < num_threads; i++) {
                    if (dr_is_thread_native(drcontexts[i])) {
                        NOTIFY(2, "\txxx taking over thread #%d " TIDFMT "\n", i,
                               dr_get_thread_id(drcontexts[i]));
                        dr_retakeover_suspended_native_thread(drcontexts[i]);
                    } else {
                        NOTIFY(2, "\tthread #%d " TIDFMT " under DR\n", i,
                               dr_get_thread_id(drcontexts[i]));
                    }
                }
                if (!dr_resume_all_other_threads(drcontexts, num_threads)) {
                    ASSERT(false, "failed to resume threads");
                }
            } else {
                ASSERT(false, "failed to suspend threads");
            }
        }
    }
    /* allocate thread private data for per-thread cache */
    if (drcov_per_thread)
        data = thread_data_create(drcontext);
    else
        data = thread_data_copy(drcontext);
    drmgr_set_tls_field(drcontext, tls_idx, data);
}

#ifndef WINDOWS
static void
event_fork(void *drcontext)
{
    if (!drcov_per_thread) {
        log_file_create(NULL, global_data);
    } else {
        per_thread_t *data = drmgr_get_tls_field(drcontext, tls_idx);
        if (data != NULL) {
            thread_data_destroy(drcontext, data);
        }
        event_thread_init(drcontext);
    }
}
#endif

drcovlib_status_t
drcovlib_logfile(void *drcontext, OUT const char **path)
{
    if (path == NULL)
        return DRCOVLIB_ERROR_INVALID_PARAMETER;
    if (drcontext != NULL) {
        per_thread_t *data;
        if (!drcov_per_thread)
            return DRCOVLIB_ERROR_INVALID_PARAMETER;
        data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
        ASSERT(data != NULL, "data must not be NULL");
        *path = data->logname;
    } else {
        if (drcov_per_thread)
            return DRCOVLIB_ERROR_INVALID_PARAMETER;
        *path = global_data->logname;
    }
    return DRCOVLIB_SUCCESS;
}

drcovlib_status_t
drcovlib_dump(void *drcontext)
{
    if (drcontext != NULL) {
        per_thread_t *data;
        if (!drcov_per_thread)
            return DRCOVLIB_ERROR_INVALID_PARAMETER;
        data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
        ASSERT(data != NULL, "data must not be NULL");
        dump_drcov_data(drcontext, data);
    } else {
        if (drcov_per_thread)
            return DRCOVLIB_ERROR_INVALID_PARAMETER;
        dump_drcov_data(drcontext, global_data);
    }
    return DRCOVLIB_SUCCESS;
}

drcovlib_status_t
drcovlib_exit(void)
{
    int count = dr_atomic_add32_return_sum(&drcovlib_init_count, -1);
    if (count != 0)
        return DRCOVLIB_SUCCESS;

    if (!drcov_per_thread) {
        dump_drcov_data(NULL, global_data);
        global_data_destroy(global_data);
    }
    drcov_per_thread = false;

    /* destroy module table */
    drmodtrack_exit();

    drmgr_unregister_tls_field(tls_idx);

    drx_exit();
    drmgr_exit();

    return DRCOVLIB_SUCCESS;
}

static drcovlib_status_t
event_init(void)
{
    drcovlib_status_t res;
    uint64 max_elide_jmp = 0;
    uint64 max_elide_call = 0;
    /* assuming no elision */
    if (!dr_get_integer_option("max_elide_jmp", &max_elide_jmp) ||
        !dr_get_integer_option("max_elide_call", &max_elide_jmp) || max_elide_jmp != 0 ||
        max_elide_call != 0)
        return DRCOVLIB_ERROR_INVALID_SETUP;

    /* create module table */
    res = drmodtrack_init();
    if (res != DRCOVLIB_SUCCESS)
        return res;

    /* create process data if whole process bb coverage. */
    if (!drcov_per_thread)
        global_data = global_data_create();
    return DRCOVLIB_SUCCESS;
}

drcovlib_status_t
drcovlib_init(drcovlib_options_t *ops)
{
    int count = dr_atomic_add32_return_sum(&drcovlib_init_count, 1);
    if (count > 1)
        return DRCOVLIB_SUCCESS;

    if (ops->struct_size != sizeof(options))
        return DRCOVLIB_ERROR_INVALID_PARAMETER;
    if ((ops->flags & (~(DRCOVLIB_DUMP_AS_TEXT | DRCOVLIB_THREAD_PRIVATE))) != 0)
        return DRCOVLIB_ERROR_INVALID_PARAMETER;
    if (TEST(DRCOVLIB_THREAD_PRIVATE, ops->flags)) {
        if (!dr_using_all_private_caches())
            return DRCOVLIB_ERROR_INVALID_SETUP;
        drcov_per_thread = true;
    }
    options = *ops;
    if (options.logdir != NULL)
        dr_snprintf(logdir, BUFFER_SIZE_ELEMENTS(logdir), "%s", ops->logdir);
    else /* default */
        dr_snprintf(logdir, BUFFER_SIZE_ELEMENTS(logdir), ".");
    NULL_TERMINATE_BUFFER(logdir);
    options.logdir = logdir;
    if (options.logprefix == NULL)
        options.logprefix = "drcov";
    if (options.native_until_thread > 0)
        go_native = true;

    drmgr_init();
    drx_init();

    /* We follow a simple model of the caller requesting the coverage dump,
     * either via calling the exit routine, using its own soft_kills nudge, or
     * an explicit dump call for unusual cases.  This means that drx's
     * soft_kills remains inside the outer later, i.e., the drcov client.  This
     * is the easiest approach for coordinating soft_kills among many libraries.
     * Thus, we do *not* register for an exit event here.
     */

    drmgr_register_thread_init_event(event_thread_init);
    drmgr_register_thread_exit_event(event_thread_exit);
    drmgr_register_bb_instrumentation_event(event_basic_block_analysis, NULL, NULL);
    dr_register_filter_syscall_event(event_filter_syscall);
    drmgr_register_pre_syscall_event(event_pre_syscall);
#ifdef UNIX
    dr_register_fork_init_event(event_fork);
#endif

    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1)
        return DRCOVLIB_ERROR;

    return event_init();
}
