/* **********************************************************
 * Copyright (c) 2010-2021 Google, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */

/*
 * ntdll.c
 * Routines for calling Windows system calls via the ntdll.dll wrappers.
 *
 * This file is used by the main library, the preinject library, and the
 * standalone injector.
 */
#include "configure.h"
#ifdef NOT_DYNAMORIO_CORE
#    define ASSERT(x)
#    define ASSERT_CURIOSITY(x)
#    define ASSERT_NOT_REACHED()
#    define ASSERT_NOT_IMPLEMENTED(x)
#    define DODEBUG(x)
#    define DOCHECK(n, x)
#    define DEBUG_DECLARE(x)
#    pragma warning(disable : 4210) // nonstd extension: function given file scope
#    pragma warning(disable : 4204) // nonstd extension: non-constant aggregate
                                    // initializer
#    define INVALID_FILE INVALID_HANDLE_VALUE
#    define snprintf _snprintf
#    include <stdio.h> /* _snprintf */
#else
/* we include globals.h mainly for ASSERT, even though we're
 * used by preinject.
 * preinject just defines its own d_r_internal_error!
 */
#    include "../globals.h"
#    include "../module_shared.h"
#endif

/* We have to hack away things we use here that won't work for non-core */
#if defined(NOT_DYNAMORIO_CORE_PROPER) || defined(NOT_DYNAMORIO_CORE)
#    undef ASSERT_OWN_NO_LOCKS
#    define ASSERT_OWN_NO_LOCKS() /* who cares if not the core */
#endif

#include "ntdll.h"
#ifndef NOT_DYNAMORIO_CORE
#    include "os_private.h"
#endif

#include <wchar.h> /* _snwprintf */

/* WARNING: these routines use the Native API, an undocumented API
 * exported by ntdll.dll.
 * It could change without warning with a new version of Windows.
 */

/* FIXME : combine NTPRINT with NTLOG */
/* must turn on VERBOSE in inject_shared.c as well since we're now
 * using display_verbose_message() -- FIXME: link them automatically */
#if defined(NOT_DYNAMORIO_CORE_PROPER) || defined(NOT_DYNAMORIO_CORE)
#    define VERBOSE 0
#else
#    define VERBOSE 0
#endif

#if VERBOSE
/* in inject_shared.c: must turn on VERBOSE=1 there as well */
void
display_verbose_message(char *format, ...);
#    define NTPRINT(...) display_verbose_message(__VA_ARGS__)
#else
#    define NTPRINT(...)
#endif

/* i#437 support ymm */
uint context_xstate = 0;

/* needed for injector and preinject, to avoid them requiring asm and syscalls */
#if defined(NOT_DYNAMORIO_CORE_PROPER) || defined(NOT_DYNAMORIO_CORE)
/* use ntdll wrappers for simplicity, to keep ntdll.c standalone */
#    define GET_SYSCALL(name, ...) GET_NTDLL(Nt##name, (__VA_ARGS__))
#    define GET_RAW_SYSCALL GET_SYSCALL
#    define NT_SYSCALL(name, ...) Nt##name(__VA_ARGS__)
#    define NTLOG(...)
#else
#    define NTLOG LOG
/* Our own syscall wrapper to avoid relying on ntdll, for 4 reasons:
 * 1) Maximum interoperability w/ ntdll hookers
 * 2) Security by avoiding being disabled via ntdll being messed up
 * 3) Early injection: although ntdll is already in the address space,
 *    this way we don't need the loader
 * 4) Easier trampolines on ntdll syscall wrappers for handling native code
 *    (don't have to worry about DR syscalls going through the trampolines)
 *
 * For now we only use our own wrapper for syscalls in the app-relevant array.
 * When we add the rest we can:
 *   1) leave out of array, and dynamically determine
 *      (ideally using our own version of GetProcAddress)
 *   2) add to array, then everything's consistent
 *   3) eliminate array completely and always dynamically determine
 * But, relying on dynamic determination means we won't work if there's a hook
 * already placed there (losing a big advantage of our own wrappers), and
 * dynamically determining doesn't give us that much more independence --
 * we still need to manually verify each new ntdll for other types of
 * syscall changes (new syscalls we care about, semantic changes, etc.)
 */
/* decides which of dynamorio_syscall_{int2e,sysenter,wow64} to use */
static enum {
    DR_SYSCALL_INT2E,
    DR_SYSCALL_SYSENTER,
    DR_SYSCALL_SYSCALL,
    DR_SYSCALL_WOW64,
} dr_which_syscall_t;

/* For x64 "raw syscalls", i.e., those we call directly w/o invoking the
 * ntdll wrapper routine, we play some games with types to work more
 * easily w/ the x64 calling convention:
 */
