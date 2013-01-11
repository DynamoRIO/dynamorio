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
 *
 * The runtime options for this client include:
 * -dump_text     Dumps the log file in text format
 * -dump_binary   Dumps the log file in binary format
 * -check_cbr     Performs simple online conditional branch coverage checks.
 *                Checks how many conditional branches are seen and how
 *                many branches/fallthroughs are not excercised.
 *                The result are printed to bbcov.*.res file.
 * -summary_only  Prints only the summary of check results. Must be used
 *                with -check_cbr option.
 * -logdir <dir>  Sets log directory, which by default is at the same directory
 *                as the client library. It must be the last option.
 */

#include "dr_api.h"
#include "drvector.h"
#include "hashtable.h"
#include "drtable.h"
#include <string.h>

#ifdef WINDOWS
# define IF_WINDOWS(x) x
#else
# define IF_WINDOWS(x) /* nothing */
#endif

#define NULL_TERMINATE(buf) (buf)[(sizeof(buf)/sizeof((buf)[0])) - 1] = '\0'

typedef struct _bbcov_option_t {
    bool dump_text;
    bool dump_binary;
    bool check;
    bool summary;
    char *logdir;
} bbcov_option_t;
bbcov_option_t options;

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
    ptr_uint_t start_offs;   /* offset of bb start from the image base */
    ptr_uint_t cbr_tgt_offs; /* offset of cbr target from the image base */
    bool   trace;
    ushort num_instrs;
    uint   size;
    int    mod_id;
} bb_entry_t;

typedef struct _per_thread_t {
    void *bb_table;
    module_entry_t *recent_mod; /* for quick per-thread query without lock */
    file_t  log;
    file_t  res;
} per_thread_t;

static per_thread_t *global_data;
static bool bbcov_per_thread = false;
static module_table_t *module_table;
static client_id_t client_id;

/****************************************************************************
 * Utility Functions
 */
