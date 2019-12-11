/* **********************************************************
 * Copyright (c) 2011-2019 Google, Inc.  All rights reserved.
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

/* note - include order matters */
#include <tchar.h>
#include "share.h"
#include "ntdll.h" /* just for typedefs and defines */
#include "processes.h"
#include "drmarker.h"
#include <AccCtrl.h>
#include <Aclapi.h>
#include <stdio.h>

/* detach.c(74) : warning C4055: 'type cast' : from data pointer
 *  'void *' to function pointer 'unsigned long (__stdcall *)(void *)'
 * (cf. drmarker.h)
 */
#pragma warning(disable : 4055)

#define PAGE_SIZE 0x1000
#define MIN_ALLOCATION_SIZE 0x10000

typedef NTSTATUS(NTAPI *NtCreateThreadType)(
    OUT PHANDLE ThreadHandle, IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes, IN HANDLE ProcessHandle,
    OUT PCLIENT_ID ClientId, IN PCONTEXT ThreadContext, IN PUSER_STACK UserStack,
    IN BOOLEAN CreateSuspended);
NtCreateThreadType NtCreateThread = NULL;
typedef NTSTATUS(NTAPI *NtCreateThreadExType)(
    OUT PHANDLE ThreadHandle, IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes, IN HANDLE ProcessHandle,
    IN LPTHREAD_START_ROUTINE StartAddress, IN LPVOID StartParameter,
    IN BOOL CreateSuspended, IN uint StackZeroBits, IN SIZE_T StackCommitSize,
    IN SIZE_T StackReserveSize, INOUT create_thread_info_t *thread_info);
NtCreateThreadExType NtCreateThreadEx = NULL;

#ifdef X64
typedef unsigned __int64 ptr_uint_t;
#    define CXT_XIP Rip
#    define CXT_XAX Rax
#    define CXT_XCX Rcx
#    define CXT_XDX Rdx
#    define CXT_XBX Rbx
#    define CXT_XSP Rsp
#    define CXT_XBP Rbp
#    define CXT_XSI Rsi
#    define CXT_XDI Rdi
/* It looks like both CONTEXT.Xmm0 and CONTEXT.FltSave.XmmRegisters[0] are filled in.
 * We use the latter so that we don't have to hardcode the index.
 */
#    define CXT_XMM(cxt, idx) ((dr_xmm_t *)&((cxt)->FltSave.XmmRegisters[idx]))
/* they kept the 32-bit EFlags field; sure, the upper 32 bits of Rflags
 * are undefined right now, but doesn't seem very forward-thinking. */
#    define CXT_XFLAGS EFlags
#else
typedef uint ptr_uint_t;
#    define CXT_XIP Eip
#    define CXT_XAX Eax
#    define CXT_XCX Ecx
#    define CXT_XDX Edx
#    define CXT_XBX Ebx
#    define CXT_XSP Esp
#    define CXT_XBP Ebp
#    define CXT_XSI Esi
#    define CXT_XDI Edi
#    define CXT_XFLAGS EFlags
/* This is not documented, but CONTEXT.ExtendedRegisters looks like fxsave layout.
 * Presumably there are no processors that have SSE but not FXSR
 * (we ASSERT on that in proc_init()).
 */
#    define FXSAVE_XMM0_OFFSET 160
#    define CXT_XMM(cxt, idx) \
        ((dr_xmm_t *)&((cxt)->ExtendedRegisters[FXSAVE_XMM0_OFFSET + (idx)*16]))
#endif

#define THREAD_START_ADDR IF_X64_ELSE(CXT_XCX, CXT_XAX)
#define THREAD_START_ARG IF_X64_ELSE(CXT_XDX, CXT_XBX)

PVOID WINAPI
dummy_func(LPVOID dummy_arg)
{
    return dummy_arg;
}

