/* **********************************************************
 * Copyright (c) 2014-2015 Google, Inc.  All rights reserved.
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

#include "tools.h"


int
main()
{
    unsigned int buf[10];
    void (*foo)(void) = (void *)buf;

    INIT();
    print("starting up\n");

    /* We use self-modification because we need the code to be sandboxed
     * for good testing.
     */
#if defined(X86)
    /* 64 67 ff 36 00 00 is addr16 push   %fs:0x00[4byte] 
     * 64 for fs
     * 67 for 16-bit address
     * ff 36 for push, using 16-bit memory displacement
     * 00 00 for value of 16-bit memory displacement
     */
    ((unsigned char *)buf)[0] = 0x64;
    ((unsigned char *)buf)[1] = 0x67;
    ((unsigned char *)buf)[2] = 0xff;
    ((unsigned char *)buf)[3] = 0x36;
    ((unsigned char *)buf)[4] = 0x00;
    ((unsigned char *)buf)[5] = 0x00;
     /* 33 c0 is xor eax, eax */
    ((unsigned char *)buf)[6] = 0x33;
    ((unsigned char *)buf)[7] = 0xc0;
    /* 58 is pop eax (to keep a good stack) */
    ((unsigned char *)buf)[8] = 0x58;
    /* c3 is ret */
    ((unsigned char *)buf)[9] = 0xc3;
#endif

    foo();

    print("about to exit\n");

    return 0;
}
