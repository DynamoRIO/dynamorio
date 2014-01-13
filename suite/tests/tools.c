/* **********************************************************
 * Copyright (c) 2013-2014 Google, Inc.  All rights reserved.
 * Copyright (c) 2005-2010 VMware, Inc.  All rights reserved.
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

#ifndef ASM_CODE_ONLY /* C code */
#include "tools.h"

#ifdef UNIX
# include <unistd.h>
# include <sys/syscall.h> /* for SYS_* numbers */
#endif

#define ASSERT_NOT_IMPLEMENTED() do { print("NYI\n"); abort();} while (0)

#ifdef WINDOWS
/* returns 0 on failure */
/* FIXME - share with core get_os_version() */
int
get_windows_version(void)
{
    OSVERSIONINFO version;
    int res;
    version.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    res = GetVersionEx(&version);
    assert(res != 0);
    if (version.dwPlatformId == VER_PLATFORM_WIN32_NT) {
        /* WinNT or descendents */
        if (version.dwMajorVersion == 6 && version.dwMinorVersion == 3) {
            return WINDOWS_VERSION_8_1;
        } else if (version.dwMajorVersion == 6 && version.dwMinorVersion == 2) {
            return WINDOWS_VERSION_8;
        } else if (version.dwMajorVersion == 6 && version.dwMinorVersion == 1) {
            return WINDOWS_VERSION_7;
        } else if (version.dwMajorVersion == 6 && version.dwMinorVersion == 0) {
            return WINDOWS_VERSION_VISTA;
        } else if (version.dwMajorVersion == 5 && version.dwMinorVersion == 2) {
            return WINDOWS_VERSION_2003;
        } else if (version.dwMajorVersion == 5 && version.dwMinorVersion == 1) {
            return WINDOWS_VERSION_XP;
        } else if (version.dwMajorVersion == 5 && version.dwMinorVersion == 0) {
            return WINDOWS_VERSION_2000;
        } else if (version.dwMajorVersion == 4) {
            return WINDOWS_VERSION_NT;
        }
    }
    return 0;
}

/* FIXME: share w/ libutil is_wow64() */
bool
is_wow64(HANDLE hProcess)
{
    /* IsWow64Pocess is only available on XP+ */
    typedef DWORD (WINAPI *IsWow64Process_Type)(HANDLE hProcess,
                                                PBOOL isWow64Process);
    static HANDLE kernel32_handle;
    static IsWow64Process_Type IsWow64Process;
    if (kernel32_handle == NULL)
        kernel32_handle = GetModuleHandle("kernel32.dll");
    if (IsWow64Process == NULL && kernel32_handle != NULL) {
        IsWow64Process = (IsWow64Process_Type)
            GetProcAddress(kernel32_handle, "IsWow64Process");
    }
    if (IsWow64Process == NULL) {
        /* should be NT or 2K */
        assert(get_windows_version() == WINDOWS_VERSION_NT ||
               get_windows_version() == WINDOWS_VERSION_2000);
        return false;
    } else {
        bool res;
        if (!IsWow64Process(hProcess, &res))
            return false;
        return res;
    }
}

/* FIXME: Port these thread routines to Linux using the ones from linux/clone.c.
 * We'll have to change existing Windows tests to pass a stack out param or leak
 * the stack on Linux.
 */

# ifndef STATIC_LIBRARY  /* FIXME i#975: conflicts with DR's symbols. */
/* Thread related functions */
thread_handle
create_thread(fptr f)
{
    thread_handle th;

    uint tid;
    th = (thread_handle) _beginthreadex(NULL, 0, f, NULL, 0, &tid);
    return th;
}
# endif

void
suspend_thread(thread_handle th)
{
    SuspendThread(th);
}

void
resume_thread(thread_handle th)
{
    ResumeThread(th);
}

void
join_thread(thread_handle th)
{
    WaitForSingleObject(th, INFINITE);
}

# ifndef STATIC_LIBRARY  /* FIXME i#975: conflicts with DR's symbols. */
void
thread_yield()
{
    Sleep(0); /* stay ready */
}
# endif
#endif  /* WINDOWS */

