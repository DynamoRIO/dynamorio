/* **********************************************************
 * Copyright (c) 2017-2025 Google, Inc.  All rights reserved.
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
#include "decode_cache.h"
#include "dr_api.h"
#include "memref.h"
#include "memtrace_stream.h"
#include "raw2trace.h"
#include "raw2trace_shared.h"
#include "trace_entry.h"
#include "utils.h"

namespace dynamorio {
namespace drmemtrace {

const std::string view_t::TOOL_NAME = "View tool";

analysis_tool_t *
view_tool_create(const std::string &module_file_path, const std::string &syntax,
                 unsigned int verbose, const std::string &alt_module_dir)
{
    return new view_t(module_file_path, syntax, verbose, alt_module_dir);
}

view_t::view_t(const std::string &module_file_path, const std::string &syntax,
               unsigned int verbose, const std::string &alt_module_dir)
    : module_file_path_(module_file_path)
    , knob_verbose_(verbose)
    , trace_version_(-1)
    , knob_syntax_(syntax)
    , knob_alt_module_dir_(alt_module_dir)
    , num_disasm_instrs_(0)
    , prev_tid_(-1)
    , filetype_(-1)
    , timestamp_(0)
{
}

std::string
view_t::initialize_stream(memtrace_stream_t *serial_stream)
{
    serial_stream_ = serial_stream;
    print_header();
    dcontext_.dcontext = dr_standalone_init();
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
    // If the framework exited (-exit_after_records, e.g.), be sure to print
    // out a delayed timestamp.
    if (timestamp_ > 0) {
        memtrace_stream_t *memstream = reinterpret_cast<memtrace_stream_t *>(shard_data);
        print_prefix(memstream, timestamp_memref_, timestamp_record_ord_);
        std::cerr << "<marker: timestamp " << timestamp_ << ">\n";
        timestamp_ = 0;
    }
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
view_t::process_memref(const memref_t &memref)
{
    return parallel_shard_memref(serial_stream_, memref);
}

bool
view_t::init_decode_cache()
{
    assert(decode_cache_ == nullptr);
    decode_cache_ =
        std::unique_ptr<decode_cache_t<disasm_info_t>>(new decode_cache_t<disasm_info_t>(
            dcontext_.dcontext,
            // We will decode the instr_t ourselves.
            /*include_decoded_instr=*/false,
            /*persist_decoded_instrs=*/false, knob_verbose_));
    if (TESTANY(OFFLINE_FILE_TYPE_ENCODINGS, filetype_)) {
        error_string_ = decode_cache_->init(static_cast<offline_file_type_t>(filetype_));
    } else if (!module_file_path_.empty()) {
        error_string_ = decode_cache_->init(static_cast<offline_file_type_t>(filetype_),
                                            module_file_path_, knob_alt_module_dir_);
    } else {
        // Continue but omit disassembly to support cases where binaries are
        // not available and OFFLINE_FILE_TYPE_ENCODINGS is also not present.
        decode_cache_.reset(nullptr);
    }
    return error_string_.empty();
}

bool
view_t::init_from_filetype()
{
    if (init_from_filetype_done_) {
        return true;
    }
    // We will not see a TRACE_MARKER_TYPE_FILETYPE if -skip_instrs was used.
    // In that case, filetype_ will be left uninitialized and we need to
    // use the one from the stream instead.
    if (filetype_ == -1) {
        filetype_ = static_cast<offline_file_type_t>(serial_stream_->get_filetype());
    }
    if (!init_decode_cache()) {
        return false;
    }
    // We remove OFFLINE_FILE_TYPE_ARCH_REGDEPS from this check since
    // DR_ISA_REGDEPS is not a real ISA and can coexist with any real
    // architecture.
    if (TESTANY(OFFLINE_FILE_TYPE_ARCH_ALL & ~OFFLINE_FILE_TYPE_ARCH_REGDEPS,
                filetype_) &&
        !TESTANY(build_target_arch_type(), filetype_)) {
        error_string_ = std::string("Architecture mismatch: trace recorded on ") +
            trace_arch_string(static_cast<offline_file_type_t>(filetype_)) +
            " but tool built for " + trace_arch_string(build_target_arch_type());
        return false;
    }

    // Set dcontext ISA mode to DR_ISA_REGDEPS if trace file type has
    // OFFLINE_FILE_TYPE_ARCH_REGDEPS set. We need this to correctly
    // disassemble DR_ISA_REGDEPS instructions.
    if (TESTANY(OFFLINE_FILE_TYPE_ARCH_REGDEPS, filetype_)) {
        dr_set_isa_mode(dcontext_.dcontext, DR_ISA_REGDEPS, nullptr);
    }

    dr_disasm_flags_t flags = IF_X86_ELSE(
        DR_DISASM_ATT,
        IF_AARCH64_ELSE(DR_DISASM_DR, IF_RISCV64_ELSE(DR_DISASM_RISCV, DR_DISASM_ARM)));
    if (TESTANY(OFFLINE_FILE_TYPE_ARCH_REGDEPS, filetype_)) {
        // Ignore the requested syntax: we only support DR style.
        // XXX i#6942: Should we return an error if the users asks for
        // another syntax?  Should DR's libraries return an error?
        flags = DR_DISASM_DR;
    } else if (knob_syntax_ == "intel") {
        flags = DR_DISASM_INTEL;
    } else if (knob_syntax_ == "dr") {
        flags = DR_DISASM_DR;
    } else if (knob_syntax_ == "arm") {
        flags = DR_DISASM_ARM;
    } else if (knob_syntax_ == "riscv") {
        flags = DR_DISASM_RISCV;
    }
    disassemble_set_syntax(flags);

    init_from_filetype_done_ = true;
    return true;
}

