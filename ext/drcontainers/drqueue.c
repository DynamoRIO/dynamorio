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

/* Containers DynamoRIO Extension: DrQueue */

#include "dr_api.h"
#include "drqueue.h"
#include <string.h> /* memcpy */

bool
drqueue_init(drqueue_t *queue, uint initial_capacity, bool synch,
              void (*free_data_func)(void*))
{
    if (queue == NULL)
        return false;
    if (initial_capacity > 0)
        queue->array = dr_global_alloc(initial_capacity * sizeof(void*));
    else
        queue->array = NULL;
    queue->front = 0;
    queue->back = 0;
    queue->capacity = initial_capacity;
    queue->synch = synch;
    queue->lock = dr_mutex_create();
    queue->free_data_func = free_data_func;
    return true;
}

static void
drqueue_increase_size(drqueue_t *queue, uint newcap)
{
    void **newarray = dr_global_alloc(newcap * sizeof(void*));
    if (queue->array != NULL) {
        if (queue->front < queue->back) {
            /* copy from front to rear */
            memcpy(newarray, &queue->array[queue->front],
                   (queue->back - queue->front) * sizeof(void*));
        } else {
            /* copy from front to end, and then begin to rear */
            memcpy(newarray, &queue->array[queue->front],
                   (queue->capacity - queue->front) * sizeof(void*));
            memcpy(&newarray[(queue->capacity - queue->front)],
                   queue->array, queue->back * sizeof(void*));
        }
        queue->front = 0;
        queue->back = queue->capacity;
        dr_global_free(queue->array, queue->capacity * sizeof(void*));
    }
    queue->array = newarray;
    queue->capacity = newcap;
}

bool
drqueue_push(drqueue_t *queue, void *data)
{
    if (queue == NULL)
        return false;
    if (queue->synch)
        dr_mutex_lock(queue->lock);
    queue->array[queue->back] = data;
    queue->back += 1;
    /* Wrap if needed */
    if (queue->back >= queue->capacity)
        queue->back = 0;
    /* Resize if needed */
    if (queue->back == queue->front)
        drqueue_increase_size(queue, queue->capacity * 2);
    if (queue->synch)
        dr_mutex_unlock(queue->lock);
    return true;
}

void *
drqueue_pop(drqueue_t *queue)
{
    void *res = NULL;
    if (queue == NULL)
        return NULL;
    if (queue->synch)
        dr_mutex_lock(queue->lock);
    if (queue->front != queue->back) {
        res = queue->array[queue->front];
        queue->array[queue->front] = NULL;
        queue->front++;
        /* Wrap if needed */
        if (queue->front >= queue->capacity) {
            queue->front = 0;
        }
    }
    if (queue->synch)
        dr_mutex_unlock(queue->lock);
    return res;
}

bool
drqueue_isempty(drqueue_t *queue)
{
    bool res = true;
    if (queue == NULL)
        return true;
    if (queue->synch)
        dr_mutex_lock(queue->lock);
    res = queue->front == queue->back;
    if (queue->synch)
        dr_mutex_unlock(queue->lock);
    return res;
}

bool
drqueue_delete(drqueue_t *queue)
{
    void *data = NULL;
    if (queue == NULL)
        return false;
    if (queue->synch)
        dr_mutex_lock(queue->lock);
    if (queue->free_data_func != NULL) {
        while ((data = drqueue_pop(queue)) != NULL)
            (queue->free_data_func)(data);
    }
    dr_global_free(queue->array, queue->capacity * sizeof(void*));
    queue->array = NULL;
    queue->front = 0;
    queue->back = 0;
    if (queue->synch)
        dr_mutex_unlock(queue->lock);
    dr_mutex_destroy(queue->lock);
    return true;
}

void
drqueue_lock(drqueue_t *queue)
{
    dr_mutex_lock(queue->lock);
}

void
drqueue_unlock(drqueue_t *queue)
{
    dr_mutex_unlock(queue->lock);
}
