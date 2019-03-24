/* **********************************************************
 * Copyright (c) 2017 Google, Inc.  All rights reserved.
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

/* dr_stats.h: DR's shared memory interface */

#ifndef _DR_STATS_H_
#define _DR_STATS_H_ 1

#include "globals_shared.h"

#ifdef WINDOWS
/* No registry for stats (b/c it requires advapi32.dll which can't be used
 * when injected via user32.dll registry key)
 * Instead, a piece of shared memory with the key base name below holds the
 * total number of DR instances.
 */
#    define DR_SHMEM_KEY "DynamoRIOStatistics"
#elif defined(UNIX)
#    define DYNAMORIO_MAGIC_STRING "DYNAMORIO_MAGIC_STRING"
#    define DYNAMORIO_MAGIC_STRING_LEN 16 /*include trailing \0*/
#endif

#define STAT_NAME_MAX_LEN 50
typedef struct _single_stat_t {
    /* We inline the stat description to make it easy for external processes
     * to view our stats: they don't have to chase pointers, and we could put
     * this in shared memory easily.
     */
    char name[STAT_NAME_MAX_LEN]; /* the description of the stat */
    /* FIXME PR 216209: we'll want 64-bit stats for x64 address regions; we can
     * either add per-stat types, or just widen them all.
     */
    stats_int_t value;
} single_stat_t;

/* Parameters and statistics exported by DR via drmarker.
 * These should be treated as read-only except for the log_ fields.
 * Unless otherwise mentioned, these stats are all process-wide.
 */
#define NUM_EVENTS 27
typedef struct _dr_statistics_t {
#ifdef UNIX
    char magicstring[DYNAMORIO_MAGIC_STRING_LEN];
#endif
    process_id_t process_id;         /* process id */
    char process_name[MAXIMUM_PATH]; /* process name */
    uint logmask;                    /* what to log */
    uint loglevel;                   /* how much detail to log */
    char logdir[MAXIMUM_PATH];       /* full path of logging directory */
    uint64 perfctr_vals[NUM_EVENTS];
    uint num_stats;
#ifdef NOT_DYNAMORIO_CORE
    /* variable-length to avoid tying to specific DR version */
    single_stat_t stats[1];
#else
#    ifdef DEBUG
#        define STATS_DEF(desc, name) single_stat_t name##_pair;
#    else
#        define RSTATS_DEF(desc, name) single_stat_t name##_pair;
#    endif
#    include "statsx.h"
#    undef STATS_DEF
#    undef RSTATS_DEF
#endif
} dr_statistics_t;

#ifndef NOT_DYNAMORIO_CORE
/* Thread local statistics */
typedef struct {
    thread_id_t thread_id;
    /* transactional stats, for multiple stats invariants to hold */
    mutex_t thread_stats_lock;
    /* TODO: We may also want to print another threads's stats without
     * necessarily halting it, TODO: add stat name##_delta, which
     * should be applied as a batch to the safe to read values.  The
     * basic idea of transactional stats is that uncommitted changes
     * are not visible to readers.  Some invariants between
     * statistics, i.e. A=B+C should hold at the dump/committed
     * points.
     *
     * The plan is:
     *   1) delta accessed w/o lock only by the owning thread,
     *   2) on dump any other thread which only reads the committed
     *     values while holding the commit lock,
     *   3) The owning thread is the single writer to the committed
     *      values to apply the deltas, while holding the commit lock.
     *
     * Used for other threads to be able to request thread local stats,
     * and also for the not fully explained self-interruption on linux?
     */
#    ifdef DEBUG
#        define STATS_DEF(desc, name) stats_int_t name##_thread;
#    else
#        define RSTATS_DEF(desc, name) stats_int_t name##_thread;
#    endif
#    include "statsx.h"
} thread_local_statistics_t;
#    undef STATS_DEF
#    undef RSTATS_DEF
#endif /* !NOT_DYNAMORIO_CORE */

#endif /* _DR_STATS_H_ */
