/* ***************************************************************************
 * Copyright (c) 2012-2013 Google, Inc.  All rights reserved.
 * ***************************************************************************/

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

/* Utilities for keeping track of un/loaded modules. */

#ifndef CLIENTS_COMMON_MODULES_H_
#define CLIENTS_COMMON_MODULES_H_

#include "dr_api.h"
#include "drvector.h"

#define NUM_GLOBAL_MODULE_CACHE 8

typedef struct _module_entry_t {
    int id;
    bool unload; /* if the module is unloaded */
    module_data_t *data;
} module_entry_t;

typedef struct _module_table_t {
    drvector_t vector;
    /* for quick query without lock, assuming pointer-aligned */
    module_entry_t *cache[NUM_GLOBAL_MODULE_CACHE];
} module_table_t;

void
module_table_load(module_table_t *table, const module_data_t *data);

/* To avoid data race, proper sychronization on module table is required for
 * accessing module table entry.
 */
module_entry_t *
module_table_lookup(module_entry_t **cache, int cache_size, module_table_t *table,
                    app_pc pc);

/* To avoid data race, proper sychronization on module table is required for
 * accessing module table entry.
 */
void
module_table_entry_print(module_entry_t *entry, file_t log, bool print_all_info);

void
module_table_unload(module_table_t *table, const module_data_t *data);

void
module_table_print(module_table_t *table, file_t log, bool print_all_info);

module_table_t *
module_table_create();

void
module_table_destroy(module_table_t *table);

#endif /* CLIENTS_COMMON_MODULES_H_ */
