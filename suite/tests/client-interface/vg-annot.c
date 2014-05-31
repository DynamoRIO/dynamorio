#include <stdio.h>
#include <stdlib.h>
#include "annotation/valgrind.h"
#include "annotation/memcheck.h"

#define MATRIX_SIZE 100

int main()
{
    uint i, j, data[MATRIX_SIZE][MATRIX_SIZE];

    void *alloc1 = malloc(1234);
    void *alloc2 = malloc(567);
    void *alloc3 = malloc(89);

    printf("The Valgrind annotation test thinks it is%srunning on Valgrind.\n",
        RUNNING_ON_VALGRIND ? " " : " not ");

    for (i = 0; i < (MATRIX_SIZE/2); i++) {
        for (j = 0; j < (MATRIX_SIZE/2); j++) {
            data[i][j] = i + (3 * j);

            if ((i == 27) && (j == 4))
                VALGRIND_MAKE_MEM_DEFINED_IF_ADDRESSABLE(alloc1, 1234);

            data[i*2][j] = (4 * i) / (j + 1);

            if (i == (2 * j))
                VALGRIND_MAKE_MEM_DEFINED_IF_ADDRESSABLE(alloc2, 567);

            data[i*2][j+i] = data[(MATRIX_SIZE/2) + (j-i)][3];

            if ((j > 0) && ((i / j) >= (MATRIX_SIZE - (j * (i % j)))))
                VALGRIND_MAKE_MEM_DEFINED_IF_ADDRESSABLE(alloc3, 89);
        }
    }

    free(alloc1);
    free(alloc2);
    free(alloc3);
}
