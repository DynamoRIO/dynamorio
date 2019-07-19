
#ifndef _SNOOP_FILTER_H_
#define _SNOOP_FILTER_H_ 1

#include "cache.h"
#include <unordered_map>
#include <bitset>
#include <vector>
#include <algorithm>

typedef struct _coherence_table_entry_t {
    std::vector<bool> sharers;
    bool dirty;
} coherence_table_entry_t;

class snoop_filter_t : public caching_device_t {
public:
    snoop_filter_t(void);
    virtual bool
    init(int block_size_, cache_t **caches_, int num_coherent_caches_);
    virtual void
    init_blocks(void);
    virtual void
    snoop(addr_t tag_in, int id_in, bool is_write);
    virtual void
    eviction_notification(addr_t tag_in, int id_in);
    void
    print_stats(void);

protected:
    // XXX: This initial coherence implementation uses a perfect snoop filter.
    std::unordered_map<addr_t, coherence_table_entry_t> coherence_table;
    cache_t **caches;
    int num_coherent_caches;
    int_least64_t num_writes;
    int_least64_t num_writebacks;
    int_least64_t num_invalidates;
};

#endif /* _SNOOP_FILTER_H_ */
