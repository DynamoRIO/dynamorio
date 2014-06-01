#include <stdio.h>
#include <stdlib.h>
#include "annotation/valgrind.h"
#include "annotation/memcheck.h"

#define MATRIX_SIZE 100

int main()
{
#ifdef _MSC_VER // WINDOWS
    unsigned int i, j, k;
#else // UNIX
# ifdef X64
    register unsigned int i asm ("rdi");
# else
    register unsigned int i asm ("edi");
# endif
    unsigned int j, k;
#endif
    unsigned int data[MATRIX_SIZE][MATRIX_SIZE];

    void *alloc1 = malloc(1234);
    void *alloc2 = malloc(567);
    void *alloc3 = malloc(89);

    printf("The Valgrind annotation test thinks it is%srunning on Valgrind.\n",
        RUNNING_ON_VALGRIND ? " " : " not ");

    for (i = 0; i < (MATRIX_SIZE/2); i++) {
        for (j = 0; j < (MATRIX_SIZE/2); j++) {
            data[i][j] = i + (3 * j);

            if ((i == 27) && (j == 4)) {
                k = i;
                VALGRIND_MAKE_MEM_DEFINED_IF_ADDRESSABLE(alloc1, 1234);
                if (k != i) {
                    printf("Annotation changed %%xdi! Was %d, but it shifted to %d.\n",
                        k, i);
                    i = k;
                }
            }

            data[i*2][j] = (4 * i) / (j + 1);

            if (i == (2 * j)) {
                k = i;
                VALGRIND_MAKE_MEM_DEFINED_IF_ADDRESSABLE(alloc2, 567);
                if (k != i) {
                    printf("Annotation changed %%xdi! Was %d, but it shifted to %d.\n",
                        k, i);
                    i = k;
                }
            }

            data[i*2][j+i] = data[(MATRIX_SIZE/2) + (j-i)][3];

            if ((j > 0) && ((i / j) >= (MATRIX_SIZE - (j * (i % j))))) {
                k = i;
                VALGRIND_MAKE_MEM_DEFINED_IF_ADDRESSABLE(alloc3, 89);
                if (k != i) {
                    printf("Annotation changed %%xdi! Was %d, but it shifted to %d.\n",
                        k, i);
                    i = k;
                }
            }
        }
    }

    free(alloc1);
    free(alloc2);
    free(alloc3);
}
