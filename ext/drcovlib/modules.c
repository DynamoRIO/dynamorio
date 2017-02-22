/* ***************************************************************************
 * Copyright (c) 2012-2017 Google, Inc.  All rights reserved.
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

#include "dr_api.h"
#include "drvector.h"
#include "drcovlib.h"
#include "drcovlib_private.h"
#include <string.h>
#include <stdio.h>

#define MODULE_FILE_VERSION 2

#define NUM_GLOBAL_MODULE_CACHE 8
#define NUM_THREAD_MODULE_CACHE 4

typedef struct _module_entry_t {
    uint id;
    bool unload; /* if the module is unloaded */
    module_data_t *data;
    void *custom;
} module_entry_t;

typedef struct _module_table_t {
    drvector_t vector;
    /* for quick query without lock, assuming pointer-aligned */
    module_entry_t *cache[NUM_GLOBAL_MODULE_CACHE];
} module_table_t;

typedef struct _per_thread_t {
    /* for quick per-thread query without lock */
    module_entry_t *cache[NUM_THREAD_MODULE_CACHE];
} per_thread_t;

static int drmodtrack_init_count;
static int tls_idx = -1;
static module_table_t module_table;

/* Custom per-module field support. */
static void * (*module_load_cb)(module_data_t *module);
static int (*module_print_cb)(void *data, char *dst, size_t max_len);
static const char * (*module_parse_cb)(const char *src, OUT void **data);
static void (*module_free_cb)(void *data);

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
    if (module_free_cb != NULL)
        module_free_cb(((module_entry_t *)entry)->custom);
    dr_free_module_data(((module_entry_t *)entry)->data);
    dr_global_free(entry, sizeof(module_entry_t));
}

static void
event_module_load(void *drcontext, const module_data_t *data, bool loaded)
{
    module_entry_t *entry = NULL;
    module_data_t  *mod;
    int i;
    /* Some apps repeatedly unload and reload the same module,
     * so we will try to re-use the old one.
     */
    ASSERT(data != NULL, "data must not be NULL");
    drvector_lock(&module_table.vector);
    /* Assuming most recently loaded entries are most likely to be unloaded,
     * we iterate the module table in a backward way for better performance.
     */
    for (i = module_table.vector.entries-1; i >= 0; i--) {
        entry = drvector_get_entry(&module_table.vector, i);
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
        entry->id = module_table.vector.entries;
        entry->unload = false;
        entry->data = dr_copy_module_data(data);
        if (module_load_cb != NULL)
            entry->custom = module_load_cb(entry->data);
        drvector_append(&module_table.vector, entry);
    }
    drvector_unlock(&module_table.vector);
    global_module_cache_add(module_table.cache, entry);
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

static inline void
lookup_helper_set_fields(module_entry_t *entry, OUT uint *mod_index, OUT app_pc *mod_base)
{
    if (mod_index != NULL)
        *mod_index = entry->id;
    if (mod_base != NULL)
        *mod_base = entry->data->start;
}

drcovlib_status_t
drmodtrack_lookup(void *drcontext, app_pc pc, OUT uint *mod_index, OUT app_pc *mod_base)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    module_entry_t *entry;
    int i;
    /* We assume we never change an entry's data field, even on unload,
     * and thus it is ok to check its value without a lock.
     */
    /* lookup thread module cache */
    for (i = 0; i < NUM_THREAD_MODULE_CACHE; i++) {
        entry = data->cache[i];
        if (pc_is_in_module(entry, pc)) {
            if (i > 0) {
                thread_module_cache_adjust(data->cache, entry, i,
                                           NUM_THREAD_MODULE_CACHE);
            }
            lookup_helper_set_fields(entry, mod_index, mod_base);
            return DRCOVLIB_SUCCESS;
        }
    }
    /* lookup global module cache */
    /* we use a direct map cache, so it is ok to access it without lock */
    for (i = 0; i < NUM_GLOBAL_MODULE_CACHE; i++) {
        entry = module_table.cache[i];
        if (pc_is_in_module(entry, pc)) {
            lookup_helper_set_fields(entry, mod_index, mod_base);
            return DRCOVLIB_SUCCESS;
        }
    }
    /* lookup module table */
    entry = NULL;
    drvector_lock(&module_table.vector);
    for (i = module_table.vector.entries - 1; i >= 0; i--) {
        entry = drvector_get_entry(&module_table.vector, i);
        ASSERT(entry != NULL, "fail to get module entry");
        if (pc_is_in_module(entry, pc)) {
            global_module_cache_add(module_table.cache, entry);
            thread_module_cache_add(data->cache, NUM_THREAD_MODULE_CACHE, entry);
            break;
        }
        entry = NULL;
    }
    if (entry != NULL)
        lookup_helper_set_fields(entry, mod_index, mod_base);
    drvector_unlock(&module_table.vector);
    return entry == NULL ? DRCOVLIB_ERROR_NOT_FOUND : DRCOVLIB_SUCCESS;
}

