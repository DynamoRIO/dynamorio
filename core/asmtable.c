#include "lib/instrument.h"
#include "asmtable.h"

static void
asmtable_init(asmtable_t *table, uint hash_bits)
{
    table->hash_bits = hash_bits;
    table->capacity = (1U << hash_bits);
    table->entry_count = 0;
    table->resize_threshold = table->capacity * table->density / 100;
    table->hash_mask = ((~PTR_UINT_0)>>(HASH_TAG_BITS-(hash_bits)));
    table->table = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, asmtable_entry_t*, table->capacity,
                                    ACCT_OTHER, UNPROTECTED);
    memset(table->table, 0, table->capacity * sizeof(asmtable_entry_t*));
}

static void
asmtable_expand(asmtable_t *table)
{
    asmtable_entry_t *e, *prev_e, *next_e;
    asmtable_entry_t **old_table = table->table;
    uint old_capacity = table->capacity;
    uint i;
#ifdef DEBUG
    dcontext_t *dcontext = get_thread_private_dcontext();
#endif

    asmtable_init(table, table->hash_bits + 1);

    for (i = 0; i < old_capacity; i++) {
        for (e = old_table[i], prev_e = NULL; e != NULL; e = next_e) {
            next_e = e->next;
            asmtable_insert(table, e);
        }
    }

    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, old_table, asmtable_entry_t*, old_capacity,
                    ACCT_OTHER, UNPROTECTED);

    table->resize_callback();

    RELEASE_LOG(THREAD, LOG_MONITOR, 1,
                "AsmTable resized to capacity\n", table->capacity);
}

asmtable_t *
asmtable_create(uint hash_bits, uint density, mutex_t *lock,
                void (*free_entry_func)(void *), void (*resize_callback)())
{
    asmtable_t *table;
    table = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, asmtable_t, ACCT_OTHER, UNPROTECTED);
    table->density = density;
    table->lock = lock;
    asmtable_init(table, hash_bits);
    table->free_entry_func = free_entry_func;
    table->resize_callback = resize_callback;
    memset(table->table, 0, sizeof(asmtable_t));
    return table;
}

void
asmtable_destroy(asmtable_t *table)
{
    asmtable_clear(table);
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, table->table, asmtable_entry_t*, table->capacity,
                    ACCT_OTHER, UNPROTECTED);
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, table, asmtable_t, ACCT_OTHER, UNPROTECTED);
}

void *
asmtable_lookup(asmtable_t *table, ptr_uint_t key)
{
    ptr_uint_t bucket = (key & table->hash_mask);
    asmtable_entry_t *entry = table->table[bucket];
    while (entry != NULL && entry->key != key)
        entry = entry->next;
    return entry;
}

void
asmtable_insert(asmtable_t *table, asmtable_entry_t *entry)
{
    ptr_uint_t bucket = (entry->key & table->hash_mask);
    entry->next = table->table[bucket];
    table->table[bucket] = entry;
    table->entry_count++;
    if (table->entry_count >= table->resize_threshold)
        asmtable_expand(table);
}

bool
asmtable_remove(asmtable_t *table, ptr_uint_t key)
{
    ptr_uint_t bucket = (key & table->hash_mask);
    asmtable_entry_t *removal, *entry = table->table[bucket];
    if (entry == NULL)
        return false;
    if (key == entry->key) { // remove the head
        removal = table->table[bucket];
        table->table[bucket] = table->table[bucket]->next;
    } else {
        while (entry->next != NULL && entry->next->key != key)
            entry = entry->next;
        if (entry->next == NULL)
            return false;
        removal = entry->next;
        entry->next = entry->next->next;
    }
    table->free_entry_func(removal);
    table->entry_count--;
    return true;
}

void
asmtable_clear(asmtable_t *table)
{
    uint i;
    asmtable_entry_t *entry, *next;
    for (i = 0; i < table->capacity; i++) {
        entry = table->table[i];
        for (; entry != NULL; entry = next) {
            next = entry->next;
            table->free_entry_func(entry);
        }
    }
    table->entry_count = 0;
}

void
asmtable_lock(asmtable_t *table)
{
    mutex_lock(table->lock);
}

void
asmtable_unlock(asmtable_t *table)
{
    mutex_unlock(table->lock);
}

