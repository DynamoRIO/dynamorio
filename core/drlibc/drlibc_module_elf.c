/* *******************************************************************************
 * Copyright (c) 2012-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2011 Massachusetts Institute of Technology  All rights reserved.
 * Copyright (c) 2008-2010 VMware, Inc.  All rights reserved.
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

/* ELF module analysis routines shared between core and non-core. */

#include "../globals.h"
#include "../module_shared.h"
#include "drlibc_unix.h"
#include "module_private.h"
#include "../utils.h"
#include "instrument.h"
#include <stddef.h> /* offsetof */
#include <link.h>   /* Elf_Symndx */

typedef union _elf_generic_header_t {
    Elf64_Ehdr elf64;
    Elf32_Ehdr elf32;
} elf_generic_header_t;

/* This routine is duplicated in privload_mem_is_elf_so_header. Any update here
 * should be updated in privload_mem_is_elf_so_header.
 */
/* Is there an ELF header for a shared object at address 'base'?
 * If size == 0 then checks for header readability else assumes that size bytes from
 * base are readable (unmap races are then callers responsibility).
 */
static bool
is_elf_so_header_common(app_pc base, size_t size, bool memory)
{
    /* FIXME We could check more fields in the header just as the
     * dlopen() does. */
    static const unsigned char ei_expected[SELFMAG] = {
        [EI_MAG0] = ELFMAG0, [EI_MAG1] = ELFMAG1, [EI_MAG2] = ELFMAG2, [EI_MAG3] = ELFMAG3
    };
    ELF_HEADER_TYPE elf_header;

    if (base == NULL) {
        ASSERT(false && "is_elf_so_header(): NULL base");
        return false;
    }

    /* Read the header.  We used to directly deref if size >= sizeof(ELF_HEADER_TYPE)
     * but given that we now have safe_read_fast() it's best to always use it and
     * avoid races (like i#2113).  However, the non-fast version hits deadlock on
     * memquery during client init, so we use a special routine safe_read_if_fast().
     */
    if (size >= sizeof(ELF_HEADER_TYPE)) {
        if (!safe_read_if_fast(base, sizeof(ELF_HEADER_TYPE), &elf_header))
            return false;
    } else if (size == 0) {
        if (!d_r_safe_read(base, sizeof(ELF_HEADER_TYPE), &elf_header))
            return false;
    } else {
        return false;
    }

    /* We check the first 4 bytes which is the magic number. */
    if ((size == 0 || size >= sizeof(ELF_HEADER_TYPE)) &&
        elf_header.e_ident[EI_MAG0] == ei_expected[EI_MAG0] &&
        elf_header.e_ident[EI_MAG1] == ei_expected[EI_MAG1] &&
        elf_header.e_ident[EI_MAG2] == ei_expected[EI_MAG2] &&
        elf_header.e_ident[EI_MAG3] == ei_expected[EI_MAG3] &&
        /* PR 475158: if an app loads a linkable but not loadable
         * file (e.g., .o file) we don't want to treat as a module
         */
        (elf_header.e_type == ET_DYN || elf_header.e_type == ET_EXEC)) {
        /* i#157, we do more check to make sure we load the right modules,
         * i.e. 32/64-bit libraries.
         * We check again in privload_map_and_relocate() in loader for nice
         * error message.
         * Xref i#1345 for supporting mixed libs, which makes more sense for
         * standalone mode tools like those using drsyms (i#1532) or
         * dr_map_executable_file, but we just don't support that yet until we
         * remove our hardcoded type defines in module_elf.h.
         *
         * i#1684: We do allow mixing arches of the same bitwidth to better support
         * drdecode tools.  We have no standalone_library var access here to limit
         * this relaxation to tools; we assume DR managed code will hit other
         * problems later for the wrong arch and that recognizing an other-arch
         * file as an ELF won't cause problems.
         */
        if ((elf_header.e_version != 1) ||
            (memory && elf_header.e_ehsize != sizeof(ELF_HEADER_TYPE)) ||
            (memory &&
#ifdef X64
             elf_header.e_machine != EM_X86_64 && elf_header.e_machine != EM_AARCH64 &&
             elf_header.e_machine != EM_RISCV
#else
             elf_header.e_machine != EM_386 && elf_header.e_machine != EM_ARM
#endif
             ))
            return false;
        /* FIXME - should we add any of these to the check? For real
         * modules all of these should hold. */
        ASSERT_CURIOSITY(elf_header.e_version == 1);
        ASSERT_CURIOSITY(!memory || elf_header.e_ehsize == sizeof(ELF_HEADER_TYPE));
        ASSERT_CURIOSITY(elf_header.e_ident[EI_OSABI] == ELFOSABI_SYSV ||
                         elf_header.e_ident[EI_OSABI] == ELFOSABI_LINUX);
        ASSERT_CURIOSITY(!memory ||
#ifdef X64
                         elf_header.e_machine == EM_X86_64 ||
                         elf_header.e_machine == EM_AARCH64 ||
                         elf_header.e_machine == EM_RISCV
#else
                         elf_header.e_machine == EM_386 || elf_header.e_machine == EM_ARM
#endif
        );
        return true;
    }
    return false;
}

