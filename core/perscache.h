/* **********************************************************
 * Copyright (c) 2012-2019 Google, Inc.  All rights reserved.
 * Copyright (c) 2006-2008 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2006-2007 Determina Corp. */

/*
 * perscache.h - persistent code cache exports
 */

#ifndef _PERSCACHE_H_
#define _PERSCACHE_H_ 1

#include "module_shared.h" /* for module_digest_t */

/***************************************************************************
 * COARSE-GRAIN UNITS
 */

/* Information kept per coarse-grain region.
 * FIXME: for sharing we want to keep htable, stubs, and incoming
 * per unit, not per cache.  That will require changing fcache_unit_t and
 * having a way to map from app pc to unit.  For this first step toward
 * sharing we stick with a per-cache scheme with the multiple units
 * internal to the cache simply being ignored at this level.
 * Cache consistency needs to treat this as a unit in any case.
 *
 * Synchronization model: the struct lock controls writes to the
 * direct fields.  The cache, htable, th_htable, and stubs fields are
 * all assumed to only be written at init time, and thus internal
 * changes to the objects do not require the struct lock.
 * The struct lock is used at init time and for later writes to incoming
 * and frozen.
 * Destruction is assumed to involve all-thread-synch and so reads of
 * fields do not require the struct lock.  Just like reset, we rely on
 * all-thread-synch plus redirection to d_r_dispatch (rather than resuming
 * at suspended location, which we only do for native threads) to
 * allow us to free these shared structures that are read w/o locks.
 * FIXME: what about where we lack setcontext permission
 */
struct _coarse_info_t {
    bool frozen : 1;
    bool persisted : 1;
    bool in_use : 1; /* are we using this unit officially? */
    /* Flag to indicate whether we've calculated the rac/rct/hotp info that
     * we only need when persisting.
     */
    bool has_persist_info : 1;
    /* Case 9653: only the 1st coarse unit in a module's +x region(s) is persisted
     * Non-in-use units inherit this from their sources, but do not
     * change the status on deletion.
     */
    bool primary_for_module : 1;
    /* case 10525 where we keep the stubs read-only */
    bool stubs_readonly : 1;
#ifdef DEBUG
    /* A local info pointer has not escaped to any other thread.
     * We only use this flag to get around lock ordering issues (case 11064).
     */
    bool is_local : 1; /* no lock needed since only known to this thread */
#endif

    void *cache; /* opaque type internal to fcache.c */

    /* For frozen units, htable holds body pcs, while th_htable holds stub pcs.
     * FIXME case 8628: split these into a body table and a stub table for
     * non-frozen units.
     */
    void *htable;    /* opaque htable mapping app pc -> stub/cache entry point */
    void *th_htable; /* opaque htable mapping trace head app pc -> cache entry point */

    /* cache pclookups to avoid htable walk (i#658) */
    void *pclookup_last_htable; /* opaque htable caching recent non-entry pclookups */

    void *stubs; /* opaque special heap */

    cache_pc fcache_return_prefix;
    cache_pc trace_head_return_prefix;
    cache_pc ibl_ret_prefix;
    cache_pc ibl_call_prefix;
    cache_pc ibl_jmp_prefix;

    coarse_incoming_t *incoming;

    /* These fields are non-NULL only for frozen units.
     * Since htable entries are offsets we need to expose the cache and stub pcs
     */
    cache_pc cache_start_pc;
    cache_pc cache_end_pc;   /* last instr, not end of allocation */
    cache_pc stubs_start_pc; /* this is post-prefixes */
    cache_pc stubs_end_pc;   /* may not fill out full mmap_size if overestimate */
    /* if not persisted, this is the bounds of the region shared by
     * the frozen cache and stubs, assumed to start at cache_start_pc;
     * if persisted, this is the bounds of the entire mmapped file.
     */
    size_t mmap_size;
    /* Performance optimization for frozen units (critical for trace building) */
    void *pclookup_htable; /* opaque htable mapping cache entry point -> app pc */
    /* end frozen-only fields */

