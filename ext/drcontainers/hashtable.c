/* **********************************************************
 * Copyright (c) 2007-2009 VMware, Inc.  All rights reserved.
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
# define _CRT_SECURE_NO_DEPRECATE 1
#endif
#include "dr_api.h"
#include "hashtable.h"
#ifdef LINUX
# include <string.h>
#endif
#include <stddef.h> /* offsetof */

/***************************************************************************
 * UTILITIES
 */

#ifdef LINUX
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
static void (*free_func)(void*, size_t);
static void (*assert_fail_func)(const char *);

/* If no assert func is registered we just abort since we don't know
 * how to log or print (could msgbox on windows I suppose).
 * If an assert func is registered, we don't want the complexity of
 * sprintf so we give up on providing the file+line for hashtable.c but
 * the msg should identify the source.
 */
#ifdef WINDOWS
# define IF_WINDOWS(x) x
#else
# define IF_WINDOWS(x)
#endif
#ifdef DEBUG
# define ASSERT(x, msg) do { \
    if (!(x)) { \
        if (assert_fail_func != NULL) { \
            (*assert_fail_func)(msg); \
        } else { \
            dr_fprintf(STDERR, "ASSERT FAILURE: %s:%d: %s (%s)", \
                       __FILE__,  __LINE__, #x, msg); \
            IF_WINDOWS(dr_messagebox("ASSERT FAILURE: %s:%d: %s (%s)", \
                                     __FILE__,  __LINE__, #x, msg);) \
            dr_abort(); \
        } \
    } \
} while (0)
#else
# define ASSERT(x, msg) /* nothing */
#endif

/* To support use in other libraries we allow parametrization */
void
hashtable_global_config(void *(*alloc_fptr)(size_t), void (*free_fptr)(void*, size_t),
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


#define HASH_MASK(num_bits) ((~0U)>>(32-(num_bits)))
#define HASH_FUNC_BITS(val, num_bits) ((val) & (HASH_MASK(num_bits)))
#define HASH_FUNC(val, mask) ((val) & (mask))

static uint
hash_key(hashtable_t *table, void *key)
{
    uint hash = 0;
    if (table->hashtype == HASH_STRING || table->hashtype == HASH_STRING_NOCASE) {
        const char *s = (const char *) key;
        char c;
        for (c = *s; c != '\0'; c = *(s++)) {
            if (table->hashtype == HASH_STRING_NOCASE)
                c = (char) tolower(c);
            hash ^= (c << (((s - (const char *)key) %4) * 8));
        }
    } else if (table->hashtype == HASH_INTPTR) {
        hash = (uint)(ptr_uint_t) key;
    } else if (table->hashtype == HASH_CUSTOM) {
        hash = table->hash_key_func(key);
    } else
        ASSERT(false, "hashtable.c hash_key internal error: invalid hash type");
    return HASH_FUNC_BITS(hash, table->table_bits);
}

static bool
keys_equal(hashtable_t *table, void *key1, void *key2)
{
    if (table->hashtype == HASH_STRING)
        return strcmp((const char *) key1, (const char *) key2) == 0;
    else if (table->hashtype == HASH_STRING_NOCASE)
        return stri_eq((const char *) key1, (const char *) key2);
    else if (table->hashtype == HASH_INTPTR)
        return key1 == key2;
    else if (table->hashtype == HASH_CUSTOM)
        return table->cmp_key_func(key1, key2);
    else
        ASSERT(false, "hashtable.c keys_equal internal error: invalid hash type");
    return false;
}

void
hashtable_init_ex(hashtable_t *table, uint num_bits, hash_type_t hashtype, bool str_dup,
                  bool synch, void (*free_payload_func)(void*),
                  uint (*hash_key_func)(void*), bool (*cmp_key_func)(void*, void*))
{
    hash_entry_t **alloc = (hash_entry_t **)
        hash_alloc((size_t)HASHTABLE_SIZE(num_bits) * sizeof(hash_entry_t*));
    memset(alloc, 0, (size_t)HASHTABLE_SIZE(num_bits) * sizeof(hash_entry_t*));
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

/* Lookup an entry by key and return a pointer to the corresponding entry 
 * Returns NULL if no such entry exists */
void *
hashtable_lookup(hashtable_t *table, void *key)
{
    void *res = NULL;
    hash_entry_t *e;
    uint hindex = hash_key(table, key);
    if (table->synch)
        dr_mutex_lock(table->lock);
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
    size_t capacity = (size_t) HASHTABLE_SIZE(table->table_bits);
    if (table->config.resizable &&
        /* avoid fp ops.  should check for overflow. */
        table->entries * 100 > table->config.resize_threshold * capacity) {
        hash_entry_t **new_table;
        size_t new_sz;
        uint i, old_bits;
        /* double the size */
        old_bits = table->table_bits;
        table->table_bits++;
        new_sz = (size_t) HASHTABLE_SIZE(table->table_bits) * sizeof(hash_entry_t*);
        new_table = (hash_entry_t **) hash_alloc(new_sz);
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
        hash_free(table->table, capacity * sizeof(hash_entry_t*));
        table->table = new_table;
        return true;
    }
    return false;
}

bool
hashtable_add(hashtable_t *table, void *key, void *payload)
{
    uint hindex = hash_key(table, key);
    hash_entry_t *e;
    if (hashtable_lookup(table, key) != NULL) {
        /* we have a use where payload != existing entry so we don't assert on that */
        return false;
    }
    /* if payload is null can't tell from lookup miss */
    ASSERT(payload != NULL, "hashtable_add internal error");
    e = (hash_entry_t *) hash_alloc(sizeof(*e));
    if (table->str_dup) {
        const char *s = (const char *) key;
        e->key = hash_alloc(strlen(s)+1);
        strncpy((char *)e->key, s, strlen(s)+1);
    } else
        e->key = key;
    e->payload = payload;
    if (table->synch)
        dr_mutex_lock(table->lock);
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
    void *old_payload = NULL;
    uint hindex = hash_key(table, key);
    hash_entry_t *e, *new_e, *prev_e;
    /* if payload is null can't tell from lookup miss */
    ASSERT(payload != NULL, "hashtable_add_replace internal error");
    new_e = (hash_entry_t *) hash_alloc(sizeof(*new_e));
    if (table->str_dup) {
        const char *s = (const char *) key;
        new_e->key = hash_alloc(strlen(s)+1);
        strncpy((char *)new_e->key, s, strlen(s)+1);
    } else
        new_e->key = key;
    new_e->payload = payload;
    if (table->synch)
        dr_mutex_lock(table->lock);
    for (e = table->table[hindex], prev_e = NULL; e != NULL; prev_e = e, e = e->next) {
        if (keys_equal(table, e->key, key)) {
            if (prev_e == NULL)
                table->table[hindex] = new_e;
            else
                prev_e->next = new_e;
            new_e->next = e->next;
            if (table->str_dup)
                hash_free(e->key, strlen((const char *)e->key) + 1);
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
    uint hindex = hash_key(table, key);
    if (table->synch)
        dr_mutex_lock(table->lock);
    for (e = table->table[hindex], prev_e = NULL; e != NULL; prev_e = e, e = e->next) {
        if (keys_equal(table, e->key, key)) {
            if (prev_e == NULL)
                table->table[hindex] = e->next;
            else
                prev_e->next = e->next;
            if (table->str_dup)
                hash_free(e->key, strlen((const char *)e->key) + 1);
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

void
hashtable_delete(hashtable_t *table)
{
    uint i;
    if (table->synch)
        dr_mutex_lock(table->lock);
    for (i = 0; i < HASHTABLE_SIZE(table->table_bits); i++) {
        hash_entry_t *e = table->table[i];
        while (e != NULL) {
            hash_entry_t *nexte = e->next;
            if (table->str_dup)
                hash_free(e->key, strlen((const char *)e->key) + 1);
            if (table->free_payload_func != NULL)
                (table->free_payload_func)(e->payload);
            hash_free(e, sizeof(*e));
            e = nexte;
        }
    }
    hash_free(table->table, (size_t)HASHTABLE_SIZE(table->table_bits) *
              sizeof(hash_entry_t*));
    table->table = NULL;
    table->entries = 0;
    if (table->synch)
        dr_mutex_unlock(table->lock);
    dr_mutex_destroy(table->lock);
}
 
