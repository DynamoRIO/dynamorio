/* **********************************************************
 * Copyright (c) 2017-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2004-2009 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2004-2007 Determina Corp. */

/*
 * stats.h - statistics related exports and internal macros
 */

#ifndef STATS_H
#define STATS_H

#ifdef KSTATS
void
kstat_init(void);
void
kstat_exit(void);
void
kstat_thread_init(dcontext_t *dcontext);
void
kstat_thread_exit(dcontext_t *dcontext);
void
dump_thread_kstats(dcontext_t *dcontext);

/* for debugging only */
#    ifdef DEBUG
void
kstats_dump_stack(dcontext_t *dcontext);
#    endif

/* Kstat variable */
typedef struct {
    /* number of executions */
    uint num_self;
    /* Currently only time data - more performance counters could be
     * added in the future if necessary
     */

    /* We could always measure total elapsed time in subroutines, by
     * keeping their start times on the stack.  However, it is better
     * to use essentially a single running timer that gets reset every
     * time we enter a new level, and then we add up all outstanding
     * times.  This allows us to be more selective not to time spent
     * waiting or when context switched.
     *
     * It in effect lets use as a drop-in replacement a sampling based
     * counter for time in self.  If we could add an action, every
     * time we get a sample we can increment the TOS entry.
     * Currently, we are measuring time at start/stop to calculate the
     * exact time with some overhead.
     */

    timestamp_t total_self; /* measured time in this block */
    timestamp_t total_sub;  /* nested subpaths */
    /* cumulative = self time + subpaths */
    /* total time for all calls */
    timestamp_t min_cum;        /* most stable number for the common case */
    timestamp_t max_cum;        /* a high number shows need for subpaths */
    timestamp_t total_outliers; /* attempt to account for context switches */
    /* VTune also keeps self_wait time, but we do not propagate that time up */
} kstat_variable_t;

/* All kstat variables - expanded as a structure instead of an array
 * referenced by index allows easy to read debugger pretty prints
 */
typedef struct {
#    define KSTAT_DEF(desc, name) kstat_variable_t name;
#    define KSTAT_SUM(description, name, var1, var2) kstat_variable_t name;
#    include "kstatsx.h"
#    undef KSTAT_SUM
#    undef KSTAT_DEF
} kstat_variables_t;

/* Stack entry for an active execution node */
typedef struct {
    kstat_variable_t *var;
    timestamp_t self_time;
    timestamp_t subpath_time;
    timestamp_t outlier_time;
} kstat_node_t;

enum { KSTAT_MAX_DEPTH = 16 };

/* Information about a current execution path */
typedef struct {
    volatile uint depth; /* Volatile for signal safety. */
    timestamp_t last_start_time;
    timestamp_t last_end_time;
    kstat_node_t node[KSTAT_MAX_DEPTH];
} kstat_stack_t;

/* Thread local context and collected data */
typedef struct {
    thread_id_t thread_id;
    kstat_variables_t vars_kstats;
    kstat_stack_t stack_kstats;
    file_t outfile_kstats;
} thread_kstats_t;

extern timestamp_t kstat_ignore_context_switch;

#    define KSTAT_THREAD_NO_PV_START(dc)                                   \
        do {                                                               \
            dcontext_t *cur_dcontext = (dc);                               \
            if (cur_dcontext != NULL && cur_dcontext != GLOBAL_DCONTEXT && \
                cur_dcontext->thread_kstats != NULL) {                     \
                kstat_stack_t *ks = &cur_dcontext->thread_kstats->stack_kstats;

#    define KSTAT_THREAD_NO_PV_END() \
        }                            \
        }                            \
        while (0)

/* ensures statement can safely use ks=stack_kstats and pv=&vars_kstats.name,
 */
#    define KSTAT_THREAD(name, statement)                                             \
        KSTAT_THREAD_NO_PV_START(get_thread_private_dcontext())                       \
        kstat_variable_t *pv UNUSED = &cur_dcontext->thread_kstats->vars_kstats.name; \
        UNUSED_VARIABLE(pv);                                                          \
        statement;                                                                    \
        KSTAT_THREAD_NO_PV_END()

