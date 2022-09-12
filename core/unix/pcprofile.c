/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2001-2010 VMware, Inc.  All rights reserved.
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
/* Copyright (c) 2001 Hewlett-Packard Company */

/*
 * pcprofile.c - pc sampling profiler for dynamo
 */

#include "../globals.h"
#include "../utils.h"
#include "../fragment.h"
#include "../fcache.h"
#include "instrument.h"
#include "disassemble.h"
#include <sys/time.h> /* ITIMER_VIRTUAL */

/* Don't use symtab, it doesn't give us anything that addr2line or
 * other post-execution tools can't (it doesn't see into shared libraries),
 * plus will want a script to process the raw data we output anyway!
 * If you put symtab back in, add dynamo option "-profexecname"
 */
#define USE_SYMTAB 0
#if USE_SYMTAB
#    include "symtab.h"
static bool valid_symtab;
#endif

/* Profiling via pc sampling
 * We store the pc for dynamo and app pc's, for fragments we store the
 * tag and offset
 * In order to handle tags that aren't original program pc's we can't
 * use tag+offset as the hashtable key -- so we use fcache pc as the
 * key.  To handle deleted fragments and later fragments re-using the same
 * fcache pcs, we have a "retired" flag that we set when a fragment is
 * deleted.  Thus we can end up printing out a separate count for
 * the same pc.
 */
typedef struct _pc_profile_entry_t {
    void *pc;   /* the pc */
    app_pc tag; /* if in fragment, tag */
#ifdef DEBUG
    int id; /* if in fragment, id */
#endif
    ushort offset;                    /* if in fragment, offset from start pc */
    dr_where_am_i_t whereami : 8;     /* location of pc */
    bool trace : 1;                   /* if in fragment, is it a trace? */
    bool retired : 1;                 /* owning fragment was deleted */
    int counter;                      /* execution counter */
    struct _pc_profile_entry_t *next; /* for chaining entries */
} pc_profile_entry_t;

#define HASH_BITS 14

/* The timer and all data are per-thread */
typedef struct _thread_pc_info_t {
    bool thread_shared;
    pc_profile_entry_t **htable; /* HASH_BITS-bit addressed hash table, key is pc */
    void *special_heap;
    file_t file;
    int where[DR_WHERE_LAST];
} thread_pc_info_t;

#define ALARM_FREQUENCY 10 /* milliseconds */

/* forward declarations for static functions */
static pc_profile_entry_t *
pcprofile_add_entry(thread_pc_info_t *info, void *pc, int whereami);
static pc_profile_entry_t *
pcprofile_lookup(thread_pc_info_t *info, void *pc);
static void
pcprofile_reset(thread_pc_info_t *info);
static void
pcprofile_results(thread_pc_info_t *info);
static void
pcprofile_alarm(dcontext_t *dcontext, priv_mcontext_t *mcontext);

/* initialization */
void
pcprofile_thread_init(dcontext_t *dcontext, bool shared_itimer, void *parent_info)
{
    int i;
    int size = HASHTABLE_SIZE(HASH_BITS) * sizeof(pc_profile_entry_t *);
    thread_pc_info_t *info;
    size_t special_heap_size = INTERNAL_OPTION(prof_pcs_heap_size);

    if (shared_itimer) {
        /* Linux kernel 2.6.12+ shares itimers across all threads.  We thus
         * share the same data and assume we don't need any synch on these
         * data structs or the file since only one timer fires at a time
         * and we block subsequent while in the handler.
         */
        ASSERT(parent_info != NULL);
        info = (thread_pc_info_t *)parent_info;
        dcontext->pcprofile_field = parent_info;
        info->thread_shared = true;
        return;
    }

    /* we use global heap so we can share with child threads */
    info = global_heap_alloc(sizeof(thread_pc_info_t) HEAPACCT(ACCT_OTHER));
    dcontext->pcprofile_field = (void *)info;
    memset(info, 0, sizeof(thread_pc_info_t));
    info->thread_shared = shared_itimer;

    info->htable = (pc_profile_entry_t **)global_heap_alloc(size HEAPACCT(ACCT_OTHER));
    memset(info->htable, 0, size);
#if USE_SYMTAB
    valid_symtab = symtab_init();
#endif
    for (i = 0; i < DR_WHERE_LAST; i++)
        info->where[i] = 0;
    info->file = open_log_file("pcsamples", NULL, 0);
    /* FIXME PR 596808: we can easily fill up the initial special heap unit,
     * and creating a new one acquires global locks and can deadlock:
     * we should allocate many units up front or something
     */
    info->special_heap = special_heap_pclookup_init(
        sizeof(pc_profile_entry_t), false /*no locks*/, false /*-x*/, true /*persistent*/,
        NULL /*vector*/, NULL /*vector data*/, NULL /*heap region*/, special_heap_size,
        false /*not full*/);

    set_itimer_callback(dcontext, ITIMER_VIRTUAL, ALARM_FREQUENCY, pcprofile_alarm, NULL);
}

