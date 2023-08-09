/* **********************************************************
 * Copyright (c) 2015-2023 Google, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* physaddr: translates virtual addresses to physical addresses.
 */

#ifndef _PHYSADDR_H_
#define _PHYSADDR_H_ 1

#include <stddef.h>
#include <stdint.h>

#include <atomic>

#include "dr_api.h"
#include "hashtable.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

// This class is not thread-safe: the caller should create a separate instance
// per thread.
class physaddr_t {
public:
    physaddr_t();
    ~physaddr_t();
    bool
    init();

    // If translation from "virt" to its corresponding physical address is
    // successful, returns true and stores the physical address in "phys".
    // 0 is a possible valid physical address, as are large values beyond
    // the amount of RAM due to holes in the physical address space.
    // Returns in "from_cache" whether the physical address had been queried before
    // and was available in a local cache (which is cleared at -virt2phys_freq).
    bool
    virtual2physical(void *drcontext, addr_t virt, OUT addr_t *phys,
                     OUT bool *from_cache = nullptr);

    // This must be called once prior to any instance variables.
    // (If this class weren't used in a DR client context we could use a C++
    // mutex or pthread do-once but those are not safe here.)
    static bool
    global_init();

private:
#ifdef LINUX
    inline addr_t
    page_start(addr_t addr)
    {
        return ALIGN_BACKWARD(addr, page_size_);
    }
    inline uint64_t
    page_offs(addr_t addr)
    {
        return addr & ((1 << page_bits_) - 1);
    }

    size_t page_size_;
    int page_bits_;
    static constexpr int NUM_CACHE = 8;
    addr_t last_vpage_[NUM_CACHE];
    addr_t last_ppage_[NUM_CACHE];
    // FIFO replacement.
    int cache_idx_;
    // TODO i#4014: An app with thousands of threads might hit open file limits,
    // and even a hundred threads will use up DR's private FD limit and push
    // other files into potential app conflicts.
    // Sharing the descriptor would require locks, however.  Evaluating
    // how best to do that (maybe the caching will reduce the contention enough)
    // is future work.
    int fd_;
    // We would use std::unordered_map, but that is not compatible with
    // statically linking drmemtrace into an app.
    // The drcontainers hashtable is too slow due to the extra dereferences:
    // we need an open-addressed table.
    void *v2p_;
    // We must pass the same context to free as we used to allocate.
    void *drcontext_;
    static constexpr addr_t PAGE_INVALID = (addr_t)-1;
    // With hashtable_t nullptr is how non-existence is shown, so we store
    // an actual 0 address (can happen for physical) as this sentinel.
    static constexpr addr_t ZERO_ADDR_PAYLOAD = PAGE_INVALID;
    unsigned int count_;
    uint64_t num_hit_cache_;
    uint64_t num_hit_table_;
    uint64_t num_miss_;
    static std::atomic<bool> has_privileges_;
#endif
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _PHYSADDR_H_ */
