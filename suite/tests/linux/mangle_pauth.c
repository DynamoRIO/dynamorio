/* **********************************************************
 * Copyright (c) 2024 Arm Limited.  All rights reserved.
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
 * * Neither the name of Arm Limited nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef ASM_CODE_ONLY /* C code */
#    include "configure.h"
#    ifndef UNIX
#        error UNIX-only
#    endif
#    ifndef AARCH64
#        error AARCH64-only
#    endif
#    include "tools.h"
#    include <setjmp.h>
#    include <signal.h>
#    include <stdint.h>

#    define ENABLE_LOGGING false
#    define LOG(...)                \
        do {                        \
            if (ENABLE_LOGGING) {   \
                print(__VA_ARGS__); \
            }                       \
        } while (0)

void
test_retaa(bool trigger_fault);

void
test_retab(bool trigger_fault);

void
test_braaz(bool trigger_fault);

void
test_brabz(bool trigger_fault);

void
test_braa(bool trigger_fault);

void
test_brab(bool trigger_fault);

void
test_blraaz(bool trigger_fault);

void
test_blrabz(bool trigger_fault);

void
test_blraa(bool trigger_fault);

void
test_blrab(bool trigger_fault);

uintptr_t
strip_pac(uintptr_t ptr);

/* Dummy function just used as a branch target for the blr* tests */
bool
dummy_func()
{
    return true;
}

static SIGJMP_BUF mark;
uintptr_t branch_instr_addr;
uintptr_t branch_target_addr;

enum {
    TEST_PASS = 1,
    TEST_PC_MISMATCH = 2,
};

static void
handle_signal(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (signal == SIGSEGV) {
        LOG("Handled SIGSEGV:\n");
        /* CPU does not have FEAT_FPACCOMBINE so it branched to a non-canonical address.
         * We need to strip the PAC from from the fault address to canonicalize it and
         * compare it to the expected branch target address.
         */
        const uintptr_t fault_pc = strip_pac(ucxt->uc_mcontext.pc);
        LOG("    ucxt->uc_mcontext.pc = " PFX "\n", ucxt->uc_mcontext.pc);
        LOG("    fault_pc =             " PFX "\n", fault_pc);
        LOG("    branch_target_addr =   " PFX "\n", branch_target_addr);
        if (fault_pc == branch_target_addr)
            SIGLONGJMP(mark, TEST_PASS);
        else
            SIGLONGJMP(mark, TEST_PC_MISMATCH);

    } else if (signal == SIGILL) {
        LOG("Handled SIGILL:\n");
        /* CPU has FEAT_FPACCOMBINE so the branch instruction generated an authentication
         * failure exception and the fault PC should match the branch instruction address.
         */
        const uintptr_t fault_pc = ucxt->uc_mcontext.pc;
        LOG("    fault_pc =          " PFX "\n", fault_pc);
        LOG("    branch_instr_addr = " PFX "\n", branch_instr_addr);
        if (fault_pc == branch_instr_addr)
            SIGLONGJMP(mark, TEST_PASS);
        else
            SIGLONGJMP(mark, TEST_PC_MISMATCH);
    } else {
        print("Unexpected signal!\n");
    }
}

