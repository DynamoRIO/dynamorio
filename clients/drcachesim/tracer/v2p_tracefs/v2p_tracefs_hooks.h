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

/* v2p_tracefs_hooks: BTF hook registration and tracefs event handlers for
 * virtual-to-physical page-table transitions.
 */

#ifndef _V2P_TRACEFS_HOOKS_H_
#define _V2P_TRACEFS_HOOKS_H_

#if defined(LINUX) && (defined(X64) || defined(__x86_64__) || defined(__aarch64__)) && \
    !defined(RISCV64) && !defined(__riscv)

#    include <sys/types.h>
#    include <cstddef>
#    include <cstdint>
#    include <string>

#    include "v2p_check_kernel_support.h"

namespace dynamorio {
namespace drmemtrace {

class v2p_tracker_t;

// Validates and registers kernel hooks against BTF and translates `trace_pipe` event
// lines into `v2p_tracker_t` shadow page-table updates.
class v2p_tracefs_hooks_t {
public:
    // Classifies each registered dynamic kprobe event line in `trace_pipe`.
    enum class event_type_t {
        kUnknown,

        // Anonymous / Copy-on-Write page-fault installations (`anon_new`).
        kAnonRmapNew,

        // Explicit virtual range invalidations (`munmap`, `MADV_DONTNEED`, `MADV_FREE`).
        kUnmapPageRange,
        kUnmapVmas,
        kZapVmaRange,

        // 2 MB transparent hugepage PMD splits (`__split_huge_pmd`).
        kSplitHugePmd,
    };

    // Validates every registered V2P hook against `btf` and populates `plan.probes`.
    static void
    check_all_hooks(v2p_btf_checker_t &btf, v2p_kernel_plan_t &plan);

    // Dispatches a `trace_pipe` event line starting at `event_pos` and returns true if
    // `line` matched a registered hook.
    bool
    handle_event(const std::string &line, size_t event_pos, pid_t tid,
                 uint64_t timestamp_us, const v2p_kernel_plan_t &plan,
                 v2p_tracker_t &tracker);

private:
    struct trace_event_t;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* LINUX && X64 && !RISCV64 */
#endif /* _V2P_TRACEFS_HOOKS_H_ */
