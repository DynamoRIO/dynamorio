/* *******************************************************************************
 * Copyright (c) 2013-2014 Google, Inc.  All rights reserved.
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
#include "os_private.h"

#include <mach/mach.h>
#include <mach/mach_vm.h>

#ifndef MACOS
# error Mac-only
#endif

/* Internal data */
typedef struct _internal_iter_t {
    struct vm_region_submap_info_64 info;
    vm_address_t address;
    uint32_t depth;
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
}

int
memquery_library_bounds(const char *name, app_pc *start/*IN/OUT*/, app_pc *end/*OUT*/,
                        char *fullpath/*OPTIONAL OUT*/, size_t path_size)
{
    ASSERT_NOT_IMPLEMENTED(false); /* XXX i#58 MacOS base impl */
    return 0;
}

bool
memquery_iterator_start(memquery_iter_t *iter, app_pc start, bool may_alloc)
{
    internal_iter_t *ii = (internal_iter_t *) &iter->internal;
    memset(ii, 0, sizeof(*ii));
    ii->address = (vm_address_t) start;
    iter->may_alloc = may_alloc;
    return true;
}

void
memquery_iterator_stop(memquery_iter_t *iter)
{
    /* nothing */
}

/* Translate mach flags to memprot flags.  They happen to equal the mmap
 * flags, but best to not rely on that.
 */
static inline uint
vmprot_to_memprot(uint prot)
{
    uint mem_prot = 0;
    if (TEST(VM_PROT_EXECUTE, prot))
        mem_prot |= MEMPROT_EXEC;
    if (TEST(VM_PROT_READ, prot))
        mem_prot |= MEMPROT_READ;
    if (TEST(VM_PROT_WRITE, prot))
        mem_prot |= MEMPROT_WRITE;
    return mem_prot;
}

bool
memquery_iterator_next(memquery_iter_t *iter)
{
    internal_iter_t *ii = (internal_iter_t *) &iter->internal;
    kern_return_t kr = KERN_SUCCESS;
    vm_size_t size = 0;
    /* XXX i#58: for 32-bit we should use the 32-bit version */
    mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
    do {
        kr = vm_region_recurse_64(mach_task_self(), &ii->address, &size, &ii->depth,
                                  (vm_region_info_64_t)&ii->info, &count);
        LOG(GLOBAL, LOG_ALL, 5, "%s: res=%d "PFX"-"PFX" sub=%d depth=%d\n",
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
        } else {
            /* Keep depth for next iter.  Kernel will reset to 0. */
            break;
        }
    } while (true);

    iter->vm_start = (app_pc) ii->address;
    /* XXX: should switch to storing size to avoid pointer overflow */
    iter->vm_end = (app_pc) ii->address + size;
    /* we do not expose ii->info.max_protection */
    iter->prot = vmprot_to_memprot(ii->info.protection);
    iter->offset = 0; /* XXX: not filling in */
    iter->inode = 0; /* XXX: not filling in */
    /* FIXME i#58: fill this in via SYS_proc_info */
    iter->comment = "";

    LOG(GLOBAL, LOG_ALL, 5, "%s: returning "PFX"-"PFX" prot=0x%x %s\n",
        __FUNCTION__, iter->vm_start, iter->vm_end, iter->prot, iter->comment);

    /* Prepare for next call */
    ii->address += size;

    return true;
}

bool
memquery_from_os(const byte *pc, OUT dr_mem_info_t *info, OUT bool *have_type)
{
    memquery_iter_t iter;
    bool res = false;
    memquery_iterator_start(&iter, (app_pc) pc, false/*won't alloc*/);
    if (memquery_iterator_next(&iter) && iter.vm_start <= pc) {
        res = true;
        info->base_pc = iter.vm_start;
        ASSERT(iter.vm_end > pc);
        /* XXX: should switch to storing size to avoid pointer overflow */
        info->size = iter.vm_end - iter.vm_start;
        info->prot = iter.prot;
        /* FIXME i#58: figure out whether image via SYS_proc_info */
        *have_type = false;
        info->type = DR_MEMTYPE_DATA;
    } else {
        /* Unlike Windows, the Mach queries skip free regions, so we have to
         * find the prior allocated region.  We could try just a few pages back,
         * but querying a free region is rare so we go with simple.
         */
        internal_iter_t *ii = (internal_iter_t *) &iter.internal;
        app_pc last_end = NULL;
        app_pc next_start = (app_pc) POINTER_MAX;
        ii->address = 0;
        while (memquery_iterator_next(&iter)) {
            if (iter.vm_start > pc) {
                next_start = iter.vm_start;
                break;
            }
            last_end = iter.vm_end;
        }
        info->base_pc = last_end;
        info->size = (next_start - last_end);
        if (next_start == (app_pc) POINTER_MAX)
            info->size++;
        info->prot = MEMPROT_NONE;
        info->type = DR_MEMTYPE_FREE;
        *have_type = true;
        res = true;
    }
    memquery_iterator_stop(&iter);
    return res;
}
