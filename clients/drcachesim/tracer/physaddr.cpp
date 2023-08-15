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

#ifdef LINUX
#    include <fcntl.h>
#    include <sys/types.h>
#    include <unistd.h>
#    include <linux/capability.h>
#    include <fstream>
#endif
#include "physaddr.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>

#include "dr_api.h"
#include "options.h"
#include "trace_entry.h"
#include "utils.h"
#include "droption.h"

namespace dynamorio {
namespace drmemtrace {

#if defined(X86_32) || defined(ARM_32)
#    define IF_X64_ELSE(x, y) y
#else
#    define IF_X64_ELSE(x, y) x
#endif

// XXX: can we share w/ core DR?
#define TESTALL(mask, var) (((mask) & (var)) == (mask))
#define TESTANY(mask, var) (((mask) & (var)) != 0)

#ifdef LINUX
#    define PAGEMAP_VALID 0x8000000000000000
#    define PAGEMAP_SWAP 0x4000000000000000
#    define PAGEMAP_PFN 0x007fffffffffffff
std::atomic<bool> physaddr_t::has_privileges_;
#endif

physaddr_t::physaddr_t()
#ifdef LINUX
    : page_size_(dr_page_size())
    , cache_idx_(0)
    , fd_(-1)
    , v2p_(nullptr)
    , drcontext_(nullptr)
    , count_(0)
    , num_hit_cache_(0)
    , num_hit_table_(0)
    , num_miss_(0)
#endif
{
#ifdef LINUX
    memset(last_vpage_, static_cast<char>(PAGE_INVALID), sizeof(last_vpage_));
    memset(last_ppage_, static_cast<char>(PAGE_INVALID), sizeof(last_ppage_));

    page_bits_ = 0;
    size_t temp = page_size_;
    while (temp > 1) {
        ++page_bits_;
        temp >>= 1;
    }
    NOTIFY(2, "Page size: %zu; bits: %d\n", page_size_, page_bits_);
#endif
}

physaddr_t::~physaddr_t()
{
#ifdef LINUX
    if (num_miss_ > 0) {
        NOTIFY(1,
               "physaddr: hit cache: " UINT64_FORMAT_STRING
               ", hit table " UINT64_FORMAT_STRING ", miss " UINT64_FORMAT_STRING "\n",
               num_hit_cache_, num_hit_table_, num_miss_);
    }
    if (v2p_ != nullptr)
        dr_hashtable_destroy(drcontext_, v2p_);
#endif
}

bool
physaddr_t::global_init()
{
#ifdef LINUX
    // This is invoked at process init time, so we can use heap in C++ classes
    // without affecting statically-linked dr$sim.

    // We need CAP_SYS_ADMIN to get valid data out of /proc/self/pagemap
    // (the kernel lets us read but just feeds us zeroes otherwise).
    constexpr int MAX_STATUS_FNAME = 64;
    char statf[MAX_STATUS_FNAME];
    dr_snprintf(statf, BUFFER_SIZE_ELEMENTS(statf), "/proc/%d/status", getpid());
    NULL_TERMINATE_BUFFER(statf);
    std::ifstream file(statf);
    std::string line;
    const std::string want("CapEff");
    while (std::getline(file, line)) {
        if (line.compare(0, want.size(), want) != 0)
            continue;
        std::istringstream ss(line);
        std::string unused;
        uint64_t bits;
        if (ss >> unused >> std::hex >> bits) {
            if (TESTALL(1 << CAP_SYS_ADMIN, bits)) {
                has_privileges_ = true;
                NOTIFY(1, "Has CAP_SYS_ADMIN\n");
            } else
                NOTIFY(1, "Does NOT have CAP_SYS_ADMIN\n");
        }
    }
    DR_ASSERT(std::atomic_is_lock_free(&has_privileges_));
    return has_privileges_;
#else
    return false;
#endif
}

bool
physaddr_t::init()
{
#ifdef LINUX
    if (!has_privileges_)
        return false;

    // Some threads may not do much, so start out small.
    constexpr int V2P_INITIAL_BITS = 9;
    // The hashtable lookup performance is important.
    // A closed-address hashtable is about 3x slower due to the extra
    // loads compared to the data inlined into the array here, and higher
    // resize thresholds are also slower.
    // With the setup here, the hashtable lookup is no longer the bottleneck.
    // We record the context so we can pass the same one in our destructor, which
    // might be called from a different thread.
    drcontext_ = dr_get_current_drcontext();
    v2p_ = dr_hashtable_create(drcontext_, V2P_INITIAL_BITS, 20,
                               /*synch=*/false, nullptr);

    // We avoid std::ostringstream to avoid malloc use for static linking.
    constexpr int MAX_PAGEMAP_FNAME = 64;
    char fname[MAX_PAGEMAP_FNAME];
    dr_snprintf(fname, BUFFER_SIZE_ELEMENTS(fname), "/proc/%d/pagemap", getpid());
    NULL_TERMINATE_BUFFER(fname);
    // We can't read pagemap with any buffered i/o, like ifstream, as we'll
    // get EINVAL on any non-8-aligned size, and ifstream at least likes to
    // read buffers of non-aligned sizes.
    fd_ = open(fname, O_RDONLY);
    // Accessing /proc/pid/pagemap requires privileges on some distributions,
    // such as Fedora with recent kernels.  We have no choice but to fail there.
    return (fd_ != -1);
#else
    // i#1727: we assume this is not possible on Windows.  If it is we
    // may want to split into physaddr_linux.cpp vs others.
    return false;
#endif
}

bool
physaddr_t::virtual2physical(void *drcontext, addr_t virt, OUT addr_t *phys,
                             OUT bool *from_cache)
{
#ifdef LINUX
    if (phys == nullptr)
        return false;
    addr_t vpage = page_start(virt);
    bool use_cache = true;
    if (from_cache != nullptr)
        *from_cache = false;
    if (op_virt2phys_freq.get_value() > 0 && ++count_ >= op_virt2phys_freq.get_value()) {
        // Flush the cache and re-sync with the kernel.
        // XXX i#4014: Provide a similar option that doesn't flush and just checks
        // whether mappings have changed?
        use_cache = false;
        memset(last_vpage_, static_cast<char>(PAGE_INVALID), sizeof(last_vpage_));
        // We do not bother to clear last_ppage_ as it is only used when
        // last_vpage_ holds legitimate values.
        dr_hashtable_clear(drcontext, v2p_);
        count_ = 0;
    }
    if (use_cache) {
        // Use cached values on the assumption that the kernel hasn't re-mapped
        // this virtual page.
        for (int i = 0; i < NUM_CACHE; ++i) {
            if (vpage == last_vpage_[i]) {
                if (from_cache != nullptr)
                    *from_cache = true;
                *phys = last_ppage_[i] + page_offs(virt);
                ++num_hit_cache_;
                return true;
            }
        }
        // XXX i#1703: add (debug-build-only) internal stats here and
        // on cache_t::request() fastpath.
        void *lookup = dr_hashtable_lookup(drcontext, v2p_, vpage);
        if (lookup != nullptr) {
            addr_t ppage = reinterpret_cast<addr_t>(lookup);
            // Restore a 0 payload.
            if (ppage == ZERO_ADDR_PAYLOAD)
                ppage = 0;
            if (from_cache != nullptr)
                *from_cache = true;
            *phys = ppage + page_offs(virt);
            last_vpage_[cache_idx_] = vpage;
            last_ppage_[cache_idx_] = ppage;
            cache_idx_ = (cache_idx_ + 1) % NUM_CACHE;
            ++num_hit_table_;
            return true;
        }
    }
    ++num_miss_;
    // Not cached, or forced to re-sync, so we have to read from the file.
    if (fd_ == -1) {
        NOTIFY(1, "v2p failure: file descriptor is invalid\n");
        return false;
    }
    // The pagemap file contains one 64-bit int per page.
    // See the docs at https://www.kernel.org/doc/Documentation/vm/pagemap.txt
    // For huge pages it's the same: there are just N consecutive entries, with
    // the first marked COMPOUND_HEAD and the rest COMPOUND_TAIL in the flags,
    // which we ignore here.
    off64_t offs = vpage / page_size_ * 8;
    if (lseek64(fd_, offs, SEEK_SET) < 0) {
        NOTIFY(1, "v2p failure: seek to " INT64_FORMAT_STRING " for %p failed\n", offs,
               vpage);
        return false;
    }
    uint64_t entry;
    if (read(fd_, (char *)&entry, sizeof(entry)) != sizeof(entry)) {
        NOTIFY(1, "v2p failure: read failed for %p\n", vpage);
        return false;
    }
    NOTIFY(3, "v2p: %p => entry " HEX64_FORMAT_STRING " @ offs " INT64_FORMAT_STRING "\n",
           vpage, entry, offs);
    if (!TESTALL(PAGEMAP_VALID, entry) || TESTANY(PAGEMAP_SWAP, entry)) {
        NOTIFY(1, "v2p failure: entry %p is invalid for %p in T%d\n", entry, vpage,
               dr_get_thread_id(drcontext));
        return false;
    }
    addr_t ppage = (addr_t)((entry & PAGEMAP_PFN) << page_bits_);
    // Despite the kernel handing out a 0 PFN for unprivileged reads, 0 is a valid
    // possible PFN.
    // Store 0 as a sentinel since 0 means no entry.
    dr_hashtable_add(drcontext, v2p_, vpage,
                     reinterpret_cast<void *>(ppage == 0 ? ZERO_ADDR_PAYLOAD : ppage));
    *phys = ppage + page_offs(virt);
    last_ppage_[cache_idx_] = ppage;
    last_vpage_[cache_idx_] = vpage;
    cache_idx_ = (cache_idx_ + 1) % NUM_CACHE;
    NOTIFY(2, "virtual %p => physical %p\n", virt, *phys);
    return true;
#else
    return false;
#endif
}

} // namespace drmemtrace
} // namespace dynamorio
