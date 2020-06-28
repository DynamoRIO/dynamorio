/* **********************************************************
 * Copyright (c) 2020 Google, Inc.  All rights reserved.
 * Copyright (c) 2006 VMware, Inc.  All rights reserved.
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

/* FIXME - should use dr_fprintf but can't get that to work, then again this is
 * standalone so doesn't really matter what we use. */

#include "dynamorio.h"
#include "dynamorio_mod.h"
#include "windows.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

int
main(int argc, char *argv[])
{
    version_info_t info;
    HMODULE hmod;

    void *drcontext = dr_standalone_init();
    if (argc != 2) {
        printf("Usage: %s <dll to read>\n", argv[0]);
        return 1;
    }

    /* Don't use LOAD_LIBRARY_AS_DATAFILE, it doesn't layout the sections as we expect
     * use DONT_RESOLVE_DLL_REFERENCES instead */
    hmod = LoadLibraryEx(argv[1], NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (hmod == NULL) {
        printf("Can't find module %s\n", argv[1]);
        return 2;
    }

    /* Need to round off the loaded as data flag to get actual base */
    if (!get_module_resource_version_info((app_pc)((uint)hmod & 0xfffffffe), &info)) {
        printf("Failed to read rsrc directory\n");
        return 3;
    }

    printf("File Version = %04d.%04d.%04d.%04d, Product Version = %04d.%04d.%04d.%04d\n",
           info.file_version.version_parts.p1, info.file_version.version_parts.p2,
           info.file_version.version_parts.p3, info.file_version.version_parts.p4,
           info.product_version.version_parts.p1, info.product_version.version_parts.p2,
           info.product_version.version_parts.p3, info.product_version.version_parts.p4);
    if (info.original_filename != NULL)
        printf("Original File Name = \"%S\"\n", info.original_filename);
    if (info.company_name != NULL)
        printf("Company Name = \"%S\"\n", info.company_name);
    if (info.product_name != NULL)
        printf("Product Name = \"%S\"\n", info.product_name);

    dr_standalone_exit();
    return 0;
}
