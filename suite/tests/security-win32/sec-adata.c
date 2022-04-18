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

/* an rwx code section that will have rct violations from emitted code */
#pragma comment(linker, "/SECTION:.adata,ERW")
#pragma code_seg(".adata")
int
expendable_function()
{
    return 1;
}
#pragma code_seg()

/* a normal code section that will have rct violations for control */
#pragma comment(linker, "/SECTION:.acode,ER")
#pragma code_seg(".acode")
int
expendable_function2()
{
    return 1;
}
#pragma code_seg()

static char *dll_name = "sec-xdata.dll.dll";

byte *
get_buf_target(byte *func)
{
    byte *buf = func;
    /* handle a jmp link */
    if (*func == 0xe9)
        buf = func + (*(uint *)(func + 1)) + 5;
    return buf + 0x10; /* arbitrary, we need to get away from the reloc */
}

int
main()
{
    byte *code;
    byte *adata_buf = (byte *)expendable_function;
    byte *acode_buf = (byte *)expendable_function2;
    int old_prot;
    INIT();
    adata_buf = get_buf_target(adata_buf);
    acode_buf = get_buf_target(acode_buf);

    print("starting good adata test\n");
    code = copy_to_buf(adata_buf, PAGE_SIZE /* section will be at least page size */,
                       NULL, CODE_INC, COPY_NORMAL);
    test_print(code, 0);

    print("starting bad acode test\n");
    VirtualProtect(acode_buf, 1024, PAGE_EXECUTE_READWRITE, &old_prot);
    code = copy_to_buf(acode_buf, 1024 /* section will be at least page size */, NULL,
                       CODE_INC, COPY_NORMAL);
    VirtualProtect(acode_buf, 1024, old_prot, &old_prot);
    test_print(code, 0);

    print("done\n");
    return 0;
}
