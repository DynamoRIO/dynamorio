/* **********************************************************
 * Copyright (c) 2021-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2008-2010 VMware, Inc.  All rights reserved.
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

static thread_id_t injection_tid;
static bool first_thread = true;

static void
dr_exit(void)
{
    dr_fprintf(STDERR, "done\n");
}

static void
dr_thread_init(void *drcontext)
{
    thread_id_t tid = dr_get_thread_id(drcontext);
#ifdef WINDOWS
    /* On Windows there is an additional thread used for attach injection.
     * XXX i#725: We should remove it or hide it, and not rely on it here.
     */
    if (tid != injection_tid && first_thread) {
        first_thread = false;
        dr_fprintf(STDERR, "thread init\n");
    }
#else
    dr_fprintf(STDERR, "thread init\n");
#endif
}

#ifdef WINDOWS
static bool
dr_exception_event(void *drcontext, dr_exception_t *excpt)
{
    thread_id_t tid = dr_get_thread_id(drcontext);
    dr_fprintf(STDERR, "exception in thread %p\ninjection thread %p\n", tid,
               injection_tid);

    dr_fprintf(STDERR, "ExceptionCode=%08x\n", excpt->record->ExceptionCode);
    dr_fprintf(STDERR, "ExceptionFlags=%08x\n", excpt->record->ExceptionFlags);
    dr_fprintf(STDERR, "ExceptionAddress=%p\n", excpt->record->ExceptionAddress);
    dr_fprintf(STDERR, "parameters:\n");
    for (DWORD i = 0; i < excpt->record->NumberParameters; i++) {
        dr_fprintf(STDERR, "parameters[%ld]:%p\n", i,
                   excpt->record->ExceptionInformation[i]);
    }

    return true;
}
#endif

static void
event_post_attach(void)
{
    dr_fprintf(STDERR, "attach\n");
}

static void
event_pre_detach(void)
{
    dr_fprintf(STDERR, "detach\n");
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    void *drcontext = dr_get_current_drcontext();
    injection_tid = dr_get_thread_id(drcontext);
    dr_register_exit_event(dr_exit);
    dr_register_thread_init_event(dr_thread_init);
    dr_register_pre_detach_event(event_pre_detach);
#ifdef WINDOWS
    dr_register_exception_event(dr_exception_event);
#endif
    if (!dr_register_post_attach_event(event_post_attach))
        dr_fprintf(STDERR, "Failed to register post-attach event");
    dr_fprintf(STDERR, "thank you for testing detach\n");
}
