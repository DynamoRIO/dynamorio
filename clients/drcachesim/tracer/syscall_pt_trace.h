/* **********************************************************
 * Copyright (c) 2022 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* syscall_pt_trace.h: header of module for recording kernel PT traces for every syscall.
 */

#ifndef _SYSCALL_PT_TRACE_
#define _SYSCALL_PT_TRACE_ 1

#include "dr_api.h"
#include "trace_entry.h"

/**
 * Name of drmgr instrumentation pass priorities for event_pre_syscall,
 * event_post_syscall, event_thread_init and event_thread_exit.
 */
#define DRMGR_PRIORITY_NAME_SYSCALL_PT_TRACE "syscall_pt_trace"

/**
 * Priorities of drmgr instrumentation passes used by syscall_pt_trace.
 */
enum {
    /**
     * Priority of event_pre_syscall, event_post_syscall, event_thread_init and
     * event_thread_exit.
     */
    DRMGR_PRIORITY_SYSCALL_PT_TRACE = 1000,
};

/* Initializes the syscall_pt_trace module. Each call must be paired with a corresponding
 * call to syscall_pt_trace_exit().
 */
bool
syscall_pt_trace_init(char *kernel_logsubdir,
                      ssize_t (*write_file)(file_t file, const void *data, size_t count),
                      bool (*post_syscall_trace)(void *drcontext,
                                                 int recorded_syscall_id));

/* Cleans up the syscall_pt_trace module */
void
syscall_pt_trace_exit(void);

#endif /* _SYSCALL_PT_TRACE_ */
