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
 * Performance matters here so we have simple optimizations.
 * Run core_unit_tests to see comparisons to libc times.
 */
        DECLARE_FUNC(memcpy)
GLOBAL_LABEL(memcpy:)
        // We're supposed to return x0, so make a copy we can modify.
        mov      x3, x0
        // If < 8, go to final 1-byte-at-a-time path.
        cmp      x2, #8
        b.lo     memcpy_post_unaligned
        // 1-byte path until reach 8-byte-aligned aligned start.
        mov      x4, #0x7
        ands     x4, x3, x4
        b.eq     memcpy_aligned_loop
        mov      x6, #8
        sub      x5, x6, x4 // Count of unaligned at start.
        sub      x2, x2, x5 // Update total count.
memcpy_pre_unaligned:
        ldrb     w6, [x1], #1
        strb     w6, [x3], #1
        subs     x5, x5, #1
        b.ne     memcpy_pre_unaligned
memcpy_aligned_loop:
        cmp      x2, #8
        b.lo     memcpy_post_unaligned
        ldr      x6, [x1], #8
        str      x6, [x3], #8
        sub      x2, x2, #8
        b        memcpy_aligned_loop
memcpy_post_unaligned:
        cbz      x2, memcpy_done
        ldrb     w6, [x1], #1
        strb     w6, [x3], #1
        sub      x2, x2, #1
        cbnz     x2, memcpy_post_unaligned
memcpy_done:
        ret
        END_FUNC(memcpy)

/* Private memset.
 * Performance matters here so we have simple optimizations.
 * Run core_unit_tests to see comparisons to libc times.
 */
        DECLARE_FUNC(memset)
GLOBAL_LABEL(memset:)
        // We're supposed to return x0, so make a copy we can modify.
        mov      x6, x0
        // If not setting zero, go to slow path.
        cbnz     w1, memset_postzva_unaligned
        // If < 128 size, go to slow path.
        cmp      x2, #128
        b.lo     memset_postzva_unaligned
        // See whether DC ZVA is available: if not, go to slow path.
        mrs      x3, dczid_el0
        tbnz     x3, #4, memset_postzva_unaligned // If 5th bit is 1: disabled.
        // Get DC ZVA block size in bytes. The bottom 4 bits hold log_2 in words.
        and      x3, x3, #0xf
        add      x3, x3, #2 // Shift an extra 2 for word size == 4.
        mov      x4, #1
        lsl      x3, x4, x3 // 1<<(log_2 + 2) = bytes
        // If memset size < block size, go to slowpath.
        // On some cores, the block size is as high as 512 bytes, though usually
        // it's 64 bytes.
        cmp      x2, x3
        b.lo     memset_postzva_unaligned
        // Slow path until reach aligned start.
        sub      x4, x3, #1 // Mask for block size.
        ands     x4, x6, x4
        b.eq     memset_alignedzva_loop
        sub      x5, x3, x4 // Count of unaligned at start.
        sub      x2, x2, x5 // Update total count.
memset_prezva_unaligned:
        strb     w1, [x6], #1
        subs     x5, x5, #1
        b.ne     memset_prezva_unaligned
memset_alignedzva_loop:
        cmp      x2, x3
        b.lo     memset_postzva_unaligned
        dc       zva, x6
        add      x6, x6, x3 // Add block size to dest.
        sub      x2, x2, x3 // Update total count.
        b        memset_alignedzva_loop
memset_postzva_unaligned:
        // Now we try to do 8 aligned bytes at a time.
        // If < 8, go to final 1-byte-at-a-time path.
        cmp      x2, #8
        b.lo     memset_post8_unaligned
        // Replicate the byte to write across a GPR.
        // (We could use "dup" if we want to assume vector register availability.)
        // We multiple the bottom byte by 0x0101010101010101.
        and      w7, w1, #0xff
        movk     x8, #0x0101, lsl #0
        movk     x8, #0x0101, lsl #16
        movk     x8, #0x0101, lsl #32
        movk     x8, #0x0101, lsl #48
        mul      x7, x7, x8
        // 1-byte path until reach 8-byte-aligned aligned start.
        mov      x4, #0x7
        ands     x4, x6, x4
        b.eq     memset_aligned8_loop
        mov      x3, #8
        sub      x5, x3, x4 // Count of unaligned at start.
        sub      x2, x2, x5 // Update total count.
memset_pre8_unaligned:
        strb     w1, [x6], #1
        subs     x5, x5, #1
        b.ne     memset_pre8_unaligned
memset_aligned8_loop:
        cmp      x2, #8
        b.lo     memset_post8_unaligned
        str      x7, [x6], #8
        sub      x2, x2, #8
        b        memset_aligned8_loop
memset_post8_unaligned:
        cbz      x2, memset_done
        strb     w1, [x6], #1
        subs     x2, x2, #1
        b.ne     memset_post8_unaligned
memset_done:
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
