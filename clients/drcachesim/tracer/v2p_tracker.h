/* **********************************************************
 * Copyright (c) 2026 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* v2p_tracker: Virtual-to-physical (V2P) page-table transition tracker. */

#ifndef _V2P_TRACKER_H_
#define _V2P_TRACKER_H_

#if defined(LINUX) && defined(X64)

#    include <sys/types.h>
#    include <atomic>
#    include <cstdint>
#    include <string>
#    include <unordered_map>
#    include "dr_api.h"
#    include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

class v2p_tracker_t {
public:
    v2p_tracker_t() = default;
    ~v2p_tracker_t()
    {
        stop();
    }

    bool
    start(pid_t pid, const std::string &output_path, uint32_t buffer_size_kb = 16384);
    void
    stop();
    bool
    is_running() const
    {
        return running_.load(std::memory_order_acquire);
    }

private:
    struct pending_fault_t {
        uint64_t vaddr = 0;
        uint64_t pfn = 0;
        uint64_t order = 0;
        bool has_alloc = false;
    };

    bool
    kernel_at_least(int major, int minor) const
    {
        return kernel_major_ > major ||
            (kernel_major_ == major && kernel_minor_ >= minor);
    }
    void
    cleanup_kernel_tracing();
    static void
    reader_thread_entry(void *arg);
    void
    drain_pipe();
    void
    process_trace_line(const std::string &line);

    pid_t target_pid_ = 0;
    pid_t reader_tid_ = 0;
    int kernel_major_ = 0;
    int kernel_minor_ = 0;
    std::string tracefs_root_;
    std::string instance_dir_;
    std::string pipe_remainder_;
    uint64_t clock_offset_us_ = 0;
    int out_fd_ = -1;
    int trace_pipe_fd_ = -1;
    bool kernel_tracing_active_ = false;
    std::atomic<bool> running_ { false };
    std::atomic<bool> stop_requested_ { false };
    std::atomic<bool> thread_running_ { false };
    std::atomic<bool> thread_exited_ { false };
    std::unordered_map<pid_t, pending_fault_t> pending_by_tid_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* LINUX */
#endif /* _V2P_TRACKER_H_ */