/* i#727: Recommend passing 0 as size if not known if the header can be safely
 * read.
 */
bool
is_elf_so_header(app_pc base, size_t size)
{
    return is_elf_so_header_common(base, size, true);
}

uint
module_segment_prot_to_osprot(ELF_PROGRAM_HEADER_TYPE *prog_hdr)
{
    uint segment_prot = 0;
    if (TEST(PF_X, prog_hdr->p_flags))
        segment_prot |= MEMPROT_EXEC;
    if (TEST(PF_W, prog_hdr->p_flags))
        segment_prot |= MEMPROT_WRITE;
    if (TEST(PF_R, prog_hdr->p_flags))
        segment_prot |= MEMPROT_READ;
    return segment_prot;
}

/* XXX: This routine may be called before dynamorio relocation when we are
 * in a fragile state and thus no globals access or use of ASSERT/LOG/STATS!
 */
/* Returns the minimum p_vaddr field, aligned to page boundaries, in
 * the loadable segments in the prog_header array, or POINTER_MAX if
 * there are no loadable segments.
 */
app_pc
module_vaddr_from_prog_header(app_pc prog_header, uint num_segments,
                              OUT app_pc *out_first_end, OUT app_pc *out_max_end)
{
    uint i;
    app_pc min_vaddr = (app_pc)POINTER_MAX;
    app_pc max_end = (app_pc)PTR_UINT_0;
    app_pc first_end = NULL;
    for (i = 0; i < num_segments; i++) {
        /* Without the ELF header we use sizeof instead of elf_hdr->e_phentsize
         * which must be a reliable assumption as dl_iterate_phdr() doesn't
         * bother to deliver the entry size.
         */
        ELF_PROGRAM_HEADER_TYPE *prog_hdr =
            (ELF_PROGRAM_HEADER_TYPE *)(prog_header +
                                        i * sizeof(ELF_PROGRAM_HEADER_TYPE));
        if (prog_hdr->p_type == PT_LOAD) {
            /* ELF requires p_vaddr to already be aligned to p_align */
            /* XXX i#4737: Our PAGE_SIZE may not match the size on a cross-arch file
             * that was loaded on another machine.  We're also ignoring
             * prog_hdr->p_align here as it is actually complex to use: some loaders
             * (notably some kernels) seem to ignore it.  These corner cases are left
             * as unsolved for now.
             */
            min_vaddr =
                MIN(min_vaddr, (app_pc)ALIGN_BACKWARD(prog_hdr->p_vaddr, PAGE_SIZE));
            if (min_vaddr == (app_pc)prog_hdr->p_vaddr)
                first_end = (app_pc)prog_hdr->p_vaddr + prog_hdr->p_memsz;
            max_end = MAX(
                max_end,
                (app_pc)ALIGN_FORWARD(prog_hdr->p_vaddr + prog_hdr->p_memsz, PAGE_SIZE));
        }
    }
    if (out_first_end != NULL)
        *out_first_end = first_end;
    if (out_max_end != NULL)
        *out_max_end = max_end;
    return min_vaddr;
}

bool
module_get_platform(file_t f, dr_platform_t *platform, dr_platform_t *alt_platform)
{
    elf_generic_header_t elf_header;
    if (alt_platform != NULL)
        *alt_platform = DR_PLATFORM_NONE;
    if (os_read(f, &elf_header, sizeof(elf_header)) != sizeof(elf_header))
        return false;
    if (!is_elf_so_header_common((app_pc)&elf_header, sizeof(elf_header), false))
        return false;
    ASSERT(offsetof(Elf64_Ehdr, e_machine) == offsetof(Elf32_Ehdr, e_machine));
    switch (elf_header.elf64.e_machine) {
    case EM_X86_64:
#ifdef EM_AARCH64
    case EM_AARCH64:
#endif
        *platform = DR_PLATFORM_64BIT;
        break;
    case EM_386:
    case EM_ARM: *platform = DR_PLATFORM_32BIT; break;
    default: return false;
    }
    return true;
}

