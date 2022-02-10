/* ******************************************************
 * Copyright (c) 2015-2020 Google, Inc.  All rights reserved.
 * ******************************************************/

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

/* This shared library is dynamically loaded by the annotation-concurrency test. */

#include <stdio.h>
#include <stdlib.h>
#include "configure.h"
#include "test_annotation_arguments.h"
#include "test_mode_annotations.h"

#if !(defined(WINDOWS) && defined(X64)) && !defined(ANNOTATIONS_DISABLED)
#    define USE_ANNOTATIONS 1
#    include "memcheck.h"
#endif

#ifdef WINDOWS
#    define EXPORT __declspec(dllexport)
#else /* UNIX */
#    define EXPORT __attribute__((visibility("default")))
#endif

typedef enum { false, true } bool;

static bool invoke_annotations;
static double *x_temp;

/* initialize the library instance */
EXPORT void
jacobi_init(int matrix_size, bool enable_annotations)
{
    invoke_annotations = enable_annotations;

    x_temp = (double *)malloc(matrix_size * sizeof(double));
    if (invoke_annotations)
        TEST_ANNOTATION_EIGHT_ARGS(matrix_size, 102, 103, 104, 105, 106, 107, 108);
}

/* compute one iteration of the Jacobi method for solving linear equations */
EXPORT void
jacobi(double *dst, double *src, double **coefficients, double *rhs_vector, int limit,
       unsigned int worker_id)
{
    int i, j;

    for (i = 0; i < limit; i++) {
        x_temp[i] = rhs_vector[i];

#ifdef USE_ANNOTATIONS
        if (invoke_annotations && worker_id < 3) {
            switch (worker_id) {
            case 0:
            case 1:
                for (j = 0; j < i; j++) {
                    x_temp[i] -= src[j] * coefficients[i][j];
                    TEST_ANNOTATION_ROTATE_VALGRIND_HANDLER((worker_id * 4) + (j & 3));
                }
                break;
            case 2:
                for (j = 0; j < i; j++) {
                    x_temp[i] -= src[j] * coefficients[i][j];
                    VALGRIND_MAKE_MEM_DEFINED_IF_ADDRESSABLE(&x_temp, i);
                }
                break;
            }
        } else {
#endif
            for (j = 0; j < i; j++)
                x_temp[i] -= src[j] * coefficients[i][j];
#ifdef USE_ANNOTATIONS
        }
#endif

        if (i == 0 && invoke_annotations)
            TEST_ANNOTATION_NINE_ARGS(i, 102, 103, 104, 105, 106, 107, 108, 109);

        for (j = 0; j < limit; j++)
            x_temp[i] -= src[j] * coefficients[i][j];
        x_temp[i] = x_temp[i] / coefficients[i][i];
    }
    for (i = 0; i < limit; i++)
        dst[i] = x_temp[i];
}

/* clean up library instance */
EXPORT void
jacobi_exit()
{
    if (invoke_annotations)
        TEST_ANNOTATION_TEN_ARGS(101, 102, 103, 104, 105, 106, 107, 108, 109, 110);
    free(x_temp);
}
