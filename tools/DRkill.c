/* **********************************************************
 * Copyright (c) 2003-2006 VMware, Inc.  All rights reserved.
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
 * DRkill.c
 *
 * 'kill' that works on win32
 *
 * to build:
 *  cl DRkill.c registry.c advapi32.lib /o DRkill.exe
 *
 * jbrink, 8/03
 *
 * (c) araksha, inc. all rights reserved
 */

#include "share.h"
#include "processes.h"

#include <stdio.h>

void
procwalk();

void
usage()
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "DRkill [-help] [-quiet] [-pid n] [-exe name] [-underdr] [-v]\n");
    exit(1);
}

void
help()
{
    fprintf(stderr, "Options:\n");
    fprintf(stderr, " -quiet n\t\t\tdon't report what is killed\n");
    fprintf(stderr, " -pid n\t\t\tkills process with pid 'n'\n");
    fprintf(stderr, " -exe name\t\tkills all processes whose executable matches\n");
    fprintf(stderr, "\t\t\t'name'\n");
    fprintf(stderr, " -underdr\t\tkills all processes running under SC\n");
    fprintf(stderr, " -v\t\t\tdisplay version information\n\n");

    exit(1);
}

BOOL underdr = FALSE, killquietly = FALSE;
uint pid = 0;
WCHAR exe[MAX_PATH];

int
main(int argc, char **argv)
{
    int argidx = 1;
    exe[0] = 0;
    if (argc < 2)
        usage();

    while (argidx < argc) {

        if (!strcmp(argv[argidx], "-help")) {
            help();
        } else if (!strcmp(argv[argidx], "-quiet")) {
            killquietly = TRUE;
        } else if (!strcmp(argv[argidx], "-pid")) {
            pid = atoi(argv[++argidx]);
        } else if (!strcmp(argv[argidx], "-exe")) {
            _snwprintf(exe, MAX_PATH, L"%S", argv[++argidx]);
        } else if (!strcmp(argv[argidx], "-underdr")) {
            underdr = TRUE;
        } else if (!strcmp(argv[argidx], "-v")) {
#ifdef BUILD_NUMBER
            printf("DRkill.exe build %d -- %s\n", BUILD_NUMBER, __DATE__);
#else
            printf("DRkill.exe custom build -- %s, %s\n", __DATE__, __TIME__);
#endif
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[argidx]);
            usage();
        }
        argidx++;
    }

    if (pid) {
        if (!killquietly)
            printf("killing process %d\n", pid);

        terminate_process(pid);
    } else {
        procwalk();
    }
}

BOOL
pw_callback(process_info_t *pi, void **param)
{
    int res;

    if ((pid != 0 && pi->ProcessID == pid) ||
        (exe[0] != '\0' && !wcsicmp(exe, pi->ProcessName)) || underdr) {

        res = under_dynamorio(pi->ProcessID);

        if (exe[0] != '\0' || pid != 0 ||
            (underdr && (res != DLL_NONE && res != DLL_UNKNOWN))) {

            if (!killquietly)
                printf("killing process %d=%S\n", pi->ProcessID, pi->ProcessName);

            terminate_process(pi->ProcessID);
        }
    }
    return TRUE;
}

void
procwalk()
{
    process_walk(&pw_callback, NULL);
}
