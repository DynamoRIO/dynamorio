/* **********************************************************
 * Copyright (c) 2026 Acira AI.  All rights reserved.
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
 * * Neither the name of Acira AI nor the names of its contributors may be
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

/* Tests reusing code memory through placeholders, as JITs such as V8 do:
 * code is placed in an allocation that replaced a placeholder, run, and
 * freed back to a placeholder with MEM_PRESERVE_PLACEHOLDER, and new code is
 * then placed at the same address.  We must not run the old code there.
 */

#include "tools.h"

#ifndef MEM_RESERVE_PLACEHOLDER
#    define MEM_RESERVE_PLACEHOLDER 0x00040000
#    define MEM_REPLACE_PLACEHOLDER 0x00004000
#endif
#ifndef MEM_COALESCE_PLACEHOLDERS
#    define MEM_COALESCE_PLACEHOLDERS 0x00000001
#    define MEM_PRESERVE_PLACEHOLDER 0x00000002
#endif

typedef PVOID(WINAPI *virtual_alloc2_t)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, PVOID,
                                        ULONG);
typedef int (*code_t)(void);

#define REGION_SIZE (16 * PAGE_SIZE)
#define ROUNDS 8

static virtual_alloc2_t virtual_alloc2;

/* Places "mov eax, value; ret" at base, in an allocation of size that replaces
 * a placeholder, runs it, and frees it back to a placeholder.
 */
static int
run_code(byte *base, size_t size, int value)
{
    DWORD old_prot;
    int res;
    byte *code = (byte *)virtual_alloc2(
        GetCurrentProcess(), base, size,
        MEM_RESERVE | MEM_COMMIT | MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0);
    if (code != base) {
        print("replacing the placeholder failed\n");
        return -1;
    }
    code[0] = 0xb8; /* mov eax, imm32 */
    memcpy(code + 1, &value, sizeof(value));
    code[5] = 0xc3; /* ret */
    if (!VirtualProtect(code, PAGE_SIZE, PAGE_EXECUTE_READ, &old_prot))
        print("VirtualProtect failed\n");
    FlushInstructionCache(GetCurrentProcess(), code, PAGE_SIZE);
    res = ((code_t)code)();
    if (!VirtualFree(code, size, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER))
        print("freeing back to a placeholder failed\n");
    return res;
}

static void
test_reuse(const char *name, byte *base, size_t size, int start)
{
    int i, stale = 0;
    for (i = 0; i < ROUNDS; i++) {
        if (run_code(base, size, start + i) != start + i)
            stale++;
    }
    print("%s: %s\n", name, stale == 0 ? "ok" : "stale code ran");
}

int
main(void)
{
    byte *base;
    virtual_alloc2 = (virtual_alloc2_t)GetProcAddress(GetModuleHandle("kernelbase.dll"),
                                                      "VirtualAlloc2");
    if (virtual_alloc2 == NULL) {
        /* Placeholders are Windows 10 1803 and later. */
        print("whole region: ok\nsplit region: ok\n");
        return 0;
    }
    base = (byte *)virtual_alloc2(GetCurrentProcess(), NULL, REGION_SIZE,
                                  MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS,
                                  NULL, 0);
    if (base == NULL) {
        print("reserving a placeholder failed\n");
        return 1;
    }
    test_reuse("whole region", base, REGION_SIZE, 0x100);
    /* Split off the first page, use only it, and coalesce the two again. */
    if (!VirtualFree(base, PAGE_SIZE, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER))
        print("splitting the placeholder failed\n");
    test_reuse("split region", base, PAGE_SIZE, 0x200);
    if (!VirtualFree(base, REGION_SIZE, MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS))
        print("coalescing the placeholders failed\n");
    if (!VirtualFree(base, 0, MEM_RELEASE))
        print("freeing the placeholder failed\n");
    return 0;
}
