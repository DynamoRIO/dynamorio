/* **********************************************************
 * Copyright (c) 2012 Google, Inc.  All rights reserved.
 * Copyright (c) 2007-2009 VMware, Inc.  All rights reserved.
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

#ifndef _NUDGE_H_
#define _NUDGE_H_

#include "dr_config.h"

/* Tiggers a nudge targeting this process.  nudge_action_mask should be drawn from the
 * NUDGE_GENERIC(***) values.  client_id is only relevant for client nudges. */
dr_config_status_t
nudge_internal(process_id_t pid, uint nudge_action_mask, uint64 client_arg,
               client_id_t client_id, uint timeout_ms);

#ifdef WINDOWS /* only Windows uses threads for nudges */
/* The following are exported only so other routines can check their addresses for
 * nudge threads.  They are not meant to be called internally. */
void
generic_nudge_target(nudge_arg_t *arg);
bool
generic_nudge_handler(nudge_arg_t *arg);
/* exit_process is only honored if dcontext != NULL, and exit_code is only honored
 * if exit_process is true
 */
bool
nudge_thread_cleanup(dcontext_t *dcontext, bool exit_process, uint exit_code);
#else

/* This routine may not return */
void
handle_nudge(dcontext_t *dcontext, nudge_arg_t *arg);

/* Only touches thread-private data and acquires no lock */
void
nudge_add_pending(dcontext_t *dcontext, nudge_arg_t *nudge_arg);
#endif

#endif /* _NUDGE_H_ */
