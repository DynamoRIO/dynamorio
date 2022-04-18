/* **********************************************************
 * Copyright (c) 2014-2019 Google, Inc.  All rights reserved.
 * Copyright (c) 2004-2006 VMware, Inc.  All rights reserved.
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

#include <windows.h>
#include <assert.h>
#include <stdio.h>

#include "share.h"

int
usage(char *us)
{
    fprintf(stderr, "\
Usage: %s\n\
balloon -p <PID> [-v <MB or KB to reserve>] [-c <MB or KB to commit>] [-f] [-t]\n\
        -f frees memory after allocation\n\
        -t grabs memory top down\n\
        -kb uses KB instead of MB as allocation unit\n\
        -r repeat until failure\n\
        -w wait\n\
    Note that reserved and committed memory are in separate regions.\n\
    Defaults are -p current -v 256MB -c 0MB\n",
            us);
    return 0;
    // To check values see $ ./DRview.exe -p 416 -showmem | tail -1 | awk {'print "virtual
    // peak " $9'}
}

#ifdef X64
enum { LAST_STATUS_VALUE_OFFSET = 0x1250 };
#else
enum { LAST_STATUS_VALUE_OFFSET = 0xbf4 }; /* on Win2000+ case 6789 */
#endif

int
get_last_status()
{
    /* get_own_teb()->LastStatusValue */
#ifdef X64
    return __readgsdword(LAST_STATUS_VALUE_OFFSET);
#else
    return __readfsdword(LAST_STATUS_VALUE_OFFSET);
#endif
}

int
main(int argc, char *argv[])
{
    int result;
    int vsize = 256;
    int csize = 0;
    LPVOID *pv = 0;
    LPVOID *pc = 0;

    int argidx = 1;
    int pid = 0;
    HANDLE phandle;
    int topdown = 0;
    int protection = PAGE_NOACCESS;

    int free_them = 0; /* just probe the process, free them after you're done */
    int allocation_unit = 1024 * 1024; /* default in MB */
    int repeat = 0;
    int fail = 0;
    int wait = 0;

    if (argc < 2)
        usage(argv[0]);

    while (argidx < argc) {

        if (!strcmp(argv[argidx], "-help")) {
            usage(argv[0]);
            exit(0);
        } else if (!strcmp(argv[argidx], "-p")) {
            pid = atoi(argv[++argidx]);
        } else if (!strcmp(argv[argidx], "-c")) {
            csize = atoi(argv[++argidx]);
        } else if (!strcmp(argv[argidx], "-v")) {
            if (++argidx >= argc) {
                usage(argv[0]);
                exit(0);
            }
            vsize = atoi(argv[argidx]);
        } else if (!strcmp(argv[argidx], "-t")) {
            topdown = MEM_TOP_DOWN;
        } else if (!strcmp(argv[argidx], "-f")) {
            free_them = 1;
        } else if (!strcmp(argv[argidx], "-kb")) {
            allocation_unit = 1024;
        } else if (!strcmp(argv[argidx], "-w")) {
            wait = 1;
        } else if (!strcmp(argv[argidx], "-r")) {
            /* if last undefinitely */
            if (++argidx >= argc) {
                repeat = 1000000;
            } else {
                repeat = atoi(argv[argidx]);
            }
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[argidx]);
            usage(argv[0]);
            exit(0);
        }
        argidx++;
    }

    if (pid) {
        acquire_privileges();
        phandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        /* FIXME: we need PROCESS_VM_OPERATION access to the process */
        /* should grab permissions */
        assert(phandle);
        release_privileges();
    } else
        phandle = GetCurrentProcess();

    do {
        if (vsize) {
            int alloc = vsize * allocation_unit;
            int flags = MEM_RESERVE | topdown;
            pv = VirtualAllocEx(phandle, NULL, alloc, flags, protection);
            printf("%s %08x virtual bytes == %d%s flags=0x%08x prot=0x%08x\n"
                   "  res = " PIFX " %s %d\n",
                   pv ? "Just reserved" : "Failed to reserve", alloc, vsize,
                   allocation_unit == 1024 ? "KB" : "MB", flags, protection,
                   pv ? (ptr_int_t)pv : (ptr_int_t)get_last_status(),
                   pc ? "success:" : "GLE", GetLastError());
            if (pv == NULL)
                fail = 1;
        }

        if (csize) {
            int alloc = csize * allocation_unit;
            int flags = MEM_RESERVE | MEM_COMMIT | topdown;
            pc = VirtualAllocEx(phandle, NULL, alloc, flags, protection);
            printf("%s 0x%08x bytes == %d%s flags=0x%08x prot=0x%08x\n"
                   "  res = : " PIFX ", %s %x\n",
                   pc ? "Just committed" : "Failed to commit", alloc, csize,
                   allocation_unit == 1024 ? "KB" : "MB", flags, protection,
                   pc ? (ptr_int_t)pc : (ptr_int_t)get_last_status(),
                   pc ? "success:" : "GLE", GetLastError());
            if (pc == NULL)
                fail = 1;
        }

        fflush(stdout);
        if (free_them) {
            if (wait) {
                printf("Press <enter> to free allocations...\n");
                fflush(stdout);
                getchar();
            }
            if (pc) {
                result = VirtualFreeEx(phandle, pc, 0, MEM_RELEASE);
                assert(result);
            }
            if (pv) {
                result = VirtualFreeEx(phandle, pv, 0, MEM_RELEASE);
                assert(result);
            }

            printf("Just freed those bytes\n");
        }
        fflush(stdout);
        --repeat;
        if (repeat > 0)
            printf("Repeating until failure %d\n", repeat);
    } while (repeat > 0 && !fail);

    if (wait) {
        printf("Press <enter> to exit...\n");
        fflush(stdout);
        getchar();
    }

    return 0;
}
