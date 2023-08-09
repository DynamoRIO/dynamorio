/* **********************************************************
 * Copyright (c) 2017-2023 Google, Inc.  All rights reserved.
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

#include "view.h"

#include <stdint.h>

#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "analysis_tool.h"
#include "dr_api.h"
#include "memref.h"
#include "memtrace_stream.h"
#include "raw2trace.h"
#include "raw2trace_directory.h"
#include "trace_entry.h"
#include "utils.h"

namespace dynamorio {
namespace drmemtrace {

const std::string view_t::TOOL_NAME = "View tool";

analysis_tool_t *
view_tool_create(const std::string &module_file_path, uint64_t skip_refs,
                 uint64_t sim_refs, const std::string &syntax, unsigned int verbose,
                 const std::string &alt_module_dir)
{
    return new view_t(module_file_path, skip_refs, sim_refs, syntax, verbose,
                      alt_module_dir);
}

view_t::view_t(const std::string &module_file_path, uint64_t skip_refs, uint64_t sim_refs,
               const std::string &syntax, unsigned int verbose,
               const std::string &alt_module_dir)
    : module_file_path_(module_file_path)
    , knob_verbose_(verbose)
    , trace_version_(-1)
    , knob_skip_refs_(skip_refs)
    , skip_refs_left_(knob_skip_refs_)
    , knob_sim_refs_(sim_refs)
    , sim_refs_left_(knob_sim_refs_)
    , knob_syntax_(syntax)
    , knob_alt_module_dir_(alt_module_dir)
    , num_disasm_instrs_(0)
    , prev_tid_(-1)
    , filetype_(-1)
    , timestamp_(0)
    , has_modules_(true)
{
}

std::string
view_t::initialize_stream(memtrace_stream_t *serial_stream)
{
    serial_stream_ = serial_stream;
    print_header();
    dcontext_.dcontext = dr_standalone_init();
    if (module_file_path_.empty()) {
        has_modules_ = false;
    } else {
        std::string error = directory_.initialize_module_file(module_file_path_);
        if (!error.empty())
            has_modules_ = false;
    }
    if (!has_modules_) {
        // Continue but omit disassembly to support cases where binaries are
        // not available and OFFLINE_FILE_TYPE_ENCODINGS is not present.
        return "";
    }
    // Legacy trace support where binaries are needed.
    // We do not support non-module code for such traces.
    module_mapper_ =
        module_mapper_t::create(directory_.modfile_bytes_, nullptr, nullptr, nullptr,
                                nullptr, knob_verbose_, knob_alt_module_dir_);
    module_mapper_->get_loaded_modules();
    std::string error = module_mapper_->get_last_error();
    if (!error.empty())
        return "Failed to load binaries: " + error;
    dr_disasm_flags_t flags = IF_X86_ELSE(
        DR_DISASM_ATT,
        IF_AARCH64_ELSE(DR_DISASM_DR, IF_RISCV64_ELSE(DR_DISASM_RISCV, DR_DISASM_ARM)));
    if (knob_syntax_ == "intel") {
        flags = DR_DISASM_INTEL;
    } else if (knob_syntax_ == "dr") {
        flags = DR_DISASM_DR;
    } else if (knob_syntax_ == "arm") {
        flags = DR_DISASM_ARM;
    } else if (knob_syntax_ == "riscv") {
        flags = DR_DISASM_RISCV;
    }
    disassemble_set_syntax(flags);
    return "";
}

bool
view_t::parallel_shard_supported()
{
    return false;
}

void *
view_t::parallel_shard_init_stream(int shard_index, void *worker_data,
                                   memtrace_stream_t *shard_stream)
{
    return shard_stream;
}

bool
view_t::parallel_shard_exit(void *shard_data)
{
    return true;
}

std::string
view_t::parallel_shard_error(void *shard_data)
{
    // Our parallel operation ignores all but one thread, so we need just
    // the one global error string.
    return error_string_;
}

bool
view_t::should_skip(memtrace_stream_t *memstream, const memref_t &memref)
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
        if (sim_refs_left_ == 0 && timestamp_ > 0) {
            // Print this timestamp right before the final record.
            print_prefix(memstream, memref, timestamp_record_ord_);
            std::cerr << "<marker: timestamp " << timestamp_ << ">\n";
            timestamp_ = 0;
        }
    }
    return false;
}

bool
view_t::process_memref(const memref_t &memref)
{
    return parallel_shard_memref(serial_stream_, memref);
}

bool
view_t::parallel_shard_memref(void *shard_data, const memref_t &memref)
{
    memtrace_stream_t *memstream = reinterpret_cast<memtrace_stream_t *>(shard_data);
    // Even for -skip_refs we need to process the up-front version and type.
    if (memref.marker.type == TRACE_TYPE_MARKER) {
        switch (memref.marker.marker_type) {
        case TRACE_MARKER_TYPE_VERSION:
            // We delay printing until we know the tid.
            if (trace_version_ == -1) {
                trace_version_ = static_cast<int>(memref.marker.marker_value);
            } else if (trace_version_ != static_cast<int>(memref.marker.marker_value)) {
                error_string_ = std::string("Version mismatch across files");
                return false;
            }
            version_record_ord_ = memstream->get_record_ordinal();
            return true; // Do not count toward -sim_refs yet b/c we don't have tid.
        case TRACE_MARKER_TYPE_FILETYPE:
            // We delay printing until we know the tid.
            if (filetype_ == -1) {
                filetype_ = static_cast<intptr_t>(memref.marker.marker_value);
            } else if (filetype_ != static_cast<intptr_t>(memref.marker.marker_value)) {
                error_string_ = std::string("Filetype mismatch across files");
                return false;
            }
            filetype_record_ord_ = memstream->get_record_ordinal();
            if (TESTANY(OFFLINE_FILE_TYPE_ARCH_ALL, memref.marker.marker_value) &&
                !TESTANY(build_target_arch_type(), memref.marker.marker_value)) {
                error_string_ = std::string("Architecture mismatch: trace recorded on ") +
                    trace_arch_string(static_cast<offline_file_type_t>(
                        memref.marker.marker_value)) +
                    " but tool built for " + trace_arch_string(build_target_arch_type());
                return false;
            }
            return true; // Do not count toward -sim_refs yet b/c we don't have tid.
        case TRACE_MARKER_TYPE_TIMESTAMP:
            // Delay to see whether this is a new window.  We assume a timestamp
            // is always followed by another marker (cpu or window).
            // We can't easily reorder and place window markers before timestamps
            // since memref iterators use the timestamps to order buffer units.
            timestamp_ = memref.marker.marker_value;
            timestamp_record_ord_ = memstream->get_record_ordinal();
            if (should_skip(memstream, memref))
                timestamp_ = 0;
            return true;
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
            if (!should_skip(memstream, memref)) {
                print_prefix(memstream, memref, version_record_ord_);
                std::cerr << "<marker: version " << trace_version_ << ">\n";
            }
        }
        if (filetype_ != -1) { // Handle old/malformed versions.
            if (!should_skip(memstream, memref)) {
                print_prefix(memstream, memref, filetype_record_ord_);
                std::cerr << "<marker: filetype 0x" << std::hex << filetype_ << std::dec
                          << ">\n";
            }
        }
    }

    if (should_skip(memstream, memref))
        return true;

    if (memref.marker.type == TRACE_TYPE_MARKER) {
        if (memref.marker.marker_type == TRACE_MARKER_TYPE_WINDOW_ID) {
            // Needs special handling to get the horizontal line before the timestamp.
            if (last_window_[memref.marker.tid] != memref.marker.marker_value) {
                std::cerr
                    << "------------------------------------------------------------\n";
                print_prefix(memstream, memref,
                             -1); // Already incremented for timestamp above.
            }
            if (timestamp_ > 0) {
                std::cerr << "<marker: timestamp " << timestamp_ << ">\n";
                timestamp_ = 0;
                print_prefix(memstream, memref);
            }
            std::cerr << "<marker: window " << memref.marker.marker_value << ">\n";
            last_window_[memref.marker.tid] = memref.marker.marker_value;
        }
        if (timestamp_ > 0) {
            print_prefix(memstream, memref, timestamp_record_ord_);
            std::cerr << "<marker: timestamp " << timestamp_ << ">\n";
            timestamp_ = 0;
        }
    }

    if (memref.instr.tid != 0) {
        print_prefix(memstream, memref);
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
            // Handled above.
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
        case TRACE_MARKER_TYPE_RSEQ_ENTRY:
            std::cerr << "<marker: rseq entry with end at 0x" << std::hex
                      << memref.marker.marker_value << std::dec << ">\n";
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
        case TRACE_MARKER_TYPE_PAGE_SIZE:
            std::cerr << "<marker: page size " << memref.marker.marker_value << ">\n";
            break;
        case TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT:
            std::cerr << "<marker: chunk instruction count " << memref.marker.marker_value
                      << ">\n";
            break;
        case TRACE_MARKER_TYPE_CHUNK_FOOTER:
            std::cerr << "<marker: chunk footer #" << memref.marker.marker_value << ">\n";
            break;
        case TRACE_MARKER_TYPE_FILTER_ENDPOINT:
            std::cerr << "<marker: filter endpoint>\n";
            break;
        case TRACE_MARKER_TYPE_PHYSICAL_ADDRESS:
            std::cerr << "<marker: physical address for following virtual: 0x" << std::hex
                      << memref.marker.marker_value << std::dec << ">\n";
            break;
        case TRACE_MARKER_TYPE_VIRTUAL_ADDRESS:
            std::cerr << "<marker: virtual address for prior physical: 0x" << std::hex
                      << memref.marker.marker_value << std::dec << ">\n";
            break;
        case TRACE_MARKER_TYPE_PHYSICAL_ADDRESS_NOT_AVAILABLE:
            std::cerr << "<marker: physical address not available for 0x" << std::hex
                      << memref.marker.marker_value << std::dec << ">\n";
            break;
        case TRACE_MARKER_TYPE_FUNC_ID:
            if (memref.marker.marker_value >=
                static_cast<intptr_t>(func_trace_t::TRACE_FUNC_ID_SYSCALL_BASE)) {
                std::cerr << "<marker: function==syscall #"
                          << (memref.marker.marker_value -
                              static_cast<uintptr_t>(
                                  func_trace_t::TRACE_FUNC_ID_SYSCALL_BASE))
                          << ">\n";
            } else {
                std::cerr << "<marker: function #" << memref.marker.marker_value << ">\n";
            }
            break;
        case TRACE_MARKER_TYPE_FUNC_RETADDR:
            std::cerr << "<marker: function return address 0x" << std::hex
                      << memref.marker.marker_value << std::dec << ">\n";
            break;
        case TRACE_MARKER_TYPE_FUNC_ARG:
            std::cerr << "<marker: function argument 0x" << std::hex
                      << memref.marker.marker_value << std::dec << ">\n";
            break;
        case TRACE_MARKER_TYPE_FUNC_RETVAL:
            std::cerr << "<marker: function return value 0x" << std::hex
                      << memref.marker.marker_value << std::dec << ">\n";
            break;
        case TRACE_MARKER_TYPE_RECORD_ORDINAL:
            std::cerr << "<marker: record ordinal 0x" << std::hex
                      << memref.marker.marker_value << std::dec << ">\n";
            break;
        case TRACE_MARKER_TYPE_SYSCALL:
            std::cerr << "<marker: system call " << memref.marker.marker_value << ">\n";
            break;
        case TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL:
            std::cerr << "<marker: maybe-blocking system call>\n";
            break;
        case TRACE_MARKER_TYPE_WINDOW_ID:
            // Handled above.
            break;
        case TRACE_MARKER_TYPE_SYSCALL_TRACE_START:
            std::cerr << "<marker: system call trace start>\n";
            break;
        case TRACE_MARKER_TYPE_SYSCALL_TRACE_END:
            std::cerr << "<marker: system call trace end>\n";
            break;
        case TRACE_MARKER_TYPE_BRANCH_TARGET:
            std::cerr << "<marker: indirect branch target 0x" << std::hex
                      << memref.marker.marker_value << std::dec << ">\n";
            break;
        default:
            std::cerr << "<marker: type " << memref.marker.marker_type << "; value "
                      << memref.marker.marker_value << ">\n";
            break;
        }
        return true;
    }

    static constexpr int name_width = 12;
    if (!type_is_instr(memref.instr.type) &&
        memref.data.type != TRACE_TYPE_INSTR_NO_FETCH) {
        std::string name; // Shared output for address-containing types.
        switch (memref.data.type) {
        default: std::cerr << "<entry type " << memref.data.type << ">\n"; return true;
        case TRACE_TYPE_THREAD_EXIT:
            std::cerr << "<thread " << memref.data.tid << " exited>\n";
            return true;
            // The rest are address-containing types.
        case TRACE_TYPE_READ: name = "read"; break;
        case TRACE_TYPE_WRITE: name = "write"; break;
        case TRACE_TYPE_INSTR_FLUSH: name = "iflush"; break;
        case TRACE_TYPE_DATA_FLUSH: name = "dflush"; break;
        case TRACE_TYPE_PREFETCH: name = "pref"; break;
        case TRACE_TYPE_PREFETCH_READ_L1: name = "pref-r-L1"; break;
        case TRACE_TYPE_PREFETCH_READ_L2: name = "pref-r-L2"; break;
        case TRACE_TYPE_PREFETCH_READ_L3: name = "pref-r-L3"; break;
        case TRACE_TYPE_PREFETCHNTA: name = "pref-NTA"; break;
        case TRACE_TYPE_PREFETCH_READ: name = "pref-r"; break;
        case TRACE_TYPE_PREFETCH_WRITE: name = "pref-w"; break;
        case TRACE_TYPE_PREFETCH_INSTR: name = "pref-i"; break;
        case TRACE_TYPE_PREFETCH_READ_L1_NT: name = "pref-r-L1-NT"; break;
        case TRACE_TYPE_PREFETCH_READ_L2_NT: name = "pref-r-L2-NT"; break;
        case TRACE_TYPE_PREFETCH_READ_L3_NT: name = "pref-r-L3-NT"; break;
        case TRACE_TYPE_PREFETCH_INSTR_L1: name = "pref-i-L1"; break;
        case TRACE_TYPE_PREFETCH_INSTR_L1_NT: name = "pref-i-L1-NT"; break;
        case TRACE_TYPE_PREFETCH_INSTR_L2: name = "pref-i-L2"; break;
        case TRACE_TYPE_PREFETCH_INSTR_L2_NT: name = "pref-i-L2-NT"; break;
        case TRACE_TYPE_PREFETCH_INSTR_L3: name = "pref-i-L3"; break;
        case TRACE_TYPE_PREFETCH_INSTR_L3_NT: name = "pref-i-L3-NT"; break;
        case TRACE_TYPE_PREFETCH_WRITE_L1: name = "pref-w-L1"; break;
        case TRACE_TYPE_PREFETCH_WRITE_L1_NT: name = "pref-w-L1-NT"; break;
        case TRACE_TYPE_PREFETCH_WRITE_L2: name = "pref-w-L2"; break;
        case TRACE_TYPE_PREFETCH_WRITE_L2_NT: name = "pref-w-L2-NT"; break;
        case TRACE_TYPE_PREFETCH_WRITE_L3: name = "pref-w-L3"; break;
        case TRACE_TYPE_PREFETCH_WRITE_L3_NT: name = "pref-w-L3-NT"; break;
        case TRACE_TYPE_HARDWARE_PREFETCH: name = "pref-HW"; break;
        }
        std::cerr << std::left << std::setw(name_width) << name << std::right
                  << std::setw(2) << memref.data.size << " byte(s) @ 0x" << std::hex
                  << std::setfill('0') << std::setw(sizeof(void *) * 2)
                  << memref.data.addr << " by PC 0x" << std::setw(sizeof(void *) * 2)
                  << memref.data.pc << std::dec << std::setfill(' ') << "\n";
        return true;
    }

    std::cerr << std::left << std::setw(name_width) << "ifetch" << std::right
              << std::setw(2) << memref.instr.size << " byte(s) @ 0x" << std::hex
              << std::setfill('0') << std::setw(sizeof(void *) * 2) << memref.instr.addr
              << std::dec << std::setfill(' ');
    if (!TESTANY(OFFLINE_FILE_TYPE_ENCODINGS, filetype_) && !has_modules_) {
        // We can't disassemble so we provide what info the trace itself contains.
        // XXX i#5486: We may want to store the taken target for conditional
        // branches; if added, we can print it here.
        // XXX: It may avoid initial confusion over the record-oriented output
        // to indicate whether an instruction accesses memory, but that requires
        // delayed printing.
        std::cerr << " ";
        switch (memref.instr.type) {
        case TRACE_TYPE_INSTR: std::cerr << "non-branch\n"; break;
        case TRACE_TYPE_INSTR_DIRECT_JUMP: std::cerr << "jump\n"; break;
        case TRACE_TYPE_INSTR_INDIRECT_JUMP: std::cerr << "indirect jump\n"; break;
        case TRACE_TYPE_INSTR_CONDITIONAL_JUMP: std::cerr << "conditional jump\n"; break;
        case TRACE_TYPE_INSTR_TAKEN_JUMP: std::cerr << "taken conditional jump\n"; break;
        case TRACE_TYPE_INSTR_UNTAKEN_JUMP:
            std::cerr << "untaken conditional jump\n";
            break;
        case TRACE_TYPE_INSTR_DIRECT_CALL: std::cerr << "call\n"; break;
        case TRACE_TYPE_INSTR_INDIRECT_CALL: std::cerr << "indirect call\n"; break;
        case TRACE_TYPE_INSTR_RETURN: std::cerr << "return\n"; break;
        case TRACE_TYPE_INSTR_NO_FETCH: std::cerr << "non-fetched instruction\n"; break;
        case TRACE_TYPE_INSTR_SYSENTER: std::cerr << "sysenter\n"; break;
        default: error_string_ = "Unknown instruction type\n"; return false;
        }
        ++num_disasm_instrs_;
        return true;
    }

    app_pc decode_pc;
    const app_pc orig_pc = (app_pc)memref.instr.addr;
    if (TESTANY(OFFLINE_FILE_TYPE_ENCODINGS, filetype_)) {
        // The trace has instruction encodings inside it.
        decode_pc = const_cast<app_pc>(memref.instr.encoding);
        if (memref.instr.encoding_is_new) {
            // The code may have changed: invalidate the cache.
            disasm_cache_.erase(orig_pc);
        }
    } else {
        // Legacy trace support where we need the binaries.
        decode_pc = module_mapper_->find_mapped_trace_address(orig_pc);
        if (!module_mapper_->get_last_error().empty()) {
            error_string_ = "Failed to find mapped address for " +
                to_hex_string(memref.instr.addr) + ": " +
                module_mapper_->get_last_error();
            return false;
        }
    }

    std::string disasm;
    auto cached_disasm = disasm_cache_.find(orig_pc);
    if (cached_disasm != disasm_cache_.end()) {
        disasm = cached_disasm->second;
    } else {
        // MAX_INSTR_DIS_SZ is set to 196 in core/ir/disassemble.h but is not
        // exported so we just use the same value here.
        char buf[196];
        byte *next_pc = disassemble_to_buffer(
            dcontext_.dcontext, decode_pc, orig_pc, /*show_pc=*/false,
            /*show_bytes=*/true, buf, BUFFER_SIZE_ELEMENTS(buf),
            /*printed=*/nullptr);
        if (next_pc == nullptr) {
            error_string_ = "Failed to disassemble " + to_hex_string(memref.instr.addr);
            return false;
        }
        disasm = buf;
        disasm_cache_.insert({ orig_pc, disasm });
    }
    // Add branch decoration, which varies and so can't be cached purely by PC.
    auto newline = disasm.find('\n');
    if (memref.instr.type == TRACE_TYPE_INSTR_TAKEN_JUMP)
        disasm.insert(newline, " (taken)");
    else if (memref.instr.type == TRACE_TYPE_INSTR_UNTAKEN_JUMP)
        disasm.insert(newline, " (untaken)");
    else if (trace_version_ >= TRACE_ENTRY_VERSION_BRANCH_INFO &&
             type_is_instr_branch(memref.instr.type) &&
             !type_is_instr_direct_branch(memref.instr.type)) {
        std::stringstream str;
        str << " (target 0x" << std::hex << memref.instr.indirect_branch_target << ")";
        disasm.insert(newline, str.str());
    }
    // Put our prefix on raw byte spillover, and skip the other columns.
    newline = disasm.find('\n');
    if (newline != std::string::npos && newline < disasm.size() - 1) {
        std::stringstream prefix;
        print_prefix(memstream, memref, -1, prefix);
        std::string skip_name(name_width, ' ');
        disasm.insert(newline + 1,
                      prefix.str() + skip_name + "                               ");
    }
    std::cerr << disasm;
    ++num_disasm_instrs_;
    return true;
}

bool
view_t::print_results()
{
    std::cerr << TOOL_NAME << " results:\n";
    std::cerr << std::setw(15) << num_disasm_instrs_ << " : total instructions\n";
    return true;
}

} // namespace drmemtrace
} // namespace dynamorio
