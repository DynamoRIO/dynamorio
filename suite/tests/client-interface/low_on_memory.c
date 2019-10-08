/*
 * API regression test for low on memory events.
 */

#ifdef WINDOWS
#    include <windows.h>
#    include <stdio.h>
#else
#    include <stdio.h>
#    include <stdlib.h>
#endif

int
main(int argc, char **argv)
{
#ifdef WINDOWS
    HANDLE heap = GetProcessHeap();
#endif
    int total = 0;
    for (int i = 0; i < 200; i++) {
#ifdef WINDOWS
        int *my_integer = (int *)HeapAlloc(heap, HEAP_ZERO_MEMORY, sizeof(int));
#else
        int *my_integer = (int *)malloc(sizeof(int));
#endif
        *my_integer = 9;
        total += *my_integer;
#ifdef WINDOWS
        HeapFree(heap, 0, sizeof(int));
#else
        free(my_integer);
#endif
    }

    fprintf(stderr, "My total is %d\n", total);
}
