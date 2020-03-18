/* **********************************************************
 * Copyright (c) 2011-2020 Google, Inc.  All rights reserved.
 * Copyright (c) 2007-2010 VMware, Inc.  All rights reserved.
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

/* Containers DynamoRIO Extension: Hashtable */

#ifdef WINDOWS
#    define _CRT_SECURE_NO_DEPRECATE 1
#endif
#include "dr_api.h"
#include "hashtable.h"
#include "containers_private.h"
#ifdef UNIX
#    include <string.h>
#endif
#include <stddef.h> /* offsetof */

/***************************************************************************
 * UTILITIES
 */

#ifdef UNIX
/* FIXME: i#30: provide safe libc routines like we do on Windows */
static int
tolower(int c)
{
    if (c >= 'A' && c <= 'Z')
        return (c - ('A' - 'a'));
    return c;
}
#endif

bool
stri_eq(const char *s1, const char *s2)
{
    char uc1, uc2;
    if (s1 == NULL || s2 == NULL)
        return false;
    do {
        if (*s1 == '\0') {
            if (*s2 == '\0')
                return true;
            return false;
        }
        uc1 = ((*s1 >= 'A') && (*s1 <= 'Z')) ? (*s1 + 'a' - 'A') : *s1;
        uc2 = ((*s2 >= 'A') && (*s2 <= 'Z')) ? (*s2 + 'a' - 'A') : *s2;
        s1++;
        s2++;
    } while (uc1 == uc2);
    return false;
}

/***************************************************************************
 * HASHTABLE
 *
 * Supports both app_pc and string keys.
 */

/* We parametrize heap and assert for use in multiple libraries */
static void *(*alloc_func)(size_t);
static void (*free_func)(void *, size_t);
static void (*assert_fail_func)(const char *);

/* If no assert func is registered we just abort since we don't know
 * how to log or print (could msgbox on windows I suppose).
 * If an assert func is registered, we don't want the complexity of
 * sprintf so we give up on providing the file+line for hashtable.c but
 * the msg should identify the source.
 */
