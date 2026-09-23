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
 * * Neither the name of ARM Limited nor the names of its contributors may be
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

#define SELFMOD_STATE_CONCAT(x, y) (x##y)
#define MAKE_DR_REG(reg) SELFMOD_STATE_CONCAT(DR_REG_, reg)

/* The number that selfmod_state_writer() adds to its argument.
 * The value is arbitrary but must fit into an immediate.
 */
#define SELFMOD_STATE_WRITER_INCREMENT 7

#ifdef AARCH64
#    define STR_SRC_REG W1
#    define STR_SRC_DR_REG MAKE_DR_REG(STR_SRC_REG)

#    define STR_ADDR_REG X0
#    define STR_ADDR_DR_REG MAKE_DR_REG(STR_ADDR_REG)

#    define ADD_DST_REG X0
#    define ADD_DST_DR_REG MAKE_DR_REG(ADD_DST_REG)

#    define ADD_SRC_REG X2
#    define ADD_SRC_DR_REG MAKE_DR_REG(ADD_SRC_REG)
#else
#    error "Test does not support the target architecture."
#endif

#ifdef X64
#    define TEST_INPUT_VALUE 0x123456789abc0000ULL
#else
#    error "Test does not support the target architecture."
#endif
