/* **********************************************************
 * Copyright (c) 2018-2023 Google, Inc.  All rights reserved.
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

#ifndef _OPCODE_MIX_H_
#define _OPCODE_MIX_H_ 1

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "dr_api.h" // Must be before trace_entry.h from analysis_tool.h.
#include "analysis_tool.h"
#include "memref.h"
#include "raw2trace.h"
#include "raw2trace_directory.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

class opcode_mix_t : public analysis_tool_t {
public:
    // The module_file_path is optional and unused for traces with
    // OFFLINE_FILE_TYPE_ENCODINGS.
    // XXX: Once we update our toolchains to guarantee C++17 support we could use
    // std::optional here.
    opcode_mix_t(const std::string &module_file_path, unsigned int verbose,
                 const std::string &alt_module_dir = "");
    virtual ~opcode_mix_t();
    std::string
    initialize() override;
    bool
    process_memref(const memref_t &memref) override;
    bool
    print_results() override;
    bool
    parallel_shard_supported() override;
    void *
    parallel_worker_init(int worker_index) override;
    std::string
    parallel_worker_exit(void *worker_data) override;
    void *
    parallel_shard_init(int shard_index, void *worker_data) override;
    bool
    parallel_shard_exit(void *shard_data) override;
    bool
    parallel_shard_memref(void *shard_data, const memref_t &memref) override;
    std::string
    parallel_shard_error(void *shard_data) override;

protected:
    struct worker_data_t {
        std::unordered_map<app_pc, int> opcode_cache;
    };

    struct shard_data_t {
        shard_data_t()
            : worker(nullptr)
            , instr_count(0)
        {
        }
        shard_data_t(worker_data_t *worker)
            : worker(worker)
            , instr_count(0)
            , last_trace_module_start(nullptr)
            , last_trace_module_size(0)
            , last_mapped_module_start(nullptr)
        {
        }
        worker_data_t *worker;
        int64_t instr_count;
        std::unordered_map<int, int64_t> opcode_counts;
        std::string error;
        app_pc last_trace_module_start;
        size_t last_trace_module_size;
        app_pc last_mapped_module_start;
        offline_file_type_t filetype = OFFLINE_FILE_TYPE_DEFAULT;
    };

    struct dcontext_cleanup_last_t {
    public:
        ~dcontext_cleanup_last_t()
        {
            if (dcontext != nullptr)
                dr_standalone_exit();
        }
        void *dcontext = nullptr;
    };

    /* We make this the first field so that dr_standalone_exit() is called after
     * destroying the other fields which may use DR heap.
     */
    dcontext_cleanup_last_t dcontext_;

    // These are all optional and unused for OFFLINE_FILE_TYPE_ENCODINGS.
    // XXX: Once we update our toolchains to guarantee C++17 support we could use
    // std::optional here.
    std::string module_file_path_;
    std::unique_ptr<module_mapper_t> module_mapper_;
    std::mutex mapper_mutex_;
    // We reference directory.modfile_bytes throughout operation, so its lifetime
    // must match ours.
    raw2trace_directory_t directory_;

    std::unordered_map<memref_tid_t, shard_data_t *> shard_map_;
    // This mutex is only needed in parallel_shard_init.  In all other accesses to
    // shard_map (process_memref, print_results) we are single-threaded.
    std::mutex shard_map_mutex_;
    unsigned int knob_verbose_;
    std::string knob_alt_module_dir_;
    static const std::string TOOL_NAME;
    // For serial operation.
    worker_data_t serial_worker_;
    shard_data_t serial_shard_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _OPCODE_MIX_H_ */
