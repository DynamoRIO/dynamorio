/* **********************************************************
 * Copyright (c) 2012-2023 Google, Inc.  All rights reserved.
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

/*
 * fragment.h - fragment_t structures
 */

#ifndef _FRAGMENT_H_
#define _FRAGMENT_H_ 1

#include "hashtable.h"
#include "translate.h"
#include "fragment_api.h"

/* Flags, stored in fragment_t->flags bitfield
 */
#define FRAG_IS_FUTURE 0x000001
#define FRAG_TRACE_LINKS_SHIFTED 0x000002
#define FRAG_IS_TRACE 0x000004
#define FRAG_IS_TRACE_HEAD 0x000008
#define FRAG_LINKED_OUTGOING 0x000010
#define FRAG_LINKED_INCOMING 0x000020
#define FRAG_CANNOT_DELETE 0x000040
#define FRAG_CANNOT_BE_TRACE 0x000080

/* Indicates an irregular fragment_t.  In particular, there are no
 * trailing linkstubs after this fragment_t struct.  Note that other
 * "fake fragment_t" flags should be set in combination with this one
 * (FRAG_IS_{FUTURE,EXTRA_VMAREA*,EMPTY_SLOT}, FRAG_FCACHE_FREE_LIST).
 */
#define FRAG_FAKE 0x000100

/* this flag indicates fragment writes all 6 flags prior to reading */
#define FRAG_WRITES_EFLAGS_6 0x000200
#define FRAG_WRITES_EFLAGS_ARITH FRAG_WRITES_EFLAGS_6
/* this flag indicates fragment writes OF before reading it */
#define FRAG_WRITES_EFLAGS_OF 0x000400

/* This is not a fragment_t but an fcache free list entry.
 * In current usage this is checked to see if the previous free list entry is
 * a free list entry (see fcache.c's free_list_header_t.flags).
 * This flag MUST be in the bottom 16 bits since free_list_header_t.flags
 * is a ushort!
 */
#define FRAG_FCACHE_FREE_LIST 0x000800

#define FRAG_HAS_SYSCALL 0x001000
/* this flags indicates that a trace is being built from fragment_t->tag */
#define FRAG_TRACE_BUILDING 0x002000

/* used on future fragments, currently only read for adaptive working set
 * also used for fragments to know whether they are on the deleted list
 * (shared) or flush queue (private)
 */
#define FRAG_WAS_DELETED 0x004000
/* indicates frag is from a non-protected page and may be self-modifying */
#define FRAG_SELFMOD_SANDBOXED 0x008000
/* indicates whether frag contains an elided direct cti */
#define FRAG_HAS_DIRECT_CTI 0x010000
/* used by fcache to distinguish fragment_t from its own empty slot struct */
#define FRAG_IS_EMPTY_SLOT 0x020000
/* used by vmarea to distinguish fragment_t from its own multi unit struct */
#define FRAG_IS_EXTRA_VMAREA 0x040000
/* If FRAG_IS_EXTRA_VMAREA is set, this value indicates this flag: */
#define FRAG_IS_EXTRA_VMAREA_INIT 0x080000
#ifdef LINUX
/* If FRAG_IS_EXTRA_VMAREA is not set, this value indicates this flag,
 * which labels the fragment as containing rseq data whose lifetime should
 * match the fragment.
 */
#    define FRAG_HAS_RSEQ_ENDPOINT 0x080000
#endif

#ifdef PROGRAM_SHEPHERDING
/* indicates from memory that wasn't part of code from image on disk */
#    define FRAG_DYNGEN 0x100000
#    ifdef DGC_DIAGNOSTICS
/* for now, only used to identify regions that fail our policies */
#        define FRAG_DYNGEN_RESTRICTED 0x200000
#    endif
#endif

#ifndef DGC_DIAGNOSTICS
/* i#107, for mangling mov_seg instruction,
 * NOTE: mangle_app_seg cannot be used with DGC_DIAGNOSTICS.
 */
#    define FRAG_HAS_MOV_SEG 0x200000
#endif

#if defined(X86) && defined(X64)
/* this fragment contains 32-bit code */
#    define FRAG_32_BIT 0x400000
#elif defined(ARM) && !defined(X64)
/* this fragment contains Thumb code */
#    define FRAG_THUMB 0x400000
#endif

#define FRAG_MUST_END_TRACE 0x800000

#define FRAG_SHARED 0x1000000
/* indicates a temporary private copy of a shared bb, used
 * for trace building
 */
#define FRAG_TEMP_PRIVATE 0x2000000

#define FRAG_TRACE_OUTPUT 0x4000000
/* Used only during block building, which means there is no conflict with
 * FRAG_TRACE_OUTPUT.
 */
#ifdef LINUX
#    define FRAG_STARTS_RSEQ_REGION 0x4000000
#endif

#define FRAG_CBR_FALLTHROUGH_SHORT 0x8000000

/* Indicates coarse-grain cache management, i.e., batch units with
 * no individual fragment_t.
 */
#define FRAG_COARSE_GRAIN 0x10000000

/* Translation info was recorded at fragment emit time in a post-fragment_t field.
 * This is NOT set for flushed fragments, which store their info in the in_xlate
 * union instead and are marked FRAG_WAS_DELETED, though if both flags are set
 * then the info is in the post-fragment_t field.
 */
#define FRAG_HAS_TRANSLATION_INFO 0x20000000

#ifdef X64
/* this fragment contains 64-bit code translated from 32-bit app code */
#    define FRAG_X86_TO_X64 0x40000000
#    ifdef SIDELINE
#        error SIDELINE not compatible with X64
#    endif
#elif defined(SIDELINE)
#    define FRAG_DO_NOT_SIDELINE 0x40000000
#endif

/* This fragment immediately follows a free entry in the fcache */
#define FRAG_FOLLOWS_FREE_ENTRY 0x80000000

