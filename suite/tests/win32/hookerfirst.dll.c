/* **********************************************************
 * Copyright (c) 2018 VMware, Inc.  All rights reserved.
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

/* case 2525 - hooker that takes over first - with a static library doing the work */

/* one should use a CALL and the other should use a JMP
 *   just to be sure
 *
 *  FIXME: need to get this test also to be done like initapc.dll.c so that
 *  this all happens BEFORE we take control
 **/

/* FIXME: we can't use a LdrLoadDll unless we chain properly - otherwise we don't get in!
 */

#include "tools.h"
#include <windows.h>

#define NAKED __declspec(naked)

/* FIXME: check for some unexpected behaviours with size = 5 and size = 0x1000, or even
 * 0x2000 */
enum { HOOK_SIZE = 0x1000 };

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
do_hook(const char *hookfn, int args, int use_call, DWORD *old_code)
{
    HANDLE ntdll = GetModuleHandle(TEXT("ntdll.DLL"));
    unsigned char *hooktarget = (char *)GetProcAddress(ntdll, hookfn);

    DWORD size = HOOK_SIZE; /* although 5 bytes would have been just fine */
    DWORD prev = 0xbadcde;
    int res;

    enum { op_JMP = 0xe9 };
    enum { op_CALL = 0xe8 };

    DWORD pc_rel_target;

    void *trampoline_target;

    *old_code = *(DWORD *)hooktarget;
    *(old_code + 1) = *(DWORD *)(hooktarget + 1);

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

    /* save hook code */
    *(old_code + 2) = *(DWORD *)hooktarget;
    *(old_code + 3) = *(DWORD *)(hooktarget + 1);

    print("do_hook() done\n");
}

void
do_unhook(const char *hookfn, int args, int use_call, DWORD *old_code)
{
    HANDLE ntdll = GetModuleHandle(TEXT("ntdll.DLL"));
    unsigned char *hooktarget = (char *)GetProcAddress(ntdll, hookfn);

    DWORD size = HOOK_SIZE; /* although 5 bytes would have been just fine */
    DWORD prev = 0xbadcde;
    int res;

    res = VirtualProtect(hooktarget, size, PAGE_EXECUTE_READWRITE, &prev);
    print("VirtualProtect(%s[" PFX "],%d,PAGE_EXECUTE_READWRITE,prev) = %d GLE=" PFMT
          " prev=" PFMT "\n",
          hookfn, (hooktarget /* disabled */, 0), size, (res /* eax noisy */, 0),
          GetLastError(), prev);

    __try {
        /* verify */
        if (*(DWORD *)hooktarget != *(old_code + 2) /* in fact new code */
            || *(DWORD *)(hooktarget + 1) != *(old_code + 3)) {
            print("there be witches! what happened to my previous hook?\n");
        } else
            print("my hook is still there, will remove now\n");

        /* restore */
        *(DWORD *)hooktarget = *old_code;
        *(DWORD *)(hooktarget + 1) = *(old_code + 1);

        if (*(DWORD *)(hooktarget + 1) != *(old_code + 1))
            print("there be witches! my good unhooking intentions were squashed on %s\n",
                  hookfn);
        else
            print("unhooked %s\n", hookfn);

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

    print("do_unhook() done\n");
}

/* having a lot of trouble with LdrUnloadDll or NtFlushInstructionCache for now trying
 * these */
#define FUNC1 "NtTerminateProcess"
#define FUNC2 "NtTerminateThread"

#define BUF_SIZE 2 + 2 /* old + new code */
DWORD func1_buf[BUF_SIZE];
DWORD func2_buf[BUF_SIZE];
DWORD ntflushwritebuffer_buf[BUF_SIZE];

int __declspec(dllexport) hookit(int arg)
{
    print("ready to hook %d\n", arg);

    do_hook(FUNC1, 4, 1, func1_buf);

    /* hack: we'll pass 4 args instead of 3 */
    do_hook(FUNC2, 4, 0, func2_buf);

    /* hack: we'll pass 4 args instead of 0 */
    /* hooking a function we really don't care much about */
    /* FIXME: should we let this through or not? */
    do_hook("NtFlushWriteBuffer", 4, 1, ntflushwritebuffer_buf);

    /* we have 4 writes to ntdll memory
     * on each of 6 calls to do_hook
     * should get app_modify_ntdll_writes = 24
     * FIXME: how to scrape a log for this?
     */
    print("hooking done with\n");
    return 0;
}

int __declspec(dllexport) unhookit(int arg)
{
    print("ready to unhook %d\n", arg);

    do_unhook(FUNC1, 4, 1, func1_buf);

    /* hack: we'll pass 4 args instead of 3 */
    do_unhook(FUNC2, 4, 0, func2_buf);

    /* hack: we'll pass 4 args instead of 0 */
    /* hooking a function we really don't care much about */
    /* FIXME: should we let this through or not? */
    do_unhook("NtFlushWriteBuffer", 4, 1, ntflushwritebuffer_buf);

    /* we have 4 writes to ntdll memory
     * on each of 6 calls to do_hook
     * should get app_modify_ntdll_writes = 24
     * FIXME: how to scrape a log for this?
     */
    print("unhooking done with\n");
    return 0;
}

BOOL APIENTRY
DllMain(HANDLE hModule, DWORD reason_for_call, LPVOID Reserved)
{
    switch (reason_for_call) {
    case DLL_PROCESS_ATTACH: hookit(1); break;
    }
    return TRUE;
}