int
get_os_prot_word(int prot)
{
#ifdef UNIX
    return ((TEST(ALLOW_READ, prot)  ? PROT_READ  : 0) |
            (TEST(ALLOW_WRITE, prot) ? PROT_WRITE : 0) |
            (TEST(ALLOW_EXEC, prot)  ? PROT_EXEC  : 0));
#else
    if (TEST(ALLOW_WRITE, prot)) {
        if (TEST(ALLOW_EXEC, prot)) {
            return PAGE_EXECUTE_READWRITE;
        } else {
            return PAGE_READWRITE;
        }
    } else {
        if (TEST(ALLOW_READ, prot)) {
            if (TEST(ALLOW_EXEC, prot)) {
                return PAGE_EXECUTE_READ;
            } else {
                return PAGE_READONLY;
            }
        } else {
            if (TEST(ALLOW_EXEC, prot)) {
                return PAGE_EXECUTE;
            } else {
                return PAGE_NOACCESS;
            }
        }
    }
#endif
}

char *
allocate_mem(int size, int prot)
{
#ifdef UNIX
    return (char *) mmap((void *)0, size, get_os_prot_word(prot),
                         MAP_PRIVATE|MAP_ANON, 0, 0);
#else
    return (char *) VirtualAlloc(NULL, size, MEM_COMMIT, get_os_prot_word(prot));
#endif
}

void
protect_mem(void *start, size_t len, int prot)
{
#ifdef UNIX
    void *page_start = (void *)(((ptr_int_t)start) & ~(PAGE_SIZE -1));
    int page_len = (len + ((ptr_int_t)start - (ptr_int_t)page_start) + PAGE_SIZE - 1)
        & ~(PAGE_SIZE - 1);
    if (mprotect(page_start, page_len, get_os_prot_word(prot)) != 0) {
        print("Error on mprotect: %d\n", errno);
    }
#else
    DWORD old;
    if (VirtualProtect(start, len, get_os_prot_word(prot), &old) == 0) {
        print("Error on VirtualProtect\n");
    }
#endif
}

void
protect_mem_check(void *start, size_t len, int prot, int expected)
{
#ifdef UNIX
    /* FIXME : add check */
    protect_mem(start, len, prot);
#else
    DWORD old;
    if (VirtualProtect(start, len, get_os_prot_word(prot), &old) == 0) {
        print("Error on VirtualProtect\n");
    }
    if (old != get_os_prot_word(expected)) {
        print("Unexpected previous permissions\n");
    }
#endif
}

void *
reserve_memory(int size)
{
#ifdef UNIX
    void *p = mmap(0, size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE, -1, 0);
    if (p == MAP_FAILED)
        return NULL;
    return p;
#else
    return VirtualAlloc(0, size, MEM_RESERVE, PAGE_EXECUTE_READWRITE);
#endif
}

/* Intel processors:  ebx:edx:ecx spell GenuineIntel */
#define INTEL_EBX /* Genu */ 0x756e6547
#define INTEL_EDX /* ineI */ 0x49656e69
#define INTEL_ECX /* ntel */ 0x6c65746e

/* AMD processors:  ebx:edx:ecx spell AuthenticAMD */
#define AMD_EBX /* Auth */ 0x68747541
#define AMD_EDX /* enti */ 0x69746e65
#define AMD_ECX /* cAMD */ 0x444d4163

#define VENDOR_INTEL    0
#define VENDOR_AMD      1
#define VENDOR_UNKNOWN  2

/* Pentium IV -- FIXME: need to distinguish extended family bits? */
#define FAMILY_PENTIUM_IV  15
/* Pentium Pro, Pentium II, Pentium III, Athlon */
#define FAMILY_PENTIUM_III  6
#define FAMILY_PENTIUM_II   6
#define FAMILY_PENTIUM_PRO  6
#define FAMILY_ATHLON       6
/* Pentium (586) */
#define FAMILY_PENTIUM      5

/*
 * On Pentium through Pentium III and Pentium 4, I-cache lines are 32 bytes.
 * On Pentium IV they are 64 bytes.
 */
