/* **********************************************************
 * Copyright (c) 2012 Google, Inc.  All rights reserved.
 * Copyright (c) 2006-2010 VMware, Inc.  All rights reserved.
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

/* case 9016 apc-shellcode.c:
 * (the APC code is borrowed from initapc.dll.c <- winapc.c)
 *
 * fun code with 7 nested and 2 queued up APCs using user-mode QueueUserAPC
 * two shellcodes sent using native NtQueueApcThread as well as with QueueUserAPC
 * FIXME: sent to current thread only
 */

#ifndef ASM_CODE_ONLY

/* FIXME: for case 8451 we'd want to add a library that gets loaded */
#    define _WIN32_WINNT 0x0400
#    include <windows.h>
#    include <winbase.h> /* for QueueUserAPC */
#    include "tools.h"

static int result = 0;
static ULONG_PTR apc_arg = 0;

/* forwards */
static void
send_apc(PAPCFUNC func, ULONG_PTR depth);
static void WINAPI
apc_func(ULONG_PTR arg);

typedef VOID(NTAPI *PKNORMAL_ROUTINE)(IN PVOID NormalContext, IN PVOID SystemArgument1,
                                      IN PVOID SystemArgument2);

/* In asm code. */
void
vse_datacode(void);
void
vse_native_datacode(void);
void
other_datacode(void);
void
other_native_datacode(void);

/* FIXME: should be copied onto a bad VirtualAllocate page */
PAPCFUNC vse_apc_func = (PAPCFUNC)vse_datacode;

PAPCFUNC other_apc_func = (PAPCFUNC)other_datacode;

/* we need native APCs */
int
native_queue_apc(HANDLE thread, PKNORMAL_ROUTINE apc_dispatch, PAPCFUNC func,
                 ULONG_PTR arg)
{
    NTSTATUS status;
    GET_NTDLL(NtQueueApcThread,
              (IN HANDLE ThreadHandle, IN PKNORMAL_ROUTINE ApcRoutine,
               IN PVOID ApcContext OPTIONAL, IN PVOID Argument1 OPTIONAL,
               IN PVOID Argument2 OPTIONAL));

    /*
     *   This is what I see in QueueUserAPC just before calling NtQueueApcThread
     *   so ApcContext in that case is the argument to the function
     *   dds esp
     *   0012fb58  fffffffe
     *   0012fb5c  7c82c0e6 kernel32!BaseDispatchAPC
     *   0012fb60  0041624b apc_shellcode!apc_func [security-win32/apc-shellcode.c @ 79]
     *   0012fb64  00000006
     *   0012fb68  00000000
     */
    /* FIXME: should really be (thread, apc_dispatch, func, arg, NULL)
     * but for more devious testing using this broken version for now
     */
    status =
        NtQueueApcThread(thread, apc_dispatch, NULL /* no context */, func, (void *)arg);
    if (!NT_SUCCESS(status)) {
        print("Error using NtQueueApcThread %x\n", status);
        return 0;
    } else {
        return 1;
    }
}

/* our replacement of kernel32!BaseDispatchAPC */
int NTAPI
our_dispatch_apc(PVOID context, PAPCFUNC func, ULONG_PTR arg)
{
    /* no SEH stuff for us */
    func(arg);
    return 0;
}

/* our replacement of kernel32!QueueUserAPC() */
int
queue_apc(bool native, PAPCFUNC func, HANDLE thread, ULONG_PTR arg)
{
    if (native) {
        return native_queue_apc(thread, (PKNORMAL_ROUTINE)&our_dispatch_apc, func, arg);
    } else {
        return QueueUserAPC(func, thread, arg);
    }
}

static void WINAPI
apc_func(ULONG_PTR arg)
{
    result += 100;
    apc_arg = arg;

    print("apc_func %d\n", (int)arg);
    /* nested APC */
    if (arg > 0) {
        send_apc(arg % 3 == 0 ? apc_func : apc_func, /* FIXME: get fancy */
                 arg - 1);
    }
}

static void WINAPI
other_apc_func_helper(ULONG_PTR arg)
{
    print("webcam or crash and burn in interop issues\n");
}

static void
send_apc(PAPCFUNC func, ULONG_PTR depth)
{
    int res = queue_apc(false, func, GetCurrentThread(), depth);
    print("QueueUserAPC returned %d\n", res);
    if (depth > 0) {
        /* we queue up two APCs at a time (producing a Fibonaccian growth) */
        /* and for change we use our own wrapper */
        res = queue_apc(true /* native */, func, GetCurrentThread(), depth - 1);
        print("second QueueUserAPC returned %d\n", res);
    }
    /* an alertable system call so we receive the APCs (FIFO order) */
    res = SleepEx(100, 1);
    /* is going to return 192 since received apc during sleep call
     * well technically 192 is io completion interruption, but seems to
     * report that for any interrupting APC */
    print("SleepEx returned %d\n", res);
    print("Apc arg = %d\n", (int)apc_arg);
    print("Result = %d\n", result);
}

