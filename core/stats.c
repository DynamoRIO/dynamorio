/* **********************************************************
 * Copyright (c) 2011-2021 Google, Inc.  All rights reserved.
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
 * stats.c - statistics related functionality
 */

/*
 * Note that timer numbers from TSC cannot fully adjust for thread
 * context switches.  We truly want a virtual time stamp counter that
 * is thread specific, yet without OS support that's not possible.  If
 * we could read the ETHREAD data that counts number of context
 * switches, we should also regularly print the ThreadTimes data from
 * ZwQueryInformationThread for sanity checking.  See
 * KSTAT_OUTLIER_THRESHOLD_MS for the current solution of not adding
 * up at least the largest outliers, although it can't help with all.
 *
 * We could also add sampling collected statistics on platforms on which
 * we can get custom action on profiling interrupts (Linux only).
 *
 * Numbers don't seem to be very reliable in VMware which isn't that
 * surprising - considering RDTSC is not easy to virtualize.  Even the
 * QueryPerformanceFrequency call is not producing meaningful data,
 * and minimums of 1 are often seen.
 */

#include "globals.h"
#include "dr_stats.h"
#include "stats.h"

#ifdef KSTATS

#    ifdef KSTAT_UNIT_TEST
/* FIXME: add a Makefile rule for this */
kstat_variables_t tkv;
kstat_stack_t ks;
#    endif

/* STATIC forward declarations */
static void
kstat_merge_var(kstat_variable_t *destination, kstat_variable_t *source);

static void
kstat_init_variable(kstat_variable_t *kv)
{
    memset(kv, 0x0, sizeof(kstat_variable_t));
    kv->min_cum = (timestamp_t)-1;
}

/* process all sum equations, any more complicated expressions should
 * better be done out of the core
 */
static void
kstats_evaluate_expressions(kstat_variables_t *kvars)
{
    /* sum can be recomputed at any time and target is reinitialized,
     * all chained KSTAT_SUM equations should appear in evaluation order
     */
#    define KSTAT_SUM(desc, name, var1, var2)        \
        kstat_init_variable(&kvars->name);           \
        kstat_merge_var(&kvars->name, &kvars->var1); \
        kstat_merge_var(&kvars->name, &kvars->var2);
#    define KSTAT_DEF(desc, name) /* nothing to do */
#    include "kstatsx.h"
#    undef KSTAT_SUM
#    undef KSTAT_DEF
}

/* equivalent to KSTAT_DEF for the rest of the file */
#    define KSTAT_SUM(desc, name, var1, var2) KSTAT_DEF(desc, name)

static void
kstat_init_variables(kstat_variables_t *ks)
{
#    define KSTAT_DEF(desc, name) kstat_init_variable(&ks->name);
#    include "kstatsx.h"
#    undef KSTAT_DEF
}

/* There is no good minimum value here since a thread can get switched
 * back in a shorter time slice, in case another thread is waiting or
 * has yielded its share.  If valid measurements are indeed taking
 * longer than 1ms then checkpoints in between will be needed to count
 * these properly.  However, a millisecond is quite a lot of time and
 * we shouldn't be doing anything like that.
 */
enum { KSTAT_OUTLIER_THRESHOLD_MS = 1 }; /* 1 ms for now */
timestamp_t kstat_ignore_context_switch;

static timestamp_t kstat_frequency_per_msec;

/* PR 312534: reduce stack usage (gcc does not combine locals) */
static void
kstat_print_individual(file_t outf, kstat_variable_t *kv, const char *name,
                       const char *desc)
{
    print_file(outf,
               "%20s:" FIXED_TIMESTAMP_FORMAT " totc,"
               "%8u num," FIXED_TIMESTAMP_FORMAT " minc," FIXED_TIMESTAMP_FORMAT
               " avg," FIXED_TIMESTAMP_FORMAT " maxc," FIXED_TIMESTAMP_FORMAT
               " self," FIXED_TIMESTAMP_FORMAT " sub,"
               "\n"
               "                   " FIXED_TIMESTAMP_FORMAT " ms," FIXED_TIMESTAMP_FORMAT
               " ms out,"
               "%s\n",
               name, kv->total_self + kv->total_sub, kv->num_self,
               (kv->min_cum == (timestamp_t)-1) ? 0 : kv->min_cum,
               (kv->total_self + kv->total_sub) / kv->num_self, kv->max_cum,
               kv->total_self, kv->total_sub,
               (kv->total_self + kv->total_sub) / kstat_frequency_per_msec,
               kv->total_outliers / kstat_frequency_per_msec, desc);
}

