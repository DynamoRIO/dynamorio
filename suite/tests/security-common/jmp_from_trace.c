/* **********************************************************
 * Copyright (c) 2006-2008 VMware, Inc.  All rights reserved.
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

/* Generates a .B from a trace.
 * FIXME: need to put in a template
 * FIXME: need to make native work (exception handling stuff)
 * FIXME: need to make it work with -detect_mode
 */

#define NEED_HANDLER 1
#include "tools.h"

#define NUM_TIMES 100
#define SWITCH_AFTER 50

/* totally random dummy function */
int
dummycall()
{
    int i = 1, j = 2;

    if ((i += 10) < (j -= 50))
        return i + j - 32;
    else
        return i - j + 32;
}

int badtarget = 10; /* .B */
int
bad_trace4()
{
    int i, j;
    int a = 1, b = 100, c = 1000;
    int (*fnptr)();
    fnptr = dummycall;
    for (i = 0; i < NUM_TIMES; i++) {
        for (j = 0; j < NUM_TIMES; j++) {
            if (a < b) {
                a++;
                c--;
                fnptr();
            } else {
                a -= 5;
                c--;
                fnptr();
            }
            if (c < SWITCH_AFTER) {
                fnptr = (int (*)()) & badtarget;
                print("Next time around jump to data section\n");
            }
        }
    }
    return 0;
}

int
main()
{
    INIT();
    print("Start\n");
    bad_trace4();
    print("SHOULD NEVER GET HERE\n");
}
