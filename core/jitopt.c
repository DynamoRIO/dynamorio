/* ******************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
 * ******************************************************/

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

#include "globals.h"
#include "fragment.h"
#include "hashtable.h"
#include "instrument.h" // hackish...
#include "annotations.h"
#include "asmtable.h"
#include "x86/instr_create.h"
#include "jitopt.h"

#ifdef JITOPT /* around whole file */

#include <string.h>

//#define TRACE_ANALYSIS 1

#include <stdlib.h>
#ifdef LINUX
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
# include "include/syscall.h"            /* our own local copy */
#else
# include <sys/syscall.h>
#endif

#define DYNAMORIO_ANNOTATE_MANAGE_CODE_AREA_NAME \
    "dynamorio_annotate_manage_code_area"

#define DYNAMORIO_ANNOTATE_UNMANAGE_CODE_AREA_NAME \
    "dynamorio_annotate_unmanage_code_area"

#define DYNAMORIO_ANNOTATE_FLUSH_FRAGMENTS_NAME \
    "dynamorio_annotate_flush_fragments"

#ifdef JITOPT

#define BUCKET_BIT_SIZE DGC_OVERLAP_BUCKET_BIT_SIZE
#define BUCKET_MASK 0x3f
#define BUCKET_BBS 3
#define BUCKET_OFFSET_SENTINEL 1
#define BUCKET_ID(pc) (((ptr_uint_t) (pc)) >> BUCKET_BIT_SIZE)
#define DGC_REF_COUNT_BITS 0xa
#define DGC_REF_COUNT_MASK 0x3ff

#ifdef X64
# define HASH_STEP_SIZE 8
# define HASH_STEP_BITS 16
# define HASH "%llx"
#else
# define HASH_STEP_SIZE 4
# define HASH_STEP_BITS 8
# define HASH "%x"
#endif

#define SHIFT_IN_EMPTY_BYTES(data, bytes_to_keep) \
    (data << ((HASH_STEP_SIZE - bytes_to_keep)*HASH_STEP_BITS)) >> \
    ((HASH_STEP_SIZE - bytes_to_keep)*HASH_STEP_BITS)

typedef ptr_uint_t bb_hash_t;

typedef struct _dgc_trace_t {
    app_pc tags[2];
    struct _dgc_trace_t *next_trace;
} dgc_trace_t;

/* x86: 20 bytes | x64: 36 bytes */
typedef struct _dgc_bb_t dgc_bb_t;
struct _dgc_bb_t {
    app_pc start;
    int ref_count;
    union {
        ptr_uint_t span;
        dgc_bb_t *head;
    };
#ifdef DEBUG
    bb_hash_t hash; // debug
#endif
    dgc_bb_t *next;
    dgc_trace_t *containing_trace_list;
};

/* x86: 80 bytes (20 + bbs) | x64 162 bytes (54 + bbs) */
typedef struct _dgc_bucket_t {
    ptr_uint_t bucket_id;
    struct _dgc_bucket_t *hashtable_next;
    dgc_bb_t blocks[BUCKET_BBS];
    uint offset_sentinel;
    struct _dgc_bucket_t *head;
    struct _dgc_bucket_t *chain;
} dgc_bucket_t;

#define DGC_BUCKET_GC_CAPACITY 0x80
typedef struct _dgc_bucket_gc_list_t {
    dgc_bucket_t **staging;
    dgc_bucket_t **removals;
    uint staging_count;
    uint max_staging;
    uint removal_count;
    uint max_removals;
    bool allow_immediate_gc;
    const char *current_operation;
} dgc_bucket_gc_list_t;

static dgc_bucket_gc_list_t *dgc_bucket_gc_list;
static asmtable_t *dgc_table;

DECLARE_CXTSWPROT_VAR(static mutex_t dgc_table_lock, INIT_LOCK_FREE(dgc_table_lock));

typedef struct _dgc_thread_state_t {
    int count;
    uint version;
    thread_record_t **threads;
    bool scaled_trace_head_tables;
} dgc_thread_state_t;

static dgc_thread_state_t *thread_state;

typedef struct _dgc_fragment_intersection_t {
    app_pc *bb_tags;
    uint bb_tag_max;
    app_pc *trace_tags;
    uint trace_tag_max;
    fragment_t **fragments; /* maybe cache deletions in this array, but need dc per frag */
    fragment_t *shared_deletion_list;
} dgc_fragment_intersection_t;

static dgc_fragment_intersection_t *fragment_intersection;

#define IS_INCOMPATIBLE_OVERLAP(start1, end1, start2, end2) \
    ((start1) < (end2) && (end1) > (start2) && (end1) != (end2))

#ifdef X64
# define MMAP SYS_mmap
#else
# define MMAP SYS_mmap2
#endif

typedef struct _dgc_writer_mapping_table_t {
    dgc_writer_mapping_t *table[DGC_MAPPING_TABLE_SIZE];
} dgc_writer_mapping_table_t;

static dgc_writer_mapping_table_t *dgc_writer_mapping_table;

DECLARE_CXTSWPROT_VAR(static mutex_t dgc_mapping_lock, INIT_LOCK_FREE(dgc_mapping_lock));

typedef struct _double_mapping_t {
    app_pc app_memory_start;
    size_t size;
    app_pc double_mapping_start;
    size_t double_mapping_size;
    int fd;
} double_mapping_t;

typedef struct double_mapping_list_t {
    uint index;
    double_mapping_t *mappings;
} double_mapping_list_t;

#define MAX_DOUBLE_MAPPINGS 500
static double_mapping_list_t *double_mappings;
#endif

typedef struct _dgc_removal_queue_t {
    app_pc *tags;
    uint index;
    uint max;
    uint sample_index;
} dgc_removal_queue_t;

static dgc_removal_queue_t *dgc_removal_queue;

#define DGC_REPORT_ONE_STAT(stat) \
    RELEASE_LOG(GLOBAL, LOG_ANNOTATIONS, 1, " | %s: %d\n", \
                stats->stat##_pair.name, \
                stats->stat##_pair.value);

typedef struct _dgc_stats_t {
    uint timer;
} dgc_stats_t;

static dgc_stats_t *dgc_stats;

generic_table_t *emulation_plans;

#define JIT_MANAGED_FLUSH_THRESHOLD 10
#define MAX_EXEC_AREA_COUNTERS 1000

typedef struct _exec_area_counter_t {
    app_pc start;
    size_t size;
    uint count;
} exec_area_counter_t;

typedef struct _exec_area_counters_t {
    uint size;
    uint max_size;
    exec_area_counter_t *counters;
} exec_area_counters_t;

static exec_area_counters_t *exec_area_counters;

#define EXPAND_ARRAY(array, max_size, type) \
do { \
    void *original_array = (void *)(array); \
    (array) = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, type, (max_size) * 2, \
                               ACCT_OTHER, UNPROTECTED); \
    memcpy((array), original_array, (max_size) * sizeof(type)); \
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, original_array, type, (max_size), \
                    ACCT_OTHER, UNPROTECTED); \
    (max_size) *= 2; \
} while (0)

#if !(defined (WINDOWS) && defined (X64))
static ptr_uint_t
valgrind_discard_translations(dr_vg_client_request_t *request);
#endif

#ifdef RELEASE_LOGGING
# define IF_RELLOG(x) x
# define _IF_RELLOG(x) , x
#else
# define IF_RELLOG(x)
# define _IF_RELLOG(x)
#endif

static bool
safe_remove_bb(dcontext_t *dcontext, fragment_t *f
               _IF_RELLOG(bool is_tweak) _IF_RELLOG(bool is_cti_tweak));

static bool
safe_delete_shared_fragment(dcontext_t *dcontext, fragment_t *f);

static void
safe_delete_fragment(dcontext_t *dcontext, fragment_t *f);

static void
dgc_table_resized();

static void
free_dgc_bucket_chain(void *p);

static void
free_double_mapping(double_mapping_t *mapping);

static void
free_emulation_plan(void *p);

static void
update_thread_state();

static void
dgc_stat_report();

static inline app_pc
maybe_exit_cti_disp_pc(app_pc maybe_branch_pc);

#ifdef JITOPT
static void
free_dgc_writer_mapping(dgc_writer_mapping_t *mapping);

static void
clear_dgc_writer_table();

static void
insert_dgc_writer_offsets(app_pc start, size_t size, ptr_int_t offset);

static void
remove_dgc_writer_offsets(app_pc start, size_t size);
#endif

#ifdef CHECK_STALE_BBS
static void
check_stale_bbs();
#endif

void
jitopt_init()
{
#ifdef JITOPT_ANNOTATION
    dr_annotation_register_call(DYNAMORIO_ANNOTATE_MANAGE_CODE_AREA_NAME,
                                (void *) annotation_manage_code_area, false, 2,
                                DR_ANNOTATION_CALL_TYPE_FASTCALL);

    dr_annotation_register_call(DYNAMORIO_ANNOTATE_UNMANAGE_CODE_AREA_NAME,
                                (void *) annotation_unmanage_code_area, false, 2,
                                DR_ANNOTATION_CALL_TYPE_FASTCALL);
    dr_annotation_register_call(DYNAMORIO_ANNOTATE_FLUSH_FRAGMENTS_NAME,
                                (void *) flush_jit_fragments, false, 2,
                                DR_ANNOTATION_CALL_TYPE_FASTCALL);
#endif
#if !(defined (WINDOWS) && defined (X64))
    dr_annotation_register_valgrind(DR_VG_ID__DISCARD_TRANSLATIONS,
                                    valgrind_discard_translations);
#endif
#ifdef JITOPT
    dgc_table = asmtable_create(20, 45, &dgc_table_lock,
                                (void *)free_dgc_bucket_chain, dgc_table_resized);

    dgc_bucket_gc_list = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dgc_bucket_gc_list_t,
                                         ACCT_OTHER, UNPROTECTED);
    dgc_bucket_gc_list->max_staging = DGC_BUCKET_GC_CAPACITY;
    dgc_bucket_gc_list->staging = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, dgc_bucket_t *,
                                                   dgc_bucket_gc_list->max_staging,
                                                   ACCT_OTHER, UNPROTECTED);
    dgc_bucket_gc_list->max_removals = DGC_BUCKET_GC_CAPACITY;
    dgc_bucket_gc_list->removals = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, dgc_bucket_t *,
                                                    dgc_bucket_gc_list->max_removals,
                                                    ACCT_OTHER, UNPROTECTED);
    thread_state = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dgc_thread_state_t,
                                   ACCT_OTHER, UNPROTECTED);
    memset(thread_state, 0, sizeof(dgc_thread_state_t));
    fragment_intersection = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dgc_fragment_intersection_t,
                                            ACCT_OTHER, UNPROTECTED);
    fragment_intersection->bb_tag_max = 0x20;
    fragment_intersection->bb_tags =
        HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, app_pc, fragment_intersection->bb_tag_max,
                         ACCT_OTHER, UNPROTECTED);
    fragment_intersection->trace_tag_max = 0x20;
    fragment_intersection->trace_tags =
        HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, app_pc, fragment_intersection->trace_tag_max,
                         ACCT_OTHER, UNPROTECTED);
    dgc_removal_queue = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dgc_removal_queue_t,
                                        ACCT_OTHER, UNPROTECTED);
    dgc_removal_queue->index = 0;
    dgc_removal_queue->max = 0x20;
    dgc_removal_queue->tags =
        HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, app_pc, dgc_removal_queue->max,
                         ACCT_OTHER, UNPROTECTED);
    dgc_removal_queue->sample_index = 0;

    double_mappings = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, double_mapping_list_t,
                                      ACCT_OTHER, UNPROTECTED);
    double_mappings->index = 0;
    double_mappings->mappings =
        HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, double_mapping_t, MAX_DOUBLE_MAPPINGS,
                         ACCT_OTHER, UNPROTECTED);

    emulation_plans = generic_hash_create(GLOBAL_DCONTEXT, 7, 80,
                                        HASHTABLE_ENTRY_SHARED | HASHTABLE_SHARED |
                                        HASHTABLE_RELAX_CLUSTER_CHECKS | HASHTABLE_PERSISTENT,
                                        free_emulation_plan _IF_DEBUG("DGC Emulation Plans"));

    dgc_writer_mapping_table = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dgc_writer_mapping_table_t,
                                               ACCT_OTHER, UNPROTECTED);
    memset(dgc_writer_mapping_table, 0, sizeof(dgc_writer_mapping_table_t));

    exec_area_counters = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, exec_area_counters_t,
                                         ACCT_OTHER, UNPROTECTED);
    memset(exec_area_counters, 0, sizeof(exec_area_counters_t));
    exec_area_counters->max_size = 100;
    exec_area_counters->counters = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, exec_area_counter_t,
                                                    exec_area_counters->max_size,
                                                    ACCT_OTHER, UNPROTECTED);
    memset(exec_area_counters->counters, 0,
           sizeof(exec_area_counter_t) * exec_area_counters->max_size);
#endif
    dgc_stats = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dgc_stats_t, ACCT_OTHER, UNPROTECTED);
    memset(dgc_stats, 0, sizeof(dgc_stats_t));
}

void
jitopt_exit()
{
    uint i;
#ifdef JITOPT
    asmtable_destroy(dgc_table);
    DELETE_LOCK(dgc_table_lock);
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, dgc_bucket_gc_list->staging, dgc_bucket_t *,
                    dgc_bucket_gc_list->max_staging, ACCT_OTHER, UNPROTECTED);
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, dgc_bucket_gc_list->removals, dgc_bucket_t *,
                    dgc_bucket_gc_list->max_removals, ACCT_OTHER, UNPROTECTED);
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, dgc_bucket_gc_list, dgc_bucket_gc_list_t,
                   ACCT_OTHER, UNPROTECTED);
    if (thread_state->threads != NULL) {
        global_heap_free(thread_state->threads,
                         thread_state->count*sizeof(thread_record_t*)
                         HEAPACCT(ACCT_THREAD_MGT));
    }
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, thread_state, dgc_thread_state_t,
                   ACCT_OTHER, UNPROTECTED);
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, fragment_intersection->bb_tags, app_pc,
                    fragment_intersection->bb_tag_max, ACCT_OTHER, UNPROTECTED);
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, fragment_intersection->trace_tags, app_pc,
                    fragment_intersection->trace_tag_max, ACCT_OTHER, UNPROTECTED);
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, fragment_intersection, dgc_fragment_intersection_t,
                   ACCT_OTHER, UNPROTECTED);
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, dgc_removal_queue->tags, app_pc,
                    dgc_removal_queue->max, ACCT_OTHER, UNPROTECTED);
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, dgc_removal_queue, dgc_removal_queue_t,
                   ACCT_OTHER, UNPROTECTED);

    for (i = 0; i < double_mappings->index; i++)
        free_double_mapping(&double_mappings->mappings[i]);
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, double_mappings->mappings, double_mapping_t,
                    MAX_DOUBLE_MAPPINGS, ACCT_OTHER, UNPROTECTED);
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, double_mappings, double_mapping_list_t,
                   ACCT_OTHER, UNPROTECTED);

    generic_hash_destroy(GLOBAL_DCONTEXT, emulation_plans);

    clear_dgc_writer_table();
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, dgc_writer_mapping_table, dgc_writer_mapping_table_t,
                   ACCT_OTHER, UNPROTECTED);
    DELETE_LOCK(dgc_mapping_lock);

    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, exec_area_counters->counters, exec_area_counter_t,
                    exec_area_counters->max_size, ACCT_OTHER, UNPROTECTED);
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, exec_area_counters, exec_area_counters_t,
                   ACCT_OTHER, UNPROTECTED);
