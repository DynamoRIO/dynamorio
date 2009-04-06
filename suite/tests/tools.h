/* **********************************************************
 * Copyright (c) 2003-2008 VMware, Inc.  All rights reserved.
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

#ifndef TOOLS_H
#define TOOLS_H

#include "configure.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h> /* memcpy */
#include <assert.h>

#ifdef LINUX
# include <sys/mman.h>
# include <stdlib.h> /* abort */
# include <errno.h>
#else
# include <windows.h>
# include <process.h> /* _beginthreadex */
# define NTSTATUS DWORD
# define NT_SUCCESS(status) (status >= 0)
#endif

#ifdef USE_DYNAMO
# include "dr_api.h"
#else
typedef unsigned int bool;
# ifdef LINUX
typedef unsigned int uint;
typedef unsigned long long int uint64;
typedef long long int int64;
# else
typedef unsigned __int64 uint64;
typedef __int64 int64;
#  define uint DWORD
# endif
# define PAGE_SIZE 0x00001000
#endif

#ifdef WINDOWS
# define IF_WINDOWS(x) x
# define IF_WINDOWS_ELSE(x,y) (x)
# ifndef USE_DYNAMO
#  define INT64_FORMAT "I64"
# endif
#else
# define IF_WINDOWS(x)
# define IF_WINDOWS_ELSE(x,y) (y)
# ifndef USE_DYNAMO
#  define INT64_FORMAT "ll"
# endif
#endif

/* some tests include dr_api.h and tools.h, so avoid duplicating */
#ifndef IF_X64
#  ifdef X64
   typedef uint64 ptr_uint_t;
   typedef int64 ptr_int_t;
#   define PFMT "%016"INT64_FORMAT"x"
#   define SZFMT "%"INT64_FORMAT"d"
#   define IF_X64(x) x
#   define IF_X64_ELSE(x, y) x
#  else
   typedef uint ptr_uint_t;
   typedef int ptr_int_t;
#   define PFMT "%08x"
#   define SZFMT "%d"
#   define IF_X64(x)
#   define IF_X64_ELSE(x, y) y
#  endif
#  define PFX "0x"PFMT
#endif

/* convenience macros for secure string buffer operations */
#define BUFFER_SIZE_BYTES(buf)      sizeof(buf)
#define BUFFER_SIZE_ELEMENTS(buf)   (BUFFER_SIZE_BYTES(buf) / sizeof(buf[0]))
#define BUFFER_LAST_ELEMENT(buf)    buf[BUFFER_SIZE_ELEMENTS(buf) - 1]
#define NULL_TERMINATE_BUFFER(buf)  BUFFER_LAST_ELEMENT(buf) = 0
#define BUFFER_ROOM_LEFT_W(wbuf)    (BUFFER_SIZE_ELEMENTS(wbuf) - wcslen(wbuf) - 1)
#define BUFFER_ROOM_LEFT(abuf)      (BUFFER_SIZE_ELEMENTS(abuf) - strlen(abuf) - 1)

#define PUSHF_MASK 0x00fcffff

#define ALLOW_READ   0x01
#define ALLOW_WRITE  0x02
#define ALLOW_EXEC   0x04

typedef enum {
    CODE_INC,
    CODE_DEC,
    CODE_SELF_MOD,
} Code_Snippet;

typedef enum {
    COPY_NORMAL,
    COPY_CROSS_PAGE,
} Copy_Mode;

#define ALIGN_FORWARD(x, alignment) ((((uint)x) + ((alignment)-1)) & (~((alignment)-1)))
#define ALIGN_BACKWARD(x, alignment) (((uint)x) & (~((alignment)-1)))

#ifndef true
# define true  (1)
# define false (0)
#endif

#if VERBOSE
# define VERBOSE_PRINT print
#else
/* FIXME: varargs for windows...for now since we don't care about efficiency we do this: */
static void VERBOSE_PRINT(char *fmt, ...) {}
#endif

