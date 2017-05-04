/* **********************************************************
 * Copyright (c) 2017 Google, Inc.  All rights reserved.
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

#include <iomanip>
#include <iostream>

#include "reuse_time.h"
#include "../common/utils.h"

#ifdef DEBUG
# define DEBUG_VERBOSE(level) (knob_verbose >= (level))
#else
# define DEBUG_VERBOSE(level) (false)
#endif

const std::string reuse_time_t::TOOL_NAME = "Reuse time tool";

analysis_tool_t *
reuse_time_tool_create(unsigned int line_size,
                       unsigned int verbose)
{
    return new reuse_time_t(line_size, verbose);
}

reuse_time_t::reuse_time_t(unsigned int line_size, unsigned int verbose) :
    time_stamp(0), knob_verbose(verbose), knob_line_size(line_size)
{
    line_size_bits = compute_log2((int)knob_line_size);
}

reuse_time_t::~reuse_time_t()
{
    // Empty.
}

bool
reuse_time_t::process_memref(const memref_t &memref)
{
    if (DEBUG_VERBOSE(3)) {
        std::cerr << " ::" << memref.data.pid << "." << memref.data.tid
                  << ":: " << trace_type_names[memref.data.type];
        if (memref.data.type != TRACE_TYPE_THREAD_EXIT) {
            std::cerr << " @ ";
            if (!type_is_instr(memref.data.type))
                std::cerr << (void *)memref.data.pc << " ";
            std::cerr << (void *)memref.data.addr << " x" << memref.data.size;
        }
        std::cerr << std::endl;
    }

    // Only care about data for now.
    if (type_is_instr(memref.instr.type)) {
        return true;
    }

    // Ignore thread events and other tracing metadata.
    if (memref.data.type != TRACE_TYPE_READ &&
        memref.data.type != TRACE_TYPE_WRITE &&
        !type_is_prefetch(memref.data.type)) {
        return true;
    }

    time_stamp++;
    addr_t line = memref.data.addr >> line_size_bits;
    if (time_map.count(line) > 0) {
        int_least64_t reuse_time = time_stamp - time_map[line];
        if (DEBUG_VERBOSE(3)) {
            std::cerr << "Reuse " << reuse_time << std::endl;
        }
        reuse_time_histogram[reuse_time]++;
    }
    time_map[line] = time_stamp;
    return true;
}

bool
reuse_time_t::print_results() {
    std::cerr << TOOL_NAME << " results:\n";
    std::cerr << "Total accesses: " << time_stamp << "\n";
    std::cerr.precision(2);
    std::cerr.setf(std::ios::fixed);

    int_least64_t count = 0;
    int_least64_t sum = 0;
    for (std::map<int_least64_t, int_least64_t>::iterator it =
         reuse_time_histogram.begin(); it != reuse_time_histogram.end(); it++) {
        count += it->second;
        sum += it->first * it->second;
    }
    std::cerr << "Mean reuse time: " << sum / static_cast<double>(count) << "\n";

    std::cerr << "Reuse time histogram:\n";
    std::cerr << std::setw(8) << "Distance"
              << std::setw(12) << "Count"
              << std::setw(9) << "Percent"
              << std::setw(12) << "Cumulative";
    std::cerr << std::endl;
    double cum_percent = 0.0;
    for (std::map<int_least64_t, int_least64_t>::iterator it =
         reuse_time_histogram.begin(); it != reuse_time_histogram.end(); it++) {
        double percent = it->second / static_cast<double>(count);
        cum_percent += percent;
        std::cerr << std::setw(8) << it->first
                  << std::setw(12) << it->second
                  << std::setw(8) << percent * 100.0 << "%"
                  << std::setw(11) << cum_percent * 100.0 << "%";
        std::cerr << std::endl;
    }
    return true;
}
