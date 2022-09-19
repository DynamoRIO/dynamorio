/* **********************************************************
 * Copyright (c) 2012-2022 Google, Inc.    All rights reserved.
 * Copyright (c) 2009 VMware, Inc.  All rights reserved.
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

/*
 * nudgeunix.c: nudges a target Linux process
 *
 * XXX: share code with DRcontrol.c?
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <assert.h>

#include "configure.h"
#include "globals_shared.h"

extern bool
create_nudge_signal_payload(siginfo_t *info OUT, uint action_mask, client_id_t client_id,
                            uint flags, uint64 client_arg);

static const char *usage_str =
    "usage: drnudgeunix [-help] [-v] [-pid <pid>] [-type <type>] [-client <ID> <arg>]\n"
    "       -help              Display this usage information\n"
    "       -v                 Display version information\n"
    "       -pid <pid>         Nudge the process with id <pid>\n"
    "       -client <ID> <arg>\n"
    "                          Nudge the client with ID <ID> in the process specified\n"
    "                          with -pid <pid>, and pass <arg> as an argument to the\n"
    "                          client's nudge callback.  <ID> must be the 8-digit hex\n"
    "                          ID of the target client.  <arg> should be a hex\n"
    "                          literal (0, 1, 3f etc.).\n"
    "       -type <type>\n"
    "                          Send a nudge of type <type> to the process specified\n"
    "                          with -pid <pid>.  Type can be a numeric value or a\n"
    "                          symbolic name.  This argument can be repeated.\n"
    "                          E.g., \"-type reset -type stats\".\n";
static int
usage(void)
{
    fprintf(stderr, "%s", usage_str);
    return 1;
}

int
main(int argc, const char *argv[])
{
    process_id_t target_pid = 0;
    uint action_mask = 0;
    client_id_t client_id = 0;
    uint64 client_arg = 0;
    int i;
    siginfo_t info;
    int arg_offs = 1;
    bool success;

    /* parse command line */
    if (argc <= 1)
        return usage();
    while (arg_offs < argc && argv[arg_offs][0] == '-') {
        if (strcmp(argv[arg_offs], "-help") == 0) {
            return usage();
        } else if (strcmp(argv[arg_offs], "-v") == 0) {
            printf("drnudgeunix version %s -- build %d\n", STRINGIFY(VERSION_NUMBER),
                   BUILD_NUMBER);
            exit(0);
        } else if (strcmp(argv[arg_offs], "-pid") == 0) {
            if (argc <= arg_offs + 1)
                return usage();
            target_pid = strtoul(argv[arg_offs + 1], NULL, 10);
            arg_offs += 2;
        } else if (strcmp(argv[arg_offs], "-client") == 0) {
            if (argc <= arg_offs + 2)
                return usage();
            action_mask |= NUDGE_GENERIC(client);
            client_id = strtoul(argv[arg_offs + 1], NULL, 16);
            client_arg = strtoull(argv[arg_offs + 2], NULL, 16);
            arg_offs += 3;
        } else if (strcmp(argv[arg_offs], "-type") == 0) {
            int type_numeric;
            if (argc <= arg_offs + 1)
                return usage();
            type_numeric = strtoul(argv[arg_offs + 1], NULL, 10);
            action_mask |= type_numeric;

            /* compare against symbolic names */
            {
                int found = 0;
#define NUDGE_DEF(name, comment)                  \
    if (strcmp(#name, argv[arg_offs + 1]) == 0) { \
        found = 1;                                \
        action_mask |= NUDGE_GENERIC(name);       \
    }
                NUDGE_DEFINITIONS();
#undef NUDGE_DEF
                if (!found && type_numeric == 0) {
                    fprintf(stderr, "ERROR: unknown -nudge %s\n", argv[arg_offs + 1]);
                    return usage();
                }
            }

            arg_offs += 2;
        } else
            return usage();
    }
    if (arg_offs < argc)
        return usage();

    /* construct the payload */
    success = create_nudge_signal_payload(&info, action_mask, 0, client_id, client_arg);
    assert(success); /* failure means kernel's sigqueueinfo has changed */

    /* send the nudge */
    i = syscall(SYS_rt_sigqueueinfo, target_pid, NUDGESIG_SIGNUM, &info);
    if (i < 0)
        fprintf(stderr, "nudge FAILED with error %d\n", i);
    return i;
}
