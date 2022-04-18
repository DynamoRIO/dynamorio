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

/* DRSyms benchmarking standalone app. */

/* This is a standalone app for benchmarking drsyms.  Currently we just time
 * symbol enumeration of an arbitrary object file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dr_api.h"
#include "drsyms.h"

static char sym_buf[4096];

static int
usage(const char *msg)
{
#ifdef WINDOWS
    /* use a symbol forwarded by DR to ntdll that's not an intrinsic to test
     * duplicate link issues vs libcmt
     */
    if (isdigit(msg[0]))
        sym_buf[0] = '\0'; /* avoid warning about empty statement */
#endif
    if (msg != NULL && msg[0] != '\0') {
        dr_fprintf(STDERR, "%s\n", msg);
    }
    dr_fprintf(STDERR, "usage: bench <modpath>\n");
    return 1;
}

/* The work done in this callback is minimal.  Right now it prints out a
 * sampling of mangled, demangled, and fully demangled names.
 */
static bool
sym_callback(const char *name, size_t modoffs, void *data)
{
    uint64 *count = (uint64 *)data;
    *count += 1;
    if (*count % 50000 == 0) {
        dr_printf("{\"%s\",\n", name);
        memset(sym_buf, 0, sizeof(sym_buf));
        if (drsym_demangle_symbol(sym_buf, sizeof(sym_buf), name, DRSYM_DEMANGLE_FULL) !=
            0) {
            dr_printf(" \"%s\",\n", sym_buf);
        }
        memset(sym_buf, 0, sizeof(sym_buf));
        if (drsym_demangle_symbol(sym_buf, sizeof(sym_buf), name, DRSYM_DEMANGLE) != 0) {
            dr_printf(" \"%s\"},\n", sym_buf);
        }
    }
    return true;
}

static void
enumerate_with_flags(const char *modpath, drsym_flags_t flags)
{
    uint64 start, end, time;
    uint64 sym_count = 0;

    dr_printf("Beginning symbol enumeration\n");
    /* Should use clock_gettime with CLOCK_MONOTONIC instead. */
    start = dr_get_milliseconds();
    drsym_enumerate_symbols(modpath, sym_callback, &sym_count, flags);
    end = dr_get_milliseconds();
    dr_printf("Finished symbol enumeration.\n");

    time = end - start;

    dr_printf("Took %d.%03d seconds.\n", (int)(time / 1000), (int)(time % 1000));
}

int
main(int argc, char **argv)
{
    const char *modpath;
#ifdef WINDOWS
    char full_path[2048];
#endif

    dr_standalone_init();
    drsym_init(0);

    if (argc != 2) {
        return usage(NULL);
    }
    modpath = argv[1];
#ifdef WINDOWS
    /* Work around i#289. */
    if (GetFullPathName(modpath, sizeof(full_path), full_path, NULL) == 0) {
        return usage("GetFullPathName failed.\n");
    }
    modpath = full_path;
#endif
    if (!dr_file_exists(modpath)) {
        return usage("Path does not exist.");
    }

    /* The first enumeration populates dbghelp's symbol cache.  We mostly care
     * about how long the second enumeration takes.
     */
    enumerate_with_flags(modpath, DRSYM_DEFAULT_FLAGS);
    enumerate_with_flags(modpath, DRSYM_DEFAULT_FLAGS);

    drsym_exit();
    dr_standalone_exit();
}
