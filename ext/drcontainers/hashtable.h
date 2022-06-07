/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
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

#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_ 1

/**
 * @file hashtable.h
 * @brief Header for DynamoRIO Hashtable Extension
 */

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************
 * HASHTABLE
 */

/**
 * \addtogroup drcontainers Container Data Structures
 */
/**@{*/ /* begin doxygen group */

/** The type of hash key */
typedef enum {
    HASH_INTPTR,        /**< A pointer-sized integer or pointer */
    HASH_STRING,        /**< A case-sensitive string */
    HASH_STRING_NOCASE, /**< A case-insensitive string */
    /**
     * A custom key.  Hash and compare operations must be provided
     * in hashtable_init_ex().  The hash operation can return a full
     * uint, as its result will be truncated via a mod of the
     * hash key bit size.  This allows for resizing the table
     * without changing the hash operation.
     */
    HASH_CUSTOM,
} hash_type_t;

typedef struct _hash_entry_t {
    void *key;
    void *payload;
    struct _hash_entry_t *next;
} hash_entry_t;

/** Configuration parameters for a hashtable. */
typedef struct _hashtable_config_t {
    size_t size;           /**< The size of the hashtable_config_t struct used */
    bool resizable;        /**< Whether the table should be resized */
    uint resize_threshold; /**< Resize the table at this % full */
    /**
     * Called whenever an entry is removed, with the key passed in.  If "str_dup" is set
     * to true in hashtable_init() or hashtable_init_ex(), this field is ignored.
     */
    void (*free_key_func)(void *);
} hashtable_config_t;

typedef struct _hashtable_t {
    hash_entry_t **table;
    hash_type_t hashtype;
    bool str_dup;
    void *lock;
    uint table_bits;
    bool synch;
    void (*free_payload_func)(void *);
    uint (*hash_key_func)(void *);
    bool (*cmp_key_func)(void *, void *);
    uint entries;
    hashtable_config_t config;
    uint persist_count;
} hashtable_t;

/* should move back to utils.c once have iterator and alloc_exit
 * doesn't need this macro
 */
#define HASHTABLE_SIZE(num_bits) (1U << (num_bits))

/** Caseless string compare */
bool
stri_eq(const char *s1, const char *s2);

/**
 * The hashtable has parametrized heap and assert routines for flexibility.
 * This routine must be called BEFORE any other hashtable_ routine; else,
 * the defaults will be used.
 */
void
hashtable_global_config(void *(*alloc_func)(size_t), void (*free_func)(void *, size_t),
                        void (*assert_fail_func)(const char *));

/**
 * Initializes a hashtable with the given size, hash type, and whether to
 * duplicate string keys.  All operations are synchronized by default.
 */
void
hashtable_init(hashtable_t *table, uint num_bits, hash_type_t hashtype, bool str_dup);

/**
 * Initializes a hashtable with the given parameters.
 *
 * @param[out] table     The hashtable to be initialized.
 * @param[in]  num_bits  The initial number of bits to use for the hash key
 *   which determines the initial size of the table itself.  The result of the
 *   hash function will be truncated to this size.  This size will be
 *   increased when the table is resized (resizing always doubles the size).
 * @param[in]  hashtype  The type of hash to perform.
 * @param[in]  str_dup   Whether to duplicate string keys.
 * @param[in]  synch     Whether to synchronize each operation.
 *   Even when \p synch is false, the hashtable's lock is initialized and can
 *   be used via hashtable_lock() and hashtable_unlock(), allowing the caller
 *   to extend synchronization beyond just the operation in question, to
 *   include accessing a looked-up payload, e.g.
 * @param[in]  free_payload_func   A callback for freeing each payload.
 *   Leave it NULL if no callback is needed.
 * @param[in]  hash_key_func       A callback for hashing a key.
 *   Leave it NULL if no callback is needed and the default is to be used.
 *   For HASH_CUSTOM, a callback must be provided.
 *   The hash operation can return a full uint, as its result will be
 *   truncated via a mod of the hash key bit size.  This allows for resizing
 *   the table without changing the hash operation.
 * @param[in]  cmp_key_func        A callback for comparing two keys.
 *   Leave it NULL if no callback is needed and the default is to be used.
 *   For HASH_CUSTOM, a callback must be provided.
 *
 * This hashtable uses closed addressing.
 * For an open-address hashtable, consider dr_hashtable_create().
 */