#endif
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, dgc_stats, dgc_stats_t, ACCT_OTHER, UNPROTECTED);
}

void
jitopt_thread_init(dcontext_t *dcontext)
{
    local_state_extended_t *state = (local_state_extended_t *) dcontext->local_state;
    state->dgc_mapping_table = dgc_writer_mapping_table;
    state->dgc_coverage_table = dgc_table->table;
    state->dgc_coverage_mask = dgc_table->hash_mask;

    RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1,
                "Initialized thread 0x%x with dgc mapping table "PFX"\n",
                get_thread_id(), state->dgc_mapping_table);
}

void
manage_code_area(app_pc start, size_t len)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    //dr_printf("Manage code area "PFX"-"PFX"\n",
    //    start, start+len);
    RELEASE_LOG(GLOBAL, LOG_ANNOTATIONS, 1, "Manage code area "PFX"-"PFX"\n",
                start, start+len);
#ifdef JITOPT_ANNOTATION
    uint prot;
    if (!set_region_jit_monitored(start, len)) {
        RELEASE_LOG(GLOBAL, LOG_VMAREAS, 1,
                    "DGC: Failed to manage area; already managed! "PFX" +0x%x \n",
                    start, len);
        return;
    }
    if (!get_memory_info(start, NULL, NULL, &prot)) {
        RELEASE_LOG(GLOBAL, LOG_VMAREAS, 1,
                    "DGC: Failed to get memory protection info for "PFX" +0x%x\n",
                    start, len);
        return;
    }
    setup_double_mapping(dcontext, start, len, prot);
#else /* JITOPT_INFERENCE */
    set_region_app_managed(start, len);
#endif

    if (!thread_state->scaled_trace_head_tables) {
        thread_state->scaled_trace_head_tables = true;
        set_trace_head_table_resize_scale(5);
    }
}

void
annotation_manage_code_area(app_pc start, size_t len)
{
    manage_code_area(start, len);
}

void
annotation_unmanage_code_area(app_pc start, size_t len)
{
    dcontext_t *dcontext = get_thread_private_dcontext();

    if (!is_jit_managed_area(start))
        return;

    RELEASE_LOG(GLOBAL, LOG_ANNOTATIONS, 1, "Unmanage code area "PFX"-"PFX"\n",
                start, start+len);

    mutex_lock(&thread_initexit_lock);
    flush_fragments_and_remove_region(dcontext, start, len,
                                      true /* own initexit_lock */, false);
    mutex_unlock(&thread_initexit_lock);
#ifdef JITOPT
    dgc_notify_region_cleared(start, start+len);
#endif
}

static void
flush_and_isolate_region(dcontext_t *dcontext, app_pc start, size_t len)
{
    mutex_lock(&thread_initexit_lock);
    flush_fragments_in_region_start(dcontext, start, len, true /*own initexit*/,
                                    false/*don't free futures*/, false/*exec valid*/,
                                    false/*don't force synchall*/ _IF_DGCDIAG(NULL));
    ASSERT_OWN_MUTEX(true, &thread_initexit_lock);
    vm_area_isolate_region(dcontext, start, start+len); // make sure per-thread regions are gone at this point
    ASSERT_OWN_MUTEX(true, &thread_initexit_lock);
    flush_fragments_in_region_finish(dcontext, true);
    mutex_unlock(&thread_initexit_lock);
}

void
flush_jit_fragments(app_pc start, size_t len)
{
    dcontext_t *dcontext = get_thread_private_dcontext();

    /* this is slow--maybe keep a local sorted list of app-managed regions */
    if (!is_jit_managed_area(start)) {
#ifdef RELEASE_LOGGING
        RSTATS_INC(non_app_managed_writes_observed);
#endif
        return;
    }
#ifdef CHECK_STALE_BBS
    check_stale_bbs();
#endif

    LOG(THREAD, LOG_ANNOTATIONS, 1, "Flush fragments "PFX"-"PFX"\n",
        start, start+len);

#ifdef RELEASE_LOGGING
    if (dgc_stats->timer++ > 1000) {
        dgc_stat_report();
        dgc_stats->timer = 0;
    }
#endif

    //if (len == 0 || is_couldbelinking(dcontext))
    //    return;
    //if (!executable_vm_area_executed_from(start, start+len))
    //    return;

#ifdef RELEASE_LOGGING
    RSTATS_INC(app_managed_writes_observed);
#endif
#ifdef JITOPT
    if (true) {
#ifdef RELEASE_LOGGING
        uint removal_count =
#endif
            remove_patchable_fragments(dcontext, start, start+len);
#ifdef RELEASE_LOGGING
        if (removal_count > 0) {
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1,
                        "Removed %d fragments in ["PFX"-"PFX"].\n",
                        removal_count, start, start+len);
            RSTATS_INC(app_managed_writes_handled);
            RSTATS_ADD(app_managed_fragments_removed, removal_count);

            if (len < 4)
                RSTATS_INC(app_managed_micro_writes);
            else if (len == 4) {
                if (maybe_exit_cti_disp_pc(start - 1) != NULL ||
                    maybe_exit_cti_disp_pc(start - 2) != NULL)
                    RSTATS_INC(app_managed_cti_target_writes);
                else
                    RSTATS_INC(app_managed_word_writes);
            } else if (len <= 0x20)
                RSTATS_INC(app_managed_small_writes);
            else if (len <= 0x100)
                RSTATS_INC(app_managed_subpage_writes);
            else if (len == PAGE_SIZE)
                RSTATS_INC(app_managed_page_writes);
            else
                RSTATS_INC(app_managed_multipage_writes);
        } else {
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1,
                        "DGC: No fragments to remove in write to ["PFX"-"PFX"].\n",
                        start, start+len);
            RSTATS_INC(app_managed_writes_ignored);
        }
#endif
    } else {
# endif
        if (len == PAGE_SIZE)
            RSTATS_INC(app_managed_page_writes);
        else
            RSTATS_INC(app_managed_multipage_writes);

        flush_and_isolate_region(dcontext, start, len);
#ifdef JITOPT
        dgc_notify_region_cleared(start, start+len);
    }
#endif
}

#if !(defined (WINDOWS) && defined (X64))
static ptr_uint_t
valgrind_discard_translations(dr_vg_client_request_t *request)
{
# ifdef JITOPT_ANNOTATIONS
    flush_jit_fragments((app_pc) request->args[0], request->args[1]);
# endif
    return 0;
}
#endif

#ifdef JITOPT
static void
free_dgc_writer_mapping(dgc_writer_mapping_t *mapping)
{
    if (mapping != NULL) {
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, mapping, dgc_writer_mapping_t,
                       ACCT_OTHER, UNPROTECTED);
    }
}

static void
clear_dgc_writer_table()
{
    uint i;
    dgc_writer_mapping_t *mapping, *next_mapping;
    for (i = 0; i < DGC_MAPPING_TABLE_SIZE; i++) {
        mapping = dgc_writer_mapping_table->table[i];
        for (; mapping != NULL; mapping = next_mapping) {
            next_mapping = mapping->next;
            free_dgc_writer_mapping(mapping);
        }
    }
}

ptr_int_t
lookup_dgc_writer_offset(app_pc addr)
{
    ptr_uint_t page_id = DGC_SHADOW_PAGE_ID(addr);
    uint key = DGC_SHADOW_KEY(page_id);
    dgc_writer_mapping_t *mapping = dgc_writer_mapping_table->table[key];
    while (mapping != NULL && mapping->page_id != page_id)
        mapping = mapping->next;
    if (mapping == NULL)
        return 0;
    else
        return mapping->offset;
}

static void
insert_dgc_writer_offsets(app_pc start, size_t size, ptr_int_t offset)
{
    uint key;
    dgc_writer_mapping_t *mapping;
    ptr_uint_t page_id = DGC_SHADOW_PAGE_ID(start);
    ptr_uint_t page_span = (size >> DGC_MAPPING_TABLE_SHIFT);
    ptr_uint_t last_page_id = page_id + page_span;
#ifdef DEBUG
    dcontext_t *dcontext = get_thread_private_dcontext();
#endif

    ASSERT_OWN_MUTEX(true, &dgc_mapping_lock);
    RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Insert writer offsets "PFX" +0x%x = "PFX" "
                "(page "PFX" +0x%x pages)\n", start, size, offset, page_id, page_span);

    for (; page_id < last_page_id; page_id++) {
        key = DGC_SHADOW_KEY(page_id);
        if (dgc_writer_mapping_table->table[key] != NULL) {
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1,
                        "DGC: Multiple writer offset buckets at "PFX" (key 0x%x).\n",
                        page_id, key);
        }
        mapping = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dgc_writer_mapping_t,
                                  ACCT_OTHER, UNPROTECTED);
        mapping->page_id = page_id;
        mapping->offset = offset;
        mapping->next = dgc_writer_mapping_table->table[key];
        dgc_writer_mapping_table->table[key] = mapping;
    }
}

static void
remove_dgc_writer_offsets(app_pc start, size_t size)
{
    uint key;
    dgc_writer_mapping_t *mapping, *removal;
    ptr_uint_t page_id = DGC_SHADOW_PAGE_ID(start);
    ptr_uint_t page_span = (size >> DGC_MAPPING_TABLE_SHIFT);
    ptr_uint_t last_page_id = page_id + page_span;
#ifdef DEBUG
    dcontext_t *dcontext = get_thread_private_dcontext();
#endif

    ASSERT_OWN_MUTEX(true, &dgc_mapping_lock);
    RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Remove writer offsets "PFX" +0x%x "
                "(page "PFX" +0x%x pages)\n", start, size, page_id, page_span);

    for (; page_id < last_page_id; page_id++) {
        key = DGC_SHADOW_KEY(page_id);
        if (dgc_writer_mapping_table->table[key] == NULL) {
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Found no writer offset "
                        "for page "PFX": bucket is empty.\n", page_id);
            continue;
        }
        if (dgc_writer_mapping_table->table[key]->page_id == page_id) { /* remove head */
            removal = dgc_writer_mapping_table->table[key];
            dgc_writer_mapping_table->table[key] = dgc_writer_mapping_table->table[key]->next;
            free_dgc_writer_mapping(removal);
        } else { // remove internal entry
            mapping = dgc_writer_mapping_table->table[key];
            while (mapping->next != NULL && mapping->next->page_id != page_id)
                mapping = mapping->next;
            if (mapping->next == NULL) {
                RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Found no writer"
                            " offset for page "PFX": not in bucket.\n", page_id);
                continue;
            }
            removal = mapping->next;
            mapping->next = mapping->next->next;
            free_dgc_writer_mapping(removal); // FIXME: race with reader!
        }
    }
}

static ptr_uint_t
get_double_mapped_page_delta(dcontext_t *dcontext, app_pc app_memory_start, size_t app_memory_size, uint prot)
{
    ptr_int_t fd;
    int result;
    uint i;
    char file[0x20];
    app_pc remap_pc;
    double_mapping_t *new_mapping;

    for (i = 0; i < double_mappings->index; i++) {
        if (double_mappings->mappings[i].app_memory_start == app_memory_start) {
            ASSERT(double_mappings->mappings[i].size == app_memory_size);
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1,
                        "DGC: Found existing double-mapping "PFX" +0x%x\n",
                        app_memory_start, app_memory_size);
            return double_mappings->mappings[i].double_mapping_start - app_memory_start;
        }
    }

    RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1,
                "DGC: Creating new double-mapping "PFX" +0x%x with index %d\n",
                app_memory_start, app_memory_size, double_mappings->index);

    ASSERT(double_mappings->index < MAX_DOUBLE_MAPPINGS);
    if (double_mappings->index >= MAX_DOUBLE_MAPPINGS) {
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 0, "Error! Too many double-mappings: %d\n",
                    double_mappings->index);
    }

    new_mapping = &double_mappings->mappings[double_mappings->index];
    new_mapping->app_memory_start = app_memory_start;
    new_mapping->size = app_memory_size;
    new_mapping->double_mapping_size = app_memory_size;

    memcpy(file, "/dev/shm/jit_", 13);
    file[13] = 'a'; // + (double_mappings->index / 26);
    file[14] = 'a'; // + (double_mappings->index % 26);
    file[15] = '\0';
    RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1,
                "DGC: Mapping "PFX" +0x%x to shmem %s\n",
                app_memory_start, app_memory_size, file);
    fd = dynamorio_syscall(SYS_open, 3ULL, file, (ptr_uint_t)(O_RDWR | O_CREAT | O_NOFOLLOW),
                           (ptr_uint_t)(S_IRUSR | S_IWUSR));
    if (fd < 0) {
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 0,
                    "DGC: Failed to create the backing file %s for the double-mapping\n", file);
        //RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 0, "Error: '%s'\n", strerror(fd));
        // TODO: find dependency and add to cmake for common.fib target
        return 0;
    }
    RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1,
                "DGC: Created the backing file %s (0x%x) for the double-mapping in process 0x%x\n",
                file, fd, get_process_id());
    result = dynamorio_syscall(SYS_ftruncate, 2ULL, fd, app_memory_size);
    if (result < 0) {
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 0,
                    "DGC: Failed to resize the backing file %s for the double-mapping\n", file);
        return 0;
    }
    RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1,
                "DGC: Extended the backing file %s to 0x%x bytes\n", file, app_memory_size);
    new_mapping->fd = fd;

    result = dynamorio_syscall(SYS_unlink, 1ULL, file);
    if (result < 0) {
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 0,
                    "DGC: Failed to unlink the backing file %s for the double-mapping\n", file);
        return 0;
    }
    RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1,
                "DGC: Unlinked the backing file %s\n", file);

    new_mapping->double_mapping_start =
        (app_pc) dynamorio_syscall(MMAP, 6ULL, 0ULL, app_memory_size,
                                   (ptr_uint_t)PROT_READ|PROT_WRITE, (ptr_uint_t)MAP_SHARED,
                                   fd, 0ULL/*offset*/);

    RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1,
                "DGC: Mapped the backing file %s to "PFX"\n", file, new_mapping->double_mapping_start);

    memcpy(new_mapping->double_mapping_start, app_memory_start, app_memory_size);

    result = dynamorio_syscall(SYS_munmap, 2ULL, app_memory_start, app_memory_size);
    if (result < 0) {
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 0,
                    "DGC: Failed to unmap the original memory at "PFX"\n", app_memory_start);
        return 0;
    }

    remap_pc =
        (app_pc) dynamorio_syscall(MMAP, 6ULL, app_memory_start, app_memory_size,
                                   (ptr_uint_t)prot, (ptr_uint_t)(MAP_SHARED | MAP_FIXED),
                                   fd, 0ULL/*offset*/);

    RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1,
                "DGC: Remap says "PFX"; new mapping is "PFX" and app memory is "PFX"\n",
              remap_pc, new_mapping->double_mapping_start, app_memory_start);

    ASSERT(remap_pc == app_memory_start);

    double_mappings->index++;
    return new_mapping->double_mapping_start - app_memory_start;
}

