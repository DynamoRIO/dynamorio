/* **********************************************************
 * Copyright (c) 2011 Google, Inc.  All rights reserved.
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

/* Symbol lookup for Linux
 *
 * For symbol and address lookup and enumeration we use a combination of libelf
 * and libdwarf.  All symbol and address lookup is dealt with by parsing the
 * .symtab section, which points to symbols in the .strtab section.  To get line
 * number information, we have to go the extra mile and use libdwarf to dig
 * through the .debug_line section, which was added in DWARF2.  We don't support
 * STABS or any other form of line number debug information.
 *
 * FIXME i#545: Provide an API to demangle the symbol names returned by
 * drsym_lookup_address.
 */

#include "dr_api.h"
#include "drsyms.h"
#include "drsyms_private.h"
#include "hashtable.h"

#include <stdio.h> /* _vsnprintf */
#include <stdarg.h> /* va_list */
#include <string.h> /* strlen */
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "demangle.h"
#include "dwarf.h"
#include "libdwarf.h"
#include "libelf.h"
#include "libelftc.h"

/* Guards our internal state and libdwarf's modifications of mod->dbg. */
void *symbol_lock;

/* Hashtable for mapping module paths to dbg_module_t*. */
#define MODTABLE_HASH_BITS 8
static hashtable_t modtable;

/* For debugging */
static bool verbose = false;

#undef NOTIFY /* from DrMem utils.h */
#define NOTIFY(...) do { \
    if (verbose) { \
        dr_fprintf(STDERR, __VA_ARGS__); \
    } \
} while (0)

#define NOTIFY_DWARF(de) do { \
    if (verbose) { \
        dr_fprintf(STDERR, "drsyms: Dwarf error: %s\n", dwarf_errmsg(de)); \
    } \
} while (0)

#define NOTIFY_ELF() do { \
    if (verbose) { \
        dr_fprintf(STDERR, "drsyms: Elf error: %s\n", elf_errmsg(elf_errno())); \
    } \
} while (0)

#ifndef MIN
# define MIN(x, y) ((x) <= (y) ? (x) : (y))
#endif

/* Sideline server support */
static int shmid;

#define IS_SIDELINE (shmid != 0)

typedef struct _dbg_module_t {
    file_t fd;
    size_t file_size;
    void *map_base;
    Elf *elf;
    Dwarf_Debug dbg;
    ptr_uint_t load_base;
} dbg_module_t;

/******************************************************************************
 * Forward declarations.
 */

static void unload_module(dbg_module_t *mod);
static dbg_module_t *follow_debuglink(const char * modpath, dbg_module_t *mod,
                                      const char *debuglink);

/******************************************************************************
 * ELF helpers.
 */

/* XXX: If we ever need to worry about ELF32 objects in an x64 process, we can
 * use gelf or some other library to translate elf32/64 structs into a common
 * representation.
 */
#ifdef X64
# define elf_getehdr elf64_getehdr
# define elf_getphdr elf64_getphdr
# define elf_getshdr elf64_getshdr
# define Elf_Ehdr Elf64_Ehdr
# define Elf_Phdr Elf64_Phdr
# define Elf_Shdr Elf64_Shdr
# define Elf_Sym  Elf64_Sym
#else
# define elf_getehdr elf32_getehdr
# define elf_getphdr elf32_getphdr
# define elf_getshdr elf32_getshdr
# define Elf_Ehdr Elf32_Ehdr
# define Elf_Phdr Elf32_Phdr
# define Elf_Shdr Elf32_Shdr
# define Elf_Sym  Elf32_Sym
#endif

static Elf_Scn *
find_elf_section_by_name(Elf *elf, const char *match_name)
{
    Elf_Scn *scn;
    size_t shstrndx;  /* Means "section header string table section index" */

    if (elf_getshdrstrndx(elf, &shstrndx) != 0) {
        NOTIFY_ELF();
        return NULL;
    }

    for (scn = elf_getscn(elf, 0); scn != NULL; scn = elf_nextscn(elf, scn)) {
        Elf_Shdr *section_header = elf_getshdr(scn);
        const char *sec_name;
        if (section_header == NULL) {
            NOTIFY_ELF();
            continue;
        }
        sec_name = elf_strptr(elf, shstrndx, section_header->sh_name);
        if (sec_name == NULL) {
            NOTIFY_ELF();
        }
        if (strcmp(sec_name, match_name) == 0) {
            return scn;
        }
    }
    return NULL;
}

