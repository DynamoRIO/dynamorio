/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
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

/* DRSyms DynamoRIO Extension */

/* Symbol lookup routines for Mach-O */

#include "dr_api.h"
#include "drsyms.h"
#include "drsyms_private.h"
#include "drsyms_obj.h"

#include "dwarf.h"
#include "libdwarf.h"

#include <mach-o/loader.h> /* mach_header */
#include <mach-o/nlist.h>

#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#ifndef MIN
# define MIN(x, y) ((x) <= (y) ? (x) : (y))
#endif

#ifndef SIZE_T_MAX
# ifdef X64
#  define SIZE_T_MAX ULLONG_MAX
# else
#  define SIZE_T_MAX UINT_MAX
# endif
#endif

static uint verbose = 0;

#undef NOTIFY
#define NOTIFY(n, ...) do { \
    if (verbose >= (n)) {             \
        dr_fprintf(STDERR, __VA_ARGS__); \
    } \
} while (0)

#define NOTIFY_DWARF(de) do { \
    if (verbose) { \
        dr_fprintf(STDERR, "drsyms: Dwarf error: %s\n", dwarf_errmsg(de)); \
    } \
} while (0)

/* XXX i#1345: support mixed-mode 32-bit and 64-bit in one process.
 * There is no official support for that on Linux or Windows and for now we do
 * not support it either, especially not mixing libraries.
 */
#ifdef X64
typedef struct mach_header_64 mach_header_t;
typedef struct segment_command_64 segment_command_t;
typedef struct section_64 section_t;
typedef struct nlist_64 nlist_t;
#else
typedef struct mach_header mach_header_t;
typedef struct segment_command segment_command_t;
typedef struct section section_t;
typedef struct nlist nlist_t;
#endif

typedef struct _macho_info_t {
    byte *map_base;
    ptr_uint_t load_base;
    drsym_debug_kind_t debug_kind;
    nlist_t *syms;
    uint num_syms;
    byte *strtab;
    uint strsz;
    /* Since we have no symbol sizes, we sort the symbol table to get
     * at least the value of the next entry.
     */
    nlist_t **sorted_syms;
    uint sorted_count;
} macho_info_t;

/******************************************************************************
 * Internal routines
 */

static bool
is_macho_header(byte *base)
{
    mach_header_t *hdr = (mach_header_t *) base;
    /* We deliberately don't check hdr->filetype as we don't want to limit
     * ourselves to just MH_EXECUTE, MH_DYLIB, or MH_BUNDLE in case others
     * have symbols as well.
     */
#ifdef X64
    return (hdr->magic == MH_MAGIC_64 && hdr->cputype == CPU_TYPE_X86_64);
#else
    return (hdr->magic == MH_MAGIC && hdr->cputype == CPU_TYPE_X86);
#endif
}

/* Iterates the program headers for a Mach-O object and returns the minimum
 * segment load address.  For executables this is generally a well-known
 * address.  For PIC shared libraries this is usually 0.  For DR clients this is
 * the preferred load address.  If we find no loadable sections, we return zero
 * also.
 */
static ptr_uint_t
find_load_base(byte *map_base)
{
    mach_header_t *hdr = (mach_header_t *) map_base;
    struct load_command *cmd, *cmd_stop;
    ptr_uint_t load_base = 0;
    bool found_seg = false;
    if (!is_macho_header(map_base))
        return 0;
    cmd = (struct load_command *)(hdr + 1);
    cmd_stop = (struct load_command *)((byte *)cmd + hdr->sizeofcmds);
    while (cmd < cmd_stop) {
        if (cmd->cmd == LC_SEGMENT) {
            segment_command_t *seg = (segment_command_t *) cmd;
            if (!found_seg) {
                found_seg = true;
                load_base = seg->vmaddr;
            } else {
                load_base = MIN(load_base, seg->vmaddr);
            }
        }
        cmd = (struct load_command *)((byte *)cmd + cmd->cmdsize);
    }
    return load_base;
}

static int
compare_symbols(const void *a_in, const void *b_in)
{
    const nlist_t *a = *(const nlist_t **)a_in;
    const nlist_t *b = *(const nlist_t **)b_in;
    if (a->n_value > b->n_value)
        return 1;
    if (a->n_value < b->n_value)
        return -1;
    return 0;
}

/* Creates a sorted array of IMAGE_SYMBOL* entries that we can use for address lookup
 * and for simpler iteration with no gaps from aux entries
 */
