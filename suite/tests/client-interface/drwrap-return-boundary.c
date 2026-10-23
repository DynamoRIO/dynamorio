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
    const byte calls[][6] = {
        { 0xe8, 32 - 5, 0, 0, 0, 0xc3 }, /* call code+32; ret */
        { 0xff, 0xd0, 0xc3 },            /* call xax; ret */
        { 0xff, 0x50, 0, 0xc3 },         /* call [xax+disp8=0]; ret */
    };
    const size_t num_calls = sizeof(calls) / sizeof(calls[0]);
    byte *memory = VirtualAlloc(NULL, PAGE_SIZE * 2 * num_calls, MEM_RESERVE | MEM_COMMIT,
                                PAGE_NOACCESS);
    DWORD previous;
    INIT();
    assert(memory != NULL);
    for (size_t i = 0; i < num_calls; ++i) {
        byte *code = memory + PAGE_SIZE * (2 * i + 1);
        byte *target = code + 32;
        byte *operand = i == 2 ? code + 48 : target;
        byte *entry = code + 64;
        byte *cursor = entry;
        int displacement;
        assert(VirtualProtect(code, PAGE_SIZE, PAGE_READWRITE, &previous));

        /* Each call starts after an unreadable page and targets a leaf ret. */
        memcpy(code, calls[i], sizeof(calls[i]));
        *target = 0xc3;
        memcpy(code + 48, &target, sizeof(target));

        /* Load the indirect operand before jumping to the page-start call. */
#ifdef X64
        *cursor++ = 0x48;
#endif
        *cursor++ = 0xb8; /* mov xax, operand */
        memcpy(cursor, &operand, sizeof(operand));
        cursor += sizeof(operand);
        *cursor++ = 0xe9;
        displacement = (int)(code - (cursor + sizeof(displacement)));
        memcpy(cursor, &displacement, sizeof(displacement));
        assert(VirtualProtect(code, PAGE_SIZE, PAGE_EXECUTE_READ, &previous));
        assert(FlushInstructionCache(GetCurrentProcess(), code, PAGE_SIZE));
        prepare_boundary(target);
        for (int j = 0; j < 3; ++j)
            ((void (*)(void))entry)();
    }
    assert(VirtualFree(memory, 0, MEM_RELEASE));
    print("boundary calls completed\n");
    return 0;
}
