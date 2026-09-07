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

#    ifdef X64
    static const uint64 expected = 0x123456789abc0000ULL;
#    else
#        error "Test does not support the target architecture."
#    endif

    /* Create a writable/executable page and copy our ret instruction there. */
    uint *code = (uint *)allocate_mem(PAGE_SIZE, ALLOW_READ | ALLOW_WRITE | ALLOW_EXEC);
    if (code == NULL) {
        print("failed to allocate code\n");
        return 1;
    }

    *code = ret;
    tools_clear_icache(code, code + 1);

    ((generated_func_t)code)();

    uint64 result = selfmod_state_writer(code, ret, expected);
    if (result != expected + 7) {
        print("register state corrupted: " PFX " != " PFX "\n", result, expected + 7);
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
        movz    ADD_SRC_REG, #0x1234, lsl 48
        movk    ADD_SRC_REG, #0x5678, lsl 32
        movk    ADD_SRC_REG, #0x9abc, lsl 16
        str     STR_SRC_REG, [STR_ADDR_REG]
        add     ADD_DST_REG, ADD_SRC_REG, #7
        ret
#    else
#        error "Test does not support the target architecture."
#    endif
#undef FUNCNAME

#endif
