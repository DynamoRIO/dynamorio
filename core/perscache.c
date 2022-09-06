/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2006-2010 VMware, Inc.  All rights reserved.
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
 * perscache.c - coarse-grain units and persistent cache management
 */

#include "globals.h"
#include "link.h"
#include "fragment.h"
#include "fcache.h"
#include "monitor.h"
#include "perscache.h"
#include "instr.h"
#include "decode_fast.h"
#include "hotpatch.h"
#include "synch.h"
#include "module_shared.h"
#include <stddef.h> /* for offsetof */
#include "instrument.h"

#ifdef DEBUG
#    include "disassemble.h"
#endif

#define MAX_PCACHE_OPTIONS_STRING (MAX_OPTIONS_STRING / 2)
/* case 10823: align option string to keep hashtable data aligned.
 * we're not using a cache-line-aligned lookuptable. */
#define OPTION_STRING_ALIGNMENT (sizeof(app_pc))
/* in general we want new data sections aligned to keep hashtable aligned */
#define CLIENT_ALIGNMENT (sizeof(app_pc))

/* used while merging */
typedef struct _jmp_tgt_list_t {
    app_pc tag;
    cache_pc jmp_end_pc;
    struct _jmp_tgt_list_t *next;
} jmp_tgt_list_t;

/* Forward decls */
static void
persist_calculate_module_digest(module_digest_t *digest, app_pc modbase, size_t modsize,
                                app_pc code_start, app_pc code_end,
                                uint validation_option);
static bool
get_persist_dir(char *directory /* OUT */, uint directory_len, bool create);

#if defined(DEBUG) && defined(INTERNAL)
static void
print_module_digest(file_t f, module_digest_t *digest, const char *prefix);
#endif

static void
coarse_unit_shift_jmps(dcontext_t *dcontext, coarse_info_t *info, ssize_t cache_shift,
                       ssize_t stubs_shift, size_t old_mapsz);

static void
coarse_unit_merge_persist_info(dcontext_t *dcontext, coarse_info_t *dst,
                               coarse_info_t *info1, coarse_info_t *info2);

#ifdef DEBUG
/* used below for pcache_dir_check_permissions() */
DECLARE_CXTSWPROT_VAR(static mutex_t pcache_dir_check_lock,
                      INIT_LOCK_FREE(pcache_dir_check_lock));
#endif

/***************************************************************************
 * COARSE-GRAIN UNITS
 */

/* case 9653/10380: only one coarse unit in a module's +x region(s) is persisted */
static void
coarse_unit_mark_primary(coarse_info_t *info)
{
    if (!info->in_use)
        return;
#ifdef WINDOWS
    /* FIXME PR 295529: put in for Linux once we have per-module flags */
    /* Go ahead and get write lock up front; else have to check again; not
     * frequently called so don't need perf opt here.
     */
    os_get_module_info_write_lock();
    if (!os_module_get_flag(info->base_pc, MODULE_HAS_PRIMARY_COARSE)) {
        os_module_set_flag(info->base_pc, MODULE_HAS_PRIMARY_COARSE);
        ASSERT(os_module_get_flag(info->base_pc, MODULE_HAS_PRIMARY_COARSE));
        info->primary_for_module = true;
        LOG(GLOBAL, LOG_CACHE, 1, "marking " PFX "-" PFX " as primary coarse for %s\n",
            info->base_pc, info->end_pc, info->module);
    }
    os_get_module_info_write_unlock();
#else
    info->primary_for_module = true;
#endif
}

static void
coarse_unit_unmark_primary(coarse_info_t *info)
{
#ifdef WINDOWS
    /* FIXME PR 295529: put in for Linux once we have per-module flags */
    if (info->primary_for_module && info->in_use) {
        ASSERT(os_module_get_flag(info->base_pc, MODULE_HAS_PRIMARY_COARSE));
        os_module_clear_flag(info->base_pc, MODULE_HAS_PRIMARY_COARSE);
        info->primary_for_module = false;
    }
#else
    info->primary_for_module = false;
#endif
}

void
coarse_unit_mark_in_use(coarse_info_t *info)
{
    info->in_use = true;
    coarse_unit_mark_primary(info);
}

coarse_info_t *
coarse_unit_create(app_pc base_pc, app_pc end_pc, module_digest_t *digest,
                   bool for_execution)
{
    coarse_info_t *info = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, coarse_info_t,
                                          /* FIXME: have separate heap acct? */
                                          ACCT_VMAREAS, PROTECTED);
    memset(info, 0, sizeof(*info));
    ASSIGN_INIT_LOCK_FREE(info->lock, coarse_info_lock);
    ASSIGN_INIT_LOCK_FREE(info->incoming_lock, coarse_info_incoming_lock);
    info->base_pc = base_pc;
    /* XXX i#704: handle overflow: better to store size */
    info->end_pc = end_pc;
    /* FIXME: set PERSCACHE_X86_{32,64} here since for x64 the live
     * unit's flags are used for 32-bit code in 64-bit processes.
     * app_memory_allocation() may need a "bool x86_mode" param that is at
     * least passed in to here: not clear if it should be stored in the
     * vmarea.
     */
    DODEBUG({
        info->is_local = false;
        info->module = os_get_module_name_strdup(info->base_pc HEAPACCT(ACCT_VMAREAS));
        if (info->module == NULL) {
            /* else our LOG statements will crash */
            info->module = dr_strdup("" HEAPACCT(ACCT_VMAREAS));
        }
        LOG(GLOBAL, LOG_CACHE, 3, "%s %s %p-%p => %p\n", __FUNCTION__, info->module,
            base_pc, end_pc, info);
    });
    if (for_execution)
        coarse_unit_mark_in_use(info);
    if (digest != NULL) {
        memcpy(&info->module_md5, digest, sizeof(info->module_md5));
    } else if (TEST(PERSCACHE_MODULE_MD5_AT_LOAD,
                    DYNAMO_OPTION(persist_gen_validation))) {
        /* case 9735: calculate the module md5 at load time so we have a consistent
         * point at which to compare it when loading in a persisted cache file.
         * If we inject at different points we may see different views of
         * post-loader vs pre-loader module changes but we'll live with that.
         * Should have consistent injection points in steady state usage.
         * FIXME PR 215036: for 4.4 we'll want to not record the at-mmap md5, but
         * rather the 1st-execution-time post-rebase md5.
         */
        app_pc modbase = get_module_base(info->base_pc);
        size_t modsize;
        os_get_module_info_lock();
        /* For linux we can't do module segment walking at initial mmap time
         * b/c the segments are not set up: we hit SIGBUS!
         */
        IF_UNIX(ASSERT_BUG_NUM(215036, true));
        if (os_get_module_info(modbase, NULL, NULL, &modsize, NULL, NULL, NULL)) {
            os_get_module_info_unlock();
            persist_calculate_module_digest(&info->module_md5, modbase, modsize,
                                            info->base_pc, info->end_pc,
                                            DYNAMO_OPTION(persist_gen_validation));
            DOLOG(1, LOG_CACHE, {
                print_module_digest(GLOBAL, &info->module_md5, "md5 at load time: ");
            });
        } else
            os_get_module_info_unlock();
    }
    /* the rest is initialized lazily in coarse_unit_init() */
    RSTATS_ADD_PEAK(num_coarse_units, 1);
    return info;
}

void
coarse_unit_free(dcontext_t *dcontext, coarse_info_t *info)
{
    LOG(GLOBAL, LOG_CACHE, 3, "%s %s %p-%p %p\n", __FUNCTION__, info->module,
        info->base_pc, info->end_pc, info);
    ASSERT(info != NULL);
    /* Elements should have been freed in coarse_unit_reset_free() */
    ASSERT(info->htable == NULL);
    ASSERT(info->th_htable == NULL);
    ASSERT(info->pclookup_htable == NULL);
    ASSERT(info->cache == NULL);
    ASSERT(info->incoming == NULL);
    ASSERT(info->stubs == NULL);
    ASSERT(info->cache_start_pc == NULL);
    ASSERT(info->stubs_start_pc == NULL);
    DODEBUG({
        if (info->module != NULL)
            dr_strfree(info->module HEAPACCT(ACCT_VMAREAS));
    });
    DELETE_LOCK(info->lock);
    DELETE_LOCK(info->incoming_lock);
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, info, coarse_info_t, ACCT_VMAREAS, PROTECTED);
    RSTATS_DEC(num_coarse_units);
}

void
coarse_unit_init(coarse_info_t *info, void *cache)
{
    ASSERT(info != NULL);
    ASSERT(cache != NULL);
    ASSERT_OWN_MUTEX(true, &info->lock);
    fragment_coarse_htable_create(info, 0, 0);
    coarse_stubs_create(info, NULL, 0);
    /* cache is passed in since it can't be created while holding info->lock */
    info->cache = cache;
}

/* If caller holds change_linking_lock and info->lock, have_locks should be true.
 * If !need_info_lock, info must be a thread-local, unlinked, private pointer!
 */
static void
coarse_unit_reset_free_internal(dcontext_t *dcontext, coarse_info_t *info,
                                bool have_locks, bool unlink, bool abdicate_primary,
                                bool need_info_lock)
{
    DEBUG_DECLARE(bool ok;)
    ASSERT(info != NULL);
    LOG(THREAD, LOG_CACHE, 2, "coarse_unit_reset_free %s\n", info->module);
    if (!have_locks) {
        /* Though only called during all-threads-synch, we still grab our lock here */
        /* higher rank than info, needed for unlink */
        if (unlink)
            acquire_recursive_lock(&change_linking_lock);
        if (need_info_lock)
            d_r_mutex_lock(&info->lock);
    }
    ASSERT(!unlink || self_owns_recursive_lock(&change_linking_lock));
    ASSERT_OWN_MUTEX(need_info_lock, &info->lock);
    ASSERT(need_info_lock || !unlink); /* else will get deadlock */
    /* case 11064: avoid rank order */
    DODEBUG({
        if (!need_info_lock)
            info->is_local = true;
    });
    if (unlink)
        coarse_unit_unlink(dcontext, info);
    fragment_coarse_htable_free(info);
    coarse_stubs_delete(info);
    fcache_coarse_cache_delete(dcontext, info);
    if (info->in_use && abdicate_primary)
        coarse_unit_unmark_primary(info);
    if (info->frozen) {
        ASSERT(info->mmap_size > 0);
        if (info->persisted) {
#if defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH)
            if (info->in_use) {
                rct_module_table_persisted_invalidate(dcontext, info->base_pc);
            }
#endif
            /* We use GLOBAL_DCONTEXT always for these in case in use */
#ifdef RCT_IND_BRANCH
            if (info->rct_table != NULL)
                rct_table_free(GLOBAL_DCONTEXT, info->rct_table, false /*data mmapped*/);
#endif
#ifdef RETURN_AFTER_CALL
            if (info->rac_table != NULL)
                rct_table_free(GLOBAL_DCONTEXT, info->rac_table, false /*data mmapped*/);
#endif
            ASSERT(info->mmap_pc != NULL);
            if (info->mmap_ro_size > 0) {
                /* two views */
                DEBUG_DECLARE(ok =) d_r_unmap_file(info->mmap_pc, info->mmap_ro_size);
                ASSERT(ok);
                DEBUG_DECLARE(ok =)
                d_r_unmap_file(info->mmap_pc + info->mmap_ro_size,
                               info->mmap_size - info->mmap_ro_size);
                ASSERT(ok);
                info->mmap_ro_size = 0;
            } else {
                DEBUG_DECLARE(ok =) d_r_unmap_file(info->mmap_pc, info->mmap_size);
                ASSERT(ok);
            }
            if (DYNAMO_OPTION(persist_lock_file)) {
                ASSERT(info->fd != INVALID_FILE);
                os_close(info->fd);
                info->fd = INVALID_FILE;
            }
        } else {
            ASSERT(info->cache_start_pc != NULL);
            ASSERT(info->stubs_start_pc != NULL);
            ASSERT(info->mmap_ro_size == 0);
            heap_munmap(info->cache_start_pc, info->mmap_size, VMM_CACHE | VMM_REACHABLE);
            if (info->has_persist_info) {
                /* Persisted units point at their mmaps for these structures;
                 * non-persisted dynamically allocate them from DR heap.
                 */
#ifdef RCT_IND_BRANCH
                if (info->rct_table != NULL)
                    rct_table_free(dcontext, info->rct_table, true);
#endif
#ifdef RETURN_AFTER_CALL
                if (info->rac_table != NULL)
                    rct_table_free(dcontext, info->rac_table, true);
#endif
#ifdef HOT_PATCHING_INTERFACE
                if (info->hotp_ppoint_vec != NULL) {
                    HEAP_ARRAY_FREE(dcontext, info->hotp_ppoint_vec, app_rva_t,
                                    info->hotp_ppoint_vec_num, ACCT_HOT_PATCHING,
                                    PROTECTED);
                }
#endif
            }
        }
    } else {
        ASSERT(info->mmap_size == 0);
        ASSERT(info->cache_start_pc == NULL);
        ASSERT(info->stubs_start_pc == NULL);
        ASSERT(!info->has_persist_info);
    }
    /* This struct may be re-used for a non-frozen/persisted unit if it was reset due
     * to a non-cache-consistency reason.  Thus we want to preserve the locks, vm
     * region, and md5, but clear everything else (case 10119).
     */
    memset(info, 0, offsetof(coarse_info_t, lock));
    if (!have_locks) {
        if (need_info_lock)
            d_r_mutex_unlock(&info->lock);
        if (unlink)
            release_recursive_lock(&change_linking_lock);
    }
}

/* If caller holds change_linking_lock and info->lock, have_locks should be true */
void
coarse_unit_reset_free(dcontext_t *dcontext, coarse_info_t *info, bool have_locks,
                       bool unlink, bool abdicate_primary)
{
    coarse_unit_reset_free_internal(dcontext, info, have_locks, unlink, abdicate_primary,
                                    true /*need_info_lock*/);
}

/* currently only one such directory expected matching
 * primary user token, see case 8812
 */
static file_t perscache_user_directory = INVALID_FILE;

void
perscache_init(void)
{
    if (DYNAMO_OPTION(use_persisted) && DYNAMO_OPTION(persist_per_user) &&
        DYNAMO_OPTION(validate_owner_dir)) {
        char dir[MAXIMUM_PATH];

        /* case 8812 we need to hold a handle to the user directory
         * from startup (we could delay until we open our first pcache file)
         */
        if (get_persist_dir(dir, BUFFER_SIZE_ELEMENTS(dir),
                            true /* note we MUST always create directory
                                  * even if never persisting */
                            )) {
            /* we just need READ_CONTROL (on Windows) to check
             * ownership, and we are NOT OK with the directory being
             * renamed (or deleted and recreated by a malactor) while
             * we still have a handle to it.
             */
            perscache_user_directory = os_open_directory(dir, 0);
            ASSERT(perscache_user_directory != INVALID_FILE);

            /* note that now that we have the actual handle open, we can validate */
            /* see os_current_user_directory() for details */
            if (perscache_user_directory != INVALID_FILE &&
                !os_validate_user_owned(perscache_user_directory)) {
                SYSLOG_INTERNAL_ERROR("%s is OWNED by an impostor!"
                                      " Persistent cache use is disabled.",
                                      dir);
                os_close(perscache_user_directory);
                perscache_user_directory = INVALID_FILE;
                /* we could also turn off use_persisted */
            } else {
                /* either FAT32 or we are the proper owner */

                /* FIXME: we have to verify that the final permissions and
                 * sharing attributes for cache/ and for the current
                 * directory, do NOT allow anyone to rename our directory
                 * while in use, and replace it.  Otherwise we'd still
                 * have to verify owner for each file as well with
                 * -validate_owner_file.  See duplicate comment in
                 * open_relocated_dlls_filecache_directory()
                 */
            }
        }
    }
}

void
perscache_fast_exit(void)
{
    if (DYNAMO_OPTION(coarse_freeze_at_exit)) {
        coarse_units_freeze_all(false /*!in place*/);
    }

    if (perscache_user_directory != INVALID_FILE) {
        ASSERT_CURIOSITY(DYNAMO_OPTION(validate_owner_dir));
        os_close(perscache_user_directory);
        perscache_user_directory = INVALID_FILE;
    }
    ASSERT(perscache_user_directory == INVALID_FILE);
}

void
perscache_slow_exit(void)
{
    DODEBUG(DELETE_LOCK(pcache_dir_check_lock););
}

/***************************************************************************
 * FROZEN UNITS
 */

/* Separated out to keep priv_mcontext_t out of critical stack path */
static void
coarse_units_freeze_translate(thread_record_t *tr,
                              const thread_synch_state_t desired_state)
{
    priv_mcontext_t mc;
    bool res;
    res = thread_get_mcontext(tr, &mc);
    ASSERT(res);
    /* We're freeing coarse fragments so we must translate all
     * threads who are currently in a coarse unit, or about
     * to enter one (case 10030).  We don't translate threads
     * in fine-grained caches as an optimization.
     * If we did one unit at a time, could compare to just that unit.
     */
    if (!res || !in_fcache((cache_pc)mc.pc) ||
        get_fcache_coarse_info((cache_pc)mc.pc) != NULL) {
        /* FIXME optimization: pass cxt for translation */
        translate_from_synchall_to_dispatch(tr, desired_state);
    } else {
        LOG(GLOBAL, LOG_FRAGMENT, 2,
            "\tin fine-grained cache so no translation needed\n");
    }
}

/* If !in_place this routine freezes (if not already) and persists.
 * FIXME case 9975: provide support for freezing in place and
 * persisting in one call?  Should we support loading in a newly
 * persisted version to replace the in-memory unit?
 */
void
coarse_units_freeze_all(bool in_place)
{
    thread_record_t **threads = NULL;
    int i, num_threads = 0;
    bool own_synch;
    dcontext_t *my_dcontext = get_thread_private_dcontext();
    const thread_synch_state_t desired_state =
        THREAD_SYNCH_SUSPENDED_VALID_MCONTEXT_OR_NO_XFER;
    if (!DYNAMO_OPTION(coarse_units) || !DYNAMO_OPTION(coarse_enable_freeze) ||
        RUNNING_WITHOUT_CODE_CACHE())
        return;
    KSTART(coarse_freeze_all);
    /* on a detach we don't need to synch or xlate the threads */
    own_synch = !dynamo_all_threads_synched;
    ASSERT(own_synch IF_WINDOWS(|| doing_detach));
    STATS_INC(coarse_freezes);
    if (own_synch) {
        /* Called from nudge threads from the code cache, so
         * if the calling fragment could be coarse, we have to
         * terminate this thread.  Case 8711 does not allow calls from
         * coarse fragments so we're fine for now.
         */
        if (!synch_with_all_threads(desired_state, &threads, &num_threads,
                                    /* FIXME: can we set mcontext->pc to next_tag and
                                     * use THREAD_SYNCH_VALID_MCONTEXT?  not if nudge
                                     * comes here */
                                    THREAD_SYNCH_NO_LOCKS_NO_XFER, /* Case 6821 */
                                    /* if we fail to suspend a thread (e.g., for
                                     * privilege reasons) just abort */
                                    THREAD_SYNCH_SUSPEND_FAILURE_ABORT
                                        /* if we get in a race with detach, or are having
                                         * synch issues for whatever reason, bail out
                                         * sooner rather than later */
                                        | THREAD_SYNCH_SMALL_LOOP_MAX)) {
            /* just give up */
            ASSERT(!OWN_MUTEX(&all_threads_synch_lock) &&
                   !OWN_MUTEX(&thread_initexit_lock));
            ASSERT(threads == NULL);
            ASSERT(!dynamo_all_threads_synched);
            STATS_INC(coarse_freeze_abort);
            LOG(GLOBAL, LOG_CACHE, 2,
                "coarse_unit_freeze: aborting due to thread synch failure\n");
            SYSLOG_INTERNAL_WARNING("coarse freeze aborted due to thread synch failure");
            KSTOP(coarse_freeze_all);
            return;
        }
    }
    ASSERT(dynamo_all_threads_synched);
    ASSERT(OWN_MUTEX(&all_threads_synch_lock) && OWN_MUTEX(&thread_initexit_lock));

    DOSTATS({
        SYSLOG_INTERNAL_INFO("freezing all coarse units @ " SSZFMT " fragments",
                             GLOBAL_STAT(num_fragments));
    });

    /* This routine does the actual freeze and persist calls
     * FIXME case 9641: should we end the synch after freezing
     * so other threads can make progress while we persist?
     */
    vm_area_coarse_units_freeze(in_place);

    if (in_place && own_synch) {
        DEBUG_DECLARE(uint removed;)
        for (i = 0; i < num_threads; i++) {
            dcontext_t *dcontext = threads[i]->dcontext;
            if (dcontext != NULL && dcontext != my_dcontext) {
                /* FIXME: share these checks w/ other synchall-and-abort users
                 * (reset) and synchall-and-don't-abort (flush).
                 */
                /* Should have aborted if we had any synch failures */
                ASSERT(thread_synch_successful(threads[i]));
                if (is_thread_currently_native(threads[i])) {
                    /* Whether in native_exec or we lost control, since we're not
                     * freeing the interception buffer, no state to worry about.
                     */
                    LOG(GLOBAL, LOG_FRAGMENT, 2,
                        "\tcurrently native so no translation needed\n");
                } else if (thread_synch_state_no_xfer(dcontext)) {
                    /* Case 6821: do not translate other synch-all-thread users. */
                    LOG(GLOBAL, LOG_FRAGMENT, 2,
                        "\tat THREAD_SYNCH_NO_LOCKS_NO_XFER so no translation needed\n");
                } else {
                    /* subroutine to avoid priv_mcontext_t on our stack when we
                     * freeze + merge&load */
                    coarse_units_freeze_translate(threads[i], desired_state);
                }
                last_exit_deleted(dcontext);
                if (is_building_trace(dcontext)) {
                    LOG(THREAD, LOG_FRAGMENT, 2,
                        "\tsquashing trace of thread " TIDFMT "\n", i);
                    trace_abort(dcontext);
                }
                if (DYNAMO_OPTION(bb_ibl_targets)) {
                    /* FIXME: we could just remove the coarse ibl entries */
                    DEBUG_DECLARE(removed =)
                    fragment_remove_all_ibl_in_region(dcontext, UNIVERSAL_REGION_BASE,
                                                      UNIVERSAL_REGION_END);
                    LOG(THREAD, LOG_FRAGMENT, 2, "\tremoved %d ibl entries\n", removed);
                }
            }
        }
        if (DYNAMO_OPTION(bb_ibl_targets)) {
            /* FIXME: we could just remove the coarse ibl entries */
            DEBUG_DECLARE(removed =)
            fragment_remove_all_ibl_in_region(GLOBAL_DCONTEXT, UNIVERSAL_REGION_BASE,
                                              UNIVERSAL_REGION_END);
            LOG(GLOBAL, LOG_FRAGMENT, 2, "\tremoved %d ibl entries\n", removed);
        }
    }

    if (own_synch)
        end_synch_with_all_threads(threads, num_threads, true /*resume*/);
    KSTOP(coarse_freeze_all);
}

/* Removes dst's data and replaces it with src's data.  Frees src.
 * Assumes that src is thread-local and not reachable by any other thread,
 * and that dst's lock is held.
 */
static void
coarse_replace_unit(dcontext_t *dcontext, coarse_info_t *dst, coarse_info_t *src)
{
    /* Perhaps we should separately allocate the locks to avoid this copying
     * for preservation?  Or memcpy all but the lock fields?  Or delete and
     * re-init them?  If we move to a model where the world isn't suspended
     * we have to ensure no other thread is trying to lock.
     */
    coarse_info_t *non_frozen;
    mutex_t temp_lock, temp_incoming_lock;
    DEBUG_DECLARE(const char *modname;)
    ASSERT_OWN_MUTEX(true, &dst->lock);
    d_r_mutex_lock(&dst->incoming_lock);
    ASSERT(src->incoming == NULL); /* else we leak */
    src->incoming = dst->incoming;
    dst->incoming = NULL; /* do not free incoming */
    d_r_mutex_unlock(&dst->incoming_lock);
    non_frozen = dst->non_frozen;
    coarse_unit_reset_free(dcontext, dst, true /*have locks*/, false /*do not unlink*/,
                           false /*keep primary*/);
    temp_lock = dst->lock;
    temp_incoming_lock = dst->incoming_lock;
    DODEBUG({ modname = dst->module; });
    memcpy(dst, src, sizeof(*dst));
    dst->lock = temp_lock;
    dst->incoming_lock = temp_incoming_lock;
    dst->non_frozen = non_frozen;
    DODEBUG({ dst->module = modname; });
    ASSERT(dst->incoming == src->incoming);
    /* update pointers from src to dst */
    fcache_coarse_set_info(dcontext, dst);
    patch_coarse_exit_prefix(dcontext, dst);
    coarse_stubs_set_info(dst);
    DODEBUG({
        /* avoid asserts */
        src->htable = NULL;
        src->th_htable = NULL;
        src->pclookup_htable = NULL;
        src->cache = NULL;
        src->incoming = NULL;
        src->stubs = NULL;
        src->cache_start_pc = NULL;
        src->stubs_start_pc = NULL;
    });
    coarse_unit_free(dcontext, src);
}

