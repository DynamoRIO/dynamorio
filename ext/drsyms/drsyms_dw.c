/* **********************************************************
 * Copyright (c) 2011-2024 Google, Inc.  All rights reserved.
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

/* Symbol lookup routines for DWARF via elfutil's libdw. */

#include "dr_api.h"
#include "drsyms.h"
#include "drsyms_private.h"
#include "drsyms_obj.h"

#include "libdw.h"

#include <stdlib.h> /* qsort */
#include <string.h>

/* For debugging */
static bool verbose = false;

/* dwarf_errmsg(-1) uses the most recent error. */
#define NOTIFY_DWARF()                                                         \
    do {                                                                       \
        if (verbose) {                                                         \
            dr_fprintf(STDERR, "drsyms: Dwarf error: %s\n", dwarf_errmsg(-1)); \
        }                                                                      \
    } while (0)

typedef struct _dwarf_module_t {
    byte *load_base;
    dwarf_lib_handle_t dbg;
    /* we cache the last CU we looked up */
    Dwarf_Die lines_cu;
    Dwarf_Lines *lines;
    size_t num_lines;
    /* Amount to adjust all offsets for __PAGEZERO + PIE (i#1365) */
    ssize_t offs_adjust;
} dwarf_module_t;

typedef enum {
    SEARCH_FOUND = 0,
    SEARCH_MAYBE = 1,
    SEARCH_NOT_FOUND = 2,
} search_result_t;

static search_result_t
search_addr2line_in_cu(dwarf_module_t *mod, Dwarf_Addr pc, Dwarf_Die *cu_die,
                       drsym_info_t *sym_info DR_PARAM_OUT);

/******************************************************************************
 * DWARF parsing code.
 */

#if 0 /* NOCHECK see below: do we need this? */
/* Find the next DIE matching this tag.  Uses the internal state of dbg to
 * determine where to start searching.
 */
static Dwarf_Die
next_die_matching_tag(dwarf_lib_handle_t dbg, Dwarf_Tag search_tag)
{
    Dwarf_Half tag = 0;
    Dwarf_Die *die = NULL;

    while (dwarf_siblingof(dbg, die, &die, &de) == DW_DLV_OK) {
        if (dwarf_tag(die, &tag, &de) != DW_DLV_OK) {
            NOTIFY_DWARF(de);
            die = NULL;
            break;
        }
        if (tag == search_tag)
            break;
    }
    return die;
}
#endif

/* Iterate over all the CUs in the module to find the CU containing the given
 * PC.
 */
static Dwarf_Die *
find_cu_die_via_iter(dwarf_lib_handle_t dbg, Dwarf_Addr pc, Dwarf_Die *cu_die)
{
    Dwarf_Die *res = NULL;
    Dwarf_Off cu_offset = 0, prev_offset = 0;
    size_t hsize;

    while (dwarf_nextcu(dbg, cu_offset, &cu_offset, &hsize, NULL, NULL, NULL) == 0) {
        /* NOCHECK: do we need to "Scan forward in the tag soup for a CU DIE" via
         * next_die_matching_tag(dbg, DW_TAG_compile_unit) as drsyms_dwarf does?
         * Wouldn't dwarf_nextcu always find a CU DIE??
         * Ditto for the other 2 dwarf_nextcu loops below.
         */
        if (dwarf_offdie(dbg, prev_offset + hsize, cu_die) != NULL) {
            /* We found a CU die, now check if it's the one we wanted. */
            Dwarf_Addr lo_pc, hi_pc;
            if (dwarf_lowpc(cu_die, &lo_pc) != 0 || dwarf_highpc(cu_die, &hi_pc) != 0) {
                NOTIFY_DWARF();
                break;
            }
            if (lo_pc <= pc && pc < hi_pc) {
                res = cu_die;
                break;
            }
        }
        prev_offset = cu_offset;
    }

    while (dwarf_nextcu(dbg, cu_offset, &cu_offset, &hsize, NULL, NULL, NULL) == 0) {
        /* Reset the internal CU header state. */
    }

    return res;
}

