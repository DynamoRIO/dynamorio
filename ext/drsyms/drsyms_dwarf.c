/* **********************************************************
 * Copyright (c) 2011-2012 Google, Inc.  All rights reserved.
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

/* Symbol lookup routines for DWARF */

#include "dr_api.h"
#include "drsyms.h"
#include "drsyms_private.h"

#include "dwarf.h"
#include "libdwarf.h"

#include <stdlib.h> /* qsort */

/* For debugging */
static bool verbose = false;

#define NOTIFY_DWARF(de) do { \
    if (verbose) { \
        dr_fprintf(STDERR, "drsyms: Dwarf error: %s\n", dwarf_errmsg(de)); \
    } \
} while (0)

typedef struct _dwarf_module_t {
    Dwarf_Debug dbg;
    /* we cache the last CU we looked up */
    Dwarf_Die lines_cu;
    Dwarf_Line *lines;
    Dwarf_Signed num_lines;
} dwarf_module_t;

static bool
search_addr2line_in_cu(dwarf_module_t *mod, Dwarf_Addr pc, Dwarf_Die cu_die,
                       drsym_info_t *sym_info INOUT);

/******************************************************************************
 * DWARF parsing code.
 */

/* Find the next DIE matching this tag.  Uses the internal state of dbg to
 * determine where to start searching.
 */