/* In-place freezing replaces info with a frozen copy.
 * Otherwise, a new copy is created for persisting, while the original
 * copy is undisturbed and unfrozen.
 * Only purpose of !in_place is to write out to disk a
 * snapshot while letting coarse unit creation continue.  Can
 * write in_place out to disk as well, so we leave that up to
 * caller.
 * Caller must hold change_linking_lock.
 * If in_place, caller is responsible for flushing the ibl tables (case 11057).
 */
coarse_info_t *
coarse_unit_freeze(dcontext_t *dcontext, coarse_info_t *info, bool in_place)
{
    coarse_info_t *frozen = NULL;
    coarse_info_t *res = NULL;
    size_t frozen_stub_size, frozen_cache_size;
    uint num_fragments, num_stubs;
    coarse_freeze_info_t *freeze_info = HEAP_TYPE_ALLOC(
        dcontext, coarse_freeze_info_t, ACCT_MEM_MGT /*appropriate?*/, PROTECTED);

    LOG(THREAD, LOG_CACHE, 2, "coarse_unit_freeze %s\n", info->module);
    STATS_INC(coarse_units_frozen);
    /* FIXME: Suspend world needed if not in-place?  Even though unit lock
     * is not held when changing unit links (e.g., unit flush holds only
     * target unit lock when unlinking incoming), the change_linking_lock
     * should give us guarantees.
     * But if we don't suspend-all we'll have other issues:
     * - fcache_coarse_init_frozen() will need to
     *   grab the shared cache lock, which is higher rank than coarse unit lock!
     * - same issue w/ fcache_coarse_cache_delete (via coarse_unit_reset_free)
     */
    /* FIXME: support single-unit freeze by having this routine itself
     * do a synch-all?
     */
    ASSERT(dynamo_all_threads_synched);

    ASSERT(info != NULL);
    ASSERT_OWN_RECURSIVE_LOCK(true, &change_linking_lock);

    /* trigger lazy initialize to avoid deadlock on calling
     * coarse_cti_is_intra_fragment() during shifting
     */
    fragment_coarse_create_entry_pclookup_table(dcontext, info);

    d_r_mutex_lock(&info->lock);
    ASSERT(info->cache != NULL); /* don't freeze empty units */
    ASSERT(!info->frozen);       /* don't freeze already frozen units */
    if (info->cache == NULL || info->frozen)
        goto coarse_unit_freeze_exit;
    /* invalid unit shouldn't get this far */
    ASSERT(!TEST(PERSCACHE_CODE_INVALID, info->flags));
    if (TEST(PERSCACHE_CODE_INVALID, info->flags)) /* paranoid */
        goto coarse_unit_freeze_exit;

    memset(freeze_info, 0, sizeof(*freeze_info));
    freeze_info->src_info = info;

    /* Tasks:
     * 1) Calculate final size of cache and stub space:
     *    Walk entrance stubs and count how many are intra-unit links
     *    that can be changed to direct jmps
     * 2) Create single contiguous region to hold both cache and stubs,
     *    rounding up to a page boundary in the middle for +r->+rw
     * 3) Copy each fragment and stub over
     *
     * FIXME case 9428: shrink the cache to take advantage of elided jmps!
     * Requires a separate pass to touch up jmps to stubs/prefixes, or
     * re-ordering w/ stubs on top and cache on bottom.  That would also
     * put a read-only page at the end, so no guard page needed -- unless we
     * hook our own cache (case 9673) and are worried about brief periods of +w.
     */

    frozen_stub_size =
        coarse_frozen_stub_size(dcontext, info, &num_fragments, &num_stubs);
    frozen_cache_size = coarse_frozen_cache_size(dcontext, info);
    /* we need the stubs to start on a new page since will be +rw vs cache +r */
    frozen_cache_size = ALIGN_FORWARD(frozen_cache_size, PAGE_SIZE);
    freeze_info->cache_start_pc = (cache_pc)heap_mmap(
        frozen_stub_size + frozen_cache_size, MEMPROT_EXEC | MEMPROT_READ | MEMPROT_WRITE,
        VMM_CACHE | VMM_REACHABLE);
    /* FIXME: should show full non-frozen size as well */
    LOG(THREAD, LOG_CACHE, 2,
        "%d frozen stubs @ " SZFMT " bytes + %d fragments @ " SZFMT " bytes => " PFX "\n",
        num_stubs, frozen_stub_size, num_fragments, frozen_cache_size,
        freeze_info->cache_start_pc);
    STATS_ADD(coarse_fragments_frozen, num_fragments);

    /* We use raw pcs to build up our cache and stubs, and later we impose
     * our regular data structures on them
     */

    /* Whether freezing in-place or not, we create a new coarse_info_t.
     * If in-place we delete the old one afterward.
     */
    frozen = coarse_unit_create(info->base_pc, info->end_pc, &info->module_md5,
                                in_place && info->in_use);
    freeze_info->dst_info = frozen;
    frozen->frozen = true;
    frozen->cache_start_pc = freeze_info->cache_start_pc;
    frozen->mmap_size = frozen_stub_size + frozen_cache_size;
    /* Our relative jmps require that we do not exceed 32-bit reachability */
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_int(frozen->mmap_size)));
    /* Same bounds, so same persistence privileges */
    frozen->primary_for_module = info->primary_for_module;

    freeze_info->stubs_start_pc = coarse_stubs_create(
        frozen, freeze_info->cache_start_pc + frozen_cache_size, frozen_stub_size);
    ASSERT(freeze_info->stubs_start_pc != NULL);
    ASSERT(ALIGNED(freeze_info->stubs_start_pc, coarse_stub_alignment(info)));
    frozen->stubs_start_pc = freeze_info->stubs_start_pc;
    ASSERT(frozen->fcache_return_prefix ==
           freeze_info->cache_start_pc + frozen_cache_size);
#if 0
    ASSERT(frozen->trace_head_return_prefix == frozen->fcache_return_prefix +
           (info->trace_head_return_prefix - info->fcache_return_prefix));
    ASSERT(frozen->ibl_ret_prefix == frozen->fcache_return_prefix +
           (info->ibl_ret_prefix - info->fcache_return_prefix));
    ASSERT(frozen->ibl_call_prefix == frozen->fcache_return_prefix +
           (info->ibl_call_prefix - info->fcache_return_prefix));
    ASSERT(frozen->ibl_jmp_prefix == frozen->fcache_return_prefix +
           (info->ibl_jmp_prefix - info->fcache_return_prefix));
#endif

    fragment_coarse_htable_create(frozen, num_fragments, num_stubs);

    fcache_coarse_init_frozen(dcontext, frozen, freeze_info->cache_start_pc,
                              frozen_cache_size);

    /* assumption: leave inter-unit links intact for in_place, but not (for
     * persisting) otherwise
     */
    freeze_info->unlink = !in_place;

    freeze_info->cache_cur_pc = freeze_info->cache_start_pc;
    freeze_info->stubs_cur_pc = freeze_info->stubs_start_pc;

    fragment_coarse_unit_freeze(dcontext, freeze_info);
    ASSERT(freeze_info->pending == NULL);
    ASSERT(freeze_info->cache_cur_pc <= freeze_info->cache_start_pc + frozen_cache_size);
    ASSERT(freeze_info->stubs_cur_pc <= freeze_info->stubs_start_pc + frozen_stub_size);
    if (frozen->fcache_return_prefix + frozen_stub_size == freeze_info->stubs_cur_pc)
        frozen->stubs_end_pc = freeze_info->stubs_cur_pc;
    else {
        /* FIXME case 9428: strange history here: I don't see a problem now,
         * but leaving some release-build code just in case.
         */
        ASSERT_NOT_REACHED();
        coarse_stubs_set_end_pc(frozen, freeze_info->stubs_cur_pc);
    }
    frozen->cache_end_pc = freeze_info->cache_cur_pc;

    LOG(THREAD, LOG_CACHE, 2, "frozen code stats for %s:\n  %6d app code\n", info->module,
        freeze_info->app_code_size);
    LOG(THREAD, LOG_CACHE, 2, "  %6d fallthrough\n", freeze_info->added_fallthrough);
    LOG(THREAD, LOG_CACHE, 2, "  %6d ind br mangle\n", freeze_info->added_indbr_mangle);
    LOG(THREAD, LOG_CACHE, 2, "  %6d indr br stubs\n", freeze_info->added_indbr_stub);
    LOG(THREAD, LOG_CACHE, 2, "  %6d jecxz mangle\n", freeze_info->added_jecxz_mangle);
    LOG(THREAD, LOG_CACHE, 2, " -%6d = 5 x %d elisions\n", freeze_info->num_elisions * 5,
        freeze_info->num_elisions);
    LOG(THREAD, LOG_CACHE, 2, "ctis: %5d cbr, %5d jmp, %5d call, %5d ind\n",
        freeze_info->num_cbr, freeze_info->num_jmp, freeze_info->num_call,
        freeze_info->num_indbr);
    LOG(THREAD, LOG_CACHE, 2,
        "frozen final size: stubs " SZFMT " bytes + cache " SZFMT " bytes\n",
        freeze_info->stubs_cur_pc - freeze_info->stubs_start_pc,
        freeze_info->cache_cur_pc - freeze_info->cache_start_pc);

    /* FIXME case 9687: mark cache as read-only */

    if (in_place) {
        coarse_replace_unit(dcontext, info, frozen);
        frozen = NULL;
        mark_executable_area_coarse_frozen(info);
        coarse_unit_shift_links(dcontext, info);
        res = info;
    } else {
        /* we made separate copy that has no outgoing or incoming links */
        res = frozen;
    }

coarse_unit_freeze_exit:
    HEAP_TYPE_FREE(dcontext, freeze_info, coarse_freeze_info_t,
                   ACCT_MEM_MGT /*appropriate?*/, PROTECTED);

    d_r_mutex_unlock(&info->lock);

    /* be sure to free to avoid missing entries if we add to info later */
    fragment_coarse_free_entry_pclookup_table(dcontext, info);

    DOLOG(3, LOG_CACHE, {
        if (res != NULL) {
            byte *pc = frozen->cache_start_pc;
            LOG(THREAD, LOG_CACHE, 1, "frozen cache for %s:\n", info->module);
            do {
                app_pc tag = fragment_coarse_entry_pclookup(dcontext, frozen, pc);
                if (tag != NULL)
                    LOG(THREAD, LOG_CACHE, 1, "tag " PFX ":\n", tag);
                pc = disassemble_with_bytes(dcontext, pc, THREAD);
            } while (pc < frozen->cache_end_pc);
        }
    });

    return res;
}

/* These decode-and-instr-using routines could go in arch/ as they assume that direct
 * jump operands are 4 bytes and are at the end of the instruction.
 */

/* Transfers a coarse stub to a new location.
 * If freeze_info->dst_info is non-NULL,
 *   shifts any unlinked stubs to point at the prefixes in freeze_info->dst_info.
 * If freeze_info->unlink is true,
 *   points any linked stubs at freeze_info->dst_info->fcache_return_prefix if
 *   freeze_info->dst_info is non-NULL, else
 *   freeze_info->src_info->fcache_return_prefix.  If trace_head, points at
 *   trace_head_return_prefix instead of fcache_return_prefix.
 * replace_outgoing really only applies if in_place: should we replace
 *   outgoing link incoming entries (true), or add new ones (false)
 */
void
transfer_coarse_stub(dcontext_t *dcontext, coarse_freeze_info_t *freeze_info,
                     cache_pc stub, bool trace_head, bool replace_outgoing)
{
    cache_pc tgt = entrance_stub_jmp_target(stub);
    cache_pc pc = freeze_info->stubs_cur_pc; /* target pc */
    uint sz;
    bool update_out = false;
    /* Should not be targeting the cache, else our later shift will be wrong */
    ASSERT(tgt < freeze_info->src_info->cache_start_pc ||
           tgt >= freeze_info->src_info->cache_end_pc);
    if (tgt == freeze_info->src_info->fcache_return_prefix) {
        ASSERT(!trace_head);
        if (freeze_info->dst_info != NULL)
            tgt = freeze_info->dst_info->fcache_return_prefix;
        LOG(THREAD, LOG_FRAGMENT, 4,
            "    transfer_coarse_stub " PFX ": tgt is fcache_return_prefix\n", stub);
    } else if (tgt == freeze_info->src_info->trace_head_return_prefix) {
        ASSERT(trace_head);
        if (freeze_info->dst_info != NULL)
            tgt = freeze_info->dst_info->trace_head_return_prefix;
        LOG(THREAD, LOG_FRAGMENT, 4,
            "    transfer_coarse_stub " PFX ": tgt is trace_head_return_prefix\n", stub);
    } else if (freeze_info->unlink) {
        coarse_info_t *info = (freeze_info->dst_info != NULL) ? freeze_info->dst_info
                                                              : freeze_info->src_info;
        if (trace_head) {
            tgt = info->trace_head_return_prefix;
            LOG(THREAD, LOG_FRAGMENT, 4,
                "    transfer_coarse_stub " PFX ": unlinking as trace head\n", stub);
        } else {
            tgt = info->fcache_return_prefix;
            LOG(THREAD, LOG_FRAGMENT, 4,
                "    transfer_coarse_stub " PFX ": unlinking as non-trace head\n", stub);
        }
    } else
        update_out = true;
    sz = exit_stub_size(dcontext, tgt, FRAG_COARSE_GRAIN) -
        (JMP_LONG_LENGTH - 1 /*get opcode*/);
    memcpy(pc, stub, sz);
    pc += sz;
    ASSERT(pc == entrance_stub_jmp(freeze_info->stubs_cur_pc) + 1 /*skip opcode*/);
#ifdef X86
    ASSERT(*(pc - 1) == JMP_OPCODE);
#elif defined(ARM)
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
#endif
    /* if tgt unchanged we still need to re-relativize it */
    ASSERT(dynamo_all_threads_synched); /* thus NOT_HOT_PATCHABLE */
    pc = insert_relative_target(pc, tgt, NOT_HOT_PATCHABLE);
    if (update_out) {
        coarse_update_outgoing(dcontext, stub, freeze_info->stubs_cur_pc,
                               freeze_info->src_info, replace_outgoing);
    }
    pc = (cache_pc)ALIGN_FORWARD(pc, coarse_stub_alignment(freeze_info->src_info));
    freeze_info->stubs_cur_pc = pc;
}

void
transfer_coarse_stub_fix_trace_head(dcontext_t *dcontext,
                                    coarse_freeze_info_t *freeze_info, cache_pc stub)
{
    /* We don't know body pc at fragment exit processing time and so can
     * add a stub and unlink it as a non-trace head if it was linked to
     * a trace, so we fix it up later
     */
    coarse_info_t *info =
        (freeze_info->dst_info != NULL) ? freeze_info->dst_info : freeze_info->src_info;
    ASSERT(freeze_info->unlink);
    if (entrance_stub_jmp_target(stub) == info->fcache_return_prefix) {
        cache_pc tgt = info->trace_head_return_prefix;
        ASSERT(dynamo_all_threads_synched); /* thus NOT_HOT_PATCHABLE */
        insert_relative_target(entrance_stub_jmp(stub) + 1 /*skip opcode*/, tgt,
                               NOT_HOT_PATCHABLE);
        LOG(THREAD, LOG_FRAGMENT, 4,
            "    fixing up stub " PFX " to be unlinked as a trace head\n", stub);
    } else
        ASSERT(entrance_stub_jmp_target(stub) == info->trace_head_return_prefix);
}

static void
push_pending_freeze(dcontext_t *dcontext, coarse_freeze_info_t *freeze_info,
                    cache_pc exit_tgt, uint cti_len, cache_pc cti_pc,
                    cache_pc body_start_pc)
{
    pending_freeze_t *pending;
    cache_pc stub_target;
    uint sz;

    pending = HEAP_TYPE_ALLOC(dcontext, pending_freeze_t, ACCT_MEM_MGT /*appropriate?*/,
                              UNPROTECTED);
    ASSERT(coarse_is_entrance_stub(exit_tgt));
    pending->tag = entrance_stub_target_tag(exit_tgt, freeze_info->src_info);
    stub_target = entrance_stub_jmp_target(exit_tgt);
    if (entrance_stub_linked(exit_tgt, freeze_info->src_info) &&
        get_fcache_coarse_info(stub_target) == freeze_info->src_info) {
        /* Intra-unit non-trace-head target: eliminate stub */
        pending->entrance_stub = false;
        pending->cur_pc = stub_target;
        /* trace heads are discovered in the htable walk, never pushed here */
        pending->trace_head = false;
    } else {
        /* Leave stub */
        pending->entrance_stub = true;
        pending->cur_pc = exit_tgt;
        DOCHECK(1, {
            cache_pc body;
            /* A rank order violation (grabbing src htable read lock while
             * holding frozen htable write lock) prevents us from calling the
             * exported interface fragment_coarse_lookup_in_unit, so instead we
             * have a hack where we call the lower-level routine that is
             * exported only for us.
             */
            coarse_body_from_htable_entry(dcontext, freeze_info->src_info, pending->tag,
                                          exit_tgt, NULL, &body);
            ASSERT(body == NULL || coarse_is_trace_head(exit_tgt) ||
                   fragment_lookup_trace(dcontext, pending->tag) != NULL);
        });
        /* We do not look up body pc to see if a trace head stub linked to
         * a trace; instead we fix up the unlinked target (for freeze_info->unlink)
         * when we proactively add the stub when processing the head body
         */
        pending->trace_head = coarse_is_trace_head(exit_tgt);
        /* If target is trace head in same unit, we could add to pending,
         * but don't have body pc handy, so we let htable walk find it
         */
    }
    ASSERT(cti_len > 4);
    sz = cti_len - 4;
    pending->link_cti_opnd = freeze_info->cache_cur_pc + sz;
    memcpy(freeze_info->cache_cur_pc, cti_pc, sz);
    if (body_start_pc == cti_pc && !DYNAMO_OPTION(unsafe_freeze_elide_sole_ubr)) {
        /* case 9677: unsafe to elide entire-bb-ubr */
        pending->elide_ubr = false;
    } else /* elide if possible */
        pending->elide_ubr = true;
    freeze_info->cache_cur_pc += cti_len;
    pending->next = freeze_info->pending;
    freeze_info->pending = pending;
}

static cache_pc
redirect_to_tgt_ibl_prefix(dcontext_t *dcontext, coarse_freeze_info_t *freeze_info,
                           cache_pc tgt)
{
    ASSERT(freeze_info != NULL && freeze_info->src_info != NULL &&
           freeze_info->dst_info != NULL);
    if (tgt == freeze_info->src_info->ibl_ret_prefix)
        return freeze_info->dst_info->ibl_ret_prefix;
    else if (tgt == freeze_info->src_info->ibl_call_prefix)
        return freeze_info->dst_info->ibl_call_prefix;
    else if (tgt == freeze_info->src_info->ibl_jmp_prefix)
        return freeze_info->dst_info->ibl_jmp_prefix;
    else
        ASSERT_NOT_REACHED();
    return tgt; /* best chance of continuing on */
}

/* Transfers a coarse fragment to a new location.  Queues up all of its
 * exit targets for transfer as well, scheduling ubr last to enable ubr elision.
 */
void
transfer_coarse_fragment(dcontext_t *dcontext, coarse_freeze_info_t *freeze_info,
                         cache_pc body)
{
    /* FIXME: for maximum code re-use, use decode_fragment instead of trying
     * to be efficient?
     * FIXME case 9428: 8-bit conversion
     */
    cache_pc pc = body, next_pc = pc; /* source pcs */
    app_pc tgt;
    size_t sz;
    bool intra_fragment = false;
    instr_t *instr;
    instr = instr_create(dcontext);
    do {
        instr_reset(dcontext, instr);
        pc = next_pc;
        ASSERT(pc - body <= MAX_FRAGMENT_SIZE);
        next_pc = decode_cti(dcontext, pc, instr);
        /* Case 8711: we can't distinguish exit ctis from others,
         * so we must assume that any cti is an exit cti, although
         * we do now support intra-fragment ctis (i#665).
         * Assumption: coarse-grain bbs have 1 ind exit or 2 direct,
         * and no code beyond the last exit!
         */
        intra_fragment = false;
        if (instr_opcode_valid(instr) && instr_is_cti(instr)) {
            if (instr_is_cti_short_rewrite(instr, pc)) {
                /* Pull in the two short jmps for a "short-rewrite" instr.
                 * We must do this before asking whether it's an
                 * intra-fragment so we don't just look at the
                 * first part of the sequence.
                 */
                next_pc = remangle_short_rewrite(dcontext, instr, pc, 0 /*same target*/);
            }
            if (coarse_cti_is_intra_fragment(dcontext, freeze_info->src_info, instr,
                                             body))
                intra_fragment = true;
        }
    } while (!instr_opcode_valid(instr) || !instr_is_cti(instr) || intra_fragment);

    /* copy body of fragment, up to start of cti */
    sz = pc - body;
    memcpy(freeze_info->cache_cur_pc, body, sz);
    freeze_info->cache_cur_pc += sz;
    DODEBUG({ freeze_info->app_code_size += sz; });

    /* Ensure we get proper target for short cti sequence */
    if (instr_is_cti_short_rewrite(instr, pc)) {
        /* We already remangled if a short-rewrite */
        DODEBUG({
            /* We mangled 2-byte jecxz/loop* into 9-byte sequence */
            freeze_info->app_code_size -= 7;
            freeze_info->added_jecxz_mangle += 7;
        });
    }
    tgt = opnd_get_pc(instr_get_target(instr));
    if (tgt == next_pc) {
        ASSERT(instr_is_ubr(instr));
        /* indirect exit stub */
        ASSERT(coarse_is_indirect_stub(tgt));
        /* elide the jmp to the stub */
        pc += JMP_LONG_LENGTH /*ubr to stub*/;
        sz = coarse_indirect_stub_size(freeze_info->src_info) - 4;
        memcpy(freeze_info->cache_cur_pc, pc, sz);
        freeze_info->cache_cur_pc += sz;
        pc += sz;
        tgt = PC_RELATIVE_TARGET(pc);
        DODEBUG({
            freeze_info->num_indbr++;
            freeze_info->app_code_size -= 6; /* save ecx */
            freeze_info->added_indbr_mangle += 6 /* save ecx */;
            if (tgt == freeze_info->src_info->ibl_ret_prefix) {
                /* ret imm goes from 3 bytes to 1+4=5 bytes
                   L3  c2 18 00             ret    $0x0018 %esp (%esp) -> %esp
                   =>
                   L4  67 64 89 0e e8 0e    addr16 mov    %ecx -> %fs:0xee8
                   L4  59                   pop    %esp (%esp) -> %ecx %esp
                   L4  8d 64 24 18          lea    0x18(%esp) -> %esp
                */
                /* guaranteed to be able to read 5 bytes back */
                if (*(pc - 4) == 0x8d && *(pc - 3) == 0x6d && *(pc - 2) == 0x24) {
                    freeze_info->app_code_size -= 2;
                    freeze_info->added_indbr_mangle += 2;
                }
            } else if (tgt == freeze_info->src_info->ibl_call_prefix) {
                /* change from call* to mov is no size diff */
                freeze_info->added_indbr_mangle += 5 /* push immed */;
            } else {
                /* jmp*: change to mov is no size difference */
            }
            freeze_info->added_indbr_stub +=
                coarse_indirect_stub_size(freeze_info->src_info);
        });
        tgt = redirect_to_tgt_ibl_prefix(dcontext, freeze_info, tgt);
        ASSERT(dynamo_all_threads_synched); /* thus NOT_HOT_PATCHABLE */
        freeze_info->cache_cur_pc =
            insert_relative_target(freeze_info->cache_cur_pc, tgt, NOT_HOT_PATCHABLE);
    } else {
        /* FIXME: if we had profile info we could reverse the branch and
         * make our cache trace-like
         */
        DEBUG_DECLARE(bool is_cbr = false;)
        if (instr_is_cbr(instr)) {
            uint cbr_len;
            /* push cbr target on todo-stack */
            if (instr_is_cti_short_rewrite(instr, pc))
                cbr_len = CBR_SHORT_REWRITE_LENGTH;
            else
                cbr_len = CBR_LONG_LENGTH;
            push_pending_freeze(dcontext, freeze_info, tgt, cbr_len, pc, body);
            ASSERT(pc + cbr_len == next_pc);

            /* process ubr next */
            instr_reset(dcontext, instr);
            pc = next_pc;
            next_pc = decode_cti(dcontext, pc, instr);
            ASSERT(instr_opcode_valid(instr) && instr_is_ubr(instr));
            tgt = opnd_get_pc(instr_get_target(instr));
            DODEBUG({
                freeze_info->num_cbr++;
                /* FIXME: assumes 32-bit cbr! */
                freeze_info->app_code_size += cbr_len;
                freeze_info->added_fallthrough += 5;
                is_cbr = true;
            });
        }

        ASSERT(instr_is_ubr(instr));
        /* push ubr last, so we can elide the jmp if we process it next */
        push_pending_freeze(dcontext, freeze_info, tgt, JMP_LONG_LENGTH, pc, body);
        ASSERT(pc + JMP_LONG_LENGTH == next_pc);
        DODEBUG({
            if (!is_cbr) {
                if (pc >= body + 5 && *(pc - 5) == 0x68) {
                    /* FIXME: could be an app push immed followed by app jmp */
                    /* call => push immed: same size, but adding jmp */
                    freeze_info->num_call++;
                    freeze_info->added_fallthrough += 5; /* jmp */
                } else {
                    /* FIXME: assumes 32-bit jmp! */
                    freeze_info->app_code_size += 5;
                    freeze_info->num_jmp++;
                }
            }
        });
    }
    instr_destroy(dcontext, instr);
}