#ifdef WINDOWS
/* FIXME: share w/ core/win32/os_exports.h */
# ifdef X64
#  define CXT_XIP Rip
#  define CXT_XAX Rax
#  define CXT_XCX Rcx
#  define CXT_XDX Rdx
#  define CXT_XBX Rbx
#  define CXT_XSP Rsp
#  define CXT_XBP Rbp
#  define CXT_XSI Rsi
#  define CXT_XDI Rdi
#  define CXT_XFLAGS EFlags
# else
#  define CXT_XIP Eip
#  define CXT_XAX Eax
#  define CXT_XCX Ecx
#  define CXT_XDX Edx
#  define CXT_XBX Ebx
#  define CXT_XSP Esp
#  define CXT_XBP Ebp
#  define CXT_XSI Esi
#  define CXT_XDI Edi
#  define CXT_XFLAGS EFlags
# endif
#endif

/* DynamoRIO prints to stderr, so we need to too to get right output, also need to flush since DynamoRIO writes directly bypassing the buffering */
static void
print(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fflush(stderr);
}

/* just to be sure */
#define printf do_not_use_printf__use_print

/* in tools_asm.asm */
int code_self_mod(int iters);
/* these don't need asm, but must be adjacent to code_self_mod and in order */
int code_inc(int foo);
int code_dec(int foo);
int dummy(void);

static size_t
size(Code_Snippet func)
{
    ptr_int_t val1 = 0, val2 = 0, ret_val;
    switch(func) {
    case CODE_INC:
        val1 = (ptr_int_t)code_inc;
        val2 = (ptr_int_t)code_dec;
        break;
    case CODE_DEC:
        val1 = (ptr_int_t)code_dec;
        val2 = (ptr_int_t)dummy;
        break;
    case CODE_SELF_MOD:
        val1 = (ptr_int_t)code_self_mod;
        val2 = (ptr_int_t)code_inc;
        break;
    default:
        return 0;
    }
    ret_val = val2 - val1;
    if (ret_val < 0) {
        print("Code layout assumption violation");
        return 0;
    }
    return ret_val;
}

static int
test(void *foo, int val)
{
    return (*(int (*) (int))foo)(val);
}

static int
call(Code_Snippet func, int val) 
{
    switch(func){
    case CODE_INC:
        return code_inc(val);
    case CODE_DEC:
        return code_dec(val);
    case CODE_SELF_MOD:
        return code_self_mod(val);
    default:
        print("Failed to find func to run\n");
    }
    return -1;
}

static char*
page_align(char *buf)
{
    return (char *)(((ptr_int_t)buf + PAGE_SIZE - 1) & ~(PAGE_SIZE-1));
}

static char*
copy_to_buf_normal(char *buf, int buf_len, int *copied_len, Code_Snippet func) 
{
    void *start;
    size_t len = size(func);
    switch(func) {
    case CODE_INC:
        start = (void *)code_inc;
        break;
    case CODE_DEC:
        start = (void *)code_dec;
        break;
    case CODE_SELF_MOD:
        start = (void *)code_self_mod;
        break;
    default:
        print("Failed to copy func\n");
    }
    if (len > (uint) buf_len) {
        print("Insufficient buffer for copy, have %d need %d\n", buf_len, len);
        len = buf_len;
    }
    memcpy(buf, start, len);
    if (copied_len != NULL) *copied_len = len;
    return buf;
}

static char*
copy_to_buf_cross_page(char *buf, int buf_len, int *copied_len, Code_Snippet func) 
{
    char* buf_back = buf;
    switch(func) {
    case CODE_INC:
    case CODE_DEC:
        buf = (char *) ((ptr_int_t)page_align(buf) + PAGE_SIZE - 0x02);
        buf_len = buf_len - ((ptr_int_t)buf - (ptr_int_t)buf_back) - PAGE_SIZE + 0x02;
        break;
    case CODE_SELF_MOD:
        buf = (char *) ((ptr_int_t)page_align(buf) + PAGE_SIZE - 0x10);
        buf_len = buf_len - ((ptr_int_t)buf - (ptr_int_t)buf_back) - PAGE_SIZE + 0x10;
        break;
    default:
        ;
    }
    return copy_to_buf_normal(buf, buf_len, copied_len, func);
}