static void
emulate_writer(priv_mcontext_t *mc, emulation_plan_t *plan, ptr_int_t page_delta,
               app_pc write_target, bool simulate)
{
    uint i;
    uint *value, *value_base;
    uint *target_access = (uint *)((ptr_int_t)write_target + page_delta);
#ifdef DEBUG
    dcontext_t *dcontext = get_thread_private_dcontext();
#endif

    if (plan->src_in_reg)
        value = (uint *)((byte *)mc + plan->src.mcontext_reg_offset);
    else
        value = (uint *)&plan->src.immediate;
    value_base = value;

    switch (plan->dst_size) {
    case 1: case 2: case 4: case 8: case 16:
        break;
    default:
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 0, "Error! Cannot emulate operand size %d!\n", plan->dst_size);
    }

    if (plan->op == EMUL_MOV) {
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1,
                    "DGC: Attempting to write %d bytes to "PFX" via "PFX"\n",
                    plan->dst_size, write_target, target_access);

        if (plan->dst_size == 1) {
            byte byte_value = (*value & 0xff);
            byte *byte_target_access = (byte *)target_access;
            if (!simulate)
                *byte_target_access = byte_value;
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC:    mov 0x%x to "PFX"\n",
                        byte_value, byte_target_access);
            ASSERT(*byte_target_access == byte_value);
            ASSERT(*(byte*)write_target == byte_value);
        } else  if (plan->dst_size == 2) {
            short short_value = (*value & 0xffff);
            short *short_target_access = (short *)target_access;
            if (!simulate)
                *short_target_access = short_value;
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC:    mov 0x%x to "PFX"\n",
                        short_value, short_target_access);
            ASSERT(*short_target_access == short_value);
            ASSERT(*(short*)write_target == short_value);
        } else {
            uint *write_target_access = (uint *)write_target;
            for (i = 0; i < (plan->dst_size/sizeof(uint)); i++, target_access++, value++, write_target_access++) {
                if (!simulate)
                    *target_access = *value;
                RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC:    mov 0x%x to "PFX"\n",
                            *value, target_access);
                ASSERT(*target_access == *value);
                ASSERT(*write_target_access == *value);
            }
            target_access = (uint *)((ptr_int_t)write_target + page_delta);
            ASSERT(*target_access == *value_base);
            ASSERT(*(uint*)write_target == *value_base);
        }
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1,
                    "DGC: "PFX" successfully wrote %d bytes to "PFX" via "PFX"\n",
                    plan->writer_pc, plan->dst_size, write_target, target_access);
    } else if (plan->op == EMUL_OR) {
        if (plan->dst_size == 1) {
            byte byte_value = (*value & 0xff);
            byte *byte_target_access = (byte *)target_access;
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Attempting to 'or' %d bytes to "PFX" via "PFX"\n", plan->dst_size, write_target, target_access);
            if (!simulate)
                *byte_target_access |= byte_value;
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC:    or 0x%x into "PFX"\n",
                        byte_value, byte_target_access);
            ASSERT((*byte_target_access & byte_value) == byte_value);
            ASSERT((*(byte*)write_target & byte_value) == byte_value);
        } else if (plan->dst_size == 4) {
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Attempting to 'or' %d bytes to "PFX" via "PFX"\n", plan->dst_size, write_target, target_access);
            if (!simulate)
                *target_access |= *value;
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC:    or 0x%x into "PFX"\n",
                        *value, target_access);
            ASSERT((*target_access & *value) == *value);
            ASSERT((*(uint*)write_target & *value) == *value);
        } else if (plan->dst_size == 8) {
            ptr_uint_t *word_target_access = (ptr_uint_t *)((ptr_int_t)write_target + page_delta);
            ptr_uint_t *word_value = (ptr_uint_t *)value;
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Attempting to 'or' %d bytes to "PFX" via "PFX"\n", plan->dst_size, write_target, target_access);
            if (!simulate)
                *word_target_access |= *word_value;
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC:    or 0x%x into "PFX"\n",
                        *word_value, word_target_access);
            ASSERT((*word_target_access & *word_value) == *word_value);
            ASSERT((*(ptr_uint_t*)write_target & *word_value) == *word_value);
        }
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "Successfully 'or'd %d bytes to "PFX" via "PFX"\n", plan->dst_size, write_target, target_access);
    } else if (plan->op == EMUL_AND) {
        if (plan->dst_size == 1) {
            byte byte_value = (*value & 0xff);
            byte *byte_target_access = (byte *)target_access;
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Attempting to 'and' %d bytes to "PFX" via "PFX"\n", plan->dst_size, write_target, target_access);
            if (!simulate)
                *byte_target_access &= byte_value;
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC:    and 0x%x into "PFX"\n",
                        byte_value, byte_target_access);
            ASSERT((*byte_target_access & ~byte_value) == 0);
            ASSERT((*(byte*)write_target & ~byte_value) == 0);
        } else if (plan->dst_size == 4) {
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Attempting to 'and' %d bytes to "PFX" via "PFX"\n", plan->dst_size, write_target, target_access);
            if (!simulate)
                *target_access &= *value;
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC:    and 0x%x into "PFX"\n",
                        *value, target_access);
            ASSERT((*target_access & ~(*value)) == 0);
            ASSERT((*(uint*)write_target & ~(*value)) == 0);
        } else if (plan->dst_size == 8) {
            ptr_uint_t *word_target_access = (ptr_uint_t *)((ptr_int_t)write_target + page_delta);
            ptr_uint_t *word_value = (ptr_uint_t *)value;
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Attempting to 'and' %d bytes to "PFX" via "PFX"\n", plan->dst_size, write_target, target_access);
            if (!simulate)
                *word_target_access &= *word_value;
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC:    and 0x%x into "PFX"\n",
                        *word_value, word_target_access);
            ASSERT((*word_target_access & ~(*word_value)) == 0);
            ASSERT((*(ptr_uint_t*)write_target & ~(*word_value)) == 0);
        }
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "Successfully 'and'd %d bytes to "PFX" via "PFX"\n", plan->dst_size, write_target, target_access);
    } else if (plan->op == EMUL_XOR) {
        if (plan->dst_size == 1) { // TODO:
            byte byte_value = (*value & 0xff);
            byte *byte_target_access = (byte *)target_access;
            DEBUG_DECLARE(byte original_value = *byte_target_access;);
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Attempting to 'and' %d bytes to "PFX" via "PFX"\n", plan->dst_size, write_target, target_access);
            if (!simulate)
                *byte_target_access ^= byte_value;
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC:    xor 0x%x into "PFX"\n",
                        byte_value, byte_target_access);
            ASSERT(((*byte_target_access & byte_value) & original_value) == 0);
            ASSERT(((*(byte*)write_target & ~byte_value) & original_value) == 0);
        } else if (plan->dst_size == 4) {
            DEBUG_DECLARE(uint original_value = *target_access;);
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Attempting to 'and' %d bytes to "PFX" via "PFX"\n", plan->dst_size, write_target, target_access);
            if (!simulate)
                *target_access ^= *value;
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC:    xor 0x%x into "PFX"\n",
                        *value, target_access);
            ASSERT(((*target_access & (*value)) & original_value) == 0);
            ASSERT(((*(uint*)write_target & (*value)) & original_value) == 0);
        } else if (plan->dst_size == 8) {
            ptr_uint_t *word_target_access = (ptr_uint_t *)((ptr_int_t)write_target + page_delta);
            ptr_uint_t *word_value = (ptr_uint_t *)value;
            DEBUG_DECLARE(ptr_uint_t original_value = *(ptr_uint_t *)target_access;);
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Attempting to 'and' %d bytes to "PFX" via "PFX"\n", plan->dst_size, write_target, target_access);
            if (!simulate)
                *word_target_access ^= *word_value;
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC:    xor 0x%x into "PFX"\n",
                        *word_value, word_target_access);
            ASSERT(((*word_target_access & ~(*word_value)) & original_value) == 0);
            ASSERT(((*(ptr_uint_t*)write_target & ~(*word_value)) & original_value) == 0);
        }
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "Successfully 'xor'd %d bytes to "PFX" via "PFX"\n", plan->dst_size, write_target, target_access);
    } else if (plan->op == EMUL_ADD) {
        if (plan->dst_size == 1) {
            byte byte_value = (*value & 0xff);
            byte *byte_target_access = (byte *)target_access;
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Attempting to add %d bytes to "PFX" via "PFX"\n", plan->dst_size, write_target, target_access);
            if (!simulate)
                *byte_target_access += byte_value;
        } else if (plan->dst_size == 4) {
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Attempting to add %d bytes to "PFX" via "PFX"\n", plan->dst_size, write_target, target_access);
            if (!simulate)
                *target_access += *value;
        } else if (plan->dst_size == 8) {
            ptr_uint_t *word_target_access = (ptr_uint_t *)((ptr_int_t)write_target + page_delta);
            ptr_uint_t *word_value = (ptr_uint_t *)value;
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Attempting to add %d bytes to "PFX" via "PFX"\n", plan->dst_size, write_target, target_access);
            if (!simulate)
                *word_target_access += *word_value;
        }
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "Successfully subtracted %d bytes to "PFX" via "PFX"\n", plan->dst_size, write_target, target_access);
    } else if (plan->op == EMUL_SUB) {
        if (plan->dst_size == 1) {
            byte byte_value = (*value & 0xff);
            byte *byte_target_access = (byte *)target_access;
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Attempting to sub %d bytes to "PFX" via "PFX"\n", plan->dst_size, write_target, target_access);
            if (!simulate)
                *byte_target_access -= byte_value;
        } else if (plan->dst_size == 4) {
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Attempting to sub %d bytes to "PFX" via "PFX"\n", plan->dst_size, write_target, target_access);
            if (!simulate)
                *target_access -= *value;
        } else if (plan->dst_size == 8) {
            ptr_uint_t *word_target_access = (ptr_uint_t *)((ptr_int_t)write_target + page_delta);
            ptr_uint_t *word_value = (ptr_uint_t *)value;
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Attempting to sub %d bytes to "PFX" via "PFX"\n", plan->dst_size, write_target, target_access);
            if (!simulate)
                *word_target_access -= *word_value;
        }
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "Successfully subtracted %d bytes to "PFX" via "PFX"\n", plan->dst_size, write_target, target_access);
    } else {
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 0, "Error! Cannot emulate opcode 0x%x!\n", instr_get_opcode(&plan->writer));
    }
}

void
setup_double_mapping(dcontext_t *dcontext, app_pc start, uint len, uint prot)
{
    ptr_int_t page_delta;

    RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1,
                "DGC: Setup double-mapping for "PFX" +0x%x on thread 0x%x\n",
                start, len, get_thread_id());

    mutex_lock(&dgc_mapping_lock);
    page_delta = get_double_mapped_page_delta(dcontext, start, len, prot);
    if (page_delta == 0) {
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 0,
                    "DGC: Failed to setup page delta for app memory "PFX" +0x%x\n",
                    start, len);
    } else {
        remove_dgc_writer_offsets(start, len);
        insert_dgc_writer_offsets(start, len, page_delta);
    }
    mutex_unlock(&dgc_mapping_lock);
}

void
notify_readonly_for_cache_consistency(app_pc start, size_t size, bool now_readonly)
{
#ifdef DEBUG
    dcontext_t *dcontext = get_thread_private_dcontext();
#endif

    mutex_lock(&dgc_mapping_lock);
    RELEASE_LOG(THREAD, LOG_VMAREAS, 1, "notify_readonly_for_cache_consistency("PFX", 0x%x, %s)\n",
                start, size, now_readonly ? "readonly" : "writable");
    if (now_readonly) {
        ptr_int_t offset = lookup_dgc_writer_offset(start);
        if (offset == 0)
            insert_dgc_writer_offsets(start, size, 1);
        // else there is a double-mapping, so leave it intact
    } else {
        remove_dgc_writer_offsets(start, size);
    }
    mutex_unlock(&dgc_mapping_lock);
}

void
locate_and_manage_code_area(app_pc pc)
{
    // TODO: check & prevent flush in this region!
    app_pc start;
    size_t size;
#ifdef DEBUG
    dcontext_t *dcontext = get_thread_private_dcontext();
#endif

    RELEASE_LOG(THREAD, LOG_VMAREAS, 1, "locate_and_manage_code_area() at "PFX"\n", pc);

    bool strange_case = false;
    bool found = get_non_jit_area_bounds(pc, &start, &size);
    /*
    if (!found) {
        found = get_non_jit_area_bounds(*(app_pc *)pc, &start, &size);
        strange_case = true;
    }
    */
    if (found) {
        dcontext_t *dcontext = get_thread_private_dcontext();
        uint prot;
        if (strange_case) {
            RELEASE_LOG(THREAD, LOG_VMAREAS, 1,
                        "locate_and_manage_code_area() strange indirection through "PFX"\n",
                        *(app_pc *)pc);
        }
        get_memory_info(start, NULL, NULL, &prot);
        if (TEST(PROT_WRITE, prot)) {
            RELEASE_LOG(THREAD, LOG_VMAREAS, 1, "locate_and_manage_code_area ignored for writable area at "PFX"\n", pc);
            return;
        }

        //manage_code_area(start, size);
        mutex_lock(&thread_initexit_lock);
        flush_fragments_and_remove_region(dcontext, start, size,
                                          true /* own initexit_lock */, false);
        mutex_unlock(&thread_initexit_lock);
        notify_exec_invalidation(start, size);
    } else {
        ptr_int_t offset = lookup_dgc_writer_offset(start);
        RELEASE_LOG(THREAD, LOG_VMAREAS, 0, "locate_and_manage_code_area failed at "
                PFX". DGC writer offset is 0x%llx.\n", offset);
        dr_exit_process(666);
    }
}

