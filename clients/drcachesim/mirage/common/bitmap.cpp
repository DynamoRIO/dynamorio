#include "bitmap.h"

struct bitmap_t *bitmap_create(uint32_t size) {
    struct bitmap_t* b = (struct bitmap_t*)malloc(sizeof(struct bitmap_t));
    assert(b != NULL);
    b->size = size;
    b->bits = (bool*)calloc(size, sizeof(bool)); 
    assert(b->bits != NULL);
    return b;
}

int bitmap_acquire(struct bitmap_t *bitmap) {
    // find the first available bit
    for (uint32_t i = 0; i < bitmap->size; i++) {
        if (bitmap->bits[i] == false) {
            bitmap->bits[i] = true;
            return i;
        }
    }
    return -1;
}