/* *******************************************************************************
 * Copyright (c) 2010-2019 Google, Inc.  All rights reserved.
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
 * memquery_emulate.c - memory querying without kernel support
 */

#include "../globals.h"
#include "memquery.h"
#include "os_private.h"

#ifdef HAVE_MEMINFO
#    error Use kernel queries instead of emulation
#endif

/* must be prior to including dlfcn.h */
#define _GNU_SOURCE 1
/* despite man page I had to define this in addition to prev line */
#define __USE_GNU 1
/* this relies on not having -Icore/ to avoid including core/link.h
 * (pre-cmake we used -iquote instead of -I when we did have -Icore/)
 */
#include <link.h> /* struct dl_phdr_info */

void
memquery_init(void)
{
}

void
memquery_exit(void)
{
}

bool
memquery_from_os_will_block(bool may_alloc)
{
    return false;
}

bool
memquery_iterator_start(memquery_iter_t *iter, app_pc start, bool may_alloc)
{
    maps_iter_t *mi = (maps_iter_t *)&iter->internal;
    /* XXX i#1270: implement an iterator that does not use allmem -- or should
     * we go ahead and use allmem and adjust callers to that?  For using
     * allmem we'd refactor find_vm_areas_via_probe() into this iterator,
     * using dl_iterate_phdr() for modules and probing in between.
     * Perhaps we could also remove memquery_library_bounds() and have
     * a generic impl that uses the memquery iterator in that case.
     */
    return false;
}

void
memquery_iterator_stop(memquery_iter_t *iter)
{
    maps_iter_t *mi = (maps_iter_t *)&iter->internal;
}

bool
memquery_iterator_next(memquery_iter_t *iter)
{
    maps_iter_t *mi = (maps_iter_t *)&iter->internal;
    return false;
}

/***************************************************************************
 * LIBRARY ITERATION
 */

/* PR 361594: os-independent alternative to /proc/maps, though this
 * relies on user libraries
 */
typedef struct _dl_iterate_data_t {
    app_pc target_addr;
    const char *target_path;
    char *path_out;
    size_t path_size;
    app_pc mod_start;
    app_pc mod_end;
} dl_iterate_data_t;

static int
dl_iterate_get_path_cb(struct dl_phdr_info *info, size_t size, void *data)
{
    dl_iterate_data_t *iter_data = (dl_iterate_data_t *)data;
    /* info->dlpi_addr is offset from preferred so we need to calculate the
     * absolute address of the base.
     * we can calculate the absolute address of the first segment, but ELF
     * doesn't seem to guarantee that either the elf header (base of
     * file) or the program headers (info->dlpi_phdr) are later than
     * the min_vaddr, so it's a little confusing as to what would be
     * in the maps file or whatever and would thus be the base we're looking
     * to match: for now we assume the page with min_vaddr is that base.
     * If elf header, program header, and 1st segment could all be on
     * separate pages, I don't see any way to find the elf header in
     * such cases short of walking backward and looking for the magic #s.
     */
    app_pc pref_start, pref_end;
    app_pc min_vaddr = module_vaddr_from_prog_header((app_pc)info->dlpi_phdr,
                                                     info->dlpi_phnum, NULL, NULL);
    app_pc base = info->dlpi_addr + min_vaddr;
    /* Note that dl_iterate_phdr doesn't give a name for the executable or
     * ld-linux.so presumably b/c those are mapped by the kernel so the
     * user-space loader doesn't need to know their file paths.
     */
    LOG(GLOBAL, LOG_VMAREAS, 2,
        "dl_iterate_get_path_cb: addr=" PFX " hdrs=" PFX " base=" PFX " name=%s\n",
        info->dlpi_addr, info->dlpi_phdr, base, info->dlpi_name);
    /* all we have is an addr somewhere in the module, so we need the end */
    if (module_walk_program_headers(base,
                                    /* FIXME: don't have view size: but
                                     * anything larger than header sizes works
                                     */
                                    PAGE_SIZE, false,
                                    true, /* i#1589: ld.so relocated .dynamic */
                                    &pref_start, NULL, &pref_end, NULL, NULL)) {
        /* we're passed back start,end of preferred base */
        if ((iter_data->target_addr != NULL && iter_data->target_addr >= base &&
             iter_data->target_addr < base + (pref_end - pref_start)) ||
            (iter_data->target_path != NULL &&
             /* if we're passed an ambiguous name, we return first hit.
              * if passed full path, should normally be what was used to
              * load, so should match.
              */
             strstr(info->dlpi_name, iter_data->target_path) != NULL)) {
            if (iter_data->path_size > 0) {
                /* We want just the path, not the filename */
                char *slash = strrchr(info->dlpi_name, '/');
                ASSERT_CURIOSITY(slash != NULL);
                ASSERT_CURIOSITY((slash - info->dlpi_name) < iter_data->path_size);
                strncpy(iter_data->path_out, info->dlpi_name,
                        MIN(iter_data->path_size, (slash - info->dlpi_name)));
                iter_data->path_out[iter_data->path_size] = '\0';
            }
            iter_data->mod_start = base;
            iter_data->mod_end = base + (pref_end - pref_start);
            return 1; /* done iterating */
        }
    } else {
        ASSERT_NOT_REACHED();
    }
    return 0; /* keep looking */
}

