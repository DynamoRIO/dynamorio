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

#include "configure.h"
#include "dr_api.h"
#include "tools.h"

int
main(void)
{
    /* Only MOV, JCXZ, and jumps execute in the loop: ZF must remain set. */
    const byte instructions[] = {
#ifdef X64
        0x48, 0x89, 0xca, /* mov rdx, rcx */
#else
        0x8b, 0x54, 0x24, 0x04, /* mov edx, [esp+4] */
#endif
        0xc7, 0x42, 0x04, 1, 0, 0, 0, /* mov [edx+4], 1: ready */
        0x31, 0xc0,                   /* xor eax, eax */
        0x75, 0x08,                   /* jnz failure */
        0x8b, 0x0a,                   /* mov ecx, [edx] */
        0xe3, 0x0c,                   /* jcxz success */
        0xeb, 0xf8,                   /* jmp loop */
        0x90, 0x90, 0xb8, 1, 0, 0, 0, /* failure: mov eax, 1 */
#ifdef X64
        0xc3, 0x90, 0x90,
#else
        0xc2, 0x04, 0x00,
#endif
        0x31, 0xc0, /* success: xor eax, eax */
#ifdef X64
        0xc3
#else
        0xc2, 0x04, 0x00
#endif
    };
    volatile LONG state[2] = { 1, 0 };
    byte *code;
    HANDLE thread;
    DWORD old, result;
    INIT();
    code = VirtualAlloc(NULL, PAGE_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    assert(code != NULL);
    memcpy(code, instructions, sizeof(instructions));
    assert(VirtualProtect(code, PAGE_SIZE, PAGE_EXECUTE_READ, &old));
    assert(FlushInstructionCache(GetCurrentProcess(), code, sizeof(instructions)));
    thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)code, (void *)state, 0, NULL);
    assert(thread != NULL);
    while (!state[1])
        Sleep(1);

    // Attach while a native peer has live flags across its loop branches.
    dr_app_setup();
    dr_app_start();
    assert(dr_app_running_under_dynamorio());
    state[0] = 0;
    assert(WaitForSingleObject(thread, 10000) == WAIT_OBJECT_0);
    assert(GetExitCodeThread(thread, &result));
    assert(result == 0);
    CloseHandle(thread);
    dr_app_stop_and_cleanup();
    assert(VirtualFree(code, 0, MEM_RELEASE));
    print("asynchronous attachment preserved flags\n");
    return 0;
}
