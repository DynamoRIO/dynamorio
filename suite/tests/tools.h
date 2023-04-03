/* **********************************************************
 * Copyright (c) 2011-2023 Google, Inc.  All rights reserved.
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

#ifndef TOOLS_H
#define TOOLS_H

/* i#1424: avoid pulling in features from recent versions to keep compatibility.
 * The core may still support 2K but officially we only support XP+.
 * Some tests need 0x0600, in particular condvar.h.
 */
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600

#define _GNU_SOURCE 1 /* for REG_RIP, etc. */
#include "configure.h"
#include "drlibc.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h> /* memcpy */
#include <assert.h>

#ifdef UNIX
#    include <sys/mman.h>
#    include <stdlib.h> /* abort */
#    include <errno.h>
#    include <signal.h>
#    ifdef MACOS
#        define _XOPEN_SOURCE \
            700 /* required to get POSIX, etc. defines out of ucontext.h */
#        define __need_struct_ucontext64 /* seems to be missing from Mac headers */
#    endif
#    include <ucontext.h>
#    include <unistd.h>
#    include "../../core/unix/os_public.h"
#    ifdef X64
/* XCode 10.1 (probably others too) toolchain wants _STRUCT_MCONTEXT
 * w/o _AVX64 and has a field named uc_mcontext with no 64.
 */
#        undef _STRUCT_MCONTEXT_AVX64
#        define _STRUCT_MCONTEXT_AVX64 _STRUCT_MCONTEXT64
#        define uc_mcontext64 uc_mcontext
#    endif
#else
#    include <windows.h>
#    include <process.h> /* _beginthreadex */
#    include <stdlib.h>  /* _set_error_mode */
#    if defined(DEBUG) && !defined(NO_DBG_CRT)
#        include <crtdbg.h>
#    endif
#    include "../../core/win32/os_public.h"
#    define NTSTATUS DWORD
#    define NT_SUCCESS(status) (status >= 0)
#endif

/* Ensure we get 0x, lower-case, and leading zeroes when not using DR's printf.  This
 * is a tradeoff: we get consistent style for golden output matching, but we have
 * reduced error detection from format string type checks by the compiler.
 */
#undef PFX
#define PFX "0x" PFMT

#ifdef LINUX
#    include <linux/version.h>
#    if defined(AARCH64) && LINUX_VERSION_CODE < KERNEL_VERSION(4, 3, 0)
/* SIGSTKSZ was incorrectly defined in Linux releases before 4.3. */
#        undef SIGSTKSZ
#        define SIGSTKSZ 16384
#    endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Get the defines for "true" and "false" w/o messing up clang-format. */
#define DR_DO_NOT_DEFINE_bool
#include "c_defines.h"

/* check if all bits in mask are set in var */
#define TESTALL(mask, var) (((mask) & (var)) == (mask))
/* check if any bit in mask is set in var */
#define TESTANY(mask, var) (((mask) & (var)) != 0)
/* check if a single bit is set in var */
#define TEST TESTANY

#ifdef USE_DYNAMO
/* to avoid non-api tests depending on dr_api headers we rely on test
 * including dr_api.h before tools.h (though then must include
 * configure.h before either)
 */
#    ifndef _DR_API_H_
#        error "must include dr_api.h before tools.h"
#    endif
#endif

#ifdef UNIX
/* Forward decl for nanosleep. */
struct timespec;

bool
find_dynamo_library(void);

/* Staticly linked versions of libc routines that don't touch globals or errno.
 */
void
nolibc_print(const char *str);
void
nolibc_print_int(int d);
void
nolibc_nanosleep(struct timespec *req);
int
nolibc_strlen(const char *str);
void *
nolibc_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int
nolibc_munmap(void *addr, size_t length);
void
nolibc_memset(void *dst, int val, size_t size);
#endif

/* Ignore any PAGE_SIZE provided by the tool chain. */
#undef PAGE_SIZE
#define PAGE_SIZE page_size()

#ifdef UNIX
/* This is a slightly simplified version of dr_page_size. Performance hardly matters
 * here. We cannot use dr_page_size as this header is also used without DynamoRIO's API.
 */

