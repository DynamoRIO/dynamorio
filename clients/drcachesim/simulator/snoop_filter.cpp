
#include "snoop_filter.h"
#include <iostream>
#include <iomanip>
#include <assert.h>
#include <algorithm>

snoop_filter_t::snoop_filter_t(void)
{
}

bool
snoop_filter_t::init(cache_t **caches_, int num_snooped_caches_)
{
    caches = caches_;
    num_snooped_caches = num_snooped_caches_;
    num_writes = 0;
    num_writebacks = 0;
    num_invalidates = 0;

    return true;
}

/*  This function should be called for all misses in snooped caches as well as
 *  all writes to coherent caches.
 */
void
snoop_filter_t::snoop(addr_t tag, int id_in, bool is_write)
{
    coherence_table_entry_t *coherence_entry = &coherence_table[tag];
    // Initialize new snoop filter entry.
    if (coherence_entry->sharers.empty()) {
        coherence_entry->sharers.resize(num_snooped_caches, false);
    }

    unsigned int num_sharers = std::count(coherence_entry->sharers.begin(),
                                          coherence_entry->sharers.end(), true);

    // Check that cache id is valid.
    assert(id_in >= 0 && id_in < num_snooped_caches);
    // Check that tag is valid.
    assert(tag != TAG_INVALID);
    // Check that any dirty line is only held in one snooped cache.
    assert(!coherence_entry->dirty || num_sharers == 1);

    if (is_write) {
        num_writes++;
    }

    if (!coherence_entry->sharers[id_in] && coherence_entry->dirty) {
        num_writebacks++;
        coherence_entry->dirty = false;
    }
    if (is_write) {
        coherence_entry->dirty = true;
    }

    if (num_sharers == 0) {
        std::fill(coherence_entry->sharers.begin(), coherence_entry->sharers.end(),
                  false);
        coherence_entry->sharers[id_in] = true;
    } else if (num_sharers > 0) {
        if (!is_write) {
            coherence_entry->sharers[id_in] = true;
        } else { /*is_write*/
            // Writes will invalidate other caches.
            for (int i = 0; i < num_snooped_caches; i++) {
                if (coherence_entry->sharers[i] && id_in != i) {
                    caches[i]->invalidate(tag, INVALIDATION_COHERENCE);
                    num_invalidates++;
                }
            }
            std::fill(coherence_entry->sharers.begin(), coherence_entry->sharers.end(),
                      false);
            coherence_entry->sharers[id_in] = true;
        }
    }
    return;
}

/* This function is called whenever a coherent cache evicts a line. */
void
snoop_filter_t::snoop_eviction(addr_t tag, int id_in)
{
    coherence_table_entry_t *coherence_entry = &coherence_table[tag];

    // Check if sharer list is initialized.
    assert(coherence_entry->sharers.size() == (uint64_t)num_snooped_caches);
    // Check that cache id is valid.
    assert(id_in >= 0 && id_in < num_snooped_caches);
    // Check that tag is valid.
    assert(tag != TAG_INVALID);
    // Check that we currently have this cache marked as a sharer.
    assert(coherence_entry->sharers[id_in]);

    if (coherence_entry->dirty) {
        num_writebacks++;
        coherence_entry->dirty = false;
    }

    coherence_entry->sharers[id_in] = false;
}

void
snoop_filter_t::print_stats(void)
{
    std::cerr.imbue(std::locale("")); // Add commas, at least for my locale.
    std::string prefix = "    ";
    std::cerr << "Coherence stats:" << std::endl;
    std::cerr << prefix << std::setw(18) << std::left << "Total writes:" << std::setw(20)
              << std::right << num_writes << std::endl;
    std::cerr << prefix << std::setw(18) << std::left << "Invalidations:" << std::setw(20)
              << std::right << num_invalidates << std::endl;
    std::cerr << prefix << std::setw(18) << std::left << "Writebacks:" << std::setw(20)
              << std::right << num_writebacks << std::endl;
    std::cerr.imbue(std::locale("C")); // Reset to avoid affecting later prints.
}
