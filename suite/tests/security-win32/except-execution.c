/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2010 VMware, Inc.  All rights reserved.
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

#include "tools.h"
#include <windows.h>

#include "except.h"

/* for repeatability we need a consistent stack */
#define STACK_BASE ((void *)0x14000000)

/***************************************************************************/
/* This is all copied from ntdll.h and from share/detach.c */

#include <AccCtrl.h> /* for SE_KERNEL_OBJECT */
#include <Aclapi.h>

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

typedef unsigned int(__stdcall *threadfunc_t)(void *);

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
        hProcess, STACK_BASE, stack_reserve - PAGE_SIZE, MEM_RESERVE, PAGE_READWRITE);
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
        /* directly targeting the start_address */
        context.CXT_XSP = ((ptr_uint_t)stack.ExpandableStackBase) - sizeof(buf);
        context.CXT_XIP = (ptr_uint_t)start_addr;
        /* set up arg on stack, give NULL return address */
        buf[0] = (ptr_uint_t)arg;
        buf[1] = 0;
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

/***************************************************************************/

typedef void (*funcptr)();

char *badfunc;

void
run_test()
{
    EXCEPTION_RECORD exception;
    CONTEXT *context;
    /* allocate on the stack for more reliable addresses for template
     * than if in data segment
     */
    char badfuncbuf[1000];

    badfunc = (char *)ALIGN_FORWARD(badfuncbuf, 256);

    /* FIXME: make this fancier */
    badfunc[0] = 0xc3; /* ret */

    /* FIXME: move this to a general win32/ test */
    /* Verifying RaiseException, FIXME: maybe move to a separate unit test */
    __try {
        ULONG arguments[] = { 0, 0xabcd };
        initialize_registry_context();
        RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 2, arguments);
        print("Never after RaiseException\n");
    } __except (print("In RaiseException filter\n"),
#ifdef RAISE_EXCEPTION_EIP_NOT_CONSTANT
                dump_exception_info((GetExceptionInformation())->ExceptionRecord,
                                    (GetExceptionInformation())->ContextRecord),
#endif
                EXCEPTION_EXECUTE_HANDLER) {
        print("In RaiseException handler\n");
    }

    /* FIXME: move this to a general win32/ test except-unreadable.c
          see case 197 and should also cover a direct CTI to bad memory,
          the latter not very likely in real apps though */

    /* These invalid address exceptions are to unreadable memory
       and we should try to transparently return normal exceptions
       [most likely these will remain unhandled]

       We want to make sure people get the seriousness of the situation
       and every case in which this happens should be taken as a real attack vector.
       Usually they would see this as a bug,
       but it may also be an attacker probing with AAAA.

       Therefore, we should let execution continue only in -detect_mode
       (fake) exceptions should be generated as if generated normally
    */
    __try {
        __try {
            __try {
                initialize_registry_context();
                __asm {
                    mov   eax, 0xbadcdef0
                    call  dword ptr [eax]
                }
                print("At statement after exception\n");
            } __except (context = (GetExceptionInformation())->ContextRecord,
                        print("Inside first filter eax=%x\n", context->CXT_XAX),
                        dump_exception_info((GetExceptionInformation())->ExceptionRecord,
                                            context),
                        context->CXT_XAX = 0xcafebabe,
                        /* FIXME: should get a CONTINUE to work  */
                        EXCEPTION_CONTINUE_EXECUTION, EXCEPTION_EXECUTE_HANDLER) {
                print("Inside first handler\n");
            }
            print("At statement after 1st try-except\n");
            __try {
                initialize_registry_context();
                __asm {
                    mov   edx, 0xdeadbeef
                    call  edx
                }
                print("NEVER Inside 2nd try\n");
            } __except (print("Inside 2nd filter\n"), EXCEPTION_CONTINUE_SEARCH) {
                print("This should NOT be printed\n");
            }
        } __finally {
            print("Finally!\n");
        }
        print("NEVER At statement after 2nd try-finally\n");
    } __except (print("Inside 3rd filter\n"),
                exception = *(GetExceptionInformation())->ExceptionRecord,
                context = (GetExceptionInformation())->ContextRecord,
                dump_exception_info(&exception, context),
                (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
                    ? EXCEPTION_EXECUTE_HANDLER
                    : EXCEPTION_CONTINUE_SEARCH) {
        /* copy of exception can be used */
        print("Expected memory access violation, ignoring it!\n");
    }
    print("After exception handler\n");

    /* Security violation handled by exceptions here */
    /* Now we should start getting different results with code origins,
       because the addresses are normally readable, but now we impose our own policy

       -detect_mode should behave normally
          no exceptions are triggered

       -throw_exception -no_detect_mode
          should behave similarly to the handling of unreadable memory above
          (fake) exceptions should be generated claiming that badfunc is not executable
     */

    print("Attempting execution of badfunc\n");
    __try {
        __try {
            __try {
                initialize_registry_context();
                ((funcptr)badfunc)();
                print("DATA: At statement after exception\n");
            } __except (
                context = (GetExceptionInformation())->ContextRecord,
                print("DATA VIOLATION: Inside first filter eax=%x\n", context->CXT_XAX),
                dump_exception_info((GetExceptionInformation())->ExceptionRecord,
                                    context),
                EXCEPTION_EXECUTE_HANDLER) {
                print("DATA VIOLATION: Inside first handler\n");
            }
            print("DATA: At statement after 1st try-except\n");
            __try {
                initialize_registry_context();
                ((funcptr)badfunc)();
                print("DATA: Inside 2nd try\n");
            } __except (EXCEPTION_CONTINUE_SEARCH) {
                print("DATA: This should NOT be printed\n");
            }
        } __finally {
            print("DATA: Finally!\n");
        }
        print("DATA: At statement after 2nd try-finally\n");
    } __except (exception = *(GetExceptionInformation())->ExceptionRecord,
                context = (GetExceptionInformation())->ContextRecord,
                (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
                    ? EXCEPTION_EXECUTE_HANDLER
                    : EXCEPTION_CONTINUE_SEARCH) {
        print("DATA: Expected execution violation!\n");
        dump_exception_info(&exception, context);
    }
    print("DATA: After exception handler\n");
}

int
thread_func()
{
    __try {
        run_test();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        print("Should never have exception bubble up to thread function\n");
    }
    /* thread is hanging so just exit here */
    ExitThread(0);
    return 0;
}

int
main()
{
    HANDLE hThread;

    /* I tried just making a new stack and swapping to it but had trouble
     * w/ exception unwinding walking off the stack even though I put 0
     * in fs:0.  In any case, making a raw thread does the trick.
     */
    hThread =
        (HANDLE)nt_create_thread(GetCurrentProcess(), (threadfunc_t)thread_func, NULL,
                                 STACK_RESERVE, STACK_COMMIT, false, NULL, false);
    WaitForSingleObject(hThread, INFINITE);

    return 0;
}