/* Get the module text section from the mapped image file,
 * Note that it must be the image file, not the loaded module.
 */
ELF_ADDR
module_get_text_section(app_pc file_map, size_t file_size)
{
    ELF_HEADER_TYPE *elf_hdr = (ELF_HEADER_TYPE *)file_map;
    ELF_SECTION_HEADER_TYPE *sec_hdr;
    char *strtab;
    uint i;
    ASSERT(is_elf_so_header(file_map, file_size));
    ASSERT(elf_hdr->e_shoff < file_size);
    ASSERT(elf_hdr->e_shentsize == sizeof(ELF_SECTION_HEADER_TYPE));
    ASSERT(elf_hdr->e_shoff + elf_hdr->e_shentsize * elf_hdr->e_shnum <= file_size);
    sec_hdr = (ELF_SECTION_HEADER_TYPE *)(file_map + elf_hdr->e_shoff);
    strtab = (char *)(file_map + sec_hdr[elf_hdr->e_shstrndx].sh_offset);
    for (i = 0; i < elf_hdr->e_shnum; i++) {
        if (strcmp(".text", strtab + sec_hdr->sh_name) == 0)
            return sec_hdr->sh_addr;
        ++sec_hdr;
    }
    /* ELF doesn't require that there's a section named ".text". */
    ASSERT_CURIOSITY(false);
    return 0;
}

/* Read until EOF or error. Return number of bytes read. */
static size_t
os_read_until(file_t fd, void *buf, size_t toread)
{
    size_t orig_toread = toread;
    ssize_t nread;
    while (toread > 0) {
        nread = os_read(fd, buf, toread);
        if (nread <= 0)
            break;
        toread -= nread;
        buf = (app_pc)buf + nread;
    }
    return orig_toread - toread;
}

bool
elf_loader_init(elf_loader_t *elf, const char *filename)
{
    memset(elf, 0, sizeof(*elf));
    elf->filename = filename;
    elf->fd = os_open(filename, OS_OPEN_READ);
    return elf->fd != INVALID_FILE;
}

void
elf_loader_destroy(elf_loader_t *elf)
{
    if (elf->fd != INVALID_FILE) {
        os_close(elf->fd);
    }
    if (elf->file_map != NULL) {
        os_unmap_file(elf->file_map, elf->file_size);
    }
    memset(elf, 0, sizeof(*elf));
}

ELF_HEADER_TYPE *
elf_loader_read_ehdr(elf_loader_t *elf)
{
    /* The initial read is sized to read both ehdr and all phdrs. */
    if (elf->fd == INVALID_FILE)
        return NULL;
    if (elf->file_map != NULL) {
        /* The user mapped the entire file up front, so use it. */
        elf->ehdr = (ELF_HEADER_TYPE *)elf->file_map;
    } else {
        size_t size = os_read_until(elf->fd, elf->buf, sizeof(elf->buf));
        if (size == 0)
            return NULL;
        if (!is_elf_so_header(elf->buf, size))
            return NULL;
        elf->ehdr = (ELF_HEADER_TYPE *)elf->buf;
    }
    return elf->ehdr;
}

app_pc
elf_loader_map_file(elf_loader_t *elf, bool reachable)
{
    uint64 size64;
    if (elf->file_map != NULL)
        return elf->file_map;
    if (elf->fd == INVALID_FILE)
        return NULL;
    if (!os_get_file_size_by_handle(elf->fd, &size64))
        return NULL;
    ASSERT_TRUNCATE(elf->file_size, size_t, size64);
    elf->file_size = (size_t)size64; /* truncate */
    /* We use os_map_file instead of map_file since this mapping is temporary.
     * We don't need to add and remove it from dynamo_areas.
     */
    elf->file_map =
        os_map_file(elf->fd, &elf->file_size, 0, NULL, MEMPROT_READ,
                    MAP_FILE_COPY_ON_WRITE | (reachable ? MAP_FILE_REACHABLE : 0));
    return elf->file_map;
}

