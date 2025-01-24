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
#ifdef WINDOWS
#    include <windows.h>
#endif

#define NUDGE_ARG_DUMP_MEMORY 1

static bool saw_thread_init_event = false;
static client_id_t client_id = 0;
static thread_id_t thread_id = 0;
static char path[MAXIMUM_PATH];

static void
event_nudge(void *drcontext, uint64 arg)
{
    dr_fprintf(STDERR, "nudge delivered %d\n", (uint)arg);
    if (arg == NUDGE_ARG_DUMP_MEMORY) {
        dr_memory_dump_spec_t spec;
        spec.size = sizeof(dr_memory_dump_spec_t);
#ifdef WINDOWS
        spec.flags = DR_MEMORY_DUMP_LDMP;
        spec.ldmp_path = (char *)&path;
        spec.ldmp_path_size = MAXIMUM_PATH;
#else
        spec.flags = DR_MEMORY_DUMP_ELF;
        spec.elf_path = (char *)&path;
        spec.elf_path_size = MAXIMUM_PATH;
#endif
        if (!dr_create_memory_dump(&spec)) {
            dr_fprintf(STDERR, "Error: failed to create memory dump.\n");
            return;
        }

        file_t memory_dump_file = dr_open_file(path, DR_FILE_READ);
        if (memory_dump_file < 0) {
            dr_fprintf(STDERR, "Error: failed to read memory dump file: %s.\n", path);
            return;
        }

        uint64 file_size;
        if (!dr_file_size(memory_dump_file, &file_size)) {
            dr_fprintf(STDERR,
                       "Error: failed to read the size of the memory dump file: %s.\n",
                       path);
            dr_close_file(memory_dump_file);
            return;
        }

        if (file_size == 0)
            dr_fprintf(STDERR, "Error: memory dump file %s is empty.\n", path);

        dr_close_file(memory_dump_file);
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
    thread_id_t tid = dr_get_thread_id(drcontext);
    if (tid != thread_id)
        return;

    dr_fprintf(STDERR, "thread init\n");
    saw_thread_init_event = true;

    if (!dr_nudge_client(client_id, NUDGE_ARG_DUMP_MEMORY))
        dr_fprintf(STDERR, "Error: failed to nudge client!\n");
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    void *drcontext = dr_get_current_drcontext();
    thread_id = dr_get_thread_id(drcontext);

    client_id = id;
    dr_register_exit_event(dr_exit);
    dr_register_thread_init_event(dr_thread_init);
    dr_register_nudge_event(event_nudge, id);
    dr_fprintf(STDERR, "thank you for testing memory dump\n");
}
