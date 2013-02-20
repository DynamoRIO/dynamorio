/* *******************************************************************************
 * Copyright (c) 2012 Google, Inc.  All rights reserved.
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

#include "../globals.h"
#include "../module_shared.h"
#include "os_private.h"
#include "../utils.h"
#include "../x86/instrument.h"
#include <string.h>
#include <stddef.h> /* offsetof */
#include <link.h>   /* Elf_Symndx */

typedef union _elf_generic_header_t {
    Elf64_Ehdr elf64;
    Elf32_Ehdr elf32;
} elf_generic_header_t;

#ifdef NOT_DYNAMORIO_CORE_PROPER
# undef LOG
# define LOG(...) /* nothing */
#else /* !NOT_DYNAMORIO_CORE_PROPER */

#ifdef CLIENT_INTERFACE
typedef struct _elf_import_iterator_t {
    dr_symbol_import_t symbol_import;   /* symbol import returned by next() */

    /* This data is copied from os_module_data_t so we don't have to hold the
     * module area lock while the client iterates.
     */
    ELF_SYM_TYPE *dynsym;               /* absolute addr of .dynsym */
    size_t symentry_size;               /* size of a .dynsym entry */
    const char *dynstr;                 /* absolute addr of .dynstr */
    size_t dynstr_size;                 /* size of .dynstr */

    ELF_SYM_TYPE *cur_sym;              /* pointer to next import in .dynsym */
    ELF_SYM_TYPE safe_cur_sym;          /* safe_read() copy of cur_sym */
    ELF_SYM_TYPE *import_end;           /* end of imports in .dynsym */
    bool error_occurred;                /* error during iteration */
} elf_import_iterator_t;
#endif /* CLIENT_INTERFACE */

/* In case want to build w/o gnu headers and use that to run recent gnu elf */
#ifndef DT_GNU_HASH
# define DT_GNU_HASH 0x6ffffef5
#endif

#ifndef STT_GNU_IFUNC
# define STT_GNU_IFUNC STT_LOOS
#endif

/* forward declarison */
static void
module_hashtab_init(os_module_data_t *os_data);

/* Question : how is the size of the initial map determined?  There seems to be no better
 * way than to walk the program headers and find the largest virtual offset.  You'd think
 * there would be a field in the header or something easier than that...
 *
 * Generally the section headers will be unavailable to us unless we go to disk 
 * (investigate, pursuant to the answer to the above question being large enough to
 * always include the section table, might they be visible briefly during the
 * first map before the program headers are processed and re-map/bss overwrites? Probably
 * would depend on the .bss being large enough, grr....), but at least the elf header and
 * program headers should be in memory.
 * 
 * So to determine individual sections probably have to go to disk, but could try to 
 * backtrack some of them out from program headers which need to point to plt relocs
 * etc.
 */

#endif /* !NOT_DYNAMORIO_CORE_PROPER */

/* Is there an ELF header for a shared object at address 'base'? 
 * If size == 0 then checks for header readability else assumes that size bytes from
 * base are readable (unmap races are then callers responsibility). */
static bool
is_elf_so_header_common(app_pc base, size_t size, bool memory)
{
    /* FIXME We could check more fields in the header just as the
     * dlopen() does. */
    static const unsigned char ei_expected[SELFMAG] = {
        [EI_MAG0] = ELFMAG0,
        [EI_MAG1] = ELFMAG1,
        [EI_MAG2] = ELFMAG2,
        [EI_MAG3] = ELFMAG3
    };
    ELF_HEADER_TYPE elf_header;

    if (base == NULL) {
        ASSERT(false && "is_elf_so_header(): NULL base");
        return false;
    }

    /* read the header */
    if (size >= sizeof(ELF_HEADER_TYPE)) {
        elf_header = *(ELF_HEADER_TYPE *)base;
    } else if (size == 0) {
        if (!safe_read(base, sizeof(ELF_HEADER_TYPE), &elf_header))
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
#ifdef CLIENT_INTERFACE
        /* i#157, we do more check to make sure we load the right modules,
         * i.e. 32/64-bit libraries. 
         */
        if (INTERNAL_OPTION(private_loader) &&
            ((elf_header.e_version != 1) || 
             (memory && elf_header.e_ehsize != sizeof(ELF_HEADER_TYPE)) ||
             (memory && elf_header.e_machine != IF_X64_ELSE(EM_X86_64, EM_386))))
            return false;
#endif
        /* FIXME - should we add any of these to the check? For real 
         * modules all of these should hold. */
        ASSERT_CURIOSITY(elf_header.e_version == 1);
        ASSERT_CURIOSITY(!memory || elf_header.e_ehsize == sizeof(ELF_HEADER_TYPE));
        ASSERT_CURIOSITY(elf_header.e_ident[EI_OSABI] == ELFOSABI_SYSV ||
                         elf_header.e_ident[EI_OSABI] == ELFOSABI_LINUX);
#ifdef X64
        ASSERT_CURIOSITY(!memory || elf_header.e_machine == EM_X86_64);
#else
        ASSERT_CURIOSITY(!memory || elf_header.e_machine == EM_386);
#endif
        return true;
    }
    return false;
}

bool
is_elf_so_header(app_pc base, size_t size)
{
    return is_elf_so_header_common(base, size, true);
}

void
os_modules_init(void)
{
    /* nothing */
}

void
os_modules_exit(void)
{
    /* nothing */
}

#ifndef NOT_DYNAMORIO_CORE_PROPER

/* Returns absolute address of the ELF dynamic array DT_ target */
static app_pc
elf_dt_abs_addr(ELF_DYNAMIC_ENTRY_TYPE *dyn, app_pc base, size_t size,
                size_t view_size, ptr_int_t load_delta, bool at_map)
{
    /* FIXME - if at_map this needs to be adjusted if not in the first segment
     * since we haven't re-mapped later ones yet. Since it's read only I've
     * never seen it not be in the first segment, but should fix or at least
     * check. PR 307610.
     */
    /* FIXME PR 307687 - on some machines (for already loaded modules) someone
     * (presumably the loader?) has relocated this address.  The Elf spec is
     * adamant that dynamic entry addresses shouldn't have relocations (for
     * consistency) so must be the loader doing it on its own (same .so on
     * different machines will be different here).
     * How can we reliably tell if it has been relocated or not?  We can
     * check against the module bounds, but if it is loaded at a small delta
     * (or the module's base_address is large) that's potentially ambiguous. No
     * other real option short of going to disk though so we'll stick to that
     * and default to already relocated (which seems to be the case for the
     * newer ld versions).
     */
    app_pc tgt = (app_pc) dyn->d_un.d_ptr;
    if (at_map || tgt < base || tgt > base + size) {
        /* not relocated, adjust by load_delta */
        tgt = (app_pc) dyn->d_un.d_ptr + load_delta;
    }
                            
    /* sanity check location */
    if (tgt < base || tgt > base + size) {
        ASSERT_CURIOSITY(false && "DT entry not in module");
        tgt = NULL;
    } else if (at_map && tgt > base + view_size) {
        ASSERT_CURIOSITY(false && "DT entry not in initial map");
        tgt = NULL;
    }
    return tgt;
}

#endif /* !NOT_DYNAMORIO_CORE_PROPER */

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

#ifndef NOT_DYNAMORIO_CORE_PROPER

/* Adds an entry for a segment to the out_data->segments array */
static void
module_add_segment_data(OUT os_module_data_t *out_data,
                        ELF_HEADER_TYPE *elf_hdr,
                        ptr_int_t load_delta,
                        ELF_PROGRAM_HEADER_TYPE *prog_hdr)
{
    uint seg, i;
    if (out_data->alignment == 0) {
        out_data->alignment = prog_hdr->p_align;
    } else {
        /* We expect all segments to have the same alignment */
        ASSERT_CURIOSITY(out_data->alignment == prog_hdr->p_align);
    }
    /* Add segments to the module vector (i#160/PR 562667).
     * For !HAVE_PROC_MAPS we should combine w/ the segment
     * walk done in dl_iterate_get_areas_cb().
     */
    if (out_data->num_segments == 0) {
        /* over-allocate to avoid 2 passes to count PT_LOAD */
        out_data->alloc_segments = elf_hdr->e_phnum;
        out_data->segments = (module_segment_t *)
            HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, module_segment_t,
                             out_data->alloc_segments, ACCT_OTHER, PROTECTED);
        out_data->contiguous = true;
    }
    /* Keep array sorted in addr order.  I'm assuming segments are disjoint! */
    for (i = 0; i < out_data->num_segments; i++) {
        if (out_data->segments[i].start > (app_pc)prog_hdr->p_vaddr + load_delta)
            break;
    }
    seg = i;
    /* Shift remaining entries */
    for (i = out_data->num_segments; i > seg; i++) {
        out_data->segments[i] = out_data->segments[i - 1];
    }
    out_data->num_segments++;
    ASSERT(out_data->num_segments <= out_data->alloc_segments);
    /* ELF requires p_vaddr to already be aligned to p_align */
    out_data->segments[seg].start = (app_pc)
        ALIGN_BACKWARD(prog_hdr->p_vaddr + load_delta, PAGE_SIZE);
    out_data->segments[seg].end = (app_pc)
        ALIGN_FORWARD(prog_hdr->p_vaddr + load_delta + prog_hdr->p_memsz, PAGE_SIZE);
    out_data->segments[seg].prot = module_segment_prot_to_osprot(prog_hdr);
    if (seg > 0) {
        ASSERT(out_data->segments[seg].start >= out_data->segments[seg - 1].end);
        if (out_data->segments[seg].start > out_data->segments[seg - 1].end)
            out_data->contiguous = false;
    }
    if (seg < out_data->num_segments - 1) {
        ASSERT(out_data->segments[seg + 1].start >= out_data->segments[seg].end);
        if (out_data->segments[seg + 1].start > out_data->segments[seg].end)
            out_data->contiguous = false;
    }
}