ELF_PROGRAM_HEADER_TYPE *
elf_loader_read_phdrs(elf_loader_t *elf)
{
    size_t ph_off;
    size_t ph_size;
    if (elf->ehdr == NULL)
        return NULL;
    ph_off = elf->ehdr->e_phoff;
    ph_size = elf->ehdr->e_phnum * elf->ehdr->e_phentsize;
    if (elf->file_map == NULL && ph_off + ph_size < sizeof(elf->buf)) {
        /* We already read phdrs, and they are in buf. */
        elf->phdrs = (ELF_PROGRAM_HEADER_TYPE *)(elf->buf + elf->ehdr->e_phoff);
    } else {
        /* We have large or distant phdrs, so map the whole file.  We could
         * seek and read just the phdrs to avoid disturbing the address space,
         * but that would introduce a dependency on DR's heap.
         */
        if (elf_loader_map_file(elf, false /*!reachable*/) == NULL)
            return NULL;
        elf->phdrs = (ELF_PROGRAM_HEADER_TYPE *)(elf->file_map + elf->ehdr->e_phoff);
    }
    return elf->phdrs;
}

bool
elf_loader_read_headers(elf_loader_t *elf, const char *filename)
{
    if (!elf_loader_init(elf, filename))
        return false;
    if (elf_loader_read_ehdr(elf) == NULL)
        return false;
    if (elf_loader_read_phdrs(elf) == NULL)
        return false;
    return true;
}

