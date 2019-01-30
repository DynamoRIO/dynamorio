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
#include "view.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <vector>

const std::string view_t::TOOL_NAME = "View tool";

analysis_tool_t *
view_tool_create(const std::string &module_file_path, uint64_t skip_refs,
                 uint64_t sim_refs, const std::string &syntax, unsigned int verbose)
{
    return new view_t(module_file_path, skip_refs, sim_refs, syntax, verbose);
}

view_t::view_t(const std::string &module_file_path_in, uint64_t skip_refs,
               uint64_t sim_refs, const std::string &syntax, unsigned int verbose)
    : dcontext(nullptr)
    , module_file_path(module_file_path_in)
    , knob_verbose(verbose)
    , instr_count(0)
    , knob_skip_refs(skip_refs)
    , knob_sim_refs(sim_refs)
    , knob_syntax(syntax)
    , num_disasm_instrs(0)
{
}

std::string
view_t::initialize()
{
    if (module_file_path.empty())
        return "Module file path is missing";
    dcontext = dr_standalone_init();
    std::string error = directory.initialize_module_file(module_file_path);
    if (!error.empty())
        return "Failed to initialize directory: " + error;
    module_mapper = module_mapper_t::create(directory.modfile_bytes, nullptr, nullptr,
                                            nullptr, nullptr, knob_verbose);
    module_mapper->get_loaded_modules();
    error = module_mapper->get_last_error();
    if (!error.empty())
        return "Failed to load binaries: " + error;
    dr_disasm_flags_t flags = DR_DISASM_ATT;
    if (knob_syntax == "intel") {
        flags = DR_DISASM_INTEL;
    } else if (knob_syntax == "dr") {
        flags = DR_DISASM_DR;
    } else if (knob_syntax == "arm") {
        flags = DR_DISASM_ARM;
    }
    disassemble_set_syntax(flags);
    return "";
}

bool
view_t::process_memref(const memref_t &memref)
{
    if (!type_is_instr(memref.instr.type) &&
        memref.data.type != TRACE_TYPE_INSTR_NO_FETCH)
        return true;

    if (instr_count < knob_skip_refs || instr_count >= (knob_skip_refs + knob_sim_refs)) {
        ++instr_count;
        return true;
    }

    ++instr_count;

    app_pc mapped_pc;
    app_pc orig_pc = (app_pc)memref.instr.addr;
    mapped_pc = module_mapper->find_mapped_trace_address(orig_pc);
    if (!module_mapper->get_last_error().empty()) {
        error_string = "Failed to find mapped address for " +
            to_hex_string(memref.instr.addr) + ": " + module_mapper->get_last_error();
        return false;
    }

    std::string disasm;
    auto cached_disasm = disasm_cache.find(mapped_pc);
    if (cached_disasm != disasm_cache.end()) {
        disasm = cached_disasm->second;
    } else {
        // MAX_INSTR_DIS_SZ is set to 196 in core/arch/disassemble.h but is not
        // exported so we just use the same value here.
        char buf[196];
        byte *next_pc =
            disassemble_to_buffer(dcontext, mapped_pc, orig_pc, /*show_pc=*/true,
                                  /*show_bytes=*/true, buf, BUFFER_SIZE_ELEMENTS(buf),
                                  /*printed=*/nullptr);
        if (next_pc == nullptr) {
            error_string = "Failed to disassemble " + to_hex_string(memref.instr.addr);
            return false;
        }
        disasm = buf;
        disasm_cache.insert({ mapped_pc, disasm });
    }
    // XXX: For now we print the disassembly of instructions only. We should extend
    // this tool to annotate load/store operations with the entries recorded in
    // the offline trace.
    std::cerr << disasm;
    ++num_disasm_instrs;
    return true;
}

bool
view_t::print_results()
{
    std::cerr << TOOL_NAME << " results:\n";
    std::cerr << std::setw(15) << num_disasm_instrs
              << " : total disassembled instructions\n";
    return true;
}