bool
view_t::parallel_shard_memref(void *shard_data, const memref_t &memref)
{
    memtrace_stream_t *memstream = reinterpret_cast<memtrace_stream_t *>(shard_data);
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP) {
        // Delay to see whether this is a new window.  We assume a timestamp
        // is always followed by another marker (cpu or window).
        // We can't easily reorder and place window markers before timestamps
        // since memref iterators use the timestamps to order buffer units.
        timestamp_ = memref.marker.marker_value;
        timestamp_record_ord_ = memstream->get_record_ordinal();
        timestamp_memref_ = memref;
        return true;
    }

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
            if (trace_version_ == -1) {
                trace_version_ = static_cast<int>(memref.marker.marker_value);
            } else if (trace_version_ != static_cast<int>(memref.marker.marker_value)) {
                error_string_ = std::string("Version mismatch across files");
                return false;
            }
            std::cerr << "<marker: version " << trace_version_ << ">\n";
            break;
        case TRACE_MARKER_TYPE_FILETYPE:
            if (filetype_ == -1) {
                filetype_ = static_cast<offline_file_type_t>(memref.marker.marker_value);
                if (!init_from_filetype()) {
                    return false;
                }
            } else if (filetype_ !=
                       static_cast<offline_file_type_t>(memref.marker.marker_value)) {
                error_string_ = std::string("Filetype mismatch across files");
                return false;
            }
            std::cerr << "<marker: filetype 0x" << std::hex << filetype_ << std::dec
                      << ">\n";
            break;
        case TRACE_MARKER_TYPE_TIMESTAMP:
            // Handled above.
            break;
        case TRACE_MARKER_TYPE_CPU_ID:
            // We include the thread ID here under the assumption that we will always
            // see a cpuid marker on a thread switch.  To avoid that assumption
            // we would want to track the prior tid and print out a thread switch
            // message whenever it changes.
            if (memref.marker.marker_value == INVALID_CPU_MARKER_VALUE) {
                std::cerr << "<marker: W" << workload_from_memref_tid(memref.data.tid)
                          << ".T" << tid_from_memref_tid(memref.data.tid)
                          << " on core unknown>\n";
            } else {
                std::cerr << "<marker: W" << workload_from_memref_tid(memref.data.tid)
                          << ".T" << tid_from_memref_tid(memref.data.tid) << " on core "
                          << memref.marker.marker_value << ">\n";
            }
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
        case TRACE_MARKER_TYPE_SIGNAL_NUMBER:
            std::cerr << "<marker: signal #" << memref.marker.marker_value << ">\n";
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
        case TRACE_MARKER_TYPE_SYSCALL_FAILED:
            std::cerr << "<marker: system call failed: " << memref.marker.marker_value
                      << ">\n";
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
        case TRACE_MARKER_TYPE_DIRECT_THREAD_SWITCH:
            std::cerr << "<marker: direct switch to thread " << memref.marker.marker_value
                      << ">\n";
            break;
        case TRACE_MARKER_TYPE_SYSCALL_UNSCHEDULE:
            std::cerr << "<marker: current thread going unscheduled>\n";
            break;
        case TRACE_MARKER_TYPE_SYSCALL_SCHEDULE:
            std::cerr << "<marker: re-schedule thread " << memref.marker.marker_value
                      << ">\n";
            break;
        case TRACE_MARKER_TYPE_SYSCALL_ARG_TIMEOUT:
            std::cerr << "<marker: syscall timeout arg " << memref.marker.marker_value
                      << ">\n";
            break;
        case TRACE_MARKER_TYPE_WINDOW_ID:
            // Handled above.
            break;
        case TRACE_MARKER_TYPE_SYSCALL_TRACE_START:
            std::cerr << "<marker: trace start for system call number "
                      << memref.marker.marker_value << ">\n";
            break;
        case TRACE_MARKER_TYPE_SYSCALL_TRACE_END:
            std::cerr << "<marker: trace end for system call number "
                      << memref.marker.marker_value << ">\n";
            break;
        case TRACE_MARKER_TYPE_CONTEXT_SWITCH_START:
            std::cerr << "<marker: trace start for context switch type "
                      << memref.marker.marker_value << ">\n";
            break;
        case TRACE_MARKER_TYPE_CONTEXT_SWITCH_END:
            std::cerr << "<marker: trace end for context switch type "
                      << memref.marker.marker_value << ">\n";
            break;
        case TRACE_MARKER_TYPE_BRANCH_TARGET:
            // These are not expected to be visible (since the reader adds them
            // to memref.instr.indirect_branch_target) but we handle nonetheless.
            std::cerr << "<marker: indirect branch target 0x" << std::hex
                      << memref.marker.marker_value << std::dec << ">\n";
            break;
        case TRACE_MARKER_TYPE_CORE_WAIT:
            std::cerr << "<marker: wait for another core>\n";
            break;
        case TRACE_MARKER_TYPE_CORE_IDLE: std::cerr << "<marker: core is idle>\n"; break;
        case TRACE_MARKER_TYPE_VECTOR_LENGTH:
            std::cerr << "<marker: vector length " << memref.marker.marker_value
                      << " bytes>\n";
            break;
        case TRACE_MARKER_TYPE_UNCOMPLETED_INSTRUCTION:
            // The value stores the encoding of the uncompleted instruction up
            // to the length of a pointer. The encoding may not be complete so
            // we do not try to print it in the regular encoding style of the
            // ISA.
            std::cerr << "<marker: uncompleted instruction, encoding 0x" << std::hex
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
            std::cerr << "<thread W" << workload_from_memref_tid(memref.data.tid) << ".T"
                      << tid_from_memref_tid(memref.data.tid) << " exited>\n";
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

    // In some configurations (e.g., when using -skip_instrs), we may not see the
    // TRACE_MARKER_TYPE_FILETYPE marker at all, so we get it from the
    // memtrace_stream_t when we get to the instrs.
    if (!init_from_filetype()) {
        return false;
    }
    // In some configurations (e.g., when using -skip_instrs), we may not see the
    // TRACE_MARKER_TYPE_VERSION marker at all, so we get it from the
    // memtrace_stream_t when we get to the instrs.
    if (trace_version_ == -1) {
        trace_version_ = static_cast<int>(serial_stream_->get_version());
    }
    std::cerr << std::left << std::setw(name_width) << "ifetch" << std::right
              << std::setw(2) << memref.instr.size << " byte(s) @ 0x" << std::hex
              << std::setfill('0') << std::setw(sizeof(void *) * 2) << memref.instr.addr
              << std::dec << std::setfill(' ');
    if (decode_cache_ == nullptr) {
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

    disasm_info_t *disasm_info;
    error_string_ = decode_cache_->add_decode_info(memref.instr, disasm_info);
    if (!error_string_.empty())
        return false;
    std::string disasm = disasm_info->disasm_;

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
    if (!parallel_shard_exit(serial_stream_))
        return false;
    std::cerr << TOOL_NAME << " results:\n";
    std::cerr << std::setw(15) << num_disasm_instrs_ << " : total instructions\n";
    return true;
}

std::string
view_t::disasm_info_t::set_decode_info_derived(
    void *dcontext, const dynamorio::drmemtrace::_memref_instr_t &memref_instr,
    instr_t *instr, app_pc decode_pc)
{
    const app_pc trace_pc = reinterpret_cast<app_pc>(memref_instr.addr);
    // MAX_INSTR_DIS_SZ is set to 196 in core/ir/disassemble.h but is not
    // exported so we just use the same value here.
    char buf[196];
    byte *next_pc =
        disassemble_to_buffer(dcontext, decode_pc, trace_pc, /*show_pc=*/false,
                              /*show_bytes=*/true, buf, BUFFER_SIZE_ELEMENTS(buf),
                              /*printed=*/nullptr);
    if (next_pc == nullptr) {
        return "Failed to disassemble " + to_hex_string(memref_instr.addr);
    }
    disasm_ = buf;
    return "";
}

} // namespace drmemtrace
} // namespace dynamorio
