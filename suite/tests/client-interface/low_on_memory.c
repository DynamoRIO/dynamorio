/*
 * API regression test for low on memory events.
 */

#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char **argv)
{
    int total = 0;
    for (int i = 0; i < 200; i++) {

        int *my_integer = (int *)malloc(sizeof(int));
        *my_integer = 9;
        total += *my_integer;
        free(my_integer);
    }

    fprintf(stderr, "My total is %d\n", total);
}
