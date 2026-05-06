 /* **********************************************************
 * Copyright (c) 2021-2026 Google, Inc.  All rights reserved.
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

/* This is a statically-linked app.
 * XXX i#3107: Pure-asm apps like this are difficult to maintain; things like
 * setting up zmm or comparing mm regs are more easily done in a C+asm app. We
 * need this to be pure-asm so that we can verify exact instruction, load and
 * store counts for the emulated scatter/gather sequence. With support for
 * #3107, we can use annotations to mark app phases and compute those counts
 * for only the intended parts in the C+asm app.
 */
.text
.globl _start
.type _start, @function
        .align   8
_start:
        // Align stack pointer to cache line.
        and      rsp, -16

        // Load data into ymm0/zmm0.
        // drx_expand_scatter_gather picks as scratch the lowest-numbered
        // xmm reg not being used by the scatter/gather instr being expanded.
        // This will be xmm0 in this app. We want to test that ymm0/zmm0 is
        // indeed restored to its app value after the expansion.
#ifdef __AVX512F__
        mov      rax, 0x0123456701234567
        vpinsrq  xmm3, xmm3, rax, 0x00
        mov      rax, 0x89abcdef89abcdef
        vpinsrq  xmm3, xmm3, rax, 0x01
        vinserti64x2 zmm0, zmm0, xmm3, 0

        mov      rax, 0x02468ace02468ace
        vpinsrq  xmm3, xmm3, rax, 0x00
        mov      rax, 0x13579bdf13579bdf
        vpinsrq  xmm3, xmm3, rax, 0x01
        vinserti64x2 zmm0, zmm0, xmm3, 1

        mov      rax, 0x1111222211112222
        vpinsrq  xmm3, xmm3, rax, 0x00
        mov      rax, 0x3333444433334444
        vpinsrq  xmm3, xmm3, rax, 0x01
        vinserti64x2 zmm0, zmm0, xmm3, 2

        mov      rax, 0x5555666655556666
        vpinsrq  xmm3, xmm3, rax, 0x00
        mov      rax, 0x7777888877778888
        vpinsrq  xmm3, xmm3, rax, 0x01
        vinserti64x2 zmm0, zmm0, xmm3, 3

        vmovups  save_mm0, zmm0
#else
        mov      rax, 0x0123456701234567
        vpinsrq  xmm3, xmm3, rax, 0x00
        mov      rax, 0x89abcdef89abcdef
        vpinsrq  xmm3, xmm3, rax, 0x01
        vinserti128 ymm0, ymm0, xmm3, 0

        mov      rax, 0x02468ace02468ace
        vpinsrq  xmm3, xmm3, rax, 0x00
        mov      rax, 0x13579bdf13579bdf
        vpinsrq  xmm3, xmm3, rax, 0x01
        vinserti128 ymm0, ymm0, xmm3, 1

        vmovups  save_mm0, ymm0
#endif

        // Load data into xmm10.
        // We embed data in instrs to avoid adding extra loads.
        mov      eax, 0xdead
        vpinsrd  xmm10, xmm10, eax, 0x00
        mov      eax, 0xbeef
        vpinsrd  xmm10, xmm10, eax, 0x01
        mov      eax, 0x8bad
        vpinsrd  xmm10, xmm10, eax, 0x02
        mov      eax, 0xf00d
        vpinsrd  xmm10, xmm10, eax, 0x03

        // Load indices into xmm11.
        mov      eax, 0x01
        vpinsrd  xmm11, xmm11, eax, 0x00
        mov      eax, 0x03
        vpinsrd  xmm11, xmm11, eax, 0x01
        mov      eax, 0x00
        vpinsrd  xmm11, xmm11, eax, 0x02
        mov      eax, 0x02
        vpinsrd  xmm11, xmm11, eax, 0x03

        // Test 1: Verify correctness of a scatter followed by a gather
        // for the same data.
#ifdef __AVX512F__
        // Scatter xmm10 data to arr, skipping element at index 1 in xmm10.
        mov      ebx, 0xd
        kmovw    k1, ebx
        vpscatterdd [arr + xmm11*4]{k1}, xmm10
#else
        // Emulate scatter instr if not available.
        mov      dword ptr [arr], 0x00008bad
        mov      dword ptr [arr + 0x04*1], 0x0000dead
        mov      dword ptr [arr + 0x04*2], 0x0000f00d
