/* **********************************************************
 * Copyright (c) 2003-2004 VMware, Inc.  All rights reserved.
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
precious()
{
#ifdef USER32    /* map user32.dll for a RunAll test */
    MessageBeep(0);
#endif
    print("M-m-my PRECIOUS is stolen! ATTACK SUCCESSFUL!\n");
    exit(1);
}

int
ring(int num)
{
    print("looking at ring\n");
    *(int*) (&num - 1) = (int)&precious;
    return num;
}

int
twofoo()
{
    int a = foo();
    print("first foo a=%d\n", a);

    a += foo();
    print("second foo a=%d\n", a);
    return a;
}

int
foo()
{
    print("in foo\n");
    return 1;
}

int
bar()
{
    print("in bar\n");
    return 3;
}

int
main()
{
    INIT();

    print("starting good function\n");
    twofoo();
    print("starting bad function\n");
    ring(1);
    print("all done [not seen]\n");
}
