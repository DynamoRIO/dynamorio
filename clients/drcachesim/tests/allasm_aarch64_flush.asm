/* **********************************************************
 * Copyright (c) 2020-2023 Google, Inc. All rights reserved.
 * Copyright (c) 2016 ARM Limited. All rights reserved.
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

        .global  _start

        .align   6
_start:
        // Align stack pointer and get some space.
        mov      x0, sp
        bic      x0, x0, #63
        mov      x1, x0        // x1 is top of region
        sub      x0, x0, #1024 // x0 is bottom of region
        mov      sp, x0

#ifdef __APPLE__
        // XXX: Try to include asm_defines.asm for AARCH64_ADR_GOT().
        adrp     x0, helloworld@PAGE
        add      x0, x0, helloworld@PAGEOFF
#else
        adr      x0, helloworld
#endif
        adr      x1, .

        // Data cache flush operations.
        dc       civac, x0
        dc       cvac, x0
        dc       cvau, x0

        // Instruction cache flush operations.
        ic       ivau, x1

        // Exit.
        mov      w0, #1            // stdout
#ifdef __APPLE__
        adrp     x1, helloworld@PAGE
        add      x1, x1, helloworld@PAGEOFF
#else
        adr      x1, helloworld
#endif
        mov      w2, #14           // sizeof(helloworld)
        mov      w8, #64           // SYS_write
        svc      #0
        mov      w0, #0            // status
        mov      w8, #94           // SYS_exit_group
        svc      #0

        .data
        .align   6
helloworld:
        .ascii   "Hello, world!\n"
