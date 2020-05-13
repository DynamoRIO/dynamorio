/* **********************************************************
 * Copyright (c) 2011-2020 Google, Inc.  All rights reserved.
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

/* DRSyms DynamoRIO Extension */

/* Symbol lookup routines for ELF */

#include "dr_api.h"
#include "drsyms.h"
#include "drsyms_private.h"
#include "drsyms_obj.h"

#include "libelf.h"
#include "dwarf.h"
#include "libdwarf.h"

#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#ifndef MIN
#    define MIN(x, y) ((x) <= (y) ? (x) : (y))
#endif

#ifndef SIZE_T_MAX
#    ifdef X64
#        define SIZE_T_MAX ULLONG_MAX
#    else
#        define SIZE_T_MAX UINT_MAX
#    endif
#endif

static bool verbose = 0;

#undef NOTIFY
#ifdef DEBUG
#    define NOTIFY(n, ...)                       \
        do {                                     \
            if (verbose >= (n)) {                \
                dr_fprintf(STDERR, __VA_ARGS__); \
            }                                    \
        } while (0)
#else
#    define NOTIFY(n, ...) /* nothing */
#endif

#define NOTIFY_ELF(msg)                                           \
    do {                                                          \
        if (verbose) {                                            \
            dr_fprintf(STDERR, "drsyms %s: Elf error: %s\n", msg, \
                       elf_errmsg(elf_errno()));                  \
        }                                                         \
    } while (0)

#define NOTIFY_DWARF(de)                                                       \
    do {                                                                       \
        if (verbose) {                                                         \
            dr_fprintf(STDERR, "drsyms: Dwarf error: %s\n", dwarf_errmsg(de)); \
        }                                                                      \
    } while (0)

/******************************************************************************
 * ELF helpers.
 */

/* XXX i#1532: If we ever need to worry about ELF32 objects in an x64 process, we can
 * use gelf or some other library to translate elf32/64 structs into a common
 * representation.
 */
#ifdef X64
#    define elf_getehdr elf64_getehdr
#    define elf_getphdr elf64_getphdr
#    define elf_getshdr elf64_getshdr
#    define Elf_Ehdr Elf64_Ehdr
#    define Elf_Phdr Elf64_Phdr
#    define Elf_Shdr Elf64_Shdr
#    define Elf_Sym Elf64_Sym
#    define ELF_ST_TYPE ELF64_ST_TYPE
#else
#    define elf_getehdr elf32_getehdr
#    define elf_getphdr elf32_getphdr
#    define elf_getshdr elf32_getshdr
#    define Elf_Ehdr Elf32_Ehdr
#    define Elf_Phdr Elf32_Phdr
#    define Elf_Shdr Elf32_Shdr
#    define Elf_Sym Elf32_Sym
#    define ELF_ST_TYPE ELF32_ST_TYPE
#endif

typedef struct _elf_info_t {
    Elf *elf;
    Elf_Sym *syms;
    int strtab_idx;
    int num_syms;
    byte *map_base;
    ptr_uint_t load_base;
    drsym_debug_kind_t debug_kind;
#define MAX_BUILD_ID_LENGTH 128
    char build_id[MAX_BUILD_ID_LENGTH];
} elf_info_t;

/* Looks for a section with real data, not just a section with a header */
static Elf_Scn *
find_elf_section_by_name(Elf *elf, const char *match_name)
{
    Elf_Scn *scn;
    size_t shstrndx; /* Means "section header string table section index" */

    if (elf_getshdrstrndx(elf, &shstrndx) != 0) {
        NOTIFY_ELF("elf_getshdrstrndx");
        return NULL;
    }

    for (scn = elf_getscn(elf, 0); scn != NULL; scn = elf_nextscn(elf, scn)) {
        Elf_Shdr *section_header = elf_getshdr(scn);
        const char *sec_name;
        if (section_header == NULL) {
            NOTIFY_ELF("elf_getshdr");
            continue;
        }
        sec_name = elf_strptr(elf, shstrndx, section_header->sh_name);
        if (sec_name == NULL) {
            NOTIFY_ELF("elf_strptr");
        }
        if (strcmp(sec_name, match_name) == 0) {
            /* For our purposes, we want to treat a no-data section
             * type as if it didn't exist.  This happens sometimes in
             * debuglink files where some sections like .symtab are
             * present b/c the headers mirror the original ELF file, but
             * there's no data there.  Xref i#642.
             */
            if (section_header->sh_type == SHT_NOBITS)
                return NULL;
            return scn;
        }
    }
    return NULL;
}