static void
native_send_apc(PKNORMAL_ROUTINE native_func1, PKNORMAL_ROUTINE native_func2)
{
    int res = native_queue_apc(GetCurrentThread(), native_func1, NULL, 0);
    print("native_queue_apc returned %d\n", res);

    /* we queue up two APCs at a time maybe of different type */
    /* note that these just queue, they WILL NOT stack up unless the
     * APC functions themselves get in Alertable state
     * FIXME: should put a TestAlert in the loaded DLL,
     * I don't think there is one already in LoadLibrary
     * (if there is it may be bad for hotpatch DLLs)
     */
    res = native_queue_apc(GetCurrentThread(), native_func2, NULL, 0);
    print("second native_queue_apc returned %d\n", res);

    /* an alertable system call so we receive the APCs (FIFO order) */
    res = SleepEx(100, 1);
    /* is going to return 192 since received apc during sleep call
     * well technically 192 is io completion interruption, but seems to
     * report that for any interrupting APC */
    print("SleepEx returned %d\n", res);
    /* FIXME: don't have a good sign that the shellcodes did execute */
}

int
main()
{
    INIT();

    print("apc-shellcode\n");

    print("normal (nested) apc\n");
    send_apc(apc_func, 7);

    __try {
        print("VSE-like native mode\n");
        native_send_apc((PKNORMAL_ROUTINE)vse_native_datacode,
                        (PKNORMAL_ROUTINE)vse_native_datacode);
        print("VSE native shellcode returned\n");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        print("VSE native shellcode exception!\n");
    }

    __try {
        print("other APC native mode\n");
        native_send_apc((PKNORMAL_ROUTINE)other_native_datacode,
                        (PKNORMAL_ROUTINE)other_native_datacode);
        print("*** other APC native shellcode returned\n");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        print("APC native shellcode exception!\n");
    }

    print("VSE-like user mode\n");
    send_apc(vse_apc_func, 7);
    print("*** VSE user shellcode allowed!\n");

    print("other APC user shellcode\n");
    send_apc(other_apc_func, 7);
    print("*** other APC user shellcode allowed!\n");

    return 0;
}

