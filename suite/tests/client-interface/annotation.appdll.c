/* Don't link against libc so we can pass with STATIC_LIBRARY. */

#include <stdio.h>
#include "configure.h"
#include "annotation/test_annotation_arguments.h"

/* We can't get this from tools.h, or we'll be linked against tools.c which uses
 * libc.
 */
#ifdef WINDOWS
# define EXPORT __declspec(dllexport)
#else /* UNIX */
# define EXPORT __attribute__((visibility("default")))
#endif

EXPORT
void
matrix_subtract(unsigned int mode, double *dst, double *src, double **coefficients,
                int base, int limit)
{
    int i;

    if (mode == 0)
        TEST_ANNOTATION_EIGHT_ARGS(1, 2, 3, 4, 5, 6, 7, 8);

    for (i = 0; i < limit; i++) {
        dst[base] -= src[i] * coefficients[base][i];
    }

    if (mode == 0)
        TEST_ANNOTATION_TEN_ARGS(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
}

EXPORT
void
matrix_divide(unsigned int mode, double *dst, double **coefficients, int i)
{
    dst[i] = dst[i] / coefficients[i][i];

    if (mode == 0)
        TEST_ANNOTATION_NINE_ARGS(1, 2, 3, 4, 5, 6, 7, 8, 9);
}
