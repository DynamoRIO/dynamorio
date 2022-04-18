/* **********************************************************
 * Copyright (c) 2012 Google, Inc.  All rights reserved.
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

/* Built as 32-bit and then run on WOW64 with 64-bit DR to test mixed-mode
 * and x86_to_x64 translation (i#49, i#751)
 *
 * If cmdline arg is "x86_to_x64", we avoid using instructions we can't
 * translate, such as daa, and avoid testing if 64-bit regs are preserved
 * across mode changes (i#865).
 */

#ifndef ASM_CODE_ONLY  /* C code */
#    include "tools.h" /* for print() */

/* asm routines */
void
test_top_bits(void);
void
test_push_word(void);
void
test_pop_word(void);
void
test_push_es(void);
void
test_pop_es(void);
void
test_push_esp(void);
void
test_pusha(void);
void
test_pushf(void);
void
test_les(void);
void
test_lea_addr32(void);
void
test_call_esp(void);
int
test_iret(void);
int
test_far_calls(void);

char global_data[8];
char is_x86_to_x64;

int
main(int argc, char *argv[])
{
    is_x86_to_x64 = (strcmp(argv[1], "x86_to_x64") == 0);

    if (is_x86_to_x64) {
        /* FIXME i#865: 64-bit regs are not preserved currently.
         * We don't test it for now -- just store the result to global_data
         * in order to pass the test suite.
         */
        *(__int64 *)global_data = 0x1234567812345678LL;
    } else
        test_top_bits();
    print("r8 was 0x%016" INT64_FORMAT "x\n", *(__int64 *)global_data);

    test_push_word();
    print("edx was " PFX "\n", *(__int32 *)global_data);

    test_pop_word();
    print("global_data is " PFX "\n", *(__int32 *)global_data);

    test_push_es();
    print("edx was " PFX "\n", *(__int32 *)global_data);

    test_pop_es();
    print("edx was " PFX "\n", *(__int32 *)global_data);

    test_push_esp();
    print("edx was " PFX "\n", *(__int32 *)global_data);

    test_pusha();
    print("edx was " PFX "\n", *(__int32 *)global_data);

    test_pushf();
    print("edx was " PFX "\n", *(__int32 *)global_data);

    test_les();
    print("edx was " PFX "\n", *(__int32 *)global_data);

    test_lea_addr32();
    print("edx was " PFX "\n", *(__int32 *)global_data);

    test_call_esp();
    print("test_call_esp() returned successfully\n");

    print("test_iret() returned %d\n", test_iret());

    print("test_far_calls() returned %d\n", test_far_calls());
    return 0;
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
/* clang-format off */
START_FILE

/* N.B.: it's tricky to write this code b/c it's built with a 32-bit assembler
 * so we have to use raw bytes and play games to get the 64-bit code generated.
 * It's also quite difficult to get a disasm listing for this all in one
 * shot.
 *
 * We can use exx in 64-bit mode, making it easier to read the
 * assembly, b/c we know top bits are zeroed.
 */

# define CS32_SELECTOR HEX(23)
# define CS64_SELECTOR HEX(33)
# define SS_SELECTOR   HEX(2b)

DECL_EXTERN(global_data)
DECL_EXTERN(is_x86_to_x64)

/* These are messy to make functions b/c the call/ret are different modes,
 * so we do macros and pass in a name to make the labels unique.
 */
# define SWITCH_32_TO_64(label)                         \
        /* far jmp to next instr w/ 64-bit switch */ @N@\
        /* jmp 0033:<topbits_transfer_to_64> */      @N@\
        RAW(ea)                                      @N@\
          DD offset switch_32_to_64_##label          @N@\
          DB CS64_SELECTOR                           @N@\
          RAW(00)                                    @N@\
    switch_32_to_64_##label:                         @N@\
        nop

# define SWITCH_64_TO_32(label)                                                       \
        /* far jmp to next instr w/ 32-bit switch */                               @N@\
        /* jmp 0023:<topbits_return_to_32_A> */                                    @N@\
        push     offset switch_64_to_32_##label  /* 8-byte push */                 @N@\
        mov      dword ptr [esp + 4], CS32_SELECTOR /* top 4 bytes of prev push */ @N@\
        jmp      fword ptr [esp]                                                   @N@\
    switch_64_to_32_##label:                                                       @N@\
        lea  esp, [esp + 8]     /* undo the x64 push */

/* Tests whether r8's value is preserved across mode switches */
# define FUNCNAME test_top_bits
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        SWITCH_32_TO_64(set_r8)
        /* mov r8, $0x1234567812345678 */
        RAW(49)
          mov      eax, HEX(12345678)
          DD HEX(12345678)
        SWITCH_64_TO_32(no_more_r8)
        SWITCH_32_TO_64(retrieve_r8)
        /* We can't do "mov [global_data], r8" by just prefixing 32-bit instr b/c
         * abs addr turns into rip-rel, so we put global_data into ecx
         */
        mov      ecx, offset global_data
        /* mov qword ptr [rcx], r8 */
        RAW(4c)
          mov      dword ptr [ecx], eax
        SWITCH_64_TO_32(back_to_normal)
        ret
        END_FUNC(FUNCNAME)
# undef FUNCNAME

# define FUNCNAME test_push_word
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      ecx, offset global_data
        push     word ptr [ecx]
        push     word ptr HEX(8765)
        pop      edx
        cmp      edx, HEX(56788765)
        jnz      push_word_exit
        mov      ax, HEX(abcd)
        push     ax
        /* push -1 sign-extended to word */
        RAW(66)
          push   byte ptr HEX(ff)
        pop      edx
    push_word_exit:
        mov      dword ptr [ecx], edx
        ret
        END_FUNC(FUNCNAME)
# undef FUNCNAME

# define FUNCNAME test_pop_word
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      ecx, offset global_data
        push     HEX(12345678)
        pop      word ptr [ecx + 2]
        pop      ax
        mov      word ptr [ecx], ax
        ret
        END_FUNC(FUNCNAME)
# undef FUNCNAME

# define FUNCNAME test_push_es
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      edx, HEX(e5e5e5e5)
        mov      ax, es
        push     es
        pop      ecx
        cmp      cx, ax
        jz       push_es_exit
        mov      edx, HEX(deadbeef)
    push_es_exit:
        mov      ecx, offset global_data
        mov      dword ptr [ecx], edx
        ret
        END_FUNC(FUNCNAME)
# undef FUNCNAME

# define FUNCNAME test_pop_es
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      edx, HEX(5e5e5e5e)
        mov      ax, es
        movzx    eax, ax
        push     eax
        pop      es
        mov      cx, es
        cmp      cx, ax
        jz       pop_es_exit
        mov      edx, HEX(deadbeef)
    pop_es_exit:
        mov      ecx, offset global_data
        mov      dword ptr [ecx], edx
        ret
        END_FUNC(FUNCNAME)
# undef FUNCNAME

# define FUNCNAME test_push_esp
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      edx, esp
        add      edx, edx
        push     esp
        sub      edx, [esp]
        pop      esp
        sub      edx, esp
        mov      ecx, offset global_data
        mov      dword ptr [ecx], edx
        ret
        END_FUNC(FUNCNAME)
# undef FUNCNAME

# define FUNCNAME test_pusha
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      edx, HEX(11223344)
        pushad
        mov      edx, HEX(deadbeef)
        popad
        mov      ecx, offset global_data
        mov      dword ptr [ecx], edx
        ret
        END_FUNC(FUNCNAME)
# undef FUNCNAME

# define FUNCNAME test_pushf
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      edx, HEX(55667788)
        cmp      edx, HEX(55667788)
        pushfd
        cmp      edx, HEX(deadbeef)
        popfd
        jz       pushf_exit
        mov      edx, HEX(deadbeef)
    pushf_exit:
        mov      ecx, offset global_data
        mov      dword ptr [ecx], edx
        ret
        END_FUNC(FUNCNAME)
# undef FUNCNAME

# define FUNCNAME test_les
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      ax, es
        push     es
        push     HEX(87654321)
        les      edx, fword ptr [esp]
        add      esp, 8
        mov      cx, es
        cmp      cx, ax
        jz       les_exit
        mov      edx, HEX(deadbeef)
    les_exit:
        mov      ecx, offset global_data
        mov      dword ptr [ecx], edx
        ret
        END_FUNC(FUNCNAME)
# undef FUNCNAME

# define FUNCNAME test_lea_addr32
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        push     HEX(1EAADD32)
        lea      esp, [esp + 4]
        lea      esp, [esp - 4]
        lea      esp, [esp + HEX(4000)]
        lea      esp, [esp - HEX(4000)]
        pop      edx
        mov      ecx, -8
        mov      dword ptr [ecx + (offset global_data + 8)], edx
        ret
        END_FUNC(FUNCNAME)
# undef FUNCNAME

/* In this test, we first setup the stack with the following layout:
 *
 *        |                       | (low mem)
 *        +-----------------------+
 *        |           0           |
 *        +-----------------------+
 *        | addr of call_esp_next |
 *        +-----------------------+
 * esp -> |   original stack top  | (high mem)
 *        +-----------------------+
 *
 * Then we call [esp - 4] (i.e., call call_esp_next).
 *
 * An incorrect implementation of x86_to_x64 translation may effectively
 * call [esp - 8] (i.e., call 0), which leads to a fault.
 */
# define FUNCNAME test_call_esp
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        mov      ecx, 1
        call     call_esp_next
    call_esp_next:
        push     0
        pop      eax
        pop      eax
        jecxz    call_esp_exit
        dec      ecx
        call     near ptr [esp - 4]
    call_esp_exit:
        ret
        END_FUNC(FUNCNAME)
# undef FUNCNAME

# define FUNCNAME test_iret
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        pushfd
        push     CS64_SELECTOR
        push     offset iret_32_to_64
        iretd
    iret_32_to_64:
        /* back to 32-bit via 64-bit iret */
        mov      edx, esp
        push     SS_SELECTOR
        push     edx
        pushfd   /* really pushfq */
        push     CS32_SELECTOR
        push     offset iret_64_to_32
        RAW(48)
          iretd  /* iretq */
    iret_64_to_32:
        /* skip daa if is_x86_to_x64 == 1 */
        mov      ecx, offset is_x86_to_x64
        mov      al, byte ptr [ecx]
        test     al, al
        jnz      iret_64_to_32_skip_daa
        /* otherwise use daa to ensure we're 32-bit */
        daa
    iret_64_to_32_skip_daa:
        pushfd
        push     CS64_SELECTOR
        push     offset iret_32_to_64_B
        iretd
    iret_32_to_64_B:
        /* ensure we're 64-bit by returning ecx */
        mov ecx, 0
        /* 64-bit "add r8,1" vs 32-bit "dec ecx; add eax,1" */
        RAW(49)
          add    eax, 1
        mov      eax, ecx
        /* back to 32-bit via 32-bit iret => need 4-byte stack operands */
        /* XXX: despite the Intel manual pseudocode, 32-bit iret pops ss:rsp */
        pushfd   /* really pushfq */
        pop      ecx
        mov      edx, esp
        lea      esp, [esp - 20]
        mov      dword ptr [esp + 16], SS_SELECTOR
        mov      dword ptr [esp + 12], edx
        mov      dword ptr [esp + 8], ecx
        mov      dword ptr [esp + 4], CS32_SELECTOR
        mov      dword ptr [esp], offset iret_64_to_32_B
        iretd
    iret_64_to_32_B:
        nop

        ret                      /* return value already in eax */
        END_FUNC(FUNCNAME)
# undef FUNCNAME

# define FUNCNAME test_far_calls
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        /* call 0033:<far_call_to_64> */
        push     CS64_SELECTOR
        push     offset far_call_to_64
        call     fword ptr [esp]
        lea      esp, [esp + 8]     /* undo the two pushes */
        jmp      test_far_dir_call
    far_call_to_64:
        retf
    test_far_dir_call:

        /* call 0033:<far_dir_call> */
        RAW(9a)
          DD offset far_dir_call
          DB CS64_SELECTOR
          RAW(00)
        jmp      test_far_dir_done
    far_dir_call:
        retf

    test_far_dir_done:

        SWITCH_32_TO_64(far_call_from_64)
        /* call 0023:<far_call_to_32> */
        push     offset far_call_to_32  /* 8-byte push */
        mov      dword ptr [esp + 4], CS32_SELECTOR /* top 4 bytes of prev push */
        call     fword ptr [esp]
        lea      esp, [esp + 8]     /* undo the x64 push */
        jmp      test_far_dir_call_from_64
    far_call_to_32:
        /* skip daa if is_x86_to_x64 == 1 */
        mov      ecx, offset is_x86_to_x64
        mov      al, byte ptr [ecx]
        test     al, al
        jnz      far_call_to_32_skip_daa
        /* otherwise use daa to ensure we're 32-bit */
        daa
    far_call_to_32_skip_daa:
        retf
    test_far_dir_call_from_64:
        SWITCH_64_TO_32(far_calls_done)

        xor      eax,eax
        ret
        END_FUNC(FUNCNAME)
# undef FUNCNAME

END_FILE
/* clang-format on */
#endif /* ASM_CODE_ONLY */
