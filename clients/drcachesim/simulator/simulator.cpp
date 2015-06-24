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

    // FIXME i#1703: take params from args
    // FIXME i#1703: build a separate L1D per specified core, and use a
    // static assignment of app threads to cores.
    if (!cache_L2.init(8, 64, 8192/*512KB cache*/, NULL, &stats_L2) ||
        !cache_L1I.init(4, 64, 512/*32KB cache*/, &cache_L2, &stats_L1I) ||
        !cache_L1D.init(4, 64, 512/*32KB cache*/, &cache_L2, &stats_L1D)) {
        ERROR("failed to initialize caches");
        return false;
    }
    return true;
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
        // FIXME i#1703: pass to caches per static core assignment.

        if (memref.type == TRACE_TYPE_INSTR)
            cache_L1I.request(memref);
        else
            cache_L1D.request(memref);

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
    std::cout << "L1I stats:" << std::endl;
    stats_L1I.print_stats();
    std::cout << "L1D stats:" << std::endl;
    stats_L1D.print_stats();
    std::cout << "L2 stats:" << std::endl;
    stats_L2.print_stats();
    return true;
}