#endif

        // Gather arr data into xmm12, skipping index 2 at xmm12,
        // using same indices as scatter.
        pcmpeqd  xmm13, xmm13
        xor      eax, eax
        vpinsrd  xmm13, xmm13, eax, 0x02
        vpgatherdd xmm12, [arr + xmm11*4], xmm13

        // Compare xmm10 and xmm12.
        // Only zeroth and last elements should be equal.
        mov      eax, 0x00
        vpinsrd  xmm10, xmm10, eax, 0x01
        vpinsrd  xmm10, xmm10, eax, 0x02
        // Compare the two 64-bit quad-words that make up an xmm reg.
        pcmpeqq  xmm12, xmm10
        vpextrd  eax, xmm12, 0
        cmp      eax, 0xffffffff
        jne      incorrect
        vpextrd  eax, xmm12, 2
        cmp      eax, 0xffffffff
        jne      incorrect

        // Test 2: Verify that the scratch reg (here, xmm0) was restored to its
        // original app value.
#ifdef __AVX512F__
        vmovups  zmm3, save_mm0
        vpcmpeqq k1, zmm0, zmm3
        kmovw    ebx, k1
        cmp      ebx, 0xff
        jne      incorrect_scratch
#else
        vmovups  ymm3, save_mm0
        vpcmpeqq ymm2, ymm0, ymm3
        /* Verify equality for each of the 4 qwords individually. */
        vextracti128 xmm1, ymm2, 0
        vpextrd  eax, xmm1, 0
        cmp      eax, 0xffffffff
        jne      incorrect_scratch
        vpextrd  eax, xmm1, 2
        cmp      eax, 0xffffffff
        jne      incorrect_scratch
        vextracti128 xmm1, ymm2, 1
        vpextrd  eax, xmm1, 0
        cmp      eax, 0xffffffff
        jne      incorrect_scratch
        vpextrd  eax, xmm1, 2
        cmp      eax, 0xffffffff
        jne      incorrect_scratch
#endif

        // Test 3: Verify that negative indices are sign extended.
        mov      eax, -0x01
        vpinsrd  xmm11, xmm11, eax, 0x00

#ifdef __AVX512F__
        // Scatter only the zeroth element.
        mov      ebx, 0x1
        kmovw    k1, ebx
        vpscatterdd [arr + xmm11*4]{k1}, xmm10
#else
        // Emulate scatter instr if not available.
        mov      dword ptr [arr_neg], 0xdead
        // Match instr count in the #if block above.
        nop
#endif
        mov      ebx, dword ptr [arr_neg]
        cmp      ebx, 0xdead
        jne      incorrect

        // Gather the element at arr-4 into xmm14, using a negative index
        // same as above scatter.
        pcmpgtd  xmm13, xmm13
        xor      eax, eax
        not      eax
        vpinsrd  xmm13, xmm13, eax, 0x00
        vpgatherdd xmm14, [arr + xmm11*4], xmm13

        // Compare xmm10 and xmm14.
        // Only zeroth elements should be equal. Elements at index 1 and 2
        // in xmm10 were already set to zero above.
        mov      eax, 0x00
        vpinsrd  xmm10, xmm10, eax, 0x03
        pcmpeqq  xmm14, xmm10
        vpextrd  eax, xmm14, 0
        cmp      eax, 0xffffffff
        jne      incorrect
        vpextrd  eax, xmm14, 2
        cmp      eax, 0xffffffff
        jne      incorrect

        // Test 4: Verify that gather into XMM zeroes the upper YMM bits.
        // Initialize ymm4 to all 1s.
        vpcmpeqd ymm4, ymm4, ymm4

        // Set up mask in xmm13 (all 1s)
        vpcmpeqd xmm13, xmm13, xmm13

        // Gather into xmm4.
        // Upper 128 bits of ymm4 should be zeroed.
        vpgatherdd xmm4, [arr + xmm11*4], xmm13

        // Extract upper 128 bits of ymm4 into xmm5.
        vextracti128 xmm5, ymm4, 1

        // Verify xmm5 is all zeroes.
        vptest   xmm5, xmm5
        jnz      incorrect_zeroing

        // Test 5: Verify that gather into XMM zeroes the upper YMM bits even
        // when mask is all 0s.
        // Initialize ymm4 to all 1s.
        vpcmpeqd ymm4, ymm4, ymm4

        // Set up mask in xmm13 (all 0s)
        vpxor    xmm13, xmm13, xmm13

        // Gather into xmm4.
        // Since mask is 0, no elements are gathered.
        // Lower 128 bits of ymm4 should remain all 1s (preserved).
        // Upper 128 bits of ymm4 should be zeroed.
        vpgatherdd xmm4, [arr + xmm11*4], xmm13

        // Verify xmm4 is all ones, since gather uses merge masking
        // and xmm4 was all ones before the vpgatherdd.
        vpcmpeqd xmm6, xmm6, xmm6
        pcmpeqq  xmm4, xmm6
        vpextrd  eax, xmm4, 0
        cmp      eax, 0xffffffff
        jne      incorrect_zeroing_mask0
        vpextrd  eax, xmm4, 2
        cmp      eax, 0xffffffff
        jne      incorrect_zeroing_mask0

        // Verify upper bits of ymm4 are zeroed.
        vextracti128 xmm5, ymm4, 1
        vptest   xmm5, xmm5
        jnz      incorrect_zeroing_mask0