/* This routine walks info's cache and updates extra-cache jmp targets by cache_shift
 * and jmps to stubs by stubs_shift.
 * If !is_cache, assumes these are stubs and decodes and acts appropriately.
 */
static void
coarse_unit_shift_jmps_internal(dcontext_t *dcontext, coarse_info_t *info,
                                ssize_t cache_shift, ssize_t stubs_shift,
                                size_t old_mapsz, cache_pc start, cache_pc end,
                                cache_pc bounds_start, cache_pc bounds_end, bool is_cache)
{
    /* We must patch up indirect and direct stub jmps to prefixes */
    cache_pc pc = start;
    cache_pc next_pc = pc;
    app_pc tgt;
    instr_t *instr;
    ASSERT(dynamo_all_threads_synched); /* thus NOT_HOT_PATCHABLE */
    ASSERT(info->frozen);
    instr = instr_create(dcontext);
    while (next_pc < end) {
        instr_reset(dcontext, instr);
        pc = next_pc;
        next_pc = decode_cti(dcontext, pc, instr);
        /* Case 8711: we can't distinguish exit ctis from others.
         * Note that we don't need to distinguish intra-fragment ctis here
         * b/c we want to shift them by the same amount (xref i#665).
         */
        if (instr_opcode_valid(instr) && instr_is_cti(instr)) {
            if (instr_is_cti_short_rewrite(instr, pc))
                next_pc = remangle_short_rewrite(dcontext, instr, pc, 0 /*same target*/);
            tgt = opnd_get_pc(instr_get_target(instr));
            if (tgt < bounds_start || tgt >= bounds_end) {
                ssize_t shift;
                if (is_cache) {
                    /* break down into whether targeting stubs or not
                     * ok to use new prefix start, which is where old padding was
                     */
                    if (tgt >= info->fcache_return_prefix &&
                        tgt < info->cache_start_pc + old_mapsz)
                        shift = stubs_shift;
                    else
                        shift = cache_shift;
                } else {
                    /* Shifting jmps from stubs
                     * We started with [cache | padding | stubs | padding]
                     * We then allocate new memory and copy there [cache | stubs]
                     * Thus, the stubs have a double shift: once for padding bet
                     * cache and stubs, and once for shift of whole alloc.
                     * This doesn't work if stubs target cache, but we assert on
                     * that in transfer_coarse_stub().
                     */
                    shift = cache_shift - stubs_shift;
                }
                LOG(THREAD, LOG_FRAGMENT, 4,
                    "\tshifting jmp @" PFX " " PFX " from " PFX " to " PFX "\n", pc,
                    next_pc - 4, tgt, tgt + shift);
                insert_relative_target(next_pc - 4, tgt + shift, NOT_HOT_PATCHABLE);
                if (!is_cache) {
                    /* we must update incoming after fixing target, since old_stub is
                     * inconsistent and we need a complete stub to dereference
                     */
                    cache_pc old_stub, new_stub;
                    /* double-check post-shift: no prefix targets */
                    ASSERT(tgt + shift < bounds_start || tgt + shift >= bounds_end);
                    new_stub = (cache_pc)ALIGN_BACKWARD(pc, coarse_stub_alignment(info));
                    old_stub = new_stub + shift;
                    /* we can't assert that old_stub or new_stub are entrance_stubs
                     * since targets are currently inconsistent wrt info
                     */
                    /* must update incoming stub for target */
                    coarse_update_outgoing(dcontext, old_stub, new_stub, info,
                                           true /*replace*/);
                }
            }
            if (!is_cache) {
                /* for stubs, skip the padding (which we'll decode as garbage */
                ASSERT(next_pc + IF_X64_ELSE(3, 1) ==
                       (cache_pc)ALIGN_FORWARD(next_pc, coarse_stub_alignment(info)));
                next_pc = (cache_pc)ALIGN_FORWARD(next_pc, coarse_stub_alignment(info));
            }
        }
    }
    instr_destroy(dcontext, instr);
}

/* This routine walks info's cache and updates extra-cache jmp targets by cache_shift
 * and jmps to stubs by stubs_shift.
 * It also walks info's stubs and updates targets that are not prefixes:
 * in other coarse units or in fine-grained fragment caches.
 */
static void
coarse_unit_shift_jmps(dcontext_t *dcontext, coarse_info_t *info, ssize_t cache_shift,
                       ssize_t stubs_shift, size_t old_mapsz)
{
    LOG(THREAD, LOG_FRAGMENT, 4, "shifting jmps for cache " PFX "-" PFX "\n",
        info->cache_start_pc, info->cache_end_pc);
    coarse_unit_shift_jmps_internal(
        dcontext, info, cache_shift, stubs_shift, old_mapsz, info->cache_start_pc,
        info->cache_end_pc, info->cache_start_pc, info->cache_end_pc, true /*cache*/);
    LOG(THREAD, LOG_FRAGMENT, 4, "shifting jmps for stubs " PFX "-" PFX "\n",
        info->stubs_start_pc, info->stubs_end_pc);
    coarse_unit_shift_jmps_internal(dcontext, info, cache_shift, stubs_shift, old_mapsz,
                                    info->stubs_start_pc, info->stubs_end_pc,
                                    /* do not re-relativize prefix targets */
                                    info->fcache_return_prefix, info->stubs_end_pc,
                                    false /*stubs*/);
}

/***************************************************************************
 * MERGING FROZEN UNITS
 */

/* Processes a stub in the original old source unit at stub whose
 * targeting cti has been copied into the new being-built merged unit
 * at dst_cache_pc and has length cti_len.  Passing dst_cache_pc==NULL
 * causes no cti patching to occur.
 */
static void
coarse_merge_process_stub(dcontext_t *dcontext, coarse_freeze_info_t *freeze_info,
                          cache_pc old_stub, uint cti_len, cache_pc dst_cache_pc,
                          bool replace_outgoing)
{
    app_pc old_stub_tgt;
    cache_pc dst_body, dst_stub, patch_pc, src_body;
    bool trace_head;
    ASSERT(coarse_is_entrance_stub(old_stub));
    ASSERT(dynamo_all_threads_synched); /* thus NOT_HOT_PATCHABLE */
    ASSERT((dst_cache_pc == NULL && cti_len == 0) || cti_len > 4);
    patch_pc = dst_cache_pc + cti_len - 4;
    old_stub_tgt = entrance_stub_target_tag(old_stub, freeze_info->src_info);
    fragment_coarse_lookup_in_unit(dcontext, freeze_info->dst_info, old_stub_tgt,
                                   &dst_stub, &dst_body);
    /* We need to know for sure whether a trace head as we're not doing
     * a pass through the htable like we do for regular freezing
     */
    fragment_coarse_lookup_in_unit(dcontext, freeze_info->src_info, old_stub_tgt, NULL,
                                   &src_body);
    /* Consider both sources for headness */
    trace_head =
        coarse_is_trace_head_in_own_unit(dcontext, old_stub_tgt, old_stub, src_body, true,
                                         freeze_info->src_info) ||
        (dst_stub != NULL &&
         coarse_is_trace_head_in_own_unit(dcontext, old_stub_tgt, dst_stub, dst_body,
                                          true, freeze_info->dst_info));
    /* Should only be adding w/ no source cti if a trace head or for the stub
     * walk for the larger unit where we have a dup stub and aren't replacing */
    ASSERT(dst_cache_pc != NULL || trace_head ||
           (dst_body == NULL && dst_stub != NULL && !replace_outgoing));
    if (dst_body != NULL && !trace_head) {
        /* Directly link and do not copy the stub */
        LOG(THREAD, LOG_FRAGMENT, 4,
            "\ttarget " PFX " is in other cache @" PFX ": directly linking\n",
            old_stub_tgt, dst_body);
        ASSERT(dst_stub == NULL);
        ASSERT(dst_body >= freeze_info->dst_info->cache_start_pc &&
               dst_body < freeze_info->dst_info->cache_end_pc);
        if (dst_cache_pc != NULL)
            insert_relative_target(patch_pc, dst_body, NOT_HOT_PATCHABLE);
        if (!freeze_info->unlink &&
            entrance_stub_linked(old_stub, freeze_info->src_info)) {
            /* ASSUMPTION: unlink == !in_place
             * If in-place, we must update target incoming info, whether source is
             * primary (being replaced) or secondary (probably being deleted since
             * now in merge result, but we don't want to crash while unlinking it
             * (case 10382)) source.
             */
            coarse_remove_outgoing(dcontext, old_stub, freeze_info->src_info);
        }
    } else if (dst_stub != NULL) {
        LOG(THREAD, LOG_FRAGMENT, 4, "\ttarget " PFX " is already in stubs @" PFX "\n",
            old_stub_tgt, dst_stub);
        ASSERT(dst_body == NULL || trace_head);
        /* Stub already exists: point to it */
        if (dst_cache_pc != NULL)
            insert_relative_target(patch_pc, dst_stub, NOT_HOT_PATCHABLE);
        /* Must remove incoming if one mergee had a cross link to the other */
        if ((dst_body != NULL ||
             /* If secondary merger was smaller and had a stub for the same target,
              * we need to remove our outgoing since secondary added a new one.
              * We want to do this only the 1st time we get here, and not if the
              * primary merger added the stub, so we have the primary unlink
              * the old stub (in else code below).
              */
             replace_outgoing) &&
            entrance_stub_linked(old_stub, freeze_info->src_info)) {
            coarse_remove_outgoing(dcontext, old_stub, freeze_info->src_info);
        }
    } else {
        /* Copy stub */
        cache_pc stub_pc = freeze_info->stubs_cur_pc;
        ASSERT(dst_body == NULL || trace_head);
        LOG(THREAD, LOG_FRAGMENT, 4, "\ttarget " PFX " is %s, adding stub @" PFX "\n",
            old_stub_tgt, trace_head ? "trace head" : "not present", stub_pc);
        transfer_coarse_stub(dcontext, freeze_info, old_stub, trace_head,
                             replace_outgoing);
        if (replace_outgoing) {
            /* Signal to later stubs that they don't need to remove the outgoing
             * entry (as opposed to new stubs added by the secondary merger,
             * for which we do need to remove).
             * Assumption: if replace_outgoing then it's ok to unlink the old stub
             * since it's going away anyway.
             */
            unlink_entrance_stub(dcontext, old_stub, trace_head ? FRAG_IS_TRACE_HEAD : 0,
                                 freeze_info->src_info);
        }
        ASSERT(freeze_info->stubs_cur_pc ==
               stub_pc + coarse_stub_alignment(freeze_info->src_info));
        fragment_coarse_th_add(dcontext, freeze_info->dst_info, old_stub_tgt,
                               stub_pc -
                                   (ptr_uint_t)freeze_info->dst_info->stubs_start_pc);
        if (dst_cache_pc != NULL)
            insert_relative_target(patch_pc, stub_pc, NOT_HOT_PATCHABLE);
    }
}

/* Assumption: cache has already been copied from src to dst.
 * This routine walks the copied cache to find inter-unit links; it
 * directly links them, eliminating their entrance stubs.
 * replace_outgoing really only applies if in_place: should we replace
 *   outgoing link incoming entries (true), or add new ones (false)
 */
static void
coarse_merge_update_jmps(dcontext_t *dcontext, coarse_freeze_info_t *freeze_info,
                         bool replace_outgoing)
{
    /* Plan: cache has already been copied from src to dst, but we need to do
     * inter-unit links.  So we decode from the original cache to find the
     * target stubs: if a target is present in dst, we do not copy the stub and
     * we directly link; if not present, we copy the stub and re-relativize the
     * jmp to the stub.  We must also patch up indirect and direct stub jmps
     * to prefixes.
     */
    cache_pc pc = freeze_info->src_info->cache_start_pc;
    cache_pc next_pc = pc;
    cache_pc stop_pc = freeze_info->src_info->cache_end_pc;
    app_pc tgt;
    uint sz;
    /* FIXME: share code w/ decode_fragment() and transfer_coarse_fragment() */
    instr_t *instr;
    /* Since mucking with caches, though if thread-private not necessary */
    ASSERT(dynamo_all_threads_synched);
    ASSERT(freeze_info->src_info->frozen);
    LOG(THREAD, LOG_FRAGMENT, 4, "coarse_merge_update_jmps %s " PFX " => " PFX "\n",
        freeze_info->src_info->module, pc, freeze_info->cache_start_pc);
    instr = instr_create(dcontext);
    while (next_pc < stop_pc) {
        instr_reset(dcontext, instr);
        pc = next_pc;
        next_pc = decode_cti(dcontext, pc, instr);
        /* Case 8711: we can't distinguish exit ctis from others,
         * so we must assume that any cti is an exit cti.
         */
        /* We don't care about fragment boundaries so we can ignore elision.
         * We only care about jmps to stubs.
         */
        if (instr_opcode_valid(instr) && instr_is_cti(instr)) {
            /* Ensure we get proper target for short cti sequence */
            if (instr_is_cti_short_rewrite(instr, pc))
                next_pc = remangle_short_rewrite(dcontext, instr, pc, 0 /*same target*/);
            tgt = opnd_get_pc(instr_get_target(instr));
            if (in_coarse_stub_prefixes(tgt)) {
                /* We should not encounter prefix targets other than indirect while
                 * in the body of the cache (rest are from the stubs) */
                ASSERT(coarse_is_indirect_stub(
                    next_pc - coarse_indirect_stub_size(freeze_info->src_info)));
                /* indirect exit stub: need to update jmp to prefix */
                ASSERT(instr_is_ubr(instr));
                sz = JMP_LONG_LENGTH /*ubr to stub*/ - 4;
                pc += sz;
                tgt = PC_RELATIVE_TARGET(pc);
                tgt = redirect_to_tgt_ibl_prefix(dcontext, freeze_info, tgt);
                ASSERT(dynamo_all_threads_synched); /* thus NOT_HOT_PATCHABLE */
                insert_relative_target(freeze_info->cache_start_pc +
                                           (pc - freeze_info->src_info->cache_start_pc),
                                       tgt, NOT_HOT_PATCHABLE);
                next_pc = pc + 4;
            } else if (tgt < freeze_info->src_info->cache_start_pc || tgt >= stop_pc) {
                /* Must go through a stub */
                cache_pc dst_cache_pc = freeze_info->cache_start_pc +
                    (pc - freeze_info->src_info->cache_start_pc);
                ASSERT(tgt >= freeze_info->src_info->stubs_start_pc &&
                       tgt < freeze_info->src_info->stubs_end_pc);
                if (instr_is_cbr(instr)) {
                    uint cbr_len;
                    if (instr_is_cti_short_rewrite(instr, pc))
                        cbr_len = CBR_SHORT_REWRITE_LENGTH;
                    else
                        cbr_len = CBR_LONG_LENGTH;
                    ASSERT(pc + cbr_len == next_pc);
                    coarse_merge_process_stub(dcontext, freeze_info, tgt, cbr_len,
                                              dst_cache_pc, replace_outgoing);
                    /* If there is a ubr next (could be elided) we just hit
                     * it next time around loop */
                } else {
                    ASSERT(instr_is_ubr(instr));
                    ASSERT(pc + JMP_LONG_LENGTH == next_pc);
                    coarse_merge_process_stub(dcontext, freeze_info, tgt, JMP_LONG_LENGTH,
                                              dst_cache_pc, replace_outgoing);
                }
            } else {
                /* intra-cache target */
                /* I would assert that a pclookup finds an entry but that hits
                 * a recursive lock on non-recursive freeze_info->src_info->lock */
            }
        }
    }
    instr_destroy(dcontext, instr);

    /* Do the loop even w/o traces in debug for the assert */
    if (!DYNAMO_OPTION(disable_traces) IF_DEBUG(|| true)) {
        /* We can have trace heads with no intra-unit targeters (secondary trace
         * heads!) so we must also walk the stubs.  Rather than require an
         * iterator or helper routine in fragment or link we directly
         * walk here. */
        for (pc = freeze_info->src_info->stubs_start_pc;
             pc < freeze_info->src_info->stubs_end_pc;
             pc += coarse_stub_alignment(freeze_info->src_info)) {
            if (in_coarse_stub_prefixes(pc))
                continue;
            ASSERT(coarse_is_entrance_stub(pc));
            if (entrance_stub_linked(pc, freeze_info->src_info)) {
                cache_pc src_body;
                /* for non-in-place merging we don't unlink stubs targeting
                 * the other mergee, so we must rule that out here.
                 * the only internally-untargeted stubs we need to add are
                 * those for our own bodies. */
                fragment_coarse_lookup_in_unit(
                    dcontext, freeze_info->src_info,
                    entrance_stub_target_tag(pc, freeze_info->src_info), NULL, &src_body);
                if (src_body != NULL) {
                    ASSERT(!DYNAMO_OPTION(disable_traces));
                    coarse_merge_process_stub(dcontext, freeze_info, pc, 0, NULL,
                                              replace_outgoing);
                }
            }
        }
    }
}

/* Assumption: cache to be merged with has already been copied to dst.
 * This routine walks the other src and copies over non-dup fragments,
 * directly linking inter-unit links along the way.
 * replace_outgoing really only applies if in_place: should we replace
 *   outgoing link incoming entries (true), or add new ones (false)
 */
