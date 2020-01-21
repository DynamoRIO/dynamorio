/* **********************************************************
 * Copyright (c) 2012-2019 Google, Inc.  All rights reserved.
 * Copyright (c) 2005-2010 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2005-2007 Determina Corp. */

/*
 * aslr.c - ASLR: address space layout randomization from user mode
 */

#include "../globals.h"
#include "ntdll.h"
#include "os_private.h"
#include "aslr.h"
#include "instr.h"  /* for instr_t and OP_* opcodes */
#include "decode.h" /* for decode_opcode */
#ifdef GBOP
#    include "gbop.h"
#    include "../fragment.h"
#    include "../hotpatch.h"
#endif
#include "../module_shared.h"

enum { ASLR_MAP_GRANULARITY = 64 * 1024 }; /* 64KB - OS allocation granularity */

/* A signature appended to relocated files in our DLL cache providing
 * stronger consistency check between source and target.
 *
 * Note that this uses another page or sector on disk but at least we
 * don't waste another file and directory entry and any additional
 * security descriptors.  Raw reads of data after the end of a
 * MEM_IMAGE may result in a new file cache mapping, yet unlikely to
 * be worse in performance or memory than having a separate
 * file. FIXME: should measure
 */
typedef struct {
    module_digest_t original_source;
    module_digest_t relocated_target;

    /* minimal file corruption check.  A mismatched signature is most
     * likely due to version mismatch, or power failure.  Note that we
     * do not require guaranteed order of flushing, so a matching
     * signature doesn't guarantee that the previous blocks are
     * consistently written.  We must maintain internal file
     * consistency by making sure that any failed file write
     * immediately terminates further work, such incomplete file
     * prefixes should never be published under well-known name.
     *
     */
    uint magic;
    /* although old files should be invalidated anyways, in case we'd
     * want to report suspiciously corrupt files we better be sure
     * we're not matching against the wrong version.
     */
    uint version;
    /* do not add any fields after version - it has to be last word in file */
} aslr_persistent_digest_t;

/* version number for file signature */
enum { ASLR_PERSISTENT_CACHE_VERSION = 1 };
/* magic footer: ADPE */
enum { ASLR_PERSISTENT_CACHE_MAGIC = 0x45504441 };

/* all ASLR state protected by this lock */
DECLARE_CXTSWPROT_VAR(static mutex_t aslr_lock, INIT_LOCK_FREE(aslr_lock));

/* We keep these vars on the heap for selfprot (case 8074). */
typedef struct _aslr_last_dll_bounds_t {
    app_pc end;
    /* used by ASLR_RANGE_BOTTOM_UP to capture failures
     * FIXME: should allow UnmapViewOfSection to rewind last DLL */
    app_pc start;
} aslr_last_dll_bounds_t;

static aslr_last_dll_bounds_t *aslr_last_dll_bounds;

/* FIXME: case 6739 to properly keep track on UnmapViewOfSection we
 * should either QueryMemory for the jitter block or keep preceding
 * padding plus the modules we've bumped into a vmarea.
 */

/* FIXME: ASLR_RANGE_TOP_DOWN needs aslr_last_dll_bounds->start and not the end */

/* used for ASLR_TRACK_AREAS and ASLR_AVOID_AREAS.  Tracks preferred
 * address ranges where a DLL would usually be located without ASLR.
 * data is base of current mapping of rebased DLL that would be in that area
 * Kept on the heap for selfprot (case 7957).
 */
vm_area_vector_t *aslr_wouldbe_areas;

/* used for ASLR_HEAP and ASLR_HEAP_FILL - tracks added pad areas that should be freed.
 * Data is base of associated real heap allocation that precedes allocation,
 *
 * FIXME: (TOTEST) We currently expect to be able to lookup the pad
 * region whenever the application heap region is freed, if any
 * version of Windows allows a subregion of original to be freed or
 * free crossing boundaries, we'll just add a real backmap as well.
 * Kept on the heap for selfprot (case 7957).
 */
vm_area_vector_t *aslr_heap_pad_areas;

/* shared Object directory for publishing Sections */
HANDLE shared_object_directory = INVALID_HANDLE_VALUE;

/* file_t directory of relocated DLL cache - shared
 * FIXME: should have one according to starting user SID
 */
HANDLE relocated_dlls_filecache_initial = INVALID_HANDLE_VALUE;

#define KNOWN_DLLS_OBJECT_DIRECTORY L"\\KnownDlls"
HANDLE known_dlls_object_directory = INVALID_HANDLE_VALUE;

#define KNOWN_DLL_PATH_SYMLINK L"KnownDllPath"
wchar_t known_dll_path[MAX_PATH];
/* needed even by consumers to be handle NtOpenSection */

/* forwards */
#ifdef DEBUG
static bool
aslr_doublecheck_wouldbe_areas(void);
#endif

static void
aslr_free_heap_pads(void);
static app_pc
aslr_reserve_initial_heap_pad(app_pc preferred_base, size_t reserve_offset);

static bool
aslr_publish_file(const wchar_t *module_name);

static void
aslr_process_worklist(void);

static HANDLE
open_relocated_dlls_filecache_directory(void);
static void
aslr_get_known_dll_path(wchar_t *w_known_dll_path, /* OUT */
                        uint max_length_characters);
static bool
aslr_generate_relocated_section(IN HANDLE unmodified_section,
                                IN OUT app_pc *new_base, /* presumably random */
                                bool search_fitting_base, OUT app_pc *mapped_base,
                                OUT size_t *mapped_size,
                                OUT module_digest_t *file_digest);

void
aslr_free_dynamorio_loadblock(void);
/* end of forwards */

void
aslr_init(void)
{
    /* big delta should be harder to guess or brute force */
    size_t big_delta;
    ASSERT(ALIGNED(DYNAMO_OPTION(aslr_dll_base), ASLR_MAP_GRANULARITY));
    ASSERT_NOT_IMPLEMENTED(!TESTANY(~(ASLR_DLL | ASLR_STACK | ASLR_HEAP | ASLR_HEAP_FILL),
                                    DYNAMO_OPTION(aslr)));
    ASSERT_NOT_IMPLEMENTED(
        !TESTANY(~(ASLR_SHARED_INITIALIZE | ASLR_SHARED_INITIALIZE_NONPERMANENT |
                   ASLR_SHARED_CONTENTS | ASLR_SHARED_PUBLISHER | ASLR_SHARED_SUBSCRIBER |
                   ASLR_SHARED_ANONYMOUS_CONSUMER | ASLR_SHARED_WORKLIST |
                   ASLR_SHARED_FILE_PRODUCER | ASLR_ALLOW_ORIGINAL_CLOBBER |
                   ASLR_RANDOMIZE_EXECUTABLE | ASLR_AVOID_NET20_NATIVE_IMAGES |
                   ASLR_SHARED_PER_USER),
                 DYNAMO_OPTION(aslr_cache)));
    ASSERT_NOT_IMPLEMENTED(
        !TESTANY(~(ASLR_PERSISTENT_PARANOID | ASLR_PERSISTENT_SOURCE_DIGEST |
                   ASLR_PERSISTENT_TARGET_DIGEST | ASLR_PERSISTENT_SHORT_DIGESTS |
                   ASLR_PERSISTENT_PARANOID_TRANSFORM_EXPLICITLY |
                   ASLR_PERSISTENT_PARANOID_PREFIX),
                 DYNAMO_OPTION(aslr_validation)));

    ASSERT_NOT_IMPLEMENTED(
        !TESTANY(~(ASLR_INTERNAL_SAME_STRESS | ASLR_INTERNAL_RANGE_NONE |
                   ASLR_INTERNAL_SHARED_NONUNIQUE),
                 INTERNAL_OPTION(aslr_internal)));
    ASSERT_NOT_IMPLEMENTED(
        !TESTANY(~(ASLR_TRACK_AREAS | ASLR_DETECT_EXECUTE | ASLR_REPORT),
                 DYNAMO_OPTION(aslr_action)));
    /* FIXME: NYI ASLR_AVOID_AREAS|ASLR_RESERVE_AREAS|
     * ASLR_DETECT_READ|ASLR_DETECT_WRITE|
     * ASLR_HANDLING|ASLR_NORMALIZE_ID
     */

    ASSERT_CURIOSITY(!TEST(ASLR_RANDOMIZE_EXECUTABLE, DYNAMO_OPTION(aslr_cache)) ||
                     TEST(ASLR_ALLOW_ORIGINAL_CLOBBER, DYNAMO_OPTION(aslr_cache)) &&
                         "case 8902 - need to duplicate handle in child");
    /* case 8902 tracks the extra work if we want to support this
     * non-recommended configuration */

    ASSERT(ASLR_CLIENT_DEFAULT == 0x7);
    ASSERT(ASLR_CACHE_DEFAULT == 0x192); /* match any numeric use in optionsx.h */
#ifdef GBOP
    ASSERT(GBOP_CLIENT_DEFAULT == 0x6037);
#endif

    aslr_last_dll_bounds =
        HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, aslr_last_dll_bounds_t, ACCT_OTHER, PROTECTED);
    aslr_last_dll_bounds->start = NULL;
    big_delta = get_random_offset(DYNAMO_OPTION(aslr_dll_offset));
    aslr_last_dll_bounds->end = (app_pc)ALIGN_FORWARD(
        DYNAMO_OPTION(aslr_dll_base) + big_delta, ASLR_MAP_GRANULARITY);
    LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1,
        "ASLR: -aslr " PFX ", dll end=" PFX ", "
        "base=" PFX ", offset=" PFX " -> delta=" PFX ", pad=" PFX "\n",
        DYNAMO_OPTION(aslr), aslr_last_dll_bounds->end, DYNAMO_OPTION(aslr_dll_base),
        DYNAMO_OPTION(aslr_dll_offset), big_delta, DYNAMO_OPTION(aslr_dll_pad));

    VMVECTOR_ALLOC_VECTOR(aslr_wouldbe_areas, GLOBAL_DCONTEXT,
                          /* allow overlap due to conflicting DLLs */
                          VECTOR_SHARED | VECTOR_NEVER_MERGE_ADJACENT, aslr_areas);
    VMVECTOR_ALLOC_VECTOR(aslr_heap_pad_areas, GLOBAL_DCONTEXT,
                          VECTOR_SHARED | VECTOR_NEVER_MERGE, aslr_pad_areas);

    if (DYNAMO_OPTION(aslr_dr)) {
        /* free loadblocks if injected by parent */
        aslr_free_dynamorio_loadblock();
    } else {
        /* note if parent had the flag enabled while the child doesn't
         * we won't risk freeing */
    }

    if (TEST(ASLR_HEAP, DYNAMO_OPTION(aslr))) {
        /* we only reserve a random padding from the beginning of memory
         * and let the OS handle all other allocations normally
         */
        app_pc big_front_pad_base = aslr_reserve_initial_heap_pad(
            NULL /* earliest possible */, DYNAMO_OPTION(aslr_heap_reserve_offset));

        /* FIXME: If we want to consider ASLR_HEAP (but not
         * ASLR_HEAP_FILL) as a default option we may want to use this
         * padding as the randomization for our own memory.  If we
         * want to avoid address space fragmentation for important
         * services, we may want to add the initial padding before
         * vmm_heap_init() disable -vm_max_offset 0x0 and use -vm_base 0x0. */

        /* FIXME: Our large reservation maybe too large to fit in
         * front of the executable, when we're not in early there may
         * be heap regions already allocated.  While apps commonly
         * start at 0x00400000, many windows services start at
         * 0x01000000 (16MB) and the initial hole may be too small to
         * randomize anyways.
         *
         * Office apps start at 0x30000000 so they may end up having
         * two heap regions if an attacker is able to control memory
         * allocations.  We only use the smaller
         * aslr_heap_exe_reserve_offset for after the executable in
         * case the original mapping was before the imagebase.
         *
         */

        /* FIXME: though just grabbing big and small usually works
         * should just fill in any space in front of the
         * executable,
         *
         * FIXME: add a random pad after the executable to make sure
         * no heap allocation will eventually be at a predictable
         * location.
         */

        app_pc small_pad_after_executable_base =
            aslr_reserve_initial_heap_pad(NULL /* FIXME: should be after executable */,
                                          DYNAMO_OPTION(aslr_heap_exe_reserve_offset));
    }

    /* initialize shared object directory - note that this should be
     * done in a high privilege process (e.g. winlogon.exe) that may
     * otherwise have no other role to serve in ASLR_SHARED_CONTENTS
     */
    if (TEST(ASLR_SHARED_INITIALIZE, DYNAMO_OPTION(aslr_cache))) {
        HANDLE initialize_directory;
        NTSTATUS res =
            nt_initialize_shared_directory(&initialize_directory, true /* permanent */);
        /* we currently don't need to do anything else with this
         * handle, unless we can't make the object permanent then may
         * want to 'leak' the handle to persist the object directory
         * until this process dies.
         */
        /* FIXME: would be nice to provide a drcontrol -shared
         * -destroy (using NtMakeTemporaryObject()) to clear the permanent directory
         * -init to recreate it for easier testing and saving a reboot.
         */
        /* FIXME: Note that in a model in which per-session or
         * per-user sharing is allowed we may have extra levels to
         * create.  Otherwise, this nt_close_object_directory() can be
         * done inside nt_initialize_shared_directory() for permanent
         * directories.
         */
        if (NT_SUCCESS(res)) {
            nt_close_object_directory(initialize_directory);
        } else {
            /* STATUS_PRIVILEGE_NOT_HELD (0xc0000061) is an expected
             * failure code for low privileged processes.  Note for
             * testing may need a dummy process with high enough
             * privileges
             */

            /* FIXME: may want to make this non-internal flag to allow
             * simple experiments with unprivileged processes in
             * release builds as well
             */
            ASSERT_CURIOSITY(res == STATUS_PRIVILEGE_NOT_HELD);
            if (TEST(ASLR_SHARED_INITIALIZE_NONPERMANENT, DYNAMO_OPTION(aslr_cache))) {
                res = nt_initialize_shared_directory(&initialize_directory,
                                                     false /* temporary */);
                ASSERT(NT_SUCCESS(res) && "unable to initialize");
                /* must 'leak' initialize_directory to persist
                 * directory until process terminates,
                 * so there is no corresponding nt_close_object_directory()
                 */
            }
        }
    }

    if (TESTANY(ASLR_SHARED_SUBSCRIBER | ASLR_SHARED_PUBLISHER,
                DYNAMO_OPTION(aslr_cache))) {
        /* Open shared DLL object directory '\Determina\SharedCache' */
        /* publisher will ask for permission to create objects in that
         * directory, consumer needs read only access */
        /* FIXME: this should change to become SID related */
        NTSTATUS res = nt_open_object_directory(
            &shared_object_directory, DYNAMORIO_SHARED_OBJECT_DIRECTORY,
            TEST(ASLR_SHARED_PUBLISHER, DYNAMO_OPTION(aslr_cache)));
        /* Only trusted publishers should be allowed to publish in the
         * SharedCache */
        /* Note  */

        /* if any of these fail in release build (most likely if the
         * root is not created, or it is created with restrictive
         * permissions) we won't be able to publish named sections.
         * Not a critical failure though.
         */

        /* FIXME: should test shared_object_directory before any
         * consumer requests, so that we don't waste any time trying
         * to request sharing */
        ASSERT_CURIOSITY(NT_SUCCESS(res) && "can't open \\Determina\\SharedCache");
    }

    if (DYNAMO_OPTION(track_module_filenames) ||
        TESTANY(ASLR_SHARED_SUBSCRIBER | ASLR_SHARED_ANONYMOUS_CONSUMER |
                    ASLR_SHARED_PUBLISHER /* just in case */,
                DYNAMO_OPTION(aslr_cache))) {
        /* we'll need to match sections from \KnownDlls, note that all
         * direct or indirect consumers have to handle NtOpenSection()
         * here to deal with KnownDlls
         */
        NTSTATUS res = nt_open_object_directory(&known_dlls_object_directory,
                                                KNOWN_DLLS_OBJECT_DIRECTORY, false);
        ASSERT(NT_SUCCESS(res));

        /* open the \KnowdnDlls\KnownDllPath directory */
        aslr_get_known_dll_path(known_dll_path, BUFFER_SIZE_ELEMENTS(known_dll_path));
    }

    if (TESTANY(ASLR_SHARED_PUBLISHER | ASLR_SHARED_ANONYMOUS_CONSUMER,
                DYNAMO_OPTION(aslr_cache))) {
        /* Open shared cache file directory */
        relocated_dlls_filecache_initial = open_relocated_dlls_filecache_directory();

        /* FIXME: may need to open one shared and in addition one
         * per-user
         */

        /* FIXME: a ASLR_SHARED_FILE_PRODUCER | ASLR_SHARED_WORKLIST
         * producer may want to be able to write to the filecache
         * directory
         */
    }

    if (TEST(ASLR_SHARED_WORKLIST, DYNAMO_OPTION(aslr_cache))) {
        aslr_process_worklist();
    }

    /* FIXME: case 6725 ASLR functionality is not fully dynamic.  The
     * only state that needs to be set up is the above random number,
     * which we can just always initialize here.  Yet not enough for
     * the product:
     *  o we can't really undo changes, so not very useful to begin with, but
     *    at least DLLs after a change can be controlled
     *  o not planning on synchronizing options, yet may allow nudge to do so
     *  o post_syscall mappings does attempt to handle dynamic changes, not tested
     */
    if (DYNAMO_OPTION(aslr) == ASLR_DISABLED)
        return;
}

void
aslr_exit(void)
{
    if (TEST(ASLR_TRACK_AREAS, DYNAMO_OPTION(aslr_action)) &&
        is_module_list_initialized()) {
        /* doublecheck and print entries to make sure they match */
        DOLOG(1, LOG_VMAREAS, { print_modules_safe(GLOBAL, DUMP_NOT_XML); });
        ASSERT(aslr_doublecheck_wouldbe_areas());
    }
    /* dynamic option => free no matter the option value now */

    /* at startup: ASLR_TRACK_AREAS */
    vmvector_delete_vector(GLOBAL_DCONTEXT, aslr_wouldbe_areas);

    /* at startup: ASLR_HEAP_FILL|ASLR_HEAP */
    aslr_free_heap_pads();
    vmvector_delete_vector(GLOBAL_DCONTEXT, aslr_heap_pad_areas);

    if (shared_object_directory != INVALID_HANDLE_VALUE) {
        ASSERT_CURIOSITY(TEST(ASLR_SHARED_CONTENTS, DYNAMO_OPTION(aslr_cache)));
        nt_close_object_directory(shared_object_directory);
    }

    if (known_dlls_object_directory != INVALID_HANDLE_VALUE) {
        ASSERT_CURIOSITY(DYNAMO_OPTION(track_module_filenames) ||
                         TEST(ASLR_SHARED_CONTENTS, DYNAMO_OPTION(aslr_cache)));
        nt_close_object_directory(known_dlls_object_directory);
    }

    if (relocated_dlls_filecache_initial != INVALID_HANDLE_VALUE) {
        ASSERT_CURIOSITY(TEST(ASLR_SHARED_CONTENTS, DYNAMO_OPTION(aslr_cache)));
        close_handle(relocated_dlls_filecache_initial);
    }

    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, aslr_last_dll_bounds, aslr_last_dll_bounds_t,
                   ACCT_OTHER, PROTECTED);
    aslr_last_dll_bounds = NULL;

    /* always release lock in case -aslr was dynamically changed
     * although currently it is not dynamic
     */
    DELETE_LOCK(aslr_lock);
}

void
aslr_thread_init(dcontext_t *dcontext)
{
}

void
aslr_thread_exit(dcontext_t *dcontext)
{
}

/* ASLR random range choice */
/* use aslr_get_next_base() to start using a range, and in combination
 * with aslr_update_failed() on failure to use it, and
 * aslr_update_view_size() to flag success and proceed to the next base.
 */
static app_pc
aslr_get_next_base(void)
{
    /* although the loader holds a lock for the DLL mappings, other
     * MapViewOfFile calls may be racy.  If really serialzed by app,
     * there will never be contention on the locks grabbed here.
     */

    size_t jitter = get_random_offset(DYNAMO_OPTION(aslr_dll_pad));
    app_pc returned_base;
    /*
     * FIXME: [minor security] Although DLLs are definitely not loaded
     * racily, if we are using this for other potentially racy
     * allocations from the same region we may have races.  The
     * aslr_last_dll_bounds->end won't be updated so multiple callers may get
     * based not far from the same last end.  If aslr_dll_pad is
     * comparable to the size of an average mapping, the jitter here
     * will make it possible for multiple racy callers to receive
     * bases that may succeed.  Nevertheless, that is not really
     * necessary nor sufficient to avoid collisions.  Still even
     * though on collision we'll currently give up attackers can't
     * rely much on this.
     */

    d_r_mutex_lock(&aslr_lock);
    /* note that we always lose the low 16 bits of randomness of the
     * padding, so adding to last dll page-aligned doesn't matter */
    aslr_last_dll_bounds->start = aslr_last_dll_bounds->end + jitter;
    aslr_last_dll_bounds->start =
        (app_pc)ALIGN_FORWARD(aslr_last_dll_bounds->start, ASLR_MAP_GRANULARITY);
    returned_base = aslr_last_dll_bounds->start; /* for racy callers */
    d_r_mutex_unlock(&aslr_lock);

    LOG(THREAD_GET, LOG_SYSCALLS | LOG_VMAREAS, 1, "ASLR: next dll recommended=" PFX "\n",
        returned_base);
    return returned_base;
}

/* preverify the range is available, leaving possibility of failure
 * only to race.  Allows us to skip ranges that get in our way,
 * especially common when used for ASLR sharing, where we quickly
 * fragment our address space when DLLs are generated by multiple
 * processs.
 */
/* returns NULL if no valid range exists */
static app_pc
aslr_get_fitting_base(app_pc requested_base, size_t view_size)
{
    bool available = true;
    app_pc current_base = requested_base;

    ASSERT(ALIGNED(current_base, ASLR_MAP_GRANULARITY));
    /* currently march forward through OS allocated regions */

    do {
        app_pc allocated_base;
        size_t size;
        if ((ptr_uint_t)(current_base + view_size) > DYNAMO_OPTION(aslr_dll_top)) {
            /* FIXME: case 6739 could try to wrap around (ONCE!) */
            ASSERT_CURIOSITY((ptr_uint_t)current_base <= DYNAMO_OPTION(aslr_dll_top) ||
                             /* case 9844: suppress for short regression for now */
                             check_filter("win32.reload-race.exe",
                                          get_short_name(get_application_name())));
            ASSERT_CURIOSITY(false && "exhausted DLL range" ||
                             /* case 9378: suppress for short regression for now */
                             check_filter("win32.reload-race.exe",
                                          get_short_name(get_application_name())));
            return NULL;
        }

        size = get_allocation_size(current_base, &allocated_base);
        if (size == 0) {
            /* very unusual, can't have passed into kernel ranges */
            ASSERT_NOT_REACHED();
            return NULL;
        }

        /* note that get_allocation_size() returns allocation size of
         * non FREE regions, while for FREE regions is the available
         * region size (exactly what we need)
         */
        if (allocated_base != NULL /* taken, skip */) {
            ASSERT(current_base < allocated_base + size);
            current_base = allocated_base + size;
            /* skip unusable unaligned MEM_FREE region */
            current_base = (app_pc)ALIGN_FORWARD(current_base, ASLR_MAP_GRANULARITY);
            available = false;
        } else {                    /* free */
            if (size < view_size) { /* we don't fit in free size, skip */
                available = false;
                ASSERT(size > 0);
                current_base = current_base + size;
                /* free blocks should end aligned at allocation granularity  */
                ASSERT_CURIOSITY(ALIGNED(current_base, ASLR_MAP_GRANULARITY));
                /* can't be too sure - could be in the middle of freed TEB entries */
                current_base = (app_pc)ALIGN_FORWARD(current_base, ASLR_MAP_GRANULARITY);
            } else {
                /* we can take this, unless someone beats us */
                available = true;
            }
        }
    } while (!available);

    if (requested_base != current_base) {
        /* update our expectations, so that aslr_update_view_size()
         * doesn't get surprised */
        d_r_mutex_lock(&aslr_lock);
        if (aslr_last_dll_bounds->start == requested_base) {
            aslr_last_dll_bounds->start = current_base;
        } else {
            /* racy requests? */
            ASSERT_CURIOSITY(false && "aslr_get_fitting_base: racy ASLR mapping");
            ASSERT_NOT_TESTED();
        }
        d_r_mutex_unlock(&aslr_lock);
    }
    ASSERT(ALIGNED(current_base, ASLR_MAP_GRANULARITY));
    return current_base;
}

/* update on failure, if request_new is true, we should look for a
 * better fit given the module needed_size.  Note requested_base is
 * just a hint for what we have tried
 */
static app_pc
aslr_update_failed(bool request_new, app_pc requested_base, size_t needed_size)
{
    app_pc new_base = NULL; /* default to native preferred base */
    LOG(THREAD_GET, LOG_SYSCALLS | LOG_VMAREAS, 1,
        "ASLR: aslr_update_failed for " PFX "\n", aslr_last_dll_bounds->start);

    if (request_new) {
        ASSERT(requested_base != NULL);
        ASSERT(needed_size != 0);
        if (requested_base != NULL && needed_size != 0) {
            new_base = aslr_get_fitting_base(requested_base, needed_size);
            ASSERT_CURIOSITY(new_base != NULL ||
                             /* case 9894: suppress for short regression for now */
                             check_filter("win32.reload-race.exe",
                                          get_short_name(get_application_name())));
        } else {
            /* give up, something is not right, just reset */
            ASSERT(new_base == NULL);
        }
    }

    if (new_base == NULL) {
        /* update old base, currently just so we can ASSERT elsewhere */
        d_r_mutex_lock(&aslr_lock);
        aslr_last_dll_bounds->start = NULL;
        d_r_mutex_unlock(&aslr_lock);
        /* just giving up, no need for new base */
    }
    return new_base;
}

static void
aslr_update_view_size(app_pc view_base, size_t view_size)
{
    ASSERT(view_base != NULL);
    ASSERT(view_size != 0);
    ASSERT_CURIOSITY_ONCE((ptr_uint_t)(view_base + view_size) <=
                              DYNAMO_OPTION(aslr_dll_top) ||
                          /* case 7059: suppress for short regr for now */
                          EXEMPT_TEST("win32.reload-race.exe"));

    /* FIXME: if aslr_dll_top is not reachable should wrap around, or
     * know not to try anymore.  Currently we'll just keep trying to
     * rebase and giving up all the time.
     */

    if (TEST(ASLR_INTERNAL_SAME_STRESS, INTERNAL_OPTION(aslr_internal))) {
        return;
    }

    /* NOTE we don't have a lock for the actual system call so we can
     * get out of order here
     */
    d_r_mutex_lock(&aslr_lock);
    if (aslr_last_dll_bounds->start == view_base) {
        aslr_last_dll_bounds->end = view_base + view_size;
    } else {
        /* racy requests? */
        ASSERT_CURIOSITY(false && "racy ASLR mapping");
        /* when the last known request is not the same we just bump to
         * largest value to resynch, although it is more likely that a
         * collision would have prevented one from reaching here
         */
        aslr_last_dll_bounds->end = MAX(aslr_last_dll_bounds->end, view_base + view_size);
        ASSERT_NOT_TESTED();
    }
    d_r_mutex_unlock(&aslr_lock);
}

/* used for tracking potential violations in ASLR_TRACK_AREAS */
static void
aslr_track_randomized_dlls(dcontext_t *dcontext, app_pc base, size_t size, bool map,
                           bool our_shared_file)
{
    if (map) {
        /* note can't use get_module_preferred_base_safe() here, since
         * not yet added to loaded_module_areas */
        app_pc preferred_base;
        if (our_shared_file) {
            DEBUG_DECLARE(app_pc our_relocated_preferred_base =
                              get_module_preferred_base(base););
            ASSERT(TEST(ASLR_SHARED_CONTENTS, DYNAMO_OPTION(aslr_cache)));
            ASSERT(dcontext->aslr_context.original_section_base !=
                   ASLR_INVALID_SECTION_BASE);

            ASSERT_CURIOSITY(our_relocated_preferred_base == base &&
                             "useless conflicting shared");

            LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 2,
                "ASLR: SHARED: mapped base " PFX ", preferred random " PFX
                ", original " PFX "\n",
                base, our_relocated_preferred_base,
                dcontext->aslr_context.original_section_base);

            preferred_base = dcontext->aslr_context.original_section_base;
        } else {
            preferred_base = get_module_preferred_base(base);
        }

        /* FIXME: should be rare, otherwise could retry if this happens */
        ASSERT_CURIOSITY(preferred_base != base && "randomly preferred base");

        /* FIXME: with ASLR_SHARED_CONTENTS we now have three bases to
         * consider original preferred base, shared preferred base,
         * real base (our shared DLL can be rebased due to conflict).
         */
        if (preferred_base != NULL &&
            preferred_base != base /* for the rare case of staying at base */) {
            /* FIXME: if overlap in aslr_wouldbe_areas then we cannot
             * tell which DLL is the one really being targeted.  Yet
             * unlikely that attackers would bother targeting one of
             * these, can still use the first loaded as most likely.
             * Note we can't properly remove overlapping DLLs either.
             * FIXME: Maybe we shouldn't flag compatibility issues and
             * accidental read/write in such contested areas.
             */

            DOLOG(0, LOG_SYSCALLS, {
                if (vmvector_overlap(aslr_wouldbe_areas, preferred_base,
                                     preferred_base + size)) {
                    LOG(THREAD_GET, LOG_SYSCALLS | LOG_VMAREAS, 1,
                        "aslr: conflicting preferred range " PFX "-" PFX
                        " currently " PFX,
                        preferred_base, preferred_base + size, base);
                }
            });

            vmvector_add(aslr_wouldbe_areas, preferred_base, preferred_base + size,
                         base /* current mapping of DLL */);
        } else {
            /* FIXME: shouldn't happen for ASLR_DLL */
            ASSERT_CURIOSITY(false && "not a PE or no image base");
        }
    } else {
        /* not all unmappings are to modules, and double mappings of a
         * PE file both as a module and as a linear memory mapping
         * exist - e.g. USER32!ExtractIconFromEXE.  Would need an
         * explicit MEM_IMAGE check on the area.
         */
        /* It should be faster to check in loaded_module_areas.
         * Ignore if unmapped view was not loaded as DLL.  Called
         * before process_mmap(unmap), still ok to use loaded module
         * list. */
        app_pc preferred_base = get_module_preferred_base_safe(base);
        if (preferred_base != NULL    /* tracked module */
            && preferred_base != base /* randomized by us, or simply rebased? */
        ) {
            /* FIXME: we don't know which DLLs we have randomized
             * ourselves and which have had a conflict, but not a
             * significant loss if we remove the range from tracking.
             * Note that simple technique for silencing the ASSERT
             * doesn't work for rebased DLLs that have been loaded
             * before we're loaded.
             */
            DOLOG(0, LOG_SYSCALLS, {
                /* case 7797 any conflicting natively DLLs may hit this */
                if (!vmvector_overlap(aslr_wouldbe_areas, preferred_base,
                                      preferred_base + size)) {
                    LOG(THREAD_GET, LOG_SYSCALLS | LOG_VMAREAS, 1,
                        "ASLR: unmap missing preferred range " PFX "-" PFX ", "
                        "probably conflict?",
                        preferred_base, preferred_base + size);
                }
            });

            /* doublecheck unsafe base, since PE still mapped in,
             * however preferred base from PE is not what we want in ASLR shared
             * see case 8507
             */
            ASSERT(preferred_base == get_module_preferred_base(base) ||
                   TEST(ASLR_SHARED_CONTENTS, DYNAMO_OPTION(aslr_cache)));

            /* FIXME: if multiple DLLs preferred regions overlap we
             * wouldn't know not to remove a hole, need refcounting, but
             * since whole notification is best effort, not doing that */
            vmvector_remove(aslr_wouldbe_areas, preferred_base, preferred_base + size);
        }
    }
}

