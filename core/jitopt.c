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
#include "jitopt.h"

#include <string.h>

#define RELEASE_NOISE 1
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

#ifdef ANNOTATIONS /* around whole file */

#define DYNAMORIO_ANNOTATE_MANAGE_CODE_AREA_NAME \
    "dynamorio_annotate_manage_code_area"

#define DYNAMORIO_ANNOTATE_UNMANAGE_CODE_AREA_NAME \
    "dynamorio_annotate_unmanage_code_area"

#define DYNAMORIO_ANNOTATE_FLUSH_FRAGMENTS_NAME \
    "dynamorio_annotate_flush_fragments"

#ifdef JITOPT

#define BUCKET_BIT_SIZE 6
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

/* x86: 16 bytes | x64: 32 bytes */
typedef struct _dgc_bb_t dgc_bb_t;
struct _dgc_bb_t {
    app_pc start;
    int ref_count;
    union {
        ptr_uint_t span;
        dgc_bb_t *head;
    };
    bb_hash_t hash; // debug
    dgc_bb_t *next;
    dgc_trace_t *containing_trace_list;
};

/* x86: 68 bytes | x64 128 bytes */
typedef struct _dgc_bucket_t {
    dgc_bb_t blocks[BUCKET_BBS];
    uint offset_sentinel;
    ptr_uint_t bucket_id;
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
static generic_table_t *dgc_table;

typedef struct _dgc_thread_state_t {
    int count;
    uint version;
    thread_record_t **threads;
    bool scaled_trace_head_tables;
} dgc_thread_state_t;

static dgc_thread_state_t *thread_state;

typedef struct _dgc_fragment_intersection_t {
    app_pc *bb_tags;
    //ptr_uint_t *bb_spans; // debug
    uint bb_tag_max;
    app_pc *trace_tags;
    uint trace_tag_max;
    fragment_t **shared_deletions;
    uint shared_deletion_index;
    uint shared_deletion_max;
} dgc_fragment_intersection_t;

static dgc_fragment_intersection_t *fragment_intersection;

#define IS_INCOMPATIBLE_OVERLAP(start1, end1, start2, end2) \
    ((start1) < (end2) && (end1) > (start2) && (end1) != (end2))

typedef struct _double_mapping_t {
    app_pc app_memory_start;
    app_pc mapping_start;
    size_t size;
    int fd;
} double_mapping_t;

typedef struct double_mapping_list_t {
    uint index;
    double_mapping_t *mappings;
} double_mapping_list_t;

#define MAX_DOUBLE_MAPPINGS 100
static double_mapping_list_t *double_mappings;
#endif

typedef struct _dgc_removal_queue_t {
    app_pc *tags;
    uint index;
    uint max;
    uint sample_index;
} dgc_removal_queue_t;

static dgc_removal_queue_t *dgc_removal_queue;

#ifdef RELEASE_NOISE
# define RELEASE_LOG(file, category, level, ...) dr_fprintf(STDERR, __VA_ARGS__)
# ifdef DEBUG
#  define RELEASE_ASSERT(cond, msg, ...) \
    if (!(cond)) \
        LOG(GLOBAL, LOG_FRAGMENT, 1, "Fail: "#cond" \""msg"\"\n", ##__VA_ARGS__)
# else
#  define RELEASE_ASSERT(cond, msg, ...) \
    if (!(cond)) \
        dr_printf("Fail: "#cond" \""msg"\"\n", ##__VA_ARGS__)
# endif
#else
# define RELEASE_LOG(file, category, level, ...)
# define RELEASE_ASSERT(cond, msg, ...)
#endif

#define DGC_REPORT_ONE_STAT(stat) \
    RELEASE_LOG(GLOBAL, LOG_ANNOTATIONS, 1, " | %s: %d\n", \
                stats->stat##_pair.name, \
                stats->stat##_pair.value);

typedef struct _dgc_stats_t {
    uint timer;
} dgc_stats_t;

static dgc_stats_t *dgc_stats;

#if !(defined (WINDOWS) && defined (X64))
static ptr_uint_t
valgrind_discard_translations(dr_vg_client_request_t *request);
#endif

static void
safe_remove_bb(dcontext_t *dcontext, fragment_t *f, bool is_tweak, bool is_cti_tweak);

static void
free_dgc_bucket_chain(void *p);

static void
update_thread_state();

static void
dgc_stat_report();

static inline app_pc
maybe_exit_cti_disp_pc(app_pc maybe_branch_pc);

#ifdef CHECK_STALE_BBS
static void
check_stale_bbs();
#endif

static void
expand_intersection_array(void **array, uint *max_size);

void
jitopt_init()
{
    dr_annotation_register_call(DYNAMORIO_ANNOTATE_MANAGE_CODE_AREA_NAME,
                                (void *) annotation_manage_code_area, false, 2,
                                DR_ANNOTATION_CALL_TYPE_FASTCALL);

    dr_annotation_register_call(DYNAMORIO_ANNOTATE_UNMANAGE_CODE_AREA_NAME,
                                (void *) annotation_unmanage_code_area, false, 2,
                                DR_ANNOTATION_CALL_TYPE_FASTCALL);
#ifndef JIT_MONITORED_AREAS
    dr_annotation_register_call(DYNAMORIO_ANNOTATE_FLUSH_FRAGMENTS_NAME,
                                (void *) annotation_flush_fragments, false, 2,
                                DR_ANNOTATION_CALL_TYPE_FASTCALL);
#endif
#if !(defined (WINDOWS) && defined (X64))
    dr_annotation_register_valgrind(DR_VG_ID__DISCARD_TRANSLATIONS,
                                    valgrind_discard_translations);
#endif
#ifdef JITOPT
    dgc_table = generic_hash_create(GLOBAL_DCONTEXT, 20, 80,
                                    HASHTABLE_ENTRY_SHARED | HASHTABLE_SHARED |
                                    HASHTABLE_RELAX_CLUSTER_CHECKS | HASHTABLE_PERSISTENT,
                                    free_dgc_bucket_chain _IF_DEBUG("DGC Coverage Table"));

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
    fragment_intersection->shared_deletion_index = 0;
    fragment_intersection->shared_deletion_max = 0x20;
    fragment_intersection->shared_deletions =
        HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, fragment_t *,
                         fragment_intersection->shared_deletion_max, ACCT_OTHER,
                         UNPROTECTED);
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
#endif
    dgc_stats = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dgc_stats_t, ACCT_OTHER, UNPROTECTED);
    memset(dgc_stats, 0, sizeof(dgc_stats_t));
}

void
jitopt_exit()
{
    uint i;
#ifdef JITOPT
    generic_hash_destroy(GLOBAL_DCONTEXT, dgc_table);
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
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, fragment_intersection->shared_deletions, fragment_t *,
                    fragment_intersection->shared_deletion_max, ACCT_OTHER, UNPROTECTED);
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, fragment_intersection, dgc_fragment_intersection_t,
                   ACCT_OTHER, UNPROTECTED);
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, dgc_removal_queue->tags, app_pc,
                    dgc_removal_queue->max, ACCT_OTHER, UNPROTECTED);
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, dgc_removal_queue, dgc_removal_queue_t,
                   ACCT_OTHER, UNPROTECTED);

    for (i = 0; i < double_mappings->index; i++) {
        int result = dynamorio_syscall(SYS_munmap, 2,
                                       double_mappings->mappings[i].mapping_start,
                                       double_mappings->mappings[i].size);
        if (result < 0) {
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "Failed to unmap the double-mapping at "PFX"\n",
                      double_mappings->mappings[i].mapping_start);
        }
    }
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, double_mappings->mappings, double_mapping_t,
                    MAX_DOUBLE_MAPPINGS, ACCT_OTHER, UNPROTECTED);
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, double_mappings, double_mapping_list_t,
                   ACCT_OTHER, UNPROTECTED);
