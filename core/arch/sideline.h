/* **********************************************************
 * Copyright (c) 2002-2008 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2002-2003 Massachusetts Institute of Technology */

/*
 * sideline.h - multiprocessor support
 */

#ifndef _SIDELINE_H_
#define _SIDELINE_H_ 1

/* sampled by sideline thread to find hot traces */
extern volatile fragment_t *sideline_trace;

extern int num_processors;

/* used for synchronization with other threads */
extern mutex_t sideline_lock;
extern thread_id_t pause_for_sideline;
extern event_t paused_for_sideline_event;
extern event_t resume_from_sideline_event;
extern mutex_t do_not_delete_lock;
/* initialization */
void
sideline_init(void);

/* atexit cleanup */
void
sideline_exit(void);

/* tells sideline threads when dynamo is active */
void
sideline_start(void);
void
sideline_stop(void);

/* adds profiling for identification of hot traces  */
void
add_sideline_prefix(dcontext_t *dcontext, instrlist_t *trace);
void
finalize_sideline_prefix(dcontext_t *dcontext, fragment_t *trace_f);

/* called when target thread is at safe point so a replaced trace can
 * be completely removed
 */
void
sideline_cleanup_replacement(dcontext_t *dcontext);

/* called by app thread to remove f from sideline data structures */
void
sideline_fragment_delete(fragment_t *f);

/* calls optimize_function on the trace of interest, safely handles replacement */
fragment_t *
sideline_optimize(fragment_t *f,
                  void (*remove_sideline_profiling)(dcontext_t *, instrlist_t *),
                  void (*optimize_function)(dcontext_t *, fragment_t *, instrlist_t *));

#endif /* _SIDELINE_H_ */
