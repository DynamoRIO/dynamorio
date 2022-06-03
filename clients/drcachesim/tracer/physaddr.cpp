/* **********************************************************
 * Copyright (c) 2015-2022 Google, Inc.  All rights reserved.
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
#    include <sys/types.h>
#    include <unistd.h>
#    include <sys/stat.h>
#    include <fcntl.h>
#endif
#include "physaddr.h"
#include "../common/options.h"
#include "../common/utils.h"

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
static const addr_t PAGE_INVALID = (addr_t)-1;
#endif

physaddr_t::physaddr_t()
#ifdef LINUX
    : page_size_(dr_page_size())
    , last_vpage_(PAGE_INVALID)
    , last_ppage_(PAGE_INVALID)
    , fd_(-1)
    , count_(0)
#endif
{
#ifdef LINUX
    v2p_.table = nullptr;

    page_bits_ = 0;
    size_t temp = page_size_;
    while (temp > 1) {
        ++page_bits_;
        temp >>= 1;
    }
    NOTIFY(1, "Page size: %zu; bits: %d\n", page_size_, page_bits_);
#endif
}

physaddr_t::~physaddr_t()
{
#ifdef LINUX
    if (v2p_.table != nullptr)
        hashtable_delete(&v2p_);
#endif
}

bool
physaddr_t::init()
{
#ifdef LINUX
    // Some threads may not do much, so start out small.
    constexpr int V2P_INITIAL_BITS = 9;
    hashtable_init_ex(&v2p_, V2P_INITIAL_BITS, HASH_INTPTR, /*strdup=*/false,
                      /*synch=*/false, nullptr, nullptr, nullptr);

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
physaddr_t::virtual2physical(addr_t virt, OUT addr_t *phys)
{
#ifdef LINUX
    if (phys == nullptr)
        return false;
    addr_t vpage = page_start(virt);
    bool use_cache = true;
    if (op_virt2phys_freq.get_value() > 0 && ++count_ >= op_virt2phys_freq.get_value()) {
        // Flush the cache and re-sync with the kernel
        use_cache = false;
        last_vpage_ = PAGE_INVALID;
        hashtable_clear(&v2p_);
        count_ = 0;
    }
    if (use_cache) {
        // Use cached values on the assumption that the kernel hasn't re-mapped
        // this virtual page.
        if (vpage == last_vpage_) {
            *phys = last_ppage_ + page_offs(virt);
            return true;
        }
        // XXX i#1703: add (debug-build-only) internal stats here and
        // on cache_t::request() fastpath.
        void *lookup = hashtable_lookup(&v2p_, reinterpret_cast<void *>(vpage));
        if (lookup != nullptr) {
            addr_t ppage = reinterpret_cast<addr_t>(lookup);
            // Restore a 0 payload.
            if (ppage == ZERO_ADDR_PAYLOAD)
                ppage = 0;
            last_vpage_ = vpage;
            last_ppage_ = ppage;
            *phys = last_ppage_ + page_offs(virt);
            return true;
        }
    }
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
        NOTIFY(1, "v2p failure: entry is invalid for %p\n", vpage);
        return false;
    }
    last_ppage_ = (addr_t)((entry & PAGEMAP_PFN) << page_bits_);
    // Store 0 as a sentinel since 0 means no entry.
    hashtable_add(
        &v2p_, reinterpret_cast<void *>(vpage),
        reinterpret_cast<void *>(last_ppage_ == 0 ? ZERO_ADDR_PAYLOAD : last_ppage_));
    last_vpage_ = vpage;
    *phys = last_ppage_ + page_offs(virt);
    NOTIFY(2, "virtual %p => physical %p\n", virt, *phys);
    return true;
#else
    return false;
#endif
}
