/* **********************************************************
 * Copyright (c) 2014-2019 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2008 VMware, Inc.  All rights reserved.
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

#define _GNU_SOURCE 1 /* for mremap */
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include "tools.h"

int
main()
{
    const size_t size = 0x00008765;
    size_t newsize = size + 0x100; /* these values of size & newsize work for
                                    * the mremap() on p3rh9 w/the 2.4.21-4
                                    * kernel
                                    */
    void *p;
    print("Calling mmap(0, " PFX ", " PFX ", " PFX ", " PFX ", 0)\n", size,
          PROT_EXEC | PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1);
    p = mmap(0, size, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (p == MAP_FAILED) {
        print("mmap ERROR " PFX "\n", p);
        return 1;
    }
#if VERBOSE
    print("mmap returned " PFX "\n", p);
#endif
#ifndef MACOS
    p = (void *)mremap(p, size, newsize, 0);
    if ((ptr_int_t)p == -1) {
        print("mremap ERROR " PFX "\n", p);
        return 1;
    }
#    if VERBOSE
    print("mremap returned " PFX "\n", p);
#    endif
#endif
    munmap(p, newsize);

    /* Test some transition sequences that have been problematic in the past
     * b/c they look like ld.so's ELF loading.
     */
    p = mmap(0, newsize, PROT_READ, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (p == MAP_FAILED) {
        print("mmap ERROR " PFX "\n", p);
        return 1;
    }
    p = mmap(p, size, PROT_READ | PROT_EXEC, MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0);
    if (p == MAP_FAILED) {
        print("mmap ERROR " PFX "\n", p);
        return 1;
    }
    munmap(p, newsize);

    p = mmap(0, newsize, PROT_READ | PROT_EXEC, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (p == MAP_FAILED) {
        print("mmap ERROR " PFX "\n", p);
        return 1;
    }
    p = mmap(p, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0);
    if (p == MAP_FAILED) {
        print("mmap ERROR " PFX "\n", p);
        return 1;
    }
    munmap(p, newsize);

    return 0;
}