static void
event_module_unload(void *drcontext, const module_data_t *data)
{
    module_entry_t *entry = NULL;
    int i;
    drvector_lock(&module_table.vector);
    for (i = module_table.vector.entries - 1; i >= 0; i--) {
        entry = drvector_get_entry(&module_table.vector, i);
        ASSERT(entry != NULL, "fail to get module entry");
        if (pc_is_in_module(entry, data->start))
            break;
        entry = NULL;
    }
    if (entry != NULL)
        entry->unload = true;
    else
        ASSERT(false, "fail to find the module to be unloaded");
    drvector_unlock(&module_table.vector);
}

static void
event_thread_init(void *drcontext)
{
    per_thread_t *data = dr_thread_alloc(drcontext, sizeof(*data));
    memset(data->cache, 0, sizeof(data->cache));
    drmgr_set_tls_field(drcontext, tls_idx, data);
}

static void
event_thread_exit(void *drcontext)
{
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    ASSERT(data != NULL, "data must not be NULL");
    dr_thread_free(drcontext, data, sizeof(*data));
}

drcovlib_status_t
drmodtrack_init(void)
{
    int count = dr_atomic_add32_return_sum(&drmodtrack_init_count, 1);
    if (count > 1)
        return DRCOVLIB_SUCCESS;

    if (!drmgr_init() ||
        !drmgr_register_thread_init_event(event_thread_init) ||
        !drmgr_register_thread_exit_event(event_thread_exit) ||
        !drmgr_register_module_load_event(event_module_load) ||
        !drmgr_register_module_unload_event(event_module_unload))
        return DRCOVLIB_ERROR;

    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1)
        return DRCOVLIB_ERROR;

    memset(module_table.cache, 0, sizeof(module_table.cache));
    drvector_init(&module_table.vector, 16, false, module_table_entry_free);

    return DRCOVLIB_SUCCESS;
}

drcovlib_status_t
drmodtrack_exit(void)
{
    int count = dr_atomic_add32_return_sum(&drmodtrack_init_count, -1);
    if (count != 0)
        return DRCOVLIB_SUCCESS;

    drvector_delete(&module_table.vector);
    drmgr_exit();
    return DRCOVLIB_SUCCESS;
}

/***************************************************************************
 * Dumping to a file and reading back in
 */

typedef struct _module_read_entry_t {
    app_pc base;
    uint64 size;
    app_pc entry;
#ifdef WINDOWS
    uint checksum;
    uint timestamp;
#endif
    char *path; /* may or may not point to path_buf */
    char path_buf[MAXIMUM_PATH];
    void *custom;
} module_read_entry_t;

typedef struct _module_read_info_t {
    const char *map;
    size_t map_size;
    uint num_mods;
    module_read_entry_t *mod;
} module_read_info_t;

