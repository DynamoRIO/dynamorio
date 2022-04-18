/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
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

#include <windows.h>
#include "tools.h"

#define DEFAULT_TARGET_ADDR 0x4

static int exception_location;
static int target_addr;
static int reg_eflags;
static int reg_eax;
static int reg_ebx;
static int reg_ecx;
static int reg_edx;
static int reg_edi;
static int reg_esi;
static int reg_ebp;
static int reg_esp;

/* FIXME : would it be better to set some of these register to known values? */
static void
fault()
{
    __asm {
        lea   eax, [exception_location_tag]
        mov   exception_location, eax
        pushfd
        pop   eax
        mov   reg_eflags, eax
        mov   eax, DEFAULT_TARGET_ADDR
        mov   reg_eax, eax
        mov   reg_ebx, ebx
        mov   reg_ecx, ecx
        mov   reg_edx, edx
        mov   reg_edi, edi
        mov   reg_esi, esi
        mov   reg_esp, esp
        mov   reg_ebp, ebp
      exception_location_tag:
        mov   dword ptr [eax], 0x2
    }
}

static void
fault_selfmod()
{
    __asm {
        pushfd
        pop   eax
        mov   reg_eflags, eax
        mov   reg_ecx, ecx
        mov   reg_edx, edx
        mov   reg_edi, edi
        mov   reg_esi, esi
        mov   reg_esp, esp
        mov   reg_ebp, ebp
        lea   eax, [selfmod_location_tag]
        mov   exception_location, eax
        lea   eax, [exception_location_tag]
        mov   ebx, [eax]
        mov   reg_ebx, ebx
        mov   reg_eax, eax
        mov   target_addr, eax
      selfmod_location_tag:
        mov   [eax], ebx
        mov   exception_location, eax
        mov   eax, DEFAULT_TARGET_ADDR
        mov   reg_eax, eax
      exception_location_tag:
        mov   dword ptr [eax], 0x2
    }
}

void
do_run(void (*func)(), uint *target_addr_location)
{
    EXCEPTION_RECORD exception;
    CONTEXT *context;
    int slot;

    __try {
        __try {
            __try {
                func();
                print("At statement after exception\n");
            } __except (context = (GetExceptionInformation())->ContextRecord,
                        context->CXT_XAX = (unsigned long)&slot,
                        EXCEPTION_CONTINUE_EXECUTION) {
                print("Inside first handler (should NOT be printed)\n");
            }
            print("At statement after 1st try-except\n");
            __try {
                func();
                print("This should NOT be printed1\n");
            } __except (EXCEPTION_CONTINUE_SEARCH) {
                print("This should NOT be printed2\n");
            }
        } __finally {
            print("Finally!\n");
        }
        print("At statement after 2nd try-finally (should NOT be printed)\n");
    } __except (exception = *(GetExceptionInformation())->ExceptionRecord,
                context = (GetExceptionInformation())->ContextRecord,
                (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
                    ? EXCEPTION_EXECUTE_HANDLER
                    : EXCEPTION_CONTINUE_SEARCH) {
        print("Caught my own memory access violation, ignoring it!\n");
        if (exception.ExceptionAddress == (void *)exception_location &&
            exception.ExceptionInformation[1] == *target_addr_location &&
            exception.ExceptionInformation[0] == 1) {
            print("Exception address and target match!\n");
            if (reg_eax == context->CXT_XAX && reg_ebx == context->CXT_XBX &&
                reg_ecx == context->CXT_XCX && reg_edx == context->CXT_XDX &&
                reg_esi == context->CXT_XSI && reg_edi == context->CXT_XDI &&
                reg_esp == context->CXT_XSP && reg_ebp == context->CXT_XBP &&
                reg_eflags == (context->EFlags & PUSHF_MASK)) {
                print("Register match!\n");
            } else {
                print("Register mismatch!");
            }
        } else {
            print("PC " PFX " (expected " PFX ") tried to %s address " PFX
                  " (expected " PFX ")\n",
                  exception.ExceptionAddress, exception_location,
                  (exception.ExceptionInformation[0] == 0) ? "read" : "write",
                  exception.ExceptionInformation[1], *target_addr_location);
        }
    }
    print("After exception handler\n");
}

int
main()
{
    uint default_target = DEFAULT_TARGET_ADDR;
    do_run(fault, &default_target);
    protect_mem(fault_selfmod, PAGE_SIZE, ALLOW_READ | ALLOW_WRITE | ALLOW_EXEC);
    do_run(fault_selfmod, &default_target);
    /* reprotect page, faulting address/target is now the selfmod write */
    protect_mem(fault_selfmod, PAGE_SIZE, ALLOW_READ | ALLOW_EXEC);
    do_run(fault_selfmod, &target_addr);
    return 0;
}
