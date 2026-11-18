/* **********************************************************
 * Copyright (c) 2026 Bryan Berns  All rights reserved.
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

/* Test drwrap post-call callbacks for short calls at the start of a readable page
 * after an unreadable page.  Each direct, register-indirect, and memory-indirect
 * call runs three times to exercise both initial and cached post-call instrumentation.
 */
#include "tools.h"
#include <string.h>

#ifndef X86
#    error This test emits x86 instructions.
#endif

/* The volatile store keeps the compiler from removing the communication call. */
static void *volatile target_echo;

/* The client wraps this function to obtain and wrap the generated call target. */
NOINLINE void EXPORT
client_wrapped_to_communicate_target(void *call_target)
{
    target_echo = call_target;
}

int
main(void)
{
    /* Leave space between the call/ret bytes, leaf callee, pointer slot, and setup
     * code.  These aligned offsets keep the regions separate; their values are arbitrary.
     */
    enum {
        CALL_TARGET_OFFSET = 32,
        CALL_TARGET_SLOT_OFFSET = 48,
        SETUP_ENTRY_OFFSET = 64,
        MEMORY_INDIRECT_CALL = 2,
    };
    const byte CALLS[][6] = {
        { 0xe8, CALL_TARGET_OFFSET - 5, 0, 0, 0, 0xc3 }, /* call call_target; ret */
        { 0xff, 0xd0, 0xc3 },                            /* call xax; ret */
        { 0xff, 0x50, 0, 0xc3 },                         /* call [xax+disp8=0]; ret */
    };
    const size_t NUM_CALLS = sizeof(CALLS) / sizeof(CALLS[0]);
    INIT();
    /* Each call has two pages: an inaccessible first page and code on the second. */
    byte *memory = (byte *)allocate_mem(PAGE_SIZE * 2 * NUM_CALLS, 0);
    assert(memory != NULL);
    for (size_t i = 0; i < NUM_CALLS; ++i) {
        byte *page_start_call = memory + PAGE_SIZE * (2 * i + 1);
        byte *call_target = page_start_call + CALL_TARGET_OFFSET;
        byte *call_target_slot = page_start_call + CALL_TARGET_SLOT_OFFSET;
        byte *indirect_operand =
            i == MEMORY_INDIRECT_CALL ? call_target_slot : call_target;
        byte *setup_entry = page_start_call + SETUP_ENTRY_OFFSET;
        byte *cursor = setup_entry;
        int displacement;
        protect_mem(page_start_call, PAGE_SIZE, ALLOW_READ | ALLOW_WRITE);

        /* The callee returns to the ret after the page-start call, which returns to
         * main.  The memory-indirect call reads the callee address from the pointer slot.
         */
        memcpy(page_start_call, CALLS[i], sizeof(CALLS[i]));
        *call_target = 0xc3;
        memcpy(call_target_slot, &call_target, sizeof(call_target));

        /* Set xax before jumping backward so the tested call is the first instruction
         * on its page.  The jump preserves main's return address on the stack.
         */
#ifdef X64
        *cursor++ = 0x48;
#endif
        *cursor++ = 0xb8; /* mov xax, indirect_operand */
        memcpy(cursor, &indirect_operand, sizeof(indirect_operand));
        cursor += sizeof(indirect_operand);
        *cursor++ = 0xe9; /* jmp page_start_call */
        displacement = (int)(page_start_call - (cursor + sizeof(displacement)));
        memcpy(cursor, &displacement, sizeof(displacement));
        protect_mem(page_start_call, PAGE_SIZE, ALLOW_READ | ALLOW_EXEC);
#ifdef WINDOWS
        assert(FlushInstructionCache(GetCurrentProcess(), page_start_call, PAGE_SIZE));
#endif
        client_wrapped_to_communicate_target(call_target);
        for (int j = 0; j < 3; ++j)
            ((void (*)(void))setup_entry)();
    }
    free_mem((char *)memory, PAGE_SIZE * 2 * NUM_CALLS);
    print("boundary calls completed\n");
    return 0;
}