/* assuming caller holds the lock */
static int
module_read_entry_print(module_read_entry_t *entry, uint idx, char *buf, size_t size)
{
    int len, total_len = 0;
    len = dr_snprintf(buf, size, "%3u, " PFX ", " PFX ", " PFX ", ",
                      idx, entry->base, entry->base+entry->size, entry->entry);
    if (len == -1)
        return -1;
    buf += len;
    total_len += len;
    size -= len;
#ifdef WINDOWS
    len = dr_snprintf(buf, size, "0x%08x, 0x%08x, ",
                      entry->checksum, entry->timestamp);
    if (len == -1)
        return -1;
    buf += len;
    total_len += len;
    size -= len;
#endif
    if (module_print_cb != NULL) {
        len = module_print_cb(entry->custom, buf, size);
        if (len == -1)
            return -1;
        buf += len;
        total_len += len;
        size -= len;
    }
    len = dr_snprintf(buf, size, " %s\n", entry->path);
    if (len == -1)
        return -1;
    buf += len;
    total_len += len;
    size -= len;
    return total_len;
}

/* assuming caller holds the lock */
static int
module_table_entry_print(module_entry_t *entry, char *buf, size_t size)
{
    module_read_entry_t read_entry;
    char *full_path = (char *)"<unknown>";
    module_data_t *data = entry->data;
    if (data->full_path != NULL && data->full_path[0] != '\0')
        full_path = data->full_path;
    read_entry.base = data->start;
    read_entry.size = data->end - data->start;
    read_entry.entry = data->entry_point;
#ifdef WINDOWS
    read_entry.checksum = data->checksum;
    read_entry.timestamp = data->timestamp;
#endif
    read_entry.path = full_path;
    read_entry.custom = entry->custom;
    return module_read_entry_print(&read_entry, entry->id, buf, size);
}

static drcovlib_status_t
drmodtrack_dump_buf_headers(char *buf_in, size_t size, uint count, OUT int *len_out)
{
    int len;
    char *buf = buf_in;
    if (buf == NULL || size == 0)
        return DRCOVLIB_ERROR_INVALID_PARAMETER;
    size--;  /* for the terminating null character */
    len = dr_snprintf(buf, size, "Module Table: version %u, count %u\n",
                      MODULE_FILE_VERSION, count);
    if (len == -1)
        return DRCOVLIB_ERROR_BUF_TOO_SMALL;
    buf += len;
    size -= len;

    len = dr_snprintf(buf, size, "Columns: id, base, end, entry");
    if (len == -1)
        return DRCOVLIB_ERROR_BUF_TOO_SMALL;
    buf += len;
    size -= len;

#ifdef WINDOWS
    len = dr_snprintf(buf, size, ", checksum, timestamp");
    if (len == -1)
        return DRCOVLIB_ERROR_BUF_TOO_SMALL;
    buf += len;
    size -= len;
#endif

    if (module_print_cb != NULL) {
        len = dr_snprintf(buf, size, ", (custom fields)");
        if (len == -1)
            return DRCOVLIB_ERROR_BUF_TOO_SMALL;
        buf += len;
        size -= len;
    }

    len = dr_snprintf(buf, size, ", path\n");
    if (len == -1)
        return DRCOVLIB_ERROR_BUF_TOO_SMALL;
    buf += len;
    *len_out = (int)(buf - buf_in);
    return DRCOVLIB_SUCCESS;
}

drcovlib_status_t
drmodtrack_dump_buf(char *buf, size_t size)
{
    uint i;
    module_entry_t *entry;
    int len;
    drcovlib_status_t res =
        drmodtrack_dump_buf_headers(buf, size, module_table.vector.entries, &len);
    if (res != DRCOVLIB_SUCCESS)
        return res;
    buf += len;
    size -= len;
    drvector_lock(&module_table.vector);
    for (i = 0; i < module_table.vector.entries; i++) {
        entry = drvector_get_entry(&module_table.vector, i);
        len = module_table_entry_print(entry, buf, size);
        if (len == -1) {
            drvector_unlock(&module_table.vector);
            return DRCOVLIB_ERROR_BUF_TOO_SMALL;
        }
        buf += len;
        size -= len;
     }
    buf[0] = '\0';
    drvector_unlock(&module_table.vector);
    return DRCOVLIB_SUCCESS;
}

