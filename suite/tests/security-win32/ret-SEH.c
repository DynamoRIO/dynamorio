/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
 * Copyright (c) 2005 VMware, Inc.  All rights reserved.
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
#include "tools.h"

/* the VC exception handling data structures that the core expects */
typedef struct scopetable_entry {
    DWORD previousTryLevel;
    PVOID lpfnFilter;
    PVOID lpfnHandler;
} scopetable_entry;

/* regular exception record, referred to in WINNT.H as
 * EXCEPTION_REGISTRATION_RECORD
 */
typedef struct _EXCEPTION_REGISTRATION {
    struct _EXCEPTION_REGISTRATION *prev;
    PVOID handler;
} EXCEPTION_REGISTRATION, *PEXCEPTION_REGISTRATION;

/* The extended exception frame used by Visual C++ */
typedef struct _VC_EXCEPTION_REGISTRATION {
    EXCEPTION_REGISTRATION exception_base;
    struct scopetable_entry *scopetable;
    int trylevel;
    int _ebp;
} VC_EXCEPTION_REGISTRATION;

static scopetable_entry scopes[] = {
    { -1, NULL, NULL },
    { 0, NULL, NULL },
    { 1, NULL, NULL },
};
#define NUM_SCOPE_ENTRIES (sizeof(scopes) / sizeof(scopetable_entry))

static VC_EXCEPTION_REGISTRATION vcex = {
    { NULL, NULL }, (scopetable_entry *)&scopes, 0, 0
};

void
foo(int level)
{
    print("in foo level %d\n", level);
}

void
ret_SEH(int level)
{
    int i;
    PVOID storeme = &scopes[level + 1].lpfnHandler;
    print("ret-SEH test: trylevel %d\n", level);
    vcex.trylevel = level;
    /* ensure DR checks the proper trylevel by clearing the rest */
    for (i = 0; i < NUM_SCOPE_ENTRIES; i++) {
        scopes[level + 1].lpfnHandler = NULL;
    }
    __asm {
        /* cannot refer to asm symbol from C except in a goto, so
         * we do scopes[level+1].lpfnHandler = myhandler from asm
         */
        mov eax, storeme
        mov dword ptr [eax], offset myhandler
            /* now that SEH scopetable is set up, we do our funny call
             * with a push immed of the retaddr immediately prior to
             * our SEH handler, to match the pattern seen on NT4
             */
            /* arg to foo */
        push level
                /* retaddr */
        push offset myretpt
    myhandler:
                    /* some amount of code in here */
        nop
        jmp foo
                        /* make ret point not be the next instr */
        nop
    myretpt:
                            /* clean up arg to foo */
        pop eax
    }
}

/* Disable "Inline asm assigning to 'FS:0' : handler not registered as safe handler" */
#pragma warning(disable : 4733)

int
main(int argc, char *argv[])
{
    int i;
    INIT();

    /* set up SEH */
    __asm {
        push eax
        mov eax, offset vcex
        mov dword ptr fs:[0], eax
        pop eax
    }

    print("ret-SEH test starting\n");
    for (i = 0; i < NUM_SCOPE_ENTRIES; i++) {
        /* levels start at -1 */
        ret_SEH(i - 1);
    }
    print("ret-SEH test stopping\n");
    return 0;
}
