/* **********************************************************
 * Copyright (c) 2004 VMware, Inc.  All rights reserved.
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
#include <stdio.h>

int
main(int argc, char **argv)
{
    int trip_count;
    int syscall_count = 0;
    int i;
    int tid = GetCurrentThreadId();
    void *addr;
    int old_flags;
    int prot_mask = PAGE_EXECUTE;
    int prot_flags = PAGE_EXECUTE_READ;
    BOOL double_syscall = FALSE;

    if (!(argc == 2 || argc == 3) || (argc == 3 && strcmp(argv[2], "-double_syscall"))) {
        fprintf(stderr, "Usage: %s <loop trip count> [-double-syscall]\n", argv[0]);
        return -1;
    }
    if (argc == 3 && !strcmp(argv[2], "-double_syscall"))
        double_syscall = TRUE;
    trip_count = atoi(argv[1]);
    addr = VirtualAlloc(NULL, 65536, MEM_RESERVE, prot_flags);
    for (i = 0; i < trip_count; i++, syscall_count++, prot_flags ^= prot_mask) {
        VirtualProtect(addr, 65536, prot_flags, &old_flags);
        if (double_syscall) {
            VirtualProtect(addr, 65536, prot_flags, &old_flags);
            syscall_count++;
            prot_flags ^= prot_mask;
        }
    }
    VirtualFree(addr, 0, MEM_RELEASE);
    fprintf(stderr, "Loop trips completed -- %d, syscalls completed -- %d\n", i,
            syscall_count);
    return 0;
}