/* Reads the build id into mod->build_id. */
static void
read_build_id(Elf *elf, elf_info_t *mod)
{
    Elf_Scn *scn;
    for (scn = elf_getscn(elf, 0); scn != NULL; scn = elf_nextscn(elf, scn)) {
        Elf_Shdr *section_header = elf_getshdr(scn);
        if (section_header == NULL || section_header->sh_type != SHT_NOTE)
            continue;
        Elf_Data *data = elf_getdata(scn, NULL);
        Elf_Note *note = (Elf_Note *)data->d_buf;
        if (note->n_type == NT_GNU_BUILD_ID) {
            /* Following the note are the name and value. */
            byte *src = ((byte *)(note + 1)) + note->n_namesz;
            size_t size = note->n_descsz;
            if ((byte *)data->d_buf + data->d_size < src + note->n_descsz) {
                NOTIFY_ELF("note data is shorter than specified length");
                size = (byte *)data->d_buf + data->d_size - src;
            }
            char *dst = mod->build_id;
            for (int i = 0; i < size; i++) {
                /* We're writing 3 chars at a time (2 digits + newline). */
                if (dst + 3 > mod->build_id + MAX_BUILD_ID_LENGTH) {
                    NOTIFY_ELF("build id is too long");
                    /* It is already null-terminated from the prior write.  Return
                     * the truncated id.  It will likely still work for buildid-dir
                     * purposes where we only need the 1st 2 chars, and the rest
                     * come from the debuglink name.
                     */
                    return;
                }
                unsigned int val = (unsigned int)*src;
                int len = dr_snprintf(dst, 3, "%02x", val);
                if (len < 0) {
                    NOTIFY_ELF("malformed build id");
                    mod->build_id[0] = '\0';
                    return;
                }
                dst += len;
                src++;
            }
            return;
        }
    }
}

/* Iterates the program headers for an ELF object and returns the minimum
 * segment load address.  For executables this is generally a well-known
 * address.  For PIC shared libraries this is usually 0.  For DR clients this is
 * the preferred load address.  If we find no loadable sections, we return zero
 * also.
 */
static ptr_uint_t
find_load_base(Elf *elf)
{
    Elf_Ehdr *ehdr = elf_getehdr(elf);
    Elf_Phdr *phdr = elf_getphdr(elf);
    uint i;
    ptr_uint_t load_base = 0;
    bool found_pt_load = false;

    if (ehdr == NULL || phdr == NULL) {
        NOTIFY_ELF("ehdr+phdr");
        return 0;
    }

    for (i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            if (!found_pt_load) {
                found_pt_load = true;
                load_base = phdr[i].p_vaddr;
            } else {
                load_base = MIN(load_base, phdr[i].p_vaddr);
            }
        }
    }

    return load_base;
}

/******************************************************************************
 * ELF interface to drsyms_unix.c
 */

void
drsym_obj_init(void)
{
    elf_version(EV_CURRENT);
}