static void
log_file_create(void *drcontext, per_thread_t *data)
{
    char logname[MAXIMUM_PATH];
    char *dirsep;
    file_t log;
    size_t len;
    bool per_thread = (drcontext != NULL);
    /* We will dump data to a log file at the same directory as our library.
     * We could also pass in a path and retrieve with dr_get_options().
     */
    len = dr_snprintf(logname, sizeof(logname)/sizeof(logname[0]), "%s",
                      options.logdir != NULL ?
                      options.logdir : dr_get_client_path(client_id));
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
    if (options.dump_text || options.dump_binary) {
        log = dr_open_file(logname,
                           DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
        DR_ASSERT(log != INVALID_FILE);
    } else {
        log = INVALID_FILE;
    }
    data->log = log;
    dr_log(drcontext, LOG_ALL, 1,
           "bbcov: log for %s %d is bbcov.%03d\n",
           per_thread ? "thread" : "process",
           per_thread ? dr_get_thread_id(drcontext) : dr_get_process_id(),
           per_thread ? dr_get_thread_id(drcontext) : dr_get_process_id());
    if (!options.check) {
        data->res = INVALID_FILE;
        return;
    }
    /* replace .log with .res */
    len = strlen(logname);
    DR_ASSERT(len > 4 && logname[len-4] == '.');
    logname[len-3] = 'r';
    logname[len-2] = 'e';
    logname[len-1] = 's';
    /* create .res file */
    log = dr_open_file(logname, DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(log != INVALID_FILE);
    data->res = log;
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
module_table_load(module_table_t *table, const module_data_t *data)
{
    module_entry_t *entry = NULL;
    module_data_t  *mod;
    int i;
    /* Some apps repeatedly unload and reload the same module,
     * so we will try to re-use the old one.
     */
    DR_ASSERT(data != NULL);
    drvector_lock(&table->vector);
    /* Assuming most recently loaded entries are most likely to be unloaded,
     * we iterate the module table in a backward way for better performance.
     */
    for (i = table->vector.entries-1; i >= 0; i--) {
        entry = drvector_get_entry(&table->vector, i);
        mod   = entry->data;
        if (entry->unload &&
            /* If the same module is re-loaded at the same address,
             * we will try to use the existing entry.
             */
            mod->start       == data->start        &&
            mod->end         == data->end          &&
            mod->entry_point == data->entry_point  &&
#ifdef WINDOWS
            mod->checksum    == data->checksum     &&
            mod->timestamp   == data->timestamp    &&
#endif
            /* If a module w/ no name (there are some) is loaded, we will
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
        entry->data = dr_copy_module_data(data);
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

    /* We assume we never change an entry's data field, even on unload,
     * and thus it is ok to check its value without a lock.
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

/* assuming caller holds the lock */
static void
module_table_entry_print(module_entry_t *entry, file_t log)
{
    const char *name;
    module_data_t *data;
    data = entry->data;
    name = dr_module_preferred_name(data);
    dr_fprintf(log, "%3u, "PFX", "PFX", "PFX", %s, %s",
               entry->id, data->start, data->end, data->entry_point,
               name == NULL ? "<unknown>" : name,
               data->full_path == NULL ? "<unknown>" : data->full_path);
#ifdef WINDOWS
    dr_fprintf(log, ", 0x%08x, 0x%08x", data->checksum, data->timestamp);
#endif
    dr_fprintf(log, "\n");
}

static void
module_table_print(module_table_t *table, file_t log)
{
    uint i;
    module_entry_t *entry;
    DR_ASSERT(log != INVALID_FILE);
    dr_fprintf(log, "Module Table: id, base, end, entry, unload, name, path");
#ifdef WINDOWS
    dr_fprintf(log, ", checksum, timestamp");
#endif
    dr_fprintf(log, "\n");

    drvector_lock(&table->vector);
    for (i = 0; i < table->vector.entries; i++) {
        entry = drvector_get_entry(&table->vector, i);
        module_table_entry_print(entry, log);
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

/* iterate data passed for branch coverage check iteration */
typedef struct _check_iter_data_t {
    per_thread_t *data;
    int           num_mods;
    /* arrays below are indexed by module id, #modules-1 for bb w/ no-module */
    ptr_uint_t   *num_bbs;
    ptr_uint_t   *num_cbr_tgts;
    ptr_uint_t   *num_cbr_falls;
    ptr_uint_t   *num_cbr_tgt_misses;
    ptr_uint_t   *num_cbr_fall_misses;
    /* stores all the bbs seen for each module */
    hashtable_t  *bb_htables;
    /* stores all the cbr targets/fallthroughs seen for each module */
    hashtable_t  *cbr_htables;
} check_iter_data_t;

static bool
bb_table_entry_check(ptr_uint_t idx, void *entry, void *iter_data)
{
    check_iter_data_t *data = (check_iter_data_t *)iter_data;
    bb_entry_t  *bb_entry = (bb_entry_t *)entry;
    hashtable_t *bb_htable;
    hashtable_t *cbr_htable;
    int mod_id = (bb_entry->mod_id == -1) ? data->num_mods-1 : bb_entry->mod_id;
    bb_htable  = &data->bb_htables[mod_id];
    cbr_htable = &data->cbr_htables[mod_id];
    if (bb_entry->cbr_tgt_offs != 0) {
        if (hashtable_add(cbr_htable, (void *)bb_entry->cbr_tgt_offs, entry)) {
            data->num_cbr_tgts[mod_id]++;
            if (hashtable_lookup(bb_htable, (void *)bb_entry->cbr_tgt_offs)
                == NULL) {
                data->num_cbr_tgt_misses[mod_id]++;
                if (!options.summary) {
                    dr_fprintf(data->data->res, "module[%3d]: "PFX" to "PFX"\n",
                               mod_id,
                               (void *)bb_entry->start_offs,
                               (void *)bb_entry->cbr_tgt_offs);
                }
            }
        }
        if (hashtable_add(cbr_htable,
                          (void *)(bb_entry->start_offs + bb_entry->size),
                          entry)) {
            data->num_cbr_falls[mod_id]++;
            if (hashtable_lookup(bb_htable,
                                 (void *)(bb_entry->start_offs +
                                          bb_entry->size))
                == NULL) {
                data->num_cbr_fall_misses[mod_id]++;
                if (!options.summary) {
                    dr_fprintf(data->data->res, "module[%3d]: "PFX" to "PFX"\n",
                               mod_id,
                               bb_entry->start_offs,
                               bb_entry->start_offs + bb_entry->size);
                }
            }
        }
    }
    return true;
}

static bool
bb_table_entry_fill_htable(ptr_uint_t idx, void *entry, void *iter_data)
{
    check_iter_data_t *data = (check_iter_data_t *)iter_data;
    bb_entry_t  *bb_entry = (bb_entry_t *)entry;
    int mod_id = (bb_entry->mod_id == -1) ?
        data->num_mods - 1 : bb_entry->mod_id;
    hashtable_t *htable = &data->bb_htables[mod_id];
    if (hashtable_add(htable, (void *)bb_entry->start_offs, entry))
        data->num_bbs[mod_id]++;
    return true;
}

static void
bb_table_check_print_result(per_thread_t *data,
                            check_iter_data_t *iter_data,
                            int mod_id)
{
    dr_fprintf(data->res,
               "\tunique basic blocks seen: "SZFMT",\n"
               "\tunique conditional branch targets: "SZFMT
               ", not excercised: "SZFMT",\n"
               "\tunique conditional branch fallthroughs: "SZFMT
               ", not excercised: "SZFMT",\n",
               iter_data->num_bbs[mod_id],
               iter_data->num_cbr_tgts[mod_id],
               iter_data->num_cbr_tgt_misses[mod_id],
               iter_data->num_cbr_falls[mod_id],
               iter_data->num_cbr_fall_misses[mod_id]);
}

/* Checks each conditional branch target and fall-through with whether
 * it was executed.
 *
 * This is done by iterating the bb_table twice:
 * - Iteration 1 scans the bb table to find all unique bbs and put them
 *   into hashtables (bb_htables) of each module.
 * - Iteration 2 scans the bb table to find all unique cbr targets and
 *   fall-throughs, which are stored in hashtables (cbr_htables), and check
 *   whether they are in bb_htables.
 */
static void
bb_table_check_cbr(module_table_t *table, per_thread_t *data)
{
    check_iter_data_t iter_data;
    int i;
    /* one additional mod for bb w/o module */
    int num_mods = table->vector.entries + 1;
    DR_ASSERT(data->res != INVALID_FILE);
    /* create hashtable for each module */
    iter_data.data          = data;
    iter_data.num_mods      = num_mods;
    iter_data.bb_htables    = dr_global_alloc(sizeof(hashtable_t)*num_mods);
    iter_data.cbr_htables   = dr_global_alloc(sizeof(hashtable_t)*num_mods);
    iter_data.num_bbs       = dr_global_alloc(sizeof(ptr_uint_t)*num_mods);
    iter_data.num_cbr_tgts  = dr_global_alloc(sizeof(ptr_uint_t)*num_mods);
    iter_data.num_cbr_falls = dr_global_alloc(sizeof(ptr_uint_t)*num_mods);
    iter_data.num_cbr_tgt_misses =
        dr_global_alloc(sizeof(ptr_uint_t)*num_mods);
    iter_data.num_cbr_fall_misses =
        dr_global_alloc(sizeof(ptr_uint_t)*num_mods);
    memset(iter_data.num_bbs, 0, sizeof(ptr_uint_t)*num_mods);
    memset(iter_data.num_cbr_tgts, 0, sizeof(ptr_uint_t)*num_mods);
    memset(iter_data.num_cbr_falls, 0, sizeof(ptr_uint_t)*num_mods);
    memset(iter_data.num_cbr_tgt_misses, 0, sizeof(ptr_uint_t)*num_mods);
    memset(iter_data.num_cbr_fall_misses, 0, sizeof(ptr_uint_t)*num_mods);
    for (i = 0; i < num_mods; i++) {
        hashtable_init_ex(&iter_data.bb_htables[i], 6, HASH_INTPTR,
                          false/*!strdup*/, false/*!sync*/, NULL, NULL, NULL);
        hashtable_init_ex(&iter_data.cbr_htables[i], 6, HASH_INTPTR,
                          false/*!strdup*/, false/*!sync*/, NULL, NULL, NULL);
    }
    /* first iteration to fill the hashtable */
    drtable_iterate(data->bb_table, &iter_data, bb_table_entry_fill_htable);
    /* second iteration to check if any cbr tgt is there */
    if (!options.summary)
        dr_fprintf(data->res, "conditional branch not excercised:\n");
    drtable_iterate(data->bb_table, &iter_data, bb_table_entry_check);
    /* check result */
    dr_fprintf(data->res, "Summary:\n");
    dr_fprintf(data->res, "module id, base, end, entry, unload, name, path");
#ifdef WINDOWS
    dr_fprintf(data->res, ", checksum, timestamp");
#endif
    dr_fprintf(data->res, "\n");
    drvector_lock(&module_table->vector);
    for (i = 0; i < num_mods-1; i++) {
        module_entry_t *entry = drvector_get_entry(&module_table->vector, i);
        DR_ASSERT(entry != NULL);
        module_table_entry_print(entry, data->res);
        bb_table_check_print_result(data, &iter_data, i);
    }
    drvector_unlock(&module_table->vector);
    if (iter_data.num_bbs[i] != 0) {
        dr_fprintf(data->res, "basic blocks from unknown module\n");
        bb_table_check_print_result(data, &iter_data, i);
    }
    /* destroy the hashtable for each modules */
    for (i = 0; i < num_mods; i++) {
        hashtable_delete(&iter_data.bb_htables[i]);
        hashtable_delete(&iter_data.cbr_htables[i]);
    }
    dr_global_free(iter_data.bb_htables, sizeof(hashtable_t)*num_mods);
    dr_global_free(iter_data.cbr_htables, sizeof(hashtable_t)*num_mods);
    dr_global_free(iter_data.num_bbs, sizeof(ptr_uint_t)*num_mods);
    dr_global_free(iter_data.num_cbr_tgts, sizeof(ptr_uint_t)*num_mods);
    dr_global_free(iter_data.num_cbr_falls, sizeof(ptr_uint_t)*num_mods);
    dr_global_free(iter_data.num_cbr_tgt_misses, sizeof(ptr_uint_t)*num_mods);
    dr_global_free(iter_data.num_cbr_fall_misses, sizeof(ptr_uint_t)*num_mods);
}

static bool
bb_table_entry_print(ptr_uint_t idx, void *entry, void *iter_data)
{
    per_thread_t *data = iter_data;
    bb_entry_t *bb_entry = (bb_entry_t *)entry;
    dr_fprintf(data->log, "module[%3d]: "PFX", "PFX", %2d, %4d, %4d\n",
               bb_entry->mod_id,
               bb_entry->start_offs,
               bb_entry->cbr_tgt_offs,
               bb_entry->trace ? 1 : 0,
               bb_entry->num_instrs,
               bb_entry->size);
    return true; /* continue iteration */
}

static void
bb_table_print(void *drcontext, per_thread_t *data)
{
    DR_ASSERT(data != NULL && data->log != INVALID_FILE);
    dr_fprintf(data->log, "BB Table: %8d bbs\n",
               drtable_num_entries(data->bb_table));
    if (options.dump_text) {
        dr_fprintf(data->log, "module id, start offs, cbr tgt offs,"
                   " trace, #instr, size:\n");
        drtable_iterate(data->bb_table, data, bb_table_entry_print);
    } else
        drtable_dump_entries(data->bb_table, data->log);
}

static void
bb_table_entry_add(void *drcontext, per_thread_t *data,
                   app_pc start, app_pc cbr_tgt,
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
        bb_entry->start_offs = (ptr_uint_t)(start - mod_entry->data->start);
        DR_ASSERT(cbr_tgt == NULL || cbr_tgt > mod_entry->data->start);
        bb_entry->cbr_tgt_offs = (cbr_tgt == NULL) ?
            0 : (ptr_uint_t)(cbr_tgt - mod_entry->data->start);
    } else {
        bb_entry->mod_id = -1;
        bb_entry->start_offs = (ptr_uint_t)start;
        bb_entry->cbr_tgt_offs = (ptr_uint_t)cbr_tgt;
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
    app_pc start_pc, end_pc, cbr_tgt;

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
    cbr_tgt  = NULL;
    for (instr  = instrlist_first(bb);
         instr != NULL;
         instr  = instr_get_next(instr)) {
        app_pc pc = instr_get_app_pc(instr);
        if (pc != NULL && instr_ok_to_mangle(instr)) {
            int len = instr_length(drcontext, instr);
            num_instrs++;
            /* no support -opt_speed (elision) */
            DR_ASSERT(pc >= start_pc);
            if (pc + len > end_pc) {
                end_pc = pc + len;
                if (instr_is_cbr(instr)) {
                    cbr_tgt = opnd_get_pc(instr_get_target(instr));
                }
            }
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
    bb_table_entry_add(drcontext, data, start_pc, cbr_tgt,
                       (uint)(end_pc - start_pc),
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
    module_table_load(module_table, info);
}

static void
event_thread_exit(void *drcontext)
{
    per_thread_t *data;
    if (!bbcov_per_thread)
        return;

    data = dr_get_tls_field(drcontext);
    DR_ASSERT(data != NULL);
    if (options.dump_text || options.dump_binary) {
        module_table_print(module_table, data->log);
        bb_table_print(drcontext, data);
    }
    if (options.check) {
        bb_table_check_cbr(module_table, data);
    }
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
        if (options.dump_text || options.dump_binary) {
            module_table_print(module_table, global_data->log);
            bb_table_print(NULL, global_data);
        }
        if (options.check) {
            bb_table_check_cbr(module_table, global_data);
        }
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

static void
options_init(client_id_t id)
{
    const char *opstr = dr_get_options(id);
    /* i#1049: DR should provide a utility routine to split the string
     * into an array of tokens.
     */
    if (strstr(opstr, "-dump_text") != NULL)
        options.dump_text = true;
    if (strstr(opstr, "-dump_binary") != NULL)
        options.dump_binary = true;
    if (options.dump_text && options.dump_binary) {
        /* If both specified, we honor the later one. */
        if (strstr(opstr, "-dump_text") > strstr(opstr, "-dump_binary"))
            options.dump_binary = false;
        else
            options.dump_text = false;
    }
    if (strstr(opstr, "-check_cbr") != NULL) {
        options.check = true;
    }
    if (strstr(opstr, "-summary_only") != NULL) {
        DR_ASSERT(options.check);
        options.summary = true;
    }
    options.logdir = strstr(opstr, "-logdir ");
    if (options.logdir != NULL) {
        options.logdir += strlen("-logdir");
        for (; *options.logdir == ' '; options.logdir++);
        DR_ASSERT(options.logdir[0] != '\0');
        DR_ASSERT(dr_directory_exists(options.logdir));
    }
    DR_ASSERT(options.dump_text || options.dump_binary ||
              options.check || options.summary);
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
    options_init(id);
    event_init();
}
