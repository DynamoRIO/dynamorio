/* *******************************************************************************
 * Copyright (c) 2010-2017 Google, Inc.  All rights reserved.
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

/*
 * memquery.c - memory querying code shared across unix platforms
 */

#include "../globals.h"
#include "memquery.h"
#include "module.h"

/* memquery_library_bounds_funcs is a collection of all functionality that we
 * swap out during memquery_library_bounds_by_iterator's unit test.
 */
typedef struct {
    bool (*memquery_iterator_start)(memquery_iter_t *iter, app_pc start, bool may_alloc);
    bool (*memquery_iterator_next)(memquery_iter_t *iter);
    void (*memquery_iterator_stop)(memquery_iter_t *iter);
    bool (*module_is_header)(app_pc base, size_t size);
    bool (*module_walk_program_headers)(app_pc base, size_t view_size, bool at_map,
                                        bool dyn_reloc,
                                        OUT app_pc *out_base /* relative pc */,
                                        OUT app_pc *out_first_end /* relative pc */,
                                        OUT app_pc *out_max_end /* relative pc */,
                                        OUT char **out_soname,
                                        OUT os_module_data_t *out_data);
} memquery_library_bounds_funcs;

/* real_memquery_library_bounds_funcs is the collection of "real" dependencies
 * for use by code outside the standalone unit test.
 */
static const memquery_library_bounds_funcs real_memquery_library_bounds_funcs = {
    .memquery_iterator_start = memquery_iterator_start,
    .memquery_iterator_next = memquery_iterator_next,
    .memquery_iterator_stop = memquery_iterator_stop,
    .module_is_header = module_is_header,
    .module_walk_program_headers = module_walk_program_headers,
};

/* Forward declaration for use in the unit test */
static int
memquery_library_bounds_by_iterator_internal(
    const char *name, app_pc *start /*IN/OUT*/, app_pc *end /*OUT*/,
    char *fulldir /*OPTIONAL OUT*/, size_t fulldir_size, char *filename /*OPTIONAL OUT*/,
    size_t filename_size, const memquery_library_bounds_funcs *funcs);

#if defined(STANDALONE_UNIT_TEST) || defined(RECORD_MEMQUERY)
#    include "memquery_test.h"
#endif

/***************************************************************************
 * LIBRARY BOUNDS
 */

/* See memquery.h for full interface specs, which are identical to
 * memquery_library_bounds().
 *
 * This module is tested by a standalone unit test in memquery_test.h by passing
 * in a set of fake memquery_library_bounds_funcs. The "real"
 * memquery_library_bounds_by_iterator is below and hard-codes the use of
 * real_memquery_library_bounds_funcs.
 */
