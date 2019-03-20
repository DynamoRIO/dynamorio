/* **********************************************************
 * Copyright (c) 2011-2019 Google, Inc.  All rights reserved.
 * Copyright (c) 2002-2009 VMware, Inc.  All rights reserved.
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
/* Copyright (c) 2002 Hewlett-Packard Company */

/*
 * sideline.c - simultaneous optimization support
 */

#include "../globals.h"

#ifdef SIDELINE /* around whole file */

/****************************************************************************/
/* include files and typedefs */

#    ifdef WINDOWS
#        include <windows.h>
#        include <ntdll.h>
#        define RUN_SIG WINAPI
#    else                        /* UNIX */
#        include <sys/types.h>   /* for wait */
#        include <sys/wait.h>    /* for wait */
#        include <linux/sched.h> /* for clone */
#        include <signal.h>      /* for SIGCHLD */
#        include <unistd.h>      /* for nice */
#        define RUN_SIG
typedef pid_t thread_t;
#        define THREAD_STACK_SIZE \
            (32 * 1024) /* FIXME: why different than DYNAMORIO_STACK_SIZE? */
#    endif

#    include "sideline.h"
#    include "arch.h"
#    include "instr.h"
#    include "instrlist.h"
#    include "decode.h"
#    include "../fragment.h"
#    include "../emit.h"
#    include "../link.h"
#    include "../fcache.h"

#    define OPTVERB_3 4
//#define VERB_3 3
#    define VERB_3 4
#    define VERB_2 2

/* minimum number of samples before a trace is optimized */
#    define SAMPLE_COUNT_THRESHOLD 3
/* frequency, in number of samples, at which we optimize hottest trace */
#    define SAMPLE_TO_OPTIMIZE_RATIO 5000

/****************************************************************************/
/* global vars */

/* sampled by sideline thread to find hot traces */
volatile fragment_t *sideline_trace;

/* number of processors we're running on */
int num_processors;

/* used to signal which thread we need to pause in d_r_dispatch */
thread_id_t pause_for_sideline;
event_t paused_for_sideline_event;
event_t resume_from_sideline_event;

DECLARE_CXTSWPROT_VAR(mutex_t sideline_lock, INIT_LOCK_FREE(sideline_lock));

/* for sideline logging */
file_t logfile;

/****************************************************************************/
/* sampling data structures */

/*
  data structure for samples: either counter field in fragment_t ->
  waste of space (have separate trace_t?), or have own hashtable here --
  not worth it to have data structure that maintains sorted order,
  just have fast lookup to incr counter, every so often can do linear
  search for top => have own hashtable, then don't have to synch w/
  thread's ftable.  Instead of linear search, can simply optimize frag
  when incrementing its count and hitting some threshold.
*/

typedef struct _sample_entry_t {
    ptr_uint_t tag;
    int counter;
    struct _sample_entry_t *next; /* for chaining entries */
} sample_entry_t;

typedef struct _sample_table_t {
    uint hash_bits;
    uint hash_mask;
    hash_function_t hash_func;
    uint hash_mask_offset; /* ignores given number of LSB bits */
    uint capacity;
    uint entries;
    mutex_t lock;           /* lock to sequence update of this table */
    sample_entry_t **table; /* hash_bits-bit addressed hash table */
} sample_table_t;

#    define INITIAL_HASH_BITS 12

static sample_table_t table;
DECLARE_CXTSWPROT_VAR(mutex_t do_not_delete_lock, INIT_LOCK_FREE(do_not_delete_lock));
static fragment_t *fragment_now_optimizing;

/* List to remember fragments we've replaced but can't delete until
 * owning thread reaches a safe point.
 * List of lists of fragment_t*'s, one sublist per dcontext.
 */

typedef struct _remember_entry_t {
    fragment_t *f;
    struct _remember_entry_t *next;
} remember_entry_t;

typedef struct _remember_list_t {
    dcontext_t *dcontext;
    remember_entry_t *list;
    struct _remember_list_t *next;
} remember_list_t;

static remember_list_t *remember;
DECLARE_CXTSWPROT_VAR(static mutex_t remember_lock, INIT_LOCK_FREE(remember_lock));