static char*
copy_to_buf(char *buf, int buf_len, int *copied_len, Code_Snippet func, Copy_Mode mode) 
{
    switch(mode) {
    case COPY_NORMAL:
        return copy_to_buf_normal(buf, buf_len, copied_len, func);
    case COPY_CROSS_PAGE:
        return copy_to_buf_cross_page(buf, buf_len, copied_len, func);
    default:
        print("Improper copy mode\n");
    }
    *copied_len = 0;
    return buf;
}

#ifndef LINUX 
/****************************************************************************
 * ntdll.dll interface 
 * cleaner to use ntdll.lib but too lazy to add to build process
 *
 * WARNING: the Native API is an undocumented API and
 * could change without warning with a new version of Windows.
 */
static HANDLE ntdll_handle = NULL;

#define GET_PROC_ADDR(func, type, name) do { \
    if (ntdll_handle == NULL) \
        ntdll_handle = GetModuleHandle((LPCTSTR)"ntdll.dll"); \
    if (func == NULL) {                      \
        assert(ntdll_handle != NULL);        \
        func = (type) GetProcAddress(ntdll_handle, (LPCSTR)name); \
        assert(func != NULL);                \
    }                                        \
} while (0)

/* A wrapper to define kernel entry point in a static function */
/* In C use only at the end of a block prologue! */
#define GET_NTDLL(NtFunction, type)                             \
  typedef int (WINAPI *NtFunction##Type) type;                  \
  static NtFunction##Type NtFunction;                           \
  GET_PROC_ADDR(NtFunction, NtFunction##Type, #NtFunction);

/* returns 0 if flush performed ok, not-zero otherwise */
static int
NTFlush(char *buf, int len)
{
    NTSTATUS status;
    GET_NTDLL(NtFlushInstructionCache,
              (IN HANDLE ProcessHandle,
               IN PVOID BaseAddress OPTIONAL,
               IN ULONG FlushSize));
    status = NtFlushInstructionCache(GetCurrentProcess(), buf, len);
    if (!NT_SUCCESS(status)) {
        print("Error using NTFlush method\n");
    } else {
        return 0;
    }
    return -1;
}

typedef enum _PROCESSINFOCLASS {
    ProcessBasicInformation,
    ProcessQuotaLimits,
    ProcessIoCounters,
    ProcessVmCounters,
    ProcessTimes,
    ProcessBasePriority,
    ProcessRaisePriority,
    ProcessDebugPort,
    ProcessExceptionPort,
    ProcessAccessToken,
    ProcessLdtInformation,
    ProcessLdtSize,
    ProcessDefaultHardErrorMode,
    ProcessIoPortHandlers,          // Note: this is kernel mode only
    ProcessPooledUsageAndLimits,
    ProcessWorkingSetWatch,
    ProcessUserModeIOPL,
    ProcessEnableAlignmentFaultFixup,
    ProcessPriorityClass,
    ProcessWx86Information,
    ProcessHandleCount,
    ProcessAffinityMask,
    ProcessPriorityBoost,
    ProcessDeviceMap,
    ProcessSessionInformation,
    ProcessForegroundInformation,
    ProcessWow64Information,
    MaxProcessInfoClass
} PROCESSINFOCLASS;

/* format of data returned by QueryInformationProcess ProcessVmCounters */
typedef struct _VM_COUNTERS {
    SIZE_T PeakVirtualSize;
    SIZE_T VirtualSize;
    ULONG PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage;
    SIZE_T QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
} VM_COUNTERS;

static int
get_process_mem_stats(HANDLE h, VM_COUNTERS *info)
{
    int i, len = 0;
    /* could share w/ other process info routines... */
    GET_NTDLL(NtQueryInformationProcess,
              (IN HANDLE ProcessHandle,
               IN PROCESSINFOCLASS ProcessInformationClass,
               OUT PVOID ProcessInformation,
               IN ULONG ProcessInformationLength,
               OUT PULONG ReturnLength OPTIONAL));
    i = NtQueryInformationProcess((HANDLE) h, ProcessVmCounters,
                                  info, sizeof(VM_COUNTERS), &len);
    if (i != 0) {
        /* function failed */
        memset(info, 0, sizeof(VM_COUNTERS));
        return 0;
    } else
        assert(len == sizeof(VM_COUNTERS));
    return 1;
}
#endif

static int
get_os_prot_word(int prot) 
{
#ifdef LINUX
    return (((prot & ALLOW_READ) != 0) ? PROT_READ : 0) |
        (((prot & ALLOW_WRITE) != 0) ? PROT_WRITE : 0) |
        (((prot & ALLOW_EXEC) != 0) ? PROT_EXEC : 0);
#else
    if ((prot & ALLOW_WRITE) != 0) {
        if ((prot & ALLOW_EXEC) != 0) {
            return PAGE_EXECUTE_READWRITE;
        } else {
            return PAGE_READWRITE;
        }
    } else {
        if ((prot & ALLOW_READ) != 0) {
            if ((prot & ALLOW_EXEC) != 0) {
                return PAGE_EXECUTE_READ;
            } else {
                return PAGE_READONLY;
            }
        } else {
            if ((prot & ALLOW_EXEC) != 0) {
                return PAGE_EXECUTE;
            } else {
                return PAGE_NOACCESS;
            }
        }
    }
#endif
}

static char *
allocate_mem(int size, int prot) 
{
#ifdef LINUX
    return (char *) mmap((void *)0, size, get_os_prot_word(prot), MAP_PRIVATE|MAP_ANON, 0, 0);
#else
    return (char *) VirtualAlloc(NULL, size, MEM_COMMIT, get_os_prot_word(prot));
#endif
}

static void
protect_mem(void *start, int len, int prot) 
{
#ifdef LINUX
    void *page_start = (void *)(((ptr_int_t)start) & ~(PAGE_SIZE -1));
    int page_len = (len + ((ptr_int_t)start - (ptr_int_t)page_start) + PAGE_SIZE - 1)
        & ~(PAGE_SIZE - 1);
    if (mprotect(page_start, page_len, get_os_prot_word(prot)) != 0) {
        print("Error on mprotect: %d\n", errno);
    }
#else
    int old;
    if (VirtualProtect(start, len, get_os_prot_word(prot), &old) == 0) {
        print("Error on VirtualProtect\n");
    }
#endif
}

static void
protect_mem_check(void *start, int len, int prot, int expected)
{
#ifdef LINUX 
    /* FIXME : add check */
    protect_mem(start, len, prot);
#else 
    int old;
    if (VirtualProtect(start, len, get_os_prot_word(prot), &old) == 0) {
        print("Error on VirtualProtect\n");
    }
    if (old != get_os_prot_word(expected)) {
        print("Unexpected previous permissions\n");
    }
#endif
}

void *
reserve_memory(int size);

static void
test_print(void *buf, int n) 
{
    print("%d\n", test(buf, n));
}

#ifdef LINUX
# define USE_USER32()
# ifdef NEED_HANDLER
#  include <unistd.h>
#  include <signal.h>
#  include <ucontext.h>
#  include <errno.h>
#  define INIT() intercept_signal(SIGSEGV, signal_handler)

/* just use single-arg handlers */
typedef void (*handler_t)(int);
typedef void (*handler_3_t)(int, struct siginfo *, void *);

static void
signal_handler(int sig)
{
    if (sig == SIGSEGV) {
        print("Unhandled exception caught.\n");
    } else {
        print("ERROR: Unexpected signal %d caught\n", sig);
    }
    exit(-1);
}

#define ASSERT_NOERR(rc) do {                               \
  if (rc) {                                                 \
     print("%s:%d rc=%d errno=%d %s\n", __FILE__, __LINE__, \
           rc, errno, strerror(errno));                     \
  }                                                         \
} while (0);

/* set up signal_handler as the handler for signal "sig" */
static void
intercept_signal(int sig, handler_t handler)
{
    int rc;
    struct sigaction act;

    act.sa_sigaction = (handler_3_t) handler;
    /* FIXME: due to DR bug 840 we cannot block ourself in the handler
     * since the handler does not end in a sigreturn, so we have an empty mask
     * and we use SA_NOMASK
     */
    rc = sigemptyset(&act.sa_mask); /* block no signals within handler */
    ASSERT_NOERR(rc);
    /* FIXME: due to DR bug #654 we use SA_SIGINFO -- change it once DR works */
    act.sa_flags = SA_NOMASK | SA_SIGINFO | SA_ONSTACK;
    
    /* arm the signal */
    rc = sigaction(sig, &act, NULL);
    ASSERT_NOERR(rc);
}
# else
#  define INIT()
# endif /* NEED_HANDLER */
#else
#  define USE_USER32() do { if (argc > 5) MessageBeep(0); } while (0)

#  define INIT() set_global_filter()

# define WINDOWS_VERSION_VISTA  60
# define WINDOWS_VERSION_2003   52
# define WINDOWS_VERSION_XP     51
# define WINDOWS_VERSION_2000   50
# define WINDOWS_VERSION_NT     40

/* returns 0 on failure */
int
get_windows_version();

static LONG WINAPI 
our_exception_filter(struct _EXCEPTION_POINTERS * pExceptionInfo)
{
    /* use EXCEPTION_CONTINUE_SEARCH to let it go all the way  */
    if (pExceptionInfo->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION) {
        print("ERROR: Unexpected exception 0x%x caught\n", pExceptionInfo->ExceptionRecord->ExceptionCode);
    }
    print("Unhandled exception caught.\n");
    return EXCEPTION_EXECUTE_HANDLER;
}

static
set_global_filter()
{
    // Set the global unhandled exception filter to the exception filter
    SetUnhandledExceptionFilter(our_exception_filter);
}

#endif /* LINUX */

#ifdef LINUX
typedef int thread_handle;
typedef unsigned int (*fptr)(void *);
#else
typedef HANDLE thread_handle;
typedef unsigned int (__stdcall *fptr)(void *);
#endif

/* Thread related functions */
thread_handle
create_thread(fptr f);
void
suspend_thread(thread_handle th);

void
resume_thread(thread_handle th);

void
join_thread(thread_handle th);

void
thread_yield();

#ifdef WINDOWS
static byte *
get_drmarker_field(uint offset)
{
    /* read DR marker
     * just hardcode the offsets for now
     */
    byte *field;
    HANDLE ntdll_handle = GetModuleHandle("ntdll.dll");
    byte *cbd = (byte *) GetProcAddress(ntdll_handle, "KiUserCallbackDispatcher");
    byte *drmarker, *landing_pad;
    if (*cbd != 0xe9) /* no jmp there */
        return NULL;

    /* see win32/callback.c:emit_landing_pad_code() for details */
    landing_pad = *((int *)(cbd+1)) /* skip jmp opcode */
        + cbd + 5; /* relative */
#ifdef X64
    drmarker = *((byte **)(landing_pad-8)); /* skip jmp opcode */
#else
    drmarker = *((byte **)(landing_pad+1)) /* skip jmp opcode */
        + (uint)landing_pad + 5; /* relative */
#endif
    drmarker = (byte *) ALIGN_BACKWARD((uint)drmarker, PAGE_SIZE);
    /* FIXME: check magic fields */
    field = *((byte **)(drmarker + offset));
    return field;
}

/* For hot patch testing only.  Only defined for Windows - hot patching hasn't
 * been implemented for Linux yet.
 * TODO: need to parameterize it with number or string to annotate a patch
 *      point - needed to handle multiple patch points automatically for 
 *      defs.cfg generation; another option would be to maintain a counter 
 *      that increments with each use of the macro.
 */
#define INSERT_HOTP_PATCH_POINT()   \
__asm {             \
    __asm jmp foo   \
    __asm _emit '$' \
    __asm _emit 'p' \
    __asm _emit 'p' \
    __asm _emit 'o' \
    __asm _emit 'i' \
    __asm _emit 'n' \
    __asm _emit 't' \
    __asm _emit '$' \
    __asm foo:      \
}
#endif

#endif /* TOOLS_H */
