/* **********************************************************
 * Copyright (c) 2012-2013 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Simple main that just calls each unit test in turn.
 */

#include "globals.h"
#include "arch.h"

void
unit_test_io(void);
#ifdef UNIX
void
unit_test_string(void);
void
unit_test_os(void);
#endif
void
unit_test_options(void);
void
unit_test_vmareas(void);
void
unit_test_utils(void);
#ifdef WINDOWS
void
unit_test_drwinapi(void);
#endif
void
unit_test_asm(dcontext_t *dc);
void
unit_test_atomic_ops(void);
void
unit_test_jit_fragment_tree(void);

#ifdef __AVX__

void
write_ymm_aux(dr_ymm_t *buffer, int regno)
{
    switch (regno) {
#    define MOVE_TO_YMM_VEX(BUF, NUM) \
        asm volatile("vmovdqu %0, "   \
                     "%%ymm" #NUM     \
                     :                \
                     : "m"(BUF[NUM]))
#    define MOVE_TO_YMM_EVEX(BUF, NUM) \
        asm volatile("vmovdqu32 %0, "  \
                     "%%ymm" #NUM      \
                     :                 \
                     : "m"(BUF[NUM]))
#    define CASE_MOVE_TO_YMM_VEX(BUF, NUM) \
    case NUM:                              \
        MOVE_TO_YMM_VEX(BUF, NUM);         \
        break;
#    define CASE_MOVE_TO_YMM_EVEX(BUF, NUM) \
    case NUM: MOVE_TO_YMM_EVEX(BUF, NUM); break;

        CASE_MOVE_TO_YMM_VEX(buffer, 0)
        CASE_MOVE_TO_YMM_VEX(buffer, 1)
        CASE_MOVE_TO_YMM_VEX(buffer, 2)
        CASE_MOVE_TO_YMM_VEX(buffer, 3)
        CASE_MOVE_TO_YMM_VEX(buffer, 4)
        CASE_MOVE_TO_YMM_VEX(buffer, 5)
        CASE_MOVE_TO_YMM_VEX(buffer, 6)
        CASE_MOVE_TO_YMM_VEX(buffer, 7)
#    ifdef X64
        CASE_MOVE_TO_YMM_VEX(buffer, 8)
        CASE_MOVE_TO_YMM_VEX(buffer, 9)
        CASE_MOVE_TO_YMM_VEX(buffer, 10)
        CASE_MOVE_TO_YMM_VEX(buffer, 11)
        CASE_MOVE_TO_YMM_VEX(buffer, 12)
        CASE_MOVE_TO_YMM_VEX(buffer, 13)
        CASE_MOVE_TO_YMM_VEX(buffer, 14)
        CASE_MOVE_TO_YMM_VEX(buffer, 15)
#        ifdef __AVX512F__
        CASE_MOVE_TO_YMM_EVEX(buffer, 16)
        CASE_MOVE_TO_YMM_EVEX(buffer, 17)
        CASE_MOVE_TO_YMM_EVEX(buffer, 18)
        CASE_MOVE_TO_YMM_EVEX(buffer, 19)
        CASE_MOVE_TO_YMM_EVEX(buffer, 20)
        CASE_MOVE_TO_YMM_EVEX(buffer, 21)
        CASE_MOVE_TO_YMM_EVEX(buffer, 22)
        CASE_MOVE_TO_YMM_EVEX(buffer, 23)
        CASE_MOVE_TO_YMM_EVEX(buffer, 24)
        CASE_MOVE_TO_YMM_EVEX(buffer, 25)
        CASE_MOVE_TO_YMM_EVEX(buffer, 26)
        CASE_MOVE_TO_YMM_EVEX(buffer, 27)
        CASE_MOVE_TO_YMM_EVEX(buffer, 28)
        CASE_MOVE_TO_YMM_EVEX(buffer, 29)
        CASE_MOVE_TO_YMM_EVEX(buffer, 30)
        CASE_MOVE_TO_YMM_EVEX(buffer, 31)
#        endif
#    endif
    default: FAIL(); break;
    }
}

void
unit_test_get_ymm_caller_saved()
{
    /* XXX i#1312: Once get_ymm_caller_saved(byte* buf) changes to reflect a dr_zmm_t type
     * of the SIMD field in DynamoRIO's mcontext, this needs to become dr_zmm_t.
     */
    dr_ymm_t ref_buffer[MCXT_NUM_SIMD_SLOTS];
    dr_ymm_t get_buffer[MCXT_NUM_SIMD_SLOTS];
    uint base = 0x78abcdef;
    for (int regno = 0; regno < proc_num_simd_registers(); ++regno) {
        for (int dword = 0; dword < sizeof(dr_ymm_t) / sizeof(uint); ++dword) {
            ref_buffer[regno].u32[dword] = 0;
            get_buffer[regno].u32[dword] = 0;
        }
        base += regno;
        for (int dword = 0; dword < sizeof(dr_ymm_t) / sizeof(uint); ++dword) {
            ref_buffer[regno].u32[dword] = base + dword;
        }
        write_ymm_aux(ref_buffer, regno);
    }
    get_ymm_caller_saved(get_buffer);
    for (int regno = 0; regno < proc_num_simd_registers(); ++regno) {
        dump_buffer_as_bytes(STDERR, &ref_buffer[regno], sizeof(ref_buffer[regno]),
                             DUMP_RAW | DUMP_DWORD);
        dump_buffer_as_bytes(STDERR, &get_buffer[regno], sizeof(get_buffer[regno]),
                             DUMP_RAW | DUMP_DWORD);
    }
    EXPECT(
        memcmp(ref_buffer, get_buffer, proc_num_simd_registers() * MCXT_SIMD_SLOT_SIZE),
        0);
}