static int num_samples;

#    ifdef DEBUG
static int num_optimized;
static int num_opt_with_no_synch;
#    endif

/****************************************************************************/
/* thread synchronization */

static thread_id_t child_tid;
#    ifdef WINDOWS
static HANDLE child_handle;
#    else /* UNIX */
static thread_t child;
static void *stack;
#    endif

static event_t wake_event;
static event_t asleep_event;
static event_t exited_event;

/* both of these are read by child and written by parent */
static volatile bool child_sleep;
static volatile bool child_exit;

/****************************************************************************/
/* forward declarations */

static int RUN_SIG
sideline_run(void *arg);
static void
sideline_sample(void);
static void
sideline_examine_traces(void);
static sample_entry_t *
update_sample_entry(uint tag);
static sample_entry_t *
find_hottest_entry(void);
static void
remove_sample_entry(uint tag);

static void
add_remember_entry(dcontext_t *dcontext, fragment_t *f);
#    ifdef UNIX
static thread_t
create_thread(int (*fcn)(void *), void *arg, void **stack);
static void
delete_thread(thread_t thread, void *stack);
#    endif

/****************************************************************************/

/* initialization */
void
sideline_init()
{
#    ifdef WINDOWS
    int res;
#    endif
    pause_for_sideline = (thread_id_t)0;
    paused_for_sideline_event = create_event();
    resume_from_sideline_event = create_event();

    num_processors = get_num_processors();
    LOG(GLOBAL, LOG_TOP | LOG_SIDELINE, 1, "Number of processors: %d\n", num_processors);

    wake_event = create_event();
    asleep_event = create_event();
    exited_event = create_event();
    child_exit = false;
    child_sleep = true;

    table.hash_bits = INITIAL_HASH_BITS;
    table.hash_mask = HASH_MASK(table.hash_bits);
    table.hash_func = HASH_FUNCTION_NONE;
    table.capacity = HASHTABLE_SIZE(table.hash_bits);
    table.entries = 0;
    ASSIGN_INIT_LOCK_FREE(table.lock, sideline_table_lock);
    table.table = (sample_entry_t **)global_heap_alloc(
        table.capacity * sizeof(sample_entry_t *) HEAPACCT(ACCT_SIDELINE));
    memset(table.table, 0, table.capacity * sizeof(sample_entry_t *));
    fragment_now_optimizing = NULL;

    remember = NULL;

    num_samples = 0;
#    ifdef DEBUG
    num_optimized = 0;
    num_opt_with_no_synch = 0;
#    endif

#    ifdef WINDOWS
    /* start thread suspended so can add_thread before it runs */
    child_handle =
        create_thread(NT_CURRENT_PROCESS, IF_X64_ELSE(true, false), (void *)sideline_run,
                      NULL, NULL, 0, 15 * PAGE_SIZE, 12 * PAGE_SIZE, true, &child_tid);

#        if 0 /* need non-kernel32 implementation, use NtSetInformationThread */
    /* give sideline thread a lower priority */
    SetThreadPriority(child_handle, THREAD_PRIORITY_BELOW_NORMAL);
    /* if want to glue thread to a single processor, use
     * SetThreadAffinity function -- I don't think we want to do that though
     */
#        else
    ASSERT_NOT_IMPLEMENTED(false);
#        endif

#    else  /* UNIX */
    child = create_thread(sideline_run, NULL, &stack);
    child_tid = child;

    /* priority of child can only be set by child itself, in sideline_run() */
#    endif /* UNIX */

    /* tell dynamo core about new thread so it won't be treated as app thread */
    /* we created without CLONE_THREAD so its own thread group */
    add_thread(IF_WINDOWS_ELSE(child_handle, child_tid) child_tid, false, NULL);

    LOG(GLOBAL, LOG_SIDELINE, 1, "Sideline thread (id " TIDFMT ") created\n", child_tid);

#    ifdef WINDOWS
    /* now let child thread run */
    res = nt_thread_resume(child_handle, NULL);
    ASSERT(res);
#    endif

    sideline_start();
}