void *
drsym_obj_mod_init_pre(byte *map_base, size_t map_size)
{
    elf_info_t *mod;
    Elf_Scn *symtab_scn;
    Elf_Scn *strtab_scn;
    Elf_Shdr *symtab_shdr;

    mod = dr_global_alloc(sizeof(*mod));
    memset(mod, 0, sizeof(*mod));
    mod->map_base = map_base;

    mod->elf = elf_memory((char *)map_base, map_size);

    symtab_scn = find_elf_section_by_name(mod->elf, ".symtab");
    strtab_scn = find_elf_section_by_name(mod->elf, ".strtab");

    if (symtab_scn != NULL) {
        mod->debug_kind |= DRSYM_SYMBOLS | DRSYM_ELF_SYMTAB;
    } else {
        /* Module is stripped, but we should still look at exports.
         * Note that .dynsym should be a subset of .symtab so if we have
         * .symtab we can ignore .dynsym.
         */
        /* XXX i#672: there may still be dwarf2 or stabs sections even if the
         * symtable is stripped and we could do symbol lookup via dwarf2
         */
        /* XXX: better to look for sh_type==SHT_DYNSYM than the name? */
        symtab_scn = find_elf_section_by_name(mod->elf, ".dynsym");
        strtab_scn = find_elf_section_by_name(mod->elf, ".dynstr");
    }

    if (symtab_scn != NULL) {
        if (strtab_scn != NULL) {
            symtab_shdr = elf_getshdr(symtab_scn);
            mod->strtab_idx = elf_ndxscn(strtab_scn);
            mod->num_syms = symtab_shdr->sh_size / symtab_shdr->sh_entsize;

            /* This assumes that the ELF file uses the same representation conventions
             * as the current machine, which is reasonable considering this module is
             * probably loaded in the current process.
             */
            mod->syms = (Elf_Sym *)(((char *)mod->map_base) + symtab_shdr->sh_offset);
        }
    }

    if (find_elf_section_by_name(mod->elf, ".debug_line") != NULL) {
        mod->debug_kind |= DRSYM_LINE_NUMS | DRSYM_DWARF_LINE;
    }

    read_build_id(mod->elf, mod);

    return (void *)mod;
}

bool
drsym_obj_mod_init_post(void *mod_in, byte *map_base, void *dwarf_info)
{
    elf_info_t *mod = (elf_info_t *)mod_in;
    mod->map_base = map_base; /* shouldn't change, though */
    mod->load_base = find_load_base(mod->elf);
    return true;
}

bool
drsym_obj_dwarf_init(void *mod_in, Dwarf_Debug *dbg)
{
    elf_info_t *mod = (elf_info_t *)mod_in;
    Dwarf_Error de; /* expensive to init (DrM#1770) */
    if (mod == NULL)
        return false;
    if (dwarf_elf_init(mod->elf, DW_DLC_READ, NULL, NULL, dbg, &de) != DW_DLV_OK) {
        NOTIFY_DWARF(de);
        return false;
    }
    return true;
}

void
drsym_obj_mod_exit(void *mod_in)
{
    elf_info_t *mod = (elf_info_t *)mod_in;
    if (mod == NULL)
        return;
    if (mod->elf != NULL)
        elf_end(mod->elf);
    dr_global_free(mod, sizeof(*mod));
}

drsym_debug_kind_t
drsym_obj_info_avail(void *mod_in)
{
    elf_info_t *mod = (elf_info_t *)mod_in;
    return mod->debug_kind;
}

byte *
drsym_obj_load_base(void *mod_in)
{
    elf_info_t *mod = (elf_info_t *)mod_in;
    return (byte *)mod->load_base;
}

/* Return the path contained in the .gnu_debuglink section or NULL if we cannot
 * find it.
 *
 * XXX: There's also a CRC in here that we could use to warn if the files are
 * out of sync.
 */
const char *
drsym_obj_debuglink_section(void *mod_in, const char *modpath)
{
    elf_info_t *mod = (elf_info_t *)mod_in;
    Elf_Shdr *section_header;
    Elf_Scn *scn = find_elf_section_by_name(mod->elf, ".gnu_debuglink");
    if (scn == NULL)
        return NULL;
    section_header = elf_getshdr(scn);
    if (section_header == NULL) {
        NOTIFY_ELF("elf_getshdr .gnu_debuglink");
        return NULL;
    }
    return ((char *)mod->map_base) + section_header->sh_offset;
}

uint
drsym_obj_num_symbols(void *mod_in)
{
    elf_info_t *mod = (elf_info_t *)mod_in;
    if (mod == NULL)
        return 0;
    return mod->num_syms;
}

const char *
drsym_obj_symbol_name(void *mod_in, uint idx)
{
    elf_info_t *mod = (elf_info_t *)mod_in;
    if (mod == NULL || idx >= mod->num_syms || mod->syms == NULL)
        return NULL;
    return elf_strptr(mod->elf, mod->strtab_idx, mod->syms[idx].st_name);
}