/* Gets kernel32!BaseThreadStartThunk which, unfortunately, isn't exported. */
/* Returns NULL on error. NOTE - on Vista there is no kernel32!BaseThreadStartThunk,
 * rather there is the moral equivalent ntdll!RtlUserThreadStart which is exported
 * (so doesn't require this convoluted lookup). */
static void *
get_kernel_thread_start_thunk()
{
    static void *start_address = NULL;
    DWORD platform = 0;

    get_platform(&platform);
    DO_ASSERT(platform != 0 && platform != PLATFORM_VISTA);
    if (platform >= PLATFORM_VISTA)
        return NULL;

    if (start_address == NULL) {
        HANDLE hThread = CreateThread(NULL, 0, dummy_func, NULL, CREATE_SUSPENDED, NULL);
        if (hThread != NULL) {
            CONTEXT cxt;
            cxt.ContextFlags = CONTEXT_FULL;
            if (GetThreadContext(hThread, &cxt)) {
                /* FIXME - would be a little more elegant to have a lock
                 * protecting start_address, but is not necessary for
                 * correctness as we'll always be writing the same value and
                 * the actual write itself should be atomic.  There are places
                 * in utils.c that really do need synchronization so if we
                 * ever add some synchronization mechanisms to share module
                 * might as well use here as well. */
                start_address = (void *)cxt.CXT_XIP;
#ifdef DEBUG
                {
                    TCHAR buf[MAX_PATH];
                    MEMORY_BASIC_INFORMATION mbi;
                    int i = 0;

                    memset(buf, 0, sizeof(buf));
                    VirtualQuery(start_address, &mbi, sizeof(mbi));
                    /* a module handle is just the allocation base of the
                     * module */
                    GetModuleFileName((HMODULE)mbi.AllocationBase, buf,
                                      BUFFER_SIZE_ELEMENTS(buf));
                    NULL_TERMINATE_BUFFER(buf);
                    /* can't believe there is no stristr */
                    while (buf[i] != _T('\0')) {
                        buf[i] = _totupper(buf[i]);
                        i++;
                    }
                    DO_ASSERT(_tcsstr(buf, _T("KERNEL32.DLL")) != NULL);
                }
#endif
            }
            /* don't terminate, otherwise won't be cleaned up on 2k */
            /* thread routine is empty so just resume */
            ResumeThread(hThread);
            CloseHandle(hThread);
        }
    }
    DO_ASSERT(start_address != NULL);
    return start_address;
}

/* returns NULL on error */
/* FIXME - is similar to core create_thread, but uses API routines where
 *     possible, could try to share. */
/* NOTE - stack_reserve and stack commit must be multiples of PAGE_SIZE and reserve
 *     should be at least 5 pages larger then commit */
/* NOTE - if !target_api && < win8, target thread routine can't exit by returning:
 *    instead it must terminate itself.
 * NOTE - caller or target thread routine is responsible for informing csrss (if
 *    necessary) and freeing the the thread stack (caller is informed of the stack
 *    via OUT remote_stack arg).
 * If arg_buf is non-null then the arg_buf_size bytes of arg_buf are copied onto
 * the new thread's stack (< win8) or into a new alloc (>= win8) and pointer to
 * them is passed as the argument to the start routine instead of arg.  For >= win8,
 * the arg_buf copy must be freed with NtFreeVirtualMemory by the caller.
 */