#endif

#ifdef __AVX512F__

void
write_zmm_aux(dr_zmm_t *buffer, int regno)
{
    switch (regno) {
#    define MOVE_TO_ZMM(BUF, NUM) \
    case NUM: asm volatile("vmovdqu32 %0, %%zmm" #NUM : : "m"(BUF[NUM])); break;
        MOVE_TO_ZMM(buffer, 0)
        MOVE_TO_ZMM(buffer, 1)
        MOVE_TO_ZMM(buffer, 2)
        MOVE_TO_ZMM(buffer, 3)
        MOVE_TO_ZMM(buffer, 4)
        MOVE_TO_ZMM(buffer, 5)
        MOVE_TO_ZMM(buffer, 6)
        MOVE_TO_ZMM(buffer, 7)
#    ifdef X64
        MOVE_TO_ZMM(buffer, 8)
        MOVE_TO_ZMM(buffer, 9)
        MOVE_TO_ZMM(buffer, 10)
        MOVE_TO_ZMM(buffer, 11)
        MOVE_TO_ZMM(buffer, 12)
        MOVE_TO_ZMM(buffer, 13)
        MOVE_TO_ZMM(buffer, 14)
        MOVE_TO_ZMM(buffer, 15)
#        ifdef __AVX512F__
        MOVE_TO_ZMM(buffer, 16)
        MOVE_TO_ZMM(buffer, 17)
        MOVE_TO_ZMM(buffer, 18)
        MOVE_TO_ZMM(buffer, 19)
        MOVE_TO_ZMM(buffer, 20)
        MOVE_TO_ZMM(buffer, 21)
        MOVE_TO_ZMM(buffer, 22)
        MOVE_TO_ZMM(buffer, 23)
        MOVE_TO_ZMM(buffer, 24)
        MOVE_TO_ZMM(buffer, 25)
        MOVE_TO_ZMM(buffer, 26)
        MOVE_TO_ZMM(buffer, 27)
        MOVE_TO_ZMM(buffer, 28)
        MOVE_TO_ZMM(buffer, 29)
        MOVE_TO_ZMM(buffer, 30)
        MOVE_TO_ZMM(buffer, 31)
#        endif
#    endif
    default: FAIL(); break;
    }
}

void
unit_test_get_zmm_caller_saved()
{
    /* XXX i#1312: get_zmm_caller_saved(byte* buf) assumes that there is enough space in
     * the buffer it's being passed. MCXT_NUM_SIMD_SLOTS does not yet reflect this. Once
     * this happens, the array size should become MCXT_NUM_SIMD_SLOTS.
     */
    if (MCXT_NUM_SIMD_SLOTS == 32) {
        /* This is a just reminder.*/
        FAIL();
    }
    dr_zmm_t ref_buffer[32];
    dr_zmm_t get_buffer[32];
    uint base = 0x78abcdef;
    for (int regno = 0; regno < proc_num_simd_registers(); ++regno) {
        for (int dword = 0; dword < sizeof(dr_zmm_t) / sizeof(uint); ++dword) {
            ref_buffer[regno].u32[dword] = 0;
            get_buffer[regno].u32[dword] = 0;
        }
        base += regno;
        for (int dword = 0; dword < sizeof(dr_zmm_t) / sizeof(uint); ++dword) {
            ref_buffer[regno].u32[dword] = base + dword;
        }
        write_zmm_aux(ref_buffer, regno);
    }
    get_zmm_caller_saved(get_buffer);
    for (int regno = 0; regno < proc_num_simd_registers(); ++regno) {
        dump_buffer_as_bytes(STDERR, &ref_buffer[regno], sizeof(ref_buffer[regno]),
                             DUMP_RAW | DUMP_DWORD);
        dump_buffer_as_bytes(STDERR, &get_buffer[regno], sizeof(get_buffer[regno]),
                             DUMP_RAW | DUMP_DWORD);
    }
    EXPECT(
        memcmp(ref_buffer, get_buffer, proc_num_simd_registers() * MCXT_SIMD_SLOT_SIZE),
        0);
}

#endif

int
main(int argc, char **argv, char **envp)
{
    dcontext_t *dc = standalone_init();

    /* Each test will abort if it fails, so we just call each in turn and return
     * 0 for success.  If we want to be able to call each test independently, it
     * might be worth looking into gtest, which already does this.
     */
    unit_test_io();
#ifdef UNIX
    unit_test_string();
    unit_test_os();
#endif
    unit_test_utils();
    unit_test_options();
    unit_test_vmareas();
#ifdef WINDOWS
    unit_test_drwinapi();
#endif
    unit_test_asm(dc);
    unit_test_atomic_ops();
    unit_test_jit_fragment_tree();
#ifdef UNIX
    /* The following unit tests are implemented outside of DynamoRIO's core modules,
     * because DynamoRIO itself is not compiled for AVX or AVX-512.
     */
#    ifdef __AVX__
    unit_test_get_ymm_caller_saved();
#    endif
#    ifdef __AVX512F__
    unit_test_get_zmm_caller_saved();
#    endif
#endif
    print_file(STDERR, "all done\n");
    return 0;
}
