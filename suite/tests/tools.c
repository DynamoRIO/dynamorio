/* **********************************************************
 * Copyright (c) 2013-2023 Google, Inc.  All rights reserved.
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
#    include "tools.h"

#    ifdef UNIX
#        include <unistd.h>
#        include <sys/syscall.h> /* for SYS_* numbers */
#    endif
#    ifdef MACOS
#        include <mach/mach.h>
#        include <mach/semaphore.h>
#        ifdef AARCH64
/* Somehow including pthread.h is not surfacing this so we explicitly list it. */
void
pthread_jit_write_protect_np(int enabled);
#        endif
#    endif

#    define ASSERT_NOT_IMPLEMENTED() \
        do {                         \
            print("NYI\n");          \
            abort();                 \
        } while (0)

#    ifdef WINDOWS
/* returns 0 on failure */
/* FIXME - share with core get_os_version() */
int
get_windows_version(void)
{
    OSVERSIONINFOW version;
    /* i#1418: GetVersionEx is just plain broken on win8.1+ so we use the Rtl version */
    typedef NTSTATUS(NTAPI * RtlGetVersion_t)(OSVERSIONINFOW * info);
    RtlGetVersion_t RtlGetVersion;
    NTSTATUS res;
    HANDLE ntdll_handle = GetModuleHandleW(L"ntdll.dll");
    assert(ntdll_handle != NULL);
    RtlGetVersion =
        (RtlGetVersion_t)GetProcAddress((HMODULE)ntdll_handle, "RtlGetVersion");
    assert(RtlGetVersion != NULL);
    version.dwOSVersionInfoSize = sizeof(version);
    res = RtlGetVersion(&version);
    assert(NT_SUCCESS(res));
    if (version.dwPlatformId == VER_PLATFORM_WIN32_NT) {
        /* WinNT or descendents */
        if (version.dwMajorVersion == 10 && version.dwMinorVersion == 0) {
            if (GetProcAddress((HMODULE)ntdll_handle, "NtAllocateVirtualMemoryEx") !=
                NULL)
                return WINDOWS_VERSION_10_1803;
            else if (GetProcAddress((HMODULE)ntdll_handle, "NtCallEnclave") != NULL)
                return WINDOWS_VERSION_10_1709;
            else if (GetProcAddress((HMODULE)ntdll_handle, "NtLoadHotPatch") != NULL)
                return WINDOWS_VERSION_10_1703;
            else if (GetProcAddress((HMODULE)ntdll_handle,
                                    "NtCreateRegistryTransaction") != NULL)
                return WINDOWS_VERSION_10_1607;
            else if (GetProcAddress((HMODULE)ntdll_handle, "NtCreateEnclave") != NULL)
                return WINDOWS_VERSION_10_1511;
            else
                return WINDOWS_VERSION_10;
        } else if (version.dwMajorVersion == 6 && version.dwMinorVersion == 3) {
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
    typedef DWORD(WINAPI * IsWow64Process_Type)(HANDLE hProcess, PBOOL isWow64Process);
    static HANDLE kernel32_handle;
    static IsWow64Process_Type IsWow64Process;
    if (kernel32_handle == NULL)
        kernel32_handle = GetModuleHandle("kernel32.dll");
    if (IsWow64Process == NULL && kernel32_handle != NULL) {
        IsWow64Process =
            (IsWow64Process_Type)GetProcAddress(kernel32_handle, "IsWow64Process");
    }
    if (IsWow64Process == NULL) {
        /* should be NT or 2K */
        assert(get_windows_version() == WINDOWS_VERSION_NT ||
               get_windows_version() == WINDOWS_VERSION_2000);
        return false;
    } else {
        BOOL res;
        if (!IsWow64Process(hProcess, &res))
            return false;
        return CAST_TO_bool(res);
    }
}
#    endif /* WINDOWS */

int
get_os_prot_word(int prot)
{
#    ifdef UNIX
    return ((TEST(ALLOW_READ, prot) ? PROT_READ : 0) |
            (TEST(ALLOW_WRITE, prot) ? PROT_WRITE : 0) |
            (TEST(ALLOW_EXEC, prot) ? PROT_EXEC : 0));
#    else
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
#    endif
}

char *
allocate_mem(size_t size, int prot)
{
#    ifdef UNIX
    int flags = MAP_PRIVATE | MAP_ANON;
#        if defined(MACOS) && defined(AARCH64)
    if (TEST(ALLOW_EXEC, prot)) {
        flags |= MAP_JIT;
        pthread_jit_write_protect_np(0);
    }
#        endif
    char *res = (char *)mmap((void *)0, size, get_os_prot_word(prot), flags, -1, 0);
    if (res == MAP_FAILED)
        return NULL;
    return res;
#    else
    return (char *)VirtualAlloc(NULL, size, MEM_COMMIT, get_os_prot_word(prot));
#    endif
}

void
free_mem(char *addr, size_t size)
{
#    ifdef UNIX
    int res = munmap(addr, size);
    if (res != 0)
        print("Error on munmap: %d\n", errno);
#    else
    BOOL res = VirtualFree(addr, size, MEM_DECOMMIT);
    if (!res)
        print("Error on VirtualFree\n");
#    endif
}

void
protect_mem(void *start, size_t len, int prot)
{
#    ifdef UNIX
    void *page_start = (void *)(((ptr_int_t)start) & ~(PAGE_SIZE - 1));
    int page_len = (len + ((ptr_int_t)start - (ptr_int_t)page_start) + PAGE_SIZE - 1) &
        ~(PAGE_SIZE - 1);
#        if defined(MACOS) && defined(AARCH64)
    if (TEST(ALLOW_EXEC, prot) && !TEST(ALLOW_WRITE, prot)) {
        pthread_jit_write_protect_np(1);
        return;
    }
#        endif
    if (mprotect(page_start, page_len, get_os_prot_word(prot)) != 0) {
        print("Error on mprotect: %d\n", errno);
    }
#    else
    DWORD old;
    if (VirtualProtect(start, len, get_os_prot_word(prot), &old) == 0) {
        print("Error on VirtualProtect\n");
    }
#    endif
}

void
protect_mem_check(void *start, size_t len, int prot, int expected)
{
#    ifdef UNIX
    /* FIXME : add check */
    protect_mem(start, len, prot);
#    else
    DWORD old;
    if (VirtualProtect(start, len, get_os_prot_word(prot), &old) == 0) {
        print("Error on VirtualProtect\n");
    }
    if (old != get_os_prot_word(expected)) {
        print("Unexpected previous permissions\n");
    }
#    endif
}

void *
reserve_memory(int size)
{
#    ifdef UNIX
    void *p =
        mmap(0, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (p == MAP_FAILED)
        return NULL;
    return p;
#    else
    return VirtualAlloc(0, size, MEM_RESERVE, PAGE_EXECUTE_READWRITE);
#    endif
}

/* Intel processors:  ebx:edx:ecx spell GenuineIntel */
#    define INTEL_EBX /* Genu */ 0x756e6547
#    define INTEL_EDX /* ineI */ 0x49656e69
#    define INTEL_ECX /* ntel */ 0x6c65746e

/* AMD processors:  ebx:edx:ecx spell AuthenticAMD */
#    define AMD_EBX /* Auth */ 0x68747541
#    define AMD_EDX /* enti */ 0x69746e65
#    define AMD_ECX /* cAMD */ 0x444d4163

#    define VENDOR_INTEL 0
#    define VENDOR_AMD 1
#    define VENDOR_UNKNOWN 2

/* Pentium IV -- FIXME: need to distinguish extended family bits? */
#    define FAMILY_PENTIUM_IV 15
/* Pentium Pro, Pentium II, Pentium III, Athlon */
#    define FAMILY_PENTIUM_III 6
#    define FAMILY_PENTIUM_II 6
#    define FAMILY_PENTIUM_PRO 6
#    define FAMILY_ATHLON 6
/* Pentium (586) */
#    define FAMILY_PENTIUM 5

void
print(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fflush(stderr);
    va_end(ap);
}

#    ifdef UNIX

/***************************************************************************/
/* a hopefuly portable /proc/@self/maps reader */

/* these are defined in /usr/src/linux/fs/proc/array.c */
#        define MAPS_LINE_LENGTH 4096
/* for systems with sizeof(void*) == 4: */
#        define MAPS_LINE_FORMAT4 "%08lx-%08lx %s %*x %*s %*u %4096s"
#        define MAPS_LINE_MAX4 49 /* sum of 8  1  8  1 4 1 8 1 5 1 10 1 */
                                  /* for systems with sizeof(void*) == 8: */
#        define MAPS_LINE_FORMAT8 "%016lx-%016lx %s %*x %*s %*u %4096s"
#        define MAPS_LINE_MAX8 73 /* sum of 16  1  16  1 4 1 16 1 5 1 10 1 */

#        define MAPS_LINE_MAX MAPS_LINE_MAX8

bool
find_dynamo_library(void)
{
    pid_t pid = getpid();
    char proc_pid_maps[64]; /* file name */

    FILE *maps;
    char line[MAPS_LINE_LENGTH];
    int count = 0;

    /* open file's /proc/id/maps virtual map description */
    int n = snprintf(proc_pid_maps, sizeof(proc_pid_maps), "/proc/%d/maps", pid);
    if (n < 0 || n == sizeof(proc_pid_maps))
        assert(0); /* paranoia */

    maps = fopen(proc_pid_maps, "r");

    while (!feof(maps)) {
        void *vm_start;
        void *vm_end;
        char perm[16];
        char comment_buffer[MAPS_LINE_LENGTH];
        int len;

        if (NULL == fgets(line, sizeof(line), maps))
            break;
        len = sscanf(line, sizeof(void *) == 4 ? MAPS_LINE_FORMAT4 : MAPS_LINE_FORMAT8,
                     (unsigned long *)&vm_start, (unsigned long *)&vm_end, perm,
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
    dynamorio_syscall(
#        ifdef MACOS
        SYS_write_nocancel,
#        else
        SYS_write,
#        endif
        3,
#        if defined(MACOS) || defined(ANDROID)
        stderr->_file,
#        else
        stderr->_fileno,
#        endif
        str, nolibc_strlen(str));
}

/* Safe print int syscall.
 */
void
nolibc_print_int(int n)
{
    char buf[12]; /* 2 ** 31 has 10 digits, plus sign and nul. */
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
#        ifdef MACOS
    /* XXX: share with os_thread_sleep */
    semaphore_t sem = MACH_PORT_NULL;
    int res;
    if (sem == MACH_PORT_NULL) {
        kern_return_t res = semaphore_create(mach_task_self(), &sem, SYNC_POLICY_FIFO, 0);
        assert(res == KERN_SUCCESS);
    }
    res = dynamorio_syscall(SYS___semwait_signal_nocancel, 6, sem, MACH_PORT_NULL, 1, 1,
                            (int64_t)req->tv_sec, (int32_t)req->tv_nsec);
#        else
    dynamorio_syscall(SYS_nanosleep, 2, req, NULL);
#        endif
}

/* Safe mmap.
 */
void *
nolibc_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
#        if defined(X64) || defined(MACOS)
    int sysnum = SYS_mmap;
#        else
    int sysnum = SYS_mmap2;
#        endif
    return (void *)dynamorio_syscall(sysnum, 6, addr, length, prot, flags, fd, offset);
}

/* Safe munmap.
 */
int
nolibc_munmap(void *addr, size_t length)
{
    return (int)dynamorio_syscall(SYS_munmap, 2, addr, length);
}

void
nolibc_memset(void *dst, int val, size_t size)
{
    size_t i;
    char *buf = (char *)dst;
    for (i = 0; i < size; i++) {
        buf[i] = (sbyte)val;
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

    act.sa_sigaction = (void (*)(int, siginfo_t *, void *))handler;
    rc = sigfillset(&act.sa_mask); /* block all signals within handler */
    ASSERT_NOERR(rc);
    act.sa_flags = SA_SIGINFO;
    if (sigstack)
        act.sa_flags |= SA_ONSTACK;

    /* arm the signal */
    rc = sigaction(sig, &act, NULL);
    ASSERT_NOERR(rc);
}

#    endif /* UNIX */

#else /* asm code *************************************************************/
/*
 * Assembly routines shared among multiple tests
 */

#    include "asm_defines.asm"

/* clang-format off */
START_FILE

/* int code_self_mod(int iters)
 * executes iters iters, by modifying the iters bound using self-modifying code.
 */
        DECLARE_FUNC(code_self_mod)
GLOBAL_LABEL(code_self_mod:)
#ifdef X86
        mov      REG_XCX, ARG1
        call     next_instr
      next_instr:
        pop      REG_XDX
    /* add to retaddr: 1 == pop
     *                 3 == mov ecx into target
     *                 1 == opcode of target movl
     */
        mov      DWORD [REG_XDX + 5], ecx /* the modifying store */
        mov      eax,HEX(12345678) /* this instr's immed gets overwritten */
        mov      ecx,0 /* counter for diagnostics */
      repeat1:
        dec      eax
        inc      ecx
        cmp      eax,0
        jnz      repeat1
        mov      eax,ecx
        ret
#elif defined(ARM)
        adr      r2, tomodify
        /* We only support a 16-bit arg */
        strb     ARG1, [r2]     /* modifying store: byte 0 */
        asr      ARG1, #8
        mov      r3, ARG1
        bfc      r3, #4, #4
        strb     r3, [r2, #1]   /* the modifying store: byte 1 bottom nibble */
        asr      ARG1, #4
        bfc      ARG1, #4, #4
        strb     ARG1, [r2, #2] /* modifying store: byte 1 top nibble */
        mov      r0, r2       /* start of region to flush */
        add      r1, r2, #4   /* end of region to flush */
        /* We can't call cache_flush() b/c this code will be copied but not relocated */
        mov      r2, #0       /* flags: must be 0 */
        push     {r7}
        movw     r7, #0x0002  /* SYS_cacheflush bottom half */
        movt     r7, #0x000f  /* SYS_cacheflush top half */
        svc      #0           /* flush icache */
        pop      {r7}
      tomodify:
        movw     r0, #0x1234 /* this instr's immed gets overwritten: e3010234 */
        mov      r1, #0 /* counter for diagnostics */
      repeat1:
        sub      r0, r0, #1
        add      r1, r1, #1
        cmp      r0, #0
        bne      repeat1
        mov      r0, r1
        bx       lr
#elif defined(AARCH64)
        adr      x1, tomodify
        ldr      w2, [x1]
        bfi      w2, w0, #5, #16 /* insert new immediate operand */
        str      w2, [x1]
        dc       cvau, x1 /* Data Cache Clean by VA to PoU */
        dsb      ish /* Data Synchronization Barrier, Inner Shareable */
        ic       ivau, x1 /* Instruction Cache Invalidate by VA to PoU */
        dsb      ish /* Data Synchronization Barrier, Inner Shareable */
        isb      /* Instruction Synchronization Barrier */
      tomodify:
        movz     w1, #0x1234 /* this instr's immed operand gets overwritten */
        mov      w0, #0 /* counter for diagnostics */
      repeat1:
        add      w0, w0, #1
        sub      w1, w1, #1
        cbnz     w1, repeat1
        ret
#elif defined(RISCV64)
        /* TODO i#3544: Port tests to RISC-V64 */
        ret
#else
# error NYI
#endif
        END_FUNC(code_self_mod)

#undef FUNCNAME
#define FUNCNAME icache_sync
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        /* Nothing to do. */
        ret
#elif defined(ARM)
        mov      r0, ARG1     /* start of region to flush */
        add      r1, r0, #64  /* end of region to flush */
        mov      r2, #0       /* flags: must be 0 */
        push     {r7}
        movw     r7, #0x0002  /* SYS_cacheflush bottom half */
        movt     r7, #0x000f  /* SYS_cacheflush top half */
        svc      #0           /* flush icache */
        pop      {r7}
        bx       lr
#elif defined(AARCH64)
        mov      x0, ARG1
        dc       cvau, x0  /* Data Cache Clean by VA to PoU */
        dsb      ish       /* Data Synchronization Barrier, Inner Shareable */
        ic       ivau, x0  /* Instruction Cache Invalidate by VA to PoU */
        dsb      ish       /* Data Synchronization Barrier, Inner Shareable */
        isb                /* Instruction Synchronization Barrier */
        RETURN
#endif
        END_FUNC(FUNCNAME)

#undef FUNCNAME
#define FUNCNAME code_inc
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#if !defined(RISCV64) /* TODO i#3544: Port tests to RISC-V 64 */
        mov      REG_SCRATCH0, ARG1
        INC(REG_SCRATCH0)
        RETURN
#endif
        END_FUNC(FUNCNAME)

#undef FUNCNAME
#define FUNCNAME code_dec
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#if !defined(RISCV64) /* TODO i#3544: Port tests to RISC-V 64 */
        mov      REG_SCRATCH0, ARG1
        DEC(REG_SCRATCH0)
        RETURN
#endif
        END_FUNC(FUNCNAME)

#undef FUNCNAME
#define FUNCNAME dummy
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#if !defined(RISCV64) /* TODO i#3544: Port tests to RISC-V 64 */
        mov      REG_SCRATCH0, HEX(1)
        RETURN
#endif
        END_FUNC(FUNCNAME)

#undef FUNCNAME
#define FUNCNAME call_with_retaddr
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        lea      REG_XAX, [REG_XSP]  /* Load address of retaddr. */
        xchg     REG_XAX, ARG1       /* Swap with function pointer in arg1. */
        jmp      REG_XAX             /* Call function, now with &retaddr as arg1. */
#elif defined(ARM)
        push     {r7, lr}
        add      r7, sp, #0
        mov      lr, r0
        add      r0, sp, #4          /* Make pointer to return address on stack. */
        blx      lr                  /* Call function, with &retaddr as arg1. */
        pop      {r7, pc}            /* Return to possibly modified return address. */
#elif defined(AARCH64)
        stp      x29, x30, [sp, #-16]!
        mov      x29, sp
        mov      x30, x0
        add      x0, sp, #8          /* Make pointer to return address on stack. */
        blr      x30                 /* Call function, with &retaddr as arg1. */
        ldp      x29, x30, [sp], #16
        ret                          /* Return to possibly modified return address. */
#elif defined(RISCV64)
        /* TODO i#3544: Port tests to RISC-V 64 */
        ret
#else
# error NYI
#endif
        END_FUNC(FUNCNAME)

#undef FUNCNAME
#define FUNCNAME tailcall_with_retaddr
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        mov      REG_XAX, [REG_XSP]  /* Load retaddr. */
        xchg     REG_XAX, ARG1       /* Swap with function pointer in arg1. */
        jmp      REG_XAX             /* Call function, now with retaddr as arg1. */
#elif defined(ARM)
        mov      r12, r0             /* Move function pointer to scratch register. */
        mov      r0, r14             /* Replace first argument with return address. */
        bx       r12                 /* Tailcall to function pointer. */
#elif defined(AARCH64)
        mov      x9, x0              /* Move function pointer to scratch register. */
        mov      x0, x30             /* Replace first argument with return address. */
        br       x9                  /* Tailcall to function pointer. */
#elif defined(RISCV64)
        /* TODO i#3544: Port tests to RISC-V64 */
        ret
#else
# error NYI
#endif
        END_FUNC(FUNCNAME)

#ifdef AARCHXX
    /* gcc's __clear_cache is not easily usable: no header, need lib; so we just
     * roll our own.
     */
# undef FUNCNAME
# define FUNCNAME tools_clear_icache
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
# ifndef X64
        push     {r7}
        mov      r2, #0       /* flags: must be 0 */
        movw     r7, #0x0002  /* SYS_cacheflush bottom half */
        movt     r7, #0x000f  /* SYS_cacheflush top half */
        svc      #0           /* flush icache */
        pop      {r7}
        bx       lr
# else
        b        GLOBAL_REF(clear_icache)
# endif
        END_FUNC(FUNCNAME)
#endif

END_FILE
/* clang-format on */

#endif