drsym_error_t
drsym_obj_symbol_offs(void *mod_in, uint idx, size_t *offs_start OUT,
                      size_t *offs_end OUT)
{
    elf_info_t *mod = (elf_info_t *)mod_in;
    if (offs_start == NULL || mod == NULL || idx >= mod->num_syms || mod->syms == NULL)
        return DRSYM_ERROR_INVALID_PARAMETER;
    /* Keep this consistent with symbol_is_import() and elf_hash_lookup(), both at
     * core/unix/module_elf.c
     */
    if ((mod->syms[idx].st_value == 0 &&
         ELF_ST_TYPE(mod->syms[idx].st_info) != STT_TLS) ||
        mod->syms[idx].st_shndx == 0) {
        /* We're looking at .dynsym and this is an import */
        *offs_start = 0;
        if (offs_end != NULL)
            *offs_end = 0;
        return DRSYM_ERROR_SYMBOL_NOT_FOUND;
    }
    *offs_start = mod->syms[idx].st_value - mod->load_base;
    if (offs_end != NULL) {
        /* XXX i#1337: we don't try to handle st_size==0 asm routines as we
         * don't want to take the time to find the next entry.  We could sort
         * symtab into our own data structure to solve that, and then assume
         * it goes to the next entry
         */
        *offs_end = mod->syms[idx].st_value + mod->syms[idx].st_size - mod->load_base;
    }
    return DRSYM_SUCCESS;
}

drsym_error_t
drsym_obj_addrsearch_symtab(void *mod_in, size_t modoffs, uint *idx OUT)
{
    elf_info_t *mod = (elf_info_t *)mod_in;
    int i;
    int closest_idx = -1;
    size_t closest_diff = SIZE_T_MAX;

    if (mod == NULL || mod->syms == NULL || idx == NULL)
        return DRSYM_ERROR;

    NOTIFY(1, "%s: +" PIFX "\n", __FUNCTION__, modoffs);
    /* XXX: if a function is split into non-contiguous pieces, will it
     * have multiple entries?
     */
    for (i = 0; i < mod->num_syms; i++) {
        size_t lo_offs = mod->syms[i].st_value - mod->load_base;
        size_t hi_offs = lo_offs + mod->syms[i].st_size;
        NOTIFY(3, "\tcomparing +" PIFX " to " PIFX "-" PIFX "\n", modoffs, lo_offs,
               hi_offs);
        if (lo_offs <= modoffs && modoffs < hi_offs) {
            NOTIFY(2, "\tfound +" PIFX " in " PIFX "-" PIFX "\n", modoffs, lo_offs,
                   hi_offs);
            *idx = i;
            return DRSYM_SUCCESS;
        }
        /* i#1337: handle st_size==0 asm routines */
        if (modoffs >= lo_offs) {
            if (modoffs - lo_offs < closest_diff) {
                closest_idx = i;
                closest_diff = modoffs - lo_offs;
                NOTIFY(3, "\tclosest diff is now " PIFX "\n", closest_diff);
            }
        }
    }

    if (closest_idx >= 0 && mod->syms[closest_idx].st_size == 0) {
        /* i#1337: rule out anything without a name */
        const char *name = drsym_obj_symbol_name(mod_in, closest_idx);
        NOTIFY(2, "\tusing closest +" PIFX " diff " PIFX "\n", modoffs, closest_diff);
        if (name != NULL && name[0] != '\0') {
            *idx = closest_idx;
            return DRSYM_SUCCESS;
        }
    }

    return DRSYM_ERROR_SYMBOL_NOT_FOUND;
}

const char *
drsym_obj_build_id(void *mod_in)
{
    elf_info_t *mod = (elf_info_t *)mod_in;
    return mod->build_id;
}

/******************************************************************************
 * Linux-specific helpers
 */

/* Returns true if the two paths have the same inode.  Returns false if there
 * was an error or they are different.
 *
 * XXX: Generally, making syscalls without going through DynamoRIO isn't safe,
 * but 'stat' isn't likely to cause resource conflicts with the app or mess up
 * DR's vm areas tracking.
 */
bool
drsym_obj_same_file(const char *path1, const char *path2)
{
    struct stat stat1;
    struct stat stat2;
    int r;

    r = stat(path1, &stat1);
    if (r != 0)
        return false;
    r = stat(path2, &stat2);
    if (r != 0)
        return false;

    return stat1.st_ino == stat2.st_ino;
}

const char *
drsym_obj_debug_path(void)
{
    return "/usr/lib/debug";
}
