/* **********************************************************
 * Copyright (c) 2016-2023 Google, Inc.  All rights reserved.
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

/* analyzer_multi: represent a memory trace analysis tool that can process
 * a trace from multiple inputs: a file, from a raw file, or over a pipe online.
 */

#ifndef _ANALYZER_MULTI_H_
#define _ANALYZER_MULTI_H_ 1

#include "analyzer.h"
#include "archive_ostream.h"
#include "scheduler.h"
#include "simulator/cache_simulator_create.h"

namespace dynamorio {
namespace drmemtrace {

class analyzer_multi_t : public analyzer_t {
public:
    // Usage: errors encountered during the constructor will set a flag that should
    // be queried via operator!.
    analyzer_multi_t();
    virtual ~analyzer_multi_t();

protected:
    scheduler_t::scheduler_options_t
    init_dynamic_schedule();
    bool
    create_analysis_tools();
    bool
    init_analysis_tools();
    void
    destroy_analysis_tools();

    analysis_tool_t *
    create_analysis_tool_from_options(const std::string &type);

    analysis_tool_t *
    create_external_tool(const std::string &id);

    analysis_tool_t *
    create_invariant_checker();

    std::string
    get_aux_file_path(std::string option_val, std::string default_filename);

    std::string
    get_module_file_path();

    /* Get the cache simulator knobs used by the cache simulator
     * and the cache miss analyzer.
     */
    cache_simulator_knobs_t *
    get_cache_simulator_knobs();

    std::unique_ptr<std::istream> serial_schedule_file_;
    // This is read in a single stream by invariant_checker and so is not
    // an archive_istream_t.
    std::unique_ptr<std::istream> cpu_schedule_file_;
    std::vector<class external_tool_creator_t> loaders_;
    // This is read as an archive and can read the same file if both are set.
    std::unique_ptr<archive_istream_t> cpu_schedule_zip_;
    std::unique_ptr<archive_ostream_t> record_schedule_zip_;
    std::unique_ptr<archive_istream_t> replay_schedule_zip_;

    static const int max_num_tools_ = 8;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _ANALYZER_MULTI_H_ */