static void
coarse_merge_without_dups(dcontext_t *dcontext, coarse_freeze_info_t *freeze_info,
                          ssize_t cache_offs, bool replace_outgoing)
{
    /* Plan: we need to append the non-dup portions of src to the already-copied
     * other source, as well as fixing up inter-unit links: if a target is present in
     * dst, we do not copy the stub and we directly link; if not present, we copy the
     * stub and re-relativize the jmp to the stub.  We must also patch up indirect
     * and direct stub jmps to prefixes.
     * Though we need to know fragment boundaries, note that walking the htable
     * instead of the cache doesn't buy us much: due to elision we still have to do
     * pclookups, so we go ahead and walk the cache as it is already laid out.
     */
    cache_pc pc = freeze_info->src_info->cache_start_pc;
    cache_pc src_body, next_pc = pc, fallthrough_body = NULL;
    cache_pc dst_body = NULL, last_dst_body;
    cache_pc stop_pc = freeze_info->src_info->cache_end_pc;
    app_pc tag, fallthrough_tag = NULL, tgt = NULL;
    /* FIXME: share code w/ decode_fragment() and transfer_coarse_fragment() */
    instr_t *instr;
    /* stored targets for fixup */
    jmp_tgt_list_t *jmp_list = NULL;
    bool intra_fragment = false;
    /* Since mucking with caches, though if thread-private not necessary */
    ASSERT(dynamo_all_threads_synched);
    ASSERT(freeze_info->src_info->frozen);
    LOG(THREAD, LOG_FRAGMENT, 4, "coarse_merge_without_dups %s " PFX " => " PFX "\n",
        freeze_info->src_info->module, pc, freeze_info->cache_cur_pc);
    instr = instr_create(dcontext);
    while (next_pc < stop_pc) {
        last_dst_body = dst_body;
        if (fallthrough_tag != NULL) {
            /* still at dup fallthrough pc */
            ASSERT(fragment_coarse_entry_pclookup(dcontext, freeze_info->src_info,
                                                  next_pc) == fallthrough_tag);
            tag = fallthrough_tag;
            ASSERT(fallthrough_body != NULL);
            dst_body = fallthrough_body;
            /* do not go again through the fallthrough code below */
            instr_reset(dcontext, instr);
        } else {
            tag =
                fragment_coarse_entry_pclookup(dcontext, freeze_info->src_info, next_pc);
        }
        /* We come back through the loop for fallthrough jmp of cbr */
        ASSERT(tag != NULL || (instr_opcode_valid(instr) && instr_is_cbr(instr)));
        if (tag != NULL && tag != fallthrough_tag) {
            LOG(THREAD, LOG_FRAGMENT, 4, "\tfragment entry point " PFX " = tag " PFX,
                next_pc, tag);
            fragment_coarse_lookup_in_unit(dcontext, freeze_info->dst_info, tag, NULL,
                                           &dst_body);
            if (dst_body == NULL) {
                cache_pc src_stub = NULL;
                fragment_coarse_add(dcontext, freeze_info->dst_info, tag,
                                    freeze_info->cache_cur_pc -
                                        (ptr_uint_t)freeze_info->cache_start_pc +
                                        cache_offs);
                LOG(THREAD, LOG_FRAGMENT, 4, " (new => " PFX ")\n",
                    freeze_info->cache_cur_pc);
                /* this may be a trace head, in which case we need to add its stub
                 * now in case there are no intra-unit targeters of it (which
                 * means it is probably a secondary trace head) */
                fragment_coarse_lookup_in_unit(dcontext, freeze_info->src_info, tag,
                                               &src_stub, NULL);
                if (src_stub != NULL) {
                    ASSERT(!DYNAMO_OPTION(disable_traces));
                    coarse_merge_process_stub(dcontext, freeze_info, src_stub, 0, NULL,
                                              replace_outgoing);
                }
            } else { /* dup */
                LOG(THREAD, LOG_FRAGMENT, 4, " (duplicate)\n");
                /* if prev is cbr, this is a fall-through, which is handled below */
            }
        } /* else carry through dst_body from last iter */
        src_body = next_pc;
        fallthrough_tag = NULL;
        fallthrough_body = NULL;
        do {
            ASSERT(next_pc < stop_pc);
            if (next_pc >= stop_pc)
                return; /* paranoid: avoid infinite loop */
            pc = next_pc;
            if (!intra_fragment &&
                (next_pc != src_body ||
                 /* fall-through of cbr will be looked up pre-1st iter above */
                 (instr_opcode_valid(instr) && instr_is_cbr(instr)))) {
                /* We assume at least one instr in each fragment, to avoid ambiguity */
                ASSERT_NOT_IMPLEMENTED(!DYNAMO_OPTION(unsafe_freeze_elide_sole_ubr));
                if (next_pc == src_body) {
                    fallthrough_tag = tag;
                    fallthrough_body = dst_body;
                } else {
                    fallthrough_tag = fragment_coarse_entry_pclookup(
                        dcontext, freeze_info->src_info, next_pc);
                    if (fallthrough_tag != NULL) {
                        fragment_coarse_lookup_in_unit(dcontext, freeze_info->dst_info,
                                                       fallthrough_tag, NULL,
                                                       &fallthrough_body);
                    }
                }
                if (fallthrough_tag != NULL) {
                    /* We'd rather keep fall-through elision if we can */
                    LOG(THREAD, LOG_FRAGMENT, 4, "\tfall-through tag " PFX " @" PFX,
                        fallthrough_tag, next_pc);
                    if (fallthrough_body == NULL) {
                        /* Just keep going and process the fall-through's cti */
                        LOG(THREAD, LOG_FRAGMENT, 4, " (new => " PFX ")\n",
                            freeze_info->cache_cur_pc + (next_pc - src_body));
                        if (dst_body != NULL) { /* prev is a dup */
                            ASSERT_NOT_TESTED();
                            src_body = next_pc;
                            tag = fallthrough_tag;
                        }
                        if (fallthrough_tag != tag) {
                            fragment_coarse_add(
                                dcontext, freeze_info->dst_info, fallthrough_tag,
                                freeze_info->cache_cur_pc + (next_pc - src_body) -
                                    (ptr_uint_t)freeze_info->cache_start_pc + cache_offs);
                            DOCHECK(1, {
                                /* We should NOT need to add a stub like we might
                                 * for the entry point add above: fall-through
                                 * cannot be trace head! */
                                cache_pc src_stub = NULL;
                                fragment_coarse_lookup_in_unit(
                                    dcontext, freeze_info->src_info, fallthrough_tag,
                                    &src_stub, NULL);
                                ASSERT(src_stub == NULL);
                            });
                        }
                        fallthrough_tag = NULL;
                    } else {
                        LOG(THREAD, LOG_FRAGMENT, 4, " (duplicate)\n");
                        break;
                    }
                }
            }
            instr_reset(dcontext, instr);
            next_pc = decode_cti(dcontext, pc, instr);
            ASSERT(next_pc - src_body <= MAX_FRAGMENT_SIZE);
            /* Case 8711: we can't distinguish exit ctis from others,
             * so we must assume that any cti is an exit cti.
             * Assumption: coarse-grain bbs have 1 ind exit or 2 direct,
             * and no code beyond the last exit!
             */
            intra_fragment = false;
            if (instr_opcode_valid(instr) && instr_is_cti(instr)) {
                if (instr_is_cti_short_rewrite(instr, pc)) {
                    /* Pull in the two short jmps for a "short-rewrite" instr.
                     * We must do this before asking whether it's an
                     * intra-fragment so we don't just look at the
                     * first part of the sequence.
                     */
                    next_pc =
                        remangle_short_rewrite(dcontext, instr, pc, 0 /*same target*/);
                }
                if (coarse_cti_is_intra_fragment(dcontext, freeze_info->src_info, instr,
                                                 src_body))
                    intra_fragment = true;
            }
        } while (!instr_opcode_valid(instr) || !instr_is_cti(instr) || intra_fragment);

        if (dst_body == NULL) { /* not a dup */
            /* copy body of fragment, including cti (if not ending @ fall-through) */
            size_t sz = next_pc - src_body;
            memcpy(freeze_info->cache_cur_pc, src_body, sz);
            freeze_info->cache_cur_pc += sz;
        }

        if (fallthrough_tag != NULL) {
            ASSERT(next_pc == pc); /* should have short-circuited */
            /* add intra-cache jmp if elided but fall-through a dup */
            ASSERT(fallthrough_body != NULL);
            /* If start bb not a dup, or post-cbr, must un-elide */
            if (dst_body == NULL || (next_pc == src_body && last_dst_body == NULL)) {
                LOG(THREAD, LOG_FRAGMENT, 4,
                    "\tadding jmp @" PFX " to " PFX " for fall-through tag " PFX "\n",
                    freeze_info->cache_cur_pc, fallthrough_body, fallthrough_tag);
                freeze_info->cache_cur_pc = insert_relative_jump(
                    freeze_info->cache_cur_pc, fallthrough_body, NOT_HOT_PATCHABLE);
            }
        } else {
            ASSERT(instr_opcode_valid(instr) && instr_is_cti(instr));
            /* We already remangled if a short-rewrite so no extra work here */
            tgt = opnd_get_pc(instr_get_target(instr));
            if (in_coarse_stub_prefixes(tgt)) {
                /* We should not encounter prefix targets other than indirect while
                 * in the body of the cache (rest are from the stubs) */
                ASSERT(coarse_is_indirect_stub(
                    next_pc - coarse_indirect_stub_size(freeze_info->src_info)));
                /* indirect exit stub: need to update jmp to prefix */
                ASSERT(instr_is_ubr(instr));
                if (dst_body == NULL) { /* not a dup */
                    tgt = PC_RELATIVE_TARGET(next_pc - 4);
                    tgt = redirect_to_tgt_ibl_prefix(dcontext, freeze_info, tgt);
                    ASSERT(dynamo_all_threads_synched); /* thus NOT_HOT_PATCHABLE */
                    /* we've already copied the stub as part of the body */
                    ASSERT(coarse_is_indirect_stub(
                        freeze_info->cache_cur_pc -
                        coarse_indirect_stub_size(freeze_info->src_info)));
                    freeze_info->cache_cur_pc -= 4;
                    freeze_info->cache_cur_pc = insert_relative_target(
                        freeze_info->cache_cur_pc, tgt, NOT_HOT_PATCHABLE);
                }
            } else if (tgt < freeze_info->src_info->cache_start_pc || tgt >= stop_pc) {
                if (dst_body == NULL) { /* not a dup */
                    /* currently goes through a stub */
                    ASSERT(tgt >= freeze_info->src_info->stubs_start_pc &&
                           tgt < freeze_info->src_info->stubs_end_pc);
                    if (instr_is_cbr(instr)) {
                        uint cbr_len;
                        if (instr_is_cti_short_rewrite(instr, pc))
                            cbr_len = CBR_SHORT_REWRITE_LENGTH;
                        else
                            cbr_len = CBR_LONG_LENGTH;
                        ASSERT(pc + cbr_len == next_pc);
                        coarse_merge_process_stub(dcontext, freeze_info, tgt, cbr_len,
                                                  freeze_info->cache_cur_pc - cbr_len,
                                                  replace_outgoing);
                        /* If there is a ubr next (could be elided) we just hit
                         * it next time around loop */
                    } else {
                        ASSERT(instr_is_ubr(instr));
                        ASSERT(pc + JMP_LONG_LENGTH == next_pc);
                        coarse_merge_process_stub(
                            dcontext, freeze_info, tgt, JMP_LONG_LENGTH,
                            freeze_info->cache_cur_pc - JMP_LONG_LENGTH,
                            replace_outgoing);
                    }
                }
            } else if (dst_body == NULL) { /* not a dup */
                /* Intra-cache target, but we're moving things around and have to do
                 * a separate pass since don't know future locations.  Since the
                 * layout is changing and later we'd need multiple lookups to find
                 * the corrsepondence between src and dst, we store the target tag in
                 * the jmp and replace it w/ the body in the later pass.
                 * We can't fit a 64-bit target, so we use offs from mod base.
                 * XXX: split pcaches up if app module is over 4GB.
                 */
                jmp_tgt_list_t *entry;
                app_pc tgt_tag =
                    fragment_coarse_entry_pclookup(dcontext, freeze_info->src_info, tgt);
                ASSERT(tgt_tag != NULL);
                LOG(THREAD, LOG_FRAGMENT, 4,
                    "\tintra-cache src " PFX "->" PFX " tag " PFX " dst pre-" PFX "\n",
                    pc, tgt, tgt_tag, freeze_info->cache_cur_pc);
                entry =
                    HEAP_TYPE_ALLOC(dcontext, jmp_tgt_list_t, ACCT_VMAREAS, PROTECTED);
                entry->tag = tgt_tag;
                entry->jmp_end_pc = freeze_info->cache_cur_pc;
                entry->next = jmp_list;
                jmp_list = entry;
            }
        }
    }

    /* Second pass to update intra-cache targets.
     * FIXME: combine w/ later coarse_unit_shift_jmps()
     */
    while (jmp_list != NULL) {
        jmp_tgt_list_t *next = jmp_list->next;
        fragment_coarse_lookup_in_unit(dcontext, freeze_info->dst_info, jmp_list->tag,
                                       NULL, &dst_body);
        ASSERT(dst_body != NULL);
        LOG(THREAD, LOG_FRAGMENT, 4, "\tintra-cache dst -" PFX "->" PFX " tag " PFX "\n",
            jmp_list->jmp_end_pc, dst_body, tgt); /* tgt always set here */
        /* FIXME: make 4 a named constant; used elsewhere as well */
        insert_relative_target(jmp_list->jmp_end_pc - 4, dst_body, NOT_HOT_PATCHABLE);
        HEAP_TYPE_FREE(dcontext, jmp_list, jmp_tgt_list_t, ACCT_VMAREAS, PROTECTED);
        jmp_list = next;
    }

    instr_destroy(dcontext, instr);
}

/* Returns a new coarse_info_t (or if in_place returns info1) that combines
 * info1 and info2.  In in_place, info1 is replaced with the result and returned;
 * else, a separate coarse_info_t is created and returned.
 * If either of the units is live, then info1 must be live.
 * If one of the two units covers a different code range, it must be info2,
 * and it must be a subset of info1's range.
 * If returns NULL, the merge failed; if in_place, info1 is unchanged on failure.
 * If in_place, caller is responsible for flushing the ibl tables (case 11057).
 */
coarse_info_t *
coarse_unit_merge(dcontext_t *dcontext, coarse_info_t *info1, coarse_info_t *info2,
                  bool in_place)
{
    coarse_info_t *merged;
    coarse_info_t *res = NULL;
    coarse_info_t *src_sm, *src_lg;
    size_t cache1_size, cachelg_size, cache2_size, merged_cache_size;
    size_t stubs1_size, stubs2_size;
    coarse_freeze_info_t freeze_info;

    LOG(THREAD, LOG_CACHE, 2, "coarse_unit_merge %s %s with %s\n", info1->module,
        info1->persisted ? "persisted" : "non-persisted",
        info2->persisted ? "persisted" : "non-persisted");
    STATS_INC(coarse_units_merged);

    ASSERT(info1 != NULL && info2 != NULL);
    ASSERT(info1->base_pc <= info2->base_pc && info1->end_pc >= info2->end_pc);
    if (info1->base_pc > info2->base_pc || info1->end_pc < info2->end_pc)
        return NULL;
    /* Currently we only do online merging where one unit is live */
    ASSERT(!info1->persisted || !info2->persisted);

    /* Much more efficient to merge smaller cache into larger */
    if (fragment_coarse_num_entries(info1) > fragment_coarse_num_entries(info2)) {
        src_lg = info1;
        src_sm = info2;
    } else {
        src_lg = info2;
        src_sm = info1;
    }

    /* Ensure the pclookup table is set up for src_sm, to avoid recursive
     * lock issues
     */
    if (src_sm->pclookup_htable == NULL) { /* read needs no lock */
        fragment_coarse_entry_pclookup(dcontext, src_sm, NULL);
        ASSERT(src_sm->pclookup_htable != NULL);
    }

    acquire_recursive_lock(&change_linking_lock);
#ifdef HOT_PATCHING_INTERFACE
    /* we may call coarse_unit_calculate_persist_info() */
    if (DYNAMO_OPTION(hot_patching))
        d_r_read_lock(hotp_get_lock());
#endif
    /* We can't grab both locks due to deadlock potential.  Currently we are
     * always fully synched, so we rely on that to synch with info2.
     */
    ASSERT(dynamo_all_threads_synched);
    d_r_mutex_lock(&info1->lock);
    ASSERT(info1->cache != NULL && info2->cache != NULL); /* don't merge empty units */
    ASSERT(info1->frozen && info2->frozen);

    /* Tasks:
     * 1) Merge the caches, eliminating duplicates.  Various optimizations to
     *    preserve fall-through pairs if 2nd part is in other cache but 1st is
     *    not could be employed.
     * 2) Turn cross-links into direct links and eliminate those entrance stubs.
     * 3) Copy the rest of the entrance stubs.
     * 4) Merge the RCT, RAC, and hotp fields, which may require calculating
     *    persist info if this unit has not been persisted; if neither unit
     *    has been persisted we do not need to do anything here.
     *
     * We will remove the extra post-stubs space since when persisting
     * we only write up through stubs_end_pc and not the allocation end.
     */
    /* Whether merging in-place or not, we create a new coarse_info_t.
     * If in-place we delete the old one afterward.
     */
    merged = coarse_unit_create(info1->base_pc, info1->end_pc, &info1->module_md5,
                                in_place && info1->in_use);
    merged->frozen = true;
    cache1_size = info1->cache_end_pc - info1->cache_start_pc;
    cache2_size = info2->cache_end_pc - info2->cache_start_pc;
    /* We shrink the cache to size below, after merging & removing dups */
    merged_cache_size = cache1_size + cache2_size;
    /* We need the stubs to start on a new page since will be +rw vs cache +r */
    merged_cache_size = ALIGN_FORWARD(merged_cache_size, PAGE_SIZE);
    /* We only need one set of prefixes */
    stubs1_size = info1->stubs_end_pc - info1->fcache_return_prefix;
    stubs2_size = info2->stubs_end_pc - info2->stubs_start_pc;
    merged->mmap_size = merged_cache_size + stubs1_size + stubs2_size;
    /* Our relative jmps require that we do not exceed 32-bit reachability */
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_int(merged->mmap_size)));
    merged->cache_start_pc = (cache_pc)heap_mmap(
        merged->mmap_size, MEMPROT_EXEC | MEMPROT_READ | MEMPROT_WRITE,
        VMM_CACHE | VMM_REACHABLE);
    merged->cache_end_pc = merged->cache_start_pc + cache1_size + cache2_size;
    merged->stubs_start_pc = coarse_stubs_create(
        merged, merged->cache_start_pc + merged_cache_size, stubs1_size + stubs2_size);
    /* will be tightened up later */
    merged->stubs_end_pc = merged->cache_start_pc + merged->mmap_size;
    ASSERT(merged->stubs_start_pc != NULL);
    ASSERT(ALIGNED(merged->stubs_start_pc, coarse_stub_alignment(info1)));
    ASSERT(merged->fcache_return_prefix == merged->cache_start_pc + merged_cache_size);
    ASSERT(merged->trace_head_return_prefix ==
           merged->fcache_return_prefix +
               (info1->trace_head_return_prefix - info1->fcache_return_prefix));
    ASSERT(merged->ibl_ret_prefix ==
           merged->fcache_return_prefix +
               (info1->ibl_ret_prefix - info1->fcache_return_prefix));
    ASSERT(merged->ibl_call_prefix ==
           merged->fcache_return_prefix +
               (info1->ibl_call_prefix - info1->fcache_return_prefix));
    ASSERT(merged->ibl_jmp_prefix ==
           merged->fcache_return_prefix +
               (info1->ibl_jmp_prefix - info1->fcache_return_prefix));

    /* Much more efficient to put the larger cache 1st, but we have to be sure
     * to use the same order for both the htable and cache.
     */

    /* Try to size the dst tables to avoid collision asserts.
     * Put the larger unit's entries into the dst table up front.
     * FIXME: do this earlier and if one is a subset of other then do
     * a simpler merge?
     */
    fragment_coarse_htable_merge(dcontext, merged, src_lg, src_sm,
                                 false /*do not add src_sm yet*/,
                                 false /*leave th htable empty*/);

    /* Copy the 1st cache intact, and bring in the non-dup portions of the second
     * (since know the offsets of the 1st); then, walk the 1st and patch up its
     * inter-unit links while decoding from the original, now that the 2nd is in
     * place.
     */
    cachelg_size = (src_lg == info2) ? cache2_size : cache1_size;
    memcpy(merged->cache_start_pc, src_lg->cache_start_pc, cachelg_size);

    memset(&freeze_info, 0, sizeof(freeze_info));
    freeze_info.dst_info = merged;
    freeze_info.stubs_start_pc = merged->stubs_start_pc;
    freeze_info.stubs_cur_pc = merged->stubs_start_pc;
    /* Just like for freezing: leave inter-unit links intact for in_place.
     * coarse_merge_process_stub() assumes that unlink == !in_place
     */
    freeze_info.unlink = !in_place;

    freeze_info.src_info = src_sm;
    freeze_info.cache_start_pc = merged->cache_start_pc + cachelg_size;
    freeze_info.cache_cur_pc = freeze_info.cache_start_pc;
    coarse_merge_without_dups(dcontext, &freeze_info, cachelg_size,
                              /* replace for primary unit; add for secondary */
                              freeze_info.src_info == info1);
    merged->cache_end_pc = freeze_info.cache_cur_pc;

    freeze_info.src_info = src_lg;
    freeze_info.cache_start_pc = merged->cache_start_pc;
    freeze_info.cache_cur_pc = freeze_info.cache_start_pc;
    coarse_merge_update_jmps(dcontext, &freeze_info,
                             /* replace for primary unit; add for secondary */
                             freeze_info.src_info == info1);

    ASSERT((ptr_uint_t)(freeze_info.stubs_cur_pc - merged->fcache_return_prefix) <=
           stubs1_size + stubs2_size);

    /* We have extra space from extra in each stub region (from case 9428),
     * from duplicate prefix space, and from eliminated inter-unit stubs, so
     * we must set end pc.
     */
    coarse_stubs_set_end_pc(merged, freeze_info.stubs_cur_pc);

    LOG(THREAD, LOG_CACHE, 2,
        "merged size: stubs " SZFMT " => " SZFMT " bytes, "
        "cache " SZFMT " (" SZFMT " align) => " SZFMT " (" SZFMT " align) bytes\n",
        stubs1_size + stubs2_size, freeze_info.stubs_cur_pc - merged->stubs_start_pc,
        cache1_size + cache2_size,
        (info1->fcache_return_prefix - info1->cache_start_pc) +
            (info2->fcache_return_prefix - info2->cache_start_pc),
        merged->cache_end_pc - merged->cache_start_pc,
        merged->fcache_return_prefix - merged->cache_start_pc);

    if (merged_cache_size - (merged->cache_end_pc - merged->cache_start_pc) > 0) {
        /* With duplicate elimination we often have a lot of empty space, so we
         * re-allocate into a proper-fitting space
         */
        size_t cachesz = merged->cache_end_pc - merged->cache_start_pc;
        size_t cachesz_aligned = ALIGN_FORWARD(cachesz, PAGE_SIZE);
        size_t stubsz = merged->stubs_end_pc - merged->fcache_return_prefix;
        size_t newsz = cachesz_aligned + stubsz;
        size_t old_mapsz = merged->mmap_size;
        cache_pc newmap =
            (cache_pc)heap_mmap(newsz, MEMPROT_EXEC | MEMPROT_READ | MEMPROT_WRITE,
                                VMM_CACHE | VMM_REACHABLE);
        ssize_t cache_shift = merged->cache_start_pc - newmap;
        /* stubs have moved too, so a relative shift not absolute */
        ssize_t stubs_shift =
            cachesz_aligned - (merged->fcache_return_prefix - merged->cache_start_pc);
        LOG(THREAD, LOG_CACHE, 2,
            "re-allocating merged unit: " SZFMT " @" PFX " " PFX " => " SZFMT " @" PFX
            " " PFX " " SZFMT " " SZFMT "\n",
            merged->mmap_size, merged->cache_start_pc, merged->fcache_return_prefix,
            newsz, newmap, newmap + cachesz_aligned, cache_shift, stubs_shift);
        memcpy(newmap, merged->cache_start_pc, cachesz);
        memcpy(newmap + cachesz_aligned, merged->fcache_return_prefix, stubsz);
        heap_munmap(merged->cache_start_pc, merged->mmap_size, VMM_CACHE | VMM_REACHABLE);
        coarse_stubs_delete(merged);
        merged->mmap_size = newsz;
        /* Our relative jmps require that we do not exceed 32-bit reachability */
        IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_int(merged->mmap_size)));
        merged->cache_start_pc = newmap;
        merged->cache_end_pc = merged->cache_start_pc + cachesz;
        merged->stubs_start_pc =
            coarse_stubs_create(merged, merged->cache_start_pc + cachesz_aligned, stubsz);
        ASSERT(merged->stubs_start_pc != NULL);
        ASSERT(ALIGNED(merged->stubs_start_pc, coarse_stub_alignment(info1)));
        ASSERT(merged->fcache_return_prefix == newmap + cachesz_aligned);
        coarse_stubs_set_end_pc(merged, merged->cache_start_pc + newsz);
        coarse_unit_shift_jmps(dcontext, merged, cache_shift, stubs_shift, old_mapsz);
    }

    /* Set cache bounds after we've potentially moved the initial cache */
    fcache_coarse_init_frozen(dcontext, merged, merged->cache_start_pc,
                              merged->fcache_return_prefix - merged->cache_start_pc);

    /* Currently we only do online merging where at least one unit is live,
     * and we expect that to be info1
     */
    ASSERT(!info1->persisted);
    /* Store the source persisted size so we know whether we need to merge
     * with that on-disk file
     */
    if (info2->persisted)
        merged->persisted_source_mmap_size = info2->mmap_size;

    /* Merge the other fields */
    coarse_unit_merge_persist_info(dcontext, merged, info1, info2);

    DOLOG(5, LOG_CACHE, {
        byte *pc = merged->cache_start_pc;
        LOG(THREAD, LOG_CACHE, 1, "merged cache:\n");
        do {
            pc = disassemble_with_bytes(dcontext, pc, THREAD);
        } while (pc < merged->cache_end_pc);
        pc = merged->stubs_start_pc;
        LOG(THREAD, LOG_CACHE, 1, "merged stubs:\n");
        do {
            if (((ptr_uint_t)pc) % coarse_stub_alignment(info1) ==
                coarse_stub_alignment(info1) - 1)
                pc++;
            pc = disassemble_with_bytes(dcontext, pc, THREAD);
        } while (pc < merged->stubs_end_pc);
    });

    /* FIXME case 9687: mark cache as read-only */

    if (in_place) {
        coarse_incoming_t *e;
        coarse_replace_unit(dcontext, info1, merged);
        merged = NULL;
        /* up to caller to call mark_executable_area_coarse_frozen() if necessary */

        /* case 10877: must combine the incoming lists
         * targets should be unique, so can just append
         */
        d_r_mutex_lock(&info1->incoming_lock);
        /* can't grab info2 lock, so just like for main lock we rely on synchall */
        DODEBUG({
            /* Make sure no inter-incoming left */
            uint in1 = 0;
            uint in2 = 0;
            for (e = info1->incoming; e != NULL; e = e->next, in1++)
                ASSERT(!e->coarse || get_stub_coarse_info(e->in.stub_pc) != info2);
            for (e = info2->incoming; e != NULL; e = e->next, in2++)
                ASSERT(!e->coarse || get_stub_coarse_info(e->in.stub_pc) != info1);
            LOG(THREAD, LOG_CACHE, 1, "merging %d incoming into %d incoming\n", in2, in1);
        });
        e = info1->incoming;
        if (e == NULL) {
            info1->incoming = info2->incoming;
        } else {
            while (e->next != NULL)
                e = e->next;
            e->next = info2->incoming;
        }
        d_r_mutex_unlock(&info1->incoming_lock);
        info2->incoming = NULL; /* ensure not freed when info2 is freed */
        coarse_unit_shift_links(dcontext, info1);

        res = info1;
    } else {
        /* we made separate copy that has no outgoing or incoming links */
        res = merged;
    }
    d_r_mutex_unlock(&info1->lock);
#ifdef HOT_PATCHING_INTERFACE
    /* we may call coarse_unit_calculate_persist_info() */
    if (DYNAMO_OPTION(hot_patching))
        d_r_read_unlock(hotp_get_lock());
#endif
    release_recursive_lock(&change_linking_lock);
    return res;
}

/***************************************************************************
 * PERSISTENT CODE CACHE
 */

#if defined(RETURN_AFTER_CALL) && defined(WINDOWS)
extern bool seen_Borland_SEH;
#endif

/* get global or per-user directory name */
bool
perscache_dirname(char *directory /* OUT */, uint directory_len)
{
    int retval;
    bool param_ok = false;
    /* Support specifying the pcache dir from either a config param (historical
     * from ASLR piggyback) or runtime option, though config param gets precedence.
     */
    const char *param_name = DYNAMO_OPTION(persist_per_user)
        ? PARAM_STR(DYNAMORIO_VAR_PERSCACHE_ROOT)
        : PARAM_STR(DYNAMORIO_VAR_PERSCACHE_SHARED);
    retval = d_r_get_parameter(param_name, directory, directory_len);
    if (IS_GET_PARAMETER_FAILURE(retval)) {
        string_option_read_lock();
        if (DYNAMO_OPTION(persist_per_user) && !IS_STRING_OPTION_EMPTY(persist_dir)) {
            strncpy(directory, DYNAMO_OPTION(persist_dir), directory_len);
            param_ok = true;
        } else if (!IS_STRING_OPTION_EMPTY(persist_shared_dir)) {
            strncpy(directory, DYNAMO_OPTION(persist_shared_dir), directory_len);
            param_ok = true;
        } else {
            /* use log dir by default
             * XXX: create subdir "logs/cache"?  default is per-user so currently
             * user dirs will be in logs/ which seems sufficient.
             */
            uint len = directory_len;
            create_log_dir(BASE_DIR);
            if (get_log_dir(BASE_DIR, directory, &len) && len <= directory_len)
                param_ok = true;
        }
        string_option_read_unlock();
    } else
        param_ok = true;
    if (param_ok)
        directory[directory_len - 1] = '\0';
    return param_ok;
}