#    define KSTAT_OTHER_THREAD(dc, name, statement)                                   \
        KSTAT_THREAD_NO_PV_START(dc)                                                  \
        kstat_variable_t *pv UNUSED = &cur_dcontext->thread_kstats->vars_kstats.name; \
        UNUSED_VARIABLE(pv);                                                          \
        statement;                                                                    \
        KSTAT_THREAD_NO_PV_END()

/* makes sure we're matching start/stop */
#    define KSTAT_TOS_MATCHING(name) KSTAT_THREAD(name, kstat_tos_matching_var(ks, pv))

/* optional - serialize instruction stream before measurement on my
 * laptop the overhead of an empty inner block is 95 cycles without
 * serialization vs 222 cycles with serialization,
 * too much overhead for little extra stability */
#    define KSTAT_SERIALIZE_INSTRUCTIONS() /* no SERIALIZE_INSTRUCTIONS() */

/* optional - accumulate outliers separately for comparable analysis,
 * for 88 vs 98 cycles in inner block - definitely worth it, e.g. two
 * simultaneously run loops get same user CPU as single run, although
 * wall clock time is twice longer.
 */
#    ifdef KSTAT_NO_OUTLIERS
#        define CORRECT_OUTLIER(stmt) /* ignore extra logic */
#    else
/* We do not update self_time on outlier so that higher level stats
 * are also able to discount the context switches from self.
 */
#        define CORRECT_OUTLIER(stmt) stmt
#    endif /* KSTAT_NO_OUTLIERS */

/* update current timer before starting a new one */
#    define UPDATE_CURRENT_COUNTER(kstack)                                            \
        do {                                                                          \
            KSTAT_SERIALIZE_INSTRUCTIONS();                                           \
            RDTSC_LL((kstack)->last_end_time);                                        \
            /* a dummy entry at 0 is always present, callers ASSERT  */               \
            CORRECT_OUTLIER(if ((kstack)->last_end_time - (kstack)->last_start_time > \
                                kstat_ignore_context_switch)(kstack)                  \
                                ->node[(kstack)->depth - 1]                           \
                                .outlier_time +=                                      \
                            (kstack)->last_end_time - (kstack)->last_start_time;      \
                            else)                                                     \
            (kstack)->node[(kstack)->depth - 1].self_time +=                          \
                (kstack)->last_end_time - (kstack)->last_start_time;                  \
            (kstack)->last_start_time = (kstack)->last_end_time;                      \
        } while (0)

/* since we want to add kstats to logging functions we can't use LOG in it */
#    define KSTAT_INTERNAL_DEBUG(kstack)                                      \
        LOG(THREAD, LOG_STATS, 3,                                             \
            "ks start=" FIXED_TIMESTAMP_FORMAT " end=" FIXED_TIMESTAMP_FORMAT \
            " cur diff=" FIXED_TIMESTAMP_FORMAT " depth=%d\n",                \
            (kstack)->last_start_time, (kstack)->last_end_time,               \
            (kstack)->node[(kstack)->depth - 1].self_time, (kstack)->depth);

#    ifdef DEBUG
/* avoid recursion in logging KSTAT - do not checkin uses */
#        define KSTAT_DUMP_STACK(kstack)                                       \
            if ((kstack)->node[(kstack)->depth - 1].var !=                     \
                &cur_dcontext->thread_kstats->vars_kstats.logging) {           \
                LOG(THREAD_GET, LOG_STATS, 1, "ks %s:%d\n"__FILE__, __LINE__); \
                kstats_dump_stack(cur_dcontext);                               \
            }
#    endif

/* Most of these could be static inline's but DEBUG builds aren't
 * inlining these and we want to minimize the measurement overhead
 * FIXME: consider replacing them if we have too many copies, it will
 * also clean up the argument evaluation in KSTAT_THREAD_NO_PV.
 *
 * KSTAT_THREAD encloses these statements in a do {} while (0) block,
 * FIXME: if DEBUG stats are at all interesting may want to remove the
 * one's from here if a compiler in fact adds real loops - e.g. cl does
 */

