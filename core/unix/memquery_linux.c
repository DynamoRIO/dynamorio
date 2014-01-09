/* *******************************************************************************
 * Copyright (c) 2010-2014 Google, Inc.  All rights reserved.
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
#include "memquery.h"
#include "os_private.h"
#include "module_private.h"
#include <string.h>
#include <sys/mman.h>

#ifndef LINUX
# error Linux-only
#endif

/* Iterator over /proc/self/maps
 * Called at arbitrary places, so cannot use fopen.
 * The whole thing is serialized, so entries cannot be referenced
 * once the iteration ends.
 */

/* Internal data */
typedef struct _maps_iter_t {
    file_t maps;
    char *newline;
    int bufread;
    int bufwant;
    char *buf;
    char *comment_buffer;
} maps_iter_t;

/* lock to guard reads from /proc/self/maps in get_memory_info_from_os(). */
static mutex_t memory_info_buf_lock = INIT_LOCK_FREE(memory_info_buf_lock);
/* lock for iterator where user needs to allocate memory */
static mutex_t maps_iter_buf_lock = INIT_LOCK_FREE(maps_iter_buf_lock);

/* On all supported linux kernels /proc/self/maps -> /proc/$pid/maps
 * However, what we want is /proc/$tid/maps, so we can't use "self"
 */
#define PROC_SELF_MAPS "/proc/self/maps"

/* these are defined in /usr/src/linux/fs/proc/array.c */
#define MAPS_LINE_LENGTH        4096
/* for systems with sizeof(void*) == 4: */
#define MAPS_LINE_FORMAT4         "%08lx-%08lx %s %08lx %*s "UINT64_FORMAT_STRING" %4096s"
#define MAPS_LINE_MAX4  49 /* sum of 8  1  8  1 4 1 8 1 5 1 10 1 */
/* for systems with sizeof(void*) == 8: */
#define MAPS_LINE_FORMAT8         "%016lx-%016lx %s %016lx %*s "UINT64_FORMAT_STRING" %4096s"
#define MAPS_LINE_MAX8  73 /* sum of 16  1  16  1 4 1 16 1 5 1 10 1 */

#define MAPS_LINE_MAX   MAPS_LINE_MAX8

/* can't use fopen -- strategy: read into buffer, look for newlines.
 * fail if single line too large for buffer -- so size it appropriately:
 */
/* since we're called from signal handler, etc., keep our stack usage
 * low by using static bufs (it's over 4K after all).
 * FIXME: now we're using 16K right here: should we shrink?
 */
#define BUFSIZE (MAPS_LINE_LENGTH+8)
static char buf_scratch[BUFSIZE];
static char comment_buf_scratch[BUFSIZE];
/* To satisfy our two uses (inner use with memory_info_buf_lock versus
 * outer use with maps_iter_buf_lock), we have two different locks and
 * two different sets of static buffers.  This is to avoid lock
 * ordering issues: we need an inner lock for use in places like signal
 * handlers, but an outer lock when the iterator user allocates memory.
 */
static char buf_iter[BUFSIZE];
static char comment_buf_iter[BUFSIZE];

void
memquery_init(void)
{
    ASSERT(sizeof(maps_iter_t) <= MEMQUERY_INTERNAL_DATA_LEN);
}

void
memquery_exit(void)
{
    DELETE_LOCK(memory_info_buf_lock);
    DELETE_LOCK(maps_iter_buf_lock);
}

bool
memquery_iterator_start(memquery_iter_t *iter, app_pc start, bool may_alloc)
{
    char maps_name[24]; /* should only need 16 for 5-digit tid */
    maps_iter_t *mi = (maps_iter_t *) &iter->internal;

    /* Don't assign the local ptrs until the lock is grabbed to make
     * their relationship clear. */
    if (may_alloc) {
        mutex_lock(&maps_iter_buf_lock);
        mi->buf = (char *) &buf_iter;
        mi->comment_buffer = (char *) &comment_buf_iter;
    } else {
        mutex_lock(&memory_info_buf_lock);
        mi->buf = (char *) &buf_scratch;
        mi->comment_buffer = (char *) &comment_buf_scratch;
    }

    /* We need the maps for our thread id, not process id.
     * "/proc/self/maps" uses pid which fails if primary thread in group
     * has exited.
     */
    snprintf(maps_name, BUFFER_SIZE_ELEMENTS(maps_name),
             "/proc/%d/maps", get_thread_id());
    mi->maps = os_open(maps_name, OS_OPEN_READ);
    ASSERT(mi->maps != INVALID_FILE);
    mi->buf[BUFSIZE-1] = '\0'; /* permanently */

    mi->newline = NULL;
    mi->bufread = 0;
    iter->comment = mi->comment_buffer;

    iter->may_alloc = may_alloc;

    /* XXX: it's quite difficult to start at the region containing "start":
     * we either need to support walking backward a line (complicated by
     * the incremental os_read() scheme) or make two passes.
     * Thus, the interface doesn't promise we'll start there.
     */
    iter->vm_start = NULL;

    return true;
}

