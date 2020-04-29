/* **********************************************************
 * Copyright (c) 2019 Google, Inc.  All rights reserved.
 * Copyright (c) 2001-2010 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2001 Hewlett-Packard Company */

/* support for looking up function containing a pc
 * for pc-based profiling
 * based on code from Giuseppe Desoli
 * modified by bruening, Jan 2001
 * need to add to build command "-lbfd -liberty"
 * FIXME: assumes ELF executable compiled w/ -static
 */

#include <errno.h>
#include <bfd.h>
/* globals.h gives us stdio, stdlib, and assert */
#include "../globals.h"

static uint bfd_symcount, nonnull_symcount;
static asymbol **bfd_syms = NULL;
static bfd *infile = (bfd *)NULL;

static int
compare_symbols(const void *ap, const void *bp)
{
    const asymbol *a = *(const asymbol **)ap;
    const asymbol *b = *(const asymbol **)bp;

    /* check for null pointers -- these are discarded syms   */
    /* they end up at the end of the sort                    */
    if (a == NULL)
        return 1;
    if (b == NULL)
        return -1;

    if (bfd_asymbol_value(a) > bfd_asymbol_value(b))
        return 1;
    if (bfd_asymbol_value(a) < bfd_asymbol_value(b))
        return -1;

    return strcmp(a->name, b->name);
}

/* sort the symbol table by section and by offset from the start of the section */
static void
sort_symtab()
{
    long i;

    qsort(bfd_syms, bfd_symcount, sizeof(asymbol *), compare_symbols);

    if (d_r_stats->loglevel > 2) {
        LOG(GLOBAL, LOG_ALL, 3, "\n\nSYMBOL TABLE\n");
        for (i = 0; i < bfd_symcount; i++) {
            if (bfd_syms[i]) {
                LOG(GLOBAL, LOG_ALL, 3, "[%5d] " PFX " 0x%x %5s %s\n", i,
                    bfd_asymbol_value(bfd_syms[i]), bfd_syms[i]->flags,
                    bfd_syms[i]->section->name, bfd_syms[i]->name);
            } else {
                LOG(GLOBAL, LOG_ALL, 3, "(null symbol)\n");
            }
        }
        LOG(GLOBAL, LOG_ALL, 3, "\n\n");
    }
}

int
lookup_symbol_address(ptr_uint_t addr)
{
    uint low_idx, high_idx, middle_idx;
    uint middle_addr;

    /* the idea is to use a binary search on the sorted symbols table */
    /* the table is sorted by sections and addresses                  */

    low_idx = 0;
    high_idx = nonnull_symcount;

    /* first try to find the two symbols containing it */
    for (;;) {
        middle_idx = low_idx + ((high_idx - low_idx) >> 1);
        if (middle_idx == low_idx)
            break;

        /* we need real addresses so use bfd macros for the value here */
        /* basically it's the symbols' value + their sections' vma     */

        middle_addr = bfd_asymbol_value(bfd_syms[middle_idx]);

        if (middle_addr > addr)
            high_idx = middle_idx;
        else if (middle_addr < addr)
            low_idx = middle_idx;
        else
            break;
    }
    return middle_idx;
}

/* sets to null uninteresting symbols, sets nonnull_symcount to the number of
 * nonnull symbols left
 */
void
prepare_symtab()
{
    int i;
    nonnull_symcount = bfd_symcount;
    for (i = 0; i < bfd_symcount; i++) {
        if (bfd_syms[i]) {
            /* FIXME: the BSF_FUNCTION flag is only for ELF
             * other ideas: remove all non-text-section symbols
             */
            if (!(bfd_syms[i]->flags & BSF_FUNCTION)) {
                /* remove from table by marking as null */
                bfd_syms[i] = NULL;
                nonnull_symcount--;
            }
        }
    }
}

static asymbol **
get_symtab(bfd *abfd)
{
    asymbol **sy = (asymbol **)NULL;
    long storage;

    if (!(bfd_get_file_flags(abfd) & HAS_SYMS)) {
        print_file(STDERR, "No symbols in \"%s\".\n", bfd_get_filename(abfd));
        bfd_symcount = 0;
        return NULL;
    }

    storage = bfd_get_symtab_upper_bound(abfd);
    if (storage < 0) {
        print_file(STDERR, "BFD fatal error bfd_get_symtab_upper_bound\n");
        return NULL;
    }

    if (storage) {
        sy = (asymbol **)xmalloc(storage);
    }
    bfd_symcount = bfd_canonicalize_symtab(abfd, sy);
    if (bfd_symcount < 0) {
        print_file(STDERR, "BFD fatal error bfd_canonicalize_symtab\n");
        return NULL;
    }
    if (bfd_symcount == 0)
        print_file(STDERR, "%s: No symbols\n", bfd_get_filename(abfd));
    return sy;
}

/* this is currently called by pcprofile_init(), so nobody else can use
 * symtab stuff until that is changed
 */
bool
symtab_init()
{
    char *filein = dynamo_options.profexecname;
    if (filein == NULL) {
        return false;
    }

    infile = bfd_openr(filein, NULL);
    if (infile == NULL) {
        USAGE_ERROR("can't find file \"%s\" named in -profexecname", filein);
        return false;
    }

    /* check the bfd format, this is required */
    if (!bfd_check_format(infile, bfd_object)) {
        bfd_close(infile);
        USAGE_ERROR("-profexecname \"%s\" : not a bfd_object!", filein);
        return false;
    }

    if ((bfd_syms = get_symtab(infile)) == NULL) {
        if (bfd_syms) {
            free(bfd_syms);
        }
        if (infile)
            bfd_close(infile);
        USAGE_ERROR("-profexecname \"%s\" : error getting symbol table", filein);
        return false;
    }

    prepare_symtab();
    sort_symtab();
    LOG(GLOBAL, LOG_ALL, 1, "SYMBOL TABLE FOR %s SUCCESSFULLY LOADED\n\n", filein);
    return true;
}

void
symtab_exit()
{
    bfd_close(infile);
}

const char *
symtab_lookup_pc(void *pc)
{
    int idx = lookup_symbol_address((ptr_uint_t)pc);
    if (bfd_syms[idx] == NULL)
        return "(null)";
    else {
        /* FIXME: try to find line number? */
        return bfd_syms[idx]->name;
    }
}