/* Flags that a future fragment can transfer to a real on taking its place:
 * Naturally we don't want FRAG_IS_FUTURE or FRAG_WAS_DELETED.
 * FRAG_SHARED has to already be on the real fragment.
 * Do NOT take the FRAG_TEMP_PRIVATE flag that we put on futures as an
 * optimization in case never used -- it will mess up which heap is used, etc.
 * There aren't really any other flags added to future fragments.
 * Even FRAG_IS_TRACE_HEAD is only used for marking shared secondary
 * trace heads from private traces.
 */
#define FUTURE_FLAGS_TRANSFER (FRAG_IS_TRACE_HEAD)
/* only used for debugging */
#define FUTURE_FLAGS_ALLOWED                                                 \
    (FUTURE_FLAGS_TRANSFER | FRAG_FAKE | FRAG_IS_FUTURE | FRAG_WAS_DELETED | \
     FRAG_SHARED | FRAG_TEMP_PRIVATE)

/* FRAG_IS_X86_TO_X64 is in x64 mode */
#define FRAG_ISA_MODE(flags)                                                        \
    IF_X86_ELSE(                                                                    \
        IF_X64_ELSE((FRAG_IS_32(flags)) ? DR_ISA_IA32 : DR_ISA_AMD64, DR_ISA_IA32), \
        IF_X64_ELSE(DR_ISA_ARM_A64,                                                 \
                    (TEST(FRAG_THUMB, (flags)) ? DR_ISA_ARM_THUMB : DR_ISA_ARM_A32)))

static inline uint
frag_flags_from_isa_mode(dr_isa_mode_t mode)
{
#ifdef X86
#    ifdef X64
    if (mode == DR_ISA_IA32)
        return FRAG_32_BIT;
    ASSERT(mode == DR_ISA_AMD64);
    return 0;
#    else
    ASSERT(mode == DR_ISA_IA32);
    return 0;
#    endif
#elif defined(AARCH64)
    ASSERT(mode == DR_ISA_ARM_A64);
    return 0;
#elif defined(ARM)
    if (mode == DR_ISA_ARM_THUMB)
        return FRAG_THUMB;
    ASSERT(mode == DR_ISA_ARM_A32);
    return 0;
#elif defined(RISCV64)
    ASSERT(mode == DR_ISA_RV64IMAFDC);
    return 0;
#endif
}

/* to save space size field is a ushort => maximum fragment size */
#ifndef AARCH64
enum { MAX_FRAGMENT_SIZE = USHRT_MAX };
#else
/* On AArch64, TBNZ/TBZ has a range of +/- 32 KiB. */
enum { MAX_FRAGMENT_SIZE = 0x8000 };
#endif

/* fragment structure used for basic blocks and traces
 * this is the core structure shared by everything
 * trace heads and traces extend it below
 */
struct _fragment_t {
    /* WARNING: the tag offset is assumed to be 0 in arch/emit_utils.c
     * Also, next and flags' offsets must match future_fragment_t's
     * And flags' offset must match fcache.c's empty_slot_t as well as
     * vmarea.c's multi_entry_t structs
     */
    app_pc tag; /* non-zero fragment tag used for lookups */

    /* Contains FRAG_ flags.  Should only be modified for FRAG_SHARED fragments
     * while holding the change_linking_lock.
     */
    uint flags;

    /* trace head counters are in separate hashtable since always private.
     * FIXME: when all fragments are private, a separate table uses more memory
     * than having a counter field for all fragments, including non-trace-heads
     */

    /* size in bytes of the fragment (includes body and stubs, and for
     * selfmod fragments also includes selfmod app code copy and size field)
     */
    ushort size;

    /* both of these fields are tiny -- padding shouldn't be more than a cache line
     * size (32 P3, 64 P4), prefix should be even smaller.
     * they combine with size to shrink fragment_t for us
     * N.B.: byte is an unsigned char
     */
    byte prefix_size;  /* size of prefix, after which is non-ind. br. entry */
    byte fcache_extra; /* padding to fit in fcache slot; includes the header */

    cache_pc start_pc; /* very top of fragment's code, equals
                        * entry point when indirect branch target */

    union {
        /* For a live fragment, we store a list of other fragments' exits that target
         * this fragment (outgoing exit stubs are all allocated with fragment_t struct,
         * use FRAGMENT_EXIT_STUBS() to access).
         */
        linkstub_t *incoming_stubs;
        /* For a pending-deletion fragment (marked with FRAG_WAS_DELETED),
         * we store translation info.
         */
        translation_info_t *translation_info;
    } in_xlate;

    fragment_t *next_vmarea; /* for chaining fragments in vmarea list */
    fragment_t *prev_vmarea; /* for chaining fragments in vmarea list */
    union {
        fragment_t *also_vmarea; /* for chaining fragments across vmarea lists */
        /* For lazily-deleted fragments, we store the flushtime here, as this
         * field is no longer used once a fragment is not live.
         */
        uint flushtime;
    } also;

#ifdef DEBUG
    int id; /* thread-shared-unique fragment identifier */
#endif

#ifdef CUSTOM_TRACES_RET_REMOVAL
    int num_calls;
    int num_rets;
#endif
}; /* fragment_t */

/* Shared fragments don't need some fields that private ones do, so we
 * dynamically choose different structs.  fragment_t is for shared only.
 * Here we again use C awkwardness to have a subclass.
 */
typedef struct _private_fragment_t {
    fragment_t f;
    fragment_t *next_fcache; /* for chaining fragments in fcache unit */
    fragment_t *prev_fcache; /* for chaining fragments in fcache unit */
} private_fragment_t;

/* Structure used for future fragments, separate to save memory.
 * next and flags must be at same offset as for fragment_t, so that
 * hashtable (next) and link.c (flags) can polymorphize fragment_t
 * and future_fragment_t.  The rule is enforced in fragment_init.
 */
struct _future_fragment_t {
    app_pc tag;                 /* non-zero fragment tag used for lookups */
    uint flags;                 /* contains FRAG_ flags */
    linkstub_t *incoming_stubs; /* list of other fragments' exits that target
                                 * this fragment */
};

typedef struct _trace_bb_info_t {
    app_pc tag;
#if defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH)
    /* PR 204770: holds # exits in the trace corresponding to that bb.
     * Used to obtain a better RCT source address.
     * We could recreate to obtain this, except for flushed fragments: but
     * we do this frequently enough that it's simpler to store for all
     * fragments.
     */
    uint num_exits;
#endif
} trace_bb_info_t;