app_pc
elf_loader_map_phdrs(elf_loader_t *elf, bool fixed, map_fn_t map_func,
                     unmap_fn_t unmap_func, prot_fn_t prot_func,
                     check_bounds_fn_t check_bounds_func, memset_fn_t memset_func,
                     modload_flags_t flags)
{
    app_pc lib_base, lib_end, last_end;
    ELF_HEADER_TYPE *elf_hdr = elf->ehdr;
    app_pc map_base, map_end;
    reg_t pg_offs;
    uint seg_prot, i;
    ptr_int_t delta;
    size_t initial_map_size;

    ASSERT(elf->phdrs != NULL && "call elf_loader_read_phdrs() first");
    if (elf->phdrs == NULL)
        return NULL;

    map_base = module_vaddr_from_prog_header((app_pc)elf->phdrs, elf->ehdr->e_phnum, NULL,
                                             &map_end);

    if (fixed && check_bounds_func != NULL)
        (*check_bounds_func)(elf, map_base, map_end);

    elf->image_size = map_end - map_base;

    /* reserve the memory from os for library */
    initial_map_size = elf->image_size;
    if (TEST(MODLOAD_SEPARATE_BSS, flags)) {
        /* place an extra no-access page after .bss */
        initial_map_size += PAGE_SIZE;
    }
    lib_base = (*map_func)(-1, &initial_map_size, 0, map_base,
                           MEMPROT_NONE, /* so the separating page is no-access */
                           MAP_FILE_COPY_ON_WRITE | MAP_FILE_IMAGE |
                               /* i#1001: a PIE executable may have NULL as preferred
                                * base, in which case the map can be anywhere
                                */
                               ((fixed && map_base != NULL) ? MAP_FILE_FIXED : 0) |
                               (TEST(MODLOAD_REACHABLE, flags) ? MAP_FILE_REACHABLE : 0) |
                               (TEST(MODLOAD_IS_APP, flags) ? MAP_FILE_APP : 0));
    if (lib_base == NULL)
        return NULL;
    LOG(GLOBAL, LOG_LOADER, 3,
        "%s: initial reservation " PFX "-" PFX " vs preferred " PFX "\n", __FUNCTION__,
        lib_base, lib_base + initial_map_size, map_base);
    if (TEST(MODLOAD_SEPARATE_BSS, flags) && initial_map_size > elf->image_size)
        elf->image_size = initial_map_size - PAGE_SIZE;
    else
        elf->image_size = initial_map_size;
    lib_end = lib_base + elf->image_size;
    elf->load_base = lib_base;
    ASSERT(elf->load_delta == 0 || map_base == NULL);

    if (map_base != NULL && map_base != lib_base) {
        /* the mapped memory is not at preferred address,
         * should be ok if it is still reachable for X64,
         * which will be checked later.
         */
        LOG(GLOBAL, LOG_LOADER, 1, "%s: module not loaded at preferred address\n",
            __FUNCTION__);
    }
    delta = lib_base - map_base;
    elf->load_delta = delta;

    /* walk over the program header to load the individual segments */
    last_end = lib_base;
    for (i = 0; i < elf_hdr->e_phnum; i++) {
        app_pc seg_base, seg_end, map, file_end;
        size_t seg_size;
        ELF_PROGRAM_HEADER_TYPE *prog_hdr =
            (ELF_PROGRAM_HEADER_TYPE *)((byte *)elf->phdrs + i * elf_hdr->e_phentsize);
        if (prog_hdr->p_type == PT_LOAD) {
            bool do_mmap = true;
            /* XXX i#4737: Our PAGE_SIZE may not match the size on a cross-arch file
             * that was loaded on another machine.  We're also ignoring
             * prog_hdr->p_align here as it is actually complex to use: some loaders
             * (notably some kernels) seem to ignore it.  These corner cases are left
             * as unsolved for now.
             */
            seg_base = (app_pc)ALIGN_BACKWARD(prog_hdr->p_vaddr, PAGE_SIZE) + delta;
            seg_end =
                (app_pc)ALIGN_FORWARD(prog_hdr->p_vaddr + prog_hdr->p_filesz, PAGE_SIZE) +
                delta;
            app_pc mem_end =
                (app_pc)ALIGN_FORWARD(prog_hdr->p_vaddr + prog_hdr->p_memsz, PAGE_SIZE) +
                delta;
            seg_size = seg_end - seg_base;
            if (seg_base != last_end) {
                /* XXX: a hole, I reserve this space instead of unmap it */
                size_t hole_size = seg_base - last_end;
                (*prot_func)(last_end, hole_size, MEMPROT_NONE);
            }
            seg_prot = module_segment_prot_to_osprot(prog_hdr);
            pg_offs = ALIGN_BACKWARD(prog_hdr->p_offset, PAGE_SIZE);
            if (TEST(MODLOAD_SKIP_WRITABLE, flags) && TEST(MEMPROT_WRITE, seg_prot) &&
                mem_end == lib_end) {
                /* We only actually skip if it's the final segment, to allow
                 * unmapping with a single mmap and not worrying about sthg
                 * else having been unmapped at the end in the meantime.
                 */
                do_mmap = false;
                elf->image_size = last_end - lib_base;
            }
            /* XXX:
             * This function can be called after dynamo_heap_initialized,
             * and we will use map_file instead of os_map_file.
             * However, map_file does not allow mmap with overlapped memory,
             * so we have to unmap the old memory first.
             * This might be a problem, e.g.
             * one thread unmaps the memory and before mapping the actual file,
             * another thread requests memory via mmap takes the memory here,
             * a racy condition.
             */
            if (seg_size > 0) { /* i#1872: handle empty segments */
                if (do_mmap) {
                    (*unmap_func)(seg_base, seg_size);
                    map = (*map_func)(
                        elf->fd, &seg_size, pg_offs, seg_base /* base */,
                        seg_prot | MEMPROT_WRITE /* prot */,
                        MAP_FILE_COPY_ON_WRITE /*writes should not change file*/ |
                            MAP_FILE_IMAGE |
                            /* we don't need MAP_FILE_REACHABLE b/c we're fixed */
                            MAP_FILE_FIXED);
                    ASSERT(map != NULL);
                    /* fill zeros at extend size */
                    file_end = (app_pc)prog_hdr->p_vaddr + prog_hdr->p_filesz;
                    if (seg_end > file_end + delta) {
                        /* There is typically one RW PT_LOAD segment for .data and
                         * .bss.  If .data ends and .bss starts before filesz bytes,
                         * we need to zero the .bss bytes manually.
                         */
                        (*memset_func)(file_end + delta, 0, seg_end - (file_end + delta));
                    }
                }
            }
            seg_end =
                (app_pc)ALIGN_FORWARD(prog_hdr->p_vaddr + prog_hdr->p_memsz, PAGE_SIZE) +
                delta;
            seg_size = seg_end - seg_base;
            if (seg_size > 0 && do_mmap)
                (*prot_func)(seg_base, seg_size, seg_prot);
            last_end = seg_end;
        }
    }
    ASSERT(last_end == lib_end);
    /* FIXME: recover from map failure rather than relying on asserts. */

    return lib_base;
}

/* Iterate program headers of a mapped ELF image and find the string that
 * PT_INTERP points to.  Typically this comes early in the file and is always
 * included in PT_LOAD segments, so we safely do this after the initial
 * mapping.
 */
const char *
elf_loader_find_pt_interp(elf_loader_t *elf)
{
    int i;
    ELF_HEADER_TYPE *ehdr = elf->ehdr;
    ELF_PROGRAM_HEADER_TYPE *phdrs = elf->phdrs;

    ASSERT(elf->load_base != NULL && "call elf_loader_map_phdrs() first");
    if (ehdr == NULL || phdrs == NULL || elf->load_base == NULL)
        return NULL;
    for (i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type == PT_INTERP) {
            return (const char *)(phdrs[i].p_vaddr + elf->load_delta);
        }
    }

    return NULL;
}