void
hashtable_init_ex(hashtable_t *table, uint num_bits, hash_type_t hashtype, bool str_dup,
                  bool synch, void (*free_payload_func)(void *),
                  uint (*hash_key_func)(void *), bool (*cmp_key_func)(void *, void *));

/** Configures optional parameters of hashtable operation. */
void
hashtable_configure(hashtable_t *table, hashtable_config_t *config);

/** Returns the payload for the given key, or NULL if the key is not found */
void *
hashtable_lookup(hashtable_t *table, void *key);

/**
 * Adds a new entry.  Returns false if an entry for \p key already exists.
 * \note Never use NULL as a payload as that is used for a lookup failure.
 */
bool
hashtable_add(hashtable_t *table, void *key, void *payload);

/**
 * Adds a new entry, replacing an existing entry if any.
 * Returns the old payload, or NULL if there was no existing entry.
 * \note Never use NULL as a payload as that is used for a lookup failure.
 */
void *
hashtable_add_replace(hashtable_t *table, void *key, void *payload);

/**
 * Removes the entry for key.  If free_payload_func was specified calls it
 * for the payload being removed.  Returns false if no such entry
 * exists.
 */
bool
hashtable_remove(hashtable_t *table, void *key);

/**
 * Removes all entries with key in [start..end).  If free_payload_func
 * was specified calls it for each payload being removed.  Returns
 * false if no such entry exists.
 */
bool
hashtable_remove_range(hashtable_t *table, void *start, void *end);

/**
 * Calls the \p apply_func for each payload.
 * @param table The hashtable to apply the function.
 * @param apply_func A pointer to a function that is called for all payloads
 * stored in the map.
 */
void
hashtable_apply_to_all_payloads(hashtable_t *table, void (*apply_func)(void *payload));

/**
 * Calls the \p apply_func for each payload with user data. Similar to
 * hashtable_apply_to_all_payloads().
 * @param table The hashtable to apply the function.
 * @param apply_func A pointer to a function that is called for all payloads
 * stored in the map. It also takes user data as a parameter.
 * @param user_data User data that is available when iterating through payloads.
 */
void
hashtable_apply_to_all_payloads_user_data(hashtable_t *table,
                                          void (*apply_func)(void *payload,
                                                             void *user_data),
                                          void *user_data);

/**
 * Removes all entries from the table.  If free_payload_func was specified
 * calls it for each payload.
 */
void
hashtable_clear(hashtable_t *table);

/**
 * Destroys all storage for the table, including all entries and the
 * table itself.  If free_payload_func was specified calls it for each
 * payload.
 */
void
hashtable_delete(hashtable_t *table);

/** Acquires the hashtable lock. */
void
hashtable_lock(hashtable_t *table);

/** Releases the hashtable lock. */
void
hashtable_unlock(hashtable_t *table);

/**
 * Returns true iff the hashtable lock is owned by the calling thread.
 * This routine is only available in debug builds.
 * In release builds it always returns true.
 */
bool
hashtable_lock_self_owns(hashtable_t *table);

