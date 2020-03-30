#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

int
main()
{
    void *bad_mem = mmap(NULL, 0, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (bad_mem != MAP_FAILED) {
        printf("zero-length mmap succeeded\n");
        return EXIT_FAILURE;
    }

    void *mem = mmap(NULL, 1, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        printf("non-zero-length mmap failed\n");
        return EXIT_FAILURE;
    }

    int mprotect_res = mprotect(mem, 0, PROT_NONE);
    if (mprotect_res == -1) {
        printf("zero-length mprotect failed\n");
        return EXIT_FAILURE;
    }

    void *new_mem = mremap(mem, 0, 0, 0);
    if (new_mem != MAP_FAILED) {
        printf("zero-length mremap succeeded\n");
        return EXIT_FAILURE;
    }

    int munmap_res = munmap(mem, 0);
    if (munmap_res != -1) {
        printf("zero-length munmap succeeded\n");
        return EXIT_FAILURE;
    }

    printf("done\n");
}