/* get global or per-user directory name */
static bool
get_persist_dir(char *directory /* OUT */, uint directory_len, bool create)
{
    if (!perscache_dirname(directory, directory_len) ||
        double_strchr(directory, DIRSEP, ALT_DIRSEP) == NULL) {
        SYSLOG_INTERNAL_ERROR_ONCE("Persistent cache root dir is invalid. "
                                   "Persistent cache will not operate.");
        return false;
    }

    if (DYNAMO_OPTION(persist_per_user)) {
        bool res = os_current_user_directory(directory, directory_len, create);
        /* null terminated */
        if (!res) {
            /* directory name may be set even on failure */
            LOG(THREAD_GET, LOG_CACHE, 2, "\terror opening per-user dir %s\n", directory);
            return false;
        }
    }

    return true;
}

/* Checks for enough space on the volume where persisted caches are stored */
bool
coarse_unit_check_persist_space(file_t fd_in /*OPTIONAL*/, size_t size_needed)
{
    bool room = false;
    file_t fd = fd_in;
    if (fd == INVALID_FILE) {
        /* Use directory to get handle on proper volume */
        char dir[MAXIMUM_PATH];
        if (get_persist_dir(dir, BUFFER_SIZE_ELEMENTS(dir),
                            true /* note we MUST always create directory
                                  * even if never persisting */
                            )) {
            fd = os_open_directory(dir, 0);
        } else
            LOG(THREAD_GET, LOG_CACHE, 2, "\terror finding persist dir\n");
    }
    if (fd != INVALID_FILE) {
        room = check_low_disk_threshold(fd, (uint64)size_needed);
        if (fd_in == INVALID_FILE) {
            /* FIXME: cache the handle, combine with -validate_owner_dir */
            os_close(fd);
        }
    } else
        LOG(THREAD_GET, LOG_CACHE, 2, "\terror opening persist dir\n");
    return room;
}

/* If force_local, pretends module at pc has been exempted (so no effect
 * unless -persist_check_exempted_options)
 */
static inline op_pcache_t
persist_get_options_level(app_pc pc, coarse_info_t *info, bool force_local)
{
    if (!DYNAMO_OPTION(persist_check_options))
        return OP_PCACHE_NOP;
    else if (DYNAMO_OPTION(persist_check_local_options) ||
             (DYNAMO_OPTION(persist_check_exempted_options) &&
              (force_local ||
               /* once loaded as local, must remain local, even if this
                * process never hit the exemption */
               (info != NULL && TEST(PERSCACHE_EXEMPTION_OPTIONS, info->flags)) ||
               os_module_get_flag(pc, MODULE_WAS_EXEMPTED)) &&
              /* don't use local if no such options: else when load will think
               * local when really global */
              has_pcache_dynamo_options(&dynamo_options, OP_PCACHE_LOCAL)))
        return OP_PCACHE_LOCAL;
    else
        return OP_PCACHE_GLOBAL;
}

static const char *
persist_get_relevant_options(dcontext_t *dcontext, char *option_buf, uint buf_len,
                             op_pcache_t level)
{
    if (level == OP_PCACHE_NOP)
        return "";
    get_pcache_dynamo_options_string(&dynamo_options, option_buf, buf_len, level);
    option_buf[buf_len - 1] = '\0';
    LOG(THREAD, LOG_CACHE, 2, "Pcache-affecting options = %s\n", option_buf);
    return option_buf;
}

/* We identify persisted caches by mapping module info into a canonical name.
 * There can be collisions (including for sub-module coarse units, such as
 * separate +x module sections (xref case 9834 and case 9653); in addition, we
 * can have different modules map to the same name), so caller must further
 * verify matches.
 * (Note that we use a different scheme than aslr's calculate_publish_name()
 * as we are not dealing with file handles here but in-memory module images).
 */
static bool
get_persist_filename(char *filename /*OUT*/, uint filename_max /* max #chars */,
                     app_pc modbase, bool write, persisted_module_info_t *modinfo,
                     const char *option_string)
{
    uint checksum, timestamp;
    size_t size, code_size;
    uint64 file_version;
    const char *name;
    uint hash;
    char dir[MAXIMUM_PATH];

    os_get_module_info_lock();
    if (!os_get_module_info(modbase, &checksum, &timestamp, &size, &name, &code_size,
                            &file_version)) {
        os_get_module_info_unlock();
        return false;
    }
    if (name == NULL) {
        /* theoretically possible but pathological, unless we came in late */
        ASSERT_CURIOSITY(IF_WINDOWS_ELSE_0(!dr_early_injected));
        LOG(GLOBAL, LOG_CACHE, 1, "\tmodule " PFX " has no name\n", modbase);
        os_get_module_info_unlock();
        return false;
    }
    /* Should not have path chars in the name */
    ASSERT(get_short_name(name) == name && name[0] != DIRSEP);
    name = get_short_name(name); /* paranoid */

    /* Exclude list applies to both read and write */
    if (!IS_STRING_OPTION_EMPTY(persist_exclude_list)) {
        bool exclude;
        string_option_read_lock();
        exclude = check_filter(DYNAMO_OPTION(persist_exclude_list), name);
        string_option_read_unlock();
        if (exclude) {
            LOG(GLOBAL, LOG_CACHE, 1, "\t%s is on exclude list\n", name);
            DOSTATS({
                if (write)
                    STATS_INC(coarse_units_persist_excluded);
                else
                    STATS_INC(perscache_load_excluded);
            });
            os_get_module_info_unlock();
            return false;
        }
    }

    /* Prepend the perscache dir.  We assume it has already been created.
     * FIXME: cache this, or better, cache the dir handle and use an
     * os_open that can take it in.  Note that the directory handle
     * doesn't help us in Linux - we can neither open files relative to it,
     * nor there is any strong chown guarantee that we depend on.
     */
    if (!get_persist_dir(dir, BUFFER_SIZE_ELEMENTS(dir), write)) {
        os_get_module_info_unlock();
        return false;
    }

    /* FIXME case 8494: version-independent names so we clobber files
     * from old versions of modules and have less need of stale file
     * cleanup?  If so should add in (hash of) full path (w/ volume)
     * to avoid name conflicts?  But if try to share across machines
     * we do not want to include path since can vary.
     */
    /* should we go to a 64-bit hash? */
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(size)));
    hash = checksum ^ timestamp ^ (uint)size;
    /* case 9799: make options part of namespace */
    if (option_string != NULL) {
        uint i;
        ASSERT(DYNAMO_OPTION(persist_check_options));
        for (i = 0; i < strlen(option_string); i++)
            hash ^= option_string[i] << ((i % 4) * 8);
    }
    LOG(GLOBAL, LOG_CACHE, 2, "\thash = 0x%08x^0x%08x^" PFX " ^ %s = " PFX "\n", checksum,
        timestamp, size, option_string == NULL ? "" : option_string, hash);
    ASSERT_CURIOSITY(hash != 0);

    if (DYNAMO_OPTION(persist_per_app)) {
        char *dirend = dir + strlen(dir);
        /* FIXME case 9692: put tls offs instead of "dbg" here, and then
         * sqlservr can have its own set if it ends up w/ separate tls offs
         * (once we have non-per-app persisted files, that is).
         */
        snprintf(dirend, BUFFER_SIZE_ELEMENTS(dir) - (dirend - dir), "%c%s%s", DIRSEP,
                 get_application_short_name(), IF_DEBUG_ELSE("-dbg", ""));
        NULL_TERMINATE_BUFFER(dir);
        LOG(GLOBAL, LOG_CACHE, 2, "\tper-app dir is %s\n", dir);

        /* check for existence first so we can require new during creation */
        if (!os_file_exists(dir, true /*is dir*/) && write) {
            if (!os_create_dir(dir, CREATE_DIR_REQUIRE_NEW)) {
                LOG(GLOBAL, LOG_CACHE, 2, "\terror creating per-app dir %s\n", dir);
                os_get_module_info_unlock();
                return false;
            } else
                LOG(GLOBAL, LOG_CACHE, 2, "\tcreated per-app dir %s\n", dir);
        }
    }
    /* FIXME PR 214088/case 9653: should we put the section ordinal or vmarea range into
     * the name to support simultaneous sub-module files?  If sections are
     * adjacent they'll be one vmarea, so this affects very few dlls.  For now
     * we only support one file per module.  We could also support multiple
     * ranges per file.
     */
    snprintf(filename, filename_max, "%s%c%s%s-0x%08x.%s", dir, DIRSEP, name,
             IF_DEBUG_ELSE("-dbg", ""), hash, PERSCACHE_FILE_SUFFIX);
    filename[filename_max - 1] = '\0';
    os_get_module_info_unlock();
    if (modinfo != NULL) {
        modinfo->base = modbase;
        modinfo->checksum = checksum;
        modinfo->timestamp = timestamp;
        modinfo->image_size = size;
        modinfo->code_size = code_size;
        modinfo->file_version = file_version;
    }
    return true;
}

#if defined(DEBUG) && defined(INTERNAL)
/* FIXME: share w/ aslr.c */
static void
print_module_digest(file_t f, module_digest_t *digest, const char *prefix)
{
    LOG(f, LOG_CACHE, 1, "%s\n  md5 short: ", prefix);
    dump_buffer_as_bytes(f, digest->short_MD5, MD5_RAW_BYTES, DUMP_RAW);
    LOG(f, LOG_CACHE, 1, "\n  md5 long:  ");
    dump_buffer_as_bytes(f, digest->full_MD5, MD5_RAW_BYTES, DUMP_RAW);
    LOG(f, LOG_CACHE, 1, "\n");
}
#endif

static void
persist_calculate_self_digest(module_digest_t *digest, coarse_persisted_info_t *pers,
                              app_pc map, uint validation_option)
{
    struct MD5Context self_md5_cxt;
    if (TEST(PERSCACHE_GENFILE_MD5_COMPLETE, validation_option)) {
        d_r_md5_init(&self_md5_cxt);
        /* Even if generated w/ -persist_map_rw_separate but loaded w/o that
         * option, the md5 should match since the memory layout is the same.
         */
        d_r_md5_update(&self_md5_cxt, map,
                       pers->header_len + pers->data_len - sizeof(persisted_footer_t));
        d_r_md5_final(digest->full_MD5, &self_md5_cxt);
    }
    if (TEST(PERSCACHE_GENFILE_MD5_SHORT, validation_option)) {
        d_r_md5_init(&self_md5_cxt);
        d_r_md5_update(&self_md5_cxt, (byte *)pers, pers->header_len);
        d_r_md5_final(digest->short_MD5, &self_md5_cxt);
    }
}

static void
persist_calculate_module_digest(module_digest_t *digest, app_pc modbase, size_t modsize,
                                app_pc code_start, app_pc code_end,
                                uint validation_option)
{
    size_t view_size = modsize;
    if (TESTANY(PERSCACHE_MODULE_MD5_COMPLETE | PERSCACHE_MODULE_MD5_SHORT,
                validation_option)) {
        /* case 9717: need view size, not image size */
        view_size = os_module_get_view_size(modbase);
    }
    if (TEST(PERSCACHE_MODULE_MD5_COMPLETE, validation_option)) {
        /* We can't use a full md5 from module_calculate_digest() since .data
         * and other sections change between persist and load times (this is
         * in-memory image, not file).  So we do md5 of code region.  If we have
         * hooks at persist time but not at load time we will cry foul;
         * PERSCACHE_MODULE_MD5_AT_LOAD tries to get around this by using
         * the load-time md5 when persisting.
         */
        struct MD5Context code_md5_cxt;
        d_r_md5_init(&code_md5_cxt);
        /* Code range should be within a single memory allocation so it should
         * all be readable.  Xref case 9653.
         */
        code_end = MIN(code_end, modbase + view_size);
        d_r_md5_update(&code_md5_cxt, code_start, code_end - code_start);
        d_r_md5_final(digest->full_MD5, &code_md5_cxt);
    }
    if (TEST(PERSCACHE_MODULE_MD5_SHORT, validation_option)) {
        /* Examine only the image header and the footer (if non-writable)
         * FIXME: if view_size < modsize, better to skip the footer than have it
         * cover a data section?  Should be ok w/ PERSCACHE_MODULE_MD5_AT_LOAD.
         */
        module_calculate_digest(digest, modbase, view_size, false /* not full */,
                                true /* yes short */, DYNAMO_OPTION(persist_short_digest),
                                /* Do not consider non-executable sections.
                                 * We used to include read-only data sections, but
                                 * new ld.so marks those noaccess at +rx mmap time.
                                 * We could try to load pcaches later to solve that;
                                 * for now it's simpler to exclude from the md5.
                                 */
                                OS_IMAGE_EXECUTE, OS_IMAGE_WRITE);
    }
}

/* Compares all but the module base */
static bool
persist_modinfo_cmp(persisted_module_info_t *mi1, persisted_module_info_t *mi2)
{
    bool match = true;
    /* We'd like to know if we have an md5 mismatch */
    ASSERT_CURIOSITY(
        module_digests_equal(
            &mi1->module_md5, &mi2->module_md5,
            TEST(PERSCACHE_MODULE_MD5_SHORT, DYNAMO_OPTION(persist_load_validation)),
            TEST(PERSCACHE_MODULE_MD5_COMPLETE, DYNAMO_OPTION(persist_load_validation)))
        /* relocs => md5 diffs, until we handle relocs wrt md5 */
        IF_WINDOWS(|| mi1->base != mi2->base) ||
        check_filter("win32.partial_map.exe", get_short_name(get_application_name())));
    if (TESTALL(PERSCACHE_MODULE_MD5_SHORT | PERSCACHE_MODULE_MD5_COMPLETE,
                DYNAMO_OPTION(persist_load_validation))) {
        return (memcmp(&mi1->checksum, &mi2->checksum,
                       sizeof(*mi1) - offsetof(persisted_module_info_t, checksum)) == 0);
    }
    match = match &&
        (memcmp(&mi1->checksum, &mi2->checksum,
                offsetof(persisted_module_info_t, module_md5) -
                    offsetof(persisted_module_info_t, checksum)) == 0);
    match =
        match &&
        module_digests_equal(
            &mi1->module_md5, &mi2->module_md5,
            TEST(PERSCACHE_MODULE_MD5_SHORT, DYNAMO_OPTION(persist_load_validation)),
            TEST(PERSCACHE_MODULE_MD5_COMPLETE, DYNAMO_OPTION(persist_load_validation)));
    return match;
}

#ifdef WINDOWS
static void
persist_record_base_mismatch(app_pc modbase)
{
    /* The idea is that we shouldn't waste our time re-persisting modules
     * whose base keeps mismatching due to ASLR (we don't support rebasing
     * pcaches yet).
     * To record whether to not persist, we can't use a VM_ flag b/c
     * no simple way to tell vmareas.c why a load failed so we use a
     * module flag
     */
    if (!DYNAMO_OPTION(coarse_freeze_rebased_aslr) && os_module_has_dynamic_base(modbase))
        os_module_set_flag(modbase, MODULE_DO_NOT_PERSIST);
}
#endif

/* key is meant to be a short string to help identify the purpose of this name.
 * FIXME: right now up to caller to figure out if the name collided w/ an
 * existing file; maybe this routine should do that and return a file handle?
 * FIXME: combine w/ get_unique_logfile, which handles file creation race?
 * As it is this is not mkstemp, and caller must use OS_OPEN_REQUIRE_NEW.
 */
static void
get_unique_name(const char *origname, const char *key, char *filename /*OUT*/,
                uint filename_max /* max #chars */)
{
    /* We need unique names for:
     * 1) case 9696: a temp file to build our pcache in
     * before renaming to the real thing
     * 2) case 9701: to rename the existing file before we replace it, as for
     * images or mmaps with file handles open we must rename before deleting.
     */
    /* FIXME: should we use full 64-bit TSC instead of pseudo-random 32-bit?
     * FIXME: if we make name w/ full path too long we'll truncate:
     * could cache dir handle and use relative name only.
     */
    /* update aslr_get_unique_wide_name() with any improvements here */
    size_t timestamp = get_random_offset(UINT_MAX);
    LOG_DECLARE(int trunc =) /* for DEBUG and INTERNAL */
    snprintf(filename, filename_max, "%s-" PIDFMT "-%010" SZFC "-%s", origname,
             get_process_id(), timestamp, key);
    ASSERT_CURIOSITY(trunc > 0 && trunc < (int)filename_max &&
                     "perscache new name truncated");
    /* FIXME: case 10677 file name truncation */

    filename[filename_max - 1] = '\0';
}

/* Merges a given frozen unit with any new persisted cache file on disk.
 * Caller must hold read lock hotp_get_lock(), if -hot_patching.
 * If merge is successful, returns a new coarse_info_t, which caller is
 * responsible for freeing; else returns NULL.
 */
static coarse_info_t *
coarse_unit_merge_with_disk(dcontext_t *dcontext, coarse_info_t *info,
                            const char *filename)
{
    coarse_info_t *merge_with, *postmerge = NULL;
    uint64 file_size;
    size_t existing_size;
    /* We may have already merged new code with an inuse persisted unit, so we
     * check the stored size of that one if info is not itself persisted.
     * FIXME: we could store the file handle: can we tell if two file handles
     * refer to the same file?
     */
    size_t inuse_size =
        (info->persisted) ? info->mmap_size : info->persisted_source_mmap_size;

    LOG(THREAD, LOG_CACHE, 2, "coarse_unit_merge_with_disk %s\n", info->module);
    ASSERT(dynamo_all_threads_synched);
    ASSERT(info != NULL && info->cache != NULL); /* don't merge empty units */
    ASSERT(info->frozen);
#ifdef HOT_PATCHING_INTERFACE
    ASSERT_OWN_READ_LOCK(DYNAMO_OPTION(hot_patching), hotp_get_lock());
#endif

    /* Strategy: check current pcache file size (not perfect but good enough):
     *   if different from source size, or source was not persisted, then
     *   load in and merge.
     *   FIXME case 10356: need a better check since can have false positive
     *   and false negatives by only looking at size.
     * Could repeat, and could also check again after writing to tmp file but
     * before renaming. FIXME: should we do those things to reduce the race
     * window where we lose another process's appended code?
     */
    if (!os_get_file_size(filename, &file_size)) {
        LOG(THREAD, LOG_CACHE, 2, "  no existing file %s to merge with\n", filename);
        return postmerge;
    }
    ASSERT_TRUNCATE(existing_size, size_t, file_size);
    existing_size = (size_t)file_size;
    LOG(THREAD, LOG_CACHE, 2, "  size of existing %s is " SZFMT " vs our " SZFMT "\n",
        filename, existing_size, inuse_size);
    if (existing_size == 0)
        return postmerge;
    /* Merge a non-persisted (and not merged with persisted) file w/ any on-disk file
     * that has appeared since startup; or, our own, if we abandoned it but stayed
     * coarse due to a reset or hotp flush.
     */
    if ((!info->persisted && info->persisted_source_mmap_size == 0 &&
         DYNAMO_OPTION(coarse_lone_merge)) ||
        /* FIXME case 10356: need a better check since can have false positive
         * and false negatives by only looking at size.
         */
        (existing_size != inuse_size && DYNAMO_OPTION(coarse_disk_merge))) {
        merge_with = coarse_unit_load(dcontext, info->base_pc, info->end_pc,
                                      false /*not for execution*/);
        /* We rely on coarse_unit_load to reject incompatible pcaches, whether for
         * tls, trace support, or other reasons.  We do need to check the region
         * here.  FIXME: once we support relocs we need to handle appropriately.
         */
        if (merge_with != NULL) {
            LOG(THREAD, LOG_CACHE, 2, "  merging to-be-persisted %s with on-disk %s\n",
                info->module, filename);
            /* Case 8640: allow merging with a smaller on-disk file, to avoid
             * being forever stuck at that size by prior cores with no IAT merging
             * or other features */
            if (merge_with->base_pc >= info->base_pc &&
                merge_with->end_pc <= info->end_pc) {
                /* If want in-place merging, need to arrange for ibl invalidation:
                 * case 11057 */
                postmerge =
                    coarse_unit_merge(dcontext, info, merge_with, false /*!in-place*/);
                ASSERT(postmerge != NULL);
                /* if NULL info is still guaranteed to be unchanged so carry on */
                DOSTATS({
                    if (postmerge == NULL)
                        STATS_INC(coarse_merge_disk_fail);
                    else
                        STATS_INC(coarse_merge_disk);
                });
            } else {
                /* FIXME case 10357: w/o -unsafe_ignore_IAT_writes
                 * we're going to see a lot
                 * of this w/ process-shared pcaches.  Should we abort
                 * the persist at this point to keep the small-region
                 * file on disk?  Otherwise we're going to keep
                 * trading w/ the small-region process.
                 */
                LOG(THREAD, LOG_CACHE, 2,
                    "  region mismatch: " PFX "-" PFX " on-disk vs " PFX "-" PFX
                    " live\n",
                    merge_with->base_pc, merge_with->end_pc, info->base_pc, info->end_pc);
                STATS_INC(coarse_merge_disk_mismatch);
            }
            coarse_unit_reset_free(dcontext, merge_with, false /*no locks*/,
                                   true /*need to unlink*/, false /*not in use anyway*/);
            coarse_unit_free(dcontext, merge_with);
            merge_with = NULL;
        } else
            STATS_INC(coarse_merge_disk_fail);
    }
    return postmerge;
}

/* Calculates information for persisting that we don't need for online-generated
 * units, such as hotp points and RAC/RCT info.
 * If just_live is true, ignores any currently in-use persisted info, assuming
 * it will be merged in in a merge step by an in-use persisted unit.
 * Caller must hold change_linking_lock, read lock hotp_get_lock(), and info's lock.
 */
static void
coarse_unit_calculate_persist_info(dcontext_t *dcontext, coarse_info_t *info)
{
#ifdef HOT_PATCHING_INTERFACE
    int len;
#endif
    /* we need real dcontext for rac_entries_in_region() */
    ASSERT(dcontext != NULL && dcontext != GLOBAL_DCONTEXT);

    ASSERT_OWN_RECURSIVE_LOCK(true, &change_linking_lock);
#ifdef HOT_PATCHING_INTERFACE
    ASSERT_OWN_READ_LOCK(DYNAMO_OPTION(hot_patching), hotp_get_lock());
#endif
    ASSERT_OWN_MUTEX(true, &info->lock);
    LOG(THREAD, LOG_CACHE, 1, "coarse_unit_calculate_persist_info %s " PFX "-" PFX "\n",
        info->module, info->base_pc, info->end_pc);
    ASSERT(info->frozen && !info->persisted && !info->has_persist_info);

    if (DYNAMO_OPTION(coarse_freeze_elide_ubr))
        info->flags |= PERSCACHE_ELIDED_UBR;
#if defined(RETURN_AFTER_CALL) && defined(WINDOWS)
    if (seen_Borland_SEH)
        info->flags |= PERSCACHE_SEEN_BORLAND_SEH;
#endif
    if (!DYNAMO_OPTION(disable_traces))
        info->flags |= PERSCACHE_SUPPORT_TRACES;

#ifdef RCT_IND_BRANCH
    ASSERT(info->rct_table == NULL);
    if ((TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_call)) ||
         TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_jump))) &&
        (DYNAMO_OPTION(persist_rct)
#    if defined(RETURN_AFTER_CALL) && defined(WINDOWS)
         /* case 8648: we can have RCT entries that come from code patterns
          * not derivable from relocation entries that we must persist.
          */
         || os_module_get_flag(info->base_pc, MODULE_HAS_BORLAND_SEH)
#    endif
             )) {
        app_pc limit_start = info->base_pc;
        app_pc limit_end = info->end_pc;
        if (DYNAMO_OPTION(persist_rct) && DYNAMO_OPTION(persist_rct_entire)) {
            limit_start = 0;
            limit_end = (app_pc)POINTER_MAX;
        }
        info->flags |= PERSCACHE_SUPPORT_RCT;
        /* We don't support pulling out just entries from this sub-module-region */
        ASSERT(DYNAMO_OPTION(persist_rct_entire));
        info->flags |= PERSCACHE_ENTIRE_MODULE_RCT;
        /* Get a copy of the live + persisted tables merged together */
        info->rct_table = rct_module_table_copy(dcontext, info->base_pc, RCT_RCT,
                                                limit_start, limit_end);
    }
