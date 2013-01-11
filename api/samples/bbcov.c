/* ***************************************************************************
 * Copyright (c) 2012-2013 Google, Inc.  All rights reserved.
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

/* Code Manipulation API Sample:
 * bbcov.c
 * 
 * Collects information about basic blocks that have been executed.
 * It simply stores the information of basic blocks seen in bb callback event
 * into a table without any instrumentation, and dumps the buffer into log files
 * on thread/process exit.
 * To collect per-thread basic block execution information, run DR with
 * a thread private code cache (i.e., -thread_private).
 * The information can be used in cases like code coverage.
 */

#include "dr_api.h"
#include "drvector.h"
#include "drtable.h"
#include <string.h>

#ifdef WINDOWS
# define IF_WINDOWS(x) x
#else
# define IF_WINDOWS(x) /* nothing */
#endif

#define NULL_TERMINATE(buf) (buf)[(sizeof(buf)/sizeof((buf)[0])) - 1] = '\0'

typedef struct _module_entry_t {
    int  id;
    bool unload; /* if the module is unloaded */
    module_data_t *data;
} module_entry_t;

typedef struct _module_table_t {
    drvector_t vector;
    module_entry_t *cache; /* for quick query without lock */
} module_table_t;

typedef struct _bb_entry_t {
    ptr_uint_t offset; /* offset from the image base */
    bool   trace;
    ushort num_instrs;
    uint   size;
    int    mod_id;
} bb_entry_t;

typedef struct _per_thread_t {
    void *bb_table;
    module_entry_t *recent_mod; /* for quick per-thread query without lock */
    file_t  log;
} per_thread_t;

static per_thread_t *global_data;
static bool bbcov_per_thread = false;
static module_table_t *module_table;
static client_id_t client_id;

/****************************************************************************
 * Utility Functions
 */