static void
drsym_macho_sort_symbols(macho_info_t *mod)
{
    nlist_t *sym = mod->syms;
    uint i;
    /* We throw out all symbols with zero value, but we don't bother
     * to do two passes or a realloc to save memory.
     */
    mod->sorted_syms = (nlist_t **)
        dr_global_alloc(mod->num_syms*sizeof(*mod->sorted_syms));
    mod->sorted_count = 0;
    for (i = 0; i < mod->num_syms; i++, sym++) {
        /* Rule out value==0 and empty names */
        if (sym->n_value > 0 && sym->n_un.n_strx > 0 &&
            sym->n_un.n_strx < mod->strsz) {
            const char *name = (const char *)(mod->strtab + sym->n_un.n_strx);
            /* There are symbols with empty strings inside strtab.
             * We could probably rule out by checking the type or sthg
             * but this will do as well.
             */
            if (name[0] != '\0')
                mod->sorted_syms[mod->sorted_count++] = sym;
        }
    }
    /* XXX: using libc qsort.  libelftoolchain is already using libc, and
     * qsort is in-place and self-contained, so it should be fine.
     */
    qsort(mod->sorted_syms, mod->sorted_count, sizeof(nlist_t *), compare_symbols);

    if (verbose >= 3) {
        NOTIFY(3, "%s:\n", __FUNCTION__);
        for (i = 0; i < mod->sorted_count; i++) {
            sym = mod->sorted_syms[i];
            NOTIFY(3, "  #%d: %-20s val=0x%x\n",
                   i, drsym_obj_symbol_name(mod, i), sym->n_value);
        }
    }
}

/******************************************************************************
 * Mach-O interface to drsyms_unix.c
 */

void
drsym_obj_init(void)
{
    /* nothing */
}

void *
drsym_obj_mod_init_pre(byte *map_base, size_t file_size)
{
    macho_info_t *mod;
    mach_header_t *hdr = (mach_header_t *) map_base;
    struct load_command *cmd, *cmd_stop;

    mod = dr_global_alloc(sizeof(*mod));
    memset(mod, 0, sizeof(*mod));
    mod->map_base = map_base;

    if (!is_macho_header(map_base))
        return NULL;

    cmd = (struct load_command *)(hdr + 1);
    cmd_stop = (struct load_command *)((char *)cmd + hdr->sizeofcmds);
    while (cmd < cmd_stop) {
        if (cmd->cmd == LC_SEGMENT) {
            segment_command_t *seg = (segment_command_t *) cmd;
            section_t *sec, *sec_stop;
            sec_stop = (section_t *)((char *)seg + seg->cmdsize);
            sec = (section_t *)(seg + 1);
            while (sec < sec_stop) {
                /* sectname is not null-terminated if it's the full 16-char size */
                if (strncmp(sec->sectname, "__debug_line", sizeof(sec->sectname)) == 0)
                    mod->debug_kind |= DRSYM_LINE_NUMS | DRSYM_DWARF_LINE;
                sec++;
            }
        } else if (cmd->cmd == LC_SYMTAB) {
            /* even if stripped, dynamic symbols are in this table */
            struct symtab_command *symtab = (struct symtab_command *) cmd;
            mod->debug_kind |= DRSYM_SYMBOLS | DRSYM_MACHO_SYMTAB;
            mod->syms = (nlist_t *)(map_base + symtab->symoff);
            mod->num_syms = symtab->nsyms;
            mod->strtab = map_base + symtab->stroff;
            mod->strsz = symtab->strsize;
        }
        cmd = (struct load_command *)((char *)cmd + cmd->cmdsize);
    }

    return (void *) mod;
}

bool
drsym_obj_mod_init_post(void *mod_in, byte *map_base)
{
    macho_info_t *mod = (macho_info_t *) mod_in;
    mod->map_base = map_base; /* shouldn't change, though */
    mod->load_base = find_load_base(mod->map_base);
    drsym_macho_sort_symbols(mod);
    return true;
}

bool
drsym_obj_dwarf_init(void *mod_in, Dwarf_Debug *dbg)
{
    macho_info_t *mod = (macho_info_t *) mod_in;
    Dwarf_Error de = {0};
    if (mod == NULL)
        return false;
    if (dwarf_macho_init(mod->map_base, DW_DLC_READ, NULL, NULL, dbg, &de) != DW_DLV_OK) {
        NOTIFY_DWARF(de);
        return false;
    }
    return true;
}

void
drsym_obj_mod_exit(void *mod_in)
{
    macho_info_t *mod = (macho_info_t *) mod_in;
    if (mod == NULL)
        return;
    if (mod->sorted_syms != NULL)
        dr_global_free(mod->sorted_syms, mod->num_syms*sizeof(*mod->sorted_syms));
    dr_global_free(mod, sizeof(*mod));
}