/* N.B.: if you add fields to trace_t, make sure to add them
 * to fragment_copy_data_fields as well as fragment_create and fragment_free
 */
typedef struct _trace_only_t {
#ifdef PROFILE_RDTSC
    uint64 count;      /* number of executions of this fragment */
    uint64 total_time; /* total time ever spent in this fragment */
#endif

    /* holds the tags (and other info) for all constituent basic blocks */
    trace_bb_info_t *bbs;
    uint num_bbs;
} trace_only_t;

/* trace extension of fragment_t */
struct _trace_t {
    fragment_t f; /* shared fields */
    trace_only_t t;
}; /* trace_t */

/* private version of trace_t */
typedef struct _private_trace_t {
    private_fragment_t f;
    trace_only_t t;
} private_trace_t;

/* convenient way to deal w/ trace fields: this returns trace_only_t* */
#define TRACE_FIELDS(f)                                      \
    (ASSERT(TEST(FRAG_IS_TRACE, (f)->flags)),                \
     (TEST(FRAG_SHARED, (f)->flags) ? &(((trace_t *)(f))->t) \
                                    : &(((private_trace_t *)(f))->t)))

/* FIXME: Can be used to determine if a frag should have a prefix since currently
 * all IB targets have the same prefix. Use a different macro if different frags
 * have different prefixes, i.e., BBs vs. traces.
 */
/* Historically traces were the only IBL targeted fragments, we'd want to have BBs
 * targeted too, yet not all of them should indeed be targeted.
 * See case 147 about possible extensions for bb non-tracehead
 * fragments.
 */
/* FIXME: case 147: private bb's would have a different prefix
 * therefore should be taken out of here.  Other than that there is no good reason
 * not to be able to target them.  case 5836 covers targeting private fragments
 * when using thread-shared ibl tables.
 * A good example is FRAG_SELFMOD_SANDBOXED bb's which are always private.
 * However it's not very useful to go after the short lived FRAG_TEMP_PRIVATE
 */
/* can a frag w/the given flags be an IBL target? */
#define IS_IBL_TARGET(flags)                                                       \
    (TEST(FRAG_IS_TRACE, (flags))                                                  \
         ? (TEST(FRAG_SHARED, (flags)) || !DYNAMO_OPTION(shared_trace_ibt_tables)) \
         : (DYNAMO_OPTION(bb_ibl_targets) &&                                       \
            (TEST(FRAG_SHARED, (flags)) || !DYNAMO_OPTION(shared_bb_ibt_tables))))

#define HASHTABLE_IBL_OFFSET(branch_type)                                    \
    (((branch_type) == IBL_INDCALL) ? DYNAMO_OPTION(ibl_indcall_hash_offset) \
                                    : DYNAMO_OPTION(ibl_hash_func_offset))

#ifdef HASHTABLE_STATISTICS
/* Statistics written from the cache that must be allocated separately */
typedef struct _unprot_ht_statistics_t {
    /* Statistics for App mode indirect branch lookups. Useful only for trace table.
     * These should be accessible by indirect_branch_lookup emitted routines.
     * Should have the form <ibl_routine>_stats, and are per hash table per routine
     * (and per thread).
     * These are in the hash table itself for easier access when sharing IBL routines.
     */
    hashtable_statistics_t trace_ibl_stats[IBL_BRANCH_TYPE_END];
    hashtable_statistics_t bb_ibl_stats[IBL_BRANCH_TYPE_END];

    /* FIXME: this should really go to arch/arch.c instead of here */
#    ifdef WINDOWS
    hashtable_statistics_t shared_syscall_hit_stats; /* miss path shared with trace_ibl */
#    endif
} unprot_ht_statistics_t;
#endif /* HASHTABLE_STATISTICS */

/* Cached tag->start_pc table used by lookuptable in fragment_table_t as
 * well as by ibl_table_t
 */
typedef struct _fragment_entry_t {
    app_pc tag_fragment;        /* non-zero fragment tag used for lookups */
    cache_pc start_pc_fragment; /* very top of fragment's code from fragment_t */
} fragment_entry_t;

#define HASHLOOKUP_SENTINEL_START_PC ((cache_pc)PTR_UINT_1)

/* Flags stored in {fragment,ibl}_table_t->flags bitfield
 */
/* Indicates that fragment entries are shared between multiple tables in an
 * inclusive hierarchical fashion, so only removal from the main table (which
 * is not so marked) will result in fragment deletion. Used primarily for
 * IBL targeted tables
 */
/* Updates to these flags should be reflected in
 * arch/arch.c:table_flags_to_frag_flags() */
#define FRAG_TABLE_INCLUSIVE_HIERARCHY HASHTABLE_NOT_PRIMARY_STORAGE
/* Set for IBL targeted tables, used in conjuction with FRAG_INCLUSIVE_HIERARCHY */
#define FRAG_TABLE_IBL_TARGETED HASHTABLE_LOCKLESS_ACCESS
/* Set for IBL targeted tables, indicates that the table holds shared targets */
#define FRAG_TABLE_TARGET_SHARED HASHTABLE_ENTRY_SHARED
/* Indicates that the table is shared */
#define FRAG_TABLE_SHARED HASHTABLE_SHARED
/* is this table allocated in persistent memory? */
#define FRAG_TABLE_PERSISTENT HASHTABLE_PERSISTENT
/* Indicates that the table targets traces */
#define FRAG_TABLE_TRACE HASHTABLE_CUSTOM_FLAGS_START

/* hashtable of fragment_t* entries */
/* macros w/ name and types are duplicated in fragment.c -- keep in sync */
#define NAME_KEY fragment
#define ENTRY_TYPE fragment_t *
/* not defining HASHTABLE_USE_LOOKUPTABLE */
#define HASHTABLEX_HEADER 1
#define CUSTOM_FIELDS /* none */
#include "hashtablex.h"
#undef HASHTABLEX_HEADER