static HANDLE
nt_create_thread(HANDLE hProcess, PTHREAD_START_ROUTINE start_addr, void *arg,
                 const void *arg_buf, SIZE_T arg_buf_size, SIZE_T stack_reserve,
                 SIZE_T stack_commit, bool suspended, ptr_uint_t *tid, bool target_api,
                 bool target_64bit, void **remote_stack /* OUT */)
{
    HANDLE hThread = NULL;
    USER_STACK stack = { 0 };
    OBJECT_ATTRIBUTES oa;
    CLIENT_ID cid;
    CONTEXT context = { 0 };
    SIZE_T num_commit_bytes, code;
    unsigned long old_prot;
    void *p;
    SECURITY_DESCRIPTOR *sd = NULL;
    DWORD platform = 0;
    void *thread_arg = arg;
    BOOL wow64 = is_wow64(hProcess);

#ifndef X64
    DO_ASSERT(!target_64bit); /* Not supported. */
#endif

    get_platform(&platform);
    DO_ASSERT(platform != 0);

    if (platform >= PLATFORM_WIN_8) {
        /* NtCreateThread is not supported to we have to use NtCreateThreadEx,
         * which simplifies the stack but complicates arg_buf and uses undocumented
         * structures.
         */
        if (NtCreateThreadEx == NULL) {
            NtCreateThreadEx = (NtCreateThreadExType)GetProcAddress(
                GetModuleHandle(L"ntdll.dll"), "NtCreateThreadEx");
            if (NtCreateThreadEx == NULL)
                goto error;
        }
    } else if (NtCreateThread == NULL) {
        /* Don't need a lock, writing the pointer (4 [8 on x64] byte write) is atomic. */
        NtCreateThread = (NtCreateThreadType)GetProcAddress(GetModuleHandle(L"ntdll.dll"),
                                                            "NtCreateThread");
        if (NtCreateThread == NULL)
            goto error;
    }

    /* both stack size and stack reserve must be multiples of PAGE_SIZE */
    DO_ASSERT((stack_reserve & (PAGE_SIZE - 1)) == 0);
    DO_ASSERT((stack_commit & (PAGE_SIZE - 1)) == 0);
    /* We stick a non-committed page on each end just to be safe and windows
     * needs three pages at the end to properly handle end of expandable stack
     * case (wants to pass exception back to the app on overflow, so needs some
     * stack for that).  Plus we need an extra page for wow64 (PR 252008). */
    DO_ASSERT(stack_reserve >= stack_commit + (5 + (wow64 ? 1 : 0) * PAGE_SIZE));

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
    DO_ASSERT(code == ERROR_SUCCESS);

    InitializeObjectAttributes(&oa, NULL, OBJ_CASE_INSENSITIVE, NULL, sd);

    if (platform >= PLATFORM_WIN_8) {
        create_thread_info_t info = { 0 };
        TEB *teb;
        if (arg_buf != NULL) {
            SIZE_T written;
            void *arg_copy =
                VirtualAllocEx(hProcess, NULL, arg_buf_size, MEM_COMMIT, PAGE_READWRITE);
            if (arg_copy == NULL)
                goto error;
            if (!WriteProcessMemory(hProcess, arg_copy, arg_buf, arg_buf_size,
                                    &written) ||
                written != arg_buf_size) {
                goto error;
            }
            thread_arg = arg_copy;
        }
        info.struct_size = sizeof(info);
        info.client_id.flags =
            THREAD_INFO_ELEMENT_CLIENT_ID | THREAD_INFO_ELEMENT_UNKNOWN_2;
        info.client_id.buffer_size = sizeof(cid);
        info.client_id.buffer = &cid;
        /* We get STATUS_INVALID_PARAMETER unless we also ask for teb. */
        info.teb.flags = THREAD_INFO_ELEMENT_TEB | THREAD_INFO_ELEMENT_UNKNOWN_2;
        info.teb.buffer_size = sizeof(TEB *);
        info.teb.buffer = &teb;
        if (!NT_SUCCESS(NtCreateThreadEx(&hThread, THREAD_ALL_ACCESS, &oa, hProcess,
                                         start_addr, thread_arg, suspended ? TRUE : FALSE,
                                         0, 0, 0, &info))) {
            goto error;
        }
        if (tid != NULL)
            *tid = (ptr_uint_t)cid.UniqueThread;
    } else {
        stack.ExpandableStackBottom =
            VirtualAllocEx(hProcess, NULL,
                           /* we leave the top page MEM_FREE: we could reserve it instead
                            * and then adjust our get_stack_bounds() core assert */
                           stack_reserve - PAGE_SIZE, MEM_RESERVE, PAGE_READWRITE);
        if (remote_stack != NULL)
            *remote_stack = stack.ExpandableStackBottom;
        if (stack.ExpandableStackBottom == NULL)
            goto error;

        /* We provide non-committed boundary page on each side of the stack just to
         * be safe (note we will get a stack overflow exception if stack grows to
         * 3rd to last page of this region (xpsp2)). */
        stack.ExpandableStackBottom = ((byte *)stack.ExpandableStackBottom) + PAGE_SIZE;
        stack.ExpandableStackBase =
            ((byte *)stack.ExpandableStackBottom) + stack_reserve - (2 * PAGE_SIZE);
        /* PR 252008: WOW64's initial APC uses the stack base, ignoring CONTEXT.Esp,
         * so we put an extra page in place for the nudge arg.  It should be
         * freed w/ no problems since the Bottom's region is freed.
         * An alternative is a separate allocation and setting NUDGE_FREE_ARG,
         * but the caller is the one who knows the structure of the arg.
         */
        if (wow64)
            stack.ExpandableStackBase = ((byte *)stack.ExpandableStackBase) - PAGE_SIZE;

        stack.ExpandableStackLimit = ((byte *)stack.ExpandableStackBase) - stack_commit;
        num_commit_bytes = stack_commit + PAGE_SIZE;
        p = ((byte *)stack.ExpandableStackBase) - num_commit_bytes;
        if (wow64)
            num_commit_bytes += PAGE_SIZE;
        p = VirtualAllocEx(hProcess, p, num_commit_bytes, MEM_COMMIT, PAGE_READWRITE);
        if (p == NULL)
            goto error;
        if (!VirtualProtectEx(hProcess, p, PAGE_SIZE, PAGE_READWRITE | PAGE_GUARD,
                              &old_prot))
            goto error;

        /* set the context: initialize with our own */
        context.ContextFlags = CONTEXT_FULL;
        GetThreadContext(GetCurrentThread(), &context);

        /* set esp */
        context.CXT_XSP = (ptr_uint_t)stack.ExpandableStackBase;
        /* On vista, the kernel sets esp a random number of bytes in from the base of
         * the stack as part of the stack ASLR.  RtlUserThreadStart assumes that
         * esp will be set at least 8 bytes into the region by writing to esp+4 and
         * esp+8. So we need to move up by at least 8 bytes.  The smallest offset
         * I've seen is 56 bytes so we'll go with that as there could be another
         * place assuming there's room above esp. We do it irregardless of whether we
         * target api or not in case it's relied on elsewhere. */
#define VISTA_THREAD_STACK_PAD 56
        if (platform >= PLATFORM_VISTA) {
            context.CXT_XSP -= VISTA_THREAD_STACK_PAD;
        }
        /* Anticipating x64 we align to 16 bytes everywhere */
#define STACK_ALIGNMENT 16
        /* write the argument buffer to the stack */
        if (arg_buf != NULL) {
            SIZE_T written;
            if (wow64) { /* PR 252008: see above */
                thread_arg = (void *)(((byte *)stack.ExpandableStackBase) + PAGE_SIZE -
                                      arg_buf_size);
            } else {
                context.CXT_XSP -= ALIGN_FORWARD(arg_buf_size, STACK_ALIGNMENT);
                thread_arg = (void *)context.CXT_XSP;
            }
            if (!WriteProcessMemory(hProcess, thread_arg, arg_buf, arg_buf_size,
                                    &written) ||
                written != arg_buf_size) {
                goto error;
            }
            if (platform >= PLATFORM_VISTA) {
                /* pad after our argument so RtlUserThreadStart won't clobber it */
                context.CXT_XSP -= VISTA_THREAD_STACK_PAD;
            }
        }

        /* set eip and argument */
        if (target_api) {
            if (platform >= PLATFORM_VISTA) {
                context.CXT_XIP = (ptr_uint_t)GetProcAddress(
                    GetModuleHandle(L"ntdll.dll"), "RtlUserThreadStart");
            } else {
                context.CXT_XIP = (ptr_uint_t)get_kernel_thread_start_thunk();
            }
            /* For kernel32!BaseThreadStartThunk and ntdll!RltUserThreadStartThunk
             * Eax contains the address of the thread routine and Ebx the arg */
            context.THREAD_START_ADDR = (ptr_uint_t)start_addr;
            context.THREAD_START_ARG = (ptr_uint_t)thread_arg;
        } else {
            void *buf[2];
            BOOL res;
            SIZE_T written;
            /* directly targeting the start_address */
            context.CXT_XIP = (ptr_uint_t)start_addr;
            context.CXT_XSP -= sizeof(buf);
            /* set up arg on stack, give NULL return address */
            buf[1] = thread_arg;
            buf[0] = NULL;
            res = WriteProcessMemory(hProcess, (void *)context.CXT_XSP, &buf, sizeof(buf),
                                     &written);
            if (!res || written != sizeof(buf)) {
                goto error;
            }
        }
        if (context.CXT_XIP == 0) {
            DO_ASSERT(false);
            goto error;
        }

        /* NOTE - CreateThread passes NULL for object attributes so despite Nebbet
         * must be optional (checked NTsp6a, XPsp2). We don't pass NULL so we can
         * specify the security descriptor. */
        if (!NT_SUCCESS(NtCreateThread(&hThread, THREAD_ALL_ACCESS, &oa, hProcess, &cid,
                                       &context, &stack,
                                       (byte)(suspended ? TRUE : FALSE)))) {
            goto error;
        }

        if (tid != NULL)
            *tid = (ptr_uint_t)cid.UniqueThread;
    }

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
        if (remote_stack != NULL)
            *remote_stack = NULL;
    }
    DO_ASSERT(hThread == NULL);
    hThread = NULL; /* just to be safe */
    SetLastError(ERROR_INVALID_HANDLE);
    goto exit;
}

