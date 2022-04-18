/* **********************************************************
 * Copyright (c) 2018 Google, Inc.  All rights reserved.
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

/* case 2525 - hooker for LdrLoadDll or NtProtectVirtualMemory */

/* one should use a CALL and the other should use a JMP
 *   just to be sure
 *
 *  FIXME: need to get this test also to be done like initapc.dll.c so that
 *  this all happens BEFORE we take control
 **/

#include "tools.h"
#include <windows.h>

#define NAKED __declspec(naked)

/* check for some unexpected behaviours with size = 5 and size = 0x1000, or even 0x2000 */
enum { HOOK_SIZE = 0x1000 };

/*
 * NTSTATUS LdrLoadDll(LPCWSTR path_name, DWORD flags,
 *                     PUNICODE_STRING libname, HMODULE* hModule)
 */

NAKED
hooker4()
{
    __asm {
        pusha
        pushf
    }
    /* here we can do whatever we need to */
    /* clang-format off */ /* work around clang-format bug */
    __asm {
        popf
        popa
    }
    /* clang-format on */

    /* know the number of args */
    __asm {
        ret 4*4;
    }
}

NAKED
hooker5()
{
    __asm {
        pusha
        pushf
    }
    /* here we can do whatever we need to */

    /* clang-format off */ /* work around clang-format bug */
    __asm {
        popf
        popa
    }
    /* clang-format on */

    /* know the number of args */
    __asm {
        ret 5*4;
    }
}

void
do_hook(const char *hookfn, int args, int use_call)
{
    HANDLE ntdll = GetModuleHandle(TEXT("ntdll.DLL"));
    unsigned char *hooktarget = (char *)GetProcAddress(ntdll, hookfn);

    DWORD old_code1 = *(DWORD *)hooktarget;
    DWORD old_code2 = *(DWORD *)(hooktarget + 1);

    DWORD size = HOOK_SIZE; /* although 5 bytes would have been just fine */
    DWORD prev = 0xbadcde;
    int res;

    DWORD pc_rel_target;

    enum { op_JMP = 0xe9 };
    enum { op_CALL = 0xe8 };

    void *trampoline_target;

    if (use_call)
        args += 1; /* clean up addr of CALL */

    switch (args) {
    case 4: trampoline_target = hooker4; break;
    case 5: trampoline_target = hooker5; break;
    default: print("BAD args\n");
    }

    __try {
        *hooktarget = 0xba;
        print("bad: why is this writable?\n");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        print("ok: can't write\n");
    }

    res = VirtualProtect(hooktarget, size, PAGE_EXECUTE_READWRITE, &prev);
    print("VirtualProtect(%s[" PFX "],%d,PAGE_EXECUTE_READWRITE,prev) = %d GLE=" PFMT
          " prev=" PFMT "\n",
          hookfn, (hooktarget /* disabled */, 0), size, (res /* eax noisy */, 0),
          GetLastError(), prev);

#ifdef VERBOSE
    print("before hooking %s = %02x %02x %02x %02x %02x\n", hookfn, hooktarget[0],
          hooktarget[1], hooktarget[2], hooktarget[3], hooktarget[4]);
#endif

    pc_rel_target = (DWORD)trampoline_target -
        ((DWORD)hooktarget + 5); /* target = offset + cur pc + size*/

    __try {
        *hooktarget = use_call ? op_CALL : op_JMP;
        *(DWORD *)(hooktarget + 1) = pc_rel_target;

        /* now let's get smart here and see if hook worked */
#ifdef VERBOSE
        print("after  hooking %s = %02x %02x %02x %02x %02x\n", hookfn, hooktarget[0],
              hooktarget[1], hooktarget[2], hooktarget[3], hooktarget[4]);
#endif
        if (*(DWORD *)(hooktarget + 1) != pc_rel_target)
            print("there be witches! what happened to my write?\n");
        else
            print("hooked %s\n", hookfn);
        /* FIXME: try it out and see what happens */

        /* restore */
        *(DWORD *)hooktarget = old_code1;
        *(DWORD *)(hooktarget + 1) = old_code2;

        print("restored old code\n");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        print("bad: can't write, though made writable\n");
    }

    /* restore page permissoins now, could be optional */
    res = VirtualProtect(hooktarget, size, PAGE_EXECUTE_READ, &prev);
    print("VirtualProtect(%s[" PFX "],%d,PAGE_EXECUTE_READ,...) = %d GLE=" PFMT "\n",
          hookfn, (hooktarget /* disabled */, 0), size, (res /* eax noisy */, 0),
          GetLastError());
    print("old permissions ...prev=" PFMT ")\n", prev);

    __try {
        *hooktarget = 0xba;
        print("bad: why is this writable?\n");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        print("ok: can't write\n");
    }

    print("all should be good\n");
}

int
main()
{
    INIT();

    print("ready to hook\n");

    do_hook("LdrLoadDll", 4, 1);
    do_hook("LdrLoadDll", 4, 0);

    /* hack: we'll pass 4 args instead of 3 */
    do_hook("NtFlushInstructionCache", 4, 1);
    do_hook("NtFlushInstructionCache", 4, 0);

    /* hack: we'll pass 4 args instead of 0 */
    /* hooking a function we really don't care much about */
    /* FIXME: should we let this through or not? */
    do_hook("NtFlushWriteBuffer", 4, 1);
    do_hook("NtFlushWriteBuffer", 4, 0);

    /* we have 4 writes to ntdll memory
     * on each of 6 calls to do_hook
     * should get app_modify_ntdll_writes = 24
     * FIXME: how to scrape a log for this?
     */
    print("hooking done with\n");
}