void
notify_exec_invalidation(app_pc start, size_t size)
{
    uint i;
#ifdef DEBUG
    dcontext_t *dcontext = get_thread_private_dcontext();
#endif

    RELEASE_LOG(THREAD, LOG_VMAREAS, 1, "notify_exec_invalidation("PFX", 0x%x)\n",
                start, size);
    mutex_lock(&dgc_mapping_lock);
    for (i = 0; i < exec_area_counters->size; i++) {
        if (exec_area_counters->counters[i].start == start) {
            uint count = exec_area_counters->counters[i].count++;
            mutex_unlock(&dgc_mapping_lock);
            if (exec_area_counters->counters[i].size != size) {
                RELEASE_LOG(THREAD, LOG_VMAREAS, 1, "Warning: exec_invalidation_count: "
                            "area size changed for counter %d from 0x%x to 0x%x\n",
                            i, exec_area_counters->counters[i].size, size);
                exec_area_counters->counters[i].size = size;
            }
            RELEASE_LOG(THREAD, LOG_VMAREAS, 1, "exec_invalidation_count %d for "PFX"\n",
                        exec_area_counters->counters[i].count, start);
            if (count > JIT_MANAGED_FLUSH_THRESHOLD) {
                RELEASE_LOG(THREAD, LOG_VMAREAS, 1, "Time to manage vmarea "PFX"\n",
                            start);
                manage_code_area(start, size);
            }
            return;
        }
    }
    i = exec_area_counters->size;
    exec_area_counters->size++;
    if (exec_area_counters->size >= exec_area_counters->max_size) {
        EXPAND_ARRAY(exec_area_counters->counters,
                     exec_area_counters->max_size, exec_area_counter_t);
    }
    exec_area_counters->counters[i].start = start;
    exec_area_counters->counters[i].size = size;
    exec_area_counters->counters[i].count = 0;
    mutex_unlock(&dgc_mapping_lock);
}

bool
shrink_double_mapping(app_pc old_start, app_pc new_start, size_t new_size)
{
    if (clear_double_mapping(old_start)) {
        uint prot;
        dcontext_t *dcontext = get_thread_private_dcontext();
        if (!get_memory_info(new_start, NULL, NULL, &prot)) {
            RELEASE_LOG(GLOBAL, LOG_VMAREAS, 1,
                        "DGC: Failed to get memory protection info for "PFX" +0x%x\n",
                        new_start, new_size);
            return false;
        }
        setup_double_mapping(dcontext, new_start, new_size, prot);
        return true;
    }
    return false;
}

bool
clear_double_mapping(app_pc start)
{
    uint i, j;
    bool removed = false;
#ifdef DEBUG
    dcontext_t *dcontext = get_thread_private_dcontext();
#endif

    mutex_lock(&dgc_mapping_lock);
    for (i = 0; i < double_mappings->index; i++) {
        if (double_mappings->mappings[i].app_memory_start == start)
            break;
    }
    if (i < double_mappings->index) {
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "clear_double_mapping "PFX"-"PFX"\n",
                    start, start + double_mappings->mappings[i].size);
        removed = true;
        remove_dgc_writer_offsets(start, double_mappings->mappings[i].size);
        free_double_mapping(&double_mappings->mappings[i]);
        double_mappings->index--;
        for (j = i; j < double_mappings->index; j++)
            double_mappings->mappings[j] = double_mappings->mappings[j+1];

        for (i = 0; i < exec_area_counters->size; i++) {
            if (exec_area_counters->counters[i].start == start)
                break;
        }
        if (i < exec_area_counters->size) {
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "clear_double_mapping: removing "
                        "exec_area_counter at "PFX"\n", start);
            exec_area_counters->size--;
            for (j = i; j < exec_area_counters->size; j++)
                exec_area_counters->counters[j] = exec_area_counters->counters[j+1];
        }
    } else {
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1,
                    "clear_double_mapping("PFX") failed to locate the mapping\n", start);
    }
    mutex_unlock(&dgc_mapping_lock);
    return removed;
}

static emulation_plan_t *
create_emulation_plan(dcontext_t *dcontext, app_pc writer_app_pc, bool is_jit_self_write)
{
    opnd_t src;
    emulation_plan_t *plan;

    RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1,
                "DGC:    Creating emulation plan for writer "PFX"\n", writer_app_pc);

    plan = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, emulation_plan_t, ACCT_OTHER, UNPROTECTED);

    plan->writer_pc = writer_app_pc;
    plan->is_jit_self_write = is_jit_self_write;
    instr_init(dcontext, &plan->writer);
    plan->resume_pc = decode(dcontext, writer_app_pc, &plan->writer); // assume readable (already decoded)
    if (!instr_valid(&plan->writer)) {
        plan->resume_pc = NULL;
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 0,
                    "DGC: Failed to decode writer at "PFX"\n", writer_app_pc);
        goto instrumentation_failure;
    }
    switch (instr_get_opcode(&plan->writer)) {
    case OP_mov_st:
    case OP_movdqu:
    case OP_movdqa:
    case OP_movups:
    case OP_movaps:
        plan->op = EMUL_MOV;
        break;
    case OP_or:
        plan->op = EMUL_OR;
        break;
    case OP_xor:
        plan->op = EMUL_XOR;
        break;
    case OP_and:
        plan->op = EMUL_AND;
        break;
    case OP_add:
        plan->op = EMUL_ADD;
        break;
    case OP_sub:
        plan->op = EMUL_SUB;
        break;
    case OP_nop_modrm:
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Warning: skipping instrumentation of opcode 0x%x.\n",
                    instr_get_opcode(&plan->writer));
        plan->resume_pc = NULL;
        goto instrumentation_failure;
    default:
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Failed to instrument opcode 0x%x.\n",
                    instr_get_opcode(&plan->writer));
        ASSERT(false);
        plan->resume_pc = NULL;
        goto instrumentation_failure;
    }

    src = instr_get_src(&plan->writer, 0);
    plan->dst = instr_get_dst(&plan->writer, 0);
    plan->dst_size = opnd_size_in_bytes(opnd_get_size(plan->dst));

    if (!(opnd_is_base_disp(plan->dst) || opnd_is_abs_addr(plan->dst)
          IF_X64( || opnd_is_rel_addr(plan->dst)))) {
        ASSERT(false);
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 0,
                    "DGC: Error! Unsupported writer operand kind 0x%x\n",
                    plan->writer.src0.kind);
        plan->resume_pc = NULL;
        goto instrumentation_failure;
    }

    ASSERT((plan->op != EMUL_OR && plan->op != EMUL_XOR && plan->op != EMUL_AND &&
            plan->op != EMUL_ADD && plan->op != EMUL_SUB)
           || plan->dst_size == 1 || plan->dst_size == 4 || plan->dst_size == 8);
    ASSERT(opnd_is_memory_reference(plan->dst));
    if (plan->dst_size < 1 || plan->dst_size > 16 || (plan->dst_size > 2 && plan->dst_size % 4 != 0)) {
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 0,
                    "DGC: Failed to instrument instruction with opcode 0x%x and dst size %d\n",
                    instr_get_opcode(&plan->writer), plan->dst_size);
        ASSERT(false);
        plan->resume_pc = NULL;
        goto instrumentation_failure;
    }

    if (opnd_is_reg(src)) {
        plan->src.mcontext_reg_offset = opnd_get_reg_mcontext_offs(opnd_get_reg(src));
        plan->src_in_reg = true;
    } else if (opnd_is_immed_int(src)) {
        plan->src.immediate = (reg_t) opnd_get_immed_int(src);
        plan->src_in_reg = false;
    } else {
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 0,
                    "DGC: Failed to instrument instruction with opcode 0x%x and unsupported src operand type\n",
                    instr_get_opcode(&plan->writer));
        plan->resume_pc = NULL;
    }

instrumentation_failure:
    if (plan->resume_pc == NULL) {
        free_emulation_plan(plan);
        return NULL;
    } else {
        generic_hash_add(GLOBAL_DCONTEXT, emulation_plans, (ptr_uint_t) writer_app_pc, plan);
        return plan;
    }
}

static inline void
remove_from_all_threads(fragment_t *f)
{
    uint i;
    dcontext_t *dc;
    per_thread_t *pt;
    ibl_branch_type_t branch_type;
    bool remove_trace_from_all_threads = false;
    bool remove_bb_from_all_threads = false;

    if (IS_IBL_TARGET(f->flags)) {
        if (TEST(FRAG_IS_TRACE, f->flags))
            remove_trace_from_all_threads = !DYNAMO_OPTION(shared_trace_ibt_tables);
        if (DYNAMO_OPTION(bb_ibl_targets) &&
            (!TEST(FRAG_IS_TRACE, f->flags) ||
             DYNAMO_OPTION(bb_ibt_table_includes_traces))) {
            remove_bb_from_all_threads = !DYNAMO_OPTION(shared_bb_ibt_tables);
        }
    }

    if (!remove_trace_from_all_threads && !remove_bb_from_all_threads)
        return;

    for (i=0; i < thread_state->count; i++) {
        dc = thread_state->threads[i]->dcontext;
        pt = (per_thread_t *) dc->fragment_field;
        if (remove_trace_from_all_threads) {
            for (branch_type = IBL_BRANCH_TYPE_START;
                 branch_type < IBL_BRANCH_TYPE_END; branch_type++) {
                fragment_prepare_for_removal_from_table(dc, f,
                                                        &pt->trace_ibt[branch_type]);
            }
        }
        if (remove_bb_from_all_threads) {
            for (branch_type = IBL_BRANCH_TYPE_START;
                 branch_type < IBL_BRANCH_TYPE_END; branch_type++) {
                fragment_prepare_for_removal_from_table(dc, f,
                                                        &pt->bb_ibt[branch_type]);
            }
        }
    }
}

app_pc
instrument_dgc_writer(dcontext_t *dcontext, priv_mcontext_t *mc, fragment_t *f, app_pc writer_app_pc,
                      app_pc write_target, size_t write_size, uint prot, bool is_jit_self_write)
{
    emulation_plan_t *plan;
    ptr_int_t offset = 0;
    bool created_plan = false;
    extern bool verbose;

#ifdef RELEASE_LOGGING
    RSTATS_INC(app_managed_instrumentations);
#endif

    TABLE_RWLOCK(emulation_plans, write, lock);
    plan = generic_hash_lookup(GLOBAL_DCONTEXT, emulation_plans, (ptr_uint_t) writer_app_pc);
    if (plan == NULL) {
        plan = create_emulation_plan(dcontext, writer_app_pc, is_jit_self_write);
        created_plan = true;
    }
    TABLE_RWLOCK(emulation_plans, write, unlock);

    if (created_plan && verbose)
        disassemble_app_bb(dcontext, writer_app_pc, STDERR);

    if (plan == NULL) {
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1,
                    "DGC: Skipping instrumentation of "PFX"\n", writer_app_pc);
        return NULL;
    }

    ASSERT(plan->resume_pc > writer_app_pc);

    mutex_lock(&dgc_mapping_lock);
    offset = lookup_dgc_writer_offset(write_target);
    if (offset == 1) // readonly marker
        offset = 0;
    mutex_unlock(&dgc_mapping_lock);

    RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1,
                "DGC: Emulating write "PFX" -> "PFX" (offset "PFX") via page fault\n",
                writer_app_pc, write_target, offset);

    ASSERT(offset != 0);
    if (offset == 0) {
        RELEASE_LOG(GLOBAL, LOG_VMAREAS, 0,
                    "Error! Mapping is gone at "PFX"! Created plan? %d\n", write_target, created_plan);
        return NULL;
    }

    /* TODO: can't we just go back in the code cache via dispatch to rebuild the
     * fragment starting at the faulting write? */
    emulate_writer(mc, plan, offset, write_target, false/*simulate*/);
    if (!is_jit_self_write)
        flush_jit_fragments(write_target, plan->dst_size);

    if (TEST(FRAG_CANNOT_DELETE, f->flags))
        return plan->resume_pc;

    if (TEST(FRAG_SHARED, f->flags)) {
        enter_couldbelinking(dcontext, NULL, false);
        if (!TEST(FRAG_LINKED_INCOMING, f->flags)) {
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1,
                        "DGC: warning: add_to_lazy_deletion_list("PFX") (0x%x) "
                        "without unlinking incoming (not linked, supposedly)\n",
                        f->tag, f->flags);
        }
        if (safe_delete_shared_fragment(dcontext, f)) {
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1,
                        "DGC: add_to_lazy_deletion_list("PFX"/"PFX") (0x%x) for future instrumentation\n",
                        f->tag, f->start_pc, f->flags);

            enter_nolinking(dcontext, NULL, false);

            mutex_lock(&thread_initexit_lock);
            update_thread_state();
            remove_from_all_threads(f);
            mutex_unlock(&thread_initexit_lock);

            // TODO: squash trace if in construction

            enter_couldbelinking(dcontext, NULL, false);
            add_to_lazy_deletion_list(dcontext, f);
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1,
                        "DGC: add_to_lazy_deletion_list("PFX") (0x%x) done\n",
                        f->tag, f->flags);
        } else {
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 0,
                        "DGC: Warning: failed to delete shared fragment "PFX" (0x%x) for future instrumentation\n",
                        f->tag, f->flags);
        }
        enter_nolinking(dcontext, NULL, false);
    } else {
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1,
                    "DGC: Deleting private fragment "PFX" (0x%x) for future instrumentation\n",
                    f->tag, f->flags);
        safe_delete_fragment(dcontext, f);
    }

    return plan->resume_pc;
}

