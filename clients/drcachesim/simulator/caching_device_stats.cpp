/* **********************************************************
 * Copyright (c) 2015-2017 Google, Inc.  All rights reserved.
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

#include <assert.h>
#include <iostream>
#include <iomanip>
#include "caching_device_stats.h"

caching_device_stats_t::caching_device_stats_t(const std::string &miss_file,
                                               bool warmup_enabled)
    : success(true)
    , num_hits(0)
    , num_misses(0)
    , num_child_hits(0)
    , num_inclusive_invalidates(0)
    , num_hits_at_reset(0)
    , num_misses_at_reset(0)
    , num_child_hits_at_reset(0)
    , warmup_enabled(warmup_enabled)
    , file(nullptr)
{
    if (miss_file.empty()) {
        dump_misses = false;
    } else {
#ifdef HAS_ZLIB
        file = gzopen(miss_file.c_str(), "w");
#else
        file = fopen(miss_file.c_str(), "w");
#endif
        if (file == nullptr) {
            dump_misses = false;
            success = false;
        } else
            dump_misses = true;
    }
}

caching_device_stats_t::~caching_device_stats_t()
{
    if (file != nullptr) {
#ifdef HAS_ZLIB
        gzclose(file);
#else
        fclose(file);
#endif
    }
}

void
caching_device_stats_t::access(const memref_t &memref, bool hit)
{
    // We assume we're single-threaded.
    // We're only computing miss rate so we just inc counters here.
    if (hit)
        num_hits++;
    else {
        num_misses++;
        if (dump_misses)
            dump_miss(memref);
    }
}

void
caching_device_stats_t::child_access(const memref_t &memref, bool hit)
{
    if (hit)
        num_child_hits++;
    // else being computed in access()
}

void
caching_device_stats_t::dump_miss(const memref_t &memref)
{
    addr_t pc, addr;
    if (type_is_instr(memref.data.type))
        pc = memref.instr.addr;
    else { // data ref: others shouldn't get here
        assert(type_is_prefetch(memref.data.type) ||
               memref.data.type == TRACE_TYPE_READ ||
               memref.data.type == TRACE_TYPE_WRITE);
        pc = memref.data.pc;
    }
    addr = memref.data.addr;
#ifdef HAS_ZLIB
    gzprintf(file, "0x%zx,0x%zx\n", pc, addr);
#else
    fprintf(file, "0x%zx,0x%zx\n", pc, addr);
#endif
}

void
caching_device_stats_t::print_warmup(std::string prefix)
{
    std::cerr << prefix << std::setw(18) << std::left << "Warmup hits:" << std::setw(20)
              << std::right << num_hits_at_reset << std::endl;
    std::cerr << prefix << std::setw(18) << std::left << "Warmup misses:" << std::setw(20)
              << std::right << num_misses_at_reset << std::endl;
}

void
caching_device_stats_t::print_counts(std::string prefix)
{
    std::cerr << prefix << std::setw(18) << std::left << "Hits:" << std::setw(20)
              << std::right << num_hits << std::endl;
    std::cerr << prefix << std::setw(18) << std::left << "Misses:" << std::setw(20)
              << std::right << num_misses << std::endl;
    std::cerr << prefix << std::setw(18) << std::left << "Invalidations:" << std::setw(20)
              << std::right << num_inclusive_invalidates << std::endl;
}

void
caching_device_stats_t::print_rates(std::string prefix)
{
    if (num_hits + num_misses > 0) {
        std::string miss_label = "Miss rate:";
        if (num_child_hits != 0)
            miss_label = "Local miss rate:";
        std::cerr << prefix << std::setw(18) << std::left << miss_label << std::setw(20)
                  << std::fixed << std::setprecision(2) << std::right
                  << ((float)num_misses * 100 / (num_hits + num_misses)) << "%"
                  << std::endl;
    }
}

void
caching_device_stats_t::print_child_stats(std::string prefix)
{
    if (num_child_hits != 0) {
        std::cerr << prefix << std::setw(18) << std::left
                  << "Child hits:" << std::setw(20) << std::right << num_child_hits
                  << std::endl;
        std::cerr << prefix << std::setw(18) << std::left
                  << "Total miss rate:" << std::setw(20) << std::fixed
                  << std::setprecision(2) << std::right
                  << ((float)num_misses * 100 / (num_hits + num_child_hits + num_misses))
                  << "%" << std::endl;
    }
}

void
caching_device_stats_t::print_stats(std::string prefix)
{
    std::cerr.imbue(std::locale("")); // Add commas, at least for my locale
    if (warmup_enabled) {
        print_warmup(prefix);
    }
    print_counts(prefix);
    print_rates(prefix);
    print_child_stats(prefix);
    std::cerr.imbue(std::locale("C")); // Reset to avoid affecting later prints.
}

void
caching_device_stats_t::reset()
{
    num_hits_at_reset = num_hits;
    num_misses_at_reset = num_misses;
    num_child_hits_at_reset = num_child_hits;
    num_hits = 0;
    num_misses = 0;
    num_child_hits = 0;
    num_inclusive_invalidates = 0;
}

void
caching_device_stats_t::invalidate()
{
    num_inclusive_invalidates++;
}