/* hashtable of fragment_entry_t entries for per-type ibl tables */
/* macros w/ name and types are duplicated in fragment.c -- keep in sync */
#define NAME_KEY ibl
#define ENTRY_TYPE fragment_entry_t
/* not defining HASHTABLE_USE_LOOKUPTABLE */
#ifdef HASHTABLE_STATISTICS
#    define HASHTABLE_ENTRY_STATS 1
#    define CUSTOM_FIELDS                                                            \
        ibl_branch_type_t branch_type;                                               \
        /* stats written from the cache must be unprotected by allocating separately \
         * FIXME: we could avoid this when protect_mask==0 by having a union here,   \
         * like we have with mcontext in the dcontext, but not worth the complexity  \
         * or space for debug-build-only stats                                       \
         */                                                                          \
        unprot_ht_statistics_t *unprot_stats;
#else
#    define CUSTOM_FIELDS ibl_branch_type_t branch_type;
#endif /* HASHTABLE_STATISTICS */
#define HASHTABLEX_HEADER 1
#include "hashtablex.h"
#undef HASHTABLEX_HEADER

#if defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH)
/* 3 macros w/ name and types are duplicated in fragment.c -- keep in sync */
#    define NAME_KEY app_pc
#    define ENTRY_TYPE app_pc
/* not defining HASHTABLE_USE_LOOKUPTABLE */
#    define CUSTOM_FIELDS /* none */
#    define HASHTABLEX_HEADER 1
#    include "hashtablex.h"
#    undef HASHTABLEX_HEADER
#endif /* defined(RETURN_AFTER_CALL) || defined (RCT_IND_BRANCH) */

/* We keep basic blocks and traces in separate hashtables.  This is to
 * speed up indirect_branch_lookup that looks for traces only, but it
 * means our lookup function has to look in both hashtables.  This has
 * no noticeable performance impact.  A strategy of having an
 * all-fragment hashtable and a trace-only hashtable that mirrors just
 * the traces of the all-fragment hashtable performs similarly but is
 * more complicated since fragments need two different next fields,
 * plus it uses more memory because traces are in two hashtables
 * simultaneously.
 *
 * FIXME: Shared bb IBL routines indirectly access only a few fields
 * from each fragment_table_t which will touch a separate cache line for
 * each.  However, trace IBL routines don't indirect so I don't expect
 * a performance hit of using the current struct layout.
 * FIXME: The bb IBL routines however are shared and therefore
 * indirect, so splitting the fragment_table_t in two compactable
 * structures may be worth trying.
 */
typedef struct _per_thread_t {
    ibl_table_t trace_ibt[IBL_BRANCH_TYPE_END]; /* trace IB targets */
    ibl_table_t bb_ibt[IBL_BRANCH_TYPE_END];    /* bb IB targets */
    fragment_table_t bb;
    fragment_table_t trace;
    fragment_table_t future;
    mutex_t fragment_delete_mutex;
    file_t tracefile;

    /* used for unlinking other threads' caches for flushing */
    bool could_be_linking;        /* accessing link data structs? */
    bool wait_for_unlink;         /* should this thread wait at synch point? */
    bool about_to_exit;           /* no need to flush if thread about to exit */
    bool flush_queue_nonempty;    /* is this thread's deletion queue nonempty? */
    event_t waiting_for_unlink;   /* synch bet flusher and flushee */
    event_t finished_with_unlink; /* ditto */
    event_t finished_all_unlink;  /* ditto */
    /* this lock controls all 4 vars above, plus linking/unlinking shared_syscall,
     * plus modifying queue of to-be-deleted thread-local vm regions
     */
    mutex_t linking_lock;
    bool soon_to_be_linking; /* tells flusher thread is at cache exit synch */
    /* for shared_deletion protocol */
    uint flushtime_last_update;
    /* for syscalls_synch_flush, only used to cache whether a thread was at
     * a syscall during early flushing stages for use in later stages.
     * not used while not flushing.
     */
    bool at_syscall_at_flush;
} per_thread_t;

#define FCACHE_ENTRY_PC(f) (f->start_pc + f->prefix_size)
#define FCACHE_PREFIX_ENTRY_PC(f) \
    (f->start_pc + f->prefix_size - FRAGMENT_BASE_PREFIX_SIZE(f->flags))
#define FCACHE_IBT_ENTRY_PC(f) (f->start_pc)

/* translation info pointer can be at end of any struct, so rather than have
 * 8 different structs we keep it out of the formal struct definitions
 */
#define FRAGMENT_STRUCT_SIZE(flags)                                                  \
    ((TEST(FRAG_IS_TRACE, (flags))                                                   \
          ? (TEST(FRAG_SHARED, (flags)) ? sizeof(trace_t) : sizeof(private_trace_t)) \
          : (TEST(FRAG_SHARED, (flags)) ? sizeof(fragment_t)                         \
                                        : sizeof(private_fragment_t))) +             \
     (TEST(FRAG_HAS_TRANSLATION_INFO, flags) ? sizeof(translation_info_t *) : 0))

#define FRAGMENT_EXIT_STUBS(f)                                                         \
    (TEST(FRAG_FAKE, (f)->flags)                                                       \
         ? (ASSERT(false && "fake fragment_t has no exit stubs!"), (linkstub_t *)NULL) \
         : ((linkstub_t *)(((byte *)(f)) + FRAGMENT_STRUCT_SIZE((f)->flags))))

/* selfmod copy size is stored at very end of fragment space */
#define FRAGMENT_SELFMOD_COPY_SIZE(f)                  \
    (ASSERT(TEST(FRAG_SELFMOD_SANDBOXED, (f)->flags)), \
     (*((uint *)((f)->start_pc + (f)->size - sizeof(uint)))))
#define FRAGMENT_SELFMOD_COPY_CODE_SIZE(f) (FRAGMENT_SELFMOD_COPY_SIZE(f) - sizeof(uint))
#define FRAGMENT_SELFMOD_COPY_PC(f)                    \
    (ASSERT(TEST(FRAG_SELFMOD_SANDBOXED, (f)->flags)), \
     ((f)->start_pc + (f)->size - FRAGMENT_SELFMOD_COPY_SIZE(f)))