/* Return true if size is a multiple of the page size. */
static bool
try_page_size(size_t size)
{
    byte *addr = (byte *)nolibc_mmap(NULL, size * 2, PROT_NONE,
                                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ((ptr_uint_t)addr >= (ptr_uint_t)-4096) /* mmap failed: should not happen */
        return false;
    if (nolibc_munmap(addr + size, size) == 0) {
        /* munmap of top half succeeded: munmap bottom half and return true */
        nolibc_munmap(addr, size);
        return true;
    }
    /* munmap of top half failed: munmap whole region and return false */
    nolibc_munmap(addr, size * 2);
    return false;
}

/* Directly determine the granularity of memory allocation using mmap and munmap. */
static size_t
find_page_size(void)
{
    size_t size = 4096;
    if (try_page_size(size)) {
        /* Try smaller sizes. */
        for (size /= 2; size > 0; size /= 2) {
            if (!try_page_size(size))
                return size * 2;
        }
    } else {
        /* Try larger sizes. */
        for (size *= 2; size * 2 > 0; size *= 2) {
            if (try_page_size(size))
                return size;
        }
    }
    /* Something went wrong... */
    return 4096;
}
#endif

static size_t
page_size(void)
{
#ifdef UNIX
    static size_t cached_page_size = 0;
    size_t size = cached_page_size; /* atomic read */
    if (size == 0) {
        size = find_page_size();
        cached_page_size = size; /* atomic write */
    }
    return size;
#else
    /* FIXME i#1680: On Windows determine page size using system call. */
    return 4096;
#endif
}

/* Some tests want to define a static array that contains a whole page. This
 * should be large enough.
 */
#define PAGE_SIZE_MAX (64 * 1024)

#ifdef WINDOWS
#    define IF_WINDOWS(x) x
#    define IF_WINDOWS_ELSE(x, y) (x)
#else
#    define IF_WINDOWS(x)
#    define IF_WINDOWS_ELSE(x, y) (y)
#endif

/* Function attributes. */
#ifdef WINDOWS
#    define EXPORT __declspec(dllexport)
#    define IMPORT __declspec(dllimport)
#    define NOINLINE __declspec(noinline)
#else /* UNIX */
#    define EXPORT __attribute__((visibility("default")))
#    define IMPORT extern
#    define NOINLINE __attribute__((noinline))
#endif

/* convenience macros for secure string buffer operations */
#define BUFFER_SIZE_BYTES(buf) sizeof(buf)
#define BUFFER_SIZE_ELEMENTS(buf) (BUFFER_SIZE_BYTES(buf) / sizeof(buf[0]))
#define BUFFER_LAST_ELEMENT(buf) buf[BUFFER_SIZE_ELEMENTS(buf) - 1]
#define NULL_TERMINATE_BUFFER(buf) BUFFER_LAST_ELEMENT(buf) = 0
#define BUFFER_ROOM_LEFT_W(wbuf) (BUFFER_SIZE_ELEMENTS(wbuf) - wcslen(wbuf) - 1)
#define BUFFER_ROOM_LEFT(abuf) (BUFFER_SIZE_ELEMENTS(abuf) - strlen(abuf) - 1)

#define PUSHF_MASK 0x00fcffff

#define ALLOW_READ 0x01
#define ALLOW_WRITE 0x02
#define ALLOW_EXEC 0x04

typedef enum {
    CODE_INC,
    CODE_DEC,
    CODE_SELF_MOD,
} Code_Snippet;

typedef enum {
    COPY_NORMAL,
    COPY_CROSS_PAGE,
} Copy_Mode;

#define ALIGN_BACKWARD(x, alignment) (((ptr_uint_t)x) & (~((ptr_uint_t)(alignment)-1)))
#define ALIGN_FORWARD(x, alignment) \
    ((((ptr_uint_t)x) + (((ptr_uint_t)alignment) - 1)) & (~(((ptr_uint_t)alignment) - 1)))
#define ALIGNED(x, alignment) ((((ptr_uint_t)x) & ((alignment)-1)) == 0)

#ifdef UNIX
#    ifndef MAP_ANONYMOUS
#        define MAP_ANONYMOUS MAP_ANON /* MAP_ANON on Mac */
#    endif
#endif

#if VERBOSE
#    define VERBOSE_PRINT print
#else
/* FIXME: varargs for windows...for now since we don't care about efficiency we do this:
 */
static inline void
VERBOSE_PRINT(const char *fmt, ...)
{
}
#endif

#ifdef UNIX
#    define ASSERT_NOERR(rc)                                                      \
        do {                                                                      \
            if (rc) {                                                             \
                print("%s:%d rc=%d errno=%d %s\n", __FILE__, __LINE__, rc, errno, \
                      strerror(errno));                                           \
            }                                                                     \
        } while (0);

typedef void (*handler_1_t)(int);
typedef void (*handler_3_t)(int, siginfo_t *, ucontext_t *);

/* set up signal_handler as the handler for signal "sig" */
void
intercept_signal(int sig, handler_3_t handler, bool sigstack);
#endif

/* for cross-plaform siglongjmp */
#ifdef UNIX
#    define SIGJMP_BUF sigjmp_buf
#    define SIGSETJMP(buf) sigsetjmp(buf, 1)
#    define SIGLONGJMP(buf, count) siglongjmp(buf, count)
#else
#    define SIGJMP_BUF jmp_buf
#    define SIGSETJMP(buf) setjmp(buf)
#    define SIGLONGJMP(buf, count) longjmp(buf, count)
#endif

#ifdef WINDOWS
#    define NOP __nop()
#    define NOP_NOP_NOP \
        __nop();        \
        __nop();        \
        __nop()
#    define NOP_NOP_CALL(tgt) \
        __nop();              \
        __nop();              \
        tgt()
#else /* UNIX */
#    define NOP asm("nop")
#    define NOP_NOP_NOP asm("nop\n nop\n nop\n")
#    ifdef X86
#        ifdef MACOS
#            define NOP_NOP_CALL(tgt) asm("nop\n nop\n call _" #            tgt)
#        else
#            define NOP_NOP_CALL(tgt) asm("nop\n nop\n call " #            tgt)
#        endif
#    elif defined(AARCHXX)
/* Make sure to mark LR/X30 as clobbered to avoid functions like
 * client-interface/call-retarget.c:main() being interpreted as a leaf
 * function that does not need the link register preserved.
 */
#        ifdef MACOS
#            define NOP_NOP_CALL(tgt) \
                asm("nop\n nop\n bl _" #tgt : : : IF_ARM_ELSE("lr", "x30"))
#        else
#            define NOP_NOP_CALL(tgt) \
                asm("nop\n nop\n bl " #tgt : : : IF_ARM_ELSE("lr", "x30"))
#        endif
#    endif
#endif

/* DynamoRIO prints directly by syscall to stderr, so we need to too to get
 * right output, esp. with ctest -j where fprintf(stderr) is buffered.
 * Likely to crash if the stack is unaligned due to possible floating point args
 * in XMM registers.
 */
void
print(const char *fmt, ...);

/* just to be sure */
#define printf do_not_use_printf__use_print

/* in tools_asm.asm */
int
code_self_mod(int iters);
void
icache_sync(void *addr);
/* these don't need asm, but must be adjacent to code_self_mod and in order */
int
code_inc(int foo);
int
code_dec(int foo);
int
dummy(void);
#ifdef DR_HOST_NOT_TARGET
static inline void
tools_clear_icache(void *start, void *end)
{
    /* We need this function to build, but we expect to never run it for host!=target
     * (an artifact of imperfect DR modularity for managed execution vs utility
     * library: i#1684, etc.).
     */
    assert(false);
}
#elif defined(AARCHXX)
/* In tools.c asm code. */
void
tools_clear_icache(void *start, void *end);
#endif

/* This function implements a trampoline that portably gets its return address
 * and calls its first argument, which is a function pointer, with a pointer
 * to the return address substituted for the first argument.  All
 * other parameters are untouched.  It can be used like so:
 *
 * void bar(void);
 * void foo(void **myretaddr, void *otherfunc) {
 *     *myretaddr = (void*)otherfunc;
 * }
 * int main(void) {
 *     call_with_retaddr((void*)foo, bar);
 * }
 *
 * Which will cause foo to return to bar.  This is useful in security tests
 * that want to overwrite their return address.
 */
int
call_with_retaddr(void *func, ...);

/* This function implements a trampoline that portably gets its return address
 * and tailcalls to its first argument, which is a function pointer, with the
 * return address substituted for the first argument.  All
 * other parameters are untouched.  It can be used like so:
 *
 * void foo(void *myretaddr, int num) {
 *     printf("Return address is %p, second arg is %d\n", myretaddr, num);
 * }
 * int main(void) {
 *     tailcall_with_retaddr((void*)foo, 123);
 * }
 */
int
tailcall_with_retaddr(void *func, ...);

static size_t
size(Code_Snippet func)
{
    ptr_int_t val1 = 0, val2 = 0, ret_val;
    switch (func) {
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
    default: return 0;
    }
    /* support ILT indirection where these are jmps to the real code
     * (adding /debug for i#567 causes ILT usage == table of jmps)
     */
    if (*(unsigned char *)val1 == 0xe9 /*jmp*/)
        val1 = val1 + 5 + *(int *)(val1 + 1); /* resolve jmp target */
    if (*(unsigned char *)val2 == 0xe9 /*jmp*/)
        val2 = val2 + 5 + *(int *)(val2 + 1); /* resolve jmp target */
    ret_val = val2 - val1;
    if (ret_val < 0) {
        print("Code layout assumption violation");
        return 0;
    }
    return ret_val;
}

static inline int
test(void *foo, int val)
{
    return (*(int (*)(int))foo)(val);
}

static inline int
call(Code_Snippet func, int val)
{
    switch (func) {
    case CODE_INC: return code_inc(val);
    case CODE_DEC: return code_dec(val);
    case CODE_SELF_MOD: return code_self_mod(val);
    default: print("Failed to find func to run\n");
    }
    return -1;
}

static char *
page_align(char *buf)
{
    return (char *)(((ptr_int_t)buf + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));
}

static char *
copy_to_buf_normal(char *buf, size_t buf_len, size_t *copied_len, Code_Snippet func)
{
    void *start;
    size_t len = size(func);
    switch (func) {
    case CODE_INC: start = (void *)code_inc; break;
    case CODE_DEC: start = (void *)code_dec; break;
    case CODE_SELF_MOD: start = (void *)code_self_mod; break;
    default: print("Failed to copy func\n");
    }
    if (*(unsigned char *)start == 0xe9 /*jmp*/) {
        /* handle ILT indirection by resolving jmp target */
        start = (unsigned char *)start + 5 + *(int *)((unsigned char *)start + 1);
    }
    if (len > buf_len) {
        print("Insufficient buffer for copy, have %d need %d\n", buf_len, len);
        len = buf_len;
    }
    memcpy(buf, start, len);
#if defined(LINUX) && defined(AARCHXX)
    tools_clear_icache(buf, buf + len);
#endif
    if (copied_len != NULL)
        *copied_len = len;
    return buf;
}

static char *
copy_to_buf_cross_page(char *buf, size_t buf_len, size_t *copied_len, Code_Snippet func)
{
    char *buf_back = buf;
    switch (func) {
    case CODE_INC:
    case CODE_DEC:
        buf = (char *)((ptr_int_t)page_align(buf) + PAGE_SIZE - 0x02);
        buf_len = buf_len - ((ptr_int_t)buf - (ptr_int_t)buf_back) - PAGE_SIZE + 0x02;
        break;
    case CODE_SELF_MOD:
        buf = (char *)((ptr_int_t)page_align(buf) + PAGE_SIZE - 0x10);
        buf_len = buf_len - ((ptr_int_t)buf - (ptr_int_t)buf_back) - PAGE_SIZE + 0x10;
        break;
    default:;
    }
    return copy_to_buf_normal(buf, buf_len, copied_len, func);
}

static inline char *
copy_to_buf(char *buf, size_t buf_len, size_t *copied_len, Code_Snippet func,
            Copy_Mode mode)
{
    switch (mode) {
    case COPY_NORMAL: return copy_to_buf_normal(buf, buf_len, copied_len, func);
    case COPY_CROSS_PAGE: return copy_to_buf_cross_page(buf, buf_len, copied_len, func);
    default: print("Improper copy mode\n");
    }
    *copied_len = 0;
    return buf;
}

#ifndef UNIX
/****************************************************************************
 * ntdll.dll interface
 * cleaner to use ntdll.lib but too lazy to add to build process
 *
 * WARNING: the Native API is an undocumented API and
 * could change without warning with a new version of Windows.
 */
static HANDLE ntdll_handle = NULL;

#    define GET_PROC_ADDR(func, type, name)                                       \
        do {                                                                      \
            if (ntdll_handle == NULL)                                             \
                ntdll_handle = GetModuleHandle((LPCTSTR) "ntdll.dll");            \
            if (func == NULL) {                                                   \
                assert(ntdll_handle != NULL);                                     \
                func = (type)GetProcAddress((HMODULE)ntdll_handle, (LPCSTR)name); \
                assert(func != NULL);                                             \
            }                                                                     \
        } while (0)

/* A wrapper to define kernel entry point in a static function */
/* In C use only at the end of a block prologue! */
#    define GET_NTDLL(NtFunction, type)              \
        typedef int(WINAPI * NtFunction##Type) type; \
        static NtFunction##Type NtFunction;          \
        GET_PROC_ADDR(NtFunction, NtFunction##Type, #NtFunction);

/* returns 0 if flush performed ok, not-zero otherwise */
static int
NTFlush(char *buf, size_t len)
{
    NTSTATUS status;
    GET_NTDLL(
        NtFlushInstructionCache,
        (IN HANDLE ProcessHandle, IN PVOID BaseAddress OPTIONAL, IN SIZE_T FlushSize));
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
    ProcessIoPortHandlers, // Note: this is kernel mode only
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
    int i;
    ULONG len = 0;
    /* could share w/ other process info routines... */
    GET_NTDLL(NtQueryInformationProcess,
              (IN HANDLE ProcessHandle, IN PROCESSINFOCLASS ProcessInformationClass,
               OUT PVOID ProcessInformation, IN ULONG ProcessInformationLength,
               OUT PULONG ReturnLength OPTIONAL));
    i = NtQueryInformationProcess((HANDLE)h, ProcessVmCounters, info, sizeof(VM_COUNTERS),
                                  &len);
    if (i != 0) {
        /* function failed */
        memset(info, 0, sizeof(VM_COUNTERS));
        return 0;
    } else
        assert(len == sizeof(VM_COUNTERS));
    return 1;
}
#endif

int
get_os_prot_word(int prot);

char *
allocate_mem(size_t size, int prot);

void
free_mem(char *addr, size_t size);

void
protect_mem(void *start, size_t len, int prot);

void
protect_mem_check(void *start, size_t len, int prot, int expected);

void *
reserve_memory(int size);

static inline void
test_print(void *buf, int n)
{
    print("%d\n", test(buf, n));
}

#define INIT()                                \
    do {                                      \
        assert(page_size() <= PAGE_SIZE_MAX); \
        OS_INIT();                            \
    } while (0)

#ifdef UNIX
#    define USE_USER32()
#    ifdef NEED_HANDLER
#        define OS_INIT() intercept_signal(SIGSEGV, (handler_3_t)signal_handler, false)

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
#    else
#        define OS_INIT()
#    endif /* NEED_HANDLER */
#else
#    define USE_USER32()        \
        do {                    \
            if (argc > 5)       \
                MessageBeep(0); \
        } while (0)

/* We can't put this in tools.c b/c we have some tests that link /MT even in
 * debug build.
 */
#    if defined(DEBUG) && !defined(NO_DBG_CRT)
#        define DISABLE_POPUPS()                                                      \
            do {                                                                      \
                /* Avoid pop-up messageboxes in tests. */                             \
                if (!IsDebuggerPresent()) {                                           \
                    /* Control CRT-internal asserts and _ASSERT, etc. */              \
                    /* Set for _CRT_{WARN,ERROR,ASSERT}. */                           \
                    for (int i = 0; i < _CRT_ERRCNT; i++) {                           \
                        _CrtSetReportMode(i, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG); \
                        _CrtSetReportFile(i, _CRTDBG_FILE_STDERR);                    \
                    }                                                                 \
                    /* This may control assert() and _wassert() in release build. */  \
                    _set_error_mode(_OUT_TO_STDERR);                                  \
                }                                                                     \
            } while (0)
#    else
#        define DISABLE_POPUPS() /* Nothing. */
#    endif

#    define OS_INIT()            \
        do {                     \
            DISABLE_POPUPS();    \
            set_global_filter(); \
        } while (0)

/* XXX: when updating here, update core/os_exports.h too */
#    define WINDOWS_VERSION_10_1803 105
#    define WINDOWS_VERSION_10_1709 104
#    define WINDOWS_VERSION_10_1703 103
#    define WINDOWS_VERSION_10_1607 102
#    define WINDOWS_VERSION_10_1511 101
#    define WINDOWS_VERSION_10 100
#    define WINDOWS_VERSION_8_1 63
#    define WINDOWS_VERSION_8 62
#    define WINDOWS_VERSION_7 61
#    define WINDOWS_VERSION_VISTA 60
#    define WINDOWS_VERSION_2003 52
#    define WINDOWS_VERSION_XP 51
#    define WINDOWS_VERSION_2000 50
#    define WINDOWS_VERSION_NT 40

/* returns 0 on failure */
int
get_windows_version(void);

bool
is_wow64(HANDLE hProcess);

static LONG WINAPI
our_exception_filter(struct _EXCEPTION_POINTERS *pExceptionInfo)
{
    /* use EXCEPTION_CONTINUE_SEARCH to let it go all the way  */
    if (pExceptionInfo->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION) {
        print("ERROR: Unexpected exception 0x%x caught\n",
              pExceptionInfo->ExceptionRecord->ExceptionCode);
    }
    print("Unhandled exception caught.\n");
    return EXCEPTION_EXECUTE_HANDLER;
}

static void
set_global_filter()
{
    // Set the global unhandled exception filter to the exception filter
    SetUnhandledExceptionFilter(our_exception_filter);
}

#endif /* UNIX */

#ifdef WINDOWS
static byte *
get_drmarker_field(uint offset)
{
    /* read DR marker
     * just hardcode the offsets for now
     */
    byte *field;
    HANDLE ntdll_handle = GetModuleHandle("ntdll.dll");
    byte *cbd = (byte *)GetProcAddress((HMODULE)ntdll_handle, "KiUserCallbackDispatcher");
    byte *drmarker, *landing_pad;
    if (*cbd != 0xe9) /* no jmp there */
        return NULL;

    /* see win32/callback.c:emit_landing_pad_code() for details */
    landing_pad = *((int *)(cbd + 1)) /* skip jmp opcode */
        + cbd + 5;                    /* relative */
#    ifdef X64
    drmarker = *((byte **)(landing_pad - 8)); /* skip jmp opcode */
#    else
    drmarker = *((byte **)(landing_pad + 1)) /* skip jmp opcode */
        + (uint)landing_pad + 5;             /* relative */
#    endif
    drmarker = (byte *)ALIGN_BACKWARD((ptr_uint_t)drmarker, PAGE_SIZE);
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
#    define INSERT_HOTP_PATCH_POINT() \
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
    __asm foo:         \
        }
#endif

#ifdef __cplusplus
}
#endif

static inline bool
my_setenv(const char *var, const char *value)
{
#ifdef UNIX
    return setenv(var, value, 1 /*override*/) == 0;
#else
    return SetEnvironmentVariable(var, value) == TRUE;
#endif
}

static inline bool
my_getenv(const char *var, char *dest, size_t size)
{
#ifdef UNIX
    const char *value = getenv(var);
    if (value == NULL)
        return false;
    strncpy(dest, value, size);
    dest[size - 1] = 0;
    return true;
#else
    unsigned int ret = GetEnvironmentVariable(var, dest, (DWORD)size);
    if (ret == 0) {
        fprintf(stderr, "Env variable %s returned 0 (not found?)\n", var);
    } else if (ret > size) {
        fprintf(stderr, "Env variable %s needs %u bytes of space!\n", var, ret);
    }
    return ret > 0 && ret <= size;
#endif
}

#endif /* TOOLS_H */