/* See memquery.h for full interface specs */
int
memquery_library_bounds(const char *name, app_pc *start /*IN/OUT*/, app_pc *end /*OUT*/,
                        char *fulldir /*OPTIONAL OUT*/, size_t fulldir_size,
                        char *filename /*OPTIONAL OUT*/, size_t filename_size)
{
    int count = 0;
    dl_iterate_data_t iter_data;
    app_pc cur_end = NULL;
    app_pc mod_start = NULL;
    ASSERT(name != NULL || start != NULL);

    /* PR 361594: os-independent alternative to /proc/maps */
    /* We don't have the base and we can't walk backwards (see comments above)
     * so we rely on dl_iterate_phdr() from glibc 2.2.4+, which will
     * also give us the path, needed for inject_library_path for execve.
     */
    iter_data.target_addr = (start == NULL) ? NULL : *start;
    iter_data.target_path = name;
    iter_data.path_out = fulldir;
    iter_data.path_size = (fulldir == NULL) ? 0 : fulldir_size;
    DEBUG_DECLARE(int res =)
    dl_iterate_phdr(dl_iterate_get_path_cb, &iter_data);
    ASSERT(res == 1);
    mod_start = iter_data.mod_start;
    cur_end = iter_data.mod_end;
    count = 1;
    LOG(GLOBAL, LOG_VMAREAS, 2, "get_library_bounds %s => " PFX "-" PFX " %s\n",
        name == NULL ? "<null>" : name, mod_start, cur_end,
        fulldir == NULL ? "<no path requested>" : fulldir);

    if (start != NULL)
        *start = mod_start;
    if (end != NULL)
        *end = cur_end;
    return count;
}

/***************************************************************************
 * ADDRESS SPACE ITERATION
 */

/* PR 361594: os-independent alternative to /proc/maps */

#define VSYSCALL_PAGE_SO_NAME "linux-gate.so"
/* For now we assume no OS config has user addresses above this value
 * We just go to max 32-bit (64-bit not supported yet: want lazy probing),
 * if don't have any kind of mmap iterator
 */
#define USER_MAX 0xfffff000

