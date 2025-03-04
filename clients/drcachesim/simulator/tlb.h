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

/* tlb: represents a single hardware TLB.
 */

#ifndef _TLB_H_
#define _TLB_H_ 1

#include <optional>
#include <random>

#include "caching_device.h"
#include "memref.h"
#include "tlb_entry.h"
#include "tlb_stats.h"

namespace dynamorio {
namespace drmemtrace {

class tlb_t : public caching_device_t {
public:
    tlb_t(const std::string &name = "tlb");

    void
    request(const memref_t &memref) override;

    // TODO i#4816: The addition of the pid as a lookup parameter beyond just the tag
    // needs to be imposed on the parent methods invalidate(), contains_tag(), and
    // propagate_eviction() by overriding them.

    // Returns the replacement policy used by this TLB.
    // XXX: i#7287: This class currently has two subclasses by replacenent policy.
    // We eventually want to decouple the policies from the classes.
    std::string
    get_replace_policy() const override = 0;

protected:
    void
    init_blocks() override;
    // Optimization: remember last pid in addition to last tag
    memref_pid_t last_pid_;
};

/**
 * TLB with least frequently used replacement policy.
 *
 * LFU has us hold a counter per way in the TLB entry.
 * The counter is incremented when the way is accessed.
 * When evicting, we choose the way with the least frequent access count.
 *
 * Note that the LFU logic itself is implemented
 * in the parent class caching_device_t.
 */
class tlb_lfu_t : public tlb_t {
public:
    tlb_lfu_t(const std::string &name = "tlb_lfu");

    std::string
    get_replace_policy() const override;
};

/**
 * TLB with a bit-based PLRU replacement policy.
 *
 * Bit pseudo-LRU has us hold one bit per way in the TLB entry.
 * The bit is set when the way is accessed.
 * Once all bits for a block are set, we reset them all to 0.
 * When evicting, we choose a random way with a zero bit to evict.
 *
 * When seed is -1, a seed is chosen with std::random_device().
 * XXX: Once we update our toolchains to guarantee C++17 support we
 * could use std::optional here.
 */
class tlb_bit_plru_t : public tlb_t {
public:
    tlb_bit_plru_t(const std::string &name = "tlb_bit_plru", int seed = -1);

    std::string
    get_replace_policy() const override;

protected:
    void
    access_update(int block_idx, int way) override;
    int
    get_next_way_to_replace(int block_idx) const override;
    mutable std::mt19937 gen_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _TLB_H_ */