#define FRAGMENT_TRANSLATION_INFO_ADDR(f)                                              \
    (TEST(FRAG_HAS_TRANSLATION_INFO, (f)->flags)                                       \
         ? ((translation_info_t **)(((byte *)(f)) + FRAGMENT_STRUCT_SIZE((f)->flags) - \
                                    sizeof(translation_info_t *)))                     \
         : ((INTERNAL_OPTION(safe_translate_flushed) &&                                \
             TEST(FRAG_WAS_DELETED, (f)->flags))                                       \
                ? &((f)->in_xlate.translation_info)                                    \
                : NULL))

#define HAS_STORED_TRANSLATION_INFO(f)              \
    (TEST(FRAG_HAS_TRANSLATION_INFO, (f)->flags) || \
     (INTERNAL_OPTION(safe_translate_flushed) && TEST(FRAG_WAS_DELETED, (f)->flags)))

#define FRAGMENT_TRANSLATION_INFO(f) \
    (HAS_STORED_TRANSLATION_INFO(f) ? (*(FRAGMENT_TRANSLATION_INFO_ADDR(f))) : NULL)

static inline const char *
fragment_type_name(fragment_t *f)
{
    if (TEST(FRAG_IS_TRACE_HEAD, f->flags))
        return "trace head";
    else if (TEST(FRAG_IS_TRACE, f->flags))
        return "trace";
    else
        return "bb";
}

/* Returns the end of the fragment body + any local stubs (excluding selfmod copy) */
cache_pc
fragment_stubs_end_pc(fragment_t *f);

/* Returns the end of the fragment body (excluding exit stubs and selfmod copy) */
cache_pc
fragment_body_end_pc(dcontext_t *dcontext, fragment_t *f);

bool
fragment_initialized(dcontext_t *dcontext);

void
fragment_init(void);

void
fragment_exit(void);

void
fragment_exit_post_sideline(void);

void
fragment_reset_init(void);

void
fragment_reset_free(void);

void
fragment_thread_init(dcontext_t *dcontext);

void
fragment_thread_exit(dcontext_t *dcontext);

bool
fragment_thread_exited(dcontext_t *dcontext);

/* re-initializes non-persistent memory */
void
fragment_thread_reset_init(dcontext_t *dcontext);

/* frees all non-persistent memory */
void
fragment_thread_reset_free(dcontext_t *dcontext);

#ifdef UNIX
void
fragment_fork_init(dcontext_t *dcontext);
#endif

fragment_t *
fragment_create(dcontext_t *dcontext, app_pc tag, int body_size, int direct_exits,
                int indirect_exits, int exits_size, uint flags);

/* Creates a new fragment_t+linkstubs from the passed-in fragment and
 * fills in linkstub_t and fragment_t fields, copying the fcache-related fields
 * from the passed-in fragment (so be careful how the fields are used).
 * Meant to be used to create a full fragment from a coarse-grain fragment.
 * Caller is responsible for freeing via fragment_free().
 */
fragment_t *
fragment_recreate_with_linkstubs(dcontext_t *dcontext, fragment_t *f_src);

/* Frees the storage associated with f.
 * Callers should use fragment_delete() instead of this routine, unless they
 * obtained their fragment_t from fragment_recreate_with_linkstubs().
 */
void
fragment_free(dcontext_t *dcontext, fragment_t *f);

void
fragment_add(dcontext_t *dcontext, fragment_t *f);

void
fragment_copy_data_fields(dcontext_t *dcontext, fragment_t *f_src, fragment_t *f_dst);

/* options for fragment_delete actions param
 * N.B.: these are NEGATIVE since callers care what's NOT done
 */
enum {
    FRAGDEL_ALL = 0x000,
    FRAGDEL_NO_OUTPUT = 0x001,
    FRAGDEL_NO_UNLINK = 0x002,
    FRAGDEL_NO_HTABLE = 0x004,
    FRAGDEL_NO_FCACHE = 0x008,
    FRAGDEL_NO_HEAP = 0x010,
    FRAGDEL_NO_MONITOR = 0x020,
    FRAGDEL_NO_VMAREA = 0x040,

    FRAGDEL_NEED_CHLINK_LOCK = 0x080,
};
void
fragment_delete(dcontext_t *dcontext, fragment_t *f, uint actions);

void
fragment_record_translation_info(dcontext_t *dcontext, fragment_t *f, instrlist_t *ilist);

void
fragment_remove_shared_no_flush(dcontext_t *dcontext, fragment_t *f);

void
fragment_unlink_for_deletion(dcontext_t *dcontext, fragment_t *f);

bool
fragment_prepare_for_removal(dcontext_t *dcontext, fragment_t *f);

/* Removes f from any IBT tables it is in. */
void
fragment_remove_from_ibt_tables(dcontext_t *dcontext, fragment_t *f, bool from_shared);

uint
fragment_remove_all_ibl_in_region(dcontext_t *dcontext, app_pc start, app_pc end);

/* Removes f from any hashtables -- BB, trace, future -- and IBT tables
 * it is in */
void
fragment_remove(dcontext_t *dcontext, fragment_t *f);

void
fragment_replace(dcontext_t *dcontext, fragment_t *f, fragment_t *new_f);

fragment_t *
fragment_lookup(dcontext_t *dcontext, app_pc tag);

fragment_t *
fragment_lookup_bb(dcontext_t *dcontext, app_pc tag);

fragment_t *
fragment_lookup_shared_bb(dcontext_t *dcontext, app_pc tag);

fragment_t *
fragment_lookup_trace(dcontext_t *dcontext, app_pc tag);

fragment_t *
fragment_lookup_same_sharing(dcontext_t *dcontext, app_pc tag, uint flags);

fragment_t *
fragment_pclookup(dcontext_t *dcontext, cache_pc pc, fragment_t *wrapper);

/* Performs a pclookup and if the result is a coarse-grain fragment, allocates
 * a new fragment_t+linkstubs.
 * Returns in alloc whether the returned fragment_t was allocated and needs to be
 * freed by the caller via fragment_free().
 * If no result is found, alloc is set to false.
 */