drsym_debug_kind_t
drsym_obj_info_avail(void *mod_in)
{
    macho_info_t *mod = (macho_info_t *) mod_in;
    return mod->debug_kind;
}

byte *
drsym_obj_load_base(void *mod_in)
{
    macho_info_t *mod = (macho_info_t *) mod_in;
    return (byte *) mod->load_base;
}

const char *
drsym_obj_debuglink_section(void *mod_in)
{
    return NULL;
}

uint
drsym_obj_num_symbols(void *mod_in)
{
    macho_info_t *mod = (macho_info_t *) mod_in;
    if (mod == NULL)
        return 0;
    return mod->sorted_count;
}

const char *
drsym_obj_symbol_name(void *mod_in, uint idx)
{
    macho_info_t *mod = (macho_info_t *) mod_in;
    nlist_t *sym;
    if (mod == NULL || idx >= mod->sorted_count || mod->sorted_syms == NULL)
        return NULL;
    sym = mod->sorted_syms[idx];
    if (sym->n_un.n_strx == 0)
        return "";
    else if (sym->n_un.n_strx >= mod->strsz) /* error: bad index */
        return "";
    else {
        /* we trust it won't run off end of strtab */
        const char *name = (const char *)(mod->strtab + sym->n_un.n_strx);
#ifdef MACOS
        if (name[0] == '_') {
            /* Mach-O symbol tables seem to always have an extra underscore. */
            return name + 1;
        }
#endif
        return name;
    }
}

drsym_error_t
drsym_obj_symbol_offs(void *mod_in, uint idx, size_t *offs_start OUT,
                      size_t *offs_end OUT)
{
    macho_info_t *mod = (macho_info_t *) mod_in;
    nlist_t *sym;
    if (offs_start == NULL || mod == NULL || idx >= mod->sorted_count ||
        mod->sorted_syms == NULL)
        return DRSYM_ERROR_INVALID_PARAMETER;
    /* We removed all the symbols with value==0 when we sorted */
    sym = mod->sorted_syms[idx];
    *offs_start = sym->n_value - mod->load_base;
    if (offs_end != NULL) {
        /* XXX: the Mach-O nlist struct doesn't store the size
         * so, like PECOFF, we use the next symbol's start as the size
         * and we document that it isn't precise.
         */
        if (idx + 1 < mod->sorted_count)
            *offs_end = mod->sorted_syms[idx+1]->n_value - mod->load_base;
        else
            *offs_end = *offs_start + 1;
    }
    return DRSYM_SUCCESS;
}

drsym_error_t
drsym_obj_addrsearch_symtab(void *mod_in, size_t modoffs, uint *idx OUT)
{
    macho_info_t *mod = (macho_info_t *) mod_in;
    uint min = 0;
    uint max = mod->sorted_count - 1;
    int min_lower = -1;
    drsym_error_t res;

    if (mod == NULL || mod->sorted_syms == NULL || idx == NULL)
        return DRSYM_ERROR_INVALID_PARAMETER;

    /* binary search */
    /* XXX: share code with drsyms_pecoff.c */
    NOTIFY(1, "%s: 0x%x\n", __FUNCTION__, modoffs);
    while (max >= min) {
        uint i = (min + max) / 2;
        size_t symoffs;
        /* we ignore unknown sec here and treat all such as 0 at front of array */
        res = drsym_obj_symbol_offs(mod, i, &symoffs, NULL);
        NOTIFY(2, "\tbinary search %d => 0x%x == %s\n", i, symoffs,
               drsym_obj_symbol_name(mod_in, i));
        if (res != DRSYM_SUCCESS && res != DRSYM_ERROR_SYMBOL_NOT_FOUND)
            return res;
        if (modoffs < symoffs) {
            max = i - 1;
        } else if (modoffs >= symoffs) {
            if (max == min || modoffs == symoffs) {
                /* found closest sym with offs <= target */
                min_lower = i;
                break;
            } else {
                min_lower = i;
                min = i + 1;
            }
        }
    }
    NOTIFY(2, "\tbinary search => %d\n", min_lower);
    if (min_lower > -1) {
        /* found closest sym with offs <= target */
        *idx = min_lower;
        return DRSYM_SUCCESS;
    }
    return DRSYM_ERROR_SYMBOL_NOT_FOUND;
}

/******************************************************************************
 * Unix-specific helpers
 */

/* Returns true if the two paths have the same inode.  Returns false if there
 * was an error or they are different.
 *
 * XXX: share this with drsyms_elf.c
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
