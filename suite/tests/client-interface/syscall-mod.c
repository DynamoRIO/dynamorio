/* **********************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
 * Copyright (c) 2008-2010 VMware, Inc.  All rights reserved.
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

#include "configure.h"

#include <stdio.h>
#if defined(MACOS) || defined(ANDROID)
#    include <sys/syscall.h>
#else
#    include <syscall.h>
#endif

#define EXPANDSTR(x) #x
#define STRINGIFY(x) EXPANDSTR(x)

int
main()
{
    int pid;
    fprintf(stderr, "starting\n");
#if defined(AARCH64)
    asm("movz x8, " STRINGIFY(SYS_getpid) ";"
                                          "svc 0;"
                                          "mov %w0, w0"
        : "=r"(pid)
        :
        : "cc", "w0");
#elif defined(X64)
    /* we don't want vsyscall since we rely on mov immed, eax being in same bb.
     * plus, libc getpid might cache the pid value.
     */
    asm("mov $" STRINGIFY(SYS_getpid) ", %%eax;"
                                      "syscall;"
                                      "mov %%eax, %0"
        : "=m"(pid)
        :
        : "cc", "rcx", "r11", "rax");
#else
    asm("mov $" STRINGIFY(SYS_getpid) ", %%eax;"
                                      "int $0x80;"
                                      "mov %%eax, %0"
        : "=m"(pid)
        :
        : "cc", "eax");
#endif
    fprintf(stderr, "pid = %d\n", pid);

    return 0;
}
