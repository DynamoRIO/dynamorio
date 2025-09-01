/* **********************************************************
 * Copyright (c) 2018-2025 Google, Inc.  All rights reserved.
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

#ifndef _VIEW_H_
#define _VIEW_H_ 1

#include <stdint.h>

#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "dr_api.h" // Must be before trace_entry.h from analysis_tool.h.
#include "analysis_tool.h"
#include "decode_cache.h"
#include "memref.h"
#include "memtrace_stream.h"
#include "raw2trace.h"

namespace dynamorio {
namespace drmemtrace {

class view_t : public analysis_tool_t {
public:
    // The module_file_path is optional and unused for traces with
    // OFFLINE_FILE_TYPE_ENCODINGS.
    // XXX: Once we update our toolchains to guarantee C++17 support we could use
    // std::optional here.
    view_t(const std::string &module_file_path, const std::string &syntax,
           unsigned int verbose, const std::string &alt_module_dir = "");
    virtual ~view_t() = default;
    std::string
    initialize_stream(memtrace_stream_t *serial_stream) override;
    bool
    parallel_shard_supported() override;
    void *
    parallel_shard_init_stream(int shard_index, void *worker_data,
                               memtrace_stream_t *shard_stream) override;
    bool
    parallel_shard_exit(void *shard_data) override;
    bool
    parallel_shard_memref(void *shard_data, const memref_t &memref) override;
    std::string
    parallel_shard_error(void *shard_data) override;
    bool
    process_memref(const memref_t &memref) override;
    bool
    print_results() override;

    static constexpr int
    tid_column_width()
    {
        return TID_COLUMN_WIDTH;
    }

protected:
    struct dcontext_cleanup_last_t {
    public:
        ~dcontext_cleanup_last_t()
        {
            if (dcontext != nullptr)
                dr_standalone_exit();
        }
        void *dcontext = nullptr;
    };
    class disasm_info_t : public decode_info_base_t {
    public:
        std::string disasm_;

    private:
        std::string
        set_decode_info_derived(
            void *dcontext, const dynamorio::drmemtrace::_memref_instr_t &memref_instr,
            instr_t *instr, app_pc decode_pc) override;
    };

    // Creates and initializes a decode_cache_t instance to decode instructions in the
    // trace. Made virtual to allow subclasses to customize.
    virtual bool
    init_decode_cache();

    // Initializes various other state based on the filetype. If the
    // TRACE_MARKER_TYPE_FILETYPE was not seen, the filetype is acquired from the
    // memtrace_stream_t object instead.
    bool
    init_from_filetype();

    virtual void
    print_header()
    {
        std::cerr << "Output format:\n"
                  << "<--record#-> <--instr#->: <Wrkld.Tid> <record details>\n"
                  << "------------------------------------------------------------\n";
    }

    void
    print_cached_records(memtrace_stream_t *memstream);

    inline void
    print_prefix(memtrace_stream_t *memstream, const memref_t &memref,
                 int64_t record_ord_subst = -1, std::ostream &stream = std::cerr)
    {
        uint64_t record_ord = record_ord_subst > -1
            ? static_cast<uint64_t>(record_ord_subst)
            : memstream->get_record_ordinal();
        if ((prev_tid_ != -1 && prev_tid_ != memref.instr.tid) ||
            // Print a divider for a skip_instructions gap too.
            (prev_record_ != 0 && prev_record_ + 1 < record_ord))
            stream << "------------------------------------------------------------\n";
        prev_tid_ = memref.instr.tid;
        prev_record_ = record_ord;

        std::ostringstream wtid;
        wtid << "W" << workload_from_memref_tid(memref.data.tid) << ".T"
             << tid_from_memref_tid(memref.data.tid);

        stream << std::setw(RECORD_COLUMN_WIDTH) << record_ord
               << std::setw(INSTR_COLUMN_WIDTH) << memstream->get_instruction_ordinal()
               << ": " << std::setw(TID_COLUMN_WIDTH) << wtid.str() << " ";
    }

    /* We make this the first field so that dr_standalone_exit() is called after
     * destroying the other fields which may use DR heap.
     */
    dcontext_cleanup_last_t dcontext_;

    std::string module_file_path_;
    unsigned int knob_verbose_;
    int trace_version_;
    static const std::string TOOL_NAME;
    std::string knob_syntax_;
    std::string knob_alt_module_dir_;
    uint64_t num_disasm_instrs_;
    memref_tid_t prev_tid_;
    uint64_t prev_record_ = 0;
    intptr_t filetype_;
    std::unordered_map<memref_tid_t, uintptr_t> last_window_;
    uintptr_t timestamp_;
    int64_t timestamp_record_ord_ = -1;
    memref_t timestamp_memref_;
    bool init_from_filetype_done_ = false;
    std::unique_ptr<decode_cache_t<disasm_info_t>> decode_cache_ = nullptr;
    memtrace_stream_t *serial_stream_ = nullptr;

private:
    static constexpr int RECORD_COLUMN_WIDTH = 12;
    static constexpr int INSTR_COLUMN_WIDTH = 12;
    static constexpr int TID_COLUMN_WIDTH = 11;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _VIEW_H_ */