    /* Fields for persisted units */
    uint flags;       /* corresponds to PERSCACHE_ flags for persisted files */
    cache_pc mmap_pc; /* start of persisted mmapped file; size is mmap_size */
    /* if this is >0, we mapped the file in two different views:
     * 1) [mmap_pc,mmap_pc+mmap_ro_size)
     * 2) [mmap_pc+mmap_ro_size,mmap_pc+mmap_size)
     */
    size_t mmap_ro_size;
    /* case 9925: we may want to keep the file handle open for the duration */
    file_t fd;

    /* If we merged with a persisted file, we store the original size so
     * we can avoid re-merging with that on-disk file.
     */
    size_t persisted_source_mmap_size;

#if defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH)
    app_pc_table_t *rct_table;
    app_pc_table_t *rac_table;
    /* Case 9639: fill ibl tables from persisted rac/rct table
     * entries.  We only do once, storing here to enforce that.
     */
    uint ibl_pending_used; /* bitmask from (1<<(IBL_<type>-1)) so we use each once */
#endif

#ifdef HOT_PATCHING_INTERFACE
    /* Case 9995: array of hotp patch points matched at persist time, which
     * means they should have been made fine-grained and NOT be present
     * in the pcache.
     * Used for persisted units only; for this-run units it would only make
     * sense to use this if hotp did not ignore idempotent ppoint changes
     * at nudge time, which may be the case today but better to fix hotp.
     */
    app_rva_t *hotp_ppoint_vec;
    uint hotp_ppoint_vec_num; /* number of elements in array */
#endif

    /* case 10525: leave stubs as writable if written too many times */
    uint stubs_write_count;

    /* case 9521: we can have a second unit in the same region for new,
     * non-frozen coarse code if the primary unit is frozen.
     * Presumably frozen unit is larger so we put it first.
     */
    struct _coarse_info_t *non_frozen;

    /* FIXME: add debug-build stats on #fragments, #stubs,
     * #trace heads, etc.
     */

    /***************************************************
     * Fields below this point are preserved across a coarse_unit_reset_free(),
     * while those above are cleared.
     */

    /* Controls access to directly changing the fields of the struct, except
     * the incoming list */
    mutex_t lock;
    /* Controls the incoming list; separated to allow holding the src main
     * lock while changing target linked unit locks (case 9809) */
    mutex_t incoming_lock;

    /* FIXME: once we persist/share we will need official labels to identify
     * source modules
     */
    app_pc base_pc; /* base of vm area this unit covers */
    app_pc end_pc;  /* end of vm area this unit covers */
#ifdef DEBUG
    const char *module;
#endif
    /* MD5 of the module, used only for persisting but we calculate at load time
     * so we're comparing the in-memory image at a consistent point.
     */
    module_digest_t module_md5;
    /* persisted base */
    app_pc persist_base;
    /* persisted base minus cur base */
    ssize_t mod_shift;

    /* Only add a field here if it should be preserved across coarse_unit_reset_free */

}; /* typedef as "coarse_info_t" is in globals.h */

#if defined(X86) && defined(X64)
#    define COARSE_32_FLAG(info) (TEST(PERSCACHE_X86_32, (info)->flags) ? FRAG_32_BIT : 0)
#else
#    define COARSE_32_FLAG(info) 0
#endif

coarse_info_t *
coarse_unit_create(app_pc base_pc, app_pc end_pc, module_digest_t *digest,
                   bool for_execution);

void
coarse_unit_init(coarse_info_t *info, void *cache);

/* If caller holds change_linking_lock and info->lock, have_locks should be true */
void
coarse_unit_reset_free(dcontext_t *dcontext, coarse_info_t *info, bool have_locks,
                       bool unlink, bool abdicate_primary);

void
coarse_unit_free(dcontext_t *dcontext, coarse_info_t *info);

void
coarse_unit_mark_in_use(coarse_info_t *info);

/***************************************************************************
 * FROZEN UNITS
 */

/* For storing information needed during the freezing process */

typedef struct _pending_freeze_t {
    bool entrance_stub;
    bool trace_head;
    app_pc tag;
    cache_pc cur_pc;
    cache_pc link_cti_opnd; /* 4-byte pc-relative opnd to re-target */
    bool elide_ubr;         /* whether to elide the link, if that's an option */
    struct _pending_freeze_t *next;
} pending_freeze_t;

