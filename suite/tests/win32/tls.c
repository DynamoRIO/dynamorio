/* **********************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
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

#include <windows.h>
#include <process.h> /* for _beginthreadex */
#include <stdio.h>
#include <AccCtrl.h> /* for SE_KERNEL_OBJECT */
#include <Aclapi.h>
#include "tools.h"

#define VERBOSE 0

/***************************************************************************/
/* This is all copied from ntdll.h, share/detach.c, and lib/globals_shared.h
 * FIXME - would be nicer to find a way to include these directly. */

typedef struct _UNICODE_STRING {
    /* Length field is size in bytes not counting final 0 */
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} UNICODE_STRING;
typedef UNICODE_STRING *PUNICODE_STRING;

typedef struct _OBJECT_ATTRIBUTES {
    ULONG Length;
    HANDLE RootDirectory;
    PUNICODE_STRING ObjectName;
    ULONG Attributes;
    PVOID SecurityDescriptor;       // Points to type SECURITY_DESCRIPTOR
    PVOID SecurityQualityOfService; // Points to type SECURITY_QUALITY_OF_SERVICE
} OBJECT_ATTRIBUTES;
typedef OBJECT_ATTRIBUTES *POBJECT_ATTRIBUTES;

#define InitializeObjectAttributes(p, n, a, r, s) \
    {                                             \
        (p)->Length = sizeof(OBJECT_ATTRIBUTES);  \
        (p)->RootDirectory = r;                   \
        (p)->Attributes = a;                      \
        (p)->ObjectName = n;                      \
        (p)->SecurityDescriptor = s;              \
        (p)->SecurityQualityOfService = NULL;     \
    }

#define OBJ_CASE_INSENSITIVE 0x00000040L
/* N.B.: this is an invalid parameter on NT4! */
#define OBJ_KERNEL_HANDLE 0x00000200L
typedef ULONG ACCESS_MASK;

typedef struct _CLIENT_ID {
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
} CLIENT_ID;
typedef CLIENT_ID *PCLIENT_ID;

typedef struct _USER_STACK {
    PVOID FixedStackBase;
    PVOID FixedStackLimit;
    PVOID ExpandableStackBase;
    PVOID ExpandableStackLimit;
    PVOID ExpandableStackBottom;
} USER_STACK, *PUSER_STACK;

/* 64kb, same as allocation granularity so is as small as we can get */
#define STACK_RESERVE 0x10000
/* 12kb, matches current core stack size, note can expand to
 * STACK_RESERVE - (5 * PAGE_SIZE), i.e. 44kb */
#define STACK_COMMIT 0x3000

/* returns NULL on error */
/* FIXME - is similar to core create_thread, but uses API routines where
 * possible, could try to share. */
/* stack_reserve and stack commit must be multiples of PAGE_SIZE and reserve
 * should be at least 5 pages larger then commit */
/* NOTE - For !target_kernel32 :
 *  target thread routine can't exit by by returning, instead it must call
 *    ExitThread or the like
 *  caller or target thread routine is responsible for informing csrss (if
 *    necessary) and freeing the the thread stack
 */