int
memquery_library_bounds_by_iterator_internal(
    const char *name, app_pc *start /*IN/OUT*/, app_pc *end /*OUT*/,
    char *fulldir /*OPTIONAL OUT*/, size_t fulldir_size, char *filename /*OPTIONAL OUT*/,
    size_t filename_size, const memquery_library_bounds_funcs *funcs)
{
    int count = 0;
    bool found_library = false;
    char libname[MAXIMUM_PATH];
    const char *name_cmp = name;
    memquery_iter_t iter;
    app_pc target = *start;
    app_pc last_lib_base = NULL;
    app_pc last_lib_end = NULL;
    app_pc prev_base = NULL;
    app_pc prev_end = NULL;
    uint prev_prot = 0;
    size_t image_size = 0;
    app_pc cur_end = NULL;
    app_pc mod_start = NULL;
    ASSERT(name != NULL || start != NULL);

    /* If name is non-NULL, start can be NULL, so we have to walk the whole
     * address space even when we have syscalls for memquery (e.g., on Mac).
     * Even if start is non-NULL, it could be in the middle of the library.
     */
    funcs->memquery_iterator_start(&iter, NULL,
                                   /* We're never called from a fragile place like a
                                    * signal handler, so as long as it's not real early
                                    * it's ok to alloc.
                                    */
                                   dynamo_heap_initialized);
    libname[0] = '\0';
    while (funcs->memquery_iterator_next(&iter)) {
        LOG(GLOBAL, LOG_VMAREAS, 5, "start=" PFX " end=" PFX " prot=%x comment=%s\n",
            iter.vm_start, iter.vm_end, iter.prot, iter.comment);

        /* Record the base of each differently-named set of entries up until
         * we find our target, when we'll clobber libpath
         */
        if (!found_library &&
            ((iter.comment[0] != '\0' &&
              strncmp(libname, iter.comment, BUFFER_SIZE_ELEMENTS(libname)) != 0) ||
             (iter.comment[0] == '\0' && prev_end != NULL &&
              prev_end != iter.vm_start))) {
            last_lib_base = iter.vm_start;
            /* Include a prior anon mapping if interrupted and a header and this
             * mapping is not a header.  This happens for some page mapping
             * schemes (i#2566).
             */
            if (prev_end == iter.vm_start && prev_prot == (MEMPROT_READ | MEMPROT_EXEC) &&
                funcs->module_is_header(prev_base, prev_end - prev_base) &&
                !funcs->module_is_header(iter.vm_start, iter.vm_end - iter.vm_start))
                last_lib_base = prev_base;
            /* last_lib_end is used to know what's readable beyond last_lib_base */
            if (TEST(MEMPROT_READ, iter.prot))
                last_lib_end = iter.vm_end;
            else
                last_lib_end = last_lib_base;
            /* remember name so we can find the base of a multiply-mapped so */
            strncpy(libname, iter.comment, BUFFER_SIZE_ELEMENTS(libname));
            NULL_TERMINATE_BUFFER(libname);
        }

        if ((name_cmp != NULL &&
             (strstr(iter.comment, name_cmp) != NULL ||
              /* For Linux, include mid-library (non-.bss) anonymous mappings.
               * Our private loader
               * fills mapping holes with anonymous memory instead of a
               * MEMPROT_NONE mapping from the original file.
               * For Mac, this includes mid-library .bss.
               */
              (found_library && iter.comment[0] == '\0' && image_size != 0 &&
               iter.vm_end - mod_start < image_size))) ||
            (name == NULL && target >= iter.vm_start && target < iter.vm_end)) {
            if (!found_library && iter.comment[0] == '\0' && last_lib_base == NULL) {
                /* Wait for the next entry which should have a file backing. */
                target = iter.vm_end;
            } else if (!found_library) {
                char *dst = (fulldir != NULL) ? fulldir : libname;
                const char *src = (iter.comment[0] == '\0') ? libname : iter.comment;
                size_t dstsz =
                    (fulldir != NULL) ? fulldir_size : BUFFER_SIZE_ELEMENTS(libname);
                size_t mod_readable_sz;
                if (src != dst) {
                    if (dst == fulldir) {
                        /* Just the path.  We use strstr for name_cmp. */
                        char *slash = strrchr(src, '/');
                        ASSERT_CURIOSITY(slash != NULL);
                        ASSERT_CURIOSITY((slash - src) < dstsz);
                        /* we keep the last '/' at end */
                        ++slash;
                        int copy_bytes = MIN(dstsz - 1, slash - src);
                        strncpy(dst, src, copy_bytes);
                        // dst was not 0-terminated by the strncpy because
                        // copy_bytes is definitely less than strlen(src);
                        // copy_bytes is either the index of the last byte in
                        // the dst buffer (dstsz-1) or the index immediately
                        // after the slash byte, so 0-terminate there:
                        dst[copy_bytes] = '\0';
                        if (filename != NULL && slash != NULL) {
                            /* slash is filename */
                            strncpy(filename, slash, MIN(strlen(slash), filename_size));
                            filename[MIN(strlen(slash), filename_size - 1)] = '\0';
                        } else
                            filename[0] = '\0';
                    } else {
                        strncpy(dst, src, dstsz);
                        /* Ensure zero termination in case dstsz < srcsz. */
                        dst[dstsz - 1] = '\0';
                    }
                }
                if (name == NULL)
                    name_cmp = dst;
                found_library = true;
                /* Most libraries have multiple segments, and some have the
                 * ELF header repeated in a later mapping, so we can't rely
                 * on is_elf_so_header() and header walking.
                 * We use the name tracking to remember the first entry
                 * that had this name.
                 */
                if (last_lib_base == NULL) {
                    mod_start = iter.vm_start;
                    mod_readable_sz = iter.vm_end - iter.vm_start;
                } else {
                    mod_start = last_lib_base;
                    mod_readable_sz = last_lib_end - last_lib_base;
                }
                if (funcs->module_is_header(mod_start, mod_readable_sz)) {
                    app_pc mod_base, mod_end;
                    if (funcs->module_walk_program_headers(
                            mod_start, mod_readable_sz, false,
                            /*i#1589: ld.so relocated .dynamic*/
                            true, &mod_base, NULL, &mod_end, NULL, NULL)) {
                        image_size = mod_end - mod_base;
                        LOG(GLOBAL, LOG_VMAREAS, 4, "%s: image size is " PIFX "\n",
                            __FUNCTION__, image_size);
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
        } else if (found_library) {
            /* hit non-matching, we expect module segments to be adjacent */
            break;
        }
        prev_base = iter.vm_start;
        prev_end = iter.vm_end;
        prev_prot = iter.prot;
    }

    /* Xref PR 208443: .bss sections are anonymous (no file name listed in
     * maps file), but not every library has one.  We have to parse the ELF
     * header to know since we can't assume that a subsequent anonymous
     * region is .bss. */
    if (image_size != 0 && cur_end - mod_start < image_size) {
        if (iter.comment[0] != '\0') {
            /* There's something else in the text-data gap: xref i#2641. */
        } else {
            /* Found a .bss section. Check current mapping (note might only be
             * part of the mapping (due to os region merging? FIXME investigate).
             */
            ASSERT_CURIOSITY(iter.vm_start == cur_end /* no gaps, FIXME might there be
                                                       * a gap if the file has large
                                                       * alignment and no data section?
                                                       * curiosity for now*/);
            ASSERT_CURIOSITY(iter.inode == 0); /* .bss is anonymous */
            /* should be big enough */
            ASSERT_CURIOSITY(iter.vm_end - mod_start >= image_size);
        }
        count++;
        cur_end = mod_start + image_size;
    } else {
        /* Shouldn't have more mapped then the size of the module, unless it's a
         * second adjacent separate map of the same file.  Curiosity for now. */
        ASSERT_CURIOSITY(image_size == 0 || cur_end - mod_start == image_size);
    }
    funcs->memquery_iterator_stop(&iter);

    if (name == NULL && *start < mod_start)
        count = 0; /* Our target adjustment missed: we never found a file-backed entry */
    if (start != NULL)
        *start = mod_start;
    if (end != NULL)
        *end = cur_end;
    return count;
}

/* See comment for memquery_library_bounds_by_iterator_internal. */
int
memquery_library_bounds_by_iterator(const char *name, app_pc *start /*IN/OUT*/,
                                    app_pc *end /*OUT*/, char *fulldir /*OPTIONAL OUT*/,
                                    size_t fulldir_size, char *filename /*OPTIONAL OUT*/,
                                    size_t filename_size)
{
    return memquery_library_bounds_by_iterator_internal(
        name, start, end, fulldir, fulldir_size, filename, filename_size,
        &real_memquery_library_bounds_funcs);
}