/* starts a timer */
#    define kstat_start_var(kstack, pvar)                                           \
        do {                                                                        \
            uint depth = (kstack)->depth;                                           \
            DODEBUG({                                                               \
                if (depth >= KSTAT_MAX_DEPTH)                                       \
                    kstats_dump_stack(cur_dcontext);                                \
            });                                                                     \
            ASSERT(depth < KSTAT_MAX_DEPTH && "probably missing a STOP on return"); \
            /* Increment depth first for re-entrancy. */                            \
            (kstack)->depth++;                                                      \
            (kstack)->node[depth].var = pvar;                                       \
            UPDATE_CURRENT_COUNTER((kstack));                                       \
            /* reset new counter */                                                 \
            (kstack)->node[depth].subpath_time = 0;                                 \
            (kstack)->node[depth].self_time = 0;                                    \
            (kstack)->node[depth].outlier_time = 0;                                 \
        } while (0)

/* updates which variable will be counted */
#    define kstat_switch_var(kstack, pvar) \
        (kstack)->node[(kstack)->depth - 1].var = (pvar)

/* allow mismatched start/stop - for use with KSWITCH */
#    define kstat_stop_not_matching_var(kstack, pvar)                         \
        do {                                                                  \
            timestamp_t last_cum;                                             \
            kstat_stop_not_propagated_var(kstack, pvar, &last_cum);           \
            if ((kstack)->depth > 0) /* update subpath in parent */           \
                (kstack)->node[(kstack)->depth - 1].subpath_time += last_cum; \
        } while (0)

#    define kstat_stop_matching_var(kstack, pvar)                                   \
        do {                                                                        \
            DODEBUG({                                                               \
                if (ks->node[ks->depth - 1].var != pvar)                            \
                    kstats_dump_stack(cur_dcontext);                                \
            });                                                                     \
            ASSERT(ks->node[ks->depth - 1].var == pvar && "stop not matching TOS"); \
            kstat_stop_not_matching_var(ks, pvar);                                  \
        } while (0)

#    define kstat_tos_matching_var(kstack, pvar) (ks->node[ks->depth - 1].var == pv)

#    define kstat_stop_not_propagated_var(kstack, pvar, pcum)                           \
        do {                                                                            \
            uint depth = (kstack)->depth - 1;                                           \
            ASSERT((kstack)->depth > 1);                                                \
            UPDATE_CURRENT_COUNTER((kstack));                                           \
            (kstack)->node[depth].var->num_self++;                                      \
            (kstack)->node[depth].var->total_self += (kstack)->node[depth].self_time;   \
            (kstack)->node[depth].var->total_sub += (kstack)->node[depth].subpath_time; \
            (kstack)->node[depth].var->total_outliers +=                                \
                (kstack)->node[depth].outlier_time;                                     \
            *pcum =                                                                     \
                (kstack)->node[depth].self_time + (kstack)->node[depth].subpath_time;   \
            /* FIXME: an outlier should be counted as a NaN for outliers on subpaths    \
             */                                                                         \
            if (*pcum > 0 && (kstack)->node[depth].var->min_cum > *pcum)                \
                (kstack)->node[depth].var->min_cum = *pcum;                             \
            if ((kstack)->node[depth].var->max_cum < *pcum)                             \
                (kstack)->node[depth].var->max_cum = *pcum;                             \
            /* Decrement after reads for re-entrancy. */                                \
            (kstack)->depth--;                                                          \
        } while (0)

/* FIXME: we may have to add a type argument to KSTAT_DEF saying whether
 * a variable should be propagated or not - here we assume all are propagated
 */
#    define kstat_stop_rewind_var(kstack, pvar)           \
        do {                                              \
            kstat_stop_not_matching_var(kstack, ignored); \
        } while (kstack->node[kstack->depth /* the removed */].var != pvar)

/* This is essentially rewind-until, stopping BEFORE deleting pvar
 * FIXME: we may have to add a type argument to KSTAT_DEF saying whether
 * a variable should be propagated or not - here we assume all are propagated
 */
#    define kstat_stop_longjmp_var(kstack, pvar)                                  \
        while (kstack->node[kstack->depth - 1 /* to be removed */].var != pvar) { \
            kstat_stop_not_matching_var(kstack, ignored);                         \
        }
#endif /* KSTATS */

#endif /* STATS_H */