fragment_t *
fragment_pclookup_with_linkstubs(dcontext_t *dcontext, cache_pc pc,
                                 /*OUT*/ bool *alloc);

#ifdef DEBUG
fragment_t *
fragment_pclookup_by_htable(dcontext_t *dcontext, cache_pc pc, fragment_t *wrapper);
#endif

void
fragment_shift_fcache_pointers(dcontext_t *dcontext, fragment_t *f, ssize_t shift,
                               cache_pc start, cache_pc end, size_t old_size);

void
fragment_update_ibl_tables(dcontext_t *dcontext);

void
fragment_add_ibl_target(dcontext_t *dcontext, app_pc tag, ibl_branch_type_t branch_type);

/* future fragments */
future_fragment_t *
fragment_create_and_add_future(dcontext_t *dcontext, app_pc tag, uint flags);

void
fragment_delete_future(dcontext_t *dcontext, future_fragment_t *fut);

future_fragment_t *
fragment_lookup_future(dcontext_t *dcontext, app_pc tag);

future_fragment_t *
fragment_lookup_private_future(dcontext_t *dcontext, app_pc tag);

#ifdef RETURN_AFTER_CALL
app_pc
fragment_after_call_lookup(dcontext_t *dcontext, app_pc tag);
void
fragment_add_after_call(dcontext_t *dcontext, app_pc tag);
void
fragment_flush_after_call(dcontext_t *dcontext, app_pc tag);
uint
invalidate_after_call_target_range(dcontext_t *dcontext, app_pc text_start,
                                   app_pc text_end);
#endif /* RETURN_AFTER_CALL */

#if defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH)
/* case 9672: we split our RCT and RAC targets into per-module tables.
 * To support sharing, we separate the persisted from the live.
 * Typedef is in globals.h
 */
struct _rct_module_table_t {
    app_pc_table_t *persisted_table;
    app_pc_table_t *live_table;
    /* Optimization: to avoid walking the table to find entries in a
     * coarse unit's region we track the max and min entries in the
     * live table.  In the common case we can then do a straight copy
     */
    app_pc live_min;
    app_pc live_max;
};

#    ifdef UNIX
extern rct_module_table_t rct_global_table;
#    endif

void
rct_module_table_free(dcontext_t *dcontext, rct_module_table_t *permod, app_pc modpc);

bool
rct_module_table_set(dcontext_t *dcontext, app_pc modpc, app_pc_table_t *table,
                     rct_type_t which);

void
rct_module_table_persisted_invalidate(dcontext_t *dcontext, app_pc modpc);

bool
rct_module_persisted_table_exists(dcontext_t *dcontext, app_pc modpc, rct_type_t which);

uint
rct_module_live_entries(dcontext_t *dcontext, app_pc modpc, rct_type_t which);

app_pc_table_t *
rct_module_table_copy(dcontext_t *dcontext, app_pc modpc, rct_type_t which,
                      app_pc limit_start, app_pc limit_end);

void
rct_table_free(dcontext_t *dcontext, app_pc_table_t *table, bool free_data);

app_pc_table_t *
rct_table_copy(dcontext_t *dcontext, app_pc_table_t *src);

app_pc_table_t *
rct_table_merge(dcontext_t *dcontext, app_pc_table_t *src1, app_pc_table_t *src2);

uint
rct_table_persist_size(dcontext_t *dcontext, app_pc_table_t *table);

bool
rct_table_persist(dcontext_t *dcontext, app_pc_table_t *table, file_t fd);

app_pc_table_t *
rct_table_resurrect(dcontext_t *dcontext, byte *mapped_table, rct_type_t which);

#endif /* RETURN_AFTER_CALL || RCT_IND_BRANCH */

/* synchronization routine for sideline thread */
void
fragment_get_fragment_delete_mutex(dcontext_t *dcontext);

void
fragment_release_fragment_delete_mutex(dcontext_t *dcontext);

/*******************************************************************************
 * COARSE-GRAIN FRAGMENT HASHTABLE
 */

/* N.B.: if you change the coarse_table_t struct you must increase
 * PERSISTENT_CACHE_VERSION!
 */
typedef struct _app_to_cache_t {
    app_pc app;
    /* cache is absolute pc for non-frozen units, but a relative offset for frozen.
     * PR 203915: should we split into separate instantation to
     * use a 32-bit offset for frozen on x64?  For now not worth it.
     */
    cache_pc cache;
} app_to_cache_t;

/* 3 macros w/ name and types are duplicated in fragment.c -- keep in sync */
#define NAME_KEY coarse
#define ENTRY_TYPE app_to_cache_t
/* not defining HASHTABLE_USE_LOOKUPTABLE */
#define CUSTOM_FIELDS ssize_t mod_shift;
#define HASHTABLEX_HEADER 1
#include "hashtablex.h"
#undef HASHTABLEX_HEADER

void
fragment_coarse_htable_create(coarse_info_t *info, uint init_capacity,
                              uint init_th_capacity);

void
fragment_coarse_htable_free(coarse_info_t *info);

/* Merges the main and th htables from info1 and info2 into new htables for dst.
 * If !add_info2, makes room for but does not add entries from info2.
 * If !add_th_htable, creates but does not add entries to dst->th_htable.
 */
void
fragment_coarse_htable_merge(dcontext_t *dcontext, coarse_info_t *dst,
                             coarse_info_t *info1, coarse_info_t *info2, bool add_info2,
                             bool add_th_htable);

uint
fragment_coarse_num_entries(coarse_info_t *info);

/* Add coarse fragment represented by wrapper f to its hashtable */
void
fragment_coarse_add(dcontext_t *dcontext, coarse_info_t *info, app_pc tag,
                    cache_pc cache);

void
fragment_coarse_th_add(dcontext_t *dcontext, coarse_info_t *info, app_pc tag,
                       cache_pc cache);

void
fragment_coarse_th_unlink_and_add(dcontext_t *dcontext, app_pc tag, cache_pc stub_pc,
                                  cache_pc body_pc);

#ifdef DEBUG
/* only exported to allow an assert to avoid rank order issues in
 * push_pending_freeze() */