#endif
#ifdef RETURN_AFTER_CALL
    if (DYNAMO_OPTION(ret_after_call)) {
        ASSERT(info->rac_table == NULL);
        info->flags |= PERSCACHE_SUPPORT_RAC;
        /* If we don't persist RAC we'll get violations when using! */
        /* Get a copy of the live + persisted tables merged together,
         * but only the entries in this region.
         * To make sure we have entries for call sites in the region, not targets,
         * we ignore target just over start and include just over end: thus the
         * +1 on both sides.
         */
        info->rac_table = rct_module_table_copy(dcontext, info->base_pc, RCT_RAC,
                                                info->base_pc + 1, info->end_pc + 1);
    }
#endif

#ifdef HOT_PATCHING_INTERFACE
    if (DYNAMO_OPTION(hot_patching)) {
        /* We expect 2 patch points per hotpatch on average; most have 1,
         * some have up to 5.  So we could hardcode a max length; instead
         * we have a separate pass to get the size.
         */
        /* FIXME: we could include the image entry point to avoid flushing the
         * whole exe, but that only happens when we inject into secondary thread.
         */
        /* FIXME: when merging live with in-use-persisted we don't need to
         * re-calculate this and can just use the persisted array, since we would
         * have flushed the unit on any new hotp
         */
        ASSERT_OWN_READ_LOCK(DYNAMO_OPTION(hot_patching), hotp_get_lock());
        info->hotp_ppoint_vec_num =
            hotp_num_matched_patch_points(info->base_pc, info->end_pc);
        if (info->hotp_ppoint_vec_num > 0) {
            info->hotp_ppoint_vec =
                HEAP_ARRAY_ALLOC(dcontext, app_rva_t, info->hotp_ppoint_vec_num,
                                 ACCT_HOT_PATCHING, PROTECTED);
            len = hotp_get_matched_patch_points(info->base_pc, info->end_pc,
                                                info->hotp_ppoint_vec,
                                                info->hotp_ppoint_vec_num);
            /* Should never have mismatch, as we're holding the hotp lock.
             * Even if len < 0 the routine still fills up the array. */
            ASSERT(len == (int)info->hotp_ppoint_vec_num);
            if (len != (int)info->hotp_ppoint_vec_num) {
                /* abort writing out hotp points */
                info->hotp_ppoint_vec_num = 0;
            }
            LOG(THREAD, LOG_CACHE, 2, "hotp points for %s " PFX "-" PFX ":\n",
                info->module, info->base_pc, info->end_pc);
            DODEBUG({
                uint i;
                for (i = 0; i < info->hotp_ppoint_vec_num; i++)
                    LOG(THREAD, LOG_CACHE, 2, "\t" PFX "\n", info->hotp_ppoint_vec[i]);
            });
        } else
            ASSERT(info->hotp_ppoint_vec == NULL);
    } else
        ASSERT(info->hotp_ppoint_vec == NULL);
#endif

    info->has_persist_info = true;
}

/* Merges the persist-calculated fields of info1 and info2 into dst,
 * as well as non-persist-calculated fields like primary_for_module.
 * Assumption: we've already checked for compatibility and here we just
 * need to take the union (minus dups).
 * Caller must hold info1's lock, and we must be dynamo_all_threads_synched.
 */
static void
coarse_unit_merge_persist_info(dcontext_t *dcontext, coarse_info_t *dst,
                               coarse_info_t *info1, coarse_info_t *info2)
{
    ASSERT(dynamo_all_threads_synched);
    LOG(THREAD, LOG_CACHE, 1, "coarse_unit_merge_persist_info %s " PFX "-" PFX "\n",
        info1->module, info1->base_pc, info1->end_pc);
    /* We can't grab both locks due to deadlock potential.  Currently we are
     * always fully synched, so we rely on that.
     */
    ASSERT(dynamo_all_threads_synched);
    ASSERT_OWN_MUTEX(true, &info1->lock);

    /* We need to incorporate flags from persisted units (Borland, e.g.) */
    dst->flags |= info1->flags;
    dst->flags |= info2->flags;
    /* Some flags are intersection rather than union (but those like
     * PERSCACHE_SUPPORT_TRACES are weeded out at load time and should
     * not differ here)
     */
    if (!TEST(PERSCACHE_MAP_RW_SEPARATE, info1->flags) ||
        !TEST(PERSCACHE_MAP_RW_SEPARATE, info2->flags))
        dst->flags &= ~PERSCACHE_MAP_RW_SEPARATE;
    /* Same bounds, so same persistence privileges */
    dst->primary_for_module = info1->primary_for_module || info2->primary_for_module;

    ASSERT(!info2->persisted || !info2->in_use ||
           info2->has_persist_info); /* must have to use */

    /* Everything else is only necessary if we've already calculated persist
     * info; if we haven't, then if we do persist later we'll calculate it
     * fresh, so no need to calculate and merge here
     */
    if (!info1->has_persist_info && !info2->has_persist_info)
        return;

    if (!info1->has_persist_info)
        coarse_unit_calculate_persist_info(dcontext, info1);
    if (!info2->has_persist_info)
        coarse_unit_calculate_persist_info(dcontext, info2);
    ASSERT(info1->has_persist_info && info2->has_persist_info);
    ASSERT(!dst->has_persist_info);

    ASSERT((info1->flags &
            (PERSCACHE_SUPPORT_TRACES | PERSCACHE_SUPPORT_RAC | PERSCACHE_SUPPORT_RCT)) ==
           (info2->flags &
            (PERSCACHE_SUPPORT_TRACES | PERSCACHE_SUPPORT_RAC | PERSCACHE_SUPPORT_RCT)));

#ifdef RCT_IND_BRANCH
    ASSERT(dst->rct_table == NULL);
    if (TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_call)) ||
        TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_jump))) {
        if (info2->persisted && info2->in_use && !info1->persisted) {
            /* coarse_unit_calculate_persist_info already merged the new entries
             * with the in-use persisted entries
             */
            dst->rct_table = rct_table_copy(dcontext, info1->rct_table);
        } else {
            /* Note that we can't simply take one source if it has
             * PERSCACHE_ENTIRE_MODULE_RCT as Borland can add entries: we must
             * merge every time
             */
            dst->rct_table =
                rct_table_merge(dcontext, info1->rct_table, info2->rct_table);
        }
    }
#endif
#ifdef RETURN_AFTER_CALL
    ASSERT(dst->rac_table == NULL);
    if (DYNAMO_OPTION(ret_after_call)) {
        if (info2->persisted && info2->in_use && !info1->persisted) {
            /* coarse_unit_calculate_persist_info already merged the new entries
             * with the in-use persisted entries
             */
            dst->rac_table = rct_table_copy(dcontext, info1->rac_table);
        } else {
            dst->rac_table =
                rct_table_merge(dcontext, info1->rac_table, info2->rac_table);
        }
    }
#endif

#ifdef HOT_PATCHING_INTERFACE
    ASSERT(dst->hotp_ppoint_vec == NULL);
    if (info2->persisted && info2->in_use && !info1->persisted) {
        /* We would have flushed info2 if a new hotp overlapped it so just
         * take its hotp list.  info1 may have a subset.
         */
        dst->hotp_ppoint_vec_num = info2->hotp_ppoint_vec_num;
        if (dst->hotp_ppoint_vec_num > 0) {
            dst->hotp_ppoint_vec =
                HEAP_ARRAY_ALLOC(dcontext, app_rva_t, dst->hotp_ppoint_vec_num,
                                 ACCT_HOT_PATCHING, PROTECTED);
            memcpy(dst->hotp_ppoint_vec, info2->hotp_ppoint_vec,
                   dst->hotp_ppoint_vec_num * sizeof(app_rva_t));
        }
    } else {
        /* We expect <= 5 entries in each array so quadratic and two passes
         * shouldn't be a perf issue.
         */
        ASSERT(dst->hotp_ppoint_vec_num == 0);
        ASSERT(sizeof(app_rva_t) == sizeof(void *));
        array_merge(dcontext, true /* intersect */, (void **)info1->hotp_ppoint_vec,
                    info1->hotp_ppoint_vec_num, (void **)info2->hotp_ppoint_vec,
                    info2->hotp_ppoint_vec_num, (void ***)&dst->hotp_ppoint_vec,
                    &dst->hotp_ppoint_vec_num HEAPACCT(ACCT_HOT_PATCHING));
    }
#endif
    dst->has_persist_info = true;
}

static bool
write_persist_file(dcontext_t *dcontext, file_t fd, const void *buf, size_t count)
{
    ASSERT(fd != INVALID_FILE && buf != NULL && count > 0);
    if (os_write(fd, buf, count) != (ssize_t)count) {
        LOG(THREAD, LOG_CACHE, 1, "  unable to write " SZFMT " bytes to file\n", count);
        SYSLOG_INTERNAL_WARNING_ONCE("unable to write persistent cache file");
        STATS_INC(coarse_units_persist_error);
        return false;
    }
    return true;
}

static bool
pad_persist_file(dcontext_t *dcontext, file_t fd, size_t bytes, coarse_info_t *info)
{
    size_t towrite = bytes;
    size_t thiswrite;
    /* We don't have a handy page of zeros so we just write the cache start.
     * FIXME: better to use file positions (just setting the size doesn't
     * advance the pointer): write_file() has win32 support.
     */
    ASSERT(fd != INVALID_FILE);
    ASSERT(bytes < 64 * 1024); /* error */
    while (towrite > 0) {
        thiswrite = MIN(
            towrite, (ptr_uint_t)info->stubs_end_pc - (ptr_uint_t)info->cache_start_pc);
        if (!write_persist_file(dcontext, fd, info->cache_start_pc, thiswrite))
            return false;
        towrite -= thiswrite;
    }
    return true;
}

/* Fills in pers with data from info */
static void
coarse_unit_set_persist_data(dcontext_t *dcontext, coarse_info_t *info,
                             coarse_persisted_info_t *pers, app_pc modbase,
                             op_pcache_t option_level, const char *option_string)
{
    size_t x_offs = 0; /* offs of start of +x section */

    /* We do not need to freeze the world: merely the state of outgoing links. */
    ASSERT_OWN_RECURSIVE_LOCK(true, &change_linking_lock);
#ifdef HOT_PATCHING_INTERFACE
    ASSERT_OWN_READ_LOCK(DYNAMO_OPTION(hot_patching), hotp_get_lock());
#endif
    ASSERT_OWN_MUTEX(true, &info->lock);

    /* Put the unit in the proper state for persisting */
    coarse_unit_unlink_outgoing(dcontext, info);

    /* Fill in the metadata */
    pers->magic = PERSISTENT_CACHE_MAGIC;
    pers->version = PERSISTENT_CACHE_VERSION;
    pers->header_len = sizeof(*pers);
    pers->data_len = 0;

    /* Take flags from info */
    pers->flags = info->flags;
    /* FIXME: see above: should be set earlier */
    pers->flags |= IF_X64_ELSE(PERSCACHE_X86_64, PERSCACHE_X86_32);
    if (option_level == OP_PCACHE_LOCAL) {
        ASSERT(option_string != NULL);
        pers->flags |= PERSCACHE_EXEMPTION_OPTIONS;
    }

#ifdef BUILD_NUMBER
    pers->build_number = BUILD_NUMBER;
#else
    pers->build_number = 0;
#endif

    if (TEST(PERSCACHE_MODULE_MD5_AT_LOAD, DYNAMO_OPTION(persist_gen_validation))) {
        ASSERT(!is_region_memset_to_char((byte *)&info->module_md5,
                                         sizeof(info->module_md5), 0));
        DOLOG(1, LOG_CACHE, {
            print_module_digest(THREAD, &info->module_md5, "using md5 from load time: ");
        });
        memcpy(&pers->modinfo.module_md5, &info->module_md5,
               sizeof(pers->modinfo.module_md5));
    } else {
        persist_calculate_module_digest(
            &pers->modinfo.module_md5, modbase, (size_t)pers->modinfo.image_size,
            info->base_pc, info->end_pc, DYNAMO_OPTION(persist_gen_validation));
    }
    /* rest of modinfo filled in by get_persist_filename() */
    ASSERT(pers->modinfo.base == modbase);

    ASSERT(info->base_pc >= modbase);
    ASSERT(info->end_pc > info->base_pc);
    pers->start_offs = info->base_pc - modbase;
    pers->end_offs = info->end_pc - modbase;
    pers->tls_offs_base = os_tls_offset(0);

    /* We need to go in forward order so we can tell the client the current offset */
    x_offs += pers->header_len;

    pers->option_string_len = (option_string == NULL || option_string[0] == '\0')
        ? 0
        : (ALIGN_FORWARD((strlen(option_string) + 1 /*include NULL*/) * sizeof(char),
                         OPTION_STRING_ALIGNMENT));
    x_offs += pers->option_string_len;

    pers->instrument_ro_len = ALIGN_FORWARD(
        instrument_persist_ro_size(dcontext, info, x_offs), CLIENT_ALIGNMENT);
    x_offs += pers->instrument_ro_len;

    /* Add new data section here */

#ifdef HOT_PATCHING_INTERFACE
    pers->hotp_patch_list_len = sizeof(app_rva_t) * info->hotp_ppoint_vec_num;
    x_offs += pers->hotp_patch_list_len;
#endif

    /* FIXME case 9581 NYI: need to add to coarse_unit_calculate_persist_info()
     * and coarse_unit_merge_persist_info() as well.
     */
    pers->reloc_len = 0;
    x_offs += pers->reloc_len;

#ifdef RETURN_AFTER_CALL
    pers->rac_htable_len = rct_table_persist_size(dcontext, info->rac_table);
#else
    pers->rac_htable_len = 0;
#endif
    x_offs += pers->rac_htable_len;
#ifdef RCT_IND_BRANCH
    pers->rct_htable_len = rct_table_persist_size(dcontext, info->rct_table);
#else
    pers->rct_htable_len = 0;
#endif
    x_offs += pers->rct_htable_len;

    pers->stub_htable_len =
        fragment_coarse_htable_persist_size(dcontext, info, false /*stub table*/);
    x_offs += pers->stub_htable_len;
    pers->cache_htable_len =
        fragment_coarse_htable_persist_size(dcontext, info, true /*cache table*/);
    x_offs += pers->cache_htable_len;

    /* end of read-only, start of +rx */
    pers->data_len += (x_offs - pers->header_len);
    /* We need to pad out the data before the +x boundary to get a new page */
    pers->pad_len = ALIGN_FORWARD(x_offs, PAGE_SIZE) - x_offs;
    pers->data_len += pers->pad_len;

    pers->instrument_rx_len =
        ALIGN_FORWARD(instrument_persist_rx_size(dcontext, info, x_offs + pers->pad_len),
                      CLIENT_ALIGNMENT);
    pers->data_len += pers->instrument_rx_len;

    /* calculate cache_len so we can calculate padding.  we need 2 different pads
     * b/c of instrument_rx needing to know its offset.
     */
    pers->cache_len = info->fcache_return_prefix - info->cache_start_pc;

    if (DYNAMO_OPTION(persist_map_rw_separate)) {
        /* We need the cache/stub split to be on a file view map boundary,
         * yet have cache+stubs adjacent
         */
        size_t rwx_offs =
            x_offs + pers->pad_len + pers->cache_len + pers->instrument_rx_len;
        pers->view_pad_len = ALIGN_FORWARD(rwx_offs, MAP_FILE_VIEW_ALIGNMENT) - rwx_offs;
        pers->flags |= PERSCACHE_MAP_RW_SEPARATE;
    }
    pers->data_len += pers->view_pad_len;

    pers->post_cache_pad_len = info->fcache_return_prefix - info->cache_end_pc;
    pers->data_len += pers->cache_len; /* includes post_cache_pad_len */
    STATS_ADD(coarse_code_persisted, info->cache_end_pc - info->cache_start_pc);

    pers->stubs_len = info->stubs_end_pc - info->stubs_start_pc;
    pers->data_len += pers->stubs_len;
    pers->ibl_jmp_prefix_len = info->stubs_start_pc - info->ibl_jmp_prefix;
    pers->data_len += pers->ibl_jmp_prefix_len;
    pers->ibl_call_prefix_len = info->ibl_jmp_prefix - info->ibl_call_prefix;
    pers->data_len += pers->ibl_call_prefix_len;
    pers->ibl_ret_prefix_len = info->ibl_call_prefix - info->ibl_ret_prefix;
    pers->data_len += pers->ibl_ret_prefix_len;
    pers->trace_head_return_prefix_len =
        info->ibl_ret_prefix - info->trace_head_return_prefix;
    pers->data_len += pers->trace_head_return_prefix_len;
    pers->fcache_return_prefix_len =
        info->trace_head_return_prefix - info->fcache_return_prefix;
    pers->data_len += pers->fcache_return_prefix_len;

    pers->instrument_rw_len = ALIGN_FORWARD(
        instrument_persist_rw_size(dcontext, info, pers->header_len + pers->data_len),
        CLIENT_ALIGNMENT);
    pers->data_len += pers->instrument_rw_len;

    /* We always write the footer regardless of whether we calculate the
     * MD5.  Otherwise we'd need a new flag; not really worth it.
     * Plus, we have our magic field as an always-on extra check.
     * Note that we do not set the total file size up front (which could make
     * our writes faster), so we know if the footer is there then it was
     * actually written.
     */
    pers->data_len += sizeof(persisted_footer_t);

    /* Our relative jmps require that we do not exceed 32-bit reachability */
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_int(pers->data_len)));
    LOG(THREAD, LOG_CACHE, 2, "  header=" SZFMT ", data=" SZFMT ", pad=" SZFMT "\n",
        pers->header_len, pers->data_len, pers->pad_len + pers->view_pad_len);
}

/* Separated from coarse_unit_persist() to keep the stack space on the
 *   persist -> merge_with_disk -> load
 * path to a minimum (case 10712).
 */
static bool
coarse_unit_persist_rename(dcontext_t *dcontext, const char *filename,
                           const char *tmpname)
{
    bool success = false;
    char rename[MAXIMUM_PATH];
    if (os_rename_file(tmpname, filename, false /*do not replace*/)) {
        success = true;
    } else {
        ASSERT_CURIOSITY(os_file_exists(filename, false /*!dir*/));
        if (DYNAMO_OPTION(coarse_freeze_rename)) {
            get_unique_name(filename, "todel", rename, BUFFER_SIZE_ELEMENTS(rename));
            LOG(THREAD, LOG_CACHE, 1, "  attempting to rename %s to %s\n", filename,
                rename);
            if (os_rename_file(filename, rename, false /*do not replace*/)) {
                LOG(THREAD, LOG_CACHE, 1, "  succeeded renaming %s to %s\n", filename,
                    rename);
                STATS_INC(persist_rename_success);
                if (DYNAMO_OPTION(coarse_freeze_clean)) {
                    /* Try to mark the renamed file for deletion.  We have no handle
                     * open and are re-targeting based on name only but should have a
                     * unique name so we aren't worried about deleting the wrong file.
                     */
                    if (os_delete_mapped_file(rename)) {
                        LOG(THREAD, LOG_CACHE, 1, "  succeeded marking for deletion %s\n",
                            rename);
                        STATS_INC(persist_delete_success);
                    }
                }
                /* We could support os_delete_mapped_file() w/o renaming first but it
                 * only works for SEC_COMMIT with -no_persist_lock_file */
                if (os_rename_file(tmpname, filename, false /*do not replace*/))
                    success = true;
                else {
                    /* Race: someone beat us to the rename (or else no permissions).
                     * FIXME: should we try again? */
                    ASSERT_CURIOSITY(os_file_exists(filename, false /*!dir*/));
                    STATS_INC(persist_rename_race);
                    ASSERT(!success);
                }
            } else {
                STATS_INC(persist_rename_fail);
                ASSERT(!success);
            }
        }
    }
    return success;
}

bool
instrument_persist_section(dcontext_t *dcontext, file_t fd, coarse_info_t *info,
                           size_t len, bool (*persist_func)(dcontext_t *, void *, file_t))
{
    if (len > 0) {
        /* keep aligned */
        int64 post_pos, pre_pos = os_tell(fd);
        if (!persist_func(dcontext, info, fd)) {
            LOG(THREAD, LOG_CACHE, 1, "  unable to write client data to file\n");
            SYSLOG_INTERNAL_WARNING_ONCE("unable to write client data to pcache file");
            STATS_INC(coarse_units_persist_error);
            return false;
        }
        post_pos = os_tell(fd);
        if (pre_pos == -1 || post_pos == -1) {
            SYSLOG_INTERNAL_WARNING_ONCE("unable to tell pcache file position");
            STATS_INC(coarse_units_persist_error);
            return false;
        }
        /* pad out to the alignment we added to len */
        ASSERT(len >= (size_t)(post_pos - pre_pos));
        if (!pad_persist_file(dcontext, fd, len - (size_t)(post_pos - pre_pos), info))
            return false; /* logs, stats in write_persist_file */
    }
    return true;
}

DR_API
app_pc
dr_persist_start(void *perscxt)
{
    coarse_info_t *info = (coarse_info_t *)perscxt;
    CLIENT_ASSERT(perscxt != NULL, "invalid arg: perscxt is NULL");
    return info->base_pc;
}

DR_API
size_t
dr_persist_size(void *perscxt)
{
    coarse_info_t *info = (coarse_info_t *)perscxt;
    CLIENT_ASSERT(perscxt != NULL, "invalid arg: perscxt is NULL");
    return info->end_pc - info->base_pc;
}

DR_API
bool
dr_fragment_persistable(void *drcontext, void *perscxt, void *tag_in)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    cache_pc res;
    /* deliberately not calling dr_fragment_app_pc() b/c any pc there won't
     * be coarse and thus not worth the extra overhead
     */
    app_pc tag = (app_pc)tag_in;
    if (perscxt != NULL) {
        coarse_info_t *info = (coarse_info_t *)perscxt;
        fragment_coarse_lookup_in_unit(dcontext, info, tag, NULL, &res);
        if (res != NULL)
            return true;
        if (info->non_frozen != NULL) {
            ASSERT(info->frozen);
            fragment_coarse_lookup_in_unit(dcontext, info->non_frozen, tag, NULL, &res);
        }
    } else {
        res = fragment_coarse_lookup(dcontext, tag);
    }
    return (res != NULL);
}

/* Unlinks all inter-unit stubs.  Can still use info afterward, as
 * lazy linking should soon re-link them.
 * Caller must hold change_linking_lock and read lock hotp_get_lock().
 */