void
memquery_iterator_stop(memquery_iter_t *iter)
{
    maps_iter_t *mi = (maps_iter_t *) &iter->internal;
    ASSERT((iter->may_alloc && OWN_MUTEX(&maps_iter_buf_lock)) ||
           (!iter->may_alloc && OWN_MUTEX(&memory_info_buf_lock)));
    os_close(mi->maps);
    if (iter->may_alloc)
        mutex_unlock(&maps_iter_buf_lock);
    else
        mutex_unlock(&memory_info_buf_lock);
}

bool
memquery_iterator_next(memquery_iter_t *iter)
{
    maps_iter_t *mi = (maps_iter_t *) &iter->internal;
    char perm[16];
    char *line;
    int len;
    app_pc prev_start = iter->vm_start;
    ASSERT((iter->may_alloc && OWN_MUTEX(&maps_iter_buf_lock)) ||
           (!iter->may_alloc && OWN_MUTEX(&memory_info_buf_lock)));
    if (mi->newline == NULL) {
        mi->bufwant = BUFSIZE-1;
        mi->bufread = os_read(mi->maps, mi->buf, mi->bufwant);
        ASSERT(mi->bufread <= mi->bufwant);
        LOG(GLOBAL, LOG_VMAREAS, 6,
            "get_memory_info_from_os: bytes read %d/want %d\n",
            mi->bufread, mi->bufwant);
        if (mi->bufread <= 0)
            return false;
        mi->buf[mi->bufread] = '\0';
        mi->newline = strchr(mi->buf, '\n');
        line = mi->buf;
    } else {
        line = mi->newline + 1;
        mi->newline = strchr(line, '\n');
        if (mi->newline == NULL) {
            /* FIXME clean up: factor out repetitive code */
            /* shift 1st part of line to start of buf, then read in rest */
            /* the memory for the processed part can be reused  */
            mi->bufwant = line - mi->buf;
            ASSERT(mi->bufwant <= mi->bufread);
            len = mi->bufread - mi->bufwant; /* what is left from last time */
            /* since strings may overlap, should use memmove, not strncpy */
            /* FIXME corner case: if len == 0, nothing to move */
            memmove(mi->buf, line, len);
            mi->bufread = os_read(mi->maps, mi->buf+len, mi->bufwant);
            ASSERT(mi->bufread <= mi->bufwant);
            if (mi->bufread <= 0)
                return false;
            mi->bufread += len; /* bufread is total in buf */
            mi->buf[mi->bufread] = '\0';
            mi->newline = strchr(mi->buf, '\n');
            line = mi->buf;
        }
    }
    LOG(GLOBAL, LOG_VMAREAS, 6,
        "\nget_memory_info_from_os: newline=[%s]\n",
        mi->newline ? mi->newline : "(null)");

    /* buffer is big enough to hold at least one line */
    ASSERT(mi->newline != NULL);
    *mi->newline = '\0';
    LOG(GLOBAL, LOG_VMAREAS, 6,
        "\nget_memory_info_from_os: line=[%s]\n", line);
    mi->comment_buffer[0]='\0';
    len = sscanf(line,
#ifdef IA32_ON_IA64
                 MAPS_LINE_FORMAT8, /* cross-compiling! */
#else
                 sizeof(void*) == 4 ? MAPS_LINE_FORMAT4 : MAPS_LINE_FORMAT8,
#endif
                 (unsigned long*)&iter->vm_start, (unsigned long*)&iter->vm_end,
                 perm, (unsigned long*)&iter->offset, &iter->inode,
                 mi->comment_buffer);
    if (iter->vm_start == iter->vm_end) {
        /* i#366 & i#599: Merge an empty regions caused by stack guard pages
         * into the stack region if the stack region is less than one page away.
         * Otherwise skip it.  Some Linux kernels (2.6.32 has been observed)
         * have empty entries for the stack guard page.  We drop the permissions
         * on the guard page, because Linux always insists that it has rwxp
         * perms, no matter how we change the protections.  The actual stack
         * region has the perms we expect.
         * XXX: We could get more accurate info if we looked at
         * /proc/self/smaps, which has a Size: 4k line for these "empty"
         * regions.
         */
        app_pc empty_start = iter->vm_start;
        bool r;
        LOG(GLOBAL, LOG_VMAREAS, 2,
            "maps_iterator_next: skipping or merging empty region 0x%08x\n",
            iter->vm_start);
        /* don't trigger the maps-file-changed check.
         * slight risk of a race where we'll pass back earlier/overlapping
         * region: we'll live with it.
         */
        iter->vm_start = NULL;
        r = memquery_iterator_next(iter);
        /* We could check to see if we're combining with the [stack] section,
         * but that doesn't work if there are multiple stacks or the stack is
         * split into multiple maps entries, so we merge any empty region within
         * one page of the next region.
         */
        if (empty_start <= iter->vm_start &&
            iter->vm_start <= empty_start + PAGE_SIZE) {
            /* Merge regions if the next region was zero or one page away. */
            iter->vm_start = empty_start;
        }
        return r;
    }
    if (iter->vm_start <= prev_start) {
        /* the maps file has expanded underneath us (presumably due to our
         * own committing while iterating): skip ahead */
        LOG(GLOBAL, LOG_VMAREAS, 2,
            "maps_iterator_next: maps file changed: skipping 0x%08x\n", prev_start);
        iter->vm_start = prev_start;
        return memquery_iterator_next(iter);
    }
    if (len<6)
        mi->comment_buffer[0]='\0';
    iter->prot = permstr_to_memprot(perm);
    return true;
}

