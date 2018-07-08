/* **********************************************************
 * Copyright (c) 2014-2015 Google, Inc.  All rights reserved.
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

/* Tests correctness of decoding and substituting Valgrind annotations. The process is
 * to traverse an array in a branchy loop, occasionally invoking a Valgrind annotation.
 *
 * Four versions of the test are executed:
 *   - default (fast decoding)
 *   - full decoding
 *   - tiny bbs (-max_bb_instrs 3)
 *   - truncation (client chops every basic block to 2 instructions or less)
 *
 * XXX i#1610: also invoke all tests on an optimized version that compiles this app
 *             with -O3 (or equivalent)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "valgrind.h"
#include "memcheck.h"

#define MATRIX_SIZE 12

/* Arbitrary sizes for some memory allocations */
#define MEMORY_BLOCK_A 1234
#define MEMORY_BLOCK_B 567
#define MEMORY_BLOCK_C 89

int
main()
{
#ifdef _MSC_VER /* WINDOWS */
    unsigned int i, j, k;
#else /* UNIX */
    unsigned int i, j;
    /* Since the Valgrind annotations use the XDI register in a sequence of `rol`
     * instructions that comprise a nop, a decoding error in DR would corrupt the app's
     * XDI value. So this register variable plays "canary" to verify that XDI remains
     * intact across the annotation under DR (not supported in Windows).
     */
#    ifdef X64
    register unsigned int k asm("rdi");
#    else
    register unsigned int k asm("edi");
#    endif
#endif
    unsigned int data[MATRIX_SIZE][MATRIX_SIZE];

    /* Allocate some memory for marking as addressable. */
    void *alloc_a = malloc(MEMORY_BLOCK_A);
    void *alloc_b = malloc(MEMORY_BLOCK_B);
    void *alloc_c = malloc(MEMORY_BLOCK_C);

    memset(data, 0, sizeof(unsigned int) * MATRIX_SIZE * MATRIX_SIZE);

    printf("The Valgrind annotation test thinks it is%srunning on Valgrind.\n",
           RUNNING_ON_VALGRIND ? " " : " not ");

    /* The purpose of the loops is just to create several basic blocks that each have more
     * than a couple instructions, so that the truncation version of the test will try to
     * split the annotations between two bbs (which would cause a test failure). Branches
     * verify that annotation handling does not interfere with the app's control flow.
     */
    for (i = 0; i < (MATRIX_SIZE / 2); i++) {
        for (j = 0; j < (MATRIX_SIZE / 2); j++) {
            data[i][j] = i + (3 * j);

            if ((i == 3) && (j == 4)) {
                j = data[(j / i) + 2][data[i][j] - (j * i)];
                printf("Before annotation: j=%d\n", j);
                k = i;
                VALGRIND_MAKE_MEM_DEFINED_IF_ADDRESSABLE(alloc_a, MEMORY_BLOCK_A);
                if (k != i) {
                    printf("Annotation changed %%xdi! Was %d, but it shifted to %d.\n", i,
                           k);
                }
                printf("After annotation: j=%d\n", j);
                j = 4;
            }

            data[i * 2][j + 1] = (4 * i) / (j + 1);

            if (i == (2 * j)) {
                j = data[i / 2][j + (j / (data[i][i] + 1))] + 1;
                printf("Before annotation: j=%d\n", j);
                k = i;
                VALGRIND_MAKE_MEM_DEFINED_IF_ADDRESSABLE(alloc_b, MEMORY_BLOCK_B);
                if (k != i) {
                    printf("Annotation changed %%xdi! Was %d, but it shifted to %d.\n", i,
                           k);
                }
                printf("After annotation: j=%d\n", j);
                j = i / 2;
            }

            data[i * 2][j + i] = data[(MATRIX_SIZE / 2) + (j - i)][3];

            if ((j > 0) && ((i / j) >= (MATRIX_SIZE - (j * (i % j))))) {
                data[i / 2][j + data[i][j]] = j;
                printf("Before annotation: data[i/2][j + data[i][j]]=%d\n",
                       data[i / 2][j + data[i][j]]);
                k = i;
                VALGRIND_MAKE_MEM_DEFINED_IF_ADDRESSABLE(alloc_c, MEMORY_BLOCK_C);
                if (k != i) {
                    printf("Annotation changed %%xdi! Was %d, but it shifted to %d.\n", i,
                           k);
                }
                printf("After annotation: data[i/2][j + data[i][j]]=%d\n",
                       data[i / 2][j + data[i][j]]);
            }
        }
    }

    printf("\n--------\n\nFinal matrix values:\n");
    for (i = 0; i < MATRIX_SIZE; i++) {
        printf("Row %02d:", i);
        for (j = 0; j < MATRIX_SIZE; j++)
            printf(" %02d", data[i][j]);
        printf("\n");
    }
    printf("\n--------\n\n");

    free(alloc_a);
    free(alloc_b);
    free(alloc_c);

    return 0;
}