unsigned int
get_cache_line_size()
{
    /* use cpuid to get cache line size */
    unsigned int cache_line_size, vendor, family;
    unsigned int res_eax, res_ebx = 0, res_ecx = 0, res_edx = 0;
#ifdef WINDOWS
    int cpuid_res_local[4]; /* eax, ebx, ecx, and edx registers (in that order) */
#endif
    /* first verify on Intel processor */
#ifdef UNIX
# ifdef X64
    assert(false); /* no pusha! */
# else
    /* GOT pointer is kept in ebx, go ahead and save all the registers */
    asm("pusha");
    asm("movl $0, %eax");
    asm("cpuid");
    asm("movl %%eax, %0" : "=m"(res_eax));
    asm("movl %%ebx, %0" : "=m"(res_ebx));
    asm("movl %%ecx, %0" : "=m"(res_ecx));
    asm("movl %%edx, %0" : "=m"(res_edx));
    asm("popa");
# endif
#else
    __cpuid(cpuid_res_local, 0);
    res_eax = cpuid_res_local[0];
    res_ebx = cpuid_res_local[1];
    res_ecx = cpuid_res_local[2];
    res_edx = cpuid_res_local[3];
#endif

    if (res_ebx == INTEL_EBX) {
        vendor = VENDOR_INTEL;
        assert(res_edx == INTEL_EDX && res_ecx == INTEL_ECX);
    } else if (res_ebx == AMD_EBX) {
        vendor = VENDOR_AMD;
        assert(res_edx == AMD_EDX && res_ecx == AMD_ECX);
    } else {
        vendor = VENDOR_UNKNOWN;
        print("get_cache_line_size: unknown processor type");
    }

    /* now get processor info */
#ifdef UNIX
    /* GOT pointer is kept in ebx, go ahead and save all the registers */
# ifdef X64
    assert(false); /* no pusha! */
# else
    asm("pusha");
    asm("movl $1, %eax");
    asm("cpuid");
    asm("movl %%eax, %0" : "=m"(res_eax));
    asm("movl %%ebx, %0" : "=m"(res_ebx));
    asm("movl %%ecx, %0" : "=m"(res_ecx));
    asm("movl %%edx, %0" : "=m"(res_edx));
    asm("popa");
# endif
#else
    __cpuid(cpuid_res_local, 1);
    res_eax = cpuid_res_local[0];
    res_ebx = cpuid_res_local[1];
    res_ecx = cpuid_res_local[2];
    res_edx = cpuid_res_local[3];
#endif
    /* eax contains basic info:
     *   extended family, extended model, type, family, model, stepping id
     *   20:27,           16:19,          12:13, 8:11,  4:7,   0:3
     */
    /* family bits: 8 through 11 */
    family = (res_eax & 0x00000f00) >> 8;
    if (family == FAMILY_PENTIUM_IV) {
        /* Pentium IV */
        /* FIXME: need to read extended family bits? */
        cache_line_size = (res_ebx & 0x0000ff00) >> 5; /* (x >> 8) * 8 == x >> 5 */
    } else if (vendor == VENDOR_INTEL &&
               (family == FAMILY_PENTIUM_III || family == FAMILY_PENTIUM_II)) {
        /* Pentium III, Pentium II */
        cache_line_size = 32;
    } else if (vendor == VENDOR_AMD && family == FAMILY_ATHLON) {
        /* Athlon */
        cache_line_size = 64;
    } else {
        print("get_cache_line_size: unsupported processor family %d\n",
              family);
        cache_line_size = 32;
    }
    /* people who use this in ALIGN* macros are assuming it's a power of 2 */
    assert((cache_line_size & (cache_line_size - 1)) == 0);
    return cache_line_size;
}

void
print(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fflush(stderr);
    va_end(ap);
}

#ifdef UNIX

/***************************************************************************/
/* a hopefuly portable /proc/@self/maps reader */

/* these are defined in /usr/src/linux/fs/proc/array.c */
#define MAPS_LINE_LENGTH        4096
/* for systems with sizeof(void*) == 4: */
#define MAPS_LINE_FORMAT4         "%08lx-%08lx %s %*x %*s %*u %4096s"
#define MAPS_LINE_MAX4  49 /* sum of 8  1  8  1 4 1 8 1 5 1 10 1 */
/* for systems with sizeof(void*) == 8: */
#define MAPS_LINE_FORMAT8         "%016lx-%016lx %s %*x %*s %*u %4096s"
#define MAPS_LINE_MAX8  73 /* sum of 16  1  16  1 4 1 16 1 5 1 10 1 */

#define MAPS_LINE_MAX   MAPS_LINE_MAX8

