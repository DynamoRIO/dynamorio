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
typedef LONG(NTAPI *nt_free_virtual_memory_t)(HANDLE, PVOID *, SIZE_T *, ULONG);
typedef int (*code_t)(void);

#define REGION_SIZE (16 * PAGE_SIZE)
#define ROUNDS 8

static virtual_alloc2_t virtual_alloc2;
static nt_free_virtual_memory_t nt_free_virtual_memory;

#define FREE_TO_PLACEHOLDER (MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER)
#define COALESCE (MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS)

/* Writes "mov eax, value; ret" at code, which must be writable, makes it
 * executable, and runs it.
 */
static int
place_and_run(byte *code, int value)
{
    DWORD old_prot;
    code[0] = 0xb8; /* mov eax, imm32 */
    memcpy(code + 1, &value, sizeof(value));
    code[5] = 0xc3; /* ret */
    if (!VirtualProtect(code, PAGE_SIZE, PAGE_EXECUTE_READ, &old_prot))
        print("VirtualProtect failed\n");
    FlushInstructionCache(GetCurrentProcess(), code, PAGE_SIZE);
    return ((code_t)code)();
}

/* Returns an allocation of size at base, which must be a placeholder of that
 * size, or NULL.
 */
static byte *
replace_placeholder(byte *base, size_t size)
{
    byte *mem = (byte *)virtual_alloc2(GetCurrentProcess(), base, size,
                                       MEM_RESERVE | MEM_COMMIT | MEM_REPLACE_PLACEHOLDER,
                                       PAGE_READWRITE, NULL, 0);
    return mem == base ? mem : NULL;
}

static void
test_reuse(const char *name, byte *base, size_t size, int start)
{
    int i, stale = 0;
    for (i = 0; i < ROUNDS; i++) {
        byte *code = replace_placeholder(base, size);
        if (code == NULL) {
            print("%s: replacing the placeholder failed\n", name);
            return;
        }
        if (place_and_run(code, start + i) != start + i)
            stale++;
        if (!VirtualFree(code, size, FREE_TO_PLACEHOLDER))
            print("%s: freeing back to a placeholder failed\n", name);
    }
    print("%s: %s\n", name, stale == 0 ? "ok" : "stale code ran");
}

/* Calls the system call itself: VirtualFree refuses some of these arguments
 * before making it.
 */
static bool
check_refused(const char *what, byte *base, size_t size, DWORD type)
{
    PVOID b = base;
    SIZE_T s = size;
    if (nt_free_virtual_memory(GetCurrentProcess(), &b, &s, type) < 0)
        return true;
    print("refusals: %s was freed\n", what);
    return false;
}

/* Frees that the kernel refuses must leave the memory, and the code in it,
 * as it was.  base is a placeholder of REGION_SIZE.
 */
static void
test_refusals(byte *base)
{
    bool ok = true;
    byte *code = replace_placeholder(base, REGION_SIZE);
    if (code == NULL) {
        print("refusals: replacing the placeholder failed\n");
        return;
    }
    ok = place_and_run(code, 0x300) == 0x300 && ok;
    ok = check_refused("size 0", code, 0, FREE_TO_PLACEHOLDER) && ok;
    ok = check_refused("a size past the allocation", code, REGION_SIZE + PAGE_SIZE,
                       FREE_TO_PLACEHOLDER) &&
        ok;
    ok = check_refused("a size not page-aligned", code, PAGE_SIZE + 1,
                       FREE_TO_PLACEHOLDER) &&
        ok;
    ok = check_refused("a base not page-aligned", code + 16, PAGE_SIZE,
                       FREE_TO_PLACEHOLDER) &&
        ok;
    ok = check_refused("coalescing size 0", code, 0, COALESCE) && ok;
    ok = ((code_t)code)() == 0x300 && ok;
    if (!VirtualFree(code, REGION_SIZE, FREE_TO_PLACEHOLDER))
        print("refusals: freeing back to a placeholder failed\n");
    print("refusals: %s\n", ok ? "ok" : "failed");
}

int
main(void)
{
    byte *base;
    INIT();
    virtual_alloc2 = (virtual_alloc2_t)GetProcAddress(GetModuleHandle("kernelbase.dll"),
                                                      "VirtualAlloc2");
    nt_free_virtual_memory = (nt_free_virtual_memory_t)GetProcAddress(
        GetModuleHandle("ntdll.dll"), "NtFreeVirtualMemory");
    if (virtual_alloc2 == NULL) {
        /* Placeholders are Windows 10 1803 and later. */
        print("whole region: ok\nsplit region: ok\nrefusals: ok\n");
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
    if (!VirtualFree(base, PAGE_SIZE, FREE_TO_PLACEHOLDER))
        print("splitting the placeholder failed\n");
    test_reuse("split region", base, PAGE_SIZE, 0x200);
    if (!VirtualFree(base, REGION_SIZE, COALESCE))
        print("coalescing the placeholders failed\n");
    test_refusals(base);
    if (!VirtualFree(base, 0, MEM_RELEASE))
        print("freeing the placeholder failed\n");
    return 0;
}