#endif
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, dgc_stats, dgc_stats_t, ACCT_OTHER, UNPROTECTED);
}

void
annotation_manage_code_area(app_pc start, size_t len)
{
    //dr_printf("Manage code area "PFX"-"PFX"\n",
    //    start, start+len);
    LOG(GLOBAL, LOG_ANNOTATIONS, 1, "Manage code area "PFX"-"PFX"\n",
        start, start+len);
#ifdef JIT_MONITORED_AREAS
    set_region_jit_monitored(start, len);
#else
    set_region_app_managed(start, len);
#endif

    if (!thread_state->scaled_trace_head_tables) {
        thread_state->scaled_trace_head_tables = true;
        set_trace_head_table_resize_scale(5);
    }
}

void
annotation_unmanage_code_area(app_pc start, size_t len)
{
    dcontext_t *dcontext = get_thread_private_dcontext();

    if (!is_jit_managed_area(start))
        return;

    LOG(GLOBAL, LOG_ANNOTATIONS, 1, "Unmanage code area "PFX"-"PFX"\n",
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
annotation_flush_fragments(app_pc start, size_t len)
{
    dcontext_t *dcontext = get_thread_private_dcontext();

    // this is slow--maybe keep a local sorted list of app-managed regions
    if (!is_jit_managed_area(start))
        return;

#ifdef CHECK_STALE_BBS
    check_stale_bbs();
#endif

    LOG(THREAD, LOG_ANNOTATIONS, 1, "Flush fragments "PFX"-"PFX"\n",
        start, start+len);

    if (dgc_stats->timer++ > 1000) {
        dgc_stat_report();
        dgc_stats->timer = 0;
    }

    //if (len == 0 || is_couldbelinking(dcontext))
    //    return;
    //if (!executable_vm_area_executed_from(start, start+len))
    //    return;

    RSTATS_INC(app_managed_writes_observed);
#ifdef JITOPT
    if (true) {
        uint removal_count = remove_patchable_fragments(dcontext, start, start+len);
        if (removal_count > 0) {
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "Removed %d fragments in ["PFX"-"PFX"].\n", removal_count, start, start+len);
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
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "Ignored write to ["PFX"-"PFX"].\n", start, start+len);
            RSTATS_INC(app_managed_writes_ignored);
        }
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
# ifndef JIT_MONITORED_AREAS
    annotation_flush_fragments((app_pc) request->args[0], request->args[1]);
# endif
    return 0;
}
#endif

#ifdef JITOPT
/*
typedef struct _double_mapping_t {
    app_pc app_memory_start;
    app_pc mapping_start;
    size_t size;
} double_mapping_t;

typedef struct double_mapping_list_t {
    uint index;
    double_mapping_t *mappings;
} double_mapping_list_t;
*/

#ifdef X64
# define MMAP SYS_mmap
#else
# define MMAP SYS_mmap2
#endif

// TODO: synch?
static ptr_uint_t
get_double_mapped_page_delta(dcontext_t *dcontext, app_pc app_memory_start, size_t app_memory_size, uint prot)
{
    int fd, result;
    uint i;
    char file[0x20];
    app_pc remap_pc;
    double_mapping_t *new_mapping;

    for (i = 0; i < double_mappings->index; i++) {
        if (double_mappings->mappings[i].app_memory_start == app_memory_start) {
            ASSERT(double_mappings->mappings[i].size == app_memory_size);
            return double_mappings->mappings[i].mapping_start - app_memory_start;
        }
    }

    ASSERT(double_mappings->index < MAX_DOUBLE_MAPPINGS);
    new_mapping = &double_mappings->mappings[double_mappings->index];
    new_mapping->app_memory_start = app_memory_start;
    new_mapping->size = app_memory_size;

    memcpy(file, "/dev/shm/jit_", 13);
    file[13] = '0' + double_mappings->index;
    file[14] = '\0';
    RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "Mapping "PFX" +0x%x to shmem %s\n", app_memory_start, app_memory_size, file);
    fd = dynamorio_syscall(SYS_open, 3, file, O_RDWR | O_CREAT | O_NOFOLLOW, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "Failed to create the backing file %s for the double-mapping\n", file);
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "Error: '%s'\n", strerror(fd));
        return 0;
    }
    result = dynamorio_syscall(SYS_ftruncate, 2, fd, app_memory_size);
    if (result < 0) {
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "Failed to resize the backing file %s for the double-mapping\n", file);
        return 0;
    }
    new_mapping->fd = fd;
    result = dynamorio_syscall(SYS_unlink, 1, file);
    if (result < 0) {
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "Failed to unlink the backing file %s for the double-mapping\n", file);
        return 0;
    }

    new_mapping->mapping_start = (app_pc) dynamorio_syscall(MMAP, 6, NULL, app_memory_size,
                                                   PROT_READ|PROT_WRITE, MAP_SHARED,
                                                   fd, 0/*offset*/);
    memcpy(new_mapping->mapping_start, app_memory_start, app_memory_size);
    result = dynamorio_syscall(SYS_munmap, 2, app_memory_start, app_memory_size);
    if (result < 0) {
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "Failed to unmap the original memory at "PFX"\n", app_memory_start);
        return 0;
    }
    remap_pc = (app_pc) dynamorio_syscall(MMAP, 6, app_memory_start, app_memory_size,
                                 prot, MAP_SHARED | MAP_FIXED, fd, 0/*offset*/);

    RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "remap says "PFX"; new mapping is "PFX" and app memory is "PFX"\n",
              remap_pc, new_mapping->mapping_start, app_memory_start);

    ASSERT(remap_pc == app_memory_start);

    double_mappings->index++;
    return new_mapping->mapping_start - app_memory_start;
}