bool
find_dynamo_library(void)
{
    pid_t pid = getpid();
    char  proc_pid_maps[64];      /* file name */

    FILE *maps;
    char  line[MAPS_LINE_LENGTH];
    int   count = 0;

    /* open file's /proc/id/maps virtual map description */
    int n = snprintf(proc_pid_maps, sizeof(proc_pid_maps),
                     "/proc/%d/maps", pid);
    if (n < 0 || n == sizeof(proc_pid_maps))
        assert(0); /* paranoia */

    maps = fopen(proc_pid_maps, "r");

    while (!feof(maps)){
        void *vm_start;
        void *vm_end;
        char perm[16];
        char comment_buffer[MAPS_LINE_LENGTH];
        int len;

        if (NULL == fgets(line, sizeof(line), maps))
            break;
        len = sscanf(line,
                     sizeof(void*) == 4 ? MAPS_LINE_FORMAT4 : MAPS_LINE_FORMAT8,
                     (unsigned long*)&vm_start, (unsigned long*)&vm_end, perm,
                     comment_buffer);
        if (len < 4)
            comment_buffer[0] = '\0';
        if (strstr(comment_buffer, "libdynamorio.so") != 0) {
            fclose(maps);
            return true;
        }
    }

    fclose(maps);
    return false;
}

/******************************************************************************
 * Staticly linked and stateless versions of libc routines.
 */

/* Safe strlen.
 */
int
nolibc_strlen(const char *str)
{
    int i;
    for (i = 0; str[i] != '\0'; i++) {
        /* pass */
    }
    return i;
}

/* Safe print syscall.
 */
void
nolibc_print(const char *str)
{
    nolibc_syscall(
#ifdef MACOS
                   SYS_write_nocancel,
#else
                   SYS_write,
#endif
                   3, stderr->_fileno, str, nolibc_strlen(str));
}

/* Safe print int syscall.
 */
void
nolibc_print_int(int n)
{
    char buf[12];  /* 2 ** 31 has 10 digits, plus sign and nul. */
    int i = 0;
    int m;
    int len;

    /* Sign. */
    if (n < 0) {
        buf[i++] = '-';
        n = -n;
    }

    /* Go forward by the number of digits we'll need. */
    m = n;
    while (m > 0) {
        m /= 10;
        i++;
    }
    len = i;
    assert(len <= sizeof(buf));

    /* Go backward for each digit. */
    while (n > 0) {
        buf[--i] = '0' + (n % 10);
        n /= 10;
    }

    buf[len] = '\0';

    nolibc_print(buf);
}

/* Safe nanosleep.
 */
void
nolibc_nanosleep(struct timespec *req)
{
    nolibc_syscall(SYS_nanosleep, 2, req, NULL);
}

/* Safe mmap.
 */
void *
nolibc_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
#ifdef X64
    int sysnum = SYS_mmap;
#else
    int sysnum = SYS_mmap2;
#endif
    return (void*)nolibc_syscall(sysnum, 6, addr, length, prot, flags, fd,
                               offset);
}

/* Safe munmap.
 */
void
nolibc_munmap(void *addr, size_t length)
{
    nolibc_syscall(SYS_munmap, 2, addr, length);
}

void
nolibc_memset(void *dst, int val, size_t size)
{
    size_t i;
    char *buf = (char*)dst;
    for (i = 0; i < size; i++) {
        buf[i] = (char)val;
    }
}

/******************************************************************************
 * Signal handling
 */

/* set up signal_handler as the handler for signal "sig" */
void
intercept_signal(int sig, handler_3_t handler, bool sigstack)
{
    int rc;
    struct sigaction act;

    act.sa_sigaction = (void (*)(int, siginfo_t *, void *)) handler;
    rc = sigfillset(&act.sa_mask); /* block all signals within handler */
    ASSERT_NOERR(rc);
    act.sa_flags = SA_SIGINFO;
    if (sigstack)
        act.sa_flags = SA_ONSTACK;

    /* arm the signal */
    rc = sigaction(sig, &act, NULL);
    ASSERT_NOERR(rc);
}

#endif /* UNIX */

#else /* asm code *************************************************************/
/*
 * Assembly routines shared among multiple tests
 */

#include "asm_defines.asm"

START_FILE

/* int code_self_mod(int iters)
 * executes iters iters, by modifying the iters bound using self-modifying code.
 */
        DECLARE_FUNC(code_self_mod)
GLOBAL_LABEL(code_self_mod:)
        mov  REG_XCX, ARG1
        call next_instr
      next_instr:
        pop  REG_XDX
    /* add to retaddr: 1 == pop
     *                 3 == mov ecx into target
     *                 1 == opcode of target movl
     */
        mov  DWORD [REG_XDX + 5], ecx /* the modifying store */
        mov  eax,HEX(12345678) /* this instr's immed gets overwritten */
        mov  ecx,0 /* counter for diagnostics */
      repeat1:
        dec  eax
        inc  ecx
        cmp  eax,0
        jnz  repeat1
        mov  eax,ecx
        ret
        END_FUNC(code_self_mod)