/* flag defines for create_remote_thread */
/* Use ntdll!NtCreateThread instead of kernel32!CreateThread to allow creating
 * threads in a different session (case 872). */
#define CREATE_REMOTE_THREAD_USE_NT 0x01

/* If ...USE_NT then target the natively created thread to the same entry point as
 * the api routines would (kernel32!BaseThreadStartThunk for pre-Vista and
 * ntdll!RtlUserThreadStart for Vista). This allows the target thread to exit by
 * returning from its thread function.  NOTE - be very careful about who frees the
 * new thread's stack. On NT and 2k returning from the thread function will free the
 * stack while on >= XP it won't. */
#define CREATE_REMOTE_THREAD_TARGET_API 0x02

/* 64kb, same as allocation granularity so is as small as we can get */
#define STACK_RESERVE 0x10000
/* 12kb, matches current core stack size, note can expand to
 * STACK_RESERVE - (5 * PAGE_SIZE), i.e. 44kb */
#define STACK_COMMIT 0x3000

/* returns NULL on failure */
static HANDLE
create_remote_thread(HANDLE hProc, PTHREAD_START_ROUTINE pfnThreadRtn, void *arg,
                     const void *arg_buf, SIZE_T arg_buf_size, uint flags,
                     void **remote_stack /* OUT */)
{
    HANDLE hThread = NULL;
    if (remote_stack != NULL)
        *remote_stack = NULL;
    if (TEST(CREATE_REMOTE_THREAD_USE_NT, flags)) {
        /* FIXME - should we allow the caller to specify the stack sizes?
         * we already just use defaults if we're using CreateRemoteThread */
        hThread = nt_create_thread(hProc, pfnThreadRtn, arg, arg_buf, arg_buf_size,
                                   STACK_RESERVE, STACK_COMMIT, false, NULL,
                                   TEST(CREATE_REMOTE_THREAD_TARGET_API, flags),
                                   false /* !64-bit */, remote_stack);
    } else {
        DO_ASSERT(false && "not tested (not currently used)");
        DO_ASSERT(false && "if used for nudge need update flags for stack/arg freeing");
        DO_ASSERT(arg_buf == NULL &&
                  "no support for buffer arguments to "
                  "create_remote_thread when not using NT");

        hThread = CreateRemoteThread(hProc, NULL, 0, pfnThreadRtn, arg, 0, NULL);
    }
    return hThread;
}