typedef enum _emulation_operation_t {
    EMUL_MOV,
    EMUL_OR,
    EMUL_AND,
    EMUL_SUB,
} emulation_operation_t;

app_pc
instrument_writer(dcontext_t *dcontext, priv_mcontext_t *mc, fragment_t *f, app_pc instr_app_pc,
                  app_pc write_target, size_t write_size, uint prot, bool is_jit_self_write)
{
    instr_t writer;
    reg_t immediate;
    uint *value, *value_base;
    opnd_t src, dst;
    uint sz;
    emulation_operation_t op;

    app_pc resume_pc;
    size_t app_memory_size;
    app_pc app_memory_start;
    uint *target_access = NULL;
    //priv_mcontext_t *mc = get_mcontext(dcontext);

    uint i;
    ptr_int_t page_delta;

    instr_init(dcontext, &writer);

    resume_pc = decode(dcontext, instr_app_pc, &writer); // assume readable (already decoded)
    if (!instr_valid(&writer)) {
        resume_pc = NULL;
        goto instrumentation_failure;
    }
    switch (instr_get_opcode(&writer)) {
    case OP_mov_st:
    case OP_movdqu:
    case OP_movdqa:
        op = EMUL_MOV;
        break;
    case OP_or:
        op = EMUL_OR;
        break;
    case OP_and:
        op = EMUL_AND;
        break;
    case OP_sub:
        op = EMUL_SUB;
        break;
    default:
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Failed to instrument opcode 0x%x.\n", instr_get_opcode(&writer));
        resume_pc = NULL;
        goto skip_instrumentation;
    }

    src = instr_get_src(&writer, 0);
    dst = instr_get_dst(&writer, 0);
    sz = opnd_size_in_bytes(opnd_get_size(dst));

    ASSERT((op != EMUL_OR && op != EMUL_AND && op != EMUL_SUB) || sz == 4 || sz == 8);
    ASSERT(opnd_is_memory_reference(dst));
    if (sz < 1 || sz > 128 || (sz > 1 && sz % 4 != 0)) {
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Failed to instrument instruction with opcode 0x%x and dst size %d\n",
                  instr_get_opcode(&writer), sz);
        resume_pc = NULL;
        goto instrumentation_failure;
    }

    if (opnd_is_reg(src)) {
        value = (uint *)((byte *)mc + opnd_get_reg_mcontext_offs(opnd_get_reg(src)));
    } else if (opnd_is_immed_int(src)) {
        immediate = (reg_t) opnd_get_immed_int(src);
        value = (uint *)&immediate;
    } else {
        resume_pc = NULL;
        goto instrumentation_failure;
    }
    value_base = value;

    if (!get_jit_monitored_area_bounds(write_target, &app_memory_start, &app_memory_size)) {
        resume_pc = NULL;
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Skipping instrumentation of "PFX"\n", write_target);
        goto skip_instrumentation;
    }

    ASSERT(write_target >= app_memory_start &&
           write_target < (app_memory_start + app_memory_size));

    page_delta = get_double_mapped_page_delta(dcontext, app_memory_start, app_memory_size, prot);
    if (page_delta == 0) {
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Failed to find page delta for app memory at "PFX"\n", app_memory_start);
        resume_pc = NULL;
        goto instrumentation_failure;
    }

    target_access = (uint *)((ptr_int_t)write_target + page_delta);
    if (op == EMUL_MOV) {
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Attempting to write %d bytes to "PFX" via "PFX"\n", sz, write_target, target_access);
        if (sz == 1) {
            byte byte_value = (*value & 0xff);
            byte *byte_target_access = (byte *)target_access;
             *byte_target_access = byte_value;
            ASSERT(*byte_target_access == byte_value);
            ASSERT(*(byte*)write_target == byte_value);
        } else {
            for (i = 0; i < (sz/sizeof(uint)); i++, target_access++, value++)
                *target_access = *value;
            target_access = (uint *)((ptr_int_t)write_target + page_delta);
            ASSERT(*target_access == *value_base);
            ASSERT(*(uint*)write_target == *value_base);
        }
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "Successfully wrote %d bytes to "PFX" via "PFX"\n", sz, write_target, target_access);
    } else if (op == EMUL_OR) {
        if (sz == 4) {
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Attempting to 'or' %d bytes to "PFX" via "PFX"\n", sz, write_target, target_access);
            *target_access |= *value;
            ASSERT((*target_access & *value) == *value);
            ASSERT((*(uint*)write_target & *value) == *value);
        } else if (sz == 8) {
            ptr_uint_t *word_target_access = (ptr_uint_t *)((ptr_int_t)write_target + page_delta);
            ptr_uint_t *word_value = (ptr_uint_t *)value;
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Attempting to 'or' %d bytes to "PFX" via "PFX"\n", sz, write_target, target_access);
            *word_target_access |= *word_value;
            ASSERT((*word_target_access & *word_value) == *word_value);
            ASSERT((*(ptr_uint_t*)write_target & *word_value) == *word_value);
        }
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "Successfully 'or'd %d bytes to "PFX" via "PFX"\n", sz, write_target, target_access);
    } else if (op == EMUL_AND) {
        if (sz == 4) {
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Attempting to 'and' %d bytes to "PFX" via "PFX"\n", sz, write_target, target_access);
            *target_access &= *value;
            ASSERT((*target_access & ~(*value)) == 0);
            ASSERT((*(uint*)write_target & ~(*value)) == 0);
        } else if (sz == 8) {
            ptr_uint_t *word_target_access = (ptr_uint_t *)((ptr_int_t)write_target + page_delta);
            ptr_uint_t *word_value = (ptr_uint_t *)value;
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Attempting to 'and' %d bytes to "PFX" via "PFX"\n", sz, write_target, target_access);
            *word_target_access &= *word_value;
            ASSERT((*word_target_access & ~(*word_value)) == 0);
            ASSERT((*(ptr_uint_t*)write_target & ~(*word_value)) == 0);
        }
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "Successfully 'and'd %d bytes to "PFX" via "PFX"\n", sz, write_target, target_access);
    } else if (op == EMUL_SUB) {
        if (sz == 4) {
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Attempting to sub %d bytes to "PFX" via "PFX"\n", sz, write_target, target_access);
            *target_access -= *value;
        } else if (sz == 8) {
            ptr_uint_t *word_target_access = (ptr_uint_t *)((ptr_int_t)write_target + page_delta);
            ptr_uint_t *word_value = (ptr_uint_t *)value;
            RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Attempting to sub %d bytes to "PFX" via "PFX"\n", sz, write_target, target_access);
            *word_target_access -= *word_value;
        }
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "Successfully subtraced %d bytes to "PFX" via "PFX"\n", sz, write_target, target_access);
    }
    if (!is_jit_self_write)
        annotation_flush_fragments(write_target, sz);

