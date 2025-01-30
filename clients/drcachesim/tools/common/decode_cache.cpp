/* **********************************************************
 * Copyright (c) 2025 Google, Inc.  All rights reserved.
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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>

// Needs to be included before trace_entry.h or build_target_arch_type will not
// be defined by trace_entry.h.
#include "dr_api.h"

#include "decode_cache.h"
#include "memref.h"
#include "raw2trace_shared.h"
#include "trace_entry.h"
#include "utils.h"

namespace dynamorio {
namespace drmemtrace {

void
decode_info_base_t::set_decode_info(
    void *dcontext, const dynamorio::drmemtrace::_memref_instr_t &memref_instr,
    instr_t *instr, app_pc decode_pc)
{
    error_string_ = set_decode_info_derived(dcontext, memref_instr, instr, decode_pc);
    is_valid_ = error_string_.empty();
}

bool
decode_info_base_t::is_valid() const
{
    return is_valid_;
}

std::string
decode_info_base_t::get_error_string() const
{
    return error_string_;
}

instr_decode_info_t::~instr_decode_info_t()
{
    if (instr_ != nullptr) {
        instr_destroy(dcontext_, instr_);
    }
}

decode_cache_base_t::decode_cache_base_t(unsigned int verbosity)
    : verbosity_(verbosity)
{
}

decode_cache_base_t::~decode_cache_base_t()
{
    std::lock_guard<std::mutex> guard(module_mapper_mutex_);
    if (use_module_mapper_) {
        --module_mapper_use_count_;
        if (module_mapper_use_count_ == 0) {
            // We cannot wait for the static module_mapper_ to be destroyed
            // automatically because we want to do it before DR's global resource
            // accounting is done at the end.
            module_mapper_.reset(nullptr);
            module_file_path_used_for_init_ = "";
            delete[] modfile_bytes_;
            modfile_bytes_ = nullptr;
        }
    }
}

std::string
decode_cache_base_t::init_module_mapper(const std::string &module_file_path,
                                        const std::string &alt_module_dir)
{
    std::lock_guard<std::mutex> guard(module_mapper_mutex_);
    use_module_mapper_ = true;
    ++module_mapper_use_count_;
    if (module_mapper_ != nullptr) {
        if (module_file_path_used_for_init_ != module_file_path) {
            return "Prior module_file_path (" + module_file_path_used_for_init_ +
                ") does not match provided (" + module_file_path + ")";
        }
        // We want only a single module_mapper_t instance to be
        // initialized that is shared among all instances of
        // decode_cache_base_t.
        return "";
    }
    std::string err = make_module_mapper(module_file_path, alt_module_dir);
    if (!err.empty())
        return "Failed to make module mapper: " + err;
    module_file_path_used_for_init_ = module_file_path;
    module_mapper_->get_loaded_modules();
    err = module_mapper_->get_last_error();
    if (!err.empty())
        return "Failed to load binaries: " + err;
    return "";
}

std::string
decode_cache_base_t::make_module_mapper(const std::string &module_file_path,
                                        const std::string &alt_module_dir)
{
    // Legacy trace support where binaries are needed.
    // We do not support non-module code for such traces.
    file_t modfile;
    std::string err = read_module_file(module_file_path, modfile, modfile_bytes_);
    if (!err.empty()) {
        return "Failed to read module file: " + err;
    }
    dr_close_file(modfile);
    module_mapper_ = module_mapper_t::create(modfile_bytes_, nullptr, nullptr, nullptr,
                                             nullptr, verbosity_, alt_module_dir);
    return "";
}

std::string
decode_cache_base_t::find_mapped_trace_address(app_pc trace_pc, app_pc &decode_pc)
{
    if (trace_pc >= last_trace_module_start_ &&
        static_cast<size_t>(trace_pc - last_trace_module_start_) <
            last_mapped_module_size_) {
        decode_pc = last_mapped_module_start_ + (trace_pc - last_trace_module_start_);
        return "";
    }
    std::lock_guard<std::mutex> guard(module_mapper_mutex_);
    decode_pc = module_mapper_->find_mapped_trace_bounds(
        trace_pc, &last_mapped_module_start_, &last_mapped_module_size_);
    if (!module_mapper_->get_last_error().empty()) {
        last_mapped_module_start_ = nullptr;
        last_mapped_module_size_ = 0;
        last_trace_module_start_ = nullptr;
        return "Failed to find mapped address for " +
            to_hex_string(reinterpret_cast<uint64_t>(trace_pc)) + ": " +
            module_mapper_->get_last_error();
    }
    last_trace_module_start_ = trace_pc - (decode_pc - last_mapped_module_start_);
    return "";
}

std::string
instr_decode_info_t::set_decode_info_derived(
    void *dcontext, const dynamorio::drmemtrace::_memref_instr_t &memref_instr,
    instr_t *instr, app_pc decode_pc)
{
    dcontext_ = dcontext;
    instr_ = instr;
    return "";
}

instr_t *
instr_decode_info_t::get_decoded_instr()
{
    return instr_;
}

offline_file_type_t
decode_cache_base_t::build_arch_file_type()
{
    // i#7236: build_target_arch_type() is defined in trace_entry.h, but it is built
    // conditionally only the IF_X64_ELSE symbol that is defined by dr_api.h
    // is already defined. It is hard to control the order in which headers
    // are included, so to make it easier we provide this build_arch_file_type()
    // implementation in this separate source file which is part of the
    // drmemtrace_decode_cache build library unit.
    //
    // Also, this had to be an API on decode_cache_base_t instead of
    // decode_cache_t because since decode_cache_t is a template class its
    // implementation must be in the header.
    return static_cast<offline_file_type_t>(build_target_arch_type());
}

// Must be in cpp and not the header, else the linker will complain about multiple
// definitions.
std::mutex decode_cache_base_t::module_mapper_mutex_;
std::unique_ptr<module_mapper_t> decode_cache_base_t::module_mapper_;
std::string decode_cache_base_t::module_file_path_used_for_init_;
char *decode_cache_base_t::modfile_bytes_ = nullptr;
int decode_cache_base_t::module_mapper_use_count_ = 0;

} // namespace drmemtrace
} // namespace dynamorio