struct _coarse_freeze_info_t {
    coarse_info_t *src_info;
    coarse_info_t *dst_info;
    cache_pc cache_start_pc;
    cache_pc cache_cur_pc;
    cache_pc stubs_start_pc;
    cache_pc stubs_cur_pc;
    bool unlink;
    pending_freeze_t *pending;
#ifdef DEBUG
    /* statistics on frozen code expansion from original app code */
    size_t app_code_size;
    uint num_cbr;
    uint num_jmp;
    uint num_call;
    uint num_indbr;
    /* removed code: */
    uint num_elisions;
    /* trying to enumerate all the added code: */
    uint added_fallthrough;
    uint added_indbr_mangle; /* all but the stub and the (removable) jmp to stub */
    uint added_indbr_stub;   /* stub plus the jmp to it */
    uint added_jecxz_mangle;
#endif
}; /* typedef as "coarse_freeze_info_t" is in globals.h */

void
perscache_init(void);

void
perscache_fast_exit(void);

void
perscache_slow_exit(void);

bool
perscache_dirname(char *directory /* OUT */, uint directory_len);

void
coarse_units_freeze_all(bool in_place);

coarse_info_t *
coarse_unit_freeze(dcontext_t *dcontext, coarse_info_t *info, bool in_place);

void
transfer_coarse_stub(dcontext_t *dcontext, coarse_freeze_info_t *freeze_info,
                     cache_pc stub, bool trace_head, bool replace_outgoing);
void
transfer_coarse_stub_fix_trace_head(dcontext_t *dcontext,
                                    coarse_freeze_info_t *freeze_info, cache_pc stub);
void
transfer_coarse_fragment(dcontext_t *dcontext, coarse_freeze_info_t *freeze_info,
                         cache_pc body);

coarse_info_t *
coarse_unit_merge(dcontext_t *dcontext, coarse_info_t *info1, coarse_info_t *info2,
                  bool in_place);

/***************************************************************************
 * PERSISTENT CODE CACHE
 */

/* For storing frozen caches on disk */

enum {
    PERSISTENT_CACHE_MAGIC = 0x244f4952, /* RIO$ */
    PERSISTENT_CACHE_VERSION = 10,
};

/* Global flags we need to process if present in a persisted cache */
enum {
    /* Identify underlying architecture */
    PERSCACHE_X86_32 = 0x00000001,
    PERSCACHE_X86_64 = 0x00000002,
    /* FIXME: should we add cache line info?  Currently we have no
     * -pad_jmps for coarse bbs, coarse bbs are aligned to 1, our only
     * hotpatched jmps are in stubs which are 16-byte-aligned and 15 bytes
     * long, and we have -tls_align 1, so we have no cache line
     * dependences.
     */

    PERSCACHE_SEEN_BORLAND_SEH = 0x00000004,

    /* Does cache contain elided ubrs? */
    PERSCACHE_ELIDED_UBR = 0x00000008,

    /* Does cache contain return-after-call or RCT entries? */
    PERSCACHE_SUPPORT_RAC = 0x00000010,
    PERSCACHE_SUPPORT_RCT = 0x00000020,

    /* Does cache contain persisted RCT for entire module? */
    PERSCACHE_ENTIRE_MODULE_RCT = 0x00000040,

    /* Does cache support trace building? */
    PERSCACHE_SUPPORT_TRACES = 0x00000080,

    /* Does cache support separately mapping the writable portion? */
    PERSCACHE_MAP_RW_SEPARATE = 0x00000100,

    /* Case 9799: local exemption options are part of option string */
    PERSCACHE_EXEMPTION_OPTIONS = 0x00000200,

    /* Used only in coarse_info_t, not in coarse_persisted_info_t.
     * We load and use persisted RCT tables prior to full code consistency checks;
     * plus, we may continue using RCT tables after code consistency checks fail.
     * Thus, the code being valid is separate from the pcache file being loaded.
     * Xref case 10601.
     */
    PERSCACHE_CODE_INVALID = 0x00000400,
};

