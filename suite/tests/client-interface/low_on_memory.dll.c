/* **********************************************************
 * Copyright (c) 2019 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 * API regression test low on memory events. Upon the calling of malloc
 * routines, the test allocates large chunks of data, filling up memory.
 * The test then checks that DynamoRIO triggers a low on memory call-back,
 * which in turn clears the memory allocated.
 *
 * The test currently assumes that the application is single-threaded.
 */

#include "dr_api.h"
#include "client_tools.h"
#include "drmgr.h"
#include "drwrap.h"
#include <stdint.h> /* uintptr_t */

#define MALLOC_ROUTINE_NAME IF_WINDOWS_ELSE("HeapAlloc", "malloc")

typedef struct node_type {

    int int_array[50000];
    struct node_type *next;

} node_t;

static bool is_wrapped;
static bool is_clear;
static node_t *head;

static const uintptr_t user_value = 9909;

static void
insert_new_node()
{
    node_t **node = &head;

    while (*node != NULL) {
        node = &((*node)->next);
    }

    *node = dr_global_alloc(sizeof(node_t));
    (*node)->next = NULL;
}

static void
wrap_pre(void *wrapcxt, OUT void **user_data)
{
    if (!is_clear)
        insert_new_node();
}

static void
module_load_event(void *drcontext, const module_data_t *mod, bool loaded)
{
    app_pc towrap = (app_pc)dr_get_proc_address(mod->handle, MALLOC_ROUTINE_NAME);
    if (towrap != NULL) {
        is_wrapped |= drwrap_wrap(towrap, wrap_pre, NULL);
    }
}

static void
low_on_memory_event()
{
    if (!is_clear) {
        if (head == NULL)
            dr_fprintf(STDERR, "clear mismatch!\n");

        node_t *node = head;
        while (node != NULL) {
            node_t *temp = node;
            node = node->next;
            dr_global_free(temp, sizeof(node_t));
        }

        dr_fprintf(STDERR, "low on memory event!\n");
        is_clear = true;
        head = NULL;

        dr_fprintf(STDERR, "priority A\n");
    } else {
        dr_fprintf(STDERR, "another low on memory event!\n");
    }
}

static void
low_on_memory_event_user(void *user_data)
{
    if (is_clear) {
        if (head != NULL)
            dr_fprintf(STDERR, "clear mismatch - head should be NULL\n");

        dr_fprintf(STDERR, "priority B\n");

        if (user_data != (void *)user_value)
            dr_fprintf(STDERR, "user data mismatch\n");
    } else {
        dr_fprintf(STDERR, "clear mismatch\n");
    }
}

static void
exit_event(void)
{
    if (!is_wrapped)
        dr_fprintf(STDERR, "was not wrapped!\n");

    if (!is_clear)
        dr_fprintf(STDERR, "was not cleared!\n");

    if (!drmgr_unregister_low_on_memory_event(low_on_memory_event) ||
        !drmgr_unregister_low_on_memory_event_user_data(low_on_memory_event_user))
        dr_fprintf(STDERR, "unregister failed!\n");

    drmgr_unregister_module_load_event(module_load_event);
    dr_unregister_exit_event(exit_event);

    dr_flush_file(STDOUT);

    drwrap_exit();
    drmgr_exit();
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    drmgr_init();
    drwrap_init();

#ifdef WINDOWS
    if (dr_is_notify_on())
        dr_enable_console_printing();
#endif

    is_wrapped = false;
    is_clear = false;
    insert_new_node();

    drmgr_register_module_load_event(module_load_event);
    dr_register_exit_event(exit_event);

    drmgr_priority_t priority = { sizeof(priority), "low-on-memory", NULL, NULL, 0 };
    drmgr_priority_t priority_user = { sizeof(priority), "low-on-memory-user", NULL, NULL,
                                       3 };

    drmgr_register_low_on_memory_event_ex(low_on_memory_event, &priority);
    drmgr_register_low_on_memory_event_user_data(low_on_memory_event_user, &priority_user,
                                                 (void *)user_value);
}
