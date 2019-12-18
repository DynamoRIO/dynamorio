/* **********************************************************
 * Copyright (c) 2013-2016 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2004 VMware, Inc.  All rights reserved.
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

#include "tools.h"

#define BUFFER_SIZE (3 * PAGE_SIZE_MAX)

static char buffer[BUFFER_SIZE];

/* FIXME: DR throws exception on unreadable memory w/o this: did we
 * used to behave differently?
 * Natively on modern hw+os this app dies right away due to NX.
 */
#define ADD_READ | ALLOW_READ
static int prot_codes[7] = { ALLOW_READ,
                             ALLOW_READ | ALLOW_EXEC,
                             ALLOW_WRITE ADD_READ,
                             ALLOW_READ | ALLOW_WRITE,
                             ALLOW_READ | ALLOW_WRITE | ALLOW_EXEC,
                             ALLOW_WRITE | ALLOW_EXEC ADD_READ,
                             ALLOW_EXEC };

static void
do_test(char *buf, size_t len)
{
    int i, j;
    const int stride = IF_X86_ELSE(1, 4 /*A32 alignment*/);
    char *code;
    protect_mem(buf, len, ALLOW_READ | ALLOW_WRITE);
    for (i = 0; i < 7; i++) {
        for (j = 0; j < 7; j++) {
            code = copy_to_buf(buf, len, NULL, CODE_INC, COPY_NORMAL);
            protect_mem_check(buf, len, prot_codes[i], ALLOW_READ | ALLOW_WRITE);
            protect_mem_check(buf, len, prot_codes[j], prot_codes[i]);
            test_print(code, 5);
            test_print(code, 2);
            if (j > 1 && j < 6) {
                code = copy_to_buf(buf, len, NULL, CODE_DEC, COPY_NORMAL);
                test_print(code, 3);
                test_print(code, 1);
                code = copy_to_buf(buf, len, NULL, CODE_SELF_MOD, COPY_NORMAL);
                test_print(code, 43981); /* 0xabcd which is < max 16-bit for ARM */
                test_print(code, 4660);
            }
            buf += stride;
            len -= stride;
            if (j > 1 && j < 6) {
                protect_mem_check(buf, len, ALLOW_READ | ALLOW_WRITE, prot_codes[j]);
                code = copy_to_buf(buf, len, NULL, CODE_SELF_MOD, COPY_NORMAL);
                protect_mem_check(buf, len, prot_codes[i], ALLOW_READ | ALLOW_WRITE);
                protect_mem_check(buf, len, prot_codes[j], prot_codes[i]);
                test_print(code, 4660);
                test_print(code, 43981);
            }
            protect_mem_check(buf, len, ALLOW_READ | ALLOW_WRITE, prot_codes[j]);
        }
    }
}

static void
test_alloc_overlap(void)
{
    /* Test i#1175: create some +rx DGC.  Then change it to +rw via mmap
     * instead of mprotect and ensure DR catches a subsequent code modification.
     * We allocate two pages and put the code one page in so that our
     * modifying mmap can have its prot match the region base's prot to
     * make it harder for DR to detect.
     */
    char *buf = allocate_mem((int)PAGE_SIZE * 2, ALLOW_READ | ALLOW_WRITE | ALLOW_EXEC);
    char *code = copy_to_buf(buf + PAGE_SIZE, PAGE_SIZE, NULL, CODE_INC, COPY_NORMAL);
    protect_mem(code, PAGE_SIZE, ALLOW_READ | ALLOW_EXEC);
    test_print(code, 42);
#ifdef UNIX
    code = mmap(buf, PAGE_SIZE * 2, PROT_READ | PROT_WRITE | PROT_EXEC,
                MAP_PRIVATE | MAP_ANON | MAP_FIXED, 0, 0);
#else
    code = VirtualAlloc(buf, PAGE_SIZE * 2, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
#endif
    code = copy_to_buf(code, PAGE_SIZE, NULL, CODE_DEC, COPY_NORMAL);
    test_print(code, 42);
    /* Hmm, there is no free_mem()... */
}

/* The stack tests require a variable-length array. */
#undef DO_STACK_TESTS
/* Variable-length arrays are always available in C99, but optional in C11. */
#if __STDC_VERSION__ >= 199901L && !defined(__STDC_NO_VLA__)
#    define DO_STACK_TESTS 1
#endif

int
main()
{
    /* get aligned test buffers */
#ifdef DO_STACK_TESTS
    char buffer_stack[3 * PAGE_SIZE]; /* PAGE_SIZE may not be a constant. */
    char *buf_stack = page_align(buffer_stack);
#endif
    char *buf = page_align(buffer);
    INIT();

#if USE_DYNAMO
    dynamorio_app_init();
    dynamorio_app_start();
#endif

    print("starting up\n");
    do_test(buf, 2 * PAGE_SIZE);
    print("starting stack tests\n");
#ifdef DO_STACK_TESTS
    do_test(buf_stack, 2 * PAGE_SIZE);
#else
    /* Run the test on the static buffer, again, to get the expected test output. */
    do_test(buf, 2 * PAGE_SIZE);
#endif

    print("starting overlap tests\n");
    test_alloc_overlap();

    print("about to exit\n");

#if USE_DYNAMO
    dynamorio_app_stop();
    dynamorio_app_exit();
#endif
}
