/* **********************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
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

#include <iostream>
#include <iomanip>
#include "cache_stats.h"

cache_stats_t::cache_stats_t() :
    num_hits(0), num_misses(0), num_flushes(0)
{
}

cache_stats_t::~cache_stats_t()
{
}

void
cache_stats_t::access(const memref_t &memref, bool hit)
{
    // We assume we're single-threaded.
    // We're only computing miss rate so we just inc counters here.
    if (hit)
        num_hits++;
    else
        num_misses++;
}

void
cache_stats_t::flush(const memref_t &memref)
{
    num_flushes++;
}

void
cache_stats_t::print_stats(std::string prefix)
{
    std::cout.imbue(std::locale("")); // Add commas, at least for my locale
    std::cout << prefix << "Hits:      " << std::setw(20) << std::right <<
        num_hits << std::endl;
    std::cout << prefix << "Misses:    " << std::setw(20) << std::right <<
        num_misses << std::endl;
    if (num_flushes != 0) {
        std::cout << prefix << "Flushes:   " << std::setw(20) << std::right <<
            num_flushes << std::endl;
    }
    std::cout << prefix << "Miss rate: " << std::setw(20) << std::fixed <<
        std::setprecision(2) <<
        ((float)num_misses*100/(num_hits+num_misses)) << "%" << std::endl;
}
