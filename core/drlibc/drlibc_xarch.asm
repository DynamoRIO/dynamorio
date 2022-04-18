/* **********************************************************
 * Copyright (c) 2014-2020 Google, Inc.  All rights reserved.
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

/*
 * cross-platform assembly and trampoline code
 */

#include "../arch/asm_defines.asm"
START_FILE

/* For AArch64, drlibc has no references to unexpected_return, and in fact we
 * have a relocation reachability error if we include it here (i#4304), so
 * we limit its use in drlibc to x86 or arm.
 */
#ifndef AARCH64
DECL_EXTERN(d_r_internal_error)

/* For debugging: report an error if the function called by call_switch_stack()
 * unexpectedly returns.  Also used elsewhere.
 */
        DECLARE_FUNC(unexpected_return)
GLOBAL_LABEL(unexpected_return:)
        CALLC3(GLOBAL_REF(d_r_internal_error), HEX(0), HEX(0), HEX(0))
        /* d_r_internal_error normally never returns */
        /* Infinite loop is intentional.  Can we do better in release build?
         * XXX: why not a debug instr?
         */
        JUMP  GLOBAL_REF(unexpected_return)
        END_FUNC(unexpected_return)
#endif

END_FILE