/* cleanup: only called for thread-shared itimer for last thread in group */
void
pcprofile_thread_exit(dcontext_t *dcontext)
{
    thread_pc_info_t *info = (thread_pc_info_t *)dcontext->pcprofile_field;
    /* don't want any alarms while holding lock for printing results
     * (see notes under pcprofile_cache_flush below)
     */
    set_itimer_callback(dcontext, ITIMER_VIRTUAL, 0, NULL, NULL);

    pcprofile_results(info);
    DEBUG_DECLARE(int size = HASHTABLE_SIZE(HASH_BITS) * sizeof(pc_profile_entry_t *);)
    pcprofile_reset(info); /* special heap so no fast path */
#ifdef DEBUG
    /* for non-debug we do fast exit path and don't free local heap */
    global_heap_free(info->htable, size HEAPACCT(ACCT_OTHER));
#endif
#if USE_SYMTAB
    if (valid_symtab)
        symtab_exit();
#endif
    close_log_file(info->file);
    special_heap_exit(info->special_heap);
#ifdef DEBUG
    /* for non-debug we do fast exit path and don't free local heap */
    global_heap_free(info, sizeof(thread_pc_info_t) HEAPACCT(ACCT_OTHER));
#endif
}

void
pcprofile_fork_init(dcontext_t *dcontext)
{
    /* itimers are not inherited across fork */
    /* FIXME: hmmm...I guess a forked child will just start from scratch?
     */
    thread_pc_info_t *info = (thread_pc_info_t *)dcontext->pcprofile_field;
    info->thread_shared = false;
    pcprofile_reset(info);
    info->file = open_log_file("pcsamples", NULL, 0);
    set_itimer_callback(dcontext, ITIMER_VIRTUAL, ALARM_FREQUENCY, pcprofile_alarm, NULL);
}

#if 0
static void
pcprof_dump_callstack(dcontext_t *dcontext, app_pc cur_pc, app_pc ebp, file_t outfile)
{
    uint *pc = (uint *) ebp;
    int num = 0;
    if (cur_pc != NULL) {
        print_file(outfile, "\tcurrent pc = "PFX"\n", cur_pc);
    }
    /* only DR routines
     * cannot call is_readable b/c that asks for memory -> livelock
     */
    while (pc != NULL && ((byte*)pc >= dcontext->dstack - DYNAMORIO_STACK_SIZE) &&
           (byte*)pc < (app_pc)dcontext->dstack) {
        print_file(outfile,
            "\tframe ptr "PFX" => parent "PFX", ret = "PFX"\n", pc, *pc, *(pc+1));
        num++;
        /* yes I've seen weird recursive cases before */
        if (pc == (uint *) *pc || num > 100)
            break;
        pc = (uint *) *pc;
    }
}
#endif

/* Handle a pc sample
 *
 * WARNING: this function could interrupt any part of dynamo!
 * Make sure nothing is done that could cause deadlock or
 * data structure mishaps.
 * Right now interrupting heap_alloc or interrupting pcprofile_results are
 * the only bad things that could happen, both are dealt with.
 */
static void
pcprofile_alarm(dcontext_t *dcontext, priv_mcontext_t *mcontext)
{
    thread_pc_info_t *info = (thread_pc_info_t *)dcontext->pcprofile_field;
    pc_profile_entry_t *entry;
    void *pc = (void *)mcontext->pc;

    entry = pcprofile_lookup(info, pc);

#if 0
#    ifdef DEBUG
    print_file(info->file, "\nalarm "PFX"\n", pc);
#    endif
    pcprof_dump_callstack(dcontext, pc, (app_pc) mcontext->xbp, info->file);
#endif

    if (entry != NULL) {
        entry->counter++;
    } else {
        /* for thread-shared itimers we block this signal in the handler
         * so we assume we won't have any data races.
         * special_heap routines do not use any locks.
         */
        entry = pcprofile_add_entry(info, pc, dcontext->whereami);
        /* if in a fragment, get fragment tag & offset now */
        if (entry->whereami == DR_WHERE_FCACHE) {
            fragment_t *fragment;
            entry->whereami =
                fcache_refine_whereami(dcontext, entry->whereami, pc, &fragment);
            if (fragment != NULL) {
#ifdef DEBUG
                entry->id = fragment->id;
#endif
                entry->tag = fragment->tag;
                ASSERT(CHECK_TRUNCATE_TYPE_int((byte *)pc - fragment->start_pc));
                entry->offset = (int)((byte *)pc - fragment->start_pc);
                entry->trace = (fragment->flags & FRAG_IS_TRACE) != 0;
            }
        }
    }

    /* update whereami counters */
    info->where[entry->whereami]++;
}

