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

/* Containers DynamoRIO Extension: DrList */

#include "dr_api.h"
#include "drlist.h"
#include <string.h> /* memcpy */

bool
drlist_init(drlist_t *list, bool synch, void (*free_data_func)(void*))
{
    if (list == NULL)
        return false;
    list->head = NULL;
    list->tail = NULL;
    list->synch = synch;
    list->lock = dr_mutex_create();
    list->free_data_func = free_data_func;
    return true;
}

static
drlist_node_t *
drlist_get_node(drlist_t *list, uint idx)
{
    drlist_node_t *itr;
    uint ctr = 0;
    if (list == NULL)
        return NULL;
    if (list->synch)
        dr_mutex_lock(list->lock);
    itr = list->head;
    while (itr != NULL && ctr < idx) {
        itr = itr->next;
        ctr++;
    }
    if (list->synch)
        dr_mutex_unlock(list->lock);
    return itr;
}
bool
drlist_insert(drlist_t *list, uint idx, void *data)
{
    drlist_node_t *new_node;
    drlist_node_t *old_node;
    if (list == NULL)
        return false;
    if (list->synch)
        dr_mutex_lock(list->lock);
    old_node = drlist_get_node(list, idx);
    /* If list is empty idk must be 0 */
    if (old_node == NULL && idx != 0)
        return false;
    new_node = dr_global_alloc(sizeof(drlist_node_t));
    new_node->next = old_node;
    if (old_node != NULL) {
        /* Link up on both sides */
        new_node->prev = old_node->prev;
        if (old_node->prev != NULL)
            old_node->prev->next = new_node;
        old_node->prev = new_node;
    } else if (list->head == NULL && list->tail == NULL) {
        /* This is the first node in our list */
        new_node->prev = NULL;
        list->head = new_node;
        list->tail = new_node;
    }
    if (list->head == old_node)
        list->head = new_node;
    if (list->synch)
        dr_mutex_unlock(list->lock);
    return true;
}

bool
drlist_remove(drlist_t *list, drlist_node_t *old_node)
{
    if (list == NULL)
        return false;
    if (list->synch)
        dr_mutex_lock(list->lock);
    /* Link neighbors together */
    if (old_node->prev)
        old_node->prev->next = old_node->next;
    if (old_node->next)
        old_node->next->prev = old_node->prev;
    /* Update head/tail if needed */
    if (list->head == old_node)
        list->head = old_node->next;
    if (list->tail == old_node)
        list->tail = old_node->prev;
    dr_global_free(old_node, sizeof(drlist_node_t));
    if (list->synch)
        dr_mutex_unlock(list->lock);
    return true;
}

bool
drlist_remove_at(drlist_t *list, uint idx)
{
    drlist_node_t *old_node;
    if (list == NULL)
        return false;
    if (list->synch)
        dr_mutex_lock(list->lock);
    old_node = drlist_get_node(list, idx);
    if (old_node == NULL)
        return false;
    /* Link neighbors together */
    if (old_node->prev)
        old_node->prev->next = old_node->next;
    if (old_node->next)
        old_node->next->prev = old_node->prev;
    /* Update head/tail if needed */
    if (list->head == old_node)
        list->head = old_node->next;
    if (list->tail == old_node)
        list->tail = old_node->prev;
    dr_global_free(old_node, sizeof(drlist_node_t));
    if (list->synch)
        dr_mutex_unlock(list->lock);
    return true;
}

bool
drlist_push_back(drlist_t *list, void *data)
{
    drlist_node_t *new_node;
    if (list == NULL)
        return false;
    if (list->synch)
        dr_mutex_lock(list->lock);
    new_node = dr_global_alloc(sizeof(drlist_node_t));
    if (new_node == NULL)
        return false;
    new_node->data = data;
    new_node->next = NULL;
    if (list->head == NULL) {
        list->head = new_node;
        new_node->prev = NULL;
    } else {
        new_node->prev = list->tail;
    }
    list->tail = new_node;
    if (list->synch)
        dr_mutex_unlock(list->lock);
    return true;
}

bool
drlist_pop_back(drlist_t *list)
{
    drlist_node_t *old_tail;
    if (list == NULL)
        return false;
    if (list->synch)
        dr_mutex_lock(list->lock);
    if (list->tail == NULL)
        return false;
    old_tail = list->tail;
    list->tail = old_tail->prev;
    if (list->tail != NULL) {
        list->tail->next = NULL;
    } else {
        list->head = NULL;
    }
    dr_global_free(old_tail, sizeof(drlist_node_t));
    if (list->synch)
        dr_mutex_unlock(list->lock);
    return true;
}

bool
drlist_push_front(drlist_t *list, void *data)
{
    drlist_node_t *new_node;
    if (list == NULL)
        return false;
    if (list->synch)
        dr_mutex_lock(list->lock);
    new_node = dr_global_alloc(sizeof(drlist_node_t));
    new_node->data = data;
    new_node->prev = NULL;
    new_node->next = list->head;
    if (list->head != NULL)
        list->head->prev = new_node;
    else
        list->tail = new_node;
    list->head = new_node;
    if (list->synch)
        dr_mutex_unlock(list->lock);
    return true;
}

bool
drlist_pop_front(drlist_t *list)
{
    drlist_node_t *old_head;
    if (list == NULL)
        return false;
    if (list->synch)
        dr_mutex_lock(list->lock);
    if (list->head == NULL)
        return false;
    old_head = list->head;
    list->head = old_head->next;
    if (list->head != NULL) {
        list->head->prev = NULL;
    } else {
        list->tail = NULL;
    }
    dr_global_free(old_head, sizeof(drlist_node_t));
    if (list->synch)
        dr_mutex_unlock(list->lock);
    return true;
}

void *
drlist_get_entry(drlist_t *list, uint idx)
{
    void *res = NULL;
    if (list == NULL)
        return NULL;
    if (list->synch)
        dr_mutex_lock(list->lock);
    res = drlist_get_node(list, idx);
    if (list->synch)
        dr_mutex_unlock(list->lock);
    return res;
}

bool
drlist_set_entry(drlist_t *list, uint idx, void *data)
{
    drlist_node_t *node;
    if (list == NULL)
        return false;
    if (list->synch)
        dr_mutex_lock(list->lock);
    node = drlist_get_node(list, idx);
    if (node == NULL)
        return false;
    node->data = data;
    if (list->synch)
        dr_mutex_unlock(list->lock);
    return true;
}

bool
drlist_delete(drlist_t *list)
{
    drlist_node_t *itr = NULL;
    if (list == NULL)
        return false;
    if (list->synch)
        dr_mutex_lock(list->lock);
    while ((itr = list->head) != NULL) {
        if (list->free_data_func != NULL)
            (list->free_data_func)(itr->data);
        list->head = itr->next;
        dr_global_free(itr, sizeof(drlist_node_t));
    }
    list->head = NULL;
    list->tail = NULL;
    if (list->synch)
        dr_mutex_unlock(list->lock);
    dr_mutex_destroy(list->lock);
    return true;
}

void
drlist_lock(drlist_t *list)
{
    dr_mutex_lock(list->lock);
}

void
drlist_unlock(drlist_t *list)
{
    dr_mutex_unlock(list->lock);
}