/* common code to fill os_module_data_t for loader and module_area_t */
static void
module_fill_os_data(ELF_PROGRAM_HEADER_TYPE *prog_hdr, /* PT_DYNAMIC entry */
                    app_pc mod_base,
                    app_pc mod_end, 
                    app_pc base,
                    size_t view_size,
                    bool at_map,
                    ptr_int_t load_delta, 
                    OUT char **soname,
                    OUT os_module_data_t *out_data)
{
    /* if at_map use file offset as segments haven't been remapped yet and
     * the dynamic section isn't usually in the first segment (FIXME in
     * theory it's possible to construct a file where the dynamic section
     * isn't mapped in as part of the initial map because large parts of the
     * initial portion of the file aren't part of the in memory image which
     * is fixed up with a PT_LOAD).
     * 
     * If not at_map use virtual address adjusted for possible loading not
     * at base. */
    ELF_DYNAMIC_ENTRY_TYPE *dyn = (ELF_DYNAMIC_ENTRY_TYPE *)
        (at_map ? base + prog_hdr->p_offset :
         (app_pc)prog_hdr->p_vaddr + load_delta);
    ASSERT(prog_hdr->p_type == PT_DYNAMIC);
    dcontext_t *dcontext = get_thread_private_dcontext();

    TRY_EXCEPT_ALLOW_NO_DCONTEXT(dcontext, {
        int soname_index = -1;
        char *dynstr = NULL;
        size_t sz = mod_end - mod_base;
        /* i#489, DT_SONAME is optional, init soname to NULL first */
        *soname = NULL;
        while (dyn->d_tag != DT_NULL) {
            if (dyn->d_tag == DT_SONAME) {
                soname_index = dyn->d_un.d_val;
                if (dynstr != NULL)
                    break;
            } else if (dyn->d_tag == DT_STRTAB) {
                dynstr = (char *)
                    elf_dt_abs_addr(dyn, base, sz, view_size, 
                                    load_delta, at_map);
                if (out_data != NULL)
                    out_data->dynstr = (app_pc) dynstr;
                if (soname_index != -1 && out_data == NULL)
                    break; /* done w/ DT entries */
            } else if (out_data != NULL) {
                if (dyn->d_tag == DT_SYMTAB) {
                    out_data->dynsym =
                        elf_dt_abs_addr(dyn, base, sz, view_size, load_delta,
                                        at_map);
                } else if (dyn->d_tag == DT_HASH &&
                           /* if has both .gnu.hash and .hash, prefer .gnu.hash */
                           !out_data->hash_is_gnu) {
                    out_data->hashtab =
                        elf_dt_abs_addr(dyn, base, sz, view_size, load_delta,
                                        at_map);
                    out_data->hash_is_gnu = false;
                } else if (dyn->d_tag == DT_GNU_HASH) {
                    out_data->hashtab =
                        elf_dt_abs_addr(dyn, base, sz, view_size, load_delta,
                                        at_map);
                    out_data->hash_is_gnu = true;
                } else if (dyn->d_tag == DT_STRSZ) {
                    out_data->dynstr_size = (size_t) dyn->d_un.d_val;
                } else if (dyn->d_tag == DT_SYMENT) {
                    out_data->symentry_size = (size_t) dyn->d_un.d_val;
                } else if (dyn->d_tag == DT_CHECKSUM) {
                    out_data->checksum = (size_t) dyn->d_un.d_val;
                } else if (dyn->d_tag == DT_GNU_PRELINKED) {
                    out_data->timestamp = (size_t) dyn->d_un.d_val;
                }
            }
            dyn++;
        }
        if (soname_index != -1 && dynstr != NULL) {
            *soname = dynstr + soname_index;

            /* sanity check soname location */
            if ((app_pc)*soname < base || (app_pc)*soname > base + sz) {
                ASSERT_CURIOSITY(false && "soname not in module");
                *soname = NULL;
            } else if (at_map && (app_pc)*soname > base + view_size) {
                ASSERT_CURIOSITY(false && "soname not in initial map");
                *soname = NULL;
            }

            /* test string readability while still in try/except
             * in case we screwed up somewhere or module is
             * malformed/only partially mapped */
            if (*soname != NULL && strlen(*soname) == -1) {
                ASSERT_NOT_REACHED();
            }
        }
        /* we put module_hashtab_init here since it should always be called
         * together with module_fill_os_data and it updates os_data.
         */
        module_hashtab_init(out_data);
    } , { /* EXCEPT */
        ASSERT_CURIOSITY(false && "crashed while walking dynamic header");
        *soname = NULL;
    });
}

/* Returned addresses out_base and out_end are relative to the actual
 * loaded module base, so the "base" param should be added to produce
 * absolute addresses.
 * If out_data != NULL, fills in the dynamic section fields and adds
 * entries to the module list vector: so the caller must be
 * os_module_area_init() if out_data != NULL!
 */
bool
module_walk_program_headers(app_pc base, size_t view_size, bool at_map,
                            OUT app_pc *out_base /* relative pc */,
                            OUT app_pc *out_end /* relative pc */,
                            OUT char **out_soname,
                            OUT os_module_data_t *out_data)
{
    app_pc mod_base = (app_pc) POINTER_MAX, mod_end = (app_pc)0;
    char *soname = NULL;
    bool found_load = false;
    ELF_HEADER_TYPE *elf_hdr = (ELF_HEADER_TYPE *) base;
    ptr_int_t load_delta; /* delta loaded at relative to base */
    ASSERT(is_elf_so_header(base, view_size));    

    /* On adjusting virtual address in the elf headers -
     * To compute the base address, one determines the memory address associated with
     * the lowest p_vaddr value for a PT_LOAD segment. One then obtains the base
     * address by truncating the memory address to the nearest multiple of the
     * maximum page size and subtracting the truncated lowest p_vaddr value.
     * All virtual addresses are assuming the module is loaded at its
     * base address. */
    ASSERT_CURIOSITY(elf_hdr->e_phoff != 0 &&
                     elf_hdr->e_phoff + elf_hdr->e_phnum * elf_hdr->e_phentsize <=
                     view_size);
    if (elf_hdr->e_phoff != 0 &&
        elf_hdr->e_phoff + elf_hdr->e_phnum * elf_hdr->e_phentsize <= view_size) {
        /* walk the program headers */
        uint i;
        ASSERT_CURIOSITY(elf_hdr->e_phentsize == sizeof(ELF_PROGRAM_HEADER_TYPE));
        /* we need mod_base and mod_end to be fully computed for use in reading
         * out_soname, so we do a full segment walk up front
         */
        mod_base = module_vaddr_from_prog_header(base + elf_hdr->e_phoff,
                                                 elf_hdr->e_phnum, &mod_end);
        load_delta = base - mod_base;
        /* now we do our own walk */
        for (i = 0; i < elf_hdr->e_phnum; i++) {
            ELF_PROGRAM_HEADER_TYPE *prog_hdr = (ELF_PROGRAM_HEADER_TYPE *)
                (base + elf_hdr->e_phoff + i * elf_hdr->e_phentsize);
            if (prog_hdr->p_type == PT_LOAD) {
                if (out_data != NULL) {
                    module_add_segment_data(out_data, elf_hdr, 
                                            load_delta, prog_hdr);
                }
                found_load = true;
            }
            if ((out_soname != NULL || out_data != NULL) &&
                prog_hdr->p_type == PT_DYNAMIC) {
                module_fill_os_data(prog_hdr, mod_base, mod_end,
                                    base, view_size, at_map, load_delta,
                                    &soname, out_data);
            }
        }
    }
    ASSERT_CURIOSITY(found_load && mod_base != (app_pc)POINTER_MAX &&
                     mod_end != (app_pc)0);
    ASSERT_CURIOSITY(mod_end > mod_base);
    if (out_base != NULL)
        *out_base = mod_base;
    if (out_end != NULL)
        *out_end = mod_end;
    if (out_soname != NULL)
        *out_soname = soname;
    return found_load;
}

uint
module_num_program_headers(app_pc base)
{
    ELF_HEADER_TYPE *elf_hdr = (ELF_HEADER_TYPE *) base;
    ASSERT(is_elf_so_header(base, 0));    
    return elf_hdr->e_phnum;
}

#endif /* !NOT_DYNAMORIO_CORE_PROPER */

/* Returns the minimum p_vaddr field, aligned to page boundaries, in
 * the loadable segments in the prog_header array, or POINTER_MAX if
 * there are no loadable segments.
 */
app_pc
module_vaddr_from_prog_header(app_pc prog_header, uint num_segments,
                              OUT app_pc *out_end)
{
    uint i;
    app_pc min_vaddr = (app_pc) POINTER_MAX;
    app_pc mod_end = (app_pc) PTR_UINT_0;
    for (i = 0; i < num_segments; i++) {
        /* Without the ELF header we use sizeof instead of elf_hdr->e_phentsize
         * which must be a reliable assumption as dl_iterate_phdr() doesn't
         * bother to deliver the entry size.
         */
        ELF_PROGRAM_HEADER_TYPE *prog_hdr = (ELF_PROGRAM_HEADER_TYPE *)
            (prog_header + i * sizeof(ELF_PROGRAM_HEADER_TYPE));
        if (prog_hdr->p_type == PT_LOAD) {
            /* ELF requires p_vaddr to already be aligned to p_align */
            min_vaddr =
                MIN(min_vaddr, (app_pc) ALIGN_BACKWARD(prog_hdr->p_vaddr, PAGE_SIZE));
            mod_end =
                MAX(mod_end, (app_pc)
                    ALIGN_FORWARD(prog_hdr->p_vaddr + prog_hdr->p_memsz, PAGE_SIZE));
        }
    }
    if (out_end != NULL)
        *out_end = mod_end;
    return min_vaddr;
}

#ifndef NOT_DYNAMORIO_CORE_PROPER