#    define GET_RAW_SYSCALL(name, arg1, ...)                         \
        GET_NTDLL(Nt##name, (arg1, __VA_ARGS__));                    \
        typedef NTSTATUS name##_type(int sysnum, arg1, __VA_ARGS__); \
        typedef NTSTATUS name##_dr_type(int sys_enum, __VA_ARGS__, arg1)

#    define GET_SYSCALL(name, ...)          \
        GET_NTDLL(Nt##name, (__VA_ARGS__)); \
        typedef NTSTATUS name##_type(int sysnum, __VA_ARGS__)

/* FIXME - since it doesn't vary we could have a variable to store the dr
 * syscall routine to use, but would be yet another function pointer in
 * our data segment... */
/* We use the wrappers till the native_exec Nt hooks go in (at which point
 * the options have been read) so that we can have sygate compatibility as a
 * runtime option. */
/* For X64 sycall we need the 1st arg last to preserve the rest in their
 * proper registers.  If we ever support 0-arg syscalls here we'll
 * need a separate macro for those.
 * Any syscall called using this macro must be declared with GET_RAW_SYSCALL
 * rather than GET_SYSCALL to get the types to match up.
 */
/* i#1011: We usually use NT_SYSCALL to invoke a system call. However,
 * for system calls that do not exist in older Windows, e.g. NtOpenKeyEx,
 * we use NT_RAW_SYSCALL to avoid static link and build failure.
 */
#    define NT_RAW_SYSCALL(name, arg1, ...)                                             \
        ((dr_which_syscall_t == DR_SYSCALL_WOW64)                                       \
             ? (!syscall_uses_edx_param_base()                                          \
                    ? ((name##_type *)dynamorio_syscall_wow64_noedx)(SYS_##name, arg1,  \
                                                                     __VA_ARGS__)       \
                    : (((name##_type *)dynamorio_syscall_wow64)(SYS_##name, arg1,       \
                                                                __VA_ARGS__)))          \
             : ((IF_X64_ELSE(dr_which_syscall_t == DR_SYSCALL_SYSCALL, false))          \
                    ? ((name##_dr_type *)IF_X64_ELSE(dynamorio_syscall_syscall, NULL))( \
                          SYS_##name, __VA_ARGS__, arg1)                                \
                    : (((name##_type *)((dr_which_syscall_t == DR_SYSCALL_SYSENTER)     \
                                            ? (DYNAMO_OPTION(dr_sygate_sysenter)        \
                                                   ? dynamorio_syscall_sygate_sysenter  \
                                                   : dynamorio_syscall_sysenter)        \
                                            : (DYNAMO_OPTION(dr_sygate_int)             \
                                                   ? dynamorio_syscall_sygate_int2e     \
                                                   : dynamorio_syscall_int2e)))(        \
                          syscalls[SYS_##name], arg1, __VA_ARGS__))))

#    define NT_SYSCALL(name, arg1, ...)                        \
        (nt_wrappers_intercepted ? Nt##name(arg1, __VA_ARGS__) \
                                 : NT_RAW_SYSCALL(name, arg1, __VA_ARGS__))

/* check syscall numbers without using any heap */
#    ifdef X64
#        define SYSNUM_OFFS 4
#    else
#        define SYSNUM_OFFS 1
#    endif
#    define CHECK_SYSNUM_AT(pc, idx)                                                   \
        ASSERT(pc != NULL &&                                                           \
               (*((int *)((pc) + SYSNUM_OFFS)) == syscalls[idx] || ALLOW_HOOKER(pc) || \
                (idx == SYS_TestAlert && *(uint *)(pc) == 0xe9505050))); /* xref 9288 */
/* assuming relative CTI's are the only one's used by hookers */
#    define ALLOW_HOOKER(pc)                           \
        (*(unsigned char *)(pc) == JMP_REL32_OPCODE || \
         *(unsigned char *)(pc) == CALL_REL32_OPCODE)
/* FIXME: we'll evaluate pc multiple times in the above macro */

static void
tls_exit(void);

#endif /* !NOT_DYNAMORIO_CORE_PROPER */

/* cached value */
static PEB *own_peb = NULL;

/****************************************************************************
 * Defines only needed internally to this file
 */

/* TlsSlots offset is hardcoded into kernel32!TlsGetValue as 0xe10 on all
 * 32-bit platforms we've seen, 0x1480 for 64-bit:
 */
#ifdef X64
#    define TEB_TLS64_OFFSET 0x1480
#else
#    define TEB_TLS64_OFFSET 0xe10
#endif

/***************************************************************************
 * declarations for ntdll exports shared by several routines in this file
 */

GET_NTDLL(NtQueryInformationProcess,
          (IN HANDLE ProcessHandle, IN PROCESSINFOCLASS ProcessInformationClass,
           OUT PVOID ProcessInformation, IN ULONG ProcessInformationLength,
           OUT PULONG ReturnLength OPTIONAL));

GET_NTDLL(NtQueryInformationFile,
          (IN HANDLE FileHandle, OUT PIO_STATUS_BLOCK IoStatusBlock,
           OUT PVOID FileInformation, IN ULONG FileInformationLength,
           IN FILE_INFORMATION_CLASS FileInformationClass));

GET_NTDLL(NtQuerySection,
          (IN HANDLE SectionHandle, IN SECTION_INFORMATION_CLASS SectionInformationClass,
           OUT PVOID SectionInformation, IN ULONG SectionInformationLength,
           OUT PULONG ResultLength OPTIONAL));

GET_NTDLL(NtQueryInformationToken,
          (IN HANDLE TokenHandle, IN TOKEN_INFORMATION_CLASS TokenInformationClass,
           OUT PVOID TokenInformation, IN ULONG TokenInformationLength,
           OUT PULONG ReturnLength));

/* routines that we may hook if specified in
 * syscall_requires_action[], all new routines can use GET_SYSCALL
 * instead of GET_NTDLL if we provide the syscall numbers - see
 * comments in GET_SYSCALL definition.
 */

GET_RAW_SYSCALL(QueryVirtualMemory, IN HANDLE ProcessHandle, IN const void *BaseAddress,
                IN MEMORY_INFORMATION_CLASS MemoryInformationClass,
                OUT PVOID MemoryInformation, IN SIZE_T MemoryInformationLength,
                OUT PSIZE_T ReturnLength OPTIONAL);

GET_RAW_SYSCALL(UnmapViewOfSection, IN HANDLE ProcessHandle, IN PVOID BaseAddress);

GET_RAW_SYSCALL(CreateSection, OUT PHANDLE SectionHandle, IN ACCESS_MASK DesiredAccess,
                IN POBJECT_ATTRIBUTES ObjectAttributes,
                IN PLARGE_INTEGER SectionSize OPTIONAL, IN ULONG Protect,
                IN ULONG Attributes, IN HANDLE FileHandle);

GET_RAW_SYSCALL(OpenSection, OUT PHANDLE SectionHandle, IN ACCESS_MASK DesiredAccess,
                IN POBJECT_ATTRIBUTES ObjectAttributes);

GET_RAW_SYSCALL(AllocateVirtualMemory, IN HANDLE ProcessHandle, IN OUT PVOID *BaseAddress,
                IN ULONG ZeroBits, IN OUT PSIZE_T AllocationSize, IN ULONG AllocationType,
                IN ULONG Protect);

GET_RAW_SYSCALL(FreeVirtualMemory, IN HANDLE ProcessHandle, IN OUT PVOID *BaseAddress,
                IN OUT PSIZE_T FreeSize, IN ULONG FreeType);

GET_RAW_SYSCALL(ProtectVirtualMemory, IN HANDLE ProcessHandle, IN OUT PVOID *BaseAddress,
                IN OUT PSIZE_T ProtectSize, IN ULONG NewProtect, OUT PULONG OldProtect);

GET_RAW_SYSCALL(QueryInformationThread, IN HANDLE ThreadHandle,
                IN THREADINFOCLASS ThreadInformationClass, OUT PVOID ThreadInformation,
                IN ULONG ThreadInformationLength, OUT PULONG ReturnLength OPTIONAL);

/* CreateFile is defined CreateFileW (Unicode) or CreateFileA (ANSI),
 * undefine here for system call.
 */
#undef CreateFile
GET_RAW_SYSCALL(CreateFile, OUT PHANDLE FileHandle, IN ACCESS_MASK DesiredAccess,
                IN POBJECT_ATTRIBUTES ObjectAttributes,
                OUT PIO_STATUS_BLOCK IoStatusBlock,
                IN PLARGE_INTEGER AllocationSize OPTIONAL, IN ULONG FileAttributes,
                IN ULONG ShareAccess, IN ULONG CreateDisposition, IN ULONG CreateOptions,
                IN PVOID EaBuffer OPTIONAL, IN ULONG EaLength);

GET_RAW_SYSCALL(CreateKey, OUT PHANDLE KeyHandle, IN ACCESS_MASK DesiredAccess,
                IN POBJECT_ATTRIBUTES ObjectAttributes, IN ULONG TitleIndex,
                IN PUNICODE_STRING Class OPTIONAL, IN ULONG CreateOptions,
                OUT PULONG Disposition OPTIONAL);

GET_RAW_SYSCALL(OpenKey, OUT PHANDLE KeyHandle, IN ACCESS_MASK DesiredAccess,
                IN POBJECT_ATTRIBUTES ObjectAttributes);

GET_RAW_SYSCALL(SetInformationFile, IN HANDLE FileHandle,
                OUT PIO_STATUS_BLOCK IoStatusBlock, IN PVOID FileInformation,
                IN ULONG FileInformationLength,
                IN FILE_INFORMATION_CLASS FileInformationClass);

/* the same structure as _CONTEXT_EX in winnt.h */
typedef struct _context_chunk_t {
    LONG offset;
    DWORD length;
} context_chunk_t;

/* the same structure as _CONTEXT_CHUNK in winnt.h */
typedef struct _context_ex_t {
    context_chunk_t all;
    context_chunk_t legacy;
    context_chunk_t xstate;
} context_ex_t;

/* XXX, the function below can be statically-linked if all versions of
 * ntdll have the corresponding routine, which need to be checked, so we use
 * get_proc_address to get instead here.
 */
typedef int(WINAPI *ntdll_RtlGetExtendedContextLength_t)(DWORD, int *);
typedef int(WINAPI *ntdll_RtlInitializeExtendedContext_t)(PVOID, DWORD, context_ex_t **);
typedef CONTEXT *(WINAPI *ntdll_RtlLocateLegacyContext_t)(context_ex_t *, DWORD);
ntdll_RtlGetExtendedContextLength_t ntdll_RtlGetExtendedContextLength = NULL;
ntdll_RtlInitializeExtendedContext_t ntdll_RtlInitializeExtendedContext = NULL;
ntdll_RtlLocateLegacyContext_t ntdll_RtlLocateLegacyContext = NULL;

#if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)
/* Nt* routines that are not available on all versions of Windows */
typedef NTSTATUS(WINAPI *NtGetNextThread_t)(__in HANDLE ProcessHandle,
                                            __in HANDLE ThreadHandle,
                                            __in ACCESS_MASK DesiredAccess,
                                            __in ULONG HandleAttributes, __in ULONG Flags,
                                            __out PHANDLE NewThreadHandle);
NtGetNextThread_t NtGetNextThread;
#endif

/***************************************************************************
 * Implementation
 */

#if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)
/* for Sygate 5441 compatibility hack, we need a tls slot for NT_SYSCALL when
 * using sysenter system calls */
uint sysenter_tls_offset = 0xffffffff; /* something that will fault */
/* will be set to false once the options are read but before the native_exec
 * Nt* hooks are put in. Till then lets NT_SYSCALL know it's safe to call via
 * the wrappers for Sygate compatibility before the option string is read in. */
static bool nt_wrappers_intercepted = true;

void
syscalls_init_options_read()
{
    if (DYNAMO_OPTION(dr_sygate_sysenter)) {
        tls_alloc(false /* don't grab lock */, &sysenter_tls_offset);
    }
    nt_wrappers_intercepted = false;
}

static int
syscalls_init_get_num(HANDLE ntdllh, int sys_enum)
{
    app_pc wrapper;
    ASSERT(ntdllh != NULL);
    /* We can't check syscalls[] for SYSCALL_NOT_PRESENT b/c it's not set up yet */
    /* d_r_get_proc_address() does invoke NtQueryVirtualMemory, but we go through the
     * ntdll wrapper for that syscall and thus it works this early.
     */
    wrapper = (app_pc)d_r_get_proc_address(ntdllh, syscall_names[sys_enum]);
    if (wrapper != NULL && !ALLOW_HOOKER(wrapper))
        return *((int *)((wrapper) + SYSNUM_OFFS));
    else
        return -1;
}

/* Called very early, prior to any system call use by us, making error
 * reporting problematic once we have all syscalls requiring this!
 * See windows_version_init() comments.
 * The other problem w/ error reporting is that other code assumes
 * that things are initialized -- that's all fixed now, with stats, dcontext,
 * etc. checked for NULL in all the right places.
 */
bool
syscalls_init()
{
    /* Determine which syscall routine to use
     * We don't have heap available yet (no syscalls yet!) so
     * we can't decode easily.
     * FIXME: for app syscalls, we wait until we see one so we know
     * the method being used -- should we move that decision up, since
     * we're checking here for DR?
     */
    /* pick a syscall that is unlikely to be hooked, ref case 5217 Sygate
     * requires all int system call to occur in ntdll.dll or sysfer.dll
     * so we borrow the int 2e from NtYieldExecution for system calls!
     * (both our own and the apps via shared_syscall).  The Nt* wrappers
     * are stdcall so NtYieldExecution is convenient since it has zero
     * args and is unlikely to be hooked. Ref case 5441, Sygate also sometimes
     * verifies the top of the stack for sysenter system calls in a similar
     * fashion (must be in ntdll/sysfer). For that we again borrow out of
     * NtYieldExecution (this time just the ret) to fix up our stack. */
    GET_NTDLL(NtYieldExecution, (VOID));
    /* N.B.: if we change which syscall, for WOW64 the wrapper can change */
    app_pc pc = (app_pc)NtYieldExecution;
    app_pc int_target = pc + 9;
    ushort check = *((ushort *)(int_target));
    HMODULE ntdllh = get_ntdll_base();

    if (!windows_version_init(syscalls_init_get_num(ntdllh, SYS_GetContextThread),
                              syscalls_init_get_num(ntdllh, SYS_AllocateVirtualMemory)))
        return false;
    ASSERT(syscalls != NULL);

    /* We check the 10th and 11th bytes to identify the gateway.
     * XXX i#1854: we should try and reduce how fragile we are wrt small
     * changes in syscall wrapper sequences.
     *
     *  int 2e: {2k}
     *    77F97BFA: B8 BA 00 00 00     mov         eax,0BAh
     *    77F97BFF: 8D 54 24 04        lea         edx,[esp+4]
     *    77F97C03: CD 2E              int         2Eh
     *    ret (stdcall)
     *  sysenter:  {xpsp[0,1] 2k3sp0}
     *    0x77f7eb23   b8 77 00 00 00       mov    $0x00000077 -> %eax
     *    0x77f7eb28   ba 00 03 fe 7f       mov    $0x7ffe0300 -> %edx
     *    0x77f7eb2d   ff d2                call   %edx
     *    ret (stdcall)
     *  sysenter:  {xpsp2 2k3sp1}
     *    0x77f7eb23   b8 77 00 00 00       mov    $0x00000077 -> %eax
     *    0x77f7eb28   ba 00 03 fe 7f       mov    $0x7ffe0300 -> %edx
     *    0x77f7eb2d   ff 12                call   [%edx]
     *    ret (stdcall)
     *  wow64 xp64 (case 3922):
     *    7d61ce3f b843000000       mov     eax,0x43
     *    7d61ce44 b901000000       mov     ecx,0x1
     *    7d61ce49 8d542404         lea     edx,[esp+0x4]
     *    7d61ce4d 64ff15c0000000   call    dword ptr fs:[000000c0]
     *    7d61ce54 c3               ret
     *  x64 syscall (PR 215398):
     *    00000000`78ef16c0 4c8bd1          mov     r10,rcx
     *    00000000`78ef16c3 b843000000      mov     eax,43h
     *    00000000`78ef16c8 0f05            syscall
     *    00000000`78ef16ca c3              ret
     *  win8+ sysenter w/ co-located "inlined" callee:
     *    77d7422c b801000000      mov     eax,1
     *    77d74231 e801000000      call    ntdll!NtYieldExecution+0xb (77d74237)
     *    77d74236 c3              ret
     *    77d74237 8bd4            mov     edx,esp
     *    77d74239 0f34            sysenter
     *    77d7423b c3              ret
     *  win8 and win8.1 wow64 syscall (has no ecx):
     *    777311bc b844000100      mov     eax,10044h
     *    777311c1 64ff15c0000000  call    dword ptr fs:[0C0h]
     *    777311c8 c3              ret
     *  win10 wow64 syscall:
     *    77cd9040 b846000100      mov     eax,10046h
     *    77cd9045 bab0d5ce77      mov     edx,offset ntdll!Wow64SystemServiceCall
     *    77cd904a ffd2            call    edx
     *    77cd904c c3              ret
     *   ntdll!Wow64SystemServiceCall:
     *    77ced5b0 648b1530000000  mov     edx,dword ptr fs:[30h]
     *    77ced5b7 8b9254020000    mov     edx,dword ptr [edx+254h]
     *    77ced5bd f7c202000000    test    edx,2
     *    77ced5c3 7403            je      ntdll!Wow64SystemServiceCall+0x18 (77ced5c8)
     *    77ced5c5 cd2e            int     2Eh
     *    77ced5c7 c3              ret
     *    77ced5c8 eacfd5ce773300  jmp     0033:77CED5CF
     *    77ced5cf 41              inc     ecx
     *   win10-1607 wow64:
     *    ntdll!Wow64SystemServiceCall:
     *    77c32330 ff251812cc77    jmp     dword ptr [ntdll!Wow64Transition (77cc1218)]
     *    0:000> U poi(77cc1218)
     *    58787000 ea097078583300  jmp     0033:58787009
     *  win10-TH2(1511) x64:
     *    7ff9`13185630 4c8bd1          mov     r10,rcx
     *    7ff9`13185633 b843000000      mov     eax,43h
     *    7ff9`13185638 f604250803fe7f01 test byte ptr [SharedUserData+0x308(`7ffe0308)],1
     *    7ff9`13185640 7503            jne     ntdll!NtContinue+0x15 (00007ff9`13185645)
     *    7ff9`13185642 0f05            syscall
     *    7ff9`13185644 c3              ret
     *    7ff9`13185645 cd2e            int     2Eh
     *    7ff9`13185647 c3              ret
     */
    if (check == 0x2ecd) {
        dr_which_syscall_t = DR_SYSCALL_INT2E;
        set_syscall_method(SYSCALL_METHOD_INT);
        int_syscall_address = int_target;
        /* ASSERT is simple ret (i.e. 0 args) */
        ASSERT(*(byte *)(int_target + 2) == 0xc3 /* ret 0 */);
    } else if (check == 0x8d00 || check == 0x0000 /* win8 */) {
        ASSERT(is_wow64_process(NT_CURRENT_PROCESS));
        dr_which_syscall_t = DR_SYSCALL_WOW64;
        set_syscall_method(SYSCALL_METHOD_WOW64);
        if (check == 0x8d00) /* xp through win7 */
            wow64_index = (int *)windows_XP_wow64_index;
        DOCHECK(1, {
            int call_start_offs = (check == 0x8d00) ? 5 : -4;
            ASSERT(*((uint *)(int_target + call_start_offs)) == 0xc015ff64);
            ASSERT(*((uint *)(int_target + call_start_offs + 3)) == WOW64_TIB_OFFSET);
        });
        DOCHECK(1, {
            /* We assume syscalls go through teb->WOW32Reserved */
            TEB *teb = get_own_teb();
            ASSERT(teb != NULL && teb->WOW32Reserved != NULL);
        });
#    ifdef X64 /* PR 205898 covers 32-bit syscall support */
    } else if (check == 0xc305 || check == 0x2504) {
        dr_which_syscall_t = DR_SYSCALL_SYSCALL;
        set_syscall_method(SYSCALL_METHOD_SYSCALL);
        /* ASSERT is syscall */
        ASSERT(*(int_target - 1) == 0x0f || *(ushort *)(int_target + 9) == 0x050f);
#    endif
    } else if (check == 0xff7f &&
               /* rule out win10 wow64 */
               *(app_pc *)(pc + 6) == VSYSCALL_BOOTSTRAP_ADDR) {
        app_pc vsys;
        /* verify is call %edx or call [%edx] followed by ret 0 [0xc3] */
        ASSERT(*((ushort *)(int_target + 2)) == 0xc3d2 ||
               *((ushort *)(int_target + 2)) == 0xc312);
        /* Double check use_ki_syscall_routines() matches type of ind call used */
        ASSERT((!use_ki_syscall_routines() && *((ushort *)(int_target + 1)) == 0xd2ff) ||
               (use_ki_syscall_routines() && *((ushort *)(int_target + 1)) == 0x12ff));
        /* verify VSYSCALL_BOOTSTRAP_ADDR */
        IF_X64(ASSERT_NOT_IMPLEMENTED(false));
        ASSERT(*((uint *)(int_target - 3)) == (uint)(ptr_uint_t)VSYSCALL_BOOTSTRAP_ADDR);
        /* DrM i#1724 (and old case 5463): old hardware, or virtualized hardware,
         * may not suport sysenter.
         * Thus we need to drill down into the vsyscall code itself:
         *   0x7ffe0300   8b d4            mov    %esp -> %edx
         *   0x7ffe0302   0f 34            sysenter
         * Versus:
         *   0x7c90e520   8d 54 24 08      lea     edx,[esp+8]
         *   0x7c90e524   cd 2e            int     2Eh
         * XXX: I'd like to use d_r_safe_read() but that's not set up yet.
         */
        if (*((ushort *)(int_target + 1)) == 0xd2ff) {
            vsys = VSYSCALL_BOOTSTRAP_ADDR;
        } else {
            vsys = *(app_pc *)VSYSCALL_BOOTSTRAP_ADDR;
        }
        if (*((ushort *)(vsys + 2)) == 0x340f) {
            sysenter_ret_address = (app_pc)int_target + 3; /* save addr of ret */
            /* i#537: we do not support XPSP{0,1} wrt showing the skipped ret,
             * which requires looking at the vsyscall code.
             */
            KiFastSystemCallRet_address =
                (app_pc)d_r_get_proc_address(ntdllh, "KiFastSystemCallRet");
            set_syscall_method(SYSCALL_METHOD_SYSENTER);
            dr_which_syscall_t = DR_SYSCALL_SYSENTER;
        } else {
            dr_which_syscall_t = DR_SYSCALL_INT2E;
            set_syscall_method(SYSCALL_METHOD_INT);
            int_syscall_address = int_target;
            ASSERT(*(byte *)(vsys + 6) == 0xc3 /* ret 0 */);
        }
    } else if (check == 0xc300 || check == 0xc200) {
        /* win8: call followed by ret */
        IF_X64(ASSERT_NOT_IMPLEMENTED(false));
        /* kernel returns control to KiFastSystemCallRet, not local sysenter, of course */
        sysenter_ret_address =
            (app_pc)d_r_get_proc_address(ntdllh, "KiFastSystemCallRet");
        ASSERT(sysenter_ret_address != NULL);
        KiFastSystemCallRet_address =
            (app_pc)d_r_get_proc_address(ntdllh, "KiFastSystemCallRet");
        set_syscall_method(SYSCALL_METHOD_SYSENTER);
        dr_which_syscall_t = DR_SYSCALL_SYSENTER;
    } else {
        /* win10 wow64 */
        app_pc tgt;
        ASSERT(*(ushort *)(pc + 10) == 0xd2ff);
        ASSERT(is_wow64_process(NT_CURRENT_PROCESS));
        tgt = *(app_pc *)(pc + 6);
        dr_which_syscall_t = DR_SYSCALL_WOW64;
        set_syscall_method(SYSCALL_METHOD_WOW64);
        wow64_syscall_call_tgt = tgt;
    }

    /* Prime use_ki_syscall_routines() */
    use_ki_syscall_routines();

    if (syscalls == windows_unknown_syscalls ||
        /* There are variations within the versions we have (e.g., i#4587), so our
         * static arrays are not foolproof.  It is simpler to just get the ground truth
         * for any moderately recent version.
         */
        get_os_version() >= WINDOWS_VERSION_10_1511) {
        /* i#1598: try to work on new, unsupported Windows versions */
        int i;
        app_pc wrapper;
        ASSERT(ntdllh != NULL);
        for (i = 0; i < SYS_MAX; i++) {
            if (syscalls[i] == SYSCALL_NOT_PRESENT) /* presumably matches known ver */
                continue;
            wrapper = (app_pc)d_r_get_proc_address(ntdllh, syscall_names[i]);
            if (wrapper != NULL && !ALLOW_HOOKER(wrapper)) {
                syscalls[i] = *((int *)((wrapper) + SYSNUM_OFFS));
            }
            /* We ignore TestAlert complications: we don't call it anyway */
        }
    } else {
        /* Quick sanity check that the syscall numbers we care about are what's
         * in our static array.  We still do our later full-decode sanity checks.
         * This will always be true if we went through the wrapper loop above.
         */
        DOCHECK(1, {
            int i;
            ASSERT(ntdllh != NULL);
            for (i = 0; i < SYS_MAX; i++) {
                if (syscalls[i] == SYSCALL_NOT_PRESENT)
                    continue;
                /* note that this check allows a hooker so we'll need a
                 * better way of determining syscall numbers
                 */
                app_pc wrapper = (app_pc)d_r_get_proc_address(ntdllh, syscall_names[i]);
                CHECK_SYSNUM_AT((byte *)d_r_get_proc_address(ntdllh, syscall_names[i]),
                                i);
            }
        });
    }
    return true;
}

/* Returns true if machine is using the Ki*SysCall routines (indirection via vsyscall
 * page), false otherwise.
 *
 * XXX: on win8, KiFastSystemCallRet is used, but KiFastSystemCall is never
 * executed even though it exists.  This routine returns true there (we have not
 * yet set up the versions so can't just call get_os_version()).
 */
bool
use_ki_syscall_routines()
{
    /* FIXME - two ways to do this.  We could use the byte matching above in
     * syscalls_init to match call edx vs call [edx] or we could check for the
     * existence of the Ki*SystemCall* routines.  We do the latter and have
     * syscalls_init assert that the two methods agree. */
    /* We use KiFastSystemCall, but KiIntSystemCall and KiFastSystemCallRet would
     * work just as well. */
    static generic_func_t ki_fastsyscall_addr = (generic_func_t)PTR_UINT_MINUS_1;
    if (ki_fastsyscall_addr == (generic_func_t)PTR_UINT_MINUS_1) {
        ki_fastsyscall_addr = d_r_get_proc_address(get_ntdll_base(), "KiFastSystemCall");
        ASSERT(ki_fastsyscall_addr != (generic_func_t)PTR_UINT_MINUS_1);
    }
    return (ki_fastsyscall_addr != NULL);
}

static void
nt_get_context_extended_functions(app_pc base)
{
    if (YMM_ENABLED()) { /* indicates OS support, not just processor support */
        ntdll_RtlGetExtendedContextLength =
            (ntdll_RtlGetExtendedContextLength_t)d_r_get_proc_address(
                base, "RtlGetExtendedContextLength");
        ntdll_RtlInitializeExtendedContext =
            (ntdll_RtlInitializeExtendedContext_t)d_r_get_proc_address(
                base, "RtlInitializeExtendedContext");
        ntdll_RtlLocateLegacyContext =
            (ntdll_RtlLocateLegacyContext_t)d_r_get_proc_address(
                base, "RtlLocateLegacyContext");
        ASSERT(ntdll_RtlGetExtendedContextLength != NULL &&
               ntdll_RtlInitializeExtendedContext != NULL &&
               ntdll_RtlLocateLegacyContext != NULL);
    }
}

static void
nt_init_dynamic_syscall_wrappers(app_pc base)
{
    NtGetNextThread = (NtGetNextThread_t)d_r_get_proc_address(base, "NtGetNextThread");
}
#endif /* !NOT_DYNAMORIO_CORE_PROPER */

void
ntdll_init()
{
    /* FIXME: decode kernel32!TlsGetValue and get the real offset
     * from there?
     */
    ASSERT(offsetof(TEB, TlsSlots) == TEB_TLS64_OFFSET);
#if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)
    nt_init_dynamic_syscall_wrappers((app_pc)get_ntdll_base());
    nt_get_context_extended_functions((app_pc)get_ntdll_base());
#endif
}

/* note that this function is called even on the release fast exit path
 * (via os_exit) and thus should only do necessary cleanup without ifdef
 * DEBUG, but also be carefull about ifdef DEBUG since Detach wants to remove
 * as much of us as possible
 */
void
ntdll_exit(void)
{
#if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)
    tls_exit();
    set_ntdll_base(NULL);

    if (doing_detach) {
        own_peb = NULL;
        sysenter_tls_offset = 0xffffffff;
        nt_wrappers_intercepted = true;
    }
#endif
}

/* export this if needed elsewhere */
static NTSTATUS
query_thread_info(HANDLE h, THREAD_BASIC_INFORMATION *info)
{
    NTSTATUS res;
    ULONG got;
    memset(info, 0, sizeof(THREAD_BASIC_INFORMATION));
    res = NT_SYSCALL(QueryInformationThread, h, ThreadBasicInformation, info,
                     sizeof(THREAD_BASIC_INFORMATION), &got);
    ASSERT(!NT_SUCCESS(res) || got == sizeof(THREAD_BASIC_INFORMATION));
    return res;
}

/* Get a segment descriptor. This code assumes the selector is set
 * appropriately in entry->Selector */
NTSTATUS
query_seg_descriptor(HANDLE hthread, DESCRIPTOR_TABLE_ENTRY *entry)
{
    NTSTATUS res;
    ULONG got;
    res = NT_SYSCALL(QueryInformationThread, hthread, ThreadDescriptorTableEntry, entry,
                     sizeof(DESCRIPTOR_TABLE_ENTRY), &got);

    /* This call only writes the LDT_ENTRY portion of the table entry */
    ASSERT(!NT_SUCCESS(res) || got == sizeof(LDT_ENTRY));
    return res;
}

/* Get a win32 start address.  NOTE: According to Nebbet, the value
 * retrieved with ThreadQuerySetWin32StartAddress is invalid if the
 * thread has call ZwReplyWaitReplyPort or ZwReplyWaitReceivePort.
 */
NTSTATUS
query_win32_start_addr(HANDLE hthread, PVOID start_addr)
{
    NTSTATUS res;
    ULONG got;
    res = NT_SYSCALL(QueryInformationThread, hthread, ThreadQuerySetWin32StartAddress,
                     start_addr, sizeof(app_pc), &got);
    ASSERT(!NT_SUCCESS(res) || got == sizeof(PVOID));
    return res;
}

/* Collects system information available through the NtQuerySystemInformation
 * system call.
 */
NTSTATUS
query_system_info(IN SYSTEM_INFORMATION_CLASS info_class, IN int info_size,
                  OUT PVOID info)
{
    NTSTATUS result;
    ULONG bytes_received = 0;

    GET_NTDLL(NtQuerySystemInformation,
              (IN SYSTEM_INFORMATION_CLASS info_class, OUT PVOID info, IN ULONG info_size,
               OUT PULONG bytes_received));

    result = NtQuerySystemInformation(info_class, info, info_size, &bytes_received);

    return result;
}

/* since not exporting get_own_teb() */
#ifndef NOT_DYNAMORIO_CORE
thread_id_t
d_r_get_thread_id()
{
    return (thread_id_t)get_own_teb()->ClientId.UniqueThread;
}

process_id_t
get_process_id()
{
    return (process_id_t)get_own_teb()->ClientId.UniqueProcess;
}

int
get_last_error()
{
    return get_own_teb()->LastErrorValue;
}

void
set_last_error(int error)
{
    get_own_teb()->LastErrorValue = error;
}
#endif /* !NOT_DYNAMORIO_CORE */

HANDLE
get_stderr_handle()
{
    HANDLE herr = get_own_peb()->ProcessParameters->StdErrorHandle;
    if (herr == NULL)
        return INVALID_HANDLE_VALUE;
    return herr;
}

HANDLE
get_stdout_handle()
{
    HANDLE hout = get_own_peb()->ProcessParameters->StdOutputHandle;
    if (hout == NULL)
        return INVALID_HANDLE_VALUE;
    return hout;
}

HANDLE
get_stdin_handle()
{
    HANDLE hin = get_own_peb()->ProcessParameters->StdInputHandle;
    if (hin == NULL)
        return INVALID_HANDLE_VALUE;
    return hin;
}

thread_exited_status_t
is_thread_exited(HANDLE hthread)
{
    LARGE_INTEGER timeout;
    wait_status_t result;
    /* Keep the timeout small, just want to check if signaled. Don't want to wait at all
     * really, but no way to specify that. Note negative => relative time offset (so is
     * a 1 millisecond timeout). */
    timeout.QuadPart = -((int)1 * TIMER_UNITS_PER_MILLISECOND);

    if (thread_id_from_handle(hthread) == (thread_id_t)PTR_UINT_MINUS_1) {
        /* not a thread handle */
        ASSERT(false && "Not a valid thread handle.");
        return THREAD_EXIT_ERROR;
    }
    if (!TEST(SYNCHRONIZE, nt_get_handle_access_rights(hthread))) {
        /* Note that our own thread handles will have SYNCHRONIZE since, like
         * THREAD_TERMINATE, that seems to be a right the thread can always get for
         * itself (prob. due to how stacks are freed). So only a potential issue with
         * app handles for which we try to dup with the required rights. xref 9529 */
        HANDLE ht = INVALID_HANDLE_VALUE;
        NTSTATUS res = duplicate_handle(NT_CURRENT_PROCESS, hthread, NT_CURRENT_PROCESS,
                                        &ht, SYNCHRONIZE, 0, 0);
        if (!NT_SUCCESS(res)) {
            ASSERT_CURIOSITY(false && "Unable to check if thread has exited.");
            return THREAD_EXIT_ERROR;
        }
        result = nt_wait_event_with_timeout(ht, &timeout);
        close_handle(ht);
    } else {
        result = nt_wait_event_with_timeout(hthread, &timeout);
    }
    if (result == WAIT_SIGNALED)
        return THREAD_EXITED;
    if (result == WAIT_TIMEDOUT)
        return THREAD_NOT_EXITED;
    ASSERT(result == WAIT_ERROR);
    ASSERT_CURIOSITY(false && "is_thread_exited() unknown error");
    return THREAD_EXIT_ERROR;
}

/* The other ways to get thread info, like OpenThread and Toolhelp, don't
 * let you go from handle to id (remember handles can be duplicated and
 * there's no way to tell equivalence), plus are only on win2k.
 * Returns POINTER_MAX on failure
 */
thread_id_t
thread_id_from_handle(HANDLE h)
{
    THREAD_BASIC_INFORMATION info;
    NTSTATUS res = query_thread_info(h, &info);
    if (!NT_SUCCESS(res))
        return POINTER_MAX;
    else
        return (thread_id_t)info.ClientId.UniqueThread;
}

/* export this if needed elsewhere */
static NTSTATUS
query_process_info(HANDLE h, PROCESS_BASIC_INFORMATION *info)
{
    NTSTATUS res;
    ULONG got;

    memset(info, 0, sizeof(PROCESS_BASIC_INFORMATION));
    res = NtQueryInformationProcess(h, ProcessBasicInformation, info,
                                    sizeof(PROCESS_BASIC_INFORMATION), &got);
    ASSERT(!NT_SUCCESS(res) || got == sizeof(PROCESS_BASIC_INFORMATION));
    return res;
}

/* Returns POINTER_MAX on failure */
process_id_t
process_id_from_handle(HANDLE h)
{
    PROCESS_BASIC_INFORMATION info;
    NTSTATUS res = query_process_info(h, &info);
    if (!NT_SUCCESS(res))
        return POINTER_MAX;
    else
        return (process_id_t)info.UniqueProcessId;
}

/* Returns POINTER_MAX on failure */
process_id_t
process_id_from_thread_handle(HANDLE h)
{
    THREAD_BASIC_INFORMATION info;
    NTSTATUS res = query_thread_info(h, &info);
    if (!NT_SUCCESS(res))
        return POINTER_MAX;
    else
        return (process_id_t)info.ClientId.UniqueProcess;
}

HANDLE
process_handle_from_id(process_id_t pid)
{
    NTSTATUS res;
    HANDLE h;
    OBJECT_ATTRIBUTES oa;
    CLIENT_ID cid;
    InitializeObjectAttributes(&oa, NULL, OBJ_CASE_INSENSITIVE, NULL, NULL);
    memset(&cid, 0, sizeof(cid));
    cid.UniqueProcess = (HANDLE)pid;
    res = nt_raw_OpenProcess(&h, PROCESS_ALL_ACCESS, &oa, &cid);
    if (!NT_SUCCESS(res)) {
        NTPRINT("nt_open_process failed: %x\n", res);
    }
    if (!NT_SUCCESS(res))
        return INVALID_HANDLE_VALUE;
    else
        return h;
}

#if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)
HANDLE
thread_handle_from_id(thread_id_t tid)
{
    NTSTATUS res;
    HANDLE h;
    OBJECT_ATTRIBUTES oa;
    CLIENT_ID cid;
    InitializeObjectAttributes(&oa, NULL, OBJ_CASE_INSENSITIVE, NULL, NULL);
    memset(&cid, 0, sizeof(cid));
    cid.UniqueThread = (HANDLE)tid;
    res = nt_raw_OpenThread(&h, THREAD_ALL_ACCESS, &oa, &cid);
    if (!NT_SUCCESS(res)) {
        NTPRINT("nt_open_thread failed: %x\n", res);
    }
    if (!NT_SUCCESS(res))
        return INVALID_HANDLE_VALUE;
    else
        return h;
}
#endif

/* PEB:
 * for a running thread this is stored at fs:[30h]
 * it's always at 0x7FFDF000 according to InsideWin2k p.290
 * but that's out of date, is randomized within 0x7ffd... on XPsp2
 * so use query_process_info to get it
 */
PEB *
get_peb(HANDLE h)
{
    PROCESS_BASIC_INFORMATION info;
    NTSTATUS res = query_process_info(h, &info);
    if (!NT_SUCCESS(res))
        return NULL;
    else
        return info.PebBaseAddress;
}

PEB *
get_own_peb()
{
    /* alt. we could use get_own_teb->PEBptr, but since we're remembering the
     * results of the first lookup doesn't really gain us much */
    if (own_peb == NULL) {
        own_peb = get_peb(NT_CURRENT_PROCESS);
        ASSERT(own_peb != NULL);
    }
    return own_peb;
}

/* Returns a 32-bit PEB for a 32-bit child and !X64 parent.
 * Else returns a 64-bit PEB.
 */
uint64
get_peb_maybe64(HANDLE h)
{
#ifdef X64
    return (uint64)get_peb(h);
#else
    /* The WOW64 query below should work regardless of whether the kernel is 32-bit
     * or the child is 32-bit or 64-bit.  But, it returns the 64-bit PEB, while we
     * would prefer the 32-bit, so we first try get_peb().
     */
    PEB *peb32 = get_peb(h);
    if (peb32 != NULL)
        return (uint64)peb32;
    PROCESS_BASIC_INFORMATION64 info;
    NTSTATUS res = nt_wow64_query_info_process64(h, &info);
    if (!NT_SUCCESS(res))
        return 0;
    else
        return info.PebBaseAddress;
#endif
}

#ifdef X64
/* Returns the 32-bit PEB for a WOW64 process, given process and thread handles. */
uint64
get_peb32(HANDLE process, HANDLE thread)
{
    THREAD_BASIC_INFORMATION info;
    NTSTATUS res = query_thread_info(thread, &info);
    if (!NT_SUCCESS(res))
        return 0;
        /* Bizarrely, info.TebBaseAddress points 2 pages too low!  We do sanity
         * checks to confirm we have a TEB by looking at its self pointer.
         */
#    define TEB32_QUERY_OFFS 0x2000
    byte *teb32 = (byte *)info.TebBaseAddress;
    uint ptr32;
    size_t sz_read;
    if (!nt_read_virtual_memory(process, teb32 + X86_SELF_TIB_OFFSET, &ptr32,
                                sizeof(ptr32), &sz_read) ||
        sz_read != sizeof(ptr32) || ptr32 != (uint64)teb32) {
        teb32 += TEB32_QUERY_OFFS;
        if (!nt_read_virtual_memory(process, teb32 + X86_SELF_TIB_OFFSET, &ptr32,
                                    sizeof(ptr32), &sz_read) ||
            sz_read != sizeof(ptr32) || ptr32 != (uint64)teb32) {
            /* XXX: Also try peb64+0x1000?  That was true for older Windows version. */
            return 0;
        }
    }
    if (!nt_read_virtual_memory(process, teb32 + X86_PEB_TIB_OFFSET, &ptr32,
                                sizeof(ptr32), &sz_read) ||
        sz_read != sizeof(ptr32))
        return 0;
    return ptr32;
}
#endif

/****************************************************************************/
#ifndef NOT_DYNAMORIO_CORE
/* avoid needing CXT_ macros and SELF_TIB_OFFSET from os_exports.h */

TEB *
get_teb(HANDLE h)
{
    THREAD_BASIC_INFORMATION info;
    NTSTATUS res = query_thread_info(h, &info);
    if (!NT_SUCCESS(res))
        return NULL;
    else
        return (TEB *)info.TebBaseAddress;
}

static app_pc ntdll_base;

void *
get_ntdll_base(void)
{
    if (ntdll_base == NULL) {
#    ifndef NOT_DYNAMORIO_CORE_PROPER
        ASSERT(!dr_earliest_injected); /* Ldr not initialized yet */
#    endif
        ntdll_base = (app_pc)get_module_handle(L"ntdll.dll");
        ASSERT(ntdll_base != NULL);
    }
    return ntdll_base;
}

#    if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)
/* for early injection we can't use get_module_handle() to find it */
void
set_ntdll_base(app_pc base)
{
    if (ntdll_base == NULL)
        ntdll_base = base;
}

/* get_allocation_size() in os.c */
bool
is_in_ntdll(app_pc pc)
{
    static app_pc ntdll_end;
    app_pc base = get_ntdll_base();
    if (ntdll_end == NULL) {
        ntdll_end = base + get_allocation_size(base, NULL);
        ASSERT(ntdll_end > base);
    }
    return (pc >= base && pc < ntdll_end);
}

static bool
context_check_extended_sizes(context_ex_t *cxt_ex, uint flags)
{
    return (cxt_ex->all.offset == -(LONG)sizeof(CONTEXT) &&
            cxt_ex->legacy.offset == -(LONG)sizeof(CONTEXT) &&
            (cxt_ex->legacy.length ==
             (DWORD)sizeof(CONTEXT)
             /* We won't allocate space for ExtendedRegisters if not saving xmm */
             IF_NOT_X64(||
                        (!TESTALL(CONTEXT_XMM_FLAG, flags) &&
                         cxt_ex->legacy.length ==
                             (DWORD)offsetof(CONTEXT, ExtendedRegisters)))));
}

/* get the ymm saved area from CONTEXT extended area
 * returns NULL if the extended area is not initialized.
 */
byte *
context_ymmh_saved_area(CONTEXT *cxt)
{
    /* i#437: ymm are inside XSTATE construct which should be
     * laid out like this: {CONTEXT, CONTEXT_EX, XSTATE}.
     * The gap between CONTEXT_EX and XSTATE varies due to
     * alignment, should read CONTEXT_EX fields to get it.
     */
    ptr_uint_t p = (ptr_uint_t)cxt;
    context_ex_t our_cxt_ex;
    context_ex_t *cxt_ex = (context_ex_t *)(p + sizeof(*cxt));
    ASSERT(proc_avx_enabled());
    /* verify the dr_cxt_ex is correct */
    if (d_r_safe_read(cxt_ex, sizeof(*cxt_ex), &our_cxt_ex)) {
        if (!context_check_extended_sizes(&our_cxt_ex, cxt->ContextFlags)) {
            ASSERT_CURIOSITY(false && "CONTEXT_EX is not setup correctly");
            return NULL;
        }
    } else {
        ASSERT_CURIOSITY(false && "fail to read CONTEXT_EX");
    }
    /* XXX: XSTATE has xsave format minus first 512 bytes, so ymm0
     * should be at offset 64.
     * Should we use kernel32!LocateXStateFeature() or
     * ntdll!RtlLocateExtendedFeature() to locate,
     * or cpuid to find Ext_Save_Area_2?
     * Currently, use hardcode XSTATE_HEADER_SIZE.
     * mcontext_to_context() also uses this to get back to the header.
     */
    p = p + sizeof(*cxt) + cxt_ex->xstate.offset + XSTATE_HEADER_SIZE;
    return (byte *)p;
}

/* routines for conversion between CONTEXT and priv_mcontext_t */
/* assumes our segment registers are the same as the app and that
 * we never touch floating-point state and debug registers.
 * Note that this code will not compile for non-core (no proc_has_feature())
 * but is not currently used there.
 */
/* all we need is CONTEXT_INTEGER and non-segment CONTEXT_CONTROL,
 * and for PR 264138 we need the XMM registers
 */
static void
context_to_mcontext_internal(priv_mcontext_t *mcontext, CONTEXT *cxt)
{
    ASSERT(TESTALL(CONTEXT_INTEGER | CONTEXT_CONTROL, cxt->ContextFlags));
    /* CONTEXT_INTEGER */
    mcontext->xax = cxt->CXT_XAX;
    mcontext->xbx = cxt->CXT_XBX;
    mcontext->xcx = cxt->CXT_XCX;
    mcontext->xdx = cxt->CXT_XDX;
    mcontext->xsi = cxt->CXT_XSI;
    mcontext->xdi = cxt->CXT_XDI;
#        ifdef X64
    mcontext->r8 = cxt->R8;
    mcontext->r9 = cxt->R9;
    mcontext->r10 = cxt->R10;
    mcontext->r11 = cxt->R11;
    mcontext->r12 = cxt->R12;
    mcontext->r13 = cxt->R13;
    mcontext->r14 = cxt->R14;
    mcontext->r15 = cxt->R15;
#        endif
    /* XXX i#1312: This will need attention for AVX-512, specifically the different
     * xstate formats supported by the processor, compacted and standard, as well as
     * MPX.
     */
    if (CONTEXT_PRESERVE_XMM && TESTALL(CONTEXT_XMM_FLAG, cxt->ContextFlags)) {
        /* no harm done if no sse support */
        /* CONTEXT_FLOATING_POINT or CONTEXT_EXTENDED_REGISTERS */
        int i;
        for (i = 0; i < proc_num_simd_sse_avx_registers(); i++)
            memcpy(&mcontext->simd[i], CXT_XMM(cxt, i), XMM_REG_SIZE);
    }
    /* if XSTATE is NOT set, the app has NOT used any ymm state and
     * thus it's fine if we do not copy dr_mcontext_t ymm value.
     */
    if (CONTEXT_PRESERVE_YMM && TESTALL(CONTEXT_XSTATE, cxt->ContextFlags)) {
        byte *ymmh_area = context_ymmh_saved_area(cxt);
        if (ymmh_area != NULL) {
            int i;
            for (i = 0; i < proc_num_simd_sse_avx_registers(); i++) {
                memcpy(&mcontext->simd[i].u32[4], &YMMH_AREA(ymmh_area, i).u32[0],
                       YMMH_REG_SIZE);
            }
        }
    }
    /* XXX i#1312: AVX-512 extended register copies missing yet. */

    /* CONTEXT_CONTROL without the segments */
    mcontext->xbp = cxt->CXT_XBP;
    mcontext->xsp = cxt->CXT_XSP;
    mcontext->xflags = cxt->CXT_XFLAGS;
    mcontext->pc = (app_pc)cxt->CXT_XIP; /* including XIP */
}

void
context_to_mcontext(priv_mcontext_t *mcontext, CONTEXT *cxt)
{
    /* i#437: cxt might come from kernel where XSTATE is not set */
    /* FIXME: This opens us up to a bug in DR where DR requests a CONTEXT but
     * forgets to set XSTATE even though app has used it and we then mess up
     * the app's ymm state. Any way we can detect that?
     * One way is to pass a flag to indicate if the context is from kernel or
     * set by DR, but it requires update a chain of calls.
     */
    ASSERT(TESTALL(CONTEXT_DR_STATE_NO_YMM, cxt->ContextFlags));
    context_to_mcontext_internal(mcontext, cxt);
}

void
context_to_mcontext_new_thread(priv_mcontext_t *mcontext, CONTEXT *cxt)
{
    /* i#1714: new threads on win10 don't have CONTEXT_EXTENDED_REGISTERS,
     * which is not a big deal as it doesn't matter if DR clobbers xmm/fp state.
     */
    ASSERT(TESTALL(CONTEXT_INTEGER | CONTEXT_CONTROL, cxt->ContextFlags));
    context_to_mcontext_internal(mcontext, cxt);
}

/* If set_cur_seg is true, cs and ss (part of CONTEXT_CONTROL) are set to
 * the current values.
 * If mcontext_to_context is used to set another thread's context,
 * the caller must initialize the cs/ss value properly and set
 * set_cur_seg to false
 */
void
mcontext_to_context(CONTEXT *cxt, priv_mcontext_t *mcontext, bool set_cur_seg)
{
    /* xref comment in context_to_mcontext */
    ASSERT(TESTALL(CONTEXT_DR_STATE_NO_YMM, cxt->ContextFlags));
    if (set_cur_seg) {
        /* i#1033: initialize CONTEXT_CONTROL segments for current thread */
        get_segments_cs_ss(&cxt->SegCs, &cxt->SegSs);
    }
    /* CONTEXT_INTEGER */
    cxt->CXT_XAX = mcontext->xax;
    cxt->CXT_XBX = mcontext->xbx;
    cxt->CXT_XCX = mcontext->xcx;
    cxt->CXT_XDX = mcontext->xdx;
    cxt->CXT_XSI = mcontext->xsi;
    cxt->CXT_XDI = mcontext->xdi;
#        ifdef X64
    cxt->R8 = mcontext->r8;
    cxt->R9 = mcontext->r9;
    cxt->R10 = mcontext->r10;
    cxt->R11 = mcontext->r11;
    cxt->R12 = mcontext->r12;
    cxt->R13 = mcontext->r13;
    cxt->R14 = mcontext->r14;
    cxt->R15 = mcontext->r15;
#        endif
    if (CONTEXT_PRESERVE_XMM && TESTALL(CONTEXT_XMM_FLAG, cxt->ContextFlags)) {
        /* no harm done if no sse support */
        /* CONTEXT_FLOATING_POINT or CONTEXT_EXTENDED_REGISTERS */
        int i;
        /* We can't set just xmm and not the rest of the fp state
         * so we fill in w/ the current (unchanged by DR) values
         * (i#462, i#457)
         */
        byte fpstate_buf[MAX_FP_STATE_SIZE];
        byte *fpstate = (byte *)ALIGN_FORWARD(fpstate_buf, 16);
        size_t written = proc_save_fpstate(fpstate);
#        ifdef X64
        ASSERT(sizeof(cxt->FltSave) == written);
        memcpy(&cxt->FltSave, fpstate, written);
        /* We also have to set the x64-only duplicate top-level MxCsr field (i#1081) */
        cxt->MxCsr = cxt->FltSave.MxCsr;
#        else
        ASSERT(MAXIMUM_SUPPORTED_EXTENSION == written);
        memcpy(&cxt->ExtendedRegisters, fpstate, written);
#        endif
        /* Now update w/ the xmm values from mcontext */
        for (i = 0; i < proc_num_simd_sse_avx_registers(); i++)
            memcpy(CXT_XMM(cxt, i), &mcontext->simd[i], XMM_REG_SIZE);
    }
    /* XXX i#1312: This may need attention for AVX-512, specifically the different
     * xstate formats supported by the kernel, compacted and standard, as well as
     * MPX.
     */
    if (CONTEXT_PRESERVE_YMM && TESTALL(CONTEXT_XSTATE, cxt->ContextFlags)) {
        byte *ymmh_area = context_ymmh_saved_area(cxt);
        if (ymmh_area != NULL) {
            uint64 *header_bv = (uint64 *)(ymmh_area - XSTATE_HEADER_SIZE);
            uint bv_high, bv_low;
            int i;
#        ifndef X64
            /* In 32-bit Windows mcontext, we do not preserve xmm/ymm 6 and 7,
             * which are callee saved registers, so we must fill them.
             */
            dr_ymm_t ymms[2];
            dr_ymm_t *ymm_ptr = ymms;
            __asm { mov ecx, ymm_ptr }
            /* Some supported (old) compilers do not support/understand AVX
             * instructions, so we use RAW bit here instead.
             */
#            define HEX(n) 0##n##h
#            define RAW(n) __asm _emit 0x##n
            /* c5 fc 11 71 00       vmovups %ymm6 -> 0x00(%XCX)
             * c5 fc 11 79 20       vmovups %ymm7 -> 0x20(%XCX)
             */
            RAW(c5) RAW(fc) RAW(11) RAW(71) RAW(00);
            RAW(c5) RAW(fc) RAW(11) RAW(79) RAW(20);
            /* XMM6/7 has been copied above, so only copy ymmh here */
            memcpy(&YMMH_AREA(ymmh_area, 6).u32[0], &ymms[0].u32[4], YMMH_REG_SIZE);
            memcpy(&YMMH_AREA(ymmh_area, 7).u32[0], &ymms[1].u32[4], YMMH_REG_SIZE);
#        endif
            for (i = 0; i < proc_num_simd_sse_avx_registers(); i++) {
                memcpy(&YMMH_AREA(ymmh_area, i).u32[0], &mcontext->simd[i].u32[4],
                       YMMH_REG_SIZE);
            }
            /* XXX i#1312: AVX-512 extended register copies missing yet. */
            /* The only un-reserved part of the AVX header saved by OP_xsave is
             * the XSTATE_BV byte.
             */
            dr_xgetbv(&bv_high, &bv_low);
            *header_bv = (((uint64)bv_high) << 32) | bv_low;
        }
    }
    /* XXX i#1312: AVX-512 extended register copies missing yet. */
    /* CONTEXT_CONTROL without the segments */
    cxt->CXT_XBP = mcontext->xbp;
    cxt->CXT_XSP = mcontext->xsp;
    IF_X64(ASSERT_TRUNCATE(cxt->CXT_XFLAGS, uint, mcontext->xflags));
    cxt->CXT_XFLAGS = (uint)mcontext->xflags;
    cxt->CXT_XIP = (ptr_uint_t)mcontext->pc; /* including XIP */
}
#    endif /* core proper */

#endif /* !NOT_DYNAMORIO_CORE */
/****************************************************************************/

/****************************************************************************/
#if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)
/* avoid needing x86_code.c from x86.asm from get_own_context_helper(),

/* unstatic for use by GET_OWN_CONTEXT macro */
void
get_own_context_integer_control(CONTEXT *cxt, reg_t cs, reg_t ss, priv_mcontext_t *mc)
{
    /* We could change the parameter types to cxt_seg_t, but the args
     * passed by get_own_context_helper() in x86.asm are best simply
     * widened in passing
     */
    DEBUG_DECLARE(uint origflags = cxt->ContextFlags;)
    IF_X64(ASSERT_TRUNCATE(cxt->SegCs, short, cs));
    cxt->SegCs = (WORD)cs; /* FIXME : need to sanitize? */
    IF_X64(ASSERT_TRUNCATE(cxt->SegSs, short, ss));
    cxt->SegSs = (WORD)ss;
    /* avoid assert in mcontext_to_context about not having xmm flags.
     * get rid of this once we implement PR 266070. */
    DODEBUG({ cxt->ContextFlags = CONTEXT_DR_STATE_NO_YMM; });
    mcontext_to_context(cxt, mc, false /* !set_cur_seg */);
    DODEBUG({ cxt->ContextFlags = origflags; });
}

/* don't call this directly, use GET_OWN_CONTEXT macro instead (it fills
 * in CONTEXT_INTEGER and CONTEXT_CONTROL values) */
void
get_own_context(CONTEXT *cxt)
{
    if (TEST(CONTEXT_SEGMENTS, cxt->ContextFlags)) {
        get_segments_defg(&cxt->SegDs, &cxt->SegEs, &cxt->SegFs, &cxt->SegGs);
    }
    /* FIXME : do we want CONTEXT_DEBUG_REGISTERS or CONTEXT_FLOATING_POINT
     * or CONTEXT_EXTENDED_REGISTERS at some point?
     * Especially in light of PR 264138.  However, no current uses need
     * to get our own xmm registers.
     */
    ASSERT_NOT_IMPLEMENTED(
        (cxt->ContextFlags & ~(CONTEXT_SEGMENTS | CONTEXT_INTEGER | CONTEXT_CONTROL)) ==
        0);
}

/***************************************************************************
 * TLS
 */

/* Lock that protects the tls_*_taken arrays */
DECLARE_CXTSWPROT_VAR(static mutex_t alt_tls_lock, INIT_LOCK_FREE(alt_tls_lock));
#    define TLS_SPAREBYTES_SLOTS \
        ((offsetof(TEB, TxFsContext) - offsetof(TEB, SpareBytes1)) / sizeof(void *))
static bool alt_tls_spare_taken[TLS_SPAREBYTES_SLOTS];
#    ifdef X64
#        define TLS_POSTTEB_SLOTS 64
static bool alt_tls_post_taken[TLS_POSTTEB_SLOTS];
/* Use the slots at the end of the 2nd page */
#        define TLS_POSTTEB_BASE_OFFS \
            ((uint)PAGE_SIZE * 2 - TLS_POSTTEB_SLOTS * sizeof(void *))
#    endif

static void
tls_exit(void)
{
#    ifdef DEBUG
    DELETE_LOCK(alt_tls_lock);
#    endif
}

/* Caller must synchronize */
static bool
alt_tls_acquire_helper(bool *taken, size_t taken_sz, size_t base_offs,
                       uint *teb_offs /* OUT */, int num_slots, uint alignment)
{
    bool res = false;
    uint i, start = 0;
    int slots_found = 0;
    for (i = 0; i < taken_sz; i++) {
        size_t offs = base_offs + i * sizeof(void *);
        if (slots_found == 0 && !taken[i] &&
            (alignment == 0 || ALIGNED(offs, alignment))) {
            start = i;
            slots_found++;
        } else if (slots_found > 0) {
            if (!taken[i])
                slots_found++;
            else
                slots_found = 0; /* start over */
        }
        if (slots_found >= num_slots)
            break;
    }
    if (slots_found >= num_slots) {
        ASSERT_TRUNCATE(uint, uint, base_offs + start * sizeof(void *));
        *teb_offs = (uint)(base_offs + start * sizeof(void *));
        for (i = start; i < start + num_slots; i++) {
            ASSERT(!taken[i]);
            taken[i] = true;
            DOCHECK(1, {
                /* Try to check for anyone else using these slots.  The TEB pages
                 * are zeroed before use.  This is only a curiosity, as we don't
                 * zero on a release and thus a release-and-re-alloc can hit this.
                 */
                TEB *teb = get_own_teb();
                ASSERT_CURIOSITY(is_region_memset_to_char((byte *)teb + *teb_offs,
                                                          num_slots * sizeof(void *), 0));
            });
        }
        res = true;
    }
    return res;
}

static bool
alt_tls_acquire(uint *teb_offs /* OUT */, int num_slots, uint alignment)
{
    bool res = false;
    ASSERT(DYNAMO_OPTION(alt_teb_tls));
    /* Strategy: first, use TEB->SpareBytes1.  The only known user of that field
     * is WINE, although Vista stole some of the space there for the TxFsContext
     * slot, and maybe now that Win8 has just about used up the TEB single page
     * for 32-bit future versions will take more?
     *
     * Second, on 64-bit, use space beyond the TEB on the 2nd TEB page.
     */
    d_r_mutex_lock(&alt_tls_lock);
    res = alt_tls_acquire_helper(alt_tls_spare_taken, TLS_SPAREBYTES_SLOTS,
                                 offsetof(TEB, SpareBytes1), teb_offs, num_slots,
                                 alignment);
#    ifdef X64
    if (!res) {
        ASSERT_NOT_TESTED();
        ASSERT(TLS_POSTTEB_BASE_OFFS > sizeof(TEB));
        res =
            alt_tls_acquire_helper(alt_tls_post_taken, TLS_POSTTEB_SLOTS,
                                   TLS_POSTTEB_BASE_OFFS, teb_offs, num_slots, alignment);
    }
#    endif
    d_r_mutex_unlock(&alt_tls_lock);
    return res;
}

/* Caller must synchronize */
static bool
alt_tls_release_helper(bool *taken, uint base_offs, uint teb_offs, int num_slots)
{
    uint i;
    uint start = (teb_offs - base_offs) / sizeof(void *);
    for (i = start; i < start + num_slots; i++) {
        ASSERT(taken[i]);
        taken[i] = false;
        /* XXX: I'd like to zero the slots out for all threads but there's
         * no simple way to do that
         */
    }
    return true;
}

static bool
alt_tls_release(uint teb_offs, int num_slots)
{
    bool res = false;
    size_t base_offs = offsetof(TEB, SpareBytes1);
    ASSERT(DYNAMO_OPTION(alt_teb_tls));
    if (teb_offs >= base_offs &&
        teb_offs < base_offs + TLS_SPAREBYTES_SLOTS * sizeof(void *)) {
        d_r_mutex_lock(&alt_tls_lock);
        res = alt_tls_release_helper(alt_tls_spare_taken, (uint)base_offs, teb_offs,
                                     num_slots);
        d_r_mutex_unlock(&alt_tls_lock);
    }
#    ifdef X64
    if (!res) {
        if (teb_offs >= TLS_POSTTEB_BASE_OFFS &&
            teb_offs < TLS_POSTTEB_BASE_OFFS + TLS_POSTTEB_SLOTS * sizeof(void *)) {
            d_r_mutex_lock(&alt_tls_lock);
            res = alt_tls_release_helper(alt_tls_post_taken, TLS_POSTTEB_BASE_OFFS,
                                         teb_offs, num_slots);
            d_r_mutex_unlock(&alt_tls_lock);
        }
    }
#    endif
    return res;
}

static inline uint
tls_segment_offs(int slot)
{
    return (uint)(offsetof(TEB, TlsSlots) + slot * sizeof(uint *));
}

/* returns the first block sequence of num_slots found either bottom
 * up or top_down, that has the selected slot aligned to given alignment.
 * Returns -1 on failure to find properly aligned sequence.
 *
 * Note that if we only want the whole sequence to fit in a cache line, callers
 * should try either align_which_slot for either first or last.
 */
int
bitmap_find_free_sequence(byte *rtl_bitmap, int bitmap_size, int num_requested_slots,
                          bool top_down, int align_which_slot, /* 0 based index */
                          uint alignment)
{
    /* note: bitmap_find_set_block_sequence() works similarly on our
     * internal bitmap_t which starts initialized to 0
     */
    uint *p = (uint *)rtl_bitmap; /* we access in 32-bit words */
    int start, open_end;
    int step; /* +/- 1 */
    int i;
    int contig = 0;
    int result;

    ASSERT(ALIGNED(rtl_bitmap, sizeof(uint))); /* they promised */
    ASSERT_CURIOSITY(bitmap_size == 64 /*TLS*/ || bitmap_size == 128 /*FLS*/);
    ASSERT(num_requested_slots < bitmap_size);
    ASSERT_CURIOSITY(alignment < 256);
    ASSERT(align_which_slot >= 0 && /* including after last */
           align_which_slot <= num_requested_slots);

    if (top_down) {
        start = bitmap_size - 1;
        open_end = -1; /* 0 included */
        step = -1;
    } else {
        start = 0;
        open_end = bitmap_size;
        step = +1;
    }

    for (i = start; i != open_end; i += step) {
        uint taken = p[i / 32] & (1 << (i % 32));
        NTPRINT("tls slot %d is %d\n", i, taken);
        if (!taken) {
            if (contig == 0) {
                /* check whether first element will be aligned */
                /* don't bother starting if not */
                /* FIXME: could add an argument which slot should be aligned here  */
                int proposed_align_slot = /* first */
                    (top_down ? i - (num_requested_slots - 1) : i) + align_which_slot;
                /* ALIGNED doesn't work for 0 so we have to special-case it */
                bool aligned =
                    (alignment == 0 ||
                     ALIGNED(tls_segment_offs(proposed_align_slot), alignment));

                NTPRINT("\t => @ " PFX ", pivot " PFX " %saligned to 0x%x\n",
                        tls_segment_offs(i), tls_segment_offs(proposed_align_slot),
                        aligned ? "" : "not ", alignment);

                if (aligned)
                    contig++;
                else
                    contig = 0; /* try at next */
            } else
                contig++;
            NTPRINT("\t => %d contig @ " PFX "\n", contig, tls_segment_offs(i));
            ASSERT(contig <= num_requested_slots);
            if (contig == num_requested_slots)
                break;
        } else {
            contig = 0; /* start over! */
        }
    }
    if (contig < num_requested_slots) {
        result = -1; /* failure */
    } else {
        result = top_down ? i : i - (num_requested_slots - 1);
        ASSERT(i >= 0 && i < bitmap_size);
        /* ALIGNED doesn't work for 0 so we have to special-case it */
        ASSERT(alignment == 0 ||
               ALIGNED(tls_segment_offs(result + align_which_slot), alignment));
    }
    return result;
}

void
bitmap_mark_taken_sequence(byte *rtl_bitmap, int bitmap_size, int first_slot,
                           int last_slot_open_end)
{
    int i;
    uint *p = (uint *)rtl_bitmap; /* we access in 32-bit words */

    ASSERT(ALIGNED(rtl_bitmap, sizeof(uint))); /* they promised */
    ASSERT(first_slot >= 0 && last_slot_open_end <= bitmap_size);
    for (i = first_slot; i < last_slot_open_end; i++)
        p[i / 32] |= (1 << (i % 32));
}

void
bitmap_mark_freed_sequence(byte *rtl_bitmap, int bitmap_size, int first_slot,
                           int num_slots)
{
    int i;
    uint *p = (uint *)rtl_bitmap; /* we access in 32-bit words */
    for (i = first_slot; i < first_slot + num_slots; i++)
        p[i / 32] &= ~(1 << (i % 32));
}

/* Our version of kernel32's TlsAlloc
 * If synch is false, assumes that the peb lock does not need to be obtained,
 * which may be safer than acquiring the lock, though when there's only a single
 * thread it shouldn't make any difference (it's a recursive lock).
 */
static bool
tls_alloc_helper(int synch, uint *teb_offs /* OUT */, int num_slots, uint alignment,
                 uint tls_flags)
{
    PEB *peb = get_own_peb();
    int start;
    RTL_BITMAP local_bitmap;
    bool using_local_bitmap = false;

    NTSTATUS res;
    if (synch) {
        /* XXX: I read somewhere they are removing more PEB pointers in Vista or
         * earlier..
         */
        /* TlsAlloc calls RtlAcquirePebLock which calls RtlEnterCriticalSection */
        res = RtlEnterCriticalSection(peb->FastPebLock);
        if (!NT_SUCCESS(res))
            return false;
    }

    /* we align the fs offset and assume that the fs base is page-aligned */
    ASSERT(alignment < PAGE_SIZE);

    /* Transparency notes: we doubt any app relies on a particular slot to be available.
     * These are dynamic TLS slots, after all, used only for dlls, who don't know
     * which other dlls may be in the address space.  The app is going to use
     * static TLS.  Furthermore, NT only has 64 slots available, so it's unlikely
     * an app uses up all the available TLS slots (though we have to have one that's
     * in the TEB itself, meaning one of the first 64).
     * We walk backward in an attempt to not disrupt the dynamic sequence if only a
     * few are in use.
     */
    /* case 6770: SQL Server 2005 broke most of the above assumptions:
     * - it allocates 38 TLS entries and expects them to all be in
     * TLS64 furthermore it assumes that 38 consecutive calls to
     * TlsAlloc() return consecutive TLS slots.  Therefore we should
     * have to make sure we do not leave any slots in a shorter
     * earlier sequence available.  Although SQL can't handle going
     * into the TlsExpansionBitMap
     */

    if (peb->TlsBitmap == NULL) {
        /* Not initialized yet so use a temp struct to point at the real bits.
         * FIXME i#812: ensure our bits here don't get zeroed when ntdll is initialized
         */
        ASSERT(dr_earliest_injected);
        using_local_bitmap = true;
        peb->TlsBitmap = &local_bitmap;
        local_bitmap.SizeOfBitMap = 64;
        local_bitmap.BitMapBuffer = (void *)&peb->TlsBitmapBits;
    } else
        ASSERT(peb->TlsBitmap != NULL);
    /* TlsBitmap always points to next field, TlsBitmapBits, but we'll only
     * use the pointer for generality
     */
    ASSERT(&peb->TlsBitmapBits == (void *)peb->TlsBitmap->BitMapBuffer);

    DOCHECK(1, {
        int first_available = bitmap_find_free_sequence(
            peb->TlsBitmap->BitMapBuffer, peb->TlsBitmap->SizeOfBitMap, 1, /* single */
            false,                                                         /* bottom up */
            0, 0 /* no alignment */);
        /* On XP ntdll seems to grab slot 0 of the TlsBitmap before loading
         * kernel32, see if early injection gets us before that */
        /* On Win2k usually first_available == 0, but not in some
         * runall tests, so can't assert on the exact value */
        ASSERT_CURIOSITY(first_available >= 0);
    });

    /* only when filling need to find a first_empty in release */
    /* TLS_FLAG_BITMAP_FILL - should first find a single slot
     * available, then look for whole sequence, then should go through
     * and mark ALL entries inbetween.  Of course we know we can't go
     * beyond index 63 in either request.
     */
    if (TEST(TLS_FLAG_BITMAP_FILL, tls_flags)) {
        int first_to_fill = bitmap_find_free_sequence(
            peb->TlsBitmap->BitMapBuffer, peb->TlsBitmap->SizeOfBitMap, 1, /* single */
            false,                                                         /* bottom up */
            0, 0 /* no alignment */);
        ASSERT_NOT_TESTED();
        /* we only fill from the front - and taking all up to the top isn't nice */
        ASSERT(!TEST(TLS_FLAG_BITMAP_TOP_DOWN, tls_flags));
        ASSERT_NOT_IMPLEMENTED(false);
        /* FIXME: need to save first slot, so we can free the
         * filled slots on exit */
    }

    /* TLS_FLAG_BITMAP_TOP_DOWN will take a slot at end if possible
     * for better transparency, also for better reproducibility */

    /* TLS_FLAG_CACHE_LINE_START - will align the first entry,
     * otherwise align either first or last since we only care to fit
     * on a line */
    /* FIXME: align at specific element - not necessary since not
     * aligning at all works well for our current choice
     */

    /*  Note the TLS64 is at fs:[0xe10-0xf10)
     *  0xf00 is a cache line start for either 32 or 64 byte
     *
     *  If we want to have commonly used items on the same cache line,
     *  but also could care about starting at its beginning (not
     *  expected to matter for data but should measure).
     *
     *  If we only needed 4 slots 0xf00 would be at a cache line start
     *  and satisfy all requirements.
     *
     *  If we can get not so important items to cross the line, then
     *  we can have 0xf00 as the balancing item, and the previous 8
     *  slots will be in one whole cache line on both 32 and 64 byte.
     *  If we keep it at that then we don't really need alignment hint
     *  at all - grabbing last is good enough.
     *
     *  Only on P4 we can fit more than 8 entries on the same cache
     *  line if presumed to all be hot, then we have to use 0xec0 as
     *  start and leave empty the 0xf00 line.  On P3 however we can
     *  use 0xee0 - only in DEBUG=+HASHTABLE_STATISTICS we use one
     *  extra slot that ends up at 0xec0.  The minor point for P4 is
     *  then whether we use the first 12 or the last 12 slots in the
     *  cache line.
     */

    /* FIXME: cache line front, otherwise should retry when either
     * start or end is fine, and choose closest to desired end of
     * bitmap */
    start = bitmap_find_free_sequence(peb->TlsBitmap->BitMapBuffer,
                                      peb->TlsBitmap->SizeOfBitMap, num_slots,
                                      TEST(TLS_FLAG_BITMAP_TOP_DOWN, tls_flags),
                                      0 /* align first element */, alignment);

    if (!TEST(TLS_FLAG_CACHE_LINE_START, tls_flags)) {
        /* try either way, worthwhile only if we fit into an alignment unit */
        int end_aligned = bitmap_find_free_sequence(
            peb->TlsBitmap->BitMapBuffer, peb->TlsBitmap->SizeOfBitMap, num_slots,
            TEST(TLS_FLAG_BITMAP_TOP_DOWN, tls_flags),
            /* align the end of last
             * element, so open ended */
            num_slots, alignment);
        if (start < 0) {
            ASSERT_NOT_TESTED();
            start = end_aligned;
        } else {
            if (TEST(TLS_FLAG_BITMAP_TOP_DOWN, tls_flags)) {
                /* prefer latest start */
                if (start < end_aligned) {
                    start = end_aligned;
                    ASSERT_NOT_TESTED();
                }
            } else {
                /* bottom up, prefer earlier start */
                if (start > end_aligned) {
                    start = end_aligned;
                }
            }
        }
    }

    if (start < 0) {
        NTPRINT("Failed to find %d slots aligned at %d\n", num_slots, alignment);
        goto tls_alloc_exit;
    }

    bitmap_mark_taken_sequence(peb->TlsBitmap->BitMapBuffer, peb->TlsBitmap->SizeOfBitMap,
                               start,
                               /* FIXME: TLS_FLAG_BITMAP_FILL should use first_to_fill */
                               start + num_slots);

    if (teb_offs != NULL) {
        *teb_offs = tls_segment_offs(start);
        /* mostly safe since using the small TLS map (of 64 entries)
         * and that is on TEB so reachable with a short */
        /* to avoid ASSERT_TRUNCATE in os_tls_offset() checking here */
        ASSERT_TRUNCATE(ushort, ushort, *teb_offs);
        NTPRINT("Taking %d tls slot(s) %d-%d at offset 0x%x\n", num_slots, start,
                start + num_slots, *teb_offs);
    }

    DOCHECK(1, {
        int first_available = bitmap_find_free_sequence(
            peb->TlsBitmap->BitMapBuffer, peb->TlsBitmap->SizeOfBitMap, 1, /* single */
            false,                                                         /* bottom up */
            0, 0 /* no alignment */);
        ASSERT_CURIOSITY(first_available >= 0);

        /* SQL2005 assumes that first available slot means start of a
         * sequence of 38 blanks that fit in TLS64.  Unfortunately
         * can't assert this for all processes, since even for make
         * progrun (notepad on XP SP2, late injection) 16 bits are
         * already taken by others.  Worse, exactly in SQL server on
         * Win2k, at the time we are started there is room, but later
         * loaded DLLs use it.  Case 6859 on other attempts to catch
         * the problem.
         */
    });

tls_alloc_exit:
    if (using_local_bitmap)
        peb->TlsBitmap = NULL;

    if (synch) {
        res = RtlLeaveCriticalSection(peb->FastPebLock);
        if (!NT_SUCCESS(res))
            return false;
    }

    /* ntdll seems to grab slot 0 of the TlsBitmap before loading
     * kernel32, see if early injection gets us before that if we go
     * bottom up, FIXME: if hit change interface, since 0 is returned
     * on error
     */
    ASSERT_CURIOSITY(start != 0);

    if (start <= 0 && DYNAMO_OPTION(alt_teb_tls)) {
        /* i#1163: fall back on other space in TEB */
        return alt_tls_acquire(teb_offs, num_slots, alignment);
    }

    return (start > 0);
}

bool
tls_alloc(int synch, uint *teb_offs /* OUT */)
{
    return tls_alloc_helper(synch, teb_offs, 1, 0 /* any alignment */,
                            /* same top down or bottom up choice as tls_calloc */
                            DYNAMO_OPTION(tls_flags));
}

/* Allocates num tls slots aligned with particular alignment
 * Alignment must be sub-page
 */
bool
tls_calloc(int synch, uint *teb_offs /* OUT */, int num, uint alignment)
{
    return tls_alloc_helper(synch, teb_offs, num, alignment, DYNAMO_OPTION(tls_flags));
}

static bool
tls_free_helper(int synch, uint teb_offs, int num)
{
    PEB *peb = get_own_peb();
    int i, start;
    int slot;
    uint *p;

    NTSTATUS res;
    GET_NTDLL(RtlTryEnterCriticalSection, (IN OUT RTL_CRITICAL_SECTION * crit));

    if (DYNAMO_OPTION(alt_teb_tls) && alt_tls_release(teb_offs, num))
        return true;

    if (synch) {
        /* TlsFree calls RtlAcquirePebLock which calls RtlEnterCriticalSection
         * I'm worried about synch problems so I'm going to just do a Try
         * and if it fails I simply will not free the slot, not too bad of a leak.
         * On a detach a suspended thread might be holding this lock, or a thread
         * killed due to an attack might have held it.  We could, on failure to
         * get the lock, xchg and read back what we write and try to fix up the bits,
         * with the worst case being the app hasn't written but has read and thus
         * our free won't go through, but in the past we just called TlsFree and
         * never had a lock problem so I'm going to assume Try will work the vast
         * majority of the time and the times it doesn't we can eat the leak.
         */
        res = RtlTryEnterCriticalSection(peb->FastPebLock);
        ASSERT_CURIOSITY(NT_SUCCESS(res));
        if (!NT_SUCCESS(res))
            return false;
    }

    ASSERT(peb->TlsBitmap != NULL);
    /* TlsBitmap always points to next field, TlsBitmapBits, but we'll only
     * use the pointer for generality
     */
    p = (uint *)peb->TlsBitmap->BitMapBuffer;
    start = (teb_offs - offsetof(TEB, TlsSlots)) / sizeof(uint *);
    for (slot = 0, i = start; slot < num; slot++, i++) {
        NTPRINT("Freeing tls slot %d at offset 0x%x -> index %d\n", slot, teb_offs, i);
        /* In case we aren't synched, zero the tls field before we release it,
         * (of course that only takes care of one of many possible races if we
         * aren't synched). */
        /* This will zero this tls index for all threads (see disassembly of
         * FreeTls in kernel32, wine srcs).  Strange interface using a
         * thread handle, would be more sensical as a process info class (esp.
         * with respect to permissions).  Note that in the wine srcs at least
         * this syscall will only accept NT_CURRENT_THREAD as the handle. Xref
         * case 8143 for why we need to zero the tls slot for all threads. */
        /* XXX i#1156: we can't zero on win8 where we write the
         * termination syscall args into our TLS slots (i#565, r1630).
         * We always synch there though.
         */
        if (!synch || doing_detach) {
            res = nt_raw_SetInformationThread(NT_CURRENT_THREAD, ThreadZeroTlsCell, &i,
                                              sizeof(i));
            ASSERT(NT_SUCCESS(res));
        }
        p[i / 32] &= ~(1 << (i % 32));
    }
    bitmap_mark_freed_sequence(peb->TlsBitmap->BitMapBuffer, peb->TlsBitmap->SizeOfBitMap,
                               start, num);

    if (synch) {
        res = RtlLeaveCriticalSection(peb->FastPebLock);
        ASSERT(NT_SUCCESS(res));
        if (!NT_SUCCESS(res))
            return false;
    }

    return true;
}

bool
tls_free(int synch, uint teb_offs)
{
    return tls_free_helper(synch, teb_offs, 1);
}

bool
tls_cfree(int synch, uint teb_offs, int num)
{
    return tls_free_helper(synch, teb_offs, num);
}

#endif /* !NOT_DYNAMORIO_CORE_PROPER */
/***************************************************************************/

bool
get_process_mem_stats(HANDLE h, VM_COUNTERS *info)
{
    NTSTATUS res;
    ULONG got;
    res =
        NtQueryInformationProcess(h, ProcessVmCounters, info, sizeof(VM_COUNTERS), &got);
    ASSERT(!NT_SUCCESS(res) || got == sizeof(VM_COUNTERS));
    return NT_SUCCESS(res);
}

/* Get process quota limits information */
/* Note returns raw NTSTATUS */
NTSTATUS
get_process_mem_quota(HANDLE h, QUOTA_LIMITS *qlimits)
{
    NTSTATUS res;
    ULONG got;
    res = NtQueryInformationProcess(h, ProcessQuotaLimits, qlimits, sizeof(QUOTA_LIMITS),
                                    &got);
    ASSERT(!NT_SUCCESS(res) || got == sizeof(QUOTA_LIMITS));
    return res;
}

/* Get process quota limits information */
/* Note returns raw NTSTATUS */
NTSTATUS
get_process_handle_count(HANDLE ph, ULONG *handle_count)
{
    NTSTATUS res;
    ULONG got;
    res = NtQueryInformationProcess(ph, ProcessHandleCount, handle_count, sizeof(ULONG),
                                    &got);
    ASSERT(!NT_SUCCESS(res) || got == sizeof(ULONG));
    return res;
}

int
get_process_load(HANDLE h)
{
    KERNEL_USER_TIMES times;
    LONGLONG scheduled_time;
    LONGLONG wallclock_time;
    NTSTATUS res;
    ULONG len = 0;
    res = NtQueryInformationProcess((HANDLE)h, ProcessTimes, &times, sizeof(times), &len);
    if (!NT_SUCCESS(res))
        return -1;
    /* return length not trustworthy, according to Nebbett, so we don't test it */
    /* we want %CPU == (scheduled time) / (wall clock time) */
    scheduled_time = times.UserTime.QuadPart + times.KernelTime.QuadPart;
    wallclock_time = query_time_100ns() - times.CreateTime.QuadPart;
    if (wallclock_time <= 0)
        return -1;
    return (int)((100 * scheduled_time) / wallclock_time);
}

/* Returns 0 for both known false and error
 * XXX: do we still have the restriction of not returning a bool for ntdll.c
 * routines?!?
 */
bool
is_wow64_process(HANDLE h)
{
    /* since this is called a lot we remember the result for the current process */
    static bool self_init = false;
    static bool self_is_wow64 = false;
    if (h == 0)
        return false;
    if (!self_init || h != NT_CURRENT_PROCESS) {
        ptr_uint_t is_wow64;
        NTSTATUS res;
        ULONG len = 0;

        res = NtQueryInformationProcess((HANDLE)h, ProcessWow64Information, &is_wow64,
                                        sizeof(is_wow64), &len);
        if (!NT_SUCCESS(res) || len != sizeof(is_wow64)) {
#if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)
            /* PR 233191: we expect failure on NT but nowhere else */
            ASSERT(res == STATUS_INVALID_INFO_CLASS &&
                   get_os_version() == WINDOWS_VERSION_NT);
#endif
            is_wow64 = 0;
        }

        if (h == NT_CURRENT_PROCESS) {
#if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)
            ASSERT(!dynamo_initialized); /* .data should be writable */
#endif
            self_is_wow64 = (is_wow64 != 0);
            self_init = true;
        }
        return (is_wow64 != 0);
    }
    return self_is_wow64;
}

bool
is_32bit_process(HANDLE h)
{
#ifdef X64
    /* Kernel is definitely 64-bit. */
    return is_wow64_process(h);
#else
    /* If kernel is 64-bit, ask about wow64; else, kernel is 32-bit, so true. */
    if (is_wow64_process(NT_CURRENT_PROCESS))
        return is_wow64_process(h);
    else
        return true;
#endif
}

NTSTATUS
nt_get_drive_map(HANDLE process, PROCESS_DEVICEMAP_INFORMATION *map OUT)
{
    ULONG len = 0;
    return NtQueryInformationProcess(process, ProcessDeviceMap, map, sizeof(*map), &len);
}

/* use base hint if present; will bump size up to PAGE_SIZE multiple
 * Note returns raw NTSTATUS.
 */
NTSTATUS
nt_remote_allocate_virtual_memory(HANDLE process, void **base, size_t size, uint prot,
                                  memory_commit_status_t commit)
{
    NTSTATUS res;
    SIZE_T sz = size;
    ASSERT(ALIGNED(*base, PAGE_SIZE) && "base argument not initialized at PAGE_SIZE");
    res = NT_SYSCALL(AllocateVirtualMemory, process, base, 0 /* zero bits */, &sz, commit,
                     prot);
    if (res == STATUS_CONFLICTING_ADDRESSES) {
        NTPRINT("NtAllocateVirtualMemory: conflict at base " PFX ", res=" PFX "\n", *base,
                res);
        /* Let caller decide whether to retry or not. */
    }

    /* FIXME: alert caller if sz > size? only happens if size not PAGE_SIZE multiple */
    NTPRINT("NtAllocateVirtualMemory: asked for %d bytes, got %d bytes at " PFX "\n",
            size, sz, *base);
    ASSERT(sz >= size);
    return res;
}

/* Decommit memory previously committed with nt_remote_allocate_virtual_memory()
 * Note returns raw NTSTATUS.
 */
NTSTATUS
nt_remote_free_virtual_memory(HANDLE process, void *base)
{
    NTSTATUS res;
    SIZE_T sz = 0; /* has to be 0 for MEM_RELEASE */
    res = NT_SYSCALL(FreeVirtualMemory, process, &base, &sz, MEM_RELEASE);
    NTPRINT("NtRemoteFreeVirtualMemory: freed " SZFMT " bytes\n", sz);
    return res;
}

/* use base hint is present; will bump size up to PAGE_SIZE multiple
 * Note returns raw NTSTATUS.
 */
NTSTATUS
nt_allocate_virtual_memory(void **base, size_t size, uint prot,
                           memory_commit_status_t commit)
{
    return nt_remote_allocate_virtual_memory(NT_CURRENT_PROCESS, base, size, prot,
                                             commit);
}

/* commit memory previously reserved with nt_allocate_virtual_memory()
 * Note returns raw NTSTATUS.
 */
NTSTATUS
nt_commit_virtual_memory(void *base, size_t size, uint prot)
{
    NTSTATUS res;
    DEBUG_DECLARE(void *original_base = base;)
    DEBUG_DECLARE(size_t original_size = size;)
    res = NT_SYSCALL(AllocateVirtualMemory, NT_CURRENT_PROCESS, &base, 0, (SIZE_T *)&size,
                     MEM_COMMIT, /* should be already reserved */ prot);
    ASSERT(base == original_base);
    ASSERT(size == original_size);
    ASSERT_CURIOSITY(NT_SUCCESS(res));
    return res;
}

/* Decommit memory previously committed with nt_commit_virtual_memory() or
 * nt_allocate_virtual_memory().  Still available for committing again.
 * Note returns raw NTSTATUS.
 */
NTSTATUS
nt_decommit_virtual_memory(void *base, size_t size)
{
    NTSTATUS res;
    SIZE_T sz = size; /* copied to compare with OUT value */
    res = NT_SYSCALL(FreeVirtualMemory, NT_CURRENT_PROCESS, &base, &sz, MEM_DECOMMIT);
    ASSERT(sz == size);
    NTPRINT("NtFreeVirtualMemory: decommitted %d bytes [res=%d]\n", sz, res);
    ASSERT_CURIOSITY(NT_SUCCESS(res));
    return res;
}

/* Decommit memory previously committed with nt_commit_virtual_memory() or
 * nt_allocate_virtual_memory().  Still available for committing again.
 * Note returns raw NTSTATUS.
 */
NTSTATUS
nt_free_virtual_memory(void *base)
{
    NTSTATUS res;
    SIZE_T sz = 0; /* has to be 0 for MEM_RELEASE */
    res = NT_SYSCALL(FreeVirtualMemory, NT_CURRENT_PROCESS, &base, &sz, MEM_RELEASE);
    NTPRINT("NtFreeVirtualMemory: freed " SZFMT " bytes\n", sz);
    ASSERT_CURIOSITY(NT_SUCCESS(res));
    return res;
}

/* FIXME: change name to nt_protect_virtual_memory() and use
 * nt_remote_protect_virtual_memory(), or maybe just change callers to
 * pass NT_CURRENT_PROCESS to nt_remote_protect_virtual_memory()
 * instead to avoid the extra function call, especially with self-protection on
 */
bool
protect_virtual_memory(void *base, size_t size, uint prot, uint *old_prot)
{
    NTSTATUS res;
    SIZE_T sz = size;
    res = NT_SYSCALL(ProtectVirtualMemory, NT_CURRENT_PROCESS, &base, &sz, prot,
                     (ULONG *)old_prot);
    NTPRINT("NtProtectVirtualMemory: " PFX "-" PFX " 0x%x => 0x%x\n", base,
            (byte *)base + size, prot, res);
    ASSERT(sz == ALIGN_FORWARD(size, PAGE_SIZE));
    return NT_SUCCESS(res);
}

bool
nt_remote_protect_virtual_memory(HANDLE process, void *base, size_t size, uint prot,
                                 uint *old_prot)
{
    NTSTATUS res;
    SIZE_T sz = size;
    res = NT_SYSCALL(ProtectVirtualMemory, process, &base, &sz, prot, (ULONG *)old_prot);
    NTPRINT("NtProtectVirtualMemory: process " PFX " " PFX "-" PFX " 0x%x => 0x%x\n",
            process, base, (byte *)base + size, prot, res);

    ASSERT(ALIGNED(base, PAGE_SIZE) && "base argument not initialized at PAGE_SIZE");
    NTPRINT("NtProtectVirtualMemory: intended to change %d bytes, "
            "modified %d bytes at " PFX "\n",
            size, sz, base);
    ASSERT(sz >= size);
    return NT_SUCCESS(res);
}

NTSTATUS
nt_remote_query_virtual_memory(HANDLE process, const byte *pc,
                               MEMORY_BASIC_INFORMATION *mbi, size_t mbilen, size_t *got)
{
    /* XXX: we can't switch this to a raw syscall as we rely on d_r_get_proc_address()
     * working in syscalls_init_get_num(), and it calls get_allocation_size()
     * which ends up here.
     */
    ASSERT(mbilen == sizeof(MEMORY_BASIC_INFORMATION));
    memset(mbi, 0, sizeof(MEMORY_BASIC_INFORMATION));
    return NT_SYSCALL(QueryVirtualMemory, process, pc, MemoryBasicInformation, mbi,
                      mbilen, (PSIZE_T)got);
}

/* We use this instead of VirtualQuery b/c there are problems using
 * win32 API routines inside of the app using them
 */
/* We make our signature look like VirtualQuery */
size_t
query_virtual_memory(const byte *pc, MEMORY_BASIC_INFORMATION *mbi, size_t mbilen)
{
    NTSTATUS res;
    size_t got;
    res = nt_remote_query_virtual_memory(NT_CURRENT_PROCESS, pc, mbi, mbilen, &got);
    ASSERT(!NT_SUCCESS(res) || got == sizeof(MEMORY_BASIC_INFORMATION));
    /* only 0 and sizeof(MEMORY_BASIC_INFORMATION) should be expected by callers */
    if (!NT_SUCCESS(res))
        got = 0;

#if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)
    /* for stress testing a fake driver access */
    if (INTERNAL_OPTION(stress_fake_userva) != 0) {
        if (pc > (app_pc)INTERNAL_OPTION(stress_fake_userva))
            return 0;
    }
#endif

    return got;
}

NTSTATUS
get_mapped_file_name(const byte *pc, PWSTR buf, USHORT buf_bytes)
{
    NTSTATUS res;
    SIZE_T got;
    /* name.SectionFileName.Buffer MUST be inlined: even if Buffer is initialized
     * to point elsewhere, the kernel modifies it.  The size passed in must include
     * the struct and the post-inlined buffer.
     */
    MEMORY_SECTION_NAME *name = (MEMORY_SECTION_NAME *)buf;
    name->SectionFileName.Length = 0;
    name->SectionFileName.MaximumLength = buf_bytes - sizeof(*name);
    name->SectionFileName.Buffer = buf + sizeof(*name);
    res = NT_SYSCALL(QueryVirtualMemory, NT_CURRENT_PROCESS, pc, MemorySectionName, name,
                     buf_bytes, &got);
    if (NT_SUCCESS(res)) {
        /* save since we'll be clobbering the fields */
        int len = name->SectionFileName.Length;
        memmove(buf, name->SectionFileName.Buffer, len);
        buf[len / sizeof(wchar_t)] = L'\0';
    }
    return res;
}

NTSTATUS
nt_raw_read_virtual_memory(HANDLE process, const void *base, void *buffer,
                           size_t buffer_length, size_t *bytes_read)
{
    NTSTATUS res;
    GET_NTDLL(NtReadVirtualMemory,
              (IN HANDLE ProcessHandle, IN const void *BaseAddress, OUT PVOID Buffer,
               IN SIZE_T BufferLength, OUT PSIZE_T ReturnLength OPTIONAL));
    res = NtReadVirtualMemory(process, base, buffer, buffer_length, (SIZE_T *)bytes_read);
    return res;
}

bool
nt_read_virtual_memory(HANDLE process, const void *base, void *buffer,
                       size_t buffer_length, size_t *bytes_read)
{
    return NT_SUCCESS(
        nt_raw_read_virtual_memory(process, base, buffer, buffer_length, bytes_read));
}

NTSTATUS
nt_raw_write_virtual_memory(HANDLE process, void *base, const void *buffer,
                            size_t buffer_length, size_t *bytes_written)
{
    NTSTATUS res;
    GET_RAW_SYSCALL(WriteVirtualMemory, IN HANDLE ProcessHandle, IN PVOID BaseAddress,
                    IN const void *Buffer, IN SIZE_T BufferLength,
                    OUT PSIZE_T ReturnLength OPTIONAL);
    res = NT_SYSCALL(WriteVirtualMemory, process, base, buffer, buffer_length,
                     (SIZE_T *)bytes_written);
    return res;
}

bool
nt_write_virtual_memory(HANDLE process, void *base, const void *buffer,
                        size_t buffer_length, size_t *bytes_written)
{
    return NT_SUCCESS(
        nt_raw_write_virtual_memory(process, base, buffer, buffer_length, bytes_written));
}

/* There are no Win32 API routines to do this, so we use NtContinue */
void
nt_continue(CONTEXT *cxt)
{
    GET_RAW_SYSCALL(Continue, IN PCONTEXT Context, IN BOOLEAN TestAlert);
    NT_SYSCALL(Continue, cxt, 0 /* don't change APC status */);
    /* should not get here */
    ASSERT_NOT_REACHED();
}

NTSTATUS
nt_get_context(HANDLE hthread, CONTEXT *cxt)
{
    GET_RAW_SYSCALL(GetContextThread, IN HANDLE ThreadHandle, OUT PCONTEXT Context);
    /* PR 263338: we get STATUS_DATATYPE_MISALIGNMENT if not aligned */
    IF_X64(ASSERT(ALIGNED(cxt, 16)));
    return NT_SYSCALL(GetContextThread, hthread, cxt);
    /* Don't assert here -- let the caller do so if it expects a particular value.
     * If we asserted here when an ldmp is being generated, we could prevent
     * generation of the ldmp if there is a handle privilege problem between
     * the calling thread and hthread.
     */
}

/* WARNING: any time we set a thread's context we must make sure we can
 * handle two cases:
 * 1) the thread was at a syscall and now we won't recognize it as such
 *    (case 6113) (not to mention that the kernel will finish the
 *    syscall and clobber eax and ecx+edx after setting to cxt: case 5074)
 * 2) the thread just hit a fault but the kernel has not yet copied the
 *    faulting context to the user mode structures for the handler
 *    (case 7393)
 */
NTSTATUS
nt_set_context(HANDLE hthread, CONTEXT *cxt)
{
    GET_RAW_SYSCALL(SetContextThread, IN HANDLE ThreadHandle, IN PCONTEXT Context);
    /* PR 263338: we get STATUS_DATATYPE_MISALIGNMENT if not aligned */
    IF_X64(ASSERT(ALIGNED(cxt, 16)));
    return NT_SYSCALL(SetContextThread, hthread, cxt);
}

bool
nt_is_thread_terminating(HANDLE hthread)
{
    ULONG previous_suspend_count;
    NTSTATUS res;
    GET_RAW_SYSCALL(SuspendThread, IN HANDLE ThreadHandle,
                    OUT PULONG PreviousSuspendCount OPTIONAL);
    res = NT_SYSCALL(SuspendThread, hthread, &previous_suspend_count);
    if (NT_SUCCESS(res)) {
        nt_thread_resume(hthread, (int *)&previous_suspend_count);
    }

    return res == STATUS_THREAD_IS_TERMINATING;
}

bool
nt_thread_suspend(HANDLE hthread, int *previous_suspend_count)
{
    NTSTATUS res;
    GET_RAW_SYSCALL(SuspendThread, IN HANDLE ThreadHandle,
                    OUT PULONG PreviousSuspendCount OPTIONAL);
    res = NT_SYSCALL(SuspendThread, hthread, (ULONG *)previous_suspend_count);
    /* Don't assert here -- let the caller do so if it expects a particular value.
     * If we asserted here when an ldmp is being generated, we could prevent
     * generation of the ldmp if there is a handle privilege problem between
     * the calling thread and hthread.
     */
    return NT_SUCCESS(res);
}

bool
nt_thread_resume(HANDLE hthread, int *previous_suspend_count)
{
    NTSTATUS res;
    GET_RAW_SYSCALL(ResumeThread, IN HANDLE ThreadHandle,
                    OUT PULONG PreviousSuspendCount OPTIONAL);
    res = NT_SYSCALL(ResumeThread, hthread, (ULONG *)previous_suspend_count);
    return NT_SUCCESS(res);
}

#if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)
NTSTATUS
nt_thread_iterator_next(HANDLE hprocess, HANDLE cur_thread, HANDLE *next_thread,
                        ACCESS_MASK access)
{
    if (NtGetNextThread == NULL)
        return STATUS_NOT_IMPLEMENTED;
    return NtGetNextThread(hprocess, cur_thread, access, 0, 0, next_thread);
}
#endif

bool
nt_terminate_thread(HANDLE hthread, NTSTATUS exit_code)
{
    NTSTATUS res;
    GET_RAW_SYSCALL(TerminateThread, IN HANDLE ThreadHandle OPTIONAL,
                    IN NTSTATUS ExitStatus);
    /* hthread == 0 means current thread, match kernel32 TerminateThread which
     * disallows null to avoid bugs in our code (we should always be passing
     * a valid handle or NT_CURRENT_THREAD) */
    ASSERT(hthread != (HANDLE)0);
    res = NT_SYSCALL(TerminateThread, hthread, exit_code);
    ASSERT(hthread != NT_CURRENT_THREAD && "terminate current thread failed");
    return NT_SUCCESS(res);
}

bool
nt_terminate_process(HANDLE hprocess, NTSTATUS exit_code)
{
    NTSTATUS res;
    GET_RAW_SYSCALL(TerminateProcess, IN HANDLE ProcessHandle OPTIONAL,
                    IN NTSTATUS ExitStatus);
    /* hprocess == 0 has special meaning (terminate all threads but this one),
     * kernel32!TerminateProcess disallows it and we currently don't use
     * that functionality */
    ASSERT(hprocess != (HANDLE)0);
    res = NT_SYSCALL(TerminateProcess, hprocess, exit_code);
    ASSERT(hprocess != NT_CURRENT_PROCESS && "terminate current process failed");
    return NT_SUCCESS(res);
}

NTSTATUS
nt_terminate_process_for_app(HANDLE hprocess, NTSTATUS exit_code)
{
    GET_RAW_SYSCALL(TerminateProcess, IN HANDLE ProcessHandle OPTIONAL,
                    IN NTSTATUS ExitStatus);
    /* we allow any argument or result values */
    return NT_SYSCALL(TerminateProcess, hprocess, exit_code);
}

NTSTATUS
nt_set_information_process_for_app(HANDLE hprocess, PROCESSINFOCLASS class, void *info,
                                   ULONG info_len)
{
    GET_RAW_SYSCALL(SetInformationProcess, IN HANDLE hprocess, IN PROCESSINFOCLASS class,
                    INOUT void *info, IN ULONG info_len);
    /* We allow any argument or result value. */
    return NT_SYSCALL(SetInformationProcess, hprocess, class, info, info_len);
}

bool
am_I_sole_thread(HANDLE hthread, int *amI /*OUT*/)
{
    NTSTATUS res;
    ULONG got;
    res = NT_SYSCALL(QueryInformationThread, hthread, ThreadAmILastThread, amI,
                     sizeof(*amI), &got);
    return NT_SUCCESS(res);
}

/* checks current thread, and turns errors into false */
bool
check_sole_thread()
{
    int amI;
    if (!am_I_sole_thread(NT_CURRENT_THREAD, &amI))
        return false;
    else
        return (amI != 0);
}

HANDLE
nt_create_and_set_timer(PLARGE_INTEGER due_time, LONG period)
{
    NTSTATUS res;
    HANDLE htimer;
    enum { NotificationTimer, SynchronizationTimer };

    GET_NTDLL(NtCreateTimer,
              (OUT PHANDLE TimerHandle, IN ACCESS_MASK DesiredAccess,
               IN POBJECT_ATTRIBUTES ObjectAttributes, IN DWORD TimerType /* TIMER_TYPE */
               ));
    res = NtCreateTimer(&htimer, TIMER_ALL_ACCESS, NULL /* no name */,
                        SynchronizationTimer);
    ASSERT(NT_SUCCESS(res));
    {
        GET_NTDLL(NtSetTimer,
                  (IN HANDLE TimerHandle, IN PLARGE_INTEGER DueTime,
                   IN PVOID TimerApcRoutine, /* PTIMER_APC_ROUTINE */
                   IN PVOID TimerContext, IN BOOLEAN Resume, IN LONG Period,
                   OUT PBOOLEAN PreviousState));
        res = NtSetTimer(htimer, due_time, NULL, NULL, false, period, NULL);
        ASSERT(NT_SUCCESS(res));
    }
    return htimer;
}

bool
nt_sleep(PLARGE_INTEGER due_time)
{
    NTSTATUS res;
    GET_NTDLL(NtDelayExecution, (IN BOOLEAN Alertable, IN PLARGE_INTEGER Interval));
    res = NtDelayExecution(false, /* non alertable sleep */
                           due_time);
    return NT_SUCCESS(res);
}

void
nt_yield()
{
    GET_NTDLL(NtYieldExecution, (VOID));
    NtYieldExecution();
}

void *
get_section_address(HANDLE h)
{
    SECTION_BASIC_INFORMATION info;
    NTSTATUS res;
    ULONG got;
    memset(&info, 0, sizeof(SECTION_BASIC_INFORMATION));
    res = NtQuerySection(h, SectionBasicInformation, &info,
                         sizeof(SECTION_BASIC_INFORMATION), &got);
    ASSERT(NT_SUCCESS(res) && got == sizeof(SECTION_BASIC_INFORMATION));
    return info.BaseAddress;
}

/* returns true if attributes can be read and sets them,
 * otherwise the values are not modified
 */
bool
get_section_attributes(HANDLE h, uint *section_attributes /* OUT */,
                       LARGE_INTEGER *section_size /* OPTIONAL OUT */)
{
    SECTION_BASIC_INFORMATION info;
    NTSTATUS res;
    ULONG got;
    memset(&info, 0, sizeof(SECTION_BASIC_INFORMATION));
    ASSERT(section_attributes != NULL);
    res = NtQuerySection(h, SectionBasicInformation, &info,
                         sizeof(SECTION_BASIC_INFORMATION), &got);
    if (NT_SUCCESS(res)) {
        ASSERT(got == sizeof(SECTION_BASIC_INFORMATION));
        *section_attributes = info.Attributes;
        if (section_size != NULL) {
            *section_size = info.Size;
        }
        return true;
    } else {
        /* Unfortunately, we are often passed section handles that are
         * created as GrantedAccess 0xe: None,
         * MapWrite,MapRead,MapExecute which cannot be queried
         */
        return false;
    }
}

NTSTATUS
nt_raw_close(HANDLE h)
{
    GET_RAW_SYSCALL(Close, IN HANDLE Handle);
    return NT_SYSCALL(Close, h);
}

bool
close_handle(HANDLE h)
{
    return NT_SUCCESS(nt_raw_close(h));
}

/* Note returns raw NTSTATUS */
NTSTATUS
duplicate_handle(HANDLE source_process, HANDLE source, HANDLE target_process,
                 HANDLE *target, ACCESS_MASK access, uint attributes, uint options)
{
    NTSTATUS res;
    GET_RAW_SYSCALL(DuplicateObject, IN HANDLE SourceProcessHandle,
                    IN HANDLE SourceHandle, IN HANDLE TargetProcessHandle,
                    OUT PHANDLE TargetHandle OPTIONAL, IN ACCESS_MASK DesiredAcess,
                    IN ULONG Atrributes, IN ULONG options_t);
    res = NT_SYSCALL(DuplicateObject, source_process, source, target_process, target,
                     access, attributes, options);
    return res;
}

GET_NTDLL(NtQueryObject,
          (IN HANDLE ObjectHandle, IN OBJECT_INFORMATION_CLASS ObjectInformationClass,
           OUT PVOID ObjectInformation, IN ULONG ObjectInformationLength,
           OUT PULONG ReturnLength OPTIONAL));

ACCESS_MASK
nt_get_handle_access_rights(HANDLE handle)
{
    NTSTATUS res;
    OBJECT_BASIC_INFORMATION obj_info;
    ULONG needed_length;
    res = NtQueryObject(handle, ObjectBasicInformation, &obj_info, sizeof(obj_info),
                        &needed_length);
    ASSERT(needed_length == sizeof(obj_info));
    ASSERT(NT_SUCCESS(res));
    return obj_info.GrantedAccess;
}

/* byte_length is total size of UNICODE_STRING struct and an embedded buffer */
NTSTATUS
nt_get_object_name(HANDLE handle, OBJECT_NAME_INFORMATION *object_name /* OUT */,
                   uint byte_length, uint *returned_byte_length /* OUT */)
{
    NTSTATUS res;
    res = NtQueryObject(handle, ObjectNameInformation, object_name, byte_length,
                        (ULONG *)returned_byte_length);
    ASSERT(NT_SUCCESS(res));
    return res;
}

NTSTATUS
wchar_to_unicode(PUNICODE_STRING dst, PCWSTR src)
{
    NTSTATUS res;
    GET_NTDLL(RtlInitUnicodeString,
              (IN OUT PUNICODE_STRING DestinationString, IN PCWSTR SourceString));
    res = RtlInitUnicodeString(dst, src);
    return res;
}

/* we don't want to allocate memory, so caller must provide
 * a buffer that's big enough for char -> wchar conversion
 */
static NTSTATUS
char_to_unicode(PUNICODE_STRING dst, PCSTR src, PWSTR buf, size_t buflen)
{
    _snwprintf(buf, buflen, L"%S", src);
    return wchar_to_unicode(dst, buf);
}

static void
char_to_ansi(PANSI_STRING dst, const char *str)
{
    GET_NTDLL(RtlInitAnsiString,
              (IN OUT PANSI_STRING DestinationString, IN PCSTR SourceString));
    RtlInitAnsiString(dst, str);
}

/* Collects file attributes.
 * Returns 1 if successful; 0 otherwise.
 * (Using bool is problematic for non-core users.)
 */
bool
query_full_attributes_file(IN PCWSTR filename, OUT PFILE_NETWORK_OPEN_INFORMATION info)
{
    NTSTATUS result;
    OBJECT_ATTRIBUTES attributes;
    UNICODE_STRING objname;

    memset(&attributes, 0, sizeof(attributes));
    wchar_to_unicode(&objname, filename);
    InitializeObjectAttributes(&attributes, &objname, OBJ_CASE_INSENSITIVE, NULL, NULL);

    result = nt_raw_QueryFullAttributesFile(&attributes, info);

    return NT_SUCCESS(result);
}

NTSTATUS
nt_query_value_key(IN HANDLE key, IN PUNICODE_STRING value_name,
                   IN KEY_VALUE_INFORMATION_CLASS class, OUT PVOID info,
                   IN ULONG info_length, OUT PULONG res_length)
{
    GET_NTDLL(NtQueryValueKey,
              (IN HANDLE KeyHandle, IN PUNICODE_STRING ValueName,
               IN KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass,
               OUT PVOID KeyValueInformation, IN ULONG Length, OUT PULONG ResultLength));
    return NtQueryValueKey(key, value_name, class, info, info_length, res_length);
}

/* rights should be KEY_READ or KEY_WRITE or both */
/* parent handle HAS to be opened with an absolute name */
HANDLE
reg_create_key(HANDLE parent, PCWSTR keyname, ACCESS_MASK rights)
{
    NTSTATUS res;
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING objname;
    ULONG disp;
    HANDLE hkey;

    res = wchar_to_unicode(&objname, keyname);
    if (!NT_SUCCESS(res))
        return NULL;
    InitializeObjectAttributes(&attr, &objname, OBJ_CASE_INSENSITIVE, parent, NULL);
    res = nt_raw_CreateKey(&hkey, rights, &attr, 0, NULL, 0, &disp);
    if (!NT_SUCCESS(res)) {
        NTPRINT("Error 0x%x in create key for \"%S\"\n", res, objname.Buffer);
        return NULL;
    } else
        return hkey;
}

/* rights should be KEY_READ or KEY_WRITE or both */
HANDLE
reg_open_key(PCWSTR keyname, ACCESS_MASK rights)
{
    NTSTATUS res;
    HANDLE hkey;
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING objname;
    GET_RAW_SYSCALL(OpenKey, OUT PHANDLE KeyHandle, IN ACCESS_MASK DesiredAccess,
                    IN POBJECT_ATTRIBUTES ObjectAttributes);
    res = wchar_to_unicode(&objname, keyname);
    if (!NT_SUCCESS(res)) {
        NTPRINT("Error in wchar to unicode\n");
        return NULL;
    }

    InitializeObjectAttributes(&attr, &objname, OBJ_CASE_INSENSITIVE, NULL, NULL);
    res = NT_SYSCALL(OpenKey, &hkey, rights, &attr);
    if (!NT_SUCCESS(res)) {
        NTPRINT("Error 0x%x in open key for \"%S\"\n", res, objname.Buffer);
        return NULL;
    } else
        return hkey;
}

bool
reg_close_key(HANDLE hkey)
{
    return close_handle(hkey);
}

bool
reg_delete_key(HANDLE hkey)
{
    NTSTATUS res;
    GET_NTDLL(NtDeleteKey, (IN HANDLE KeyHandle));
    res = NtDeleteKey(hkey);
    NTPRINT("Got %d for deleting key\n", res);
    return NT_SUCCESS(res);
}

/* Enumerates the values of a registry key via the NtEnumerateValueKey
 * system call.
 *
 * Note that the caller must allocate memory at the end of
 * KEY_VALUE_FULL_INFORMATION to store the actual data.
 * WARNING: the Name field often has no null terminating it.  It
 * either runs right up next to Data or has an un-initialized value
 * in it -- so make sure you zero out your buffer before calling
 * this routine, and use the NameLength field (bytes not chars) and
 * then check for null and skip over it if nec. to find the data start.
 */
reg_query_value_result_t
reg_query_value(IN PCWSTR keyname, IN PCWSTR subkeyname,
                IN KEY_VALUE_INFORMATION_CLASS info_class, OUT PVOID info,
                IN ULONG info_size, IN ACCESS_MASK rights)
{
    int res;
    ULONG outlen = 0;
    UNICODE_STRING valuename;
    HANDLE hkey = reg_open_key(keyname, KEY_READ | rights);

    if (hkey == NULL)
        return REG_QUERY_FAILURE;

    res = wchar_to_unicode(&valuename, subkeyname);
    if (!NT_SUCCESS(res))
        return REG_QUERY_FAILURE;

    res = nt_query_value_key(hkey, &valuename, info_class, info, info_size, &outlen);
    reg_close_key(hkey);
#if VERBOSE
    if (!NT_SUCCESS(res))
        NTPRINT("Error 0x%x in query key \"%S\"\n", res, subkeyname);
#endif
    /* When buffer is insufficient I see it return BUFFER_OVERFLOW, but nebbet
     * mentions BUFFER_TOO_SMALL as well. */
    if (res == STATUS_BUFFER_TOO_SMALL || res == STATUS_BUFFER_OVERFLOW) {
        return REG_QUERY_BUFFER_TOO_SMALL;
    }
    return NT_SUCCESS(res) ? REG_QUERY_SUCCESS : REG_QUERY_FAILURE;
}

GET_RAW_SYSCALL(SetValueKey, IN HANDLE KeyHandle, IN PUNICODE_STRING ValueName,
                IN ULONG TitleIndex OPTIONAL, IN ULONG Type, IN PVOID Data,
                IN ULONG DataSize);

bool
reg_set_key_value(HANDLE hkey, PCWSTR subkey, PCWSTR val)
{
    UNICODE_STRING name;
    UNICODE_STRING value;
    NTSTATUS res;
    res = wchar_to_unicode(&name, subkey);
    if (!NT_SUCCESS(res))
        return NT_SUCCESS(res);
    res = wchar_to_unicode(&value, val);
    if (!NT_SUCCESS(res))
        return NT_SUCCESS(res);
    /* Length field is really size in bytes, have to add 1 for final 0 */
    res = NT_SYSCALL(SetValueKey, hkey, &name, 0, REG_SZ, (LPBYTE)value.Buffer,
                     value.Length + sizeof(wchar_t));
    return NT_SUCCESS(res);
}

bool
reg_set_dword_key_value(HANDLE hkey, PCWSTR subkey, DWORD value)
{
    UNICODE_STRING name;
    NTSTATUS res;
    res = wchar_to_unicode(&name, subkey);
    if (!NT_SUCCESS(res))
        return NT_SUCCESS(res);
    res = NT_SYSCALL(SetValueKey, hkey, &name, 0, REG_DWORD, &value, sizeof(DWORD));
    return NT_SUCCESS(res);
}

/* Flushes registry changes for the given key to the disk.
 * Returns 1 on success, 0 otherwise.
 * Notes:       See case 4138.  For a valid opened key, failure can happen
 *              only if registry IO fails, i.e., this function shouldn't fail
 *              for most cases.
 */
bool
reg_flush_key(HANDLE hkey)
{
    NTSTATUS res;
    GET_NTDLL(NtFlushKey, (IN HANDLE KeyHandle));
    res = NtFlushKey(hkey);
    return NT_SUCCESS(res);
}

/* Enumerates the subkeys of a registry key via the NtEnumerateKey
 * system call.
 *
 * Note that the caller must allocate memory at the end of
 * KEY_VALUE_FULL_INFORMATION to store the actual data.
 * WARNING: the Name field often has no null terminating it.  It
 * either runs right up next to Data or has an un-initialized value
 * in it -- so make sure you zero out your buffer before calling
 * this routine, and use the NameLength field (bytes not chars) and
 * then check for null and skip over it if nec. to find the data start.
 *
 * Returns 1 on success, 0 otherwise.
 */
bool
reg_enum_key(IN PCWSTR keyname, IN ULONG index, IN KEY_INFORMATION_CLASS info_class,
             OUT PVOID key_info, IN ULONG key_info_size)
{
    NTSTATUS result;
    ULONG received = 0;
    HANDLE hkey = reg_open_key(keyname, KEY_READ);

    GET_NTDLL(NtEnumerateKey,
              (IN HANDLE hkey, IN ULONG index, IN KEY_INFORMATION_CLASS info_class,
               OUT PVOID key_info, IN ULONG key_info_size, OUT PULONG bytes_received));

    if (hkey == NULL)
        return false;

    result = NtEnumerateKey(hkey, index, info_class, key_info, key_info_size, &received);
    reg_close_key(hkey);

    return NT_SUCCESS(result);
}

/* Enumerates the values of a registry key via the NtEnumerateValueKey
 * system call.
 *
 * Note that the caller must allocate memory at the end of
 * KEY_VALUE_FULL_INFORMATION to store the actual data.
 * WARNING: the Name field often has no null terminating it.  It
 * either runs right up next to Data or has an un-initialized value
 * in it -- so make sure you zero out your buffer before calling
 * this routine, and use the NameLength field (bytes not chars) and
 * then check for null and skip over it if nec. to find the data start.
 * Returns 1 on success, 0 otherwise.
 */
bool
reg_enum_value(IN PCWSTR keyname, IN ULONG index,
               IN KEY_VALUE_INFORMATION_CLASS info_class, OUT PVOID key_info,
               IN ULONG key_info_size)
{
    NTSTATUS result;
    ULONG bytes_received = 0;
    HANDLE hkey = reg_open_key(keyname, KEY_READ);

    GET_NTDLL(NtEnumerateValueKey,
              (IN HANDLE hKey, IN ULONG index, IN KEY_VALUE_INFORMATION_CLASS info_class,
               OUT PVOID key_info, IN ULONG key_info_size, OUT PULONG bytes_received));

    if (hkey == NULL)
        return false;

    result = NtEnumerateValueKey(hkey, index, info_class, key_info, key_info_size,
                                 &bytes_received);
    reg_close_key(hkey);

    return NT_SUCCESS(result);
}

/* queries the process env vars: NOT the separate copies used in the C
 * library and in other libraries
 */
bool
env_get_value(PCWSTR var, wchar_t *val, size_t valsz)
{
    PEB *peb = get_own_peb();
    PWSTR env = (PWSTR)get_process_param_buf(peb->ProcessParameters,
                                             peb->ProcessParameters->Environment);
    NTSTATUS res;
    UNICODE_STRING var_us, val_us;
    GET_NTDLL(RtlQueryEnvironmentVariable_U,
              (PWSTR Environment, PUNICODE_STRING Name, PUNICODE_STRING Value));
    res = wchar_to_unicode(&var_us, var);
    if (!NT_SUCCESS(res))
        return false;
    val_us.Length = 0;
    val_us.MaximumLength = (USHORT)valsz;
    val_us.Buffer = val;
    res = RtlQueryEnvironmentVariable_U(env, &var_us, &val_us);
    return NT_SUCCESS(res);
}

/* thread token can be primary token, impersonated, or anonymous */
NTSTATUS
get_current_user_token(PTOKEN_USER ptoken, USHORT token_buffer_length)
{
    NTSTATUS res;
    HANDLE htoken;
    ULONG len = 0;

    res = nt_raw_OpenThreadToken(NT_CURRENT_THREAD, TOKEN_QUERY, TRUE, &htoken);
    if (!NT_SUCCESS(res)) {
        /* anonymous impersonation token cannot be opened  */
        res = nt_raw_OpenProcessToken(NT_CURRENT_PROCESS, TOKEN_QUERY, &htoken);
        if (!NT_SUCCESS(res)) {
            return res;
        }
    }

    res = NtQueryInformationToken(htoken, TokenUser, ptoken, token_buffer_length, &len);
    close_handle(htoken);

    ASSERT(len <= token_buffer_length);
    if (!NT_SUCCESS(res)) {
        ASSERT_CURIOSITY(false && "can't query token, impersonated?");
    }
    return res;
}

NTSTATUS
get_primary_user_token(PTOKEN_USER ptoken, USHORT token_buffer_length)
{
    NTSTATUS res;
    HANDLE htoken;
    ULONG len = 0;

    res = nt_raw_OpenProcessToken(NT_CURRENT_PROCESS, TOKEN_QUERY, &htoken);
    if (!NT_SUCCESS(res)) {
        return res;
    }

    res = NtQueryInformationToken(htoken, TokenUser, ptoken, token_buffer_length, &len);
    close_handle(htoken);

    ASSERT(len <= token_buffer_length);
    if (!NT_SUCCESS(res)) {
        ASSERT_CURIOSITY(false && "can't query token?");
    }
    return res;
}

/* returns the Owner that will be recorded for any objects created by
 * this process (when not impersonating)
 */
NTSTATUS
get_primary_owner_token(PTOKEN_OWNER powner, USHORT owner_buffer_length)
{
    NTSTATUS res;
    HANDLE htoken;
    ULONG len = 0;

    res = nt_raw_OpenProcessToken(NT_CURRENT_PROCESS, TOKEN_QUERY, &htoken);
    if (!NT_SUCCESS(res)) {
        return res;
    }

    res = NtQueryInformationToken(htoken, TokenOwner, powner, owner_buffer_length, &len);
    close_handle(htoken);

    ASSERT(len <= owner_buffer_length);
    if (!NT_SUCCESS(res)) {
        ASSERT_CURIOSITY(false && "can't query token?");
    }
    return res;
}

/* Note that the caller must allocate buffer_length bytes in sid_string */
NTSTATUS
get_current_user_SID(PWSTR sid_string, USHORT buffer_length)
{
    GET_NTDLL(RtlConvertSidToUnicodeString,
              (OUT PUNICODE_STRING UnicodeString, IN PSID Sid,
               BOOLEAN AllocateDestinationString));
    NTSTATUS res;
    UNICODE_STRING ustr;
    UCHAR buf[SECURITY_MAX_TOKEN_SIZE];
    PTOKEN_USER ptoken = (PTOKEN_USER)buf;

    res = get_current_user_token(ptoken, sizeof(buf));
    if (!NT_SUCCESS(res)) {
        return res;
    }

    ustr.Length = 0;
    ustr.MaximumLength = buffer_length;
    ustr.Buffer = sid_string;

    /* We assume that by passing FALSE, no memory will be allocated
     * and the routine is reentrant.
     */
    res = RtlConvertSidToUnicodeString(&ustr, ptoken->User.Sid, FALSE);
    return res;
}

const PSID
get_process_primary_SID()
{
    static PSID primary_SID = NULL;
    static UCHAR buf[SECURITY_MAX_TOKEN_SIZE];

    if (primary_SID == NULL) {
        PTOKEN_USER ptoken = (PTOKEN_USER)buf;
        NTSTATUS res;
        res = get_primary_user_token(ptoken, sizeof(buf));
        ASSERT(NT_SUCCESS(res));

        if (!NT_SUCCESS(res)) {
            return NULL;
        }
        primary_SID = ptoken->User.Sid;
    }
    return primary_SID;
}

/* based on RtlpQuerySecurityDescriptorPointers from reactos/0.2.9/lib/rtl/sd.c */
static void
get_sd_pointers(IN PISECURITY_DESCRIPTOR SecurityDescriptor, OUT PSID *Owner OPTIONAL,
                OUT PSID *Group OPTIONAL, OUT PACL *Sacl OPTIONAL,
                OUT PACL *Dacl OPTIONAL)
{
    /* we usually deal with self-relative SIDs as returned by NtQuerySecurityObject */
    if (TEST(SE_SELF_RELATIVE, SecurityDescriptor->Control)) {
        PISECURITY_DESCRIPTOR_RELATIVE RelSD =
            (PISECURITY_DESCRIPTOR_RELATIVE)SecurityDescriptor;
        if (Owner != NULL) {
            *Owner =
                ((RelSD->Owner != 0) ? (PSID)((ULONG_PTR)RelSD + RelSD->Owner) : NULL);
        }
        if (Group != NULL) {
            *Group =
                ((RelSD->Group != 0) ? (PSID)((ULONG_PTR)RelSD + RelSD->Group) : NULL);
        }
        if (Sacl != NULL) {
            *Sacl = (((RelSD->Control & SE_SACL_PRESENT) && (RelSD->Sacl != 0))
                         ? (PSID)((ULONG_PTR)RelSD + RelSD->Sacl)
                         : NULL);
        }
        if (Dacl != NULL) {
            *Dacl = (((RelSD->Control & SE_DACL_PRESENT) && (RelSD->Dacl != 0))
                         ? (PSID)((ULONG_PTR)RelSD + RelSD->Dacl)
                         : NULL);
        }
    } else {
        if (Owner != NULL) {
            *Owner = SecurityDescriptor->Owner;
        }
        if (Group != NULL) {
            *Group = SecurityDescriptor->Group;
        }
        if (Sacl != NULL) {
            *Sacl = ((SecurityDescriptor->Control & SE_SACL_PRESENT)
                         ? SecurityDescriptor->Sacl
                         : NULL);
        }
        if (Dacl != NULL) {
            *Dacl = ((SecurityDescriptor->Control & SE_DACL_PRESENT)
                         ? SecurityDescriptor->Dacl
                         : NULL);
        }
    }
}

bool
get_owner_sd(PISECURITY_DESCRIPTOR SecurityDescriptor, OUT PSID *Owner)
{
    /* RtlGetOwnerSecurityDescriptor is clean enough, so could be used
     * without reentrancy risks instead of writing ours here
     */

    if (SecurityDescriptor->Revision != SECURITY_DESCRIPTOR_REVISION1) {
        return false;
    }

    get_sd_pointers(SecurityDescriptor, Owner, NULL, NULL, NULL);
    return true;
}

void
initialize_security_descriptor(PISECURITY_DESCRIPTOR SecurityDescriptor)
{
    SecurityDescriptor->Revision = SECURITY_DESCRIPTOR_REVISION1;
    SecurityDescriptor->Sbz1 = 0;
    /* note using absolute format, not SE_SELF_RELATIVE */
    SecurityDescriptor->Control = 0;
    SecurityDescriptor->Owner = NULL;
    SecurityDescriptor->Group = NULL;
    SecurityDescriptor->Sacl = NULL;
    SecurityDescriptor->Dacl = NULL;
}

/* use only on security descriptors created with initialize_security_descriptor() */
bool
set_owner_sd(PISECURITY_DESCRIPTOR SecurityDescriptor, PSID Owner)
{
    /* RtlGetOwnerSecurityDescriptor is clean enough, so could be used
     * without reentrancy risks instead of writing ours here
     */

    if (SecurityDescriptor->Revision != SECURITY_DESCRIPTOR_REVISION1) {
        return false;
    }
    if (TEST(SE_SELF_RELATIVE, SecurityDescriptor->Control)) {
        ASSERT(false && "we only create absolute security descriptors");
        return false;
    }

    ASSERT(ALIGNED(SecurityDescriptor->Owner, sizeof(void *)));
    SecurityDescriptor->Owner = Owner;

    /* In case we are editing an existing SD makes remove possible tag
     * that Owner field was provided with default or inheritance
     * mechanisms..  Otherwise practically a nop for us when building
     * an SD from scratch
     */
    SecurityDescriptor->Control &= ~SE_OWNER_DEFAULTED;

    return true;
}

static int
length_sid(IN PSID Sid_)
{
    PISID Sid = Sid_;
    /* we only know about usable length of SID */
    return LengthRequiredSID(Sid->SubAuthorityCount);
}

bool
equal_sid(IN PSID Sid1_, IN PSID Sid2_)
{
    PISID Sid1 = Sid1_;
    PISID Sid2 = Sid2_;

    /* note ntdll!RtlEqualSid returns BOOLEAN and so its result is
     * just in AL!  I don't want to deal with here after it got me
     * once when assuming regular bool=int.
     *
     * ntdll!RtlEqualSid+0x2e:
     * 7c91a493 32c0             xor     al,al
     * ...
     * 7c91a498 c20800           ret     0x8
     */
    /* preferred to reimplement based on reactos/0.2.x/lib/rtl/sid.c*/
    SIZE_T SidLen;

    if (Sid1->Revision != Sid2->Revision ||
        Sid1->SubAuthorityCount != Sid2->SubAuthorityCount) {
        return false;
    }

    SidLen = length_sid(Sid1);
    return memcmp(Sid1, Sid2, SidLen) == 0;
}

#ifndef NOT_DYNAMORIO_CORE
/* To avoid any possible races, we ensure tbat the static buffers are
 * initialized before we become multi-threaded via
 * os_init->init_debugbox_title_buf() which calls these routines */

/* get application name, (cached), used for options, event logging and
 * following children */
char *
get_application_name()
{
    static char exename[MAXIMUM_PATH];
    if (!exename[0]) {
        snprintf(exename, BUFFER_SIZE_ELEMENTS(exename), "%ls", get_own_qualified_name());
        NULL_TERMINATE_BUFFER(exename);
    }
    return exename;
}

const char *
get_application_short_name()
{
    static char short_exename[MAXIMUM_PATH];
    if (!short_exename[0]) {
        snprintf(short_exename, BUFFER_SIZE_ELEMENTS(short_exename), "%ls",
                 get_own_short_qualified_name());
        NULL_TERMINATE_BUFFER(short_exename);
    }
    return short_exename;
}

const char *
get_application_short_unqualified_name()
{
    static char short_unqual_exename[MAXIMUM_PATH];
    if (!short_unqual_exename[0]) {
        snprintf(short_unqual_exename, BUFFER_SIZE_ELEMENTS(short_unqual_exename), "%ls",
                 get_own_short_unqualified_name());
        NULL_TERMINATE_BUFFER(short_unqual_exename);
    }
    return short_unqual_exename;
}

/* get application pid, (cached), used for event logging */
char *
get_application_pid()
{
    static char pidstr[16];

    if (!pidstr[0]) {
        process_id_t pid = get_process_id();
        snprintf(pidstr, BUFFER_SIZE_ELEMENTS(pidstr), PIDFMT, pid);
        NULL_TERMINATE_BUFFER(pidstr);
    }
    return pidstr;
}
#endif /* NOT_DYNAMORIO_CORE */

wchar_t *
get_process_param_buf(RTL_USER_PROCESS_PARAMETERS *params, wchar_t *buf)
{
#if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)
    /* Many of the UNICODE_STRING.Buffer fields contain a relative offset
     * from the start of ProcessParameters as set by the parent process,
     * until the child's init updates it, on pre-Vista.
     * Xref the adjustments done inside the routines here that read
     * a child's params.
     */
    if (dr_earliest_injected && get_os_version() < WINDOWS_VERSION_VISTA &&
        /* sanity check: some may be real ptrs, such as Environment which
         * we replaced from parent.  the offsets should all be small, laid
         * out after the param struct.
         */
        (ptr_uint_t)buf < 64 * 1024) {
        return (wchar_t *)((ptr_uint_t)buf + (ptr_uint_t)params);
    } else
        return buf;
#else
    /* Shouldn't need this routine since shouldn't be reading own params, but
     * rather than ifdef-ing out all callers we just make it work
     */
    return buf;
#endif
}

wchar_t *
get_application_cmdline(void)
{
    PEB *peb = get_own_peb();
    return get_process_param_buf(peb->ProcessParameters,
                                 peb->ProcessParameters->CommandLine.Buffer);
}

LONGLONG
query_time_100ns()
{
    /* FIXME: we could use KUSER_SHARED_DATA here, but it's too volatile
     * since we can't programmatically grab its address (all we know is
     * 0x7ffe0000) and it changed on win2003 (tickcount deprecated, e.g.).
     * Since these time routines aren't currently on critical path we just
     * use the more-stable syscalls.
     */
    LARGE_INTEGER systime;
    GET_NTDLL(NtQuerySystemTime, (IN PLARGE_INTEGER SystemTime));
    NtQuerySystemTime(&systime);
    return systime.QuadPart;
}

uint64
query_time_micros()
{
    LONGLONG time100ns = query_time_100ns();
    return ((uint64)time100ns / TIMER_UNITS_PER_MICROSECOND);
}

uint64
query_time_millis()
{
    LONGLONG time100ns = query_time_100ns();
    return ((uint64)time100ns / TIMER_UNITS_PER_MILLISECOND);
}

uint
query_time_seconds()
{
    /* ntdll provides RtlTimeToSecondsSince1970 but we've standardized on
     * UTC so we just divide ourselves
     */
    uint64 ms = query_time_millis();
    return (uint)(ms / 1000);
}

/* uses convert_millis_to_date() in utils.c so core-only for simpler linking */
#if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)
/* note that ntdll!RtlTimeToTimeFields has this same functionality */
void
convert_100ns_to_system_time(uint64 time_in_100ns, SYSTEMTIME *st OUT)
{
    LONGLONG time = time_in_100ns / TIMER_UNITS_PER_MILLISECOND;
    dr_time_t dr_time;
    convert_millis_to_date((uint64)time, &dr_time);
    st->wYear = (WORD)dr_time.year;
    st->wMonth = (WORD)dr_time.month;
    st->wDayOfWeek = (WORD)dr_time.day_of_week;
    st->wDay = (WORD)dr_time.day;
    st->wHour = (WORD)dr_time.hour;
    st->wMinute = (WORD)dr_time.minute;
    st->wSecond = (WORD)dr_time.second;
    st->wMilliseconds = (WORD)dr_time.milliseconds;
}

void
convert_system_time_to_100ns(const SYSTEMTIME *st, uint64 *time_in_100ns OUT)
{
    uint64 time;
    dr_time_t dr_time;
    dr_time.year = st->wYear;
    dr_time.month = st->wMonth;
    dr_time.day_of_week = st->wDayOfWeek;
    dr_time.day = st->wDay;
    dr_time.hour = st->wHour;
    dr_time.minute = st->wMinute;
    dr_time.second = st->wSecond;
    dr_time.milliseconds = st->wMilliseconds;
    convert_date_to_millis(&dr_time, &time);
    *time_in_100ns = time * TIMER_UNITS_PER_MILLISECOND;
}

void
query_system_time(SYSTEMTIME *st OUT)
{
    convert_100ns_to_system_time(query_time_100ns(), st);
}
#endif

/* returns NULL (default security descriptor) if can't setup owner,
 * otherwise edits in place the passed in security descriptor
 */
static PSECURITY_DESCRIPTOR
set_primary_user_owner(PSECURITY_DESCRIPTOR psd)
{
    bool ok;
    initialize_security_descriptor(psd);

    /* for consistency, we override the NoDefaultAdminOwner feature
     * which creates files with owner Administrators instead of current user
     */
    /* note we could also just return a NULL SD, if TokenOwner == TokenUser
     * and create an explicit one only if we really need it
     */
    ok = set_owner_sd(psd, get_process_primary_SID());
    ASSERT(ok);
    if (!ok)
        return NULL;
    /* FIXME: (not verified) note that even if we set owner, we may
     * not be allowed to use it as an owner it it is not present in
     * the current token.
     */

    /* we rely on the correct DACL to be provided through inheritance */
    /* FIXME: we don't specify primary Group, we may end up with no primary group,
     * which should be OK too */
    return psd; /* use the constructed security descriptor */
}

/* Exposes full power of NtCreateFile.  Caller should first consider
 * create_file() or nt_create_module_file() before calling this
 * routine directly.  See comments above nt_create_module_file() for
 * more details on some of these arguments.
 *
 * Note that instead of asking for raw OBJECT_ATTRIBUTES we have
 * enriched the NT interface with a directory handle and an additional
 * disposition FILE_DISPOSITION_SET_OWNER.
 */
NTSTATUS
nt_create_file(OUT HANDLE *file_handle, const wchar_t *filename,
               HANDLE dir_handle OPTIONAL, size_t alloc_size, ACCESS_MASK rights,
               uint attributes, uint sharing, uint create_disposition,
               uint create_options)
{
    NTSTATUS res;
    OBJECT_ATTRIBUTES file_attributes;
    IO_STATUS_BLOCK iob = { 0, 0 };
    UNICODE_STRING file_path_unicode;
    SECURITY_DESCRIPTOR sd;
    PSECURITY_DESCRIPTOR pSD = NULL; /* default security descriptor */
    LARGE_INTEGER create_allocation_size;
    create_allocation_size.QuadPart = alloc_size;

    res = wchar_to_unicode(&file_path_unicode, filename);
    if (!NT_SUCCESS(res)) {
        NTPRINT("nt_create_file: name conversion failed, res: %x\n", res);
        return res;
    }

    if (TEST(FILE_DISPOSITION_SET_OWNER, create_disposition)) {
        pSD = set_primary_user_owner(&sd);
        create_disposition &= ~FILE_DISPOSITION_SET_OWNER;
    }

    InitializeObjectAttributes(&file_attributes, &file_path_unicode, OBJ_CASE_INSENSITIVE,
                               dir_handle, pSD);
    ASSERT(create_disposition <= FILE_MAXIMUM_DISPOSITION);
    res = NT_SYSCALL(CreateFile, file_handle, rights, &file_attributes, &iob,
                     (alloc_size == 0 ? NULL : &create_allocation_size), attributes,
                     sharing, create_disposition, create_options, NULL, 0);
    if (!NT_SUCCESS(res)) {
        NTPRINT("Error 0x%x in nt_create_file %S \"%S\"\n", res, filename,
                file_path_unicode.Buffer);
    }
    return res;
}

/* For ordinary use of NtCreateFile */
/* FIXME : can't simultaneously have GENERIC_READ, GENERIC_WRITE and SYNCH_IO
 * get invalid parameter error, but any two succeeds, makes sense because
 * <speculation> SYNCH_IO tells the io system to keep track of the
 * current file postion which should start at 0 for read and end of file for
 * write </speculation>, could do non_synch_io but had trouble getting that to
 * work with read/write */
HANDLE
create_file(PCWSTR filename, bool is_dir, ACCESS_MASK rights, uint sharing,
            uint create_disposition, bool synch)
{
    NTSTATUS res;
    HANDLE hfile;
    DEBUG_DECLARE(static const uint access_allow = READ_CONTROL | GENERIC_READ |
                      GENERIC_WRITE | GENERIC_EXECUTE | FILE_GENERIC_READ |
                      FILE_GENERIC_WRITE | FILE_GENERIC_EXECUTE;)
    DEBUG_DECLARE(static const uint dir_access_allow = READ_CONTROL | 0;)

    /* FIXME : only support these possibilities for access mask for now
     * should be all we need unless we decide to export more functionality
     * from os_open/write/read */
    ASSERT((synch &&
            ((!is_dir && ((rights & (~access_allow)) == 0)) ||
             (is_dir && ((rights & (~dir_access_allow)) == 0)))) ||
           (!synch && !is_dir && (GENERIC_READ | GENERIC_WRITE) == rights));

    res = nt_create_file(&hfile, filename, NULL, 0,
                         rights | SYNCHRONIZE |
                             (is_dir ? FILE_LIST_DIRECTORY : FILE_READ_ATTRIBUTES),
                         /* CreateDirectory uses F_ATTRIB_NORM too, even though
                          * there is a F_ATTRIB_DIR as well */
                         FILE_ATTRIBUTE_NORMAL, sharing, create_disposition,
                         (synch ? FILE_SYNCHRONOUS_IO_NONALERT : 0) |
                             /* FIXME: MSDN instructs to use FILE_FLAG_BACKUP_SEMANTICS
                              * for opening a dir handle but we don't seem to need it */
                             (is_dir ? FILE_DIRECTORY_FILE | FILE_OPEN_FOR_BACKUP_INTENT
                                     : FILE_NON_DIRECTORY_FILE));
    if (!NT_SUCCESS(res))
        return INVALID_FILE;
    else
        return hfile;
}

#if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)
NTSTATUS
nt_open_file(HANDLE *handle OUT, PCWSTR filename, ACCESS_MASK rights, uint sharing,
             uint options)
{
    NTSTATUS res;
    OBJECT_ATTRIBUTES oa;
    IO_STATUS_BLOCK iob = { 0, 0 };
    UNICODE_STRING us;

    res = wchar_to_unicode(&us, filename);
    if (!NT_SUCCESS(res))
        return res;

    InitializeObjectAttributes(&oa, &us, OBJ_CASE_INSENSITIVE, NULL, NULL);
    res = nt_raw_OpenFile(handle, rights | SYNCHRONIZE, &oa, &iob, sharing,
                          FILE_SYNCHRONOUS_IO_NONALERT | options);
    return res;
}
#endif

NTSTATUS
nt_delete_file(PCWSTR nt_filename)
{
    /* We follow the lead of Win32 and use FileDispositionInformation
     * and not NtDeleteFile.
     */
    /* Xref os_delete_mapped_file() which does more: but here we want
     * to match something more like Win32 DeleteFile().
     */
    NTSTATUS res;
    HANDLE hf;
    FILE_DISPOSITION_INFORMATION file_dispose_info;

    res = nt_create_file(&hf, nt_filename, NULL, 0, SYNCHRONIZE | DELETE,
                         FILE_ATTRIBUTE_NORMAL,
                         FILE_SHARE_DELETE | /* if already deleted */
                             FILE_SHARE_READ,
                         FILE_OPEN,
                         FILE_SYNCHRONOUS_IO_NONALERT |
                             FILE_DELETE_ON_CLOSE
                             /* This should open a handle on a symlink rather
                              * than its target, and avoid other reparse code.
                              * Otherwise the FILE_DELETE_ON_CLOSE would cause
                              * us to delete the target of a symlink!
                              * FIXME: fully test this: case 10067
                              */
                             | FILE_OPEN_REPARSE_POINT);
    if (!NT_SUCCESS(res))
        return res;

    file_dispose_info.DeleteFile = TRUE;
    res = nt_set_file_info(hf, &file_dispose_info, sizeof(file_dispose_info),
                           FileDispositionInformation);
    /* close regardless of success */
    close_handle(hf);
    return res;
}

NTSTATUS
nt_flush_file_buffers(HANDLE file_handle)
{
    IO_STATUS_BLOCK ret;

    GET_NTDLL(NtFlushBuffersFile,
              (IN HANDLE FileHandle, OUT PIO_STATUS_BLOCK IoStatusBlock));

    return NtFlushBuffersFile(file_handle, &ret);
}

bool
read_file(HANDLE file_handle, void *buffer, uint num_bytes_to_read,
          IN uint64 *file_byte_offset OPTIONAL, OUT size_t *num_bytes_read)
{
    NTSTATUS res;
    IO_STATUS_BLOCK ret = { 0 };
    LARGE_INTEGER ByteOffset; /* should be the same as uint64 */

    if (file_byte_offset != NULL) {
        ByteOffset.QuadPart = *file_byte_offset;
    }

    /* if file is opened with FILE_SYNCHRONOUS_IO_[NON]ALERT then can pass
     * NULL for ByteOffset to read from current file positions, otherwise
     * need to pass special value to ByteOffset to read from current position
     * (special value is highpart -1, low part FILE_USE_FILE_POINTER_POSITION),
     *  but I can't get this to work so assuming opened with SYNCH_IO */
    res = NtReadFile(file_handle, NULL, NULL, NULL, &ret, buffer, num_bytes_to_read,
                     file_byte_offset != NULL ? &ByteOffset : NULL, NULL);

    *num_bytes_read = ret.Information;
    return NT_SUCCESS(res);
}

bool
write_file(HANDLE file_handle, const void *buffer, uint num_bytes_to_write,
           OPTIONAL uint64 *file_byte_offset, OUT size_t *num_bytes_written)
{
    NTSTATUS res;
    IO_STATUS_BLOCK ret = { 0 };
    LARGE_INTEGER ByteOffset;

    if (file_byte_offset != NULL) {
        ByteOffset.QuadPart = *file_byte_offset;
    }
    /* if file is opened with FILE_SYNCHRONOUS_IO_[NON]ALERT then can pass
     * NULL for ByteOffset to append to end (well append to after the last
     * write or end if file just opened), otherwise need to pass special value
     * to ByteOffset to append, unless opened with just append permissions in
     * which case always appends (special value is highpart -1, low part
     * FILE_WRITE_TO_END_OF_FILE for middle case), but I can't get this to
     * work so assuming opened with FILE_SYNCHRONOUS_IO_* */
    res = NtWriteFile(file_handle, NULL, NULL, NULL, &ret, buffer, num_bytes_to_write,
                      file_byte_offset != NULL ? &ByteOffset : NULL, NULL);

    *num_bytes_written = ret.Information;
    return NT_SUCCESS(res);
}

bool
close_file(HANDLE hfile)
{
    return close_handle(hfile);
}

HANDLE
create_iocompletion()
{
    NTSTATUS res;
    HANDLE hiocompletion;

    GET_NTDLL(NtCreateIoCompletion,
              (OUT PHANDLE IoCompletionHandle, IN ACCESS_MASK DesiredAccess,
               IN POBJECT_ATTRIBUTES ObjectAttributes,
               IN ULONG NumberOfConcurrentThreads));

    res = NtCreateIoCompletion(&hiocompletion, EVENT_ALL_ACCESS /* 0x1f0003 */,
                               NULL /* no name */,
                               0 /* FIXME: 0 observed, shouldn't it be 1? */);

    if (!NT_SUCCESS(res)) {
        NTPRINT("Error 0x%x in create IoCompletion \n", res);
        return NULL;
    } else
        return hiocompletion;
}

typedef struct _FILE_PIPE_INFORMATION { // Information Class 23
    ULONG ReadModeMessage;
    ULONG WaitModeBlocking;
} FILE_PIPE_INFORMATION, *PFILE_PIPE_INFORMATION;

typedef struct _FILE_COMPLETION_INFORMATION { // Information Class 30
    HANDLE IoCompletionHandle;
    ULONG CompletionKey;
} FILE_COMPLETION_INFORMATION, *PFILE_COMPLETION_INFORMATION;

/* Takes a pipename, and an optional IoCompletion object */
HANDLE
open_pipe(PCWSTR pipename, HANDLE hsync)
{
    NTSTATUS res;
    HANDLE h;
    IO_STATUS_BLOCK iob;
    FILE_PIPE_INFORMATION pipeinfo = { 1 /* message */, 0 /* no wait*/ };
    /* setting this to wait doesn't work */

    // CHECK: object attributes we see in RegisterEventSource // 1242580, "name"
    h = create_file(pipename, false, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ,
                    FILE_OPEN, false);
    if (h == INVALID_FILE)
        return NULL;
    /* FIXME: call nt_set_file_info */
    res = NT_SYSCALL(SetInformationFile, h, &iob, &pipeinfo, sizeof(pipeinfo),
                     FilePipeInformation);
    if (!NT_SUCCESS(res)) {
        /* FIXME: get __FUNCTION__ working for windows */
        NTPRINT("Error 0x%x in %s:%d\n", res, __FILE__, __LINE__);
        return NULL;
    }

    /* CHECK: How should synchronization work here; all I want is blocking I/O
       then we'd skip this step, (yet we fail with FILE_SYNCHRONOUS_IO_NONALERT) */

    /* set FileCompletionInformation just like RegisterSource does
       FIXME: The problem is that the IoCompletion that is used here is not created
       by RegisterSource and instead an earlier one is used.

       FIXME: There is a NtCreateEvent call, but that is what should go in
       NtFsControlFile calls, and I can't match how that handle gets used either.
    */
    if (hsync) {
        FILE_COMPLETION_INFORMATION completioninfo;
        completioninfo.IoCompletionHandle = hsync;
        completioninfo.CompletionKey = 0xffff0000; /* observed key */

        /* FIXME: call nt_set_file_info */
        res = NT_SYSCALL(SetInformationFile, h, &iob, &completioninfo,
                         sizeof(completioninfo), FileCompletionInformation);
        if (!NT_SUCCESS(res)) {
            /* FIXME: get __FUNCTION__ working for windows */
            NTPRINT("Error 0x%x in %s:%d\n", res, __FILE__, __LINE__);
            return NULL;
        }
    }

    return h;
}

/* see example in Nebbett p. 419 */
/* status codes ntstatus.h (arg 1), also includes descriptions of expected
 * arguments which must match the actual arguments described by args 2-4
 * (which are just for packaging), (i.e the status code defines the format
 * string), note that many status codes won't produce message box
 * arg 2 is number of substitutions,
 * arg 3 is a mask of what substitutions are pointers (i.e. strings), i.e. if
 * substitutions 2 and 4 are strings then the 2nd and 4th bits will be set and
 * will get 1010 or 10
 * arg 4 is an array of ULONGS comprimising the substitutions (ULONG will be
 * interpreted as pointer as defined by arg 3)
 * arg5 is response options, eqv. of MB_OK, MB_YESNO etc, see Nebbet enum 418
 * (1 is OK)
 * arg 6 is return value from box, see Nebbet enum 418 */

/* bad news is that the following is somewhat brittle, the format changed
 * between win2k and XP substantially (though in a forward, but not backwards
 * compatible way).  The following uses the format for XP since that works for
 * win2k too, but the reverse is not true.  On both platforms
 * ServiceMessageBox (what we use, probably because of service notification
 * flag?) uses the undocumented status code 0x50000018L.  On win2k a three
 * element array with first being the msg string, the second being the title
 * and the third element being 0x10 (which seems to be ignored) is used and
 * the MsgBoxType arg specifies the msg box and buttons shown.  On XP a four
 * element array with the first element being the msg string,
 * the second being the title, the third being 0x10 (which controls the msg
 * box and buttons shown), and the fourth being 0xffffffffL (seems to be
 * ignored) is used and the MsgBoxType arg seems to be ignored.  Also
 * having the wrong arguments for RaiseHardError can leave the machine in a
 * bad state.  The offending thread will hang at the syscall and (if for ex.
 * you use the 2k form on XP) the machine can be left in a state where it is
 * unable to display any message box from any process (any thread that tries
 * just hangs).  At one point had to power cycle the machine since couldn't
 * shut down or get task manager to appear.  But following seems to work.
 */

#define STATUS_SHOW_MESSAGEBOX_UNDOCUMENTED (NTSTATUS)0x50000018L

bool
nt_messagebox(const wchar_t *msg, const wchar_t *title)
{
    UNICODE_STRING m, t;
    NTSTATUS res;
    GET_NTDLL(NtRaiseHardError,
              (IN NTSTATUS ErrorStatus, IN ULONG NumberOfArguments,
               /* FIXME: ReactOS claims this is a PUNICODE_STRING */
               IN ULONG UnicodeStringArgumentsMask, IN PVOID Arguments,
               IN ULONG MessageBoxType, /* HARDERROR_RESPONSE_OPTION */
               OUT PULONG MessageBoxResult));

    /* the 0xfff... is only for XP, win2k has three element args array, its
     * function is unknown (doesn't seem to matter what is there) */
    /* the 0x10 argument is ignored ? on win2k, on XP chooses the icon and
     * response options of the resulting dialog box */
    ULONG ret;
    void *args[] = { NULL, NULL, (void *)(ptr_uint_t)0x10L, (void *)PTR_UINT_MINUS_1 };

    /* make UNICODE_STRINGs */
    res = wchar_to_unicode(&m, msg);
    ASSERT(NT_SUCCESS(res));
    if (!NT_SUCCESS(res))
        return NT_SUCCESS(res);
    res = wchar_to_unicode(&t, title);
    ASSERT(NT_SUCCESS(res));
    if (!NT_SUCCESS(res))
        return NT_SUCCESS(res);

    args[0] = (void *)&m;
    args[1] = (void *)&t;

    /* see notes above */
    /* 4 = length of args, set to 3 to match native behavior on win2k */
    /* 1 is OptionOK dialog for win2k, but is ignored ? on XP */
    res = NtRaiseHardError(STATUS_SHOW_MESSAGEBOX_UNDOCUMENTED, 4, 0x1 | 0x2, args, 1,
                           &ret);

    return NT_SUCCESS(res);
}

bool
nt_raise_exception(EXCEPTION_RECORD *pexcrec, CONTEXT *pcontext)
{
    NTSTATUS res;
    GET_NTDLL(NtRaiseException,
              (IN PEXCEPTION_RECORD ExceptionRecord, IN PCONTEXT Context,
               IN BOOLEAN SearchFrames));

    res = NtRaiseException(pexcrec, pcontext, true);

    /* we just threw an exception, shouldn't get here */
    ASSERT_NOT_REACHED();

    return NT_SUCCESS(res);
}

HANDLE
nt_create_event(EVENT_TYPE event_type)
{
    NTSTATUS res;
    HANDLE hevent;

    GET_NTDLL(NtCreateEvent,
              (OUT PHANDLE EventHandle, IN ACCESS_MASK DesiredAccess,
               IN POBJECT_ATTRIBUTES ObjectAttributes, IN EVENT_TYPE EventType,
               IN BOOLEAN InitialState));

    res = NtCreateEvent(&hevent, EVENT_ALL_ACCESS, NULL /* no name */, event_type,
                        0 /* start non-signaled */);

    if (!NT_SUCCESS(res)) {
        NTPRINT("Error 0x%x in create event \n", res);
        return NULL;
    } else
        return hevent;
}

void
nt_close_event(HANDLE hevent)
{
    close_handle(hevent);
}

wait_status_t
nt_wait_event_with_timeout(HANDLE hevent, PLARGE_INTEGER timeout)
{
    NTSTATUS res;
    /* i#4075: We use a raw syscall to keep the PC in dynamorio.dll for
     * os_take_over_all_unknown_threads() and synch_with_* routines to more
     * easily identify a thread in DR code.  In particular this is required to
     * avoid a double takeover on a race between intercept_new_thread() and
     * os_take_over_all_unknown_threads().
     */
    GET_RAW_SYSCALL(WaitForSingleObject, IN HANDLE ObjectHandle, IN BOOLEAN Alertable,
                    IN PLARGE_INTEGER TimeOut);
    res = NT_SYSCALL(WaitForSingleObject, hevent, false /* not alertable */, timeout);
    if (!NT_SUCCESS(res))
        return WAIT_ERROR;
    if (res == STATUS_TIMEOUT)
        return WAIT_TIMEDOUT;
    return WAIT_SIGNALED;
}

void
nt_set_event(HANDLE hevent)
{
    GET_NTDLL(NtSetEvent, (IN HANDLE EventHandle, OUT PLONG PreviousState OPTIONAL));
    NTSTATUS res;

    res = NtSetEvent(hevent, NULL /* no previous */);
    /* on WinXP critical sections use ZwSetEventBoostPriority, yet
       Inside Win2k p.362 claims we always get a boost on Win2000 */

#if VERBOSE
    if (!NT_SUCCESS(res)) {
        NTPRINT("Error 0x%x in set event \n", res);
    }
#endif
}

/* This is currently used only for manual broadcast events
 * It looks like NtPulseEvent will not be a good idea.
 * MSDN says that PulseEvent is bad because of kernel APCs
 * taking a thread out of the wait queue.  If it was only user APCs
 * we wouldn't have to worry about it.  However, MSDN should have said that
 * non-alertable waits will not be affected, instead they say don't use it.
 * Therefore we are stuck with manual event handling.
 */
void
nt_clear_event(HANDLE hevent)
{
    GET_NTDLL(NtClearEvent, (IN HANDLE EventHandle));
    NTSTATUS res;

    res = NtClearEvent(hevent);
#if VERBOSE
    if (!NT_SUCCESS(res)) {
        NTPRINT("Error 0x%x in clear event \n", res);
    }
#endif
}

void
nt_signal_and_wait(HANDLE hevent_to_signal, HANDLE hevent_to_wait)
{
    GET_NTDLL(NtSignalAndWaitForSingleObject,
              (IN HANDLE ObjectToSignal, IN HANDLE WaitableObject, IN BOOLEAN Alertable,
               IN PLARGE_INTEGER Time OPTIONAL));
    NTSTATUS res;

    res =
        NtSignalAndWaitForSingleObject(hevent_to_signal, hevent_to_wait,
                                       false /* not alertable */, NULL /* no timeout */);
#if VERBOSE
    if (!NT_SUCCESS(res)) {
        NTPRINT("Error 0x%x in set event \n", res);
    }
#endif
}

void
nt_query_performance_counter(PLARGE_INTEGER counter, PLARGE_INTEGER frequency)
{
    GET_NTDLL(NtQueryPerformanceCounter,
              (OUT PLARGE_INTEGER PerformanceCount,
               OUT PLARGE_INTEGER PerformanceFrequency OPTIONAL));
    NTSTATUS res;
    res = NtQueryPerformanceCounter(counter, frequency);
#if VERBOSE
    if (!NT_SUCCESS(res)) {
        NTPRINT("Error 0x%x in query_performance_counter  \n", res);
    }
#endif
    ASSERT(NT_SUCCESS(res));
}

/* Pipe transceive macros and types */

/* macros from WinIoCtl.h */
#define CTL_CODE(DeviceType, Function, Method, Access) \
    (((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))

// Define the method codes for how buffers are passed for I/O and FS controls
#define METHOD_BUFFERED 0
#define METHOD_IN_DIRECT 1
#define METHOD_OUT_DIRECT 2
#define METHOD_NEITHER 3

// Define the access check value for any access
#define FILE_READ_ACCESS (0x0001)  // file & pipe
#define FILE_WRITE_ACCESS (0x0002) // file & pipe

#define FSCTL_PIPE_TRANSCEIVE                           \
    CTL_CODE(FILE_DEVICE_NAMED_PIPE, 5, METHOD_NEITHER, \
             FILE_READ_DATA | FILE_WRITE_DATA) /* 0x11c017 */

#ifdef DEBUG
#    if defined(NOT_DYNAMORIO_CORE_PROPER) || defined(NOT_DYNAMORIO_CORE)
static bool do_once_nt_pipe_transceive = false;
#    else
DECLARE_FREQPROT_VAR(static bool do_once_nt_pipe_transceive, false);
#    endif
#endif

/* read/write from a pipe. Returns length of output if successful
 * 0 on failure */
size_t
nt_pipe_transceive(HANDLE hpipe, void *input, uint input_size, void *output,
                   uint output_size, uint timeout_ms)
{
    LARGE_INTEGER liDueTime;
    NTSTATUS res;
    IO_STATUS_BLOCK iob = { 0, 0 };

    /* NOTE use an event => asynch IO, if event caller will be notified
     * that routine finishes by signaling the event */

    /* FIXME shared utility for this style of computation,
     * is used in several places in os.c */
    liDueTime.QuadPart = -((int)timeout_ms * TIMER_UNITS_PER_MILLISECOND);

    ASSERT(hpipe);
    res = NtFsControlFile(hpipe, NULL, NULL, NULL, &iob, FSCTL_PIPE_TRANSCEIVE, input,
                          input_size, output, output_size);

    /* make sure that I/O is complete before we go back to the client
       - otherwise we may corrupt the stack if output is a stack allocated buffer */

    if (NT_SUCCESS(res)) {
        /* it worked, check if we need to wait for the IO to finish */
        if (res == STATUS_PENDING) {
            /* we need to wait on the pipe handle */
            /* ref case 666, sometimes when we are in services.exe this wait
             * can hang (presumably this thread is needed on the other side
             * of the pipe or something like that) so we timeout the wait */
            /* name is a bit misleading (pipe vs event), but does the
             * right thing */
            res = nt_wait_event_with_timeout(hpipe, &liDueTime);
            if (res == WAIT_TIMEDOUT) {
                /* CancelIO is wrapper for NtCancelIO, msdn claims only works
                 * on asynch IO, but as they point out it shouldn't be possible
                 * to use it on synch IO (the routine shouldn't return for you
                 * to cancel).  Still not sure why the Fs... returns when synch
                 * IO.  Try to cancel, we may have to eventually go to quasi
                 * asynch IO though this appears to work. */
                /* pipe == file */
                GET_NTDLL(NtCancelIoFile,
                          (IN HANDLE FileHandle, OUT PIO_STATUS_BLOCK IoStatusBlock));
                IO_STATUS_BLOCK cancel_iob;
                NTLOG(GLOBAL, LOG_NT, 1, "pipe transceive timed out\n");
                NTLOG(THREAD_GET, LOG_NT, 1, "pipe transceive timed out\n");
                res = NtCancelIoFile(hpipe, &cancel_iob);
                if (!NT_SUCCESS(res)) {
                    /* check, maybe finished before we could cancel (if so
                     * our timeout was too short) */
                    NTLOG(GLOBAL, LOG_NT, 1,
                          "pipe transceive cancel failed code=" PFX "\n", res);
                    NTLOG(THREAD_GET, LOG_NT, 1,
                          "pipe transceive cancel failed code=" PFX "\n", res);
                    res = nt_wait_event_with_timeout(hpipe, &liDueTime);
                    if (res == WAIT_TIMEDOUT) {
                        /* Now we are in a world of hurt, just return and hope
                         * for the best */
                        NTLOG(GLOBAL, LOG_NT, 1, "pipe transceive 2nd try FAILED!\n");
                        NTLOG(THREAD_GET, LOG_NT, 1, "pipe transceive 2nd try FAILED!\n");
                        /* DO_ONCE to avoid an infinite recursion here */
                        DOCHECK(1, {
                            /* custom DO_ONCE to avoid selfprot link issues with
                             * NOT_DYNAMORIO_CORE_PROPER
                             */
                            if (!do_once_nt_pipe_transceive) {
                                do_once_nt_pipe_transceive = true;
                                ASSERT_NOT_REACHED();
                            }
                        });
                        return 0;
                    }
                } else {
                    NTLOG(GLOBAL, LOG_NT, 1,
                          "pipe transceive cancel succeeded code=" PFX "\n", res);
                    NTLOG(THREAD_GET, LOG_NT, 1,
                          "pipe transceive cancel succeeded code=" PFX "\n", res);
                    return 0;
                }
            }
        } else {
            /* completed synchronously (success) */
            NTLOG(GLOBAL, LOG_NT, 1, "pipe transceive completed sync\n");
            NTLOG(THREAD_GET, LOG_NT, 1, "pipe transceive completed sync\n");
        }
    } else {
        NTLOG(GLOBAL, LOG_NT, 1, "pipe transceive fail\n");
        NTLOG(THREAD_GET, LOG_NT, 1, "pipe transceive fail\n");
        return 0;
    }

    /* length of output */
    return iob.Information;
};

#ifdef PURE_NTDLL

/* FIXME: The following should be pure ntdll.dll replacements of kernel32.dll */

/* FIXME: Currently kernel32 counterparts are used */
/* FIXME: Impersonation needs to be handled */

typedef struct _THREAD_IMPERSONATION_INFORMATION {
    HANDLE ThreadImpersonationToken;
} THREAD_IMPERSONATION_INFORMATION, *PTHREAD_IMPERSONATION_INFORMATION;

/* gets a handle to thread impersonation token, returns NULL on failure  */
static HANDLE
get_thread_impersonation_token(HANDLE hthread)
{
    NTSTATUS res;
    HANDLE htoken;
    ACCESS_MASK rights = 0XC; /* CHECK */

    res = NtOpenThreadToken(hthread, rights, true /* as self */, &htoken);

    if (!NT_SUCCESS(res)) {
        NTPRINT("Error 0x%x in get thread token\n", res);
        return NULL;
    } else
        return htoken;
}

/* sets impersonation token, returns 0 on failure  */
static bool
set_thread_impersonation_token(HANDLE hthread, HANDLE himptoken)
{
    NTSTATUS res;
    THREAD_IMPERSONATION_INFORMATION imp_info = { himptoken };

    res = nt_raw_SetInformationThread(hthread, ThreadImpersonationToken, &imp_info,
                                      sizeof(imp_info));

    if (!NT_SUCCESS(res)) {
        NTPRINT("Error 0x%x in set thread token\n", res);
        return false;
    } else
        return true;
}

#endif /* PURE_NTDLL */

#ifdef WINDOWS_PC_SAMPLE
/* for profiling */

/* buffer size is in bytes, buffer_size >= 4 * (size / 2^shift <rounded up>) */
HANDLE
nt_create_profile(HANDLE process_handle, void *start, uint size, uint *buffer,
                  uint buffer_size, uint shift)
{
    NTSTATUS res;
    HANDLE prof_handle;

    GET_NTDLL(NtCreateProfile,
              (OUT PHANDLE ProfileHandle, IN HANDLE ProcessHandle, IN PVOID Base,
               IN ULONG Size, IN ULONG BucketShift, IN PULONG Buffer,
               IN ULONG BufferLength, IN KPROFILE_SOURCE Source, IN ULONG ProcessorMask));

    /* there are restrictions on shift, check FIXME */

    res = NtCreateProfile(&prof_handle, process_handle, start, size, shift,
                          (ULONG *)buffer, buffer_size, ProfileTime, 0);

    ASSERT(NT_SUCCESS(res));

    return prof_handle;
}

void
nt_set_profile_interval(uint nanoseconds)
{
    NTSTATUS res;

    GET_NTDLL(NtSetIntervalProfile, (IN ULONG Interval, IN KPROFILE_SOURCE Source));

    res = NtSetIntervalProfile(nanoseconds, ProfileTime);

    ASSERT(NT_SUCCESS(res));
}

int
nt_query_profile_interval()
{
    NTSTATUS res;
    ULONG interval;

    GET_NTDLL(NtQueryIntervalProfile, (IN KPROFILE_SOURCE Source, OUT PULONG Interval));

    res = NtQueryIntervalProfile(ProfileTime, &interval);

    ASSERT(NT_SUCCESS(res));

    return interval;
}

void
nt_start_profile(HANDLE profile_handle)
{
    NTSTATUS res;

    GET_NTDLL(NtStartProfile, (IN HANDLE ProfileHandle));

    res = NtStartProfile(profile_handle);

    ASSERT(NT_SUCCESS(res));
}

void
nt_stop_profile(HANDLE profile_handle)
{
    NTSTATUS res;

    GET_NTDLL(NtStopProfile, (IN HANDLE ProfileHandle));

    res = NtStopProfile(profile_handle);

    ASSERT(NT_SUCCESS(res));
}
#endif

/****************************************************************************
 * These process creation routines are based on Nebbett example 6.2
 */

typedef struct _PORT_MESSAGE {
    USHORT DataSize;
    USHORT MessageSize;
    USHORT MessageType;
    USHORT VirtualRangesOffset;
    CLIENT_ID ClientId;
    ULONG MessageId;
    ULONG SectionSize;
    // UCHAR Data[];
} PORT_MESSAGE, *PPORT_MESSAGE;

typedef struct _CSRSS_MESSAGE {
    ULONG Unknown1;
    ULONG Opcode;
    ULONG Status;
    ULONG Unknown2;
} CSRSS_MESSAGE, *PCSRSS_MESSAGE;

/* N.B.: we now rely on this Csr routine, it works on 2K, XP, and 2003, let's
 * hope it doesn't change in the future
 */
static int
inform_csrss(HANDLE hProcess, HANDLE hthread, process_id_t pid, thread_id_t tid)
{
    GET_NTDLL(CsrClientCallServer,
              (IN OUT PVOID Message, IN PVOID, IN ULONG Opcode, IN ULONG Size));
    /* We pass a layered message with two headers to csrss.
     * However, the two headers, PORT_MESSAGE and CSRSS_MESSAGE, are OUT values,
     * not IN at all.  CsrClientCallServer fills in the first 4 fields of
     * PORT_MESSAGE and the first 2 fields of CSRSS_MESSAGE.
     * It adds 0x10, the size of CSRSS_MESSAGE, to the size passed in when
     * it passes this buffer to NtRequestWaitReplyPort, as everything after
     * PORT_MESSAGE is data for the LPC to csrss.
     * Coming out, everything is now filled in except the final field of CSRSS_MESSAGE.
     * The CreateProcessInternalW code that calls CsrClientCallServer pushes
     * the opcode and the size as immediates so this is all known at compile time.
     */
    struct {
        PORT_MESSAGE PortMessage; /* port header */
        /* port data follows */
        CSRSS_MESSAGE CsrssMessage; /* csrss header */
        /* csrss data follows */
        PROCESS_INFORMATION ProcessInformation;
        CLIENT_ID Debugger;
        ULONG CreationFlags;
        ULONG VdmInfo[2];
        /* the above csrss data fields (size 0x24) are all that's passed to NT,
         * but other platforms have more, always observed to be 0, max of 0x98 on XP:
         */
        ULONG Unknown[0x98 - 0x24];
    } csrmsg = { { 0 }, { 0 }, { hProcess, hthread, (DWORD)pid, (DWORD)tid }, { 0 }, 0,
                 { 0 }, { 0 } };

    uint size = 0x24;
    PEB *peb = get_own_peb();
    /* Note the discrepancy: CLIENT_ID and PROCESS_BASIC_INFORMATION use
     * HANDLE or ULONG_PTR for the ids, but here we have DWORD, and
     * Windows API routines like kernel32!GetProcessId return DWORD.
     */
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(pid)));
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(tid)));
    if (peb->OSMajorVersion == 4)
        size = 0x24; /* NT */
    else {
        ASSERT(peb->OSMajorVersion == 5);
        if (peb->OSMinorVersion == 0)
            size = 0x28; /* 2000 */
        else if (peb->OSMinorVersion == 1)
            size = 0x98; /* XP */
        else if (peb->OSMinorVersion == 2)
            size = 0x90; /* 2003 */
    }
    return NT_SUCCESS(CsrClientCallServer(&csrmsg, 0, 0x10000, size));
}

static wchar_t *
copy_environment(HANDLE hProcess)
{
    /* this is precisely what KERNEL32!GetEnvironmentStringsW returns: */
    wchar_t *env = (wchar_t *)get_process_param_buf(
        get_own_peb()->ProcessParameters, get_own_peb()->ProcessParameters->Environment);
    SIZE_T n;
    SIZE_T m;
    void *p;

    for (n = 0; env[n] != 0; n += wcslen(env + n) + 1)
        ;
    n *= sizeof(*env);

    m = n;
    p = NULL;
    if (!NT_SUCCESS(NT_SYSCALL(AllocateVirtualMemory, hProcess, &p, 0, &m, MEM_COMMIT,
                               PAGE_READWRITE)))
        return NULL;
    if (!nt_write_virtual_memory(hProcess, p, env, n, 0))
        return NULL;
    return (wchar_t *)p;
}

static NTSTATUS
create_process_parameters(HANDLE hProcess, PEB *peb, UNICODE_STRING *imagefile,
                          UNICODE_STRING *cmdline)
{
    RTL_USER_PROCESS_PARAMETERS *pp;
    SIZE_T n;
    void *p;
    GET_NTDLL(RtlCreateProcessParameters,
              (OUT PRTL_USER_PROCESS_PARAMETERS * ProcParams,
               IN PUNICODE_STRING ImageFile, IN PUNICODE_STRING DllPath OPTIONAL,
               IN PUNICODE_STRING CurrentDirectory OPTIONAL,
               IN PUNICODE_STRING CommandLine OPTIONAL, IN ULONG CreationFlags,
               IN PUNICODE_STRING WindowTitle OPTIONAL,
               IN PUNICODE_STRING Desktop OPTIONAL, IN PUNICODE_STRING Reserved OPTIONAL,
               IN PUNICODE_STRING Reserved2 OPTIONAL));
    GET_NTDLL(RtlDestroyProcessParameters, (IN PRTL_USER_PROCESS_PARAMETERS ProcParams));

    RtlCreateProcessParameters(&pp, imagefile, 0, 0, cmdline, 0, 0, 0, 0, 0);
    pp->Environment = copy_environment(hProcess);
    if (pp->Environment == NULL)
        return 0;
    n = pp->Length;
    p = NULL;
    if (!NT_SUCCESS(NT_SYSCALL(AllocateVirtualMemory, hProcess, &p, 0, &n, MEM_COMMIT,
                               PAGE_READWRITE)))
        return 0;
    if (!nt_write_virtual_memory(hProcess, p, pp, pp->Length, 0))
        return 0;
    /* update the pointer in child's PEB */
    if (!nt_write_virtual_memory(hProcess, &peb->ProcessParameters, &p, sizeof(void *),
                                 0))
        return 0;
    if (!NT_SUCCESS(RtlDestroyProcessParameters(pp)))
        return 0;
    return 1;
}

/* avoid needing x86_code.c from x86.asm from get_own_context_helper() */
#if !defined(NOT_DYNAMORIO_CORE) && !defined(NOT_DYNAMORIO_CORE_PROPER)
/* Executable name must be in kernel object name form
 * (e.g., \SystemRoot\System32\notepad.exe, or \??\c:\foo\bar.exe)
 * The executable name on the command line can be in any form.
 * On success returns a handle for the child
 * On failure returns INVALID_HANDLE_VALUE
 */
HANDLE
create_process(wchar_t *exe, wchar_t *cmdline)
{
    UNICODE_STRING uexe;
    UNICODE_STRING ucmdline;
    HANDLE hProcess = INVALID_HANDLE_VALUE;
    HANDLE hthread = INVALID_HANDLE_VALUE;
    HANDLE hSection = INVALID_HANDLE_VALUE;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    OBJECT_ATTRIBUTES oa;
    IO_STATUS_BLOCK iosb;
    SECTION_IMAGE_INFORMATION sii;
    thread_id_t tid;
    PROCESS_BASIC_INFORMATION pbi;

    GET_NTDLL(NtCreateProcess,
              (OUT PHANDLE ProcessHandle, IN ACCESS_MASK DesiredAccess,
               IN POBJECT_ATTRIBUTES ObjectAttributes, IN HANDLE InheritFromProcessHandle,
               IN BOOLEAN InheritHandles, IN HANDLE SectionHandle OPTIONAL,
               IN HANDLE DebugPort OPTIONAL, IN HANDLE ExceptionPort OPTIONAL));
    GET_NTDLL(NtTerminateProcess,
              (IN HANDLE ProcessHandle OPTIONAL, IN NTSTATUS ExitStatus));

    NTPRINT("create_process starting\n");
    if (!NT_SUCCESS(wchar_to_unicode(&uexe, exe)))
        goto creation_error;
    if (!NT_SUCCESS(wchar_to_unicode(&ucmdline, cmdline)))
        goto creation_error;

    /* create a section and a process that maps it in */
    InitializeObjectAttributes(&oa, &uexe, OBJ_CASE_INSENSITIVE, NULL, NULL);
    if (!NT_SUCCESS(nt_raw_OpenFile(&hFile, FILE_EXECUTE | SYNCHRONIZE, &oa, &iosb,
                                    FILE_SHARE_READ, FILE_SYNCHRONOUS_IO_NONALERT))) {
        NTPRINT("create_process: failed to open file %S\n", uexe.Buffer);
        goto creation_error;
    }
    oa.ObjectName = 0;
    if (!NT_SUCCESS(NT_SYSCALL(CreateSection, &hSection, SECTION_ALL_ACCESS, &oa, 0,
                               PAGE_EXECUTE, SEC_IMAGE, hFile))) {
        NTPRINT("create_process: failed to create section\n");
        goto creation_error;
    }
    close_file(hFile);
    if (!NT_SUCCESS(NtCreateProcess(&hProcess, PROCESS_ALL_ACCESS, &oa,
                                    NT_CURRENT_PROCESS, TRUE, hSection, 0, 0))) {
        NTPRINT("create_process: failed to create process\n");
        goto creation_error;
    }
    if (!NT_SUCCESS(
            NtQuerySection(hSection, SectionImageInformation, &sii, sizeof(sii), 0))) {
        NTPRINT("create_process: failed to query section\n");
        goto creation_error;
    }
    close_handle(hSection);
    NTPRINT("create_process: created section and process\n");

    /* FIXME : if thread returns from its EntryPoint function will crash because
     * our_create_thread skips the kernel32 ThreadStartThunk */
    /* FIXME : need to know whether target process is 32bit or 64bit, for now
     * assume 32bit. */
    hthread = our_create_thread(hProcess, false, sii.EntryPoint, NULL, NULL, 0,
                                sii.StackReserve, sii.StackCommit, TRUE, &tid);

    if (hthread == INVALID_HANDLE_VALUE) {
        NTPRINT("create_process: failed to create thread\n");
        goto creation_error;
    }

    if (!NT_SUCCESS(query_process_info(hProcess, &pbi))) {
        NTPRINT("create_process: failed to query process info\n");
        goto creation_error;
    }

    if (!create_process_parameters(hProcess, pbi.PebBaseAddress, &uexe, &ucmdline)) {
        NTPRINT("create_process: failed to create process parameters\n");
        goto creation_error;
    }

    if (!inform_csrss(hProcess, hthread, pbi.UniqueProcessId, tid)) {
        NTPRINT("create_process: failed to inform csrss\n");
        goto creation_error;
    }

    if (!nt_thread_resume(hthread, NULL)) {
        NTPRINT("create_process: failed to resume initial thread\n");
        goto creation_error;
    }

    close_handle(hthread);
    NTPRINT("create_process: successfully created process %d!\n", pbi.UniqueProcessId);
    return hProcess;

creation_error:
    if (hFile != INVALID_HANDLE_VALUE)
        close_file(hFile);
    if (hSection != INVALID_HANDLE_VALUE)
        close_handle(hSection);
    if (hthread != INVALID_HANDLE_VALUE)
        close_handle(hthread);
    if (hProcess != INVALID_HANDLE_VALUE) {
        NtTerminateProcess(hProcess, 0);
        close_handle(hProcess);
    }
    return INVALID_HANDLE_VALUE;
}

/* NOTE does not inform csrss, if caller wants csrss informed must do it
 * themselves (see inform_csrss). If csrss isn't informed then the stack will
 * probably not be freed when the thread exits and certain other apps (some
 * cygwin versions, debuggers) will choke on these threads. Threads created with
 * this routine must also kill themselves as opposed to returning from their
 * start routines (we skip the kernel32 ThreadStartThunk since we can't
 * programatically get its address) and no top-level exception handler is set up
 * (again the kernel32 StartThunk does that). FIXME on Vista the StartThunk equivalent
 * ntdll!RtlUserThreadStart is exported so we could target it on that platform.
 */
/* If arg_buf != NULL then arg_buf_size bytes are copied from arg_buf to the new thread's
 * stack and a pointer to that is passed as the argument to the thread routine instead of
 * arg.
 */
/* returns INVALID_HANDLE_VALUE on error */
static HANDLE
create_thread_common(HANDLE hProcess, bool target_64bit, void *start_addr, void *arg,
                     const void *arg_buf, size_t arg_buf_size, USER_STACK *stack,
                     bool suspended, thread_id_t *tid)
{
    HANDLE hthread;
    OBJECT_ATTRIBUTES oa;
    CLIENT_ID cid;
    /* Context must be 16 byte aligned on 64bit */
    byte context_buf[sizeof(CONTEXT) + 16] = { 0 };
    CONTEXT *context = (CONTEXT *)ALIGN_FORWARD(context_buf, 16);
    void *thread_arg = arg;
    ptr_uint_t final_stack = 0;
    NTSTATUS res;
    GET_RAW_SYSCALL(CreateThread, OUT PHANDLE ThreadHandle, IN ACCESS_MASK DesiredAccess,
                    IN POBJECT_ATTRIBUTES ObjectAttributes, IN HANDLE ProcessHandle,
                    OUT PCLIENT_ID ClientId, IN PCONTEXT ThreadContext,
                    IN PUSER_STACK UserStack, IN BOOLEAN CreateSuspended);

    InitializeObjectAttributes(&oa, NULL, OBJ_CASE_INSENSITIVE, NULL, NULL);

    /* set the context: initialize with our own
     * We need CONTEXT_CONTROL and CONTEXT_INTEGER for setting the state here.  Also,
     * on 2k3 (but not XP) we appear to need CONTEXT_SEGMENTS (xref PR 269230) as well.
     * FIXME - on 64-bit CONTEXT_FULL includes CONTEXT_FLOATING_POINT (though not
     * CONTEXT_SEGMENTS) so might be nice to grab that as well once PR 266070 is
     * implemented. */
    context->ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS;
    GET_OWN_CONTEXT(context);
    context->CXT_XSP = (ptr_uint_t)stack->ExpandableStackBase;
    context->CXT_XIP = (ptr_uint_t)start_addr;

    /* write the argument(s) */
    if (arg_buf != NULL) {
        context->CXT_XSP -= arg_buf_size;
        thread_arg = (void *)context->CXT_XSP;
        if (!nt_write_virtual_memory(hProcess, thread_arg, arg_buf, arg_buf_size, NULL)) {
            NTPRINT("create_thread: failed to write arguments\n");
            return INVALID_HANDLE_VALUE;
        }
    }

    /* set up function call */
    if (target_64bit) {
        context->CXT_XCX = (ptr_uint_t)thread_arg;
        /* 64-bit Windows requires 16-byte stack alignment (see calling convention) */
        context->CXT_XSP = ALIGN_BACKWARD(context->CXT_XSP, 16);
        /* leave spill space for 4 64-byte registers (see calling convention) */
        context->CXT_XSP -= 4 * sizeof(uint64);
        /* push the return address (i.e. 0) */
        context->CXT_XSP -= 8;
    } else {
        void *buf[2];
        buf[1] = thread_arg;
        buf[0] = NULL; /* would be return address */
        context->CXT_XSP -= sizeof(buf);
        if (!nt_write_virtual_memory(hProcess, (void *)context->CXT_XSP, buf, sizeof(buf),
                                     NULL)) {
            NTPRINT("create_thread: failed to write argument\n");
            return INVALID_HANDLE_VALUE;
        }
    }
    final_stack = context->CXT_XSP;

    /* create the thread - NOTE always creating suspended (see below) */
    res = NT_SYSCALL(CreateThread, &hthread, THREAD_ALL_ACCESS, &oa, hProcess, &cid,
                     context, stack, (byte)TRUE);
    if (!NT_SUCCESS(res)) {
        NTPRINT("create_thread: failed to create thread: %x\n", res);
        return INVALID_HANDLE_VALUE;
    }
    /* Xref PR 252008 & PR 252745 - on 32-bit Windows the kernel will set esp
     * for the initialization APC to the value supplied in the context when the thread
     * was created. However, on WOW64 and 64-bit Windows the kernel sets xsp to 20 bytes
     * in from the stack base for the initialization APC. Since we have data/arguments
     * sitting on the stack we need to explicitly set the context before we let the
     * thread run the initialization APC. */
    nt_get_context(hthread, context);
    context->CXT_XSP = final_stack;
    nt_set_context(hthread, context);
    if (!suspended) {
        int prev_count;
        nt_thread_resume(hthread, &prev_count);
    }
    if (tid != NULL)
        *tid = (thread_id_t)cid.UniqueThread;
    return hthread;
}

static HANDLE
our_create_thread_ex(HANDLE hProcess, bool target_64bit, void *start_addr, void *arg,
                     const void *arg_buf, size_t arg_buf_size, uint stack_reserve,
                     uint stack_commit, bool suspended, thread_id_t *tid)
{
    HANDLE hthread;
    OBJECT_ATTRIBUTES oa;
    CLIENT_ID cid;
    TEB *teb;
    void *thread_arg = arg;
    create_thread_info_t info = { 0 };
    NTSTATUS res;
    /* NtCreateThreadEx doesn't exist prior to Vista. */
    ASSERT(syscalls[SYS_CreateThreadEx] != SYSCALL_NOT_PRESENT);
    GET_RAW_SYSCALL(CreateThreadEx, OUT PHANDLE ThreadHandle,
                    IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes,
                    IN HANDLE ProcessHandle, IN LPTHREAD_START_ROUTINE Win32StartAddress,
                    IN LPVOID StartParameter, IN BOOL CreateSuspended,
                    IN uint StackZeroBits, IN SIZE_T StackCommitSize,
                    IN SIZE_T StackReserveSize, INOUT create_thread_info_t * thread_info);

    InitializeObjectAttributes(&oa, NULL, OBJ_CASE_INSENSITIVE, NULL, NULL);

    if (arg_buf != NULL) {
        /* XXX: Currently we leak this memory, except for nudge where the caller
         * sets NUDGE_FREE_ARG.
         */
        if (!NT_SUCCESS(nt_remote_allocate_virtual_memory(
                hProcess, &thread_arg, arg_buf_size, PAGE_READWRITE, MEM_COMMIT))) {
            NTPRINT("create_thread: failed to allocate arg buf\n");
            return INVALID_HANDLE_VALUE;
        }
        if (!nt_write_virtual_memory(hProcess, thread_arg, arg_buf, arg_buf_size, NULL)) {
            NTPRINT("create_thread: failed to write arguments\n");
            return INVALID_HANDLE_VALUE;
        }
    }

    info.struct_size = sizeof(info);
    info.client_id.flags = THREAD_INFO_ELEMENT_CLIENT_ID | THREAD_INFO_ELEMENT_UNKNOWN_2;
    info.client_id.buffer_size = sizeof(cid);
    info.client_id.buffer = &cid;
    /* We get STATUS_INVALID_PARAMETER unless we also ask for teb. */
    info.teb.flags = THREAD_INFO_ELEMENT_TEB | THREAD_INFO_ELEMENT_UNKNOWN_2;
    info.teb.buffer_size = sizeof(TEB *);
    info.teb.buffer = &teb;
    res = NT_RAW_SYSCALL(CreateThreadEx, &hthread, THREAD_ALL_ACCESS, &oa, hProcess,
                         (LPTHREAD_START_ROUTINE)convert_data_to_function(start_addr),
                         thread_arg, suspended ? TRUE : FALSE, 0, stack_commit,
                         stack_reserve, &info);
    if (!NT_SUCCESS(res)) {
        NTPRINT("create_thread_ex: failed to create thread: %x\n", res);
        return INVALID_HANDLE_VALUE;
    }
    if (tid != NULL)
        *tid = (thread_id_t)cid.UniqueThread;
    return hthread;
}

/* Creates a new stack w/ guard page */
HANDLE
our_create_thread(HANDLE hProcess, bool target_64bit, void *start_addr, void *arg,
                  const void *arg_buf, size_t arg_buf_size, uint stack_reserve,
                  uint stack_commit, bool suspended, thread_id_t *tid)
{
    USER_STACK stack = { 0 };
    uint num_commit_bytes, old_prot;
    void *p;

    ASSERT(stack_commit + PAGE_SIZE <= stack_reserve &&
           ALIGNED(stack_commit, PAGE_SIZE) && ALIGNED(stack_reserve, PAGE_SIZE));

    if (get_os_version() >= WINDOWS_VERSION_8) {
        /* NtCreateThread not available: use Ex where the kernel makes the stack. */
        return our_create_thread_ex(hProcess, target_64bit, start_addr, arg, arg_buf,
                                    arg_buf_size, stack_reserve, stack_commit, suspended,
                                    tid);
    }

    if (!NT_SUCCESS(nt_remote_allocate_virtual_memory(
            hProcess, &stack.ExpandableStackBottom, stack_reserve, PAGE_READWRITE,
            MEM_RESERVE))) {
        NTPRINT("create_thread: failed to allocate stack\n");
        return INVALID_HANDLE_VALUE;
    }
    /* For failures beyond this point we don't bother deallocating the stack. */
    stack.ExpandableStackBase = ((byte *)stack.ExpandableStackBottom) + stack_reserve;
    stack.ExpandableStackLimit = ((byte *)stack.ExpandableStackBase) - stack_commit;
    num_commit_bytes = stack_commit + (uint)PAGE_SIZE;
    p = ((byte *)stack.ExpandableStackBase) - num_commit_bytes;
    if (!NT_SUCCESS(nt_remote_allocate_virtual_memory(hProcess, &p, num_commit_bytes,
                                                      PAGE_READWRITE, MEM_COMMIT))) {
        NTPRINT("create_thread: failed to commit stack pages\n");
        return INVALID_HANDLE_VALUE;
    }
    if (!nt_remote_protect_virtual_memory(hProcess, p, PAGE_SIZE,
                                          PAGE_READWRITE | PAGE_GUARD, &old_prot)) {
        NTPRINT("create_thread: failed to protect stack guard page\n");
        return INVALID_HANDLE_VALUE;
    }

    return create_thread_common(hProcess, target_64bit, start_addr, arg, arg_buf,
                                arg_buf_size, &stack, suspended, tid);
}

/* is_new_thread_client_thread() assumes param is the stack */
void
our_create_thread_wrapper(void *param)
{
    /* Thread was initialized in intercept_new_thread() */
    dcontext_t *dcontext = get_thread_private_dcontext();
    /* Get the data we need from where our_create_thread_have_stack() wrote them. */
    byte *stack_base = (byte *)param;
    size_t stack_size = *(size_t *)(stack_base - sizeof(void *));
    byte *src = stack_base - stack_size;
    void *func = *(void **)src;
    size_t args_size = *(size_t *)(src + sizeof(void *));
    void *arg = src + 2 * sizeof(void *);
    /* Update TEB for proper SEH, etc. */
    TEB *teb = get_own_teb();
    teb->StackLimit = src;
    teb->StackBase = stack_base;
    call_switch_stack(arg, stack_base, (void (*)(void *))convert_data_to_function(func),
                      NULL, false /*no return*/);
    ASSERT_NOT_REACHED();
}

/* Uses caller-allocated stack.  hProcess must be NT_CURRENT_PROCESS for win8+. */
HANDLE
our_create_thread_have_stack(HANDLE hProcess, bool target_64bit, void *start_addr,
                             void *arg, const void *arg_buf, size_t arg_buf_size,
                             byte *stack_base, size_t stack_size, bool suspended,
                             thread_id_t *tid)
{
    if (get_os_version() >= WINDOWS_VERSION_8) {
        /* i#1309: we need a wrapper function so we can use NtCreateThreadEx
         * and then switch stacks.  This is too hard to arrange in another process.
         */
        ASSERT(hProcess == NT_CURRENT_PROCESS &&
               "No support for creating a remote thread with a custom stack");
        /* We store what the wrapper needs on the end of the stack so it won't
         * get clobbered by call_switch_stack().
         */
        byte *dest = stack_base - stack_size;
        *(void **)dest = start_addr;
        if (arg_buf == NULL)
            arg_buf_size = sizeof(void *);
        *(size_t *)(dest + sizeof(void *)) = arg_buf_size;
        if (arg_buf != NULL)
            memcpy(dest + 2 * sizeof(void *), arg_buf, arg_buf_size);
        else
            *(void **)(dest + 2 * sizeof(void *)) = arg;
        /* We store the stack size at the base so we can find the top. */
        *(size_t *)(stack_base - sizeof(void *)) = stack_size;
        return our_create_thread_ex(hProcess, target_64bit,
                                    (void *)our_create_thread_wrapper, stack_base, NULL,
                                    0, 0, 0, suspended, tid);
    } else {
        USER_STACK stack = { 0 };
        stack.ExpandableStackBase = stack_base;
        stack.ExpandableStackLimit = stack_base - stack_size;
        stack.ExpandableStackBottom = stack_base - stack_size;
        return create_thread_common(hProcess, target_64bit, start_addr, arg, arg_buf,
                                    arg_buf_size, &stack, suspended, tid);
    }
}
#endif /* !defined(NOT_DYNAMORIO_CORE) && !defined(NOT_DYNAMORIO_CORE_PROPER) */

/* Except where otherwise noted the following notes are taken from analysis of
 * LoadLibrary on xpsp2 (\\araksha-tim). */
/* The kernel32 version of this function does some additional work. It checks
 * to see if you are loading twain_32.dll and handles that separately. It also
 * does the necessary string conversions to ntdll formats. When kernel32 calls
 * LdrLoadDll it passes in a ; separated path string for PathToFile. The second
 * argument is a little trickier. Some sources say it's a ULONG flags argument
 * (that corresponds to Ex ver flags) while others say its PDWORD LdrErr. On
 * the platforms observed, xpsp2 (\\araksha-tim) and win2k (test-east), it's
 * definitely a pointer to a stack location that appears to hold a flag value.
 * The flag value does not match the Ex flags however. It doesn't appear to be
 * IN/OUT argument (or OUT error) since I've never seen it written to, even when
 * the loader had an error). A Summary of observed 2nd arguments:
 *
 * for xpsp2 (araksha-tim) & win2k (test-east) (all values hex)
 * Ex Flag             2nd arg to LdrLoadDll    2nd arg deref
 * 1                   stack ptr                2
 * 0,8                 stack ptr                0
 * 2 (dll not loaded) calls BasepLoadLibraryAsDataFile instead of LdrLoadDll
 * 2 (dll loaded)      NULL
 * 10 (xpsp2 only)     stack ptr                1000
 *
 * See msdn for explanation of Ex flag values (0 is what we want and what the
 * non Ex versions use). The 2nd argument definitely appears to PDWORD/PULONG
 * and not a pointer to a larger struct, the next value on the stack after the
 * deref value is uninitialized (both before and after LdrLoadDll is called, even
 * if LdrLoadDll has an error). The argument does indeed appear to be optional.
 * Our load_library appears to work fine with either a ptr to 0 or NULL as the
 * 2nd argument (DllMain is called, load count adjusted correctly etc. either
 * way). Another mystery is that LoadLibraryExW also goes to the trouble of
 * building a UNICODE_STRING version of PathToFile, but then doesn't appear to
 * use it, perhaps it is for an unusual path through the function.
 * FIXME : understand behavior more.
 */
/* returns NULL on failure */
module_handle_t
load_library(wchar_t *lib_name)
{
    UNICODE_STRING ulib_name;
    HANDLE hMod;
    NTSTATUS res;
    ULONG flags = 0;
    GET_NTDLL(LdrLoadDll,
              (IN PCWSTR PathToFile OPTIONAL, IN PULONG Flags OPTIONAL,
               IN PUNICODE_STRING ModuleFileName, OUT PHANDLE ModuleHandle));

    /* we CANNOT be holding any DR locks here, since we are going to
     * execute app code (we call LdrLoadDll) that may grab app locks
     */
    ASSERT_OWN_NO_LOCKS();
    wchar_to_unicode(&ulib_name, lib_name);
    res = LdrLoadDll(NULL, &flags, &ulib_name, &hMod);
    if (!NT_SUCCESS(res))
        return NULL;
    return hMod;
}

/* Kernel32 FreeLibrary is a simple wrapper around this routine normally.
 * However, if the lsb of the module handle is set, unmaps and calls
 * LdrUnloadAlternateResourceModule. For our usage (which is always real dlls)
 * I think this should be fine. */
bool
free_library(module_handle_t lib)
{
    NTSTATUS res;
    GET_NTDLL(LdrUnloadDll, (IN HANDLE ModuleHandle));
    /* we CANNOT be holding any DR locks here, since we are going to
     * execute app code (we call LdrLoadDll) that may grab app locks
     */
    ASSERT_OWN_NO_LOCKS();
    res = LdrUnloadDll(lib);
    return NT_SUCCESS(res);
}

/* FIXME : the following function (get_module_handle)
 * should really be implemented in module.c rather than as wrappers to the
 * undocumented ntdll ldr routines.  In particular, LdrGetDllHandle does
 * allocate memory on the app's heap, so this is not fully transparent!
 */

/* The Kernel32 version appears to be more or less a wrapper around this
 * function. The kernel32 version has lots of code for processing the name into
 * a unicode string and what looks like handling the flags for the ex version. */
/* returns NULL on failure */
module_handle_t
get_module_handle(const wchar_t *lib_name)
{
    UNICODE_STRING ulib_name;
    HANDLE hMod;
    NTSTATUS res;
    /* NOTE - I've seen the first argument be 0, 1, or a pointer to a ; separated
     * path string.  GetModuleHandle usually seems to use 1 though I have no idea
     * what that means.  Seems to work fine either way (doesn't seem to adjust
     * the load count which was my first guess). */
#define LDR_GET_DLL_HANDLE_ARG1 ((PCWSTR)PTR_UINT_1)
    GET_NTDLL(LdrGetDllHandle,
              (IN PCWSTR PathToFile OPTIONAL, IN ULONG Unused OPTIONAL,
               IN PUNICODE_STRING ModuleFileName, OUT PHANDLE ModuleHandle));

    /* we CANNOT be holding any DR locks here, since we are going to
     * execute app code (we call LdrLoadDll) that may grab app locks
     */
    ASSERT_OWN_NO_LOCKS();
    wchar_to_unicode(&ulib_name, lib_name);
    res = LdrGetDllHandle(LDR_GET_DLL_HANDLE_ARG1, 0, &ulib_name, &hMod);
    if (!NT_SUCCESS(res))
        return NULL;
    return hMod;
}

/* Mostly a wrapper around NtCreateDirectoryObject.
 *
 * Note that dacl == NULL allows only owner to use the object -
 * sufficient for sharing only between processes of one user.
 */
NTSTATUS
nt_create_object_directory(OUT HANDLE *directory /* OUT */, PCWSTR object_directory_name,
                           bool permanent_directory, PSECURITY_DESCRIPTOR dacl)
{
    NTSTATUS res;
    UNICODE_STRING directory_name;
    OBJECT_ATTRIBUTES directory_attributes = { 0 };

    GET_NTDLL(NtCreateDirectoryObject,
              (OUT PHANDLE DirectoryHandle, IN ACCESS_MASK DesiredAccess,
               IN POBJECT_ATTRIBUTES ObjectAttributes));

    res = wchar_to_unicode(&directory_name, object_directory_name);
    if (!NT_SUCCESS(res)) {
        NTPRINT("nt_create_object_directory: base name conversion failed, res: %x\n",
                res);
        return res;
    }

    /* see DDK about all other flags */
    InitializeObjectAttributes(&directory_attributes, &directory_name,
                               (permanent_directory ? OBJ_PERMANENT : 0) | OBJ_OPENIF |
                                   OBJ_CASE_INSENSITIVE,
                               NULL,
                               /* no root, directory name should be fully qualified */
                               dacl);
    res = NtCreateDirectoryObject(directory, DIRECTORY_ALL_ACCESS, /* for creation */
                                  &directory_attributes);
    if (!NT_SUCCESS(res)) {
        NTPRINT("nt_create_object_directory: failed to create directory\n");
        return res;
    }

    return res;
}

/* only privileged processes will be allowed to create the directory
 * and set DACLs.
 *
 * creating a permanent (until next reboot) directory requires
 * SeCreatePermanentPrivilege.  Note that most user mode processes do
 * not have this privilege.  Most services, including winlogon.exe,
 * for sure have it though.
 *
 * If we pick any of these with ASLR_SHARED_INITIALIZE we'll be able
 * to set up a directory for all other processes (in fact all
 * processes can try but only the services will be able to create it,
 * and only the first one will succeed so indeed can have all.  Only
 * unusual case will be if a low user process is started before a
 * privileged process and therefore prevents a permanent directory
 * from being created.
 *
 */
NTSTATUS
nt_initialize_shared_directory(HANDLE *shared_directory /* OUT */, bool permanent)
{
    NTSTATUS res;
    HANDLE basedh = INVALID_HANDLE_VALUE;
    HANDLE dh = INVALID_HANDLE_VALUE;

    /* FIXME: TOFILE: need to create at least some reasonable DACL,
     * note that NULL allows only creator to use, so it is not as bad
     * as Everyone, but then prevents lower privileged users from even
     * using this Directory
     */
    PSECURITY_DESCRIPTOR dacl = NULL;
    /* The ACLs in the default security descriptor come from the
     * primary or impersonation token of the creator.  So in fact we
     * won't be able to do open this from others as is too restrictive
     * in this instance.
     */

    /* Create base object directory '\Determina' */
    res = nt_create_object_directory(&basedh, DYNAMORIO_SHARED_OBJECT_BASE, permanent,
                                     dacl);
    if (!NT_SUCCESS(res)) {
        NTPRINT("nt_create_shared_directory: failed to create shared directory\n");
        return res;
    }

    /* SECURITY_LOCAL_SID_AUTHORITY should be allowed to read all contents
     * to allow sharing between all process
     */
    dacl = NULL;

    /* Create shared DLL object directory '\Determina\SharedCache' */
    /* FIXME: we will need directories for specific SIDs, and further
     * restrict which processes can read what.  See
     * ASLR_SHARED_INITIALIZE.  Even this shared cache security
     * settings would need to be strengthened.
     */
    res = nt_create_object_directory(&dh, DYNAMORIO_SHARED_OBJECT_DIRECTORY, permanent,
                                     dacl);
    if (!NT_SUCCESS(res)) {
        NTPRINT("nt_create_shared_directory: failed to create shared directory\n");
        return res;
    }

    /* FIXME: note the dual use of the permanent flag here - in
     * addition to controlling the OBJ_PERMANENT object creation
     * attribute, for INTERNAL uses when we don't have a proper
     * initializer we simulate permanence by keeping a handle open in
     * the creating process */
    if (permanent) {
        /* close base handle only if permanent, otherwise dh=\...\SharedCache,
         * but subsequent lookup by name can't find \Determina\SharedCache,
         * so closing this handle would not be really useful for a non-permanent
         */
        close_handle(basedh);
    }

    /* caller only needs leaf node */
    *shared_directory = dh;
    return res;
}

/* any process should be able to open the shared mappings directory,
 * and maybe even add entries to it given high enough permissions
 */
NTSTATUS
nt_open_object_directory(HANDLE *shared_directory /* OUT */, PCWSTR object_directory_name,
                         bool allow_object_creation)
{
    NTSTATUS res;
    UNICODE_STRING directory_name;
    OBJECT_ATTRIBUTES directory_attributes = { 0 };
    HANDLE dh = INVALID_HANDLE_VALUE;

    GET_NTDLL(NtOpenDirectoryObject,
              (OUT PHANDLE DirectoryHandle, IN ACCESS_MASK DesiredAccess,
               IN POBJECT_ATTRIBUTES ObjectAttributes));

    res = wchar_to_unicode(&directory_name, object_directory_name);
    if (!NT_SUCCESS(res)) {
        NTPRINT("nt_open_object_directory: name conversion failed, res: %x\n", res);
        return res;
    }

    /* see DDK about all other flags */
    InitializeObjectAttributes(&directory_attributes, &directory_name,
                               OBJ_CASE_INSENSITIVE, NULL,
                               /* no root, directory name should be fully qualified */
                               NULL);
    res = NtOpenDirectoryObject(&dh,
                                DIRECTORY_QUERY | DIRECTORY_TRAVERSE |
                                    /* should it try to obtain permission
                                     * to create objects (e.g. publisher) */
                                    (allow_object_creation ? DIRECTORY_CREATE_OBJECT : 0),
                                &directory_attributes);
    /* note DIRECTORY_CREATE_OBJECT doesn't allow creating subdirs,
     * for which DIRECTORY_CREATE_SUBDIRECTORY is needed */
    if (!NT_SUCCESS(res)) {
        NTPRINT("nt_open_object_directory: failed to open res: %x\n", res);
        return res;
    }
    /* FIXME: we could retry if we can't get higher permissions */

    *shared_directory = dh;
    return res;
}

void
nt_close_object_directory(HANDLE hobjdir)
{
    close_handle(hobjdir);
}

/* Returns the symbolic link target in target_name. */
/* note target_name should be initialized with a valid Buffer and MaximumLength
 * Also according to the DDK returned_byte_length may be an IN
 * argument setting max bytes to copy
 */
NTSTATUS
nt_get_symlink_target(IN HANDLE directory_handle, IN PCWSTR symlink_name,
                      IN OUT UNICODE_STRING *target_name, OUT uint *returned_byte_length)
{
    NTSTATUS res;
    UNICODE_STRING link_unicode_name;
    OBJECT_ATTRIBUTES link_attributes = { 0 };
    HANDLE link_handle = INVALID_HANDLE_VALUE;

    GET_NTDLL(NtOpenSymbolicLinkObject,
              (OUT PHANDLE DirectoryHandle, IN ACCESS_MASK DesiredAccess,
               IN POBJECT_ATTRIBUTES ObjectAttributes));
    GET_NTDLL(NtQuerySymbolicLinkObject,
              (IN HANDLE DirectoryHandle, IN OUT PUNICODE_STRING TargetName,
               OUT PULONG ReturnLength OPTIONAL));

    res = wchar_to_unicode(&link_unicode_name, symlink_name);
    if (!NT_SUCCESS(res)) {
        NTPRINT("nt_get_symlink_target: name conversion failed, res: %x\n", res);
        return res;
    }

    InitializeObjectAttributes(&link_attributes, &link_unicode_name,
                               OBJ_CASE_INSENSITIVE
                               /* note it doesn't seem to require |
                                * OBJ_OPENLINK and in fact returns
                                * STATUS_INVALID_PARAMETER when that
                                * is set
                                */
                               ,
                               directory_handle, NULL);
    res = NtOpenSymbolicLinkObject(&link_handle, SYMBOLIC_LINK_QUERY, &link_attributes);
    if (!NT_SUCCESS(res)) {
        return res;
    }

    res = NtQuerySymbolicLinkObject(link_handle, target_name,
                                    (ULONG *)returned_byte_length);
    close_handle(link_handle);
    ASSERT(NT_SUCCESS(res));
    return res;
}

/* General notes about sharing memory */
/* section<PAGE_EXECUTE, SEC_IMAGE, app_file> gives us CoW in each
 * process, and we can't share the relocation information
 */
/* section<PAGE_EXECUTE_READWRITE, SEC_IMAGE, original app_file>
 * gives access denied since file is open only for execution.
 * Though even proper privileges do not overwrite the original
 * file - SEC_IMAGE is always copy on write.
 */

/* only using SEC_COMMIT either with page file, or with a
 * {file<FILE_EXECUTE | FILE_READ_DATA | FILE_WRITE_DATA>,
 * createsection<PAGE_EXECUTE_READWRITE, SEC_COMMIT, file>,
 * map<PAGE_READWRITE>} allows writers to write to a true shared
 * memory with readers.

 * If a particular reader needs private writes they can use a mapping
 * created as above by writers {file<FILE_EXECUTE | FILE_READ_DATA>,
 * opensection<SEC_COMMIT>, map<PAGE_WRITECOPY>} (can even track the
 * pages that have transitioned from PAGE_WRITECOPY into
 * PAGE_READWRITE to find which ones have been touched.
 */

/* complete wrapper around NtCreateSection but embeds InitializeObjectAttributes */
NTSTATUS
nt_create_section(OUT PHANDLE SectionHandle, IN ACCESS_MASK DesiredAccess,
                  IN PLARGE_INTEGER SectionSize OPTIONAL, IN ULONG Protect,
                  IN ULONG section_creation_attributes, IN HANDLE FileHandle,
                  /* object name attributes */
                  IN PCWSTR section_name OPTIONAL, IN ULONG object_name_attributes,
                  IN HANDLE object_directory, IN PSECURITY_DESCRIPTOR dacl)
{
    NTSTATUS res;
    UNICODE_STRING section_name_unicode;
    OBJECT_ATTRIBUTES section_attributes;

    if (section_name != NULL) {
        res = wchar_to_unicode(&section_name_unicode, section_name);
        ASSERT(NT_SUCCESS(res));
        if (!NT_SUCCESS(res))
            return res;
    }
    InitializeObjectAttributes(
        &section_attributes, ((section_name != NULL) ? &section_name_unicode : NULL),
        OBJ_CASE_INSENSITIVE | object_name_attributes, object_directory, dacl);

    res = NT_SYSCALL(CreateSection, SectionHandle, DesiredAccess, &section_attributes,
                     SectionSize, Protect, section_creation_attributes, FileHandle);
    if (!NT_SUCCESS(res)) {
        NTPRINT("nt_create_section: failed res: %x\n", res);
    }
    return res;
}

#if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)

/* complete wrapper around NtOpenSection */
/* note that section_name is required and is case insensitive to
 * support normal Windows case insensitivity of DLL lookup.
 * FIXME: unlikely may need to be changed for POSIX support
 */
NTSTATUS
nt_open_section(OUT PHANDLE SectionHandle, IN ACCESS_MASK DesiredAccess,
                /* object name attributes */
                IN PCWSTR section_name, /* required */
                IN ULONG object_name_attributes, IN HANDLE object_directory)
{
    NTSTATUS res;
    UNICODE_STRING section_name_unicode;
    OBJECT_ATTRIBUTES section_attributes;

    ASSERT(section_name != NULL);
    res = wchar_to_unicode(&section_name_unicode, section_name);
    ASSERT(NT_SUCCESS(res));
    if (!NT_SUCCESS(res))
        return res;
    InitializeObjectAttributes(&section_attributes, &section_name_unicode,
                               OBJ_CASE_INSENSITIVE | object_name_attributes,
                               object_directory, NULL);
    res = NT_SYSCALL(OpenSection, SectionHandle, DesiredAccess, &section_attributes);
    if (!NT_SUCCESS(res)) {
        NTPRINT("nt_open_section: failed res: %x\n", res);
    }
    return res;
}

bool
are_mapped_files_the_same(app_pc addr1, app_pc addr2)
{
#    if 0 /* NYI: case 8502 */

    /* FIXME: this doesn't exist on NT4 - make sure we handle
     * gracefully not finding the target - needs a very explicit
     * d_r_get_proc_address() here.
     */
    GET_NTDLL(ZwAreMappedFilesTheSame, (
                                        IN PVOID Address1,
                                        IN PVOID Address2
                                        ));
#    endif

    ASSERT_NOT_TESTED();
    ASSERT_NOT_IMPLEMENTED(false);
    /* Testing: check return values for: addresses in different DLLs;
     * addresses in same DLL; addresses in same DLL but coming from
     * different mappings - the key one for us.
     */

    return false;
}

#endif /* !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE) */

/* Mostly a wrapper around NtCreateFile, geared to opening existing
 * module files.  See the DDK and SDK for complete argument documentation.
 *
 * callers have to close_handle() after use.
 *
 * file_path can be a path relative to root_directory_handle, or if
 * root_directory_handle is NULL file_path has to be an NT absolute
 * path (e.g. produced by the likes of RtlDosPathNameToNtPathName_U)
 */

/* desired_access_rights - a subset of FILE_EXECUTE | FILE_READ_DATA |
 * FILE_WRITE_DATA.  Note: FILE_READ_DATA would be necessary for a
 * later section mapping as PAGE_WRITECOPY, and FILE_WRITE_DATA may be
 * needed for a PAGE_EXECUTE_WRITECOPY mapping if not SEC_IMAGE
 */

/* file_special_attributes typically FILE_ATTRIBUTE_NORMAL, possible
 * other flags to use here FILE_ATTRIBUTE_TEMPORARY and maybe
 * FILE_FLAG_DELETE_ON_CLOSE? */

/* file_sharing_flags  */
NTSTATUS
nt_create_module_file(OUT HANDLE *file_handle, const wchar_t *file_path,
                      HANDLE root_directory_handle OPTIONAL,
                      ACCESS_MASK desired_access_rights, uint file_special_attributes,
                      uint file_sharing_flags, uint create_disposition,
                      size_t allocation_size)
{
    NTSTATUS res = nt_create_file(
        file_handle, file_path, root_directory_handle, allocation_size,
        SYNCHRONIZE | desired_access_rights, file_special_attributes, file_sharing_flags,
        create_disposition, FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE);
    if (!NT_SUCCESS(res)) {
        NTPRINT("nt_open_module_file: can't open file, res: %x\n", res);
    }
    return res;
}

/* thin wrapper around ZwQueryInformationFile -
 * see DDK for documented information classes */
NTSTATUS
nt_query_file_info(IN HANDLE FileHandle, OUT PVOID FileInformation,
                   IN ULONG FileInformationLength,
                   IN FILE_INFORMATION_CLASS FileInformationClass)
{
    NTSTATUS res;
    IO_STATUS_BLOCK iob = { 0, 0 };

    res = NtQueryInformationFile(FileHandle, &iob, FileInformation, FileInformationLength,
                                 FileInformationClass);
    if (!NT_SUCCESS(res)) {
        NTPRINT("nt_query_file_info: can't open file, res: %x\n", res);
    }

    return res;
}

/* thin wrapper around ZwSetInformationFile -
 * see DDK for fully documented information classes
 */
NTSTATUS
nt_set_file_info(IN HANDLE FileHandle, IN PVOID FileInformation,
                 IN ULONG FileInformationLength,
                 IN FILE_INFORMATION_CLASS FileInformationClass)
{
    NTSTATUS res;
    IO_STATUS_BLOCK iob = { 0, 0 };

    res = NtSetInformationFile(FileHandle, &iob, FileInformation, FileInformationLength,
                               FileInformationClass);
    if (!NT_SUCCESS(res)) {
        NTPRINT("nt_set_file_info: can't open file, res: %x\n", res);
    }

    return res;
}

/* thin wrapper around ZwQueryVolumeInformationFile -
 * see Windows Driver Kit: Installable File System Drivers
 * for documented information classes,
 * Note handle can be file, directory, device or volume.
 */
NTSTATUS
nt_query_volume_info(IN HANDLE FileHandle, OUT PVOID FsInformation,
                     IN ULONG FsInformationLength,
                     IN FS_INFORMATION_CLASS FsInformationClass)
{
    NTSTATUS res;
    IO_STATUS_BLOCK iob = { 0, 0 };

    GET_NTDLL(NtQueryVolumeInformationFile,
              (IN HANDLE FileHandle, OUT PIO_STATUS_BLOCK IoStatusBlock,
               OUT PVOID FsInformation, IN ULONG Length,
               IN FS_INFORMATION_CLASS FsInformationClass));

    res = NtQueryVolumeInformationFile(FileHandle, &iob, FsInformation,
                                       FsInformationLength, FsInformationClass);
    if (!NT_SUCCESS(res)) {
        NTPRINT("nt_query_volume_info: can't open file, res: %x\n", res);
    } else {
        ASSERT(iob.Information == FsInformationLength ||
               /* volume info needs a big buffer so ok to be oversized */
               (FsInformationClass == FileFsVolumeInformation &&
                iob.Information >= offsetof(FILE_FS_VOLUME_INFORMATION, VolumeLabel)));
    }
    return res;
}

#if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)

/* thin wrapper around ZwQuerySecurityObject -
 * Note handle can be any executive objective: including file, directory
 */
NTSTATUS
nt_query_security_object(IN HANDLE Handle, IN SECURITY_INFORMATION RequestedInformation,
                         OUT PSECURITY_DESCRIPTOR SecurityDescriptor,
                         IN ULONG SecurityDescriptorLength, OUT PULONG ReturnLength)
{
    NTSTATUS res;

    /* note that SecurityDescriptor returned is always PISECURITY_DESCRIPTOR_RELATIVE */
    GET_NTDLL(NtQuerySecurityObject,
              (IN HANDLE Handle, IN SECURITY_INFORMATION RequestedInformation,
               OUT PSECURITY_DESCRIPTOR SecurityDescriptor,
               IN ULONG SecurityDescriptorLength, OUT PULONG ReturnLength));
    res = NtQuerySecurityObject(Handle, RequestedInformation, SecurityDescriptor,
                                SecurityDescriptorLength, ReturnLength);
    /* if SecurityDescriptorLength is too small ReturnLength is set to
     * the number of bytes required for the available data
     */

    /* for a file handle possibly insufficient permissions or unsupported file system */
    if (!NT_SUCCESS(res)) {
        NTPRINT("nt_query_security_object: can't open file, res: %x\n", res);
    }
    return res;
}

/* expect SID to have already been allocated statically
 * (note we cannot use RtlAllocateAndInitializeSid to allocate memory!)
 */
void
initialize_known_SID(PSID_IDENTIFIER_AUTHORITY IdentifierAuthority, ULONG SubAuthority0,
                     SID *pSid)
{
    UCHAR SubAuthorityCount = 1;
    ASSERT(pSid != NULL);

    pSid->Revision = SID_REVISION;
    pSid->SubAuthorityCount = SubAuthorityCount;
    memcpy(&pSid->IdentifierAuthority, IdentifierAuthority,
           sizeof(SID_IDENTIFIER_AUTHORITY));

    pSid->SubAuthority[0] = SubAuthority0;
}

/* Use nt_get_context64_size() from 32-bit for the 64-bit max size. */
size_t
nt_get_context_size(DWORD flags)
{
    /* Moved out of nt_initialize_context():
     *   8d450c          lea     eax,[ebp+0Ch]
     *   50              push    eax
     *   57              push    edi
     *   ff15b0007a76    call    dword ptr [_imp__RtlGetExtendedContextLength]
     */
    int len;
    int res = ntdll_RtlGetExtendedContextLength(flags, &len);
    ASSERT(res >= 0);
    /* Add 16 so we can align it forward to 16. */
    return len + 16;
}

/* Initialize the buffer as CONTEXT with extension and return the pointer
 * pointing to the start of CONTEXT.
 * Normally buf_len would come from nt_get_context_size(flags).
 */
CONTEXT *
nt_initialize_context(char *buf, size_t buf_len, DWORD flags)
{
    /* Ideally, kernel32!InitializeContext is used to setup context.
     * However, DR should NEVER use kernel32. DR never uses anything in
     * any user library other than ntdll.
     */
    CONTEXT *cxt;
    if (TESTALL(CONTEXT_XSTATE, flags)) {
        context_ex_t *cxt_ex;
        int res;
        ASSERT(proc_avx_enabled());
        /* 8d45fc          lea     eax,[ebp-4]
         * 50              push    eax
         * 57              push    edi
         * ff7508          push    dword ptr [ebp+8]
         * ff15b4007a76    call    dword ptr [_imp__RtlInitializeExtendedContext]
         */
        res = ntdll_RtlInitializeExtendedContext((PVOID)buf, flags, (PVOID)&cxt_ex);
        ASSERT(res == 0);
        /* 6a00            push    0
         * ff75fc          push    dword ptr [ebp-4]
         * ff15b8007a76    call    dword ptr [_imp__RtlLocateLegacyContext]
         */
        cxt = (CONTEXT *)ntdll_RtlLocateLegacyContext(cxt_ex, 0);
        ASSERT(context_check_extended_sizes(cxt_ex, flags));
        ASSERT(cxt != NULL && (char *)cxt >= buf &&
               (char *)cxt + cxt_ex->all.length < buf + buf_len);
    } else {
        /* make it 16-byte aligned */
        cxt = (CONTEXT *)(ALIGN_FORWARD(buf, 0x10));
        ASSERT(!CONTEXT_DYNAMICALLY_LAID_OUT(flags)); /* ensure in synch */
    }
    cxt->ContextFlags = flags;
    return cxt;
}

/****************************************************************************
 * DrM-i#1066: We implement raw system call invocation for system calls
 * hooked by applications so that they can be used by private libs.
 * Most raw system calls are put into NOT_DYNAMORIO_CORE_PROPER since they
 * are not needed in NOT_DYNAMORIO_CORE_PROPER.
 */
GET_RAW_SYSCALL(OpenFile, PHANDLE file_handle, ACCESS_MASK desired_access,
                POBJECT_ATTRIBUTES object_attributes, PIO_STATUS_BLOCK io_status_block,
                ULONG share_access, ULONG open_options);

GET_RAW_SYSCALL(OpenKeyEx, PHANDLE key_handle, ACCESS_MASK desired_access,
                POBJECT_ATTRIBUTES object_attributes, ULONG open_options);

GET_RAW_SYSCALL(OpenProcessTokenEx, HANDLE process_handle, ACCESS_MASK desired_access,
                ULONG handle_attributes, PHANDLE token_handle);

GET_RAW_SYSCALL(OpenThread, PHANDLE thread_handle, ACCESS_MASK desired_access,
                POBJECT_ATTRIBUTES object_attributes, PCLIENT_ID client_id);

GET_RAW_SYSCALL(OpenThreadTokenEx, HANDLE thread_handle, ACCESS_MASK desired_access,
                BOOLEAN open_as_self, ULONG handle_attributes, PHANDLE token_handle);

GET_RAW_SYSCALL(QueryAttributesFile, POBJECT_ATTRIBUTES object_attributes,
                PFILE_BASIC_INFORMATION file_information);

GET_RAW_SYSCALL(SetInformationThread, HANDLE thread_handle,
                THREADINFOCLASS thread_information_class, PVOID thread_information,
                ULONG thread_information_length);

NTSTATUS
nt_raw_CreateFile(PHANDLE file_handle, ACCESS_MASK desired_access,
                  POBJECT_ATTRIBUTES object_attributes, PIO_STATUS_BLOCK io_status_block,
                  PLARGE_INTEGER allocation_size, ULONG file_attributes,
                  ULONG share_access, ULONG create_disposition, ULONG create_options,
                  PVOID ea_buffer, ULONG ea_length)
{
    NTSTATUS res;
    res = NT_SYSCALL(CreateFile, file_handle, desired_access, object_attributes,
                     io_status_block, allocation_size, file_attributes, share_access,
                     create_disposition, create_options, ea_buffer, ea_length);
#    ifdef DEBUG
    if (!NT_SUCCESS(res)) {
        NTLOG(GLOBAL, LOG_NT, 1, "nt_raw_CreateFile failed, res: %x\n", res);
    }
#    endif
    return res;
}

NTSTATUS
nt_raw_OpenFile(PHANDLE file_handle, ACCESS_MASK desired_access,
                POBJECT_ATTRIBUTES object_attributes, PIO_STATUS_BLOCK io_status_block,
                ULONG share_access, ULONG open_options)
{
    NTSTATUS res;
    res = NT_SYSCALL(OpenFile, file_handle, desired_access, object_attributes,
                     io_status_block, share_access, open_options);
#    ifdef DEBUG
    if (!NT_SUCCESS(res)) {
        NTLOG(GLOBAL, LOG_NT, 1, "nt_raw_OpenFile failed, res: %x\n", res);
    }
#    endif
    return res;
}

NTSTATUS
nt_raw_OpenKey(PHANDLE key_handle, ACCESS_MASK desired_access,
               POBJECT_ATTRIBUTES object_attributes)
{
    NTSTATUS res;
    res = NT_SYSCALL(OpenKey, key_handle, desired_access, object_attributes);
#    ifdef DEBUG
    if (!NT_SUCCESS(res)) {
        NTLOG(GLOBAL, LOG_NT, 1, "nt_raw_OpenKey failed, res: %x\n", res);
    }
#    endif
    return res;
}

NTSTATUS
nt_raw_OpenKeyEx(PHANDLE key_handle, ACCESS_MASK desired_access,
                 POBJECT_ATTRIBUTES object_attributes, ULONG open_options)
{
    NTSTATUS res;
    /* i#1011, OpenKeyEx does not exist in older Windows version */
    ASSERT(syscalls[SYS_OpenKeyEx] != SYSCALL_NOT_PRESENT);
    res = NT_RAW_SYSCALL(OpenKeyEx, key_handle, desired_access, object_attributes,
                         open_options);
#    ifdef DEBUG
    if (!NT_SUCCESS(res)) {
        NTLOG(GLOBAL, LOG_NT, 1, "nt_raw_OpenKeyEx failed, res: %x\n", res);
    }
#    endif
    return res;
}

NTSTATUS
nt_raw_OpenProcessTokenEx(HANDLE process_handle, ACCESS_MASK desired_access,
                          ULONG handle_attributes, PHANDLE token_handle)
{
    NTSTATUS res;
    res = NT_RAW_SYSCALL(OpenProcessTokenEx, process_handle, desired_access,
                         handle_attributes, token_handle);
#    ifdef DEBUG
    if (!NT_SUCCESS(res)) {
        NTLOG(GLOBAL, LOG_NT, 1, "nt_raw_OpenProcessTokenEx failed, res: %x\n", res);
    }
#    endif
    return res;
}

NTSTATUS
nt_raw_OpenThread(PHANDLE thread_handle, ACCESS_MASK desired_access,
                  POBJECT_ATTRIBUTES object_attributes, PCLIENT_ID client_id)
{
    NTSTATUS res;
    res = NT_SYSCALL(OpenThread, thread_handle, desired_access, object_attributes,
                     client_id);
#    ifdef DEBUG
    if (!NT_SUCCESS(res)) {
        NTLOG(GLOBAL, LOG_NT, 1, "nt_raw_OpenThread failed, res: %x\n", res);
    }
#    endif
    return res;
}

NTSTATUS
nt_raw_OpenThreadTokenEx(HANDLE thread_handle, ACCESS_MASK desired_access,
                         BOOLEAN open_as_self, ULONG handle_attributes,
                         PHANDLE token_handle)
{
    NTSTATUS res;
    res = NT_RAW_SYSCALL(OpenThreadTokenEx, thread_handle, desired_access, open_as_self,
                         handle_attributes, token_handle);
#    ifdef DEBUG
    if (!NT_SUCCESS(res)) {
        NTLOG(GLOBAL, LOG_NT, 1, "nt_raw_NtOpenThreadTokenEx failed, res: %x\n", res);
    }
#    endif
    return res;
}

NTSTATUS
nt_raw_QueryAttributesFile(POBJECT_ATTRIBUTES object_attributes,
                           PFILE_BASIC_INFORMATION file_information)
{
    NTSTATUS res;
    res = NT_SYSCALL(QueryAttributesFile, object_attributes, file_information);
#    ifdef DEBUG
    if (!NT_SUCCESS(res)) {
        NTLOG(GLOBAL, LOG_NT, 1, "nt_raw_QueryAttributesFile failed, res: %x\n", res);
    }
#    endif
    return res;
}

NTSTATUS
nt_raw_SetInformationFile(HANDLE file_handle, PIO_STATUS_BLOCK io_status_block,
                          PVOID file_information, ULONG length,
                          FILE_INFORMATION_CLASS file_information_class)
{
    NTSTATUS res;
    res = NT_SYSCALL(SetInformationFile, file_handle, io_status_block, file_information,
                     length, file_information_class);
#    ifdef DEBUG
    if (!NT_SUCCESS(res)) {
        NTLOG(GLOBAL, LOG_NT, 1, "nt_raw_SetInformationFile failed, res: %x\n", res);
    }
#    endif
    return res;
}

NTSTATUS
nt_raw_SetInformationThread(HANDLE thread_handle,
                            THREADINFOCLASS thread_information_class,
                            PVOID thread_information, ULONG thread_information_length)
{
    NTSTATUS res;
    res = NT_SYSCALL(SetInformationThread, thread_handle, thread_information_class,
                     thread_information, thread_information_length);
#    ifdef DEBUG
    if (!NT_SUCCESS(res)) {
        NTLOG(GLOBAL, LOG_NT, 1, "nt_raw_SetInformationThread failed, res: %x\n", res);
    }
#    endif
    return res;
}

NTSTATUS
nt_raw_UnmapViewOfSection(HANDLE process_handle, PVOID base_address)
{
    NTSTATUS res;
    res = NT_SYSCALL(UnmapViewOfSection, process_handle, base_address);
#    ifdef DEBUG
    if (!NT_SUCCESS(res)) {
        NTLOG(GLOBAL, LOG_NT, 1, "nt_raw_UnmapViewOfSection failed, res: %x\n", res);
    }
#    endif
    return res;
}
#endif /* !NOT_DYNAMORIO_CORE_PROPER && !NOT_DYNAMORIO_CORE */

GET_RAW_SYSCALL(MapViewOfSection, IN HANDLE SectionHandle, IN HANDLE ProcessHandle,
                IN OUT PVOID *BaseAddress, IN ULONG_PTR ZeroBits, IN SIZE_T CommitSize,
                IN OUT PLARGE_INTEGER SectionOffset OPTIONAL, IN OUT PSIZE_T ViewSize,
                IN SECTION_INHERIT InheritDisposition, IN ULONG AllocationType,
                IN ULONG Protect);

GET_RAW_SYSCALL(OpenProcess, PHANDLE process_handle, ACCESS_MASK desired_access,
                POBJECT_ATTRIBUTES object_attributes, PCLIENT_ID client_id);

GET_RAW_SYSCALL(QueryFullAttributesFile, POBJECT_ATTRIBUTES object_attributes,
                PFILE_NETWORK_OPEN_INFORMATION file_information);

GET_RAW_SYSCALL(OpenThreadToken, HANDLE thread_handle, ACCESS_MASK desired_access,
                BOOLEAN open_as_self, PHANDLE token_handle);

GET_RAW_SYSCALL(OpenProcessToken, HANDLE process_handle, ACCESS_MASK desired_access,
                PHANDLE token_handle);

NTSTATUS
nt_raw_MapViewOfSection(HANDLE section_handle, HANDLE process_handle, PVOID *base_address,
                        ULONG_PTR zero_bits, SIZE_T commit_size,
                        PLARGE_INTEGER section_offset, PSIZE_T view_size,
                        SECTION_INHERIT inherit_disposition, ULONG allocation_type,
                        ULONG win32_protect)
{
    NTSTATUS res;
    res = NT_SYSCALL(MapViewOfSection, section_handle, process_handle, base_address,
                     zero_bits, commit_size, section_offset, view_size,
                     inherit_disposition, allocation_type, win32_protect);
#ifdef DEBUG
    if (!NT_SUCCESS(res)) {
        NTLOG(GLOBAL, LOG_NT, 1, "nt_raw_MapViewOfSection failed, res: %x\n", res);
    }
#endif
    return res;
}

NTSTATUS
nt_raw_OpenProcess(PHANDLE process_handle, ACCESS_MASK desired_access,
                   POBJECT_ATTRIBUTES object_attributes, PCLIENT_ID client_id)
{
    NTSTATUS res;
    res = NT_SYSCALL(OpenProcess, process_handle, desired_access, object_attributes,
                     client_id);
#ifdef DEBUG
    if (!NT_SUCCESS(res)) {
        NTLOG(GLOBAL, LOG_NT, 1, "nt_raw_OpenProcess failed, res: %x\n", res);
    }
#endif
    return res;
}

NTSTATUS
nt_raw_QueryFullAttributesFile(POBJECT_ATTRIBUTES object_attributes,
                               PFILE_NETWORK_OPEN_INFORMATION file_information)
{
    NTSTATUS res;
    res = NT_SYSCALL(QueryFullAttributesFile, object_attributes, file_information);
#ifdef DEBUG
    if (!NT_SUCCESS(res)) {
        NTLOG(GLOBAL, LOG_NT, 1, "nt_raw_QueryFullAttributesFile failed, res: %x\n", res);
    }
#endif
    return res;
}

NTSTATUS
nt_raw_CreateKey(PHANDLE key_handle, ACCESS_MASK desired_access,
                 POBJECT_ATTRIBUTES object_attributes, ULONG title_index,
                 PUNICODE_STRING class, ULONG create_options, PULONG disposition)
{
    NTSTATUS res;
    res = NT_SYSCALL(CreateKey, key_handle, desired_access, object_attributes,
                     title_index, class, create_options, disposition);
#ifdef DEBUG
    if (!NT_SUCCESS(res)) {
        NTLOG(GLOBAL, LOG_NT, 1, "nt_raw_CreateKey failed, res: %x\n", res);
    }
#endif
    return res;
}

NTSTATUS
nt_raw_OpenThreadToken(HANDLE thread_handle, ACCESS_MASK desired_access,
                       BOOLEAN open_as_self, PHANDLE token_handle)
{
    NTSTATUS res;
    res = NT_SYSCALL(OpenThreadToken, thread_handle, desired_access, open_as_self,
                     token_handle);
#ifdef DEBUG
    if (!NT_SUCCESS(res)) {
        NTLOG(GLOBAL, LOG_NT, 1, "nt_raw_OpenThreadToken failed, res: %x\n", res);
    }
#endif
    return res;
}

NTSTATUS
nt_raw_OpenProcessToken(HANDLE process_handle, ACCESS_MASK desired_access,
                        PHANDLE token_handle)
{
    NTSTATUS res;
    res = NT_SYSCALL(OpenProcessToken, process_handle, desired_access, token_handle);
#ifdef DEBUG
    if (!NT_SUCCESS(res)) {
        NTLOG(GLOBAL, LOG_NT, 1, "nt_raw_OpenProcessToken failed, res: %x\n", res);
    }
#endif
    return res;
}
