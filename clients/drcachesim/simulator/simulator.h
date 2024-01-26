/* **********************************************************
 * Copyright (c) 2015-2024 Google, Inc.  All rights reserved.
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

/* simulator: represent a simulator of a set of caching devices.
 */

#ifndef _SIMULATOR_H_
#define _SIMULATOR_H_ 1

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include <unordered_map>
#include <vector>

#include "analysis_tool.h"
#include "caching_device.h"
#include "caching_device_stats.h"
#include "memref.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

class simulator_t : public analysis_tool_t {
public:
    simulator_t()
    {
    }
    simulator_t(unsigned int num_cores, uint64_t skip_refs, uint64_t warmup_refs,
                double warmup_fraction, uint64_t sim_refs, bool cpu_scheduling,
                bool use_physical, unsigned int verbose);
    virtual ~simulator_t() = 0;

    std::string
    initialize_stream(memtrace_stream_t *serial_stream) override;

    std::string
    initialize_shard_type(shard_type_t shard_type) override;

    bool
    process_memref(const memref_t &memref) override;

protected:
    // Initialize knobs. Success or failure is indicated by setting/resetting
    // the success variable.
    void
    init_knobs(unsigned int num_cores, uint64_t skip_refs, uint64_t warmup_refs,
               double warmup_fraction, uint64_t sim_refs, bool cpu_scheduling,
               bool use_physical, unsigned int verbose);
    void
    print_core(int core) const;
    int
    find_emptiest_core(std::vector<int> &counts) const;
    virtual int
    core_for_thread(memref_tid_t tid);
    virtual void
    handle_thread_exit(memref_tid_t tid);

    addr_t
    virt2phys(addr_t virt) const;
    memref_t
    memref2phys(memref_t memref) const;

    addr_t
    page_start(addr_t addr) const
    {
        assert(page_size_ > 0);
        return addr & ~(page_size_ - 1);
    }

    addr_t
    synthetic_virt2phys(addr_t virt) const;

    // We use -1 instead of INVALID_THREAD_ID==0 because we have many tests
    // which set tid to 0 to mean "don't care".
    static constexpr memref_tid_t INVALID_LAST_THREAD = -1;
    static constexpr int INVALID_CORE_INDEX = -1;

    unsigned int knob_num_cores_;
    uint64_t knob_skip_refs_;
    uint64_t knob_warmup_refs_;
    double knob_warmup_fraction_;
    uint64_t knob_sim_refs_;
    bool knob_cpu_scheduling_;
    bool knob_use_physical_;
    unsigned int knob_verbose_;

    shard_type_t shard_type_ = SHARD_BY_THREAD;
    memtrace_stream_t *serial_stream_ = nullptr;
    memref_tid_t last_thread_ = INVALID_LAST_THREAD; // Only used for SHARD_BY_THREAD.
    int last_core_index_ = INVALID_CORE_INDEX;

    // For thread mapping to cores:
    std::unordered_map<int64_t, int> cpu2core_;
    // The following fields are only used for SHARD_BY_THREAD.
    std::unordered_map<memref_tid_t, int> thread2core_;
    std::vector<int> cpu_counts_;
    std::vector<int> thread_counts_;
    std::vector<int> thread_ever_counts_;

    // For virtual to physical page mappings.
    size_t page_size_ = 0;
    std::unordered_map<addr_t, addr_t> virt2phys_;
    addr_t prior_phys_addr_ = 0;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _SIMULATOR_H_ */