/* atexit cleanup */
void
sideline_exit()
{
    uint i;
    sample_entry_t *sample;
    remember_list_t *l, *nextl;
    remember_entry_t *e, *nexte;

    if (child_exit) {
        /* we were invoked in recursive loop through assert in delete_thread */
        return;
    }

    LOG(logfile, LOG_SIDELINE, VERB_3, "sideline_exit\n");

    child_exit = true;

    if (pause_for_sideline != 0) {
        /* sideline thread could be waiting for another thread */
        signal_event(paused_for_sideline_event);
    }

    if (child_sleep) {
        child_sleep = false;
        signal_event(wake_event);
    }
    wait_for_event(exited_event, 0);

#    ifdef WINDOWS
    /* wait for child to die */
    nt_wait_event_with_timeout(child_handle, INFINITE_WAIT);
#    else /* UNIX */
    delete_thread(child, stack);
#    endif
    LOG(logfile, LOG_SIDELINE, 1, "Sideline thread destroyed\n");

    LOG(logfile, LOG_SIDELINE | LOG_STATS, 1, "Sideline samples taken: %d\n",
        num_samples);
#    ifdef DEBUG
    LOG(logfile, LOG_SIDELINE | LOG_STATS, 1, "Sideline optimizations performed: %d\n",
        num_optimized);
    LOG(logfile, LOG_SIDELINE | LOG_STATS, 1,
        "Sideline optimizations performed w/o synch: %d\n", num_opt_with_no_synch);
#    endif

    for (i = 0; i < table.capacity; i++) {
        while (table.table[i]) {
            sample = table.table[i];
            table.table[i] = sample->next;
            global_heap_free(sample, sizeof(sample_entry_t) HEAPACCT(ACCT_SIDELINE));
        }
    }
    global_heap_free(table.table,
                     table.capacity * sizeof(sample_entry_t *) HEAPACCT(ACCT_SIDELINE));

    l = remember;
    while (l != NULL) {
        nextl = l->next;
        e = l->list;
        while (e != NULL) {
            nexte = e->next;
            global_heap_free(e, sizeof(remember_entry_t) HEAPACCT(ACCT_SIDELINE));
            e = nexte;
        }
        global_heap_free(l, sizeof(remember_list_t) HEAPACCT(ACCT_SIDELINE));
        l = nextl;
    }

    destroy_event(wake_event);
    destroy_event(asleep_event);
    destroy_event(exited_event);

    destroy_event(paused_for_sideline_event);
    destroy_event(resume_from_sideline_event);

    close_log_file(logfile);

    DELETE_LOCK(sideline_lock);
    DELETE_LOCK(do_not_delete_lock);
    DELETE_LOCK(remember_lock);
    DELETE_LOCK(table.lock);
}

/* add profiling to top of trace
 * at top of trace store fragment_t * in global slot
 * (clear it at top of shared_syscall & fcache_return)
 * sideline thread samples that slot to find hot traces
 */
void
add_sideline_prefix(dcontext_t *dcontext, instrlist_t *trace)
{
    instr_t *inst = instr_build(dcontext, OP_mov_st, 1, 1);
    instr_set_src(inst, 0, opnd_create_immed_int(0x12345678, OPSZ_4));
    instr_set_dst(
        inst, 0,
        opnd_create_base_disp(REG_NULL, REG_NULL, 0, (int)&sideline_trace, OPSZ_4));
    instrlist_prepend(trace, inst);
}

void
finalize_sideline_prefix(dcontext_t *dcontext, fragment_t *trace_f)
{
    byte *start_pc = (byte *)FCACHE_ENTRY_PC(trace_f);
    byte *pc;

    /* ASSUMPTION: sideline prefix is at top of fragment! */

    /* fill in address of owning fragment now that that fragment exists */
    pc = start_pc + 6; /* 2 bytes of opcode, 4 bytes of store address */
    *((int *)pc) = (int)trace_f;
}

