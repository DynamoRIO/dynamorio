/* **********************************************************
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

#ifndef ASM_CODE_ONLY /* C code */
#include "tools.h"

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
        if (version.dwMajorVersion == 6 && version.dwMinorVersion == 1) {
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
#endif

/* Thread related functions */
thread_handle
create_thread(fptr f)
{
    thread_handle th;

#ifdef LINUX
    /* FIXME: use the one from linux/clone.c */
    ASSERT_NOT_IMPLEMENTED();
#else
    uint tid;
    th = (thread_handle) _beginthreadex(NULL, 0, f, NULL, 0, &tid);
#endif
    return th;
}

void
suspend_thread(thread_handle th)
{
#ifdef LINUX
    ASSERT_NOT_IMPLEMENTED();
#else
    SuspendThread(th);
#endif
}

void
resume_thread(thread_handle th)
{
#ifdef LINUX
    ASSERT_NOT_IMPLEMENTED();
#else
    ResumeThread(th);
#endif
}

void
join_thread(thread_handle th)
{
#ifdef LINUX
    ASSERT_NOT_IMPLEMENTED();
#else
    WaitForSingleObject(th, INFINITE);
#endif
}

void
thread_yield()
{
#ifdef LINUX
    ASSERT_NOT_IMPLEMENTED();
#else
    Sleep(0); /* stay ready */
#endif
}

void *
reserve_memory(int size)
{
#ifdef LINUX
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
#ifdef LINUX
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
#ifdef LINUX
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

END_FILE
#endif
