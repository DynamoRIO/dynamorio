/* **********************************************************
 * Copyright (c) 2017-2018 Google, Inc.  All rights reserved.
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

/* This trace analyzer requires access to the modules.log file and the
 * libraries and binary from the traced execution in order to obtain further
 * information about each instruction than was stored in the trace.
 * It does not support online use, only offline.
 */

#include "dr_api.h"
#include "opcode_mix.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <vector>
#include <string.h>

const std::string opcode_mix_t::TOOL_NAME = "Opcode mix tool";

analysis_tool_t *
opcode_mix_tool_create(const std::string &module_file_path, unsigned int verbose)
{
    return new opcode_mix_t(module_file_path, verbose);
}

opcode_mix_t::opcode_mix_t(const std::string &module_file_path, unsigned int verbose)
    : dcontext(nullptr)
    , directory(module_file_path)
    , knob_verbose(verbose)
    , instr_count(0)
{
    if (module_file_path.empty()) {
        error_string = "Module file path is missing";
        success = false;
        return;
    }
    dcontext = dr_standalone_init();
    module_mapper = module_mapper_t::create(directory.modfile_bytes, nullptr, nullptr,
                                            nullptr, nullptr, verbose);
    module_mapper->get_loaded_modules();
    std::string error = module_mapper->get_last_error();
    if (!error.empty()) {
        error_string = "Failed to load binaries: " + error;
        success = false;
        return;
    }
}

bool
opcode_mix_t::process_memref(const memref_t &memref)
{
    if (!type_is_instr(memref.instr.type) &&
        memref.data.type != TRACE_TYPE_INSTR_NO_FETCH) {
        return true;
    }
    ++instr_count;
    app_pc mapped_pc;
    mapped_pc = module_mapper->find_mapped_trace_address((app_pc)memref.instr.addr);
    if (!module_mapper->get_last_error().empty()) {
        error_string = "Failed to find mapped address for " +
            to_hex_string(memref.instr.addr) + ": " + module_mapper->get_last_error();
        return false;
    }
    int opcode;
    auto cached_opcode = opcode_cache.find(mapped_pc);
    if (cached_opcode != opcode_cache.end()) {
        opcode = cached_opcode->second;
    } else {
        instr_t instr;
        instr_init(dcontext, &instr);
        app_pc next_pc = decode(dcontext, mapped_pc, &instr);
        if (next_pc == NULL || !instr_valid(&instr)) {
            error_string =
                "Failed to decode instruction " + to_hex_string(memref.instr.addr);
            return false;
        }
        opcode = instr_get_opcode(&instr);
        opcode_cache[mapped_pc] = opcode;
        instr_free(dcontext, &instr);
    }
    ++opcode_counts[opcode];
    return true;
}

static bool
cmp_val(const std::pair<int, int_least64_t> &l, const std::pair<int, int_least64_t> &r)
{
    return (l.second > r.second);
}

bool
opcode_mix_t::print_results()
{
    std::cerr << TOOL_NAME << " results:\n";
    std::cerr << std::setw(15) << instr_count << " : total executed instructions\n";
    std::vector<std::pair<int, int_least64_t>> sorted(opcode_counts.begin(),
                                                      opcode_counts.end());
    std::sort(sorted.begin(), sorted.end(), cmp_val);
    for (const auto &keyvals : sorted) {
        std::cerr << std::setw(15) << keyvals.second << " : " << std::setw(9)
                  << decode_opcode_name(keyvals.first) << "\n";
    }
    return true;
}