/* Consistency and security checking options */
enum {
    /* Checks on the app module */
    PERSCACHE_MODULE_MD5_SHORT = 0x00000001,    /* header + footer */
    PERSCACHE_MODULE_MD5_COMPLETE = 0x00000002, /* entire code region */
    /* Checks on our own generated file */
    PERSCACHE_GENFILE_MD5_SHORT = 0x00000004,    /* header */
    PERSCACHE_GENFILE_MD5_COMPLETE = 0x00000008, /* entire file */
    /* When to calculate gen-side module md5's
     * In 4.4 this will be stored at 1st execution, not load time (case 10601)
     */
    PERSCACHE_MODULE_MD5_AT_LOAD = 0x00000010, /* else, at persist time */
};

/* FIXME: share with hotp_module_sig_t in hotpatch.c
 * Xref case 9543 on combining all these module structs.
 * N.B.: the precise layout of the fields here is relied upon
 * in persist_modinfo_cmp()
 */
typedef struct _persisted_module_info_t {
    app_pc base; /* base of module at persist time */
    uint checksum;
    uint timestamp;
    /* Ordinarily we'd use size_t for image_size and code_size, but we
     * want the same header size for both 32 and 64 bit
     */
    uint64 image_size;
    uint64 code_size; /* sum of sizes of executable sections in module */
    uint64 file_version;

    /* FIXME case 10087: move to module list and share w/ module-level
     * process control, aslr?
     */
    /* FIXME: granularity: may want to support loading small pieces of the
     * cache+stubs at a time, to reduce cache capacity and consistency checks
     * and RAC security loosening.  Should then have separate md5's.
     */
    module_digest_t module_md5;
} persisted_module_info_t;

/* We put an MD5 of our generated file at the end, to make it easier
 * to exclude itself from the calc and to provide assurance the end
 * of the file was written.
 */
typedef struct _persisted_footer_t {
    /* Self-consistency: MD5 of the header or of the whole file.
     * We store them separately, even though file is superset, to allow
     * checking just the header even if we generated the whole.
     */
    module_digest_t self_md5; /* should we take module out of type name? */
    uint magic;
} persisted_footer_t;

/* The layout of a frozen coarse unit for persisting to disk.
 * For the htables, cache, and stubs, we store the raw data here and when
 * reading back in we generate separate header structs for each.
 *
 * The layout is split into two pieces: the header and the data.  For
 * each section of data (stubs, cache, htable) we store a length in
 * the header.  The lengths are stored in reverse order, allowing for
 * adding new sections (to the read-only-not-executable side at least)
 * that are ignored by older versions of the code.
 *
 * FIXME: we have our own format here which we keep page-aligned on disk and set
 * memory privileges properly ourselves upon loading in.  We could instead use
 * an image format (PE/ELF): with PE the kernel will set up the privileges for
 * us, though with ELF it's the user-mode loader that does that.  Our own format
 * lets us map sub-views of the file, though currently only the small header
 * data is not needed as a map.  An image format will make it easier to use
 * tools like debuggers with our caches.  It also has an impact on sharing and
 * copy-on-write.  But, the kernel may make assumptions about images that don't
 * apply to our files.
 */
