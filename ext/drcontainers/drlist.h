/* **********************************************************
 * Copyright (c) 2016 Google, Inc.  All rights reserved.
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

/* Containers DynamoRIO Extension: DrList */

#ifndef _DRLIST_H_
#define _DRLIST_H_ 1

/**
 * @file drlist.h
 * @brief Header for DynamoRIO DrList Extension
 */

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************
 * DRLIST
 */

/**
 * \addtogroup drcontainers Container Data Structures
 */
/*@{*/ /* begin doxygen group */

/** A node in the list. */
typedef struct _drlist_node_t {
    void *data;
    struct _drlist_node_t *next;
    struct _drlist_node_t *prev;
} drlist_node_t;

/** The storage for a queue. */
typedef struct _drlist_t {
    drlist_node_t *head;  /**< The head of the linked list */
    drlist_node_t *tail;  /**< The tail of the linked list */
    bool synch;           /**< Whether to automatically synchronize each operation. */
    void *lock;           /**< The lock used for synchronization. */
    void (*free_data_func)(void*);  /**< The routine called when freeing each entry. */
} drlist_t;

/**
 * Initializes a drlist with the given parameters
 *
 * @param[out] list     The list to be initialized.
 * @param[in]  synch     Whether to synchronize each operation.
 *   Even when \p synch is false, the queue's lock is initialized and can
 *   be used via drlist_lock() and drlist_unlock(), allowing the caller
 *   to extend synchronization beyond just the operation in question, to
 *   include accessing a looked-up payload, e.g.
 * @param[in]  free_data_func   A callback for freeing each data item.
 *   Leave it NULL if no callback is needed.
 */
bool
drlist_init(drlist_t *list, bool synch, void (*free_data_func)(void*));

/**
 * Inserts \p data at index \p idx in the list.
 */
bool
drlist_insert(drlist_t *list, uint idx, void *data);

/**
 * Removes the node \p node from the list.
 */
bool
drlist_remove(drlist_t *list, drlist_node_t *node);

/**
 * Removes the entry at index \p idx from the list.
 */
bool
drlist_remove_at(drlist_t *list, uint idx);

/**
 * Adds a new entry after the tail of the list.
 */
bool
drlist_push_back(drlist_t *list, void *data);

/**
 * Removes the tail of the list.
 */
bool
drlist_pop_back(drlist_t *list);

/**
 * Adds a new entry before the head of the list.
 */
bool
drlist_push_front(drlist_t *list, void *data);

/**
 * Removes the head of the list.
 */
bool
drlist_pop_front(drlist_t *list);

/**
 * Returns the entry at index \p idx.  For an unsychronized list, the caller
 * is free to directly walk the list using the head or tail.
 */
void *
drlist_get_entry(drlist_t *list, uint idx);

/**
 * Sets the entry at index \p idx to \p data. Returns whether successful.
 */
bool
drlist_set_entry(drlist_t *list, uint idx, void *data);

/**
 * Destroys all storage for the list.  If free_payload_func was specified
 * calls it for each payload.
 */
bool
drlist_delete(drlist_t *list);

/** Acquires the lisk lock. */
void
drlist_lock(drlist_t *list);

/** Releases the list lock. */
void
drlist_unlock(drlist_t *list);

/*@}*/ /* end doxygen group */

#ifdef __cplusplus
}
#endif

#endif /* _DRLIST_H_ */