/* PRE hook for NtMapViewOfSection */
void
aslr_pre_process_mapview(dcontext_t *dcontext)
{
    reg_t *param_base = dcontext->sys_param_base;
    priv_mcontext_t *mc = get_mcontext(dcontext);

    HANDLE section_handle = (HANDLE)sys_param(dcontext, param_base, 0);
    HANDLE process_handle = (HANDLE)sys_param(dcontext, param_base, 1);
    void **pbase_unsafe = (void *)sys_param(dcontext, param_base, 2);
    uint zerobits = (uint)sys_param(dcontext, param_base, 3);
    size_t commit_size = (size_t)sys_param(dcontext, param_base, 4);
    LARGE_INTEGER *psection_offs_unsafe =
        (LARGE_INTEGER *)sys_param(dcontext, param_base, 5); /* OPTIONAL */
    size_t *pview_size_unsafe = (size_t *)sys_param(dcontext, param_base, 6);
    uint inherit_disposition = (uint)sys_param(dcontext, param_base, 7);
    uint allocation_type = (uint)sys_param(dcontext, param_base, 8);
    uint prot = (uint)sys_param(dcontext, param_base, 9);

    app_pc requested_base;
    size_t requested_size;
    app_pc modified_base = 0;

    /* flag currently used only for MapViewOfSection */
    dcontext->aslr_context.sys_aslr_clobbered = false;

    if (!d_r_safe_read(pbase_unsafe, sizeof(requested_base), &requested_base) ||
        !d_r_safe_read(pview_size_unsafe, sizeof(requested_size), &requested_size)) {
        /* we expect the system call to fail */
        DODEBUG(dcontext->expect_last_syscall_to_fail = true;);
        return;
    }

    DOLOG(1, LOG_SYSCALLS, {
        uint queried_section_attributes = 0;
        bool attrib_ok =
            get_section_attributes(section_handle, &queried_section_attributes, NULL);

        /* Unfortunately, the loader creates sections that do not have
         * Query access (SECTION_QUERY 0x1), and we can't rely on
         * being able to use this
         *
         * windbg> !handle 0 f section
         * GrantedAccess 0xe:
         *    None, MapWrite,MapRead,MapExecute
         * vs
         * GrantedAccess 0xf001f:
         *      Delete,ReadControl,WriteDac,WriteOwner
         *      Query,MapWrite,MapRead,MapExecute,Extend
         * Object Specific Information
         *   Section base address 0
         *   Section attributes 0x4000000
         */

        /* FIXME: unknown flag 0x20000000
         * when running notepad I get
         * Section attributes 0x21800000 only on two DLLs
         * I:\Program Files\WIDCOMM\Bluetooth Software\btkeyind.dll (my bluetooth)
         * I:\Program Files\Dell\QuickSet\dadkeyb.dll are using 0x20000000,
         * why are they special?
         */
        ASSERT_CURIOSITY(
            !TESTANY(~(SEC_BASED_UNSUPPORTED | SEC_NO_CHANGE_UNSUPPORTED | SEC_FILE |
                       SEC_IMAGE | SEC_VLM | SEC_RESERVE | SEC_COMMIT |
                       SEC_NOCACHE
                       /* FIXME: value is 0x20000000
                        * could also be IMAGE_SCN_MEM_EXECUTE, or
                        * MEM_LARGE_PAGES
                        */
                       | GENERIC_EXECUTE),
                     queried_section_attributes));

        LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
            "syscall: pre NtMapViewOfSection *base=" PFX " *size=" PIFX " prot=%s\n"
            "         sh=" PIFX " zero=%d commit=%d &secoffs=" PIFX
            " inherit=%d type=0x%x;"
            "%s%x\n",
            requested_base, requested_size, prot_string(prot), section_handle, zerobits,
            commit_size, psection_offs_unsafe, inherit_disposition, allocation_type,
            attrib_ok ? "attrib=0x" : "unknown ", queried_section_attributes);
    });

    /* Reversing notes: on XP SP2
     *
     * o Protect - all modules are first attempted with --x- but
     *   DLLs that need rebasing are remapped as rw--
     *
     * intercept_load_dll: I:\Program Files\Dell\QuickSet\dadkeyb.dll (always conflicts)
     * syscall: NtMapViewOfSection *base=0x00000000 *size=0x0 prot=--x-
     *          sh=1832 zero=0 commit=0 &secoffs=0 inherit=1 type=0
     * syscall: NtMapViewOfSection 0x00980000 size=0x12000 prot=--x- => 0
     * syscall: NtUnmapViewOfSection 0x00980000 size=0x12000
     *
     * syscall: NtMapViewOfSection *base=0x00000000 *size=0x0 prot=rw--
     *          sh=1836 zero=0 commit=0 &secoffs=0 inherit=1 type=0
     * syscall: NtMapViewOfSection 0x00980000 size=0x13000 prot=rw-- => 0x40000003
     *   Note the size is now larger, in fact mapping is MEM_IMAGE, and
     *   so get STATUS_IMAGE_NOT_AT_BASE, yet we can't always even query our section,
     *   so we would have to track NtCreateSection to determine that.
     *
     * syscall: NtProtectVirtualMemory process=0xffffffff base=0x00981000
     *          size=0x8000 prot=rw-- 0x4
     *
     * And most weird is a call that always fails while processing the above DLL
     * syscall: NtMapViewOfSection *base=0x00980000 *size=0x13000 prot=rw--
     *          sh=1832 zero=0 commit=0 &secoffs=0 inherit=1 type=0
     * syscall: failed NtMapViewOfSection prot=rw--
     *   => 0xc0000018 STATUS_CONFLICTING_ADDRESSES
     */

    if (is_phandle_me(process_handle)) {
        ASSERT_CURIOSITY(psection_offs_unsafe == NULL || prot != PAGE_EXECUTE);
        /* haven't seen a DLL mapping that specifies an offset  */

        /* * SectionOffset is NULL for the loader,
         * kernel32!MapViewOfFileEx (on Windows XP and Win2k)
         * always passes the psection_offs_unsafe as a stack
         * variable, since offset is user exposed.
         * DLL loading on the other hand doesn't need this argument.
         */
        ASSERT_NOT_IMPLEMENTED(!TEST(ASLR_MAPPED, DYNAMO_OPTION(aslr)));

        if (psection_offs_unsafe == NULL && prot != PAGE_READONLY) {
            /* FIXME: should distinguish SEC_IMAGE for the
             * purpose of ASLR_MAPPED in pre-processing, and
             * should be able to tell MEM_IMAGE from
             * MEM_MAPPED.  Can do only if tracking
             * NtCreateSection(), or better yet should just
             * NtQuerySection() which would work for
             * NtCreateSection(), but the loader uses NtOpenSection()
             * without SECTION_QUERY.
             *
             * FIXME: see if using queried_section_attributes would
             * help.  There is nothing interesting in
             * SectionImageInformation (other than that
             * NtQuerySection() would return STATUS_SECTION_NOT_IMAGE
             * when asking for it, if not ).  We should touch only
             * SEC_IMAGE and definitely not mess with SEC_BASED
             *
             * Extra syscall here won't be too critical - we're
             * already calling at least NtQueryVirtualMemory() in
             * process_mmap(), and currently safe_read/safe_write are
             * also system calls.
             */
            /* FIXME: still unclear whether the loader always
             * first maps as PAGE_EXECUTE and only afterwards
             * it tries a rw- mapping
             */

            /* on xp sp2 seen this use of NtMapViewOfSection PAGE_READONLY
             * kernel32!BasepCreateActCtx+0x3d8:
             * 7c8159b1 push    0x2
             * 7c8159cf call dword ptr [kernel32!_imp__NtMapViewOfSection]
             */

            ASSERT_CURIOSITY(zerobits == 0);
            ASSERT_CURIOSITY(commit_size == 0);

            /* only nodemgr and services have been observed to use
             * ViewUnmap, in nodemgr it is on Module32Next from the
             * ToolHelp.

             * FIXME: unclear whether we'll want to do something
             * different for ViewShare handle inheritance if we go
             * after ASLR_SHARED_PER_USER.  Unlikely that a high
             * privilege service will share handles with a low
             * privilege one though.
             */
            ASSERT_CURIOSITY(inherit_disposition == 0x1 /* ViewShare */ ||
                             inherit_disposition == 0x2 /* ViewUnmap */);
            /* cygwin uses AT_ROUND_TO_PAGE but specify file offset,
             * not seen in DLL mappings */
            ASSERT_CURIOSITY(allocation_type == 0);
            ASSERT_CURIOSITY(prot == PAGE_EXECUTE || prot == PAGE_READWRITE);

            DOSTATS({
                if (prot == PAGE_EXECUTE)
                    STATS_INC(app_mmap_section_x);
                else {
                    STATS_INC(app_mmap_section_rw);
                }
            });

            /* seen only either both 0 or both set */
            ASSERT_CURIOSITY(requested_size == 0 || requested_base != 0);

            /* assumption: loader never suggests base in 1st map */
            if (requested_base == 0) {
                DODEBUG({
                    if (TEST(ASLR_SHARED_CONTENTS, DYNAMO_OPTION(aslr_cache)) &&
                        dcontext->aslr_context.randomized_section_handle !=
                            section_handle) {
                        STATS_INC(aslr_dlls_not_shared);

                        ASSERT_CURIOSITY(dcontext->aslr_context.last_app_section_handle ==
                                         section_handle);
                        /* unusual uses of sections other than loader can trigger this */

                        if (dcontext->aslr_context.last_app_section_handle ==
                            section_handle)
                            /* FIXME: with MapViewOfSection private
                             * ASLR processing we don't quite know
                             * whether we're dealing with an image or
                             * mapped file.  This is always hit by
                             * LdrpCheckForLoadedDll, it suggests that
                             * only SEC_IMAGE should be bumped,
                             * instead of SEC_COMMIT as well.  Maybe
                             * there is nothing with doing this and
                             * should take out this warning.
                             */
                            SYSLOG_INTERNAL_WARNING_ONCE("non-image DLL pre-processed "
                                                         "for private ASLR");
                        else {
                            /* could have been exempted */
                            SYSLOG_INTERNAL_WARNING_ONCE("image DLL ASLRed without "
                                                         "sharing");
                        }
                    }
                });

                if (TEST(ASLR_SHARED_CONTENTS, DYNAMO_OPTION(aslr_cache)) &&
                    dcontext->aslr_context.randomized_section_handle == section_handle) {
                    /* shared DLL mapping at presumably randomized location,
                     * leave base unset for preferred mapping
                     */

                    /* we may want to check whether preferred
                     * base+size is available, but since racy we
                     * anyways have to check the success afterwards
                     */
                    STATS_INC(aslr_dlls_shared_mapped);

                    /* mark so that we can handle failures */
                    dcontext->aslr_context.sys_aslr_clobbered = true;
                } else { /* private ASLR */
                    /* FIXME: we may want to take a hint from prot and expected size */
                    modified_base = aslr_get_next_base();

                    if (!TEST(ASLR_INTERNAL_RANGE_NONE, INTERNAL_OPTION(aslr_internal))) {
                        /* really modify base now */
                        /* note that pbase_unsafe is an IN/OUT argument,
                         * so it is not likely that the application would
                         * have used the passed value.  If we instead
                         * passed a pointer to our own (dcontext) variable
                         * we'd have to safe_write it back in aslr_post_process_mapview.
                         */
                        DEBUG_DECLARE(bool ok =)
                        safe_write(pbase_unsafe, sizeof(modified_base), &modified_base);
                        ASSERT(ok);
                        STATS_INC(aslr_dlls_bumped);
                        LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
                            "ASLR: NtMapViewOfSection prot=%s BUMPED to " PFX "\n",
                            prot_string(prot), modified_base);
                        /* mark so that we can handle failures, not allow
                         * detach when system call arguments are modified
                         * from what the application can handle if we do
                         * not deal with possible failures */
                        dcontext->aslr_context.sys_aslr_clobbered = true;
                    } else {
                        LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
                            "ASLR: NtMapViewOfSection prot=%s RANGE_NONE: would be "
                            "at " PFX "\n",
                            prot_string(prot), modified_base);
                    }
                }
            } else {
                /* Apparently the loader maps again with the known base and size
                 * since we have modified the base already, we'll just leave it alone
                 * in same example as noted in the above dadkeyb.dll
                 * syscall: NtMapViewOfSection *base=0x00980000 *size=0x13000 prot=rw--
                 *          sh=1832 zero=0 commit=0 &secoffs=0 inherit=1 type=0
                 * syscall: failed NtMapViewOfSection prot=rw-- => 0xc0000018
                 * Since it fails and goes to already randomized DLL nothing to do here.
                 *
                 * All other yet to be seen users that set base are
                 * also assumed to not need to be randomized.  We may
                 * have to revisit for MEM_MAPPED.
                 */
                ASSERT_CURIOSITY(
                    aslr_last_dll_bounds->start == 0 ||           /* given up */
                    aslr_last_dll_bounds->start == requested_base /* may be race? */
                    || TEST(ASLR_SHARED_CONTENTS, DYNAMO_OPTION(aslr_cache))
                    /* not keeping keep track for shared */);
                /* FIXME: for ASLR_SHARED_CONTENTS would be at the
                 * requested shared preferred mapping address which is
                 * not the same as the private address!  or if it is
                 * hitting a conflict it is in fact the base of the
                 * last mapping that was left to the kernel */
                LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
                    "ASLR: not touching NtMapViewOfSection prot=%s requested " PFX "\n",
                    prot_string(prot), requested_base);
                STATS_INC(app_mmap_requested_base);
            }
        } else {
            DOSTATS({
                if (psection_offs_unsafe == NULL) {
                    if (prot == PAGE_READONLY)
                        STATS_INC(app_mmap_section_r);
                    else {
                        /* not seen other prot requests */
                        ASSERT_CURIOSITY(false && "unseen protection");
                    }
                }
            });
        }
    } else {
        IPC_ALERT("WARNING: MapViewOfSection on another process\n");
    }
}

NTSTATUS
aslr_retry_map_syscall(dcontext_t *dcontext, reg_t *param_base)
{
    /* FIXME: we could issue a system call from app and just pass the
     * sysnum and param_base, yet we don't have the facility to handle
     * post_system_call for that case.  Instead we issue our own copy
     * of the arguments, note that all OUT arguments will be modified
     * directly in the app space anyways.  Only any IN argument races
     * and overwrites won't be transparent.
     */

    NTSTATUS res;
    /* Minor hit of unnecessary argument copying, allows us to work
     * with any special handling needed by NT_SYSCALL
     */
    HANDLE section_handle = (HANDLE)postsys_param(dcontext, param_base, 0);
    HANDLE process_handle = (HANDLE)postsys_param(dcontext, param_base, 1);
    void **pbase_unsafe = (void *)postsys_param(dcontext, param_base, 2);
    ULONG_PTR zerobits = (ULONG_PTR)postsys_param(dcontext, param_base, 3);
    size_t commit_size = (size_t)postsys_param(dcontext, param_base, 4);
    LARGE_INTEGER *section_offs = (LARGE_INTEGER *)postsys_param(dcontext, param_base, 5);
    SIZE_T *view_size = (SIZE_T *)postsys_param(dcontext, param_base, 6);
    uint inherit_disposition = (uint)postsys_param(dcontext, param_base, 7);
    uint type = (uint)postsys_param(dcontext, param_base, 8);
    uint prot = (uint)postsys_param(dcontext, param_base, 9);

    /* Atypical use of NT types in nt_map_view_of_section to reaffirm
     * that we are using this on behalf of the application. */
    res = nt_raw_MapViewOfSection(section_handle,      /* 0 */
                                  process_handle,      /* 1 */
                                  pbase_unsafe,        /* 2 */
                                  zerobits,            /* 3 */
                                  commit_size,         /* 4 */
                                  section_offs,        /* 5 */
                                  view_size,           /* 6 */
                                  inherit_disposition, /* 7 */
                                  type,                /* 8 */
                                  prot);               /* 9 */

    LOG(THREAD_GET, LOG_SYSCALLS | LOG_VMAREAS, 1,
        "syscall: aslr_retry_map_syscall NtMapViewOfSection *pbase=" PFX
        ", prot=%s, res " PFX "\n",
        *pbase_unsafe, prot_string(prot), res);
    ASSERT_CURIOSITY(NT_SUCCESS(res));
    return res;
}

/* get mapping size needed for an application section */
bool
aslr_get_module_mapping_size(HANDLE section_handle, size_t *module_size, uint prot)
{
    NTSTATUS res;
    app_pc base = (app_pc)0x0; /* default mapping */
    size_t commit_size = 0;
    SIZE_T view_size = 0; /* we need to know full size */
    uint type = 0;        /* commit is default */

    /* note the section characteristics determine whether MEM_MAPPED
     * or MEM_IMAGE is needed */

    /* we need protection flags given by the caller, so we can avert a
     * STATUS_SECTION_PROTECTION error - A view to a section specifies
     * a protection which is incompatible with the initial view's
     * protection.
     */

    /* FIXME: case 9669 - if we have SECTION_QUERY privilege we can
     * try to get the size from SectionBasicInformation.Size, and map
     * only on failure
     */
    res = nt_raw_MapViewOfSection(section_handle,     /* 0 */
                                  NT_CURRENT_PROCESS, /* 1 */
                                  &base,              /* 2 */
                                  0,                  /* 3 */
                                  commit_size,        /* 4 */
                                  NULL,               /* 5 */
                                  &view_size,         /* 6 */
                                  ViewShare,          /* 7 */
                                  type,               /* 8 */
                                  prot);              /* 9 */
    ASSERT(NT_SUCCESS(res));
    if (!NT_SUCCESS(res))
        return false;
    /* side note: windbg receives a ModLoad: for our temporary mapping
     * at the NtMapViewOfSection(), no harm */
    *module_size = view_size;

    res = nt_raw_UnmapViewOfSection(NT_CURRENT_PROCESS, base);
    ASSERT(NT_SUCCESS(res));
    return true;
}

/* since always coming from d_r_dispatch now, only need to set mcontext, but we
 * continue to set reg_eax in case it's read later in the routine
 * FIXME: assumes local variable reg_eax
 */
#define SET_RETURN_VAL(dc, val) \
    reg_eax = (reg_t)(val);     \
    get_mcontext(dc)->xax = (reg_t)(val);

/* POST processing of NtMapViewOfSection.  Should be called only when
 * base is clobbered by us.  Potentially modifies app registers and system
 * call parameters.
 */
void
aslr_post_process_mapview(dcontext_t *dcontext)
{
    reg_t *param_base = dcontext->sys_param_base;
    reg_t reg_eax = get_mcontext(dcontext)->xax;
    NTSTATUS status = (NTSTATUS)reg_eax; /* get signed result */

    HANDLE section_handle = (HANDLE)postsys_param(dcontext, param_base, 0);
    HANDLE process_handle = (HANDLE)postsys_param(dcontext, param_base, 1);
    void **pbase_unsafe = (void *)postsys_param(dcontext, param_base, 2);
    uint zerobits = (uint)postsys_param(dcontext, param_base, 3);
    size_t commit_size = (size_t)postsys_param(dcontext, param_base, 4);
    uint *section_offs = (uint *)postsys_param(dcontext, param_base, 5);
    size_t *view_size = (size_t *)postsys_param(dcontext, param_base, 6);
    uint inherit_disposition = (uint)postsys_param(dcontext, param_base, 7);
    uint type = (uint)postsys_param(dcontext, param_base, 8);
    uint prot = (uint)postsys_param(dcontext, param_base, 9);
    size_t size;
    app_pc base;

    /* retries to recover private ASLR from range conflict */
    uint retries_left = DYNAMO_OPTION(aslr_retry) + 1 /* must fallback to native */;

    ASSERT(dcontext->aslr_context.sys_aslr_clobbered);

    /* unlikely that a dynamic option change happened in-between */
    ASSERT_CURIOSITY(TESTANY(ASLR_DLL | ASLR_MAPPED, DYNAMO_OPTION(aslr)));

    ASSERT(is_phandle_me(process_handle));

    /* FIXME: should distinguish SEC_IMAGE for the purpose of
     * ASLR_MAPPED in pre-processing.  Should be able to tell
     * MEM_IMAGE from MEM_MAPPED, here at least ASSERT.
     */

    /* expected attributes only when we have decided to clobber,
     * under ASLR_DLL it is only loader objects.
     */
    DOCHECK(1, {
        uint section_attributes;
        get_section_attributes(section_handle, &section_attributes, NULL);
        ASSERT_CURIOSITY(section_attributes == 0 ||
                         TESTALL(SEC_IMAGE | SEC_FILE, section_attributes));
        ASSERT_CURIOSITY(
            section_attributes == 0 || /* no Query access */
            !TESTANY(~(SEC_IMAGE | SEC_FILE | GENERIC_EXECUTE), section_attributes));
    });

    ASSERT_CURIOSITY(status == STATUS_SUCCESS || status == STATUS_IMAGE_NOT_AT_BASE ||
                     status == STATUS_CONFLICTING_ADDRESSES);

    /* handle shared DLL ASLR mapping */
    if (TEST(ASLR_SHARED_CONTENTS, DYNAMO_OPTION(aslr_cache)) &&
        dcontext->aslr_context.randomized_section_handle == section_handle) {
        if (NT_SUCCESS(status)) {
            if (status == STATUS_SUCCESS) {
                STATS_INC(aslr_dlls_shared_mapped_good);
            } else if (status == STATUS_IMAGE_NOT_AT_BASE) {
                /* we can live with not being at our choice as well,
                 * though it breaks all the work we did to share this
                 * mapping.
                 */
                /* If we fail to map a shared DLL at its preferred
                 * base we're not gaining any sharing.  Should revert
                 * this DLL back to private randomization for better
                 * controlled randomization worse the kernel will pick
                 * the lowest possible address that may be easier to
                 * predict.  TOFILE currently useful to leave
                 * as is for testing full sharing.
                 */
                SYSLOG_INTERNAL_WARNING("conflicting shared mapping "
                                        "should use private instead\n");
                /* FIXME: should get some systemwide stats on how
                 * often do we get the correct base so we can measure
                 * the effectiveness of the randomization mapping  */
                STATS_INC(aslr_dlls_shared_map_rebased);
            } else
                ASSERT_NOT_REACHED();

            /* if successful, we'll use the original base from our records,
             * not from mapped PE, so we can detect attacks.
             *
             * case 8507 similarly we have to register to fool hotpatching's
             * timestamp/checksum.  Saved on section create or open
             * aslr_context.original_section_{base,checksum,timestamp}.
             */

            /* add to preferred module range */
            if (TEST(ASLR_TRACK_AREAS, DYNAMO_OPTION(aslr_action))) {
                ASSERT(NT_SUCCESS(status));

                /* we assume that since syscall succeeded these dereferences are safe
                 * FIXME : could always be multi-thread races though */
                size = *view_size; /* ignore commit_size? */
                base = *((app_pc *)pbase_unsafe);

                LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
                    "ASLR: SHARED NtMapViewOfSection " PFX " size=" PIFX
                    " prot=%s => " PFX "\n",
                    base, size, prot_string(prot), reg_eax);

                /* We need to provide the original preferred address
                 * which was preserved at the section creation in
                 * aslr_context.  We also keep the original base in
                 * the module list so that on UnMapViewOfSection we
                 * can remove the preferred region
                 */
                aslr_track_randomized_dlls(dcontext, base, size, true /* Map */,
                                           true /* Our Shared File */);
            }
        } else {
            /* FIXME: we've went too far here - we can still switch the
             * file handle to the original handle for creating a new
             * section, and then map that instead and recover the
             * application's intent.  Or should have kept the
             * original_section_handle open until here?
             */
            STATS_INC(aslr_dlls_shared_map_failed);

            LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
                "ASLR: unexpected failure on shared NtMapViewOfSection"
                " prot=%s => " PFX "\n",
                prot_string(prot), reg_eax);

            /* we can't simply restore application request below, and retry */
            ASSERT_CURIOSITY(false && "unexpected error status");

            /* FIXME: return error to app hoping it would have been a
             * native error as well.  Would we be out of virtual
             * address space?
             */
            ASSERT_NOT_IMPLEMENTED(false);
        }

        dcontext->aslr_context.randomized_section_handle = INVALID_HANDLE_VALUE;
        dcontext->aslr_context.sys_aslr_clobbered = false;
        return;
    } else if (TEST(ASLR_SHARED_CONTENTS, DYNAMO_OPTION(aslr_cache)) &&
               dcontext->aslr_context.randomized_section_handle != section_handle) {
        /* flag that private mapping should be processed in
         * update_module_list()
         */
        dcontext->aslr_context.original_section_base = ASLR_INVALID_SECTION_BASE;
    }

    /* handle private rebasing on ASLR mapping */

    /* FIXME: STATUS_ILLEGAL_DLL_RELOCATION: what defines the
     * "The system DLL %hs was relocated in memory. The
     * application will not run properly.  The relocation
     * occurred because the DLL %hs occupied an address range
     * reserved for Windows system DLLs. "
     */

    /* Three potential problems that prevent us from randomizing all
     * mappings: DLL exemptions by name, DLL FIXED, and races due to
     * non-atomic interposition on system calls.
     *
     * Three approaches to solving them:
     * 1) keep track of handles from file to section -
     *     may be able to do exemption on file name
     * 2) presyscall
     *     o may grab a lock to deal with in-process races
     *     o extra map/unmap - can figure out FIXED or PE name
     * 3) postsyscall undo and redo
     *    o can handle racy allocation failure by retrying on failure
     *    o can figure out PE name and FIXED exemption, unmap & retry
     *
     * Currently using 3) to avoid adding the Map/UnMap on the normal
     * path, assuming exemptions are exceptions not the norm, and also
     * allows dealing with IPC allocations.
     */

    if (!NT_SUCCESS(status)) {
        app_pc retry_base = 0;
        int retry_result = 0;

        /* Need to handle failures and retry.  For sure we can cause
         * STATUS_CONFLICTING_ADDRESSES and since the loader doesn't
         * retry we have to retry for it.  Conservatively we should
         * retry on any other unknown failure.
         */

        /* FIXME: should we look for the end/beginning of the current
         * mapping at the conflicting location and try one more time?
         * mostly needed for ASLR_RANGE_BOTTOM_UP/ASLR_RANGE_TOP_DOWN.
         * ASLR_RANGE_RANDOM should have a full address space map to
         * allow it to choose any location.
         */

        /* Note that SQL server is grabbing a lot of virtual
         * address space in the example I've seen it has taken
         * everything from after sqlsort.dll 42b70000 and
         * reserves all the memory until rpcrt4.dll 77d30000
         * So a scheme that simply gives up randomizing after
         * hitting these will not do us much good here.
         * Should wrap around and continue looking for good
         * ranges.
         *
         * Side note that due to the above reservation some
         * dynamically loaded DLLs are not at predictable
         * locations, since loaded by multiple threads. SQL
         * Slammer used a stable location in the statically
         * linked sqlsort.dll as a trampoline.
         */

        /* FIXME: alternative solution is to retry with no
         * base address - and use the returned mapping as a
         * hint where the OS would rather have us, then unmap,
         * add jitter and try again.  The problem is that most
         * DLLs in the usual case will prefer to be at their
         * preferred base.
         */

        ASSERT_CURIOSITY(status == STATUS_CONFLICTING_ADDRESSES);

        ASSERT(*pbase_unsafe != 0); /* ASSERT can take a risk */
        ASSERT(*pbase_unsafe == aslr_last_dll_bounds->start);

        ASSERT(retries_left >= 0);
        /* possibly several ASLR attempts, and a final native base retry */
        /* retry syscall */
        do {
            if (status == STATUS_CONFLICTING_ADDRESSES) {
                /* we can modify the arguments and give it another shot */
                if (retries_left > 1) {
                    /* note aslr_last_dll_bounds->start is global so
                     * subject to race, while the *pbase_unsafe is app
                     * memory similarly beyond our control, so neither
                     * one can really be trusted to be what the
                     * syscall really used.  We choose to use the app
                     * for the base_requested hint.
                     */
                    app_pc base_requested = 0;
                    size_t size_needed;

                    TRY_EXCEPT(dcontext, { base_requested = *pbase_unsafe; },
                               { /* nothing */ });

                    /* although we could skip the first MEM_FREE block
                     * and assume we were too big, we're not
                     * guaranteed we'd find enough room in the next
                     * hole either in a small number of retries, so
                     * we're doing a full NtMapViewOfSection() to
                     * obtain the actual size needed
                     */
                    if (aslr_get_module_mapping_size(section_handle, &size_needed,
                                                     prot)) {
                        retry_base = aslr_update_failed(true /* request a better fit */,
                                                        base_requested, size_needed);
                        ASSERT_CURIOSITY(
                            retry_base != 0 ||
                            /* case 9893: suppress for short regr for now */
                            check_filter("win32.reload-race.exe",
                                         get_short_name(get_application_name())));
                    } else {
                        retry_base = NULL;
                    }
                    if (retry_base == NULL) {
                        SYSLOG_INTERNAL_WARNING_ONCE("ASLR conflict at " PFX ", "
                                                     "no good fit, giving up",
                                                     *pbase_unsafe);
                        /* couldn't find a better match */
                        STATS_INC(aslr_dll_conflict_giveup);

                        /* if giving up we just process as if application request */
                        retries_left = 0;
                        /* same as handling any other error */
                    } else {
                        SYSLOG_INTERNAL_WARNING_ONCE("ASLR conflict at " PFX
                                                     ", retrying at " PFX,
                                                     *pbase_unsafe, retry_base);

                        /* we'll give it another shot at the new address
                         * although it may still fail there due to races,
                         * so we have to be ready to retry the original app
                         */
                        ASSERT(dcontext->aslr_context.sys_aslr_clobbered);
                        retries_left--;
                        ASSERT(retries_left > 0);
                        STATS_INC(aslr_dll_conflict_fit_retry);
                    }
                } else {
                    /* first solution: give up our randomization and move on */
                    SYSLOG_INTERNAL_WARNING_ONCE("ASLR conflict at " PFX ", giving up",
                                                 *pbase_unsafe);
                    /* if giving up we just process as if application request */
                    retries_left = 0;
                    retry_base = aslr_update_failed(false /* no new request */, NULL, 0);
                    STATS_INC(aslr_dll_conflict_giveup);
                }
                /* side note: WinDbg seems to get notified even when the system call fails
                 * so when executing this under a debugger a sequence like this is seen:
                 * when run with ASLR_RANGE_SAME_STRESS
                 *
                 * WARNING: WS2HELP overlaps Msi
                 * ModLoad: 43b40000 43b40000   I:\WINDOWS\system32\WS2HELP.dll
                 * ModLoad: 71aa0000 71aa8000   I:\WINDOWS\system32\WS2HELP.dll
                 *
                 * WARNING: WSOCK32 overlaps IMAGEHLP
                 * WARNING: WSOCK32 overlaps urlmon
                 * WARNING: WSOCK32 overlaps appHelp
                 * WARNING: WSOCK32 overlaps btkeyind
                 * ModLoad: 43aa0000 43aeb000   I:\WINDOWS\system32\WSOCK32.dll
                 * ModLoad: 71ad0000 71ad9000   I:\WINDOWS\system32\WSOCK32.dll
                 */
            } else {
                ASSERT_NOT_TESTED();
                LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
                    "ASLR: unexpected failure on NtMapViewOfSection prot=%s => " PFX "\n",
                    prot_string(prot), reg_eax);

                /* FIXME: note that we may be able to retry on out of
                 * page file memory if there was a transient memory
                 * use, though success is unlikely to be worthwhile
                 */

                /* just restore application request below, and retry */
                ASSERT_CURIOSITY(false && "unexpected error status");
                /* directly pass retried result to the application */
                retries_left = 0;
                retry_base = aslr_update_failed(false /* no retry */, NULL, 0);
            }

            if (retries_left == 0) {
                dcontext->aslr_context.sys_aslr_clobbered = false;
                ASSERT(retry_base == NULL);
                /* we get here only when aslr_pre_process_mapview()
                 * has verified the app request was for base 0
                 */
            }

            safe_write(pbase_unsafe, sizeof(retry_base), &retry_base);

            /* here we reset all IN/OUT arguments */

            /* make sure that even on syscall failure OUT arguments aren't set */
            ASSERT(*view_size == 0);      /* we handle only when not set */
            ASSERT(section_offs == NULL); /* optional, we handle only when not set */

            /* we have to be able to handle failure of new base */
            ASSERT(retry_base == 0 || dcontext->aslr_context.sys_aslr_clobbered);
            ASSERT(*pbase_unsafe == retry_base); /* retry at base,
                                                  * unsafe ASSERT can take a risk */
            /* retry with new mapping base - passing arguments */
            retry_result = aslr_retry_map_syscall(dcontext, param_base);
            SET_RETURN_VAL(dcontext, retry_result); /* sets reg_eax */

            /* reread all OUT arguments since we have to handle
             * the retried system call as if that's what really happened
             */
            ASSERT(section_handle == (HANDLE)postsys_param(dcontext, param_base, 0));
            ASSERT(process_handle == (HANDLE)postsys_param(dcontext, param_base, 1));
            pbase_unsafe = (void *)postsys_param(dcontext, param_base, 2); /* OUT */
            ASSERT(zerobits == (uint)postsys_param(dcontext, param_base, 3));
            ASSERT(commit_size == (size_t)postsys_param(dcontext, param_base, 4));
            section_offs = (uint *)postsys_param(dcontext, param_base, 5); /* OUT */
            view_size = (size_t *)postsys_param(dcontext, param_base, 6);  /* OUT */
            ASSERT(inherit_disposition == (uint)postsys_param(dcontext, param_base, 7));
            ASSERT(type == (uint)postsys_param(dcontext, param_base, 8));
            ASSERT(prot == (uint)postsys_param(dcontext, param_base, 9));

            STATS_INC(aslr_error_retry);
            DOSTATS({
                if (!NT_SUCCESS(status)) {
                    STATS_INC(aslr_error_on_retry);
                } else {
                    if (status == STATUS_SUCCESS)
                        STATS_INC(aslr_retry_at_base);
                    else if (status == STATUS_IMAGE_NOT_AT_BASE)
                        STATS_INC(aslr_retry_not_at_base);
                    else
                        ASSERT_NOT_REACHED();
                }
            });

            /* we retry further only if we tried a different base, and
             * otherwise leave to the application as it was
             */
        } while (!NT_SUCCESS(status) && (retries_left > 0));

        /* last retry is native, implication */
        ASSERT(!(retries_left == 0) || !dcontext->aslr_context.sys_aslr_clobbered);
        ASSERT(!dcontext->aslr_context.sys_aslr_clobbered || NT_SUCCESS(status));
    }

    DOCHECK(1, {
        if (dcontext->aslr_context.sys_aslr_clobbered && NT_SUCCESS(status)) {
            /* really handle success later, after safe read of base and size */

            /* verify that we always get a (success) code */
            /* STATUS_IMAGE_NOT_AT_BASE ((NTSTATUS)0x40000003L)
             *
             * FIXME: I presume the loader maps MEM_MAPPED as
             * MapViewOfSection(--x) and it maybe just reads the PE
             * headers?  Only the MapViewOfSection(rw-) in fact returns
             * STATUS_IMAGE_NOT_AT_BASE
             */

            /* Note the confusing mapping of MEM_MAPPED as --x, and MEM_IMAGE as rw-! */
            ASSERT_CURIOSITY(
                (prot == PAGE_EXECUTE && status == STATUS_SUCCESS) ||
                (prot == PAGE_READWRITE && status == STATUS_IMAGE_NOT_AT_BASE));
            /* FIXME: case 6736 is hitting this as well - assumed
             * SEC_RESERVE 0x4000000, prot = RW, inherit_disposition = ViewUnmap
             * and should simply allow that to get STATUS_SUCCESS
             */

            /* FIXME: case 2298 needs to check for /FIXED DLLs - are they
             * going to fail above, or loader will fail when presented
             * with them.
             *
             * FIXME: -exempt_aslr_list needs to be handled here
             * FIXME: need to reset all IN/OUT arguments
             */
        }
    });

    /* note this is failure after retrying at default base */
    /* so if it fails it is not our fault
     */
    if (!NT_SUCCESS(status)) {
        ASSERT_NOT_TESTED();
        LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
            "ASLR: retry failed NtMapViewOfSection prot=%s => " PFX "\n",
            prot_string(prot), reg_eax);
        ASSERT_CURIOSITY(false);

        /* directly pass retried result to the application */
        return;
    }

    ASSERT(NT_SUCCESS(status));

    /* we assume that since syscall succeeded these dereferences are safe
     * FIXME : could always be multi-thread races though */
    size = *view_size; /* ignore commit_size? */
    base = *((app_pc *)pbase_unsafe);

    LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
        "ASLR: NtMapViewOfSection " PFX " size=" PIFX " prot=%s => " PFX "\n", base, size,
        prot_string(prot), reg_eax);

    /* verify if need to exempt, only if we are still processing our randomization */
    /* we are exempting only after the fact here */
    /* keep in synch with is_aslr_exempted_file_name() */
    if (dcontext->aslr_context.sys_aslr_clobbered &&
        (!IS_STRING_OPTION_EMPTY(exempt_aslr_default_list) ||
         !IS_STRING_OPTION_EMPTY(exempt_aslr_list) ||
         !IS_STRING_OPTION_EMPTY(exempt_aslr_extra_list))) {
        MEMORY_BASIC_INFORMATION mbi;

        bool exempt = false; /* NOTE: we do not give up if can't find
                              * name, case 3858 if no name */

        /* -exempt_aslr_list '*' is really only interesting as a
         * stress test option, otherwise should just turn off ASLR_DLL
         */
        if (IS_LISTSTRING_OPTION_FORALL(exempt_aslr_list))
            exempt = true;

        if (query_virtual_memory(base, &mbi, sizeof(mbi)) == sizeof(mbi)) {
            ASSERT(mbi.Type == MEM_IMAGE || mbi.Type == MEM_MAPPED);

            LOG(THREAD, LOG_SYSCALLS, 2, "ASLR: !vprot " PFX "\n", base);
            DOLOG(2, LOG_SYSCALLS, { dump_mbi(THREAD, &mbi, false); });
        } else
            ASSERT_NOT_REACHED();

        if (is_readable_pe_base(base)) {
            /* Note that the loader first maps an image as MEM_MAPPED */
            /* FIXME: in those allocations RVAs have to be converted
             * for our reads of export table and thus PE name to work
             * properly!
             */
            /*
             * 0:000> !vprot 0x43ab0000
             *   BaseAddress:       43ab0000
             *   AllocationBase:    43ab0000
             *   AllocationProtect: 00000010  PAGE_EXECUTE
             *   RegionSize:        00048000
             *   State:             00001000  MEM_COMMIT
             *   Protect:           00000010  PAGE_EXECUTE
             *   Type:              00040000  MEM_MAPPED
             */
            if (mbi.Type == MEM_IMAGE) {
                /* For MEM_IMAGE can properly get PE name.  We haven't yet added
                 * to the loaded_module_areas so we can't use
                 * get_module_short_name().  We could use
                 * get_module_short_name_uncached(), but
                 * is_aslr_exempted_file_name() uses file name only, so we use
                 * that as well. (For example, in IE we have browselc.dll
                 * filename vs BROWSEUI.DLL rsrc name, and we don't want the
                 * user having to specify a different name for private vs shared
                 * exemptions).
                 */
                const char *module_name = NULL;
                bool alloc = false;
                uint module_characteristics;
                if (DYNAMO_OPTION(track_module_filenames)) {
                    const char *path = section_to_file_lookup(section_handle);
                    if (path != NULL) {
                        module_name = get_short_name(path);
                        if (module_name != NULL)
                            module_name = dr_strdup(module_name HEAPACCT(ACCT_OTHER));
                        dr_strfree(path HEAPACCT(ACCT_VMAREAS));
                    }
                }
                if (module_name == NULL) {
                    alloc = true;
                    module_name = get_module_short_name_uncached(dcontext, base,
                                                                 true /*at map*/
                                                                 HEAPACCT(ACCT_OTHER));
                }

                LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
                    "ASLR: NtMapViewOfSection prot=%s mapped %s\n", prot_string(prot),
                    module_name ? module_name : "<noname>");

                /* note that although we are undoing randomization
                 * of the MEM_IMAGE mapping (usually done in
                 * ntdll!LdrpMapDll), we don't handle it when
                 * loaded as MEM_MAPPED earlier */

                ASSERT(module_name != NULL);
                if (module_name != NULL &&
                    check_list_default_and_append(dynamo_options.exempt_aslr_default_list,
                                                  dynamo_options.exempt_aslr_list,
                                                  module_name)) {
                    SYSLOG_INTERNAL_WARNING("ASLR exempted DLL %s", module_name);
                    exempt = true;
                }

                if (module_name != NULL && DYNAMO_OPTION(aslr_extra) &&
                    check_list_default_and_append("", /* no default list */
                                                  dynamo_options.exempt_aslr_extra_list,
                                                  module_name)) {
                    SYSLOG_INTERNAL_WARNING("ASLR exempted extra DLL %s", module_name);
                    exempt = true;
                }

                module_characteristics = get_module_characteristics(base);
                if (TEST(IMAGE_FILE_DLL, module_characteristics) &&
                    TEST(IMAGE_FILE_RELOCS_STRIPPED, module_characteristics)) {
                    /* Note that we still privately ASLR EXEs that are
                     * presumed to not be executable but only loaded
                     * for ther resources */
                    /* FIXME: case 2298 this test doesn't really work
                     * for one version of /FIXED in our test suite as
                     * security-win32/secalign-fixed.dll.c, yet works
                     * for sec-fixed.dll.c*/
                    SYSLOG_INTERNAL_WARNING("ASLR exempted /FIXED DLL %s",
                                            module_name ? module_name : "noname");
                    exempt = true;
                }
                DODEBUG({
                    if (!exempt && !TEST(IMAGE_FILE_DLL, module_characteristics)) {
                        /* EXE usually have no PE name, and note that we
                         * see for example in notepad.exe help on (xp sp2)
                         * we get helpctr.exe loaded as
                         * C:\WINDOWS\PCHealth\HelpCtr\Binaries\HelpCtr.exe
                         *  LDRP_ENTRY_PROCESSED
                         *  LDRP_IMAGE_NOT_AT_BASE
                         */
                        SYSLOG_INTERNAL_INFO("ASLR note randomizing mapped EXE %s",
                                             module_name != NULL ? module_name
                                                                 : "noname");
                    }
                });

                /* add to preferred module range only if MEM_IMAGE */
                if (TEST(ASLR_TRACK_AREAS, DYNAMO_OPTION(aslr_action)) && !exempt) {
                    /* FIXME: only DLLs that are randomized by us get added,
                     * not any DLL rebased due to other conflicts (even if
                     * due to overlap our own allocations we don't take blame)
                     */
                    /* FIXME: case 8490 on moving out */
                    aslr_track_randomized_dlls(dcontext, base, size, true /* Map */,
                                               false /* Original File */);
                }

                if (alloc && module_name != NULL)
                    dr_strfree(module_name HEAPACCT(ACCT_OTHER));
            } else {
                ASSERT(mbi.Type == MEM_MAPPED);
                /* FIXME: case 5325 still have to call get_dll_short_name()
                 * alternative that knows to use our ImageRvaToVa() FIXME: case 6766
                 * to get the PE name and properly exempt these mappings
                 */
                /* Note: Although ntdll!LdrpCheckForLoadedDll maps DLL
                 * as MEM_MAPPED and we'll currently randomize that,
                 * it in fact doesn't depend on this mapping to be at
                 * the normal DLL location.  We will not exempt here.
                 */
                LOG(THREAD, LOG_SYSCALLS, 1,
                    "ASLR: NtMapViewOfSection " PFX " module not mapped as image!\n",
                    base);
                STATS_INC(app_mmap_PE_as_MAPPED);
                /* FIXME: we do not check nor set exempt here! */
            }
        } else {
            /* FIXME: case 6737 ASLR_MAPPED should we rebase other
             * mappings that are not PEs?  Reversing note: seen in
             * notepad help, and currently rebased even for ASLR_DLL
             *
             * <?xml version="1.0" ...>
             * <assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
             * <assemblyIdentity processorArchitecture="*" version="5.1.0.0"
             * type="win32" name="Microsoft.Windows.Shell.shell32"/>
             * <description>Windows Shell</description>
             *
             * 00b664e4 7c91659e ntdll!LdrGetDllHandleEx+0x258
             * 00b66500 7c801d1f ntdll!LdrGetDllHandle+0x18
             * 00b66568 7c816f55 kernel32!LoadLibraryExW+0x161
             *                   "I:\WINDOWS\WindowsShell.manifest"
             * 00b66594 7c816ed5 kernel32!BasepSxsFindSuitableManifestResourceFor+0x51
             * 00b66894 7d58f157 kernel32!CreateActCtxW+0x69e
             * 00b66acc 7d58f0a8 mshtml!DllGetClassObject+0x1291
             */
            LOG(THREAD, LOG_SYSCALLS, 1,
                "ASLR: NtMapViewOfSection " PFX " not a module!\n", base);
            STATS_INC(app_mmap_not_PE_rebased);
        }

        if (exempt) {
            /* have to undo and redo app mapping */
            app_pc redo_base = 0;
            app_pc redo_size = 0;
            int redo_result;
            /* undo: issue unmap on what we have bumped */
            NTSTATUS res = nt_raw_UnmapViewOfSection(process_handle, base);
            LOG(THREAD_GET, LOG_SYSCALLS | LOG_VMAREAS, 1,
                "syscall: aslr exempt: NtUnmapViewOfSection base=" PFX ", res " PFX "\n",
                base, res);
            ASSERT(NT_SUCCESS(res));
            /* if we cannot unmap our own mapping we're in trouble, but app should be ok
             * it will just have some wasted memory, we can continue
             */

            /* here we reset IN/OUT arguments in our current param_base
             * (currently only pbase_unsafe and view_size),
             * then retry just as above to remap at good base. */
            safe_write(pbase_unsafe, sizeof(redo_base), &redo_base);
            /* redo OUT argument view_size, whose value would have changed */
            ASSERT_CURIOSITY(*view_size != 0);
            safe_write(view_size, sizeof(redo_size), &redo_size);
            ASSERT(*view_size == 0); /* we handle only when not set originally */

            ASSERT(section_offs == NULL); /* optional, we handle only when not set */

            /* no plans on trying a different base */
            ASSERT(*pbase_unsafe == 0); /* retry at base, unsafe ASSERT can take a risk */

            /* retry with new mapping base - passing arguments */
            redo_result = aslr_retry_map_syscall(dcontext, param_base);
            SET_RETURN_VAL(dcontext, redo_result); /* sets reg_eax */

            LOG(THREAD_GET, LOG_SYSCALLS | LOG_VMAREAS, 1,
                "syscall: aslr exempt: NtMapViewOfSection got base=" PFX ", res " PFX
                "\n",
                *pbase_unsafe, res);

            /* no further processing of arguments here */

            /* this worked for us, it should succeed for app, though there
             * may be a conflict at the original base, while ours was good
             */
            ASSERT_CURIOSITY(NT_SUCCESS(status));

            ASSERT(dcontext->aslr_context.sys_aslr_clobbered);
            aslr_update_failed(false /* no retry */, NULL, 0);
            dcontext->aslr_context.sys_aslr_clobbered = false;
            STATS_INC(aslr_dlls_exempted);
        }
    }

    /* update if randomized, but not if had to retry on conflict, or if exempted */
    if (dcontext->aslr_context.sys_aslr_clobbered) {
        aslr_update_view_size(base, size);
        dcontext->aslr_context.sys_aslr_clobbered = false;
    }
}