/* DR_API EXPORT BEGIN */
/** Flags to control hashtable persistence */
typedef enum {
    /**
     * Valid for hashtable_persist() and hashtable_resurrect() and the
     * same value must be passed to both.  Treats payloads as pointers
     * to allocated memory.  By default payloads are treated as
     * inlined values if this flag is not set.
     */
    DR_HASHPERS_PAYLOAD_IS_POINTER = 0x0001,
    /**
     * Valid for hashtable_resurrect().  Only applies if
     * DR_HASHPERS_KEY_IS_POINTER.  Performs a shallow clone of the
     * payload upon resurrection.  If this flag is not set, the
     * payloads will remain pointing into the mapped file.
     */
    DR_HASHPERS_CLONE_PAYLOAD = 0x0002,
    /**
     * Valid for hashtable_persist_size(), hashtable_persist(), and
     * hashtable_resurrect(), and the same value must be passed to all.
     * Only applies if keys are of type HASH_INTPTR.  Adjusts each key by
     * the difference in the persist-time start address of the persisted
     * code region and the resurrected start address.  The value of this
     * flag must match across all three calls hashtable_persist_size(),
     * hashtable_persist(), and hashtable_resurrect().
     */
    DR_HASHPERS_REBASE_KEY = 0x0004,
    /**
     * Valid for hashtable_persist_size() and hashtable_persist() and
     * the same value must be passed to both.  Only applies if keys
     * are of type HASH_INTPTR.  Only persists entries whose key is
     * in the address range being persisted.
     */
    DR_HASHPERS_ONLY_IN_RANGE = 0x0008,
    /**
     * Valid for hashtable_persist_size() and hashtable_persist() and
     * the same value must be passed to both.  Only applies if keys
     * are of type HASH_INTPTR.  Only persists entries for which
     * dr_fragment_persistable() returns true.
     */
    DR_HASHPERS_ONLY_PERSISTED = 0x0010,
} hasthable_persist_flags_t;
/* DR_API EXPORT END */

/**
 * For use persisting a table of single-alloc entries (i.e., via a
 * shallow copy) for loading into a live table later.
 *
 * These routines assume that the caller is synchronizing across the
 * call to hashtable_persist_size() and hashtable_persist().  If these
 * are called using DR's persistence interface, DR guarantees
 * synchronization.
 *
 * @param[in] drcontext   The opaque DR context
 * @param[in] table       The table to persist
 * @param[in] entry_size  The size of each table entry payload
 * @param[in] perscxt     The opaque persistence context from DR's persist events
 * @param[in] flags       Controls various aspects of the persistence
 */
size_t
hashtable_persist_size(void *drcontext, hashtable_t *table, size_t entry_size,
                       void *perscxt, hasthable_persist_flags_t flags);

/**
 * For use persisting a table of single-alloc entries (i.e., via a
 * shallow copy) for loading into a live table later.
 *
 * These routines assume that the caller is synchronizing across the
 * call to hashtable_persist_size() and hashtable_persist().  If these
 * are called using DR's persistence interface, DR guarantees
 * synchronization.
 *
 * hashtable_persist_size() must be called immediately prior to
 * calling hashtable_persist().
 *
 * @param[in] drcontext   The opaque DR context
 * @param[in] table       The table to persist
 * @param[in] entry_size  The size of each table entry payload
 * @param[in] fd          The target persisted file handle
 * @param[in] perscxt     The opaque persistence context from DR's persist events
 * @param[in] flags       Controls various aspects of the persistence
 */
bool
hashtable_persist(void *drcontext, hashtable_t *table, size_t entry_size, file_t fd,
                  void *perscxt, hasthable_persist_flags_t flags);

/**
 * For use persisting a table of single-alloc entries (i.e., via a
 * shallow copy) for loading into a live table later.
 *
 * Reads in entries from disk and adds them to the live table.
 *
 * @param[in] drcontext   The opaque DR context
 * @param[in] map         The mapped-in persisted file, pointing at the
 *   data written by hashtable_persist()
 * @param[in] table       The live table to add to
 * @param[in] entry_size  The size of each table entry payload
 * @param[in] perscxt     The opaque persistence context from DR's persist events
 * @param[in] flags       Controls various aspects of the persistence
 * @param[in] process_payload  If non-NULL, calls process_payload instead of
 *   hashtable_add.  process_payload can then adjust the paylod and if
 *   it wishes invoke hashtable_add.
 */
bool
hashtable_resurrect(void *drcontext, byte **map /*INOUT*/, hashtable_t *table,
                    size_t entry_size, void *perscxt, hasthable_persist_flags_t flags,
                    bool (*process_payload)(void *key, void *payload, ptr_int_t shift));

/**@}*/ /* end doxygen group */

#ifdef __cplusplus
}
#endif

#endif /* _HASHTABLE_H_ */