void
coarse_body_from_htable_entry(dcontext_t *dcontext, coarse_info_t *info, app_pc tag,
                              cache_pc res, cache_pc *stub_pc_out /*OUT*/,
                              cache_pc *body_pc_out /*OUT*/);
#endif

void
fragment_coarse_lookup_in_unit(dcontext_t *dcontext, coarse_info_t *info, app_pc tag,
                               cache_pc *stub_pc /*OUT*/, cache_pc *body_pc /*OUT*/);

cache_pc
fragment_coarse_lookup(dcontext_t *dcontext, app_pc tag);

void
fragment_coarse_wrapper(fragment_t *wrapper, app_pc tag, cache_pc body_pc);

fragment_t *
fragment_coarse_lookup_wrapper(dcontext_t *dcontext, app_pc tag, fragment_t *wrapper);

fragment_t *
fragment_lookup_fine_and_coarse(dcontext_t *dcontext, app_pc tag, fragment_t *wrapper,
                                linkstub_t *last_exit);

fragment_t *
fragment_lookup_fine_and_coarse_sharing(dcontext_t *dcontext, app_pc tag,
                                        fragment_t *wrapper, linkstub_t *last_exit,
                                        uint share_flags);

coarse_info_t *
get_fragment_coarse_info(fragment_t *f);

bool
coarse_is_trace_head_in_own_unit(dcontext_t *dcontext, app_pc tag, cache_pc stub,
                                 cache_pc body_in, bool body_valid,
                                 coarse_info_t *info_in);

bool
fragment_coarse_replace(dcontext_t *dcontext, coarse_info_t *info, app_pc tag,
                        cache_pc new_value);

/* Returns the tag for the coarse fragment whose body contains pc, as well as the
 * body pc in the optional OUT param.
 */
app_pc
fragment_coarse_pclookup(dcontext_t *dcontext, coarse_info_t *info, cache_pc pc,
                         /*OUT*/ cache_pc *body);

/* Creates a reverse lookup table.  For a non-frozen unit, the caller should only
 * do this while all threads are suspended, and should free the table before
 * resuming other threads.
 */
void
fragment_coarse_create_entry_pclookup_table(dcontext_t *dcontext, coarse_info_t *info);

void
fragment_coarse_free_entry_pclookup_table(dcontext_t *dcontext, coarse_info_t *info);

/* Returns the tag for the coarse fragment whose body _begins at_ pc */
app_pc
fragment_coarse_entry_pclookup(dcontext_t *dcontext, coarse_info_t *info, cache_pc pc);

void
fragment_coarse_unit_freeze(dcontext_t *dcontext, coarse_freeze_info_t *freeze_info);

uint
fragment_coarse_htable_persist_size(dcontext_t *dcontext, coarse_info_t *info,
                                    bool cache_table);

bool
fragment_coarse_htable_persist(dcontext_t *dcontext, coarse_info_t *info,
                               bool cache_table, file_t fd);

void
fragment_coarse_htable_resurrect(dcontext_t *dcontext, coarse_info_t *info,
                                 bool cache_table, byte *mapped_table);

/*******************************************************************************/

void
fragment_output(dcontext_t *dcontext, fragment_t *f);

bool
fragment_overlaps(dcontext_t *dcontext, fragment_t *f, byte *region_start,
                  byte *region_end, bool page_only, overlap_info_t *info_res,
                  app_pc *bb_tag);

bool
is_self_flushing(void);

bool
is_self_allsynch_flushing(void);

bool
is_self_couldbelinking(void);

/* The "couldbelinking" status is used for the efficient, lightweight "unlink"
 * flushing.  Rather than requiring suspension of every thread at a safe spot
 * from which that thread can be relocated, supporting immediate cache removal,
 * "unlink" flushing allows delayed removal by unlinking target fragments and
 * waiting for any threads inside them to come out naturally.  The flusher needs
 * safe access to unlink fragments belonging or used by other threads.  Any
 * thread that might change a fragment link status, or allocate memory in the
 * nonpersistent heap units (i#1791), must first enter a "couldbelinking" state.
 * The unlink flush process must wait for each target thread to exit this
 * "couldbelinking" state prior to proceeding with that thread.
 */

/* N.B.: can only call if target thread is suspended or waiting for flush */
bool
is_couldbelinking(dcontext_t *dcontext);

/* Returns false iff watch ends up being deleted */
bool
enter_nolinking(dcontext_t *dcontext, fragment_t *watch, bool temporary);

/* Returns false iff watch ends up being deleted */
bool
enter_couldbelinking(dcontext_t *dcontext, fragment_t *watch, bool temporary);

void
enter_threadexit(dcontext_t *dcontext);

uint
get_flushtime_last_update(dcontext_t *dcontext);

void
set_flushtime_last_update(dcontext_t *dcontext, uint val);

/* caller must hold shared_cache_flush_lock */
void
increment_global_flushtime(void);

void
set_at_syscall(dcontext_t *dcontext, bool val);

bool
get_at_syscall(dcontext_t *dcontext);

