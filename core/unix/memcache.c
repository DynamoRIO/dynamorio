/* *******************************************************************************
 * Copyright (c) 2010-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2011 Massachusetts Institute of Technology  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
 * *******************************************************************************/

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
 * memquery_linux.c - memory querying via /proc/self/maps
 */

#include "../globals.h"
#include "memcache.h"
#include "memquery.h"
#include "os_private.h"
#include <sys/mman.h>
#include "instrument.h"

#ifdef HAVE_MEMINFO_QUERY
#    error Should use direct query and no cache
#endif

/* XXX; the separation of allmem from platforms that don't need it is not entirely
 * clean.  We have all_memory_areas_{lock,unlock}(), update_all_memory_areas(),
 * and remove_from_all_memory_areas() declared in os_shared.h and nop-ed out where
 * not needed; DYNAMO_OPTION(use_all_memory_areas); some calls into here inside
 * ifndef HAVE_MEMINFO_QUERY or IF_NO_MEMQUERY().
 */

/* Track all memory regions seen by DR. We track these ourselves to prevent
 * repeated reads of /proc/self/maps (case 3771). An allmem_info_t struct is
 * stored in the custom field.
 * all_memory_areas is updated along with dynamo_areas, due to cyclic
 * dependencies.
 */
/* exported for debug to avoid rank order in print_vm_area() */
IF_DEBUG_ELSE(, static) vm_area_vector_t *all_memory_areas;

typedef struct _allmem_info_t {
    uint prot;
    dr_mem_type_t type;
    bool shareable;
    bool vdso;
    bool dr_vmm;
} allmem_info_t;

static void
allmem_info_free(void *data);
static void *
allmem_info_dup(void *data);
static bool
allmem_should_merge(bool adjacent, void *data1, void *data2);
static void *
allmem_info_merge(void *dst_data, void *src_data);

/* HACK to make all_memory_areas->lock recursive
 * protected for both read and write by all_memory_areas->lock
 * FIXME: provide general rwlock w/ write portion recursive
 * FIXME: eliminate duplicate code (see dynamo_areas_recursion)
 */
DECLARE_CXTSWPROT_VAR(uint all_memory_areas_recursion, 0);

void
memcache_init(void)
{
    /* Need to be after heap_init */
    VMVECTOR_ALLOC_VECTOR(all_memory_areas, GLOBAL_DCONTEXT, VECTOR_SHARED,
                          all_memory_areas);
    vmvector_set_callbacks(all_memory_areas, allmem_info_free, allmem_info_dup,
                           allmem_should_merge, allmem_info_merge);
}

void
memcache_exit(void)
{
    vmvector_delete_vector(GLOBAL_DCONTEXT, all_memory_areas);
    all_memory_areas = NULL;
}

bool
memcache_initialized(void)
{
    return (all_memory_areas != NULL && !vmvector_empty(all_memory_areas) &&
            /* not really set until vm_areas_init() */
            dynamo_initialized);
}

/* HACK to get recursive write lock for internal and external use
 * FIXME: code blatantly copied from dynamo_vm_areas_{un}lock(); eliminate duplication!
 */
void
memcache_lock(void)
{
    /* ok to ask for locks or mark stale before all_memory_areas is allocated,
     * during heap init and before we can allocate it.  no lock needed then.
     */
    ASSERT(all_memory_areas != NULL ||
           d_r_get_num_threads() <= 1 /* must be only DR thread */);
    if (all_memory_areas == NULL)
        return;
    if (self_owns_write_lock(&all_memory_areas->lock)) {
        all_memory_areas_recursion++;
        /* we have a 5-deep path:
         *   global_heap_alloc | heap_create_unit | get_guarded_real_memory |
         *   heap_low_on_memory | release_guarded_real_memory
         */
        ASSERT_CURIOSITY(all_memory_areas_recursion <= 4);
    } else
        d_r_write_lock(&all_memory_areas->lock);
}

void
memcache_unlock(void)
{
    /* ok to ask for locks or mark stale before all_memory_areas is allocated,
     * during heap init and before we can allocate it.  no lock needed then.
     */
    ASSERT(all_memory_areas != NULL ||
           d_r_get_num_threads() <= 1 /*must be only DR thread*/);
    if (all_memory_areas == NULL)
        return;
    if (all_memory_areas_recursion > 0) {
        ASSERT_OWN_WRITE_LOCK(true, &all_memory_areas->lock);
        all_memory_areas_recursion--;
    } else
        d_r_write_unlock(&all_memory_areas->lock);
}

