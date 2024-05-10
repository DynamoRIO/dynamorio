/* **********************************************************
 * Copyright (c) 2022 Rivos, Inc.  All rights reserved.
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
 * * Neither the name of Rivos, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL RIVOS, INC. OR CONTRIBUTORS BE LIABLE
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
 * Optimize private memcpy by using loop unrolling.
 */
        DECLARE_FUNC(memcpy)
GLOBAL_LABEL(memcpy:)
        addi     t6, x0, 32
        mv       t0, ARG2 /* Save dst for return */
copy32_: 
        /* When size is greater than 32, we use 4 ld/sd pairs 
         * to copy 4*8=32 bytes in each iteration. 
         * When size is less than 32, we jump to copy_remain and 
         * use other optimized copy methods.
         */
        blt      ARG3, t6, copy_remain
        ld       t1, 0(ARG2)
        ld       t2, 8(ARG2)
        ld       t3, 16(ARG2)
        ld       t4, 24(ARG2)
        sd       t1, 0(ARG1)
        sd       t2, 8(ARG1)
        sd       t3, 16(ARG1)
        sd       t4, 24(ARG1)
        addi     ARG3, ARG3, -32
        addi     ARG1, ARG1, 32
        addi     ARG2, ARG2, 32
        j        copy32_

copy_remain:
        add      a6, ARG2, ARG3 /* a6 = src + size */
        add      a7, ARG1, ARG3 /* a7 = dst + size */
        addi     t6, x0, 8
        bge      ARG3, t6, copy8_32
        addi     t6, x0, 4
        bge      ARG3, t6, copy4_8
        bgtz     ARG3, copy0_4
        j        copyexit

copy0_4:
        /* 0 < size < 4:
         * Using a trick to avoid branches.
         * t2 and t3 may be redundant.
         * size = 1: t1 = src[0], t2 = src[0], t3 = src[0]
         * size = 2: t1 = src[0], t2 = src[1], t3 = src[1]
         * size = 3: t1 = src[0], t2 = src[1], t3 = src[2]
         */
        srli     t4, ARG3, 1
        add      t5, t4, ARG1
        add      t4, t4, ARG2
        lbu      t1, 0(ARG2)
        lbu      t2, -1(a6)
        lbu      t3, 0(t4)
        sb       t1, 0(ARG1)
        sb       t2, -1(a7)
        sb       t3, 0(t5)
        j        copyexit

copy4_8:
        /* 4 < size < 8:
         * Step1: Use lwu/sw pair to copy src[0]~src[3] to dst[0]~dst[3].
         * Step2: Copy src[size-4]~src[size-1] to dst[size-4]~dst[size-1].
         * Some overlap here, but the overhead is negligible.
         */
        lwu      t1, 0(ARG2)
        lwu      t2, -4(a6)
        sw       t1, 0(ARG1)
        sw       t2, -4(a7)
        j        copyexit
        
copy8_32:
        /* 8 < size < 32: */

        /* Copy src[0]~src[7] and src[size-8]~size[size-1].
         * Some overlap here, but the overhead is negligible.
         */
        ld       t1, 0(ARG2)
        ld       t2, -8(a6)
        sd       t1, 0(ARG1)
        sd       t2, -8(a7)

        /* If size > 16, src[8]~src[size-9] has not been copied. 
         * Copy src[8]~src[15] with ld/sd pair.
         */
        addi     t6, x0, 16
        ble      ARG3, t6, copyexit
        ld       t1, 8(ARG2)
        sd       t1, 8(ARG1)

        /* If size > 24, src[16]~src[size-9] has not been copied. 
         * Copy src[16]~src[23] with ld/sd pair.
         */
        addi     t6, x0, 24
        ble      ARG3, t6, copyexit
        ld       t1, 16(ARG2)
        sd       t1, 16(ARG1)
copyexit:
        mv       a0, t0 /* Restore original dst as return value */
        ret
        END_FUNC(memcpy)

/* Private memset.
 * Optimize private memset by using loop unrolling.
 * Optimization method is same as memcpy.
 */
        DECLARE_FUNC(memset)
GLOBAL_LABEL(memset:)
        addi     t6, x0, 32
        mv       t0, ARG1 /* save for return */

        /* duplicate byte into whole register */
        andi     ARG2, ARG2, 0xff
        mv       t1, ARG2
        slli     ARG2, ARG2, 8
        or       t1, t1, ARG2
        slli     ARG2, ARG2, 8
        or       t1, t1, ARG2
        slli     ARG2, ARG2, 8
        or       t1, t1, ARG2
        slli     ARG2, ARG2, 8
        or       t1, t1, ARG2
        slli     ARG2, ARG2, 8
        or       t1, t1, ARG2
        slli     ARG2, ARG2, 8
        or       t1, t1, ARG2
        slli     ARG2, ARG2, 8
        or       t1, t1, ARG2
set32_:
        blt      ARG3, t6, set0_32
        sd       t1, 0(ARG1)
        sd       t1, 8(ARG1)
        sd       t1, 16(ARG1)
        sd       t1, 24(ARG1)
        addi     ARG3, ARG3, -32
        addi     ARG1, ARG1, 32
        j        set32_
set0_32:
        add      a6, ARG1, ARG3
        addi     t6, x0, 8
        bge      ARG3, t6, set8_32
        addi     t6, x0, 4
        bge      ARG3, t6, set4_8
        bgtz     ARG3, set0_4
        j        setexit
set0_4:
        srli     t4, ARG3, 1
        add      t4, t4, ARG1
        sb       t1, 0(ARG1)
        sb       t1, -1(a6)
        sb       t1, 0(t4)
        j        setexit
set4_8:
        sw       t1, 0(ARG1)
        sw       t1, -4(a6)
        j        setexit
set8_32:
        sd       t1, 0(ARG1)
        sd       t1, -8(a6)
        addi     t6, x0, 16
        ble      ARG3, t6, setexit
        sd       t1, 8(ARG1)
        addi     t6, x0, 24
        ble      ARG3, t6, setexit
        sd       t1, 16(ARG1)
setexit:
        mv       a0, t0 /* Restore original dst as return value */
        ret
        END_FUNC(memset)

/* See x86.asm notes about needing these to avoid gcc invoking *_chk */
.global __memcpy_chk
.hidden __memcpy_chk
WEAK(__memcpy_chk)
.set __memcpy_chk,memcpy

.global __memset_chk
.hidden __memset_chk
WEAK(__memset_chk)
.set __memset_chk,memset

#endif /* UNIX */

END_FILE
