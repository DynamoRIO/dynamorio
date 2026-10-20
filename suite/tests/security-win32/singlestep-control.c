/* **********************************************************
 * Copyright (c) Bryan Berns  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "tools.h"
#ifdef X64
#    define IP Rip
#    define SP Rsp
#    define AX Rax
#else
#    define IP Eip
#    define SP Esp
#    define AX Eax
#endif
#define TRAP_FLAG 0x100
#define ZERO_FLAG 0x40
static byte *code, *expected_pc;
static int test_index, steps;
static int faults, readable_value;
static BOOL wow64;
static const int offsets[] = { 0, 16, 32, 16, 48, 80, 80 };
static LONG CALLBACK
handler(EXCEPTION_POINTERS *exception)
{
    CONTEXT *context = exception->ContextRecord;
    if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT) {
        context->IP = (ptr_uint_t)(code + offsets[test_index] + 1);
        context->EFlags |= TRAP_FLAG | ZERO_FLAG;
        if (test_index == 3)
            context->EFlags &= ~ZERO_FLAG; /* Exercise the fall-through exit as well. */
        if (test_index == 2)
            expected_pc = *(byte **)(ptr_uint_t)context->SP;
        if (test_index >= 5)
            context->AX = 0;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (test_index >= 5 &&
        exception->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        assert((byte *)(ptr_uint_t)context->IP == code + 81);
        assert(exception->ExceptionRecord->ExceptionAddress == code + 81);
        assert(((context->EFlags & TRAP_FLAG) != 0) == !wow64);
        ++faults;
        /* Retry the faulting instruction with stepping cancelled or retained. */
        context->AX = (ptr_uint_t)&readable_value;
        if (test_index == 5)
            context->EFlags &= ~TRAP_FLAG;
        else
            context->EFlags |= TRAP_FLAG;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (exception->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP)
        return EXCEPTION_CONTINUE_SEARCH;
    if (test_index == 5) {
        print("cancelled single step was delivered\n");
        ExitProcess(1);
    }
    if (exception->ExceptionRecord->ExceptionAddress != expected_pc ||
        (byte *)(ptr_uint_t)context->IP != expected_pc) {
        print("incorrect step destination in case %d\n", test_index);
        ExitProcess(1);
    }
    ++steps;
    context->EFlags &= ~TRAP_FLAG;
    return EXCEPTION_CONTINUE_EXECUTION;
}
int
main(void)
{
    const byte nop[] = { 0xcc, 0x90, 0xc3 };
    const byte branch[] = { 0xcc, 0x74, 2, 0x90, 0x90, 0xc3 };
    const byte ret[] = { 0xcc, 0xc3 };
    const byte relative[] = { 0xcc, 0x8b, 0x05, 9, 0, 0, 0, 0xc3 };
    const byte fault[] = { 0xcc, 0x8b, 0x00, 0xc3 };
    const int ends[] = { 2, 21, 0, 19, 55 };
    PVOID registration;
    DWORD old;
    INIT();
    assert(IsWow64Process(GetCurrentProcess(), &wow64));
    code = VirtualAlloc(NULL, PAGE_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    assert(code != NULL);
    memcpy(code, nop, sizeof(nop));
    memcpy(code + 16, branch, sizeof(branch));
    memcpy(code + 32, ret, sizeof(ret));
    memcpy(code + 48, relative, sizeof(relative));
    memcpy(code + 80, fault, sizeof(fault));
    *(int *)(code + 64) = 0x12345678;
    assert(VirtualProtect(code, PAGE_SIZE, PAGE_EXECUTE_READ, &old));
    assert(FlushInstructionCache(GetCurrentProcess(), code, PAGE_SIZE));
    registration = AddVectoredExceptionHandler(1, handler);
    assert(registration != NULL);
    // Exception continuation sets TF for exactly one application instruction.
    for (test_index = 0; test_index < IF_X64_ELSE(5, 4); ++test_index) {
        expected_pc = code + ends[test_index];
        ((void (*)(void))(code + offsets[test_index]))();
        if (steps != test_index + 1)
            print("case=%d steps=%d\n", test_index, steps);
        assert(steps == test_index + 1);
    }

    /* A fault handler chooses whether the retried instruction is single-stepped. */
    for (test_index = 5; test_index <= 6; ++test_index) {
        expected_pc = code + 83;
        ((void (*)(void))(code + offsets[test_index]))();
        assert(faults == test_index - 4);
        assert(steps == IF_X64_ELSE(5, 4) + test_index - 5);
    }
    assert(RemoveVectoredExceptionHandler(registration));
    assert(VirtualFree(code, 0, MEM_RELEASE));
    print("continuation step destinations preserved\n");
    return 0;
}
