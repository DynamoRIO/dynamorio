/* **********************************************************
 * Copyright (c) 2016-2017 Google, Inc.  All rights reserved.
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

#ifndef ASM_CODE_ONLY /* C code */
#    include "tools.h"
#    include "drx_buf-test-shared.h"
#    include <setjmp.h>

#    define CHECK(x, msg)                                                             \
        do {                                                                          \
            if (!(x)) {                                                               \
                fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, msg); \
                abort();                                                              \
            }                                                                         \
        } while (0);

/* asm routines */
void
test_asm_123();
void
test_asm_45();

static SIGJMP_BUF mark;

#    if defined(UNIX)
#        include <pthread.h>
#        include <signal.h>
static void
handle_signal(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    print("drx_buf signal test PASS\n");
    SIGLONGJMP(mark, 1);
}

void *
thread_asm_test(void *unused)
{
    int i;
    /* tests 1, 2 and 3 */
    for (i = 0; i < NUM_ITER; ++i)
        test_asm_123();
    /* tests 4 and 5 */
    test_asm_45();
    return NULL;
}
#    elif defined(WINDOWS)
#        include <windows.h>
static LONG WINAPI
handle_exception(struct _EXCEPTION_POINTERS *ep)
{
    print("drx_buf signal test PASS\n");
    SIGLONGJMP(mark, 1);
}

DWORD WINAPI
thread_asm_test(LPVOID lpParam)
{
    int i;
    /* tests 1, 2 and 3 */
    for (i = 0; i < NUM_ITER; ++i)
        test_asm_123();
    /* tests 4 and 5 */
    test_asm_45();
    return 0;
}
#    endif

int
main(void)
{
    /* XXX: We can also trigger a segfault by trying to execute the buffer. We could
     * get the address of the buffer in the app using some annotation-based approach.
     */
#    if defined(UNIX)
    pthread_t thread;
#    elif defined(WINDOWS)
    HANDLE thread;
    DWORD threadId;
#    endif

    print("Starting drx_buf threaded test\n");
#    if defined(UNIX)
    CHECK(!pthread_create(&thread, NULL, thread_asm_test, NULL), "create failed");
    /* make sure that the buffers are threadsafe */
    (void)thread_asm_test(NULL);
    CHECK(!pthread_join(thread, NULL), "join failed");
#    elif defined(WINDOWS)
    CHECK((thread = CreateThread(NULL, 0, thread_asm_test, NULL, 0, &threadId)) != NULL,
          "CreateThread failed");
    /* make sure that the buffers are threadsafe */
    (void)thread_asm_test(NULL);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
#    endif
    print("Ending drx_buf threaded test\n");

    /* install signals */
#    if defined(UNIX)
    intercept_signal(SIGSEGV, (handler_3_t)&handle_signal, false);
#    elif defined(WINDOWS)
    SetUnhandledExceptionFilter(&handle_exception);
#    endif

    print("Starting drx_buf signal test\n");
    /* try to cause a segfault and make sure it didn't trigger the buffer to dump */
    if (SIGSETJMP(mark) == 0) {
        int *x = NULL;
        return *x;
    }
    print("Ending drx_buf signal test\n");
    return 0;
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
#    include "drx_buf-test-shared.h"
/* clang-format off */
START_FILE

#ifdef X64
# define FRAME_PADDING 0
#else
# define FRAME_PADDING 0
#endif

#define FUNCNAME test_asm_123
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test1
        /* Test 1: test the fast circular buffer */
     test1:
        mov      TEST_REG_ASM, DRX_BUF_TEST_1_ASM
        mov      TEST_REG_ASM, DRX_BUF_TEST_1_ASM
        jmp      test2
        /* Test 2: test the slow circular buffer */
     test2:
        mov      TEST_REG_ASM, DRX_BUF_TEST_2_ASM
        mov      TEST_REG_ASM, DRX_BUF_TEST_2_ASM
        jmp      test3
        /* Test 3: test the faulting buffer */
     test3:
        mov      TEST_REG_ASM, DRX_BUF_TEST_3_ASM
        mov      TEST_REG_ASM, DRX_BUF_TEST_3_ASM
        jmp      epilog1
     epilog1:
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
#elif defined(AARCHXX)
        b        test1
        /* Test 1: test the fast circular buffer */
     test1:
        MOV16    TEST_REG_ASM, DRX_BUF_TEST_1_ASM
        MOV16    TEST_REG_ASM, DRX_BUF_TEST_1_ASM
        b        test2
        /* Test 2: test the slow circular buffer */
     test2:
        MOV16    TEST_REG_ASM, DRX_BUF_TEST_2_ASM
        MOV16    TEST_REG_ASM, DRX_BUF_TEST_2_ASM
        b        test3
        /* Test 3: test the faulting buffer */
     test3:
        MOV16    TEST_REG_ASM, DRX_BUF_TEST_3_ASM
        MOV16    TEST_REG_ASM, DRX_BUF_TEST_3_ASM
        b        epilog1
    epilog1:
        RETURN
#else
# error NYI
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

#define FUNCNAME test_asm_45
        DECLARE_FUNC_SEH(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
#ifdef X86
        PUSH_CALLEE_SAVED_REGS()
        sub      REG_XSP, FRAME_PADDING /* align */
        END_PROLOG

        jmp      test4
        /* Test 4: test store registers */
     test4:
        mov      TEST_REG_ASM, DRX_BUF_TEST_4_ASM
        mov      TEST_REG_ASM, DRX_BUF_TEST_4_ASM
        jmp      test5
        /* Test 5: test store immediates */
     test5:
        mov      TEST_REG_ASM, DRX_BUF_TEST_5_ASM
        mov      TEST_REG_ASM, DRX_BUF_TEST_5_ASM
        jmp      test6
        /* Test 6: test drx_buf_insert_buf_memcpy() */
     test6:
        mov      TEST_REG_ASM, DRX_BUF_TEST_6_ASM
        mov      TEST_REG_ASM, DRX_BUF_TEST_6_ASM
        jmp      epilog2
     epilog2:
        add      REG_XSP, FRAME_PADDING /* make a legal SEH64 epilog */
        POP_CALLEE_SAVED_REGS()
        ret
#elif defined(AARCHXX)
        b        test4
        /* Test 4: test store registers */
     test4:
        MOV16    TEST_REG_ASM, DRX_BUF_TEST_4_ASM
        MOV16    TEST_REG_ASM, DRX_BUF_TEST_4_ASM
        b        test5
        /* Test 5: test store immediates */
     test5:
        MOV16    TEST_REG_ASM, DRX_BUF_TEST_5_ASM
        MOV16    TEST_REG_ASM, DRX_BUF_TEST_5_ASM
        b        test6
        /* Test 6: test drx_buf_insert_buf_memcpy() */
     test6:
        MOV16    TEST_REG_ASM, DRX_BUF_TEST_6_ASM
        MOV16    TEST_REG_ASM, DRX_BUF_TEST_6_ASM
        b        epilog2
    epilog2:
        RETURN
#else
# error NYI
#endif
        END_FUNC(FUNCNAME)
#undef FUNCNAME

END_FILE
/* clang-format on */
#endif