/* callback for dl_iterate_phdr() for adding existing modules to our lists */
static int
dl_iterate_get_areas_cb(struct dl_phdr_info *info, size_t size, void *data)
{
    int *count = (int *)data;
    uint i;
    /* see comments in dl_iterate_get_path_cb() */
    app_pc modend;
    app_pc min_vaddr = module_vaddr_from_prog_header((app_pc)info->dlpi_phdr,
                                                     info->dlpi_phnum, NULL, &modend);
    app_pc modbase = info->dlpi_addr + min_vaddr;
    size_t modsize = modend - min_vaddr;
    LOG(GLOBAL, LOG_VMAREAS, 2,
        "dl_iterate_get_areas_cb: addr=" PFX " hdrs=" PFX " base=" PFX " name=%s\n",
        info->dlpi_addr, info->dlpi_phdr, modbase, info->dlpi_name);
    ASSERT(info->dlpi_phnum == module_num_program_headers(modbase));

    ASSERT(count != NULL);
    if (*count == 0) {
        /* since we don't get a name for the executable, for now we
         * assume that the first iter is the executable itself.
         * XXX: this seems to hold, but there's no guarantee: can we do better?
         */
        executable_start = modbase;
    }

#ifndef X64
    if (modsize == PAGE_SIZE && info->dlpi_name[0] == '\0') {
        /* Candidate for VDSO.  Xref PR 289138 on using AT_SYSINFO to locate. */
        /* Xref VSYSCALL_PAGE_START_HARDCODED but later linuxes randomize */
        char *soname;
        if (module_walk_program_headers(modbase, modsize, false,
                                        true, /* i#1589: ld.so relocated .dynamic */
                                        NULL, NULL, NULL, &soname, NULL) &&
            strncmp(soname, VSYSCALL_PAGE_SO_NAME, strlen(VSYSCALL_PAGE_SO_NAME)) == 0) {
            ASSERT(!dynamo_initialized); /* .data should be +w */
            ASSERT(vsyscall_page_start == NULL);
            vsyscall_page_start = modbase;
            LOG(GLOBAL, LOG_VMAREAS, 1, "found vsyscall page @ " PFX "\n",
                vsyscall_page_start);
        }
    }
#endif
    if (modbase != vsyscall_page_start)
        module_list_add(modbase, modsize, false, info->dlpi_name, 0 /*don't have inode*/);

    for (i = 0; i < info->dlpi_phnum; i++) {
        app_pc start, end;
        uint prot;
        size_t align;
        if (module_read_program_header(modbase, i, &start, &end, &prot, &align)) {
            start += info->dlpi_addr;
            end += info->dlpi_addr;
            LOG(GLOBAL, LOG_VMAREAS, 2, "\tsegment %d: " PFX "-" PFX " %s align=%d\n", i,
                start, end, memprot_string(prot), align);
            start = (app_pc)ALIGN_BACKWARD(start, PAGE_SIZE);
            end = (app_pc)ALIGN_FORWARD(end, PAGE_SIZE);
            LOG(GLOBAL, LOG_VMAREAS, 4,
                "find_executable_vm_areas: adding: " PFX "-" PFX " prot=%d\n", start, end,
                prot);
            all_memory_areas_lock();
            update_all_memory_areas(start, end, prot, DR_MEMTYPE_IMAGE);
            all_memory_areas_unlock();
            if (app_memory_allocation(NULL, start, end - start, prot,
                                      true /*image*/
                                      _IF_DEBUG("ELF SO")))
                (*count)++;
        }
    }
    return 0; /* keep iterating */
}

/* helper for find_vm_areas_via_probe() and get_memory_info_from_os()
 * returns the passed-in pc if the probe was successful; else, returns
 * where the next probe should be (to skip DR memory).
 * if the probe was successful, returns in prot the results.
 */
static app_pc
probe_address(dcontext_t *dcontext, app_pc pc_in, OUT uint *prot)
{
    app_pc base;
    size_t size;
    app_pc pc = (app_pc)ALIGN_BACKWARD(pc_in, PAGE_SIZE);
    ASSERT(ALIGNED(pc, PAGE_SIZE));
    ASSERT(prot != NULL);
    *prot = MEMPROT_NONE;

    /* skip our own vmheap */
    app_pc heap_end;
    if (is_vmm_reserved_address(pc, 1, NULL, &heap_end))
        return heap_end;
    /* if no vmheap and we probe our own stack, the SIGSEGV handler will
     * report stack overflow as it checks that prior to handling TRY
     */
    if (is_stack_overflow(dcontext, pc))
        return pc + PAGE_SIZE;
#ifdef VMX86_SERVER
    /* Workaround for PR 380621 */
    if (is_vmkernel_addr_in_user_space(pc, &base)) {
        LOG(GLOBAL, LOG_VMAREAS, 4, "%s: skipping vmkernel region " PFX "-" PFX "\n",
            __func__, pc, base);
        return base;
    }
#endif
    /* Only for find_vm_areas_via_probe(), skip modules added by
     * dl_iterate_get_areas_cb.  Subsequent probes are about gettting
     * info from OS, so do the actual probe.  See PR 410907.
     */
    if (!dynamo_initialized && get_memory_info(pc, &base, &size, prot))
        return base + size;

    TRY_EXCEPT(dcontext, /* try */
               {
                   PROBE_READ_PC(pc);
                   *prot |= MEMPROT_READ;
               },
               /* except */
               {
                   /* nothing: just continue */
               });
    /* x86 can't be writable w/o being readable.  avoiding nested TRY though. */
    if (TEST(MEMPROT_READ, *prot)) {
        TRY_EXCEPT(dcontext, /* try */
                   {
                       PROBE_WRITE_PC(pc);
                       *prot |= MEMPROT_WRITE;
                   },
                   /* except */
                   {
                       /* nothing: just continue */
                   });
    }

    LOG(GLOBAL, LOG_VMAREAS, 5, "%s: probe " PFX " => %s\n", __func__, pc,
        memprot_string(*prot));

    /* PR 403000 - for unaligned probes return the address passed in as arg. */
    return pc_in;
}

