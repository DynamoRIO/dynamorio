/* *******************************************************************************
 * Copyright (c) 2013-2019 Google, Inc.  All rights reserved.
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

/*
 * memquery_macos.c - memory querying for OSX
 *
 * XXX i#58: NYI (see comments below as well):
 * + use 32-bit query version for 32-bit
 * + longer-term i#1291: use raw syscalls instead of libSystem wrappers
 */

#include "../globals.h"
#include "memquery.h"
#include "memquery_macos.h"
#include "os_private.h"

#include <mach/mach.h>
#include <mach/mach_vm.h>

/* For querying which file backs an mmap */
#include <libproc.h>
#include <sys/proc_info.h>
#include <sys/syscall.h>

#ifndef MACOS
#    error Mac-only
#endif

#ifdef NOT_DYNAMORIO_CORE_PROPER
#    undef LOG
#    define LOG(...) /* nothing */
#endif

/* Code passed to SYS_proc_info */
#define PROC_INFO_PID_INFO 2

/* We need a large (1272 byte) structure to obtain file backing info.
 * For regular queries we allocate this on the heap, but for fragile no-alloc
 * queries we use a static struct.
 */
static struct proc_regionwithpathinfo backing_info;
static mutex_t memquery_backing_lock = INIT_LOCK_FREE(memquery_backing_lock);

/* Internal data */
typedef struct _internal_iter_t {
    struct vm_region_submap_info_64 info;
    vm_address_t address;
    uint32_t depth;
    /* This points at either the heap or the global backing_info */
    struct proc_regionwithpathinfo *backing;
    dcontext_t *dcontext; /* GLOBAL_DCONTEXT for global heap; NULL for global struct */
} internal_iter_t;

void
memquery_init(void)
{
    ASSERT(sizeof(internal_iter_t) <= MEMQUERY_INTERNAL_DATA_LEN);
    ASSERT(sizeof(vm_address_t) == sizeof(app_pc));
}

void
memquery_exit(void)
{
    DELETE_LOCK(memquery_backing_lock);
}

bool
memquery_from_os_will_block(void)
{
#ifdef DEADLOCK_AVOIDANCE
    return memquery_backing_lock.owner != INVALID_THREAD_ID;
#else
    /* "may_alloc" is false for memquery_from_os() */
    bool res = true;
    if (d_r_mutex_trylock(&memquery_backing_lock)) {
        res = false;
        d_r_mutex_unlock(&memquery_backing_lock);
    }
    return res;
#endif
}

static bool
memquery_file_backing(struct proc_regionwithpathinfo *info, app_pc addr)
{
    int res;
#ifdef X64
    res = dynamorio_syscall(SYS_proc_info, 6, PROC_INFO_PID_INFO, get_process_id(),
                            PROC_PIDREGIONPATHINFO, (uint64_t)addr, info, sizeof(*info));
#else
    res = dynamorio_syscall(SYS_proc_info, 7, PROC_INFO_PID_INFO, get_process_id(),
                            PROC_PIDREGIONPATHINFO,
                            /* represent 64-bit arg as 2 32-bit args */
                            addr, NULL, info, sizeof(*info));
#endif
    return (res >= 0);
}

int
memquery_library_bounds(const char *name, app_pc *start /*IN/OUT*/, app_pc *end /*OUT*/,
                        char *fulldir /*OPTIONAL OUT*/, size_t fulldir_size,
                        char *filename /*OPTIONAL OUT*/, size_t filename_size)
{
    return memquery_library_bounds_by_iterator(name, start, end, fulldir, fulldir_size,
                                               filename, filename_size);
}

bool
memquery_iterator_start(memquery_iter_t *iter, app_pc start, bool may_alloc)
{
    internal_iter_t *ii = (internal_iter_t *)&iter->internal;
    memset(ii, 0, sizeof(*ii));
    ii->address = (vm_address_t)start;
    iter->may_alloc = may_alloc;
    if (may_alloc) {
        ii->dcontext = get_thread_private_dcontext();
        if (ii->dcontext == NULL)
            ii->dcontext = GLOBAL_DCONTEXT;
        ii->backing = HEAP_TYPE_ALLOC(ii->dcontext, struct proc_regionwithpathinfo,
                                      ACCT_MEM_MGT, PROTECTED);
    } else {
        d_r_mutex_lock(&memquery_backing_lock);
        ii->backing = &backing_info;
        ii->dcontext = NULL;
    }
    return true;
}

void
memquery_iterator_stop(memquery_iter_t *iter)
{
    internal_iter_t *ii = (internal_iter_t *)&iter->internal;
    if (ii->dcontext != NULL) {
        HEAP_TYPE_FREE(ii->dcontext, ii->backing, struct proc_regionwithpathinfo,
                       ACCT_MEM_MGT, PROTECTED);
    } else
        d_r_mutex_unlock(&memquery_backing_lock);
}

