/* **********************************************************
 * Copyright (c) 2021-2022 Google, LLC  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, LLC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Test for checks performed by invariant_checker_t that are not tested
 * by the signal_invariants app's prefetch and handler markers.
 * This looks for precise error strings from invariant_checker.cpp: but
 * we will notice if the literals get out of sync as the test will fail.
 */

#include <iostream>
#include <vector>

#include "../tools/invariant_checker.h"
#include "../common/memref.h"
#include "memref_gen.h"

namespace {

class checker_no_abort_t : public invariant_checker_t {
public:
    checker_no_abort_t(bool offline)
        : invariant_checker_t(offline)
    {
    }
    std::vector<std::string> errors;
    std::vector<memref_tid_t> error_tids;
    std::vector<uint64_t> error_refs; // Memref count (ordinal) at error point.

protected:
    void
    report_if_false(per_shard_t *shard, bool condition,
                    const std::string &message) override
    {
        if (!condition) {
            errors.push_back(message);
            error_tids.push_back(shard->tid);
            error_refs.push_back(shard->ref_count);
        }
    }
};

/* Assumes there are at most 3 threads with tids 1, 2, and 3 in memrefs. */
bool
run_checker(const std::vector<memref_t> &memrefs, bool expect_error,
            memref_tid_t error_tid = 0, uint64_t error_refs = 0,
            const std::string &expected_message = "",
            const std::string &toprint_if_fail = "")
{
    // Serial.
    {
        checker_no_abort_t checker(true /*offline*/);
        for (const auto &memref : memrefs) {
            checker.process_memref(memref);
        }
        if (expect_error) {
            if (checker.errors.size() != 1 || checker.errors[0] != expected_message ||
                checker.error_tids[0] != error_tid ||
                checker.error_refs[0] != error_refs) {
                std::cerr << toprint_if_fail << "\n";
                return false;
            }
        } else if (!checker.errors.empty()) {
            for (const auto &error : checker.errors) {
                std::cerr << "Unexpected error: " << error << "\n";
            }
            return false;
        }
    }
    // Parallel.
    {
        checker_no_abort_t checker(true /*offline*/);
        void *shard1 = checker.parallel_shard_init(1, NULL);
        void *shard2 = checker.parallel_shard_init(2, NULL);
        void *shard3 = checker.parallel_shard_init(3, NULL);
        for (const auto &memref : memrefs) {
            if (memref.instr.tid == 1)
                checker.parallel_shard_memref(shard1, memref);
            else if (memref.instr.tid == 2)
                checker.parallel_shard_memref(shard2, memref);
            else if (memref.instr.tid == 3)
                checker.parallel_shard_memref(shard3, memref);
            else {
                std::cerr << "Internal test error: unknown tid\n";
                return false;
            }
        }
        checker.parallel_shard_exit(shard1);
        checker.parallel_shard_exit(shard2);
        checker.parallel_shard_exit(shard3);
        if (expect_error) {
            if (checker.errors.size() != 1 || checker.errors[0] != expected_message ||
                checker.error_tids[0] != error_tid ||
                checker.error_refs[0] != error_refs) {
                std::cerr << toprint_if_fail << "\n";
                return false;
            }
        } else if (!checker.errors.empty()) {
            for (const auto &error : checker.errors) {
                std::cerr << "Unexpected error: " << error << "\n";
            }
            return false;
        }
    }
    return true;
}

bool
check_branch_target_after_branch()
{
    // Positive simple test.
    {
        std::vector<memref_t> memrefs = {
            gen_instr(1, 1), gen_branch(1, 2),
            gen_instr(1, 3), gen_marker(2, TRACE_MARKER_TYPE_TIMESTAMP, 0),
            gen_instr(2, 1),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Negative simple test.
    {
        std::vector<memref_t> memrefs = {
            gen_instr(1, 1),
            gen_branch(1, 2),
            gen_marker(2, TRACE_MARKER_TYPE_TIMESTAMP, 0),
            gen_instr(2, 1),
            gen_marker(1, TRACE_MARKER_TYPE_TIMESTAMP, 0),
            gen_instr(1, 3),
        };
        if (!run_checker(memrefs, true, 1, 4,
                         "Branch target not immediately after branch",
                         "Failed to catch bad branch target position"))
            return false;
    }
    // Invariant relaxed for thread exit or signal.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(3, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(3, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_branch(3, 2),
            gen_exit(3),
            gen_instr(1, 1),
            gen_branch(1, 2),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_EVENT, 3),
            gen_marker(2, TRACE_MARKER_TYPE_TIMESTAMP, 0),
            gen_instr(2, 4),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    return true;
}

bool
check_sane_control_flow()
{
    // Positive test: branches.
    {
        std::vector<memref_t> memrefs = {
            gen_instr(1, 1),   gen_branch(1, 2),  gen_instr(1, 3), // Not taken.
            gen_branch(1, 4),  gen_instr(1, 101),                  // Taken.
            gen_instr(1, 102),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Negative simple test.
    {
        std::vector<memref_t> memrefs = {
            gen_instr(1, 1),
            gen_instr(1, 3),
        };
        if (!run_checker(memrefs, true, 1, 2, "Non-explicit control flow has no marker",
                         "Failed to catch bad control flow"))
            return false;
    }
    // String loop.
    {
        std::vector<memref_t> memrefs = {
            gen_instr_type(TRACE_TYPE_INSTR_NO_FETCH, 1, 1),
            gen_instr_type(TRACE_TYPE_INSTR_NO_FETCH, 1, 1),
            gen_instr_type(TRACE_TYPE_INSTR_NO_FETCH, 1, 1),
            gen_instr_type(TRACE_TYPE_INSTR_NO_FETCH, 1, 1),
            gen_instr(1, 2),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Kernel-mediated.
    {
        std::vector<memref_t> memrefs = {
            gen_instr(1, 1),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_instr(1, 101),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    return true;
}

bool
check_kernel_xfer()
{
#ifdef UNIX
    // Return to recorded interruption point.
    {
        std::vector<memref_t> memrefs = {
            gen_instr(1, 1),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_instr(1, 101),
            // XXX: This marker value is actually not guaranteed, yet the checker
            // requires it and the view tool prints it.
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_XFER, 102),
            gen_instr(1, 2),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Fail to return to recorded interruption point.
    {
        std::vector<memref_t> memrefs = {
            gen_instr(1, 1),   gen_marker(1, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_instr(1, 101), gen_marker(1, TRACE_MARKER_TYPE_KERNEL_XFER, 102),
            gen_instr(1, 3),
        };
        if (!run_checker(memrefs, true, 1, 5, "Signal handler return point incorrect",
                         "Failed to catch bad signal handler return"))
            return false;
    }
#endif
    return true;
}

bool
check_rseq()
{
#ifdef UNIX
    // Roll back rseq final instr.
    {
        std::vector<memref_t> memrefs = {
            gen_instr(1, 1),
            gen_instr(1, 2),
            gen_marker(1, TRACE_MARKER_TYPE_RSEQ_ABORT, 1),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_EVENT, 1),
            gen_instr(1, 1),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Fail to roll back rseq final instr.
    {
        std::vector<memref_t> memrefs = {
            gen_instr(1, 1),
            gen_instr(1, 2),
            gen_marker(1, TRACE_MARKER_TYPE_RSEQ_ABORT, 2),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_instr(1, 1),
        };
        if (!run_checker(memrefs, true, 1, 3,
                         "Rseq post-abort instruction not rolled back",
                         "Failed to catch bad rseq abort"))
            return false;
    }
#endif
    return true;
}
} // namespace

int
main(int argc, const char *argv[])
{
    if (check_branch_target_after_branch() && check_sane_control_flow() &&
        check_kernel_xfer() && check_rseq()) {
        std::cerr << "invariant_checker_test passed\n";
        return 0;
    }
    std::cerr << "invariant_checker_test FAILED\n";
    exit(1);
}
