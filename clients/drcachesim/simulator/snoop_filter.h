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

#ifndef _SNOOP_FILTER_H_
#define _SNOOP_FILTER_H_ 1

#include <stdint.h>

#include <unordered_map>
#include <vector>

#include "cache.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

struct coherence_table_entry_t {
    std::vector<bool> sharers;
    bool dirty;
};

class snoop_filter_t {
public:
    snoop_filter_t(void);
    virtual ~snoop_filter_t()
    {
    }
    virtual bool
    init(cache_t **caches, int num_snooped_caches);
    virtual void
    snoop(addr_t tag, int id, bool is_write);
    virtual void
    snoop_eviction(addr_t tag, int id);
    void
    print_stats(void);

protected:
    // XXX: This initial coherence implementation uses a perfect snoop filter.
    std::unordered_map<addr_t, coherence_table_entry_t> coherence_table_;
    cache_t **caches_;
    int num_snooped_caches_;
    int64_t num_writes_;
    int64_t num_writebacks_;
    int64_t num_invalidates_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _SNOOP_FILTER_H_ */