/* create a new, initialized profile pc entry */
static pc_profile_entry_t *
pcprofile_add_entry(thread_pc_info_t *info, void *pc, int whereami)
{
    uint hindex;
    pc_profile_entry_t *e;

    /* special_heap is hardcoded for sizeof(pc_profile_entry_t) */
    apicheck(special_heap_can_calloc(info->special_heap, 1),
             "Profile pc heap capacity exceeded. Use option -prof_pcs_heap_size "
             "to rerun with a larger profiling heap.");
    e = (pc_profile_entry_t *)special_heap_alloc(info->special_heap);
    e->pc = pc;
    e->counter = 1;
    e->whereami = whereami;
    e->next = NULL;
#ifdef DEBUG
    e->id = 0;
#endif
    e->tag = 0;
    e->offset = 0;
    e->trace = false;
    e->retired = false;

    /* add e to the htable */
    hindex = HASH_FUNC_BITS((ptr_uint_t)pc, HASH_BITS);
    e->next = info->htable[hindex];
    info->htable[hindex] = e;

    return e;
}

/* Lookup an entry by pc and return a pointer to the corresponding entry
 * Returns NULL if no such entry exists */
static pc_profile_entry_t *
pcprofile_lookup(thread_pc_info_t *info, void *pc)
{
    uint hindex;
    pc_profile_entry_t *e;
    /* find the entry corresponding to tag */
    hindex = HASH_FUNC_BITS((ptr_uint_t)pc, HASH_BITS);
    for (e = info->htable[hindex]; e; e = e->next) {
        if (e->pc == pc && !e->retired)
            return e;
    }
    return NULL;
}

/* When a fragment is deleted we "retire" all its entries
 * Thus we end up with multiple entires with the same pc
 */
void
pcprofile_fragment_deleted(dcontext_t *dcontext, fragment_t *f)
{
    thread_pc_info_t *info;
    int i;
    pc_profile_entry_t *e;
    if (dcontext == GLOBAL_DCONTEXT) {
        dcontext = get_thread_private_dcontext();
    }
    if (dcontext == NULL) {
        ASSERT(dynamo_exited);
        return;
    }
    info = (thread_pc_info_t *)dcontext->pcprofile_field;
    for (i = 0; i < HASHTABLE_SIZE(HASH_BITS); i++) {
        for (e = info->htable[i]; e; e = e->next) {
            if (e->tag == f->tag) {
                e->retired = true;
            }
        }
    }
}

#if 0 /* not used */
/* delete the trace head entry corresponding to tag if it exists */
static void
pcprofile_destroy_entry(thread_pc_info_t *info, void *pc)
{
    uint hindex;
    pc_profile_entry_t *e, *prev_e = NULL;

    /* find the trace head entry corresponding to tag and delete it */
    hindex = HASH_FUNC_BITS((ptr_uint_t)pc, HASH_BITS);
    for (e = info->htable[hindex]; e; prev_e = e, e = e->next) {
        if (e->pc == pc) {
            if (prev_e)
                prev_e->next = e->next;
            else
                info->htable[hindex] = e->next;
            break;
        }
    }
    if (e)
        special_heap_free(info->special_heap, e);
}
#endif

/* reset profile structures */
static void
pcprofile_reset(thread_pc_info_t *info)
{
    int i;
    for (i = 0; i < HASHTABLE_SIZE(HASH_BITS); i++) {
        pc_profile_entry_t *e = info->htable[i];
        while (e) {
            pc_profile_entry_t *nexte = e->next;
            special_heap_free(info->special_heap, e);
            e = nexte;
        }
        info->htable[i] = NULL;
    }
    for (i = 0; i < DR_WHERE_LAST; i++)
        info->where[i] = 0;
}

