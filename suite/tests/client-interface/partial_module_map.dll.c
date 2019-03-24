/* **********************************************************
 * Copyright (c) 2013 Google, Inc.  All rights reserved.
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

#include "dr_api.h"
#include "client_tools.h"

#include <string.h>

enum { MODULE_PATH_LEN = 1024 };

static char main_module[MODULE_PATH_LEN] = { '\0' };
static int num_main_module_loads = 0;

static void
module_load_event(void *dcontext, const module_data_t *data, bool loaded)
{
    /* Testing strategy:
     * The partial_module_map.c test case mmaps part of itself, which might
     * appear like a second load of the main module. If we detect a second load
     * (by comparing file names) then we report this as an error, because
     * the second load will be too small to contain all segments of the binary.
     * We guarantee that the mmap is too small to load the binary by doing an
     * mmap of size 4096, where the data segment of the binary requires at
     * least 4097 bytes.
     */
    if (data->full_path == NULL || data->full_path[0] == '\0') {
        return;
    }

    if (strncmp(main_module, data->full_path, MODULE_PATH_LEN) == 0) {
        ++num_main_module_loads;

        if (num_main_module_loads > 1) {
            dr_printf("Re-loaded module '%s'\n", data->full_path);
        }
    }
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    module_data_t *mod = dr_get_main_module();
    strncpy(main_module, mod->full_path, MODULE_PATH_LEN);
    dr_free_module_data(mod);
    dr_register_module_load_event(module_load_event);
}