#ifdef WINDOWS
#    define IF_WINDOWS(x) x
#else
#    define IF_WINDOWS(x)
#endif
#ifdef DEBUG
#    define ASSERT(x, msg)                                                               \
        do {                                                                             \
            if (!(x)) {                                                                  \
                if (assert_fail_func != NULL) {                                          \
                    (*assert_fail_func)(msg);                                            \
                } else {                                                                 \
                    dr_fprintf(STDERR, "ASSERT FAILURE: %s:%d: %s (%s)", __FILE__,       \
                               __LINE__, #x, msg);                                       \
                    IF_WINDOWS(dr_messagebox("ASSERT FAILURE: %s:%d: %s (%s)", __FILE__, \
                                             __LINE__, #x, msg);)                        \
                    dr_abort();                                                          \
                }                                                                        \
            }                                                                            \
        } while (0)
#else
#    define ASSERT(x, msg) /* nothing */
#endif

/* To support use in other libraries we allow parametrization */
void
hashtable_global_config(void *(*alloc_fptr)(size_t), void (*free_fptr)(void *, size_t),
                        void (*assert_fail_fptr)(const char *))
{
    alloc_func = alloc_fptr;
    free_func = free_fptr;
    assert_fail_func = assert_fail_fptr;
}

static void *
hash_alloc(size_t size)
{
    if (alloc_func != NULL)
        return (*alloc_func)(size);
    else
        return dr_global_alloc(size);
}

static void
hash_free(void *ptr, size_t size)
{
    if (free_func != NULL)
        (*free_func)(ptr, size);
    else
        dr_global_free(ptr, size);
}

#define HASH_MASK(num_bits) ((~0U) >> (32 - (num_bits)))
#define HASH_FUNC_BITS(val, num_bits) ((val) & (HASH_MASK(num_bits)))
#define HASH_FUNC(val, mask) ((val) & (mask))

/* caller must hold lock */
static uint
hash_key(hashtable_t *table, void *key)
{
    uint hash = 0;
    if (table->hash_key_func != NULL) {
        hash = table->hash_key_func(key);
    } else if (table->hashtype == HASH_STRING || table->hashtype == HASH_STRING_NOCASE) {
        const char *s = (const char *)key;
        char c;
        uint i, shift;
        uint max_shift = ALIGN_FORWARD(table->table_bits, 8);
        /* XXX: share w/ core's hash_value() function */
        for (i = 0; s[i] != '\0'; i++) {
            c = s[i];
            if (table->hashtype == HASH_STRING_NOCASE)
                c = (char)tolower(c);
            shift = (i % 4) * 8;
            if (shift > max_shift)
                shift = max_shift;
            hash ^= c << shift;
        }
    } else {
        /* HASH_INTPTR, or fallback for HASH_CUSTOM in release build */
        ASSERT(table->hashtype == HASH_INTPTR,
               "hashtable.c hash_key internal error: invalid hash type");
        hash = (uint)(ptr_uint_t)key;
    }
    return HASH_FUNC_BITS(hash, table->table_bits);
}

static bool
keys_equal(hashtable_t *table, void *key1, void *key2)
{
    if (table->cmp_key_func != NULL)
        return table->cmp_key_func(key1, key2);
    else if (table->hashtype == HASH_STRING)
        return strcmp((const char *)key1, (const char *)key2) == 0;
    else if (table->hashtype == HASH_STRING_NOCASE)
        return stri_eq((const char *)key1, (const char *)key2);
    else {
        /* HASH_INTPTR, or fallback for HASH_CUSTOM in release build */
        ASSERT(table->hashtype == HASH_INTPTR,
               "hashtable.c keys_equal internal error: invalid hash type");
        return key1 == key2;
    }
}

void
hashtable_init_ex(hashtable_t *table, uint num_bits, hash_type_t hashtype, bool str_dup,
                  bool synch, void (*free_payload_func)(void *),
                  uint (*hash_key_func)(void *), bool (*cmp_key_func)(void *, void *))
{
    hash_entry_t **alloc = (hash_entry_t **)hash_alloc((size_t)HASHTABLE_SIZE(num_bits) *
                                                       sizeof(hash_entry_t *));
    memset(alloc, 0, (size_t)HASHTABLE_SIZE(num_bits) * sizeof(hash_entry_t *));
    table->table = alloc;
    table->hashtype = hashtype;
    table->str_dup = str_dup;
    ASSERT(!str_dup || hashtype == HASH_STRING || hashtype == HASH_STRING_NOCASE,
           "hashtable_init_ex internal error: invalid hashtable type");
    table->lock = dr_mutex_create();
    table->table_bits = num_bits;
    table->synch = synch;
    table->free_payload_func = free_payload_func;
    table->hash_key_func = hash_key_func;
    table->cmp_key_func = cmp_key_func;
    ASSERT(table->hashtype != HASH_CUSTOM ||
               (table->hash_key_func != NULL && table->cmp_key_func != NULL),
           "hashtable_init_ex missing cmp/hash key func");
    table->entries = 0;
    table->config.size = sizeof(table->config);
    table->config.resizable = true;
    table->config.resize_threshold = 75;
    table->config.free_key_func = NULL;
}

void
hashtable_init(hashtable_t *table, uint num_bits, hash_type_t hashtype, bool str_dup)
{
    hashtable_init_ex(table, num_bits, hashtype, str_dup, true, NULL, NULL, NULL);
}

void
hashtable_configure(hashtable_t *table, hashtable_config_t *config)
{
    ASSERT(table != NULL && config != NULL, "invalid params");
    /* Ignoring size of field: shouldn't be in between */
    if (config->size > offsetof(hashtable_config_t, resizable))
        table->config.resizable = config->resizable;
    if (config->size > offsetof(hashtable_config_t, resize_threshold))
        table->config.resize_threshold = config->resize_threshold;
    if (config->size > offsetof(hashtable_config_t, free_key_func))
        table->config.free_key_func = config->free_key_func;
}

void
hashtable_lock(hashtable_t *table)
{
    dr_mutex_lock(table->lock);
}

void
hashtable_unlock(hashtable_t *table)
{
    dr_mutex_unlock(table->lock);
}

bool
hashtable_lock_self_owns(hashtable_t *table)
{
    return dr_mutex_self_owns(table->lock);
}

/* Lookup an entry by key and return a pointer to the corresponding entry
 * Returns NULL if no such entry exists */
void *
hashtable_lookup(hashtable_t *table, void *key)
{
    void *res = NULL;
    hash_entry_t *e;
    if (table->synch) {
        dr_mutex_lock(table->lock);
    }
    uint hindex = hash_key(table, key);
    for (e = table->table[hindex]; e != NULL; e = e->next) {
        if (keys_equal(table, e->key, key)) {
            res = e->payload;
            break;
        }
    }
    if (table->synch)
        dr_mutex_unlock(table->lock);
    return res;
}

/* caller must hold lock */
static bool
hashtable_check_for_resize(hashtable_t *table)
{
    size_t capacity = (size_t)HASHTABLE_SIZE(table->table_bits);
    if (table->config.resizable &&
        /* avoid fp ops.  should check for overflow. */
        table->entries * 100 > table->config.resize_threshold * capacity) {
        hash_entry_t **new_table;
        size_t new_sz;
        uint i, old_bits;
        /* double the size */
        old_bits = table->table_bits;
        table->table_bits++;
        new_sz = (size_t)HASHTABLE_SIZE(table->table_bits) * sizeof(hash_entry_t *);
        new_table = (hash_entry_t **)hash_alloc(new_sz);
        memset(new_table, 0, new_sz);
        /* rehash the old table into the new */
        for (i = 0; i < HASHTABLE_SIZE(old_bits); i++) {
            hash_entry_t *e = table->table[i];
            while (e != NULL) {
                hash_entry_t *nexte = e->next;
                uint hindex = hash_key(table, e->key);
                e->next = new_table[hindex];
                new_table[hindex] = e;
                e = nexte;
            }
        }
        hash_free(table->table, capacity * sizeof(hash_entry_t *));
        table->table = new_table;
        return true;
    }
    return false;
}

bool
hashtable_add(hashtable_t *table, void *key, void *payload)
{
    /* if payload is null can't tell from lookup miss */
    ASSERT(payload != NULL, "hashtable_add internal error");
    if (table->synch) {
        dr_mutex_lock(table->lock);
    }
    uint hindex = hash_key(table, key);
    hash_entry_t *e;
    for (e = table->table[hindex]; e != NULL; e = e->next) {
        if (keys_equal(table, e->key, key)) {
            /* we have a use where payload != existing entry so we don't assert on that */
            if (table->synch)
                dr_mutex_unlock(table->lock);
            return false;
        }
    }
    e = (hash_entry_t *)hash_alloc(sizeof(*e));
    if (table->str_dup) {
        const char *s = (const char *)key;
        e->key = hash_alloc(strlen(s) + 1);
        strncpy((char *)e->key, s, strlen(s) + 1);
    } else
        e->key = key;
    e->payload = payload;
    e->next = table->table[hindex];
    table->table[hindex] = e;
    table->entries++;
    hashtable_check_for_resize(table);
    if (table->synch)
        dr_mutex_unlock(table->lock);
    return true;
}

void *
hashtable_add_replace(hashtable_t *table, void *key, void *payload)
{
    /* if payload is null can't tell from lookup miss */
    ASSERT(payload != NULL, "hashtable_add_replace internal error");
    if (table->synch) {
        dr_mutex_lock(table->lock);
    }
    void *old_payload = NULL;
    uint hindex = hash_key(table, key);
    hash_entry_t *e, *new_e, *prev_e;
    new_e = (hash_entry_t *)hash_alloc(sizeof(*new_e));
    if (table->str_dup) {
        const char *s = (const char *)key;
        new_e->key = hash_alloc(strlen(s) + 1);
        strncpy((char *)new_e->key, s, strlen(s) + 1);
    } else
        new_e->key = key;
    new_e->payload = payload;
    for (e = table->table[hindex], prev_e = NULL; e != NULL; prev_e = e, e = e->next) {
        if (keys_equal(table, e->key, key)) {
            if (prev_e == NULL)
                table->table[hindex] = new_e;
            else
                prev_e->next = new_e;
            new_e->next = e->next;
            if (table->str_dup)
                hash_free(e->key, strlen((const char *)e->key) + 1);
            else if (table->config.free_key_func != NULL)
                (table->config.free_key_func)(e->key);
            /* up to caller to free payload */
            old_payload = e->payload;
            hash_free(e, sizeof(*e));
            break;
        }
    }
    if (old_payload == NULL) {
        new_e->next = table->table[hindex];
        table->table[hindex] = new_e;
        table->entries++;
        hashtable_check_for_resize(table);
    }
    if (table->synch)
        dr_mutex_unlock(table->lock);
    return old_payload;
}

bool
hashtable_remove(hashtable_t *table, void *key)
{
    bool res = false;
    hash_entry_t *e, *prev_e;
    if (table->synch) {
        dr_mutex_lock(table->lock);
    }
    uint hindex = hash_key(table, key);
    for (e = table->table[hindex], prev_e = NULL; e != NULL; prev_e = e, e = e->next) {
        if (keys_equal(table, e->key, key)) {
            if (prev_e == NULL)
                table->table[hindex] = e->next;
            else
                prev_e->next = e->next;
            if (table->str_dup)
                hash_free(e->key, strlen((const char *)e->key) + 1);
            else if (table->config.free_key_func != NULL)
                (table->config.free_key_func)(e->key);
            if (table->free_payload_func != NULL)
                (table->free_payload_func)(e->payload);
            hash_free(e, sizeof(*e));
            res = true;
            table->entries--;
            break;
        }
    }
    if (table->synch)
        dr_mutex_unlock(table->lock);
    return res;
}

bool
hashtable_remove_range(hashtable_t *table, void *start, void *end)
{
    bool res = false;
    uint i;
    hash_entry_t *e, *prev_e, *next_e;
    if (table->synch)
        hashtable_lock(table);
    for (i = 0; i < HASHTABLE_SIZE(table->table_bits); i++) {
        for (e = table->table[i], prev_e = NULL; e != NULL; e = next_e) {
            next_e = e->next;
            if (e->key >= start && e->key < end) {
                if (prev_e == NULL)
                    table->table[i] = e->next;
                else
                    prev_e->next = e->next;
                if (table->str_dup)
                    hash_free(e->key, strlen((const char *)e->key) + 1);
                else if (table->config.free_key_func != NULL)
                    (table->config.free_key_func)(e->key);
                if (table->free_payload_func != NULL)
                    (table->free_payload_func)(e->payload);
                hash_free(e, sizeof(*e));
                table->entries--;
                res = true;
            } else
                prev_e = e;
        }
    }
    if (table->synch)
        hashtable_unlock(table);
    return res;
}

void
hashtable_apply_to_all_payloads(hashtable_t *table, void (*apply_func)(void *payload))
{
    DR_ASSERT_MSG(apply_func != NULL, "The apply_func ptr cannot be NULL.");
    uint i;
    for (i = 0; i < HASHTABLE_SIZE(table->table_bits); i++) {
        hash_entry_t *e = table->table[i];
        while (e != NULL) {
            hash_entry_t *nexte = e->next;
            apply_func(e->payload);
            e = nexte;
        }
    }
}

void
hashtable_apply_to_all_payloads_user_data(hashtable_t *table,
                                          void (*apply_func)(void *payload,
                                                             void *user_data),
                                          void *user_data)
{
    DR_ASSERT_MSG(apply_func != NULL, "The apply_func ptr cannot be NULL.");
    uint i;
    for (i = 0; i < HASHTABLE_SIZE(table->table_bits); i++) {
        hash_entry_t *e = table->table[i];
        while (e != NULL) {
            hash_entry_t *nexte = e->next;
            apply_func(e->payload, user_data);
            e = nexte;
        }
    }
}

static void
hashtable_clear_internal(hashtable_t *table)
{
    uint i;
    for (i = 0; i < HASHTABLE_SIZE(table->table_bits); i++) {
        hash_entry_t *e = table->table[i];
        while (e != NULL) {
            hash_entry_t *nexte = e->next;
            if (table->str_dup)
                hash_free(e->key, strlen((const char *)e->key) + 1);
            else if (table->config.free_key_func != NULL)
                (table->config.free_key_func)(e->key);
            if (table->free_payload_func != NULL)
                (table->free_payload_func)(e->payload);
            hash_free(e, sizeof(*e));
            e = nexte;
        }
        table->table[i] = NULL;
    }
    table->entries = 0;
}

void
hashtable_clear(hashtable_t *table)
{
    if (table->synch)
        dr_mutex_lock(table->lock);
    hashtable_clear_internal(table);
    if (table->synch)
        dr_mutex_unlock(table->lock);
}

void
hashtable_delete(hashtable_t *table)
{
    if (table->synch)
        dr_mutex_lock(table->lock);
    hashtable_clear_internal(table);
    hash_free(table->table,
              (size_t)HASHTABLE_SIZE(table->table_bits) * sizeof(hash_entry_t *));
    table->table = NULL;
    table->entries = 0;
    if (table->synch)
        dr_mutex_unlock(table->lock);
    dr_mutex_destroy(table->lock);
}

/***************************************************************************
 * PERSISTENCE
 */

/* Persists a table of single-alloc entries (i.e., does a shallow
 * copy).  The model here is that the user is using a global table and
 * reading in all the persisted entries into the live table at
 * resurrect time, rather than splitting up the table and using the
 * read-only mmapped portion when live (note that DR has the latter
 * approach for some of its tables and its built-in persistence
 * support in hashtablex.h).  Thus, we write the count and then the
 * entries (key followed by payload) collapsed into an array.
 *
 * Note that we assume the caller is synchronizing across the call to
 * hashtable_persist_size() and hashtable_persist().  If these
 * are called using DR's persistence interface, DR guarantees
 * synchronization.
 *
 * If size > 0 and the table uses HASH_INTPTR keys, these routines
 * only persist those entries with keys in [start..start+size).
 * Pass 0 for size to persist all entries.
 */

static bool
key_in_range(hashtable_t *table, hash_entry_t *he, ptr_uint_t start, size_t size)
{
    if (table->hashtype != HASH_INTPTR || size == 0)
        return true;
    /* avoiding overflow by subtracting one */
    return ((ptr_uint_t)he->key >= start && (ptr_uint_t)he->key <= (start + (size - 1)));
}

static bool
hash_write_file(file_t fd, void *ptr, size_t sz)
{
    return (dr_write_file(fd, ptr, sz) == (ssize_t)sz);
}

size_t
hashtable_persist_size(void *drcontext, hashtable_t *table, size_t entry_size,
                       void *perscxt, hasthable_persist_flags_t flags)
{
    uint count = 0;
    if (table->hashtype == HASH_INTPTR &&
        TESTANY(DR_HASHPERS_ONLY_IN_RANGE | DR_HASHPERS_ONLY_PERSISTED, flags)) {
        /* synch is already provided */
        uint i;
        ptr_uint_t start = 0;
        size_t size = 0;
        if (perscxt != NULL) {
            start = (ptr_uint_t)dr_persist_start(perscxt);
            size = dr_persist_size(perscxt);
        }
        count = 0;
        for (i = 0; i < HASHTABLE_SIZE(table->table_bits); i++) {
            hash_entry_t *he;
            for (he = table->table[i]; he != NULL; he = he->next) {
                if ((!TEST(DR_HASHPERS_ONLY_IN_RANGE, flags) ||
                     key_in_range(table, he, start, size)) &&
                    (!TEST(DR_HASHPERS_ONLY_PERSISTED, flags) ||
                     dr_fragment_persistable(drcontext, perscxt, he->key)))
                    count++;
            }
        }
    } else
        count = table->entries;
    /* we could have an OUT count param that user must pass to hashtable_persist,
     * but that's actually a pain for the user when persisting multiple tables,
     * and usage should always call hashtable_persist() right after calling
     * hashtable_persist_size().
     */
    table->persist_count = count;
    return sizeof(count) +
        (TEST(DR_HASHPERS_REBASE_KEY, flags) ? sizeof(ptr_uint_t) : 0) +
        count * (entry_size + sizeof(void *));
}

bool
hashtable_persist(void *drcontext, hashtable_t *table, size_t entry_size, file_t fd,
                  void *perscxt, hasthable_persist_flags_t flags)
{
    uint i;
    ptr_uint_t start = 0;
    size_t size = 0;
    IF_DEBUG(uint count_check = 0;)
    if (TEST(DR_HASHPERS_REBASE_KEY, flags) && perscxt == NULL)
        return false; /* invalid params */
    if (perscxt != NULL) {
        start = (ptr_uint_t)dr_persist_start(perscxt);
        size = dr_persist_size(perscxt);
    }
    if (!hash_write_file(fd, &table->persist_count, sizeof(table->persist_count)))
        return false;
    if (TEST(DR_HASHPERS_REBASE_KEY, flags)) {
        if (!hash_write_file(fd, &start, sizeof(start)))
            return false;
    }
    /* synch is already provided */
    for (i = 0; i < HASHTABLE_SIZE(table->table_bits); i++) {
        hash_entry_t *he;
        for (he = table->table[i]; he != NULL; he = he->next) {
            if ((!TEST(DR_HASHPERS_ONLY_IN_RANGE, flags) ||
                 key_in_range(table, he, start, size)) &&
                (!TEST(DR_HASHPERS_ONLY_PERSISTED, flags) ||
                 dr_fragment_persistable(drcontext, perscxt, he->key))) {
                IF_DEBUG(count_check++;)
                if (!hash_write_file(fd, &he->key, sizeof(he->key)))
                    return false;
                if (TEST(DR_HASHPERS_PAYLOAD_IS_POINTER, flags)) {
                    if (!hash_write_file(fd, he->payload, entry_size))
                        return false;
                } else {
                    ASSERT(entry_size <= sizeof(void *), "inlined data too large");
                    if (!hash_write_file(fd, &he->payload, entry_size))
                        return false;
                }
            }
        }
    }
    ASSERT(table->persist_count == count_check, "invalid count");
    return true;
}

/* Loads from disk and adds to table
 * Note that clone should only be false for tables that do their own payload
 * freeing and can avoid freeing a payload in the mmap.
 */
bool
hashtable_resurrect(void *drcontext, byte **map INOUT, hashtable_t *table,
                    size_t entry_size, void *perscxt, hasthable_persist_flags_t flags,
                    bool (*process_payload)(void *key, void *payload, ptr_int_t shift))
{
    uint i;
    ptr_uint_t stored_start = 0;
    ptr_int_t shift_amt = 0;
    uint count = *(uint *)(*map);
    *map += sizeof(count);
    if (TEST(DR_HASHPERS_REBASE_KEY, flags)) {
        if (perscxt == NULL)
            return false; /* invalid parameter */
        stored_start = *(ptr_uint_t *)(*map);
        *map += sizeof(stored_start);
        shift_amt = (ptr_int_t)dr_persist_start(perscxt) - (ptr_int_t)stored_start;
    }
    for (i = 0; i < count; i++) {
        void *inmap, *toadd;
        void *key = *(void **)(*map);
        *map += sizeof(key);
        inmap = (void *)*map;
        *map += entry_size;
        if (TEST(DR_HASHPERS_PAYLOAD_IS_POINTER, flags)) {
            toadd = inmap;
            if (TEST(DR_HASHPERS_CLONE_PAYLOAD, flags)) {
                void *inheap = hash_alloc(entry_size);
                memcpy(inheap, inmap, entry_size);
                toadd = inheap;
            }
        } else {
            toadd = NULL;
            memcpy(&toadd, inmap, entry_size);
        }
        if (TEST(DR_HASHPERS_REBASE_KEY, flags)) {
            key = (void *)(((ptr_int_t)key) + shift_amt);
        }
        if (process_payload != NULL) {
            if (!process_payload(key, toadd, shift_amt))
                return false;
        } else if (!hashtable_add(table, key, toadd))
            return false;
    }
    return true;
}