/* vmvector callbacks */
static void
allmem_info_free(void *data)
{
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, data, allmem_info_t, ACCT_MEM_MGT, PROTECTED);
}

static void *
allmem_info_dup(void *data)
{
    allmem_info_t *src = (allmem_info_t *)data;
    allmem_info_t *dst =
        HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, allmem_info_t, ACCT_MEM_MGT, PROTECTED);
    ASSERT(src != NULL);
    *dst = *src;
    return dst;
}

static bool
allmem_should_merge(bool adjacent, void *data1, void *data2)
{
    allmem_info_t *i1 = (allmem_info_t *)data1;
    allmem_info_t *i2 = (allmem_info_t *)data2;
    /* we do want to merge identical regions, whether overlapping or
     * adjacent, to avoid continual splitting due to mprotect
     * fragmenting our list
     */
    return (i1->prot == i2->prot && i1->type == i2->type &&
            i1->shareable == i2->shareable &&
            /* i#1583: kernel doesn't merge 2-page vdso after we hook vsyscall */
            !i1->vdso && !i2->vdso &&
            /* kernel doesn't merge app anon region with vmheap */
            !i1->dr_vmm && !i2->dr_vmm);
}

static void *
allmem_info_merge(void *dst_data, void *src_data)
{
    DOCHECK(1, {
        allmem_info_t *src = (allmem_info_t *)src_data;
        allmem_info_t *dst = (allmem_info_t *)dst_data;
        ASSERT(allmem_should_merge(true, dst, src));
    });
    allmem_info_free(src_data);
    return dst_data;
}

static void
sync_all_memory_areas(void)
{
    /* The all_memory_areas list has the same circular dependence issues
     * as the dynamo_areas list.  For allocs outside of vmheap we can
     * be out of sync.
     */
    if (are_dynamo_vm_areas_stale()) {
        /* Trigger a sync */
        dynamo_vm_area_overlap((app_pc)NULL, (app_pc)1);
    }
}

/* caller should call sync_all_memory_areas first */
static void
add_all_memory_area(app_pc start, app_pc end, uint prot, int type, bool shareable)
{
    allmem_info_t *info;
    ASSERT(ALIGNED(start, PAGE_SIZE));
    ASSERT_OWN_WRITE_LOCK(true, &all_memory_areas->lock);
    LOG(GLOBAL, LOG_VMAREAS | LOG_SYSCALLS, 3,
        "update_all_memory_areas: adding: " PFX "-" PFX " prot=%d type=%d share=%d\n",
        start, end, prot, type, shareable);
    info = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, allmem_info_t, ACCT_MEM_MGT, PROTECTED);
    info->prot = prot;
    ASSERT(type >= 0);
    info->type = type;
    info->shareable = shareable;
    info->vdso = (start == vsyscall_page_start);
    info->dr_vmm = is_vmm_reserved_address(start, 1, NULL, NULL);
    vmvector_add(all_memory_areas, start, end, (void *)info);
}