instrumentation_failure:
    if (resume_pc == NULL)
        RELEASE_LOG(THREAD, LOG_ANNOTATIONS, 1, "DGC: Failed to write "PFX" via "PFX"\n", write_target, target_access);
skip_instrumentation:
    instr_free(dcontext, &writer);
    return resume_pc;
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

static inline bb_hash_t
dgc_bb_hash(dgc_bb_t *bb)
{
    dgc_bb_t *head = dgc_bb_head(bb);
    return head->hash;
}

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
        return (dgc_bucket_t *)(bb - 2);
    if (*(uint *)(bb + 2) == BUCKET_OFFSET_SENTINEL)
        return (dgc_bucket_t *)(bb - 1);
    if (*(uint *)(bb + 3) == BUCKET_OFFSET_SENTINEL)
        return (dgc_bucket_t *)bb;
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

static void
dgc_stat_report()
{
    RELEASE_LOG(GLOBAL, LOG_ANNOTATIONS, 1, " |   ==== DGC Stats ====\n");
    DGC_REPORT_ONE_STAT(app_managed_writes_observed);
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
    dgc_bucket_t *bucket = generic_hash_lookup(GLOBAL_DCONTEXT, dgc_table, bucket_id);
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
        RSTATS_DEC(app_managed_trace_buckets_live);
        RSTATS_INC(app_managed_trace_buckets_freed);
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
                    generic_hash_remove(GLOBAL_DCONTEXT, dgc_table, bucket->bucket_id);
                    RELEASE_ASSERT(anchor->offset_sentinel == BUCKET_OFFSET_SENTINEL, "Freed already?");
                    generic_hash_add(GLOBAL_DCONTEXT, dgc_table, anchor->bucket_id, anchor);
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
                    RSTATS_DEC(app_managed_bb_buckets_live);
                    RSTATS_INC(app_managed_bb_buckets_freed);
                    bucket = anchor->chain;
                }
            } else {
                all_empty = false;
                anchor = bucket;
                bucket = bucket->chain;
            }
        } while (bucket != NULL);
        if (all_empty)
            generic_hash_remove(GLOBAL_DCONTEXT, dgc_table, bucket_id);
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
        generic_hash_remove(GLOBAL_DCONTEXT, dgc_table,
                            dgc_bucket_gc_list->removals[i]->bucket_id);
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
            expand_intersection_array((void **) &dgc_bucket_gc_list->staging,
                                      &dgc_bucket_gc_list->max_staging);
            expand_intersection_array((void **) &dgc_bucket_gc_list->removals,
                                      &dgc_bucket_gc_list->max_removals);
            //RELEASE_ASSERT(false, "GC staging list too full (%d) during %s",
            //               i, dgc_bucket_gc_list->current_operation);
        }
        dgc_bucket_gc_list->staging[dgc_bucket_gc_list->staging_count++] = bucket;
    }
}