/*
0:000> uf kernel32!QueueUserAPC
kernel32!QueueUserAPC:
7c82c082 8bff             mov     edi,edi
7c82c084 55               push    ebp
7c82c085 8bec             mov     ebp,esp
7c82c087 51               push    ecx
7c82c088 51               push    ecx
7c82c089 8365f800         and     dword ptr [ebp-0x8],0x0
7c82c08d 57               push    edi
7c82c08e 33c0             xor     eax,eax
7c82c090 50               push    eax
7c82c091 6a08             push    0x8
7c82c093 8d7dfc           lea     edi,[ebp-0x4]
7c82c096 ab               stosd
7c82c097 8d45f8           lea     eax,[ebp-0x8]
7c82c09a 50               push    eax
7c82c09b 6a01             push    0x1
7c82c09d 6a00             push    0x0
7c82c09f 6a00             push    0x0
7c82c0a1 6a01             push    0x1
7c82c0a3 ff15ec14807c call dword ptr [kernel32!_imp__RtlQueryInformationActivationContext
(7c8014ec)] 7c82c0a9 85c0             test    eax,eax 7c82c0ab 5f               pop edi
7c82c0ac 0f8c51890100     jl      kernel32!QueueUserAPC+0x2c (7c844a03)

kernel32!QueueUserAPC+0x2c:
7c844a03 50               push    eax
7c844a04 68804a847c       push    0x7c844a80
7c844a09 68284a847c       push    0x7c844a28
        7c844a80  "QueueUserAPC"
        0:000> da 0x7c844a28
        7c844a28  "SXS: %s failing because RtlQuery"
        7c844a48  "InformationActivationContext() r"
        7c844a68  "eturned status %08lx."
7c844a0e e856ae0300       call    kernel32!DbgPrint (7c87f869)
7c844a13 83c40c           add     esp,0xc
7c844a16 33c0             xor     eax,eax
7c844a18 e9c076feff       jmp     kernel32!QueueUserAPC+0x6d (7c82c0dd)

kernel32!QueueUserAPC+0x43:
7c82c0b2 f645fc01         test    byte ptr [ebp-0x4],0x1
7c82c0b6 8b45f8           mov     eax,[ebp-0x8]
7c82c0b9 0f855e890100     jne     kernel32!QueueUserAPC+0x4c (7c844a1d)

kernel32!QueueUserAPC+0x4c:
7c844a1d 83c8ff           or      eax,0xffffffff
7c844a20 e99a76feff       jmp     kernel32!QueueUserAPC+0x4f (7c82c0bf)

kernel32!QueueUserAPC+0x4f:
7c82c0bf 50               push    eax
7c82c0c0 ff7510           push    dword ptr [ebp+0x10]
7c82c0c3 ff7508           push    dword ptr [ebp+0x8]
7c82c0c6 68e6c0827c       push    0x7c82c0e6  ; kernel32!BaseDispatchAPC
7c82c0cb ff750c           push    dword ptr [ebp+0xc] ; thread
7c82c0ce ff152015807c call dword ptr [kernel32!_imp__NtQueueApcThread (7c801520)]
7c82c0d4 33c9             xor     ecx,ecx
7c82c0d6 85c0             test    eax,eax
7c82c0d8 0f9dc1           setge   cl
7c82c0db 8bc1             mov     eax,ecx

kernel32!QueueUserAPC+0x6d:
7c82c0dd c9               leave
7c82c0de c20c00           ret     0xc

0:000> uf 0x7c82c0e6
kernel32!BaseDispatchAPC:
seems to be mostly concerned about SXS
kernel32!BaseDispatchAPC+0x33:
7c82c13a c20c00           ret     0xc

0:000> uf kernel32!BaseDispatchApc
kernel32!BaseDispatchAPC:
7c82c0e6 6a20             push    0x20
7c82c0e8 6840c1827c       push    0x7c82c140
7c82c0ed e8d463fdff       call    kernel32!_SEH_prolog (7c8024c6)
7c82c0f2 c745d014000000   mov     dword ptr [ebp-0x30],0x14
7c82c0f9 c745d401000000   mov     dword ptr [ebp-0x2c],0x1
7c82c100 33c0             xor     eax,eax
7c82c102 8d7dd8           lea     edi,[ebp-0x28]
7c82c105 ab               stosd
7c82c106 ab               stosd
7c82c107 ab               stosd
7c82c108 8b4510           mov     eax,[ebp+0x10]
7c82c10b 8945e4           mov     [ebp-0x1c],eax
7c82c10e 83f8ff           cmp     eax,0xffffffff
7c82c111 0f84e1880100     je      kernel32!BaseDispatchAPC+0x2d (7c8449f8)

kernel32!BaseDispatchAPC+0x3b:
7c82c117 50               push    eax
7c82c118 8d45d0           lea     eax,[ebp-0x30]
7c82c11b 50               push    eax
7c82c11c ff153c12807c call dword ptr
[kernel32!_imp__RtlActivateActivationContextUnsafeFast (7c80123c)] 7c82c122 8365fc00 and
dword ptr [ebp-0x4],0x0 7c82c126 ff750c           push    dword ptr [ebp+0xc] 7c82c129
ff5508           call    dword ptr [ebp+0x8] 7c82c12c 834dfcff         or      dword ptr
[ebp-0x4],0xffffffff 7c82c130 e81c000000       call    kernel32!BaseDispatchAPC+0x5b
(7c82c151)

kernel32!BaseDispatchAPC+0x33:
7c82c135 e8c763fdff       call    kernel32!_SEH_epilog (7c802501)
7c82c13a c20c00           ret     0xc

kernel32!BaseDispatchAPC+0x2d:
7c8449f8 ff750c           push    dword ptr [ebp+0xc]
7c8449fb ff5508           call    dword ptr [ebp+0x8]
7c8449fe e93277feff       jmp     kernel32!BaseDispatchAPC+0x33 (7c82c135)


kernel32!BaseDispatchAPC+0x5b: helper
7c82c151 8d45d0           lea     eax,[ebp-0x30]
7c82c154 50               push    eax
7c82c155 ff153812807c call dword ptr
[kernel32!_imp__RtlDeactivateActivationContextUnsafeFast (7c801238)] 7c82c15b ff75e4 push
dword ptr [ebp-0x1c] 7c82c15e ff15e414807c call dword ptr
[kernel32!_imp__RtlReleaseActivationContext (7c8014e4)] 7c82c164 c3               ret

*/

#else /* ASM_CODE_ONLY */

#    include "asm_defines.asm"

/* clang-format off */
START_FILE

DECLARE_GLOBAL(vse_datacode)
DECLARE_GLOBAL(vse_native_datacode)
DECLARE_GLOBAL(other_datacode)
DECLARE_GLOBAL(other_native_datacode)

/* PR 229292: we need to isolate these vars to avoid flushes to them via
 * writes to other vars
 */
REPEAT 4096
        nop
        ENDM

/* match PIC shellcode header, for example
 * 0013004c 53               push    ebx
 * 0013004d e800000000       call    00130052
 */
ADDRTAKEN_LABEL(vse_datacode:)
        push     REG_XBX
        call     next1
    next1:
        pop      REG_XBX
        pop      REG_XBX
        ret      IF_NOT_X64(ARG_SZ)

ADDRTAKEN_LABEL(vse_native_datacode:)
        push     REG_XBX
        call     next2
    next2:
        pop      REG_XBX
        pop      REG_XBX
        ret      IF_NOT_X64(3 * ARG_SZ)

ADDRTAKEN_LABEL(other_datacode:)
        ret      IF_NOT_X64(ARG_SZ)
        ret      IF_NOT_X64(ARG_SZ)

ADDRTAKEN_LABEL(other_native_datacode:)
        ret      IF_NOT_X64(3 * ARG_SZ)
        ret      IF_NOT_X64(3 * ARG_SZ)

/* Tail padding. */
REPEAT 4096
        nop
        ENDM

END_FILE
/* clang-format on */

#endif /* ASM_CODE_ONLY */