static file_t
log_file_create(void *drcontext)
{
    char logname[MAXIMUM_PATH];
    char *dirsep;
    file_t log;
    size_t len;
    bool per_thread = (drcontext != NULL);
    /* We will dump data to a log file at the same directory as our library.
     * We could also pass in a path and retrieve with dr_get_options().
     */
    len = dr_snprintf(logname, sizeof(logname)/sizeof(logname[0]),
                      "%s", dr_get_client_path(client_id));
    DR_ASSERT(len > 0);
    for (dirsep   = logname + len;
         *dirsep != '/' IF_WINDOWS(&& *dirsep != '\\');
         dirsep--) {
        DR_ASSERT(dirsep > logname);
    }
    len = dr_snprintf(dirsep + 1,
                      (sizeof(logname) - (dirsep - logname))/sizeof(logname[0]),
                      drcontext == NULL ?
                      "bbcov.%d.proc.log" : "bbcov.%d.thd.log",
                      drcontext == NULL ?
                      dr_get_process_id() : dr_get_thread_id(drcontext));
    DR_ASSERT(len > 0);
    NULL_TERMINATE(logname);
    log = dr_open_file(logname,
                             DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(log != INVALID_FILE);
    dr_log(drcontext, LOG_ALL, 1,
           "bbcov: log for %s %d is bbcov.%03d\n",
           per_thread ? "thread" : "process",
           per_thread ? dr_get_thread_id(drcontext) : dr_get_process_id(),
           per_thread ? dr_get_thread_id(drcontext) : dr_get_process_id());
    return log;
}

/****************************************************************************
 * Module Table Functions
 */
static void
module_table_entry_free(void *entry)
{
    dr_free_module_data(((module_entry_t *)entry)->data);
    dr_global_free(entry, sizeof(module_entry_t));
}

static void
module_table_load(module_table_t *table, module_data_t *data)
{
    module_entry_t *entry = NULL;
    module_data_t  *mod;
    uint i;
    /* Some apps repeatedly unload and reload the same module,
     * so we will try to re-use the old one.
     */
    DR_ASSERT(data != NULL);
    drvector_lock(&table->vector);
    for (i = 0; i < table->vector.entries; i++) {
        entry = drvector_get_entry(&table->vector, i);
        mod   = entry->data;
        if (entry->unload &&
            /* if the same module is re-loaded at a different address,
             * we will try to use the old one.
             */
            mod->end - mod->start == data->end - data->start &&
            mod->end - mod->entry_point == data->end - data->entry_point &&
            /* if a module w/ no name (there are some) is loaded, we will
             * keep making new entries.
             */
            dr_module_preferred_name(data) != NULL &&
            dr_module_preferred_name(mod)  != NULL &&
            strcmp(dr_module_preferred_name(data),
                   dr_module_preferred_name(mod)) == 0) {
            entry->unload = false;
            break;
        }
        entry = NULL;
    }
    if (entry == NULL) {
        entry = dr_global_alloc(sizeof(*entry));
        DR_ASSERT(entry != NULL);
        entry->id = table->vector.entries;
        entry->unload = false;
        entry->data = data;
        drvector_append(&table->vector, entry);
    }
    table->cache = entry;
    drvector_unlock(&table->vector);
}

static module_entry_t *
module_table_lookup(per_thread_t *data, module_table_t *table, app_pc pc)
{
    module_entry_t *entry;
    module_data_t  *mod;
    int i;

    /* racy check on cache
     * assuming we never changed an entry's data field,
     * so it is ok to check its value without lock.
     */
    entry = (data == NULL) ? NULL : data->recent_mod;
    if (entry != NULL && !entry->unload) {
        mod = entry->data;
        if (pc >= mod->start && pc < mod->end)
            return entry;
    }
    entry = table->cache;
    if (entry != NULL && !entry->unload) {
        mod = entry->data;
        if (pc >= mod->start && pc < mod->end) {
            return entry;
        }
    }
    drvector_lock(&table->vector);
    table->cache = NULL;
    for (i = table->vector.entries - 1; i >= 0; i--) {
        entry = drvector_get_entry(&table->vector, i);
        DR_ASSERT(entry != NULL);
        mod = entry->data;
        if (mod != NULL && !entry->unload &&
            pc >= mod->start && pc < mod->end) {
            table->cache = entry;
            if (data != NULL)
                data->recent_mod = entry;
            break;
        }
        entry = NULL;
    }
    drvector_unlock(&table->vector);
    return entry;
}

static void
module_table_unload(module_table_t *table, const module_data_t *data)
{
    module_entry_t *entry = module_table_lookup(NULL, table, data->start);
    if (entry != NULL) {
        entry->unload = true;
    } else {
        DR_ASSERT(false);
    }
    table->cache = NULL;
}

static void
module_table_print(module_table_t *table, file_t log)
{
    uint i;
    module_entry_t *entry;
    module_data_t  *data;
    if (log == INVALID_FILE)
        DR_ASSERT(false);
    dr_fprintf(log, "Module Table: id, base, end, entry, unload, name, path\n");
    drvector_lock(&table->vector);
    for (i = 0; i < table->vector.entries; i++) {
        const char *name;
        entry = drvector_get_entry(&table->vector, i);
        data  = entry->data;
        name  = dr_module_preferred_name(data);
        dr_fprintf(log, "%3u, "PFX", "PFX", "PFX", %s, %s\n",
                   entry->id, data->start, data->end, data->entry_point,
                   name == NULL ? "<unknown>" : name,
                   data->full_path == NULL ? "<unknow>" : data->full_path);
    }
    drvector_unlock(&table->vector);
    dr_fprintf(log, "\n");
}

static module_table_t *
module_table_create()
{
    module_table_t *table = dr_global_alloc(sizeof(*table));
    table->cache = NULL;
    drvector_init(&table->vector, 16, false, module_table_entry_free);
    return table;
}

static void
module_table_destroy(module_table_t *table)
{
    drvector_delete(&table->vector);
    dr_global_free(table, sizeof(*table));
}

/****************************************************************************
 * BB Table Functions
 */
static bool
bb_table_entry_print(uint idx, void *entry, void *iter_data)
{
    per_thread_t *data = iter_data;
    bb_entry_t *bb_entry = (bb_entry_t *)entry;
    dr_fprintf(data->log, ""PFX", %2d, %2d, %4d, %4d\n",
               bb_entry->offset,
               bb_entry->mod_id,
               bb_entry->trace ? 1 : 0,
               bb_entry->num_instrs,
               bb_entry->size);
    return true; /* continue iteration */
}

static void
bb_table_print(void *drcontext, per_thread_t *data)
{
    dr_fprintf(data->log, "BB Table: %8d bbs\n",
               drtable_num_entries(data->bb_table));
    dr_fprintf(data->log, "offset, mod, trace, #instr, size:\n");
    drtable_iterate(data->bb_table, data, bb_table_entry_print);
}

static void
bb_table_entry_add(void *drcontext, per_thread_t *data, app_pc start,
                   uint size, ushort num_instrs, bool trace)
{
    bb_entry_t *bb_entry = drtable_alloc(data->bb_table, 1, NULL);
    module_entry_t *mod_entry = module_table_lookup(data, module_table, start);
    /* we do not de-duplicate repeated bbs */
    bb_entry->trace = trace;
    bb_entry->size  = size;
    bb_entry->num_instrs = num_instrs;
    if (mod_entry != NULL && mod_entry->data != NULL) {
        bb_entry->mod_id = mod_entry->id;
        DR_ASSERT(start > mod_entry->data->start);
        bb_entry->offset = (ptr_uint_t)(start - mod_entry->data->start);
    } else {
        bb_entry->mod_id = -1;
        bb_entry->offset = (ptr_uint_t)start;
    }
}

#define INIT_BB_TABLE_ENTRIES 4096
static void *
bb_table_create(bool synch)
{
    return drtable_create(INIT_BB_TABLE_ENTRIES,
                          sizeof(bb_entry_t), 0 /* flags */, synch, NULL);
}

static void
bb_table_destroy(void *table, void *data)
{
    drtable_destroy(table, data);
}

/****************************************************************************
 * Thread/Global Data Creation/Destroy
 */
static per_thread_t *
thread_data_create(void *drcontext)
{
    per_thread_t *data;
    if (drcontext == NULL) {
        DR_ASSERT(!bbcov_per_thread);
        data = dr_global_alloc(sizeof(*data));
    } else {
        DR_ASSERT(bbcov_per_thread);
        data = dr_thread_alloc(drcontext, sizeof(*data));
    }
    data->bb_table = bb_table_create(drcontext == NULL ? true : false);
    data->recent_mod = NULL;
    data->log = log_file_create(drcontext);
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
        DR_ASSERT(!bbcov_per_thread);
        dr_global_free(data, sizeof(*data));
    } else {
        DR_ASSERT(bbcov_per_thread);
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

/* We collect the basic block information including offset from module base,
 * size, and num of instructions, and add it into a basic block table without
 * instrumentation.
 */
static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag,
                  instrlist_t *bb, bool for_trace, bool translating)
{
    per_thread_t *data;
    instr_t *instr;
    ushort num_instrs;
    app_pc start_pc, end_pc;

    /* do nothing for translation */
    if (translating)
        return DR_EMIT_DEFAULT;

    if (bbcov_per_thread)
        data = dr_get_tls_field(drcontext);
    else
        data = global_data;

    /* Collect the number of instructions and the basic block size,
     * assuming the basic block does not have any elision on control
     * transfer instructions, which is true for default options passed
     * to DR but not for -opt_speed.
     */
    num_instrs = 0;
    start_pc = dr_fragment_app_pc(tag);
    end_pc   = start_pc; /* for finding the size */
    for (instr  = instrlist_first(bb);
         instr != NULL;
         instr  = instr_get_next(instr)) {
        app_pc pc = instr_get_app_pc(instr);
        if (pc != NULL && instr_ok_to_mangle(instr)) {
            int len = instr_length(drcontext, instr);
            num_instrs++;
            /* no support -opt_speed (elision) */
            DR_ASSERT(pc >= start_pc);
            if (pc + len > end_pc)
                end_pc = pc + len;
        }
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
    bb_table_entry_add(drcontext, data, start_pc, (uint)(end_pc - start_pc),
                       num_instrs, for_trace);
    return DR_EMIT_DEFAULT;
}

static void
event_module_unload(void *drcontext, const module_data_t *info)
{
    /* we do not delete the module entry but clean the cache only. */
    module_table_unload(module_table, info);
}

static void
event_module_load(void *drcontext, const module_data_t *info, bool loaded)
{
    module_table_load(module_table, dr_copy_module_data(info));
}

static void
event_thread_exit(void *drcontext)
{
    per_thread_t *data;
    if (!bbcov_per_thread)
        return;

    data = dr_get_tls_field(drcontext);
    DR_ASSERT(data != NULL);
    module_table_print(module_table, data->log);
    bb_table_print(drcontext, data);
    thread_data_destroy(drcontext, data);
}

static void 
event_thread_init(void *drcontext)
{
    per_thread_t *data;

    if (!bbcov_per_thread)
        return;
    /* allocate thread private data */
    data = thread_data_create(drcontext);
    dr_set_tls_field(drcontext, data);
}

static void
event_exit(void)
{
    if (!bbcov_per_thread) {
        module_table_print(module_table, global_data->log);
        bb_table_print(NULL, global_data);
        global_data_destroy(global_data);
    }
    /* destroy module table */
    module_table_destroy(module_table);
}

static void
event_init(void)
{
    uint64 max_elide_jmp  = 0;
    uint64 max_elide_call = 0;
    /* assuming no elision */
    DR_ASSERT(dr_get_integer_option("max_elide_jmp", &max_elide_jmp) &&
              dr_get_integer_option("max_elide_call", &max_elide_jmp) &&
              max_elide_jmp == 0 && max_elide_call == 0);
    /* create module table */
    module_table = module_table_create();
    /* create process data if whole process bb coverage. */
    if (!bbcov_per_thread)
        global_data = global_data_create();
}

DR_EXPORT void 
dr_init(client_id_t id)
{
    dr_register_exit_event(event_exit);
    dr_register_thread_init_event(event_thread_init);
    dr_register_thread_exit_event(event_thread_exit);
    dr_register_bb_event(event_basic_block);
    dr_register_module_load_event(event_module_load);
    dr_register_module_unload_event(event_module_unload);
    client_id = id;
    if (dr_using_all_private_caches())
        bbcov_per_thread = true;
    event_init();
}