void
emulate_dgc_write(app_pc writer_pc)
{
    emulation_plan_t *plan;
    dcontext_t *dcontext = get_thread_private_dcontext();
    priv_mcontext_t *mc = get_priv_mcontext_from_dstack(dcontext);
    app_pc write_target;
    bool simulating =
#ifdef JITOPT_EMULATE
    false;
#else
    true;
#endif

#ifdef RELEASE_LOGGING
    RSTATS_INC(app_managed_clean_calls);
#endif

    TABLE_RWLOCK(emulation_plans, read, lock);
    plan = generic_hash_lookup(GLOBAL_DCONTEXT, emulation_plans, (ptr_uint_t) writer_pc);
    TABLE_RWLOCK(emulation_plans, read, unlock);

    ASSERT(plan != NULL);
    if (plan == NULL) {
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Error! Cannot find emulation plan "
                    "for DGC writer at "PFX"\n", writer_pc);
        return;
    }

    write_target = opnd_compute_address_priv(plan->dst, mc);

    if (!simulating) {
        ptr_int_t offset = lookup_dgc_writer_offset(write_target);
        if (offset == 0 || offset == 1) {
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: No double-mapping "
                        "for DGC write "PFX" -> "PFX" via clean call\n",
                        writer_pc, write_target);
        }

        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1,
                    "DGC: %s write "PFX" -> "PFX" via clean call\n",
                    simulating ? "Simulating" : "Emulating",
                    writer_pc, write_target);

        /* TODO: can't we just go back in the code cache after flushing? */
        emulate_writer(mc, plan, offset, write_target, simulating);
    }

#ifndef JITOPT_EMULATE
    ASSERT(!plan->is_jit_self_write);
#endif
    if (!plan->is_jit_self_write)
        flush_jit_fragments(write_target, plan->dst_size);
}

bool
apply_dgc_emulation_plan(dcontext_t *dcontext, OUT app_pc *pc, OUT instr_t **instr)
{
    emulation_plan_t *plan;
    instr_t *label;
    dr_instr_label_data_t *label_data;

#ifdef JITOPT_PAGE_FAULT
    if (true)
        return false;
#endif

    TABLE_RWLOCK(emulation_plans, read, lock);
    plan = generic_hash_lookup(GLOBAL_DCONTEXT, emulation_plans, (ptr_uint_t) *pc);
    TABLE_RWLOCK(emulation_plans, read, unlock);

    if (plan == NULL)
        return false;

    RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1,
                "DGC: Instrumenting clean call for writer at "PFX"\n", *pc);

    // with in-cache offsetting; skip clean call for plan->is_jit_self_write
    label = INSTR_CREATE_label(dcontext);
    label_data = instr_get_label_data_area(label);
    label_data->data[0] = (ptr_uint_t) (void *) plan;
    instr_set_note(label, (void *) DR_NOTE_DGC_OPTIMIZATION);
    instr_set_ok_to_mangle(label, false);

    ASSERT(plan->resume_pc > *pc);

    *instr = label;
    *pc = plan->resume_pc;
    return true;
}

static inline bool
dgc_bb_is_head(dgc_bb_t *bb)
{
    return (ptr_uint_t)bb->span < 0x4000;
}

static inline dgc_bb_t *
dgc_bb_head(dgc_bb_t *bb)
{
    return dgc_bb_is_head(bb) ? bb : bb->head;
}

static inline dgc_trace_t *
dgc_bb_traces(dgc_bb_t *bb)
{
    return dgc_bb_head(bb)->containing_trace_list;
}

static inline app_pc
dgc_bb_start(dgc_bb_t *bb)
{
    return bb->start;
}

static inline app_pc
dgc_bb_end(dgc_bb_t *bb)
{
    dgc_bb_t *head = dgc_bb_head(bb);
    return (app_pc)((ptr_uint_t)head->start + (ptr_uint_t)head->span + 1);
}

#ifdef DEBUG
static inline bb_hash_t
dgc_bb_hash(dgc_bb_t *bb)
{
    dgc_bb_t *head = dgc_bb_head(bb);
    return head->hash;
}
#endif

static inline ptr_uint_t
dgc_bb_start_bucket_id(dgc_bb_t *bb)
{
    return BUCKET_ID(bb->start);
}

static inline ptr_uint_t
dgc_bb_end_bucket_id(dgc_bb_t *bb)
{
    dgc_bb_t *head = dgc_bb_head(bb);
    return BUCKET_ID((ptr_uint_t)head->start + (ptr_uint_t)head->span);
}

static inline dgc_bucket_t *
dgc_get_containing_bucket(dgc_bb_t *bb)
{
    if (*(uint *)(bb + 1) == BUCKET_OFFSET_SENTINEL)
        return (dgc_bucket_t *)((ptr_uint_t)(bb - 2) - sizeof(asmtable_entry_t));
    if (*(uint *)(bb + 2) == BUCKET_OFFSET_SENTINEL)
        return (dgc_bucket_t *)((ptr_uint_t)(bb - 1) - sizeof(asmtable_entry_t));
    if (*(uint *)(bb + 3) == BUCKET_OFFSET_SENTINEL)
        return (dgc_bucket_t *)((ptr_uint_t)bb - sizeof(asmtable_entry_t));
    ASSERT(false);
    return NULL;
}

static inline bool
dgc_bb_overlaps(dgc_bb_t *bb, app_pc start, app_pc end)
{
    dgc_bb_t *head = dgc_bb_head(bb);
    ptr_uint_t bb_end = (ptr_uint_t) dgc_bb_end(head);
    return (ptr_uint_t)head->start < (ptr_uint_t)end && bb_end > (ptr_uint_t)start;
}
#endif /* JITOPT */

static void
dgc_stat_report()
{
    RELEASE_LOG(GLOBAL, LOG_ANNOTATIONS, 1, " |   ==== DGC Stats ====\n");
    DGC_REPORT_ONE_STAT(app_managed_writes_observed);
    DGC_REPORT_ONE_STAT(non_app_managed_writes_observed);
    DGC_REPORT_ONE_STAT(app_managed_page_writes);
    DGC_REPORT_ONE_STAT(app_managed_multipage_writes);
#ifdef JITOPT
    DGC_REPORT_ONE_STAT(app_managed_writes_ignored);
    DGC_REPORT_ONE_STAT(app_managed_writes_handled);
    DGC_REPORT_ONE_STAT(app_managed_fragments_removed);
    DGC_REPORT_ONE_STAT(app_managed_micro_writes);
    DGC_REPORT_ONE_STAT(app_managed_cti_target_writes);
    DGC_REPORT_ONE_STAT(app_managed_word_writes);
    DGC_REPORT_ONE_STAT(app_managed_small_writes);
    DGC_REPORT_ONE_STAT(app_managed_subpage_writes);
    DGC_REPORT_ONE_STAT(app_managed_bb_buckets_allocated);
    DGC_REPORT_ONE_STAT(app_managed_bb_buckets_freed);
    DGC_REPORT_ONE_STAT(app_managed_bb_buckets_live);
    DGC_REPORT_ONE_STAT(app_managed_trace_buckets_allocated);
    DGC_REPORT_ONE_STAT(app_managed_trace_buckets_freed);
    DGC_REPORT_ONE_STAT(app_managed_trace_buckets_live);
    DGC_REPORT_ONE_STAT(app_managed_bb_count);
    DGC_REPORT_ONE_STAT(app_managed_small_bb_count);
    DGC_REPORT_ONE_STAT(app_managed_large_bb_count);
    DGC_REPORT_ONE_STAT(app_managed_bb_bytes);
    DGC_REPORT_ONE_STAT(app_managed_one_bucket_bbs);
    DGC_REPORT_ONE_STAT(app_managed_two_bucket_bbs);
    DGC_REPORT_ONE_STAT(app_managed_many_bucket_bbs);
    DGC_REPORT_ONE_STAT(app_managed_direct_links)
    DGC_REPORT_ONE_STAT(app_managed_indirect_links)
    DGC_REPORT_ONE_STAT(app_managed_micro_flush_no_bucket)
    DGC_REPORT_ONE_STAT(app_managed_clean_calls)
    DGC_REPORT_ONE_STAT(app_managed_instrumentations)
    DGC_REPORT_ONE_STAT(direct_linked_bb_removed)
    DGC_REPORT_ONE_STAT(indirect_linked_bb_removed)
    DGC_REPORT_ONE_STAT(special_linked_bb_removed)
    DGC_REPORT_ONE_STAT(direct_linked_bb_cti_tweaked)
    DGC_REPORT_ONE_STAT(direct_linked_bb_tweaked)
    DGC_REPORT_ONE_STAT(indirect_linked_bb_cti_tweaked)
    DGC_REPORT_ONE_STAT(indirect_linked_bb_tweaked)
    DGC_REPORT_ONE_STAT(special_linked_bb_cti_tweaked)
    DGC_REPORT_ONE_STAT(special_linked_bb_tweaked)
    DGC_REPORT_ONE_STAT(max_incoming_direct_linkstubs)
#endif
}

#ifdef JITOPT
static inline app_pc
maybe_exit_cti_disp_pc(app_pc maybe_branch_pc)
{
    app_pc byte_ptr = maybe_branch_pc;
    byte opcode = *byte_ptr;
    uint length = 0;

    if (opcode == RAW_PREFIX_jcc_taken || opcode == RAW_PREFIX_jcc_not_taken) {
        length++;
        byte_ptr++;
        opcode = *byte_ptr;
        /* branch hints are only valid with jcc instrs, and if present on
         * other ctis we strip them out during mangling (i#435)
         */
        if (opcode != RAW_OPCODE_jcc_byte1)
            return NULL;
    }
    if (opcode == ADDR_PREFIX_OPCODE) { /* used w/ jecxz/loop* */
        length++;
        byte_ptr++;
        opcode = *byte_ptr;
    }

    if (opcode >= RAW_OPCODE_loop_start && opcode <= RAW_OPCODE_loop_end) {
        /* assume that this is a mangled jcxz/loop*
         * target pc is in last 4 bytes of "9-byte instruction"
         */
        length += CTI_SHORT_REWRITE_LENGTH;
    } else if (opcode == RAW_OPCODE_jcc_byte1) {
        /* 2-byte opcode, 6-byte instruction, except for branch hint */
        if (*(byte_ptr+1) < RAW_OPCODE_jcc_byte2_start ||
            *(byte_ptr+1) > RAW_OPCODE_jcc_byte2_end)
            return NULL;
        length += CBR_LONG_LENGTH;
    } else {
        /* 1-byte opcode, 5-byte instruction */
        if (opcode != RAW_OPCODE_jmp && opcode != RAW_OPCODE_call)
            return NULL;
        length += JMP_LONG_LENGTH;
    }
    return maybe_branch_pc + length - 4; /* disp is 4 even on x64 */
}

#ifdef CHECK_STALE_BBS
static void
check_stale_bbs()
{
    ptr_uint_t key;
    void *bucket_void;
    dgc_bucket_t *bucket;
    dgc_bb_t *bb;
    int iter;
    uint i, j;
    dcontext_t *tgt_dcontext;

    mutex_lock(&thread_initexit_lock);
    update_thread_state();
    TABLE_RWLOCK(dgc_table, read, lock);
    iter = 0;
    do {
        iter = generic_hash_iterate_next(GLOBAL_DCONTEXT, dgc_table,
                                         iter, &key, &bucket_void);
        if (iter < 0)
            break;

        bucket = (dgc_bucket_t *) bucket_void;
        for (i=0; i < thread_state->count; i++) {
            tgt_dcontext = thread_state->threads[i]->dcontext;
            while (bucket != NULL) {
                for (j = 0; j < BUCKET_BBS; j++) {
                    bb = &bucket->blocks[j];
                    if (bb->start != NULL && dgc_bb_is_head(bb) &&
                        fragment_lookup(tgt_dcontext, bb->start) == NULL) {
                        RELEASE_LOG(GLOBAL, LOG_FRAGMENT, 1,
                                    "DGC: stale bb "PFX" found in scan\n", bb->start);
                    }
                }
                bucket = bucket->chain;
            }
        }
    } while (true);
    TABLE_RWLOCK(dgc_table, read, unlock);
    mutex_unlock(&thread_initexit_lock);
}
#endif

/*
static uint
dgc_change_ref_count(dgc_bucket_t *bucket, uint i, int delta)
{
    uint shift = (i * DGC_REF_COUNT_BITS);
    uint mask = DGC_REF_COUNT_MASK << shift;
    int ref_count = (bucket->ref_counts & mask) >> shift;
    ref_count += delta;
    ASSERT(i < BUCKET_BBS);
    ASSERT(ref_count >= 0);
    if (ref_count > 0x2ff) {
        LOG(GLOBAL, LOG_FRAGMENT, 1, "DGC: 0x%x refs to bb "PFX"\n",
            ref_count, bucket->blocks[i].start);
    }
    ASSERT(ref_count < DGC_REF_COUNT_MASK);
    bucket->ref_counts &= ~mask;
    bucket->ref_counts |= (ref_count << shift);
    return ref_count;
}

static void
dgc_set_ref_count(dgc_bucket_t *bucket, uint i, uint value)
{
    uint shift = (i * DGC_REF_COUNT_BITS);
    uint mask = DGC_REF_COUNT_MASK << shift;
    ASSERT(value == 1);
    bucket->ref_counts &= ~mask;
    bucket->ref_counts |= (value << shift);
}
*/

static dgc_bb_t *
dgc_table_find_bb(app_pc tag, dgc_bucket_t **out_bucket, uint *out_i)
{
    uint i;
    ptr_uint_t bucket_id = BUCKET_ID(tag);
    dgc_bucket_t *bucket = (dgc_bucket_t *)asmtable_lookup(dgc_table, bucket_id);
    // assert table lock
    while (bucket != NULL) {
        for (i = 0; i < BUCKET_BBS; i++) {
            if (bucket->blocks[i].start == tag) {
                if (out_bucket != NULL)
                    *out_bucket = bucket;
                if (out_i != NULL)
                    *out_i = i;
                return &bucket->blocks[i];
            }
        }
        bucket = bucket->chain;
    }
    return NULL;
}

static void
free_dgc_traces(dgc_bb_t *bb) {
    dgc_trace_t *next_trace, *trace = bb->containing_trace_list;
    ASSERT(bb->start == NULL);
    ASSERT(dgc_bb_head(bb)->start == NULL);
    while (trace != NULL) {
        next_trace = trace->next_trace;
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, trace, dgc_trace_t, ACCT_OTHER, UNPROTECTED);
#ifdef RELEASE_LOGGING
        RSTATS_DEC(app_managed_trace_buckets_live);
        RSTATS_INC(app_managed_trace_buckets_freed);
#endif
        trace = next_trace;
    }
}

