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

#ifndef _DRTABLE_H_
#define _DRTABLE_H_ 1

/**
 * @file drtable.h
 * @brief Header for DynamoRIO DrTable Extension
 */

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************
 * DRTABLE
 */

/**
 * \addtogroup drcontainers Container Data Structures
 */
/**@{*/ /* begin doxygen group */

/**
 * Flags used for drtable_create
 */
typedef enum {
    /** allocated table entries must be reachable from code cache */
    DRTABLE_MEM_REACHABLE = 0x1,
    /**
     * Allocates table entries from the addresss space that can be
     * converted to a 32-bit int.
     */
    DRTABLE_MEM_32BIT = 0x2,
    /**
     * Allocates table entries as compactly as possible, which may return
     * indics in a random order.
     */
    DRTABLE_ALLOC_COMPACT = 0x4,
} drtable_flags_t;

/** Invalid index of drtable */
#define DRTABLE_INVALID_INDEX ((ptr_uint_t)-1)

/**
 * Creates a drtable with the given parameters.
 * @param[in]  capacity   The approximate number of entries for the table.
 *   The capacity is only a suggestion for better memory usage.
 * @param[in]  entry_size The size of each table entry, which should be
 *   greater than 0 and smaller than USHRT_MAX (65535).
 * @param[in]  flags      The flags to specify the features of the table.
 * @param[in]  synch      Whether to synchronize each operation.
 *   Even when \p synch is false, the table's lock is initialized and can
 *   be used via drtable_lock() and drtable_unlock(), allowing the caller
 *   to extend synchronization beyond just the operation in question.
 * @param[in]  free_entry_func  The callback for freeing each table entry.
 *   Leave it NULL if no callback is needed.
 */
void *
drtable_create(ptr_uint_t capacity, size_t entry_size, uint flags, bool synch,
               void (*free_entry_func)(ptr_uint_t idx, void *entry, void *user_data));

/**
 * Allocates memory for an array of \p num_entries table entries, and returns
 * a pointer to the allocated memory. Returns NULL if fails.
 * If \p idx_ptr is not NULL, the index for the first entry is returned in
 * \p idx_ptr, and all the entries from the same allocation can be referred to
 * as index + n.
 */
void *
drtable_alloc(void *tab, ptr_uint_t num_entries, ptr_uint_t *idx_ptr);

/**
 * Destroys all storage for the table.
 * The \p user_data is passed to each \p free_entry_func if specified.
 */
void
drtable_destroy(void *tab, void *user_data);

/**
 * Iterates over entries in the table and call the callback function.
 * @param[in]  tab        The drtable to be operated on.
 * @param[in]  iter_data  Iteration data passed to \p iter_func.
 * @param[in]  iter_func  The callback for iterating each table entry.
 *   Returns false to stop iterating.
 */
void
drtable_iterate(void *tab, void *iter_data,
                bool (*iter_func)(ptr_uint_t id, void *, void *));

/**
 * Returns a pointer to the entry at index \p idx.
 * Returns NULL if the entry for \p idx is not allocated.
 */
void *
drtable_get_entry(void *tab, ptr_uint_t idx);

/**
 * Returns an index to the entry pointed at by \p ptr.
 * Returns DRTABLE_INVALID_INDEX if \p ptr does not point to any allocated
 * entries.
 */
ptr_uint_t
drtable_get_index(void *tab, void *ptr);

/** Acquires the table lock. */
void
drtable_lock(void *tab);

/** Releases the table lock. */
void
drtable_unlock(void *tab);

/** Returns the number of entries in the table. */
ptr_uint_t
drtable_num_entries(void *tab);

/**
 * Dumps all the table entries as an array into a file in binary format.
 * There is no header add, so the user should add one if so desired.
 * Returns the number of entries dumped.
 */
ptr_uint_t
drtable_dump_entries(void *tab, file_t log);

/**@}*/ /* end doxygen group */

#ifdef __cplusplus
}
#endif

#endif /* _DRTABLE_H_ */
