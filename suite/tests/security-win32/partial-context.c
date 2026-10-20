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

#ifdef X64
#    define IP Rip
#    define AX Rax
#    define DX Rdx
#else
#    define IP Eip
#    define AX Eax
#    define DX Edx
#endif

int
main(void)
{
    /* Two volatile integer registers stay live throughout the spin loop. */
    const byte instructions[] = { 0xb8, 0x78, 0x56, 0x34, 0x12, 0xba, 0x89,
                                  0x67, 0x45, 0x23, 0xeb, 0xfe, 0x31, 0xc0,
#ifdef X64
                                  0xc3
#else
                                  0xc2, 0x04, 0x00
#endif
    };
    byte *code;
    HANDLE thread;
    CONTEXT original, update, observed;
    DWORD old;
    bool found = false;
    INIT();
    code = VirtualAlloc(NULL, PAGE_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    assert(code != NULL);
    memcpy(code, instructions, sizeof(instructions));
    assert(VirtualProtect(code, PAGE_SIZE, PAGE_EXECUTE_READ, &old));
    assert(FlushInstructionCache(GetCurrentProcess(), code, sizeof(instructions)));
    thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)code, NULL, 0, NULL);
    assert(thread != NULL);

    // Suspend at the application loop, allowing initial thread setup to finish.
    for (int attempt = 0; attempt < 1000; ++attempt) {
        Sleep(1);
        assert(SuspendThread(thread) != (DWORD)-1);
        memset(&original, 0, sizeof(original));
        original.ContextFlags = CONTEXT_FULL;
        assert(GetThreadContext(thread, &original));
        if ((byte *)(ptr_uint_t)original.IP == code + 10) {
            found = true;
            break;
        }
        assert(ResumeThread(thread) != (DWORD)-1);
    }
    assert(found);
    assert(original.AX == 0x12345678 && original.DX == 0x23456789);

    // Control-only updates must retain integer state and expose the queued app PC.
    update = original;
    update.ContextFlags = CONTEXT_CONTROL;
    update.AX = update.DX = 0;
    assert(SetThreadContext(thread, &update));
    memset(&observed, 0, sizeof(observed));
    observed.ContextFlags = CONTEXT_FULL;
    assert(GetThreadContext(thread, &observed));
    assert(observed.IP == original.IP);
    assert(observed.AX == original.AX && observed.DX == original.DX);

    // Integer-only updates must retain the queued control state.
    update = observed;
    update.ContextFlags = CONTEXT_INTEGER;
    update.AX = 0x3456789a;
    update.IP = 0;
    assert(SetThreadContext(thread, &update));
    observed.ContextFlags = CONTEXT_FULL;
    assert(GetThreadContext(thread, &observed));
    assert(observed.IP == original.IP);
    assert(observed.AX == 0x3456789a && observed.DX == original.DX);

    // Resume through the normal thread return instead of terminating a live context.
    update = observed;
    update.ContextFlags = CONTEXT_CONTROL;
    update.IP = (ptr_uint_t)(code + 12);
    assert(SetThreadContext(thread, &update));
    assert(ResumeThread(thread) != (DWORD)-1);
    assert(WaitForSingleObject(thread, 10000) == WAIT_OBJECT_0);
    CloseHandle(thread);
    assert(VirtualFree(code, 0, MEM_RELEASE));
    print("partial and queued contexts preserved\n");
    return 0;
}