void
memcache_update(app_pc start, app_pc end_in, uint prot, int type)
{
    allmem_info_t *info;
    app_pc end = (app_pc)ALIGN_FORWARD(end_in, PAGE_SIZE);
    bool removed;
    ASSERT(ALIGNED(start, PAGE_SIZE));
    /* all_memory_areas lock is held higher up the call chain to avoid rank
     * order violation with heap_unit_lock */
    ASSERT_OWN_WRITE_LOCK(true, &all_memory_areas->lock);
    sync_all_memory_areas();
    LOG(GLOBAL, LOG_VMAREAS, 4, "update_all_memory_areas " PFX "-" PFX " %d %d\n", start,
        end_in, prot, type);
    DOLOG(5, LOG_VMAREAS, memcache_print(GLOBAL, ""););

    if (type == -1) {
        /* to preserve existing types we must iterate b/c we cannot
         * merge images into data
         */
        app_pc pc, sub_start, sub_end, next_add = start;
        pc = start;
        /* XXX i#704: pointer overflow is not guaranteed to behave like
         * arithmetic overflow: need better handling here, though most problems
         * we've seen have been on "pc + x < pc" checks where the addition is
         * built into the comparison and the compiler can say "won't happen".
         */
        while (pc < end && pc >= start /*overflow*/ &&
               vmvector_lookup_data(all_memory_areas, pc, &sub_start, &sub_end,
                                    (void **)&info)) {
            if (info->type == DR_MEMTYPE_IMAGE) {
                bool shareable = false;
                app_pc overlap_end;
                dr_mem_type_t info_type = info->type;
                /* process prior to image */
                if (next_add < sub_start) {
                    vmvector_remove(all_memory_areas, next_add, pc);
                    add_all_memory_area(next_add, pc, prot, DR_MEMTYPE_DATA, false);
                }
                next_add = sub_end;
                /* change image prot */
                overlap_end = (sub_end > end) ? end : sub_end;
                if (sub_start == pc && sub_end == overlap_end) {
                    /* XXX: we should read maps to fully handle COW but for
                     * now we do some simple checks to prevent merging
                     * private with shareable regions
                     */
                    /* We assume a writable transition is accompanied by an actual
                     * write => COW => no longer shareable (i#669)
                     */
                    shareable = info->shareable;
                    if (TEST(MEMPROT_WRITE, prot) != TEST(MEMPROT_WRITE, info->prot))
                        shareable = false;
                    /* re-add so we can merge w/ adjacent non-shareable */
                } else {
                    /* assume we're here b/c was written and now marked +rx or sthg
                     * so no sharing
                     */
                    shareable = false;
                }
                vmvector_remove(all_memory_areas, pc, overlap_end);
                add_all_memory_area(pc, overlap_end, prot, info_type, shareable);
            }
            pc = sub_end;
        }
        /* process after last image */
        if (next_add < end) {
            vmvector_remove(all_memory_areas, next_add, end);
            add_all_memory_area(next_add, end, prot, DR_MEMTYPE_DATA, false);
        }
    } else {
        if (vmvector_overlap(all_memory_areas, start, end)) {
            LOG(THREAD_GET, LOG_VMAREAS | LOG_SYSCALLS, 4,
                "update_all_memory_areas: overlap found, removing and adding: " PFX
                "-" PFX " prot=%d\n",
                start, end, prot);
            /* New region to be added overlaps with one or more existing
             * regions.  Split already existing region(s) accordingly and add
             * the new region */
            removed = vmvector_remove(all_memory_areas, start, end);
            ASSERT(removed);
        }
        add_all_memory_area(start, end, prot, type, type == DR_MEMTYPE_IMAGE);
    }
    LOG(GLOBAL, LOG_VMAREAS, 5, "update_all_memory_areas " PFX "-" PFX " %d %d: post:\n",
        start, end_in, prot, type);
    DOLOG(5, LOG_VMAREAS, memcache_print(GLOBAL, ""););
}

void
memcache_update_locked(app_pc start, app_pc end, uint prot, int type, bool exists)
{
    memcache_lock();
    /* A curiosity as it can happen when attaching to a many-threaded
     * app (e.g., the api.detach_spawn test), or when dr_app_setup is
     * separate from dr_app_start (i#2037).
     */
    ASSERT_CURIOSITY(!exists || vmvector_overlap(all_memory_areas, start, end) ||
                     /* we could synch up: instead we relax the assert if DR areas not
                      * in allmem
                      */
                     are_dynamo_vm_areas_stale() || !dynamo_initialized);
    LOG(GLOBAL, LOG_VMAREAS, 3, "\tupdating all_memory_areas " PFX "-" PFX " prot->%d\n",
        start, end, prot);
    memcache_update(start, end, prot, type);
    memcache_unlock();
}

bool
memcache_remove(app_pc start, app_pc end)
{
    bool ok;
    DEBUG_DECLARE(dcontext_t *dcontext = get_thread_private_dcontext());
    ok = vmvector_remove(all_memory_areas, start, end);
    LOG(THREAD, LOG_VMAREAS | LOG_SYSCALLS, 3,
        "remove_from_all_memory_areas: %s: " PFX "-" PFX "\n",
        ok ? "removed" : "not found", start, end);
    return ok;
}