/* Print the profile results
 * FIXME: It would be nice to print counts integrated with fragment listings
 * That would require re-ordering *_exit() sequence (fragments are deleted first)
 * Instead of doing that, can use a script to combine these tag+offsets with
 * previously printed fragments
 * FIXME: this routine uses floating point ops, if ever called not at thread exit
 * must preserve fp state around the whole routine!
 */
static void
pcprofile_results(thread_pc_info_t *info)
{
    int i, total = 0;
    pc_profile_entry_t *e;

    for (i = 0; i < DR_WHERE_LAST; i++)
        total += info->where[i];

    print_file(info->file, "DynamoRIO library: " PFX "-" PFX "\n",
               get_dynamorio_dll_start(), get_dynamorio_dll_end());
    {
        app_pc client_start, client_end;
        if (get_client_bounds(0, &client_start, &client_end)) {
            print_file(info->file, "client library: " PFX "-" PFX "\n", client_start,
                       client_end);
        }
    }
    print_file(info->file, "ITIMER distribution (%d):\n", total);
    if (info->where[DR_WHERE_APP] > 0) {
        print_file(info->file, "  %5.1f%% of time in APPLICATION (%d)\n",
                   (float)info->where[DR_WHERE_APP] / (float)total * 100.0,
                   info->where[DR_WHERE_APP]);
    }
    if (info->where[DR_WHERE_INTERP] > 0) {
        print_file(info->file, "  %5.1f%% of time in INTERPRETER (%d)\n",
                   (float)info->where[DR_WHERE_INTERP] / (float)total * 100.0,
                   info->where[DR_WHERE_INTERP]);
    }
    if (info->where[DR_WHERE_DISPATCH] > 0) {
        print_file(info->file, "  %5.1f%% of time in DISPATCH (%d)\n",
                   (float)info->where[DR_WHERE_DISPATCH] / (float)total * 100.0,
                   info->where[DR_WHERE_DISPATCH]);
    }
    if (info->where[DR_WHERE_MONITOR] > 0) {
        print_file(info->file, "  %5.1f%% of time in MONITOR (%d)\n",
                   (float)info->where[DR_WHERE_MONITOR] / (float)total * 100.0,
                   info->where[DR_WHERE_MONITOR]);
    }
    if (info->where[DR_WHERE_SYSCALL_HANDLER] > 0) {
        print_file(info->file, "  %5.1f%% of time in SYSCALL HANDLER (%d)\n",
                   (float)info->where[DR_WHERE_SYSCALL_HANDLER] / (float)total * 100.0,
                   info->where[DR_WHERE_SYSCALL_HANDLER]);
    }
    if (info->where[DR_WHERE_SIGNAL_HANDLER] > 0) {
        print_file(info->file, "  %5.1f%% of time in SIGNAL HANDLER (%d)\n",
                   (float)info->where[DR_WHERE_SIGNAL_HANDLER] / (float)total * 100.0,
                   info->where[DR_WHERE_SIGNAL_HANDLER]);
    }
    if (info->where[DR_WHERE_TRAMPOLINE] > 0) {
        print_file(info->file, "  %5.1f%% of time in TRAMPOLINES (%d)\n",
                   (float)info->where[DR_WHERE_TRAMPOLINE] / (float)total * 100.0,
                   info->where[DR_WHERE_TRAMPOLINE]);
    }
    if (info->where[DR_WHERE_CONTEXT_SWITCH] > 0) {
        print_file(info->file, "  %5.1f%% of time in CONTEXT SWITCH (%d)\n",
                   (float)info->where[DR_WHERE_CONTEXT_SWITCH] / (float)total * 100.0,
                   info->where[DR_WHERE_CONTEXT_SWITCH]);
    }
    if (info->where[DR_WHERE_IBL] > 0) {
        print_file(info->file, "  %5.1f%% of time in INDIRECT BRANCH LOOKUP (%d)\n",
                   (float)info->where[DR_WHERE_IBL] / (float)total * 100.0,
                   info->where[DR_WHERE_IBL]);
    }
    if (info->where[DR_WHERE_FCACHE] > 0) {
        print_file(info->file, "  %5.1f%% of time in FRAGMENT CACHE (%d)\n",
                   (float)info->where[DR_WHERE_FCACHE] / (float)total * 100.0,
                   info->where[DR_WHERE_FCACHE]);
    }
    if (info->where[DR_WHERE_CLEAN_CALLEE] > 0) {
        print_file(info->file, "  %5.1f%% of time in CLEAN CALL (%d)\n",
                   (float)info->where[DR_WHERE_CLEAN_CALLEE] / (float)total * 100.0,
                   info->where[DR_WHERE_CLEAN_CALLEE]);
    }
    if (info->where[DR_WHERE_UNKNOWN] > 0) {
        print_file(info->file, "  %5.1f%% of time in UNKNOWN (%d)\n",
                   (float)info->where[DR_WHERE_UNKNOWN] / (float)total * 100.0,
                   info->where[DR_WHERE_UNKNOWN]);
    }

    print_file(info->file, "\nPC PROFILING RESULTS\n");

    for (i = 0; i < HASHTABLE_SIZE(HASH_BITS); i++) {
        e = info->htable[i];
        while (e) {
            if (e->whereami == DR_WHERE_FCACHE) {
                const char *type;
                if (e->trace)
                    type = "trace";
                else
                    type = "fragment";
                print_file(info->file,
#ifdef DEBUG
                           "pc=" PFX "\t#=%d\tin %s #%6d @" PFX " w/ offs " PFX "\n",
                           e->pc, e->counter, type, e->id, e->tag, e->offset);
#else
                           "pc=" PFX "\t#=%d\tin %s @" PFX " w/ offs " PFX "\n", e->pc,
                           e->counter, type, e->tag, e->offset);
#endif
#if USE_SYMTAB
                /* FIXME: this only works for fragments whose tags are app pc's! */
                if (valid_symtab) {
                    print_file(info->file, "\tin app = %s\n",
                               symtab_lookup_pc((void *)(e->tag + e->offset)));
                }
#endif
            } else if (e->whereami == DR_WHERE_APP) {
#if USE_SYMTAB
                if (valid_symtab) {
                    print_file(info->file, "pc=" PFX "\t#=%d\tin the app = %s\n", e->pc,
                               e->counter, symtab_lookup_pc(e->pc));
                } else {
#else
                print_file(info->file, "pc=" PFX "\t#=%d\tin the app\n", e->pc,
                           e->counter);
#endif
#if USE_SYMTAB
                }
#endif
            } else if (e->whereami == DR_WHERE_UNKNOWN) {
                if (is_dynamo_address(e->pc)) {
                    print_file(info->file,
                               "pc=" PFX "\t#=%d\tin DynamoRIO <SOMEWHERE> | ", e->pc,
                               e->counter);
                } else {
                    char *comment = NULL;
                    DODEBUG({ comment = get_address_comment(e->pc); });
                    print_file(info->file, "pc=" PFX "\t#=%d\tin uncategorized: %s | ",
                               e->pc, e->counter,
                               (comment == NULL) ? "<UNKNOWN>" : comment);
                }
                disassemble_with_info(GLOBAL_DCONTEXT, e->pc, info->file,
                                      false /*show pc*/, false /*show bytes*/);
            } else {
#if USE_SYMTAB
                if (valid_symtab) {
                    print_file(info->file, "pc=" PFX "\t#=%d\tin DynamoRIO = %s\n", e->pc,
                               e->counter, symtab_lookup_pc(e->pc));
                } else {
#else
                print_file(info->file, "pc=" PFX "\t#=%d\tin DynamoRIO", e->pc,
                           e->counter);
                if (e->whereami == DR_WHERE_INTERP) {
                    print_file(info->file, " interpreter\n");
                } else if (e->whereami == DR_WHERE_DISPATCH) {
                    print_file(info->file, " dispatch\n");
                } else if (e->whereami == DR_WHERE_MONITOR) {
                    print_file(info->file, " monitor\n");
                } else if (e->whereami == DR_WHERE_SYSCALL_HANDLER) {
                    print_file(info->file, " syscall handler\n");
                } else if (e->whereami == DR_WHERE_SIGNAL_HANDLER) {
                    print_file(info->file, " signal handler\n");
                } else if (e->whereami == DR_WHERE_TRAMPOLINE) {
                    print_file(info->file, " trampoline\n");
                } else if (e->whereami == DR_WHERE_CONTEXT_SWITCH) {
                    print_file(info->file, " context switch\n");
                } else if (e->whereami == DR_WHERE_IBL) {
                    print_file(info->file, " indirect_branch_lookup\n");
                } else if (e->whereami == DR_WHERE_CLEAN_CALLEE) {
                    print_file(info->file, " clean call\n");
                } else {
                    print_file(STDERR, "ERROR: unknown whereAmI %d\n", e->whereami);
                    ASSERT_NOT_REACHED();
                }
#endif
#if USE_SYMTAB
                }
#endif
            }
            e = e->next;
        }
    }
}