static Dwarf_Die *
find_cu_die(dwarf_lib_handle_t dbg, Dwarf_Addr pc, Dwarf_Die *cu_die)
{
    Dwarf_Aranges *arlist;
    size_t arcnt;
    Dwarf_Arange *ar;
    Dwarf_Off die_offs;
    if (dwarf_getaranges(dbg, &arlist, &arcnt) != 0) {
        NOTIFY_DWARF();
        return NULL;
    }
    ar = dwarf_getarange_addr(arlist, pc);
    if (ar == NULL || dwarf_getarangeinfo(ar, NULL, NULL, &die_offs) != 0 ||
        dwarf_offdie(dbg, die_offs, cu_die) == NULL) {
        NOTIFY_DWARF();
        /* Try to find it by walking all CU's and looking at their lowpc+highpc
         * entries, which should work if each has a single contiguous
         * range.  Note that Cygwin and MinGW gcc don't seen to include
         * lowpc+highpc in their CU's.
         */
        return find_cu_die_via_iter(dbg, pc, cu_die);
    }
    return cu_die;
}

/* Given a function DIE and a PC, fill out sym_info with line information.
 */
bool
drsym_dwarf_search_addr2line(void *mod_in, Dwarf_Addr pc,
                             drsym_info_t *sym_info DR_PARAM_OUT)
{
    dwarf_module_t *mod = (dwarf_module_t *)mod_in;
    Dwarf_Die cu_die;
    Dwarf_Off cu_offset = 0, prev_offset = 0;
    size_t hsize;
    bool success = false;
    search_result_t res;

    pc += mod->offs_adjust;

    /* On failure, these should be zeroed.
     */
    sym_info->file_available_size = 0;
    if (sym_info->file != NULL)
        sym_info->file[0] = '\0';
    sym_info->line = 0;
    sym_info->line_offs = 0;

    /* First try cutting down the search space by finding the CU (i.e., the .c
     * file) that this function belongs to.
     */
    if (find_cu_die(mod->dbg, pc, &cu_die) == NULL) {
        NOTIFY("%s: failed to find CU die for " PFX ", searching all CUs\n", __FUNCTION__,
               (ptr_uint_t)pc);
    } else {
        return (search_addr2line_in_cu(mod, pc, &cu_die, sym_info) != SEARCH_NOT_FOUND);
    }

    /* We failed to find a CU containing this PC.  Some compilers (clang) don't
     * put lo_pc hi_pc attributes on compilation units.  In this case, we
     * iterate all the CUs and dig into the dwarf tag soup for all of them.
     */
    while (dwarf_nextcu(mod->dbg, cu_offset, &cu_offset, &hsize, NULL, NULL, NULL) == 0) {
        /* Scan forward in the tag soup for a CU DIE. */
        if (dwarf_offdie(mod->dbg, prev_offset + hsize, &cu_die) != NULL) {
            /* We found a CU die, now check if it's the one we wanted. */
            res = search_addr2line_in_cu(mod, pc, &cu_die, sym_info);
            if (res == SEARCH_FOUND) {
                success = true;
                break;
            } else if (res == SEARCH_MAYBE) {
                success = true;
                /* try to find a better fit: continue searching */
            }
        }
        prev_offset = cu_offset;
    }

    while (dwarf_nextcu(mod->dbg, cu_offset, &cu_offset, &hsize, NULL, NULL, NULL) == 0) {
        /* Reset the internal CU header state. */
    }

    return success;
}

static size_t
get_lines_from_cu(dwarf_module_t *mod, Dwarf_Die *cu_die,
                  Dwarf_Lines **lines_out DR_PARAM_OUT)
{
    if (memcmp(&mod->lines_cu, cu_die, sizeof(mod->lines_cu)) != 0) {
        Dwarf_Lines *lines;
        size_t num_lines;
        if (dwarf_getsrclines(cu_die, &lines, &num_lines) != 0) {
            NOTIFY_DWARF();
            return -1;
        }
        /* XXX: Confirm that libdw sorts, unlike libelftc; seems to in
         * libdw/dwarf_getsrclines.c, so we don't re-sort here.
         */
        mod->lines_cu = *cu_die;
        mod->lines = lines;
        mod->num_lines = num_lines;
    }
    *lines_out = mod->lines;
    return mod->num_lines;
}