static void
remove_sideline_profiling(dcontext_t *dcontext, instrlist_t *trace)
{
    /* remove sideline profiling instruction */
    instr_t *instr;
    instr = instrlist_first(trace);
    ASSERT(instr_get_opcode(instr) == OP_mov_st &&
           opnd_is_near_base_disp(instr_get_dst(instr, 0)) &&
           opnd_get_disp(instr_get_dst(instr, 0)) == (int)&sideline_trace);
    instrlist_remove(trace, instr);
    instr_destroy(dcontext, instr);
}

void
sideline_start()
{
    if (child_sleep) {
        LOG(logfile, LOG_SIDELINE, VERB_3, "sideline: in sideline_start()\n");
        child_sleep = false;
        signal_event(wake_event);
    }
}

void
sideline_stop()
{
    if (!child_sleep) {
        LOG(logfile, LOG_SIDELINE, VERB_3, "SIDELINE: in sideline_stop()\n");
        child_sleep = true;
        /* signal paused_for_sideline_event to prevent wait-forever */
        signal_event(paused_for_sideline_event);
        wait_for_event(asleep_event, 0);
    }
}

/* Procedure executed by sideline thread
 */
static int RUN_SIG
sideline_run(void *arg)
{
    logfile = open_log_file("sideline", NULL, 0);
    LOG(logfile, LOG_SIDELINE, VERB_3, "SIDELINE: in sideline_run()\n");

#    ifdef UNIX
    /* priority can only be set by thread itself, so do it here */
    nice(10);
#    endif

    while (!child_exit) {
        if (child_sleep) {
            LOG(logfile, LOG_SIDELINE, VERB_3,
                "SIDELINE: sideline thread going to sleep\n");
            signal_event(asleep_event);
            wait_for_event(wake_event, 0);
            continue;
        }

        /* take a sample, find a hot trace to optimize */
        sideline_sample();

        /* let other threads run */
        os_thread_yield();
    }
    signal_event(exited_event);

    LOG(logfile, LOG_SIDELINE, VERB_3,
        "SIDELINE: sideline thread exiting sideline_run()\n");
#    ifdef WINDOWS
    /* with current create_thread implementation can't return from the run
     * function */
    os_terminate(NULL, TERMINATE_THREAD);
    ASSERT_NOT_REACHED();
#    endif
    return 0;
}

/* optimize_trace takes a tag, this routine pulls tag from frag */
static void
optimize_trace_wrapper(dcontext_t *dcontext, fragment_t *frag, instrlist_t *trace)
{
    optimize_trace(dcontext, frag->tag, trace);
}

static void
sideline_sample()
{
    sample_entry_t *e;
    fragment_t *sample = (fragment_t *)sideline_trace; /* the sample! */

    /* WARNING: we're using fragment_t* to make it easier to find target thread
     * and its trace at once, but the trace could have been deleted, so be
     * careful what you do with the fragment_t* -- here we never dereference it
     */

    /* first increment counter on current sample */
    if (sample == NULL) {
        LOG(logfile, LOG_SIDELINE, VERB_3, "\tSIDELINE: sample slot empty\n");
    } else {
        e = update_sample_entry((ptr_uint_t)sample);
        LOG(logfile, LOG_SIDELINE, VERB_3,
            "\tSIDELINE: sample now is " PFX " == F%d with count %d\n", sample,
            ((fragment_t *)sample)->id, e->counter);
    }

    /* we would clear the entry -- but a write to the shared memory is a big
     * performance hit on SMP (not on SMT though...FIXME: distinguish?)
     * we'll just accept an inability to distinguish a loop from
     * a blocked thread.
     */

    num_samples++;
    if (num_samples % SAMPLE_TO_OPTIMIZE_RATIO == 0) {
        /* clear entry to prevent stale samples after optimizing */
        sideline_trace = NULL;
        sideline_examine_traces();
    }
}

