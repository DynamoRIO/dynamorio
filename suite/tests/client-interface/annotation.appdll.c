/* Don't link against libc so we can pass with STATIC_LIBRARY. */

#include <stdlib.h>
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

static int invoke_annotations;
static double *x_temp;

EXPORT
void
jacobi_init(int matrix_size, int annotation_mode)
{
    invoke_annotations = annotation_mode;

    x_temp = (double *) malloc(matrix_size * sizeof(double));
    if (invoke_annotations)
        TEST_ANNOTATION_EIGHT_ARGS(1, 2, 3, 4, 5, 6, 7, 8);
}

EXPORT
void
jacobi(double *dst, double *src, double **coefficients, double *rhs_vector, int limit)
{
    int i, j;

    for (i = 0; i < limit; i++) {
        x_temp[i] = rhs_vector[i];

        for (j = 0; j < i; j++) {
            x_temp[i] -= src[j] * coefficients[i][j];
        }

        if ((i == 0) && invoke_annotations)
            TEST_ANNOTATION_NINE_ARGS(1, 2, 3, 4, 5, 6, 7, 8, 9);

        for (j = 0; j < limit; j++) {
            x_temp[i] -= src[j] * coefficients[i][j];
        }
        x_temp[i] = x_temp[i] / coefficients[i][i];
    }
    for (i = 0; i < limit; i++) {
        dst[i] = x_temp[i];
    }
}

EXPORT
void
jacobi_exit()
{
    if (invoke_annotations)
        TEST_ANNOTATION_TEN_ARGS(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    free(x_temp);
}
