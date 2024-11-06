/* **********************************************************
 * Copyright (c) 2024 Google, Inc.  All rights reserved.
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

#define NUDGE_ARG_DUMP_MEMORY 1

static bool saw_thread_init_event = false;
static client_id_t client_id = 0;

static void
event_nudge(void *drcontext, uint64 arg)
{
    dr_fprintf(STDERR, "nudge delivered %d\n", (uint)arg);
    if (arg == NUDGE_ARG_DUMP_MEMORY) {
        dr_memory_dump_spec_t spec;
        spec.size = sizeof(dr_memory_dump_spec_t);
#ifdef WINDOWS
        spec.flags = DR_MEMORY_DUMP_LDMP;
#else
        spec.flags = DR_MEMORY_DUMP_ELF;
#endif
        if (!dr_create_memory_dump(&spec))
            dr_fprintf(STDERR, "Error: failed to create memory dump.\n");
        // TODO i#7046: Verify the content of the output file.
        return;
    }
    dr_fprintf(STDERR, "Error: unexpected nudge event!\n");
}

static void
dr_exit(void)
{
    if (!saw_thread_init_event)
        dr_fprintf(STDERR, "Error: never saw thread init event!\n");
}

static void
dr_thread_init(void *drcontext)
{
    dr_fprintf(STDERR, "thread init\n");
    saw_thread_init_event = true;

    if (!dr_nudge_client(client_id, NUDGE_ARG_DUMP_MEMORY))
        dr_fprintf(STDERR, "Error: failed to nudge client!\n");
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    client_id = id;
    dr_register_exit_event(dr_exit);
    dr_register_thread_init_event(dr_thread_init);
    dr_register_nudge_event(event_nudge, id);
    dr_fprintf(STDERR, "thank you for testing memory dump\n");
}
