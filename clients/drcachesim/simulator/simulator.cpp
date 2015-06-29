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
#include <iterator>
#include <string>
#include "utils.h"
#include "memref.h"
#include "ipc_reader.h"
#include "cache_stats.h"
#include "cache.h"
#include "droption.h"
#include "../common/options.h"
#include "simulator.h"

bool
simulator_t::init()
{
    // XXX: add a "required" flag to droption to avoid needing this here
    if (op_ipc_name.get_value().empty()) {
        ERROR("Usage error: ipc name is required\nUsage:\n%s",
              droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
        return false;
    }
    ipc_iter = ipc_reader_t(op_ipc_name.get_value().c_str());

    // XXX i#1703: get defaults from hardware being run on.
    num_cores = op_num_cores.get_value();
    if (!IS_POWER_OF_2(num_cores)) { // power of 2 for our mask
        ERROR("Usage error: %s must be a power of 2\n", op_num_cores.get_name().c_str());
        return false;
    }
    num_cores_mask = num_cores - 1;

    if (!llcache.init(op_LL_assoc.get_value(), op_line_size.get_value(),
                      op_LL_size.get_value(), NULL, new cache_stats_t)) {
        ERROR("Usage error: failed to initialize LL cache.  Ensure sizes and "
              "associativity are powers of 2 "
              "and that the total size is a multiple of the line size.\n");
        return false;
    }
    icaches = new cache_t[num_cores];
    dcaches = new cache_t[num_cores];
    for (int i = 0; i < num_cores; i++) {
        if (!icaches[i].init(op_L1I_assoc.get_value(), op_line_size.get_value(),
                             op_L1I_size.get_value(), &llcache, new cache_stats_t) ||
            !dcaches[i].init(op_L1D_assoc.get_value(), op_line_size.get_value(),
                             op_L1D_size.get_value(), &llcache, new cache_stats_t)) {
            ERROR("Usage error: failed to initialize L1 caches.  Ensure sizes and "
                  "associativity are powers of 2 "
                  "and that the total sizes are multiples of the line size.\n");
            return false;
        }
    }

    return true;
}

simulator_t::~simulator_t()
{
    delete llcache.get_stats();
    for (int i = 0; i < num_cores; i++) {
        delete icaches[i].get_stats();
        delete dcaches[i].get_stats();
    }
    delete [] icaches;
    delete [] dcaches;
}

bool
simulator_t::run()
{
    if (!ipc_iter.init()) {
        ERROR("failed to read from pipe %s", op_ipc_name.get_value().c_str());
        return false;
    }
    // FIXME i#1703: add options to select either ipc_reader_t or
    // a recorded trace file reader, and use a base class reader_t
    // here.
    for (; ipc_iter != ipc_end; ++ipc_iter) {
        memref_t memref = *ipc_iter;

        // We use a simple static core assignment to map threads to cores,
        // as it is not practical to measure which core each thread actually
        // ran on for each memref.
        // FIXME i#1703: use a fairer round-robin assignment.
        int core = memref.tid & num_cores_mask;

        if (memref.type == TRACE_TYPE_INSTR)
            icaches[core].request(memref);
        else if (memref.type == TRACE_TYPE_READ ||
                 memref.type == TRACE_TYPE_WRITE ||
                 // we may potentially handle prefetches differently
                 memref.type == TRACE_TYPE_PREFETCH)
            dcaches[core].request(memref);
        else if (memref.type == TRACE_TYPE_INSTR_FLUSH)
            icaches[core].flush(memref);
        else if (memref.type == TRACE_TYPE_DATA_FLUSH)
            dcaches[core].flush(memref);
        else {
            ERROR("unhandled memref type");
            return false;
        }

        if (op_verbose.get_value() > 2) {
            std::cout << "::" << memref.pid << "." << memref.tid << ":: " <<
                " @" << (void *)memref.pc <<
                ((memref.type == TRACE_TYPE_READ) ? " R " :
                 // FIXME i#1703: auto-convert to string
                 ((memref.type == TRACE_TYPE_WRITE) ? " W " : " I ")) <<
                (void *)memref.addr << " x" << memref.size << std::endl;
        }
    }
    return true;
}

bool
simulator_t::print_stats()
{
    for (int i = 0; i < num_cores; i++) {
        std::cout << "Core #" << i << " L1I stats:" << std::endl;
        icaches[i].get_stats()->print_stats();
        std::cout << "Core #" << i << " L1D stats:" << std::endl;
        dcaches[i].get_stats()->print_stats();
    }
    std::cout << "LL stats:" << std::endl;
    llcache.get_stats()->print_stats();
    return true;
}
