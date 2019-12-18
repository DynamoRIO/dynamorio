/* **********************************************************
 * Copyright (c) 2013 Google, Inc.  All rights reserved.
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

#include "tools.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>

/* Forces the size of the binary to be bigger than 4096 bytes, by ensuring that
 * at least one of the segments is larger than 4096 bytes.
 */
static const char big_array[4097];

/* XXX i#1246: Make this a cross-platform test. */
int
main(int argc, char **argv)
{
    void *mmap_base;
    int fd;

    /* Open up an ELF file (this executable) so that we can mmap it,
     * which will look like the mmaping behavior of a dlopen.
     */
    fd = open(argv[0], O_RDONLY, 0);
    if (fd < 0) {
        return 1;
    }

    print("About to mmap.\n");

    /* Get a read-only, non-anonymous mmap to the ELF file. We care about
     * detecting that this mmap is too small to actually be used for loading
     * the executable. The assumption is that if the mmap is too small then the
     * app is likely using it to read the ELF header, or other parts of the ELF.
     *
     * XREF i#1240
     */
    mmap_base = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0);
    if (mmap_base == MAP_FAILED) {
        return 1;
    }

    /* Tell the compiler not to optimize away the existence of big_array */
    __asm__ __volatile__("" ::"m"(big_array[0]));
    (void)big_array;

    print("Done mmaping.\n");
    return 0;
}