int
main(int argc, const char *argv[])
{
#    define FOR_EACH_TEST(M) \
        do {                 \
            M(retaa);        \
            M(retab);        \
            M(braaz);        \
            M(brabz);        \
            M(braa);         \
            M(brab);         \
            M(blraaz);       \
            M(blrabz);       \
            M(blraa);        \
            M(blrab);        \
        } while (0)

#    define NON_FAULT_TEST(instr_name)                \
        do {                                          \
            LOG("Non-fault test: " #instr_name "\n"); \
            test_##instr_name(false);                 \
        } while (0)

    FOR_EACH_TEST(NON_FAULT_TEST);

    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal, false);
    intercept_signal(SIGILL, (handler_3_t)&handle_signal, false);

#    define FAULT_TEST(instr_name)                                      \
        do {                                                            \
            LOG("Fault test: " #instr_name "\n");                       \
            switch (SIGSETJMP(mark)) {                                  \
            default:                                                    \
                test_##instr_name(true);                                \
                print(#instr_name " fault test failed: No fault\n");    \
                break;                                                  \
            case TEST_PC_MISMATCH:                                      \
                print(#instr_name " fault test failed: PC mismatch\n"); \
            case TEST_PASS: break;                                      \
            }                                                           \
        } while (0)
    FOR_EACH_TEST(FAULT_TEST);

    print("Test complete\n");

    return 0;
}

#else /* asm code *************************************************************/
/* clang-format off */
#    include "asm_defines.asm"
START_FILE

.arch armv8.3-a

DECL_EXTERN(branch_instr_addr)
DECL_EXTERN(branch_target_addr)
DECL_EXTERN(dummy_func)

/* Write a value to the global branch_target_addr variable. */
#define SAVE_BRANCH_TARGET_ADDR(addr_reg, tmp_reg)                    \
        AARCH64_ADRP_GOT(GLOBAL_REF(branch_target_addr), tmp_reg) @N@ \
        str     addr_reg, [tmp_reg]

/* Write the address of a label to the global branch_instr_addr variable. */
#define SAVE_BRANCH_INSTR_ADDR(label, tmp_reg1, tmp_reg2)             \
        AARCH64_ADRP_GOT(GLOBAL_REF(branch_instr_addr), tmp_reg1) @N@ \
        adr     tmp_reg2, label                                   @N@ \
        str     tmp_reg2, [tmp_reg1]

/* Give some names to some of the registers we need in the tests. */
#define TRIGGER_FAULT_ARG_REG x0
#define BRANCH_TARGET_REG x1
#define TMP1_REG x2
#define TMP2_REG x3
#define MODIFIER_REG x4

/*
 * Corrupt the signed address in addr_reg if trigger_fault_reg contains 1.
 * We corrupt the address by flipping one of the PAC bits so the PAC will no longer be
 * valid. The bit that we flip can be any bit that is part of the PAC code so we
 * arbitrarily choose bit 53.
 *
 * addr_reg = addr_reg ^ (trigger_fault << 53);
 */
#define CORRUPT_ADDR_IF_NEEDED(addr_reg, trigger_fault_reg) \
        eor     addr_reg, addr_reg, trigger_fault_reg, lsl 53

#define FUNCNAME test_retaa
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        SAVE_BRANCH_TARGET_ADDR(x30, TMP1_REG)
        SAVE_BRANCH_INSTR_ADDR(LOCAL_LABEL(branch_instr), TMP1_REG, TMP2_REG)
        paciasp
        CORRUPT_ADDR_IF_NEEDED(x30, TRIGGER_FAULT_ARG_REG)
LOCAL_LABEL(branch_instr):
        retaa
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME test_retab
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        SAVE_BRANCH_TARGET_ADDR(x30, TMP1_REG)
        SAVE_BRANCH_INSTR_ADDR(LOCAL_LABEL(branch_instr), TMP1_REG, TMP2_REG)
        pacibsp
        CORRUPT_ADDR_IF_NEEDED(x30, TRIGGER_FAULT_ARG_REG)
LOCAL_LABEL(branch_instr):
        retab
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME test_braaz
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        adr     BRANCH_TARGET_REG, LOCAL_LABEL(branch_target)
        SAVE_BRANCH_TARGET_ADDR(BRANCH_TARGET_REG, TMP1_REG)
        SAVE_BRANCH_INSTR_ADDR(LOCAL_LABEL(branch_instr), TMP1_REG, TMP2_REG)
        paciza  BRANCH_TARGET_REG
        CORRUPT_ADDR_IF_NEEDED(BRANCH_TARGET_REG, TRIGGER_FAULT_ARG_REG)
LOCAL_LABEL(branch_instr):
        braaz   BRANCH_TARGET_REG
LOCAL_LABEL(branch_target):
        ret
#undef FUNCNAME

#define FUNCNAME test_brabz
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        adr     BRANCH_TARGET_REG, LOCAL_LABEL(branch_target)
        SAVE_BRANCH_TARGET_ADDR(BRANCH_TARGET_REG, TMP1_REG)
        SAVE_BRANCH_INSTR_ADDR(LOCAL_LABEL(branch_instr), TMP1_REG, TMP2_REG)
        pacizb  BRANCH_TARGET_REG
        CORRUPT_ADDR_IF_NEEDED(BRANCH_TARGET_REG, TRIGGER_FAULT_ARG_REG)
LOCAL_LABEL(branch_instr):
        brabz   BRANCH_TARGET_REG
LOCAL_LABEL(branch_target):
        ret
#undef FUNCNAME

#define FUNCNAME test_braa
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        adr     BRANCH_TARGET_REG, LOCAL_LABEL(branch_target)
        mov     MODIFIER_REG, #42
        SAVE_BRANCH_TARGET_ADDR(BRANCH_TARGET_REG, TMP1_REG)
        SAVE_BRANCH_INSTR_ADDR(LOCAL_LABEL(branch_instr), TMP1_REG, TMP2_REG)
        pacia   BRANCH_TARGET_REG, MODIFIER_REG
        CORRUPT_ADDR_IF_NEEDED(BRANCH_TARGET_REG, TRIGGER_FAULT_ARG_REG)
LOCAL_LABEL(branch_instr):
        braa    BRANCH_TARGET_REG, MODIFIER_REG
LOCAL_LABEL(branch_target):
        ret
#undef FUNCNAME

#define FUNCNAME test_brab
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#define TRIGGER_FAULT_ARG_REG x0
        adr     BRANCH_TARGET_REG, LOCAL_LABEL(branch_target)
        mov     MODIFIER_REG, #42
        SAVE_BRANCH_TARGET_ADDR(BRANCH_TARGET_REG, TMP1_REG)
        SAVE_BRANCH_INSTR_ADDR(LOCAL_LABEL(branch_instr), TMP1_REG, TMP2_REG)
        pacib   BRANCH_TARGET_REG, MODIFIER_REG
        CORRUPT_ADDR_IF_NEEDED(BRANCH_TARGET_REG, TRIGGER_FAULT_ARG_REG)
LOCAL_LABEL(branch_instr):
        brab    BRANCH_TARGET_REG, MODIFIER_REG
LOCAL_LABEL(branch_target):
        ret
#undef FUNCNAME

#define FUNCNAME test_blraaz
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        AARCH64_ADRP_GOT(GLOBAL_REF(dummy_func), BRANCH_TARGET_REG)
        SAVE_BRANCH_TARGET_ADDR(BRANCH_TARGET_REG, TMP1_REG)
        SAVE_BRANCH_INSTR_ADDR(LOCAL_LABEL(branch_instr), TMP1_REG, TMP2_REG)
        paciza  BRANCH_TARGET_REG
        CORRUPT_ADDR_IF_NEEDED(BRANCH_TARGET_REG, TRIGGER_FAULT_ARG_REG)
        str     x30, [sp, #-16]!
LOCAL_LABEL(branch_instr):
        blraaz  BRANCH_TARGET_REG
        ldr     x30, [sp], #16
        ret
#undef FUNCNAME

#define FUNCNAME test_blrabz
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        AARCH64_ADRP_GOT(GLOBAL_REF(dummy_func), BRANCH_TARGET_REG)
        SAVE_BRANCH_TARGET_ADDR(BRANCH_TARGET_REG, TMP1_REG)
        SAVE_BRANCH_INSTR_ADDR(LOCAL_LABEL(branch_instr), TMP1_REG, TMP2_REG)
        pacizb  BRANCH_TARGET_REG
        CORRUPT_ADDR_IF_NEEDED(BRANCH_TARGET_REG, TRIGGER_FAULT_ARG_REG)
        str     x30, [sp, #-16]!
LOCAL_LABEL(branch_instr):
        blrabz  BRANCH_TARGET_REG
        ldr     x30, [sp], #16
        ret
#undef FUNCNAME

#define FUNCNAME test_blraa
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        AARCH64_ADRP_GOT(GLOBAL_REF(dummy_func), BRANCH_TARGET_REG)
        SAVE_BRANCH_TARGET_ADDR(BRANCH_TARGET_REG, TMP1_REG)
        SAVE_BRANCH_INSTR_ADDR(LOCAL_LABEL(branch_instr), TMP1_REG, TMP2_REG)
        mov     MODIFIER_REG, #42
        pacia   BRANCH_TARGET_REG, MODIFIER_REG
        CORRUPT_ADDR_IF_NEEDED(BRANCH_TARGET_REG, TRIGGER_FAULT_ARG_REG)
        str     x30, [sp, #-16]!
LOCAL_LABEL(branch_instr):
        blraa   BRANCH_TARGET_REG, MODIFIER_REG
        ldr     x30, [sp], #16
        ret
#undef FUNCNAME

#define FUNCNAME test_blrab
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        AARCH64_ADRP_GOT(GLOBAL_REF(dummy_func), BRANCH_TARGET_REG)
        SAVE_BRANCH_TARGET_ADDR(BRANCH_TARGET_REG, TMP1_REG)
        SAVE_BRANCH_INSTR_ADDR(LOCAL_LABEL(branch_instr), TMP1_REG, TMP2_REG)
        mov     MODIFIER_REG, #42
        pacib   BRANCH_TARGET_REG, MODIFIER_REG
        CORRUPT_ADDR_IF_NEEDED(BRANCH_TARGET_REG, TRIGGER_FAULT_ARG_REG)
        str     x30, [sp, #-16]!
LOCAL_LABEL(branch_instr):
        blrab   BRANCH_TARGET_REG, MODIFIER_REG
        ldr     x30, [sp], #16
        ret
#undef FUNCNAME

#define FUNCNAME strip_pac
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        xpaci   x0
        ret
        END_FUNC(FUNCNAME)
#undef FUNCNAME

END_FILE

#endif
/* clang-format on */