#ifdef __AVX512F__
        // Test 6: Verify that EVEX gather into XMM zeroes the upper ZMM
        // bits (128-511).
        // Initialize zmm4 to all 1s.
        vmovdqu64 zmm4, [ones_512]

        // Set up mask in k1 (all 1s for 4 elements)
        mov      eax, 0xf
        kmovw    k1, eax

        // Gather into xmm4.
        vpgatherdd xmm4 {k1}, [arr + xmm11*4]

        // Store zmm4 to memory to inspect.
        vmovdqu64 [zmm_dest], zmm4

        // Verify upper 384 bits (bytes 16-63) are all zeroes.
        mov      rax, qword ptr [zmm_dest + 16]
        or       rax, qword ptr [zmm_dest + 24]
        or       rax, qword ptr [zmm_dest + 32]
        or       rax, qword ptr [zmm_dest + 40]
        or       rax, qword ptr [zmm_dest + 48]
        or       rax, qword ptr [zmm_dest + 56]
        cmp      rax, 0
        jne      incorrect_avx512_zeroing

        // Test 7: Verify that EVEX gather into XMM zeroes the upper ZMM
        // bits even when mask is 0.
        // Initialize zmm4 to all 1s.
        vmovdqu64 zmm4, [ones_512]

        // Set up mask in k1 (all 0s)
        xor      eax, eax
        kmovw    k1, eax

        // Gather into xmm4.
        vpgatherdd xmm4 {k1}, [arr + xmm11*4]

        // Store zmm4 to memory to inspect.
        vmovdqu64 [zmm_dest], zmm4

        // Verify the xmm part is all ones, since gather uses merge
        // masking and xmm was all ones before the vpgatherdd.
        mov      rax, qword ptr [zmm_dest]
        and      rax, qword ptr [zmm_dest + 8]
        cmp      rax, -1
        jne      incorrect_avx512_zeroing_mask0

        mov      rax, qword ptr [zmm_dest + 16]
        or       rax, qword ptr [zmm_dest + 24]
        or       rax, qword ptr [zmm_dest + 32]
        or       rax, qword ptr [zmm_dest + 40]
        or       rax, qword ptr [zmm_dest + 48]
        or       rax, qword ptr [zmm_dest + 56]
        cmp      rax, 0
        jne      incorrect_avx512_zeroing_mask0

        // Test 8: Verify that EVEX gather into YMM zeroes the upper ZMM
        // bits (256-511).
        // Prepare ymm11 (indices) by duplicating xmm11.
        vinserti128 ymm11, ymm11, xmm11, 1

        // Initialize zmm4 to all 1s.
        vmovdqu64 zmm4, [ones_512]

        // Set up mask in k1 (all 1s for 8 elements)
        mov      eax, 0xff
        kmovw    k1, eax

        // Gather into ymm4.
        vpgatherdd ymm4 {k1}, [arr + ymm11*4]

        // Store zmm4 to memory.
        vmovdqu64 [zmm_dest], zmm4

        // Verify upper 256 bits (bytes 32-63) are all zeroes.
        mov      rax, qword ptr [zmm_dest + 32]
        or       rax, qword ptr [zmm_dest + 40]
        or       rax, qword ptr [zmm_dest + 48]
        or       rax, qword ptr [zmm_dest + 56]
        cmp      rax, 0
        jne      incorrect_avx512_ymm_zeroing

        // Test 9: Verify that EVEX gather into YMM zeroes the upper ZMM
        // bits even when mask is 0.
        // Initialize zmm4 to all 1s.
        vmovdqu64 zmm4, [ones_512]

        // Set up mask in k1 (all 0s)
        xor      eax, eax
        kmovw    k1, eax

        // Gather into ymm4.
        vpgatherdd ymm4 {k1}, [arr + ymm11*4]

        // Store zmm4 to memory.
        vmovdqu64 [zmm_dest], zmm4

        // Verify the ymm part is all ones, because gather is merge
        // masking and it was all ones before the vpgatherdd.
        mov      rax, qword ptr [zmm_dest]
        and      rax, qword ptr [zmm_dest + 8]
        and      rax, qword ptr [zmm_dest + 16]
        and      rax, qword ptr [zmm_dest + 24]
        cmp      rax, -1
        jne      incorrect_avx512_ymm_zeroing_mask0

        // Verify the upper part of zmm is all zero.
        mov      rax, qword ptr [zmm_dest + 32]
        or       rax, qword ptr [zmm_dest + 40]
        or       rax, qword ptr [zmm_dest + 48]
        or       rax, qword ptr [zmm_dest + 56]
        cmp      rax, 0
        jne      incorrect_avx512_ymm_zeroing_mask0
