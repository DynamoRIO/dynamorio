/* ****************************************************
 * Copyright (c) 2026 Arm Limited  All rights reserved.
 * ***************************************************/

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
 * * Neither the name of Arm Limited nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL ARM LIMITED OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef ASM_CODE_ONLY

#    include "tools.h"
#    include "selfmod-state-shared.h"

/*
 * Write `value` to `*target` and return `live+SELFMOD_STATE_WRITER_INCREMENT`.
 * We test that the value of `live` is not corrupted when its register is spilled
 * by a client across the store to *target.
 */
extern uint64
selfmod_state_writer(uint *target, uint value, uint64 live);

typedef void (*generated_func_t)(void);

int
main(int argc, char **argv)
{
#    ifdef AARCH64
    static const uint ret = 0xd65f03c0;
#    else
#        error "Test does not support the target architecture."
#    endif

    static const ptr_uint_t expected = TEST_INPUT_VALUE + SELFMOD_STATE_WRITER_INCREMENT;

    /* Create a writable/executable page and copy our ret instruction there. */
    uint *code = (uint *)allocate_mem(PAGE_SIZE, ALLOW_READ | ALLOW_WRITE | ALLOW_EXEC);
    if (code == NULL) {
        print("failed to allocate code\n");
        return 1;
    }

    *code = ret;
    tools_clear_icache(code, code + 1);

    /* Call the generated code to make sure it is in the code cache. */
    ((generated_func_t)code)();

    ptr_uint_t result = selfmod_state_writer(code, ret, TEST_INPUT_VALUE);
    if (result != expected) {
        print("register state corrupted: " PFX " != " PFX "\n", result, expected);
        return 1;
    }

    print("state preserved\n");
    free_mem((char *)code, PAGE_SIZE);
    return 0;
}

#else /* ASM_CODE_ONLY */

#    include "asm_defines.asm"
#    include "client-interface/selfmod-state-shared.h"
/* clang-format off */
START_FILE

#define FUNCNAME selfmod_state_writer
    DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#    ifdef AARCH64
        /* The client looks for this exact sequence of instructions. Any changes here
         * will need corresponding changes in is_test_store() in the client.
         */
        nop
        /* selfmod_state_writer(STR_ADDR, STR_SRC, ADD_SRC)
        /* Client clobbers ADD_SRC_REG here.
         * The address we are writing to is in the read/write/execute page which DR will
         * have set to read-only to detect writes. The str will fault and DR returns from
         * the signal handler back to the dispatcher (rather than directly back to fcache
         * PC). We test that app state is correctly preserved in this situation.
         */
        str     STR_SRC_REG, [STR_ADDR_REG]
        /* Client restores ADD_SRC_REG here, but the fault means the restore isn't
         * reached.
         */
        add     ADD_DST_REG, ADD_SRC_REG, #(SELFMOD_STATE_WRITER_INCREMENT)
        ret
#    else
#        error "Test does not support the target architecture."
#    endif
#undef FUNCNAME

#endif
