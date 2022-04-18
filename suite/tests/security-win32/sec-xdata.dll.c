/* **********************************************************
 * Copyright (c) 2007 VMware, Inc.  All rights reserved.
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

#define VERBOSE 0

#if 0
typedef struct _code_buf {
    char code_buffer[1024];
} code_buf;
code_buf good_code_buffer = {0};
#endif

/* a good xdata section */
#pragma comment(linker, "/SECTION:.xdata,ERW")
#pragma data_seg(".xdata")
/* NOTE - compilers are stupid, without the "= {0}" this will be put in the regular
 * .data section and now .xdata section will be made. wtf??? */
char good_xdata_buf[1024] = { 0 };
#pragma data_seg()

/* our Makefile expects a .lib */
int __declspec(dllexport) make_a_lib(int arg)
{
    return 1;
}

BOOL APIENTRY
DllMain(HANDLE hModule, DWORD reason_for_call, LPVOID Reserved)
{
    char *code;
    switch (reason_for_call) {
    case DLL_PROCESS_ATTACH:
        code = copy_to_buf(good_xdata_buf, sizeof(good_xdata_buf), NULL, CODE_INC,
                           COPY_NORMAL);
        test_print(code, 0);
        break;
    }
    return TRUE;
}