/* define this here so that clients don't need to know about dr_marker_t */
DWORD
get_dr_marker(process_id_t ProcessID, dr_marker_t *marker,
              hotp_policy_status_table_t **hotp_status, int *found);

DWORD
nudge_dr(process_id_t pid, BOOL allow_upgraded_perms, DWORD timeout_ms,
         nudge_arg_t *nudge_arg)
{
    DWORD res = ERROR_SUCCESS;
    int found;
    dr_marker_t marker;
    PTHREAD_START_ROUTINE pfnThreadRtn;
    /* FIXME: case 7038 */
    /* It is not safe to convert our routines to a PTHREAD_START_ROUTINE?
     * That one is expected to be a
     * DWORD (WINAPI *PTHREAD_START_ROUTINE)(LPVOID lpThreadParameter);
     * and if we are using the wrong calling convention the application may fail
     * MSDN notes that this is common but crashes only on 64-bit
     */

    HANDLE hThread = NULL, hProcess = NULL;
    void *remote_stack = NULL;

    DWORD platform = 0;
    get_platform(&platform);
    DO_ASSERT(platform != 0);

    /* Open the process handle before getting the drmarker, is slight perf.
     * hit if process turns out not to be running under DR, but we avoid the
     * race of the process exiting and it's id getting recycled, the os won't
     * recycle the id till we free our handle */
    hProcess = OpenProcess(PROCESS_QUERY_INFORMATION + PROCESS_VM_WRITE +
                               PROCESS_VM_READ + PROCESS_VM_OPERATION +
                               PROCESS_CREATE_THREAD + READ_CONTROL + SYNCHRONIZE,
                           FALSE, (DWORD)pid);
    res = GetLastError();
    if (hProcess == NULL) {
        if (allow_upgraded_perms) {
            acquire_privileges();
            hProcess = OpenProcess(PROCESS_QUERY_INFORMATION + PROCESS_VM_WRITE +
                                       PROCESS_VM_READ + PROCESS_VM_OPERATION +
                                       PROCESS_CREATE_THREAD + READ_CONTROL + SYNCHRONIZE,
                                   FALSE, (DWORD)pid);
            res = GetLastError();
            release_privileges();
            if (hProcess == NULL) {
                if (res != ERROR_SUCCESS)
                    res = ERROR_DETACH_ERROR;
                goto exit;
            }
        } else {
            if (res != ERROR_SUCCESS)
                res = ERROR_DETACH_ERROR;
            goto exit;
        }
    }

    res = get_dr_marker(pid, &marker, NULL, &found);
    /* first make sure the PID is running under dr */
    if (res != ERROR_SUCCESS || found != DR_MARKER_FOUND) {
        res = ERROR_MOD_NOT_FOUND;
        goto exit;
    }

    pfnThreadRtn = (PTHREAD_START_ROUTINE)marker.dr_generic_nudge_target;

    if (pfnThreadRtn != NULL) { /* Fix for case 5464. */
        /* We use the native api to create the thread (CREATE_REMOTE_THREAD_USE_NT)
         * to avoid session id issues.  FIXME - we don't really need to TARGET_API
         * anymore since the nudge routine never returns now (which could simplify
         * the core nudge thread detection a little).  Note - if we stop using USE_NT
         * we need to update the stack freeing code below and change the nudge flags. */
        hThread = create_remote_thread(
            hProcess, pfnThreadRtn, NULL, (void *)nudge_arg, sizeof(nudge_arg_t),
            CREATE_REMOTE_THREAD_USE_NT | CREATE_REMOTE_THREAD_TARGET_API, &remote_stack);
        if (hThread == NULL) {
            res = GetLastError();
            goto exit;
        }
    } else {
        hThread = NULL;
        res = ERROR_INVALID_FUNCTION;
        goto exit;
    }

    res = WaitForSingleObject(hThread, timeout_ms);

    if (res == WAIT_FAILED) {
        res = GetLastError();
    } else if (res == WAIT_TIMEOUT) {
        res = ERROR_TIMEOUT;
    } else {
        res = ERROR_SUCCESS;
        /* We have a general problem on how to free the application stack for nudges.
         * Currently the app/os will never free a nudge thread's app stack:
         *  On NT and 2k ExitThread would normally free the app stack, but we always
         *   terminate nudge threads instead of allowing them to return and exit normally.
         *  On XP and 2k3 none of our nudge creation routines inform csrss of the new
         *   thread(which is who typically frees the stacks) assuming we don't use switch
         *   to using CreateRemoteThread() here.
         *  On Vista we don't use NtCreateThreadEx to create the nudge threads so the
         *   kernel doesn't free the stack.
         * As such we are left with two options: free the app stack here (nudgee free) or
         * have the nudge thread creator free the app stack (nudger free). We use
         * nudge flags to specify which we are using. */
        if (TEST(NUDGE_NUDGER_FREE_STACK, nudge_arg->flags) && remote_stack != NULL) {
            VirtualFreeEx(hProcess, remote_stack, 0, MEM_RELEASE);
        }
    }

exit:
    if (hThread != NULL)
        CloseHandle(hThread);
    if (hProcess != NULL)
        CloseHandle(hProcess);
    return res;
}

