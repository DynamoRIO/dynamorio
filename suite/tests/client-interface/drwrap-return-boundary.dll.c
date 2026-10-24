/* **********************************************************
 * Copyright (c) Bryan Berns  All rights reserved.
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

#include "dr_api.h"
#include "client_tools.h"
#include "drmgr.h"
#include "drwrap.h"

static uint entries;
static uint returns;

static void
boundary_pre(void *wrapcxt, void **user_data)
{
    ++entries;
}

static void
boundary_post(void *wrapcxt, void *user_data)
{
    CHECK(wrapcxt != NULL, "boundary call unexpectedly unwound");
    ++returns;
}

static void
prepare_pre(void *wrapcxt, void **user_data)
{
    app_pc target = (app_pc)drwrap_get_arg(wrapcxt, 0);
    CHECK(returns == entries, "previous boundary return callback missing");
    CHECK(drwrap_wrap(target, boundary_pre, boundary_post), "boundary wrap failed");
}

static void
module_load(void *drcontext, const module_data_t *module, bool loaded)
{
    app_pc prepare = (app_pc)dr_get_proc_address(module->handle, "prepare_boundary");
    if (prepare != NULL)
        CHECK(drwrap_wrap(prepare, prepare_pre, NULL), "setup wrap failed");
}

static void
event_exit(void)
{
    CHECK(entries == 9, "boundary entry count differs");
    CHECK(returns == entries, "boundary return callback missing");
    dr_fprintf(STDERR, "wrapped calls: %u returns: %u\n", entries, returns);
    drwrap_exit();
    drmgr_exit();
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    CHECK(drmgr_init() && drwrap_init(), "extension initialization failed");
    CHECK(drmgr_register_module_load_event(module_load), "module event failed");
    CHECK(drmgr_register_exit_event(event_exit), "exit event failed");
}
