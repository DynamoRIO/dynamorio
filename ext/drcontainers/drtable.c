/* **********************************************************
 * Copyright (c) 2012-2013 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Containers DynamoRIO Extension: DrTable */

#include "dr_api.h"
#include "containers_private.h"
#include "drtable.h"
#include "drvector.h"
#include <string.h>

#undef PAGE_SIZE
#define PAGE_SIZE dr_page_size()

#define DRTABLE_MAGIC 0x42545244 /* "DRTB" */
#define MAX_ENTRY_SIZE PAGE_SIZE
#ifdef UNIX
#    define ALLOC_UNIT_SIZE PAGE_SIZE
#else
#    define ALLOC_UNIT_SIZE (16 * PAGE_SIZE) /* 64KB */
#endif

typedef struct _drtable_chunk_t drtable_chunk_t;
typedef struct _drtable_t {
    uint magic;      /* magic number for verify */
    uint flags;      /* flags from drtable_flags_t */
    void *lock;      /* lock for synch */
    void *user_data; /* user_data for iteration */
    void (*free_entry_func)(ptr_uint_t, void *, void *);
    bool stop_iter;      /* should we stop iteration */
    bool synch;          /* should drtable do the synch */
    size_t entry_size;   /* table entry size in byte */
    ptr_uint_t entries;  /* total number of entries allocated */
    ptr_uint_t capacity; /* total number of entries can hold */
    size_t size;         /* total table size in byte */
    /* the chunk won't be changed after creation, so last_chunk can be
     * accessed without lock.
     */
    drtable_chunk_t *last_chunk; /* cache for quick query without synch */
    drvector_t vec;              /* vector for chunks */
} drtable_t;

struct _drtable_chunk_t {
    drtable_t *table;    /* points to table for callbacks */
    ptr_uint_t index;    /* the start index for current chunk */
    ptr_uint_t entries;  /* number of entries allocated */
    ptr_uint_t capacity; /* number of entries in total */
    size_t size;         /* the chunk size in bytes */
    byte *base;          /* chunk base */
    byte *cur_ptr;       /* start address of unallocated entries */
};

static bool
drtable_free_callback(ptr_uint_t id, void *entry, void *table)
{
    ((drtable_t *)table)->free_entry_func(id, entry, ((drtable_t *)table)->user_data);
    return true;
}

static void
drtable_chunk_iterate(drtable_chunk_t *chunk, void *iter_data,
                      bool (*iter_func)(ptr_uint_t, void *, void *))
{
    ptr_uint_t i;
    byte *entry = chunk->base;
    drtable_t *table = chunk->table;
    if (iter_func == NULL) {
        table->stop_iter = true;
        return;
    }
    for (i = 0; i < chunk->entries; i++) {
        if (!iter_func(i + chunk->index, entry, iter_data)) {
            table->stop_iter = true;
            break;
        }
        entry += table->entry_size;
    }
}