static inline void
dgc_stage_bucket_id_for_gc(ptr_uint_t bucket_id)
{
    dgc_stage_bucket_for_gc(generic_hash_lookup(GLOBAL_DCONTEXT, dgc_table, bucket_id));
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
        RSTATS_DEC(app_managed_bb_buckets_live);
        RSTATS_INC(app_managed_bb_buckets_freed);
        bucket = next_bucket;
    } while (bucket != NULL);
}

void
dgc_table_dereference_bb(app_pc tag)
{
    dgc_bb_t *bb;
    RELEASE_LOG(GLOBAL, LOG_FRAGMENT, 1, "DGC: dereferencing bb "PFX"\n", tag);
    TABLE_RWLOCK(dgc_table, write, lock);
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
    TABLE_RWLOCK(dgc_table, write, unlock);
}

static void
dgc_stage_removal_gc_outliers(ptr_uint_t bucket_id)
{
    uint i;
    bool found = false;
    dgc_bucket_t *bucket = generic_hash_lookup(GLOBAL_DCONTEXT, dgc_table, bucket_id);
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

    TABLE_RWLOCK(dgc_table, write, lock);
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
    TABLE_RWLOCK(dgc_table, write, unlock);

    dgc_stat_report();
}

void
dgc_cache_reset()
{
    TABLE_RWLOCK(dgc_table, write, lock);
    generic_hash_clear(GLOBAL_DCONTEXT, dgc_table);
    TABLE_RWLOCK(dgc_table, write, unlock);
}

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
    ptr_uint_t hash = hash_bits(span+1, start);

    // this is slow--maybe keep a local sorted list of app-managed regions
    if (!is_jit_managed_area(start))
        return;

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

    TABLE_RWLOCK(dgc_table, write, lock);
    for (bucket_id = start_bucket_id; bucket_id <= end_bucket_id; bucket_id++) {
        bucket = generic_hash_lookup(GLOBAL_DCONTEXT, dgc_table, bucket_id);
        if (bucket == NULL) {
            bucket = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, dgc_bucket_t, ACCT_OTHER, UNPROTECTED);
            memset(bucket, 0, sizeof(dgc_bucket_t));
            generic_hash_add(GLOBAL_DCONTEXT, dgc_table, bucket_id, bucket);
            bucket->offset_sentinel = BUCKET_OFFSET_SENTINEL;
            bucket->bucket_id = bucket_id;
            bucket->head = bucket;
            i = 0;
            RSTATS_INC(app_managed_bb_buckets_live);
            RSTATS_INC(app_managed_bb_buckets_allocated);
        } else {
            dgc_bucket_t *head_bucket = bucket, *available_bucket = NULL;
            uint available_slot = 0xff;
            RELEASE_ASSERT(bucket->offset_sentinel == BUCKET_OFFSET_SENTINEL, "Freed already?");
            RELEASE_ASSERT(bucket == bucket->head, "No!");
            while (true) {
                for (i = 0; i < BUCKET_BBS; i++) {
                    bb = &bucket->blocks[i];
                    if (bb->start == start) {
//#ifdef DEBUG
                        if (dgc_bb_end(bb) == end &&
                            dgc_bb_hash(bb) == hash) {
//#endif
                            found = true;
                            break;
//#ifdef DEBUG
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
//#endif
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
                RSTATS_INC(app_managed_bb_buckets_live);
                RSTATS_INC(app_managed_bb_buckets_allocated);
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
            first_bb->hash = hash;
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
    TABLE_RWLOCK(dgc_table, write, unlock);
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

    TABLE_RWLOCK(dgc_table, write, lock);
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
                    RSTATS_INC(app_managed_trace_buckets_live);
                    RSTATS_INC(app_managed_trace_buckets_allocated);
                }
            }
        }
    }
#ifdef FULL_TRACE_LOG
    RELEASE_LOG(GLOBAL, LOG_FRAGMENT, 1, "}\n");
#endif
    TABLE_RWLOCK(dgc_table, write, unlock);

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
        TABLE_RWLOCK(dgc_table, write, lock);
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
                    expand_intersection_array((void **) &dgc_removal_queue->tags,
                                              &dgc_removal_queue->max);
                }
                s = t;
                count--;
            }
        }
        TABLE_RWLOCK(dgc_table, write, unlock);
    }