static Dwarf_Die
next_die_matching_tag(Dwarf_Debug dbg, Dwarf_Tag search_tag)
{
    Dwarf_Half tag = 0;
    Dwarf_Die die = NULL;
    Dwarf_Error de = {0};

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

/* Iterate over all the CUs in the module to find the CU containing the given
 * PC.
 */
static Dwarf_Die
find_cu_die_via_iter(Dwarf_Debug dbg, Dwarf_Addr pc)
{
    Dwarf_Die die = NULL;
    Dwarf_Unsigned cu_offset = 0;
    Dwarf_Error de = {0};
    Dwarf_Die cu_die = NULL;

    while (dwarf_next_cu_header(dbg, NULL, NULL, NULL, NULL,
                                &cu_offset, &de) == DW_DLV_OK) {
        /* Scan forward in the tag soup for a CU DIE. */
        die = next_die_matching_tag(dbg, DW_TAG_compile_unit);

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

static Dwarf_Die
find_cu_die(Dwarf_Debug dbg, Dwarf_Addr pc)
{
    Dwarf_Error de = {0};
    Dwarf_Die cu_die = NULL;
    Dwarf_Arange *arlist;
    Dwarf_Signed arcnt;
    Dwarf_Arange ar;
    Dwarf_Off die_offs;
    if (dwarf_get_aranges(dbg, &arlist, &arcnt, &de) != DW_DLV_OK ||
        dwarf_get_arange(arlist, arcnt, pc, &ar, &de) != DW_DLV_OK ||
        dwarf_get_cu_die_offset(ar, &die_offs, &de) != DW_DLV_OK ||
        dwarf_offdie(dbg, die_offs, &cu_die, &de) != DW_DLV_OK) {
        NOTIFY_DWARF(de);
        /* Try to find it by walking all CU's and looking at their lowpc+highpc
         * entries, which should work if each has a single contiguous
         * range.  Note that Cygwin and MinGW gcc don't seen to include
         * lowpc+highpc in their CU's.
         */
        cu_die = find_cu_die_via_iter(dbg, pc);
    }
    return cu_die;
}

static int
compare_lines(const void *a_in, const void *b_in)
{
    const Dwarf_Line a = *(const Dwarf_Line *)a_in;
    const Dwarf_Line b = *(const Dwarf_Line *)b_in;
    Dwarf_Addr addr_a, addr_b;
    Dwarf_Error de = {0};
    if (dwarf_lineaddr(a, &addr_a, &de) != DW_DLV_OK ||
        dwarf_lineaddr(b, &addr_b, &de) != DW_DLV_OK)
        return 0;
    if (addr_a > addr_b)
        return 1;
    if (addr_a < addr_b)
        return -1;
    return 0;
}

/* Given a function DIE and a PC, fill out sym_info with line information.
 */
bool
drsym_dwarf_search_addr2line(void *mod_in, Dwarf_Addr pc, drsym_info_t *sym_info INOUT)
{
    dwarf_module_t *mod = (dwarf_module_t *) mod_in;
    Dwarf_Error de = {0};
    Dwarf_Die cu_die;
    Dwarf_Unsigned cu_offset = 0;
    bool success = false;

    /* On failure, these should be zeroed.
     */
    sym_info->file = NULL;
    sym_info->line = 0;
    sym_info->line_offs = 0;

    /* First try cutting down the search space by finding the CU (i.e., the .c
     * file) that this function belongs to.
     */
    cu_die = find_cu_die(mod->dbg, pc);
    if (cu_die == NULL) {
        NOTIFY("%s: failed to find CU die for "PFX", searching all CUs\n",
               __FUNCTION__, pc);
    } else {
        return search_addr2line_in_cu(mod, pc, cu_die, sym_info);
    }

    /* We failed to find a CU containing this PC.  Some compilers (clang) don't
     * put lo_pc hi_pc attributes on compilation units.  In this case, we
     * iterate all the CUs and dig into the dwarf tag soup for all of them.
     */
    while (dwarf_next_cu_header(mod->dbg, NULL, NULL, NULL, NULL,
                                &cu_offset, &de) == DW_DLV_OK) {
        /* Scan forward in the tag soup for a CU DIE. */
        cu_die = next_die_matching_tag(mod->dbg, DW_TAG_compile_unit);

        /* We found a CU die, now check if it's the one we wanted. */
        if (cu_die != NULL && search_addr2line_in_cu(mod, pc, cu_die, sym_info)) {
            success = true;
            break;
        }
    }

    while (dwarf_next_cu_header(mod->dbg, NULL, NULL, NULL, NULL,
                                &cu_offset, &de) == DW_DLV_OK) {
        /* Reset the internal CU header state. */
    }

    return success;
}

static bool
search_addr2line_in_cu(dwarf_module_t *mod, Dwarf_Addr pc, Dwarf_Die cu_die,
                       drsym_info_t *sym_info INOUT)
{
    Dwarf_Line *lines;
    Dwarf_Signed num_lines;
    int i;
    Dwarf_Addr lineaddr, next_lineaddr = 0;
    Dwarf_Line dw_line;
    bool success = false;
    Dwarf_Error de = {0};

    if (mod->lines_cu == cu_die) {
        lines = mod->lines;
        num_lines = mod->num_lines;
    } else {
        if (dwarf_srclines(cu_die, &lines, &num_lines, &de) != DW_DLV_OK) {
            NOTIFY_DWARF(de);
            return false;
        }
        /* XXX: we should fix libelftc to sort as it builds the table but for now
         * it's easier to sort and store here
         */
        qsort(lines, (size_t)num_lines, sizeof(*lines), compare_lines);
        /* Save for next query */
        dwarf_srclines_dealloc(mod->dbg, mod->lines, mod->num_lines);
        mod->lines_cu = cu_die;
        mod->lines = lines;
        mod->num_lines = num_lines;
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
            sym_info->line_offs = (size_t) (pc - lineaddr);
            success = true;
        }
    }

    return success;
}

void *
drsym_dwarf_init(Dwarf_Debug dbg)
{
    dwarf_module_t *mod = (dwarf_module_t *) dr_global_alloc(sizeof(*mod));
    mod->dbg = dbg;
    mod->lines_cu = NULL;
    mod->lines = NULL;
    return mod;
}

void
drsym_dwarf_exit(void *mod_in)
{
    dwarf_module_t *mod = (dwarf_module_t *) mod_in;
    if (mod->lines != NULL)
        dwarf_srclines_dealloc(mod->dbg, mod->lines, mod->num_lines);
    dwarf_finish(mod->dbg, NULL);
    dr_global_free(mod, sizeof(*mod));
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
    res = (char *) malloc(strlen(s) + 1);
    strncpy(res, s, len);
    res[len - 1] = '\0';
    return res;
}
#endif