/* Detaches from the specified process by creating a thread in the target
 * process that directly targets the detach routine.
 */
DWORD
detach(process_id_t pid, BOOL allow_upgraded_perms, DWORD timeout_ms)
{
    return generic_nudge(pid, allow_upgraded_perms, NUDGE_GENERIC(detach), 0, NULL,
                         timeout_ms);
}

/* generic nudge DR, action mask determines which actions will be executed */
DWORD
generic_nudge(process_id_t pid, BOOL allow_upgraded_perms, DWORD action_mask,
              client_id_t client_id /* optional */, uint64 client_arg /* optional */,
              DWORD timeout_ms)
{
    nudge_arg_t arg = { 0 };
    DWORD platform = 0;
    get_platform(&platform);
    DO_ASSERT(platform != 0);
    arg.version = NUDGE_ARG_CURRENT_VERSION;
    arg.nudge_action_mask = action_mask;
    /* default flags */
    if (platform >= PLATFORM_WIN_8) {
        /* i#1309: We use NtCreateThreadEx which is different: */
        /* The kernel owns and frees the stack. */
        arg.flags |= NUDGE_NUDGER_FREE_STACK;
        /* The arg is placed in a new kernel alloc. */
        arg.flags |= NUDGE_FREE_ARG;
    }
    arg.client_id = client_id;
    arg.client_arg = client_arg;
    return nudge_dr(pid, allow_upgraded_perms, timeout_ms, &arg);
}