static search_result_t
search_addr2line_in_cu(dwarf_module_t *mod, Dwarf_Addr pc, Dwarf_Die *cu_die,
                       drsym_info_t *sym_info DR_PARAM_OUT)
{
    Dwarf_Lines *lines = NULL;
    size_t num_lines;
    int i;
    Dwarf_Addr lineaddr, next_lineaddr = 0;
    Dwarf_Line *dw_line;
    search_result_t res = SEARCH_NOT_FOUND;

    num_lines = get_lines_from_cu(mod, cu_die, &lines);
    if (num_lines < 0)
        return SEARCH_NOT_FOUND;

    if (verbose) {
        const char *name = dwarf_diename(cu_die);
        if (name != NULL) {
            NOTIFY("%s: searching cu %s for pc 0" PFX "\n", __FUNCTION__, name,
                   (ptr_uint_t)pc);
        }
    }

    /* We could binary search this, but we assume dwarf_srclines is the
     * bottleneck.
     */
    dw_line = NULL;
    for (i = 0; i < num_lines - 1; i++) {
        Dwarf_Line *line = dwarf_onesrcline(lines, i);
        Dwarf_Line *next_line = dwarf_onesrcline(lines, i + 1);
        if (line == NULL || next_line == NULL || dwarf_lineaddr(line, &lineaddr) != 0 ||
            dwarf_lineaddr(next_line, &next_lineaddr) != 0) {
            NOTIFY_DWARF();
            break;
        }
        NOTIFY("%s: pc " PFX " vs line " PFX "-" PFX "\n", __FUNCTION__, (ptr_uint_t)pc,
               (ptr_uint_t)lineaddr, (ptr_uint_t)next_lineaddr);
        if (lineaddr <= pc && pc < next_lineaddr) {
            dw_line = line;
            res = SEARCH_FOUND;
            break;
        }
    }
    /* Handle the case when the PC is from the last line of the CU. */
    if (i == num_lines - 1 && dw_line == NULL && next_lineaddr <= pc) {
        NOTIFY("%s: pc " PFX " vs last line " PFX "\n", __FUNCTION__, (ptr_uint_t)pc,
               (ptr_uint_t)next_lineaddr);
        dw_line = dwarf_onesrcline(lines, num_lines - 1);
        if (dw_line != NULL)
            res = SEARCH_MAYBE;
    }

    /* If we found dw_line, use it to fill out sym_info. */
    if (dw_line != NULL) {
        const char *file;
        int lineno;

        file = dwarf_linesrc(dw_line, NULL, NULL);
        if (file == NULL || dwarf_lineno(dw_line, &lineno) != 0 ||
            dwarf_lineaddr(dw_line, &lineaddr) != 0) {
            NOTIFY_DWARF();
            res = SEARCH_NOT_FOUND;
        } else {
            /* File comes from .debug_str and therefore lives until
             * drsym_exit, but caller has provided space that we must copy into.
             */
            sym_info->file_available_size = strlen(file);
            if (sym_info->file != NULL) {
                strncpy(sym_info->file, file, sym_info->file_size);
                sym_info->file[sym_info->file_size - 1] = '\0';
            }
            sym_info->line = lineno;
            sym_info->line_offs = (size_t)(pc - lineaddr);
        }
    }

    return res;
}

/* Return value: 0 means success but break; 1 means success and continue;
 * -1 means error.
 */