/***************************************************************************
 * LIBRARY BOUNDS
 */

/* See memquery.h for full interface specs.
 * Gets the library bounds from walking the map file (as opposed to using our cached
 * module_list) since is only used for dr and client libraries which aren't on the list.
 */
int
memquery_library_bounds(const char *name, app_pc *start/*IN/OUT*/, app_pc *end/*OUT*/,
                        char *fullpath/*OPTIONAL OUT*/, size_t path_size)
{
    int count = 0;
    bool found_library = false;
    char libname[MAXIMUM_PATH];
    const char *name_cmp = name;
    memquery_iter_t iter;
    app_pc last_base = NULL;
    app_pc last_end = NULL;
    size_t image_size = 0;
    app_pc cur_end = NULL;
    app_pc mod_start = NULL;
    ASSERT(name != NULL || start != NULL);

    memquery_iterator_start(&iter, NULL, true/*ok to alloc*/);
    libname[0] = '\0';
    while (memquery_iterator_next(&iter)) {
        LOG(GLOBAL, LOG_VMAREAS, 5,"start="PFX" end="PFX" prot=%x comment=%s\n",
            iter.vm_start, iter.vm_end, iter.prot, iter.comment);

        /* Record the base of each differently-named set of entries up until
         * we find our target, when we'll clobber libpath
         */
        if (!found_library &&
            strncmp(libname, iter.comment, BUFFER_SIZE_ELEMENTS(libname)) != 0) {
            last_base = iter.vm_start;
            /* last_end is used to know what's readable beyond last_base */
            if (TEST(PROT_READ, iter.prot))
                last_end = iter.vm_end;
            else
                last_end = last_base;
            /* remember name so we can find the base of a multiply-mapped so */
            strncpy(libname, iter.comment, BUFFER_SIZE_ELEMENTS(libname));
            NULL_TERMINATE_BUFFER(libname);
        }

        if ((name_cmp != NULL &&
             (strstr(iter.comment, name_cmp) != NULL ||
              /* Include mid-library (non-.bss) anonymous mappings.  Our private loader
               * fills mapping holes with anonymous memory instead of a
               * MEMPROT_NONE mapping from the original file.
               * .bss, which can be merged with a subsequent +rw anon mapping,
               * is handled post-loop.
               */
              (found_library && iter.comment[0] == '\0' && image_size != 0 &&
               iter.vm_end - mod_start < image_size))) ||
            (name == NULL && *start >= iter.vm_start && *start < iter.vm_end)) {
            if (!found_library) {
                size_t mod_readable_sz;
                char *dst = (fullpath != NULL) ? fullpath : libname;
                size_t dstsz = (fullpath != NULL) ? path_size :
                    BUFFER_SIZE_ELEMENTS(libname);
                char *slash = strrchr(iter.comment, '/');
                ASSERT_CURIOSITY(slash != NULL);
                ASSERT_CURIOSITY((slash - iter.comment) < dstsz);
                /* we keep the last '/' at end */
                ++slash;
                strncpy(dst, iter.comment, MIN(dstsz, (slash - iter.comment)));
                /* if max no null */
                dst[dstsz - 1] = '\0';
                if (name == NULL)
                    name_cmp = dst;
                found_library = true;
                /* Most library have multiple segments, and some have the
                 * ELF header repeated in a later mapping, so we can't rely
                 * on is_elf_so_header() and header walking.
                 * We use the name tracking to remember the first entry
                 * that had this name.
                 */
                if (last_base == NULL) {
                    mod_start = iter.vm_start;
                    mod_readable_sz = iter.vm_end - iter.vm_start;
                } else {
                    mod_start = last_base;
                    mod_readable_sz = last_end - last_base;
                }
                if (is_elf_so_header(mod_start, mod_readable_sz)) {
                    app_pc mod_base, mod_end;
                    if (module_walk_program_headers(mod_start, mod_readable_sz, false,
                                                    &mod_base, &mod_end, NULL, NULL)) {
                        image_size = mod_end - mod_base;
                        ASSERT_CURIOSITY(image_size != 0);
                    } else {
                        ASSERT_NOT_REACHED();
                    }
                } else {
                    ASSERT(false && "expected elf header");
                }
            }
            count++;
            cur_end = iter.vm_end;
#if 0
            /* FIXME: this is for libdynamorio.so only: */
            if (TESTANY(SELFPROT_ANY_DATA_SECTION, dynamo_options.protect_mask) &&
                TESTALL(MEMPROT_READ|MEMPROT_WRITE, iter.prot)) {
                /* FIXME: PR 212458/212459: we could get entire rw region here and
                 * subtract out .data, .fspdata, .cspdata, and .nspdata,
                 * but better to explicitly add .got, etc., since we should
                 * examine each rw section and decide on frequency of unprotection.
                 */
            }
#endif
        } else if (found_library) {
            /* hit non-matching, we expect module segments to be adjacent */
            break;
        }
    }

    /* Xref PR 208443: .bss sections are anonymous (no file name listed in
     * maps file), but not every library has one.  We have to parse the ELF
     * header to know since we can't assume that a subsequent anonymous
     * region is .bss. */
    if (image_size != 0 && cur_end - mod_start < image_size) {
        /* Found a .bss section. Check current mapping (note might only be
         * part of the mapping (due to os region merging? FIXME investigate). */
        ASSERT_CURIOSITY(iter.vm_start == cur_end /* no gaps, FIXME might there be
                                                   * a gap if the file has large
                                                   * alignment and no data section?
                                                   * curiosity for now*/);
        ASSERT_CURIOSITY(iter.inode == 0); /* .bss is anonymous */
        ASSERT_CURIOSITY(iter.vm_end - mod_start >= image_size);/* should be big enough */
        count++;
        cur_end = mod_start + image_size;
    } else {
        /* Shouldn't have more mapped then the size of the module, unless it's a
         * second adjacent separate map of the same file.  Curiosity for now. */
        ASSERT_CURIOSITY(image_size == 0 || cur_end - mod_start == image_size);
    }
    memquery_iterator_stop(&iter);

    if (start != NULL)
        *start = mod_start;
    if (end != NULL)
        *end = cur_end;
    return count;
}