bool
coarse_unit_persist(dcontext_t *dcontext, coarse_info_t *info)
{
    coarse_persisted_info_t pers;
    persisted_footer_t footer;
    /* FIXME: this is a lot of stack space: make static and serialize? */
    char filename[MAXIMUM_PATH];
    char tmpname[MAXIMUM_PATH];
    char option_buf[MAX_PCACHE_OPTIONS_STRING];
    const char *option_string;
    op_pcache_t option_level;
    app_pc modbase;
    bool success = false;
    bool created_temp = false;
    bool free_info = false;
    file_t fd = INVALID_FILE;
    /* we need real dcontext for rac_entries_in_region() */
    ASSERT(dcontext != NULL && dcontext != GLOBAL_DCONTEXT);

    KSTART(persisted_generation);
    LOG(THREAD, LOG_CACHE, 1, "coarse_unit_persist %s " PFX "-" PFX "\n", info->module,
        info->base_pc, info->end_pc);
    STATS_INC(coarse_units_persist_try);
    ASSERT(info->frozen && !info->persisted);

    /* invalid unit shouldn't get this far */
    ASSERT(!TEST(PERSCACHE_CODE_INVALID, info->flags));
    if (TEST(PERSCACHE_CODE_INVALID, info->flags)) /* paranoid */
        goto coarse_unit_persist_exit;

    modbase = get_module_base(info->base_pc);
    if (modbase == NULL) {
        LOG(THREAD, LOG_CACHE, 1, "  no module base for " PFX "\n", info->base_pc);
        goto coarse_unit_persist_exit;
    }
    /* case 9653/10380: only one coarse unit in a module's +x region(s) is persisted */
    if (!info->primary_for_module) {
        /* the primary may have been flushed, so see if we can become primary */
        coarse_unit_mark_primary(info);
        if (!info->primary_for_module) {
            LOG(THREAD, LOG_CACHE, 1,
                "  not primary unit for module %s: not persisting\n", info->module);
            STATS_INC(coarse_units_persist_dup);
            goto coarse_unit_persist_exit;
        }
    }
#ifdef WINDOWS
    if (!DYNAMO_OPTION(coarse_freeze_rebased_aslr) &&
        os_module_get_flag(modbase, MODULE_DO_NOT_PERSIST)) {
        LOG(THREAD, LOG_CACHE, 1, "  %s marked as do-not-persist\n", info->module);
        goto coarse_unit_persist_exit;
    }
#endif
    /* case 9799: store pcache-affecting options */
    option_level = persist_get_options_level(modbase, info, false /*use exemptions*/);
    LOG(THREAD, LOG_CACHE, 2, "  persisting option string at %d level\n", option_level);
    option_string = persist_get_relevant_options(
        dcontext, option_buf, BUFFER_SIZE_ELEMENTS(option_buf), option_level);
    /* get_persist_filename() fills in pers.modinfo */
    memset(&pers, 0, sizeof(pers));
    if (!get_persist_filename(filename, BUFFER_SIZE_ELEMENTS(filename), modbase,
                              true /*write*/, &pers.modinfo, option_string)) {
        LOG(THREAD, LOG_CACHE, 1, "  error calculating filename (or excluded)\n");
        STATS_INC(coarse_units_persist_error);
        goto coarse_unit_persist_exit;
    }
    LOG(THREAD, LOG_CACHE, 1, "  persisted filename = %s\n", filename);
    /* Open the file before unlinking since may be common point of failure.
     * FIXME: better to cache base dir handle and pass just name here
     * and to os_{rename,delete_mapped}_file?  Would be win32-specific code.
     */
    if (!DYNAMO_OPTION(coarse_freeze_rename) && !DYNAMO_OPTION(coarse_freeze_clobber) &&
        os_file_exists(filename, false /*FIXME: would like to check all*/)) {
        /* Check up front to avoid work */
        LOG(THREAD, LOG_CACHE, 1, "  will be unable to replace existing file %s\n",
            filename);
        STATS_INC(coarse_units_persist_error);
        goto coarse_unit_persist_exit;
    }

    get_unique_name(filename, "tmp", tmpname, BUFFER_SIZE_ELEMENTS(tmpname));
    fd = os_open(tmpname,
                 OS_OPEN_WRITE |
                     /* We ask for read access to map it later for md5 */
                     OS_OPEN_READ |
                     /* Case 9964: we want to allow renaming while holding
                      * the file handle */
                     OS_SHARE_DELETE |
                     /* Renaming is the better option; and for SEC_COMMIT
                      * we can delete immediately, w/o the symlink risk case 9057;
                      * so we leave the CURIOSITY we get w/ the clobber option.
                      */
                     (DYNAMO_OPTION(coarse_freeze_clobber) ? 0 : OS_OPEN_REQUIRE_NEW) |
                     /* case 10884: needed only for validate_owner_file */
                     OS_OPEN_FORCE_OWNER);
    if (fd == INVALID_FILE) {
        /* FIXME: it's possible but unlikely that our name collided: should we
         * try again?
         */
        LOG(THREAD, LOG_CACHE, 1, "  unable to open temp file %s\n", tmpname);
        STATS_INC(coarse_units_persist_error);
        goto coarse_unit_persist_exit;
    }
    ASSERT_CURIOSITY(
        (!DYNAMO_OPTION(validate_owner_file) || os_validate_user_owned(fd)) &&
        "persisted while impersonating?");

    created_temp = true;
    /* FIXME case 9758: we could mmap the file: would that be more efficient?
     * We could then more easily write each body section at the same
     * time as we calculate its size for the header, instead of linear
     * write order.  We could also pass file offsets to out-of-order writes.
     * Could we control write-through if we mmap (case 9696)?
     * Note the prescribed order of unmap and then flush in case 9696.
     */

    d_r_mutex_lock(&info->lock);
    /* Get info that is not needed for online coarse units but is needed at persist
     * time (RCT, RAC, and hotp info) now, so we can merge it more easily.
     */
    if (!info->has_persist_info)
        coarse_unit_calculate_persist_info(dcontext, info);
    ASSERT(info->has_persist_info);
    /* FIXME: the lock situation in all these routines is ugly: here we must
     * release since coarse_unit_merge() will acquire.  Should we not grab any
     * info lock?  We're already relying on dynamo_all_threads_synched.
     */
    d_r_mutex_unlock(&info->lock);

    /* Attempt to merge with any new on-disk file */
    if (DYNAMO_OPTION(coarse_lone_merge) || DYNAMO_OPTION(coarse_disk_merge)) {
        coarse_info_t *postmerge = coarse_unit_merge_with_disk(dcontext, info, filename);
        if (postmerge != NULL) {
            /* Just abandon the original info */
            info = postmerge;
            free_info = true;
        }
    }

    d_r_mutex_lock(&info->lock);

    /* Fill in the rest of pers */
    coarse_unit_set_persist_data(dcontext, info, &pers, modbase, option_level,
                                 option_string);

    if (!coarse_unit_check_persist_space(fd, pers.header_len + pers.data_len)) {
        /* We also check this prior to freezing/merging to avoid work on a
         * perpetually-full volume */
        LOG(THREAD, LOG_CACHE, 1,
            "  not enough disk space available for module %s: not persisting\n",
            info->module);
        STATS_INC(coarse_units_persist_nospace);
        goto coarse_unit_persist_exit;
    }

    /* Give clients a chance to patch the cache mmap before we write it to
     * the file.  We do it here at a clean point between the size and persist
     * calls, rather than in the middle of the persist (must be before persist_rw).
     */
    if (!instrument_persist_patch(dcontext, info, info->cache_start_pc,
                                  info->cache_end_pc - info->cache_start_pc)) {
        LOG(THREAD, LOG_CACHE, 1, "  client unable to patch module %s: not persisting\n",
            info->module);
        STATS_INC(coarse_units_persist_nopatch);
        goto coarse_unit_persist_exit;
    }

    /* Write the headers */
    if (!write_persist_file(dcontext, fd, &pers, pers.header_len))
        goto coarse_unit_persist_exit; /* logs, stats are in write_persist_file */

    if (pers.option_string_len > 0) {
        size_t len = strlen(option_string) + 1 /*NULL*/;
        ASSERT(len <= pers.option_string_len);
        if (!write_persist_file(dcontext, fd, option_string, len))
            goto coarse_unit_persist_exit; /* logs, stats are in write_persist_file */
        if (pers.option_string_len - len > 0) {
            if (!pad_persist_file(dcontext, fd, pers.option_string_len - len, info))
                goto coarse_unit_persist_exit; /* logs, stats in write_persist_file */
        }
    }

    /* New data section goes here */

    if (!instrument_persist_section(dcontext, fd, info, pers.instrument_ro_len,
                                    instrument_persist_ro))
        goto coarse_unit_persist_exit;

#ifdef HOT_PATCHING_INTERFACE
    if (pers.hotp_patch_list_len > 0) {
        if (!write_persist_file(dcontext, fd, info->hotp_ppoint_vec,
                                pers.hotp_patch_list_len))
            goto coarse_unit_persist_exit; /* logs, stats are in write_persist_file */
    }
#endif

    /* FIXME case 9581 NYI: write reloc section */

#ifdef RETURN_AFTER_CALL
    if (pers.rac_htable_len > 0) {
        if (!rct_table_persist(dcontext, info->rac_table, fd)) {
            LOG(THREAD, LOG_CACHE, 1, "  unable to write RAC htable to file\n");
            SYSLOG_INTERNAL_WARNING_ONCE("unable to write RAC htable to pcache file");
            STATS_INC(coarse_units_persist_error);
            goto coarse_unit_persist_exit;
        }
    }
#else
    ASSERT(pers.rac_htable_len == 0);
#endif
#ifdef RCT_IND_BRANCH
    if (pers.rct_htable_len > 0) {
        if (!rct_table_persist(dcontext, info->rct_table, fd)) {
            LOG(THREAD, LOG_CACHE, 1, "  unable to write RCT htable to file\n");
            SYSLOG_INTERNAL_WARNING_ONCE("unable to write RCT htable to pcache file");
            STATS_INC(coarse_units_persist_error);
            goto coarse_unit_persist_exit;
        }
    }
#else
    ASSERT(pers.rct_htable_len == 0);
#endif

    /* For modularity we split out htable persistence.  We could combine the
     * tables with our cache+stubs mmap region at freeze time but there's no
     * need as there are no cross-links so we keep them separate.
     */
    if (!fragment_coarse_htable_persist(dcontext, info, true /*cache table*/, fd) ||
        !fragment_coarse_htable_persist(dcontext, info, false /*stub table*/, fd)) {
        LOG(THREAD, LOG_CACHE, 1, "  unable to write htable(s) to file\n");
        SYSLOG_INTERNAL_WARNING_ONCE("unable to write htable(s) to pcache file");
        STATS_INC(coarse_units_persist_error);
        goto coarse_unit_persist_exit;
    }

    /* If we used an image format, or did separate mappings, we could avoid
     * storing padding on disk
     */
    if (pers.pad_len > 0) {
        if (!pad_persist_file(dcontext, fd, pers.pad_len, info))
            goto coarse_unit_persist_exit; /* logs, stats in write_persist_file */
    }

    if (!instrument_persist_section(dcontext, fd, info, pers.instrument_rx_len,
                                    instrument_persist_rx))
        goto coarse_unit_persist_exit;

    if (pers.view_pad_len > 0) {
        if (!pad_persist_file(dcontext, fd, pers.view_pad_len, info))
            goto coarse_unit_persist_exit; /* logs, stats in write_persist_file */
    }

    /* Write the cache + stubs.  We omit the guard pages of course.
     * FIXME: when freezing and persisting at once, more efficient to
     * make our cache+stubs mmap backed by the file from the start.
     */
    if (!write_persist_file(dcontext, fd, info->cache_start_pc,
                            info->stubs_end_pc - info->cache_start_pc))
        goto coarse_unit_persist_exit; /* logs, stats are in write_persist_file */

    if (!instrument_persist_section(dcontext, fd, info, pers.instrument_rw_len,
                                    instrument_persist_rw))
        goto coarse_unit_persist_exit;

    if (TESTANY(PERSCACHE_GENFILE_MD5_SHORT | PERSCACHE_GENFILE_MD5_COMPLETE,
                DYNAMO_OPTION(persist_gen_validation))) {
        byte *map = (byte *)&pers;
        uint which = DYNAMO_OPTION(persist_gen_validation);
        size_t sz = 0;
        if (TEST(PERSCACHE_GENFILE_MD5_COMPLETE, DYNAMO_OPTION(persist_gen_validation))) {
            /* FIXME if mmap up front (case 9758) don't need this mmap */
            sz = pers.header_len + pers.data_len - sizeof(persisted_footer_t);
            map = d_r_map_file(fd, &sz, 0, NULL, MEMPROT_READ,
                               /* won't change so save pagefile by not asking for COW */
                               MAP_FILE_REACHABLE);
            ASSERT(map != NULL);
            if (map == NULL) {
                /* give up */
                which &= ~PERSCACHE_GENFILE_MD5_COMPLETE;
            }
        }
        persist_calculate_self_digest(&footer.self_md5, &pers, map, which);
        DOLOG(1, LOG_CACHE,
              { print_module_digest(THREAD, &footer.self_md5, "self md5: "); });
        if (TEST(PERSCACHE_GENFILE_MD5_COMPLETE, DYNAMO_OPTION(persist_gen_validation)) &&
            map != (byte *)&pers) {
            ASSERT(map != NULL); /* we cleared _COMPLETE so shouldn't get here */
            d_r_unmap_file(map, sz);
        }
    } else {
        memset(&footer, 0, sizeof(footer));
    }
    footer.magic = PERSISTENT_CACHE_MAGIC;
    if (!write_persist_file(dcontext, fd, &footer, sizeof(footer)))
        goto coarse_unit_persist_exit; /* logs, stats are in write_persist_file */

    os_flush(fd);
    /* Temp file is all flushed and ready to go.  Only now do we try to replace
     * the original, to ensure we have a complete file (case 9696).
     * We can't rename it if we have a handle with write privileges open,
     * regardless of FILE_SHARE_* flags, so we close it and live with races.
     * Nobody else should be creating a file of the same name (tmpname includes process
     * id).  Therefore no other process may have racily created an
     * incomplete file overwriting the file we just produced and are
     * about to rename.
     */
    os_close(fd);
    fd = INVALID_FILE;

    success = coarse_unit_persist_rename(dcontext, filename, tmpname);
    if (!success) {
        LOG(THREAD, LOG_CACHE, 1, "  unable to rename to %s\n", filename);
        STATS_INC(persist_rename_tmp_fail);
        goto coarse_unit_persist_exit;
    }
    /* success */
    STATS_INC(coarse_units_persist);
    ASSERT(success);

coarse_unit_persist_exit:
    if (fd != INVALID_FILE)
        os_close(fd);
    if (!success && created_temp) {
        if (!os_delete_mapped_file(tmpname)) {
            LOG(THREAD, LOG_CACHE, 1, "  failed to delete on failure temp %s\n", tmpname);
            STATS_INC(persist_delete_tmp_fail);
        }
    }
    if (created_temp)
        d_r_mutex_unlock(&info->lock);
    if (free_info) {
        /* if we dup-ed when merging with disk, free now */
        coarse_unit_reset_free(dcontext, info, false /*no locks*/, false /*no unlink*/,
                               false /*not in use anyway*/);
        coarse_unit_free(dcontext, info);
    }
    KSTOP(persisted_generation);
    return success;
}

static bool
persist_check_option_compat(dcontext_t *dcontext, coarse_persisted_info_t *pers,
                            const char *option_string)
{
    const char *pers_options;
    ASSERT(option_string != NULL);

    if (os_tls_offset(0) != pers->tls_offs_base) {
        /* Bail out
         * WARNING: if we ever change our -tls_align default from 1 we should
         * consider implications on platform-independence of persisted caches.
         * FIXME: should we have release-build notification so we can know
         * when we have problems here?
         */
        LOG(THREAD, LOG_CACHE, 1, "  tls offset mismatch %d vs persisted %d\n",
            os_tls_offset(0), pers->tls_offs_base);
        STATS_INC(perscache_tls_mismatch);
        SYSLOG_INTERNAL_WARNING_ONCE("persistent cache tls offset mismatch");
        return false;
    }

    if (!TEST(PERSCACHE_SUPPORT_TRACES, pers->flags) && !DYNAMO_OPTION(disable_traces)) {
        /* We bail out; the consequences of continuing are huge performance
         * problems akin to -no_link_ibl.
         */
        LOG(THREAD, LOG_CACHE, 1, "  error: persisted cache has no trace support\n");
        STATS_INC(perscache_trace_mismatch);
        SYSLOG_INTERNAL_WARNING_ONCE("persistent cache trace support mismatch");
        return false;
    }

    /* we don't bother to split the ifdefs */
#if defined(RCT_IND_BRANCH) || defined(RETURN_AFTER_CALL)
    if ((!TEST(PERSCACHE_SUPPORT_RAC, pers->flags) && DYNAMO_OPTION(ret_after_call)) ||
        (!TEST(PERSCACHE_SUPPORT_RCT, pers->flags) &&
         (TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_call)) ||
          TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_jump))))) {
        LOG(THREAD, LOG_CACHE, 1, "  error: persisted cache has no RAC/RCT support\n");
        STATS_INC(perscache_rct_mismatch);
        SYSLOG_INTERNAL_WARNING_ONCE("persistent cache RAC/RCT support mismatch");
        return false;
    }
#endif

    if (DYNAMO_OPTION(persist_check_options)) {
        /* Option string is 1st data section; we persisted a NULL so we can
         * point directly at it. */
        if (pers->option_string_len == 0)
            pers_options = "";
        else
            pers_options = (const char *)(((byte *)pers) + pers->header_len);
        LOG(THREAD, LOG_CACHE, 2, "  checking pcache options \"%s\" vs current \"%s\"\n",
            pers_options, option_string);
        if (strcmp(option_string, pers_options) != 0) {
            /* Incompatible options!  In many cases the pcache may be safe to
             * execute but we play it conservatively and don't delve deeper (case 9799).
             * FIXME: put local options in name?!?
             */
            LOG(THREAD, LOG_CACHE, 1, "  error: options mismatch \"%s\" vs \"%s\"\n",
                pers_options, option_string);
            STATS_INC(perscache_options_mismatch);
            SYSLOG_INTERNAL_WARNING_ONCE("persistent cache options mismatch");
            return false;
        }
    }

    return true;
}

#ifdef DEBUG
/* case 10712: this would be a lot of stack space so we use static buffers
 * to avoid stack overflow
 */
DECLARE_NEVERPROT_VAR(static char pcache_dir_check_root[MAXIMUM_PATH], { 0 });
DECLARE_NEVERPROT_VAR(static char pcache_dir_check_temp[MAXIMUM_PATH], { 0 });

static void
pcache_dir_check_permissions(dcontext_t *dcontext, const char *filename)
{
    /* Test that we cannot rename per-user directory before
     * opening the file.  Prevented by sharing/in use
     * restrictions, not ACLs.  xref case 10504
     */
    /* `dirname filename` */
    const char *file_parent = double_strrchr(filename, DIRSEP, ALT_DIRSEP);
    size_t per_user_len;
    d_r_mutex_lock(&pcache_dir_check_lock);
    if (DYNAMO_OPTION(persist_per_app)) {
        /* back up one level higher */
        /* note that we only need to check whether the per-user directory
         * cannot be renamed by an attacker, while the per-user\per-app
         * directory should be created with correct ACLs
         */
        snprintf(pcache_dir_check_temp, BUFFER_SIZE_ELEMENTS(pcache_dir_check_temp),
                 "%.*s", file_parent - filename, filename);
        /* `dirname pcache_dir_check_temp` */
        per_user_len = double_strrchr(pcache_dir_check_temp, DIRSEP, ALT_DIRSEP) -
            pcache_dir_check_temp;
    } else {
        per_user_len = file_parent - filename;
    }
    snprintf(pcache_dir_check_root, BUFFER_SIZE_ELEMENTS(pcache_dir_check_root), "%.*s",
             per_user_len, filename);
    snprintf(pcache_dir_check_temp, BUFFER_SIZE_ELEMENTS(pcache_dir_check_temp),
             "%s-bumped", pcache_dir_check_root);
    LOG(THREAD, LOG_CACHE, 3, "  attempting rename %s -> %s\n", pcache_dir_check_root,
        pcache_dir_check_temp);
    ASSERT(!os_rename_file(pcache_dir_check_root, pcache_dir_check_temp, false) &&
           "directory can be bumped!");
    d_r_mutex_unlock(&pcache_dir_check_lock);
}
#endif

static file_t
persist_get_name_and_open(dcontext_t *dcontext, app_pc modbase, char *filename OUT,
                          uint filename_sz, char *option_buf OUT, uint option_buf_sz,
                          const char **option_string OUT, op_pcache_t *option_level OUT,
                          persisted_module_info_t *modinfo OUT _IF_DEBUG(app_pc start)
                              _IF_DEBUG(app_pc end))
{
    file_t fd = INVALID_FILE;

    /* case 9799: pcache-affecting options are part of name.
     * if -persist_check_exempted_options we must first try w/ the local
     * options and then the global, hence the do-while loop.
     */
    *option_level = persist_get_options_level(modbase, NULL, true /*force local*/);

    do {
        *option_string = persist_get_relevant_options(dcontext, option_buf, option_buf_sz,
                                                      *option_level);
        memset(modinfo, 0, sizeof(*modinfo));
        if (!get_persist_filename(filename, filename_sz, modbase, false /*read*/, modinfo,
                                  *option_string)) {
            LOG(THREAD, LOG_CACHE, 1,
                "  error computing name/excluded for " PFX "-" PFX "\n", start, end);
            STATS_INC(perscache_load_noname);
            return fd;
        }
        LOG(THREAD, LOG_CACHE, 1, "  persisted filename = %s\n", filename);

        if (DYNAMO_OPTION(validate_owner_dir)) {
            ASSERT(DYNAMO_OPTION(persist_per_user));
            if (perscache_user_directory == INVALID_FILE) {
                /* there is no alternative location to test - currently
                 * only single per-user directories supported
                 */
                LOG(THREAD, LOG_CACHE, 1,
                    "  directory is unsafe, cannot use persistent cache\n");
                return fd;
            }

            DOCHECK(1, { pcache_dir_check_permissions(dcontext, filename); });
        }

        /* On win32 OS_EXECUTE is required to create a section w/ rwx
         * permissions, which is in turn required to map a view w/ rwx
         */
        fd = os_open(filename,
                     OS_OPEN_READ | OS_EXECUTE |
                         /* Case 9964: we want to allow renaming while holding
                          * the file handle */
                         OS_SHARE_DELETE);
        if (fd == INVALID_FILE && *option_level == OP_PCACHE_LOCAL) {
            ASSERT(DYNAMO_OPTION(persist_check_exempted_options));
            LOG(THREAD, LOG_CACHE, 1, "  local-options file not found %s\n", filename);
            *option_level = OP_PCACHE_GLOBAL;
            /* try again */
        } else
            break;
    } while (true);

    if (fd == INVALID_FILE) {
        LOG(THREAD, LOG_CACHE, 1, "  error opening file %s\n", filename);
        STATS_INC(perscache_load_nofile);
    }

    return fd;
}

/* It's up to the caller to do the work of mark_executable_area_coarse_frozen().
 * Caller must hold read lock hotp_get_lock().
 */
