/* **********************************************************
 * Copyright (c) 2012-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
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
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2000-2001 Hewlett-Packard Company */

/* file "monitor.h"
 */

#ifndef _MONITOR_H_
#define _MONITOR_H_ 1

#include "fragment.h" /* for trace_bb_info_t, and for custom traces
                       * the "fragment_t wrapper" struct */

/* synchronization of shared traces */
extern mutex_t trace_building_lock;

void
d_r_monitor_init(void);
void
d_r_monitor_exit(void);
void
monitor_thread_init(dcontext_t *dcontext);
void
monitor_thread_exit(dcontext_t *dcontext);

/* re-initializes non-persistent memory */
void
monitor_thread_reset_init(dcontext_t *dcontext);
/* frees all non-persistent memory */
void
monitor_thread_reset_free(dcontext_t *dcontext);

void
monitor_remove_fragment(dcontext_t *dcontext, fragment_t *f);
bool
monitor_delete_would_abort_trace(dcontext_t *dcontext, fragment_t *f);
bool
monitor_is_linkable(dcontext_t *dcontext, fragment_t *from_f, linkstub_t *from_l,
                    fragment_t *to_f, bool have_link_lock, bool mark_new_trace_head);
void
mark_trace_head(dcontext_t *dcontext, fragment_t *f, fragment_t *src_f,
                linkstub_t *src_l);

enum {
    TRACE_HEAD_YES = 0x01,
    TRACE_HEAD_OBTAINED_LOCK = 0x02,
};

/* Returns TRACE_HEAD_* flags indicating whether to_tag should be
 * a trace head.
 * For -shared_bbs, will return TRACE_HEAD_OBTAINED_LOCK if the
 * change_linking_lock is not already held (meaning from_l->fragment is
 * private) and the to_tag is FRAG_SHARED and TRACE_HEAD_YES is being returned,
 * since the change_linking_lock must be held and the TRACE_HEAD_YES result
 * re-verified.  In that case the caller must free the change_linking_lock.
 */
uint
should_be_trace_head(dcontext_t *dcontext, fragment_t *from_f, linkstub_t *from_l,
                     app_pc to_tag, uint to_flags, bool have_link_lock);

fragment_t *
monitor_cache_enter(dcontext_t *dcontext, fragment_t *f);
void
monitor_cache_exit(dcontext_t *dcontext);
bool
is_building_trace(dcontext_t *dcontext);
app_pc
cur_trace_tag(dcontext_t *dcontext);
void *
cur_trace_vmlist(dcontext_t *dcontext);

/* This routine internally calls enter_couldbelinking, thus it is safe
 * to call from any linking state. Restores linking to previous state at exit.
 * If calling on another thread, caller should be synchornized with that thread
 * (either via flushing synch or thread_synch methods) FIXME : verify all users
 * on other threads are properly synchronized
 */
void
trace_abort(dcontext_t *dcontext);

/* Equivalent to trace_abort, except that lazily deleted fragments are cleaned
 * up eagerly.  Can only be called at safe points when we know the app is not
 * executing in the fragment, such as thread termination or reset events.
 */
void
trace_abort_and_delete(dcontext_t *dcontext);

void
thcounter_range_remove(dcontext_t *dcontext, app_pc start, app_pc end);

bool
mangle_trace_at_end(void);

/* trace head counters are thread-private and must be kept in a
 * separate table and not in the fragment_t structure.
 * FIXME: may want to do this for non-shared-cache, since persistent counters
 * may mitigate the performance hit of a small bb cache -- but for that
 * we could keep the counters in future_fragment_t when a bb dies and
 * re-initialize to that value when it comes back.
 */

typedef struct _trace_head_counter_t {
    app_pc tag;
    uint counter;
} trace_head_counter_t;

typedef struct _trace_bb_build_t {
    trace_bb_info_t info;
    /* PR 299808: we need to check bb bounds at emit time.  Also used
     * for trace state translation.
     */
    void *vmlist;
    instr_t *end_instr;
    /* i#806: to support elision, we need to know whether each block ends
     * in a control transfer or not, so we can find the between-bb ctis
     * that need to be mangled.
     */
    bool final_cti;
} trace_bb_build_t;

typedef struct _monitor_data_t {
    /* Fields used by monitor, but also by arch-specific code, so it's
     * exported in the header file.
     * Needs to be in separate struct to share across callbacks.
     */
    app_pc trace_tag;           /* tag of trace head */
    uint trace_flags;           /* FRAGMENT_ flags for trace */
    instrlist_t trace;          /* place to build the instruction trace */
    byte *trace_buf;            /* place to temporarily store instr bytes */
    uint trace_buf_size;        /* length of trace_buf in bytes */
    uint trace_buf_top;         /* index of next free location in trace_buf */
    void *trace_vmlist;         /* list of vmareas trace touches */
    uint num_blks;              /* count of the number of blocks in trace */
    trace_bb_build_t *blk_info; /* info for all basic blocks making up trace */
    uint blk_info_length;       /* length of blk_info array */
    uint emitted_size;          /* calculated final trace size once emitted */

    /* private copy of shared bb for trace building only
     * equals the previous last_fragment that was shared
     */
    fragment_t *last_copy;
    fragment_t *last_fragment; /* for restoring (can't just use last_exit) */
    uint last_fragment_flags;  /* for restoring */

    /* trace head counters are thread-private and must be kept in a
     * separate table and not in the fragment_t structure.
     */
    generic_table_t *thead_table;

    /* PR 299808: we re-build each bb and pass to the client */
    instrlist_t unmangled_ilist;
    instrlist_t *unmangled_bb_ilist; /* next bb */
    /* cache at start of trace building whether we're going to pass to client */
    bool pass_to_client;
    /* Record whether final block ends in syscall or int.
     * FIXME: remove once we have PR 307284.
     */
    uint final_exit_flags;

    fragment_t wrapper; /* for creating new shadowed trace heads */
} monitor_data_t;

/* PR 204770: use trace component bb tag for RCT source address */
app_pc
get_trace_exit_component_tag(dcontext_t *dcontext, fragment_t *f, linkstub_t *l);

#endif /* _MONITOR_H_ */