static void
dgc_table_bucket_gc(dgc_bucket_t *bucket)
{
    if (bucket != NULL) {
        uint i;
        bool is_empty, all_empty = true;
        ptr_uint_t bucket_id = bucket->bucket_id;
        dgc_bucket_t *anchor = NULL;
        do {
            RELEASE_ASSERT(bucket->offset_sentinel == BUCKET_OFFSET_SENTINEL, "Freed already?");
            is_empty = true;
            for (i = 0; i < BUCKET_BBS; i++) {
                if (bucket->blocks[i].start != NULL) {
                    is_empty = false;
                    break;
                }
            }
            if (is_empty) {
                if (anchor == NULL) {
                    dgc_bucket_t *walk;
                    if (bucket->chain == NULL)
                        break;
                    anchor = bucket->chain;
                    bucket->chain = NULL;
                    asmtable_remove(dgc_table, bucket->bucket_id);
                    RELEASE_ASSERT(anchor->offset_sentinel == BUCKET_OFFSET_SENTINEL, "Freed already?");
                    asmtable_insert(dgc_table, (asmtable_entry_t *)anchor);
                    walk = anchor;
                    do {
                        walk->head = anchor;
                        walk = walk->chain;
                    } while (walk != NULL);
                    bucket = anchor;
                    anchor = NULL;
                    /* do not advance to the next bucket--this one has not been checked */
                } else {
                    anchor->chain = bucket->chain;
                    RELEASE_ASSERT(bucket != bucket->head, "Freeing the head bucket w/o removing it!\n");
                    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, bucket, dgc_bucket_t,
                                   ACCT_OTHER, UNPROTECTED);
#ifdef RELEASE_LOGGING
                    RSTATS_DEC(app_managed_bb_buckets_live);
                    RSTATS_INC(app_managed_bb_buckets_freed);
#endif
                    bucket = anchor->chain;
                }
            } else {
                all_empty = false;
                anchor = bucket;
                bucket = bucket->chain;
            }
        } while (bucket != NULL);
        if (all_empty)
            asmtable_remove(dgc_table, bucket_id);
    }
}

static inline void
dgc_bucket_gc_list_init(const char *current_operation)
{
    dgc_bucket_gc_list->staging_count = 0;
    dgc_bucket_gc_list->removal_count = 0;
    dgc_bucket_gc_list->current_operation = current_operation;
}

#ifdef DEBUG
static bool
dgc_bucket_is_packed(dgc_bucket_t *bucket)
{
    uint i;
    bool packed = false;

    if ((ptr_uint_t)bucket == 0xcdcdcdcdUL ||
        bucket->offset_sentinel != BUCKET_OFFSET_SENTINEL) { // freed
        return true;
    }

    while (bucket != NULL) {
        for (i = 0; i < BUCKET_BBS; i++) {
            if (bucket->blocks[i].start != NULL) {
                packed = true;
                break;
            }
        }
        if (!packed)
            return false;
        bucket = bucket->chain;
    }
    return true;
}
#endif

static void
dgc_bucket_gc()
{
    uint i, j;
    for (i = 0; i < dgc_bucket_gc_list->removal_count; i++) {
        for (j = 0; j < dgc_bucket_gc_list->staging_count; j++) {
            if (dgc_bucket_gc_list->staging[j] == dgc_bucket_gc_list->removals[i]) {
                dgc_bucket_gc_list->staging[j] = NULL;
                break;
            }
        }
        asmtable_remove(dgc_table, dgc_bucket_gc_list->removals[i]->bucket_id);
    }

    for (i = 0; i < dgc_bucket_gc_list->staging_count; i++) {
        if (dgc_bucket_gc_list->staging[i] != NULL)
            dgc_table_bucket_gc(dgc_bucket_gc_list->staging[i]);
    }

    for (i = 0; i < dgc_bucket_gc_list->staging_count; i++) {
        if (dgc_bucket_gc_list->staging[i] != NULL)
            ASSERT(dgc_bucket_is_packed(dgc_bucket_gc_list->staging[i]));
    }
}

static void
dgc_stage_bucket_for_gc(dgc_bucket_t *bucket)
{
    uint i;
    bool found = false;
    if (bucket == NULL)
        return;
    RELEASE_ASSERT(bucket->offset_sentinel == BUCKET_OFFSET_SENTINEL, "Freed already?");
    RELEASE_ASSERT(bucket == bucket->head, "No!");
    for (i = 0; i < dgc_bucket_gc_list->staging_count; i++) {
        if (dgc_bucket_gc_list->staging[i]->bucket_id == bucket->bucket_id) {
            found = true;
            break;
        }
    }
    if (!found) {
        if (i >= (dgc_bucket_gc_list->max_staging-1)) {
            EXPAND_ARRAY(dgc_bucket_gc_list->staging,
                         dgc_bucket_gc_list->max_staging, dgc_bucket_t *);
            EXPAND_ARRAY(dgc_bucket_gc_list->removals,
                         dgc_bucket_gc_list->max_removals, dgc_bucket_t *);
            //RELEASE_ASSERT(false, "GC staging list too full (%d) during %s",
            //               i, dgc_bucket_gc_list->current_operation);
        }
        dgc_bucket_gc_list->staging[dgc_bucket_gc_list->staging_count++] = bucket;
    }
}

static inline void
dgc_stage_bucket_id_for_gc(ptr_uint_t bucket_id)
{
    dgc_stage_bucket_for_gc(asmtable_lookup(dgc_table, bucket_id));
}

static void
dgc_set_all_slots_empty(dgc_bb_t *bb)
{
    dgc_bb_t *next_bb;
    dgc_bucket_t *bucket;
    if (bb->start == NULL)
        return; // already gc'd
    bb = dgc_bb_head(bb);
    bb->start = NULL;
    free_dgc_traces(bb);
    do {
        next_bb = bb->next;
        bucket = dgc_get_containing_bucket(bb)->head;
        dgc_stage_bucket_for_gc(bucket);
        bb->start = NULL;
        ASSERT(dgc_bb_head(bb)->start == NULL);
        DODEBUG(bb->span = 0;);
        bb = next_bb;
    } while (bb != NULL);
}

static void
dgc_table_resized()
{
    uint i;
    dcontext_t *dc;
    local_state_extended_t *state;

    mutex_lock(&thread_initexit_lock);
    update_thread_state();
    for (i=0; i < thread_state->count; i++) {
        dc = thread_state->threads[i]->dcontext;
        state = (local_state_extended_t *) dc->local_state;
        state->dgc_coverage_table = dgc_table->table; // TODO: race!
        state->dgc_coverage_mask = dgc_table->hash_mask;
    }
    mutex_unlock(&thread_initexit_lock);
}

static void
free_dgc_bucket_chain(void *p)
{
    uint i;
    dgc_bucket_t *next_bucket, *bucket = (dgc_bucket_t *)p;
    ASSERT(bucket->offset_sentinel == BUCKET_OFFSET_SENTINEL);
    do {
        next_bucket = bucket->chain;
        for (i = 0; i < BUCKET_BBS; i++) {
            if (bucket->blocks[i].start != NULL && dgc_bb_is_head(&bucket->blocks[i])) {
                bucket->blocks[i].start = NULL;
                free_dgc_traces(&bucket->blocks[i]);
            }
        }
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, bucket, dgc_bucket_t, ACCT_OTHER, UNPROTECTED);
#ifdef RELEASE_LOGGING
        RSTATS_DEC(app_managed_bb_buckets_live);
        RSTATS_INC(app_managed_bb_buckets_freed);
#endif
        bucket = next_bucket;
    } while (bucket != NULL);
}

static void
free_double_mapping(double_mapping_t *mapping)
{
#ifdef DEBUG
    dcontext_t *dcontext = get_thread_private_dcontext();
#endif

    RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "free_double_mapping of "PFX": "PFX" + 0x%x\n",
                mapping->app_memory_start, mapping->double_mapping_start,
                mapping->double_mapping_size);
    int result = dynamorio_syscall(SYS_munmap, 2, mapping->double_mapping_start,
                                   mapping->double_mapping_size);
    dynamorio_syscall(SYS_close, 1ULL, (ptr_uint_t) mapping->fd);
    if (result < 0) {
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1,
                    "free_double_mapping error: failed to unmap the double-mapping at "PFX"\n",
                    mapping->double_mapping_start);
    }
}

static void
free_emulation_plan(void *p)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    emulation_plan_t *plan = (emulation_plan_t *)p;
    instr_free(dcontext, &plan->writer);
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, plan, emulation_plan_t, ACCT_OTHER, UNPROTECTED);
}

void
dgc_table_dereference_bb(app_pc tag)
{
    dgc_bb_t *bb;
    RELEASE_LOG(GLOBAL, LOG_FRAGMENT, 1, "DGC: dereferencing bb "PFX"\n", tag);
    asmtable_lock(dgc_table);
    bb = dgc_table_find_bb(tag, NULL, NULL);
    if (bb != NULL) {
        bb = dgc_bb_head(bb);
        if ((--bb->ref_count) == 0) {
            dgc_bucket_gc_list_init("dgc_table_dereference_bb");
            dgc_set_all_slots_empty(bb); // _IF_DEBUG("refcount"));
            dgc_bucket_gc();
        } else {
            ASSERT(bb->ref_count >= 0);
        }
    }
    asmtable_unlock(dgc_table);
}

static void
dgc_stage_removal_gc_outliers(ptr_uint_t bucket_id)
{
    uint i;
    bool found = false;
    dgc_bucket_t *bucket = (dgc_bucket_t *)asmtable_lookup(dgc_table, bucket_id);
    if (bucket != NULL) {
        dgc_bucket_t *head_bucket = bucket;
        RELEASE_ASSERT(bucket->offset_sentinel == BUCKET_OFFSET_SENTINEL, "Freed already?");
        RELEASE_ASSERT(bucket == bucket->head, "No!");

        do {
            ASSERT(bucket->offset_sentinel == BUCKET_OFFSET_SENTINEL);
            for (i = 0; i < BUCKET_BBS; i++) {
                if (bucket->blocks[i].start != NULL)
                    dgc_set_all_slots_empty(&bucket->blocks[i]);
            }
            bucket = bucket->chain;
        } while (bucket != NULL);

        bucket = head_bucket;
        for (i = 0; i < dgc_bucket_gc_list->removal_count; i++) {
            if (dgc_bucket_gc_list->removals[i]->bucket_id == bucket->bucket_id) {
                found = true;
                break;
            }
        }
        if (!found) {
            if (i >= (DGC_BUCKET_GC_CAPACITY-1)) {
                RELEASE_ASSERT(false, "GC removal list too full (%d) during %s",
                               i, dgc_bucket_gc_list->current_operation);
            }
            dgc_bucket_gc_list->removals[dgc_bucket_gc_list->removal_count++] = bucket;
        }
    }
}

void
dgc_notify_region_cleared(app_pc start, app_pc end)
{
    ptr_uint_t first_bucket_id = BUCKET_ID(start);
    ptr_uint_t last_bucket_id = BUCKET_ID(end - 1);
    bool is_start_of_bucket = (((ptr_uint_t)start & BUCKET_MASK) == 0);
    bool is_end_of_bucket = (((ptr_uint_t)end & BUCKET_MASK) == 0);
    ptr_uint_t bucket_id = first_bucket_id;

    RELEASE_LOG(GLOBAL, LOG_FRAGMENT, 1, "DGC: clearing ["PFX"-"PFX"]\n", start, end);

    asmtable_lock(dgc_table);
    dgc_bucket_gc_list_init("dgc_notify_region_cleared");
    if (is_start_of_bucket && (is_end_of_bucket || bucket_id < last_bucket_id))
        dgc_stage_removal_gc_outliers(bucket_id);
    else
        dgc_stage_bucket_id_for_gc(bucket_id);
    for (++bucket_id; bucket_id < last_bucket_id; bucket_id++)
        dgc_stage_removal_gc_outliers(bucket_id);
    if (bucket_id == last_bucket_id) {
        if (is_end_of_bucket && (bucket_id > first_bucket_id))
            dgc_stage_removal_gc_outliers(bucket_id);
        else
            dgc_stage_bucket_id_for_gc(bucket_id);
    }
    dgc_bucket_gc();
    asmtable_unlock(dgc_table);

    dgc_stat_report();
}

void
dgc_cache_reset()
{
    asmtable_clear(dgc_table);
}

#ifdef DEBUG
static inline bb_hash_t
hash_bits(uint length, byte *bits) {
    ushort b;
    bb_hash_t hash = 0;

    while (length >= HASH_STEP_SIZE) {
        hash = hash ^ (hash << 5) ^ *(uint *)(bits);
        length -= HASH_STEP_SIZE;
        bits += HASH_STEP_SIZE;
    }
    if (length != 0) {
        uint tail = 0UL;
        for (b = 0; b < length; b++)
            tail |= ((uint)(*(bits + b)) << (b * HASH_STEP_BITS));
        tail = SHIFT_IN_EMPTY_BYTES(tail, length);
        hash = hash ^ (hash << 5) ^ tail;
    }
    return hash;
}
#endif