/* PRE hook for NtUnmapViewOfSection */
void
aslr_pre_process_unmapview(dcontext_t *dcontext, app_pc base, size_t size)
{
    reg_t *param_base = dcontext->sys_param_base;

    /* remove from preferred module range */
    if (TEST(ASLR_TRACK_AREAS, DYNAMO_OPTION(aslr_action))) {
        /* FIXME: should move to post processing in
         * aslr_post_process_mapview, for the unlikely case
         * NtUnmapViewOfSection fails, and so that we remove only when
         * really removed.  We need to preserve all our data across
         * system call.
         */
        aslr_track_randomized_dlls(dcontext, base, size, false /* Unmap */, false);
    }

    /* FIXME: need to mark in our range or vmmap that memory
     * is available Note that the loader always does a
     * MapViewOfSection(--x);UnmapViewOfSection();MapViewOfSection(rw-);
     * so we'll leave a growing hole in case of DLL churn -
     * case 6739 about virtual memory reclamation
     * case 6729 on committed memory leaks and optimizations this also affects
     */
    ASSERT_NOT_IMPLEMENTED(true);
}

/* POST processing of NtUnmapViewOfSection with possibly clobbered by us base */
reg_t
aslr_post_process_unmapview(dcontext_t *dcontext)
{
    reg_t *param_base = dcontext->sys_param_base;
    reg_t reg_eax = get_mcontext(dcontext)->xax;
    NTSTATUS status = (NTSTATUS)reg_eax; /* get signed result */

    ASSERT_NOT_IMPLEMENTED(false);
    return reg_eax;
}

#ifdef DEBUG
/* doublecheck wouldbe areas subset of loaded module preferred ranges
 * by removing all known loaded modules preferred ranges
 * returns true if vmvector_empty(aslr_wouldbe_areas)
 * Call once only!
 */
static bool
aslr_doublecheck_wouldbe_areas(void)
{
    module_iterator_t *iter = module_iterator_start();
    while (module_iterator_hasnext(iter)) {
        size_t size;
        module_area_t *ma = module_iterator_next(iter);
        ASSERT(ma != NULL);
        size = (ma->end - ma->start);

        /* not all modules are randomized, ok not to find an overlapping one */
        vmvector_remove(aslr_wouldbe_areas, ma->os_data.preferred_base,
                        ma->os_data.preferred_base + size);
    }
    module_iterator_stop(iter);

    return vmvector_empty(aslr_wouldbe_areas);
}
#endif /* DEBUG */

bool
aslr_is_possible_attack(app_pc target_addr)
{
    /* FIXME: split by ASLR_DETECT_EXECUTE, ASLR_DETECT_READ,
     * ASLR_DETECT_WRITE */

    /* FIXME: case 7017 case 6287 check aslr_heap_pad_areas - less
     * clear that this is an attack rather than stray execution, so
     * we'd want that check under a different flag.
     *
     * FIXME: case TOFILE: should have a flag to detect any read/write
     * exceptions in the aslr_wouldbe_areas or aslr_heap_pad_areas
     * areas and make sure they are incompatibilities or real
     * application bugs, although maybe present only with
     * randomization so considered incompatibilities.
     */
    return TESTALL(ASLR_TRACK_AREAS | ASLR_DETECT_EXECUTE, DYNAMO_OPTION(aslr_action)) &&
        vmvector_overlap(aslr_wouldbe_areas, target_addr, target_addr + 1);
}

/* returns NULL if not relevant or not found */
app_pc
aslr_possible_preferred_address(app_pc target_addr)
{
    if (TESTALL(ASLR_TRACK_AREAS | ASLR_DETECT_EXECUTE, DYNAMO_OPTION(aslr_action))) {
        app_pc wouldbe_module_current_base =
            vmvector_lookup(aslr_wouldbe_areas, target_addr);
        app_pc wouldbe_preferred_base;
        if (wouldbe_module_current_base == NULL) {
            /* note we check according to aslr_action (e.g. always
             * since default on) even in case ASLR was never enabled,
             * to be able to handle having -aslr dynamically disabled.
             * We add areas only when ASLR is enabled.
             */
            return NULL;
        }

        /* note that we don't have a vmvector interface to get the
         * base of the wouldbe area, from which we got this */
        /* but we anyways doublecheck with the loaded_module_areas as well */
        /* FIXME: such an interface is being added on the Marlin branch, use when ready */
        wouldbe_preferred_base =
            get_module_preferred_base_safe(wouldbe_module_current_base);
        ASSERT(vmvector_lookup(aslr_wouldbe_areas, wouldbe_preferred_base) ==
                   wouldbe_module_current_base ||
               /* FIXME case 10727: if serious then let's fix this */
               check_filter("win32.reload-race.exe",
                            get_short_name(get_application_name())));
        return target_addr - wouldbe_preferred_base + wouldbe_module_current_base;
    } else {
        ASSERT_NOT_TESTED();
        return NULL;
    }
}

static bool
aslr_reserve_remote_random_pad(HANDLE process_handle, size_t pad_size)
{
    NTSTATUS res;
    HANDLE child_handle = process_handle;
    void *early_reservation_base = NULL; /* allocate earliest possible address */

    size_t early_reservation_delta = get_random_offset(pad_size);
    size_t early_reservation_size =
        ALIGN_FORWARD(early_reservation_delta, ASLR_MAP_GRANULARITY);
    ASSERT(!is_phandle_me(process_handle));

    res = nt_remote_allocate_virtual_memory(child_handle, &early_reservation_base,
                                            early_reservation_size, PAGE_NOACCESS,
                                            MEMORY_RESERVE_ONLY);
    ASSERT(NT_SUCCESS(res));
    /* not a critical failure if reservation has failed */

    LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1,
        "ASLR: initial padding in child " PIFX ", pad base=" PFX ", size=" PIFX
        ", res=0x%x\n",
        child_handle, early_reservation_base, early_reservation_size, res);

    /* FIXME: case 7017 should pass the early
     * reservation region to child for detecting exploits
     * targeting a predictable stack (for ASLR_STACK), need to identify a good
     * stable across core versions mechanism.  Once that's there
     * child should ASSERT its stack immediately follows this.
     *
     * Alternatively, for case 5366 we may choose to free this
     * padding, and if freeing we can use a lot larger initial one,
     * risking only fragmentation
     */
    return NT_SUCCESS(res);
}

/* FIXME: this routine bases its decisions on the parent options
 * instead of the target process, currently controlled by option
 * string options, too much effort to check remotely.
 */
/* may decide that the target is not a stack */
void
aslr_maybe_pad_stack(dcontext_t *dcontext, HANDLE process_handle)
{
    /* note that should be careful to properly detect only this is
     * done before very first thread injection in a newly created
     * process, otherwise we'd risk a virtual memory leak
     *
     * FIXME: case 7682 tracks correctly identifying remote thread
     * injectors other than parent process
     */
    ASSERT(!is_phandle_me(process_handle));

    /* we should only handle remote reservation from parent to
     * child */

    /* we check if child is at all configured, note that by doing this
     * check only for a presumed thread stack, we can rely on
     * ProcessParameters being created.  FIXME: Since the
     * ProcessParameters will get normalized from offsets to pointers
     * only when the child starts running, if this is not a first
     * child we may get a random or incorrect value - e.g. the global
     * settings if the read name is not good enough.
     */
    /* Remotely injected threads should not need this since will get
     * their padding from the general ASLR_HEAP in the child.
     */
    LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1,
        "ASLR: check if thread is in new child " PIFX "\n", process_handle);

    if (TEST(ASLR_STACK, DYNAMO_OPTION(aslr)) && DYNAMO_OPTION(aslr_parent_offset) > 0 &&
        should_inject_into_process(get_thread_private_dcontext(), process_handle, NULL,
                                   NULL)) {
        /* Case 9173: ensure we only do this once, as 3rd party
         * hookers allocating memory can cause this routine to be
         * invoked many times for the same child
         */
        process_id_t pid = process_id_from_handle(process_handle);
        if (pid == dcontext->aslr_context.last_child_padded) {
            SYSLOG_INTERNAL_WARNING_ONCE("extra memory allocations for child " PIFX
                                         " %d: hooker?",
                                         process_handle, pid);
        } else {
            bool ok = aslr_reserve_remote_random_pad(process_handle,
                                                     DYNAMO_OPTION(aslr_parent_offset));
            ASSERT(ok);
            if (ok)
                dcontext->aslr_context.last_child_padded = pid;
        }
    } else {
        DODEBUG({
            if (TEST(ASLR_STACK, DYNAMO_OPTION(aslr)) &&
                DYNAMO_OPTION(aslr_parent_offset) > 0) {
                LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1,
                    "ASLR: child not configured for protection, not padding\n");
            }
        });
    }
}

#define LOADBLOCK_PAGE_PROTECT (PAGE_READWRITE | PAGE_GUARD)

/* Prevents the loader from mapping our DLL at its preferred base, by
 * allocating a random initial padding before we're loaded, padding is
 * independent of ASLR_STACK or ASLR_PROCESS_PARAM if those aren't
 * enabled.
 *
 * Child process should call aslr_free_dynamorio_loadblock() to free
 * FIXME: may want to make this routine available for pre_inject.c
 *
 * Note option active in parent and applies to its children.
 * FIXME: Eventually should share views similar to ASLR_SHARED_CONTENT.
 */
void
aslr_force_dynamorio_rebase(HANDLE process_handle)
{
    /* We'll assume that the preferred base is the same in parent and
     * child */
    app_pc preferred_base = get_dynamorio_dll_preferred_base();
    NTSTATUS res;
    DEBUG_DECLARE(bool ok;)
    LOG(THREAD_GET, LOG_SYSCALLS | LOG_THREADS, 1, "\ttaking over expected DLL base\n");

    ASSERT(DYNAMO_OPTION(aslr_dr));

    ASSERT(!is_phandle_me(process_handle));

    res =
        nt_remote_allocate_virtual_memory(process_handle, &preferred_base, 1 * PAGE_SIZE,
                                          LOADBLOCK_PAGE_PROTECT, MEM_RESERVE);
    ASSERT(NT_SUCCESS(res));
    /* not critical if we fail, though failure expected only if
     * the target executable is also at our preferred base */

    /* child process should free the page at preferred base if it
     * looks like what we have created to not fragment the address
     * space.
     */

    /* no need to do both */
    if (!(TEST(ASLR_STACK, DYNAMO_OPTION(aslr)))) {
        /* random padding to have the loader load us in a not so
         * determinstic location */
        DEBUG_DECLARE(ok =)
        aslr_reserve_remote_random_pad(process_handle, DYNAMO_OPTION(aslr_parent_offset));
        ASSERT(ok);
    } else {
        /* do nothing, ASLR_STACK will add a pad */
    }
    /* FIXME: note that we should pass this region just as ASLR_STACK
     * is supposed to so that the child can free that region, yet only
     * at beginning of address space, and can double as extra heap
     * randomization
     */
}

void
aslr_free_dynamorio_loadblock(void)
{
    /* we don't want the l-roadblock to be a tombstone and get in the
     * way of other allocations, so we'll try to clean it up.
     */

    /* we also need to make sure that we have the preferred_base base
     * collected earlier */
    app_pc preferred_base = get_dynamorio_dll_preferred_base();
    NTSTATUS res;
    MEMORY_BASIC_INFORMATION mbi;

    /* note that parent may have had different settings */
    ASSERT(DYNAMO_OPTION(aslr_dr));

    if (get_dynamorio_dll_start() == preferred_base) {
        /* not rebased, no loadblock to free */
        return;
    }

    /* first check whether we have allocated this */
    if (query_virtual_memory(preferred_base, &mbi, sizeof(mbi)) == sizeof(mbi)) {
        /* FIXME: the only way to get a strong guarantee that no other
         * block is allocated at our preferred base by passing the
         * loadblock information to the child.  This check using
         * trying unusual combination of State and AllocationProtect
         * will make very unlikely we'd accidentally free something
         * else. */
        if (mbi.RegionSize == PAGE_SIZE && mbi.State == MEM_RESERVE &&
            mbi.Type == MEM_PRIVATE && mbi.AllocationProtect == LOADBLOCK_PAGE_PROTECT) {
            LOG(GLOBAL, LOG_SYSCALLS | LOG_THREADS, 1,
                "\t freeing loadblock at preferred base\n");
            res = nt_free_virtual_memory(preferred_base);
            ASSERT(NT_SUCCESS(res));
        } else {
            /* We'd expect mbi.State==MEM_FREE, or the large reserved block
             * that cygwin apps use if we come in late, or an executable
             * at our preferred base (for which this will fire).
             */
            ASSERT_CURIOSITY(mbi.State == MEM_FREE || !dr_early_injected);
            LOG(GLOBAL, LOG_SYSCALLS | LOG_THREADS, 1,
                "something other than loadblock, leaving as is\n");
        }
    }
}

/* post processing of successful application reservations */
void
aslr_post_process_allocate_virtual_memory(dcontext_t *dcontext,
                                          app_pc last_allocation_base,
                                          size_t last_allocation_size)
{
    ASSERT(ALIGNED(last_allocation_base, PAGE_SIZE));
    ASSERT(ALIGNED(last_allocation_size, PAGE_SIZE));

    ASSERT(TEST(ASLR_HEAP_FILL, DYNAMO_OPTION(aslr)));
    if (DYNAMO_OPTION(aslr_reserve_pad) > 0) {
        /* We need to randomly pad memory around each memory
         * allocation as well.  Conservatively, we reserve a new
         * region after each successful native reservation and would
         * have to free it whenever the target region itself is freed.
         * Assumption: one can't free separately allocated regions
         * with a single NtFreeVirtualMemory.
         *
         * Alternatively we can increase the size of the allocation,
         * at the risk of breaking some application.  Further even
         * more risky, within a larger reservation, we could return a
         * base that is not at the allocation granularity (but I
         * wouldn't consider not returning at page granularity).
         * Instead of actually keeping the reservation we could just
         * forcefully reserve at a slighly padded address without
         * really keeping the reservation ourselves.
         */
        heap_error_code_t error_code;
        size_t heap_pad_delta = get_random_offset(DYNAMO_OPTION(aslr_reserve_pad));
        size_t heap_pad_size = ALIGN_FORWARD(heap_pad_delta, ASLR_MAP_GRANULARITY);
        app_pc heap_pad_base;
        app_pc append_heap_pad_base = (app_pc)ALIGN_FORWARD(
            last_allocation_base + last_allocation_size, ASLR_MAP_GRANULARITY);
        bool immediate_taken = get_memory_info(append_heap_pad_base, NULL, NULL, NULL);
        /* there may be an allocation immediately tracking us, or a
         * hole too small for our request.
         *
         * FIXME: get_memory_info() should provide size of hole, but
         * can't change the interface on Linux easily, so not using
         * that for now, so we just try
         */

        if (immediate_taken) {
            STATS_INC(aslr_heap_giveup_filling);
            /* FIXME: TOFILE we shouldn't give up here if we also want
             * to fill uniformly */

            /* currently not adding a pad if the immediate next region
             * is already allocated e.g. MEM_MAPPED, or due to best
             * fit allocation/fragmentation virtual memory allocation
             * is in non-linear order)
             */

            LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1,
                "ASLR: ASLR_HEAP: giving up since next region " PFX " is taken\n",
                append_heap_pad_base);

            return;
        }

        /* racy memory reservation with respect to other threads, but
         * if we didn't get one where we wanted it to be, unlikely to
         * be useful to attackers if not deterministic.
         */
        heap_pad_base = os_heap_reserve(append_heap_pad_base, heap_pad_size, &error_code,
                                        false /*ignored on Windows*/);
        if (heap_pad_base == NULL) {
            /* unable to get preferred, let the os pick a spot */
            /* FIXME - remove this - no real reason to reserve if we can't get our
             * preferred, but the old os_heap_reserve implementation automatically
             * tried again for us and the code below assumes so. */
            heap_pad_base = os_heap_reserve(NULL, heap_pad_size, &error_code,
                                            false /*ignored on Windows*/);
        }

        LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 2,
            "ASLR: ASLR_HEAP: reserved pad base=" PFX ", size=" PIFX
            ", err=%x, after " PFX "\n",
            heap_pad_base, heap_pad_size, error_code, append_heap_pad_base);

        ASSERT_CURIOSITY(
            NT_SUCCESS(error_code) ||
            check_filter("win32.oomtest.exe", get_short_name(get_application_name())));
        /* not critical functionality loss if we have failed to
         * reserve this memory, but shouldn't happen */
        if (NT_SUCCESS(error_code)) {
            /* FIXME: currently nt_remote_allocate_virtual_memory()
             * automatically retries for the next available region and for
             * dual meaning of padding to mean waste some memory to detect
             * brute force fill attacks, we can keep the allocation.
             *
             * However, we'd need a way to quickly lookup a region getting
             * freed to find its corresponding pad.
             * FIXME: For now on race I'd immediately give up the padding.
             */
            /* FIXME: we checked earlier only if the immediate next
             * region is already allocated, but when the size of the
             * allocation is too large we also miss here
             */
            if (heap_pad_base != append_heap_pad_base) {
                size_t existing_size = 0;
                bool now_immediate_taken =
                    get_memory_info(append_heap_pad_base, NULL, &existing_size, NULL);
                /* FIXME: possible to simply not have enough room in current hole */
                /* or somebody else already got the immediate next region */
                ASSERT_CURIOSITY(!now_immediate_taken && "racy allocate");

                /* FIXME: get_memory_info() DOESN'T fill in size when MEM_FREE,
                 * this DOESN'T actually check existing_size it's just 0 */
                if (!now_immediate_taken && existing_size < heap_pad_size) {
                    /* FIXME: should we at least fill the hole? */
                    LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 2,
                        "ASLR: ASLR_HEAP: giving up, hole after region " PFX
                        " is too small, req " PIFX " hole\n",
                        append_heap_pad_base, heap_pad_size);
                    /* XXX: need to track these - is there too much fragmentation? */
                }

                STATS_INC(aslr_heap_giveup_filling);
                os_heap_free(heap_pad_base, heap_pad_size, &error_code);
                LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 2,
                    "ASLR: ASLR_HEAP: giving up, freed pad base=" PFX ", size=" PIFX
                    ", err=0x%x\n",
                    heap_pad_base, heap_pad_size, error_code);
                ASSERT(NT_SUCCESS(error_code));
            } else {
                /* register this allocation
                 * 1) so that we can lookup free from original base and return the memory
                 *
                 * 2) so that we can detect attempted execution from it and flag
                 */
                ASSERT(!vmvector_overlap(aslr_heap_pad_areas, heap_pad_base,
                                         heap_pad_base + heap_pad_size));
                /* FIXME: case 7017 should check the reservation region
                 * for detecting attacks targeting predictable heaps or
                 * brute force heap fill style attacks */
                vmvector_add(aslr_heap_pad_areas, heap_pad_base,
                             heap_pad_base + heap_pad_size,
                             last_allocation_base /* tag to match reservations */);
                ASSERT(vmvector_overlap(aslr_heap_pad_areas, heap_pad_base,
                                        heap_pad_base + heap_pad_size));
                ASSERT(vmvector_lookup(aslr_heap_pad_areas, heap_pad_base) ==
                       last_allocation_base);
                STATS_ADD_PEAK(aslr_heap_total_reservation, heap_pad_size / 1024);
                STATS_ADD_PEAK(aslr_heap_pads, 1);
                STATS_INC(ever_aslr_heap_pads);
            }
        } else {
            SYSLOG_INTERNAL_WARNING(
                "ASLR_HEAP_FILL: error " PIFX " on (" PFX "," PFX ")\n", error_code,
                append_heap_pad_base, append_heap_pad_base + heap_pad_size);

            /* FIXME: should try to flag if out of memory - could be
             * an application incompatible with too aggressive ASLR_HEAP_FILL
             *
             * (NTSTATUS) 0xc00000f2 - An invalid parameter was passed to a
             * service or function as the fourth argument.
             *
             * This was the result of 0x7ff90000+80000 = 0x80010000 which of course is
             * an invalid region.
             */

            /* or
             * Error code: (NTSTATUS) 0xc0000017 (3221225495) - {Not Enough Quota}
             * Not enough virtual memory or paging file quota is available to complete
             * the specified operation.
             */
            ASSERT_CURIOSITY(error_code == STATUS_INVALID_PARAMETER_4 ||
                             error_code == STATUS_NO_MEMORY);
        }
    }
}

/* should be called before application memory reservation is released.
 * Note that currently in addition to explicit memory free it is also
 * called for implicit stack release on XP+.
 * If application system call fails not a critical failure that we have freed a pad.
 */
void
aslr_pre_process_free_virtual_memory(dcontext_t *dcontext, app_pc freed_base,
                                     size_t freed_size)
{
    /* properly adjusted base and size for next allocation unit */
    app_pc expected_pad_base =
        (app_pc)ALIGN_FORWARD(freed_base + freed_size, ASLR_MAP_GRANULARITY);
    app_pc heap_pad_base, heap_pad_end;
    size_t heap_pad_size;
    heap_error_code_t error_code;
    ASSERT(ALIGNED(freed_base, PAGE_SIZE));
    ASSERT(ALIGNED(freed_size, PAGE_SIZE));

    /* should have had a pad */

    if (vmvector_lookup(aslr_heap_pad_areas, expected_pad_base) != NULL) {
        /* case 6287: due to handling MEM_COMMIT on stack allocations
         * now it is possible that the original MEM_RESERVE allocation
         * fails to pad (e.g. due to a MEM_MAPPED) allocation, yet the
         * later MEM_RESERVE|MEM_COMMIT has a second chance.  Rare, so
         * leaving for now. */
        ASSERT_CURIOSITY(vmvector_lookup(aslr_heap_pad_areas, expected_pad_base) ==
                         freed_base);

        /* need to remove atomically to make sure that nobody else is
         * freeing the same region at this point, otherwise on an
         * application double free race, we may attempt to double free
         * a region that may have been given back to the application.
         */
        vmvector_remove_containing_area(aslr_heap_pad_areas, expected_pad_base,
                                        &heap_pad_base, &heap_pad_end);
        ASSERT(heap_pad_base == expected_pad_base);
        ASSERT_CURIOSITY(!vmvector_overlap(aslr_heap_pad_areas, expected_pad_base,
                                           expected_pad_base + 1));

        /* have to free it up, even if we picked the wrong pad we
         * already removed it from vmvector */
        heap_pad_size = heap_pad_end - heap_pad_base;
        os_heap_free(heap_pad_base, heap_pad_size, &error_code);
        LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 2,
            "ASLR: ASLR_HEAP: freed pad base=" PFX ", size=" PIFX ", err=0x%x\n",
            heap_pad_base, heap_pad_size, error_code);
        ASSERT(NT_SUCCESS(error_code));

        STATS_SUB(aslr_heap_total_reservation, (heap_pad_size / 1024));
        STATS_DEC(aslr_heap_pads);
    } else {
        /* no overlap */
        ASSERT_CURIOSITY(!vmvector_overlap(aslr_heap_pad_areas, expected_pad_base,
                                           expected_pad_base + 1));
    }
}