static void
sideline_examine_traces()
{
    sample_entry_t *e;
    fragment_t *f;

    /* WARNING: we're using fragment_t* to make it easier to find target thread
     * and its trace at once, but the trace could have been deleted, so be
     * careful what you do with the fragment_t* -- we use this do_not_delete_lock
     * to make sure no fragments are deleted until our last dereference of
     * the hot fragment
     */
    d_r_mutex_lock(&do_not_delete_lock);

    /* now find hottest trace */
    e = find_hottest_entry();
    if (e == NULL || e->counter <= SAMPLE_COUNT_THRESHOLD) {
        LOG(logfile, LOG_SIDELINE, VERB_3,
            "\tSIDELINE: cannot find a hot entry w/ count > %d\n",
            SAMPLE_COUNT_THRESHOLD);
        d_r_mutex_unlock(&do_not_delete_lock);
        return;
    }

    f = (fragment_t *)e->tag;
    LOG(logfile, LOG_SIDELINE, VERB_3,
        "\tSIDELINE: hottest entry is " PFX " == F%d with count %d\n", f, f->id,
        e->counter);
    /* don't need entry anymore, no matter what happens */
    remove_sample_entry(e->tag);

    /* a trace we optimized could still run old code that posts samples */
    if ((f->flags & FRAG_DO_NOT_SIDELINE) != 0) {
        LOG(logfile, LOG_SIDELINE, VERB_3,
            "\tSIDELINE: hottest entry F%d already sidelined\n", f->id);
        /* don't loop here looking for another entry -- let run() loop */
        d_r_mutex_unlock(&do_not_delete_lock);
        return;
    } else {
        LOG(logfile, LOG_SIDELINE, VERB_2,
            "\tSIDELINE: optimizing hottest entry == F%d with count %d\n", f->id,
            e->counter);
        f = sideline_optimize(f, remove_sideline_profiling, optimize_trace_wrapper);
#    ifdef DEBUG
        if (f != NULL)
            LOG(logfile, LOG_SIDELINE, VERB_2,
                "\t  SIDELINE: optimized fragment is F%d\n", f->id);
#    endif
    }

    d_r_mutex_unlock(&do_not_delete_lock);
}