drcovlib_status_t
drmodtrack_dump(file_t log)
{
    drcovlib_status_t res;
    size_t size = 200 + module_table.vector.entries * (MAXIMUM_PATH + 40);
    char *buf;
    do {
        buf = dr_global_alloc(size);
        res = drmodtrack_dump_buf(buf, size);
        if (res == DRCOVLIB_SUCCESS)
            dr_write_file(log, buf, strlen(buf));
        dr_global_free(buf, size);
        size *= 2;
    } while (res == DRCOVLIB_ERROR_BUF_TOO_SMALL);
    return res;
}

static inline const char *
move_to_next_line(const char *ptr)
{
    const char *end = strchr(ptr, '\n');
    if (end == NULL) {
        ptr += strlen(ptr);
    } else {
        for (ptr = end; *ptr == '\n' || *ptr == '\r'; ptr++)
            ; /* do nothing */
    }
    return ptr;
}

static inline const char *
skip_commas_and_spaces(const char *ptr, uint num_skip)
{
    const char *end = ptr;
    while (num_skip > 0) {
        end = strchr(end, ',');
        if (end == NULL)
            return NULL;
        end++;
        num_skip--;
    }
    while (*end == ' ' || *end == '\t')
        end++;
    return end;
}

drcovlib_status_t
drmodtrack_offline_read(file_t file, const char **map,
                        OUT void **handle, OUT uint *num_mods)
{
    module_read_info_t *info = NULL;
    uint i;
    uint64 file_size;
    size_t map_size = 0;
    const char *buf, *map_start;
    uint version;

    if (handle == NULL || num_mods == NULL)
        return DRCOVLIB_ERROR_INVALID_PARAMETER;
    if (file == INVALID_FILE) {
        if (map == NULL)
            return DRCOVLIB_ERROR_INVALID_PARAMETER;
        map_start = *map;
    } else {
        if (!dr_file_size(file, &file_size))
            return DRCOVLIB_ERROR_INVALID_PARAMETER;
        map_size = (size_t)file_size;
        map_start = (char *) dr_map_file(file, &map_size, 0, NULL, DR_MEMPROT_READ, 0);
        if (map_start == NULL || map_size < file_size)
            return DRCOVLIB_ERROR_INVALID_PARAMETER; /* assume bad perms or sthg */
    }
    if (map_start == NULL)
        return DRCOVLIB_ERROR_INVALID_PARAMETER;
    buf = map_start;

    /* Module table header, handling the pre-versioning legacy format. */
    if (dr_sscanf(buf, "Module Table: %u\n", num_mods) == 1)
        version = 1;
    else if (dr_sscanf(buf, "Module Table: version %u, count %u\n", &version,
                       num_mods) != 2 ||
             version != MODULE_FILE_VERSION)
        goto read_error;
    buf = move_to_next_line(buf);
    if (version > 1) {
        // Skip header line
        buf = move_to_next_line(buf);
    }

    info = (module_read_info_t *)dr_global_alloc(sizeof(*info));
    if (file != INVALID_FILE) {
        info->map = map_start;
        info->map_size = map_size;
    } else
        info->map = NULL;
    info->num_mods = *num_mods;
    info->mod = (module_read_entry_t *)dr_global_alloc(*num_mods * sizeof(*info->mod));

    /* module lists */
    for (i = 0; i < *num_mods; i++) {
        uint mod_id;
        if (version == 1) {
            if (dr_sscanf(buf, " %u, %" INT64_FORMAT"u, %[^\n\r]",
                          &mod_id, &info->mod[i].size, info->mod[i].path) != 3 ||
                mod_id != i)
                goto read_error;
        } else {
            app_pc end;
            if (dr_sscanf(buf, "%u, "PIFX", "PIFX", "PIFX", ",
                          &mod_id, &info->mod[i].base, &end, &info->mod[i].entry) != 4 ||
                mod_id != i)
                goto read_error;
            buf = skip_commas_and_spaces(buf, 4);
            if (buf == NULL)
                goto read_error;
            info->mod[i].size = end - info->mod[i].base;
#ifdef WINDOWS
            if (dr_sscanf(buf, "0x%x, 0x%x, ", &info->mod[i].checksum,
                          &info->mod[i].timestamp) != 2)
                goto read_error;
            buf = skip_commas_and_spaces(buf, 2);
            if (buf == NULL)
                goto read_error;
#endif
            if (module_parse_cb != NULL)
                buf = module_parse_cb(buf, &info->mod[i].custom);
            if (buf == NULL)
                goto read_error;
            info->mod[i].path = info->mod[i].path_buf;
            if (dr_sscanf(buf, " %[^\n\r]", info->mod[i].path) != 1)
                goto read_error;
        }
        buf = move_to_next_line(buf);
    }
    if (file == INVALID_FILE)
        *map = buf;
    *handle = (void *)info;
    return DRCOVLIB_SUCCESS;

 read_error:
    if (info != NULL) {
        dr_global_free(info->mod, *num_mods * sizeof(*info->mod));
        dr_global_free(info, sizeof(*info));
    }
    if (file != INVALID_FILE)
        dr_unmap_file((char *)map_start, map_size);
    return DRCOVLIB_ERROR;
}

