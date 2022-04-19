/* **********************************************************
 * Copyright (c) 2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2005 VMware, Inc.  All rights reserved.
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

/* except.h  shared exception info routines */
/* NOTE this is not really a header! */

#ifndef EXCEPT_H
#define EXCEPT_H

#include "tools.h"

#define initialize_registry_context() /* stick to a known value */ \
    __asm { push 0 }                                               \
    __asm                                                          \
    {                                                              \
        popfd                                                      \
    }                                                              \
    __asm { mov  ebx, 0xbbcdcdcd }                                  \
    __asm                                                          \
    {                                                              \
        mov ecx, 0xcccdcdcd                                        \
    }                                                              \
    __asm { mov  edx, 0xddcdcdcd }                                  \
    __asm                                                          \
    {                                                              \
        mov edi, 0xeecdcdcd                                        \
    }                                                              \
    __asm { mov  esi, 0xffcdcdcd }

#define NO_DEBUG_REGISTERS /* We shouldn't be messing with these registers, \
                              so diffs here maybe not our fault (VMware's?) */

void
dump_context_info(CONTEXT *context, int all)
{
#define DUMP(r) print(#r "=0x%08x ", context->r);
    /* Avoid trailing spaces. */
#define DUMP_NOSPACE(r) print(#r "=0x%08x", context->r);
#define DUMP_DOUBLE_EOL(r) print(#r "=0x%08x\n\n  ", context->r);
#define DUMP_EOL(r) print(#r "=0x%08x\n  ", context->r);
    print("  ");
    DUMP_EOL(ContextFlags);

    if (all || context->ContextFlags & CONTEXT_INTEGER) {
        DUMP(Edi);
        DUMP(Esi);
        DUMP_EOL(Ebx);
        DUMP(Edx);
        DUMP_DOUBLE_EOL(Ecx);
        DUMP_EOL(Eax);
    }

    if (all || context->ContextFlags & CONTEXT_CONTROL) {
        DUMP(Ebp);
        DUMP(Eip);
        DUMP_EOL(SegCs); // MUST BE SANITIZED
        /* only printing low word - RF is different between SP0 & SP4 */
        DUMP(EFlags & 0xFFFF);
        DUMP(Esp);
        DUMP_EOL(SegSs);
    }

#ifndef NO_DEBUG_REGISTERS
    if (all || context->ContextFlags & CONTEXT_DEBUG_REGISTERS) {
        DUMP(Dr0);
        DUMP(Dr1);
        DUMP(Dr2);
        DUMP_EOL(Dr3);
        DUMP(Dr6);
        DUMP_EOL(Dr7);
    }
#endif

    if (all || context->ContextFlags & CONTEXT_FLOATING_POINT) {
        print("<floating point area>\n  ");
    }

    if (all || context->ContextFlags & CONTEXT_SEGMENTS) {
        DUMP(SegGs);
        DUMP(SegFs);
        DUMP(SegEs);
        DUMP_NOSPACE(SegDs);
    }

#undef DUMP
    print("\n");
}

void
dump_exception_info(EXCEPTION_RECORD *exception, CONTEXT *context)
{
    print("    exception code = 0x%08x, ExceptionFlags=0x%08x\n"
          "    record=%p, params=%d\n",
          exception->ExceptionCode, exception->ExceptionFlags,
          exception->ExceptionRecord, /* follow if non NULL */
          exception->NumberParameters);
    if (exception->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        print("    PC 0x%08x tried to %s address 0x%08x\n", exception->ExceptionAddress,
              (exception->ExceptionInformation[0] == 0) ? "read" : "write",
              exception->ExceptionInformation[1]);
    }
    print("    pc=0x%08x eax=0x%08x\n", context->Eip, context->Eax);
    dump_context_info(context, 0); /* existing context */
}

#endif
