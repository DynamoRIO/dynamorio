/* **********************************************************
 * Copyright (c) 2014-2019 Google, Inc.  All rights reserved.
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

#include "share.h"
#include "elm.h"

#include <stdio.h>
#include <time.h>

int count = 0;
int last = -1;

void
my_elm_formatted_cb(unsigned int mID, unsigned int type, WCHAR *message, DWORD timestamp)
{
    char *type_s;
    count++;

    switch (type) {
    case 0x1: type_s = "ERROR"; break;
    case 0x2: type_s = "WARNING"; break;
    case 0x4: type_s = "INFO"; break;
    default: type_s = "<unknown>"; break;
    }
    printf("Record %d, type=%s, %s%S\n", mID, type_s, ctime((long *)&timestamp), message);
}

void
my_elm_err_cb(unsigned int errcode, WCHAR *msg)
{
    if (errcode == ELM_ERR_FATAL) {
        fprintf(stderr, "elm FATAL error: %S\n", msg);
        exit(1);
    } else if (errcode == ELM_ERR_WARN) {
        fprintf(stderr, "elm warning: %S\n", msg);
        fflush(stderr);
    }
}

extern BOOL do_once;

void
usage()
{
    fprintf(stderr, "Usage: dumpevts [-clear | -start N]\n");
    fprintf(stderr, "   With no args, dumps full eventlog; use -start N to\n");
    fprintf(stderr, "     dump starting with record N.\n");
    exit(-1);
}

int
main(int argc, char **argv)
{
    DWORD res;

    if (argc > 1) {
        if (0 == strcmp(argv[1], "-clear")) {
            res = clear_eventlog();
            if (res != ERROR_SUCCESS)
                fprintf(stderr, "Error %d clearing Event Log!\n", res);
            else
                printf("Eventlog cleared.\n");
            return 0;
        } else if (0 == strcmp(argv[1], "-start")) {
            if (argc > 2) {
                last = atoi(argv[2]);
            } else {
                usage();
            }
        } else {
            usage();
        }
    }

    do_once = TRUE;
    res = start_eventlog_monitor(TRUE, &my_elm_formatted_cb, NULL, &my_elm_err_cb, last);

    if (res != ERROR_SUCCESS) {
        printf("error %d starting monitor\n", res);
    }

    WaitForSingleObject(get_elm_thread_handle(), INFINITE);

    if (count == 0)
        printf("No Events found.\n");

    return 0;
}