bool
memquery_iterator_next(memquery_iter_t *iter)
{
    internal_iter_t *ii = (internal_iter_t *)&iter->internal;
    kern_return_t kr = KERN_SUCCESS;
    vm_size_t size = 0;
    vm_address_t query_addr = ii->address;
    /* 64-bit versions seem to work fine for 32-bit */
    mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
    do {
        kr = vm_region_recurse_64(mach_task_self(), &ii->address, &size, &ii->depth,
                                  (vm_region_info_64_t)&ii->info, &count);
        LOG(GLOBAL, LOG_ALL, 5, "%s: res=%d " PFX "-" PFX " sub=%d depth=%d\n",
            __FUNCTION__, kr, ii->address, ii->address + size, ii->info.is_submap,
            ii->depth);
        if (kr != KERN_SUCCESS) {
            /* We expect KERN_INVALID_ADDRESS at end of address space, but we
             * still want to return false there.
             */
            return false;
        }
        if (ii->info.is_submap) {
            /* Query again w/ greater depth */
            ii->depth++;
            ii->address = query_addr;
        } else {
            /* Keep depth for next iter.  Kernel will reset to 0. */
            break;
        }
    } while (true);

    iter->vm_start = (app_pc)ii->address;
    /* XXX: should switch to storing size to avoid pointer overflow */
    iter->vm_end = (app_pc)ii->address + size;
    /* we do not expose ii->info.max_protection */
    iter->prot = vmprot_to_memprot(ii->info.protection);
    iter->offset = 0; /* XXX: not filling in */
    iter->inode = 0;  /* XXX: not filling in */
    if (memquery_file_backing(ii->backing, iter->vm_start)) {
        NULL_TERMINATE_BUFFER(ii->backing->prp_vip.vip_path);
        iter->comment = ii->backing->prp_vip.vip_path;
    } else
        iter->comment = "";

    LOG(GLOBAL, LOG_ALL, 5, "%s: returning " PFX "-" PFX " prot=0x%x %s\n", __FUNCTION__,
        iter->vm_start, iter->vm_end, iter->prot, iter->comment);

    /* Prepare for next call */
    ii->address += size;

    return true;
}

/* Not exported.  More efficient than stop+start. */
static void
memquery_reset_internal_iterator(memquery_iter_t *iter, const byte *new_pc)
{
    internal_iter_t *ii = (internal_iter_t *)&iter->internal;
    ii->address = (vm_address_t)new_pc;
    ii->depth = 0;
}

bool
memquery_from_os(const byte *pc, OUT dr_mem_info_t *info, OUT bool *have_type)
{
    memquery_iter_t iter;
    bool res = false;
    bool free = true;
    memquery_iterator_start(&iter, (app_pc)pc, false /*won't alloc*/);
    bool ok = memquery_iterator_next(&iter);
    if (ok && iter.vm_start <= pc) {
        /* There may be some inner regions we have to wade through */
        while (iter.vm_end <= pc) {
            if (!memquery_iterator_next(&iter))
                return false;
        }
        /* Sometimes the kernel returns a much earlier region so this may still be free */
        if (iter.vm_start <= pc) {
            res = true;
            info->base_pc = iter.vm_start;
            ASSERT(iter.vm_end > pc);
            /* XXX: should switch to storing size to avoid pointer overflow */
            info->size = iter.vm_end - iter.vm_start;
            info->prot = iter.prot;
            /* FIXME i#58: figure out whether image via SYS_proc_info */
            *have_type = false;
            info->type = DR_MEMTYPE_DATA;
            free = false;
        } else
            free = true;
    }
    if (free) {
        /* Unlike Windows, the Mach queries skip free regions, so we have to
         * find the prior allocated region.  While starting at 0 seems fine on 32-bit
         * its overhead shows up on 64-bit so we try to be more efficient.
         */
        app_pc last_end = NULL;
        size_t step = 8 * 1024;
        byte *try_pc = (byte *)pc;
        while (try_pc > (byte *)step) {
            try_pc -= step;
            memquery_reset_internal_iterator(&iter, try_pc);
            if (memquery_iterator_next(&iter) && iter.vm_start <= pc)
                break;
            if (step * 2 < step)
                break;
            step *= 2;
        }
        if (iter.vm_start > pc)
            last_end = 0;
        else
            last_end = iter.vm_end;
        app_pc next_start = (app_pc)POINTER_MAX;
        memquery_reset_internal_iterator(&iter, last_end);
        while (memquery_iterator_next(&iter)) {
            if (iter.vm_start > pc) {
                next_start = iter.vm_start;
                break;
            }
            last_end = iter.vm_end;
        }
        info->base_pc = last_end;
        info->size = (next_start - last_end);
        if (next_start == (app_pc)POINTER_MAX)
            info->size++;
        info->prot = MEMPROT_NONE;
        info->type = DR_MEMTYPE_FREE;
        *have_type = true;
        res = true;
    }
    memquery_iterator_stop(&iter);
    return res;
}
