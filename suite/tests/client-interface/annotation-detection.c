
#include <stdlib.h>
#include <stdio.h>
#include "dr_annotations.h"
#include "test_annotation_arguments.h"

#ifdef _MSC_VER
# define INLINE __inline
#else
# define INLINE inline
# ifdef __LP64__
#  define uintptr_t unsigned long long
# else
#  define uintptr_t unsigned long
# endif
#endif

static void
annotation_wrapper(int a, int b, int c, int d)
{
    if (DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO())
        TEST_ANNOTATION_EIGHT_ARGS(a, b, c, d, a, b, c, d);
}

static int
power(int x, unsigned int exp)
{
    unsigned int i;
    for (i = 1; i < exp; i++)
        x *= x;
    return x;
}

static INLINE int
two()
{
    TEST_ANNOTATION_TWO_ARGS(__LINE__, 5, {
        printf("Native two args: %d, %d\n", __LINE__, 5);
    });
    return 2;
}

static INLINE int
three(unsigned int a)
{
    return TEST_ANNOTATION_THREE_ARGS(__LINE__, a, 4);
}

static void
colocated_annotation_test()
{
    TEST_ANNOTATION_EIGHT_ARGS(__LINE__, 2, 3, 4, 5, 6, 7, 8); TEST_ANNOTATION_NINE_ARGS(__LINE__, 2, 3, 4, 5, 6, 7, 8, 9);
}

int main(void)
{
    unsigned int i, j;

    // TEST_ANNOTATION_THREE_ARGS(64, 65, 66);

    test_annotation_nine_args(__LINE__, 2, 3, 4, 5, 6, 7, 8, 9);

    TEST_ANNOTATION_TWO_ARGS(1, two(), { printf("Native line %d\n", __LINE__); });
    TEST_ANNOTATION_TWO_ARGS(two(), 4, { printf("Native line %d\n", __LINE__); });
    printf("three args #0: %d\n", TEST_ANNOTATION_THREE_ARGS(1, 2, 3));
    printf("three args #1: %d\n", TEST_ANNOTATION_THREE_ARGS(three(9), two(), 1));
    printf("three args #2: %d\n", TEST_ANNOTATION_THREE_ARGS(two(), 4, three(2)));

    colocated_annotation_test();

    //if (1) return 0;

    j = (rand() % 10);
    for (i = 0; i < 10; i++) {
        switch ((i + j) % 10) {
            case 0:
                TEST_ANNOTATION_NINE_ARGS(
                    power(2, power(i, 3)), power(3, 4), power(i, j), power(2, i),
                    power(two(), 3), power(3, 4), DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO(),
                    power(i, j), power(DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO(), i));
            case 1:
                TEST_ANNOTATION_EIGHT_ARGS(__LINE__, 2, 3, 4, 5, 6, 7, 8);
            case 2:
case2:
            case 3:
            case 4:
                TEST_ANNOTATION_NINE_ARGS(__LINE__, 2, 3, 4, 5, 6, 7, 8, 9);
                TEST_ANNOTATION_EIGHT_ARGS(__LINE__, 2, 3, 4, 5, 6, 7, 8);
                break;
            case 5:
                TEST_ANNOTATION_NINE_ARGS(__LINE__, 2, 3, 4, 5, 6, 7, 8, 9);
                annotation_wrapper(i, j, i + j, i * j);
                TEST_ANNOTATION_TEN_ARGS(__LINE__, 2, 3, 4, 5, 6, 7, 8, 9, 10);
                break;
            case 6:
                TEST_ANNOTATION_EIGHT_ARGS(__LINE__, 2, 3, 4, 5, 6, 7, 8);
            case 7: {
                unsigned int a = 0, b = 0;
                TEST_ANNOTATION_TEN_ARGS(__LINE__, 2, 3, 4, 5, 6, 7, 8, 9, 10);
                TEST_ANNOTATION_TEN_ARGS(__LINE__, 2, 3, (unsigned int) (uintptr_t) &b, 5, 6, 7, 8, 9, 10);
                a = b;
                if (b > 0)
                    goto case2;
            }
            case 8:
            case 9:
            default:
                TEST_ANNOTATION_EIGHT_ARGS(__LINE__, 2, 3, 4, 5, 6, 7, 8);
                TEST_ANNOTATION_NINE_ARGS(__LINE__, 2, 3, 4, 5, 6, 7, 8, 9);
                TEST_ANNOTATION_TEN_ARGS(__LINE__, 2, 3, 4, 5, 6, 7, 8, 9, 10);
        }
    }
}
