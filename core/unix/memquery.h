/* *******************************************************************************
 * Copyright (c) 2013-2017 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
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
 * memquery.h - cross-unix-platform memory iteration and querying
 */

#ifndef _MEMQUERY_H_
#define _MEMQUERY_H_ 1

typedef struct _memquery_iter_t {
    /* --- EXTERNAL OUT PARAMS --- */
    app_pc vm_start;
    app_pc vm_end;
    uint prot;
    size_t offset; /* offset into the file being mapped */
    /* XXX: use ino_t?  We need to know what size code to use for the
     * scanf and I don't trust that we, the maps file, and any clients
     * will all agree on its size since it seems to be defined
     * differently depending on whether large file support is compiled
     * in etc.  Just using uint64 might be safer (see also inode in
     * module_names_t).
     */
    /* XXX i#58: not filled in on Mac */
    uint64 inode;
    /* Path of file backing, or name of region ("[vdso]", e.g.) */
    const char *comment;

    /* --- INTERNAL --- */
    /* Indicates whether the heap can be used.  If the iteration is done
     * very early during DR init, the heap is not set up yet.
     */
    bool may_alloc;
    /* For internal use by the iterator w/o requiring dynamic allocation and
     * without using static data and limiting to one iterator (and having
     * to unprotect and reprotect if in .data).
     */
#define MEMQUERY_INTERNAL_DATA_LEN 116 /* 104 bytes needed for MacOS 64-bit */
    char internal[MEMQUERY_INTERNAL_DATA_LEN];
} memquery_iter_t;

void
memquery_init(void);

void
memquery_exit(void);

/* The passed-in "start" parameter is a performance hint to start
 * iteration at the region containing that address.  However, the
 * iterator may start before that point.
 *
 * Pass true for may_alloc, unless the caller is in a fragile situation (e.g.,
 * a signal handler) where we shouldn't allocate heap.
 */
bool
memquery_iterator_start(memquery_iter_t *iter, app_pc start, bool may_alloc);

void
memquery_iterator_stop(memquery_iter_t *iter);

bool
memquery_iterator_next(memquery_iter_t *iter);

/* Finds the bounds of the library with name "name".  If "name" is NULL,
 * "start" must be non-NULL and must be an address within the library.
 * The name match is done using strstr.
 *
 * Note that we can't just walk backward and look for is_elf_so_header() b/c
 * some ELF files are mapped twice and it's not clear how to know if
 * one has hit the original header or a later header: this is why we allow
 * any address in the library.
 * The resulting start and end are the bounds of the library.
 * They include any .bss section.
 * Return value is the number of distinct memory regions that comprise the library.
 */
int
memquery_library_bounds(const char *name, app_pc *start /*IN/OUT*/, app_pc *end /*OUT*/,
                        char *fulldir /*OPTIONAL OUT*/, size_t fulldir_size,
                        char *filename /*OPTIONAL OUT*/, size_t filename_size);

/* Interface is identical to memquery_library_bounds().  This is an iterator-based
 * impl shared among Linux and Mac.
 */
int
memquery_library_bounds_by_iterator(const char *name, app_pc *start /*IN/OUT*/,
                                    app_pc *end /*OUT*/, char *fulldir /*OPTIONAL OUT*/,
                                    size_t fulldir_size, char *filename /*OPTIONAL OUT*/,
                                    size_t filename_size);

/* XXX i#1270: ideally we could have os.c use generic memquery iterator code,
 * but the probe + dl_iterate_phdr approach is difficult to fit into that mold
 * without relying on allmem, so for now we have this full caller routine pulled
 * into here.
 */
#ifndef HAVE_MEMINFO
int
find_vm_areas_via_probe(void);
#endif

/* This routine might acquire locks.  is_readable_without_exception_query_os_noblock()
 * can be used to avoid blocking.
 */
bool
memquery_from_os(const byte *pc, OUT dr_mem_info_t *info, OUT bool *have_type);

/* The result can change if another thread grabs the lock, but this will identify
 * whether the current thread holds the lock, avoiding a hang.
 */
bool
memquery_from_os_will_block(void);

#endif /* _MEMQUERY_H_ */
