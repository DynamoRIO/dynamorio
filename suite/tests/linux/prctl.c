/* **********************************************************
 * Copyright (c) 2012 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Test that prctl(PR_GET_NAME) gives the right string.  This is what killall
 * and ps -C use to identify processes.
 */

#include <assert.h>
#include <string.h>
#include <sys/prctl.h>

#include "tools.h"

/* Max name length according to the manpage. */
enum { PR_NAME_LENGTH = 16 };

int
main(int argc, char **argv)
{
    char *cur;
    char prctl_name[PR_NAME_LENGTH + 1];

    /* Check that argv[0] matches. */
    assert(argc > 0);
    cur = strrchr(argv[0], '/');
    if (cur == NULL) {
        cur = argv[0];
    } else {
        cur++;
    }
    print("basename argv[0]: %s\n", cur);

    /* Check that PR_GET_NAME matches. */
    prctl(PR_GET_NAME, (unsigned long)prctl_name, 0, 0, 0);
    prctl_name[PR_NAME_LENGTH] = '\0';
    print("PR_GET_NAME: %s\n", prctl_name);

    /* Set it and get it. */
    strncpy(prctl_name, "set_prctl", PR_NAME_LENGTH);
    prctl_name[PR_NAME_LENGTH] = '\0';
    prctl(PR_SET_NAME, (unsigned long)prctl_name, 0, 0, 0);
    memset(prctl_name, 0, sizeof(prctl_name));
    prctl(PR_GET_NAME, (unsigned long)prctl_name, 0, 0, 0);
    prctl_name[PR_NAME_LENGTH] = '\0';
    print("after PR_SET_NAME: %s\n", prctl_name);

    print("all done\n");

    return 0;
}
