/* **********************************************************
 * Copyright (c) 2017-2021 Google, Inc.  All rights reserved.
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
view_tool_create(const std::string &module_file_path, memref_tid_t thread,
                 uint64_t skip_refs, uint64_t sim_refs, const std::string &syntax,
                 unsigned int verbose, const std::string &alt_module_dir)
{
    return new view_t(module_file_path, thread, skip_refs, sim_refs, syntax, verbose,
                      alt_module_dir);
}

view_t::view_t(const std::string &module_file_path, memref_tid_t thread,
               uint64_t skip_refs, uint64_t sim_refs, const std::string &syntax,
               unsigned int verbose, const std::string &alt_module_dir)
    : module_file_path_(module_file_path)
    , knob_verbose_(verbose)
    , trace_version_(-1)
    , knob_thread_(thread)
    , knob_skip_refs_(skip_refs)
    , skip_refs_left_(knob_skip_refs_)
    , knob_sim_refs_(sim_refs)
    , sim_refs_left_(knob_sim_refs_)
    , knob_syntax_(syntax)
    , knob_alt_module_dir_(alt_module_dir)
    , num_disasm_instrs_(0)
    , prev_tid_(-1)
    , filetype_(-1)
{
}

std::string
view_t::initialize()
{
    if (module_file_path_.empty())
        return "Module file path is missing";
    dcontext_.dcontext = dr_standalone_init();
    std::string error = directory_.initialize_module_file(module_file_path_);
    if (!error.empty())
        return "Failed to initialize directory: " + error;
    module_mapper_ =
        module_mapper_t::create(directory_.modfile_bytes_, nullptr, nullptr, nullptr,
                                nullptr, knob_verbose_, knob_alt_module_dir_);
    module_mapper_->get_loaded_modules();
    error = module_mapper_->get_last_error();
    if (!error.empty())
        return "Failed to load binaries: " + error;
    dr_disasm_flags_t flags =
        IF_X86_ELSE(DR_DISASM_ATT, IF_AARCH64_ELSE(DR_DISASM_DR, DR_DISASM_ARM));
    if (knob_syntax_ == "intel") {
        flags = DR_DISASM_INTEL;
    } else if (knob_syntax_ == "dr") {
        flags = DR_DISASM_DR;
    } else if (knob_syntax_ == "arm") {
        flags = DR_DISASM_ARM;
    }
    disassemble_set_syntax(flags);
    return "";
}

bool
view_t::parallel_shard_supported()
{
    // When just one thread is selected, we support parallel operation to reduce
    // overhead from reading all the other thread files in series.
    return knob_thread_ > 0;
}

void *
view_t::parallel_shard_init(int shard_index, void *worker_data)
{
    return nullptr;
}

bool
view_t::parallel_shard_exit(void *shard_data)
{
    return true;
}

std::string
view_t::parallel_shard_error(void *shard_data)
{
    return "";
}

bool
view_t::parallel_shard_memref(void *shard_data, const memref_t &memref)
{
    return process_memref(memref);
}

bool
view_t::should_skip()
{
    if (skip_refs_left_ > 0) {
        skip_refs_left_--;
        // I considered printing the version and filetype even when skipped but
        // it adds more confusion from the memref counting than it removes.
        // A user can do two views, one without a skip, to see the headers.
        return true;
    }
    if (knob_sim_refs_ > 0) {
        if (sim_refs_left_ == 0)
            return true;
        sim_refs_left_--;
    }
    return false;
}

bool
view_t::process_memref(const memref_t &memref)
{
    if (knob_thread_ > 0 && memref.data.tid > 0 && memref.data.tid != knob_thread_)
        return true;
    // Even for -skip_refs we need to process the up-front version and type.
    if (memref.marker.type == TRACE_TYPE_MARKER) {
        switch (memref.marker.marker_type) {
        case TRACE_MARKER_TYPE_VERSION:
            // We delay printing until we know the tid.
            if (trace_version_ == -1)
                trace_version_ = static_cast<int>(memref.marker.marker_value);
            else if (trace_version_ != static_cast<int>(memref.marker.marker_value)) {
                error_string_ = std::string("Version mismatch across files");
                return false;
            }
            return true; // Do not count toward -sim_refs yet b/c we don't have tid.
        case TRACE_MARKER_TYPE_FILETYPE:
            // We delay printing until we know the tid.
            if (filetype_ == -1)
                filetype_ = static_cast<intptr_t>(memref.marker.marker_value);
            else if (filetype_ != static_cast<intptr_t>(memref.marker.marker_value)) {
                error_string_ = std::string("Filetype mismatch across files");
                return false;
            }
            if (TESTANY(OFFLINE_FILE_TYPE_ARCH_ALL, memref.marker.marker_value) &&
                !TESTANY(build_target_arch_type(), memref.marker.marker_value)) {
                error_string_ = std::string("Architecture mismatch: trace recorded on ") +
                    trace_arch_string(static_cast<offline_file_type_t>(
                        memref.marker.marker_value)) +
                    " but tool built for " + trace_arch_string(build_target_arch_type());
                return false;
            }
            return true; // Do not count toward -sim_refs yet b/c we don't have tid.
        default: break;
        }
    }

    // We delay the initial markers until we know the tid.
    // There are always at least 2 markers (timestamp+cpu) immediately after the
    // first two, and on newer versions there is a 3rd (line size).
    if (memref.marker.type == TRACE_TYPE_MARKER && memref.marker.tid != 0 &&
        printed_header_.find(memref.marker.tid) == printed_header_.end()) {
        printed_header_.insert(memref.marker.tid);
        if (trace_version_ != -1) { // Old versions may not have a version marker.
            if (!should_skip()) {
                std::cerr << "T" << memref.marker.tid << " "
                          << "<marker: version " << trace_version_ << ">\n";
            }
        }
        if (filetype_ != -1) { // Handle old/malformed versions.
            if (!should_skip()) {
                std::cerr << "T" << memref.marker.tid << " "
                          << "<marker: filetype 0x" << std::hex << filetype_ << std::dec
                          << ">\n";
            }
        }
    }

    if (should_skip())
        return true;

    if (memref.instr.tid != 0) {
        if (prev_tid_ != -1 && prev_tid_ != memref.instr.tid)
            std::cerr << "------------------------------------------------------------\n";
        prev_tid_ = memref.instr.tid;
        std::cerr << "T" << memref.instr.tid << " ";
    }

    if (memref.marker.type == TRACE_TYPE_MARKER) {
        switch (memref.marker.marker_type) {
        case TRACE_MARKER_TYPE_VERSION:
            // Handled above.
            break;
        case TRACE_MARKER_TYPE_FILETYPE:
            // Handled above.
            break;
        case TRACE_MARKER_TYPE_TIMESTAMP:
            std::cerr << "<marker: timestamp " << memref.marker.marker_value << ">\n";
            break;
        case TRACE_MARKER_TYPE_CPU_ID:
            // We include the thread ID here under the assumption that we will always
            // see a cpuid marker on a thread switch.  To avoid that assumption
            // we would want to track the prior tid and print out a thread switch
            // message whenever it changes.
            std::cerr << "<marker: tid " << memref.marker.tid << " on core "
                      << memref.marker.marker_value << ">\n";
            break;
        case TRACE_MARKER_TYPE_KERNEL_EVENT:
            if (trace_version_ <= TRACE_ENTRY_VERSION_NO_KERNEL_PC) {
                // Legacy traces just have the module offset.
                std::cerr << "<marker: kernel xfer from module offset +0x" << std::hex
                          << memref.marker.marker_value << std::dec << " to handler>\n";
            } else {
                std::cerr << "<marker: kernel xfer from 0x" << std::hex
                          << memref.marker.marker_value << std::dec << " to handler>\n";
            }
            break;
        case TRACE_MARKER_TYPE_RSEQ_ABORT:
            std::cerr << "<marker: rseq abort from 0x" << std::hex
                      << memref.marker.marker_value << std::dec << " to handler>\n";
            break;
        case TRACE_MARKER_TYPE_KERNEL_XFER:
            if (trace_version_ <= TRACE_ENTRY_VERSION_NO_KERNEL_PC) {
                // Legacy traces just have the module offset.
                std::cerr << "<marker: syscall xfer from module offset +0x" << std::hex
                          << memref.marker.marker_value << std::dec << ">\n";
            } else {
                std::cerr << "<marker: syscall xfer from 0x" << std::hex
                          << memref.marker.marker_value << std::dec << ">\n";
            }
            break;
        case TRACE_MARKER_TYPE_INSTRUCTION_COUNT:
            std::cerr << "<marker: instruction count " << memref.marker.marker_value
                      << ">\n";
            break;
        case TRACE_MARKER_TYPE_CACHE_LINE_SIZE:
            std::cerr << "<marker: cache line size " << memref.marker.marker_value
                      << ">\n";
            break;
        default:
            std::cerr << "<marker: type " << memref.marker.marker_type << "; value "
                      << memref.marker.marker_value << ">\n";
            break;
        }
        return true;
    }

    if (!type_is_instr(memref.instr.type) &&
        memref.data.type != TRACE_TYPE_INSTR_NO_FETCH) {
        switch (memref.data.type) {
        case TRACE_TYPE_READ:
            std::cerr << "    read  " << memref.data.size << " byte(s) @ 0x" << std::hex
                      << memref.data.addr << std::dec << "\n";
            break;
        case TRACE_TYPE_WRITE:
            std::cerr << "    write " << memref.data.size << " byte(s) @ 0x" << std::hex
                      << memref.data.addr << std::dec << "\n";
            break;
        case TRACE_TYPE_THREAD_EXIT:
            std::cerr << "<thread " << memref.data.tid << " exited>\n";
            break;
        default: std::cerr << "<entry type " << memref.data.type << ">\n"; break;
        }
        return true;
    }

    app_pc mapped_pc;
    app_pc orig_pc = (app_pc)memref.instr.addr;
    mapped_pc = module_mapper_->find_mapped_trace_address(orig_pc);
    if (!module_mapper_->get_last_error().empty()) {
        error_string_ = "Failed to find mapped address for " +
            to_hex_string(memref.instr.addr) + ": " + module_mapper_->get_last_error();
        return false;
    }

    std::string disasm;
    auto cached_disasm = disasm_cache_.find(mapped_pc);
    if (cached_disasm != disasm_cache_.end()) {
        disasm = cached_disasm->second;
    } else {
        // MAX_INSTR_DIS_SZ is set to 196 in core/ir/disassemble.h but is not
        // exported so we just use the same value here.
        char buf[196];
        byte *next_pc = disassemble_to_buffer(
            dcontext_.dcontext, mapped_pc, orig_pc, /*show_pc=*/true,
            /*show_bytes=*/true, buf, BUFFER_SIZE_ELEMENTS(buf),
            /*printed=*/nullptr);
        if (next_pc == nullptr) {
            error_string_ = "Failed to disassemble " + to_hex_string(memref.instr.addr);
            return false;
        }
        disasm = buf;
        disasm_cache_.insert({ mapped_pc, disasm });
    }
    // Put our prefix on raw byte spillover.
    auto newline = disasm.find('\n');
    if (newline != std::string::npos && newline < disasm.size() - 1) {
        std::stringstream prefix;
        prefix << "T" << memref.instr.tid << " ";
        disasm.insert(newline + 1, prefix.str());
    }
    std::cerr << disasm;
    ++num_disasm_instrs_;
    return true;
}

bool
view_t::print_results()
{
    std::cerr << TOOL_NAME << " results:\n";
    std::cerr << std::setw(15) << num_disasm_instrs_
              << " : total disassembled instructions\n";
    return true;
}
