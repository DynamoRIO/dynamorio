/* **********************************************************
 * Copyright (c) 2011-2013 Google, Inc.  All rights reserved.
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

/* Containers DynamoRIO Extension: DrVector */

#ifndef _DRVECTOR_H_
#define _DRVECTOR_H_ 1

/**
 * @file drvector.h
 * @brief Header for DynamoRIO DrVector Extension
 */

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************
 * DRVECTOR
 */

/**
 * \addtogroup drcontainers Container Data Structures
 */
/**@{*/ /* begin doxygen group */

/** The storage for a vector. */
typedef struct _drvector_t {
    uint entries;  /**< The index at which drvector_append() will write. */
    uint capacity; /**< The size of \p array. */
    void **array;  /**< The dynamically allocated storage for the vector entries. */
    bool synch;    /**< Whether to automatically synchronize each operation. */
    void *lock;    /**< The lock used for synchronization. */
    void (*free_data_func)(void *); /**< The routine called when freeing each entry. */
} drvector_t;

/**
 * Initializes a drvector with the given parameters
 *
 * @param[out] vec     The vector to be initialized.
 * @param[in]  initial_capacity  The initial number of entries allocated
     for the vector.
 * @param[in]  synch     Whether to synchronize each operation.
 *   Even when \p synch is false, the vector's lock is initialized and can
 *   be used via vector_lock() and vector_unlock(), allowing the caller
 *   to extend synchronization beyond just the operation in question, to
 *   include accessing a looked-up payload, e.g.
 * @param[in]  free_data_func   A callback for freeing each data item.
 *   Leave it NULL if no callback is needed.
 */
bool
drvector_init(drvector_t *vec, uint initial_capacity, bool synch,
              void (*free_data_func)(void *));

/**
 * Returns the entry at index \p idx.  For an unsychronized table, the caller
 * is free to directly access the \p array field of \p vec.
 */
void *
drvector_get_entry(drvector_t *vec, uint idx);

/**
 * Sets the entry at index \p idx to \p data.  For an unsychronized
 * table, the caller is free to directly set the \p array field of \p
 * vec.  Entries in between the last set index and \p idx are left
 * uninitialized.  Returns whether successful.
 */
bool
drvector_set_entry(drvector_t *vec, uint idx, void *data);

/**
 * Adds a new entry to the end of the vector, resizing it if necessary.
 * If drvector_set_entry() has been called, this will add to the index beyond
 * the last index passed to drvector_set_entry().
 */
bool
drvector_append(drvector_t *vec, void *data);

/**
 * Destroys all storage for the vector.  If free_payload_func was specified
 * calls it for each payload.
 */
bool
drvector_delete(drvector_t *vec);

/** Acquires the vector lock. */
void
drvector_lock(drvector_t *vec);

/** Releases the vector lock. */
void
drvector_unlock(drvector_t *vec);

/**@}*/ /* end doxygen group */

#ifdef __cplusplus
}
#endif

#endif /* _DRVECTOR_H_ */