bool
memcache_query_memory(const byte *pc, OUT dr_mem_info_t *out_info)
{
    allmem_info_t *info;
    bool found;
    app_pc start, end;
    ASSERT(out_info != NULL);
    memcache_lock();
    sync_all_memory_areas();
    if (vmvector_lookup_data(all_memory_areas, (app_pc)pc, &start, &end,
                             (void **)&info)) {
        ASSERT(info != NULL);
        out_info->base_pc = start;
        out_info->size = (end - start);
        out_info->prot = info->prot;
        out_info->type = info->type;
#ifdef HAVE_MEMINFO
        DOCHECK(2, {
            byte *from_os_base_pc;
            size_t from_os_size;
            uint from_os_prot;
            found = get_memory_info_from_os(pc, &from_os_base_pc, &from_os_size,
                                            &from_os_prot);
            ASSERT(found);
            /* we merge adjacent identical-prot image sections: .bss into .data,
             * DR's various data segments, etc., so that mismatch is ok.
             */
            if ((from_os_prot == info->prot ||
                 /* allow maps to have +x (PR 213256)
                  * +x may be caused by READ_IMPLIES_EXEC set in personality flag (i#262)
                  */
                 (from_os_prot & (~MEMPROT_EXEC)) ==
                     info->prot
                         /* DrMem#1778, i#1861: we have fake flags */
                         IF_LINUX(||
                                  (from_os_prot & (~MEMPROT_META_FLAGS)) ==
                                      (info->prot & (~MEMPROT_META_FLAGS)))) &&
                ((info->type == DR_MEMTYPE_IMAGE && from_os_base_pc >= start &&
                  from_os_size <= (end - start)) ||
                 (from_os_base_pc == start && from_os_size == (end - start)))) {
                /* ok.  easier to think of forward logic. */
            } else {
                /* /proc/maps could break/combine regions listed so region bounds as
                 * listed by all_memory_areas and /proc/maps won't agree.
                 * FIXME: Have seen instances where all_memory_areas lists the region as
                 * r--, where /proc/maps lists it as r-x.  Infact, all regions listed in
                 * /proc/maps are executable, even guard pages --x (see case 8821)
                 */
                /* We add the whole client lib as a single entry.  Unfortunately we
                 * can't safely ask about aux client libs so we have to ignore them here
                 * (else we hit a rank order violation: i#5127).
                 */
                if (!is_in_client_lib_ignore_aux(start) ||
                    !is_in_client_lib_ignore_aux(end - 1)) {
                    SYSLOG_INTERNAL_WARNING_ONCE(
                        "get_memory_info mismatch! "
                        "(can happen if os combines entries in /proc/pid/maps)\n"
                        "\tos says: " PFX "-" PFX " prot=0x%08x\n"
                        "\tcache says: " PFX "-" PFX " prot=0x%08x\n",
                        from_os_base_pc, from_os_base_pc + from_os_size, from_os_prot,
                        start, end, info->prot);
                }
            }
        });
#endif
    } else {
        app_pc prev, next;
        found = vmvector_lookup_prev_next(all_memory_areas, (app_pc)pc, &prev, NULL,
                                          &next, NULL);
        ASSERT(found);
        if (prev != NULL) {
            found = vmvector_lookup_data(all_memory_areas, prev, NULL, &out_info->base_pc,
                                         NULL);
            ASSERT(found);
        } else
            out_info->base_pc = NULL;
        out_info->size = (next - out_info->base_pc);
        out_info->prot = MEMPROT_NONE;
        out_info->type = DR_MEMTYPE_FREE;
        /* It's possible there is memory here that was, say, added by
         * a client without our knowledge.  We can end up in an
         * infinite loop trying to forge a SIGSEGV in that situation
         * if executing from what we think is unreadable memory, so
         * best to check with the OS (xref PR 363811).
         */
#ifdef HAVE_MEMINFO
        byte *from_os_base_pc;
        size_t from_os_size;
        uint from_os_prot;
        if (get_memory_info_from_os(pc, &from_os_base_pc, &from_os_size, &from_os_prot) &&
            /* maps file shows our reserved-but-not-committed regions, which
             * are holes in all_memory_areas
             */
            from_os_prot != MEMPROT_NONE) {
            SYSLOG_INTERNAL_WARNING_ONCE(
                "all_memory_areas is missing regions including " PFX "-" PFX,
                from_os_base_pc, from_os_base_pc + from_os_size);
            DOLOG(4, LOG_VMAREAS, memcache_print(THREAD_GET, ""););
            /* be paranoid */
            out_info->base_pc = from_os_base_pc;
            out_info->size = from_os_size;
            out_info->prot = from_os_prot;
            out_info->type = DR_MEMTYPE_DATA; /* hopefully we won't miss an image */
            /* Update our list to avoid coming back here again (i#2037). */
            memcache_update_locked(from_os_base_pc, from_os_base_pc + from_os_size,
                                   from_os_prot, -1, false /*!exists*/);
        }
#else
        /* We now have nested probes, but currently probing sometimes calls
         * get_memory_info(), so we can't probe here unless we remove that call
         * there.
         */
#endif
    }
    memcache_unlock();
    return true;
}