bool
module_read_program_header(app_pc base,
                           uint segment_num,
                           OUT app_pc *segment_base /* relative pc */,
                           OUT app_pc *segment_end /* relative pc */,
                           OUT uint *segment_prot,
                           OUT size_t *segment_align)
{
    ELF_HEADER_TYPE *elf_hdr = (ELF_HEADER_TYPE *) base;
    ELF_PROGRAM_HEADER_TYPE *prog_hdr;
    ASSERT(is_elf_so_header(base, 0));    
    if (elf_hdr->e_phoff != 0) {
        ASSERT_CURIOSITY(elf_hdr->e_phentsize == sizeof(ELF_PROGRAM_HEADER_TYPE));
        prog_hdr = (ELF_PROGRAM_HEADER_TYPE *)
            (base + elf_hdr->e_phoff + segment_num * elf_hdr->e_phentsize);
        if (prog_hdr->p_type == PT_LOAD) {
            /* ELF requires p_vaddr to already be aligned to p_align */
            if (segment_base != NULL)
                *segment_base = (app_pc) prog_hdr->p_vaddr;
            /* up to caller to align end if desired */
            if (segment_end != NULL) {
                *segment_end = (app_pc) (prog_hdr->p_vaddr + prog_hdr->p_memsz);
            }
            if (segment_prot != NULL)
                *segment_prot = module_segment_prot_to_osprot(prog_hdr);
            if (segment_align != NULL)
                *segment_align = prog_hdr->p_align;
            return true;
        }
    }
    return false;
}

/* fill os_module_data_t for hashtable lookup */
static void
module_hashtab_init(os_module_data_t *os_data)
{
    if (os_data->hashtab != NULL) {
        /* set up symbol lookup fields */
        if (os_data->hash_is_gnu) {
            /* .gnu.hash format.  can't find good docs for it. */
            Elf32_Word bitmask_nwords;
            Elf32_Word *htab = (Elf32_Word *) os_data->hashtab;
            os_data->num_buckets = (size_t) *htab++;
            os_data->gnu_symbias = *htab++;
            bitmask_nwords = *htab++;
            os_data->gnu_bitidx = (ptr_uint_t) (bitmask_nwords - 1);
            os_data->gnu_shift = (ptr_uint_t) *htab++;
            os_data->gnu_bitmask = (app_pc) htab;
            htab += ELF_WORD_SIZE / 32 * bitmask_nwords;
            os_data->buckets = (app_pc) htab;
            htab += os_data->num_buckets;
            os_data->chain = (app_pc) (htab - os_data->gnu_symbias);
        } else {
            /* sysv .hash format: nbuckets; nchain; buckets[]; chain[] */
            Elf_Symndx *htab = (Elf_Symndx *) os_data->hashtab;
            os_data->num_buckets = (size_t) *htab++;
            os_data->num_chain = (size_t) *htab++;
            os_data->buckets = (app_pc) htab;
            os_data->chain = (app_pc) (htab + os_data->num_buckets);
        }
        ASSERT(os_data->symentry_size == sizeof(ELF_SYM_TYPE));
    }
}

void
os_module_area_init(module_area_t *ma, app_pc base, size_t view_size,
                    bool at_map, const char *filepath, uint64 inode
                    HEAPACCT(which_heap_t which))
{
    app_pc mod_base, mod_end;
    ptr_int_t load_delta;
    char *soname = NULL;
    ELF_HEADER_TYPE *elf_hdr = (ELF_HEADER_TYPE *) base;
    ASSERT(is_elf_so_header(base, view_size));  

    module_walk_program_headers(base, view_size, at_map,
                                &mod_base, &mod_end, &soname, &ma->os_data);
    if (ma->os_data.contiguous)
        module_list_add_mapping(ma, base, base+view_size);
    else {
        /* Add the non-contiguous segments (i#160/PR 562667).  We could just add
         * them all separately but vmvectors are more efficient with fewer
         * entries so we merge.  We don't want general merging in our vector
         * either.
         */
        app_pc seg_base;
        uint i;
        ASSERT(ma->os_data.num_segments > 0 && ma->os_data.segments != NULL);
        seg_base = ma->os_data.segments[0].start;
        for (i = 1; i < ma->os_data.num_segments; i++) {
            if (ma->os_data.segments[i].start > ma->os_data.segments[i - 1].end) {
                module_list_add_mapping(ma, seg_base, ma->os_data.segments[i - 1].end);
                seg_base = ma->os_data.segments[i].start;
            }
        }
        module_list_add_mapping(ma, seg_base, ma->os_data.segments[i - 1].end);
        DOLOG(2, LOG_VMAREAS, {
            LOG(GLOBAL, LOG_INTERP|LOG_VMAREAS, 2, "segment list\n");
            for (i = 0; i < ma->os_data.num_segments; i++) {
                LOG(GLOBAL, LOG_INTERP|LOG_VMAREAS, 2,
                    "\tsegment %d: ["PFX","PFX") prot=%x\n", i,
                    ma->os_data.segments[i].start, ma->os_data.segments[i].end,
                    ma->os_data.segments[i].prot);
            }
        });
    }

    LOG(GLOBAL, LOG_SYMBOLS, 2,
        "%s: hashtab="PFX", dynsym="PFX", dynstr="PFX", strsz="SZFMT", symsz="SZFMT"\n",
        __func__, ma->os_data.hashtab, ma->os_data.dynsym, ma->os_data.dynstr,
        ma->os_data.dynstr_size, ma->os_data.symentry_size); 

    /* expect to map whole module */
    /* XREF 307599 on rounding module end to the next PAGE boundary */
    ASSERT_CURIOSITY(mod_end - mod_base == at_map ?
                     ALIGN_FORWARD(view_size, PAGE_SIZE) : view_size);

    ma->os_data.base_address = mod_base;
    load_delta = base - mod_base;

    ma->entry_point = (app_pc)elf_hdr->e_entry + load_delta;

    /* names - note os.c callers don't distinguish between no filename and an empty
     * filename, we treat both as NULL, but leave the distinction for SONAME. */
    if (filepath == NULL || filepath[0] == '\0') {
        ma->names.file_name = NULL;
#ifdef VMX86_SERVER
        /* XXX: provide a targeted query to avoid full walk */
        void *iter = vmk_mmaps_iter_start();
        if (iter != NULL) { /* backward compatibility: support lack of iter */
            byte *start;
            size_t length;
            char name[MAXIMUM_PATH];
            while (vmk_mmaps_iter_next(iter, &start, &length, NULL,
                                       name, BUFFER_SIZE_ELEMENTS(name))) {
                if (base == start) {
                    ma->names.file_name = dr_strdup(name HEAPACCT(which));
                    break;
                }
            }
            vmk_mmaps_iter_stop(iter);
        }
#endif
        ma->full_path = NULL;
    } else {
        ma->names.file_name = dr_strdup(get_short_name(filepath) HEAPACCT(which));
        /* We could share alloc w/ names.file_name but simpler to separate */
        ma->full_path = dr_strdup(filepath HEAPACCT(which));
    }
    ma->names.inode = inode;
    if (soname == NULL)
        ma->names.module_name = NULL;
    else
        ma->names.module_name = dr_strdup(soname HEAPACCT(which));

    /* Fields for pcaches (PR 295534).  These entries are not present in
     * all libs: I see DT_CHECKSUM and the prelink field on FC12 but not
     * on Ubuntu 9.04.
     */
    if (ma->os_data.checksum == 0 &&
        (DYNAMO_OPTION(coarse_enable_freeze) || DYNAMO_OPTION(use_persisted))) {
        /* Use something so we have usable pcache names */
        ma->os_data.checksum = crc32((const char *)ma->start, PAGE_SIZE);
    }
    /* Timestamp we just leave as 0 */
}

void
free_module_names(module_names_t *mod_names HEAPACCT(which_heap_t which))
{
    ASSERT(mod_names != NULL);

    if (mod_names->module_name != NULL)
        dr_strfree(mod_names->module_name HEAPACCT(which));
    if (mod_names->file_name != NULL)
        dr_strfree(mod_names->file_name HEAPACCT(which));
}

void
print_modules(file_t f, bool dump_xml)
{
    module_iterator_t *mi;

    /* we walk our own module list that is populated on an initial walk through memory,
     * and further kept consistent on memory mappings of likely modules */
    print_file(f, dump_xml ? "<loaded-modules>\n" : "\nLoaded modules:\n");

    mi = module_iterator_start();
    while (module_iterator_hasnext(mi)) {
        module_area_t *ma = module_iterator_next(mi);
        print_file(f, dump_xml ? 
                   "\t<so range=\""PFX"-"PFX"\" "
                   "entry=\""PFX"\" base_address="PFX"\n"
                   "\tname=\"%s\" />\n" :
                   "  "PFX"-"PFX" entry="PFX" base_address="PFX"\n"
                   "\tname=\"%s\" \n",
                   ma->start, ma->end - 1, /* inclusive */
                   ma->entry_point, ma->os_data.base_address,
                   GET_MODULE_NAME(&ma->names) == NULL ?
                       "(null)" : GET_MODULE_NAME(&ma->names));
    }
    module_iterator_stop(mi);

    if (dump_xml)
        print_file(f, "</loaded-modules>\n");
    else
        print_file(f, "\n");
}

void
os_module_area_reset(module_area_t *ma HEAPACCT(which_heap_t which))
{
    if (ma->os_data.contiguous)
        module_list_remove_mapping(ma, ma->start, ma->end);
    else {
        /* Remove the non-contiguous segments (i#160/PR 562667) */
        app_pc seg_base;
        uint i;
        ASSERT(ma->os_data.num_segments > 0 && ma->os_data.segments != NULL);
        seg_base = ma->os_data.segments[0].start;
        for (i = 1; i < ma->os_data.num_segments; i++) {
            if (ma->os_data.segments[i].start > ma->os_data.segments[i - 1].end) {
                module_list_remove_mapping(ma, seg_base, ma->os_data.segments[i - 1].end);
                seg_base = ma->os_data.segments[i].start;
            }
        }
        module_list_remove_mapping(ma, seg_base, ma->os_data.segments[i - 1].end);
    }
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, ma->os_data.segments, module_segment_t,
                    ma->os_data.alloc_segments, ACCT_OTHER, PROTECTED);
    if (ma->full_path != NULL)
        dr_strfree(ma->full_path HEAPACCT(which));
}

/* The hash func used in the ELF hash tables.
 * Even for ELF64 .hash entries are 32-bit.  See Elf_Symndx in elfclass.h.
 * Thus chain table and symbol table entries must be 32-bit; but string table
 * entries are 64-bit.
 */
