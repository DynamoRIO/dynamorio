/* **********************************************************
 * Copyright (c) 2011-2025 Google, Inc.  All rights reserved.
 * Copyright (c) 2008-2010 VMware, Inc.  All rights reserved.
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

#ifndef MODULE_ELF_H
#define MODULE_ELF_H

#include <elf.h> /* for ELF types */
#include "../module_shared.h"
#include "elf_defines.h"

bool
get_elf_platform(file_t f, dr_platform_t *platform);

bool
is_elf_so_header(app_pc base, size_t size);

bool
is_elf_partial_map(app_pc base, size_t size, uint memprot);

app_pc
module_vaddr_from_prog_header(app_pc prog_header, uint num_segments,
                              DR_PARAM_OUT app_pc *first_end, /* first segment's end */
                              DR_PARAM_OUT app_pc *max_end);

bool
module_read_program_header(app_pc base, uint segment_num,
                           DR_PARAM_OUT app_pc *segment_base,
                           DR_PARAM_OUT app_pc *segment_end,
                           DR_PARAM_OUT uint *segment_prot,
                           DR_PARAM_OUT size_t *segment_align);

ELF_ADDR
module_get_section_with_name(app_pc image, size_t img_size, const char *sec_name);

void
module_relocate_rel(app_pc modbase, os_privmod_data_t *pd, ELF_REL_TYPE *start,
                    ELF_REL_TYPE *end);

void
module_relocate_rela(app_pc modbase, os_privmod_data_t *pd, ELF_RELA_TYPE *start,
                     ELF_RELA_TYPE *end);

void
module_relocate_relr(app_pc modbase, os_privmod_data_t *pd, const ELF_WORD *relr,
                     size_t size);

bool
module_get_relro(app_pc base, DR_PARAM_OUT app_pc *relro_base,
                 DR_PARAM_OUT size_t *relro_size);

bool
module_read_os_data(app_pc base, bool dyn_reloc, DR_PARAM_OUT ptr_int_t *delta,
                    DR_PARAM_OUT os_module_data_t *os_data, DR_PARAM_OUT char **soname);

uint
module_segment_prot_to_osprot(ELF_PROGRAM_HEADER_TYPE *prog_hdr);

/* Data structure for loading an ELF.
 */
typedef struct elf_loader_t {
    const char *filename;
    file_t fd;
    ELF_HEADER_TYPE *ehdr;          /* Points into buf. */
    ELF_PROGRAM_HEADER_TYPE *phdrs; /* Points into buf or file_map. */
    app_pc load_base;               /* Load base. */
    ptr_int_t load_delta;           /* Delta from preferred base. */
    size_t image_size;              /* Size of the mapped image. */
    void *file_map;                 /* Whole file map, if needed. */
    size_t file_size;               /* Size of the file map. */

    /* Static buffer sized to hold most headers in a single read.  A typical ELF
     * file has an ELF header followed by program headers.  On my workstation,
     * most ELFs in /usr/lib have 7 phdrs, and the maximum is 9.  We choose 12
     * as a good upper bound and to allow for padding.  If the headers don't
     * fit, we fall back to file mapping.
     */
    byte buf[sizeof(ELF_HEADER_TYPE) + sizeof(ELF_PROGRAM_HEADER_TYPE) * 12];
} elf_loader_t;

typedef byte *(*map_fn_t)(file_t f, size_t *size DR_PARAM_INOUT, uint64 offs, app_pc addr,
                          uint prot /*MEMPROT_*/, map_flags_t map_flags);
typedef bool (*unmap_fn_t)(byte *map, size_t size);
/* Similar to map_fn_t, except that it expects the requested addr range to
 * already be reserved by an existing mapping. In addition to mapping the provided
 * file, it also updates bookkeeping if needed for the old and new maps.
 * On Linux, the atomic replacement of the old map with the new one may be achieved
 * by using MAP_FIXED (which is MAP_FILE_FIXED in map_flags_t). Note that MAP_FIXED
 * documents that the only safe way to use it is with a range that was previously
 * reserved using another mapping, otherwise it may end up forcibly removing
 * someone else's existing mappings.
 */
typedef byte *(*overlap_map_fn_t)(file_t f, size_t *size DR_PARAM_INOUT, uint64 offs,
                                  app_pc addr, uint prot /*MEMPROT_*/,
                                  map_flags_t map_flags);
typedef bool (*prot_fn_t)(byte *map, size_t size, uint prot /*MEMPROT_*/);
typedef void (*check_bounds_fn_t)(elf_loader_t *elf, byte *start, byte *end);
typedef void *(*memset_fn_t)(void *dst, int val, size_t size);

/* Initialized an ELF loader for use with the given file. */
bool
elf_loader_init(elf_loader_t *elf, const char *filename);

/* Frees resources needed to load the ELF, not the mapped image itself. */
void
elf_loader_destroy(elf_loader_t *elf);

/* Reads the main ELF header. */
ELF_HEADER_TYPE *
elf_loader_read_ehdr(elf_loader_t *elf);

/* Reads the ELF program headers, via read() or mmap() syscalls. */
ELF_PROGRAM_HEADER_TYPE *
elf_loader_read_phdrs(elf_loader_t *elf);

/* Shorthand to initialize the loader and read the ELF and program headers. */
bool
elf_loader_read_headers(elf_loader_t *elf, const char *filename);

/* Maps in the entire ELF file, including unmapped portions such as section
 * headers and debug info.  Does not re-map the same file if called twice.
 */
app_pc
elf_loader_map_file(elf_loader_t *elf, bool reachable);

/* Maps in the PT_LOAD segments of an ELF file, returning the base.  Must be
 * called after reading program headers with elf_loader_read_phdrs() or the
 * elf_loader_read_headers() shortcut.  All image mappings are done via the
 * provided function pointers.  If an overlap_map_func is specified, it is used
 * when we must unmap a certain part of a prior reserved address range and use it
 * for another mapping; unlike unmap_func followed by map_func, overlap_map_func
 * is expected to do this atomically to mitigate risk of that region getting
 * mmaped by another thread between the unmap and map events (i#7192). On
 * Linux, this may be achieved if the map call uses MAP_FIXED which
 * atomically unmaps the overlapping address range. Prefer to provide the
 * overlap_map_func implementation when possible.
 *
 * check_bounds_func is only called if fixed=true.
 *
 * XXX: fixed is only a hint as PIEs with a base of 0 should not use MAP_FIXED,
 * should we remove it?
 */
app_pc
elf_loader_map_phdrs(elf_loader_t *elf, bool fixed, map_fn_t map_func,
                     unmap_fn_t unmap_func, prot_fn_t prot_func,
                     check_bounds_fn_t check_bounds_func, memset_fn_t memset_func,
                     modload_flags_t flags, overlap_map_fn_t overlap_map_func);

/* Iterate program headers of a mapped ELF image and find the string that
 * PT_INTERP points to.  Typically this comes early in the file and is always
 * included in PT_LOAD segments, so we safely do this after the initial
 * mapping.
 */
const char *
elf_loader_find_pt_interp(elf_loader_t *elf);

#ifdef LINUX
bool
module_init_rseq(module_area_t *ma, bool at_map);
#endif

#endif /* MODULE_ELF_H */