typedef struct _coarse_persisted_info_t {
    /* Read-only **************************************/

    /* For verification */
    uint magic;
    uint version;

    /* The lengths of the two divisions in the file.
     * We limit entire file to 2GB on x64, but use size_t as
     * the natural size for all our length computations.
     */
    size_t header_len;
    size_t data_len;

    /* Unit-wide flags for state such as whether we've seen Borland SEH.
     * Uses the PERSCACHE_ flags.
     */
    uint flags;

    /* For diagnostics */
    uint build_number;

    /* Consistency with original file */
    persisted_module_info_t modinfo;

    /* the address range that this cache covers, offset from module_base */
    size_t start_offs;
    size_t end_offs;

    /* We require a match here; alternative is to put all uses in relocs */
    uint tls_offs_base; /* could be ushort */

    /* Now we store the lengths of each data section, in reverse
     * order, to allow for expansion */

    /* +rw data sections */
    size_t instrument_rw_len;

    /* +rwx data sections */
    size_t stubs_len;
    size_t ibl_jmp_prefix_len;
    size_t ibl_call_prefix_len;
    size_t ibl_ret_prefix_len;
    size_t trace_head_return_prefix_len;
    size_t fcache_return_prefix_len;

    /* +rx data sections */
    size_t cache_len;
    size_t post_cache_pad_len; /* included in cache_len, this lets us know the
                                * end of actual instrs in the cache */
    size_t pad_len;            /* padding to get +rx onto new page */
    size_t instrument_rx_len;
    size_t view_pad_len; /* padding to get cache|stubs on map boundary */

    /* +r data sections */
    size_t stub_htable_len;
    size_t cache_htable_len;
    size_t rct_htable_len;
    size_t rac_htable_len;
    size_t reloc_len;
#ifdef HOT_PATCHING_INTERFACE
    size_t hotp_patch_list_len; /* in bytes, like all the other *_len fields */
#endif
    size_t instrument_ro_len;

    /* Case 9799: pcache-affecting options that differ from default values */
    size_t option_string_len;

    /* Add length of new +r data section here (header grows downward)
     * header_len indicates the start of the data section
     */

    /* Data sections ****************************************/
    /* option_string is the 1st data section; it may be padded to a 4-byte alignment */

    /* Add new data section here (data grows upward across versions) */

    /* Client data */

#ifdef HOT_PATCHING_INTERFACE
    /* Hotp patch points matched at persist time to avoid flushing if
     * the same vulns are active in the current run (case 9969).
     */
#endif

    /* Relocations
     * FIXME case 9581 NYI: other than app code relocs, all
     * we add w/ coarse bbs are "call->push immed" manglings.  Once have traces,
     * also stay-on-trace cmp.  No off-fragment jmps are currently allowed
     * except for fcache/trace-head return and ibl, which are indirected.
     * For app relocs we can either store in our own format at freeze time
     * or do all processing at load time.
     * FIXME: our non-entry pclookup is slow -- how slow will applying relocs be?
     * FIXME case 9649: We could make our own call->push manglings
     * PIC using pc-relative addressing on x86-64.
     */

    /* RAC and RCT tables -- RCT only for Borland SEH
     * FIXME case 8648: instead keep as flag hidden in msb of main htable?
     *   won't work for persisting the entire RCT tables, but will for Borland
     *   and RAC where all targets are present in cache.
     * FIXME case 9777: we're loosening security by allowing ret to target any
     * after-call executed in any prior run of this app or whatever
     * app is producing, instead of just this run.
     */

    /* Hashtables
     * Up to the table what to persist: current plan has the struct and the table
     * here, and on load we make a copy of the struct so that the lock can be
     * writable.
     */

    /* Padding to get +x section on new page */

    /* Client +x gencode */

    /****** Begin offset-sensitive sequence */
    /* We have one single contiguous piece of memory containing our cache and
     * stubs which must be kept together, as they contain relative jmps to each
     * other, including to the prefixes at the top of the stubs.
     *
     * --------------------------------------
     * executable below here
     *
     *   code cache
     *
     * read-only above here
     * --------------------------------------
     * writable below here
     *   FIXME case 9650: we can make the prefixes read-only after
     *   loading if we put them on their own page
     *
     *   fcache_return_prefix
     *   trace_head_return_prefix
     *   ibl_ret_prefix
     *   ibl_call_prefix
     *   ibl_jmp_prefix
     *   stubs
     *
     ****** End offset-sensitive sequence */

    /* Client writable data */

    /* persisted_footer_t here */

    /* We place a guard page here at load time if we fill out the allocation
     * region.  We don't need one in front since we have read-only pages there.
     */

} coarse_persisted_info_t;

bool
coarse_unit_persist(dcontext_t *dcontext, coarse_info_t *info);

coarse_info_t *
coarse_unit_load(dcontext_t *dcontext, app_pc start, app_pc end, bool for_execution);

bool
exists_coarse_ibl_pending_table(dcontext_t *dcontext, coarse_info_t *info,
                                ibl_branch_type_t branch_type);

/* Checks for enough space on the volume where persisted caches are stored */
bool
coarse_unit_check_persist_space(file_t fd_in /*OPTIONAL*/, size_t size_needed);

/* If pc is in a module, marks that module as exempted (case 9799) */
void
mark_module_exempted(app_pc pc);

#endif /* _PERSCACHE_H_ */