/* called at startup to randomize immediately after known fixed
 * addresses, note that if a hole at preferred_base is not available
 * we let the OS choose an allocation
 */
static app_pc
aslr_reserve_initial_heap_pad(app_pc preferred_base, size_t reserve_offset)
{
    size_t heap_initial_delta = get_random_offset(reserve_offset);
    heap_error_code_t error_code;
    size_t heap_reservation_size =
        ALIGN_FORWARD(heap_initial_delta, ASLR_MAP_GRANULARITY);
    app_pc heap_reservation_base = os_heap_reserve(
        preferred_base, heap_reservation_size, &error_code, false /*ignored on Windows*/);
    if (heap_reservation_base == NULL) {
        /* unable to get a preferred, let the os pick a spot */
        /* FIXME - remove this - no real reason to reserve if we can't get our
         * preferred, but the old os_heap_reserve implementation automatically
         * tried again for us and the code below assumes so. */
        heap_reservation_base = os_heap_reserve(NULL, heap_reservation_size, &error_code,
                                                false /*ignored on Windows*/);
    }
    LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1,
        "ASLR: ASLR_HEAP: requested random offset=" PFX ",\n"
        "ASLR: reservation base=" PFX ", real size=" PFX ", err=0x%x\n",
        reserve_offset, heap_reservation_base, heap_reservation_size, error_code);

    ASSERT_CURIOSITY(NT_SUCCESS(error_code));
    /* not critical functionality loss if we have failed to
     * reserve this memory, but shouldn't happen */
    if (NT_SUCCESS(error_code)) {
        /* register this allocation */
        STATS_ADD(aslr_heap_initial_reservation, heap_reservation_size / 1024);
        STATS_ADD_PEAK(aslr_heap_total_reservation, heap_reservation_size / 1024);
        STATS_ADD_PEAK(aslr_heap_pads, 1);
        STATS_INC(ever_aslr_heap_pads);

        ASSERT(!vmvector_overlap(aslr_heap_pad_areas, heap_reservation_base,
                                 heap_reservation_base + heap_reservation_size));
        /* FIXME: case 7017 should check the reservation region
         * for detecting attacks targeting predictable heaps or
         * brute force heap fill style attacks */
        vmvector_add(aslr_heap_pad_areas, heap_reservation_base,
                     heap_reservation_base + heap_reservation_size, preferred_base);
        /* Note breaking invariant for custom field - this is not
         * base of previous allocation but initial padding or
         * executable are not supposed to be freed, and in case
         * there was a smaller region in front of our pad that
         * gets freed we still get to keep it
         */
    }
    return heap_reservation_base;
}

/* release all heap reservation pads go through the
 * aslr_heap_pad_areas, used on exit or detach.  There will still
 * be lasting effects due to fragmentation.
 *
 * FIXME: case 6287 on application! or on DR out of reservation memory
 * should release all heap pads as well - the big initial reservations
 * should help free up some.  Should do if case 6498 can be reproduced
 * with inflated reservation sizes.  Yet attackers may control the
 * reservation sizes and would force a failing large request, or may
 * be able to fill all available heap in smaller requests.
 */
static void
aslr_free_heap_pads(void)
{

    DEBUG_DECLARE(uint count_freed = 0;)

    vmvector_iterator_t vmvi;
    vmvector_iterator_start(aslr_heap_pad_areas, &vmvi);
    while (vmvector_iterator_hasnext(&vmvi)) {
        app_pc start, end;
        app_pc previous_base = vmvector_iterator_next(&vmvi, &start, &end);
        app_pc heap_pad_base = start; /* assuming not overlapping */
        size_t heap_pad_size = (end - start);
        heap_error_code_t error_code;

        os_heap_free(heap_pad_base, heap_pad_size, &error_code);
        LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 2,
            "ASLR: ASLR_HEAP: final cleanup pad base=" PFX ", size=" PIFX
            ", app_base=" PFX ", err=0x%x\n",
            heap_pad_base, heap_pad_size, previous_base, error_code);
        ASSERT(NT_SUCCESS(error_code));

        STATS_SUB(aslr_heap_total_reservation, (heap_pad_size / 1024));
        STATS_DEC(aslr_heap_pads);
        DODEBUG({ count_freed++; });
    }
    vmvector_iterator_stop(&vmvi);
    LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1, "aslr_free_heap_pads: %d freed\n",
        count_freed);
}

/* ASLR_SHARED_CONTENTS related functions */

/* File backing seems to be unavoidable to allow using our own private
 * section of a file, that is sharing the relocated version.  See
 * comments in aslr_experiment_with_section_handle() about the
 * different layers of file, section, and view content backing and
 * sharing.
 */

/* currently doesn't require callers to close_handle(),
 * Note: subject to change if we support impersonation
 */
HANDLE
get_relocated_dlls_filecache_directory(bool write_access)
{
    /* FIXME: today both publishers and producers are getting full access */

    /* the file cache may be per user (though impersonation may make
     * this harder) we'll assume that only cache of initial caller
     * should be used, while all other later impersonations will
     * simply fail to share anything.  (The FTP problem still exists
     * if the FTP user is allowed to create files.)
     */
    /* FIXME: currently only a single shared one, with full permissions */
    return relocated_dlls_filecache_initial;
}

/* opens the DLL cache directory for this user,
 * for now assuming both read and write privileges and opening a shared directory.
 */
/* FIXME: if per user may keep in e.g. \??\Program
 * Files\Determina\Determina Agent\cache\USER or some other better
 * per-USER location not under \Program Files.  Note nodemgr may not
 * be able to create these directories in advance, e.g. in a domain
 * where a new user may log in at any time.  For such a scenario,
 * maybe we really wouldn't want the per-user directory at all...
 */
static HANDLE
open_relocated_dlls_filecache_directory(void)
{
    char base_directory[MAXIMUM_PATH];
    wchar_t wbuf[MAXIMUM_PATH];
    HANDLE directory_handle;
    int retval;
    bool per_user = TEST(ASLR_SHARED_PER_USER, DYNAMO_OPTION(aslr_cache));

    /* FIXME: note a lot of overlap with code in os_create_dir() yet
     * that assumes we'll deal with file names, while I want to avoid
     * further string concatenation.  It also goes through a lot more
     * hoops for unique and not yet created paths, while here we
     * assume proper installer.
     */

    /* If not per user we use the SHARED directory which requires
     * content validation.   FIXME: note that ASLR_SHARED_INHERIT may
     * ask for opening two directories as trusted sources
     * DYNAMORIO_VAR_CACHE_ROOT (\cache) in addition to the a per USER
     * subdirectory \cache\SID
     */
    retval = d_r_get_parameter((per_user ? PARAM_STR(DYNAMORIO_VAR_CACHE_ROOT)
                                         : PARAM_STR(DYNAMORIO_VAR_CACHE_SHARED)),
                               base_directory, sizeof(base_directory));
    if (IS_GET_PARAMETER_FAILURE(retval) || strchr(base_directory, DIRSEP) == NULL) {
        SYSLOG_INTERNAL_ERROR(
            " %s not set!"
            " ASLR sharing ineffective.\n",
            (per_user ? DYNAMORIO_VAR_CACHE_ROOT : DYNAMORIO_VAR_CACHE_SHARED));
        return INVALID_HANDLE_VALUE;
    }
    NULL_TERMINATE_BUFFER(base_directory);

    LOG(GLOBAL, LOG_ALL, 1, "ASLR_SHARED: Opening file cache directory %s\n",
        base_directory);

    if (per_user) {
        /* for now we'll always create directory, since without
         * ASLR_SHARED_INHERIT almost every process will need to
         * create some non-exempt from sharing DLLs
         */
        bool res = os_current_user_directory(base_directory,
                                             BUFFER_SIZE_ELEMENTS(base_directory),
                                             true /* create if missing */);
        if (!res) {
            /* directory may be set even on failure */
            LOG(GLOBAL, LOG_CACHE, 2, "\terror creating per-user dir %s\n",
                base_directory);
            return INVALID_HANDLE_VALUE;
        }
    }

    /* now using potentially modified base_directory per-user */
    snwprintf(wbuf, BUFFER_SIZE_ELEMENTS(wbuf), GLOBAL_NT_PREFIX L"%hs", base_directory);
    NULL_TERMINATE_BUFFER(wbuf);

    /* the shared directory is supposed to be created by nodemgr as
     * world writable.  We should not create it if it doesn't exist
     * with FILE_OPEN_IF (if we did it would inherit the permissions
     * of the parent which are too restrictive).
     */
    directory_handle =
        create_file(wbuf, true /* is_dir */, READ_CONTROL /* generic rights */,
                    FILE_SHARE_READ
                        /* case 10255: allow persisted cache files
                         * in same directory */
                        | FILE_SHARE_WRITE,
                    FILE_OPEN, true);
    if (directory_handle == INVALID_HANDLE_VALUE) {
        SYSLOG_INTERNAL_ERROR(
            "%s=%s is invalid!"
            " ASLR sharing is ineffective.\n",
            (per_user ? DYNAMORIO_VAR_CACHE_ROOT : DYNAMORIO_VAR_CACHE_SHARED),
            base_directory);
    } else {
        /* note that now that we have the actual handle open, we can validate */
        if (per_user &&
            (DYNAMO_OPTION(validate_owner_dir) || DYNAMO_OPTION(validate_owner_file))) {
            /* see os_current_user_directory() for details */
            if (!os_validate_user_owned(directory_handle)) {
                /* we could report in release, but it's unlikely that
                 * it will get reported */
                SYSLOG_INTERNAL_ERROR(
                    "%s -> %s is OWNED by an impostor!"
                    " ASLR sharing is disabled.",
                    (per_user ? DYNAMORIO_VAR_CACHE_ROOT : DYNAMORIO_VAR_CACHE_SHARED),
                    base_directory);
                close_handle(directory_handle);
                directory_handle = INVALID_HANDLE_VALUE;
            } else {
                /* either FAT32 or we are the proper owner */

                /* FIXME: case 10504 we have to verify that the final permissions and
                 * sharing attributes for cache/ and for the current
                 * directory, do NOT allow anyone to rename our directory
                 * while in use, and replace it.  Otherwise we'd still
                 * have to verify owner for each file as well with -validate_owner_file.
                 */
            }
        }
    }
    return directory_handle;
}

/* note that this is currently done mostly as a hack, to allow fast
 * first level checksum comparison just based on a file handle.
 * Returns true if the files were the same size, or we have
 * successfully made them so.
 */
static bool
aslr_module_force_size(IN HANDLE app_file_handle, IN HANDLE randomized_file_handle,
                       const wchar_t *file_name, OUT uint64 *final_file_size)
{
    uint64 app_file_size;
    uint64 randomized_file_size;
    bool ok;
    ok = os_get_file_size_by_handle(app_file_handle, &app_file_size);
    if (!ok) {
        ASSERT_NOT_TESTED();
        return false;
    }

    ok = os_get_file_size_by_handle(randomized_file_handle, &randomized_file_size);
    if (!ok) {
        ASSERT_NOT_TESTED();
        return false;
    }

    if (randomized_file_size != app_file_size) {
        ASSERT(randomized_file_size < app_file_size);
        SYSLOG_INTERNAL_WARNING("aslr_module_force_size: "
                                "forcing %ls, padding %d bytes\n",
                                file_name, (app_file_size - randomized_file_size));

        /* note that Certificates Directory or debugging information
         * are the usual sources of such not-loaded by
         * NtMapViewOfSection memory.  Since we pass such file handle
         * only to SEC_IMAGE NtCreateSection() calls, we don't need to
         * call os_copy_file() to fill the missing data.  The
         * SEC_COMMIT use by the loader in ntdll!LdrpCheckForLoadedDll
         * will be given the original file.
         */
        ok = os_set_file_size(randomized_file_handle, app_file_size);
        if (!ok) {
            ASSERT_NOT_TESTED();
            return false;
        }

        ok = os_get_file_size_by_handle(randomized_file_handle, final_file_size);
        if (!ok) {
            ASSERT_NOT_TESTED();
            return false;
        }
        ASSERT(*final_file_size == app_file_size);
        if (*final_file_size != app_file_size) {
            ASSERT_NOT_TESTED();
            return false;
        }
        /* note we don't care whether we have had to force */
    } else {
        *final_file_size = randomized_file_size;
    }

    return true;
}

/* we expect produced_file_pointer to be a location where the file's
 * signature can be written
 */
static bool
aslr_module_append_signature(HANDLE produced_file, uint64 *produced_file_pointer,
                             aslr_persistent_digest_t *persistent_digest)
{
    bool ok;
    size_t num_written;

    persistent_digest->version = ASLR_PERSISTENT_CACHE_VERSION;
    persistent_digest->magic = ASLR_PERSISTENT_CACHE_MAGIC;

    /* Note we do not preclude having aslr_module_force_size()
     * always force the size to be the final size |app size|+|aslr_persistent_digest_t|
     * but unlikely we'd care to do this
     */
    DOLOG(1, LOG_SYSCALLS | LOG_VMAREAS, {
        LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1, "ASLR: aslr_module_append_signature:");
        LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1, "\n\t source.full :");
        /* FIXME: should abstract out the md5sum style printing from
         * get_md5_for_file() */
        dump_buffer_as_bytes(GLOBAL, persistent_digest->original_source.full_MD5,
                             MD5_RAW_BYTES, DUMP_RAW);
        LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1, "\n\t source.short:");
        dump_buffer_as_bytes(GLOBAL, persistent_digest->original_source.short_MD5,
                             MD5_RAW_BYTES, DUMP_RAW);

        LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1, "\n\t target.full :");
        dump_buffer_as_bytes(GLOBAL, persistent_digest->relocated_target.full_MD5,
                             MD5_RAW_BYTES, DUMP_RAW);
        LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1, "\n\t target.short:");
        dump_buffer_as_bytes(GLOBAL, persistent_digest->relocated_target.short_MD5,
                             MD5_RAW_BYTES, DUMP_RAW);
        LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1, "\n");
    });

    ok = write_file(produced_file, persistent_digest, sizeof(aslr_persistent_digest_t),
                    produced_file_pointer, &num_written);
    ASSERT(ok);
    ASSERT(num_written == sizeof(aslr_persistent_digest_t));
    ok = ok && (num_written == sizeof(aslr_persistent_digest_t));
    return ok;
}

static bool
aslr_module_read_signature(HANDLE randomized_file, uint64 *randomized_file_pointer,
                           OUT aslr_persistent_digest_t *persistent_digest)
{
    size_t num_read;
    bool ok;
    ok = read_file(randomized_file, persistent_digest, sizeof(*persistent_digest),
                   randomized_file_pointer, &num_read);
    ASSERT(ok);

    ok = ok && (num_read == sizeof(aslr_persistent_digest_t));
    ASSERT(ok);

    if (ok) {
        ok = persistent_digest->version == ASLR_PERSISTENT_CACHE_VERSION;
        ASSERT_CURIOSITY(ok && "invalid version");
    }

    if (ok) {
        ok = persistent_digest->magic == ASLR_PERSISTENT_CACHE_MAGIC;
        ASSERT_CURIOSITY(ok && "bad magic");
    }

    /* Where can we store any additional checksums and metadata:
     * - [CURRENTLY] after the end of the file - just like certificates and
     *   debugging information is in PEs, we could replace the
     *   existing certificates, but we simply pad the file with 0 for those,
     *   and add our signature after the end of the file.

     * Alternatives:
     * - in a PE field - good enough if using only a 32bit Checksum
     * - NTFS streams - no, since we need to support FAT32

     * - in a separate file or .chksum or for many files in a .cat
     * most flexible though adds overhead.  Could throw in the
     * registry, but we already have to secure the files so easier to
     * use the same permissions.
     *
     *  <metadata>  <!- not really going be in xml -->
     *    name=""
     *    <original>
     *     <checksum md5|d_r_crc32|sha1= /> <-- staleness -->
     *    <rebased>
     *     <checksum md5|d_r_crc32|sha1= /> <-- corruption -->
     *  </metadata>
     *  <hash>md5(metadata)</hash>
     *
     * - append to file name - content-based addressing
     *   possible only for data based on original application file
     */

    /* FIXME: for unique name we can add the PE section Image.Checksum
     * to generate different ID's.  Note we do not keep different
     * possible mappings for the same name.  So we hope no two
     * simultaneously needed files will clobber each other due to name
     * collision.
     *
     * FIXME: yet we still need to verify any calculated checksum between our
     * generated file and the file that it purportedly backs
     * or better yet fully compare it
     */

    /* see reactos/0.2.9/lib/ntdll/ldr/utils.c for the original
     * LdrpCheckImageChecksum, though we could produce our own d_r_crc32()
     * checksum on original file as well and store it as checksum of
     * our generated file in some PE orifice.
     */

    /* see pecoff v6 Appendix B, or pecoff v8 Appendix A: Calculating
     * Authenticode PE Image Hash for reference
     * where Checksum and Certificate Tables are excluded
     */

    return ok;
}

/* For our relocated version we should be validating a private section
 * before publishing one.  Note that when calculating digest on
 * original application section we have a section handle already that
 * is assumed to be private.
 */
static bool
aslr_get_section_digest(OUT module_digest_t *digest, HANDLE section_handle,
                        bool short_digest_only)
{
    NTSTATUS res;
    app_pc base = (app_pc)0x0;
    size_t commit_size = 0;

    SIZE_T view_size = 0;
    /* full file view, since even our short digest includes both
     * header and footer */

    uint type = 0; /* commit not needed for original DLL */
    uint prot = PAGE_READONLY;

    res = nt_raw_MapViewOfSection(section_handle,     /* 0 */
                                  NT_CURRENT_PROCESS, /* 1 */
                                  &base,              /* 2 */
                                  0,                  /* 3 */
                                  commit_size,        /* 4 */
                                  NULL,               /* 5 */
                                  &view_size,         /* 6 */
                                  ViewShare,          /* 7 */
                                  type,               /* 8 */
                                  prot);              /* 9 */
    ASSERT(NT_SUCCESS(res));
    if (!NT_SUCCESS(res))
        return false;
    /* side note: windbg receives a ModLoad: for our temporary mapping
     * at the NtMapViewOfSection(), no harm */

    module_calculate_digest(digest, base, view_size, !short_digest_only, /* full */
                            short_digest_only,                           /* short */
                            DYNAMO_OPTION(aslr_short_digest), UINT_MAX /*all secs*/,
                            0 /*all secs*/);
    res = nt_raw_UnmapViewOfSection(NT_CURRENT_PROCESS, base);
    ASSERT(NT_SUCCESS(res));
    return true;
}

/* returns a private image section.
 * a simple wrapper around nt_create_section() with common attributes
 * on success callers need to close_handle() after use
 */
static inline bool
aslr_create_private_module_section(OUT HANDLE private_section, HANDLE file_handle)
{
    NTSTATUS res;
    res = nt_create_section(private_section,
                            SECTION_ALL_ACCESS, /* FIXME: maybe less privileges needed */
                            NULL,               /* full file size */
                            PAGE_EXECUTE,
                            /* PAGE_EXECUTE gives us COW in readers
                             * but can't share any changes.
                             * Unmodified pages are always shared.
                             */
                            /* PAGE_EXECUTE_READWRITE - gives us true
                             * overwrite ability only in SEC_COMMIT */
                            /* PAGE_EXECUTE_WRITECOPY is still COW,
                             * though it also needs FILE_READ_DATA
                             * privileges to at all create the section
                             * which the loader doesn't use */
                            SEC_IMAGE, file_handle,

                            /* process private - no security needed */
                            /* object name attributes */
                            NULL /* unnamed */, 0, NULL, NULL);
    ASSERT_CURIOSITY(NT_SUCCESS(res) && "create failed - maybe invalid PE");
    /* seen STATUS_INVALID_IMAGE_FORMAT when testing non-aligned PE base */
    if (!NT_SUCCESS(res))
        return false;
    return true;
}

static bool
aslr_get_file_digest(OUT module_digest_t *digest, HANDLE relocated_file_handle,
                     bool short_only)
{
    /* Keep in mind that we have to create a private section mapping
     * before we publish it for other consumers to use
     * in aslr_publish_section_handle
     */

    /* note we produce all of these on MEM_IMAGE versions so that the
     * kernel doesn't have to page in both MEM_IMAGE and MEM_MAPPED
     * copies, and the only cost of these is the extra virtual address
     * remappings
     */

    /* see comments in aslr_get_original_metadata() about sharing some
     * of the extraneous mappings
     */
    HANDLE private_section;
    bool ok;

    ok = aslr_create_private_module_section(&private_section, relocated_file_handle);
    if (!ok)
        return false;

    ok = aslr_get_section_digest(digest, private_section, short_only);

    close_handle(private_section);
    /* Note: we may need to keep this handle OPEN if that is to
     * guarantee that the file cannot be overwritten.  Assuming that
     * effect is already achieved by the flags we use to open the file
     * and we will not close the file handle until finished.
     */

    return ok;
}

/* Caller must unmap mapping if original_mapped_base != NULL
 * regardless of return value.
 *
 * Also see notes in aslr_generate_relocated_section() which this
 * routine mostly borrows from.  Comparing in place avoids the
 * CopyOnWrite faults and associated page copies.
 */
static bool
aslr_compare_in_place(IN HANDLE original_section, OUT app_pc *original_mapped_base,
                      OUT size_t *original_mapped_size,

                      app_pc suspect_mapped_base, size_t suspect_mapped_size,
                      app_pc suspect_preferred_base, size_t validation_prefix)
{
    bool ok;
    NTSTATUS res;

    HANDLE section_handle = original_section;
    app_pc base = (app_pc)0x0;
    size_t commit_size = 0;
    SIZE_T view_size = 0; /* full file view */
    uint type = 0;        /* commit not needed for original DLL */
    uint prot = PAGE_READWRITE;
    /* PAGE_READWRITE would allow us to update the backing section */
    /* PAGE_WRITECOPY - will only provide the current mapping */

    app_pc original_preferred_base;

    ASSERT(*original_mapped_base == NULL);

    res = nt_raw_MapViewOfSection(section_handle,     /* 0 */
                                  NT_CURRENT_PROCESS, /* 1 */
                                  &base,              /* 2 */
                                  0,                  /* 3 */
                                  commit_size,        /* 4 */
                                  NULL,               /* 5 */
                                  &view_size,         /* 6 */
                                  ViewShare,          /* 7 */
                                  type,               /* 8 */
                                  prot);              /* 9 */
    ASSERT_CURIOSITY(NT_SUCCESS(res));
    if (!NT_SUCCESS(res)) {
        *original_mapped_base = NULL;
        return false;
    }

    *original_mapped_base = base;
    *original_mapped_size = view_size;

    /* Be aware of LdrVerifyImageMatchesChecksum() for our relocations
     * - but that maps in as SEC_COMMIT based on the original file, so
     * even if it is called for anything other than what is exported
     * in KnownDlls we'd be ok.  If we want to match that checksum we
     * can follow suit and process the file image, or we can emulate
     * that on a mapped image Section.
     *
     * FIXME: check what is the meaning of
     * "IMAGE_DLL_CHARACTERISTICS_FORCE_INTEGRITY 0x0080 Code
     * Integrity checks are enforced", documented in PECOFF v8.0
     */
    original_preferred_base = get_module_preferred_base(base);
    if (original_preferred_base == NULL) {
        ASSERT_CURIOSITY(false && "base at 0, bad PE?");
        /* maybe not a PE */
        ASSERT_NOT_TESTED();
        return false;
    }

    if (suspect_preferred_base == original_preferred_base) {
        /* note we don't really care */
        ASSERT_CURIOSITY(false && "old and new base the same!");
        ASSERT_NOT_TESTED();
        /* FIXME: we may want to force the new base in
         * aslr_generate_relocated_section() to never be the same as
         * original, but that may or may not be generally good,
         * remember Enigma */
    }

    ok = (*original_mapped_size == suspect_mapped_size) &&
        module_contents_compare(*original_mapped_base, suspect_mapped_base,
                                *original_mapped_size, false /* not relocated */,
                                suspect_preferred_base - original_preferred_base,
                                validation_prefix);
    return ok;
}

/* paranoid mode check that a provided memory area is what it claims
 * to be.  FIXME: note the relocated file should have such permissions
 * that its contents cannot be overwritten after this point.
 *
 */
static bool
aslr_module_verify_relocated_contents(HANDLE original_file_handle,
                                      HANDLE suspect_file_handle)
{
    /* In paranoid mode: should verify that the image is exactly the
     * same as the original except for the relocations which should be
     * exactly what we expect.
     */

    HANDLE original_file_section;
    app_pc relocated_original_mapped_base = NULL;
    size_t relocated_original_size = 0;

    HANDLE suspect_file_section;
    app_pc suspect_base = NULL; /* any base */
    SIZE_T suspect_size = 0;    /* request full file view */
    app_pc suspect_preferred_base;
    bool ok;
    NTSTATUS res;

    size_t validation_prefix =
        (TEST(ASLR_PERSISTENT_PARANOID_PREFIX, DYNAMO_OPTION(aslr_validation))
             ? DYNAMO_OPTION(aslr_section_prefix)
             : POINTER_MAX);

    /* create a private section for suspect  */
    ok = aslr_create_private_module_section(&suspect_file_section, suspect_file_handle);
    if (!ok) {
        return false;
    }

    /* map relocated suspect copy */
    res = nt_raw_MapViewOfSection(suspect_file_section, /* 0 */
                                  NT_CURRENT_PROCESS,   /* 1 */
                                  &suspect_base,        /* 2 */
                                  0,                    /* 3 */
                                  0,                    /* 4 commit_size*/
                                  NULL,                 /* 5 */
                                  &suspect_size,        /* 6 */
                                  ViewShare,            /* 7 */
                                  0,                    /* 8 type */
                                  PAGE_READWRITE);      /* 9 prot */
    /* FIXME: we are asking for PAGE_READWRITE on the whole file -
     * affecting commit memory case 10251 */

    /* we can close the handle as soon as we have a mapping */
    close_handle(suspect_file_section);

    ASSERT_CURIOSITY(NT_SUCCESS(res) && "map failed - maybe invalid PE");
    if (!NT_SUCCESS(res)) {
        ASSERT_NOT_TESTED();
        return false;
    }

    /* FIXME: [minor perf] we should pass a handle to original section
     * which is available to all publishers
     */
    ok = aslr_create_private_module_section(&original_file_section, original_file_handle);
    if (!ok) {
        nt_raw_UnmapViewOfSection(NT_CURRENT_PROCESS, suspect_base);
        return false;
    }

    /* Compare relocated files byte by byte
     * 1.1) memcmp() relocated copy at the same base
     *
     * 1.1.1) [CURRENTLY] apply relocations to original DLL as if
     * going to the relocated DLL location
     *    memcmp(original privately relocated, relocated DLL)
     *    Note that our changes to a mapping of the original are only
     *    temporary (even if we did reuse the application section).
     * 1.1.2) alternatively we could transform the relocated section
     *    back into the original.  We would also have to be extra
     *    careful when processing a potentially rogue PE.
     *
     * Note that the MD5 sum of the relocated DLL may be more expensive
     * than comparing the DLLs adjusting for relocations.
     * Note we can't trust MD5s saved in the file.
     * 1.2) (MD5(relocated DLL) == MD5(original privately relocated)
     *
     * FIXME: [perf] Need to do some perf testing to see 1.1 is good enough -
     * note we will have to check this only once for publisher, not consumer
     *
     * 2.1) relocation at a time we'd save the extra private copy of
     * the pages we need to touch if we do this smarter. We need to
     * provide a compare function that for each section traverses
     * relocations to do the proper match and compares verbatim all
     * bytes between relocations.
     */

    suspect_preferred_base = get_module_preferred_base(suspect_base);
    ASSERT_CURIOSITY(suspect_preferred_base != NULL && "bad PE file");
    DODEBUG({
        if (suspect_preferred_base != suspect_base) {
            /* this is the earliest we know this DLL won't fit in this process */
            SYSLOG_INTERNAL_WARNING("DLL mapping is not shareable");
            /* of course we may have a conflict, and so DLL won't really be
             * shared if not loaded at preferred base
             */
        }
    });

    if (TEST(ASLR_PERSISTENT_PARANOID_TRANSFORM_EXPLICITLY,
             DYNAMO_OPTION(aslr_validation))) {
        KSTART(aslr_validate_relocate);
        /* note we're transforming our good section into the relocated one
         * including any header modifications
         */
        ok = (suspect_preferred_base != NULL) &&
            aslr_generate_relocated_section(
                 original_file_section, &suspect_preferred_base, false,
                 &relocated_original_mapped_base, &relocated_original_size,
                 NULL /* no digest */);
        KSTOP(aslr_validate_relocate);
        if (!ok) {
            ASSERT(relocated_original_mapped_base == NULL);
        } else {
            ASSERT(relocated_original_mapped_base != NULL);
        }

        ASSERT_CURIOSITY(ok && "invalid source file!");

        if (ok) {
            KSTART(aslr_compare);
            ok = (relocated_original_size == suspect_size) &&
                module_contents_compare(relocated_original_mapped_base, suspect_base,
                                        relocated_original_size,
                                        true /* already relocated */, 0,
                                        validation_prefix);
            KSTOP(aslr_compare);
        }
    } else {
        /* we must do the comparison in place */
        KSTART(aslr_compare);
        ok = aslr_compare_in_place(original_file_section, &relocated_original_mapped_base,
                                   &relocated_original_size, suspect_base, suspect_size,
                                   suspect_preferred_base, validation_prefix);
        KSTOP(aslr_compare);
        /* note we don't keep track whether failed due to bad original
         * file or due to mismatch with suspect file
         */
    }

    ASSERT_CURIOSITY((relocated_original_size == suspect_size) && "mismatched PE size");
    ASSERT_CURIOSITY(ok && "mismatched relocated file!");
    /* on failure here inspect with
     * 0:000> c poi(relocated_original_mapped_base) L13000 poi(suspect_base) */
    if (!ok) {
        SYSLOG_INTERNAL_ERROR("ASLR_SHARED: stale, corrupt or rogue file!");
    }

    if (relocated_original_mapped_base != NULL) {
        res =
            nt_raw_UnmapViewOfSection(NT_CURRENT_PROCESS, relocated_original_mapped_base);
        ASSERT(NT_SUCCESS(res));
    }

    /* FIXME: [perf] we unmap everything that was paged in (presumably
     * the whole file), yet we expect that the system cache will keep
     * the file views until the app maps it again.  Alternatively, we
     * could preserve this mapping and present it to the application
     * whenever it calls NtMapViewOfSection().  Note that then our
     * working set will visibly include these pages, so letting the
     * system cache keep these for us may be better (in addition to easier).
     */
    if (suspect_base != NULL) {
        res = nt_raw_UnmapViewOfSection(NT_CURRENT_PROCESS, suspect_base);
        ASSERT(NT_SUCCESS(res));
    }

    close_handle(original_file_section);

    return ok;
}

/* verify whether a randomized_file_handle is a valid backer of the
 * application file that's given to us.  Needed only for persistence.
 * FIXME: Otherwise, for sharing only have to figure out how to
 * safely create a file exclusively and allow only the producer to
 * become a publisher.
 */
