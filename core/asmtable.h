
#ifndef _ASMTABLE_H_
#define _ASMTABLE_H_ 1

#include "globals.h"

#ifdef JITOPT

typedef struct asmtable_entry_t asmtable_entry_t;
struct asmtable_entry_t {
    ptr_uint_t key;
    asmtable_entry_t *next;
};

typedef struct _asmtable_t {
    uint entry_count;
    uint density;
    uint capacity;
    uint resize_threshold;
    ptr_uint_t hash_mask;
    ptr_uint_t hash_bits;
    mutex_t *lock;
    void (*free_entry_func)(void *);
    void (*resize_callback)();
    asmtable_entry_t **table;
} asmtable_t;

asmtable_t *
asmtable_create(uint hash_bits, uint density, mutex_t *lock,
                void (*free_entry_func)(void *), void (*resize_callback)());

void
asmtable_destroy(asmtable_t *table);

void *
asmtable_lookup(asmtable_t *table, ptr_uint_t key);

void
asmtable_insert(asmtable_t *table, asmtable_entry_t *entry);

bool
asmtable_remove(asmtable_t *table, ptr_uint_t key);

void
asmtable_clear(asmtable_t *table);

void
asmtable_lock(asmtable_t *table);

void
asmtable_unlock(asmtable_t *table);

#endif /* JITOPT */

#endif

