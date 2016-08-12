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

/* Containers DynamoRIO Extension: DrQueue */

#ifndef _DRQUEUE_H_
#define _DRQUEUE_H_ 1

/**
 * @file drqueue.h
 * @brief Header for DynamoRIO DrQueue Extension
 */

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************
 * DRQUEUE
 */

/**
 * \addtogroup drcontainers Container Data Structures
 */
/*@{*/ /* begin doxygen group */

/** The storage for a queue. */
typedef struct _drqueue_t {
    uint front;     /**< The index at which drqueue_pop() will read. */
    uint back;      /**< The index at which drqueue_push() will write. */
    uint capacity;  /**< The size of \p array. */
    void **array;   /**< The dynamically allocated storage for the queue entries. */
    bool synch;     /**< Whether to automatically synchronize each operation. */
    void *lock;     /**< The lock used for synchronization. */
    void (*free_data_func)(void*);  /**< The routine called when freeing each entry. */
} drqueue_t;

/**
 * Initializes a drqueue with the given parameters
 *
 * @param[out] queue     The queue to be initialized.
 * @param[in]  initial_capacity  The initial number of entries allocated
     for the queue.
 * @param[in]  synch     Whether to synchronize each operation.
 *   Even when \p synch is false, the queue's lock is initialized and can
 *   be used via drqueue_lock() and drqueue_unlock(), allowing the caller
 *   to extend synchronization beyond just the operation in question, to
 *   include accessing a looked-up payload, e.g.
 * @param[in]  free_data_func   A callback for freeing each data item.
 *   Leave it NULL if no callback is needed.
 */
bool
drqueue_init(drqueue_t *queue, uint initial_capacity, bool synch,
             void (*free_data_func)(void*));

/**
 * Adds a new entry to the back of the queue, resizing it if necessary.
 */
bool
drqueue_push(drqueue_t *queue, void *data);

/**
 * Returns the entry at the front of the queue.
 */
void *
drqueue_pop(drqueue_t *queue);

/**
 * Returns true if there are no items in the queue.
 */
bool
drqueue_isempty(drqueue_t *queue);

/**
 * Destroys all storage for the queue.  If free_payload_func was specified
 * calls it for each payload.
 */
bool
drqueue_delete(drqueue_t *queue);

/** Acquires the queue lock. */
void
drqueue_lock(drqueue_t *queue);

/** Releases the queue lock. */
void
drqueue_unlock(drqueue_t *queue);

/*@}*/ /* end doxygen group */

#ifdef __cplusplus
}
#endif

#endif /* _DRQUEUE_H_ */