static int
enumerate_lines_in_cu(dwarf_module_t *mod, Dwarf_Die *cu_die,
                      drsym_enumerate_lines_cb callback, void *data)
{
    Dwarf_Lines *lines = NULL;
    size_t num_lines;
    int i;
    drsym_line_info_t info;

    info.cu_name = dwarf_diename(cu_die);
    if (info.cu_name == NULL) {
        /* i#1477: it is possible that a DIE entrie has a NULL name */
        NOTIFY_DWARF();
    }

    num_lines = get_lines_from_cu(mod, cu_die, &lines);
    if (num_lines < 0) {
        /* This cu has no line info.  Don't bail: keep going. */
        info.file = NULL;
        info.line = 0;
        info.line_addr = 0;
        if (!(*callback)(&info, data))
            return 0;
        return 1;
    }

    for (i = 0; i < num_lines; i++) {
        int lineno;
        Dwarf_Addr lineaddr;
        Dwarf_Line *line = dwarf_onesrcline(lines, i);

        /* We do not want to bail on failure of any of these: we want to
         * provide as much information as possible.
         */
        info.file = dwarf_linesrc(line, NULL, NULL);
        if (info.file == NULL)
            NOTIFY_DWARF();

        if (dwarf_lineno(line, &lineno) != 0) {
            NOTIFY_DWARF();
            info.line = 0;
        } else
            info.line = lineno;

        if (dwarf_lineaddr(line, &lineaddr) != 0) {
            NOTIFY_DWARF();
            info.line_addr = 0;
        } else {
            info.line_addr = (size_t)(lineaddr - (Dwarf_Addr)(ptr_uint_t)mod->load_base -
                                      mod->offs_adjust);
        }
        if (!(*callback)(&info, data))
            return 0;
    }

    return 1;
}

drsym_error_t
drsym_dwarf_enumerate_lines(void *mod_in, drsym_enumerate_lines_cb callback, void *data)
{
    drsym_error_t success = DRSYM_SUCCESS;
    dwarf_module_t *mod = (dwarf_module_t *)mod_in;
    Dwarf_Die cu_die;
    Dwarf_Off cu_offset = 0, prev_offset = 0;
    size_t hsize;

    /* Enumerate all CU's */
    while (dwarf_nextcu(mod->dbg, cu_offset, &cu_offset, &hsize, NULL, NULL, NULL) == 0) {
        if (dwarf_offdie(mod->dbg, prev_offset + hsize, &cu_die) != NULL) {
            int res = enumerate_lines_in_cu(mod, &cu_die, callback, data);
            if (res < 0)
                success = DRSYM_ERROR_LINE_NOT_AVAILABLE;
            if (res <= 0)
                break;
        }
        prev_offset = cu_offset;
    }

    while (dwarf_nextcu(mod->dbg, cu_offset, &cu_offset, &hsize, NULL, NULL, NULL) == 0) {
        /* Reset the internal CU header state. */
    }

    return success;
}

void *
drsym_dwarf_init(dwarf_lib_handle_t dbg)
{
    dwarf_module_t *mod = (dwarf_module_t *)dr_global_alloc(sizeof(*mod));
    memset(mod, 0, sizeof(*mod));
    mod->dbg = dbg;
    return mod;
}

void
drsym_dwarf_exit(void *mod_in)
{
    dwarf_module_t *mod = (dwarf_module_t *)mod_in;
    dwarf_end(mod->dbg);
    dr_global_free(mod, sizeof(*mod));
}

void
drsym_dwarf_set_obj_offs(void *mod_in, ssize_t adjust)
{
    dwarf_module_t *mod = (dwarf_module_t *)mod_in;
    mod->offs_adjust = adjust;
}

void
drsym_dwarf_set_load_base(void *mod_in, byte *load_base)
{
    dwarf_module_t *mod = (dwarf_module_t *)mod_in;
    mod->load_base = load_base;
}

#if defined(WINDOWS) && defined(STATIC_LIB)
/* if we build as a static library with "/MT /link /nodefaultlib libcmt.lib",
 * somehow we're missing strdup
 */
char *
strdup(const char *s)
{
    char *res;
    size_t len;
    if (s == NULL)
        return NULL;
    len = strlen(s) + 1;
    res = (char *)malloc(strlen(s) + 1);
    strncpy(res, s, len);
    res[len - 1] = '\0';
    return res;
}
#endif
