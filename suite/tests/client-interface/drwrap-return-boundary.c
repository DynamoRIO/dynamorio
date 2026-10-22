/* **********************************************************
 * Copyright (c) Bryan Berns  All rights reserved.
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
#include <string.h>

static void *volatile target_echo;

NOINLINE void EXPORT
prepare_boundary(void *target)
{
    target_echo = target;
}

int
main(void)
{
    byte *memory =
        VirtualAlloc(NULL, PAGE_SIZE * 2, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS);
    DWORD previous;
    int displacement = 32 - 5;
    byte *code;
    INIT();
    assert(memory != NULL);
    code = memory + PAGE_SIZE;
    assert(VirtualProtect(code, PAGE_SIZE, PAGE_READWRITE, &previous));

    /* Both generated functions are leaf assembly and do not access arguments. */
    code[0] = 0xe8;
    memcpy(code + 1, &displacement, sizeof(displacement));
    code[5] = 0xc3;
    code[32] = 0xc3;
    assert(VirtualProtect(code, PAGE_SIZE, PAGE_EXECUTE_READ, &previous));
    assert(FlushInstructionCache(GetCurrentProcess(), code, PAGE_SIZE));
    prepare_boundary(code + 32);
    for (int i = 0; i < 3; ++i)
        ((void (*)(void))code)();
    assert(VirtualFree(memory, 0, MEM_RELEASE));
    print("boundary calls completed\n");
    return 0;
}