#endif

        // Print comparison result.
        lea      rsi, correct_str
        mov      rdx, 8           // sizeof(correct_str)
        jmp      done_cmp

incorrect:
        lea      rsi, incorrect_str
        mov      rdx, 10          // sizeof(incorrect_str)
        jmp      done_cmp
incorrect_scratch:
        lea      rsi, incorrect_scratch_str
        mov      rdx, 18
        jmp      done_cmp
incorrect_zeroing:
        lea      rsi, incorrect_zeroing_str
        mov      rdx, 28          // sizeof(incorrect_zeroing_str)
        jmp      done_cmp
incorrect_zeroing_mask0:
        lea      rsi, incorrect_zeroing_mask0_str
        mov      rdx, 34          // sizeof(incorrect_zeroing_mask0_str)
        jmp      done_cmp
incorrect_avx512_zeroing:
        lea      rsi, incorrect_avx512_zeroing_str
        mov      rdx, 30          // sizeof(incorrect_avx512_zeroing_str)
        jmp      done_cmp
incorrect_avx512_zeroing_mask0:
        lea      rsi, incorrect_avx512_zeroing_mask0_str
        mov      rdx, 36          // sizeof(incorrect_avx512_zeroing_mask0_str)
        jmp      done_cmp
incorrect_avx512_ymm_zeroing:
        lea      rsi, incorrect_avx512_ymm_zeroing_str
        mov      rdx, 30          // sizeof(incorrect_avx512_ymm_zeroing_str)
        jmp      done_cmp
incorrect_avx512_ymm_zeroing_mask0:
        lea      rsi, incorrect_avx512_ymm_zeroing_mask0_str
        mov      rdx, 36          // sizeof(incorrect_avx512_ymm_zeroing_mask0_str)
done_cmp:
        mov      rdi, 2           // stderr
        mov      eax, 1           // SYS_write
        syscall

        // Print end message.
        mov      rdi, 2           // stderr
        lea      rsi, hello_str
        mov      rdx, 13          // sizeof(hello_str)
        mov      eax, 1           // SYS_write
        syscall

        // Exit.
        mov      rdi, 0           // exit code
        mov      eax, 231         // SYS_exit_group
        syscall

        .data
        .align   8
hello_str:
        .string  "Hello world!\n"
correct_str:
        .string  "Correct\n"
incorrect_str:
        .string  "Incorrect\n"
incorrect_scratch_str:
        .string  "Incorrect scratch\n"
incorrect_zeroing_str:
        .string  "Incorrect YMM upper zeroing\n"
incorrect_zeroing_mask0_str:
        .string  "Incorrect YMM upper zeroing mask0\n"
arr_neg:
        .zero    4
arr:
        .zero    16
save_mm0:
        .zero    64
incorrect_avx512_zeroing_str:
        .string  "Incorrect AVX-512 XMM zeroing\n"
incorrect_avx512_zeroing_mask0_str:
        .string  "Incorrect AVX-512 XMM zeroing mask0\n"
incorrect_avx512_ymm_zeroing_str:
        .string  "Incorrect AVX-512 YMM zeroing\n"
incorrect_avx512_ymm_zeroing_mask0_str:
        .string  "Incorrect AVX-512 YMM zeroing mask0\n"
ones_512:
        .quad    -1, -1, -1, -1, -1, -1, -1, -1
zmm_dest:
        .zero    64