/* helper for find_vm_areas_via_probe() */
static inline int
probe_add_region(app_pc *last_start, uint *last_prot, app_pc pc, uint prot, bool force)
{
    int count = 0;
    if (force || prot != *last_prot) {
        /* We record unreadable regions as the absence of an entry */
        if (*last_prot != MEMPROT_NONE) {
            all_memory_areas_lock();
            /* images were done separately */
            update_all_memory_areas(*last_start, pc, *last_prot, DR_MEMTYPE_DATA);
            all_memory_areas_unlock();
            if (app_memory_allocation(NULL, *last_start, pc - *last_start, *last_prot,
                                      false /*!image*/ _IF_DEBUG("")))
                count++;
        }
        *last_prot = prot;
        *last_start = pc;
    }
    return count;
}

/* non-/proc/maps version of find_executable_vm_areas() */
int
find_vm_areas_via_probe(void)
{
    /* PR 361594: now that sigsegv handler is set up, loop&probe.
     * First, dl_iterate_phdr() to get modules, and walk their
     * segments to get internal regions: then can avoid
     * wasting time probing modules.
     *
     * TODO PR 364552:
     * Would be nice to probe lazily to avoid touching all non-module
     * pages and avoid wasting our time on faults in large empty areas
     * of the address space.  This is especially important for 64-bit.
     * If done lazily, to avoid races, should do "lock add *page,0".
     * If that faults, then just try to read.  Note that we need
     * nested SIGSEGV support to handle probing while inside a SIGSEGV
     * handler (see PR 287309).
     *
     * Note that we have no good way (at least that's not racy, or
     * that'll work if there's no NX) to check for +x, and as such we
     * require HAVE_MEMINFO for PROGRAM_SHEPHERDING (also xref PR
     * 210383: NX transparency).
     *
     * Note also that we assume a "normal" segment setup: no funny
     * +x but -rw segments.
     */
    int count = 0;
    dcontext_t *dcontext = get_thread_private_dcontext();
    app_pc pc, last_start = NULL;
    uint prot, last_prot = MEMPROT_NONE;

    DEBUG_DECLARE(int res =)
    dl_iterate_phdr(dl_iterate_get_areas_cb, &count);
    ASSERT(res == 0);

    ASSERT(dcontext != NULL);

#ifdef VMX86_SERVER
    /* We only need to probe inside allocated regions */
    void *iter = vmk_mmemquery_iter_start();
    if (iter != NULL) { /* backward compatibility: support lack of iter */
        byte *start;
        size_t length;
        char name[MAXIMUM_PATH];
        LOG(GLOBAL, LOG_ALL, 1, "VSI mmaps:\n");
        while (vmk_mmemquery_iter_next(iter, &start, &length, (int *)&prot, name,
                                       BUFFER_SIZE_ELEMENTS(name))) {
            LOG(GLOBAL, LOG_ALL, 1, "\t" PFX "-" PFX ": %d %s\n", start, start + length,
                prot, name);
            ASSERT(ALIGNED(start, PAGE_SIZE));
            last_prot = MEMPROT_NONE;
            for (pc = start; pc < start + length;) {
                prot = MEMPROT_NONE;
                app_pc next_pc = probe_address(dcontext, pc, &prot);
                count +=
                    probe_add_region(&last_start, &last_prot, pc, prot, next_pc != pc);
                if (next_pc != pc) {
                    pc = next_pc;
                    last_prot = MEMPROT_NONE; /* ensure we add adjacent region */
                    last_start = pc;
                } else
                    pc += PAGE_SIZE;
            }
            count += probe_add_region(&last_start, &last_prot, pc, prot, true);
            last_start = pc;
        }
        vmk_mmemquery_iter_stop(iter);
        return count;
    } /* else, fall back to full probing */
#else
#    ifdef X64
    /* no lazy probing support yet */
#        error X64 requires HAVE_MEMINFO: PR 364552
#    endif
#endif
    ASSERT(ALIGNED(USER_MAX, PAGE_SIZE));
    for (pc = (app_pc)PAGE_SIZE; pc < (app_pc)USER_MAX;) {
        prot = MEMPROT_NONE;
        app_pc next_pc = probe_address(dcontext, pc, &prot);
        count += probe_add_region(&last_start, &last_prot, pc, prot, next_pc != pc);
        if (next_pc != pc) {
            pc = next_pc;
            last_prot = MEMPROT_NONE; /* ensure we add adjacent region */
            last_start = pc;
        } else
            pc += PAGE_SIZE;
    }
    count += probe_add_region(&last_start, &last_prot, pc, prot, true);
    return count;
}

