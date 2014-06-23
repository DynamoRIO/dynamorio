
#include <stdlib.h>
#include <stdio.h>
#include "annotation/dr_annotations.h"
#include "annotation/test_annotation_arguments.h"

int main(void)
{
    int i, j;

    j = (rand() % 10);
    for (i = 0; i < 10; i++) {
        switch ((i + j) % 10) {
            case 0:
                TEST_ANNOTATION_NINE_ARGS(DYNAMORIO_ANNOTATE_RUNNING_ON_DYNAMORIO(),
                                          2, 3, 4, 5, 6, 7, 8, 9);
            case 1:
                TEST_ANNOTATION_EIGHT_ARGS(1, 2, 3, 4, 5, 6, 7, 8);
            case 2:
case2:
            case 3:
            case 4:
                TEST_ANNOTATION_NINE_ARGS(1, 2, 3, 4, 5, 6, 7, 8, 9);
                TEST_ANNOTATION_EIGHT_ARGS(1, 2, 3, 4, 5, 6, 7, 8);
                break;
            case 5:
                TEST_ANNOTATION_NINE_ARGS(1, 2, 3, 4, 5, 6, 7, 8, 9);
                TEST_ANNOTATION_TEN_ARGS(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
                break;
            case 6:
                TEST_ANNOTATION_EIGHT_ARGS(1, 2, 3, 4, 5, 6, 7, 8);
            case 7:
                TEST_ANNOTATION_TEN_ARGS(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
                TEST_ANNOTATION_TEN_ARGS(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
                goto case2;
            case 8:
            case 9:
            default:
                TEST_ANNOTATION_EIGHT_ARGS(1, 2, 3, 4, 5, 6, 7, 8);
                TEST_ANNOTATION_NINE_ARGS(1, 2, 3, 4, 5, 6, 7, 8, 9);
                TEST_ANNOTATION_TEN_ARGS(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
        }
    }
}
