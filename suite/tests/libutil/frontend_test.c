/* **********************************************************
 * Copyright (c) 2014-2019 Google, Inc.  All rights reserved.
 * Copyright (c) 2003 VMware, Inc.  All rights reserved.
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

#include <stdio.h>
#include "dr_api.h"
#include "dr_frontend.h"

#define DBGHELP_LIB "dbghelp.dll"

int
main()
{
    bool dir_exists;
    int res;
    res = drfront_create_dir("test_dir");
    if (res != DRFRONT_SUCCESS && res != DRFRONT_EXIST) {
        printf("drfront_create_dir failed \n");
        return -1;
    }
    if (drfront_access("test_dir", DRFRONT_EXIST, &dir_exists) != DRFRONT_SUCCESS ||
        !dir_exists) {
        printf("failed to get access to test_dir\n");
        return -1;
    }

    res = drfront_remove_dir("test_dir");
    if (res != DRFRONT_SUCCESS) {
        printf("drfront_remove_dir failed\n");
        return -1;
    }
#ifdef WINDOWS
    if (drfront_set_verbose(1) != DRFRONT_SUCCESS) {
        printf("drfront_set_verbose failed\n");
        return -1;
    }
    if (drfront_sym_init(NULL, DBGHELP_LIB) != DRFRONT_SUCCESS) {
        printf("drfront_sym_init failed\n");
        return -1;
    }
    if (drfront_sym_exit() != DRFRONT_SUCCESS) {
        printf("drfront_sym_init failed\n");
        return -1;
    }
#endif

    /* XXX i#1488: We need more tests for frontend routines. */
    printf("all done\n");
    return 0;
}