coarse_info_t *
coarse_unit_load(dcontext_t *dcontext, app_pc start, app_pc end, bool for_execution)
{
    coarse_persisted_info_t *pers;
    persisted_footer_t *footer;
    coarse_info_t *info = NULL;
    char option_buf[MAX_PCACHE_OPTIONS_STRING];
    char filename[MAXIMUM_PATH];
    const char *option_string;
    op_pcache_t option_level;
    file_t fd = INVALID_FILE;
    byte *map = NULL, *map2 = NULL;
    size_t map_size = 0, map2_size = 0;
    uint64 file_size;
    size_t stubs_and_prefixes_len;
    byte *pc, *rx_pc, *rwx_pc;
    persisted_module_info_t modinfo;
    app_pc modbase = get_module_base(start);
    bool success = false;
    DEBUG_DECLARE(bool ok;)

    KSTART(persisted_load);
    DOLOG(1, LOG_CACHE, {
        char modname[MAX_MODNAME_INTERNAL];
        os_get_module_name_buf(modbase, modname, BUFFER_SIZE_ELEMENTS(modname));
        LOG(THREAD, LOG_CACHE, 1, "coarse_unit_load %s " PFX "-" PFX "%s\n", modname,
            start, end, for_execution ? "" : " (not for exec)");
    });
    DOSTATS({
        if (for_execution)
            STATS_INC(perscache_load_attempt);
        else
            STATS_INC(perscache_load_nox_attempt);
    });
#ifdef HOT_PATCHING_INTERFACE
    ASSERT_OWN_READWRITE_LOCK(DYNAMO_OPTION(hot_patching), hotp_get_lock());
#endif

    fd = persist_get_name_and_open(
        dcontext, modbase, filename, BUFFER_SIZE_ELEMENTS(filename), option_buf,
        BUFFER_SIZE_ELEMENTS(option_buf), &option_string, &option_level,
        &modinfo _IF_DEBUG(start) _IF_DEBUG(end));

    if (fd == INVALID_FILE) {
        /* stats done in persist_get_name_and_open */
        goto coarse_unit_load_exit;
    }
    if (DYNAMO_OPTION(validate_owner_file)) {
        if (!os_validate_user_owned(fd)) {
            SYSLOG_INTERNAL_ERROR_ONCE("%s not owned by current process!"
                                       " Persistent cache may be compromised, not using.",
                                       filename);
            /* note we don't attempt to overwrite here */
            goto coarse_unit_load_exit;
        }
    } else {
        ASSERT(!DYNAMO_OPTION(validate_owner_dir) ||
               perscache_user_directory != INVALID_FILE);
        /* note that if have a directory handle open, there is no need to
         * validate file with DYNAMO_OPTION(validate_owner_file) here.
         * of course it doesn't hurt to check both.
         */
        DOCHECK(1, {
            ASSERT_CURIOSITY(
                (!DYNAMO_OPTION(validate_owner_file) || os_validate_user_owned(fd)) &&
                "impostor not detected!");
        });
    }

    if (!os_get_file_size_by_handle(fd, &file_size)) {
        LOG(THREAD, LOG_CACHE, 1, "  error obtaining file size for %s\n", filename);
        goto coarse_unit_load_exit;
    }
    ASSERT_TRUNCATE(map_size, size_t, file_size);
    map_size = (size_t)file_size;
    LOG(THREAD, LOG_CACHE, 1, "  size of %s is " SZFMT "\n", filename, map_size);
    /* FIXME case 9642: control where in address space we map the file:
     * right after vmheap?  Randomized?
     */
    map = d_r_map_file(fd, &map_size, 0, NULL,
                       /* Ask for max, then restrict pieces */
                       MEMPROT_READ | MEMPROT_WRITE | MEMPROT_EXEC,
                       /* case 9599: asking for COW commits pagefile space
                        * up front, so we map two separate views later: see below
                        */
                       MAP_FILE_COPY_ON_WRITE /*writes should not change file*/ |
                           MAP_FILE_REACHABLE);
    /* case 9925: if we keep the file handle open we can prevent writes
     * to the file while it's mapped in, but it prevents our rename replacement
     * scheme (case 9701/9720) so we have it under option control.
     */
    if (!DYNAMO_OPTION(persist_lock_file)) {
        os_close(fd);
        fd = INVALID_FILE;
    }

    if (map == NULL) {
        LOG(THREAD, LOG_CACHE, 1, "  error mapping file %s\n", filename);
        goto coarse_unit_load_exit;
    }
    pers = (coarse_persisted_info_t *)map;
    ASSERT(pers->header_len + pers->data_len <= map_size &&
           ALIGN_FORWARD(pers->header_len + pers->data_len, PAGE_SIZE) ==
               ALIGN_FORWARD(map_size, PAGE_SIZE));
    footer = (persisted_footer_t *)(map + pers->header_len + pers->data_len -
                                    sizeof(persisted_footer_t));

    if (pers->magic != PERSISTENT_CACHE_MAGIC ||
        /* case 10160: don't crash trying to read footer */
        pers->header_len + pers->data_len > map_size ||
        ALIGN_FORWARD(pers->header_len + pers->data_len, PAGE_SIZE) !=
            ALIGN_FORWARD(map_size, PAGE_SIZE) ||
        footer->magic != PERSISTENT_CACHE_MAGIC ||
        TEST(PERSCACHE_CODE_INVALID, pers->flags)) {
        LOG(THREAD, LOG_CACHE, 1, "  invalid persisted file %s\n", filename);
        /* This flag shouldn't be set for on-disk units */
        ASSERT(!TEST(PERSCACHE_CODE_INVALID, pers->flags));
        STATS_INC(perscache_bad_file);
        /* We didn't have a footer prior to version 4 */
        ASSERT_CURIOSITY_ONCE(pers->version < 4 && "persistent cache file corrupt");
        goto coarse_unit_load_exit;
    }

    if (pers->version != PERSISTENT_CACHE_VERSION) {
        /* FIXME case 9655: should we clobber such a file when we persist? */
        LOG(THREAD, LOG_CACHE, 1, "  invalid persisted file version %d for %s\n",
            pers->version, filename);
        STATS_INC(perscache_version_mismatch);
        goto coarse_unit_load_exit;
    }

    if (!TEST(IF_X64_ELSE(PERSCACHE_X86_64, PERSCACHE_X86_32), pers->flags)) {
        LOG(THREAD, LOG_CACHE, 1, "  invalid architecture: not %s %s\n",
            IF_X64_ELSE("AMD64", "IA-32"), filename);
        STATS_INC(perscache_version_mismatch);
        SYSLOG_INTERNAL_WARNING_ONCE("persistent cache architecture mismatch");
        goto coarse_unit_load_exit;
    }

    /* Check file's md5 */
    if (TESTANY(PERSCACHE_GENFILE_MD5_SHORT | PERSCACHE_GENFILE_MD5_COMPLETE,
                DYNAMO_OPTION(persist_load_validation))) {
        module_digest_t self_md5;
        persist_calculate_self_digest(&self_md5, pers, map,
                                      DYNAMO_OPTION(persist_load_validation));
        DOLOG(1, LOG_CACHE, {
            print_module_digest(THREAD, &footer->self_md5, "md5 stored in file: ");
            print_module_digest(THREAD, &footer->self_md5, "md5 calculated:     ");
        });
        if ((TEST(PERSCACHE_GENFILE_MD5_SHORT, DYNAMO_OPTION(persist_load_validation)) &&
             !md5_digests_equal(self_md5.short_MD5, footer->self_md5.short_MD5)) ||
            (TEST(PERSCACHE_GENFILE_MD5_COMPLETE,
                  DYNAMO_OPTION(persist_load_validation)) &&
             !md5_digests_equal(self_md5.full_MD5, footer->self_md5.full_MD5))) {
            LOG(THREAD, LOG_CACHE, 1, "  file header md5 mismatch\n");
            STATS_INC(perscache_md5_mismatch);
            ASSERT_CURIOSITY_ONCE(false && "persistent cache md5 mismatch");
            goto coarse_unit_load_exit;
        }
    }

    /* Consistency with original module */
    persist_calculate_module_digest(&modinfo.module_md5, modbase,
                                    (size_t)modinfo.image_size,
                                    modbase + pers->start_offs, modbase + pers->end_offs,
                                    DYNAMO_OPTION(persist_load_validation));
    /* Compare the module digest and module fields (except base) all at once */
    if (!persist_modinfo_cmp(&modinfo, &pers->modinfo)) {
        LOG(THREAD, LOG_CACHE, 1, "  module info mismatch\n");
        DOLOG(1, LOG_CACHE, {
            LOG(THREAD, LOG_CACHE, 1, "modinfo stored in file: ");
            dump_buffer_as_bytes(THREAD, &pers->modinfo, sizeof(pers->modinfo),
                                 DUMP_RAW | DUMP_DWORD);
            LOG(THREAD, LOG_CACHE, 1, "\nmodinfo in memory:      ");
            dump_buffer_as_bytes(THREAD, &modinfo, sizeof(modinfo),
                                 DUMP_RAW | DUMP_DWORD);
            LOG(THREAD, LOG_CACHE, 1, "\n");
        });
        SYSLOG_INTERNAL_WARNING_ONCE("persistent cache module mismatch");
#ifdef WINDOWS
        if (modbase != pers->modinfo.base)
            persist_record_base_mismatch(modbase);
#endif
        STATS_INC(perscache_modinfo_mismatch);
        goto coarse_unit_load_exit;
    }

    /* modinfo cmp checks all but base, which is separate for reloc support */
    if (modbase != pers->modinfo.base) {
#ifdef UNIX
        /* for linux, we can trust lack of textrel flag as guaranteeing no text relocs */
        if (DYNAMO_OPTION(persist_trust_textrel) &&
            /* XXX: may not be at_map if re-add coarse post-load (e.g., IAT or other
             * special cases): how know?
             */
            !module_has_text_relocs(modbase,
                                    /* If !for_execution we're freezing at exit
                                     * or sthg and not at_map.
                                     */
                                    for_execution && dynamo_initialized /*at_map*/)) {
            LOG(THREAD, LOG_CACHE, 1,
                "  module base mismatch " PFX " vs persisted " PFX
                ", but no text relocs so ok\n",
                modbase, pers->modinfo.base);
        } else {
#endif
            /* FIXME case 9581/9649: Bail out for now since relocs NYI
             * Once we do support them, make sure to do the right thing when merging:
             * current code will always apply relocs before merging.
             */
            LOG(THREAD, LOG_CACHE, 1,
                "  module base mismatch " PFX " vs persisted " PFX "\n", modbase,
                pers->modinfo.base);
#ifdef WINDOWS
            persist_record_base_mismatch(modbase);
#endif
            STATS_INC(perscache_base_mismatch);
            goto coarse_unit_load_exit;
#ifdef UNIX
        }
#endif
    }

    if (modbase + pers->start_offs < start || modbase + pers->end_offs > end) {
        /* case 9653: only load a file that covers a subset of the vmarea region */
        LOG(THREAD, LOG_CACHE, 1,
            "  region mismatch " PFX "-" PFX " vs persisted " PFX "-" PFX "\n", start,
            end, modbase + pers->start_offs, modbase + pers->end_offs);
        STATS_INC(perscache_region_mismatch);
        goto coarse_unit_load_exit;
    }

    if (!persist_check_option_compat(dcontext, pers, option_string)) {
        goto coarse_unit_load_exit;
    }

    stubs_and_prefixes_len =
        (pers->stubs_len + pers->ibl_jmp_prefix_len + pers->ibl_call_prefix_len +
         pers->ibl_ret_prefix_len + pers->trace_head_return_prefix_len +
         pers->fcache_return_prefix_len);

    if (TEST(PERSCACHE_MAP_RW_SEPARATE, pers->flags) &&
        DYNAMO_OPTION(persist_map_rw_separate)) {
        size_t ro_size;
        map2_size = stubs_and_prefixes_len + sizeof(persisted_footer_t);
        ro_size = (size_t)file_size /*un-aligned*/ - map2_size;
        ASSERT(ro_size ==
               ALIGN_FORWARD(pers->header_len + pers->data_len - stubs_and_prefixes_len -
                                 pers->view_pad_len - sizeof(persisted_footer_t),
                             MAP_FILE_VIEW_ALIGNMENT));
        LOG(THREAD, LOG_CACHE, 2,
            "  attempting double mapping: size " PIFX " and " PIFX "\n", ro_size,
            map2_size);
        if (!DYNAMO_OPTION(persist_lock_file)) {
            fd = os_open(filename, OS_OPEN_READ | OS_EXECUTE | OS_SHARE_DELETE);
            /* FIXME: case 10547 what do we know about this second
             * file that we just opened?  Nothing guarantees it is
             * still the same one we used earlier.
             */
            if (fd != INVALID_FILE && DYNAMO_OPTION(validate_owner_file)) {
                if (!os_validate_user_owned(fd)) {
                    os_close(fd);
                    fd = INVALID_FILE;
                    SYSLOG_INTERNAL_ERROR_ONCE("%s not owned by current process!"
                                               " Persistent cache may be compromised, "
                                               "not using.",
                                               filename);
                }
            }
        }
        ASSERT(fd != INVALID_FILE);
        if (fd != INVALID_FILE) {
            d_r_unmap_file(map, map_size);
            pers = NULL;
            map_size = ro_size;
            /* we know the whole thing fit @map, so try there */
            map = d_r_map_file(fd, &map_size, 0, map,
                               /* Ask for max, then restrict pieces */
                               MEMPROT_READ | MEMPROT_EXEC,
                               /* no COW to avoid pagefile cost */
                               MAP_FILE_REACHABLE);
            map2 = d_r_map_file(fd, &map2_size, map_size, map + map_size,
                                /* Ask for max, then restrict pieces */
                                MEMPROT_READ | MEMPROT_WRITE | MEMPROT_EXEC,
                                MAP_FILE_COPY_ON_WRITE /*writes should not change file*/ |
                                    MAP_FILE_REACHABLE);
            /* FIXME: try again if racy alloc and they both don't fit */
            if (!DYNAMO_OPTION(persist_lock_file)) {
                os_close(fd);
                fd = INVALID_FILE;
            }
            if (map == NULL || map2 != map + ro_size) {
                SYSLOG_INTERNAL_ERROR_ONCE("double perscache mapping failed");
                LOG(THREAD, LOG_CACHE, 1,
                    "  error: 2nd map " PFX " not adjacent to 1st " PFX "\n", map, map2);
                STATS_INC(perscache_maps_not_adjacent);
                goto coarse_unit_load_exit;
            }
            /* the two views are adjacent so most code can treat it as one view */
            LOG(THREAD, LOG_CACHE, 1, "  mapped view1 @" PFX " and view2 @" PFX "\n", map,
                map2);
            pers = (coarse_persisted_info_t *)map;
        }
    }

    /* We assume that once info!=NULL we have been successful, though we do
     * abort for hotp or client conflicts below */
    info = coarse_unit_create(modbase + pers->start_offs, modbase + pers->end_offs,
                              &modinfo.module_md5, for_execution);
    info->frozen = true;
    info->persisted = true;
    info->has_persist_info = true;
    info->persist_base = pers->modinfo.base;
    info->mod_shift = (pers->modinfo.base - modbase);
    info->mmap_pc = map;
    if (map2 != NULL) {
        info->mmap_ro_size = map_size;
        info->mmap_size = map_size + map2_size;
        ASSERT(map2 == info->mmap_pc + info->mmap_ro_size);
    } else
        info->mmap_size = map_size;
    ASSERT(ALIGN_FORWARD(info->mmap_size, PAGE_SIZE) ==
           ALIGN_FORWARD(pers->header_len + pers->data_len, PAGE_SIZE));
    if (DYNAMO_OPTION(persist_lock_file))
        info->fd = fd;

    info->flags = pers->flags;
#if defined(RETURN_AFTER_CALL) && defined(WINDOWS)
    /* FIXME: provide win32/ interface for setting the flag? */
    if (TEST(PERSCACHE_SEEN_BORLAND_SEH, pers->flags) && !seen_Borland_SEH) {
        SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
        seen_Borland_SEH = true;
        SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
    }
#endif
    ASSERT(option_level != OP_PCACHE_LOCAL ||
           TEST(PERSCACHE_EXEMPTION_OPTIONS, info->flags));

    /* Process data sections (other than option string) in reverse order */
    pc = map + pers->header_len + pers->data_len; /* end of file */
    pc -= sizeof(persisted_footer_t);

    pc -= pers->instrument_rw_len;
    if (pers->instrument_rw_len > 0) {
        if (!instrument_resurrect_rw(GLOBAL_DCONTEXT, info, pc))
            goto coarse_unit_load_exit;
    }

    info->stubs_end_pc = pc;
    pc -= stubs_and_prefixes_len;
    info->fcache_return_prefix = pc;
    rwx_pc = info->fcache_return_prefix;
    ASSERT(ALIGNED(rwx_pc, PAGE_SIZE));
    /* To patch up the prefixes, it's just as easy to re-emit them all as
     * most are just plain jmps anyway
     */
    info->stubs_start_pc =
        coarse_stubs_create(info, info->fcache_return_prefix, stubs_and_prefixes_len);
    /* We could omit these from the file and assume their sizes */
    DOCHECK(1, {
        byte *check = info->fcache_return_prefix;
        check += pers->fcache_return_prefix_len;
        ASSERT(check == info->trace_head_return_prefix);
        check += pers->trace_head_return_prefix_len;
        ASSERT(check == info->ibl_ret_prefix);
        check += pers->ibl_ret_prefix_len;
        ASSERT(check == info->ibl_call_prefix);
        check += pers->ibl_call_prefix_len;
        ASSERT(check == info->ibl_jmp_prefix);
        check += pers->ibl_jmp_prefix_len;
        ASSERT(check == info->stubs_start_pc);
        check += pers->stubs_len;
        ASSERT(check == info->stubs_end_pc);
    });
    if (DYNAMO_OPTION(persist_protect_stubs)) {
        /* Mark prefixes + stubs read-only.  Note that we can't map in as +rx, since
         * we must have +w to get COW (unless we switch to MEM_IMAGE).  So we have
         * no way to avoid the pagefile cost, except the mysterious case 10074,
         * which we take advantage of here by touching every page (yes it works
         * even after the prefix write above), paying a one-time working set cost.
         */
        if (DYNAMO_OPTION(persist_touch_stubs)) {
            app_pc touch_pc = rwx_pc;
            volatile byte touch_value UNUSED;
            for (; touch_pc < (app_pc)(map + pers->header_len + pers->data_len);
                 touch_pc += PAGE_SIZE) {
                touch_value = *touch_pc;
                STATS_INC(pcache_stub_touched);
            }
        }
        DEBUG_DECLARE(ok =)
        set_protection(rwx_pc, map + pers->header_len + pers->data_len - rwx_pc,
                       MEMPROT_READ | MEMPROT_EXEC);
        ASSERT(ok);
        info->stubs_readonly = true;
    } else {
        /* FIXME case 9650: we could mark the prefixes as read-only now, if we put them
         * on their own page
         */
    }

    info->cache_end_pc = pc - pers->post_cache_pad_len;
    pc -= pers->cache_len;
    info->cache_start_pc = pc;
    pc -= pers->instrument_rx_len;
    rx_pc = pc;
    ASSERT(ALIGNED(rx_pc, PAGE_SIZE));
    fcache_coarse_init_frozen(dcontext, info, info->cache_start_pc,
                              info->fcache_return_prefix - info->cache_start_pc);

    pc -= pers->view_pad_len; /* just skip it */
    if (pers->instrument_rx_len > 0) {
        if (!instrument_resurrect_rx(GLOBAL_DCONTEXT, info, pc))
            goto coarse_unit_load_exit;
    }
    pc -= pers->pad_len; /* just skip it */

    pc -= pers->stub_htable_len;
    fragment_coarse_htable_resurrect(GLOBAL_DCONTEXT, info, false /*stub table*/, pc);
    ASSERT(fragment_coarse_htable_persist_size(dcontext, info, false /*stub*/) ==
           pers->stub_htable_len);
    pc -= pers->cache_htable_len;
    fragment_coarse_htable_resurrect(GLOBAL_DCONTEXT, info, true /*cache table*/, pc);
    ASSERT(fragment_coarse_htable_persist_size(dcontext, info, true /*cache*/) ==
           pers->cache_htable_len);
    ASSERT(offsetof(coarse_persisted_info_t, cache_htable_len) < pers->header_len);

    /* From here on out, check offsets so will work w/ earlier-versioned file
     */

    if (offsetof(coarse_persisted_info_t, rct_htable_len) < pers->header_len) {
        pc -= pers->rct_htable_len;
#ifdef RCT_IND_BRANCH
        if (pers->rct_htable_len > 0) {
            /* We may be merging with the on-disk file, so load it in
             * regardless of whether we care in this process instance
             * Use GLOBAL_DCONTEXT in case we do use.
             */
            info->rct_table = rct_table_resurrect(GLOBAL_DCONTEXT, pc, RCT_RCT);
            ASSERT(info->rct_table != NULL);
            if (for_execution &&
                (TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_call)) ||
                 TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_jump))) &&
                (DYNAMO_OPTION(use_persisted_rct) ||
                 TEST(PERSCACHE_SEEN_BORLAND_SEH, pers->flags)) &&
                /* Case 9834: avoid double-add from earlier entire-module resurrect */
                !os_module_get_flag(info->base_pc, MODULE_RCT_LOADED)) {
                /* load into the official module table */
                DEBUG_DECLARE(bool used =)
                rct_module_table_set(GLOBAL_DCONTEXT, modbase, info->rct_table, RCT_RCT);
                ASSERT(used);
#    ifdef WINDOWS
                if (TEST(PERSCACHE_ENTIRE_MODULE_RCT, pers->flags)) {
                    /* mark as not needing to be analyzed */
                    os_module_set_flag(info->base_pc, MODULE_RCT_LOADED);
                }
#    endif
            }
        }
#endif
    }
    if (offsetof(coarse_persisted_info_t, rac_htable_len) < pers->header_len) {
        pc -= pers->rac_htable_len;
#ifdef RETURN_AFTER_CALL
        if (pers->rac_htable_len > 0) {
            /* We may be merging with the on-disk file, so load it in
             * regardless of whether we care in this process instance.
             * Use GLOBAL_DCONTEXT in case we do use.
             */
            info->rac_table = rct_table_resurrect(GLOBAL_DCONTEXT, pc, RCT_RAC);
            ASSERT(info->rac_table != NULL);
            if (for_execution && DYNAMO_OPTION(ret_after_call)) {
                /* load into the official module table */
                DEBUG_DECLARE(bool used =)
                rct_module_table_set(GLOBAL_DCONTEXT, modbase, info->rac_table, RCT_RAC);
                ASSERT(used);
            }
        }
#endif
    }

    /* FIXME case 9581 NYI: reloc section */
    if (offsetof(coarse_persisted_info_t, reloc_len) < pers->header_len) {
        pc -= pers->reloc_len;
    }

#ifdef HOT_PATCHING_INTERFACE
    if (offsetof(coarse_persisted_info_t, hotp_patch_list_len) < pers->header_len) {
        /* Case 9995: store list of active patch points in perscache file and don't
         * abort if those are the only matching points at this time.
         * FIXME if we had an efficient way to see if an app pc is present
         * in the pcache (case 9969) we could be more precise.
         */
        pc -= pers->hotp_patch_list_len;
        IF_X64(ASSERT_TRUNCATE(info->hotp_ppoint_vec_num, uint,
                               pers->hotp_patch_list_len / sizeof(app_rva_t)));
        info->hotp_ppoint_vec_num = (uint)pers->hotp_patch_list_len / sizeof(app_rva_t);
        if (info->hotp_ppoint_vec_num > 0)
            info->hotp_ppoint_vec = (app_rva_t *)pc;
    } else
        info->hotp_ppoint_vec_num = 0;
    ASSERT(info->hotp_ppoint_vec == NULL || info->hotp_ppoint_vec_num > 0);
    if (DYNAMO_OPTION(hot_patching) &&
        hotp_point_not_on_list(info->base_pc, info->end_pc, true /*own hotp lock*/,
                               info->hotp_ppoint_vec, info->hotp_ppoint_vec_num)) {
        LOG(THREAD, LOG_CACHE, 1, "  error: hotp match prevents using persistence\n");
        STATS_INC(perscache_hotp_conflict);
        /* FIXME: we could duplicate the calculation of info->hotp_ppoint_vec
         * to detect this error before allocating info.  Instead we have to
         * free what we've created.  A bonus of waiting to detect the error
         * is that we've already loaded the RCT entries, which the error does
         * not invalidate.
         */
        goto coarse_unit_load_exit;
    }
#endif

    /* we go ahead and check offset here, but we inserted fields above to keep
     * in a sane order and bumped the version.  currently no backcompat anyway.
     */
    if (offsetof(coarse_persisted_info_t, instrument_ro_len) < pers->header_len) {
        pc -= pers->instrument_ro_len;
        if (pers->instrument_ro_len > 0) {
            if (!instrument_resurrect_ro(GLOBAL_DCONTEXT, info, pc))
                goto coarse_unit_load_exit;
        }
    }

    ASSERT(pc - map >= (int)pers->header_len);

    DEBUG_DECLARE(ok =)
    set_protection(map, rx_pc - map, MEMPROT_READ);
    ASSERT(ok);
    DEBUG_DECLARE(ok =)
    set_protection(rx_pc, rwx_pc - rx_pc, MEMPROT_READ | MEMPROT_EXEC);
    ASSERT(ok);

    /* FIXME case 9648: don't forget to append a guard page -- but we
     * should only need it if we fill up the full allocation region,
     * so on win32 we will get a free guard page whenever we're not
     * end-aligned to 64KB.
     */
    RSTATS_INC(perscache_loaded);
    success = true;

coarse_unit_load_exit:
    if (!success) {
        if (info != NULL) {
            /* XXX: see hotp notes above: better to determine invalidation
             * prior to creating info.  Note that RCT entries are not reset here.
             */
            coarse_unit_reset_free_internal(dcontext, info, false /*no locks*/,
                                            false /*not linked yet*/,
                                            true /*give up primary*/,
                                            /* case 11064: avoid rank order */
                                            false /*local, no info lock needed*/);
            coarse_unit_free(dcontext, info);
            /* reset_free unmaps map and map2 for us */
            info = NULL;
        } else {
            if (map != NULL)
                d_r_unmap_file(map, map_size);
            if (map2 != NULL)
                d_r_unmap_file(map2, map2_size);
            if (fd != INVALID_FILE)
                os_close(fd);
        }
    }
    KSTOP(persisted_load);
    return info;
}

bool
exists_coarse_ibl_pending_table(dcontext_t *dcontext, /* in case per-thread someday */
                                coarse_info_t *info, ibl_branch_type_t branch_type)
{
#if defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH)
    bool exists = false;
    if (info != NULL) {
        if (branch_type == IBL_RETURN)
            exists = rct_module_persisted_table_exists(dcontext, info->base_pc, RCT_RAC);
        else if (branch_type == IBL_INDCALL)
            exists = rct_module_persisted_table_exists(dcontext, info->base_pc, RCT_RCT);
        else {
            ASSERT(branch_type == IBL_INDJMP);
            exists =
                rct_module_persisted_table_exists(dcontext, info->base_pc, RCT_RCT) ||
                rct_module_persisted_table_exists(dcontext, info->base_pc, RCT_RAC);
        }
        return (exists &&
                !TEST(COARSE_FILL_IBL_MASK(branch_type), info->ibl_pending_used));
    }
#endif
    return false;
}

/* If pc is in a module, marks that module as exempted (case 9799) */
void
mark_module_exempted(app_pc pc)
{
    if (DYNAMO_OPTION(persist_check_options) &&
        /* if persist_check_local_options is on no reason to track exemptions */
        !DYNAMO_OPTION(persist_check_local_options) &&
        DYNAMO_OPTION(persist_check_exempted_options) && module_info_exists(pc) &&
        !os_module_get_flag(pc, MODULE_WAS_EXEMPTED)) {
        LOG(GLOBAL, LOG_VMAREAS, 1, "marking module @" PFX " as exempted\n");
        os_module_set_flag(pc, MODULE_WAS_EXEMPTED);
    }
}
