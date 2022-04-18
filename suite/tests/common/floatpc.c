/* **********************************************************
 * Copyright (c) 2013 Google, Inc.  All rights reserved.
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
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
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

#ifndef ASM_CODE_ONLY  /* C code */
#    include "tools.h" /* for print() */

#    include <stdio.h>

#    ifndef X64
extern ptr_int_t
test_fnstenv_intra(ptr_int_t *real_pc);
extern ptr_int_t
test_fnstenv_inter(ptr_int_t *real_pc);
#    else
extern ptr_int_t
test_fxsave64_intra(ptr_int_t *real_pc);
extern ptr_int_t
test_fxsave64_inter(ptr_int_t *real_pc);
#    endif
extern ptr_int_t
test_fxsave_intra(ptr_int_t *real_pc);
extern ptr_int_t
test_fxsave_inter(ptr_int_t *real_pc);

int
main(void)
{
    ptr_int_t rpc, fpc;

#    ifndef X64
    fpc = test_fnstenv_intra(&rpc);
    if (fpc == rpc)
        print("FNSTENV intra is correctly handled\n");
    else
        print("FNSTENV intra is **incorrectly** handled\n");
    fpc = test_fnstenv_inter(&rpc);
    if (fpc == rpc)
        print("FNSTENV inter is correctly handled\n");
    else
        print("FNSTENV inter is **incorrectly** handled\n");
#    else
    fpc = test_fxsave64_intra(&rpc);
    if (fpc == rpc)
        print("FXSAVE64 intra is correctly handled\n");
    else
        print("FXSAVE64 intra is **incorrectly** handled\n");
    fpc = test_fxsave64_inter(&rpc);
    if (fpc == rpc)
        print("FXSAVE64 inter is correctly handled\n");
    else
        print("FXSAVE64 inter is **incorrectly** handled\n");
#    endif

    fpc = test_fxsave_intra(&rpc);
    if ((int)fpc == (int)rpc)
        print("FXSAVE intra is correctly handled\n");
    else
        print("FXSAVE intra is **incorrectly** handled\n");

    fpc = test_fxsave_inter(&rpc);
    if ((int)fpc == (int)rpc)
        print("FXSAVE inter is correctly handled\n");
    else
        print("FXSAVE inter is **incorrectly** handled\n");

    return 0;
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
/* clang-format off */
START_FILE

# define FNSAVE_PC_OFFS  12
# define FXSAVE_PC_OFFS   8

# ifndef X64
/* 32-bit only, as it will not save the top 32 bits and thus our test
 * could fail if the executable is loaded above the bottom 4GB.
 */
#  define FUNCNAME test_fnstenv_intra
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      REG_XAX, ARG1
        lea      REG_XDX, SYMREF(fldz_addr)
        mov      PTRSZ [REG_XAX], REG_XDX
ADDRTAKEN_LABEL(fldz_addr:)
        fldz
        sub      REG_XSP, 32 /* make space for fnstenv */
        fnstenv  [REG_XSP]
        mov      eax, DWORD [REG_XSP + FNSAVE_PC_OFFS] /* PC field is 32 bits */
        add      REG_XSP, 32
        ret
        END_FUNC(FUNCNAME)
#  undef FUNCNAME

#  define FUNCNAME test_fnstenv_inter
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      REG_XAX, ARG1
        lea      REG_XDX, SYMREF(fldz_addr1)
        mov      PTRSZ [REG_XAX], REG_XDX
ADDRTAKEN_LABEL(fldz_addr1:)
        fldz
        /* conditional to put fldz in prior bb */
        mov      eax, 1
        cmp      eax, 1
        jne      skip
        sub      REG_XSP, 32 /* make space for fnstenv */
        fnstenv  [REG_XSP]
        mov      eax, DWORD [REG_XSP + FNSAVE_PC_OFFS] /* PC field is 32 bits */
        add      REG_XSP, 32
skip:
        ret
        END_FUNC(FUNCNAME)
#  undef FUNCNAME

# else

#  define FUNCNAME test_fxsave64_intra
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        END_PROLOG
        mov      REG_XAX, ARG1
        lea      REG_XDX, SYMREF(fldz_addr)
        mov      PTRSZ [REG_XAX], REG_XDX
ADDRTAKEN_LABEL(fldz_addr:)
        fldz
        mov      REG_XDX, REG_XSP
        sub      REG_XSP, 512+16 /* make space for fxsave + align */
        and      REG_XSP, -16    /* align to 16 */
        /* VS2005 doesn't know "fxsave64" */
        RAW(48) RAW(0f) RAW(ae) RAW(04) RAW(24) /* fxsave64 [REG_XSP] */
        mov      REG_XAX, PTRSZ [REG_XSP + FXSAVE_PC_OFFS]
        mov      REG_XSP, REG_XDX
        ret
        END_FUNC(FUNCNAME)
#  undef FUNCNAME

#  define FUNCNAME test_fxsave64_inter
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        END_PROLOG
        mov      REG_XAX, ARG1
        lea      REG_XDX, SYMREF(fldz_addr1)
        mov      PTRSZ [REG_XAX], REG_XDX
ADDRTAKEN_LABEL(fldz_addr1:)
        fldz
        /* conditional to put fldz in prior bb */
        mov      eax, 1
        cmp      eax, 1
        jne      skip
        mov      REG_XDX, REG_XSP
        sub      REG_XSP, 512+16 /* make space for fxsave + align */
        and      REG_XSP, -16    /* align to 16 */
        /* VS2005 doesn't know "fxsave64" */
        RAW(48) RAW(0f) RAW(ae) RAW(04) RAW(24) /* fxsave64 [REG_XSP] */
        mov      REG_XAX, PTRSZ [REG_XSP + FXSAVE_PC_OFFS]
        mov      REG_XSP, REG_XDX
skip:
        ret
        END_FUNC(FUNCNAME)
#  undef FUNCNAME
# endif

# define FUNCNAME test_fxsave_intra
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        END_PROLOG
        mov      REG_XAX, ARG1
        lea      REG_XDX, SYMREF(fldz_addr2)
        mov      PTRSZ [REG_XAX], REG_XDX
ADDRTAKEN_LABEL(fldz_addr2:)
        fldz
        mov      REG_XDX, REG_XSP
        sub      REG_XSP, 512+16 /* make space for fxsave + align */
        and      REG_XSP, -16    /* align to 16 */
        fxsave   [REG_XSP]
        mov      eax, DWORD [REG_XSP + FXSAVE_PC_OFFS]
        mov      REG_XSP, REG_XDX
        ret
        END_FUNC(FUNCNAME)
# undef FUNCNAME

# define FUNCNAME test_fxsave_inter
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        END_PROLOG
        mov      REG_XAX, ARG1
        lea      REG_XDX, SYMREF(fldz_addr3)
        mov      PTRSZ [REG_XAX], REG_XDX
ADDRTAKEN_LABEL(fldz_addr3:)
        fldz
        /* conditional to put fldz in prior bb */
        mov      eax, 1
        cmp      eax, 1
        jne      skip1
        mov      REG_XDX, REG_XSP
        sub      REG_XSP, 512+16 /* make space for fxsave + align */
        and      REG_XSP, -16    /* align to 16 */
        fxsave   [REG_XSP]
        mov      eax, DWORD [REG_XSP + FXSAVE_PC_OFFS]
        mov      REG_XSP, REG_XDX
skip1:
        ret
        END_FUNC(FUNCNAME)
# undef FUNCNAME

END_FILE
/* clang-format on */
#endif /* ASM_CODE_ONLY */