static HANDLE
nt_create_thread(HANDLE hProcess, PTHREAD_START_ROUTINE start_addr, void *arg,
                 uint stack_reserve, uint stack_commit, bool suspended, uint *tid,
                 bool target_kernel32)
{
    HANDLE hThread = NULL;
    USER_STACK stack = { 0 };
    OBJECT_ATTRIBUTES oa;
    CLIENT_ID cid;
    CONTEXT context = { 0 };
    uint num_commit_bytes, code;
    unsigned long old_prot;
    void *p;
    SECURITY_DESCRIPTOR *sd = NULL;

    GET_NTDLL(NtCreateThread,
              (OUT PHANDLE ThreadHandle, IN ACCESS_MASK DesiredAccess,
               IN POBJECT_ATTRIBUTES ObjectAttributes, IN HANDLE ProcessHandle,
               OUT PCLIENT_ID ClientId, IN PCONTEXT ThreadContext,
               IN PUSER_STACK UserStack, IN BOOLEAN CreateSuspended));

    /* both stack size and stack reserve must be multiples of PAGE_SIZE */
    assert((stack_reserve & (PAGE_SIZE - 1)) == 0);
    assert((stack_commit & (PAGE_SIZE - 1)) == 0);
    /* We stick a non-committed page on each end just to be safe and windows
     * needs three pages at the end to properly handle end of expandable stack
     * case (wants to pass exception back to the app on overflow, so needs some
     * stack for that). */
    assert(stack_reserve >= stack_commit + (5 * PAGE_SIZE));

    /* Use the security descriptor from the target process for creating the
     * thread so that once created the thread will be able to open a full
     * access handle to itself (xref case 2096). */
    /* NOTES - tried many ways to impersonate based on target process token
     * so we could just use the default and was unable to get anywhere with
     * that.  Easiest thing to do here is just create a new security descriptor
     * with a NULL (not empty) DACL [just InitializeSecurityDescriptor();
     * SetSecurityDescriptorDacl()], but that's a privilege escalation
     * problem (allows anybody full access to the thread)].  If we instead get
     * the full security descriptor from the target process and try to use that
     * the kernel complains that its a bad choice of owner.  What we do instead
     * is get just the DACL and leave the rest empty (will be filled in with
     * defaults during create thread).  Thus the security descriptor for the
     * thread will end up having the owner, group, and SACL from this
     * process and the DACL from the target.  Upshot is the the thread pseudo
     * handle will have full permissions (from the DACL), but the owner will be
     * us and, even though the handle we get back from CreateThread will be
     * fully permissioned as we request, any subsequent attempts by us to
     * OpenThread will fail since we aren't on the DACL.  We could always add
     * ourselves to the DACL later or we can use the SE_DEBUG_PRIVILEGE to
     * allow us to open it anyways.  Note if for some reason we want to view the
     * SACL we need to enable the ACCESS_SYSTEM_SECURITY privilege when opening
     * the handle.
     * FIXME - we could instead build our own DACL combining the two, we could
     * also try setting the owner/group after the thread is created if we
     * really wanted to look like the target process thread, and could also
     * start with a NULL sd and set the DACL later if want to match
     * CreateThread as closely as possible.  If we do anything post system
     * call should be sure to always create the thread suspended.
     */
    code = GetSecurityInfo(hProcess, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION, NULL,
                           NULL, NULL, NULL, &sd);
    assert(code == ERROR_SUCCESS);

    InitializeObjectAttributes(&oa, NULL, OBJ_CASE_INSENSITIVE, NULL, sd);

    stack.ExpandableStackBottom = VirtualAllocEx(
        hProcess, NULL, stack_reserve - PAGE_SIZE, MEM_RESERVE, PAGE_READWRITE);
    if (stack.ExpandableStackBottom == NULL)
        goto error;

    /* We provide non-committed boundary page on each side of the stack just to
     * be safe (note we will get a stack overflow exception if stack grows to
     * 3rd to last page of this region (xpsp2)). */
    stack.ExpandableStackBottom = ((byte *)stack.ExpandableStackBottom) + PAGE_SIZE;
    stack.ExpandableStackBase =
        ((byte *)stack.ExpandableStackBottom) + stack_reserve - (2 * PAGE_SIZE);

    stack.ExpandableStackLimit = ((byte *)stack.ExpandableStackBase) - stack_commit;
    num_commit_bytes = stack_commit + PAGE_SIZE;
    p = ((byte *)stack.ExpandableStackBase) - num_commit_bytes;
    p = VirtualAllocEx(hProcess, p, num_commit_bytes, MEM_COMMIT, PAGE_READWRITE);
    if (p == NULL)
        goto error;
    if (!VirtualProtectEx(hProcess, p, PAGE_SIZE, PAGE_READWRITE | PAGE_GUARD, &old_prot))
        goto error;

    /* set the context: initialize with our own */
    context.ContextFlags = CONTEXT_FULL;
    GetThreadContext(GetCurrentThread(), &context);
    if (target_kernel32) {
        assert(false);
#if 0 /* not implemented here */
        /* For kernel32!BaseThreadStartThunk CXT_XAX contains the address of the
         * thread routine and CXT_XBX the arg */
        context.CXT_XSP = (ptr_uint_t)stack.ExpandableStackBase;
        context.CXT_XIP = (ptr_uint_t)get_kernel_thread_start_thunk();
        context.CXT_XAX = (ptr_uint_t)start_addr;
        context.CXT_XBX = (ptr_uint_t)arg;
#endif
    } else {
        ptr_uint_t buf[2];
        bool res;
        SIZE_T written;
        IF_X64(assert(false)); /* FIXME won't work for x64: need a SetContext */
        /* directly targeting the start_address */
        context.CXT_XSP = ((ptr_uint_t)stack.ExpandableStackBase) - sizeof(buf);
        context.CXT_XIP = (ptr_uint_t)start_addr;
        /* set up arg on stack, give NULL return address */
        buf[1] = (ptr_uint_t)arg;
        buf[0] = 0;
        res = WriteProcessMemory(hProcess, (void *)context.CXT_XSP, &buf, sizeof(buf),
                                 &written);
        if (!res || written != sizeof(buf)) {
            goto error;
        }
    }
    if (context.CXT_XIP == 0) {
        goto error;
    }

    /* NOTE - CreateThread passes NULL for object attributes so despite Nebbet
     * must be optional (checked NTsp6a, XPsp2). We don't pass NULL so we can
     * specify the security descriptor. */
    if (!NT_SUCCESS(NtCreateThread(&hThread, THREAD_ALL_ACCESS, &oa, hProcess, &cid,
                                   &context, &stack, (byte)(suspended ? TRUE : FALSE)))) {
        goto error;
    }

    if (tid != NULL)
        *tid = (ptr_uint_t)cid.UniqueThread;

exit:
    if (sd != NULL) {
        /* Free the security descriptor. */
        LocalFree(sd);
    }

    return hThread;

error:
    if (stack.ExpandableStackBottom != NULL) {
        /* Free remote stack on error. */
        VirtualFreeEx(hProcess, stack.ExpandableStackBottom, 0, MEM_RELEASE);
    }
    assert(hThread == NULL);
    hThread = NULL; /* just to be safe */
    goto exit;
}