/****************************************************************************
 * FLUSHING
 *
 * Option 1
 * --------
 * Typical use is to flush a memory region in
 * flush_fragments_and_remove_region().  If custom executable areas editing
 * is required, the memory-region-flushing pair
 * flush_fragments_in_region_start() and flush_fragments_in_region_finish()
 * should be used (they MUST be used together). If no executable area removal
 * is needed use flush_fragments_from_region().
 *
 * Option 2
 * --------
 * The trio (also not usable individually)
 *   1) flush_fragments_synch_unlink_priv
 *   2) flush_fragments_unlink_shared
 *   3) flush_fragments_end_synch
 * provide flushing for non-memory regions via a list of fragments.  For
 * stage 2, the list of shared fragments should be chained by next_vmarea,
 * and vm_area_remove_fragment() should already have been called on each
 * fragment in the list.  This usage does not involve the executable_areas
 * lock at all.
 *
 * The exec_invalid parameter must be set to indicate whether
 * the executable area is being invalidated as well or this is just a
 * capacity flush (or a flush to change instrumentation).
 *
 * Option 3
 * --------
 * The trio (also not usable individually)
 *   1) flush_fragments_synch_priv
 *   2) flush_fragments_unlink_shared
 *   3) flush_fragments_end_synch
 * provide flushing for an arbitrary list of fragments without flushing
 * any vmarea. Stage 1 will synch with each thread and invoke the callback
 * with its dcontext, allowing the caller to do any per-thread activity.
 * For stage 2, the list of shared fragments should be chained by
 * next_vmarea, and vm_area_remove_fragment() should already have been
 * called on each fragment in the list.  This usage does not involve the
 * executable_areas lock at all.
 *
 * The thread_synch_callback will be invoked after synching with the
 * thread (per_thread_t.linking_lock is held). If shared syscalls and/or
 * special IBL transfer are thread-private, they will be unlinked for
 * each thread prior to invocation of the callback. The return value of
 * the callback indicates whether to relink these routines (true), or
 * wait for a later operation (such as vm_area_flush_fragments()) to
 * relink them later (false).
 *
 * All options
 * -----------
 * Possibly the thread_initexit_lock (if there are fragments to
 * flush), and always executable_areas lock for region flushing, ARE
 * HELD IN BETWEEN the routines, and no thread is could_be_linking()
 * in between.
 * WARNING: case 8572: the caller owning the thread_initexit_lock
 * is incompatible w/ suspend-the-world flushing!
 *
 * See the comments above is_couldbelinking() regarding the unlink flush safety
 * approach.
 */
/* Always performs synch. Invokes thread_synch_callback per thread. */
void
flush_fragments_synch_priv(dcontext_t *dcontext, app_pc base, size_t size,
                           bool own_initexit_lock,
                           bool (*thread_synch_callback)(dcontext_t *dcontext,
                                                         int thread_index,
                                                         dcontext_t *thread_dcontext)
                               _IF_DGCDIAG(app_pc written_pc));

/* If size>0, returns whether there is an overlap; if not, no synch is done.
 * If size==0, synch is always performed and true is always returned.
 */
bool
flush_fragments_synch_unlink_priv(dcontext_t *dcontext, app_pc base, size_t size,
                                  bool own_initexit_lock, bool exec_invalid,
                                  bool force_synchall _IF_DGCDIAG(app_pc written_pc));

void
flush_fragments_unlink_shared(dcontext_t *dcontext, app_pc base, size_t size,
                              fragment_t *list _IF_DGCDIAG(app_pc written_pc));

/* Invalidates (does not remove) shared fragment f from the private/shared
 * ibl tables.  Can only be called in flush stage 2.
 */
void
flush_invalidate_ibl_shared_target(dcontext_t *dcontext, fragment_t *f);

void
flush_fragments_end_synch(dcontext_t *dcontext, bool keep_initexit_lock);

void
flush_fragments_in_region_start(dcontext_t *dcontext, app_pc base, size_t size,
                                bool own_initexit_lock, bool free_futures,
                                bool exec_invalid,
                                bool force_synchall _IF_DGCDIAG(app_pc written_pc));

void
flush_fragments_in_region_finish(dcontext_t *dcontext, bool keep_initexit_lock);

void
flush_fragments_and_remove_region(dcontext_t *dcontext, app_pc base, size_t size,
                                  bool own_initexit_lock, bool free_futures);

void
flush_fragments_from_region(dcontext_t *dcontext, app_pc base, size_t size,
                            bool force_synchall,
                            void (*flush_completion_callback)(void *user_data),
                            void *user_data);

void
flush_fragments_custom_list(dcontext_t *dcontext, fragment_t *list,
                            bool own_initexit_lock, bool exec_invalid);

/* Invalidate all fragments in all caches.  Currently executed
 * fragments may be alive until they reach an exit.  It should be used
 * for correctness when finishing existing fragment execution is
 * acceptable. There are no guarantees on actual deletion from this
 * interface.
 */
void
invalidate_code_cache(void);

/* Flushes all areas stored in the vector toflush.
 * Synch is up to caller, but as locks cannot be held when flushing,
 * toflush needs to be thread-private.
 */
void
flush_vmvector_regions(dcontext_t *dcontext, vm_area_vector_t *toflush, bool free_futures,
                       bool exec_invalid);

/*
 ****************************************************************************/

/* locks must be exported for DEADLOCK_AVOIDANCE */
/* allows independent sequences of flushes and delayed deletions */
extern mutex_t shared_cache_flush_lock;
/* Global count of flushes, used as a timestamp for shared deletion.
 * Writes to it are protected by shared_cache_flush_lock.
 */
extern uint flushtime_global;

extern mutex_t client_flush_request_lock;
extern client_flush_req_t *client_flush_requests;

#ifdef PROFILE_RDTSC
void
profile_fragment_enter(fragment_t *f, uint64 end_time);

void
profile_fragment_dispatch(dcontext_t *dcontext);
#endif

void
fragment_self_write(dcontext_t *dcontext);

#ifdef SHARING_STUDY
void
print_shared_stats(void);
#endif

#define PROTECT_FRAGMENT_ENABLED(flags)                                              \
    (TEST(FRAG_SHARED, (flags)) ? TEST(SELFPROT_GLOBAL, dynamo_options.protect_mask) \
                                : TEST(SELFPROT_LOCAL, dynamo_options.protect_mask))

#ifdef DEBUG
void
study_all_hashtables(dcontext_t *dcontext);
#endif /* DEBUG */

#ifdef HASHTABLE_STATISTICS
/* in arch/interp.c */
int
append_ib_trace_last_ibl_exit_stat(dcontext_t *dcontext, instrlist_t *trace,
                                   app_pc speculate_next_tag);

static INLINE_ONCE hashtable_statistics_t *
get_ibl_per_type_statistics(dcontext_t *dcontext, ibl_branch_type_t branch_type)
{
    per_thread_t *pt = (per_thread_t *)dcontext->fragment_field;
    return &pt->trace_ibt[branch_type].unprot_stats->trace_ibl_stats[branch_type];
}
#endif /* HASHTABLE_STATISTICS */

#endif /* _FRAGMENT_H_ */