static void
kstat_report(file_t outf, kstat_variables_t *ks)
{
    kstats_evaluate_expressions(ks);
    /* FIXME: Outliers may make the minc number appear smaller than
     * real, should at least mark with a '*' */
#    define KSTAT_DEF(desc, name) \
        if (ks->name.num_self)    \
            kstat_print_individual(outf, &ks->name, #name, desc);
#    include "kstatsx.h"
#    undef KSTAT_DEF
}

DECLARE_CXTSWPROT_VAR(mutex_t process_kstats_lock, INIT_LOCK_FREE(process_kstats_lock));
DECLARE_NEVERPROT_VAR(kstat_variables_t process_kstats,
                      { {
                          0,
                      } });
DECLARE_NEVERPROT_VAR(file_t process_kstats_outfile, INVALID_FILE);

/* Log files are only needed for non-debug builds. */
#    ifndef DEBUG
static const char *
kstats_main_logfile_name(void)
{
    return "process-kstats";
}

static const char *
kstats_thread_logfile_name(void)
{
    return "kstats";
}
#    endif

void
kstat_init()
{
    kstat_frequency_per_msec = get_timer_frequency();
    kstat_ignore_context_switch = KSTAT_OUTLIER_THRESHOLD_MS * kstat_frequency_per_msec;

    LOG(GLOBAL, LOG_STATS, 1, "Processor speed: " UINT64_FORMAT_STRING "MHz\n",
        kstat_frequency_per_msec / 1000);

    /* FIXME: There is no check for TSC feature and whether CR4.TSD is set
     * so we can read it at CPL 3
     */

    if (!DYNAMO_OPTION(kstats))
        return;

    kstat_init_variables(&process_kstats);
#    ifdef DEBUG
    process_kstats_outfile = GLOBAL;
#    else
    /* Open a process-wide kstats file. open_thread_private_file() does the
     * job when passed the appropriate basename (2nd arg).
     */
    process_kstats_outfile = open_log_file(kstats_main_logfile_name(), NULL, 0);
#    endif
}

void
kstat_exit()
{
    if (!DYNAMO_OPTION(kstats))
        return;

    /* report merged process statistics */
    d_r_mutex_lock(&process_kstats_lock);
    print_file(process_kstats_outfile, "Process KSTATS:\n");
    kstat_report(process_kstats_outfile, &process_kstats);
    d_r_mutex_unlock(&process_kstats_lock);

    DELETE_LOCK(process_kstats_lock);

#    ifndef DEBUG
    os_close(process_kstats_outfile);
#    endif
}

static void
kstat_calibrate()
{
    uint i;
    static bool kstats_calibrated = false;
    if (kstats_calibrated)
        return;
    kstats_calibrated = true; /* slight innocent race */

    /* FIXME: once we calculate the overhead of calibrate_empty we can
     * subtract that from every self_time measurement.
     * FIXME: The cost of
     * overhead_nested-overhead_empty should be subtracted from each
     * subpath_time.
     */
    for (i = 0; i < 10000; i++) {
        KSTART(overhead_nested);
        KSTART(overhead_empty);
        KSTOP_NOT_PROPAGATED(overhead_empty);
        KSTOP(overhead_nested);
    }
}

void
kstat_thread_init(dcontext_t *dcontext)
{
    thread_kstats_t *new_thread_kstats;
    if (!DYNAMO_OPTION(kstats))
        return; /* dcontext->thread_kstats stays NULL */

    /* allocated on thread heap - use global if timing initialization matters */
    new_thread_kstats =
        HEAP_TYPE_ALLOC(dcontext, thread_kstats_t, ACCT_STATS, UNPROTECTED);
    LOG(THREAD, LOG_STATS, 2, "thread_kstats=" PFX " size=%d\n", new_thread_kstats,
        sizeof(thread_kstats_t));
    /* initialize any thread stats bookkeeping fields before assigning to dcontext */
    kstat_init_variables(&new_thread_kstats->vars_kstats);
    /* add a dummy node to save one branch in UPDATE_CURRENT_COUNTER */
    new_thread_kstats->stack_kstats.depth = 1;

    new_thread_kstats->thread_id = d_r_get_thread_id();
#    ifdef DEBUG
    new_thread_kstats->outfile_kstats = THREAD;
#    else
    new_thread_kstats->outfile_kstats =
        open_log_file(kstats_thread_logfile_name(), NULL, 0);
#    endif
    dcontext->thread_kstats = new_thread_kstats;

    /* need to do this in a thread after it's initialized */
    kstat_calibrate();

    KSTART_DC(dcontext, thread_measured);

    LOG(THREAD, LOG_STATS, 2, "threads_started\n");
}

static void
kstat_merge_var(kstat_variable_t *destination, kstat_variable_t *source)
{
    destination->num_self += source->num_self;
    destination->total_self += source->total_self;
    destination->total_sub += source->total_sub;
    destination->total_outliers += source->total_outliers;
    if (destination->min_cum > source->min_cum)
        destination->min_cum = source->min_cum;
    if (destination->max_cum < source->max_cum)
        destination->max_cum = source->max_cum;
}

/* make sure sourcevars are merged in only once */
static void
kstat_merge(kstat_variables_t *destinationvars, kstat_variables_t *sourcevars)
{
#    define KSTAT_DEF(desc, name) \
        kstat_merge_var(&destinationvars->name, &sourcevars->name);
#    include "kstatsx.h"
#    undef KSTAT_DEF
}

void
dump_thread_kstats(dcontext_t *dcontext)
{
    if (dcontext->thread_kstats == NULL)
        return;

    /* add thread id's in case outfile is rerouted to process_kstats_outfile */
    print_file(dcontext->thread_kstats->outfile_kstats, "Thread %d KSTATS {\n",
               dcontext->thread_kstats->thread_id);
    kstat_report(dcontext->thread_kstats->outfile_kstats,
                 &dcontext->thread_kstats->vars_kstats);
    print_file(dcontext->thread_kstats->outfile_kstats, "} KSTATS\n");
}

#    ifdef DEBUG
/* We don't keep the variable name, but instead we lookup by addr when necessary */
/* Too convoluted solution but since not common case, we don't bother
 * to initialize a name for each var in kstat_init_variables() */
static const char *
kstat_var_name(dcontext_t *dcontext, kstat_variable_t *kvar)
{
    kstat_variables_t *kvs = &dcontext->thread_kstats->vars_kstats;
#        define KSTAT_DEF(desc, name) \
            if (kvar == &kvs->name)   \
                return #name;
#        include "kstatsx.h"
#        undef KSTAT_DEF
    ASSERT_NOT_REACHED();
    return "#ERROR#";
}

void
kstats_dump_stack(dcontext_t *dcontext)
{
    uint i;
    LOG(THREAD, 1, LOG_STATS, "Thread KSTAT stack:\n");
    if (dcontext->thread_kstats == NULL)
        return;
    for (i = dcontext->thread_kstats->stack_kstats.depth - 1; i > 0; i--) {
        LOG(THREAD, 1, LOG_STATS, "[%d] " PIFX " %s\n", i,
            &dcontext->thread_kstats->stack_kstats.node[i],
            kstat_var_name(dcontext, dcontext->thread_kstats->stack_kstats.node[i].var));
    }
}
#    endif

void
kstat_thread_exit(dcontext_t *dcontext)
{
    thread_kstats_t *old_thread_kstats = dcontext->thread_kstats;
    if (old_thread_kstats == NULL)
        return;
    LOG(THREAD, LOG_ALL, 2, "kstat_thread_exit: kstats stack is:\n");
    DOLOG(2, LOG_STATS, { kstats_dump_stack(dcontext); });
    KSTOP_DC(dcontext, thread_measured);
    ASSERT(old_thread_kstats->stack_kstats.depth == 1);
    dump_thread_kstats(dcontext);

    /* a good time to combine all of these with the global statistics */
    d_r_mutex_lock(&process_kstats_lock);
    kstat_merge(&process_kstats, &old_thread_kstats->vars_kstats);
    d_r_mutex_unlock(&process_kstats_lock);

#    ifndef DEBUG
    close_log_file(dcontext->thread_kstats->outfile_kstats);
#    endif
    /* Disable kstats before freeing memory to avoid use-after-free on free path. */
    dcontext->thread_kstats = NULL;
    /* We need to free kstats even in non-debug b/c unprot local heap is global. */
    HEAP_TYPE_FREE(dcontext, old_thread_kstats, thread_kstats_t, ACCT_STATS, UNPROTECTED);
}

#endif /* KSTATS */

#ifdef KSTAT_UNIT_TEST
uint
kstat_test()
{
    KSTART(measured);
    printf("test %d\n", __LINE__);
    KSTART(empty);
    KSTOP(empty);
    KSTART(empty);
    KSTOP(empty);

    printf("test %d\n", __LINE__);
    KSTART(dr_default);
    KSWITCH(dr_existing_bb);
    KSTOP_NOT_MATCHING(dr_default);

    KSTART(dr_default);
    KSTART(empty);
    KSTOP(empty);
    KSTOP_NOT_MATCHING(dr_default);

    KSTART(dr_default);
    KSTOP_NOT_MATCHING(dr_default);

    printf("test %d\n", __LINE__);
    KSTART(wait_event);
    KSTOP_NOT_PROPAGATED(wait_event);
    printf("test %d\n", __LINE__);

    {
        uint i;
        for (i = 0; i < 100000; i++) {
            KSTART(bb);
            KSTOP(bb);
        }
    }

    {
        uint i, j, k;
        for (i = 0; i < 100; i++) {
            KSTART(iloop);
            for (j = 0; j < 100; j++) {
                KSTART(jloop);
                for (k = 0; k < 100000; k++)
                    /* nothing */;
                KSTOP(jloop);
            }

            KSTOP(iloop);
        }
    }

    KSTART(syscalls);
    KSTART(wait_event);
    KSTOP_NOT_PROPAGATED(wait_event);

    KSTART(wait_event);
    KSTOP(wait_event);

    KSTOP(syscalls);
    printf("test %d\n", __LINE__);
    KSTOP(measured);
}
int
main()
{
    kstat_init();
    kstat_thread_init();

    kstat_test();

    kstat_thread_exit();
    kstat_exit();
}
#endif /* KSTAT_UNIT_TEST */