void
add_patchable_bb(app_pc start, app_pc end, bool is_trace_head)
{
    bool found = false;
    uint i, span = (uint)((end - start) - 1);
    ptr_uint_t bucket_id;
    ptr_uint_t start_bucket_id = BUCKET_ID(start);
    ptr_uint_t end_bucket_id = BUCKET_ID(end - 1);
    dgc_bb_t *bb, *last_bb = NULL, *first_bb = NULL;
    dgc_bucket_t *bucket;
#ifdef DEBUG
    ptr_uint_t hash = hash_bits(span+1, start);
#endif

#ifdef RELEASE_LOGGING
    RELEASE_LOG(GLOBAL, LOG_FRAGMENT, 1, "DGC: add bb ["PFX"-"PFX"]%s\n",
                start, end, is_trace_head ? " (trace head)" : "");
    RSTATS_INC(app_managed_bb_count);
    RSTATS_ADD(app_managed_bb_bytes, end - start);
    if (span < 12)
        RSTATS_INC(app_managed_small_bb_count);
    else if (span > 32)
        RSTATS_INC(app_managed_large_bb_count);
    if (start_bucket_id == end_bucket_id)
        RSTATS_INC(app_managed_one_bucket_bbs);
    else if ((end_bucket_id - start_bucket_id) == 1)
        RSTATS_INC(app_managed_two_bucket_bbs);
    else
        RSTATS_INC(app_managed_many_bucket_bbs);
#endif
    asmtable_lock(dgc_table);
    for (bucket_id = start_bucket_id; bucket_id <= end_bucket_id; bucket_id++) {
        bucket = (dgc_bucket_t *)asmtable_lookup(dgc_table, bucket_id);
        if (bucket == NULL) {
            bucket = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dgc_bucket_t, ACCT_OTHER, UNPROTECTED);
            memset(bucket, 0, sizeof(dgc_bucket_t));
            bucket->bucket_id = bucket_id;
            bucket->offset_sentinel = BUCKET_OFFSET_SENTINEL;
            bucket->head = bucket;
            asmtable_insert(dgc_table, (asmtable_entry_t *)bucket);
            i = 0;
#ifdef RELEASE_LOGGING
            RSTATS_INC(app_managed_bb_buckets_live);
            RSTATS_INC(app_managed_bb_buckets_allocated);
#endif
        } else {
            dgc_bucket_t *head_bucket = bucket, *available_bucket = NULL;
            uint available_slot = 0xff;
            RELEASE_ASSERT(bucket->offset_sentinel == BUCKET_OFFSET_SENTINEL, "Freed already?");
            RELEASE_ASSERT(bucket == bucket->head, "No!");
            while (true) {
                for (i = 0; i < BUCKET_BBS; i++) {
                    bb = &bucket->blocks[i];
                    if (bb->start == start) {
#ifdef DEBUG
                        if (dgc_bb_end(bb) == end &&
                            dgc_bb_hash(bb) == hash) {
#endif
                            found = true;
                            break;
#ifdef DEBUG
                        } else {
                            if (dgc_bb_end(bb) != end) {
                                RELEASE_LOG(GLOBAL, LOG_ANNOTATIONS, 1,
                                            "DGC: stale bb ["PFX"-"PFX"]! Resetting span to %d\n",
                                            start, dgc_bb_end(bb), span);
                                dgc_bb_head(bb)->span = span;
                            }
                            if (dgc_bb_hash(bb) != hash) {
                                RELEASE_LOG(GLOBAL, LOG_ANNOTATIONS, 1,
                                            "DGC: stale bb ["PFX"-"PFX"] has hash "HASH
                                            " but current bb has hash "HASH"!\n",
                                            start, dgc_bb_end(bb),
                                            dgc_bb_hash(bb), hash);
                            }

                            /*
                            LOG(GLOBAL, LOG_FRAGMENT, 1, "DGC: stale bb "
                                PFX"-"PFX"!\n", start, dgc_bb_end(bb));
                            //bucket->blocks[i].start = NULL;
                            dgc_bucket_gc_list_init("add_patchable_bb");
                            dgc_set_all_slots_empty(bb);
                            dgc_bucket_gc();
                            */
                        }
#endif
                    } else if (bb->start != NULL &&
                               IS_INCOMPATIBLE_OVERLAP(start, end, bb->start, dgc_bb_end(bb))) {
                        RELEASE_LOG(GLOBAL, LOG_ANNOTATIONS, 1,
                                    "DGC: stale bb ["PFX"-"PFX"] overlaps new bb ["PFX"-"PFX")!\n",
                                    start, dgc_bb_end(bb), start, end-1);
                    }
                    if (available_bucket == NULL && bb->start == NULL) {
                        available_bucket = bucket;
                        available_slot = i;
                    }
                }
                if (found || bucket->chain == NULL)
                    break;
                bucket = bucket->chain;
            }
            if (found) {
                bucket->blocks[i].ref_count++;
                //dgc_change_ref_count(bucket, i, 1);
                ASSERT(bucket->blocks[i].ref_count > 1);
                ASSERT(bucket->blocks[i].ref_count < 0x10000000);
                ASSERT(first_bb == NULL);
                break;
            }
            if (available_bucket == NULL) {
                dgc_bucket_t *new_bucket = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dgc_bucket_t,
                                                           ACCT_OTHER, UNPROTECTED);
                memset(new_bucket, 0, sizeof(dgc_bucket_t));
                ASSERT(bucket->chain == NULL);
                new_bucket->bucket_id = bucket_id;
                new_bucket->head = head_bucket;
                new_bucket->offset_sentinel = BUCKET_OFFSET_SENTINEL;
                bucket->chain = new_bucket;
                bucket = new_bucket;
                i = 0;
#ifdef RELEASE_LOGGING
                RSTATS_INC(app_managed_bb_buckets_live);
                RSTATS_INC(app_managed_bb_buckets_allocated);
#endif
            } else {
                bucket = available_bucket;
                i = available_slot;
            }
        }
        if (found)
            break;
        bucket->blocks[i].start = start;
        if (first_bb == NULL) {
            first_bb = &bucket->blocks[i];
            first_bb->span = span;
            first_bb->containing_trace_list = NULL;
            first_bb->ref_count = 1;
#ifdef DEBUG
            first_bb->hash = hash;
#endif
            if (span > 0x100) {
                RELEASE_LOG(GLOBAL, LOG_ANNOTATIONS, 1,
                            "DGC: Warning! giant bb ["PFX"-"PFX"] (0x%x)\n", start, end, span);
            }
        } else {
            bucket->blocks[i].head = first_bb;
            last_bb->next = &bucket->blocks[i];
        }
        last_bb = &bucket->blocks[i];
    }
    if (!found)
        last_bb->next = NULL;
    asmtable_unlock(dgc_table);
}

bool
add_patchable_trace(monitor_data_t *md)
{
    bool found_trace = false, added = false;
    dgc_bb_t *bb;
    app_pc bb_tag;
    uint i;

    if (md->num_blks == 1)
        return false;

    asmtable_lock(dgc_table);
#ifdef FULL_TRACE_LOG
    RELEASE_LOG(GLOBAL, LOG_FRAGMENT, 1, "DGC: add trace {"PFX, md->trace_tag);
#endif
    for (i = 1; i < md->num_blks; i++) {
        bb_tag = md->blk_info[i].info.tag;
#ifdef FULL_TRACE_LOG
        RELEASE_LOG(GLOBAL, LOG_FRAGMENT, 1, ", "PFX, bb_tag);
#endif
        bb = dgc_table_find_bb(bb_tag, NULL, NULL);
        if (bb != NULL) {
            added = true;
            found_trace = false;
            dgc_trace_t *trace = bb->containing_trace_list;
            while (trace != NULL) {
                if (trace->tags[0] == md->trace_tag) {
                    found_trace = true;
                    break;
                }
                if (trace->tags[1] == NULL)
                    break;
                if (trace->tags[1] == md->trace_tag) {
                    found_trace = true;
                    break;
                }
                trace = trace->next_trace;
            }
            if (!found_trace) {
                if (trace != NULL) {
                    ASSERT(trace->tags[1] == NULL);
                    trace->tags[1] = md->trace_tag;
                } else {
                    trace = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dgc_trace_t,
                                            ACCT_OTHER, UNPROTECTED);
                    trace->tags[0] = md->trace_tag;
                    trace->tags[1] = NULL;
                    trace->next_trace = bb->containing_trace_list;
                    bb->containing_trace_list = trace;
#ifdef RELEASE_LOGGING
                    RSTATS_INC(app_managed_trace_buckets_live);
                    RSTATS_INC(app_managed_trace_buckets_allocated);
#endif
                }
            }
        }
    }
#ifdef FULL_TRACE_LOG
    RELEASE_LOG(GLOBAL, LOG_FRAGMENT, 1, "}\n");
#endif
    asmtable_unlock(dgc_table);

    return added;
}

void
patchable_bb_linked(dcontext_t *dcontext, fragment_t *f)
{
#define MAX_LINKSTUBS 0x1000
#define LINKSTUB_SAMPLE_INTERVAL 0x40
    if (TEST(FRAG_SHARED, f->flags) && !TEST(FRAG_COARSE_GRAIN, f->flags) &&
        (++dgc_removal_queue->sample_index > LINKSTUB_SAMPLE_INTERVAL)) {
        uint count = 0;
        common_direct_linkstub_t *s, *t;
        asmtable_lock(dgc_table);
        dgc_removal_queue->sample_index = 0;
        s = (common_direct_linkstub_t *)f->in_xlate.incoming_stubs;
        for (; s; s = (common_direct_linkstub_t *)s->next_incoming, count++);
        if (count > MAX_LINKSTUBS) {
            RELEASE_LOG(GLOBAL, LOG_ANNOTATIONS, 1, "Incoming links crowded on "PFX
                        "; removing oldest %d fan-in bbs.\n", f->tag, (count - MAX_LINKSTUBS));
            s = (common_direct_linkstub_t *)f->in_xlate.incoming_stubs;
            while (count > MAX_LINKSTUBS) {
                t = (common_direct_linkstub_t *)s->next_incoming;
                fragment_t *in = linkstub_fragment(dcontext, (linkstub_t *)s);
                dgc_removal_queue->tags[dgc_removal_queue->index++] = in->tag;
                if (dgc_removal_queue->index == dgc_removal_queue->max) {
                    EXPAND_ARRAY(dgc_removal_queue->tags,
                                 dgc_removal_queue->max, app_pc);
                }
                s = t;
                count--;
            }
        }
        asmtable_unlock(dgc_table);
    }
#undef MAX_LINKSTUBS
#undef LINKSTUB_SAMPLE_INTERVAL
}

static bool
safe_delete_shared_fragment(dcontext_t *dcontext, fragment_t *f)
{
    bool deleted = false;
    mutex_lock(&bb_building_lock);
    SHARED_FLAGS_RECURSIVE_LOCK(f->flags, acquire, change_linking_lock);
    acquire_vm_areas_lock(dcontext, f->flags);
    if (TEST(FRAG_WAS_DELETED, f->flags)) {
        RELEASE_LOG(GLOBAL, LOG_ANNOTATIONS, 1, "Warning: duplicate deletion of %s"
                    "app-managed fragment "PFX"\n",
                    TEST(FRAG_APP_MANAGED, f->flags) ? "" : "non-", f->tag);
    } else {
        /* FIXME: share all this code w/ vm_area_unlink_fragments()
         * The work there is just different enough to make that hard, though.
         */
        if (TEST(FRAG_LINKED_OUTGOING, f->flags))
            unlink_fragment_outgoing(GLOBAL_DCONTEXT, f);
        if (TEST(FRAG_LINKED_INCOMING, f->flags))
            unlink_fragment_incoming(GLOBAL_DCONTEXT, f);
        incoming_remove_fragment(GLOBAL_DCONTEXT, f);

        /* remove from ib lookup tables in a safe manner. this removes the
         * frag only from this thread's tables OR from shared tables.
         */
        fragment_prepare_for_removal(GLOBAL_DCONTEXT, f);
        /* fragment_remove ignores the ibl tables for shared fragments */
        fragment_remove(GLOBAL_DCONTEXT, f, false);

        vm_area_remove_fragment(dcontext, f);

        /* case 8419: make marking as deleted atomic w/ fragment_t.also_vmarea field
         * invalidation, so that users of vm_area_add_to_list() can rely on this
         * flag to determine validity
         */
        f->flags |= FRAG_WAS_DELETED;

        if (!TEST(FRAG_HAS_TRANSLATION_INFO, f->flags)) {
            fragment_record_translation_info(dcontext, f, NULL);
        }

        deleted = true;
    }
    release_vm_areas_lock(dcontext, f->flags);
    SHARED_FLAGS_RECURSIVE_LOCK(f->flags, release, change_linking_lock);
    mutex_unlock(&bb_building_lock);
    return deleted;
}

static void
safe_delete_fragment(dcontext_t *dcontext, fragment_t *f)
{
    if (TEST(FRAG_CANNOT_DELETE, f->flags)) {
        RELEASE_LOG(GLOBAL, LOG_ANNOTATIONS, 1,
                    "Warning: Cannot delete fragment "PFX" with flags 0x%x!\n", f->tag, f->flags);
        return;
    }

    if (TEST(FRAG_SHARED, f->flags)) {
        safe_delete_shared_fragment(dcontext, f);

        f->next_vmarea = fragment_intersection->shared_deletion_list;
        fragment_intersection->shared_deletion_list = f;
    } else {
        acquire_vm_areas_lock(dcontext, f->flags);
        fragment_delete(dcontext, f, FRAGDEL_NO_OUTPUT | FRAGDEL_NO_MONITOR |
                                     FRAGDEL_NO_HEAP | FRAGDEL_NO_FCACHE);
        release_vm_areas_lock(dcontext, f->flags);

        f->flags |= FRAG_WAS_DELETED;
        fragment_delete(dcontext, f, FRAGDEL_NO_OUTPUT | FRAGDEL_NO_VMAREA |
                                     FRAGDEL_NO_UNLINK | FRAGDEL_NO_HTABLE);
    }
}

#ifdef RELEASE_LOGGING
static inline void
link_stats(fragment_t *f, bool is_tweak, bool is_cti_tweak)
{
    linkstub_t *l;
    for (l = FRAGMENT_EXIT_STUBS(f); l; l = LINKSTUB_NEXT_EXIT(l)) {
        if (TESTANY(IF_WINDOWS(LINK_CALLBACK_RETURN |)
                    LINK_SPECIAL_EXIT | LINK_NI_SYSCALL, l->flags)) {
            if (is_cti_tweak)
                RSTATS_INC(special_linked_bb_cti_tweaked);
            else if (is_tweak)
                RSTATS_INC(special_linked_bb_tweaked);
            else
                RSTATS_INC(special_linked_bb_removed);
            return;
        }
        if (LINKSTUB_INDIRECT(l->flags)) {
            if (is_cti_tweak)
                RSTATS_INC(indirect_linked_bb_cti_tweaked);
            else if (is_tweak)
                RSTATS_INC(indirect_linked_bb_tweaked);
            else
                RSTATS_INC(indirect_linked_bb_removed);
            return;
        }
    }
    if (is_cti_tweak)
        RSTATS_INC(direct_linked_bb_cti_tweaked);
    else if (is_tweak)
        RSTATS_INC(direct_linked_bb_tweaked);
    else
        RSTATS_INC(direct_linked_bb_removed);
}
#endif

/* Returns true if `f` was deleted and it was a shared fragment */
static inline bool
safe_remove_bb(dcontext_t *dcontext, fragment_t *f
               _IF_RELLOG(bool is_tweak) _IF_RELLOG(bool is_cti_tweak))
{
    if (f != NULL) {
#ifdef RELEASE_LOGGING
        link_stats(f, is_tweak, is_cti_tweak);
#endif
        safe_delete_fragment(dcontext, f);
        return TEST(FRAG_SHARED, f->flags);
    }
    return false;
}