/* As a nice benefit of tools.h now including globals_shared.h, we have
 * the NUDGE_ defines already here.
 */

/***************************************************************************/

byte *
get_nudge_target()
{
    /* read DR marker
     * just hardcode the offsets for now
     */
    static const uint DR_NUDGE_FUNC_OFFSET = IF_X64_ELSE(0x28, 0x20);
    return get_drmarker_field(DR_NUDGE_FUNC_OFFSET);
}

typedef unsigned int(__stdcall *threadfunc_t)(void *);

static bool test_tls = false;
#define TLS_SLOTS 64
static bool tls_own[TLS_SLOTS];

byte *
get_own_teb()
{
    byte *teb;
    __asm {
        mov  eax, fs:[0x18]
        mov  teb, eax
    }
    return teb;
}

int WINAPI
thread_func(void *arg)
{
    int i;
    while (!test_tls)
        ; /* spin */
    for (i = 0; i < TLS_SLOTS; i++) {
        if (tls_own[i]) {
            if (TlsGetValue(i) != 0) {
                print("TLS slot %d is " PFX " when it should be 0!\n", i, TlsGetValue(i));
            }
        }
    }
    print("Done testing tls slots\n");
    return 0;
}

int
main()
{
    int tid;
    HANDLE detach_thread;
    HANDLE my_thread;
    byte *nudge_target;
    int i;

#if DYNAMO
    dynamo_init();
    dynamo_start();
#endif

    my_thread = (HANDLE)_beginthreadex(NULL, 0, thread_func, NULL, 0, &tid);

    nudge_target = get_nudge_target();
    if (nudge_target == NULL) {
        print("Cannot find DRmarker -- are you running natively?\n");
    } else {
        /* This test needs to do some work after detaching.  We exploit a
         * hole in DR by creating a thread that directly targets the DR
         * detach routine.  Hopefully this will motivate us to close
         * the hole (case 552) :)
         * The alternative is to create a new runall type that detaches from
         * the outside and then waits a while, but would be hard to time.
         */
        nudge_arg_t *arg = (nudge_arg_t *)VirtualAlloc(NULL, sizeof(nudge_arg_t),
                                                       MEM_COMMIT, PAGE_READWRITE);
        arg->version = NUDGE_ARG_CURRENT_VERSION;
        arg->nudge_action_mask = NUDGE_GENERIC(detach);
        arg->flags = 0;
        arg->client_arg = 0;
        print("About to detach using underhanded methods\n");
        detach_thread =
            (HANDLE)nt_create_thread(GetCurrentProcess(), (threadfunc_t)nudge_target, arg,
                                     STACK_RESERVE, STACK_COMMIT, false, NULL, false);
        WaitForSingleObject(detach_thread, INFINITE);

        assert(get_nudge_target() == NULL);
        print("Running natively now\n");
    }

    for (i = 0; i < TLS_SLOTS; i++) {
        /* case 8143: a runtime-loaded dll calling TlsAlloc needs to set
         * a value for already-existing threads.  The "official" method
         * is to directly TlsGetValue() and if not NULL assume that dll
         * has already set that value.  Our detach needs to clear values
         * to ensure this.  We have the simplest test possible here, of course.
         * We do need another thread as TlsAlloc seems to clear the slot for
         * the current thread.
         * http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dllproc/base/using_thread_local_storage_in_a_dynamic_link_library.asp
         */
        unsigned long tlshandle = 0UL;
        tlshandle = TlsAlloc();
        if (tlshandle == TLS_OUT_OF_INDEXES) {
            break;
        }
#if VERBOSE
        print("handle %d is %d\n", i, tlshandle);
#endif
        assert(tlshandle >= 0);
        /* we only want teb slots */
        if (tlshandle >= TLS_SLOTS)
            break;
        tls_own[tlshandle] = true;
    }

    /* tell thread to GO */
    test_tls = true;

    WaitForSingleObject(my_thread, INFINITE);

#if DYNAMO
    dynamo_stop();
    dynamo_exit();
#endif
    return 0;
}
