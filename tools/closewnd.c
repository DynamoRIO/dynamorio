/* **********************************************************
 * Copyright (c) 2019 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2010 VMware, Inc.  All rights reserved.
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

#include "Windows.h"
#include "stdio.h"

/* make this pretty long, some regression runs are pretty slow and we just
 * want to keep the suite from completely hanging */
#define TIMEOUT_MS 60000

#define SLEEP_INTERVAL_MS 500

int
main(int argc, char **argv)
{
    HANDLE hw;
    LRESULT res;
    UINT wait_tot, wait_max;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <window caption> <timeout (sec)>\n", argv[0]);
        exit(1);
    }

    wait_tot = 0;
    wait_max = atoi(argv[2]) * 1000;

    while (wait_tot < wait_max) {
        hw = FindWindow(NULL, argv[1]);
        if (hw == NULL) {
            wait_tot += SLEEP_INTERVAL_MS;
            Sleep(SLEEP_INTERVAL_MS);
        } else {
            res = SendMessageTimeout(hw, WM_CLOSE, 0, 0, SMTO_NORMAL, TIMEOUT_MS, NULL);
            printf("Close message sent.\n");
            if (res == 0) {
                /* error case */
                res = GetLastError();
                /* msdn lies! claims GetLastError returns 0 for timeout case, yet
                 * it in fact returns ERROR_TIMEOUT (no surprise) just check for
                 * both, not too suprising, I suppose, as I think they have some
                 * typos in the flag descriptions for the function also */
                if (res == 0 || res == ERROR_TIMEOUT) {
                    printf("Window timed out without response\n");
                } else {
                    printf("Error sending close message %zd\n", res);
                }
            }

            break;
        }
    }
}