static Elf_Symndx
elf_hash(const char *name)
{
    Elf_Symndx h = 0, g;
    while (*name != '\0') {
        h = (h << 4) + *name;
        g = h & 0xf0000000;
        if (g != 0)
            h ^= g >> 24;
        h &= ~g;
        name++;
    }
    return h;
}

static Elf_Symndx
elf_gnu_hash(const char *name)
{
    Elf_Symndx h = 5381;
    for (unsigned char c = *name; c != '\0'; c = *++name)
        h = h * 33 + c;
    return (h & 0xffffffff);
}

static bool
elf_sym_matches(ELF_SYM_TYPE *sym, char *strtab, const char *name,
                bool *is_indirect_code OUT)
{
    /* i#248/PR 510905: FC12 libc strlen has this type */
    bool is_ifunc = (ELF_ST_TYPE(sym->st_info) == STT_GNU_IFUNC);
    LOG(GLOBAL, LOG_SYMBOLS, 4, "%s: considering type=%d %s\n",
        __func__, ELF_ST_TYPE(sym->st_info), strtab + sym->st_name);
    /* Only consider "typical" types */
    if ((ELF_ST_TYPE(sym->st_info) <= STT_FUNC || is_ifunc) &&
        /* Paranoid so limiting to 4K */
        strncmp(strtab + sym->st_name, name, PAGE_SIZE) == 0) {
        if (is_indirect_code != NULL)
            *is_indirect_code = is_ifunc;
        return true;
    }
    return false;
}

/* The new GNU hash scheme to improve lookup speed.
 * Can't find good doc to reference here.
 */
static app_pc 
gnu_hash_lookup(const char   *name,
                ptr_int_t     load_delta,
                ELF_SYM_TYPE *symtab,
                char         *strtab,
                Elf_Symndx   *buckets,
                Elf_Symndx   *chain,
                ELF_ADDR     *bitmask, 
                ptr_uint_t    bitidx, 
                ptr_uint_t    shift,
                size_t        num_buckets,
                bool         *is_indirect_code)
{
    Elf_Symndx sidx;
    Elf_Symndx hidx;
    ELF_ADDR entry;
    uint h1, h2;
    app_pc res = NULL;

    ASSERT(bitmask != NULL);
    hidx = elf_gnu_hash(name);
    entry = bitmask[(hidx / ELF_WORD_SIZE) & bitidx];
    h1 = hidx & (ELF_WORD_SIZE - 1);
    h2 = (hidx >>shift) & (ELF_WORD_SIZE - 1);
    if (TEST(1, (entry >> h1) & (entry >> h2))) {
        Elf32_Word bucket = buckets[hidx % num_buckets];
        if (bucket != 0) {
            Elf32_Word *harray = &chain[bucket];
            do {
                if ((((*harray) ^ hidx) >> 1) == 0) {
                    sidx = harray - chain;
                    if (elf_sym_matches(&symtab[sidx], strtab, name, is_indirect_code)) {
                        res = (app_pc) (symtab[sidx].st_value + load_delta);
                        break;
                    }
                }
            } while (!TEST(1, *harray++));
        }
    }
    return res;
}

/* See the ELF specs: hashtable entry holds first symbol table index;
 * chain entries hold subsequent that have same hash.
 */
static app_pc
elf_hash_lookup(const char   *name,
                ptr_int_t     load_delta,
                ELF_SYM_TYPE *symtab,
                char         *strtab,
                Elf_Symndx   *buckets,
                Elf_Symndx   *chain,
                size_t        num_buckets,
                size_t        dynstr_size,
                bool         *is_indirect_code)
{
    Elf_Symndx    sidx;
    Elf_Symndx    hidx;
    ELF_SYM_TYPE *sym;
    app_pc        res;

    hidx = elf_hash(name);
    for (sidx = buckets[hidx % num_buckets];
         sidx != STN_UNDEF;
         sidx = chain[sidx]) {
        sym = &symtab[sidx];
        if (sym->st_name >= dynstr_size) {
            ASSERT(false && "malformed ELF symbol entry");
            continue;
        }
        if (sym->st_value == 0 && ELF_ST_TYPE(sym->st_info) != STT_TLS)
            continue; /* no value */
        if (elf_sym_matches(sym, strtab, name, is_indirect_code))
            break;
    }
    if (sidx != STN_UNDEF) 
        res = (app_pc) (sym->st_value + load_delta);
    else 
        res = NULL;
    return res;
}

/* get the address by using the hashtable information in os_module_data_t */
app_pc
get_proc_address_from_os_data(os_module_data_t *os_data,
                              ptr_int_t load_delta,
                              const char *name,
                              OUT bool *is_indirect_code)
{
    if (os_data->hashtab != NULL) {
        Elf_Symndx *buckets = (Elf_Symndx *) os_data->buckets;
        Elf_Symndx *chain = (Elf_Symndx *) os_data->chain;
        ELF_SYM_TYPE *symtab = (ELF_SYM_TYPE *) os_data->dynsym;
        char *strtab = (char *) os_data->dynstr;
        size_t num_buckets = os_data->num_buckets;
        if (os_data->hash_is_gnu) {
            /* The new GNU hash scheme */
            return gnu_hash_lookup(name, load_delta, symtab, strtab, 
                                   buckets, chain,
                                   (ELF_ADDR *)os_data->gnu_bitmask,
                                   (ptr_uint_t)os_data->gnu_bitidx,
                                   (ptr_uint_t)os_data->gnu_shift,
                                   num_buckets, is_indirect_code);
        } else {
            /* ELF hash scheme */
            return elf_hash_lookup(name, load_delta, symtab, strtab, 
                                   buckets, chain,
                                   num_buckets, os_data->dynstr_size,
                                   is_indirect_code);
        }
    }
    return NULL;
}

/* if we add any more values, switch to a globally-defined dr_export_info_t 
 * and use it here
 */
generic_func_t
get_proc_address_ex(module_base_t lib, const char *name, bool *is_indirect_code OUT)
{
    app_pc res = NULL;
    module_area_t *ma;
    bool is_ifunc;
    os_get_module_info_lock();
    ma = module_pc_lookup((app_pc)lib);
    if (ma != NULL) {
        res = get_proc_address_from_os_data(&ma->os_data,
                                            ma->start - 
                                            ma->os_data.base_address,
                                            name, &is_ifunc);
        /* XXX: for the case of is_indirect_code being true, should we call
         * the ifunc to get the real symbol location?
         * Current solution is:
         * If the caller asking about is_indirect_code, i.e. passing a not-NULL,
         * we assume it knows about the ifunc, and leave it to decide to call 
         * the ifunc or not.
         * If is_indirect_code is NULL, we will call the ifunc for caller.
         */
        if (is_indirect_code != NULL) {
            *is_indirect_code = is_ifunc;
        } else if (res != NULL) {
            if (is_ifunc) {
                TRY_EXCEPT_ALLOW_NO_DCONTEXT(get_thread_private_dcontext(), {
                    res = ((app_pc (*) (void)) (res)) ();
                }, { /* EXCEPT */
                    ASSERT_CURIOSITY(false && "crashed while executing ifunc");
                    res = NULL;
                });
            }
        }
    }
    os_get_module_info_unlock();
    LOG(GLOBAL, LOG_SYMBOLS, 2, "%s: %s => "PFX"\n", __func__, name, res);
    return convert_data_to_function(res);
}

generic_func_t
get_proc_address(module_base_t lib, const char *name)
{
    return get_proc_address_ex(lib, name, NULL);
}

/* Returns the bounds of the first section with matching name. */
bool
get_named_section_bounds(app_pc module_base, const char *name,
                         app_pc *start/*OUT*/, app_pc *end/*OUT*/)
{
    /* FIXME: not implemented */
    ASSERT(is_elf_so_header(module_base, 0));
    if (start != NULL)
        *start = NULL;
    if (end != NULL)
        *end = NULL;
    return false;
}

bool
rct_is_exported_function(app_pc tag)
{
    /* FIXME: not implemented */
    return false;
}


