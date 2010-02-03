/* **********************************************************
 * Copyright (c) 2008-2009 VMware, Inc.  All rights reserved.
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

#include "../globals.h"
#include "../module_shared.h"
#include "os_private.h"
#include <elf.h>    /* for ELF types */
#include <string.h>
#include <stddef.h> /* offsetof */
#include <link.h>   /* Elf_Symndx */

typedef union _elf_generic_header_t {
    Elf64_Ehdr elf64;
    Elf32_Ehdr elf32;
} elf_generic_header_t;

/* In case want to build w/o gnu headers and use that to run recent gnu elf */
#ifndef DT_GNU_HASH
# define DT_GNU_HASH 0x6ffffef5
#endif

#ifndef STT_GNU_IFUNC
# define STT_GNU_IFUNC STT_LOOS
#endif

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

/* Returns absolute address of the ELF dynamic array DT_ target */
static app_pc
elf_dt_abs_addr(ELF_DYNAMIC_ENTRY_TYPE *dyn, app_pc base, size_t size,
                size_t view_size, ssize_t offset, bool at_map)
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
     * check against the module bounds, but if it is loaded at a small offset
     * (or the module's base_address is large) that's potentially ambiguous. No
     * other real option short of going to disk though so we'll stick to that
     * and default to already relocated (which seems to be the case for the
     * newer ld versions).
     */
    app_pc tgt = (app_pc) dyn->d_un.d_ptr;
    if (at_map || tgt < base || tgt > base + size) {
        /* not relocated, adjust by offset */
        tgt = (app_pc) dyn->d_un.d_ptr + offset;
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

/* Returned addresses out_base and out_end are relative to the actual
 * loaded module base, so the "base" param should be added to produce
 * absolute addresses.
 * If out_data != NULL, fills in the dynamic section fields.
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
        /* now we do our own walk */
        for (i = 0; i < elf_hdr->e_phnum; i++) {
            ELF_PROGRAM_HEADER_TYPE *prog_hdr = (ELF_PROGRAM_HEADER_TYPE *)
                (base + elf_hdr->e_phoff + i * elf_hdr->e_phentsize);
            if (prog_hdr->p_type == PT_LOAD) {
                if (out_data != NULL) {
                    if (!found_load) {
                        out_data->alignment = prog_hdr->p_align;
                    } else {
                        /* We expect all segments to have the same alignment */
                        ASSERT_CURIOSITY(out_data->alignment == prog_hdr->p_align);
                    }
                }
                found_load = true;
            }
            if ((out_soname != NULL || out_data != NULL) &&
                prog_hdr->p_type == PT_DYNAMIC) {
                /* if at_map use file offset as segments haven't been remapped yet and
                 * the dynamic section isn't usually in the first segment (FIXME in
                 * theory it's possible to construct a file where the dynamic section
                 * isn't mapped in as part of the initial map because large parts of the
                 * initial portion of the file aren't part of the in memory image which
                 * is fixed up with a PT_LOAD).
                 * 
                 * If not at_map use virtual address adjusted for possible loading not
                 * at base. */
                /* FIXME: somehow dl_iterate_phdr() implies that this offset can
                 * be unsigned, but I don't trust that: probably only works b/c
                 * of wraparound arithmetic
                 */
                ssize_t offset = base - mod_base; /* offset loaded at relative to base */
                ELF_DYNAMIC_ENTRY_TYPE *dyn = (ELF_DYNAMIC_ENTRY_TYPE *)
                    (at_map ? base + prog_hdr->p_offset :
                     (app_pc)prog_hdr->p_vaddr + offset);
                dcontext_t *dcontext = get_thread_private_dcontext();

                TRY_EXCEPT(dcontext, {
                    int soname_index = -1;
                    char *dynstr = NULL;
                    size_t sz = mod_end - mod_base;
                    while (dyn->d_tag != DT_NULL) {
                        if (dyn->d_tag == DT_SONAME) {
                            soname_index = dyn->d_un.d_val;
                            if (dynstr != NULL)
                                break;
                        } else if (dyn->d_tag == DT_STRTAB) {
                            dynstr = (char *)
                                elf_dt_abs_addr(dyn, base, sz, view_size, offset, at_map);
                            if (out_data != NULL)
                                out_data->dynstr = (app_pc) dynstr;
                            if (soname_index != -1 && out_data == NULL)
                                break; /* done w/ DT entries */
                        } else if (out_data != NULL) {
                            if (dyn->d_tag == DT_SYMTAB) {
                                out_data->dynsym =
                                    elf_dt_abs_addr(dyn, base, sz, view_size, offset,
                                                    at_map);
                            } else if (dyn->d_tag == DT_HASH &&
                                       /* if has both .gnu.hash and .hash, prefer
                                        * .gnu.hash
                                        */
                                       !out_data->hash_is_gnu) {
                                out_data->hashtab =
                                    elf_dt_abs_addr(dyn, base, sz, view_size, offset,
                                                    at_map);
                                out_data->hash_is_gnu = false;
                            } else if (dyn->d_tag == DT_GNU_HASH) {
                                out_data->hashtab =
                                    elf_dt_abs_addr(dyn, base, sz, view_size, offset,
                                                    at_map);
                                out_data->hash_is_gnu = true;
                            } else if (dyn->d_tag == DT_STRSZ) {
                                out_data->dynstr_size = (size_t) dyn->d_un.d_val;
                            } else if (dyn->d_tag == DT_SYMENT) {
                                out_data->symentry_size = (size_t) dyn->d_un.d_val;
                            }
                        }
                        dyn++;
                    }
                    if (soname_index != -1 && dynstr != NULL) {
                        soname = dynstr + soname_index;

                        /* sanity check soname location */
                        if ((app_pc)soname < base || (app_pc)soname > base + sz) {
                            ASSERT_CURIOSITY(false && "soname not in module");
                            soname = NULL;
                        } else if (at_map && (app_pc)soname > base + view_size) {
                            ASSERT_CURIOSITY(false && "soname not in inital map");
                            soname = NULL;
                        }

                        /* test string readability while still in try/except
                         * in case we screwed up somewhere or module is
                         * malformed/only partially mapped */
                        if (soname != NULL && strlen(soname) == -1) {
                            ASSERT_NOT_REACHED();
                        }
                    }
                } , { /* EXCEPT */
                    ASSERT_CURIOSITY(false && "crashed while walking dynamic header");
                    soname = NULL;
                });
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
            if (segment_prot != NULL) {
                *segment_prot = 0;
                if (TEST(PF_X, prog_hdr->p_flags))
                    *segment_prot |= MEMPROT_EXEC;
                if (TEST(PF_W, prog_hdr->p_flags))
                    *segment_prot |= MEMPROT_WRITE;
                if (TEST(PF_R, prog_hdr->p_flags))
                    *segment_prot |= MEMPROT_READ;
            }
            if (segment_align != NULL)
                *segment_align = prog_hdr->p_align;
            return true;
        }
    }
    return false;
}

void
os_module_area_init(module_area_t *ma, app_pc base, size_t view_size,
                    bool at_map, const char *filepath, uint64 inode
                    HEAPACCT(which_heap_t which))
{
    app_pc mod_base, mod_end;
    int offset;
    char *soname = NULL;
    ELF_HEADER_TYPE *elf_hdr = (ELF_HEADER_TYPE *) base;
    ASSERT(is_elf_so_header(base, view_size));  

    module_walk_program_headers(base, view_size, at_map,
                                &mod_base, &mod_end, &soname, &ma->os_data);
    LOG(GLOBAL, LOG_SYMBOLS, 2,
        "%s: hashtab="PFX", dynsym="PFX", dynstr="PFX", strsz="SZFMT", symsz="SZFMT"\n",
        __func__, ma->os_data.hashtab, ma->os_data.dynsym, ma->os_data.dynstr,
        ma->os_data.dynstr_size, ma->os_data.symentry_size); 
    if (ma->os_data.hashtab != NULL) {
        /* set up symbol lookup fields */
        if (ma->os_data.hash_is_gnu) {
            /* .gnu.hash format.  can't find good docs for it. */
            Elf32_Word symbias;
            Elf32_Word bitmask_nwords;
            Elf32_Word *htab = (Elf32_Word *) ma->os_data.hashtab;
            ma->os_data.num_buckets = (size_t) *htab++;
            symbias = *htab++;
            bitmask_nwords = *htab++;
            ma->os_data.gnu_bitidx = (ptr_uint_t) (bitmask_nwords - 1);
            ma->os_data.gnu_shift = (ptr_uint_t) *htab++;
            ma->os_data.gnu_bitmask = (app_pc) htab;
            htab += ELF_WORD_SIZE / 32 * bitmask_nwords;
            ma->os_data.buckets = (app_pc) htab;
            htab += ma->os_data.num_buckets;
            ma->os_data.chain = (app_pc) (htab - symbias);
        } else {
            /* sysv .hash format: nbuckets; nchain; buckets[]; chain[] */
            Elf_Symndx *htab = (Elf_Symndx *) ma->os_data.hashtab;
            ma->os_data.num_buckets = (size_t) *htab++;
            ma->os_data.num_chain = (size_t) *htab++;
            ma->os_data.buckets = (app_pc) htab;
            ma->os_data.chain = (app_pc) (htab + ma->os_data.num_buckets);
        }
        ASSERT(ma->os_data.symentry_size == sizeof(ELF_SYM_TYPE));
    }

    /* expect to map whole module */
    /* XREF 307599 on rounding module end to the next PAGE boundary */
    ASSERT_CURIOSITY(mod_end - mod_base == at_map ?
                     ALIGN_FORWARD(view_size, PAGE_SIZE) : view_size);

    ma->os_data.base_address = mod_base;
    offset = base - mod_base;

    ma->entry_point = (app_pc)elf_hdr->e_entry + offset;

    /* names - note os.c callers don't distinguish between no filename and an empty
     * filename, we treat both as NULL, but leave the distinction for SONAME. */
    if (filepath == NULL || filepath[0] == '\0') {
        ma->names.file_name = NULL;
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

/* if we add any more values, switch to a globally-defined dr_export_info_t 
 * and use it here
 */
generic_func_t
get_proc_address_ex(module_handle_t lib, const char *name, bool *is_indirect_code OUT)
{
    app_pc res = NULL;
    module_area_t *ma;
    os_get_module_info_lock();
    ma = module_pc_lookup((app_pc)lib);
    if (ma != NULL && ma->os_data.hashtab != NULL) {
        Elf_Symndx hidx;
        Elf_Symndx *buckets = (Elf_Symndx *) ma->os_data.buckets;
        Elf_Symndx *chain = (Elf_Symndx *) ma->os_data.chain;
        ELF_SYM_TYPE *symtab = (ELF_SYM_TYPE *) ma->os_data.dynsym;
        char *strtab = (char *) ma->os_data.dynstr;
        Elf_Symndx sidx;
        ELF_SYM_TYPE sym;
        if (ma->os_data.hash_is_gnu) {
            /* The new GNU hash scheme to improve lookup speed.
             * Can't find good doc to reference here.
             */
            ELF_ADDR *bitmask = (ELF_ADDR *) ma->os_data.gnu_bitmask;
            ELF_ADDR entry;
            uint h1, h2;
            ASSERT(bitmask != NULL);
            hidx = elf_gnu_hash(name);
            entry = bitmask[(hidx / ELF_WORD_SIZE) & ma->os_data.gnu_bitidx];
            h1 = hidx & (ELF_WORD_SIZE - 1);
            h2 = (hidx >> ma->os_data.gnu_shift) & (ELF_WORD_SIZE - 1);
            if (TEST(1, (entry >> h1) & (entry >> h2))) {
                Elf32_Word bucket = buckets[hidx % ma->os_data.num_buckets];
                if (bucket != 0) {
                    Elf32_Word *harray = &chain[bucket];
                    do {
                        if ((((*harray) ^ hidx) >> 1) == 0) {
                            sidx = harray - chain;
                            if (elf_sym_matches(&symtab[sidx], strtab, name,
                                                is_indirect_code)) {
                                res = (app_pc) (symtab[sidx].st_value +
                                                (ma->start - ma->os_data.base_address));
                                break;
                            }
                        }
                    } while (!TEST(1, *harray++));
                }
            }
        } else {
            /* See the ELF specs: hashtable entry holds first symbol table index;
             * chain entries hold subsequent that have same hash.
             */
            hidx = elf_hash(name);
            for (sidx = buckets[hidx % ma->os_data.num_buckets];
                 sidx != STN_UNDEF;
                 sidx = chain[sidx]) {
                sym = symtab[sidx];
                if (sym.st_name >= ma->os_data.dynstr_size) {
                    ASSERT(false && "malformed ELF symbol entry");
                    continue;
                }
                if (sym.st_value == 0 && ELF_ST_TYPE(sym.st_info) != STT_TLS)
                    continue; /* no value */
                if (elf_sym_matches(&sym, strtab, name, is_indirect_code))
                    break;
            }
            if (sidx != STN_UNDEF) {
                res = (app_pc) (sym.st_value + (ma->start - ma->os_data.base_address));
            }
        }
    }
    os_get_module_info_unlock();
    LOG(GLOBAL, LOG_SYMBOLS, 2, "%s: %s => "PFX"\n", __func__, name, res);
    return convert_data_to_function(res);
}

generic_func_t
get_proc_address(module_handle_t lib, const char *name)
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

/* FIXME PR 295529: NYI, here so code origins policies aren't all ifdef WINDOWS */
app_pc
get_module_base(app_pc pc)
{
    return NULL;
    ASSERT_NOT_IMPLEMENTED(false);
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

/* FIXME PR 295529: NYI, here so code origins policies aren't all ifdef WINDOWS */
bool
is_mapped_as_image(app_pc module_base)
{
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}


/* FIXME PR 295529: NYI, present so that all hot patching code aren't all ifdef WINDOWS */
bool
os_get_module_info(const app_pc pc, uint *checksum, uint *timestamp,
                   size_t *size, const char **pe_name, size_t *code_size,
                   uint64 *file_version)
{
    return false;
    ASSERT_NOT_IMPLEMENTED(false); /* yes I know it's dead: leave it here (case 10703) */
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

void
module_calculate_digest(/*OUT*/ module_digest_t *digest,
                        app_pc module_base, 
                        size_t module_size,
                        bool full_digest,
                        bool short_digest,
                        uint short_digest_size,
                        uint sec_characteristics)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME PR 295534: NYI */
}

bool
file_is_elf64(file_t f)
{
    /* on error, assume same arch as us */
    bool res = IF_X64_ELSE(true, false);
    elf_generic_header_t elf_header;
    if (os_read(f, &elf_header, sizeof(elf_header)) != sizeof(elf_header))
        return res;
    if (!is_elf_so_header_common((app_pc)&elf_header, sizeof(elf_header), false))
        return res;
    ASSERT(offsetof(Elf64_Ehdr, e_machine) == 
           offsetof(Elf32_Ehdr, e_machine));
    return (elf_header.elf64.e_machine == EM_X86_64);
}