#if defined(DEBUG) && defined(INTERNAL)
void
memcache_print(file_t outf, const char *prefix)
{
    vmvector_iterator_t vmvi;
    if (all_memory_areas == NULL || vmvector_empty(all_memory_areas))
        return;

    print_file(outf, "%s", prefix);

    vmvector_iterator_start(all_memory_areas, &vmvi);
    while (vmvector_iterator_hasnext(&vmvi)) {
        app_pc start, end;
        void *nxt = vmvector_iterator_next(&vmvi, &start, &end);
        allmem_info_t *info = (allmem_info_t *)nxt;
        print_file(outf, PFX "-" PFX " prot=%s type=%s\n", start, end,
                   memprot_string(info->prot),
                   (info->type == DR_MEMTYPE_FREE
                        ? "free"
                        : (info->type == DR_MEMTYPE_IMAGE ? "image" : "data")));
    }
    vmvector_iterator_stop(&vmvi);
}
#endif

void
memcache_handle_mmap(dcontext_t *dcontext, app_pc base, size_t size, uint memprot,
                     bool image)
{
    app_pc area_start;
    app_pc area_end;
    allmem_info_t *info;

    memcache_lock();
    sync_all_memory_areas();
    if (vmvector_lookup_data(all_memory_areas, base, &area_start, &area_end,
                             (void **)&info)) {
        uint new_memprot;
        LOG(THREAD, LOG_SYSCALLS, 4, "\tprocess overlap w/" PFX "-" PFX " prot=%d\n",
            area_start, area_end, info->prot);
        /* can't hold lock across call to app_memory_protection_change */
        memcache_unlock();
        if (info->prot != memprot) {
            /* We detect some alloc-based prot changes here.  app_memory_pre_alloc()
             * should have already processed these (i#1175) but no harm calling
             * app_memory_protection_change() again just in case.
             */
            DEBUG_DECLARE(uint res =)
            app_memory_protection_change(dcontext, base, size, memprot, &new_memprot,
                                         NULL, image);
            ASSERT_NOT_IMPLEMENTED(res != PRETEND_APP_MEM_PROT_CHANGE &&
                                   res != SUBSET_APP_MEM_PROT_CHANGE);
        }
        memcache_lock();
    }
    update_all_memory_areas(base, base + size, memprot,
                            image ? DR_MEMTYPE_IMAGE : DR_MEMTYPE_DATA);
    memcache_unlock();
}

void
memcache_handle_mremap(dcontext_t *dcontext, byte *base, size_t size, byte *old_base,
                       size_t old_size, uint old_prot, uint old_type)
{
    DEBUG_DECLARE(bool ok;)
    memcache_lock();
    /* Now modify the all-mems list. */
    /* We don't expect an existing entry for the new region. */

    /* i#175: overlap w/ existing regions is not an error */
    DEBUG_DECLARE(ok =)
    remove_from_all_memory_areas(old_base, old_base + old_size);
    ASSERT(ok);
    memcache_update(base, base + size, old_prot, old_type);
    memcache_unlock();
}

void
memcache_handle_app_brk(byte *lowest_brk /*if known*/, byte *old_brk, byte *new_brk)
{
    DEBUG_DECLARE(bool ok;)
    ASSERT(ALIGNED(old_brk, PAGE_SIZE));
    ASSERT(ALIGNED(new_brk, PAGE_SIZE));
    if (new_brk < old_brk) {
        memcache_lock();
        DEBUG_DECLARE(ok =)
        memcache_remove(new_brk, old_brk);
        ASSERT(ok);
        memcache_unlock();
    } else if (new_brk > old_brk) {
        allmem_info_t *info;
        uint prot;
        memcache_lock();
        sync_all_memory_areas();
        /* If the heap hasn't been created yet (no brk syscalls), then info
         * will be NULL.  We assume the heap is RW- on creation.
         */
        if (lowest_brk != NULL && old_brk == lowest_brk)
            prot = MEMPROT_READ | MEMPROT_WRITE;
        else {
            info = vmvector_lookup(all_memory_areas, old_brk - 1);
            prot = ((info != NULL) ? info->prot : MEMPROT_READ | MEMPROT_WRITE);
        }
        update_all_memory_areas(old_brk, new_brk, prot, DR_MEMTYPE_DATA);
        memcache_unlock();
    }
}

void
memcache_update_all_from_os(void)
{
    memquery_iter_t iter;
    LOG(GLOBAL, LOG_SYSCALLS, 1, "updating memcache from maps file\n");
    memquery_iterator_start(&iter, NULL, true /*may alloc*/);
    memcache_lock();
    /* We clear the entire cache to avoid false positive queries. */
    vmvector_reset_vector(GLOBAL_DCONTEXT, all_memory_areas);
    os_walk_address_space(&iter, false);
    memcache_unlock();
    memquery_iterator_stop(&iter);
}