static bool
aslr_verify_file_checksum(IN HANDLE app_file_handle, IN HANDLE randomized_file_handle)
{
    /* we do some basic sanity checking - is the
     * FileStandardInformation.EndOfFile the same for both the
     * original and the rebased file.  Otherwise disk file may be
     * corrupt.  Should preferably verify its new checksum as well if
     * requested.  For best consistency and security check a byte by
     * byte comparison is performed.
     *
     * case 8492 covers discussion on alternative checks to allow for
     * patching/tampering detection
     */
    uint64 app_file_size;
    uint64 randomized_file_size;
    uint64 adjusted_file_size;
    aslr_persistent_digest_t persistent_digest;

    module_digest_t calculated_digest;
    bool short_only = TEST(ASLR_PERSISTENT_SHORT_DIGESTS, DYNAMO_OPTION(aslr_validation));

    bool ok;
    ok = os_get_file_size_by_handle(app_file_handle, &app_file_size);
    if (!ok) {
        ASSERT_NOT_TESTED();
        return false;
    }

    ok = os_get_file_size_by_handle(randomized_file_handle, &randomized_file_size);
    if (!ok) {
        ASSERT_NOT_TESTED();
        return false;
    }

    /* minimum check is whether original_file_size matches, so that we
     * know we're not dealing with a corrupt randomized file Note
     * aslr_create_relocated_dll_file() shouldn't provide an
     * allocation hint in case that would immediately set the file
     * size.
     */

    adjusted_file_size = randomized_file_size - sizeof(aslr_persistent_digest_t);

    /*
     * Note that this scheme will not work if some other software (AV,
     * backup) adds any data after the end of our own files in the
     * DATA stream.
     *
     * FIXME: could make these size checks optional but there will be
     * very few things we can really do without our own signature
     * record.
     *
     * Note, somewhat not orthogonal, we in fact ignore any changes
     * that are not a part of the file, so we will not complain about
     * modifications of the original application, but we will complain
     * about appends.
     */

    if (adjusted_file_size != app_file_size) {
        SYSLOG_INTERNAL_WARNING("aslr_verify_file_checksum: "
                                "wrong size - stale or possibly corrupt file\n");
        /* note that as a publisher we should not proactively attempt
         * deleting the file, but we'll ask producer to attempt to
         * produce again.  Hopefully this time it will be produced
         * properly.  If we have a bug in the field with some DLL the
         * exemptions by name should allow us to skip trying over and
         * over.
         */
        return false;
    }

    /* at least we know that our signature version and magic match */
    /* always reading signature even if we won't need the fields */
    ok = aslr_module_read_signature(randomized_file_handle,
                                    /* expected pointer to signature */
                                    &adjusted_file_size, &persistent_digest);
    if (!ok) {
        return false;
    }

    /* FIXME: in order to not break the abstraction here we'll need to
     * use another private nt_create_section(),
     * nt_raw_MapViewOfSection() before officially publishing
     * Measure for performance problems and may streamline.
     */

    if (TEST(ASLR_PERSISTENT_MODIFIED_TIME, DYNAMO_OPTION(aslr_validation))) {
        /* FIXME: currently impossible to check application times */
        ASSERT_NOT_IMPLEMENTED(false);
        if (!ok) {
            SYSLOG_INTERNAL_WARNING("aslr_verify_file_checksum: "
                                    "modified time differs - stale file!\n");
            return false;
        }
    }

    if (TEST(ASLR_PERSISTENT_PARANOID, DYNAMO_OPTION(aslr_validation))) {
        ok = aslr_module_verify_relocated_contents(app_file_handle,
                                                   randomized_file_handle);
        if (!ok) {
            SYSLOG_INTERNAL_WARNING("aslr_verify_file_checksum: paranoid check failed "
                                    "- stale, corrupt, or rogue file!\n");
            /* FIXME: do we want to report to the authorities?  Maybe
             * only for rogues, then caller needs to verify in other
             * ways.  To make sure file wasn't truncated due to power
             * failure - corrupt; and doublecheck with MD5 of original
             * file to see if it is not just a stale file.
             */
            return false;
        }
    }

    if (TEST(ASLR_PERSISTENT_SOURCE_DIGEST, DYNAMO_OPTION(aslr_validation))) {
        /* FIXME: note that we should pass the original section to
         * aslr_publish_section_handle() and use
         * aslr_get_section_digest() instead of a private mapping
         */

        /* FIXME: the original file may have been opened in
         * non-exclusive mode but that is very unlikely, so we'll
         * assume our section can be used without a race
         */
        ok = aslr_get_file_digest(&calculated_digest, app_file_handle, short_only);
        if (!ok) {
            ASSERT_NOT_TESTED();
            return false;
        }

        if (!module_digests_equal(&persistent_digest.original_source, &calculated_digest,
                                  short_only, !short_only)) {
            SYSLOG_INTERNAL_WARNING("aslr_verify_file_checksum: "
                                    "invalid source checksum - stale!\n");
            return false;
        }
    }

    if (TEST(ASLR_PERSISTENT_TARGET_DIGEST, DYNAMO_OPTION(aslr_validation))) {
        /* FIXME: note that this routine should not be completely
         * trusted, if we're trying to prevent a high privileged
         * process from crashing on a bad DLL for extra safety we
         * may need to wrap this call in a try/except block.
         */
        ok = aslr_get_file_digest(&calculated_digest, randomized_file_handle, short_only);
        if (!ok) {
            ASSERT_NOT_TESTED();
            return false;
        }

        if (!module_digests_equal(&persistent_digest.relocated_target, &calculated_digest,
                                  short_only, !short_only)) {
            SYSLOG_INTERNAL_ERROR("aslr_verify_file_checksum: "
                                  "invalid target checksum - corrupt!\n");
            return false;
        }
    }

    if (!TESTANY(ASLR_PERSISTENT_PARANOID | ASLR_PERSISTENT_SOURCE_DIGEST |
                     ASLR_PERSISTENT_TARGET_DIGEST,
                 DYNAMO_OPTION(aslr_validation))) {
        SYSLOG_INTERNAL_WARNING_ONCE("aslr_verify_file_checksum: no checksum\n");
    }

    return true;
}

/* used by section publishers for providing our alternative file
 * backing in current user DLL file cache,
 *
 * returns true if module_name exists and is not stale.  Caller should
 * close file on success.
 */
static bool
aslr_open_relocated_dll_file(OUT HANDLE *relocated_file, IN HANDLE original_file,
                             const wchar_t *module_name)
{
    HANDLE relocated_dlls_directory = get_relocated_dlls_filecache_directory(false);
    NTSTATUS res;
    HANDLE new_file = INVALID_HANDLE_VALUE;

    if (relocated_dlls_directory == INVALID_HANDLE_VALUE) {
        return false;
    }

    /* FIXME: case 8494: staleness trigger
     * we may want to check for stale files - e.g. if not
     * asking for ASLR_PERSISTENT we want only freshly produced and
     * still open by publisher file.  In that case may want to try
     * exclusive access first - and if we can get it then the file is
     * not freshly produced.
     *
     * Alternatively, even if we allow persistence we may use the file
     * creation time to decide that a file has been created too long
     * ago (aslr_module_get_times()), or that is has been used too
     * many times (e.g. brute forcing a process).  Although such
     * measures improve resistance to brute force attack only as much
     * as one bit of randomness.
     *
     * If we do want to refuse loading a file on brute forcing, best
     * recourse is to switch to private ASLR.  If the file is too old,
     * but borderline old, such that some processes are still using
     * the old version, and for some reason we do not allow a new copy
     * to to supersede the existing produced file or published section
     * we should just wait until next reboot and use private ASLR in
     * the mean time as well.  Could also use alternative bases.
     *
     * Probably, best is to not bother with the above directly, but
     * instead use a reaper process (nodemgr.exe) which will regularly
     * schedule for removal files in FIFO, both for capacity
     * management and for staleness.
     */
    /* case 8494 FILE_SHARE_DELETE doesn't allow deletion of a memory
     * mapped file, so aside from the tight interval before we
     * nt_create_section() the file cannot be deleted.  Setting that
     * flag now would also allow successfully opening a file, if it has
     * been already marked for deletion, but we don't expect any such
     * in common use.
     */
    res = nt_create_module_file(&new_file, module_name, relocated_dlls_directory,
                                (DYNAMO_OPTION(validate_owner_file) ? READ_CONTROL : 0) |
                                    FILE_EXECUTE | FILE_READ_DATA,
                                FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ, FILE_OPEN, 0);
    if (NT_SUCCESS(res)) {
        if (DYNAMO_OPTION(validate_owner_file)) {
            if (!os_validate_user_owned(new_file)) {
                SYSLOG_INTERNAL_ERROR_ONCE("%S not owned by current process!"
                                           " ASLR cache may be compromised, not using.",
                                           module_name);
                close_handle(new_file);
                return false;
            }
        }

        if (!aslr_verify_file_checksum(original_file, new_file)) {
            close_handle(new_file);
            return false;
        }

        *relocated_file = new_file;
        return true;
    } else {
        LOG(GLOBAL, LOG_ALL, 2, "aslr_open_relocated_dll_file: res %x\n", res);
        if (res == STATUS_OBJECT_NAME_NOT_FOUND) {
            LOG(GLOBAL, LOG_ALL, 1, "aslr_open_relocated_dll_file: DLL not found\n");
        } else {
            if (res == STATUS_ACCESS_DENIED) {
                ASSERT_CURIOSITY(false &&
                                 "insufficient permissions, or non-executable file");
            } else if (res == STATUS_SHARING_VIOLATION) {
                ASSERT_NOT_TESTED();
                /* currently this may happen in a race with producer */
                ASSERT_CURIOSITY(false && "a producer has exclusive read lock");

            } else if (res == STATUS_DELETE_PENDING) {
                ASSERT_CURIOSITY(false && "some process has a handle to a deleted file");
                /* Maybe you're debugging and need to close windbg.
                 * Otherwise use procexp to find who owns the handle.
                 */
                /* very mysteriously windbg was holding a handle to a DLL that was
                 * consecutively rm'ed from cygwin which wasn't truly enough to allow
                 * $ ls -l
                 *   ls: dadkeyb.dll-12628e13: No such file or directory
                 *   total 104147
                 *   -rwxr-xr-x  1 vlk None   163903 May  3 21:09 dll.dll.dll-885d0011
                 * SIC! although listing the whole directory ls was complaining about
                 * the file.
                 *
                 */

                /* see SDK comments on what should DeleteFile() which
                 * uses NtSetInformationFile If we similarly delete the
                 * files, until all consumers are done with the file,
                 * new consumers may be allowed to map the exported
                 * section, new publishers should fail, while producers
                 * are supposed to request to supersede the file.
                 * There is only a tiny window in which we can delete file
                 * - before we create a section based on it.
                 */
            } else if (res == STATUS_FILE_IS_A_DIRECTORY) {
                /* 0xc00000ba - The file that was specified as a target is a
                 * directory and the caller specified that it could be anything
                 * but a directory.
                 */
                /* test example: mkdir unique_name */
                ASSERT_CURIOSITY(false && "a directory is in the way");
                /* FIXME: we should expect nodemgr to clean all files and directories */
            } else {
                ASSERT(false);
            }

            /* maybe insufficient privileges */
        }
        return false;
    }
}

/* returns false if we are too low on disk to create a file of desired size */
bool
aslr_check_low_disk_threshold(uint64 new_file_size)
{
    /* see case 8494 on other capacity triggers: low disk threshold,
     * should share heuristic with nodemgr though that won't depend on
     * request size, but on current total use, as well as available
     * disk space.  Note that we do not track current use but instead
     * leave nodemgr to calculate that itself (if not done too
     * frequently).  We may want to have nodemgr run any cache
     * trimming only in case the total available disk space is below
     * another threshold.
     */
    bool ok;
    HANDLE producer_directory = get_relocated_dlls_filecache_directory(true);

    ok = check_low_disk_threshold(producer_directory, new_file_size);

    /* FIXME: should we memoize the value on failure so that we don't
     * bother even with a syscall in the future?  We'll then ignore
     * the potential for someone freeing up disk space. */
    /* Note that this should be present as a quick check.  If we are
     * the only writer to the volume then we'll keep checking for each
     * file until we get really close to the cache size.  If there are
     * other producers this check is racy so we may easily end up
     * beyond the desired minimum.  The same of course will happen if
     * any other application is writing to the disk.
     */

    return ok;
}

/* used by file producers for providing our alternative file backing */
/* callers should close the handle */
static bool
aslr_create_relocated_dll_file(OUT HANDLE *new_file, const wchar_t *unique_name,
                               uint64 original_file_size, bool persistent
                               /* hint whether file is persistent */)
{
    HANDLE our_relocated_dlls_directory = get_relocated_dlls_filecache_directory(true);
    uint attributes;
    size_t allocation_size = 0x0;
    NTSTATUS res;
    bool retry;
    uint attempts = 0;

    /* note that we could pass the original_file_size as allocation
     * size, but it's not obvious what does that really mean on
     * compressed volumes so it's better to not optimze.  While we
     * could get an immediate feedback if we're out of disk space, we
     * have to handle it anyways when we write to the file as
     * well. Note that aslr_verify_file_checksum() can now assume a
     * file is not truncated.
     */

    /* FIXME: should append a suffix to the file image */

    if (!persistent) {
        /* FIXME: if we're creating the file only for sharing but not for
         * persistence we should compare performance compared to FILE_ATTRIBUTE_NORMAL */
        /* FIXME: maybe should add FILE_FLAG_DELETE_ON_CLOSE? */

        /* FIXME: we may want to always use FILE_ATTRIBUTE_TEMPORARY
         * even if persistent to push disk write to better times, but
         * if never written before power off may be bad for persistence. */
        attributes = FILE_ATTRIBUTE_TEMPORARY;
    } else {
        attributes = FILE_ATTRIBUTE_NORMAL;
        ASSERT_NOT_TESTED();
    }

    /* FIXME: in a race between producers the exclusive access should
     * prevent them from overwriting the same file.  using
     * FILE_SUPERSEDE for the case of a file has been created already
     * we wouldn't get this far unless it was deemed corrupt!  Note
     * that there is a tiny window in which serialized producers will
     * supersede each other's copies, which wouldn't happen with
     * FILE_CREATE but we cannot return success in that case either.
     * also cf. FILE_OVERWRITE_IF */

    /* to prevent malicious hard link (or symbolic link introduced in
     * Longhorn) we need to make sure we overwrite the link itself not
     * its target.  FILE_SUPERSEDE unfortunately overwrites the target
     * FIXME: there is a race so we should return failure and not
     * attempt to produce a file if after we have deleted a file it is
     * still present.
     */
    do {
        retry = false;
        attempts++;

        res = nt_create_module_file(
            new_file, unique_name, our_relocated_dlls_directory,
            READ_CONTROL | FILE_READ_DATA | FILE_WRITE_DATA, attributes, 0,
            /* exclusive read/write access */
            FILE_CREATE /* create only if non-existing */
                /* case 10884: needed only for validate_owner_file */
                | FILE_DISPOSITION_SET_OWNER,
            allocation_size);
        /* FIXME: adding FILE_SHARE_DELETE would allow us to supersede
         * a file that has been marked for deletion while in use.
         * However that normally isn't useful since we map sections
         * from these files which precludes deletion
         * (STATUS_CANNOT_DELETE) .  Rogue users getting in our way
         * can always just open exclusively.
         */

        if (NT_SUCCESS(res)) {
            ASSERT_CURIOSITY(os_validate_user_owned(*new_file) &&
                             "DLL loaded while impersonating?");
            return true;
        }

        /* note that name collision error should be returned before
         * any other reason for failure is found, e.g. non-executable
         * etc. so we can attempt to delete the file
         */
        if (res == STATUS_OBJECT_NAME_COLLISION) {
            bool deleted;

            ASSERT_CURIOSITY(attempts == 1 &&
                             "ln attack, in use, or race with another producer");
            /* the file could legally be in use only if valid for a
             * different core version */

            deleted = os_delete_file_w(unique_name, our_relocated_dlls_directory);
            if (deleted) {
                SYSLOG_INTERNAL_WARNING("deleted (invalid) file %ls in the way",
                                        unique_name);
                retry = true;

                /* note that even if we have marked for deletion the file
                 * will really disappear only when the last user is done
                 * with it
                 */

                /* Note deleted file creation time may be preserved : see
                 * MSDN on CreateFile "If you rename or delete a file and
                 * then restore it shortly afterward, the system searches
                 * the cache for file information to restore. Cached
                 * information includes its short/long name pair and
                 * creation time."
                 */
            } else {
                ASSERT_CURIOSITY(deleted && "can't delete: maybe directory");
            }
        }
        /* normally attempt 1 should succeed,
         * if corrupt and successfully deleted should succeed on attempt 2
         * only in some odd race one would try for a third time
         */
    } while (retry && attempts <= 3);

    return false;
}

/* returns true if file name is on exempt from ASLR list */
/* FIXME: merge or keep in synch with the checks based on a PE mapping
 * in aslr_post_process_mapview()
 */
static bool
is_aslr_exempted_file_name(const wchar_t *short_file_name)
{
    if (!IS_STRING_OPTION_EMPTY(exempt_aslr_default_list) ||
        !IS_STRING_OPTION_EMPTY(exempt_aslr_list) ||
        !IS_STRING_OPTION_EMPTY(exempt_aslr_extra_list) ||
        DYNAMO_OPTION(aslr_cache_list) != ASLR_CACHE_LIST_DEFAULT) {
        char file_name[MAXIMUM_PATH];
        /* need to convert since exemption lists work on char strings */
        /* name may also come directly from section name which for the
         * KnownDlls will have to match
         */

        /* -exempt_aslr_list '*' is really only interesting as a
         * stress test option, otherwise should just turn off ASLR_DLL
         */
        if (IS_LISTSTRING_OPTION_FORALL(exempt_aslr_list))
            return true;

        wchar_to_char(file_name, BUFFER_SIZE_ELEMENTS(file_name), short_file_name,
                      wcslen(short_file_name) * sizeof(wchar_t) /* size in bytes */);
        NULL_TERMINATE_BUFFER(file_name);

        /* note that almost all exempted DLLs are KnownDlls, and
         * kbdus.dll is ok in -hotp_only, yet to be prepared we still
         * get the name.  Note that we use a FILE name, not a PE name here.
         */

        /* we're using the same exemption list as private ASLR, though we
         * may separate these
         */
        if (check_list_default_and_append(dynamo_options.exempt_aslr_default_list,
                                          dynamo_options.exempt_aslr_list, file_name)) {
            SYSLOG_INTERNAL_WARNING("ASLR exempted from sharing DLL %s", file_name);
            return true;
        }

        /* FIXME: in fact we may want to share only these 'extra'
         * exempted from private ASLR DLLs only due to memory concerns */
        if (DYNAMO_OPTION(aslr_extra) &&
            check_list_default_and_append("", /* no default list */
                                          dynamo_options.exempt_aslr_extra_list,
                                          file_name)) {
            ASSERT_NOT_TESTED();
            SYSLOG_INTERNAL_WARNING("ASLR exempted extra DLL %s", file_name);
            return true;
        }

        if (DYNAMO_OPTION(aslr_cache_list) == ASLR_CACHE_LIST_INCLUDE &&
            /* using include list, exempt if NOT on list */
            !check_list_default_and_append("", /* no default list */
                                           dynamo_options.aslr_cache_include_list,
                                           file_name)) {
            SYSLOG_INTERNAL_WARNING("ASLR exempted DLL %s not on include list",
                                    file_name);
            return true;
        }
        if (DYNAMO_OPTION(aslr_cache_list) == ASLR_CACHE_LIST_EXCLUDE &&
            /* using exclude list, exempt if on list */
            check_list_default_and_append("", /* no default list */
                                          dynamo_options.aslr_cache_exclude_list,
                                          file_name)) {
            SYSLOG_INTERNAL_WARNING("ASLR exempted DLL %s on exclude list", file_name);
            return true;
        }
    }
    return false;
}

/* Returns a pointer to the short name within the long name that is copied
 * into name_info
 */
const wchar_t *
get_file_short_name(IN HANDLE file_handle, IN OUT FILE_NAME_INFORMATION *name_info)
{
    NTSTATUS res;
    /* note FileName is not NULL-terminated */
    res = nt_query_file_info(file_handle, name_info, sizeof(*name_info),
                             FileNameInformation);
    if (!NT_SUCCESS(res))
        return NULL;

    /* now have to properly NULL terminate the wide string we got */
    /* ok to overwrite the last character  */
    NULL_TERMINATE_BUFFER(name_info->FileName);
    if ((name_info->FileNameLength - sizeof(wchar_t)) <= sizeof(name_info->FileName)) {
        /* Length is supposed to be in bytes */
        name_info->FileName[name_info->FileNameLength / sizeof(wchar_t)] = 0;
    }

    /* very unlikely that we'd get a relative name, then we'll get full name */
    return w_get_short_name(name_info->FileName);
}

/* returns true if a likely unique generated_name name was successfully
 * produced.  Note name collisions are possible, so callers need to
 * ensure sections correspond to the same file by other means.
 *
 * produced name is guaranteed to have no backslashes,
 */
static bool
calculate_publish_name(wchar_t *generated_name /* OUT */,
                       uint max_name_length /* in elements */, HANDLE file_handle,
                       HANDLE section_handle)
{
    /* FIXME: if we are post-processing a successful app
     * NtCreateSection/NtOpenSection can also map it and calculate any
     * other interesting properties.  Note we won't have the FILE for
     * KnownDlls!  We need to somehow inherit from the original
     * section, or we'd need to guess the file name.  Or, otherwise
     * recreate it in the way we need it based on KnownDllPath.  If we
     * do this based on the section not file can handle both the same
     * way, otherwise need to assume that KnownDlls are the only ones
     * opened via OpenSection, and we'd open them appropriately and
     * maybe use are_mapped_files_the_same() to doublecheck, or use
     * NtQueryObject ObjectNameInformation to verify the directory
     * used is the expected KnownDlls.
     */

    /* We're using only file attributes here:
     * - FileNameInformation - path name.  See DDK for the odd case in
     * which a relative path will be returned, if a user has
     * SeChangeNotifyPrivilege ZwQueryInformationFile returns the full
     * path in all cases. Note usually all process do have
     * SeChangeNotifyPrivilege so we can always expect a full path.
     * Since we will only use it for a hash, even that is OK as long
     * as all users get it the same way.
     *
     * - FileStandardInformation - EndOfFile as a byte offset
     *
     * Other more restrained sources we could have used -
     * file: FileBasicInformation - can use create/access/times, however
     * requires FILE_READ_ATTRIBUTES; FileInternalInformation - gives
     * us a unique file ID but is valid only on NTFS
     *
     * Section: SectionBasicInformation.Size or
     * SectionImageInformation.EntryPoint can also be used for an
     * image if we had Query access but we don't have that on
     * KnownDlls until we map them in.
     *
     * Mapped PE: PE name, timestamp, checksum - none is reliable
     * enough by itself. We could use add a hash of the PE header
     * (like FX!32) or the short MD5 module_digest_t.  Yet for most
     * practical cases files with same name but different contents
     * will not have simulatenous lifetimes.
     *
     * One could use consider only content-based naming and use a
     * hash of the mapping (maybe short) and ignore names - then any
     * files with the same content will use the same copy.  Main
     * benefit that it allows multiple paths to the same files to
     * match.  Not so useful if we use a publisher cache that
     * consumers can quickly lookup with a faster name hash and file
     * handle comparison.
     */
    NTSTATUS res;
    const wchar_t *short_name;
    uint name_hash;
    uint final_hash;

    FILE_STANDARD_INFORMATION standard_info;
    FILE_NAME_INFORMATION name_info; /* note: large struct */
    /* note FileName is not NULL-terminated */

    res = nt_query_file_info(file_handle, &standard_info, sizeof(standard_info),
                             FileStandardInformation);
    if (!NT_SUCCESS(res)) {
        /* should always be able to get this */
        ASSERT(false && "bad handle?");
        return false; /* can't generate name */
    }

    short_name = get_file_short_name(file_handle, &name_info);
    if (short_name == NULL) {
        ASSERT(false);
        return false; /* can't generate name */
    }

    /* name hash over the wide char as bytes (many will be 0's but OK) */
    name_hash = d_r_crc32((char *)name_info.FileName, name_info.FileNameLength);

    /* xor over the file size as bytes */
    final_hash =
        name_hash ^ (standard_info.EndOfFile.LowPart ^ standard_info.EndOfFile.HighPart);

    LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 2,
        "ASLR: calculate_publish_name: short name %ls\n"
        "  full file name '%ls', file size %d\n"
        "  name_hash " PFX ", final_hash " PFX "\n",
        short_name, name_info.FileName, standard_info.EndOfFile.LowPart, name_hash,
        final_hash);

    if (is_aslr_exempted_file_name(short_name)) {
        return false; /* exempted, shouldn't publish */
    }

    _snwprintf(generated_name, max_name_length, L"%s-" L_PFMT, short_name, final_hash);

    if (TEST(ASLR_INTERNAL_SHARED_NONUNIQUE, INTERNAL_OPTION(aslr_internal))) {
        /* stress testing: temporarily testing multiple file sections by unique
         * within process name */
        static int unique = 0;
        _snwprintf(generated_name, max_name_length, L"unique-7ababcd-%d", unique++);
    }

    generated_name[max_name_length - 1] = 0;
    ASSERT(w_get_short_name(generated_name) == generated_name &&
           generated_name[0] != L_EXPAND_LEVEL(DIRSEP));
    return true; /* name should be usable */
}

/* assumes mapped_module_base's header page is writable */
static bool
aslr_write_header(app_pc mapped_module_base, size_t module_size,
                  app_pc new_preferred_base, uint new_checksum, uint new_timestamp)
{
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt_hdr;

    ASSERT(is_readable_pe_base(mapped_module_base));
    ASSERT_CURIOSITY(new_preferred_base != mapped_module_base &&
                     "usually relocated at original address");
    /* note that mapped_module_base is not necessarily the original
     * preferred image base for DLLs with poorly chosen base */

    dos = (IMAGE_DOS_HEADER *)mapped_module_base;
    nt_hdr = (IMAGE_NT_HEADERS *)(((byte *)dos) + dos->e_lfanew);

    /* from pecoff_v8.doc CheckSum The image file checksum. The
     * algorithm for computing the checksum is incorporated into
     * IMAGHELP.DLL. The following are checked for validation at load
     * time: all drivers, any DLL loaded at boot time, and any DLL
     * that is loaded into a critical Windows process.
     */

    LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1,
        "ASLR: aslr_write_header checksum old " PFX ", new " PFX "\n",
        nt_hdr->OptionalHeader.CheckSum, new_checksum);
    LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1,
        "ASLR: aslr_write_header ImageBase old " PFX ", new " PFX "\n",
        OPT_HDR(nt_hdr, ImageBase), mapped_module_base);
    LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1,
        "ASLR: aslr_write_header TimeDateStamp old " PFX ", new " PFX "\n",
        nt_hdr->FileHeader.TimeDateStamp, new_timestamp);

    /* note that the FileHeader.TimeDateStamp is different than
     * IMAGE_EXPORT_DIRECTORY.TimeDateStamp - yet IAT addresses should
     * better match the one used.  Since we have a new base and yet
     * the loader doesn't know that the file is relocated we have to
     * change the timestamp.
     */

    /* FIXME: may need to set Imagebase if the loader decides based on
     * a comparison with that whether to relocate, instead of using
     * the result code of NtMapViewOfSection */

    /* case 8507 discusses ramifications of modifying or
     * preserving the original value for each of these fields.
     */
    nt_hdr->OptionalHeader.CheckSum = new_checksum;
    nt_hdr->FileHeader.TimeDateStamp = new_timestamp;
    if (module_is_32bit(mapped_module_base)) {
        /* wow64 process - new base can't be > 32 bits even in 64-bit process
         * address space for 32-bit dlls.  */
        IF_X64(ASSERT(is_wow64_process(NT_CURRENT_PROCESS));)
        IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint((ptr_uint_t)new_preferred_base));)
        /* ImageBase for a 32-bit dll is 32-bits for 32-bit and 64-bit DR. */
        *(uint *)(OPT_HDR_P(nt_hdr, ImageBase)) = (uint)(ptr_uint_t)new_preferred_base;
    } else {
#ifdef X64
        /* ImageBase for a 64-bit dll is 64-bits. */
        *(uint64 *)(OPT_HDR_P(nt_hdr, ImageBase)) = (uint64)new_preferred_base;
#else
        ASSERT_NOT_REACHED();
#endif
    }

    return true;
}

/* returns true if successful, caller is responsible for unmapping the
 * mapped view if mapped_base is set.
 * If search_fitting_base then new_base is set to the new random base.
 *
 * Returned view is writable, but is not intended to be used for
 * execution.  Note that the section handle is irrelevant for COW (
 * i.e. always for SEC_IMAGE), we need to keep the section mapped if
 * the produced private contents is to be used.
 *
 * Note this function can be called by producers as well as consumers
 * to verify a mapping.
 */
static bool
aslr_generate_relocated_section(IN HANDLE unmodified_section,
                                IN OUT app_pc *new_base, /* presumably random */
                                bool search_fitting_base, OUT app_pc *mapped_base,
                                OUT size_t *mapped_size, OUT module_digest_t *file_digest)
{
    bool success;
    NTSTATUS res;
    HANDLE section_handle = unmodified_section;
    app_pc base = (app_pc)0x0;
    /* we won't necessarily use this mapping's base, no need to
     * require it to be at new_base.  If producer is going to be a
     * consumer it better choose a good new_base that will work. */
    /* Note that we could force the new base, but we may prefer to not
     * ask the kernel to do more than necessary (though all PE fields
     * should be RVAs that do not to be modified).  We may have wanted
     * to pass a mapping here earlier if we had read the image size to
     * support ASLR_RANGE_TOP_DOWN */

    size_t commit_size = 0;
    /* commit_size for an explicit anonymous mapping
     * will need to match section size */
    SIZE_T view_size = 0; /* full file view */
    uint type = 0;        /* commit not needed for original DLL */
    uint prot = PAGE_READWRITE;
    /* PAGE_READWRITE would allow us to update the backing section */
    /* PAGE_WRITECOPY - will only provide the current mapping */

    app_pc original_preferred_base;
    uint module_characteristics;

    ASSERT(*mapped_base == NULL);

    res = nt_raw_MapViewOfSection(section_handle,     /* 0 */
                                  NT_CURRENT_PROCESS, /* 1 */
                                  &base,              /* 2 */
                                  0,                  /* 3 */
                                  commit_size,        /* 4 */
                                  NULL,               /* 5 */
                                  &view_size,         /* 6 */
                                  ViewShare,          /* 7 */
                                  type,               /* 8 */
                                  prot);              /* 9 */
    ASSERT(NT_SUCCESS(res));
    if (!NT_SUCCESS(res)) {
        *mapped_base = NULL;
        return false;
    }
    /* side note: windbg receives a ModLoad: for our temporary mapping
     * at the NtMapViewOfSection(), no harm.  Note that the path name
     * the debugger uses is the value the loader sets in
     * PEB->SubSystemData = PathFileName.Buffer.  In case it is
     * setting that before NtMapViewOfSection(), but not in front of
     * NtCreateSection() then the DLL path the debugger may be
     * confused.  Though on XPSP2 it showed the correct current DLL
     * name.
     */

    *mapped_base = base;
    *mapped_size = view_size;

    /* Be aware of LdrVerifyImageMatchesChecksum() for our relocations
     * - but that maps in as SEC_COMMIT based on the original file, so
     * even if it is called for anything other than what is exported
     * in KnownDlls we'd be ok.  If we want to match that checksum we
     * can follow suit and process the file image, or we can emulate
     * that on a mapped image Section.
     *
     * FIXME: check what is the meaning of
     * "IMAGE_DLL_CHARACTERISTICS_FORCE_INTEGRITY 0x0080 Code
     * Integrity checks are enforced", documented in PECOFF v8.0
     */
    original_preferred_base = get_module_preferred_base(base);
    if (original_preferred_base == NULL) {
        ASSERT_CURIOSITY(false && "base at 0, bad PE?");
        /* maybe not a PE */
        ASSERT_NOT_TESTED();
        goto unmap_and_exit;
    }

    /* This is the earliest we can tell that an EXECUTABLE is being
     * mapped, e.g. for CreateProcess.  FIXME: case 8459 - unless we
     * need to apply any other binary transformations, here we return
     * failure so not to create a copy of the original executable.
     */
    /* check for PE already done by get_module_preferred_base() */
    module_characteristics = get_module_characteristics(base);
    if (TEST(IMAGE_FILE_RELOCS_STRIPPED, module_characteristics)) {
        LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1,
            "ASLR: aslr_generate_relocated_section skipping non-relocatable module\n");
        goto unmap_and_exit;
    }
    if (!TEST(IMAGE_FILE_DLL, module_characteristics)) {
        if (TEST(ASLR_RANDOMIZE_EXECUTABLE, DYNAMO_OPTION(aslr_cache))) {
            /* note that we have no problem randomizing an executable with
             * relocations, when we're in the parent
             */
            SYSLOG_INTERNAL_INFO("randomizing executable with .reloc");
        } else {
            /* FIXME: minor perf: every time we're starting an executable
             * we'd be wasting a lot of work until we get here,
             * see if any other mapping already exists
             */
            SYSLOG_INTERNAL_INFO("skipping executable, though it has .reloc");
            goto unmap_and_exit;
        }
    }

    /* check if we can deal with all sections in the module - .shared, non-readable */
    if (!module_file_relocatable(base)) {
        /* note that attackers don't have a chance to fool our checks
         * with a fake .shared section since we do match the PE
         * headers on verification.  But still minimal cost on
         * verification to keep this routine the same for both
         * producers and verifiers.
         */
        SYSLOG_INTERNAL_INFO("non relocatable DLL .shared section - can't replicate");
        goto unmap_and_exit;
    }

    /* .NET DLLs */
    if (TEST(ASLR_AVOID_NET20_NATIVE_IMAGES, DYNAMO_OPTION(aslr_cache)) &&
        module_has_cor20_header(base)) {
        /* FIXME:case 9164 once we have better capacity management
         * currently only fear of new temporary DLLs generated by ASP.NET */
        SYSLOG_INTERNAL_INFO_ONCE("not producing .NET 2.0 DLL - case 9164");
        goto unmap_and_exit;
    }

    if (search_fitting_base) {
        /* expected to be called by producer only */
        *new_base = aslr_get_fitting_base(*new_base, view_size);
        if (*new_base == NULL) {
            SYSLOG_INTERNAL_INFO_ONCE("no good fit, don't bother producing");
            goto unmap_and_exit;
        }
    }

    /* optional: could check here for valid image checksum, or any
     * future restrictions on original file contents which may need to
     * be preserved */
    if (file_digest != NULL) {
        module_calculate_digest(
            file_digest, base, view_size, true, true, /* both short and full */
            DYNAMO_OPTION(aslr_short_digest), UINT_MAX /*all secs*/, 0 /*all secs*/);
    }

    success = module_rebase(base, view_size, *new_base - original_preferred_base,
                            false /*batch +w*/);

    /* need to perform all actions usually taken by rebase.exe note
     * rebase modifies in the header the timestamp, imagebase and
     * checksum
     */

    /* FIXME: case 8507: test bound DLLs - a too clean relocation may
     * fool other DLLs bound to this one that they have the correct
     * prebound IAT entries.
     *
     * The loader checks only timestamp and whether a DLL was
     * relocated.  We need to tell the loader ImageBase matches
     * mapping so it does not break our sharing, but then bounding
     * will be fooled too.  FIXME: find if we can control separately,
     * otherwise we'll have to hack somehow one idea is to increment
     * the timestamp and have our hotpatching match [timestamp,
     * timestamp+1]
     */

    /* windbg will not be happy with our mapped images having
     * incorrect timestamp and checksum
     *
     * May be better solution is to set IMAGE_NOT_AT_BASE in the
     * MODULEITEM which hopefully will be used by the loader for any
     * binding requests.  We cannot return STATUS_IMAGE_NOT_AT_BASE
     * because then the loader will relocate and lose our sharing, and
     * for that reason we may need to set the MODULEITEM at some later
     * point.  Yet it is not obvious what would be a good time to
     * modify these loader structures.
     *
     * Alternative, is to disable BINDing in the
     * importers - they are anyways supposed to fail, so we can walk
     * through the list of IMAGE_IMPORT_DESCRIPTOR (old BIND?),
     * IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT (new BIND),
     * yet worst kind is the DelayLoad timestamp that may now be found to be bound.
     * pecoff.doc: 5.8.1. The Delay-Load Directory Table, Delay Import Descriptor
     * offset 28, size 4, Time Stamp, Time stamp of DLL to which this image has been
     * bound.
     * e.g. ImgDelayDescr.dwTimeStamp in Microsoft Visual Studio/VC98/Include/DELAYIMP.H
     * So it may require too many possibly custom delay import implementations.
     *
     * presumably we cannot use IMAGE_DLLCHARACTERISTICS_ NO_BIND
     * 0x0800 Do not bind the image (in pecoff v8.0), however should
     * check whether the binder or the loader checks this.
     *
     * FIXME: case 8508: As an optimization we could bind all of our cached DLLs
     * to our randomized version to recoup any losses.
     * see "Optimizing DLL Load Time Performance"by Matt Pietrek
     *   http://msdn.microsoft.com/msdnmag/issues/0500/hood/
     * to measure if that is at all worth it on current machines.
     *
     * "Even under the slowest scenario, [his P3 550MHz] machine still
     * loaded the program under Windows 2000 in less than 1/50th of a
     * second.  On Windows 2000, properly basing the DLLs improved the
     * load time by roughly 12 percent. Basing and binding the EXE and
     * the DLLs improved the load time by around 18 percent. "
     */
    /* Note we leave the
     * PE.Checksum invalid since it is the value of the
     * original file.  In case anyone matches the original
     * file they will be happy, or in case they validates
     * hopefully they will do against the disk image as
     * SEC_COMMIT/MEM_MAPPED which will be OK since we'll pass the original image.
     */

    if (success) {
        uint old_checksum;
        uint old_timestamp;
        uint new_timestamp;

        /* we could use the current time as a new timestamp, but using
         * old_timestamp + 1 will give us at least a way of finding
         * the module in case of limited diagnostic information */
        bool ok = get_module_info_pe(*mapped_base, &old_checksum, &old_timestamp, NULL,
                                     NULL, NULL);
        ASSERT(ok);

        /* imagine any other product like our one-off hotpatches would
         * get fooled by this non-transparency */
        new_timestamp = aslr_timestamp_transformation(old_timestamp);
        /* coordinate any changes here with aslr_compare_header() */

        aslr_write_header(*mapped_base, *mapped_size, *new_base, old_checksum,
                          new_timestamp);
        /* FIXME: we need to somehow preserve original_preferred_base
         * for detection, see aslr_get_original_metadata() for other considerations */
    }

    return success;