fragment_t *
sideline_optimize(fragment_t *f,
                  void (*remove_profiling_func)(dcontext_t *, instrlist_t *),
                  void (*optimize_function)(dcontext_t *, fragment_t *, instrlist_t *))
{
    fragment_t *new_f = NULL;
    dcontext_t *dcontext;
    instrlist_t *ilist;
    uint flags;
    void *vmlist = NULL;
    DEBUG_DECLARE(bool ok;)

    LOG(logfile, LOG_SIDELINE, VERB_3, "\nsideline_optimize: F%d\n", f->id);
    ASSERT((f->flags & FRAG_IS_TRACE) != 0);

    LOG(logfile, LOG_SIDELINE, 1, "\nsideline_optimize:  tag= " PFX, f->tag);

    dcontext = get_dcontext_for_fragment(f);
    /* HACK to work with routines like unlink_branch that don't
     * get a dcontext passed in and instead call get_thread_private_dcontext()
     */
    set_thread_private_dcontext(dcontext);

    /* To avoid synch problems we do not let target thread (the one that owns
     * the trace f) execute dynamo code while we are in this routine
     */
    pause_for_sideline = dcontext->owning_thread;
    ASSERT(is_thread_known(pause_for_sideline));

    if (dcontext->whereami != DR_WHERE_FCACHE) {
        /* wait for thread to reach waiting point in d_r_dispatch */
        LOG(logfile, LOG_SIDELINE, VERB_3,
            "\nsideline_optimize: waiting for target thread " TIDFMT "\n",
            pause_for_sideline);
        if (!dynamo_exited && !child_sleep && !child_exit) {
            /* must give up do_not_delete_lock in case app thread is going to
             * call fragment_delete, but must know if f ends up being deleted,
             * so store it in fragment_now_optimizing
             * ASSUMPTION: will not examine sample hashtable again, so ok to
             * delete other fragments than f
             */
            fragment_now_optimizing = f;
            d_r_mutex_unlock(&do_not_delete_lock);
            wait_for_event(paused_for_sideline_event, 0);
            d_r_mutex_lock(&do_not_delete_lock);
            /* if f was deleted, exit now */
            if (fragment_now_optimizing == NULL)
                goto exit_sideline_optimize;
        }
    }
    if (dynamo_exited || child_sleep || child_exit) {
        /* a different thread than the one we asked to pause could be exiting
         * (hence the child_sleep command), so we better wake up the paused
         * thread
         */
        goto exit_sideline_optimize;
    }

    /* build IR */
    ilist = decode_fragment(dcontext, f, NULL, NULL, f->flags, NULL, NULL);

#    ifdef DEBUG
    ASSERT(instr_get_opcode(instrlist_last(ilist)) == OP_jmp);
    LOG(logfile, LOG_SIDELINE, VERB_3, "\nbefore removing profiling:\n");
    if (d_r_stats->loglevel >= VERB_3 && (d_r_stats->logmask & LOG_SIDELINE) != 0)
        instrlist_disassemble(dcontext, f->tag, ilist, THREAD);
#    endif
    remove_profiling_func(dcontext, ilist);

#    ifdef DEBUG
    LOG(logfile, LOG_SIDELINE, VERB_3, "\nafter removing profiling:\n");
    if (d_r_stats->loglevel >= VERB_3 && (d_r_stats->logmask & LOG_SIDELINE) != 0)
        instrlist_disassemble(dcontext, f->tag, ilist, THREAD);
#    endif

        /* FIXME: separate always-do-online optimizations from
         * sideline optimizations
         * for now, we do all online or all sideline
         * FIXME: our ilist is already fully decoded, so we could avoid the
         * full decode pass in optimize_trace
         */
#    ifdef DEBUG
    LOG(logfile, LOG_SIDELINE, OPTVERB_3, "\nbefore optimization:\n");
    if (d_r_stats->loglevel >= OPTVERB_3 && (d_r_stats->logmask & LOG_SIDELINE) != 0)
        instrlist_disassemble(dcontext, f->tag, ilist, THREAD);
#    endif
    optimize_function(dcontext, f, ilist);
#    ifdef DEBUG
    LOG(logfile, LOG_SIDELINE, OPTVERB_3, "\nafter optimization:\n");
    if (d_r_stats->loglevel >= OPTVERB_3 && (d_r_stats->logmask & LOG_SIDELINE) != 0)
        instrlist_disassemble(dcontext, f->tag, ilist, THREAD);
#    endif
    /* Note that the offline optimization interface cannot be used
     * here because it requires inserting the optimized code prior
     * to creating a fragment.  Here we must replace the entire
     * old fragment.
     */

    /*
   replacement can be done w/o synch: opt thread moves all links from
   old trace to new (single writes -> atomic) -- don't unlink and relink,
   instead change_link to ensure even prof_counts is atomic.
   moves link in hashtable to point to new trace, but leaves old trace's
   next hashtable pointer intact.  only when app thread is in dynamo
   code does it fully remove old trace from hashtable.
    */

    /* Emit fragment but don't make visible yet */
    /* mark both old and new as DO_NOT_SIDELINE */
    /* FIXME: if f is shared we must hold change_linking_lock here */
    ASSERT(!TEST(FRAG_SHARED, f->flags));
    f->flags |= FRAG_DO_NOT_SIDELINE;
    flags = f->flags;
    DEBUG_DECLARE(ok =)
    vm_area_add_to_list(dcontext, f->tag, &vmlist, flags, f, false /*no locks*/);
    ASSERT(ok); /* should never fail for private fragments */
    new_f = emit_invisible_fragment(dcontext, f->tag, ilist, flags, vmlist);
    fragment_copy_data_fields(dcontext, f, new_f);

    LOG(logfile, LOG_SIDELINE, VERB_3, "emitted invisible fragment F%d\n", new_f->id);

    shift_links_to_new_fragment(dcontext, f, new_f);

    /* use fragment_replace to insert new_f into table while leaving f's
     * next field intact in case hashtable lookup routine is examining ftable
     */
    fragment_replace(dcontext, f, new_f);

    /* remember old fragment so can delete it later */
    add_remember_entry(dcontext, f);

    /* clean up */
    instrlist_clear_and_destroy(dcontext, ilist);

#    ifdef DEBUG
    num_optimized++;
    if (d_r_stats->loglevel >= 2 && (d_r_stats->logmask & LOG_SIDELINE) != 0) {
        disassemble_fragment(dcontext, new_f, d_r_stats->loglevel < 3);
        LOG(logfile, LOG_SIDELINE, 2,
            "\tSIDELINE: emitted optimized F%d to replace F%d\n", new_f->id, f->id);
    }
#    endif /* DEBUG */

exit_sideline_optimize:
    pause_for_sideline = (thread_id_t)0;
    if (!d_r_mutex_trylock(&sideline_lock)) {
        /* thread is waiting in d_r_dispatch */
        signal_event(resume_from_sideline_event);
        d_r_mutex_lock(&sideline_lock);
        /* at this point we know thread has read our resume event
         * clear all state
         */
        reset_event(paused_for_sideline_event);
        reset_event(resume_from_sideline_event);
    }
#    ifdef DEBUG
    else
        num_opt_with_no_synch++;
#    endif
    d_r_mutex_unlock(&sideline_lock);
    /* see HACK above */
    set_thread_private_dcontext(NULL);

    return new_f;
}