/* Loads the specified dll in process pid and waits on the loading thread, does
 * not free the dll once the loading thread exits. Usual usage is for the
 * loaded dll to do something in its DllMain. If you do not want the dll to
 * stay loaded its DllMain should return false. To unload a dll from a process
 * later, inject another dll whose dll main unloads that dll and then returns
 * false. If loading_thread != NULL returns a handle to the loading thread (dll
 * could call FreeLibraryAndExitThread on itself in its dll main to return a
 * value out via the exit code). inject_dll provides no way to pass arguments
 * in to the dll. */
DWORD
inject_dll(process_id_t pid, const WCHAR *dll_name, BOOL allow_upgraded_perms,
           DWORD timeout_ms, PHANDLE loading_thread)
{
    DWORD res;
    PTHREAD_START_ROUTINE pfnThreadRtn = NULL;
    HANDLE hThread = NULL, hProcess = NULL, file_tmp = NULL;
    void *remote_stack = NULL;
    DWORD platform = 0;
    get_platform(&platform);
    DO_ASSERT(platform != 0);

    if (loading_thread != NULL)
        *loading_thread = NULL;

    file_tmp =
        CreateFile(dll_name, 0, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_tmp == INVALID_HANDLE_VALUE)
        return ERROR_FILE_NOT_FOUND;
    CloseHandle(file_tmp);

    hProcess = OpenProcess(PROCESS_QUERY_INFORMATION + PROCESS_VM_WRITE +
                               PROCESS_VM_READ + PROCESS_VM_OPERATION +
                               PROCESS_CREATE_THREAD + READ_CONTROL + SYNCHRONIZE,
                           FALSE, (DWORD)pid);
    if (hProcess == NULL) {
        if (allow_upgraded_perms) {
            acquire_privileges();
            hProcess = OpenProcess(PROCESS_QUERY_INFORMATION + PROCESS_VM_WRITE +
                                       PROCESS_VM_READ + PROCESS_VM_OPERATION +
                                       PROCESS_CREATE_THREAD + READ_CONTROL + SYNCHRONIZE,
                                   FALSE, (DWORD)pid);
            release_privileges();
            if (hProcess == NULL) {
                return GetLastError();
            }
        } else {
            return GetLastError();
        }
    }

    pfnThreadRtn = (PTHREAD_START_ROUTINE)GetProcAddress(
        GetModuleHandle(TEXT("Kernel32")), "LoadLibraryW");
    if (pfnThreadRtn == NULL) {
        res = GetLastError();
        goto exit;
    }
    /* USE_NT to avoid session id issues, TARGET_API so LoadLibrary can return, if
     * stop using NT need to update stack freeing case below and allocate space to
     * hold the remote library name ourselves like we used to. */
    hThread = create_remote_thread(
        hProcess, pfnThreadRtn, NULL, dll_name, sizeof(WCHAR) * (1 + wcslen(dll_name)),
        CREATE_REMOTE_THREAD_USE_NT | CREATE_REMOTE_THREAD_TARGET_API, &remote_stack);
    if (hThread == NULL) {
        res = GetLastError();
        goto exit;
    }

    res = WaitForSingleObject(hThread, timeout_ms);
    if (res == WAIT_FAILED) {
        res = GetLastError();
        goto exit;
    } else if (res == WAIT_TIMEOUT) {
        res = ERROR_TIMEOUT;
        goto exit;
    }

    /* thread has finished */
    /* On NT and 2k the remote stack will be freed by the remote thread exiting
     * (returning from LoadLibrary or calling ExitThread), though not if it terminates
     * itself (which it shouldn't do, lock issues etc.).  On XP and higher the stack
     * won't be freed since we didn't inform csrss of this thread so we free here. */
    if (remote_stack != NULL && platform != PLATFORM_WIN_NT_4 &&
        platform != PLATFORM_WIN_2000) {
        VirtualFreeEx(hProcess, remote_stack, 0, MEM_RELEASE);
    }
    res = ERROR_SUCCESS;

exit:
    if (hThread != NULL) {
        if (loading_thread != NULL) {
            *loading_thread = hThread;
        } else {
            CloseHandle(hThread);
        }
    }
    if (hProcess != NULL)
        CloseHandle(hProcess);

    return res;
}
