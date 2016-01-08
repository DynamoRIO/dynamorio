/* ***************************************************************************
 * Copyright (c) 2012-2016 Google, Inc.  All rights reserved.
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

#include "modules.h"
#include "drcovlib_private.h"

#include <string.h>

/* we use direct map cache to avoid locking */
static inline void
global_module_cache_add(module_entry_t **cache, module_entry_t *entry)
{
    cache[entry->id % NUM_GLOBAL_MODULE_CACHE] = entry;
}

/* Maintains LRU order in thread-private caches. A new/recent entry is moved to
 * the front, and all other entries are shifted back to make place. For new
 * entries, shifting results in the oldest entry being discarded.
 */
static inline void
thread_module_cache_adjust(module_entry_t **cache,
                           module_entry_t  *entry,
                           uint pos,
                           uint max_pos)
{
    uint i;
    ASSERT(pos < max_pos, "wrong pos");
    for (i = pos; i > 0; i--)
        cache[i] = cache[i-1];
    cache[0] = entry;
}

static inline void
thread_module_cache_add(module_entry_t **cache, uint cache_size,
                        module_entry_t *entry)
{
    thread_module_cache_adjust(cache, entry, cache_size - 1, cache_size);
}

static void
module_table_entry_free(void *entry)
{
    dr_free_module_data(((module_entry_t *)entry)->data);
    dr_global_free(entry, sizeof(module_entry_t));
}

void
module_table_load(module_table_t *table, const module_data_t *data)
{
    module_entry_t *entry = NULL;
    module_data_t  *mod;
    int i;
    /* Some apps repeatedly unload and reload the same module,
     * so we will try to re-use the old one.
     */
    ASSERT(data != NULL, "data must not be NULL");
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
        entry->id = table->vector.entries;
        entry->unload = false;
        entry->data = dr_copy_module_data(data);
        drvector_append(&table->vector, entry);
    }
    drvector_unlock(&table->vector);
    global_module_cache_add(table->cache, entry);
}

static inline bool
pc_is_in_module(module_entry_t *entry, app_pc pc)
{
    if (entry != NULL && !entry->unload && entry->data != NULL) {
        module_data_t *mod = entry->data;
        if (pc >= mod->start && pc < mod->end)
            return true;
    }
    return false;
}

module_entry_t *
module_table_lookup(module_entry_t **cache, int cache_size,
                    module_table_t *table, app_pc pc)
{
    module_entry_t *entry;
    int i;

    /* We assume we never change an entry's data field, even on unload,
     * and thus it is ok to check its value without a lock.
     */
    /* lookup thread module cache */
    if (cache != NULL) {
        for (i = 0; i < cache_size; i++) {
            entry = cache[i];
            if (pc_is_in_module(entry, pc)) {
                if (i > 0)
                    thread_module_cache_adjust(cache, entry, i, cache_size);
                return entry;
            }
        }
    }
    /* lookup global module cache */
    /* we use a direct map cache, so it is ok to access it without lock */
    for (i = 0; i < NUM_GLOBAL_MODULE_CACHE; i++) {
        entry = table->cache[i];
        if (pc_is_in_module(entry, pc))
            return entry;
    }
    /* lookup module table */
    entry = NULL;
    drvector_lock(&table->vector);
    for (i = table->vector.entries - 1; i >= 0; i--) {
        entry = drvector_get_entry(&table->vector, i);
        ASSERT(entry != NULL, "fail to get module entry");
        if (pc_is_in_module(entry, pc)) {
            global_module_cache_add(table->cache, entry);
            if (cache != NULL)
                thread_module_cache_add(cache, cache_size, entry);
            break;
        }
        entry = NULL;
    }
    drvector_unlock(&table->vector);
    return entry;
}

void
module_table_unload(module_table_t *table, const module_data_t *data)
{
    module_entry_t *entry = module_table_lookup(NULL, 0, table, data->start);
    if (entry != NULL) {
        entry->unload = true;
    } else {
        ASSERT(false, "fail to find the module to be unloaded");
    }
}

/* assuming caller holds the lock */
void
module_table_entry_print(module_entry_t *entry, file_t log, bool print_all_info)
{
    const char *name;
    module_data_t *data;
    const char *full_path = "<unknown>";
    data = entry->data;
    name = dr_module_preferred_name(data);
    if (data->full_path != NULL && data->full_path[0] != '\0')
        full_path = data->full_path;

    if (print_all_info) {
        dr_fprintf(log, "%3u, "PFX", "PFX", "PFX", %s, %s",
                   entry->id, data->start, data->end, data->entry_point,
                   (name == NULL || name[0] == '\0') ? "<unknown>" : name,
                   full_path);
#ifdef WINDOWS
        dr_fprintf(log, ", 0x%08x, 0x%08x", data->checksum, data->timestamp);
#endif /* WINDOWS */
        dr_fprintf(log, "\n");
    } else {
        dr_fprintf(log, " %u, %llu, %s\n", entry->id,
                   (uint64)(data->end - data->start), full_path);
    }
}

void
module_table_print(module_table_t *table, file_t log, bool print_all_info)
{
    uint i;
    module_entry_t *entry;
    if (log == INVALID_FILE) {
        /* It is possible that failure on log file creation is caused by the
         * running process not having enough privilege, so this is not a
         * release-build fatal error
         */
        ASSERT(false, "invalid log file");
        return;
    }
    drvector_lock(&table->vector);
    dr_fprintf(log, "Module Table: %u\n", table->vector.entries);

    if (print_all_info) {
        dr_fprintf(log, "Module Table: id, base, end, entry, unload, name, path");
#ifdef WINDOWS
        dr_fprintf(log, ", checksum, timestamp");
#endif
        dr_fprintf(log, "\n");
    }

    for (i = 0; i < table->vector.entries; i++) {
        entry = drvector_get_entry(&table->vector, i);
        module_table_entry_print(entry, log, print_all_info);
    }
    drvector_unlock(&table->vector);
}

module_table_t *
module_table_create()
{
    module_table_t *table = dr_global_alloc(sizeof(*table));
    memset(table->cache, 0, sizeof(table->cache));
    drvector_init(&table->vector, 16, false, module_table_entry_free);
    return table;
}

void
module_table_destroy(module_table_t *table)
{
    drvector_delete(&table->vector);
    dr_global_free(table, sizeof(*table));
}