#undef FUNCNAME
#define FUNCNAME code_inc
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov  REG_XAX, ARG1
        inc  REG_XAX
        ret
        END_FUNC(FUNCNAME)

#undef FUNCNAME
#define FUNCNAME code_dec
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov  REG_XAX, ARG1
        dec  REG_XAX
        ret
        END_FUNC(FUNCNAME)

#undef FUNCNAME
#define FUNCNAME dummy
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov  REG_XAX, 1
        ret
        END_FUNC(FUNCNAME)

#ifdef UNIX
/* Raw system call adapter for Linux.  Useful for threading and clone tests that
 * need to avoid using libc routines.  Using libc routines can enter the loader
 * and/or touch global state and TLS state.  Our tests use CLONE_VM and don't
 * initialize TLS segments, so the TLS is actually *shared* with the parent
 * (xref i#500).
 *
 * Copied from core/x86/x86.asm dynamorio_syscall.
 */
#undef FUNCNAME
#define FUNCNAME nolibc_syscall
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* x64 kernel doesn't clobber all the callee-saved registers */
        push     REG_XBX
# ifdef X64
        /* reverse order so we don't clobber earlier args */
        mov      REG_XBX, ARG2 /* put num_args where we can reference it longer */
        mov      rax, ARG1 /* sysnum: only need eax, but need rax to use ARG1 (or movzx) */
        cmp      REG_XBX, 0
        je       syscall_ready
        mov      ARG1, ARG3
        cmp      REG_XBX, 1
        je       syscall_ready
        mov      ARG2, ARG4
        cmp      REG_XBX, 2
        je       syscall_ready
        mov      ARG3, ARG5
        cmp      REG_XBX, 3
        je       syscall_ready
        mov      ARG4, ARG6
        cmp      REG_XBX, 4
        je       syscall_ready
        mov      ARG5, [2*ARG_SZ + REG_XSP] /* arg7: above xbx and retaddr */
        cmp      REG_XBX, 5
        je       syscall_ready
        mov      ARG6, [3*ARG_SZ + REG_XSP] /* arg8: above arg7, xbx, retaddr */
syscall_ready:
        mov      r10, rcx
        syscall
# else
        push     REG_XBP
        push     REG_XSI
        push     REG_XDI
        /* add 16 to skip the 4 pushes
         * FIXME: rather than this dispatch, could have separate routines
         * for each #args, or could just blindly read upward on the stack.
         * for dispatch, if assume size of mov instr can do single ind jmp */
        mov      ecx, [16+ 8 + esp] /* num_args */
        cmp      ecx, 0
        je       syscall_0args
        cmp      ecx, 1
        je       syscall_1args
        cmp      ecx, 2
        je       syscall_2args
        cmp      ecx, 3
        je       syscall_3args
        cmp      ecx, 4
        je       syscall_4args
        cmp      ecx, 5
        je       syscall_5args
        mov      ebp, [16+32 + esp] /* arg6 */
syscall_5args:
        mov      edi, [16+28 + esp] /* arg5 */
syscall_4args:
        mov      esi, [16+24 + esp] /* arg4 */
syscall_3args:
        mov      edx, [16+20 + esp] /* arg3 */
syscall_2args:
        mov      ecx, [16+16 + esp] /* arg2 */
syscall_1args:
        mov      ebx, [16+12 + esp] /* arg1 */
syscall_0args:
        mov      eax, [16+ 4 + esp] /* sysnum */
        /* PR 254280: we assume int$80 is ok even for LOL64 */
        int      HEX(80)
        pop      REG_XDI
        pop      REG_XSI
        pop      REG_XBP
# endif /* X64 */
        pop      REG_XBX
        /* return val is in eax for us */
        ret
        END_FUNC(FUNCNAME)
#endif /* UNIX */

#undef FUNCNAME
#define FUNCNAME call_with_retaddr
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        lea     REG_XAX, [REG_XSP]  /* Load address of retaddr. */
        xchg    REG_XAX, ARG1       /* Swap with function pointer in arg1. */
        jmp     REG_XAX             /* Call function, now with &retaddr as arg1. */
        END_FUNC(FUNCNAME)

END_FILE

#endif
