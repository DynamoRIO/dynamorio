/* **********************************************************
 * Copyright (c) 2011-2016 Google, Inc.  All rights reserved.
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

/* Containers DynamoRIO Extension: DrVector */

#include "dr_api.h"
#include "drvector.h"
#include <string.h> /* memcpy */

/* Arbitrary value for first allocation, if user asks for 0.  We
 * lazily allocate it, assuming that a request for 0 means the user
 * doesn't want to waste any memory until the vector is used.
 */
#define INITIAL_CAPACITY_IF_ZERO_REQUESTED 8

bool
drvector_init(drvector_t *vec, uint initial_capacity, bool synch,
              void (*free_data_func)(void *))
{
    if (vec == NULL)
        return false;
    if (initial_capacity > 0)
        vec->array = dr_global_alloc(initial_capacity * sizeof(void *));
    else
        vec->array = NULL;
    vec->entries = 0;
    vec->capacity = initial_capacity;
    vec->synch = synch;
    vec->lock = dr_mutex_create();
    vec->free_data_func = free_data_func;
    return true;
}

void *
drvector_get_entry(drvector_t *vec, uint idx)
{
    void *res = NULL;
    if (vec == NULL)
        return NULL;
    if (vec->synch)
        dr_mutex_lock(vec->lock);
    if (idx < vec->entries)
        res = vec->array[idx];
    if (vec->synch)
        dr_mutex_unlock(vec->lock);
    return res;
}

static void
drvector_increase_size(drvector_t *vec, uint newcap)
{
    void **newarray = dr_global_alloc(newcap * sizeof(void *));
    if (vec->array != NULL) {
        memcpy(newarray, vec->array, vec->entries * sizeof(void *));
        dr_global_free(vec->array, vec->capacity * sizeof(void *));
    }
    vec->array = newarray;
    vec->capacity = newcap;
}

bool
drvector_set_entry(drvector_t *vec, uint idx, void *data)
{
    if (vec == NULL)
        return false;
    if (vec->synch)
        dr_mutex_lock(vec->lock);
    if (idx >= vec->capacity) {
        if (idx == 0)
            drvector_increase_size(vec, INITIAL_CAPACITY_IF_ZERO_REQUESTED);
        else
            drvector_increase_size(vec, idx * 2);
    }
    vec->array[idx] = data;
    if (idx >= vec->entries)
        vec->entries = idx + 1; /* ensure append goes beyond this entry */
    if (vec->synch)
        dr_mutex_unlock(vec->lock);
    return true;
}

bool
drvector_append(drvector_t *vec, void *data)
{
    if (vec == NULL)
        return false;
    if (vec->synch)
        dr_mutex_lock(vec->lock);
    if (vec->entries >= vec->capacity) {
        if (vec->capacity == 0)
            drvector_increase_size(vec, INITIAL_CAPACITY_IF_ZERO_REQUESTED);
        else
            drvector_increase_size(vec, vec->capacity * 2);
    }
    vec->array[vec->entries] = data;
    vec->entries++;
    if (vec->synch)
        dr_mutex_unlock(vec->lock);
    return true;
}

bool
drvector_delete(drvector_t *vec)
{
    uint i;
    if (vec == NULL)
        return false;

    if (vec->synch)
        dr_mutex_lock(vec->lock);

    /* Since we lazily initialize the array, vec->array could be NULL if we
     * called drvector_init with capacity 0 and never inserted an element into
     * the vec. We check vec->array here and below before access.
     * */
    if (vec->free_data_func != NULL && vec->array != NULL) {
        for (i = 0; i < vec->entries; i++) {
            (vec->free_data_func)(vec->array[i]);
        }
    }

    if (vec->array != NULL) {
        dr_global_free(vec->array, vec->capacity * sizeof(void *));
        vec->array = NULL;
        vec->entries = 0;
    }

    if (vec->synch)
        dr_mutex_unlock(vec->lock);
    dr_mutex_destroy(vec->lock);
    return true;
}

void
drvector_lock(drvector_t *vec)
{
    dr_mutex_lock(vec->lock);
}

void
drvector_unlock(drvector_t *vec)
{
    dr_mutex_unlock(vec->lock);
}