/***************************************************************************
 * QUERY
 */

bool
memquery_from_os(const byte *pc, OUT dr_mem_info_t *info, OUT bool *have_type)
{
    app_pc probe_pc, next_pc;
    app_pc start_pc = (app_pc)pc, end_pc = (app_pc)pc + PAGE_SIZE;
    byte *our_heap_start, *our_heap_end;
    uint cur_prot = MEMPROT_NONE;
    dcontext_t *dcontext = get_thread_private_dcontext();
    ASSERT(info != NULL);
    /* don't crash if no dcontext, which happens (PR 452174) */
    if (dcontext == NULL)
        return false;
    /* FIXME PR 235433: replace w/ real query to avoid all these probes */

    next_pc = probe_address(dcontext, (app_pc)pc, &cur_prot);
    if (next_pc != pc) {
        if (is_vmm_reserved_address(pc, 1, &our_heap_start, &our_heap_end)) {
            /* Just making all readable for now */
            start_pc = our_heap_start;
            end_pc = our_heap_end;
            cur_prot = MEMPROT_READ;
        } else {
            /* FIXME: should iterate rest of cases */
            return false;
        }
    } else {
        for (probe_pc = (app_pc)ALIGN_BACKWARD(pc, PAGE_SIZE) - PAGE_SIZE;
             probe_pc > (app_pc)NULL; probe_pc -= PAGE_SIZE) {
            uint prot = MEMPROT_NONE;
            next_pc = probe_address(dcontext, probe_pc, &prot);
            if (next_pc != pc || prot != cur_prot)
                break;
        }
        start_pc = probe_pc + PAGE_SIZE;
        ASSERT(ALIGNED(USER_MAX, PAGE_SIZE));
        for (probe_pc = (app_pc)ALIGN_FORWARD(pc, PAGE_SIZE); probe_pc < (app_pc)USER_MAX;
             probe_pc += PAGE_SIZE) {
            uint prot = MEMPROT_NONE;
            next_pc = probe_address(dcontext, probe_pc, &prot);
            if (next_pc != pc || prot != cur_prot)
                break;
        }
        end_pc = probe_pc;
    }
    info->base_pc = start_pc;
    info->size = end_pc - start_pc;
    info->prot = cur_prot;
    if (cur_prot == MEMPROT_NONE) {
        /* FIXME: how distinguish from reserved but inaccessable?
         * Could try mprotect() and see whether it fails
         */
        info->type = DR_MEMTYPE_FREE;
        *have_type = true;
    }
    return true;
}