static inline void
safe_remove_trace(dcontext_t *dcontext, trace_t *t)
{
    if (t != NULL) {
        bool found = false;
        uint i;
        for (i = 0; i < t->t.num_bbs; i++) {
            for (app_pc *bb_tag = fragment_intersection->bb_tags; *bb_tag != NULL; bb_tag++) {
                if (t->t.bbs[i].tag == *bb_tag) {
                    found = true;
                    break;
                }
            }
            if (found)
                break;
        }
        if (!found) {
            RELEASE_LOG(GLOBAL, LOG_ANNOTATIONS, 1, "DGC: stale trace "PFX" no longer "
                        "contains any bb in the intersection.\n", t->f.tag);
        } else {
            dgc_bucket_t *bucket;
            dgc_bb_t * bb;

            asmtable_lock(dgc_table); // yuk
            bb = dgc_table_find_bb(t->f.tag, &bucket, NULL);
            asmtable_unlock(dgc_table);
            if (bb != NULL) {
                dgc_set_all_slots_empty(bb);
                dgc_stage_bucket_for_gc(bucket->head);
            }

            RELEASE_LOG(GLOBAL, LOG_ANNOTATIONS, 1, "DGC: removing trace "PFX
                        " for overlap with bb "PFX"\n", t->f.tag, t->t.bbs[i].tag);
#ifdef TRACE_ANALYSIS
            //dr_printf("DGC: removing trace "PFX
            //          " for overlap with bb "PFX"\n", t->f.tag, t->t.bbs[i].tag);
#endif
            safe_delete_fragment(dcontext, (fragment_t *)t);
        }
    }
}

static void
remove_patchable_fragment_list(dcontext_t *dcontext, app_pc patch_start, app_pc patch_end)
{
    int i;
    uint j;
    app_pc *bb_tag, *trace_tag;
    bool thread_has_fragment;
#ifdef RELEASE_LOGGING
    bool is_tweak = ((patch_end - patch_start) <= sizeof(ptr_uint_t));
    bool is_cti_tweak = (is_tweak &&
                         (maybe_exit_cti_disp_pc(patch_start-1) != NULL ||
                          maybe_exit_cti_disp_pc(patch_start-2) != NULL));
#endif
    per_thread_t *tgt_pt;
    dcontext_t *tgt_dcontext;

    for (i=0; i < thread_state->count; i++) {
        tgt_dcontext = thread_state->threads[i]->dcontext;

        thread_has_fragment = false;

        // why doesn't this work??
        //if (!thread_vm_area_overlap(tgt_dcontext, patch_start, patch_end))
        //    continue;

        // TODO: could put the fragments on the fragment_intersection, to avoid
        // looking them up repeatedly.
        for (bb_tag = fragment_intersection->bb_tags; *bb_tag != NULL; bb_tag++) {
            if (fragment_lookup(tgt_dcontext, *bb_tag) != NULL) {
                thread_has_fragment = true;
                break;
            }
        }

        if (!thread_has_fragment) {
            trace_tag = fragment_intersection->trace_tags;
            for (; *trace_tag != NULL; trace_tag++) {
                if (fragment_lookup_trace(tgt_dcontext, *trace_tag) != NULL) {
                    thread_has_fragment = true;
                    break;
                }
            }
        }

        if (!thread_has_fragment)
            continue;

        tgt_pt = (per_thread_t *) tgt_dcontext->fragment_field;

        if (tgt_dcontext != dcontext) {
            mutex_lock(&tgt_pt->linking_lock);
            if (tgt_pt->could_be_linking) {
                /* remember we have a global lock, thread_initexit_lock, so two threads
                 * cannot be here at the same time!
                 */
                LOG(GLOBAL, LOG_FRAGMENT, 1,
                    "\twaiting for thread "TIDFMT"\n", tgt_dcontext->owning_thread);
                tgt_pt->wait_for_unlink = true;
                mutex_unlock(&tgt_pt->linking_lock);
                wait_for_event(tgt_pt->waiting_for_unlink);
                mutex_lock(&tgt_pt->linking_lock);
                tgt_pt->wait_for_unlink = false;
                LOG(GLOBAL, LOG_FRAGMENT, 1,
                    "\tdone waiting for thread "TIDFMT"\n", tgt_dcontext->owning_thread);
            } else {
                LOG(GLOBAL, LOG_FRAGMENT, 1,
                    "\tthread "TIDFMT" synch not required\n", tgt_dcontext->owning_thread);
            }
            mutex_unlock(&tgt_pt->linking_lock);
        }

        if (is_building_trace(tgt_dcontext)) {
            /* not locking b/c a race should at worst abort a valid trace */
            bool clobbered = false;
            monitor_data_t *md = (monitor_data_t *) tgt_dcontext->monitor_field;
            for (j = 0; j < md->blk_info_length && !clobbered; j++) {
                for (bb_tag = fragment_intersection->bb_tags; *bb_tag != NULL; bb_tag++) {
                    if (*bb_tag == md->blk_info[j].info.tag) {
                        clobbered = true;
                        break;
                    }
                }
            }
            if (clobbered) {
                RELEASE_LOG(GLOBAL, LOG_ANNOTATIONS, 1,
                            "Warning! Squashing trace "PFX" because it overlaps removal bb "
                            PFX"\n", md->trace_tag, *bb_tag);
                trace_abort(tgt_dcontext);
            }
        }
        //if (DYNAMO_OPTION(syscalls_synch_flush) && get_at_syscall(tgt_dcontext)) {
#ifdef CLIENT_INTERFACE
            //RELEASE_LOG("Warning! Thread is at syscall while removing frags from that thread.\n");
#endif
            // does this matter??
            /* we have to know exactly which threads were at_syscall here when
             * we get to post-flush, so we cache in this special bool
             */
        //}

        for (bb_tag = fragment_intersection->bb_tags; *bb_tag != NULL; bb_tag++) {
            safe_remove_trace(tgt_dcontext, (trace_t *)fragment_lookup_trace(tgt_dcontext,
                              *bb_tag));
            if (!safe_remove_bb(tgt_dcontext, fragment_lookup_bb(tgt_dcontext, *bb_tag)
                           _IF_RELLOG(is_tweak) _IF_RELLOG(is_cti_tweak))) {
                safe_remove_bb(tgt_dcontext, fragment_lookup_shared_bb(tgt_dcontext, *bb_tag)
                               _IF_RELLOG(is_tweak) _IF_RELLOG(is_cti_tweak));
            }
        }
        trace_tag = fragment_intersection->trace_tags;
        for (; *trace_tag != NULL; trace_tag++) {
            safe_remove_trace(tgt_dcontext, (trace_t *)fragment_lookup_trace(tgt_dcontext,
                              *trace_tag));
        }

        if (tgt_dcontext != dcontext) {
            tgt_pt = (per_thread_t *) tgt_dcontext->fragment_field;
            mutex_lock(&tgt_pt->linking_lock);
            if (tgt_pt->could_be_linking) {
                signal_event(tgt_pt->finished_with_unlink);
            } else {
                /* we don't need to wait on a !could_be_linking thread
                 * so we use this bool to tell whether we should signal
                 * the event.
                 */
                if (tgt_pt->soon_to_be_linking)
                    signal_event(tgt_pt->finished_all_unlink);
            }
            mutex_unlock(&tgt_pt->linking_lock);
        }
    }
}

static void
update_thread_state()
{
    uint thread_state_version = get_thread_state_version();
    if (thread_state->threads == NULL) {
        thread_state->version = thread_state_version;
        get_list_of_threads(&thread_state->threads, &thread_state->count);
    } else if (thread_state_version != thread_state->version) {
        thread_state->version = thread_state_version;
        global_heap_free(thread_state->threads,
                         thread_state->count*sizeof(thread_record_t*)
                         HEAPACCT(ACCT_THREAD_MGT));
        get_list_of_threads(&thread_state->threads, &thread_state->count);
    }
}

static inline bool
has_tag(app_pc tag, app_pc *tags, uint count)
{
    uint i;
    for (i = 0; i < count; i++) {
        if (tags[i] == tag)
            return true;
    }
    return false;
}

static bool
buckets_exist_in_range(ptr_uint_t start, ptr_uint_t end)
{
    ptr_uint_t i;
    for (i = start; i < end; i++) {
        if (asmtable_lookup(dgc_table, i) != NULL)
            return true;
    }
    return false;
}

static uint
add_trace_intersection(dgc_trace_t *trace, uint i)
{
    if (!has_tag(trace->tags[0], fragment_intersection->trace_tags, i))
        fragment_intersection->trace_tags[i++] = trace->tags[0];
    if (i == fragment_intersection->trace_tag_max) {
        EXPAND_ARRAY(fragment_intersection->trace_tags,
                     fragment_intersection->trace_tag_max, app_pc);
    }
    if (trace->tags[1] != NULL && !has_tag(trace->tags[1],
                                           fragment_intersection->trace_tags, i))
        fragment_intersection->trace_tags[i++] = trace->tags[1];
    if (i == fragment_intersection->trace_tag_max) {
        EXPAND_ARRAY(fragment_intersection->trace_tags,
                     fragment_intersection->trace_tag_max, app_pc);
    }
    return i;
}

uint
extract_fragment_intersection(app_pc patch_start, app_pc patch_end)
{
    ptr_uint_t bucket_id;
    ptr_uint_t start_bucket = BUCKET_ID(patch_start);
    ptr_uint_t end_bucket = BUCKET_ID(patch_end - 1);
    bool is_patch_start_bucket = true;
    DEBUG_DECLARE(bool found = false;)
    uint i, i_bb = 0, i_trace = 0;
    uint fragment_total = 0;
    dgc_bucket_t *bucket;
    dgc_trace_t *trace;
    dgc_bb_t *bb;

    asmtable_lock(dgc_table);
    dgc_bucket_gc_list_init("remove_patchable_fragments");
    for (bucket_id = start_bucket; bucket_id <= end_bucket; bucket_id++) {
        bucket = (dgc_bucket_t *)asmtable_lookup(dgc_table, bucket_id);

#ifdef RELEASE_LOGGING
        // logging only
        if (bucket == NULL && start_bucket == end_bucket && (patch_end - patch_start) <= 8)
            RSTATS_INC(app_managed_micro_flush_no_bucket);
#endif

        while (bucket != NULL) {
            for (i = 0; i < BUCKET_BBS; i++) {
                bb = &bucket->blocks[i];
                if (bb->start != NULL &&
                    dgc_bb_overlaps(bb, patch_start, patch_end) &&
                    (is_patch_start_bucket || dgc_bb_is_head(bb))) {
                    if (!has_tag(bb->start, fragment_intersection->bb_tags, i_bb)) {
                        fragment_intersection->bb_tags[i_bb] = bb->start;

                        RELEASE_LOG(GLOBAL, LOG_FRAGMENT, 1,
                            "DGC: remove bb ["PFX"-"PFX"]:\n",
                            bb->start, dgc_bb_end(bb));

                        //fragment_intersection->bb_spans[i_bb] = bb->span;
                        i_bb++;
                    }
                    if (i_bb == fragment_intersection->bb_tag_max) {
                        EXPAND_ARRAY(fragment_intersection->bb_tags,
                                     fragment_intersection->bb_tag_max, app_pc);
                    }
                    trace = dgc_bb_head(bb)->containing_trace_list;
                    while (trace != NULL) {
                        i_trace = add_trace_intersection(trace, i_trace);
                        trace = trace->next_trace;
                    }
                    dgc_set_all_slots_empty(bb);
                    DODEBUG(found = true;);
                }
            }
            bucket = bucket->chain;
        }
        is_patch_start_bucket = false;
    }

    for (i = 0; i < dgc_removal_queue->index; i++) {
        fragment_intersection->bb_tags[i_bb++] = dgc_removal_queue->tags[i];
        if (i_bb == fragment_intersection->bb_tag_max) {
            EXPAND_ARRAY(fragment_intersection->bb_tags,
                         fragment_intersection->bb_tag_max, app_pc);
        }
    }
    dgc_removal_queue->index = 0;

    fragment_intersection->bb_tags[i_bb] = NULL;
    fragment_intersection->trace_tags[i_trace] = NULL;
    dgc_bucket_gc();
    RELEASE_ASSERT(!buckets_exist_in_range(start_bucket+1, end_bucket), "buckets exist");
    fragment_total = i_bb + i_trace;
    asmtable_unlock(dgc_table);

    return fragment_total;
}

/* returns the number of fragments removed */
uint
remove_patchable_fragments(dcontext_t *dcontext, app_pc patch_start, app_pc patch_end)
{
    uint /*i,*/ fragment_intersection_count;

    if (RUNNING_WITHOUT_CODE_CACHE()) /* case 7966: nothing to flush, ever */
        return 0;

    LOG(GLOBAL, LOG_FRAGMENT, 1,
        "DGC: remove all fragments containing ["PFX"-"PFX"]:\n",
        patch_start, patch_end);
    RELEASE_LOG(GLOBAL, LOG_FRAGMENT, 1,
        "DGC: remove all fragments containing ["PFX"-"PFX"]:\n",
        patch_start, patch_end);

    mutex_lock(&thread_initexit_lock);

    fragment_intersection_count = extract_fragment_intersection(patch_start, patch_end);
    fragment_intersection->shared_deletion_list = NULL;
    if (fragment_intersection_count > 0) {
        fragment_t *f;

        update_thread_state();
        enter_couldbelinking(dcontext, NULL, false);

        dgc_bucket_gc_list_init("remove_patchable_fragment_list");

        remove_patchable_fragment_list(dcontext, patch_start, patch_end);

        asmtable_lock(dgc_table);
        dgc_bucket_gc();
        RELEASE_ASSERT(!buckets_exist_in_range(BUCKET_ID(patch_start)+1,
                                               BUCKET_ID(patch_end-1)),
                       "buckets exist");
        asmtable_unlock(dgc_table);

        LOG(GLOBAL, LOG_FRAGMENT, 1,
                    "DGC: done removing %d fragments in ["PFX"-"PFX"]\n",
                    fragment_intersection_count, patch_start, patch_end);

        enter_nolinking(dcontext, NULL, false);

        for (f = fragment_intersection->shared_deletion_list;
             f != NULL; f = f->next_vmarea) {
            remove_from_all_threads(f);
        }
    } else {
        LOG(GLOBAL, LOG_FRAGMENT, 1, "DGC: no fragments found in "PFX"-"PFX"\n",
                    patch_start, patch_end);
    }

    mutex_unlock(&thread_initexit_lock);

    if (fragment_intersection->shared_deletion_list != NULL) {
        enter_couldbelinking(dcontext, NULL, false);
        add_to_lazy_deletion_list(dcontext, fragment_intersection->shared_deletion_list);
        enter_nolinking(dcontext, NULL, false);
    }

    return fragment_intersection_count;
}
#endif /* JITOPT */
#endif /* ANNOTATIONS */