static void *
drtable_chunk_alloc(size_t size, uint flags)
{
    byte *buf;
    if (TESTANY(DRTABLE_MEM_32BIT | DRTABLE_MEM_REACHABLE, flags))
        buf = dr_nonheap_alloc(size, DR_MEMPROT_READ | DR_MEMPROT_WRITE);
    else {
        /* will this disrupt the address space layout? */
        buf = dr_raw_mem_alloc(size, DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
    }
    DR_ASSERT(buf != NULL);
    memset(buf, 0, size);
    return buf;
}

static drtable_chunk_t *
drtable_chunk_create(drtable_t *table, ptr_uint_t num_entries)
{
    drtable_chunk_t *chunk;
    chunk = dr_global_alloc(sizeof(*chunk));
    chunk->table = table;
    chunk->index = table->capacity;
    chunk->entries = 0;
    /* new chunk would be the size of all the prior combined with exception:
     * - table->size is 0 on the first chunk creation
     * - allocation size is larger than the size of all the prior combined
     */
    chunk->size =
        ALIGN_FORWARD(MAX(table->size, table->entry_size * num_entries), ALLOC_UNIT_SIZE);
    chunk->base = drtable_chunk_alloc(chunk->size, table->flags);
    /* XXX: we should handle the case when alloc failed */
    DR_ASSERT(chunk->base != NULL);
    chunk->cur_ptr = chunk->base;
    table->size = table->size + chunk->size;
    chunk->capacity = (uint)(chunk->size / table->entry_size);
    table->capacity += chunk->capacity;
    drvector_append(&table->vec, chunk);
    return chunk;
}

static void
drtable_chunk_free(void *data)
{
    drtable_chunk_t *chunk = (drtable_chunk_t *)data;
    drtable_t *table = chunk->table;
    if (table->free_entry_func != NULL) {
        drtable_chunk_iterate(chunk, table, drtable_free_callback);
    }
    if (TESTANY(DRTABLE_MEM_32BIT | DRTABLE_MEM_REACHABLE, table->flags))
        dr_nonheap_free(chunk->base, chunk->size);
    else
        dr_raw_mem_free(chunk->base, chunk->size);
    dr_global_free(chunk, sizeof(*chunk));
}

void *
drtable_create(ptr_uint_t capacity, size_t entry_size, uint flags, bool synch,
               void (*free_entry_func)(ptr_uint_t, void *, void *))
{
    drtable_t *table;
    size_t size;

    DR_ASSERT(entry_size > 0 && entry_size < MAX_ENTRY_SIZE);

    table = dr_global_alloc(sizeof(*table));
    table->magic = DRTABLE_MAGIC;
    table->flags = flags;
    table->lock = dr_mutex_create();
    table->synch = synch;
    table->entry_size = entry_size;
    table->user_data = NULL;
    table->free_entry_func = free_entry_func;
    table->stop_iter = false;
    table->entries = 0;
    size = ALIGN_FORWARD((capacity > 0 ? capacity : 1) * entry_size, ALLOC_UNIT_SIZE);
    capacity = (ptr_uint_t)(size / entry_size);
    table->size = 0;
    table->capacity = 0;
    drvector_init(&table->vec, 2, false, drtable_chunk_free);
    table->last_chunk = drtable_chunk_create(table, capacity);
    return (void *)table;
}

void
drtable_destroy(void *tab, void *user_data)
{
    drtable_t *table = (drtable_t *)tab;
    DR_ASSERT(table != NULL && table->magic == DRTABLE_MAGIC);
    if (table->synch)
        drtable_lock(table);
    table->user_data = user_data;
    table->stop_iter = false;
    drvector_delete(&table->vec);
    if (table->synch)
        drtable_unlock(table);
    dr_mutex_destroy(table->lock);
    dr_global_free(table, sizeof(*table));
}

void *
drtable_alloc(void *tab, ptr_uint_t num_entries, ptr_uint_t *idx_ptr)
{
    void *entry;
    drtable_t *table = (drtable_t *)tab;
    drtable_chunk_t *chunk;
    int i;

    DR_ASSERT(table != NULL && table->magic == DRTABLE_MAGIC);
    if (table->synch)
        drtable_lock(table);
    /* 1. find a chunk for holding entries */
    /* check last chunk */
    chunk = table->last_chunk;
    if (chunk->capacity - chunk->entries < num_entries)
        chunk = NULL;
    if (chunk == NULL && TESTANY(DRTABLE_ALLOC_COMPACT, table->flags)) {
        /* iterate over chunks to find one */
        for (i = table->vec.entries - 1; i >= 0; i--) {
            chunk = drvector_get_entry(&table->vec, i);
            DR_ASSERT(chunk != NULL);
            if (chunk->capacity - chunk->entries >= num_entries)
                break;
            chunk = NULL;
        }
    }
    /* 2. if cannot find one, alloc one */
    if (chunk == NULL) {
        table->last_chunk = drtable_chunk_create(table, num_entries);
        chunk = table->last_chunk;
        if (chunk == NULL) {
            if (table->synch)
                drtable_unlock(table);
            if (idx_ptr != NULL)
                *idx_ptr = DRTABLE_INVALID_INDEX;
            return NULL;
        }
    }
    /* 3. alloc entries from chunk */
    entry = chunk->cur_ptr;
    chunk->cur_ptr += (num_entries * table->entry_size);
    DR_ASSERT(chunk->cur_ptr <= chunk->base + chunk->size);
    if (idx_ptr != NULL)
        *idx_ptr = chunk->index + chunk->entries;
    chunk->entries += num_entries;
    DR_ASSERT(chunk->entries <= chunk->capacity);
    table->entries += num_entries;
    DR_ASSERT(table->entries <= table->capacity);

    if (table->synch)
        drtable_unlock(table);
    return entry;
}

void
drtable_iterate(void *tab, void *iter_data, bool (*iter_func)(ptr_uint_t, void *, void *))
{
    uint i;
    drtable_t *table = (drtable_t *)tab;
    DR_ASSERT(table != NULL && table->magic == DRTABLE_MAGIC);
    DR_ASSERT(iter_func != NULL);
    if (table->synch)
        drtable_lock(table);
    table->stop_iter = false;
    for (i = 0; i < table->vec.entries; i++) {
        drtable_chunk_t *chunk = drvector_get_entry(&table->vec, i);
        drtable_chunk_iterate(chunk, iter_data, iter_func);
        if (table->stop_iter)
            break;
    }
    if (table->synch)
        drtable_unlock(table);
}

void
drtable_lock(void *tab)
{
    drtable_t *table = (drtable_t *)tab;
    DR_ASSERT(table != NULL && table->magic == DRTABLE_MAGIC);
    dr_mutex_lock(table->lock);
}

void
drtable_unlock(void *tab)
{
    drtable_t *table = (drtable_t *)tab;
    DR_ASSERT(table != NULL && table->magic == DRTABLE_MAGIC);
    dr_mutex_unlock(table->lock);
}

static drtable_chunk_t *
drtable_chunk_lookup_index(drtable_t *table, ptr_uint_t index)
{
    uint i;
    drtable_chunk_t *chunk;
    if (index > table->capacity)
        return NULL;
    chunk = table->last_chunk;
    /* we have a racy here, the entries might be updated by others */
    if (index >= chunk->index && index < chunk->index + chunk->entries)
        return chunk;
    if (table->synch)
        drtable_lock(table);
    for (i = table->vec.entries; i > 0; i--) {
        chunk = drvector_get_entry(&table->vec, i - 1);
        DR_ASSERT(chunk != NULL);
        if (index >= chunk->index && index < chunk->index + chunk->capacity)
            break;
        chunk = NULL;
    }
    if (table->synch)
        drtable_unlock(table);
    return chunk;
}

static drtable_chunk_t *
drtable_chunk_lookup_entry(drtable_t *table, byte *entry)
{
    uint i;
    drtable_chunk_t *chunk = table->last_chunk;
    /* we have a racy here, the cur_ptr might be updated by others */
    if (entry >= chunk->base && entry < chunk->cur_ptr)
        return chunk;
    if (table->synch)
        drtable_lock(table);
    for (i = table->vec.entries; i > 0; i--) {
        chunk = drvector_get_entry(&table->vec, i - 1);
        DR_ASSERT(chunk != NULL);
        if (entry >= chunk->base && entry < chunk->cur_ptr)
            break;
        chunk = NULL;
    }
    if (table->synch)
        drtable_unlock(table);
    return chunk;
}

void *
drtable_get_entry(void *tab, ptr_uint_t index)
{
    drtable_t *table = (drtable_t *)tab;
    drtable_chunk_t *chunk;
    DR_ASSERT(table != NULL && table->magic == DRTABLE_MAGIC);
    chunk = drtable_chunk_lookup_index(table, index);
    if (chunk == NULL)
        return NULL;
    return (chunk->base + index * table->entry_size);
}

ptr_uint_t
drtable_get_index(void *tab, void *entry)
{
    drtable_t *table = (drtable_t *)tab;
    drtable_chunk_t *chunk;
    ptr_uint_t index;
    DR_ASSERT(table != NULL && table->magic == DRTABLE_MAGIC);
    chunk = drtable_chunk_lookup_entry(table, (byte *)entry);
    if (chunk == NULL)
        return DRTABLE_INVALID_INDEX;
    index = (ptr_uint_t)(((byte *)entry - chunk->base) / table->entry_size);
    return (chunk->index + index);
}

ptr_uint_t
drtable_num_entries(void *tab)
{
    drtable_t *table = (drtable_t *)tab;
    DR_ASSERT(table != NULL && table->magic == DRTABLE_MAGIC);
    return table->entries;
}

ptr_uint_t
drtable_dump_entries(void *tab, file_t log)
{
    drtable_t *table = (drtable_t *)tab;
    drtable_chunk_t *chunk;
    ssize_t size;
    ptr_uint_t entries = 0;
    uint i;

    DR_ASSERT(table != NULL && table->magic == DRTABLE_MAGIC);
    if (table->synch)
        drtable_lock(table);
    entries = table->entries;
    entries = 0;
    for (i = 0; i < table->vec.entries; i++) {
        chunk = drvector_get_entry(&table->vec, i);
        entries += chunk->entries;
        size = dr_write_file(log, chunk->base, table->entry_size * chunk->entries);
        DR_ASSERT((size_t)size == table->entry_size * chunk->entries);
    }
    DR_ASSERT(entries == (uint64)table->entries);
    if (table->synch)
        drtable_unlock(table);
    return entries;
}
