/* **********************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
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
#include <stdio.h>

#ifdef WINDOWS
#    include <windows.h>
#endif

#define CPUID_INTEL_EBX /* Genu */ 0x756e6547
#define CPUID_INTEL_EDX /* ineI */ 0x49656e69
#define CPUID_INTEL_ECX /* ntel */ 0x6c65746e

#define CPUID_AMD_EBX /* Auth */ 0x68747541
#define CPUID_AMD_EDX /* enti */ 0x69746e65
#define CPUID_AMD_ECX /* cAMD */ 0x444d4163

#define FEAT_EDX_MMX (1 << 23)
#define FEAT_EDX_SSE (1 << 25)
#define FEAT_EDX_SSE2 (1 << 26)
#define FEAT_ECX_SSE3 (1 << 0)
#define FEAT_ECX_SSSE3 (1 << 9)
#define FEAT_ECX_SSE41 (1 << 19)
#define FEAT_ECX_SSE42 (1 << 20)
#define FEAT_ECX_AVX (1 << 28)

#ifdef UNIX
#    ifdef X64
#        define XDI "rdi"
#    else
#        define XDI "edi"
#    endif
#endif

static void
invoke_cpuid(uint output[4], uint eax)
{
#ifdef UNIX
    __asm__("mov %0, %%" XDI "; mov %1, %%eax; cpuid; mov %%eax, (%%" XDI "); "
            "mov %%ebx, 4(%%" XDI "); mov %%ecx, 8(%%" XDI "); mov %%edx, 12(%%" XDI ")"
            :
            : "g"(output), "g"(eax)
            : "%eax", "%ebx", "%ecx", "%edx", "%" XDI "");
#else
    __cpuid(output, eax);
#endif
}

static void
invoke_cpuid_ecx(uint output[4], uint eax, uint ecx)
{
#ifdef UNIX
    __asm__("mov %0, %%" XDI "; mov %1, %%eax; mov %2, %%ecx; cpuid; mov %%eax, (%%" XDI
            "); "
            "mov %%ebx, 4(%%" XDI "); mov %%ecx, 8(%%" XDI "); mov %%edx, 12(%%" XDI ")"
            :
            : "g"(output), "g"(eax), "g"(ecx)
            : "%eax", "%ebx", "%ecx", "%edx", "%" XDI "");
#else
    __cpuidex(output, eax, ecx);
#endif
}

int
main(int argc, char **argv)
{
    uint cpuid_val[4]; /* eax, ebx, ecx, edx */
    uint max_eax, max_ext_eax;
    uint family, model;
    uint feat_edx, feat_ecx;
    uint ext_edx = 0, ext_ecx = 0, sext_ebx = 0;

    /* We asssume cpuid exists and do not mess with bit 21 of eflags */

    invoke_cpuid(cpuid_val, 0);
    max_eax = cpuid_val[0];
    if (cpuid_val[1] == CPUID_INTEL_EBX && cpuid_val[2] == CPUID_INTEL_ECX &&
        cpuid_val[3] == CPUID_INTEL_EDX)
        print("Running on an Intel processor\n");
    else if (cpuid_val[1] == CPUID_AMD_EBX && cpuid_val[2] == CPUID_AMD_ECX &&
             cpuid_val[3] == CPUID_AMD_EDX)
        print("Running on an AMD processor\n");
    else
        print("Running on an unknown processor\n");

    invoke_cpuid(cpuid_val, 1);
    family = (cpuid_val[0] >> 8) & 0xf;
    model = (cpuid_val[0] >> 4) & 0xf;
    if (family == 6 || family == 15) {
        uint ext_model = (cpuid_val[0] >> 16) & 0xf;
        model += (ext_model << 4);
        if (family == 15) {
            uint ext_family = (cpuid_val[0] >> 20) & 0xff;
            family += ext_family;
        }
    }
    print("Type = %d, family = %d, model = %d, stepping = %d\n",
          (cpuid_val[0] >> 12) & 0x3, family, model, cpuid_val[0] & 0xf);
    feat_edx = cpuid_val[3];
    feat_ecx = cpuid_val[2];

    /* Extended features */
    invoke_cpuid(cpuid_val, 0x80000000);
    max_ext_eax = cpuid_val[0];
    if (max_ext_eax >= 0x80000001) {
        invoke_cpuid(cpuid_val, 0x80000001);
        ext_edx = cpuid_val[3];
        ext_ecx = cpuid_val[2];
    }

    /* Structured extended features */
    if (max_eax >= 7) {
        invoke_cpuid_ecx(cpuid_val, 7, 0);
        sext_ebx = cpuid_val[1];
    }

    print("Raw features:\n  edx = 0x%08x\n  ecx = 0x%08x\n", feat_edx, feat_ecx);
    print("  ext_edx = 0x%08x\n  ext_ecx = 0x%08x\n", ext_edx, ext_ecx);
    print("  sext_ebx = 0x%08x\n", sext_ebx);
    print("Major ISA features:\n");
    if (TEST(FEAT_EDX_MMX, feat_edx))
        print("  MMX\n");
    if (TEST(FEAT_EDX_SSE, feat_edx))
        print("  SSE\n");
    if (TEST(FEAT_EDX_SSE2, feat_edx))
        print("  SSE2\n");
    if (TEST(FEAT_ECX_SSE3, feat_ecx))
        print("  SSE3\n");
    if (TEST(FEAT_ECX_SSSE3, feat_ecx))
        print("  SSSE3\n");
    if (TEST(FEAT_ECX_SSE41, feat_ecx))
        print("  SSE41\n");
    if (TEST(FEAT_ECX_SSE42, feat_ecx))
        print("  SSE42\n");
    if (TEST(FEAT_ECX_AVX, feat_ecx))
        print("  AVX\n");
}