/* called by target thread when it is at safe point so a replaced trace can
 * be completely removed
 */
void
sideline_cleanup_replacement(dcontext_t *dcontext)
{
    remember_list_t *l, *prev_l;
    remember_entry_t *e, *next_e;

    /* clear sample entry, it could still contain a pointer to a fragment
     * we're about to delete
     */
    sideline_trace = NULL;

    d_r_mutex_lock(&remember_lock);
    for (l = remember, prev_l = NULL; l != NULL; prev_l = l, l = l->next) {
        if (l->dcontext == dcontext) {
            e = l->list;
            while (e != NULL) {
                next_e = e->next;

                /* clean up fragment
                 * deliberately do not call incoming_remove_fragment
                 */
                LOG(logfile, LOG_SIDELINE, VERB_3, "sideline_cleanup: cleaning up F%d\n",
                    e->f->id);

                fragment_delete(dcontext, e->f,
                                FRAGDEL_NO_OUTPUT | FRAGDEL_NO_UNLINK |
                                    FRAGDEL_NO_HTABLE);
                STATS_INC(num_fragments_deleted_sideline);

                global_heap_free(e, sizeof(remember_entry_t) HEAPACCT(ACCT_SIDELINE));
                e = next_e;
            }
            if (prev_l != NULL)
                prev_l->next = l->next;
            else
                remember = l->next;
            global_heap_free(l, sizeof(remember_list_t) HEAPACCT(ACCT_SIDELINE));
            d_r_mutex_unlock(&remember_lock);
            return;
        }
    }
    d_r_mutex_unlock(&remember_lock);
}

static sample_entry_t *
find_hottest_entry()
{
    uint i;
    sample_entry_t *e, *max = NULL;
    int max_counter = 0;

    d_r_mutex_lock(&table.lock);
    for (i = 0; i < table.capacity; i++) {
        for (e = table.table[i]; e; e = e->next) {
            if (e->counter > max_counter) {
                max_counter = e->counter;
                max = e;
            }
        }
    }
    d_r_mutex_unlock(&table.lock);
    return max;
}

static sample_entry_t *
update_sample_entry(ptr_uint_t tag)
{
    uint hindex;
    sample_entry_t *e;
    bool found = false;

    /* look up entry for id */
    d_r_mutex_lock(&table.lock);
    hindex = HASH_FUNC((ptr_uint_t)tag, &table);
    for (e = table.table[hindex]; e; e = e->next) {
        if (e->tag == tag) {
            found = true;
            break;
        }
    }

    if (found) {
        e->counter++;
    } else {
        /* make new entry */
        e = (sample_entry_t *)global_heap_alloc(sizeof(sample_entry_t)
                                                    HEAPACCT(ACCT_SIDELINE));
        e->tag = tag;
        e->counter = 1;
        e->next = table.table[hindex];
        table.table[hindex] = e;
    }

    d_r_mutex_unlock(&table.lock);
    return e;
}