/* Return the path contained in the .gnu_debuglink section or NULL if we cannot
 * find it.
 *
 * XXX: There's also a CRC in here that we could use to warn if the files are
 * out of sync.
 */
static const char *
find_debuglink_section(void *map_base, Elf *elf)
{
    Elf_Shdr *section_header =
        elf_getshdr(find_elf_section_by_name(elf, ".gnu_debuglink"));
    if (section_header == NULL) {
        NOTIFY_ELF();
        return NULL;
    }
    return ((char*) map_base) + section_header->sh_offset;
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
        NOTIFY_ELF();
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
 * Module loading and unloading.
 */

static dbg_module_t *
load_module(const char *modpath)
{
    dbg_module_t *mod;
    size_t map_size;
    Dwarf_Error de = {0};
    bool ok;
    const char *debuglink;
    uint64_t file_size;

    /* static depth count to prevent stack overflow from circular .gnu_debuglink
     * sections.  We're protected by symbol_lock.
     */
    static int load_module_depth;

    if (load_module_depth >= 2) {
        NOTIFY("drsyms: Refusing to follow .gnu_debuglink more than 2 times.\n");
        return NULL;
    }
    load_module_depth++;

    NOTIFY("loading debug info for module %s\n", modpath);

    /* Alloc and zero the struct so it can be unloaded safely in case of error. */
    mod = dr_global_alloc(sizeof(*mod));
    memset(mod, 0, sizeof(*mod));

    mod->fd = dr_open_file(modpath, DR_FILE_READ);
    if (mod->fd == INVALID_FILE)
        goto error;

    ok = dr_file_size(mod->fd, &file_size);
    if (!ok)
        goto error;
    mod->file_size = file_size;

    map_size = mod->file_size;
    mod->map_base = dr_map_file(mod->fd, &map_size, 0, NULL, DR_MEMPROT_READ,
                                DR_MAP_PRIVATE);
    if (mod->map_base == NULL)
        goto error;
    if (map_size != mod->file_size)
        goto error;

    mod->elf = elf_memory(mod->map_base, mod->file_size);
    if (mod->elf == NULL)
        goto error;

    /* If there is a .gnu_debuglink section, then all the debug info we care
     * about is in the file it points to.
     */
    debuglink = find_debuglink_section(mod->map_base, mod->elf);
    if (debuglink != NULL) {
        mod = follow_debuglink(modpath, mod, debuglink);
    } else {
        /* If there is no .gnu_debuglink, initialize parsing. */
        mod->load_base = find_load_base(mod->elf);
        if (dwarf_elf_init(mod->elf, DW_DLC_READ, NULL, NULL, &mod->dbg,
                           &de) != DW_DLV_OK) {
            NOTIFY_DWARF(de);
            goto error;
        }
    }

    load_module_depth--;
    return mod;

error:
    unload_module(mod);
    load_module_depth--;
    return NULL;
}

/* Returns true if the two paths have the same inode.  Returns false if there
 * was an error or they are different.
 *
 * XXX: Generally, making syscalls without going through DynamoRIO isn't safe,
 * but 'stat' isn't likely to cause resource conflicts with the app or mess up
 * DR's vm areas tracking.
 */
static bool
is_same_file(const char *path1, const char *path2)
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

/* Construct a hybrid dbg_module_t that loads debug information from the
 * debuglink path.
 *
 * Gdb's search algorithm for finding debug info files is documented here:
 * http://sourceware.org/gdb/onlinedocs/gdb/Separate-Debug-Files.html
 *
 * FIXME: We should allow the user to register additional search directories.
 * XXX: We may need to support the --build-id debug file mechanism documented at
 * the above URL, but for now, .gnu_debuglink seems to work for most Linux
 * systems.
 */
static dbg_module_t *
follow_debuglink(const char *modpath, dbg_module_t *mod, const char *debuglink)
{
    char mod_dir[MAXIMUM_PATH];
    char debug_modpath[MAXIMUM_PATH];
    char *last_slash;

    /* Get the module's directory. */
    strncpy(mod_dir, modpath, MAXIMUM_PATH);
    NULL_TERMINATE_BUFFER(mod_dir);
    last_slash = strrchr(mod_dir, '/');
    if (last_slash != NULL)
        *last_slash = '\0';

    /* 1. Check $mod_dir/$debuglink */
    dr_snprintf(debug_modpath, MAXIMUM_PATH,
                "%s/%s", mod_dir, debuglink);
    NULL_TERMINATE_BUFFER(debug_modpath);
    /* If debuglink is the basename of modpath, this can point to the same file.
     * Infinite recursion is prevented with a depth check, but we would fail to
     * test the other paths, so we check here if these paths resolve to the same
     * file, ignoring hard and soft links and other path quirks.
     */
    if (dr_file_exists(debug_modpath) && !is_same_file(modpath, debug_modpath))
        goto found;

    /* 2. Check $mod_dir/.debug/$debuglink */
    dr_snprintf(debug_modpath, MAXIMUM_PATH,
                "%s/.debug/%s", mod_dir, debuglink);
    NULL_TERMINATE_BUFFER(debug_modpath);
    if (dr_file_exists(debug_modpath))
        goto found;

    /* 3. Check /usr/lib/debug/$mod_dir/$debuglink */
    dr_snprintf(debug_modpath, MAXIMUM_PATH,
                "/usr/lib/debug%s/%s", mod_dir, debuglink);
    NULL_TERMINATE_BUFFER(debug_modpath);
    if (dr_file_exists(debug_modpath))
        goto found;

    /* We couldn't find the debug file, so we make do with the original module
     * instead.
     *
     * XXX: We should parse the .dynsym section so this is actually useful.
     * Right now clients use a mix of dr_get_proc_address and drsyms, when we
     * could handle all of that for them.
     */
    return mod;

found:
    /* debuglink points into the mapped module, so we can't free until now. */
    unload_module(mod);

    return load_module(debug_modpath);
}

/* Free all resources associated with the debug module and the object itself.
 */
static void
unload_module(dbg_module_t *mod)
{
    if (mod->dbg != NULL)
        dwarf_finish(mod->dbg, NULL);
    if (mod->elf != NULL)
        elf_end(mod->elf);
    if (mod->map_base != NULL)
        dr_unmap_file(mod->map_base, mod->file_size);
    if (mod->fd != INVALID_FILE)
        dr_close_file(mod->fd);
    dr_global_free(mod, sizeof(*mod));
}

static dbg_module_t *
lookup_or_load(const char *modpath)
{
    dbg_module_t *mod = (dbg_module_t*) hashtable_lookup(&modtable, (void*)modpath);
    if (mod == NULL) {
        mod = load_module(modpath);
        if (mod != NULL) {
            hashtable_add(&modtable, (void*)modpath, mod);
        }
    }
    return mod;
}

/******************************************************************************
 * ELF .symtab parsing helpers.
 */

/* Get a pointer into the .symtab section of an ELF object.  Return the number
 * of symbols and the strtab index in the two output parameters.  Returns NULL
 * on failure.
 */
static Elf_Sym *
get_elf_syms(dbg_module_t *mod, int *strtab_idx OUT, int *num_syms OUT)
{
    Elf_Scn *symtab_scn;
    Elf_Scn *strtab_scn;
    Elf_Shdr *symtab_shdr;

    DR_ASSERT(strtab_idx != NULL && num_syms != NULL);

    symtab_scn = find_elf_section_by_name(mod->elf, ".symtab");
    strtab_scn = find_elf_section_by_name(mod->elf, ".strtab");

    if (symtab_scn == NULL || strtab_scn == NULL)
        return NULL;

    symtab_shdr = elf_getshdr(symtab_scn);
    *strtab_idx = elf_ndxscn(strtab_scn);
    *num_syms = symtab_shdr->sh_size / symtab_shdr->sh_entsize;

    /* This assumes that the ELF file uses the same representation conventions
     * as the current machine, which is reasonable considering this module is
     * probably loaded in the current process.
     */
    return (Elf_Sym*)(((char*) mod->map_base) + symtab_shdr->sh_offset);
}

static drsym_error_t
symsearch_symtab(dbg_module_t *mod, drsym_enumerate_cb callback, void *data,
                 uint flags)
{
    Elf_Sym *syms;
    int strtab_idx;
    int num_syms;
    int i;
    bool keep_searching = true;
    char *symbol_buf;
    size_t symbol_buf_size = 1024;  /* C++ symbols can be quite long. */
    void *dc = dr_get_current_drcontext();

    syms = get_elf_syms(mod, &strtab_idx, &num_syms);
    if (syms == NULL)
        return DRSYM_ERROR;

    symbol_buf = dr_thread_alloc(dc, symbol_buf_size);

    for (i = 0; keep_searching && i < num_syms; i++) {
        const char *mangled = elf_strptr(mod->elf, strtab_idx, syms[i].st_name);
        const char *unmangled = mangled;  /* Points at mangled or symbol_buf. */
        size_t modoffs = syms[i].st_value - mod->load_base;

        if (TEST(DRSYM_DEMANGLE, flags)) {
            size_t len;
            /* Resize until it's big enough. */
            while ((len = drsym_demangle_symbol(symbol_buf, symbol_buf_size,
                                                mangled, flags))
                   > symbol_buf_size) {
                dr_thread_free(dc, symbol_buf, symbol_buf_size);
                symbol_buf_size = len;
                symbol_buf = dr_thread_alloc(dc, symbol_buf_size);
            }
            if (len != 0) {
                /* Success. */
                unmangled = symbol_buf;
            }
        }

        keep_searching = callback(unmangled, modoffs, data);
    }

    dr_thread_free(dc, symbol_buf, symbol_buf_size);

    return DRSYM_SUCCESS;
}

static drsym_error_t
addrsearch_symtab(dbg_module_t *mod, size_t modoffs, drsym_info_t *info INOUT,
                  uint flags)
{
    Elf_Sym *syms;
    int strtab_idx;
    int num_syms;
    int i;

    syms = get_elf_syms(mod, &strtab_idx, &num_syms);
    if (syms == NULL)
        return DRSYM_ERROR;

    for (i = 0; i < num_syms; i++) {
        size_t lo_offs = syms[i].st_value - mod->load_base;
        size_t hi_offs = lo_offs + syms[i].st_size;
        if (lo_offs <= modoffs && modoffs < hi_offs) {
            const char *symbol = elf_strptr(mod->elf, strtab_idx, syms[i].st_name);
            size_t name_len = 0;

            if (TEST(DRSYM_DEMANGLE, flags)) {
                name_len = drsym_demangle_symbol(info->name, info->name_size,
                                                 symbol, flags);
            }
            if (name_len == 0) {
                /* Demangling either failed or was not requested. */
                name_len = strlen(symbol) + 1;
                strncpy(info->name, symbol, info->name_size);
                info->name[info->name_size - 1] = '\0';
            }

            info->name_available_size = name_len;
            info->start_offs = lo_offs;
            info->end_offs = hi_offs;

            return DRSYM_SUCCESS;
        }
    }

    return DRSYM_ERROR_SYMBOL_NOT_FOUND;
}

/******************************************************************************
 * DWARF parsing code.
 */

/* Iterate over all the CUs in the module to find the CU containing the given
 * PC.
 */
static Dwarf_Die
find_cu_die(Dwarf_Debug dbg, Dwarf_Addr pc)
{
    Dwarf_Die die = NULL;
    Dwarf_Unsigned cu_offset;
    Dwarf_Error de = {0};
    Dwarf_Die cu_die = NULL;

    while (dwarf_next_cu_header(dbg, NULL, NULL, NULL, NULL,
                                &cu_offset, &de) == DW_DLV_OK) {
        /* Find the first CU DIE in this CU.  dwarf_next_cu_header updates
         * internal state to track the current CU, and dwarf_siblingof gets us
         * the first DIE in the CU.
         */
        die = NULL;
        Dwarf_Half tag;
        while (dwarf_siblingof(dbg, die, &die, &de) == DW_DLV_OK) {
            if (dwarf_tag(die, &tag, &de) != DW_DLV_OK) {
                NOTIFY_DWARF(de);
                die = NULL;
                break;
            }
            if (tag == DW_TAG_compile_unit)
                break;
        }

        /* We found a CU die, now check if it's the one we wanted. */
        if (die != NULL) {
            Dwarf_Addr lo_pc, hi_pc;
            if (dwarf_lowpc(die, &lo_pc, &de) != DW_DLV_OK ||
                dwarf_highpc(die, &hi_pc, &de) != DW_DLV_OK) {
                NOTIFY_DWARF(de);
                break;
            }

            if (lo_pc <= pc && pc < hi_pc) {
                cu_die = die;
                break;
            }
        }
    }

    while (dwarf_next_cu_header(dbg, NULL, NULL, NULL, NULL,
                                &cu_offset, &de) == DW_DLV_OK) {
        /* Reset the internal CU header state. */
    }

    return cu_die;
}

/* Given a function DIE and a PC, fill out sym_info with line information.
 */
static bool
search_addr2line(Dwarf_Debug dbg, Dwarf_Addr pc, drsym_info_t *sym_info INOUT)
{
    Dwarf_Error de = {0};
    Dwarf_Die cu_die;
    Dwarf_Line *lines;
    Dwarf_Signed num_lines;
    int i;
    Dwarf_Addr lineaddr, next_lineaddr;
    Dwarf_Line dw_line;
    bool success = false;

    /* On failure, these should be zeroed.
     */
    sym_info->file = NULL;
    sym_info->line = 0;
    sym_info->line_offs = 0;

    /* First cut down the search space by finding the CU (ie the .c file) that
     * this function belongs to.
     */
    cu_die = find_cu_die(dbg, pc);
    if (cu_die == NULL) {
        return false;
    }

    if (dwarf_srclines(cu_die, &lines, &num_lines, &de) != DW_DLV_OK) {
        NOTIFY_DWARF(de);
        return false;
    }

    /* We could binary search this, but we assume dwarf_srclines is the
     * bottleneck.
     */
    dw_line = NULL;
    for (i = 0; i < num_lines - 1; i++) {
        if (dwarf_lineaddr(lines[i], &lineaddr, &de) != DW_DLV_OK ||
            dwarf_lineaddr(lines[i + 1], &next_lineaddr, &de) != DW_DLV_OK) {
            NOTIFY_DWARF(de);
            break;
        }
        if (lineaddr <= pc && pc < next_lineaddr) {
            dw_line = lines[i];
            break;
        }
    }
    /* Handle the case when the PC is from the last line of the CU. */
    if (i == num_lines - 1 && dw_line == NULL && next_lineaddr <= pc) {
        dw_line = lines[num_lines - 1];
    }

    /* If we found dw_line, use it to fill out sym_info. */
    if (dw_line != NULL) {
        char *file;
        Dwarf_Unsigned lineno;

        if (dwarf_linesrc(dw_line, &file, &de) != DW_DLV_OK ||
            dwarf_lineno(dw_line, &lineno, &de) != DW_DLV_OK ||
            dwarf_lineaddr(dw_line, &lineaddr, &de) != DW_DLV_OK) {
            NOTIFY_DWARF(de);
        } else {
            /* We assume file comes from .debug_str and therefore lives until
             * drsym_exit.
             */
            sym_info->file = file;
            sym_info->line = lineno;
            sym_info->line_offs = pc - lineaddr;
            success = true;
        }
    }

    dwarf_srclines_dealloc(dbg, lines, num_lines);
    return success;
}

/******************************************************************************
 * Local process symbol search helpers.
 */

static drsym_error_t
drsym_enumerate_symbols_local(const char *modpath, drsym_enumerate_cb callback,
                              void *data, uint flags)
{
    dbg_module_t *mod;
    drsym_error_t r;

    if (modpath == NULL || callback == NULL)
        return DRSYM_ERROR_INVALID_PARAMETER;

    dr_mutex_lock(symbol_lock);
    mod = lookup_or_load(modpath);
    if (mod == NULL) {
        dr_mutex_unlock(symbol_lock);
        return DRSYM_ERROR_LOAD_FAILED;
    }

    r = symsearch_symtab(mod, callback, data, flags);

    dr_mutex_unlock(symbol_lock);
    return r;
}

/* Params to sym_lookup_cb passed through data. */
typedef struct _sym_lookup_params_t {
    const char *search_sym;
    size_t search_sym_len;
    size_t *modoffs;
} sym_lookup_params_t;

/* Symbol enumeration callback for doing a single lookup.
 */
static bool
sym_lookup_cb(const char *sym, size_t modoffs, void *data INOUT)
{
    sym_lookup_params_t *p = (sym_lookup_params_t*)data;

    if (strncmp(sym, p->search_sym, p->search_sym_len) == 0 &&
        strlen(sym) >= p->search_sym_len &&
        (sym[p->search_sym_len] == '\0' ||
         /* Left paren means the beginning of the parameter list.  Since the
          * parameter list starts where our search string ends, we assume the
          * user doesn't care about possible overloads.
          */
         sym[p->search_sym_len] == '(')) {
        NOTIFY("Looked up symbol: %s %s\n", p->search_sym, sym);
        *p->modoffs = modoffs;
        /* Stop after the first match. */
        return false;
    }
    return true;
}

static drsym_error_t
drsym_lookup_symbol_local(const char *modpath, const char *symbol,
                          size_t *modoffs OUT, uint flags)
{
    drsym_error_t r;
    sym_lookup_params_t params;
    const char *sym_no_mod;

    if (modpath == NULL || symbol == NULL || modoffs == NULL)
        return DRSYM_ERROR_INVALID_PARAMETER;

    if (symbol == NULL) {
        sym_no_mod = NULL;
    } else {
        /* Ignore the module portion of the match string.  We search the module
         * specified by modpath.
         *
         * FIXME #574: Change the interface for both Linux and Windows
         * implementations to not include the module name.
         */
        sym_no_mod = strchr(symbol, '!');
        if (sym_no_mod != NULL) {
            sym_no_mod++;
        } else {
            sym_no_mod = symbol;
        }
    }

    *modoffs = 0;
    params.search_sym = sym_no_mod;
    params.search_sym_len = strlen(sym_no_mod);
    params.modoffs = modoffs;
    r = drsym_enumerate_symbols_local(modpath, sym_lookup_cb, &params, flags);
    if (r != DRSYM_SUCCESS)
        return r;
    if (*modoffs == 0)
        return DRSYM_ERROR_SYMBOL_NOT_FOUND;
    return DRSYM_SUCCESS;
}

static drsym_error_t
drsym_lookup_address_local(const char *modpath, size_t modoffs,
                           drsym_info_t *out INOUT, uint flags)
{
    dbg_module_t *mod;
    drsym_error_t r;

    if (modpath == NULL || out == NULL)
        return DRSYM_ERROR_INVALID_PARAMETER;
    /* If we add fields in the future we would dispatch on out->struct_size */
    if (out->struct_size != sizeof(*out))
        return DRSYM_ERROR_INVALID_SIZE;

    dr_mutex_lock(symbol_lock);
    mod = lookup_or_load(modpath);
    if (mod == NULL) {
        dr_mutex_unlock(symbol_lock);
        return DRSYM_ERROR_LOAD_FAILED;
    }

    r = addrsearch_symtab(mod, modoffs, out, flags);

    /* If we did find an address for the symbol, go look for its line number
     * information.
     */
    if (r == DRSYM_SUCCESS) {
        /* Search through .debug_line for line and file information.  We always
         * report success even if we only get partial line information we at
         * least have the name of the function.
         */
        if (!search_addr2line(mod->dbg, mod->load_base + modoffs, out)) {
            r = DRSYM_ERROR_LINE_NOT_AVAILABLE;
        }
    }

    dr_mutex_unlock(symbol_lock);
    return r;
}


/******************************************************************************
 * Exports.
 */

DR_EXPORT
drsym_error_t
drsym_init(int shmid_in)
{
    shmid = shmid_in;

    symbol_lock = dr_mutex_create();

    elf_version(EV_CURRENT);

    if (IS_SIDELINE) {
        /* FIXME NYI i#446: establish connection with sideline server via shared
         * memory specified by shmid
         */
    } else {
        hashtable_init_ex(&modtable, MODTABLE_HASH_BITS, HASH_STRING,
                          true/*strdup*/, false/*!synch: using symbol_lock*/,
                          (generic_func_t)unload_module, NULL, NULL);
    }
    return DRSYM_SUCCESS;
}

DR_EXPORT
drsym_error_t
drsym_exit(void)
{
    drsym_error_t res = DRSYM_SUCCESS;
    if (IS_SIDELINE) {
        /* FIXME NYI i#446 */
    }
    hashtable_delete(&modtable);
    dr_mutex_destroy(symbol_lock);
    return res;
}

DR_EXPORT
drsym_error_t
drsym_lookup_address(const char *modpath, size_t modoffs, drsym_info_t *out INOUT,
                     uint flags)
{
    if (IS_SIDELINE) {
        return DRSYM_ERROR_NOT_IMPLEMENTED;
    } else {
        return drsym_lookup_address_local(modpath, modoffs, out, flags);
    }
}

DR_EXPORT
drsym_error_t
drsym_lookup_symbol(const char *modpath, const char *symbol, size_t *modoffs OUT,
                    uint flags)
{
    if (IS_SIDELINE) {
        return DRSYM_ERROR_NOT_IMPLEMENTED;
    } else {
        return drsym_lookup_symbol_local(modpath, symbol, modoffs, flags);
    }
}

DR_EXPORT
drsym_error_t
drsym_enumerate_symbols(const char *modpath, drsym_enumerate_cb callback, void *data,
                        uint flags)
{
    if (IS_SIDELINE) {
        return DRSYM_ERROR_NOT_IMPLEMENTED;
    } else {
        return drsym_enumerate_symbols_local(modpath, callback, data, flags);
    }
}

DR_EXPORT
drsym_error_t
drsym_get_func_type(const char *modpath, size_t modoffs, char *buf,
                    size_t buf_sz, drsym_func_type_t **func_type OUT)
{
    return DRSYM_ERROR_NOT_IMPLEMENTED;
}

DR_EXPORT
size_t
drsym_demangle_symbol(char *dst OUT, size_t dst_sz, const char *mangled,
                      uint flags)
{
    int status;

    if (!TEST(DRSYM_DEMANGLE_FULL, flags)) {
        /* The demangle.cc implementation is fast and replaces template args and
         * overloads with <> and () respectively.  Use it if the user doesn't
         * want either of those.  Its return value always follows our
         * conventions.
         */
        int len = Demangle(mangled, dst, dst_sz, DEMANGLE_DEFAULT);
        if (len > 0) {
            return len;  /* Success or truncation. */
        }
    } else {
        /* If the user wants template arguments or overloads, we use the
         * libelftc demangler which is slower, but can properly demangle
         * template arguments.
         */
        status = elftc_demangle(mangled, dst, dst_sz, ELFTC_DEM_GNU3);
        if (status == 0) {
            return strlen(dst) + 1;
        } else if (errno == ENAMETOOLONG) {
            /* FIXME: libelftc actually doesn't copy the output into dst and
             * truncate it, so we do the next best thing and put the truncated
             * mangled name in there.
             */
            strncpy(dst, mangled, dst_sz);
            dst[dst_sz-1] = '\0';
            /* FIXME: This return value is made up and may not be large enough.
             * It will work eventually if the caller reallocates their buffer
             * and retries in a loop, or if they just want to detect truncation.
             */
            return dst_sz * 2;
        }
    }

    /* If the demangling failed, copy the mangled symbol into the output. */
    strncpy(dst, mangled, dst_sz);
    dst[dst_sz-1] = '\0';
    return 0;
}