#undef MAX_LINKSTUBS
#undef LINKSTUB_SAMPLE_INTERVAL
}

static void
expand_intersection_array(void **array, uint *max_size)
{
    void *original_array = *array;

    *array = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, app_pc, *max_size * 2,
                              ACCT_OTHER, UNPROTECTED);
    memcpy(*array, original_array, *max_size * sizeof(void *));
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, original_array, app_pc, *max_size,
                    ACCT_OTHER, UNPROTECTED);
    *max_size *= 2;
}

static void
safe_delete_fragment(dcontext_t *dcontext, fragment_t *f, bool is_tweak)
{
    if (TEST(FRAG_CANNOT_DELETE, f->flags)) {
        RELEASE_LOG(GLOBAL, LOG_ANNOTATIONS, 1,
                    "Cannot delete fragment "PFX" with flags 0x%x!\n", f->tag, f->flags);
        return;
    }

    if (TEST(FRAG_IS_TRACE_HEAD, f->flags) && is_tweak)
        set_trace_head_jit_tweaked(dcontext, f->tag);

    if (TEST(FRAG_SHARED, f->flags)) {
        //acquire_recursive_lock(&change_linking_lock);
        SHARED_FLAGS_RECURSIVE_LOCK(f->flags, acquire, change_linking_lock);
        acquire_vm_areas_lock(dcontext, f->flags);

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

        release_vm_areas_lock(dcontext, f->flags);
        SHARED_FLAGS_RECURSIVE_LOCK(f->flags, release, change_linking_lock);
        //release_recursive_lock(&change_linking_lock);

        fragment_intersection->shared_deletions[fragment_intersection->shared_deletion_index++] = f;
        if (fragment_intersection->shared_deletion_index == fragment_intersection->shared_deletion_max) {
            expand_intersection_array((void **) &fragment_intersection->shared_deletions,
                                      &fragment_intersection->shared_deletion_max);
        }
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

static inline void
safe_remove_bb(dcontext_t *dcontext, fragment_t *f, bool is_tweak, bool is_cti_tweak)
{
    if (f != NULL) {
        link_stats(f, is_tweak, is_cti_tweak);
        safe_delete_fragment(dcontext, f, is_tweak);
    }
}

static inline void
safe_remove_trace(dcontext_t *dcontext, trace_t *t, bool is_tweak, bool is_cti_tweak)
{
    if (t != NULL) {
        uint i;
        bool found = false;
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

            TABLE_RWLOCK(dgc_table, read, lock); // yuk
            bb = dgc_table_find_bb(t->f.tag, &bucket, NULL);
            TABLE_RWLOCK(dgc_table, read, unlock);
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
            safe_delete_fragment(dcontext, (fragment_t *)t, is_tweak);
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
    bool is_tweak = ((patch_end - patch_start) <= sizeof(ptr_uint_t));
    bool is_cti_tweak = ((patch_end - patch_start) == sizeof(ptr_uint_t) &&
                         (maybe_exit_cti_disp_pc(patch_start-1) != NULL ||
                          maybe_exit_cti_disp_pc(patch_start-2) != NULL));
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
            monitor_data_t *md = (monitor_data_t *) dcontext->monitor_field;
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
                              *bb_tag), is_tweak, is_cti_tweak);
            safe_remove_trace(tgt_dcontext,
                              (trace_t *)fragment_lookup_shared_trace(tgt_dcontext,
                              *bb_tag), is_tweak, is_cti_tweak);
            safe_remove_bb(tgt_dcontext, fragment_lookup_bb(tgt_dcontext, *bb_tag), is_tweak, is_cti_tweak);
            safe_remove_bb(tgt_dcontext, fragment_lookup_shared_bb(tgt_dcontext, *bb_tag), is_tweak, is_cti_tweak);
        }
        trace_tag = fragment_intersection->trace_tags;
        for (; *trace_tag != NULL; trace_tag++) {
            safe_remove_trace(tgt_dcontext, (trace_t *)fragment_lookup_trace(tgt_dcontext,
                              *trace_tag), is_tweak, is_cti_tweak);
            safe_remove_trace(tgt_dcontext,
                              (trace_t *)fragment_lookup_shared_trace(tgt_dcontext,
                              *trace_tag), is_tweak, is_cti_tweak);
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

#ifdef RELEASE_NOISE
static bool
buckets_exist_in_range(ptr_uint_t start, ptr_uint_t end)
{
    ptr_uint_t i;
    for (i = start; i < end; i++) {
        if (generic_hash_lookup(GLOBAL_DCONTEXT, dgc_table, i) != NULL)
            return true;
    }
    return false;
}
#endif

static uint
add_trace_intersection(dgc_trace_t *trace, uint i)
{
    if (!has_tag(trace->tags[0], fragment_intersection->trace_tags, i))
        fragment_intersection->trace_tags[i++] = trace->tags[0];
    if (i == fragment_intersection->trace_tag_max) {
        expand_intersection_array((void **) &fragment_intersection->trace_tags,
                                  &fragment_intersection->trace_tag_max);
    }
    if (trace->tags[1] != NULL && !has_tag(trace->tags[1],
                                           fragment_intersection->trace_tags, i))
        fragment_intersection->trace_tags[i++] = trace->tags[1];
    if (i == fragment_intersection->trace_tag_max) {
        expand_intersection_array((void **) &fragment_intersection->trace_tags,
                                  &fragment_intersection->trace_tag_max);
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

    TABLE_RWLOCK(dgc_table, write, lock);
    dgc_bucket_gc_list_init("remove_patchable_fragments");
    for (bucket_id = start_bucket; bucket_id <= end_bucket; bucket_id++) {
        bucket = generic_hash_lookup(GLOBAL_DCONTEXT, dgc_table, bucket_id);
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
                        expand_intersection_array((void **) &fragment_intersection->bb_tags,
                                                  &fragment_intersection->bb_tag_max);
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
            expand_intersection_array((void **) &fragment_intersection->bb_tags,
                                      &fragment_intersection->bb_tag_max);
        }
    }
    dgc_removal_queue->index = 0;

    fragment_intersection->bb_tags[i_bb] = NULL;
    fragment_intersection->trace_tags[i_trace] = NULL;
    dgc_bucket_gc();
    RELEASE_ASSERT(!buckets_exist_in_range(start_bucket+1, end_bucket), "buckets exist");
    fragment_total = i_bb + i_trace;
    TABLE_RWLOCK(dgc_table, write, unlock);

    return fragment_total;
}

/* returns the number of fragments removed */
uint
remove_patchable_fragments(dcontext_t *dcontext, app_pc patch_start, app_pc patch_end)
{
    uint i, fragment_intersection_count;

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
    fragment_intersection->shared_deletion_index = 0;
    if (fragment_intersection_count > 0) {
        update_thread_state();
        enter_couldbelinking(dcontext, NULL, false);

        dgc_bucket_gc_list_init("remove_patchable_fragment_list");

        remove_patchable_fragment_list(dcontext, patch_start, patch_end);

        TABLE_RWLOCK(dgc_table, write, lock);
        dgc_bucket_gc();
        RELEASE_ASSERT(!buckets_exist_in_range(BUCKET_ID(patch_start)+1,
                                               BUCKET_ID(patch_end-1)),
                       "buckets exist");
        TABLE_RWLOCK(dgc_table, write, unlock);

        LOG(GLOBAL, LOG_FRAGMENT, 1,
                    "DGC: done removing %d fragments in ["PFX"-"PFX"]\n",
                    fragment_intersection_count, patch_start, patch_end);

        enter_nolinking(dcontext, NULL, false);
    } else {
        LOG(GLOBAL, LOG_FRAGMENT, 1, "DGC: no fragments found in "PFX"-"PFX"\n",
                    patch_start, patch_end);
    }
    mutex_unlock(&thread_initexit_lock);

    if (fragment_intersection->shared_deletion_index > 0) {
        enter_couldbelinking(dcontext, NULL, false);
        for (i = 0; i < fragment_intersection->shared_deletion_index; i++)
            add_to_lazy_deletion_list(dcontext, fragment_intersection->shared_deletions[i]);
        enter_nolinking(dcontext, NULL, false);
    }

    return fragment_intersection_count;
}
#endif /* JITOPT */
#endif /* ANNOTATIONS */


