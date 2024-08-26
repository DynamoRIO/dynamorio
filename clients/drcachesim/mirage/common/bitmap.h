#ifndef __BITMAP_H__
#define __BITMAP_H__

// a simple bitmap implementation
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "assert.h"


struct bitmap_t {
    uint32_t size;
    bool *bits;
};

struct bitmap_t *bitmap_create(uint32_t size);
int bitmap_acquire(struct bitmap_t *bitmap);

#endif // __BITMAP_H__