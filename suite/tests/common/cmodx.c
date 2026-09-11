/* **********************************************************
 * Copyright (c) 2026 ARM Limited. All rights reserved.
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

#include "tools.h"
#include "thread.h"

#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>

#ifdef AARCH64
#    define BRANCH1 0x14000001 /* b . + 4 */
#    define BRANCH2 0x14000002 /* b . + 8 */
#    define MOV0 0x52800000    /* mov w0, #0 */
#    define RET 0xd65f03c0     /* ret */
#else
#    error NYI
#endif

THREAD_FUNC_RETURN_TYPE
modify_code(void *arg)
{
    uint32_t *code = (uint32_t *)arg;
    code[0] = BRANCH2;

    return THREAD_FUNC_RETURN_ZERO;
}

int
main(int argc, char **argv)
{
    uint32_t *x = mmap(0, 12, PROT_EXEC | PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (x == MAP_FAILED) {
        perror("Failed to map memory:");
        return 1;
    }
    int (*f)(int) = (int (*)(int))x;
    unsigned long long n;
    int i;

    /* Initial version of f ignores its argument and returns 0. */
    x[0] = BRANCH1;
    x[1] = MOV0;
    x[2] = RET;
    tools_clear_icache(x, x + 3);

    /* Execute it a few times to warm up the cache. */
    for (i = 0; i < 1000; i++) {
        int r = f(i);
        if (r != 0) {
            print("FAIL 1: %d %d\n", i, r);
            exit(1);
        }
    }

    /* Start a thread to modify f. */
    thread_t modification_thread = create_thread(modify_code, (void *)x);

    /* We do not clear the cache but expect the change to take effect eventually. */
    for (n = 0;; n++) {
        /* Modified version of f returns its argument. */
        int r = f(7);
        if (r == 7)
            break;
        if (r != 0) {
            print("FAIL 2: %llu %d\n", n, r);
            exit(1);
        }
    }

    join_thread(modification_thread);

    print("PASS: %llu\n", n);

    return 0;
}