/* FIXME PR 295529: NYI, here so code origins policies aren't all ifdef WINDOWS */
const char *
get_module_short_name(app_pc pc HEAPACCT(which_heap_t which))
{
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

/* FIXME PR 295529: NYI, here so moduledb code isn't all ifdef WINDOWS */
bool
get_module_company_name(app_pc mod_base, char *out_buf, size_t out_buf_size)
{
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

app_pc
get_module_base(app_pc pc)
{
    app_pc base = NULL;
    module_area_t *ma;
    os_get_module_info_lock();
    ma = module_pc_lookup(pc);
    if (ma != NULL)
        base = ma->start;
    os_get_module_info_unlock();
    return base;
}

/* FIXME PR 212458: NYI, here so code origins policies aren't all ifdef WINDOWS */
bool
is_range_in_code_section(app_pc module_base, app_pc start_pc, app_pc end_pc,
                         app_pc *sec_start /* OUT */, app_pc *sec_end /* OUT */)
{
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

/* FIXME PR 212458: NYI, here so code origins policies aren't all ifdef WINDOWS */
bool
is_in_code_section(app_pc module_base, app_pc addr,
                   app_pc *sec_start /* OUT */, app_pc *sec_end /* OUT */)
{
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

/* FIXME PR 212458: NYI, here so code origins policies aren't all ifdef WINDOWS */
bool
is_in_dot_data_section(app_pc module_base, app_pc addr,
                       app_pc *sec_start /* OUT */, app_pc *sec_end /* OUT */)
{
    return false;
    ASSERT_NOT_IMPLEMENTED(false);
}

/* FIXME PR 212458: NYI, here so code origins policies aren't all ifdef WINDOWS */
bool
is_in_any_section(app_pc module_base, app_pc addr,
                  app_pc *sec_start /* OUT */, app_pc *sec_end /* OUT */)
{
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

bool
is_mapped_as_image(app_pc module_base)
{
    return is_elf_so_header(module_base, 0);
}


/* Gets module information of module containing pc, cached from our module list.
 * Returns false if not in module; none of the OUT arguments are set in that case.
 *
 * FIXME: share code w/ win32/module.c's os_get_module_info()
 *
 * Note: this function returns only one module name using the rule established
 * by GET_MODULE_NAME(); for getting all possible ones use
 * os_get_module_info_all_names() directly.  Part of fix for case 9842.
 *
 * If name != NULL, caller must acquire the module_data_lock beforehand
 * and call os_get_module_info_unlock() when finished with the name
 * (caller can use dr_strdup to make a copy first if necessary; validity of the
 * name is guaranteed only as long as the caller holds module_data_lock).
 * If name == NULL, this routine acquires and releases the lock and the
 * caller has no obligations.
 */
bool
os_get_module_info(const app_pc pc, uint *checksum, uint *timestamp,
                   size_t *size, const char **name, size_t *code_size,
                   uint64 *file_version)
{
    module_area_t *ma;
    if (!is_module_list_initialized())
        return false;

    /* read lock to protect custom data */
    if (name == NULL)
        os_get_module_info_lock();
    ASSERT(os_get_module_info_locked());;

    ma = module_pc_lookup(pc);
    if (ma != NULL) {
        if (checksum != NULL)
            *checksum = ma->os_data.checksum;
        if (timestamp != NULL)
            *timestamp = ma->os_data.timestamp;
        if (size != NULL)
            *size = ma->end - ma->start;
        if (name != NULL)
            *name = GET_MODULE_NAME(&ma->names);
        if (code_size != NULL) {
            /* Using rx segment size since don't want to impl section
             * iterator (i#76/PR 212458)
             */
            uint i;
            size_t rx_sz = 0;
            ASSERT(ma->os_data.num_segments > 0 && ma->os_data.segments != NULL);
            for (i = 0; i < ma->os_data.num_segments; i++) {
                if (ma->os_data.segments[i].prot == (MEMPROT_EXEC | MEMPROT_READ)) {
                    rx_sz = ma->os_data.segments[i].end -
                        ma->os_data.segments[i].start;
                    break;
                }
            }
            *code_size = rx_sz;
        }
        if (file_version != NULL) {
            /* FIXME: NYI: make windows-only everywhere if no good linux source */
            *file_version = 0;
        }
    }

    if (name == NULL)
        os_get_module_info_unlock();
    return (ma != NULL);
}

bool
os_get_module_info_all_names(const app_pc pc, uint *checksum, uint *timestamp,
                             size_t *size, module_names_t **names,
                             size_t *code_size, uint64 *file_version)
{
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

#if defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH)
extern rct_module_table_t rct_global_table;

/* Caller must hold module_data_lock */
rct_module_table_t *
os_module_get_rct_htable(app_pc pc, rct_type_t which)
{
    /* FIXME: until we have a module list we use global rct and rac tables */
    if (which == RCT_RCT)
        return &rct_global_table;
    return NULL; /* we use rac_non_module_table */
}
#endif

/* Returns true if the module has an nth segment, false otherwise. */
bool
module_get_nth_segment(app_pc module_base, uint n,
                       app_pc *start/*OPTIONAL OUT*/, app_pc *end/*OPTIONAL OUT*/,
                       uint *chars/*OPTIONAL OUT*/)
{
    module_area_t *ma;
    bool res = false;
    if (!is_module_list_initialized())
        return false;
    os_get_module_info_lock();
    ma = module_pc_lookup(module_base);
    if (ma != NULL && n < ma->os_data.num_segments) {
        LOG(GLOBAL, LOG_INTERP|LOG_VMAREAS, 3, "%s: ["PFX"-"PFX") %x\n",
            __FUNCTION__, ma->os_data.segments[n].start, ma->os_data.segments[n].end,
            ma->os_data.segments[n].prot);
        if (start != NULL)
            *start = ma->os_data.segments[n].start;
        if (end != NULL)
            *end = ma->os_data.segments[n].end;
        if (chars != NULL)
            *chars = ma->os_data.segments[n].prot;
        res = true;
    }
    os_get_module_info_unlock();
    return res;
}

#endif /* !NOT_DYNAMORIO_CORE_PROPER */

size_t
module_get_header_size(app_pc module_base)
{
    ELF_HEADER_TYPE *elf_header = (ELF_HEADER_TYPE *) module_base;
    if (!is_elf_so_header_common(module_base, 0, true))
        return 0;
    ASSERT(offsetof(Elf64_Ehdr, e_machine) == 
           offsetof(Elf32_Ehdr, e_machine));
    if (elf_header->e_machine == EM_X86_64)
        return sizeof(Elf64_Ehdr);
    else
        return sizeof(Elf32_Ehdr);
}

bool
get_elf_platform(file_t f, dr_platform_t *platform)
{
    elf_generic_header_t elf_header;
    if (os_read(f, &elf_header, sizeof(elf_header)) != sizeof(elf_header))
        return false;
    if (!is_elf_so_header_common((app_pc)&elf_header, sizeof(elf_header), false))
        return false;
    ASSERT(offsetof(Elf64_Ehdr, e_machine) ==
           offsetof(Elf32_Ehdr, e_machine));
    switch (elf_header.elf64.e_machine) {
    case EM_X86_64: *platform = DR_PLATFORM_64BIT; break;
    case EM_386:    *platform = DR_PLATFORM_32BIT; break;
    default:
        return false;
    }
    return true;
}

bool
file_is_elf64(file_t f)
{
    dr_platform_t platform;
    if (get_elf_platform(f, &platform))
        return platform == DR_PLATFORM_64BIT;
    /* on error, assume same arch as us */
    return IF_X64_ELSE(true, false);
}

#ifndef NOT_DYNAMORIO_CORE_PROPER

/* returns true if the module is marked as having text relocations.
 * XXX: should we also have a routine that walks the relocs (once that
 * code is in) and really checks whether there are any text
 * relocations?  then don't need -persist_trust_textrel option.
 */
bool
module_has_text_relocs(app_pc base, bool at_map)
{
    app_pc mod_base, mod_end;
    ptr_int_t load_delta; /* delta loaded at relative to base */
    uint i;
    ELF_HEADER_TYPE *elf_hdr = (ELF_HEADER_TYPE *)base;
    ELF_PROGRAM_HEADER_TYPE *prog_hdr;
    ELF_DYNAMIC_ENTRY_TYPE  *dyn = NULL;

    ASSERT(is_elf_so_header(base, 0));
    /* walk program headers to get mod_base */
    mod_base = module_vaddr_from_prog_header(base + elf_hdr->e_phoff,
                                             elf_hdr->e_phnum, &mod_end);
    load_delta = base - mod_base;
    /* walk program headers to get dynamic section pointer */
    prog_hdr = (ELF_PROGRAM_HEADER_TYPE *)(base + elf_hdr->e_phoff);
    for (i = 0; i < elf_hdr->e_phnum; i++) {
        if (prog_hdr->p_type == PT_DYNAMIC) {
            dyn = (ELF_DYNAMIC_ENTRY_TYPE *)
                ((at_map ? prog_hdr->p_offset : prog_hdr->p_vaddr) + load_delta);
            break;
        }
        prog_hdr++;
    }
    if (dyn == NULL)
        return false;
    ASSERT((app_pc)dyn > base && (app_pc)dyn < mod_end + load_delta);
    while (dyn->d_tag != DT_NULL) {
        /* Older binaries have a separate DT_TEXTREL entry */
        if (dyn->d_tag == DT_TEXTREL)
            return true;
        /* Newer binaries have a DF_TEXTREL flag in DT_FLAGS */
        if (dyn->d_tag == DT_FLAGS) {
            if (TEST(DF_TEXTREL, dyn->d_un.d_val))
                return true;
        }
        dyn++;
    }
    return false;
}

/* check if module has text relocations by checking os_privmod_data's 
 * textrel field.
 */
bool
module_has_text_relocs_ex(app_pc base, os_privmod_data_t *pd)
{
    ASSERT(pd != NULL);
    return pd->textrel;
}

/* This is a helper function that get section from the image with 
 * specific name.
 * Note that it must be the image file, not the loaded module.
 * It may return NULL if no such section exist.
 */
ELF_ADDR
module_get_section_with_name(app_pc image, size_t img_size, const char *sec_name)
{
    ELF_HEADER_TYPE *elf_hdr = (ELF_HEADER_TYPE *) image;
    ELF_SECTION_HEADER_TYPE *sec_hdr;
    char *strtab;
    uint i;
    /* XXX: How can I check if it is a mapped file in memory
     * not mapped semgents?
     */
    ASSERT(is_elf_so_header(image, img_size));
    ASSERT(elf_hdr->e_shoff < img_size);
    ASSERT(elf_hdr->e_shentsize == sizeof(ELF_SECTION_HEADER_TYPE));
    ASSERT(elf_hdr->e_shoff + elf_hdr->e_shentsize * elf_hdr->e_shnum
           <= img_size);
    sec_hdr = (ELF_SECTION_HEADER_TYPE *)(image + elf_hdr->e_shoff);
    ASSERT(sec_hdr[elf_hdr->e_shstrndx].sh_offset < img_size);
    strtab = (char *)(image + sec_hdr[elf_hdr->e_shstrndx].sh_offset);
    /* walk the section table to check if a section name is ".text" */
    for (i = 0; i < elf_hdr->e_shnum; i++) {
        if (strcmp(sec_name, strtab + sec_hdr->sh_name) == 0)
            return sec_hdr->sh_addr;
        ++sec_hdr;
    }
    return (ELF_ADDR)NULL;
}

/* fills os_data and initializes the hash table. */
bool
module_read_os_data(app_pc base, 
                    OUT ptr_int_t *load_delta,
                    OUT os_module_data_t *os_data,
                    OUT char **soname)
{
    app_pc v_base, v_end;
    ELF_HEADER_TYPE *elf_hdr = (ELF_HEADER_TYPE *) base;

    /* walk the program headers */
    uint i;
    ASSERT_CURIOSITY(elf_hdr->e_phentsize == sizeof(ELF_PROGRAM_HEADER_TYPE));
    v_base = module_vaddr_from_prog_header(base + elf_hdr->e_phoff,
                                           elf_hdr->e_phnum, &v_end);
    *load_delta = base - v_base;
    /* now we do our own walk */
    for (i = 0; i < elf_hdr->e_phnum; i++) {
        ELF_PROGRAM_HEADER_TYPE *prog_hdr = (ELF_PROGRAM_HEADER_TYPE *)
            (base + elf_hdr->e_phoff + i * elf_hdr->e_phentsize);
        if (prog_hdr->p_type == PT_DYNAMIC) {
            module_fill_os_data(prog_hdr, v_base, v_end,
                                base, 0, false, *load_delta,
                                soname, os_data);
            return true;
        }
    }
    return false;
}

char *
get_shared_lib_name(app_pc map)
{
    ptr_int_t load_delta;
    char *soname;
    os_module_data_t os_data;
    memset(&os_data, 0, sizeof(os_data));
    module_read_os_data(map, &load_delta, &os_data, &soname);
    return soname;
}

/* Get module information from the loaded module.
 * We assume the segments are mapped into memory, not mapped file.
 */
void
module_get_os_privmod_data(app_pc base, size_t size, bool relocated,
                           OUT os_privmod_data_t *pd)
{
    app_pc mod_base, mod_end;
    ELF_HEADER_TYPE *elf_hdr = (ELF_HEADER_TYPE *)base;
    uint i;
    ELF_PROGRAM_HEADER_TYPE *prog_hdr;
    ELF_DYNAMIC_ENTRY_TYPE  *dyn = NULL;
    ptr_int_t load_delta;

    /* sanity checks */
    ASSERT(is_elf_so_header(base, size));
    ASSERT(elf_hdr->e_phentsize == sizeof(ELF_PROGRAM_HEADER_TYPE));
    ASSERT(elf_hdr->e_phoff != 0 &&
           elf_hdr->e_phoff + elf_hdr->e_phnum * elf_hdr->e_phentsize <=
           size);

    /* walk program headers to get mod_base mod_end and delta */
    mod_base = module_vaddr_from_prog_header(base + elf_hdr->e_phoff,
                                             elf_hdr->e_phnum, &mod_end);
    /* delta from preferred address, used for calcuate real address */
    load_delta = base - mod_base;
    pd->load_delta = load_delta;
    /* walk program headers to get dynamic section pointer and TLS info */
    prog_hdr = (ELF_PROGRAM_HEADER_TYPE *)(base + elf_hdr->e_phoff);
    for (i = 0; i < elf_hdr->e_phnum; i++) {
        if (prog_hdr->p_type == PT_DYNAMIC) {
            dyn = (ELF_DYNAMIC_ENTRY_TYPE *)(prog_hdr->p_vaddr + load_delta);
            pd->dyn = dyn;
        } else if (prog_hdr->p_type == PT_TLS && prog_hdr->p_memsz > 0) {
            /* TLS (Thread Local Storage) relocation information */
            pd->tls_block_size = prog_hdr->p_memsz;
            pd->tls_align      = prog_hdr->p_align;
            pd->tls_image      = (app_pc)prog_hdr->p_vaddr + load_delta;
            pd->tls_image_size = prog_hdr->p_filesz;
            if (pd->tls_align == 0)
                pd->tls_first_byte = 0;
            else {
                /* the first tls variable's offset of the alignment. */
                pd->tls_first_byte = prog_hdr->p_vaddr & (pd->tls_align - 1);
            }
        }
        ++prog_hdr;
    }
    ASSERT(dyn != NULL);
    /* XXX: this is a big switch table. There are other ways to parse it
     * with better performance, but I feel switch table is clear to read,
     * and it should not be called often.
     */
    pd->textrel = false;
    /* We assume the segments are mapped into memory, so the actual address
     * is calculated by adding d_ptr and load_delta, unless the loader already
     * relocated the module.
     */
    if (relocated) {
        load_delta = 0;
    }
    while (dyn->d_tag != DT_NULL) {
        switch (dyn->d_tag) {
        case DT_PLTGOT:
            pd->pltgot = (ELF_ADDR)(dyn->d_un.d_ptr + load_delta);
            break;
        case DT_PLTRELSZ:
            pd->pltrelsz = (size_t)dyn->d_un.d_val;
            break;
        case DT_PLTREL:
            pd->pltrel = dyn->d_un.d_val;
            break;
        case DT_TEXTREL:
            pd->textrel = true;
            break;
        case DT_FLAGS:
            if (TEST(DF_TEXTREL, dyn->d_un.d_val))
                pd->textrel = true;
            break;
        case DT_JMPREL:
            pd->jmprel = (app_pc)(dyn->d_un.d_ptr + load_delta);
            break;
        case DT_REL:
            pd->rel = (ELF_REL_TYPE *)(dyn->d_un.d_ptr + load_delta);
            break;
        case DT_RELSZ:
            pd->relsz = (size_t)dyn->d_un.d_val;
            break;
        case DT_RELENT:
            pd->relent = (size_t)dyn->d_un.d_val;
            break;
        case DT_RELA:
            pd->rela = (ELF_RELA_TYPE *)(dyn->d_un.d_ptr + load_delta);
            break;
        case DT_RELASZ:
            pd->relasz = (size_t)dyn->d_un.d_val;
            break;
        case DT_RELAENT:
            pd->relaent = (size_t)dyn->d_un.d_val;
            break;
        case DT_VERNEED:
            pd->verneed = (app_pc)(dyn->d_un.d_ptr + load_delta);
            break;
        case DT_VERNEEDNUM:
            pd->verneednum = dyn->d_un.d_val;
            break;
        case DT_VERSYM:
            pd->versym = (ELF_HALF *)(dyn->d_un.d_ptr + load_delta);
            break;
        case DT_RELCOUNT:
            pd->relcount = dyn->d_un.d_val;
            break;
        case DT_INIT:
            pd->init = (fp_t)(dyn->d_un.d_ptr + load_delta);
            break;
        case DT_FINI:
            pd->fini = (fp_t)(dyn->d_un.d_ptr + load_delta);
            break;
        case DT_INIT_ARRAY:
            pd->init_array = (fp_t *)(dyn->d_un.d_ptr + load_delta);
            break;
        case DT_INIT_ARRAYSZ:
            pd->init_arraysz = dyn->d_un.d_val;
            break;
        case DT_FINI_ARRAY:
            pd->fini_array = (fp_t *)(dyn->d_un.d_ptr + load_delta);
            break;
        case DT_FINI_ARRAYSZ:
            pd->fini_arraysz = dyn->d_un.d_val;
            break;
        default:
            break;
        }
        ++dyn;
    }
}

/* Returns a pointer to the phdr of the given type.
 */
ELF_PROGRAM_HEADER_TYPE *
module_find_phdr(app_pc base, uint phdr_type)
{
    ELF_HEADER_TYPE *ehdr = (ELF_HEADER_TYPE *)base;
    uint i;
    for (i = 0; i < ehdr->e_phnum; i++) {
        ELF_PROGRAM_HEADER_TYPE *phdr = (ELF_PROGRAM_HEADER_TYPE *)
            (base + ehdr->e_phoff + i * ehdr->e_phentsize);
        if (phdr->p_type == phdr_type) {
            return phdr;
        }
    }
    return NULL;
}

bool
module_get_relro(app_pc base, OUT app_pc *relro_base, OUT size_t *relro_size)
{
    ELF_PROGRAM_HEADER_TYPE *phdr = module_find_phdr(base, PT_GNU_RELRO);
    app_pc mod_base;
    ptr_int_t load_delta;
    ELF_HEADER_TYPE *ehdr = (ELF_HEADER_TYPE *)base;

    if (phdr == NULL)
        return false;
    mod_base = module_vaddr_from_prog_header(base + ehdr->e_phoff,
                                             ehdr->e_phnum, NULL);
    load_delta = base - mod_base;
    *relro_base = (app_pc) phdr->p_vaddr + load_delta;
    *relro_size = phdr->p_memsz;
    return true;
}

static app_pc
module_lookup_symbol(ELF_SYM_TYPE *sym, os_privmod_data_t *pd)
{
    app_pc res;
    const char *name;
    privmod_t *mod;
    bool is_ifunc;
    dcontext_t *dcontext = get_thread_private_dcontext();

    /* no name, do not search */
    if (sym->st_name == 0 || pd == NULL)
        return NULL;

    name = (char *)pd->os_data.dynstr + sym->st_name;
    LOG(GLOBAL, LOG_LOADER, 3, "sym lookup for %s from %s\n", 
        name, pd->soname);
    /* check my current module */
    res = get_proc_address_from_os_data(&pd->os_data,
                                        pd->load_delta,
                                        name, &is_ifunc);
    if (res != NULL) {
        if (is_ifunc) {
            TRY_EXCEPT_ALLOW_NO_DCONTEXT(dcontext, {
                res = ((app_pc (*) (void)) (res)) ();
            }, { /* EXCEPT */
                ASSERT_CURIOSITY(false && "crashed while executing ifunc");
                res = NULL;
            });
        }
        return res;
    }

    /* If not find the symbol in current module, iterate over all modules
     * in the dependency order.
     * FIXME: i#461 We do not tell weak/global, but return on the first we see.
     */
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    mod = privload_first_module();
    while (mod != NULL) {
        pd = mod->os_privmod_data;
        ASSERT(pd != NULL && name != NULL);
        LOG(GLOBAL, LOG_LOADER, 3, "sym lookup for %s from %s\n", 
            name, pd->soname);
        res = get_proc_address_from_os_data(&pd->os_data,
                                            pd->load_delta,
                                            name, &is_ifunc);
        if (res != NULL) {
            if (is_ifunc) {
                TRY_EXCEPT_ALLOW_NO_DCONTEXT(dcontext, {
                    res = ((app_pc (*) (void)) (res)) ();
                }, { /* EXCEPT */
                    ASSERT_CURIOSITY(false && "crashed while executing ifunc");
                    res = NULL;
                });
            }
            return res;
        }
        mod = privload_next_module(mod);
    }
    return NULL;
}

static void
module_undef_symbols()
{
    FATAL_USAGE_ERROR(UNDEFINED_SYMBOL_REFERENCE, 0, "");
}

#ifdef CLIENT_INTERFACE
/* XXX: We could implement import iteration of PE files in Wine, so we provide
 * these stubs.
 */
dr_module_import_iterator_t *
dr_module_import_iterator_start(module_handle_t handle)
{
    CLIENT_ASSERT(false, "No imports on Linux, use "
                  "dr_symbol_import_iterator_t instead");
    return NULL;
}

bool
dr_module_import_iterator_hasnext(dr_module_import_iterator_t *iter)
{
    return false;
}

dr_module_import_t *
dr_module_import_iterator_next(dr_module_import_iterator_t *iter)
{
    return NULL;
}

void
dr_module_import_iterator_stop(dr_module_import_iterator_t *iter)
{
}

static void
dynsym_next(elf_import_iterator_t *iter)
{
    iter->cur_sym = (ELF_SYM_TYPE *) ((byte *) iter->cur_sym +
                                      iter->symentry_size);
}

static void
dynsym_next_import(elf_import_iterator_t *iter)
{
    /* Imports have zero st_value fields.  Anything else is something else, so
     * we skip it.  Modules using .gnu.hash symbol lookup tend to have imports
     * come first, but sysv hash tables don't have any such split.
     */
    do {
        dynsym_next(iter);
        if (iter->cur_sym >= iter->import_end)
            return;
        if (!SAFE_READ_VAL(iter->safe_cur_sym, iter->cur_sym)) {
            memset(&iter->safe_cur_sym, 0, sizeof(iter->safe_cur_sym));
            iter->error_occurred = true;
            return;
        }
    } while (iter->safe_cur_sym.st_value != 0);

    if (iter->safe_cur_sym.st_name >= iter->dynstr_size) {
        ASSERT_CURIOSITY(false && "st_name out of .dynstr bounds");
        iter->error_occurred = true;
        return;
    }
}

dr_symbol_import_iterator_t *
dr_symbol_import_iterator_start(module_handle_t handle,
                                dr_module_import_desc_t *from_module)
{
    module_area_t *ma;
    elf_import_iterator_t *iter;
    size_t max_imports;

    if (from_module != NULL) {
        CLIENT_ASSERT(false, "Cannot iterate imports from a given module on "
                      "Linux");
        return NULL;
    }

    iter = global_heap_alloc(sizeof(*iter) HEAPACCT(ACCT_CLIENT));
    if (iter == NULL)
        return NULL;
    memset(iter, 0, sizeof(*iter));

    os_get_module_info_lock();
    ma = module_pc_lookup((byte *) handle);
    if (ma != NULL) {
        iter->dynsym = (ELF_SYM_TYPE *) ma->os_data.dynsym;
        iter->symentry_size = ma->os_data.symentry_size;
        iter->dynstr = (const char *) ma->os_data.dynstr;
        iter->dynstr_size = ma->os_data.dynstr_size;
        iter->cur_sym = iter->dynsym;

        /* The length of .dynsym is not available in the mapped image, so we
         * have to be creative.  The two export hashtables point into dynsym,
         * though, so they have some info about the length.
         */
        if (ma->os_data.hash_is_gnu) {
            /* See https://blogs.oracle.com/ali/entry/gnu_hash_elf_sections
             * "With GNU hash, the dynamic symbol table is divided into two
             * parts. The first part receives the symbols that can be omitted
             * from the hash table."
             * gnu_symbias is the index of the first symbol in the hash table,
             * so all of the imports are before it.  If we ever want to iterate
             * all of .dynsym, we will have to look at the contents of the hash
             * table.
             */
            max_imports = ma->os_data.gnu_symbias;
        } else {
            /* See http://www.sco.com/developers/gabi/latest/ch5.dynamic.html#hash
             * "The number of symbol table entries should equal nchain"
             */
            max_imports = ma->os_data.num_chain;
        }
        iter->import_end = (ELF_SYM_TYPE *)((app_pc)iter->dynsym +
                                            (max_imports * iter->symentry_size));

        /* Set up invariant that cur_sym and safe_cur_sym point to the next
         * symbol to yield.  This skips the first entry, which is fake according
         * to the spec.
         */
        ASSERT_CURIOSITY(iter->cur_sym->st_name == 0);
        dynsym_next_import(iter);
    } else {
        global_heap_free(iter, sizeof(*iter) HEAPACCT(ACCT_CLIENT));
        iter = NULL;
    }
    os_get_module_info_unlock();

    return (dr_symbol_import_iterator_t *) iter;
}

bool
dr_symbol_import_iterator_hasnext(dr_symbol_import_iterator_t *dr_iter)
{
    elf_import_iterator_t *iter = (elf_import_iterator_t *) dr_iter;
    return (iter != NULL && !iter->error_occurred &&
            iter->cur_sym < iter->import_end);
}

dr_symbol_import_t *
dr_symbol_import_iterator_next(dr_symbol_import_iterator_t *dr_iter)
{
    elf_import_iterator_t *iter = (elf_import_iterator_t *) dr_iter;

    CLIENT_ASSERT(iter != NULL, "invalid parameter");
    iter->symbol_import.name = iter->dynstr + iter->safe_cur_sym.st_name;
    iter->symbol_import.modname = NULL;  /* no module for ELFs */
    iter->symbol_import.delay_load = false;

    dynsym_next_import(iter);
    return &iter->symbol_import;
}

void
dr_symbol_import_iterator_stop(dr_symbol_import_iterator_t *dr_iter)
{
    elf_import_iterator_t *iter = (elf_import_iterator_t *) dr_iter;
    if (iter == NULL)
        return;
    global_heap_free(iter, sizeof(*iter) HEAPACCT(ACCT_CLIENT));
}
#endif /* CLIENT_INTERFACE */

static void
module_relocate_symbol(ELF_REL_TYPE *rel,
                       os_privmod_data_t *pd,
                       bool is_rela)
{
    ELF_ADDR *r_addr;
    uint r_type, r_sym;
    ELF_SYM_TYPE *sym;
    app_pc res = NULL;
    reg_t addend;
    const char *name;
    bool resolved;

    /* XXX: we assume ELF_REL_TYPE and ELF_RELA_TYPE only differ at the end,
     * i.e. with or without r_addend.
     */
    if (is_rela)
        addend = ((ELF_RELA_TYPE *)rel)->r_addend;
    else
        addend = 0;

    /* XXX: should use safe_write or TRY_EXCEPT around whole thing: 
     * for now: ok to die on malicious lib. 
     * Windows loader has exception handler around whole thing and won't
     * crash. Linux loader does nothing so possible crash.
     */
    r_addr = (ELF_ADDR *)(rel->r_offset + pd->load_delta);
    r_type = (uint)ELF_R_TYPE(rel->r_info);
    /* handle the most common case, i.e. ELF_R_RELATIVE */
    if (r_type == ELF_R_RELATIVE) {
        if (is_rela)
            *r_addr = addend + pd->load_delta;
        else
            *r_addr += pd->load_delta;
        return;
    } else if (r_type == ELF_R_NONE)
        return;

    r_sym = (uint)ELF_R_SYM(rel->r_info);
    sym   = &((ELF_SYM_TYPE *)pd->os_data.dynsym)[r_sym];
    name  = (char *)pd->os_data.dynstr + sym->st_name;

#ifdef CLIENT_INTERFACE
    if (INTERNAL_OPTION(private_loader) && privload_redirect_sym(r_addr, name))
        return;
#endif
    
    resolved = true;
    /* handle syms that do not need symbol lookup */
    switch (r_type) {
    case ELF_R_TLS_DTPMOD:
        /* XXX: Is it possible it ask for a module id not itself? */
        *r_addr = pd->tls_modid;
        break;
    case ELF_R_TLS_TPOFF:
        /* The offset is negative, forward from the thread pointer. */
        if (sym != NULL) {
            *r_addr = sym->st_value + (is_rela ? addend : *r_addr) 
                - pd->tls_offset;
        }
        break;
    case ELF_R_TLS_DTPOFF:
        /* During relocation all TLS symbols are defined and used.
           Therefore the offset is already correct. 
        */
        if (sym != NULL)
            *r_addr = sym->st_value + addend;
        break;
    case ELF_R_TLS_DESC:
        /* FIXME: TLS descriptor, not implemented */
        ASSERT_NOT_IMPLEMENTED(false);
        break;
#ifndef X64
    case R_386_TLS_TPOFF32:
        /* offset is positive, backward from the thread pointer */
        if (sym != NULL)
            *r_addr += pd->tls_offset - sym->st_value;
        break;
#endif
    case ELF_R_IRELATIVE:
        res = (byte *)pd->load_delta + (is_rela ? addend : *r_addr);
        *r_addr =  ((ELF_ADDR (*) (void)) res) ();
        break;
    default:
        resolved = false;
    }
    if (resolved)
        return;

    res = module_lookup_symbol(sym, pd);
    LOG(GLOBAL, LOG_LOADER, 3, "symbol lookup for %s %p\n", name, res);
    if (res == NULL && ELF_ST_BIND(sym->st_info) != STB_WEAK) {
        /* Warn up front on undefined symbols.  Don't warn for weak symbols,
         * which should be resolved to NULL if they are not present.  Weak
         * symbols are used in situations where libc needs to interact with a
         * system that may not be present, such as pthreads or the profiler.
         * Examples:
         * libc.so.6: undefined symbol _dl_starting_up
         * libempty.so: undefined symbol __gmon_start__
         * libempty.so: undefined symbol _Jv_RegisterClasses
         * libgcc_s.so.1: undefined symbol pthread_cancel
         * libstdc++.so.6: undefined symbol pthread_cancel
         */
        SYSLOG(SYSLOG_WARNING, UNDEFINED_SYMBOL, 2, pd->soname, name);
        if (r_type == ELF_R_JUMP_SLOT)
            *r_addr = (reg_t)module_undef_symbols;
        return;
    }
    switch (r_type) {
    case ELF_R_GLOB_DAT:
    case ELF_R_JUMP_SLOT:
        *r_addr = (reg_t)res + addend;
        break;
    case ELF_R_DIRECT:
        *r_addr = (reg_t)res + (is_rela ? addend : *r_addr);
        break;
    case ELF_R_COPY:
        if (sym != NULL)
            memcpy(r_addr, res, sym->st_size);
        break;
    case ELF_R_PC32:
        res += addend - (reg_t)r_addr;
        *(uint *)r_addr = (uint)(reg_t)res;
        break;
#ifdef X64
    case R_X86_64_32:
        res += addend;
        *(uint *)r_addr = (uint)(reg_t)res;
        break;
#endif
    default:
        /* unhandled rel type */
        ASSERT_NOT_REACHED();
        break;
    }
}

void
module_relocate_rel(app_pc modbase,
                    os_privmod_data_t *pd,
                    ELF_REL_TYPE *start,
                    ELF_REL_TYPE *end)
{
    ELF_REL_TYPE *rel;

    for (rel = start; rel < end; rel++)
        module_relocate_symbol(rel, pd, false);
}

void
module_relocate_rela(app_pc modbase,
                     os_privmod_data_t *pd,
                     ELF_RELA_TYPE *start,
                     ELF_RELA_TYPE *end)
{
    ELF_RELA_TYPE *rela;

    for (rela = start; rela < end; rela++)
        module_relocate_symbol((ELF_REL_TYPE *)rela, pd, true);
}

#endif /* !NOT_DYNAMORIO_CORE_PROPER */

/* Get the module text section from the mapped image file, 
 * Note that it must be the image file, not the loaded module.
 */
ELF_ADDR
module_get_text_section(app_pc file_map, size_t file_size)
{
    ELF_HEADER_TYPE *elf_hdr = (ELF_HEADER_TYPE *) file_map;
    ELF_SECTION_HEADER_TYPE *sec_hdr;
    char *strtab;
    uint i;
    ASSERT(is_elf_so_header(file_map, file_size));
    ASSERT(elf_hdr->e_shoff < file_size);
    ASSERT(elf_hdr->e_shentsize == sizeof(ELF_SECTION_HEADER_TYPE));
    ASSERT(elf_hdr->e_shoff + elf_hdr->e_shentsize * elf_hdr->e_shnum
           <= file_size);
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

#ifndef NOT_DYNAMORIO_CORE_PROPER

/* This routine allocates memory from DR's global memory pool.  Unlike
 * dr_global_alloc(), however, we store the size of the allocation in
 * the first few bytes so redirect_free() can retrieve it.
 */
void *
redirect_malloc(size_t size)
{
    void *mem;
    ASSERT(sizeof(size_t) >= HEAP_ALIGNMENT);
    size += sizeof(size_t);
    mem = global_heap_alloc(size HEAPACCT(ACCT_LIBDUP));
    if (mem == NULL) {
        CLIENT_ASSERT(false, "malloc failed: out of memory");
        return NULL;
    }
    *((size_t *)mem) = size;
    return mem + sizeof(size_t);
}

/* This routine allocates memory from DR's global memory pool. Unlike
 * dr_global_alloc(), however, we store the size of the allocation in
 * the first few bytes so redirect_free() can retrieve it.
 */
void *
redirect_realloc(void *mem, size_t size)
{
    void *buf = NULL;
    if (size > 0) {
        buf = redirect_malloc(size);
        if (buf != NULL && mem != NULL) {
            size_t old_size = *((size_t *)(mem - sizeof(size_t)));
            size_t min_size = MIN(old_size, size);
            memcpy(buf, mem, min_size);
        }
    }
    redirect_free(mem);
    return buf;
}

/* This routine allocates memory from DR's global memory pool.
 * It uses redirect_malloc to get the memory and then set to all 0.
 */
void *
redirect_calloc(size_t nmemb, size_t size)
{
    void *buf = NULL;
    size = size * nmemb;
    
    buf = redirect_malloc(nmemb * size);
    if (buf != NULL)
        memset(buf, 0, size);
    return buf;
}

/* This routine frees memory allocated by redirect_malloc and expects the
 * allocation size to be available in the few bytes before 'mem'.
 */
void
redirect_free(void *mem)
{
    /* PR 200203: must_not_be_inlined() is assuming this routine calls
     * no other DR routines besides global_heap_free!
     */
    if (mem != NULL) {
        mem -= sizeof(size_t);
        global_heap_free(mem, *((size_t *)mem) HEAPACCT(ACCT_LIBDUP));
    }
}

#endif /* !NOT_DYNAMORIO_CORE_PROPER */

static bool
os_read_until(file_t fd, void *buf, size_t toread)
{
    ssize_t nread;
    while (toread > 0) {
        nread = os_read(fd, buf, toread);
        if (nread < 0)
            break;
        toread -= nread;
        buf = (app_pc)buf + nread;
    }
    return (toread == 0);
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
        elf->ehdr = (ELF_HEADER_TYPE *) elf->file_map;
    } else {
        if (!os_read_until(elf->fd, elf->buf, sizeof(elf->buf)))
            return NULL;
        if (!is_elf_so_header(elf->buf, sizeof(elf->buf)))
            return NULL;
        elf->ehdr = (ELF_HEADER_TYPE *) elf->buf;
    }
    return elf->ehdr;
}

app_pc
elf_loader_map_file(elf_loader_t *elf)
{
    uint64 size64;
    if (elf->file_map != NULL)
        return elf->file_map;
    if (elf->fd == INVALID_FILE)
        return NULL;
    if (!os_get_file_size_by_handle(elf->fd, &size64))
        return NULL;
    ASSERT_TRUNCATE(elf->file_size, size_t, size64);
    elf->file_size = (size_t)size64;  /* truncate */
    /* We use os_map_file instead of map_file since this mapping is temporary.
     * We don't need to add and remove it from dynamo_areas.
     */
    elf->file_map = os_map_file(elf->fd, &elf->file_size, 0, NULL, MEMPROT_READ,
                                true/*cow*/, false/*image*/, false/*fixed*/);
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
        elf->phdrs = (ELF_PROGRAM_HEADER_TYPE *) (elf->buf + elf->ehdr->e_phoff);
    } else {
        /* We have large or distant phdrs, so map the whole file.  We could
         * seek and read just the phdrs to avoid disturbing the address space,
         * but that would introduce a dependency on DR's heap.
         */
        if (elf_loader_map_file(elf) == NULL)
            return NULL;
        elf->phdrs = (ELF_PROGRAM_HEADER_TYPE *) (elf->file_map +
                                                  elf->ehdr->e_phoff);
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
                     unmap_fn_t unmap_func, prot_fn_t prot_func)
{
    app_pc lib_base, lib_end, last_end;
    ELF_HEADER_TYPE *elf_hdr = elf->ehdr;
    app_pc  map_base, map_end;
    reg_t   pg_offs;
    uint   seg_prot, i;
    ptr_int_t delta;

    ASSERT(elf->phdrs != NULL && "call elf_loader_read_phdrs() first");
    if (elf->phdrs == NULL)
        return NULL;

    map_base = module_vaddr_from_prog_header((app_pc)elf->phdrs,
                                             elf->ehdr->e_phnum, &map_end);
    elf->image_size = map_end - map_base;

    /* reserve the memory from os for library */
    lib_base = (*map_func)(-1, &elf->image_size, 0, map_base,
                           MEMPROT_WRITE | MEMPROT_READ /* prot */,
                           true  /* copy-on-write */,
                           true  /* image, make it reachable */,
                           fixed);
    ASSERT(lib_base != NULL);
    lib_end = lib_base + elf->image_size;
    elf->load_base = lib_base;

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
        ELF_PROGRAM_HEADER_TYPE *prog_hdr = (ELF_PROGRAM_HEADER_TYPE *)
            ((byte *)elf->phdrs + i * elf_hdr->e_phentsize);
        if (prog_hdr->p_type == PT_LOAD) {
            seg_base = (app_pc)ALIGN_BACKWARD(prog_hdr->p_vaddr, PAGE_SIZE)
                       + delta;
            seg_end  = (app_pc)ALIGN_FORWARD(prog_hdr->p_vaddr +
                                             prog_hdr->p_filesz,
                                             PAGE_SIZE)
                       + delta;
            seg_size = seg_end - seg_base;
            if (seg_base != last_end) {
                /* XXX: a hole, I reserve this space instead of unmap it */
                size_t hole_size = seg_base - last_end;
                (*prot_func)(last_end, hole_size, MEMPROT_NONE);
            }
            seg_prot = module_segment_prot_to_osprot(prog_hdr);
            pg_offs  = ALIGN_BACKWARD(prog_hdr->p_offset, PAGE_SIZE);
            /* FIXME:
             * This function can be called after dynamorio_heap_initialized,
             * and we will use map_file instead of os_map_file.
             * However, map_file does not allow mmap with overlapped memory,
             * so we have to unmap the old memory first.
             * This might be a problem, e.g.
             * one thread unmaps the memory and before mapping the actual file,
             * another thread requests memory via mmap takes the memory here,
             * a racy condition.
             */
            (*unmap_func)(seg_base, seg_size);
            map = (*map_func)(elf->fd, &seg_size, pg_offs,
                              seg_base /* base */,
                              seg_prot | MEMPROT_WRITE /* prot */,
                              true /* writes should not change file */,
                              true /* image */,
                              true /* fixed */);
            ASSERT(map != NULL);
            /* fill zeros at extend size */
            file_end = (app_pc)prog_hdr->p_vaddr + prog_hdr->p_filesz;
            if (seg_end > file_end + delta) {
#ifndef NOT_DYNAMORIO_CORE_PROPER
                memset(file_end + delta, 0, seg_end - (file_end + delta));
#else
                /* FIXME i#37: use a remote memset to zero out this gap or fix
                 * it up in the child.  There is typically one RW PT_LOAD
                 * segment for .data and .bss.  If .data ends and .bss starts
                 * before filesz bytes, we need to zero the .bss bytes manually.
                 */
#endif /* !NOT_DYNAMORIO_CORE_PROPER */
            }
            seg_end  = (app_pc)ALIGN_FORWARD(prog_hdr->p_vaddr +
                                             prog_hdr->p_memsz,
                                             PAGE_SIZE) + delta;
            seg_size = seg_end - seg_base;
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
            return (const char *) (phdrs[i].p_vaddr + elf->load_delta);
        }
    }

    return NULL;
}