/***************************************************************************
 * QUERY
 */

bool
memquery_from_os(const byte *pc, OUT dr_mem_info_t *info, OUT bool *have_type)
{
    memquery_iter_t iter;
    app_pc last_end = NULL;
    app_pc next_start = (app_pc) POINTER_MAX;
    bool found = false;
    ASSERT(info != NULL);
    memquery_iterator_start(&iter, (app_pc) pc, false/*won't alloc*/);
    while (memquery_iterator_next(&iter)) {
        if (pc >= iter.vm_start && pc < iter.vm_end) {
            info->base_pc = iter.vm_start;
            info->size = (iter.vm_end - iter.vm_start);
            info->prot = iter.prot;
            /* On early (pre-Fedora 2) kernels the vsyscall page is listed
             * with no permissions at all in the maps file.  Here's RHEL4:
             *   ffffe000-fffff000 ---p 00000000 00:00 0
             * We return "rx" as the permissions in that case.
             */
            if (vsyscall_page_start != NULL &&
                pc >= vsyscall_page_start && pc < vsyscall_page_start+PAGE_SIZE) {
                ASSERT(iter.vm_start == vsyscall_page_start);
                ASSERT(iter.vm_end - iter.vm_start == PAGE_SIZE);
                if (iter.prot == MEMPROT_NONE) {
                    info->prot = (MEMPROT_READ|MEMPROT_EXEC);
                }
            }
            found = true;
            break;
        } else if (pc < iter.vm_start) {
            next_start = iter.vm_start;
            break;
        }
        last_end = iter.vm_end;
    }
    memquery_iterator_stop(&iter);
    if (!found) {
        info->base_pc = last_end;
        info->size = (next_start - last_end);
        info->prot = MEMPROT_NONE;
        info->type = DR_MEMTYPE_FREE;
        *have_type = true;
    }
    return true;
}