/* executed by an application thread when it deletes fragment f */
void
sideline_fragment_delete(fragment_t *f)
{
    if ((f->flags & FRAG_IS_TRACE) != 0) {
        /* see notes above on reasons for this extra lock */
        d_r_mutex_lock(&do_not_delete_lock);
        /* clear sample entry, it could still contain a pointer to f */
        sideline_trace = NULL;
        remove_sample_entry((ptr_uint_t)f);
        /* let sideline_optimize know if we delete its fragment while it's waiting */
        if (fragment_now_optimizing == f)
            fragment_now_optimizing = NULL;

        d_r_mutex_unlock(&do_not_delete_lock);
    }
}

static void
remove_sample_entry(ptr_uint_t tag)
{
    uint hindex;
    sample_entry_t *e, *preve;

    d_r_mutex_lock(&table.lock);
    hindex = HASH_FUNC((ptr_uint_t)tag, &table);
    for (e = table.table[hindex], preve = NULL; e != NULL; preve = e, e = e->next) {
        if (e->tag == tag) {
            if (preve)
                preve->next = e->next;
            else
                table.table[hindex] = e->next;
            global_heap_free(e, sizeof(sample_entry_t) HEAPACCT(ACCT_SIDELINE));
            break;
        }
    }
    d_r_mutex_unlock(&table.lock);
}

static void
add_remember_entry(dcontext_t *dcontext, fragment_t *f)
{
    remember_list_t *l;
    remember_entry_t *e;
    d_r_mutex_lock(&remember_lock);
    l = remember;
    while (l != NULL) {
        if (l->dcontext == dcontext) {
            /* make new entry */
            e = (remember_entry_t *)global_heap_alloc(sizeof(remember_entry_t)
                                                          HEAPACCT(ACCT_SIDELINE));
            e->f = f;
            e->next = l->list;
            l->list = e;
            d_r_mutex_unlock(&remember_lock);
            return;
        }
        l = l->next;
    }
    /* make new list */
    l = (remember_list_t *)global_heap_alloc(sizeof(remember_list_t)
                                                 HEAPACCT(ACCT_SIDELINE));
    l->dcontext = dcontext;
    l->list = (remember_entry_t *)global_heap_alloc(sizeof(remember_entry_t)
                                                        HEAPACCT(ACCT_SIDELINE));
    l->list->f = f;
    l->list->next = NULL;
    l->next = remember;
    remember = l;
    d_r_mutex_unlock(&remember_lock);
}

#    ifdef UNIX /***************************************************************/
/* Create a new thread. It should be passed "fcn", a function which
 * takes two arguments, (the second one is a dummy, always 4). The
 * first argument is passed in "arg". Returns the PID of the new
 * thread
 * QUESTION: why does bbthreads use asm syscall instead of libc wrapper?
 * WARNING: clone is not present in libc5 or earlier
 */
static thread_t
create_thread(int (*fcn)(void *), void *arg, void **stack)
{
    thread_t thread;

    int flags;
    void *my_stack;
    my_stack = stack_alloc(THREAD_STACK_SIZE, NULL);
    /* need SIGCHLD so parent will get that signal when child dies,
     * else have "no children" errors doing a wait
     */
    /* We're not using CLONE_THREAD.  If we do want it to be in the same thread
     * group as the parent we may want a runtime check for Linux 2.4 kernel where
     * CLONE_THREAD was added.
     */
    flags = SIGCHLD | CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND;
    thread = clone(fcn, my_stack, flags, arg); /* old: __clone */

    if (thread == -1) {
        SYSLOG_INTERNAL_ERROR("SIDELINE: Error calling __clone");
        stack_free(my_stack, THREAD_STACK_SIZE);
        ASSERT_NOT_REACHED();
    }

    *stack = my_stack;

    return thread;
}

static void
delete_thread(thread_t thread, void *stack)
{
    pid_t result;
    result = waitpid(thread, NULL, 0);
    stack_free(stack, THREAD_STACK_SIZE);
    if (result == -1) {
        perror("delete_thread");
        SYSLOG_INTERNAL_ERROR("SIDELINE: Error deleting thread");
        ASSERT_NOT_REACHED();
    }
}
#    endif /* UNIX ***************************************************************/

#endif /* SIDELINE -- around whole file */
