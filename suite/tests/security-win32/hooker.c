/* **********************************************************
 * Copyright (c) 2005-2008 VMware, Inc.  All rights reserved.
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

/* case 3102 - multiple VirtualProtect calls on the same page */

/* tasks
Hook three times the same guy with different hooks
VirtualProtect()
VirtualProtect()
VirtualProtect()

and make sure that they get properly executed
since we'd have a cache consistency problem when leaving RWX

Then we have a real hook that doesn't let the syscall through,
therefore TWO writes are considered bad and we have a security violation.

*/

#include "tools.h"
#include <windows.h>

const char *hookfn = "VirtualProtect"; /* and then next page */

#define NDEP /* for testing case 10387 */
#define FARAWAY_DLL "ADVAPI32.DLL"
const char *faraway_hook = "RegOpenKeyA";
/* FIXME: a lot better will be in one another module */

#define NAKED __declspec(naked)

/* FIXME: not fully fleshed out to add in regression suite need to
 * find a function with the same prologue or actually do some real
 * work to get it copied around.
 */
NAKED
hooker1()
{
#if 0
    /* a real hooker will take more than what's necessary for this test */
    asm {
        pusha
        pushf
    }
    /* here we can do whatever we need to */

    asm {
        popf
        popa
    }

    /* now it's time to get back to the original code */

    /* HACK: assuming beginning of function always looks like */
    asm {
     push    ebp
     mov     ebp,esp
     push    dword ptr [ebp+0x14]
    }
    /* and now we should know where we're going */
    /* JMP through memory to the address following our original instruction */
#endif
}

/* check for some unexpected behaviours with size = 5 and size = 0x1000, or even 0x2000 */
enum { HOOK_SIZE = 0x1000 };

/* FIXME: we have to test this on something other than kernel32!VirtualProtect for DEP
 * machines, for now keeping here
 */
void
unset_x(DWORD *hooktarget, DWORD prot)
{
    DWORD prev;
    DWORD size = HOOK_SIZE; /* although 5 bytes would have been just fine */
    int res = VirtualProtect(hooktarget, size, prot, &prev);
    DWORD gle = GetLastError();
    print("VirtualProtect(%s[" PFX "],%d," PFX ",prev) = %d GLE=" PFMT " prev=" PFMT "\n",
          hookfn, (hooktarget /* disabled */, 0), size, prot, res, gle, prev);

    /* we don't touch anything here */

    /* note we don't even bother to make again PAGE_EXECUTE_READ */
}

void
hook(DWORD *hooktarget)
{
    DWORD prev;
    DWORD size = HOOK_SIZE; /* although 5 bytes would have been just fine */
    DWORD prot = PAGE_EXECUTE_READWRITE;
    int res = VirtualProtect(hooktarget, size, prot, &prev);
    DWORD gle = GetLastError();
    print("VirtualProtect(%s[" PFX "],%d," PFX ",prev) = %d GLE=" PFMT " prev=" PFMT "\n",
          hookfn, (hooktarget /* disabled */, 0), size, prot, res, gle, prev);

    /* pretend hooking - actually a NOP */
    *hooktarget = *hooktarget;

    /* note we don't even bother to make again PAGE_EXECUTE_READ */
}

void
ret_hook(DWORD *hooktarget)
{
    DWORD old_code = *hooktarget;

    DWORD prev = 0xbadcde;
    DWORD size = HOOK_SIZE; /* although 5 bytes would have been just fine */
    int res;

    /* Simple hook that simply does a ret 0x10 - to clear up the 4 arguments */
    *hooktarget = 0x900010c2; /* ret 0x10; nop  */

    /* prev should remain unchanged */
    res = VirtualProtect(hooktarget, size, PAGE_EXECUTE_READWRITE, &prev);
    /* FIXME: eax may be garbage - but seems to be the same as GLE */
    print("VirtualProtect(%s[" PFX "],%d,PAGE_EXECUTE_READWRITE,prev) = %d GLE=" PFMT
          " prev=" PFMT "\n",
          hookfn, (hooktarget /* disabled */, 0), size, (res /* eax noisy */, 0),
          GetLastError(), prev);

    *hooktarget = old_code;
}

char *
prot_string(uint prot)
{
    uint ignore_extras = prot & ~(PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE);
    switch (ignore_extras) {
    case PAGE_NOACCESS: return "----";
    case PAGE_READONLY: return "r---";
    case PAGE_READWRITE: return "rw--";
    case PAGE_WRITECOPY: return "rw-c";
    case PAGE_EXECUTE: return "--x-";
    case PAGE_EXECUTE_READ: return "r-x-";
    case PAGE_EXECUTE_READWRITE: return "rwx-";
    case PAGE_EXECUTE_WRITECOPY: return "rwxc";
    }
    return "(error)";
}

bool
prot_is_executable(uint prot)
{
    prot &= ~(PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE);
    return (prot == PAGE_EXECUTE || prot == PAGE_EXECUTE_READ ||
            prot == PAGE_EXECUTE_READWRITE || prot == PAGE_EXECUTE_WRITECOPY);
}

void
query(DWORD *hooktarget)
{
    MEMORY_BASIC_INFORMATION mbi;
    DWORD res = VirtualQuery(hooktarget, &mbi, sizeof(mbi));
    if (res == sizeof(mbi)) {
        print("VirtualQuery(" PFX ") = %d GLE=" PFMT " prev=" PFMT " %s\n",
              (hooktarget /* disabled */, 0), res, GetLastError(), mbi.Protect,
              prot_string(mbi.Protect));
        /* at least confirm EXECUTABLE */
        print(" DEP => %s\n", prot_is_executable(mbi.Protect) ? "ok" : "NOT EXECUTABLE");
    } else {
        print("VirtualQuery(" PFX ") = %d GLE=" PFMT "\n", (hooktarget /* disabled */, 0),
              res, GetLastError());
    }
}

int
main()
{
    HANDLE kern32 = GetModuleHandle(TEXT("KERNEL32.DLL"));
    DWORD *addr_hook = (DWORD *)GetProcAddress(kern32, hookfn);

    HANDLE far_dll = LoadLibrary(TEXT(FARAWAY_DLL));
    DWORD *unset_hook = (DWORD *)GetProcAddress(far_dll, faraway_hook);

    INIT();

#ifdef NDEP
    assert(far_dll != NULL);
    assert(unset_hook != NULL);
    assert(((ptr_int_t)unset_hook >> 12) != ((ptr_int_t)addr_hook >> 12));
    /* we don't really execute from this */
    print("unset X bit\n");
    unset_x(unset_hook, PAGE_WRITECOPY);
    unset_x(unset_hook, PAGE_READWRITE);
    unset_x(unset_hook, PAGE_READWRITE | PAGE_GUARD);
    unset_x(unset_hook, PAGE_READWRITE | PAGE_GUARD);
    print("ready to hook far\n");
    hook(unset_hook);
    print("doublecheck flags\n");
    query(unset_hook);
#endif

    print("ready to hook\n");
    hook(addr_hook);
    print("one more\n");
    /* pretend we're hooking something else */
    hook(addr_hook);
    /* case 3102 triggers here */

    print("now third ...\n");
    /* and once more to see what happens */
    hook(addr_hook);
    print("doublecheck flags\n");
    query(addr_hook);

    /* now see if we didn't have proper consistency what would happen */
    print("check consistency ...\n");
    ret_hook(addr_hook);

    /* and once more to see what happens */
    hook(addr_hook);

    print("hooking done with\n");
}