unmap_and_exit:
    /* we do not need the section mapping */
    res = nt_raw_UnmapViewOfSection(NT_CURRENT_PROCESS, *mapped_base);
    ASSERT(NT_SUCCESS(res));
    *mapped_base = NULL;
    return false;
}

#ifdef DEBUG
/* UNUSED */
/* keeping this scaffolding code to still compile to allow
 * experimentation with any new mapping and sharing features that may
 * have different attributes
 *
 * given a handle to original section with original file contents,
 * publishes in the appropriate section directory a randomized copy
 */
/* returns true if caller should expect to find a mapped relocated copy */
bool
aslr_experiment_with_section_handle(IN HANDLE file_handle,
                                    const wchar_t *mostly_unique_name)
{
    /* publish in shared or private view */
    HANDLE object_directory = shared_object_directory;
    HANDLE new_published_handle;
    NTSTATUS res;
    PSECURITY_DESCRIPTOR dacl = NULL;
    bool permanent = false;

    /* section<PAGE_EXECUTE, SEC_IMAGE, app_file> gives us CoW in each
     * process, and we can't share the relocation information
     */
    /* section<PAGE_EXECUTE_READWRITE, SEC_IMAGE, original app_file>
     * gives access denied since file is open only for execution.
     * Though even proper privileges do not overwrite the original
     * file - SEC_IMAGE is always copy on write.
     */

    /* only using SEC_COMMIT either with page file, or with a
     * {file<FILE_EXECUTE | FILE_READ_DATA | FILE_WRITE_DATA>,
     * section<PAGE_EXECUTE_READWRITE, SEC_COMMIT, file>,
     * map<PAGE_READWRITE>} allows writers to write to a true shared
     * memory with readers.  If a particular reader needs private
     * writes they can use map<PAGE_WRITECOPY> (can even track the
     * pages that have transitioned from PAGE_WRITECOPY into
     * PAGE_READWRITE to find which ones have been touched.
     *
     * Note if we could use SEC_COMMIT for mapping DLLs we'd always
     * need PAGE_WRITECOPY to allow hotp or other hookers to modify
     * privately.  We may also depend on CoW for a shared DR cache if
     * there similarly may be some rare private invalidations.
     */

    /* most likely places to experiment with flags are marked with CHANGEME */

    /* FIXME: doublecheck flags and privileges with what smss does */
    res = nt_create_section(&new_published_handle,
                            SECTION_ALL_ACCESS, /* FIXME: maybe less privileges needed */
                            NULL,               /* full file size */
                            PAGE_EXECUTE_READWRITE,
                            /* PAGE_EXECUTE_READWRITE - gives us true overwrite ability */
                            /* PAGE_EXECUTE gives us COW but not sharing */
                            /* PAGE_EXECUTE_WRITECOPY is still COW,
                             * though it needs FILE_READ_DATA
                             * privileges to at all create a section,
                             * CHANGEME */
                            SEC_COMMIT,  /* CHANGEME SEC_IMAGE or SEC_COMMIT (default) */
                            file_handle, /* CHANGEME */
                            /* NULL for page file backed */
                            /* file_handle for file backed */

                            /* object name attributes */
                            mostly_unique_name, (permanent ? OBJ_PERMANENT : 0),
                            object_directory, dacl);

    /* FIXME: is SEC_BASED supported - and what good does that do to
     * us?  For sure a convenient place to keep our current
     * BaseAddress, since SECTION_BASIC_INFORMATION.BaseAddress is
     * supposed to be valid only for SEC_BASED_UNSUPPORTED
     */
    if (NT_SUCCESS(res)) {
        /* FIXME: this is done for real in aslr_file_relocate_cow(),
         * FIXME: duplication here is left just for future experimentation
         * now comes the interesting part of rebasing the executable to a random new
         * address relocating and possibly updating all other fields that need to change
         */
        app_pc mapped_base;
        size_t mapped_size;
        app_pc new_base = (app_pc)(ptr_uint_t)0x12340000;
        bool relocated = aslr_generate_relocated_section(
            new_published_handle, &new_base, false, &mapped_base, &mapped_size, NULL);
        if (relocated) {
            /* for testing purposes we're just touching the checksum
             * to verify sharing and private pages */
            /* FIXME: note that we want unique value for testing only! */
            relocated = aslr_write_header(mapped_base, mapped_size, mapped_base,
                                          (uint)win32_pid, 1);
        }

        if (0 && relocated) { /* CHANGEME:  */
            /* finally verifying that the data doesn't stick around if not unmapped */
            res = nt_raw_UnmapViewOfSection(NT_CURRENT_PROCESS, mapped_base);
            ASSERT(NT_SUCCESS(res));
        }

        /* CHANGEME now for experiment we do it again to see whether
         * first write was a private COW or it is a still globally
         * visible shared write */
        /* unfortunately it is not - visible */
        new_base = (app_pc)(ptr_uint_t)0x23450000;
        aslr_generate_relocated_section(new_published_handle, &new_base, false,
                                        &mapped_base, &mapped_size, NULL);
        if (relocated) {
            /* for testing purposes just touching the checksum to verify
             * sharing and private pages */
            /* FIXME: note that we want unique value for testing only! */
            relocated = aslr_write_header(mapped_base, mapped_size, mapped_base,
                                          (uint)win32_pid, 2);
        }

        if (!relocated) {
            ASSERT_NOT_TESTED();
            close_handle(new_published_handle);
            return false;
        }

        if (permanent) {
            ASSERT_NOT_TESTED();
            close_handle(new_published_handle);
        }
        ASSERT_NOT_TESTED();
        return true;
    } else {
        /* FIXME: need to check for name collisions in a race, we
         * should still return true so that the caller tries to open
         * the created object
         */
        if (res == STATUS_OBJECT_NAME_COLLISION) {
            /* we don't need to create a section if it already exists,
             * STATUS_OBJECT_NAME_EXISTS presumably on OBJ_OPENIF? */
            ASSERT_CURIOSITY(false);
            /* we assume caller should now try to use this - ok for
             * SEC_IMAGE since published only in consistent views */
            return true;
        } else {
            /* any other error presumed to mean sharing is not possible */
            ASSERT_CURIOSITY(false);
            return false;
        }
    }
    ASSERT_NOT_REACHED();
    return false;
}
#endif /* DEBUG */

/* returns true if a section has been mapped locally as Copy on Write,
 * and has been relocated at a randomly chosen base.  Caller has to
 * unmap the returned private view on success.
 */
static bool
aslr_file_relocate_cow(IN HANDLE original_file_handle,
                       OUT app_pc *relocated_module_mapped_base,
                       OUT size_t *relocated_module_size,
                       OUT app_pc *random_preferred_module_base,
                       OUT module_digest_t *original_digest)
{
    NTSTATUS res;
    HANDLE relocated_section;
    res = nt_create_section(&relocated_section,
                            SECTION_ALL_ACCESS, /* FIXME: maybe less privileges needed */
                            NULL,               /* full file size */
                            PAGE_EXECUTE,
                            /* PAGE_EXECUTE gives us COW in readers
                             * but can't share any changes.
                             * Unmodified pages are always shared.
                             */
                            /* PAGE_EXECUTE_READWRITE - gives us true
                               overwrite ability only in SEC_COMMIT */
                            /* PAGE_EXECUTE_WRITECOPY is still COW,
                             * though it also needs FILE_READ_DATA
                             * privileges to at all create the section
                             * which the loader doesn't use */
                            SEC_IMAGE,
                            /* note we can't map a SEC_IMAGE as PAGE_READWRITE, also
                             * original_file_handle can't be pagefile - since we can't
                             * open such section as a SEC_IMAGE later.
                             */
                            original_file_handle,

                            /* process private - no security needed */
                            /* object name attributes */
                            NULL /* unnamed */, 0, NULL, NULL);
    ASSERT(NT_SUCCESS(res));
    if (NT_SUCCESS(res)) {
        /* now comes the interesting part of rebasing the executable
         * to a random new address relocating and possibly updating
         * all other fields that need to change
         */

        /* FIXME: have to pick a random address, yet such that we can
         * share across processes
         */
        /* FIXME: if we'll do top down we'll need to map first to
         * obtain the section size before we choose an address,
         * similarly if we read the preferred address
         */
        /* FIXME: currently using the same address generation as the
         * private mappings (a linearly growing random range for each
         * process.  TOFILE better strategy may be needed
         * e.g. an affine transformation that converts the usually
         * non-overlapping region of system DLLs into some other
         * region with a different base and possibly increased holes
         * between DLLs.  Could also use different copies in different
         * applications, due to conflicts, or if we simply don't want
         * to share between different users.
         */
        bool relocated;
        *random_preferred_module_base = aslr_get_next_base();
        *relocated_module_mapped_base = NULL;
        /* note that if the producer is not using this mapping of the
         * DLL, we don't care about it really being mapped where we
         * want it in other processes, so relocated_module_mapped_base
         * will not be the same as random_preferred_module_base.  Yet
         * we do want the random_preferred_module_base to fit at least
         * in the current producer's layout, so once we map the module
         * and know its size we may choose a different base. */
        relocated = aslr_generate_relocated_section(
            relocated_section, random_preferred_module_base,
            true, /* search to avoid conflict */
            relocated_module_mapped_base, relocated_module_size, original_digest);
        if (!relocated) {
            ASSERT(*relocated_module_mapped_base == NULL);
        }

        /* caller doesn't care about the section handle, but only
         * about the mapping base and size
         */
        close_handle(relocated_section);
        return relocated; /* caller will unmap the view */
    }
    return false;
}

/* Note: this routine cannot be used on original handles since we
 * don't have proper permissions for the application handle.  Leaving
 * the routine in case we find it useful for files that we have opened
 * ourselves and we want to detect too old files.
 *
 * Collect module source times to keep in our signature field.  Note
 * that we can't just copy to our own fields for exact match since
 * NTFS and FAT have different time granularites.
 *
 * Note our target file creation time is a good measure for a
 * cleanup tool to remove files that are too old 'expired'.  FIXME: If
 * we update our produced files in place then the modification time
 * will be the correct time to use.
 */
bool
aslr_module_get_times(HANDLE file_handle, uint64 *last_write_time)
{
    NTSTATUS res;

    FILE_BASIC_INFORMATION basic_info;

    /* FileBasicInformation A FILE_BASIC_INFORMATION structure. The
     * caller must have opened the file with the FILE_READ_ATTRIBUTES flag
     * specified in the DesiredAccess parameter.
     */

    /* Note we're missing FILE_READ_ATTRIBUTES when using original
     * application handles * and we don't ask for them in
     * aslr_recreate_known_dll_file() either,
     */

    /* It looks like files are
     * Type             File
     * Attributes       0
     * GrantedAccess    0x100020:
     *   Synch
     *   Execute/Traverse
     */

    HANDLE read_attrib_handle = file_handle;

    /* FIXME: we can't even use DuplicateHandle() SDK: For example, a
     * file handle created with the GENERIC_READ access right cannot
     * be duplicated so that it has both the GENERIC_READ and
     * GENERIC_WRITE access right.
     */
    ASSERT_CURIOSITY(TESTALL(FILE_READ_ATTRIBUTES | SYNCHRONIZE,
                             nt_get_handle_access_rights(read_attrib_handle)));

    /* FIXME: the only possibility left is to try to reopen the file
     * starting with full path, but we don't really have that.
     * For now giving up on this route for original files.
     */
    res = nt_query_file_info(read_attrib_handle, &basic_info, sizeof(basic_info),
                             FileBasicInformation);

    if (!NT_SUCCESS(res)) {
        ASSERT(false && "insufficient privilege or bad handle?");
        return false; /* can't read times */
    }

    /* the LastAccessTime and FileAttributes aren't useful to us The
     * other three times are interesting - most likely LastWriteTime
     * by itself is sufficient for normal use.  Still interesting
     * whether we should invalidate if any of the other fields are
     * notably ChangeTime which is not exposed through Win32 makes a
     * good candidate.
     *
     * DDK
     *   CreationTime
     *        Specifies the time that the file was created.
     *   LastWriteTime
     *        Specifies the time that the file was last written to.
     *   ChangeTime
     *        Specifies the last time the file was changed.
     *
     *  rumors and speculations that: this is the time the MFT entry is changed
     * FIXME: should test
     *
     * http://www.cygwin.com/ml/cygwin/2005-04/msg00492.html
     * " Windows NT supports a fourth timestamp which is inaccessible from the
     *   Win32 API.  The NTFS filesystem actually implements it.  It behaves
     *   as a ctime in a POSIX-like fashion.  Cygwin's st_ctime stat member now
     *   contains this ChangeTime, if it's available.
     * "
     *
     * "ctime attribute keeps track of when the content or meta information
     * about the file has changed - the owner, group, file permission,
     * etc. Ctime may also be used as an approximation of when a file was
     * deleted."
     */
    ASSERT_NOT_TESTED();
    *last_write_time = basic_info.LastWriteTime.QuadPart;
    return true;
}

/* given original application file and hashed name, returns a handle
 * to a relocated version of the file, caller should close_handle().
 *
 * otherwise returns INVALID_HANDLE_VALUE if not found
 */
/*
 * FIXME: alternative 1) add an item to a work queue - have a trusted
 * process (nodemgr) produce=copy+rebase the file next time and
 * (winlogon) can publish the section mappings and can even get rid of
 * the image itself.
 */

static bool
aslr_produce_randomized_file(IN HANDLE original_file_handle,
                             const wchar_t *mostly_unique_name, OUT HANDLE *produced_file)
{
    aslr_persistent_digest_t aslr_digest;
    /* FIXME: TOFILE: need to create a file from a properly secured
     * location to avoid privilege elevation!  FIXME: there is still a
     * hole in that in the current implementation we let go of our
     * exclusive write (0) access handle to convert from producer into
     * publisher which allows someone else to replace the file!
     */

    /* FIXME: alternatively: can create a new one on the fly with
     * FILE_ATTRIBUTE_TEMPORARY FILE_SHARE_READ and use these
     * attributes.  Yet (FILE_SHARE_READ)
     * may let publishers see an incomplete file, so we'll need to get
     * a safe checksum for synchronization.
     */

    /* FIXME: case 8458 about consulting with sharing and persistence
     * threshold heuristics to decide whether to produce a sharable
     * file.  FIXME: may also want to add a random jitter to the
     * persistence thresholds to smoothen the transition over multiple
     * runs - though it will make it hard to benchmark what we're
     * doing.
     */

    LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1,
        "ASLR: aslr_produce_randomized_file for %ls\n", mostly_unique_name);

    if (TEST(ASLR_SHARED_FILE_PRODUCER, DYNAMO_OPTION(aslr_cache))) {
        /* Note that SEC_IMAGE is always mapped as
         * PAGE_EXECUTE_WRITECOPY therefore one can't write back
         * relocations to a file mapped as image, rather only
         * SEC_COMMIT mappings allow writing back - and ImageRVAToVa
         * conversions are necessary when applying any relocations.
         */

        /* We have two options for producing a rebased copy:
         *
         * rebase+save: Rebase into created section, we relocate in
         * memory the original file (as SEC_IMAGE), we'll end up with
         * a COW copy, and then we'd have to write that to a file (as
         * SEC_COMMIT) (converting from raw memory to PE aligned
         * sections).  Not very efficient since results in two memory
         * copies.  Reusable module relocate and a PE dump, current
         * plan.  Any data that is not in a PE section is not going
         * the be mapped in memory by later SEC_IMAGE, so we can
         * ignore that.
         *
         * copy+rebase: Alternatively, we can copy the whole file to disk (as
         * SEC_COMMIT) and then relocate that one again as SEC_COMMIT
         * in the private copy (with no COW) - easy file copy, harder
         * file relocate.
         */
        bool ok;
        app_pc relocated_module_mapped_base; /* mapping in current process */
        size_t module_size;
        app_pc new_preferred_module_base; /* new random preferred base */

        uint64 randomized_file_size;
        uint64 original_file_size;
        uint64 requested_size;

        ok = os_get_file_size_by_handle(original_file_handle, &original_file_size);
        if (!ok)
            return false;

        requested_size = original_file_size + sizeof(aslr_persistent_digest_t);
        if (!aslr_check_low_disk_threshold(requested_size)) {
            /* Note there is no point in checking earlier whether
             * available disk space is already beyond the minimum. If
             * we preserve the original file size, we could save that
             * one syscall here.  [perf minor] We may want to memoize
             * the value and decide to never try again.
             */
            return false;
        }

        ok = aslr_file_relocate_cow(original_file_handle, &relocated_module_mapped_base,
                                    &module_size, &new_preferred_module_base,
                                    &aslr_digest.original_source);

        if (ok) {
            NTSTATUS res;
            bool persistent = TEST(ASLR_PERSISTENT, DYNAMO_OPTION(aslr_cache));

            /* note that SEC_IMAGE is larger than the real file size,
             * but could use module_size to be slightly more conservative */

            /* note we test whether we can create a file after we've
             * done a lot of work, but in fact as close as possible to
             * actually producing the file is good */
            ok = aslr_create_relocated_dll_file(produced_file, mostly_unique_name,
                                                original_file_size, persistent);

            if (ok) {
                /* FIXME: case 8459 now that we have a private copy of
                 * the file we can also apply any other binary
                 * rewriting here: hooking all exported functions, or
                 * rewriting the export table; applying hotpatches as
                 * coldpatches - e.g. if we have many functions to
                 * patch, or for GBOP, etc.
                 */

                ok = module_dump_pe_file(*produced_file, relocated_module_mapped_base,
                                         module_size);
            } else {
                *produced_file = INVALID_HANDLE_VALUE;
            }

            if (ok) {
                module_calculate_digest(&aslr_digest.relocated_target,
                                        relocated_module_mapped_base, module_size, true,
                                        true, /* both short and full */
                                        DYNAMO_OPTION(aslr_short_digest),
                                        UINT_MAX /*all secs*/, 0 /*all secs*/);
                /* other than crashing digest can't fail  */
            }

            /* we do not use the private section mapping any more */
            res = nt_raw_UnmapViewOfSection(NT_CURRENT_PROCESS,
                                            relocated_module_mapped_base);
            ASSERT(NT_SUCCESS(res));

            if (ok) {
                /* not all file contents are mapped in memory - see if
                 * there is more to preserve to appease aslr_verify_file_checksum() */
                /* FIXME: case 8496 tracks possibly removing that part */
                ok = aslr_module_force_size(original_file_handle, *produced_file,
                                            mostly_unique_name, &randomized_file_size);
                ASSERT(ok);
            }

            if (ok) {
                /* always produce and append signature in case clients
                 * need to use it, not necessary in lax or fully
                 * strict mode.  */
                /* Target MD5 also allows a cleanup tool to detect
                 * files that are corrupt 'rotten'.  Such a tool can't
                 * determine if a file is 'stale' (not matching its
                 * original) since we don't keep the original path
                 * only its hash is in the name.
                 */

                ok = aslr_module_append_signature(*produced_file, &randomized_file_size,
                                                  &aslr_digest);
                ASSERT(ok);
            }

            if (!ok) {
                if (*produced_file != INVALID_HANDLE_VALUE)
                    close_handle(*produced_file);

                return false;
            }

            /* at this point the file is still exclusive access to the
             * producer and our current handle disallows execute
             * access to make sure we don't map that as an exclusive
             * copy.  In case of a power failure the file will be
             * accessible to others so unless we are using
             * aslr_safe_save, we need to have publishers do complete
             * validation checks.
             */
            aslr_update_view_size(new_preferred_module_base, module_size);

            SYSLOG_INTERNAL_INFO("ASLR: produced DLL cache copy %ls", mostly_unique_name);

            return true;
        } else {
            /* release attempted region to randomize */
            aslr_update_failed(false /* no retry */, NULL, 0);
        }
    } else {
        /* Need to ask a producer to produce a relocated version of a
         * file at a given path, message should probably be keyed with
         * the mostly_unique_name to allow faster processing of
         * duplicate requests.
         */

        /* FIXME: note that we don't have a good way to obtain fully qualified name
         * the names from FileNameInformation don't contain the volume path!
         */

        /* FIXME: if we want to use the workqueue model we should find the
         * volume name from the file  */
        /* Note that we currently only have the unqualified with volume path */
        SYSLOG_INTERNAL_WARNING("ASLR: ask somebody to produce file %ls",
                                mostly_unique_name);

        /* FIXME: if we want synchronous LPC to nodemgr or winlogon we
         * need to write a secure server side. */
        /* It looks harder to ask somebody else to do the work than to do
         * it ourselves */

        /* see also aslr_process_worklist() about using the registry
         * as an asynchronous mailbox */
    }

    return false;
}

static void
aslr_get_unique_wide_name(const wchar_t *origname, const wchar_t *key,
                          wchar_t *newname /*OUT*/, uint newname_max /* max #wchars */)
{
    /* note this routine is a copy of get_unique_name but for
     * wchar_t should keep in synch any improvements
     */
    uint timestamp = (uint)get_random_offset(UINT_MAX);
    DEBUG_DECLARE(int trunc =) /* for DEBUG and INTERNAL */
    _snwprintf(newname, newname_max, L"%s-%d-%010u-%s", origname, get_process_id(),
               timestamp, key);

    ASSERT(trunc > 0 && trunc < (int)newname_max &&
           "aslr_get_unique_wide_name name truncated");
    /* FIXME: case 10677 */
    /* Truncation may result in incorrect use of the wrong file - we
     * should not truncate any strings at end, but should rather
     * truncate the app controlled name, and have a fixed format (zero
     * padded) for any known suffix that we are adding.
     */
    newname[newname_max - 1] = L'\0';
}

/* produced_temporary_file is closed regardless of success */
static bool
aslr_rename_temporary_file(const wchar_t *mostly_unique_name_target,
                           IN HANDLE produced_temporary_file,
                           const wchar_t *temporary_unique_name)
{
    HANDLE our_relocated_dlls_directory = get_relocated_dlls_filecache_directory(true);
    ASSERT(our_relocated_dlls_directory != INVALID_HANDLE_VALUE);

    ASSERT(DYNAMO_OPTION(aslr_safe_save));
    /* Note that we are providing an implicit guarantee that the file
     * that we are renaming is safe to load without complete file
     * validation:
     * 1) it corresponds to the correct application file
     * 2) we accept the risk of not allowing byte patching within files
     * 3) FIXME: case 10378 files are internally consistent - e.g. out of disk errors
     * 4) files are completely flushed on disk
     * 5) the file that we are renaming has been freshly produced
     */
    /* FIXME: if we do pass the original file handle while still open,
     * we know we are dealing with the same file.
     */
    close_handle(produced_temporary_file);

    /* FIXME: we may want to be able to rename the file while holding
     * this handle, otherwise we have to count on names being unique
     * enough, so we don't get our file overwritten before rename */

    /* to use os_rename_file() we'd have had to convert the full path
     * names and the per-user directory paths.
     */
    if (!os_rename_file_in_directory(our_relocated_dlls_directory, temporary_unique_name,
                                     mostly_unique_name_target,
                                     false /*do not replace*/)) {
        SYSLOG_INTERNAL_WARNING_ONCE("aslr_rename_temporary_file failed");
        return false;
    }

    return true;
}

/* given a handle to original file, publishes in the appropriate
 * section directory a section to a randomized copy, on success calls
 * to aslr_subscribe_section_handle() should be able to find one
 *
 * if anonymous then the created section is not published, just a
 * private section is created instead
 *
 * returns true if caller should expect to find a mapped relocated copy
 * on subscribe, (may return true if a published section already exists)
 *
 * if new_section_handle is not INVALID_HANDLE_VALUE caller may use
 * that section handle instead of subscribing
 */
static bool
aslr_publish_section_handle(IN HANDLE original_file_handle,
                            const wchar_t *mostly_unique_name, bool anonymous,
                            OUT HANDLE *new_section_handle)
{
    HANDLE object_directory =
        shared_object_directory; /* publish in shared or private view */
    HANDLE new_published_handle;
    NTSTATUS res;
    PSECURITY_DESCRIPTOR dacl = NULL;
    HANDLE randomized_file_handle = NULL;

    bool permanent = false;
    /* Whether published handle should be left after all consumers
     * unmap their views.
     * Note that in asynchronous
     * consumer/publisher it should always be persisted until reboot.
     *
     * Note this notion of 'permanence' is different from
     * ASLR_SHARED_INITIALIZE_NONPERMANENT which only controls the
     * lifetime of the directory itself.
     *
     * TOFILE: publisher may or may not create permanent
     * sections probably shouldn't so that we keep a matching original
     * application handle with the same lifetime as normal DLL
     * mappings, as well as a non-leaking publisher handle.  If we
     * leak within the process DLL churn will be handled a lot faster.
     * Persistence (across application restarts) will require full
     * validation of produced files on process churn.  Even more
     * importantly we _cannot_ create permanent section objects from
     * all applications, so our only option is to keep a handle open.
     */

    if (permanent && !anonymous) {
        permanent = false;
    }

    ASSERT(new_section_handle != NULL);
    *new_section_handle = INVALID_HANDLE_VALUE;

    if (object_directory == INVALID_HANDLE_VALUE && !anonymous) {
        /* FIXME: currently this could be evaluated in caller, yet
         * in the future may have multiple possible locations to try
         */
        return false;
    }

    /* Note that the two alternative implementations here are to share
     * a relocated Section produced from the original file, or a
     * Section produced from a randomized file produced by relocating
     * the original file.  Since SEC_IMAGE is always COW and can't
     * share writes, currently pursuing the second option - opening a
     * randomized file on disk.
     */

    /* No section was published, we are the first to publish */
    /* Note distinction between section publisher and file producer.
     * It is possible to consider a separate process creating the
     * files, in that case the publisher has to decide whether the
     * file is corrupt or stale - especially when we don't need
     * persistence.
     */

    /* FIXME: for perisistence: look for an already created randomized
     * file, for pure randomization without persistence should attempt
     * overwriting the previously generated file.  If in a race with
     * another publisher who has created the file but not exported the
     * section, then should make sure the overwrite will fail due to
     * exclusive write access.
     */
    if (!aslr_open_relocated_dll_file(&randomized_file_handle, original_file_handle,
                                      mostly_unique_name)) {
        HANDLE produced_file_handle;
        const wchar_t *randomized_file_name;
        wchar_t temporary_more_unique_name[MAX_PUBLISHED_SECTION_NAME];

        /* should attempt to produce a new one if allowed to produce,
         * or request for one to be produced by a trusted producer
         */
        randomized_file_handle = NULL;
        if (DYNAMO_OPTION(aslr_safe_save)) {
            /* we first create the randomized file version in a
             * temporary file */
            aslr_get_unique_wide_name(mostly_unique_name, L"tmp",
                                      temporary_more_unique_name,
                                      BUFFER_SIZE_ELEMENTS(temporary_more_unique_name));
            randomized_file_name = temporary_more_unique_name;
        } else {
            randomized_file_name = mostly_unique_name;
        }

        if (aslr_produce_randomized_file(original_file_handle, randomized_file_name,
                                         &produced_file_handle)) {
            /* TOFILE: note that currently we cannot cleanly hand-off
             * from producer handle to allow for a non-persistent file
             * handle, see discussion in
             * aslr_produce_randomized_file() about possible
             * alternatives.  FIXME: We should be closing the
             * exclusive write producer handle only after we open it
             * as shared read/no write as a publisher.  Currently
             * produced_file_handle has exclusive read hence we cannot
             * publish.
             */
            /* case 9696 for DYNAMO_OPTION(aslr_safe_save) should
             * guarantee file is written correctly before we rename */
            if (DYNAMO_OPTION(aslr_safe_save)) {
                os_flush(produced_file_handle);
                /* temporary file version is self-consistent */
                /* consumers don't need full validation */
                if (!aslr_rename_temporary_file(mostly_unique_name, produced_file_handle,
                                                temporary_more_unique_name)) {
                    ASSERT_CURIOSITY(false && "couldn't rename just produced temp file!");
                    randomized_file_handle = NULL;
                }
                /* produced_file_handle is closed regardless of success */
                LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1,
                    "ASLR: aslr_publish_section_handle: renamed %ls to %ls\n",
                    temporary_more_unique_name, mostly_unique_name);

            } else {
                /* file is created with well-known name,
                 * consumers must validate carefully
                 */
                close_handle(produced_file_handle);
            }

            if (aslr_open_relocated_dll_file(&randomized_file_handle,
                                             original_file_handle, mostly_unique_name)) {
            } else {
                ASSERT_CURIOSITY(false && "couldn't open just produced file!");
                randomized_file_handle = NULL;
            }
        }
    } else {
        LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1,
            "ASLR: aslr_publish_section_handle existing copy of %ls\n",
            mostly_unique_name);
        /* more visibility only when logging */
        DOLOG(1, LOG_VMAREAS, {
            SYSLOG_INTERNAL_INFO("ASLR: using existing DLL cache copy %ls",
                                 mostly_unique_name);
        });
    }

    if (TEST(ASLR_INTERNAL_SHARED_APPFILE, INTERNAL_OPTION(aslr_internal))) {
        ASSERT_CURIOSITY(randomized_file_handle == NULL);
        /* stress testing: temporarily testing application file
         * sections instead of our own files, provides original file,
         * so nothing is really randomized, just testing the other
         * basic components
         */
        ASSERT_NOT_TESTED();
        duplicate_handle(NT_CURRENT_PROCESS, original_file_handle, NT_CURRENT_PROCESS,
                         &randomized_file_handle, 0, 0,
                         DUPLICATE_SAME_ACCESS | DUPLICATE_SAME_ATTRIBUTES);
    }

    if (randomized_file_handle == NULL) {
        return false;
    }

    /* FIXME: doublecheck flags and privileges with what smss does for
     * exporting KnownDlls */
    res = nt_create_section(
        &new_published_handle,
        /* even as publisher we don't need any of
         * SECTION_ALL_ACCESS rights after
         * creating the object */
        SECTION_QUERY | /* optional */
            SECTION_MAP_WRITE | SECTION_MAP_READ | SECTION_MAP_EXECUTE,
        NULL, /* full file size */
        PAGE_EXECUTE,
        /* PAGE_EXECUTE
         * gives us COW in readers but not sharing */
        /* PAGE_EXECUTE_READWRITE
         * - gives us true overwrite ability only in SEC_COMMIT*/
        /* PAGE_EXECUTE_WRITECOPY is still COW,
         * though it also needs FILE_READ_DATA
         * privileges to at all create the section
         * which loader doesn't use */
        SEC_IMAGE, /* PE file mapping */
        randomized_file_handle,
        /* NULL for page file backed */
        /* object name attributes */
        /* if anonymous sections is not named after all */

        anonymous ? NULL : mostly_unique_name, (permanent ? OBJ_PERMANENT : 0),
        anonymous ? NULL : object_directory, anonymous ? NULL : dacl);
    /* we can close the file handle whether check section was created or not */
    close_handle(randomized_file_handle);

    /* FIXME: is SEC_BASED supported - and what good does that do to
     * us?  Only potential value is
     * SECTION_BASIC_INFORMATION.BaseAddress as a convenient place to
     * keep our current BaseAddress, since is supposed to be valid
     * only for SEC_BASED_UNSUPPORTED
     */
    if (NT_SUCCESS(res)) {
        /* FIXME: TOFILE if not permanent may want to keep this section
         * handle around - memory and handle leaked until process
         * dies.  Permanent is even worse - leaked until reboot.
         */

        /* Note that a single process reloading a DLL should now find
         * it all the time, so we'd leak a handle only for each unique
         * DLL.  However, a lot more problematic is the commit memory
         * leak if a process is producing temporary DLLs (like
         * ASP.NET) that are later not needed.
         *
         * Maybe the right thing to do is to close the handle right
         * after the publisher opens it as a subscriber as well.  That
         * way a section will exist - though I am not sure whether it
         * will be visible - if there is at least one user at any
         * time, and will disappear if not needed.  That's all that's
         * needed for true sharing.  Otherwise need STICKY bit to mark
         * DLLs that end up being reused.
         */
        if (permanent) {
            ASSERT_NOT_TESTED();
            close_handle(new_published_handle);
            *new_section_handle = INVALID_HANDLE_VALUE;
        } else {
            *new_section_handle = new_published_handle;
            /* leaked, or up to caller */
        }
        return true;
    } else {
        if (res == STATUS_OBJECT_NAME_COLLISION) {
            /* FIXME: need to check for name collisions in a race, we
             * should still return true so that the caller tries to open
             * the created object
             */
            /* we don't need to create a section if it already exists,
             * STATUS_OBJECT_NAME_EXISTS is a Warning presumably on
             * OBJ_OPENIF? */

            /* shouldn't happen if callers always first attempt to
             * subscribe, multiple workqueue publishers may hit this
             * as well */
            ASSERT_CURIOSITY(false && "already published");
            /* we assume caller should now try to use this - ok for
             * SEC_IMAGE since published only in consistent views */
            *new_section_handle = INVALID_HANDLE_VALUE;
            return true; /* we don't give out any new handles */
        } else {
            /* any other error presumed to mean sharing is not possible */
            /* e.g. insufficient permissions STATUS_ACCESS_DENIED,*/
            if (res == STATUS_INVALID_FILE_FOR_SECTION) {
                /* an invalid PE file is used - e.g. created by us, or
                 * maybe truncated due to power loss */
                /* FIXME: if persistent we should request producer to redo,
                 * otherwise someone should have caught this as stale.
                 * Producer while writing should be exclusive.
                 */
                ASSERT_CURIOSITY(false && "bad PE file");
            } else if (res == STATUS_ACCESS_DENIED) {
                ASSERT_CURIOSITY(false && "insufficient privileges");
            } else
                ASSERT_CURIOSITY(false && "unexpected failure on nt_create_section");
            return false;
        }
    }
    ASSERT_NOT_REACHED();
    return false;
}