drcovlib_status_t
drmodtrack_offline_lookup(void *handle, uint index, OUT app_pc *mod_base,
                          OUT size_t *mod_size, OUT char **mod_path,
                          OUT void **custom)
{
    module_read_info_t *info = (module_read_info_t *)handle;
    if (info == NULL || index >= info->num_mods)
        return DRCOVLIB_ERROR_INVALID_PARAMETER;
    if (mod_base != NULL)
        *mod_base = info->mod[index].base;
    if (mod_size != NULL)
        *mod_size = (size_t)info->mod[index].size;
    if (mod_path != NULL)
        *mod_path = info->mod[index].path;
    if (custom != NULL)
        *custom = info->mod[index].custom;
    return DRCOVLIB_SUCCESS;
}

drcovlib_status_t
drmodtrack_offline_write(void *handle, OUT char *buf, size_t size)
{
    int len;
    uint i;
    drcovlib_status_t res;
    module_read_info_t *info = (module_read_info_t *)handle;
    if (info == NULL || buf == NULL)
        return DRCOVLIB_ERROR_INVALID_PARAMETER;
    res = drmodtrack_dump_buf_headers(buf, size, info->num_mods, &len);
    if (res != DRCOVLIB_SUCCESS)
        return res;
    buf += len;
    size -= len;
    for (i = 0; i < info->num_mods; ++i) {
        len = module_read_entry_print(&info->mod[i], i, buf, size);
        if (len == -1)
            return DRCOVLIB_ERROR_BUF_TOO_SMALL;
        buf += len;
        size -= len;
    }
    buf[size-1] = '\0';
    return DRCOVLIB_SUCCESS;
}

drcovlib_status_t
drmodtrack_offline_exit(void *handle)
{
    module_read_info_t *info = (module_read_info_t *)handle;
    if (info == NULL)
        return DRCOVLIB_ERROR_INVALID_PARAMETER;
    if (module_free_cb != NULL) {
        uint i;
        for (i = 0; i < info->num_mods; ++i)
            module_free_cb(info->mod[i].custom);
    }
    dr_global_free(info->mod, info->num_mods * sizeof(*info->mod));
    if (info->map != NULL)
        dr_unmap_file((char *)info->map, info->map_size);
    dr_global_free(info, sizeof(*info));
    return DRCOVLIB_SUCCESS;
}

drcovlib_status_t
drmodtrack_add_custom_data(void * (*load_cb)(module_data_t *module),
                           int (*print_cb)(void *data, char *dst, size_t max_len),
                           const char * (*parse_cb)(const char *src, OUT void **data),
                           void (*free_cb)(void *data))
{
    /* We blindly replace if values already exist, as documented. */
    module_load_cb = load_cb;
    module_print_cb = print_cb;
    module_parse_cb = parse_cb;
    module_free_cb = free_cb;
    return DRCOVLIB_SUCCESS;
}
