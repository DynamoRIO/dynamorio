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

/* DRSyms benchmarking standalone app. */

/* This is a standalone app for benchmarking drsyms.  Currently we just time
 * symbol enumeration of an arbitrary object file.
 */

#include <stdio.h>
#include <stdlib.h>

#include "dr_api.h"
#include "drsyms.h"

static int
usage(const char *msg)
{
    if (msg != NULL && msg[0] != '\0') {
        dr_fprintf(STDERR, "%s\n", msg);
    }
    dr_fprintf(STDERR, "usage: bench <modpath>\n");
    return 1;
}

static bool
sym_callback(const char *name, size_t modoffs, void *data)
{
    uint64 *count = (uint64*)data;
    *count += 1;
    if (*count % 50000 == 0) {
        dr_printf("sym: %s\n", name);
    }
    return true;
}

int
main(int argc, char **argv)
{
    uint64 start, end, time;
    const char *modpath;
    uint64 sym_count = 0;

    dr_app_setup();
    drsym_init(0);

    if (argc != 2) {
        return usage(NULL);
    }
    modpath = argv[1];
    if (!dr_file_exists(modpath)) {
        return usage("Path does not exist.");
    }

    dr_printf("Beginning symbol enumeration.\n");
    /* Should use clock_gettime with CLOCK_MONOTONIC instead. */
    /* XXX: Once we have a flag for toggling demangling, we can time both. */
    start = dr_get_milliseconds();
    drsym_enumerate_symbols(modpath, sym_callback, &sym_count);
    end = dr_get_milliseconds();
    dr_printf("Finished symbol enumeration.\n");

    time = end - start;

    dr_printf("Took %d.%03d seconds.\n", (int)(time / 1000), (int)(time % 1000));

    drsym_exit();
    dr_app_cleanup();
}