/* FIXME: for persistent file generation, we may want to observe a
 * code generation rule that says that if current user has rights to
 * modify the original DLL that is getting loaded, then it's not a
 * problem to let them create a new copy (that may get overwritten).
 * For transient files however publisher(==producer) only creates a
 * section of the produced file and never looks at the file again, so
 * there is no attack vector.
 */

/* preserve state about not having to aslr privately this section */
static void
aslr_set_randomized_handle(dcontext_t *dcontext, HANDLE relocated_section_handle,
                           app_pc original_preferred_base, uint original_checksum,
                           uint original_timestamp)
{
    /* FIXME: at this point we should keep track of this handle
     * and if it is indeed truly randomized then we can skip
     * randomizing it again in our aslr_pre_process_mapview()
     * handling, should also keep original base and maybe
     * timestamp/checksum.
     */
    /* it appears that windbg is capable of finding the correct
     * symbols for a rebased DLL just fine.
     * FIXME: case 8439 tracks verifying how exactly it works.
     *
     * The only user known to be unhappy about a different timestamp
     * is hotpatching.
     *
     * FIXME: case 8507 should make sure that all possible bindings
     * really care about timestamp+1 (negative test - easier to see
     * which ones crash on leaving timestamp).  Regular statically
     * loaded and bound DLLs do crash.  Delay loaded and bound DLLs
     * are also expected to crash.
     */

    /* FIXME: case 1272: now can also add to module list
     * (short filename: already added in syscall routines)
     * full path (except for volume name)
     * original base
     * original timestamp
     * original checksum
     *
     * Slightly complicated to track until the section is mapped,
     * allocating memory here for the structure may sometimes leak -
     * so maybe not worth dealing with the full path (and more than
     * some limited short filename).  Can keep a file handle per
     * thread assuming single threaded, yet should defer allocating
     * any memory for these until a NtMapViewOfSection() - on success
     * add to module list, on failure can free.  App's file handle may get
     * closed after NtCreateSection() so we can't really use it to
     * reread the names.  In general other than for matching, we
     * should use duplicate_handle(), but then we open a handle leak
     * problem if we never see a corresponding event to
     * close_handle().  We will have to
     * close_handle(previous_file_handle) if it is not closed by the
     * next time we get here, or similarly could free any memory that
     * we may have allocated, so will always have at most one
     * outstanding.
     *
     * FIXME: should we keep the original application handle just in
     * case it is needed to handle failure of NtMapViewOfSection(), we
     * can't duplicate?
     */
    dcontext->aslr_context.randomized_section_handle = relocated_section_handle;
    dcontext->aslr_context.original_section_base = original_preferred_base;
    dcontext->aslr_context.original_section_checksum = original_checksum;
    dcontext->aslr_context.original_section_timestamp = original_timestamp;

    if (TEST(ASLR_INTERNAL_SHARED_AND_PRIVATE, INTERNAL_OPTION(aslr_internal))) {
        dcontext->aslr_context.randomized_section_handle = INVALID_HANDLE_VALUE;
    }
}

static bool
aslr_get_original_metadata(HANDLE original_app_section_handle,
                           OUT app_pc *original_preferred_base,
                           OUT uint *original_checksum, OUT uint *original_timestamp)
{
    /* currently mapping the original section and read all the values
     * that we need */
    /* FIXME: If consumers don't need to map this file for any other
     * reason it may be faster to have publishers publish this data
     * with the relocated section.  Could possibly be kept as metadata
     * by the producers, though it is easy enough to regenerate by
     * publishers.
     */

    /* side note: currently producers need to map in the original
     * file, publishers need to do so to calculate any hashes, and
     * consumers may need to do this in some faster way to verify that
     * exported sections are still valid, so anyone can produce these
     * if we allow for a mapping to exist.
     */
    bool ok;
    NTSTATUS res;
    app_pc base = (app_pc)0x0;
    size_t commit_size = 0;

    SIZE_T view_size = 0; /* full file view */
    /* FIXME: we really only need the header, if that makes things
     * faster otherwise system cache will get up to a 256KB view
     */

    uint type = 0; /* commit not needed for original DLL */
    uint prot = PAGE_READONLY;

    res = nt_raw_MapViewOfSection(original_app_section_handle, /* 0 */
                                  NT_CURRENT_PROCESS,          /* 1 */
                                  &base,                       /* 2 */
                                  0,                           /* 3 */
                                  commit_size,                 /* 4 */
                                  NULL,                        /* 5 */
                                  &view_size,                  /* 6 */
                                  ViewShare,                   /* 7 */
                                  type,                        /* 8 */
                                  prot);                       /* 9 */
    ASSERT(NT_SUCCESS(res));
    if (!NT_SUCCESS(res))
        return false;
    /* side note: windbg receives a ModLoad: for our temporary mapping
     * at the NtMapViewOfSection(), no harm */

    *original_preferred_base = get_module_preferred_base(base);
    ASSERT(*original_preferred_base != NULL);

    ok =
        get_module_info_pe(base, original_checksum, original_timestamp, NULL, NULL, NULL);
    ASSERT(ok);

    res = nt_raw_UnmapViewOfSection(NT_CURRENT_PROCESS, base);
    ASSERT(NT_SUCCESS(res));

    return ok;
}

/* FIXME: TOFILE we may need to export the original file handle to preserve
 * no-clobber transparency
 * FIXME: verification is not yet implemented, but
 * good enoguh for a demo and casual use
 */
static bool
aslr_verify_section_backing(IN HANDLE original_app_section_handle,
                            IN HANDLE new_relocated_handle)
{
    /* FIXME: on Win2k+ we can count on
     * are_mapped_files_the_same(addr1, addr2) if we have made an
     * incorrect guess of the DLL name. Since our published sections
     * can't be based on the original file, we can use
     * are_mapped_files_the_same() only if we keep a section mapped to
     * the original file.  We'd just need to Map that section to
     * verify the file is the same (e.g. it hasn't been renamed).  A
     * modification to a file on the other hand should be prevented by
     * keeping a handle to the existing one.
     */

    SYSLOG_INTERNAL_WARNING_ONCE("ASLR consumer relying on name hash only");

    ASSERT_NOT_IMPLEMENTED(true);
    return true;
}

static bool
aslr_replace_section_handle(IN HANDLE original_app_section_handle,
                            IN HANDLE new_relocated_handle)

{
    dcontext_t *dcontext = get_thread_private_dcontext();

    bool ok;
    app_pc original_preferred_base;
    uint original_checksum;
    uint original_timestamp;
    /* get metadata about original section that is lost in the rebased section */
    ok = aslr_get_original_metadata(original_app_section_handle, &original_preferred_base,
                                    &original_checksum, &original_timestamp);
    if (!ok) {
        ASSERT_NOT_TESTED();
        ASSERT(false && "can't read metadata");
        close_handle(new_relocated_handle);
        return false;
    }

    aslr_set_randomized_handle(dcontext, new_relocated_handle, original_preferred_base,
                               original_checksum, original_timestamp);

    /* We need to preserve original handle to maintain
     * transparent behavior with regards to attempts to
     * modify a DLL while in use.
     * Note that each consumer will keep a handle and only when all
     * are done with it the file would be replacable.
     */
    if (!TEST(ASLR_ALLOW_ORIGINAL_CLOBBER, DYNAMO_OPTION(aslr_cache))) {
        /* Since we'll keep the app handle after mangling
         * NtCreateSection/NtOpenSection we'll have to close the old
         * handle the next time we're at NtCreateSection/NtOpenSection.
         * At most one per-thread may be missing - and there
         * is a mess tracking these across threads!  e.g. if a
         * MapViewOfSection is done in another thread we'll not
         * maintain our handle.  That's not the way the loader
         * currently does things, so this fragile solution should hold
         * up.
         */
        if (dcontext->aslr_context.original_image_section_handle !=
            INVALID_HANDLE_VALUE) {
            /* FIXME: we don't follow NtCreateProcess, for a
             * known such incorrect leak in parent instead of child
             * if we randomize EXEs case 8902
             */
            ASSERT_CURIOSITY("unexpected unused handle");
            ASSERT(dcontext->aslr_context.original_image_section_handle !=
                   original_app_section_handle);
            close_handle(dcontext->aslr_context.original_image_section_handle);
        }

        dcontext->aslr_context.original_image_section_handle =
            original_app_section_handle;
        /* note that the app has never seen this handle,
         * ignoring the miniscule race for another thread
         * watching the OUT argument that this system call
         * returns.  Very unlikely a supported app would use
         * undefined values, and we already make enough
         * assumptions about single threaded sequences.
         */
    } else {
        /* we don't need to preserve anything */
        ASSERT(dcontext->aslr_context.original_image_section_handle ==
               INVALID_HANDLE_VALUE);
        dcontext->aslr_context.original_image_section_handle = INVALID_HANDLE_VALUE;
    }

    return true;
}

/* given a handle to original file and hashed name, subscribes to a
 * copy of the appropriate section directory a section to a presumably
 * randomized copy.  After verifying it is really for the same file.
 * Returns true if a shared relocated section is found.
 */
static bool
aslr_subscribe_section_handle(IN HANDLE original_app_section_handle,
                              IN HANDLE file_handle, const wchar_t *mostly_unique_name,
                              OUT HANDLE *new_relocated_handle)
{
    HANDLE object_directory =
        shared_object_directory; /* publish in shared or private view */
    NTSTATUS res;

    if (object_directory == INVALID_HANDLE_VALUE) {
        /* FIXME: currently this could be evaluated in caller, yet
         * in the future may have multiple possible locations to try
         */
        return false;
    }

    /* open our candidate section based on expected name */

    /* note on XP SP2 that when the loader creates a section it typically has
     * the following access flags and attributes
     * 0:000> !handle 750 f
     * Handle 750
     *   Type           Section
     *   Attributes     0
     *   GrantedAccess  0xf:
     *          None
     *          Query,MapWrite,MapRead,MapExecute
     *   Name           <none>
     *   Object Specific Information
     *     Section base address 0
     *     Section attributes 0x1800000  SEC_IMAGE | SEC_FILE
     *     Section max size 0x4b000
     */
    /* note that loader doesn't use SECTION_QUERY on open section
     * (KnownDlls) but we may want to be able to query, instead of
     * adding only SECTION_MAP_READ | SECTION_MAP_WRITE |
     * SECTION_MAP_EXECUTE,
     * also we don't need STANDARD_RIGHTS_REQUIRED either
     */
    /* FIXME: on XP SP2+ there is a new flag
     * SECTION_MAP_EXECUTE_EXPLICIT that the loader doesn't seem to
     * use.  Not clear what it does, we should experiment with it, but
     * easier not to depend on it.
     */
    res = nt_open_section(new_relocated_handle,
                          SECTION_QUERY | /* optional */
                              SECTION_MAP_WRITE | SECTION_MAP_READ | SECTION_MAP_EXECUTE,
                          mostly_unique_name, 0, object_directory);
    if (NT_SUCCESS(res)) {
        /* FIXME: now should for sure check whether this new mapping is
         * related to the original file.
         */

        if (!aslr_verify_section_backing(original_app_section_handle,
                                         *new_relocated_handle)) {
            ASSERT(false && "stale published section");
            ASSERT_NOT_TESTED();
            close_handle(*new_relocated_handle);
            return false;
        }

        /* FIXME: should make sure that sizes are for sure the same,
         * can't do that in case of query access, for now debug only check
         */
        DODEBUG({
            uint new_section_attributes = 0;
            uint original_section_attributes = 0;
            LARGE_INTEGER new_section_size;
            LARGE_INTEGER original_section_size;
            bool new_attrib_ok = get_section_attributes(
                *new_relocated_handle, &new_section_attributes, &new_section_size);
            bool original_attrib_ok = get_section_attributes(original_app_section_handle,
                                                             &original_section_attributes,
                                                             &original_section_size);
            /* if we don't have Query access (e.g. for KnownDlls) we
             * can't even tell what else we have or don't have */
            if (!original_attrib_ok) {
                SYSLOG_INTERNAL_WARNING_ONCE("ASLR sharing on KnownDll %ls",
                                             mostly_unique_name);
            }

            ASSERT(new_section_attributes == original_section_attributes ||
                   !original_attrib_ok);
            ASSERT(new_section_size.QuadPart == original_section_size.QuadPart ||
                   !original_attrib_ok);

            SYSLOG_INTERNAL_INFO("ASLR: consumer: using section cache %ls",
                                 mostly_unique_name);
        });

        return aslr_replace_section_handle(original_app_section_handle,
                                           *new_relocated_handle);
    } else {
        if (res == STATUS_OBJECT_NAME_NOT_FOUND) {
            return false;
        } else {
            ASSERT_CURIOSITY(false && "nt_open_section failure");
            return false;
        }
    }
    ASSERT_NOT_REACHED();
    return false;
}

/* returns true and sets new_relocated_handle if a randomized section
 * should be used instead of the original handle returned by the OS
 */
static bool
aslr_post_process_create_section_internal(IN HANDLE old_app_section_handle,
                                          IN HANDLE file_handle,
                                          OUT HANDLE *new_relocated_handle)
{
    wchar_t mostly_unique_name[MAX_PUBLISHED_SECTION_NAME];
    bool ok;
    HANDLE new_published_handle;

    ASSERT(TEST(ASLR_SHARED_CONTENTS, DYNAMO_OPTION(aslr_cache)));
    ASSERT(TESTANY(ASLR_SHARED_PUBLISHER | ASLR_SHARED_SUBSCRIBER |
                       ASLR_SHARED_ANONYMOUS_CONSUMER,
                   DYNAMO_OPTION(aslr_cache)));

    /* Obtain our unique name -
     * based on file name and path hash
     */
    ok = calculate_publish_name(mostly_unique_name,
                                BUFFER_SIZE_ELEMENTS(mostly_unique_name), file_handle,
                                old_app_section_handle);

    /* FIXME: may need to append suffixes L".new" or L".orig" if
     * necessary to publish both relocated and original sections in the namespace
     * in aslr_verify_section_backing()
     */
    if (!ok) {
        LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1, "ASLR: shared: exempted DLL\n");
        return false;
    }

    /* FIXME: we may want to create another section based on the
     * original file so that we maintain file consistency checks -
     * e.g. one can't replace the original file with a new copy and
     * when we close this handle we may want the exported sections to
     * disappear as well.  Both publishers and subscribers may need to
     * do that to keep reference counters matching.
     */

    /* if we are a subscriber, try opening a published section, and
     * verify whether it is from correct DLL
     */
    if (TEST(ASLR_SHARED_SUBSCRIBER, DYNAMO_OPTION(aslr_cache)) &&
        aslr_subscribe_section_handle(old_app_section_handle, file_handle,
                                      mostly_unique_name, new_relocated_handle)) {
        return true;
    }

    /* if we are a publisher, publish relocated section */
    if (TESTANY(ASLR_SHARED_PUBLISHER | ASLR_SHARED_ANONYMOUS_CONSUMER,
                DYNAMO_OPTION(aslr_cache)) &&
        aslr_publish_section_handle(
            file_handle, mostly_unique_name,
            TEST(ASLR_SHARED_ANONYMOUS_CONSUMER, DYNAMO_OPTION(aslr_cache)),
            &new_published_handle)) {
        /* anonymous publisher==subscriber */
        if (TEST(ASLR_SHARED_ANONYMOUS_CONSUMER, DYNAMO_OPTION(aslr_cache))) {
            /* reuses the private section handle, just needs to
             * register the metadata */
            ASSERT(new_published_handle != INVALID_HANDLE_VALUE);
            *new_relocated_handle = new_published_handle;
            /* note handle may be closed on error,
             * otherwise we'll just return the private handle
             */
            return aslr_replace_section_handle(old_app_section_handle,
                                               *new_relocated_handle);
        }

        /* see discussion in aslr_publish_section_handle() */
        /* note we will leak the new_published_handle which will make
         * hanlding of DLL churn within single process faster,
         * FIXME: we still have to do more work as a subscriber to
         * make sure it is consistent */

        /* We don't need to always consume what we have
         * produced - producer may want to produce only for
         * others' consumption but refrain from using these...
         */
        if (TEST(ASLR_SHARED_SUBSCRIBER, DYNAMO_OPTION(aslr_cache))) {
            /* we now reopen the object as a regular subscriber, so
             * that we don't pass to the application a handle that may
             * have higher privileges than necessary.  FIXME: this
             * shouldn't be the case for file based handles, so could
             * just return a handle from aslr_publish_section_handle()
             */
            if (aslr_subscribe_section_handle(old_app_section_handle, file_handle,
                                              mostly_unique_name, new_relocated_handle)) {
                return true;
            } else {
                ASSERT_CURIOSITY(false && "publisher can't subscribe?");
            }
        } else {
            /* just publishing, then we don't get sharing in this process */
            /* FIXME: xref case 8458 may want to count number of
             * simultaneous process mappings for deciding what to
             * share
             */
            /* note we will leak the new_published_handle */
            ASSERT_CURIOSITY(new_published_handle != INVALID_HANDLE_VALUE);
            return false;
        }
    }

    return false;
}

/* aslr_publish_file() may be used for proactive loading of a list of
 *  files to relocate (similar to KnownDlls) the main problem with
 *  this scheme is that we'd need the file path.  We could provide
 *  full paths, or expect all important files to be only in %system32%
 *  (which precludes IE or Office DLLs from being preprocessed).
 *  Alternatively a work queue (ASLR_SHARED_WORKLIST) to publish can
 *  be generated by previous runs.
 */
bool
aslr_publish_file(const wchar_t *module_name)
{
    HANDLE file;
    HANDLE published_section;
    bool ok;
    wchar_t mostly_unique_name[MAX_PUBLISHED_SECTION_NAME];
    NTSTATUS res;

    HANDLE preloaded_dlls_directory = NULL;
    /* FIXME: module_name should provide a full path */
    res = nt_create_module_file(&file, module_name, preloaded_dlls_directory,
                                FILE_EXECUTE | FILE_READ_DATA, FILE_ATTRIBUTE_NORMAL,
                                FILE_SHARE_READ, FILE_OPEN, 0);
    if (!NT_SUCCESS(res)) {
        return false;
    }
    ok = calculate_publish_name(mostly_unique_name,
                                BUFFER_SIZE_ELEMENTS(mostly_unique_name), file, NULL);
    if (!ok) {
        ASSERT_NOT_TESTED();
        return false;
    }
    aslr_publish_section_handle(file, mostly_unique_name, false, &published_section);
    /* may leak published_section */
    close_handle(file);
    return true;
}

/* get target of symbolic link as file path and keep a directory handle open */
static void
aslr_get_known_dll_path(wchar_t *known_dll_path_buffer, /* OUT */
                        uint max_length_characters)
{
    UNICODE_STRING link_target_name;
    /* initialize with \??\ */
    wchar_t link_target_name_prefixed[MAX_PATH] = GLOBAL_NT_PREFIX;
    NTSTATUS res;
    uint bytes_length;

    /* FIXME: for now we don't keep the link_target_name, but just
     * open a file directory.
     * FIXME: We may not need the hash for full path, if we later
     * query the file handles.
     */
    /* FIXME: we may not need to use the full path name string.
     * According to Windows Internals the symlinks should be usable
     * directly as the object manager should apply the substitions -
     * so \KnownDlls\KnownDllPath\kernel32.dll should open the right
     * file.
     */

    /* initialize using stack buffer using room after \??\ prefix */
    link_target_name.Length = 0;
    link_target_name.MaximumLength =
        (ushort)(sizeof(link_target_name_prefixed) -
                 wcslen(link_target_name_prefixed) * sizeof(wchar_t));
    link_target_name.Buffer =
        link_target_name_prefixed + wcslen(link_target_name_prefixed);
    bytes_length = link_target_name.MaximumLength;

    ASSERT(known_dlls_object_directory != NULL);

    res = nt_get_symlink_target(known_dlls_object_directory, KNOWN_DLL_PATH_SYMLINK,
                                &link_target_name, &bytes_length);
    ASSERT(NT_SUCCESS(res));
    /* sometimes the final null is not included */
    if (bytes_length == (uint)link_target_name.Length) {
        ASSERT(link_target_name.MaximumLength > link_target_name.Length);
        link_target_name.Buffer[bytes_length / sizeof(wchar_t)] = '\0';
    } else {
        ASSERT(bytes_length ==
               (uint)(link_target_name.Length + sizeof(wchar_t) /* final NULL */));
    }

    if (!NT_SUCCESS(res)) {
        ASSERT_NOT_TESTED();
        known_dll_path_buffer[0] = 0;
        return;
    }

    LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1,
        "ASLR: aslr_get_known_dll_path KnownDllPath = %ls\n", link_target_name.Buffer);

    wcsncpy(known_dll_path_buffer, link_target_name_prefixed, max_length_characters);
    known_dll_path_buffer[max_length_characters - 1] = 0;

    LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1, "ASLR: known_dll_path = %ls\n",
        known_dll_path_buffer);
}

/* given a handle, returns whether its name exactly matches \KnownDlls  */
bool
aslr_is_handle_KnownDlls(HANDLE directory_handle)
{
    /* We are always doing a slowish name comparison. */

    /* Expected that loader uses the same handle for the whole process
     * so we can cache the handle value it is using.  We can
     * still support multiple handles, but we'll fail in the unlikely
     * case that this one is closed and another reopened.
     * Only once we have found the loader handle we can quickly check
     * and discard the \BaseNamedObjects.  We could also traverse all
     * handles to find the value instead of waiting until a first use
     * of a KnownDll and set last_known_dlls_handle_is_valid.
     */

    /* ObjectBasicInformation NameInformationLength can tell us whether it is our length,
     * but we anyways want an exact match
     */

    /* ObjectNameInformation will give us complete name match
     * verify it is \KnownDlls
     */
    NTSTATUS res;
    OBJECT_NAME_INFORMATION name_info;
    uint bytes_length;
    uint returned_byte_length;

    name_info.ObjectName.Length = 0;
    name_info.ObjectName.MaximumLength = sizeof(name_info.object_name_buffer);
    name_info.object_name_buffer[0] = L'\0';
    name_info.ObjectName.Buffer = name_info.object_name_buffer;
    bytes_length = sizeof(name_info);

    STATS_INC(aslr_dlls_known_dlls_query);
    res = nt_get_object_name(directory_handle, &name_info, bytes_length,
                             &returned_byte_length);
    ASSERT(NT_SUCCESS(res));
    /* UNICODE_STRING doesn't guarantee NULL termination */
    NULL_TERMINATE_BUFFER(name_info.object_name_buffer);
    if (!NT_SUCCESS(res) || /* xref 9984 */ name_info.ObjectName.Buffer == NULL) {
        return false;
    }
    ASSERT_CURIOSITY(name_info.ObjectName.Buffer == name_info.object_name_buffer);

    if (wcscmp(name_info.ObjectName.Buffer, KNOWN_DLLS_OBJECT_DIRECTORY) == 0) {
        return true;
    } else {
        return false;
    }
}

bool
aslr_recreate_known_dll_file(OBJECT_ATTRIBUTES *obj_attr, OUT HANDLE *recreated_file)
{
    /* NOTE: we are making the assumption that all KnownDlls and their
     * dependents are all physically located in the KnownDllPath.
     * Presumably the loader makes the same assumption so hopefully
     * that is enforced somehow - e.g. smss.exe's DllPath may not
     * allow other paths to resolve dependencies etc.
     */

    /* we need the section name to allow us to open the
     * appropriate original file
     */

    /* To avoid risking reliance on having SeChangeNotifyPrivilege
     * we'll construct the absolute path name instead of using a saved
     * directory name, so that calculate_publish_name() can look it
     * up.  Note currently that the full path name is all that we use
     * as a hash, but we may need to use more file properties for hash
     * and for validation, so we are still creating a file.
     */

    wchar_t dll_full_file_name[MAX_PATH];
    NTSTATUS res;

    /* Name of handle is \KnownDlls\appHelp.dll but when NtOpenSection
     * is called with \KnownDlls directory root, the object name is
     * simply appHelp.dll */

    /* FIXME: we should use the obj_attr->ObjectName with careful
     * checks or case 1800 try/except.  Alternatively we can use a
     * single syscall to load it in our own buffer by calling
     * nt_get_object_name() just like the code in
     * aslr_is_handle_KnownDlls() and have some extra string
     * manipulation.  For now trusting the loader not to change the
     * object name in a race.
     */

    _snwprintf(dll_full_file_name, BUFFER_SIZE_ELEMENTS(dll_full_file_name), L"%s\\%s",
               known_dll_path, obj_attr->ObjectName->Buffer);
    NULL_TERMINATE_BUFFER(dll_full_file_name);

    LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1,
        "ASLR: aslr_recreate_known_dll_file = %ls\n", dll_full_file_name);

    res = nt_create_module_file(recreated_file, dll_full_file_name,
                                NULL /* absolute path name */,
                                FILE_EXECUTE | FILE_READ_DATA, FILE_ATTRIBUTE_NORMAL,
                                FILE_SHARE_READ, FILE_OPEN, 0);
    ASSERT(NT_SUCCESS(res));
    if (!NT_SUCCESS(res)) {
        return false;
    }

    /* Note that KnownDlls sections are cached and if the file has
     * been modified the proper semantics is to still use the original
     * file!  While the file may not be modifiable it may have been
     * superseded. */

    /* FIXME: case 8503 need to verify that the file that we have
     * opened is the same as the one in KnownDlls - e.g. it may have
     * been modified, in that case we cannot use the current file and
     * should give up on doing anything that would use the new file
     * contents.
     */
    /* FIXME: should use are_mapped_files_the_same() to validate that
     * a section created from the new file is the same as one from the
     * original section */
    /* FIXME: Alternatively could use some of the checks in
     * aslr_verify_file_checksum() file to verify that a section from
     * that matches the opened application section.  Especially for
     * producers this may be a good additional sanity check.
     */

    LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1,
        "ASLR sharing assuming KnownDll file %ls hasn't changed", dll_full_file_name);
    /* FIXME: since we currently don't really care about anything
     * other than the name itself, it is safe to ignore potential
     * change */

    return true;
}

/* common handler for NtCreateSection and NtOpenSection, note that
 *
 * NtOpenSection doesn't have a file_handle to the original file the
 * section was created from, and instead it passes a file handle to a
 * reopened expected matching original
 *
 * returns true if application section is replaced with our own
 */
bool
aslr_post_process_create_or_open_section(dcontext_t *dcontext, bool is_create,
                                         IN HANDLE file_handle /* OPTIONAL */,
                                         OUT HANDLE *sysarg_section_handle)
{
    /* reading handle is unsafe to race only, since syscall succeeded */
    HANDLE safe_section_handle;
    bool result = false;

    ASSERT(NT_SUCCESS(get_mcontext(dcontext)->xax));

    d_r_safe_read(sysarg_section_handle, sizeof(safe_section_handle),
                  &safe_section_handle);

    ASSERT(TEST(ASLR_DLL, DYNAMO_OPTION(aslr)));
    ASSERT(file_handle != NULL && file_handle != INVALID_HANDLE_VALUE);

    if (TEST(ASLR_SHARED_CONTENTS, DYNAMO_OPTION(aslr_cache)) &&
        TESTANY(ASLR_SHARED_PUBLISHER | ASLR_SHARED_SUBSCRIBER |
                    ASLR_SHARED_ANONYMOUS_CONSUMER,
                DYNAMO_OPTION(aslr_cache))) {
        HANDLE new_section = NULL;

        if (aslr_post_process_create_section_internal(safe_section_handle, file_handle,
                                                      &new_section)) {
            /* we'll replace original application handle with our
             * new mapping, so closing the original handle */
            /* FIXME: see note in aslr_post_process_mapview about handling failures
             * where we may need to preserve the original handle as well
             */
            if (TEST(ASLR_ALLOW_ORIGINAL_CLOBBER, DYNAMO_OPTION(aslr_cache))) {
                /* we can just close the handle to the original file */
                close_handle(safe_section_handle);
            } else {
                ASSERT(safe_section_handle ==
                       dcontext->aslr_context.original_image_section_handle);
            }

            /* safe_write() our new handle into argument location
             * address in case old application pointer is no
             * longer writable.
             */
            safe_write(sysarg_section_handle, sizeof(new_section), &new_section);
            result = true;
        } else {
            /* we leave original handle as is */
        }
    }
    return result;
}

static void
aslr_process_worklist(void)
{
    ASSERT_NOT_IMPLEMENTED(false);
    /* FIXME: case 8505 worklist - synchronous or asynchronous communication
     *
     * This should use either an option string that lists all files
     * that we'd want to publish, or better yet a global registry key
     * that others may be allowed to write to.  Since non-critical
     * loss due to races or malicious intent the latter may be good enough.
     *
     * FIXME: a more complicated worklist based on IPC with message
     * passing/writable shared memory will require a lot more work.
     *
     * For a portable worklist scheme we may use files.  Though on
     * Windows the registry may be lighter weight.  In fact, both the
     * registry and file system allow notification on additions with
     * either ZwNotifyChangeKey or ZwNotifyChangeDirectoryFile so
     * timely (though not synchronous) response is possible.  (Loop
     * every 60s and work if CPU is idle is probably also acceptable.)
     */
}

#ifdef GBOP
/* Generic Buffer Overflow Protection
 *
 *   Detection is not based on controlling the PC under DR, but on
 * running natively and expecting select hook locations to be targeted
 * by injected shellcode or manipulated activation records.
 *
 * o BOP can be bypassed by setting up the stack frame to look as
 *   whatever would pass any policies whether from user or kernel mode.
 *
 * o usermode BOP can simply be bypassed by directly going to the kernel
 *
 * o usermode BOP can also easily be bypassed if hooks go just after our hooks
 *
 * o Simplest bypass is to use a non-hooked routine, and for shallow
 *   hookers simply a level higher
 *
 * The last two make it very easy to bypass our usermode hooking BOP,
 * but add the extra inconvenience for attackers to lookup the windows
 * version (KUSER_SHARED_DATA or TEB->PEB) to find the correct syscall
 * numbers.  They could also traverse our data structures and execute
 * from our copy, yet to bypass all other BOPs as well, the first
 * attack is most likely.
 *
 * FIXME: we're mostly ignoring new attacks from having our
 * data structures writable.  Yet unlike MF here the threat is a lot
 * higher since attackers are running active shellcode and can look up
 * interesting data.
 */

/* FIXME: scramble this table so that an attacker can't search for it in
 *          memory and overwrite it with what they want.
 */

#    define GBOP_DEFINE_HOOK(module, symbol) { module, #    symbol },
#    define GBOP_DEFINE_HOOK_MODULE(module_name, set_name) \
        GBOP_DEFINE_##module_name##_##set_name##_HOOKS(#module_name ".dll")

/* For each point to be hooked for generic buffer overflow protection, add
 * an entry in gbop_hooks as a module name, function name pair.
 * Note: this array is only for those gbop hooks that are to be injected with
 * the hotp_only interface, not for piggy-backing gbop
 * Note: the names are case sensitive, so enter the correct name.
 */
static const gbop_hook_desc_t gbop_hooks[] = { GBOP_ALL_HOOKS };

#    undef GBOP_DEFINE_HOOK
#    undef GBOP_DEFINE_HOOK_MODULE

#    define GBOP_HOOK_LIST_END_SENTINEL ((uint)-1)

/* way too hacky linearization of a two dimensional array */
/* via templates: expands each set to a list of +1+1 = 2 which would
 * be the number of entries in that set */
