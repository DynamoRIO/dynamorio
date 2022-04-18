/* **********************************************************
 * Copyright (c) 2013-2020 Google, Inc.  All rights reserved.
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

/* Tests using drsyms from a standalone app */

#include "configure.h"
#include "dr_api.h"
#include "drsyms.h"
#include <assert.h>

static bool
enum_cb(const char *name, size_t modoffs, void *data)
{
    const char *match = (const char *)data;
    if (match != NULL && strstr(name, match) != NULL)
        dr_printf("Found %s\n", name);
    return true; /* keep iterating */
}

int
main(int argc, char *argv[])
{
    drsym_error_t symres;
    void *drcontext = dr_standalone_init();
    int i;
    const char *match = NULL;
    if (drsym_init(IF_WINDOWS_ELSE(NULL, 0)) != DRSYM_SUCCESS) {
        assert(false);
        return 1;
    }

    /* Current design is to pass in paths of libraries to search for symbols */
    for (i = 1; i < argc; i++) {
        if (strstr(argv[i], "libstdc++") != NULL) {
            /* Test i#680: MinGW stripped symbols */
            match = "operator new";
        } else
            match = NULL;
        /* XXX: add more tests */
        symres = drsym_enumerate_symbols(argv[i], enum_cb, (void *)match, DRSYM_DEMANGLE);
        assert(symres == DRSYM_SUCCESS);
    }

    symres = drsym_exit();
    assert(symres == DRSYM_SUCCESS);
    dr_standalone_exit();
    return 0;
}
