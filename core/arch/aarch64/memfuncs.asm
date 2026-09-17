/* **********************************************************
 * Copyright (c) 2019-2026 Google, Inc. All rights reserved.
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

/*
 * memfuncs.asm: Contains our custom memcpy and memset routines.
 *
 * See the long comment at the top of x86/memfuncs.asm.
 */

#include "../asm_defines.asm"
START_FILE
#ifdef UNIX

/* Private memcpy.
 * XXX i#1569: We should optimize this as it can be on the critical path.
 */
        DECLARE_FUNC(memcpy)
GLOBAL_LABEL(memcpy:)
        mov      x3, ARG1
        cbz      ARG3, 2f
1:      ldrb     w4, [ARG2], #1
        strb     w4, [x3], #1
        sub      ARG3, ARG3, #1
        cbnz     ARG3, 1b
2:      ret
        END_FUNC(memcpy)

/* Private memset.
 * Performance matters here so we have a fastpath, currently limited to
 * setting memory to 0.
 * XXX i#8125: Add a fastpath for non-0 values.
 * Run core_unit_tests to see comparisons to libc times.
 */
        DECLARE_FUNC(memset)
GLOBAL_LABEL(memset:)
        // We're supposed to return x0, so make a copy we can modify.
        mov      x6, x0
        // If not setting zero, go to slow path.
        cbnz     w1, slow_path
        // If < 128 size, go to slow path.
        cmp      x2, #128
        b.lo     slow_path
        // See whether DC ZVA is available: if not, go to slow path.
        mrs      x3, dczid_el0
        tbnz     x3, #4, slow_path // If 5th bit is 1: disabled.
        // Get DC ZVA block size in bytes. The bottom 4 bits hold log_2 in words.
        and      x3, x3, #0xf
        add      x3, x3, #2 // Shift an extra 2 for word size == 4.
        mov      x4, #1
        lsl      x3, x4, x3 // 1<<(log_2 + 2) = bytes
        // Slow path until reach aligned start.
        sub      x4, x3, #1 // Mask for block size.
        ands     x4, x6, x4
        b.eq     aligned_loop
        sub      x5, x3, x4 // Count of unaligned at start.
        sub      x2, x2, x5 // Update total count.
pre_unaligned:
        strb     w1, [x6], #1
        subs     x5, x5, #1
        b.ne     pre_unaligned
aligned_loop:
        cmp      x2, x3
        b.lo     slow_path
        dc       zva, x6
        add      x6, x6, x3 // Add block size to dest.
        sub      x2, x2, x3 // Update total count.
        b        aligned_loop
slow_path:
        cbz      x2, done
slow_loop_or_post_unaligned:
        strb     w1, [x6], #1
        subs     x2, x2, #1
        b.ne     slow_loop_or_post_unaligned
done:
        ret
        END_FUNC(memset)

/* See x86.asm notes about needing these to avoid gcc invoking *_chk */
.global __memcpy_chk
HIDDEN(__memcpy_chk)
WEAK(__memcpy_chk)
.set __memcpy_chk,GLOBAL_REF(memcpy)

.global __memset_chk
HIDDEN(__memset_chk)
WEAK(__memset_chk)
.set __memset_chk,GLOBAL_REF(memset)

#endif /* UNIX */

END_FILE