#    define GBOP_DEFINE_HOOK(module, symbol) +1
#    define GBOP_DEFINE_HOOK_MODULE(module_name, set_name) \
        GBOP_DEFINE_##module_name##_##set_name##_HOOKS(unused),

/* size of array is determined by number of modules in GBOP_ALL_HOOKS,
 * end is demarcated by GBOP_HOOK_LIST_END_SENTINEL
 * Note that GBOP_SET_NTDLL_BASE is also included
 */
static const uint gbop_hooks_set_sizes[] = { 0, /* GBOP_SET_NTDLL_BASE */
                                             GBOP_ALL_HOOKS GBOP_HOOK_LIST_END_SENTINEL };

#    undef GBOP_DEFINE_HOOK
#    undef GBOP_DEFINE_HOOK_MODULE

/* size of array is determined by number of modules in GBOP_ALL_HOOKS,
 * end is demarkated by GBOP_HOOK_LIST_END_SENTINEL,
 * Note that GBOP_SET_NTDLL_BASE is also included
 */
/* each set expands into a ON entry, so all sets are enabled by default */
#    define GBOP_DEFINE_HOOK_MODULE(module_name, set_name) 1,

static int gbop_hooks_set_enabled[] = { 1, /* GBOP_SET_NTDLL_BASE */
                                        GBOP_ALL_HOOKS GBOP_HOOK_LIST_END_SENTINEL };
#    undef GBOP_DEFINE_HOOK_MODULE

static const uint gbop_num_hooks = BUFFER_SIZE_ELEMENTS(gbop_hooks);

/* FIXME: need better gbop for gdc; today a simple vm load would render our
 * gbop useless.  case 8087.
 */
/* This flag tracks if a VM has been loaded; used by gbop to identify the
 * presence of gdc.  Note: it doesn't track unloads or the actual execution of
 * dgc that lead to a gbop hook.
 */
bool gbop_vm_loaded = false;

/* converting from a condensed index (order after eliminating disabled
 * sets) into a real index into gbop_hooks[] */
const gbop_hook_desc_t *
gbop_get_hook(uint condensed_index)
{
    uint set_index = 0;
    uint real_index = 0;

    ASSERT(condensed_index < gbop_num_hooks);
    LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1, "gbop_get_hook: %d hook\n",
        condensed_index);

    /* skip earlier sets and expand from condensed index not including disabled sets */
    while (condensed_index >= gbop_hooks_set_sizes[set_index] ||
           !gbop_hooks_set_enabled[set_index]) {
        if (gbop_hooks_set_enabled[set_index]) {
            condensed_index -= gbop_hooks_set_sizes[set_index];
        } else
            ASSERT_NOT_TESTED();

        real_index += gbop_hooks_set_sizes[set_index];
        ASSERT(real_index < gbop_num_hooks);
        set_index++;
        ASSERT(set_index < gbop_num_hooks); /* at least less than all possible hooks */
    }
    real_index += condensed_index;
    ASSERT(real_index < gbop_num_hooks);
    LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1, "gbop_get_hook: => %d real, %s!%s\n",
        real_index, gbop_hooks[real_index].mod_name, gbop_hooks[real_index].func_name);

    return &gbop_hooks[real_index];
}

uint
gbop_get_num_hooks(void)
{
    static uint num_hooks = BUFFER_SIZE_ELEMENTS(gbop_hooks);
    static bool gbop_hooks_initialized = false;

    if (gbop_hooks_initialized)
        return num_hooks;

    /* we evaluate only before .data is protected, and we do not
     * support dynamically changing the hooks
     */
    ASSERT(!dynamo_initialized);

    if (DYNAMO_OPTION(gbop_include_set) != 0) {
        /* case 8246: the set of hooks is currently not dynamic should
         * unprotect .data around this if gbop_include_set is made dynamic
         */
        uint set_index = 0;
        size_t total_size = 0;
        while (gbop_hooks_set_sizes[set_index] != GBOP_HOOK_LIST_END_SENTINEL) {
            if (TEST((1 << set_index), DYNAMO_OPTION(gbop_include_set))) {
                gbop_hooks_set_enabled[set_index] = 1;
                LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1,
                    "gbop_get_num_hooks: 0x%x => %d enabled \n", (1 << set_index),
                    gbop_hooks_set_sizes[set_index]);
                total_size += gbop_hooks_set_sizes[set_index];
            } else {
                gbop_hooks_set_enabled[set_index] = 0;
                LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1,
                    "gbop_get_num_hooks: 0x%x => %d disabled \n", (1 << set_index),
                    gbop_hooks_set_sizes[set_index]);
            }
            set_index++;
        }
        ASSERT(total_size <= num_hooks);
        IF_X64(ASSERT_TRUNCATE(num_hooks, uint, total_size));
        num_hooks = (uint)total_size;
    }

    if (DYNAMO_OPTION(gbop_last_hook) != 0 && !dynamo_initialized) {
        num_hooks = MIN(num_hooks, DYNAMO_OPTION(gbop_last_hook));
    }

    gbop_hooks_initialized = true;
    return num_hooks;
}

bool
gbop_exclude_filter(const gbop_hook_desc_t *gbop_hook)
{
    char qualified_name[MAXIMUM_SYMBOL_LENGTH];
    char *os_exclude_list = "";

    if (IS_LISTSTRING_OPTION_FORALL(gbop_exclude_list)) {
        return true;
    }

    if (get_os_version() >= WINDOWS_VERSION_VISTA) {
        /* xref 9772, on Vista+ shell32!RealShellExecuteA == shell32!RealShellExecuteW
         * and shell32!RealShellExecuteExA == shell32!RealShellExecuteExW so we have to
         * avoid hooking the same spot twice. Arbitrarily exclude W versions.
         * FIXME: on win7 they all seem to be the same:
         *   0:003> x shell32!RealShellExecute*
         *   000007fe`fd9aa7bc SHELL32!RealShellExecuteExA = <no type information>
         *   000007fe`fd9aa7bc SHELL32!RealShellExecuteExW = <no type information>
         *   000007fe`fd9aa7bc SHELL32!RealShellExecuteA = <no type information>
         *   000007fe`fd9aa7bc SHELL32!RealShellExecuteW = <no type information>
         */
        os_exclude_list = "shell32.dll!RealShellExecuteW;shell32.dll!RealShellExecuteExW";
        DODEBUG_ONCE({
            HANDLE shell_mod = get_module_handle(L"shell32.dll");
            ASSERT(d_r_get_proc_address(shell_mod, "RealShellExecuteA") ==
                   d_r_get_proc_address(shell_mod, "RealShellExecuteW"));
            ASSERT(d_r_get_proc_address(shell_mod, "RealShellExecuteExA") ==
                   d_r_get_proc_address(shell_mod, "RealShellExecuteExW"));
        });
    }

    /* optimization - skip the string concatenation below if nothing to check */
    if (IS_STRING_OPTION_EMPTY(gbop_exclude_list) && *os_exclude_list == '\0') {
        return false;
    }

    /* concatenating names */
    snprintf(qualified_name, BUFFER_SIZE_ELEMENTS(qualified_name), "%s!%s",
             gbop_hook->mod_name, gbop_hook->func_name);
    NULL_TERMINATE_BUFFER(qualified_name);
    if (check_list_default_and_append(os_exclude_list, dynamo_options.gbop_exclude_list,
                                      qualified_name)) {
        return true;
    }

    /* optimization - skip the string concatenation below if nothing to check */
    if (IS_STRING_OPTION_EMPTY(gbop_exclude_list)) {
        return false;
    }

    /* check for all */
    snprintf(qualified_name, BUFFER_SIZE_ELEMENTS(qualified_name), "%s!%s",
             gbop_hook->mod_name, "*");
    NULL_TERMINATE_BUFFER(qualified_name);
    if (check_list_default_and_append("", /* no default list, we checked os above */
                                      dynamo_options.gbop_exclude_list, qualified_name)) {
        return true;
    }
    return false;
}

/* NOTE: Assumes x86
 * NOTE: CTI sizes do not include prefixes, and assumes prefixes
 *       do not change the opcode (cf. ff)
 */
enum {
    CTI_MIN_LENGTH = CTI_IND1_LENGTH,
    CTI_MAX_LENGTH = CTI_FAR_ABS_LENGTH,
};

/* Check if instruction preceding return address on TOS is a call */
/* FIXME: add stats */
static bool
gbop_is_after_cti(const app_pc ret_addr)
{
    /* Instructions are checked for cti in this order, put the most common
     * cti first
     */
    const uint cti_sizes[] = {
        CTI_DIRECT_LENGTH, CTI_IAT_LENGTH,  CTI_IND1_LENGTH,
        CTI_IND2_LENGTH,   CTI_IND3_LENGTH, CTI_FAR_ABS_LENGTH,
    };
    const int num_cti_types = sizeof(cti_sizes) / sizeof(cti_sizes[0]);

    /* While decoding we could be looking for a CTI instruction of size 2
     * e.g., and we could end up decoding  beyond CTI_MAX_LENGTH if raw bits
     * there look like other long instructions.  This could even result in
     * stack underflows.  So raw_bytes has some extra padding.
     */
    byte raw_bytes[CTI_MAX_LENGTH + MAX_INSTR_LENGTH /*padding*/];
    int i = 0;
    uint bytes_read = 0;
    bool done = false;
    dcontext_t *dcontext;

    ASSERT(TEST(GBOP_CHECK_INSTR_TYPE, DYNAMO_OPTION(gbop)));
    if (!TEST(GBOP_IS_CALL, DYNAMO_OPTION(gbop))) {
        DODEBUG_ONCE(LOG(THREAD_GET, LOG_ALL, 1,
                         "GBOP: gbop_is_after_cti: GBOP_CHECK_INSTR_TYPE is "
                         "enabled, but GBOP_IS_CALL is not\n"));
        return false;
    }

    memset((void *)raw_bytes, 0, sizeof(raw_bytes));

    /* safe_read instructions before ret_addr. Try and read the max, if
     * unsuccessful try reading one byte less at a time until min.  Common
     * case we'd do one read of CTI_MAX_LENGTH bytes.
     *
     * FIXME: OPTIMIZATION: If the first read fails, we could instead
     * align_backward on page_size, find the delta and just try that size.
     * Not worth doing unless this routine proves to be expensive.
     */
    for (bytes_read = CTI_MAX_LENGTH; bytes_read >= CTI_MIN_LENGTH; bytes_read--) {
        done = d_r_safe_read(ret_addr - bytes_read, bytes_read, (void *)raw_bytes);
        if (done)
            break;
        ASSERT_NOT_TESTED();
    }

    if (!done) {
        LOG(THREAD_GET, LOG_INTERP, 1,
            "GBOP: gbop_is_after_cti: could not read %d to %d bytes above "
            "return addr=0x%0x\n",
            CTI_MIN_LENGTH, CTI_MAX_LENGTH, ret_addr);

        ASSERT_NOT_TESTED();
        return false; /* cannot read instructions above return addr */
    }

    ASSERT(bytes_read >= CTI_MIN_LENGTH);

    /* FIXME: CLEANUP: dcontext is not used at all in decode_opcode, but
     * don't want to pass GLOBAL_DCONTEXT or NULL
     */
    dcontext = get_thread_private_dcontext();

    /* Now that we have read raw instructions, check to see if ret_addr was
     * preceded by a call.  Check if we find a call opcode at offsets listed
     * in cti_sizes[].
     */
    for (i = 0; i < num_cti_types; i++) {
        app_pc pc = NULL;
        app_pc next_pc = NULL;
        instr_t instr;

        /* skip call instruction types that are larger than bytes read */
        if (bytes_read < cti_sizes[i]) {
            ASSERT_NOT_TESTED();
            continue;
        }

        /* We come here only if bytes_read <= cti_sizes[i], and use
         * bytes_read - cti_sizes[i] as an index into raw_bytes read.
         * e.g., if bytes_read is 7 and if cti_sizes[i] = 2, an indirect
         * call, we check for this type of call at index 5.  Also, if we
         * find a call we do not expect its size to be > 2.
         */
        pc = &raw_bytes[bytes_read - cti_sizes[i]];

        /* set up instr for decode_opcode */
        instr_init(dcontext, &instr);
        DODEBUG({
            /* case 9151: only report invalid instrs for normal code decoding */
            instr.flags |= INSTR_IGNORE_INVALID;
        });

        /* NOTE: Make sure we do not do any allocations here (see note of
         * CAUTION below about hotpach limitations).  decode, if asked for
         * operands e.g., can allocate memory trying to up-decode the
         * instruction.  Even if the first src operand is statically
         * allocated and is asked for, decode may fill up other implicit
         * operands and could lead to memory allocation.
         * GBOP_EMULATE_SOURCE needs to be careful while looking at
         * operands.
         */
        next_pc = decode_opcode(dcontext, pc, &instr);
        ASSERT(!instr_has_allocated_bits(&instr));

        if (next_pc != NULL) {
            ASSERT(!instr_operands_valid(&instr));
            ASSERT(!instr_needs_encoding(&instr));

            /* If we found a valid call instruction and it is of the size we
             * expect to find, then return valid, otherwise continue looking.
             * FIXME: GBOP_IS_JMP NYI
             */
            if (instr_is_call(&instr)) {
                /* FIXME: make sure next_pc == ret_addr (NOTE: next_pc is inside
                 *        raw_bytes buffer)
                 *        something like: next_pc - pc == cti_sizes[i], since
                 *        call instructions do not have prefix
                 *
                 * FIXME: in debug build, after finding a match continue looking
                 *        and incr. stat on conflicts
                 * Note: GBOP_EMULATE_SOURCE may have to do PC relativization
                 */
                LOG(THREAD_GET, LOG_ALL, 3,
                    "GBOP: gbop_is_after_cti: found valid call preceding return "
                    "addr=0x%0x\n",
                    ret_addr);
                return true;
            }
        }
        /* loop back and check for other possible ctis */
    }

    LOG(THREAD_GET, LOG_ALL, 1,
        "GBOP: gbop_is_after_cti: no valid call preceding return addr=0x%0x\n", ret_addr);
    return false; /* didn't find a valid call instruction preceding ret_addr */
}

/* note we currently don't care which rule was broken, an exemption
 * will overrule any */
static inline bool
check_exempt_gbop_addr(app_pc violating_source_addr)
{
    /* Currently exempting only if source is a named DLL */
    /* Most violations will be failing GBOP source memory page rules
     * and not really in a proper DLL.  Yet, it is possible that
     * source is a PE module that is not properly loaded as MEM_IMAGE
     * and breaks all other allow rules based on source page
     * properties, this will look for a PE name at the allocation
     * base.  FIXME: considered a feature not a bug to allow
     * MEM_MAPPED
     *
     * case 8245 about an example where a DLL breaks the source
     * instruction type properties.
     */
    if (!IS_STRING_OPTION_EMPTY(exempt_gbop_from_default_list) ||
        !IS_STRING_OPTION_EMPTY(exempt_gbop_from_list)) {
        const char *source_module_name;
        os_get_module_info_lock();
        os_get_module_name(violating_source_addr, &source_module_name);
        LOG(THREAD_GET, LOG_INTERP, 2,
            "check_exempt_gbop_addr: source_fragment=" PFX " module_name=%s\n",
            violating_source_addr,
            source_module_name != NULL ? source_module_name : "<none>");
        /* note check_list_default_and_append will grab string_option_read_lock */
        if (source_module_name != NULL &&
            check_list_default_and_append(dynamo_options.exempt_gbop_from_default_list,
                                          dynamo_options.exempt_gbop_from_list,
                                          source_module_name)) {
            LOG(THREAD_GET, LOG_INTERP, 1,
                "GBOP: exception from exempt source module --ok\n");
            os_get_module_info_unlock();
            return true;
        }
        os_get_module_info_unlock();
    }
    return false;
}

/* CAUTION: this routine is called by hotp_only_gbop_detector, which means
 *          that it has to adhere to all limitations prescribed for hot patch
 *          code, i.e., no system calls, no calls to dr code, allocating
 *          memory, holding locks or changing whereami.  If any of those need
 *          to be done, the code should be carefully examined.
 * FIXME: as of now this function hasn't been examined as stated above.
 */
/* returns true if caller to hooked routine looks valid
 */
bool
gbop_check_valid_caller(app_pc reg_ebp, app_pc reg_esp, app_pc cur_pc,
                        app_pc *violating_source_addr /* OUT */)
{
    /* FIXME: optional: check PC -- should check PC if this is done in
     * kernel mode, otherwise we can detect only locations we have
     * hooked */

    /* optional: adjust ESP to TOS (needs FPO information).  Here we
     * assume that we have hooked at function entry points, or at
     * least early enough that [ESP] still points to the return
     * address.  Can use [EBP] in earlier frames, and only if we know
     * it is really used.
     */
    app_pc ret_on_stack = reg_esp;
    dcontext_t *dcontext = get_thread_private_dcontext();
    uint depth = 0; /* NYI stack walk */
    ASSERT(violating_source_addr != NULL);
    ASSERT(dcontext != NULL && dcontext != GLOBAL_DCONTEXT);
    if (dcontext == NULL /* case 9385: unknown thread */ || dcontext == GLOBAL_DCONTEXT) {
        /* if we don't know what's going on, we shouldn't block nor crash */
        return true;
    }

    /* for each frame */
    do {
        /* assume app stack is valid if attackers are trying to execute code */
        /* FIXME: make this a safe read, or a TRY/EXCEPT */
        /* otherwise an attack with ESP just starting off a good page
         * will be reported as DR crash.  Not very useful for attacks
         * other than that instead of return an exception is produced
         * instead, (and still generated on the new good stack) */
        app_pc purported_ret_addr = *(app_pc *)ret_on_stack; /* unsafe_read */

        /* We want to make sure that we check the properties of a page
         * on which the expected CALL instruction for sure is.  case
         * 3821 about our own interop problems with ones incorrectly
         * dealing with the very beginning of a page, and using the
         * ret addr as is may already be on the next page (though
         * interesting only if counting on exception on return).
         */
        app_pc suspect_shellcode_addr = purported_ret_addr - 1;

        bool source_page_ok = false;
        /* collect all properties for single shot diagnostics */
        bool is_exec = false;
        bool is_x = false;     /* updated only if !is_exec */
        bool is_image = false; /* updated only if !is_exec */
        bool on_stack = false;
        bool is_future_exec = false; /* updated only if !is_exec */

        reg_t spill_mc_esp;

        bool source_instr_ok = !(TEST(GBOP_CHECK_INSTR_TYPE, DYNAMO_OPTION(gbop)));
        bool is_call = false;   /* NYI */
        bool is_hp_jmp = false; /* NYI */
        bool is_jmp = false;    /* NYI */

        /* In case of failure, we point to the next instruction in
         * shellcode (not the arbitrary one in the middle of a CALL
         * that we check as suspect_shellcode_addr).  We obviously
         * can't know the beginning of the shellcode, so it's really
         * last not first bad instruction
         */
        *violating_source_addr = purported_ret_addr;

        ASSERT(DYNAMO_OPTION(gbop) != GBOP_DISABLED);
        ASSERT(
            TESTANY(GBOP_IS_EXECUTABLE | GBOP_IS_X | GBOP_IS_IMAGE | GBOP_IS_FUTURE_EXEC,
                    DYNAMO_OPTION(gbop)));

        /* You cannot have GBOP_CHECK_INSTR_TYPE w/o one of
         * GBOP_IS_{CALL,JMP,HOTPATCH_JMP}
         */
        ASSERT(!TESTANY(GBOP_CHECK_INSTR_TYPE, DYNAMO_OPTION(gbop)) ||
               (TESTANY(GBOP_CHECK_INSTR_TYPE, DYNAMO_OPTION(gbop)) &&
                TESTANY(GBOP_IS_CALL | GBOP_IS_JMP | GBOP_IS_HOTPATCH_JMP,
                        DYNAMO_OPTION(gbop))));

        ASSERT_NOT_IMPLEMENTED(!TESTANY(
            ~(GBOP_IS_EXECUTABLE | GBOP_IS_X | GBOP_IS_IMAGE | GBOP_CHECK_INSTR_TYPE |
              GBOP_IS_CALL | GBOP_IS_DGC | GBOP_IS_FUTURE_EXEC | GBOP_IS_NOT_STACK),
            DYNAMO_OPTION(gbop)));
        /* FIXME: NYI: GBOP_IS_JMP | GBOP_IS_HOTPATCH_JMP
         * GBOP_EMULATE_SOURCE | GBOP_IS_RET_TO_ENTRY
         * GBOP_WHEN_NATIVE_EXEC
         * GBOP_DIAGNOSE_SOURCE
         */

        LOG(THREAD_GET, LOG_SYSCALLS, 2,
            "GBOP: checking pc=" PFX ", reg_esp=" PFX ", reg_ebp=" PFX "\n", cur_pc,
            reg_esp, reg_ebp);

        /* As long as we make the check fast enough it will be OK to hook
         * even all functions in kernel32, or e.g. ntdll!Nt* to hook all
         * system calls.  Could use a last_area (will have to be writable
         * so not very secure) - per function or a global last_area though
         * could make check that it points within the array so can't be
         * overwritten trivially (and assuming DR memory is not writable).
         */
        if (TEST(GBOP_IS_EXECUTABLE, DYNAMO_OPTION(gbop))) {
            is_exec = is_executable_address(suspect_shellcode_addr);
        }

#    ifdef PROGRAM_SHEPHERDING
        if (!is_exec && TEST(GBOP_IS_FUTURE_EXEC, DYNAMO_OPTION(gbop))) {
            /* is_future_exec is cheaper to evaluate policy
             * than the the policies that use query_virtual_memory()
             * so doing before the rest
             */
            is_future_exec = is_in_futureexec_area(suspect_shellcode_addr);
            LOG(THREAD_GET, LOG_VMAREAS, 1,
                "GBOP: using GBOP_IS_FUTURE_EXEC " PFX " %s\n", suspect_shellcode_addr,
                is_future_exec ? "allowing future" : "not future");
            /*
             * FIXME: not supporting GBOP_DIAGNOSE_SOURCE, so
             * evaluating only if needed
             */
        }
#    endif /* PROGRAM_SHEPHERDING */

        /* may still allow with the alternative policies, note that
         * GBOP_IS_EXECUTABLE is not always a superset, of even
         * GBOP_IS_X, but usually should match what we'd need.
         */
        if (!is_exec && !is_future_exec) {
            /* note that we always check whether the weaker policies would
             * have worked even if they are not currently enabled.
             * FIXME: should be done only under GBOP_DIAGNOSE_SOURCE
             *
             * FIXME: Maybe should just have these turned on all the time,
             * (GBOP_IS_X is a problem only on Win2003 RTM), but could
             * have staging use the information to propose adding
             * post-factum since known to work.
             */
            /* FIXME: should have a vm_area_t that we track our quick list,
             * and in addition have to doublecheck for any memory that may
             * have shown up in our process without our knowledge.  Since
             * we anyways have to do check for the latter, adding to list
             * on violation instead of tracking protection changes.
             */

            /* FIXME: for the time being will do the system call all
             * the time.  Shoul add the entry to a vmarea, for the
             * latter we should be getting a real lock and make DR
             * datastructures writable.  We could otherwise process
             * these hooks without protection changes (other than locks).
             */
            app_pc check_pc = (app_pc)ALIGN_BACKWARD(suspect_shellcode_addr, PAGE_SIZE);
            MEMORY_BASIC_INFORMATION mbi;
            if (query_virtual_memory(check_pc, &mbi, sizeof(mbi)) == sizeof(mbi)) {
                is_image = (mbi.Type == MEM_IMAGE);
                is_x = prot_is_executable(mbi.Protect);
            }
            /* we only use current Protect mapping for these stateless
             * policies.  Here MEM_FREE or MEM_RESERVE are still
             * worthy of reporting a violation - since after all they
             * somehow managed to make this call - so maybe somebody
             * is fooling with us. */
            DOLOG(2, LOG_VMAREAS, {
                if (is_x && TEST(GBOP_IS_X, DYNAMO_OPTION(gbop))) {
                    LOG(THREAD_GET, LOG_VMAREAS, 1, "GBOP: using is GBOP_IS_X\n");
                }
                if (is_image && TEST(GBOP_IS_IMAGE, DYNAMO_OPTION(gbop))) {
                    /* FIXME: all we check for GBOP_IS_IMAGE is whether MEM_IMAGE
                     * is set, note executable_if_image in fact only counts on
                     * getting module base, but should also be checking this.
                     */
                    LOG(THREAD_GET, LOG_VMAREAS, 1, "GBOP: using GBOP_IS_IMAGE\n");
                }
            });
        }

        /* Is the bad address on the stack?  Case 8085.  Have to save, use and
         * restore mcontext esp because is_address_on_stack() directly gets
         * the esp from the mcontext to find the stack base & size.
         * Note: The app stack can change when we walk the frames (app. may
         *       switch stacks), so compute on_stack for each frame walked.
         */
        spill_mc_esp = get_mcontext(dcontext)->xsp;
        get_mcontext(dcontext)->xsp = (reg_t)reg_esp;
        on_stack = is_address_on_stack(dcontext, purported_ret_addr);
        get_mcontext(dcontext)->xsp = spill_mc_esp;

        ASSERT(!is_exec || TEST(GBOP_IS_EXECUTABLE, DYNAMO_OPTION(gbop)));
        ASSERT(!is_future_exec || TEST(GBOP_IS_FUTURE_EXEC, DYNAMO_OPTION(gbop)));

        /* CAUTION: the order of the source page checks shouldn't be changed! */
        source_page_ok = is_exec || is_future_exec ||
            (TEST(GBOP_IS_IMAGE, DYNAMO_OPTION(gbop)) && is_image) ||
            (TEST(GBOP_IS_X, DYNAMO_OPTION(gbop)) && is_x);

        /* Allow any target but the current stack; case 8085. */
        if (!source_page_ok &&
            (TEST(GBOP_IS_NOT_STACK, DYNAMO_OPTION(gbop)) && !on_stack)) {
            LOG(THREAD_GET, LOG_VMAREAS, 1, "GBOP: using GBOP_IS_NOT_STACK\n");
            source_page_ok = true;
        }

        /* Allow any target but the current stack, if a vm is loaded; case 8087. */
        if (!source_page_ok &&
            (TEST(GBOP_IS_DGC, DYNAMO_OPTION(gbop)) && gbop_vm_loaded && !on_stack)) {
            LOG(THREAD_GET, LOG_VMAREAS, 1, "GBOP: using GBOP_IS_DGC\n");
            source_page_ok = true;
        }

        DOLOG(2, LOG_VMAREAS, {
            /* note regular callstack dumps assume EBP chain, yet comes
             * with expensive checks for is_readable_without_exception()
             */
            dump_callstack(cur_pc, reg_ebp, THREAD_GET, DUMP_NOT_XML);
        });

        if (!source_page_ok) {
            if (!check_exempt_gbop_addr(*violating_source_addr)) {
                return false; /* bad source memory type */
            }
            LOG(THREAD_GET, LOG_VMAREAS, 1,
                "GBOP: exempted bad source memory properties\n");
            /* continuing in case we're walking stack frames */
        }

        if (TEST(GBOP_CHECK_INSTR_TYPE, DYNAMO_OPTION(gbop)))
            source_instr_ok = gbop_is_after_cti(purported_ret_addr);

        if (!source_instr_ok) {
            if (!check_exempt_gbop_addr(*violating_source_addr)) {
                return false; /* bad source instruction type */
            }
            LOG(THREAD_GET, LOG_VMAREAS, 1,
                "GBOP: exempted bad source instruction type\n");
            SYSLOG_INTERNAL_WARNING("GBOP exempted instr type @" PFX "\n",
                                    *violating_source_addr);
            /* continuing in case we're walking stack frames */
        }

        /* FIXME: stack walking will just have to iterate in the above
         * loop for the next frame, if needed
         */
        if (DYNAMO_OPTION(gbop_frames) > 0) {
            /* FIXME: how to reliably walk the stack:
             * - could check if EBP is within current thread stack
             *   as long as that is not changed by the app or for fibers
             * - it's ok to skip frames if we follow EBP,
             * - can also use SEH frames as a guideline to where function
             *   frames should be, but those may have been overwritten as well
             * - see windbg "Manually Walking a Stack" for manual ones, though
             *   kb isn't doing much
             * - look at gdb source code to see if they have a good heuristic
             */
            ASSERT_NOT_IMPLEMENTED(false);
            /* get next frame:   ra = poi(reg_ebp+4), reg_ebp = poi(reg_ebp) if safe */
            /* FIXME: check if readable without exception safely */
        }
    } while (0); /* FIXME: currently not walking frames */

    return true; /* valid, or haven't found reason */
}

/* used to validate the hooks in ntdll.dll on system calls and loader
 * routines, but not for any additional hotpatch hooks */
void
gbop_validate_and_act(app_state_at_intercept_t *state, byte fpo_adjustment,
                      app_pc hooked_target)
{
    /* FIXME: while the 'extra' hook locations (hotpatched) from
     * gbop_include_list will simply not be injected the always hooked
     * (native_exec hooked) locations may need a bitmask say
     * gbop_active[] for syscalls.  Callers should check so not to
     * burden this routine with indexing or lookup.
     *
     * FIXME: plan should add LdrLoadDll as a fake syscall and could
     * even add to syscall_trampoline_hook_pc[i], so we can deal with
     * all of these uniformly - we need index for quick check, and
     * original PC for producing the generic obfuscated threat ID.
     */
    app_pc bad_addr;

    ASSERT(DYNAMO_OPTION(gbop) != GBOP_DISABLED);
    if (!TEST(GBOP_SET_NTDLL_BASE, DYNAMO_OPTION(gbop_include_set))) {
        return;
    }

    STATS_INC(gbop_validations);
    if (!gbop_check_valid_caller((app_pc)state->mc.xbp,
                                 (app_pc)state->mc.xsp + fpo_adjustment, hooked_target,
                                 &bad_addr)) {
        dcontext_t *dcontext = get_thread_private_dcontext();
        security_option_t type_handling = OPTION_BLOCK | OPTION_REPORT;
#    ifdef PROGRAM_SHEPHERDING
        app_pc old_next_tag;
        fragment_t src_frag = { 0 }, *old_last_frag;
        priv_mcontext_t old_mc = { 0 };
#    endif

        STATS_INC(gbop_violations);
        /* FIXME: should provide the failure depth or simpy first bad
         * target to report
         */
        LOG(THREAD_GET, LOG_ASYNCH, 1, "GBOP invalid source to " PFX "!\n",
            hooked_target);
        SYSLOG_INTERNAL_ERROR("GBOP: execution attempt to " PFX " from bad " PFX "\n",
                              hooked_target, bad_addr);
        /* FIXME: reporting: have to reverse the usual meaning of good source,
         * BAD target, here Threat ID should be of hooked target as
         * constant part, and contents of source
         */

        /* FIXME: case 7946 action - standard attack handling,
         * or alternative handling return error - easy for the ntdll!Nt*,
         * not so easy for the other entry points - will need to cleanup arguments,
         * and probably not going to lead to more than crashes anyways.
         * xref case 846 about competitors silently eating these
         */
        /* FIXME: may want to return after_intercept_action_t in that
         * case to be able to modify the target to go to the function
         * exit.  However, that also requires knowing a correct
         * address viable only as a liveshield update.  A better solution
         * is to pass the number of arguments and just clean those up.
         */
        /* FIXME: need to test separately the handling of nt syscalls,
         * LdrLoadDll and the 'extra' hooks
         */

        /* FIXME: reporting for gbop should be via one path; today it is
         * different for core gbop hooks and hotp_only gbop hooks; case 8096.
         * changes here must be kept in synch with hotp_event_notify till then.
         */
#    ifdef PROGRAM_SHEPHERDING
        /* Save the last fragment, next tag & registers state, use the correct
         * ones, report & then restore.
         */
        hotp_spill_before_notify(dcontext, &old_last_frag, &src_frag, hooked_target,
                                 &old_next_tag, bad_addr, &old_mc, state,
                                 CXT_TYPE_CORE_HOOK);

        /* does not return when OPTION_BLOCK is enforced */
        if (security_violation(dcontext, bad_addr, GBOP_SOURCE_VIOLATION,
                               type_handling) == GBOP_SOURCE_VIOLATION) {
            /* running in detect mode, or action didn't kill control flow */
            ASSERT_NOT_TESTED();
        } else {
            /* exempted Threat ID */
            ASSERT_NOT_TESTED();
        }
        hotp_restore_after_notify(dcontext, old_last_frag, old_next_tag, &old_mc);
#    endif /* PROGRAM_SHEPHERDING */
        /* FIXME: we may want to cache violation source location, if
         * survived either due to either detect mode or exemption */
    }
}

void
gbop_init(void)
{
    /* all hooks already used by DR for native_exec and hotp_only do
     * not need special initialization
     */
    /* FIXME: here should hook all routines other than the ones we
     * have already hooked in ntdll!Ldr* and ntdll!Nt*
     */

    /* FIXME: note that DLLs that are not yet loaded will have to be
     * hooked at normal hotp_only times */

    /* FIXME: on hook conflicts we should give up our optional hooks -
     *   just like hotp_only should already do.
     *
     * FIXME: conflict with hotp_only hashing - we'll have to
     * recalculate hashes.  Best to treat the extra hooks as
     * hotpatches, and if we don't chain our hotpatches, should kick
     * out the existing extra hooks.
     */

    /* FIXME: Tim brought up that we can't use LdrGetProcedureAddress
     * at the time of load, but rather have to read the exports
     * ourselves - a problem mostly for already loaded DLLs that we'd
     * process at startup */

    /* FIXME: currently not called - decide its relationship with
     * hotp_init: whether before so that it uses the loader list walk,
     * or after so it can explicitly use hotp routines
     */
    ASSERT_NOT_IMPLEMENTED(false);
}

void
gbop_exit(void)
{
    /* FIXME: have to unhook all additional routines on detach */
    ASSERT_NOT_IMPLEMENTED(false);
}
#endif /* GBOP */
