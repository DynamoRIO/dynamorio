/* **********************************************************
 * Copyright (c) 2021-2023 Google, LLC  All rights reserved.
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

#include <fstream>
#include <iostream>
#include <vector>

#include "../tools/invariant_checker.h"
#include "../common/memref.h"
#include "memref_gen.h"

namespace dynamorio {
namespace drmemtrace {

struct error_info_t {
    std::string invariant_name;
    memref_tid_t tid;
    uint64_t ref_ordinal;
    uint64_t last_timestamp;
    uint64_t instrs_since_last_timestamp;

    bool
    operator!=(const error_info_t &rhs) const
    {
        return rhs.invariant_name != invariant_name || rhs.tid != tid ||
            rhs.ref_ordinal != ref_ordinal || rhs.last_timestamp != last_timestamp ||
            rhs.instrs_since_last_timestamp != instrs_since_last_timestamp;
    }
};

class checker_no_abort_t : public invariant_checker_t {
public:
    explicit checker_no_abort_t(bool offline)
        : invariant_checker_t(offline)
    {
    }
    checker_no_abort_t(bool offline, std::istream *serial_schedule_file)
        : invariant_checker_t(offline, 1, "invariant_checker_test", serial_schedule_file)
    {
    }
    std::vector<error_info_t> errors_;

    bool
    print_results() override
    {
        per_shard_t global;
        check_schedule_data(&global);
        return true;
    }

protected:
    void
    report_if_false(per_shard_t *shard, bool condition,
                    const std::string &invariant_name) override
    {
        if (!condition) {
            std::cerr << "Recording |" << invariant_name << "| in T" << shard->tid_
                      << " @ ref # " << shard->ref_count_ << " ("
                      << shard->instr_count_since_last_timestamp_
                      << " instrs since timestamp " << shard->last_timestamp_ << ")\n";
            errors_.push_back({ invariant_name, shard->tid_, shard->ref_count_,
                                shard->last_timestamp_,
                                shard->instr_count_since_last_timestamp_ });
        }
    }
};

/* Assumes there are at most 3 threads with tids 1, 2, and 3 in memrefs. */
bool
run_checker(const std::vector<memref_t> &memrefs, bool expect_error,
            const error_info_t &expected_error_info = {},
            const std::string &toprint_if_fail = "",
            std::istream *serial_schedule_file = nullptr)
{
    // Serial.
    {
        checker_no_abort_t checker(true /*offline*/, serial_schedule_file);
        for (const auto &memref : memrefs) {
            checker.process_memref(memref);
        }
        checker.print_results();
        if (expect_error) {
            if (checker.errors_.size() != 1 ||
                expected_error_info != checker.errors_[0]) {
                std::cerr << toprint_if_fail << "\n";
                return false;
            }
        } else if (!checker.errors_.empty()) {
            for (auto &error : checker.errors_) {
                std::cerr << "Unexpected error: " << error.invariant_name
                          << " at ref: " << error.ref_ordinal << "\n";
            }
            return false;
        }
    }
    // Parallel.
    {
        if (serial_schedule_file != nullptr) {
            // Reset to the start of the file.
            serial_schedule_file->clear();
            serial_schedule_file->seekg(0, std::ios::beg);
        }
        checker_no_abort_t checker(true /*offline*/, serial_schedule_file);
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
        checker.print_results();
        if (expect_error) {
            if (checker.errors_.size() != 1 ||
                checker.errors_[0] != expected_error_info) {
                std::cerr << toprint_if_fail << "\n";
                return false;
            }
        } else if (!checker.errors_.empty()) {
            for (auto &error : checker.errors_) {
                std::cerr << "Unexpected error: " << error.invariant_name
                          << " at ref: " << error.ref_ordinal << "\n";
            }
            return false;
        }
    }
    return true;
}

bool
check_branch_target_after_branch()
{
    std::cerr << "Testing branch targets\n";
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
        constexpr uintptr_t TIMESTAMP = 3;
        constexpr memref_tid_t TID = 1;
        std::vector<memref_t> memrefs = {
            gen_instr(TID, 1),
            gen_branch(TID, 2),
            gen_marker(TID + 1, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP),
            gen_instr(TID + 1, 1),
            gen_marker(TID, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP),
            gen_instr(TID, 3),
        };
        if (!run_checker(memrefs, true,
                         { "Branch target not immediately after branch", TID,
                           /*ref_ordinal=*/4, /*last_timestamp=*/TIMESTAMP,
                           /*instrs_since_last_timestamp=*/1 },
                         "Failed to catch bad branch target position")) {
            return false;
        }
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
    std::cerr << "Testing control flow\n";
    constexpr memref_tid_t TID = 1;
    // Negative simple test.
    {
        std::vector<memref_t> memrefs = {
            gen_instr(TID, 1),
            gen_instr(TID, 3),
        };
        if (!run_checker(memrefs, true,
                         { "Non-explicit control flow has no marker", TID,
                           /*ref_ordinal=*/2, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/2 },
                         "Failed to catch bad control flow"))
            return false;
    }
    // Negative test with timestamp markers.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID, TRACE_MARKER_TYPE_TIMESTAMP, 2),
            gen_instr(TID, 1),
            gen_marker(TID, TRACE_MARKER_TYPE_TIMESTAMP, 3),
            gen_instr(TID, 3),
        };
        if (!run_checker(memrefs, true,
                         { "Non-explicit control flow has no marker", TID,
                           /*ref_ordinal=*/4, /*last_timestamp=*/3,
                           /*instrs_since_last_timestamp=*/1 },
                         "Failed to catch bad control flow")) {
            return false;
        }
    }
    // Positive test: branches with no encodings.
    {
        std::vector<memref_t> memrefs = {
            gen_instr(TID, 1),   gen_branch(TID, 2),  gen_instr(TID, 3), // Not taken.
            gen_branch(TID, 4),  gen_instr(TID, 101),                    // Taken.
            gen_instr(TID, 102),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Tests with encodings:
    // We use these client defines which are the target and so drdecode's target arch.
#if defined(X86_64) || defined(X86_32) || defined(ARM_64)
    // XXX: We hardcode encodings here.  If we need many more we should generate them
    // from DR IR.

    // Negative test: branches with encodings which do not go to their targets.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
#    if defined(X86_64) || defined(X86_32)
            // 0x74 is "je" with the 2nd byte the offset.
            gen_branch_encoded(TID, 0x71019dbc, { 0x74, 0x32 }),
            gen_instr_encoded(0x71019ded, { 0x01 }, TID),
#    elif defined(ARM_64)
            // 71019dbc:   540001a1        b.ne    71019df0
            // <__executable_start+0x19df0>
            gen_branch_encoded(TID, 0x71019dbc, 0x540001a1),
            gen_instr_encoded(0x71019ded, 0x01, TID),
#    else
        // TODO i#5871: Add AArch32 (and RISC-V) encodings.
#    endif
        };

        if (!run_checker(memrefs, true,
                         { "Branch does not go to the correct target", TID,
                           /*ref_ordinal=*/3, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/2 },
                         "Failed to catch branch not going to its target")) {
            return false;
        }
    }
    // Positive test: branches with encodings which go to their targets.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
#    if defined(X86_64) || defined(X86_32)
            // 0x74 is "je" with the 2nd byte the offset.
            gen_branch_encoded(TID, 0x71019dbc, { 0x74, 0x32 }),
#    elif defined(ARM_64)
            // 71019dbc:   540001a1        b.ne    71019df0
            // <__executable_start+0x19df0>
            gen_branch_encoded(TID, 0x71019dbc, 0x540001a1),
#    else
        // TODO i#5871: Add AArch32 (and RISC-V) encodings.
#    endif
            gen_instr(TID, 0x71019df0),
        };

        if (!run_checker(memrefs, false)) {
            return false;
        }
    }
#endif
    // String loop.
    {
        std::vector<memref_t> memrefs = {
            gen_instr_type(TRACE_TYPE_INSTR_NO_FETCH, TID, 1),
            gen_instr_type(TRACE_TYPE_INSTR_NO_FETCH, TID, 1),
            gen_instr_type(TRACE_TYPE_INSTR_NO_FETCH, TID, 1),
            gen_instr_type(TRACE_TYPE_INSTR_NO_FETCH, TID, 1),
            gen_instr(TID, 2),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Kernel-mediated.
    {
        std::vector<memref_t> memrefs = {
            gen_instr(TID, 1),
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_instr(TID, 101),
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
    std::cerr << "Testing kernel xfers\n";
    constexpr memref_tid_t TID = 1;
    // Return to recorded interruption point.
    {
        std::vector<memref_t> memrefs = {
            gen_instr(TID, 1),
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_instr(TID, 101),
            // XXX: This marker value is actually not guaranteed, yet the checker
            // requires it and the view tool prints it.
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_XFER, 102),
            gen_instr(TID, 2),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Signal before any instr in the trace.
    {
        std::vector<memref_t> memrefs = {
            // No instr in the beginning here. Should skip pre-signal instr check
            // on return.
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_instr(TID, 101),
            // XXX: This marker value is actually not guaranteed, yet the checker
            // requires it and the view tool prints it.
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_XFER, 102),
            gen_instr(TID, 2),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Nested signals without any intervening instr.
    {
        std::vector<memref_t> memrefs = {
            gen_instr(TID, 1),
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            // No intervening instr here. Should skip pre-signal instr check on
            // return.
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_EVENT, 101),
            gen_instr(TID, 201),
            // XXX: This marker value is actually not guaranteed, yet the checker
            // requires it and the view tool prints it.
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_XFER, 202),
            gen_instr(TID, 101),
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_XFER, 102),
            gen_instr(TID, 2),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Nested signals without any intervening instr or initial instr.
    {
        std::vector<memref_t> memrefs = {
            // No initial instr. Should skip pre-signal instr check on return.
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            // No intervening instr here. Should skip pre-signal instr check on
            // return.
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_EVENT, 101),
            gen_instr(TID, 201),
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_XFER, 202),
            gen_instr(TID, 101),
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_XFER, 102),
            gen_instr(TID, 2),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Consecutive signals (that are nested at the same depth) without any
    // intervening instr between them.
    {
        std::vector<memref_t> memrefs = {
            gen_instr(TID, 1),
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_instr(TID, 101),
            // First signal.
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_EVENT, 102),
            gen_instr(TID, 201),
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_XFER, 202),
            // Second signal.
            // No intervening instr here. Should use instr at pc = 101 for
            // pre-signal instr check on return.
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_EVENT, 102),
            gen_instr(TID, 201),
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_XFER, 202),
            gen_instr(TID, 102),
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_XFER, 103),
            gen_instr(TID, 2),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Consecutive signals (that are nested at the same depth) without any
    // intervening instr between them, and no instr before the first of them
    // and its outer signal.
    {
        std::vector<memref_t> memrefs = {
            gen_instr(TID, 1),
            // Outer signal.
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            // First signal.
            // No intervening instr here. Should skip pre-signal instr check
            // on return.
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_EVENT, 102),
            gen_instr(TID, 201),
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_XFER, 202),
            // Second signal.
            // No intervening instr here. Since there's no pre-signal instr
            // for the first signal as well, we did not see any instr at this
            // signal-depth. So the pre-signal check should be skipped on return
            // of this signal too.
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_EVENT, 102),
            gen_instr(TID, 201),
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_XFER, 202),
            gen_instr(TID, 102),
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_XFER, 103),
            gen_instr(TID, 2),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Trace starts in a signal.
    {
        std::vector<memref_t> memrefs = {
            // Already inside the first signal.
            gen_instr(TID, 11),
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_XFER, 12),
            // Should skip the pre-signal instr check and the kernel_event marker
            // equality check, since we did not see the beginning of the signal in
            // the trace.
            gen_instr(TID, 2),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Trace starts in a signal with a back-to-back signal without any intervening
    // instr after we return from the first one.
    {
        std::vector<memref_t> memrefs = {
            // Already inside the first signal.
            gen_instr(TID, 11),
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_XFER, 12),
            // No intervening instr here. Should skip pre-signal instr check on
            // return; this is a special case as it would require *removing* the
            // pc = 11 instr from pre_signal_instr_ as it was not in this newly
            // discovered outermost scope.
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_instr(TID, 21),
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_XFER, 22),
            gen_instr(TID, 2),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Fail to return to recorded interruption point.
    {
        std::vector<memref_t> memrefs = {
            gen_instr(TID, 1),   gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_instr(TID, 101), gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_XFER, 102),
            gen_instr(TID, 3),
        };
        if (!run_checker(memrefs, true,
                         { "Signal handler return point incorrect", TID,
                           /*ref_ordinal=*/5, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/3 },
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
    std::cerr << "Testing rseq\n";
    constexpr memref_tid_t TID = 1;
    // Roll back rseq final instr.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID, TRACE_MARKER_TYPE_RSEQ_ENTRY, 3),
            gen_instr(TID, 1),
            // Rolled back instr at pc=2 size=1.
            // Point to the abort handler.
            gen_marker(TID, TRACE_MARKER_TYPE_RSEQ_ABORT, 4),
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_EVENT, 4),
            gen_instr(TID, 4),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID, TRACE_MARKER_TYPE_RSEQ_ENTRY, 3),
            gen_instr(TID, 1),
            gen_instr(TID, 2),
            // A fault in the instrumented execution.
            gen_marker(TID, TRACE_MARKER_TYPE_RSEQ_ABORT, 2),
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_EVENT, 4),
            gen_instr(TID, 10),
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_XFER, 11),
            gen_instr(TID, 4),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Fail to roll back rseq final instr.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID, TRACE_MARKER_TYPE_RSEQ_ENTRY, 3),
            gen_instr(TID, 1),
            gen_instr(TID, 2),
            gen_marker(TID, TRACE_MARKER_TYPE_RSEQ_ABORT, 4),
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_EVENT, 4),
            gen_instr(TID, 4),
        };
        if (!run_checker(memrefs, true,
                         { "Rseq post-abort instruction not rolled back", TID,
                           /*ref_ordinal=*/4, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/2 },
                         "Failed to catch bad rseq abort"))
            return false;
    }
#endif
    return true;
}

bool
check_function_markers()
{
    std::cerr << "Testing function markers\n";
    constexpr memref_tid_t TID = 1;
    constexpr addr_t CALL_PC = 2;
    constexpr size_t CALL_SZ = 2;
    // Incorrectly between instr and memref.
    {
        std::vector<memref_t> memrefs = {
            gen_instr_type(TRACE_TYPE_INSTR_DIRECT_CALL, TID, CALL_PC, CALL_SZ),
            gen_marker(1, TRACE_MARKER_TYPE_FUNC_ID, 2),
            // There should be just one error.
            gen_marker(1, TRACE_MARKER_TYPE_FUNC_RETADDR, CALL_PC + CALL_SZ),
            gen_marker(1, TRACE_MARKER_TYPE_FUNC_ARG, 2),
            gen_data(1, true, 42, 8),
        };
        if (!run_checker(memrefs, true,
                         { "Function marker misplaced between instr and memref", TID,
                           /*ref_ordinal=*/5, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/1 },
                         "Failed to catch misplaced function marker"))
            return false;
    }
    // Incorrectly not after a branch.
    {
        std::vector<memref_t> memrefs = {
            gen_instr(TID, 1),
            gen_marker(TID, TRACE_MARKER_TYPE_FUNC_ID, 2),
        };
        if (!run_checker(memrefs, true,
                         { "Function marker should be after a branch", TID,
                           /*ref_ordinal=*/2, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/1 },
                         "Failed to catch function marker not after branch"))
            return false;
    }
    // Incorrect return address.
    {
        std::vector<memref_t> memrefs = {
            gen_instr_type(TRACE_TYPE_INSTR_DIRECT_CALL, TID, CALL_PC, CALL_SZ),
            gen_marker(TID, TRACE_MARKER_TYPE_FUNC_ID, 2),
            gen_marker(TID, TRACE_MARKER_TYPE_FUNC_RETADDR, CALL_PC + CALL_SZ + 1),
            gen_marker(TID, TRACE_MARKER_TYPE_FUNC_ARG, 2),
        };
        if (!run_checker(memrefs, true,
                         { "Function marker retaddr should match prior call", TID,
                           /*ref_ordinal=*/3, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/1 },
                         "Failed to catch wrong function retaddr"))
            return false;
    }
    // Incorrectly not after a branch with a load in between.
    {
        std::vector<memref_t> memrefs = {
            gen_instr(TID, 1),
            gen_data(TID, true, 42, 8),
            gen_marker(TID, TRACE_MARKER_TYPE_FUNC_ID, 2),
        };
        if (!run_checker(memrefs, true,
                         { "Function marker should be after a branch", TID,
                           /*ref_ordinal=*/3, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/1 },
                         "Failed to catch function marker after non-branch with load"))
            return false;
    }
    // Correctly after a branch.
    {
        std::vector<memref_t> memrefs = {
            gen_instr(TID, 1),
            gen_instr_type(TRACE_TYPE_INSTR_DIRECT_CALL, TID, CALL_PC, CALL_SZ),
            gen_marker(TID, TRACE_MARKER_TYPE_FUNC_ID, 2),
            gen_marker(TID, TRACE_MARKER_TYPE_FUNC_RETADDR, CALL_PC + CALL_SZ),
            gen_marker(TID, TRACE_MARKER_TYPE_FUNC_ARG, 2),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Correctly after a branch with memref for the branch.
    {
        std::vector<memref_t> memrefs = {
            gen_instr(TID, 1),
            gen_instr_type(TRACE_TYPE_INSTR_DIRECT_CALL, TID, CALL_PC, CALL_SZ),
            gen_instr_type(TRACE_TYPE_INSTR_INDIRECT_JUMP, TID, 3),
            gen_data(TID, true, 42, 8),
            gen_marker(TID, TRACE_MARKER_TYPE_FUNC_ID, 2),
            gen_marker(TID, TRACE_MARKER_TYPE_FUNC_RETADDR, CALL_PC + CALL_SZ),
            gen_marker(TID, TRACE_MARKER_TYPE_FUNC_ARG, 2),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    return true;
}

bool
check_duplicate_syscall_with_same_pc()
{
    std::cerr << "Testing duplicate syscall\n";
    // Negative: syscalls with the same PC.
#if defined(X86_64) || defined(X86_32) || defined(ARM_64)
    constexpr addr_t ADDR = 0x7fcf3b9d;
    {
        std::vector<memref_t> memrefs = {
            gen_marker(1, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
#    if defined(X86_64) || defined(X86_32)
            gen_instr_encoded(ADDR, { 0x0f, 0x05 }), // 0x7fcf3b9d: 0f 05 syscall
            gen_marker(1, TRACE_MARKER_TYPE_TIMESTAMP, 6),
            gen_marker(1, TRACE_MARKER_TYPE_CPU_ID, 3),
            gen_instr_encoded(ADDR, { 0x0f, 0x05 }), // 0x7fcf3b9d: 0f 05 syscall
#    elif defined(ARM_64)
            gen_instr_encoded(ADDR,
                              0xd4000001), // 0x7fcf3b9d: 0xd4000001 svc #0x0
            gen_marker(1, TRACE_MARKER_TYPE_TIMESTAMP, 6),
            gen_marker(1, TRACE_MARKER_TYPE_CPU_ID, 3),
            gen_instr_encoded(ADDR,
                              0xd4000001), // 0x7fcf3b9d: 0xd4000001 svc #0x0
#    else
        // TODO i#5871: Add AArch32 (and RISC-V) encodings.
#    endif
        };
        if (!run_checker(memrefs, true,
                         { "Duplicate syscall instrs with the same PC", /*tid=*/1,
                           /*ref_ordinal=*/5, /*last_timestamp=*/6,
                           /*instrs_since_last_timestamp=*/1 },
                         "Failed to catch duplicate syscall instrs with the same PC"))
            return false;
    }

    // Positive test: syscalls with different PCs.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(1, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
#    if defined(X86_64) || defined(X86_32)
            gen_instr_encoded(ADDR, { 0x0f, 0x05 }), // 0x7fcf3b9d: 0f 05 syscall
            gen_marker(1, TRACE_MARKER_TYPE_TIMESTAMP, 0),
            gen_marker(1, TRACE_MARKER_TYPE_CPU_ID, 3),
            gen_instr_encoded(ADDR + 2, { 0x0f, 0x05 }), // 0x7fcf3b9dd9eb: 0f 05 syscall
#    elif defined(ARM_64)
            gen_instr_encoded(ADDR, 0xd4000001,
                              2), // 0x7fcf3b9d: 0xd4000001 svc #0x0
            gen_marker(1, TRACE_MARKER_TYPE_TIMESTAMP, 0),
            gen_marker(1, TRACE_MARKER_TYPE_CPU_ID, 3),
            gen_instr_encoded(ADDR + 4, 0xd4000001,
                              2), // 0x7fcf3b9dd9eb: 0xd4000001 svc #0x0
#    else
        // TODO i#5871: Add AArch32 (and RISC-V) encodings.
#    endif
        };
        if (!run_checker(memrefs, false)) {
            return false;
        }
    }
#endif
    return true;
}

bool
check_false_syscalls()
{
    // Ensure missing syscall markers (from "false syscalls") are detected.
#if defined(WINDOWS) && !defined(X64)
    // TODO i#5949: For WOW64 instr_is_syscall() always returns false, so our
    // checks do not currently work properly there.
    return true;
#else
    // XXX: Just like raw2trace_unit_tests, we need to create a syscall instruction
    // and it turns out there is no simple cross-platform way.
#    ifdef X86
    instr_t *sys = INSTR_CREATE_syscall(GLOBAL_DCONTEXT);
#    elif defined(AARCHXX)
    instr_t *sys =
        INSTR_CREATE_svc(GLOBAL_DCONTEXT, opnd_create_immed_int((sbyte)0x0, OPSZ_1));
#    elif defined(RISCV64)
    instr_t *sys = INSTR_CREATE_ecall(GLOBAL_DCONTEXT);
#    else
#        error Unsupported architecture.
#    endif
    instr_t *move1 =
        XINST_CREATE_move(GLOBAL_DCONTEXT, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instrlist_t *ilist = instrlist_create(GLOBAL_DCONTEXT);
    instrlist_append(ilist, sys);
    instrlist_append(ilist, move1);
    static constexpr addr_t BASE_ADDR = 0x123450;
    static constexpr uintptr_t FILE_TYPE =
        OFFLINE_FILE_TYPE_ENCODINGS | OFFLINE_FILE_TYPE_SYSCALL_NUMBERS;
    bool res = true;
    {
        // Correct: syscall followed by marker.
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(1, TRACE_MARKER_TYPE_FILETYPE, FILE_TYPE), nullptr },
            { gen_instr(1), sys },
            { gen_marker(1, TRACE_MARKER_TYPE_SYSCALL, 42), nullptr },
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        if (!run_checker(memrefs, false))
            res = false;
    }
    {
        // Correct: syscall followed by marker with timestamp+cpu in between.
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(1, TRACE_MARKER_TYPE_FILETYPE, FILE_TYPE), nullptr },
            { gen_instr(1), sys },
            { gen_marker(1, TRACE_MARKER_TYPE_TIMESTAMP, 101), nullptr },
            { gen_marker(1, TRACE_MARKER_TYPE_CPU_ID, 3), nullptr },
            { gen_marker(1, TRACE_MARKER_TYPE_SYSCALL, 42), nullptr },
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        if (!run_checker(memrefs, false))
            res = false;
    }
    {
        // Incorrect: syscall with no marker.
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(1, TRACE_MARKER_TYPE_FILETYPE, FILE_TYPE), nullptr },
            { gen_instr(1), sys },
            { gen_instr(1), move1 }
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        if (!run_checker(memrefs, true,
                         { "Syscall instruction not followed by syscall marker",
                           /*tid=*/1,
                           /*ref_ordinal=*/3, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/2 },
                         "Failed to catch syscall without number marker")) {
            res = false;
        }
    }
    {
        // Incorrect: marker with no syscall.
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(1, TRACE_MARKER_TYPE_FILETYPE, FILE_TYPE), nullptr },
            { gen_instr(1), move1 },
            { gen_marker(1, TRACE_MARKER_TYPE_SYSCALL, 42), nullptr },
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        if (!run_checker(memrefs, true,
                         { "Syscall marker not placed after syscall instruction",
                           /*tid=*/1,
                           /*ref_ordinal=*/3, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/1 },
                         "Failed to catch misplaced syscall marker")) {
            res = false;
        }
    }
    instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
    return res;
#endif
}

bool
check_rseq_side_exit_discontinuity()
{
    std::cerr << "Testing rseq side exits\n";
    // Negative test: Seemingly missing instructions in a basic block due to rseq side
    // exit.
    instr_t *store = XINST_CREATE_store(GLOBAL_DCONTEXT, OPND_CREATE_MEMPTR(REG2, 0),
                                        opnd_create_reg(REG1));
    instr_t *move1 =
        XINST_CREATE_move(GLOBAL_DCONTEXT, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instr_t *move2 =
        XINST_CREATE_move(GLOBAL_DCONTEXT, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instr_t *cond_jmp =
        XINST_CREATE_jump_cond(GLOBAL_DCONTEXT, DR_PRED_EQ, opnd_create_instr(move2));

    instrlist_t *ilist = instrlist_create(GLOBAL_DCONTEXT);
    instrlist_append(ilist, cond_jmp);
    instrlist_append(ilist, store);
    instrlist_append(ilist, move1);
    instrlist_append(ilist, move2);

    std::vector<memref_with_IR_t> memref_instr_vec = {
        { gen_marker(1, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
          nullptr },
        // Rseq entry marker not added to make the sequence look like a legacy
        // trace.
        { gen_branch(1), cond_jmp },
        { gen_instr(1), store },
        { gen_data(1, false, 42, 4), nullptr },
        // move1 instruction missing due to the 'side-exit' at move2 which is
        // the target of cond_jmp.
        { gen_instr(1), move2 }
    };

    // TODO i#6023: Use this IR based encoder in other tests as well.
    static constexpr addr_t BASE_ADDR = 0xeba4ad4;
    auto memrefs = add_encodings_to_memrefs(ilist, memref_instr_vec, BASE_ADDR);
    instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
    if (!run_checker(memrefs, true,
                     { "PC discontinuity due to rseq side exit", /*tid=*/1,
                       /*ref_ordinal=*/5, /*last_timestamp=*/0,
                       /*instrs_since_last_timestamp=*/3 },
                     "Failed to catch PC discontinuity from rseq side exit")) {
        return false;
    }
    return true;
}

bool
check_schedule_file()
{
    std::cerr << "Testing schedule files\n";
    // Synthesize a serial schedule file.
    // We leave the cpu-schedule testing to the real-app tests.
    static constexpr memref_tid_t TID_BASE = 1; // Assumed by run_checker.
    static constexpr memref_tid_t TIMESTAMP_BASE = 100;
    static constexpr int CPU_BASE = 6;
    std::string serial_fname = "tmp_inv_check_serial.bin";
    std::vector<schedule_entry_t> sched;
    sched.emplace_back(TID_BASE, TIMESTAMP_BASE, CPU_BASE, 0);
    // Include same-timestamp records to stress handling that.
    sched.emplace_back(TID_BASE + 2, TIMESTAMP_BASE, CPU_BASE + 1, 0);
    sched.emplace_back(TID_BASE + 1, TIMESTAMP_BASE, CPU_BASE + 2, 0);
    sched.emplace_back(TID_BASE, TIMESTAMP_BASE + 1, CPU_BASE + 1, 2);
    sched.emplace_back(TID_BASE + 1, TIMESTAMP_BASE + 2, CPU_BASE, 1);
    sched.emplace_back(TID_BASE + 2, TIMESTAMP_BASE + 3, CPU_BASE + 2, 3);
    {
        std::ofstream serial_file(serial_fname, std::ofstream::binary);
        if (!serial_file)
            return false;
        if (!serial_file.write(reinterpret_cast<char *>(sched.data()),
                               sched.size() * sizeof(sched[0])))
            return false;
    }
    {
        // Create a schedule that matches the file.
        std::vector<memref_t> memrefs = {
            gen_marker(TID_BASE, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE),
            gen_marker(TID_BASE, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE),
            gen_instr(TID_BASE, 1),
            gen_instr(TID_BASE, 2),
            gen_marker(TID_BASE + 2, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE),
            gen_marker(TID_BASE + 2, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE + 1),
            gen_instr(TID_BASE + 2, 1),
            gen_instr(TID_BASE + 2, 2),
            gen_instr(TID_BASE + 2, 3),
            gen_marker(TID_BASE + 1, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE),
            gen_marker(TID_BASE + 1, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE + 2),
            gen_instr(TID_BASE + 1, 1),
            gen_marker(TID_BASE, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE + 1),
            gen_marker(TID_BASE, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE + 1),
            gen_instr(TID_BASE, 3),
            gen_instr(TID_BASE, 4),
            gen_marker(TID_BASE + 1, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE + 2),
            gen_marker(TID_BASE + 1, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE),
            gen_instr(TID_BASE + 1, 2),
            gen_marker(TID_BASE + 2, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE + 3),
            gen_marker(TID_BASE + 2, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE + 2),
            gen_instr(TID_BASE + 2, 4),
        };
        std::ifstream serial_read(serial_fname, std::ifstream::binary);
        if (!serial_read)
            return false;
        if (!run_checker(memrefs, false, {}, "", &serial_read))
            return false;
    }
    {
        // Create a schedule that does not match the file in record count.
        std::vector<memref_t> memrefs = {
            gen_marker(TID_BASE, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE),
            gen_marker(TID_BASE, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE),
            gen_instr(TID_BASE, 1),
            gen_instr(TID_BASE, 2),
            gen_marker(TID_BASE + 2, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE),
            gen_marker(TID_BASE + 2, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE + 1),
            gen_instr(TID_BASE + 2, 1),
            gen_instr(TID_BASE + 2, 2),
            gen_instr(TID_BASE + 2, 3),
            gen_marker(TID_BASE + 1, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE),
            gen_marker(TID_BASE + 1, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE + 2),
            gen_instr(TID_BASE + 1, 1),
            gen_marker(TID_BASE, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE + 1),
            gen_marker(TID_BASE, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE + 1),
            gen_instr(TID_BASE, 3),
            gen_instr(TID_BASE, 4),
            gen_marker(TID_BASE + 1, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE + 2),
            gen_marker(TID_BASE + 1, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE),
            gen_instr(TID_BASE + 1, 2),
            // Missing the final timestamp+cpu.
        };
        std::ifstream serial_read(serial_fname, std::ifstream::binary);
        if (!serial_read)
            return false;
        if (!run_checker(memrefs, true,
                         { "Serial schedule entry count does not match trace", /*tid=*/-1,
                           /*ref_ordinal=*/0, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/0 },
                         "Failed to catch incorrect serial schedule count", &serial_read))
            return false;
    }
    {
        // Create a schedule that does not match the file in one record.
        std::vector<memref_t> memrefs = {
            gen_marker(TID_BASE, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE),
            gen_marker(TID_BASE, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE),
            gen_instr(TID_BASE, 1),
            gen_instr(TID_BASE, 2),
            gen_marker(TID_BASE + 2, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE),
            gen_marker(TID_BASE + 2, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE + 1),
            gen_instr(TID_BASE + 2, 1),
            gen_instr(TID_BASE + 2, 2),
            // Missing one instruction here.
            gen_marker(TID_BASE + 1, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE),
            gen_marker(TID_BASE + 1, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE + 2),
            gen_instr(TID_BASE + 1, 1),
            gen_marker(TID_BASE, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE + 1),
            gen_marker(TID_BASE, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE + 1),
            gen_instr(TID_BASE, 3),
            gen_instr(TID_BASE, 4),
            gen_marker(TID_BASE + 1, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE + 2),
            gen_marker(TID_BASE + 1, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE),
            gen_instr(TID_BASE + 1, 2),
            gen_marker(TID_BASE + 2, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE + 3),
            gen_marker(TID_BASE + 2, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE + 2),
            gen_instr(TID_BASE + 2, 3),
        };
        std::ifstream serial_read(serial_fname, std::ifstream::binary);
        if (!serial_read)
            return false;
        if (!run_checker(memrefs, true,
                         { "Serial schedule entry does not match trace", TID_BASE + 2,
                           /*ref_ordinal=*/3, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/0 },
                         "Failed to catch incorrect serial schedule entry", &serial_read))
            return false;
    }

    return true;
}

bool
check_branch_decoration()
{
    std::cerr << "Testing branch decoration\n";
    static constexpr memref_tid_t TID = 1;
    // Indirect branch target: correct.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID, TRACE_MARKER_TYPE_VERSION, TRACE_ENTRY_VERSION_BRANCH_INFO),
            gen_instr(TID, /*pc=*/1),
            gen_instr_type(TRACE_TYPE_INSTR_INDIRECT_CALL, TID, /*pc=*/2, /*size=*/1,
                           /*target=*/32),
            gen_instr(TID, /*pc=*/32),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
#ifdef UNIX
    // Indirect branch target with kernel event: correct.
    // We ensure the next PC is obtained from the kernel event interruption.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID, TRACE_MARKER_TYPE_VERSION, TRACE_ENTRY_VERSION_BRANCH_INFO),
            gen_instr(TID, /*pc=*/1),
            gen_instr_type(TRACE_TYPE_INSTR_INDIRECT_CALL, TID, /*pc=*/2, /*size=*/1,
                           /*target=*/32),
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_EVENT, 32),
            gen_instr(TID, /*pc=*/999),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
#endif
    // Indirect branch target: incorrect zero target PC.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID, TRACE_MARKER_TYPE_VERSION, TRACE_ENTRY_VERSION_BRANCH_INFO),
            gen_instr(TID, /*pc=*/1),
            gen_instr_type(TRACE_TYPE_INSTR_INDIRECT_CALL, TID, /*pc=*/2, /*size=*/1,
                           /*target=*/0),
        };
        if (!run_checker(memrefs, true,
                         { "Indirect branches must contain targets", TID,
                           /*ref_ordinal=*/3, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/2 },
                         "Failed to catch missing indirect branch target field"))
            return false;
    }
    // Indirect branch target: incorrect target value.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID, TRACE_MARKER_TYPE_VERSION, TRACE_ENTRY_VERSION_BRANCH_INFO),
            gen_instr(TID, /*pc=*/1),
            gen_instr_type(TRACE_TYPE_INSTR_INDIRECT_CALL, TID, /*pc=*/2, /*size=*/1,
                           /*target=*/32),
            gen_instr(TID, /*pc=*/33),
        };
        if (!run_checker(memrefs, true,
                         { "Branch does not go to the correct target", TID,
                           /*ref_ordinal=*/4, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/3 },
                         "Failed to catch bad indirect branch target field"))
            return false;
    }
#ifdef UNIX
    // Indirect branch target with kernel event: marker value incorrect.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID, TRACE_MARKER_TYPE_VERSION, TRACE_ENTRY_VERSION_BRANCH_INFO),
            gen_instr(TID, /*pc=*/1),
            gen_instr_type(TRACE_TYPE_INSTR_INDIRECT_CALL, TID, /*pc=*/2, /*size=*/1,
                           /*target=*/32),
            gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_EVENT, 999),
            gen_instr(TID, /*pc=*/32),
        };
        if (!run_checker(memrefs, true,
                         { "Branch does not go to the correct target", TID,
                           /*ref_ordinal=*/4, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/2 },
                         "Failed to catch bad indirect branch target field"))
            return false;
    }
#endif
    // Deprecated branch type.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID, TRACE_MARKER_TYPE_VERSION, TRACE_ENTRY_VERSION_BRANCH_INFO),
            gen_instr(TID, /*pc=*/1),
            gen_instr_type(TRACE_TYPE_INSTR_CONDITIONAL_JUMP, TID, /*pc=*/2),
        };
        if (!run_checker(
                memrefs, true,
                { "The CONDITIONAL_JUMP type is deprecated and should not appear", TID,
                  /*ref_ordinal=*/3, /*last_timestamp=*/0,
                  /*instrs_since_last_timestamp=*/2 },
                "Failed to catch deprecated branch type"))
            return false;
    }
    // Taken branch target: correct.
    {
        instr_t *move = XINST_CREATE_move(GLOBAL_DCONTEXT, opnd_create_reg(REG1),
                                          opnd_create_reg(REG2));
        instr_t *cbr_to_move =
            XINST_CREATE_jump_cond(GLOBAL_DCONTEXT, DR_PRED_EQ, opnd_create_instr(move));
        instr_t *nop = XINST_CREATE_nop(GLOBAL_DCONTEXT);
        instrlist_t *ilist = instrlist_create(GLOBAL_DCONTEXT);
        instrlist_append(ilist, cbr_to_move);
        instrlist_append(ilist, nop);
        instrlist_append(ilist, move);
        static constexpr addr_t BASE_ADDR = 0x123450;
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID, TRACE_MARKER_TYPE_VERSION, TRACE_ENTRY_VERSION_BRANCH_INFO),
              nullptr },
            { gen_marker(TID, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_instr_type(TRACE_TYPE_INSTR_TAKEN_JUMP, TID), cbr_to_move },
            { gen_instr(TID), move },
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
        if (!run_checker(memrefs, false))
            return false;
    }
#ifdef UNIX
    // Taken branch target with kernel event: correct.
    {
        instr_t *move = XINST_CREATE_move(GLOBAL_DCONTEXT, opnd_create_reg(REG1),
                                          opnd_create_reg(REG2));
        instr_t *cbr_to_move =
            XINST_CREATE_jump_cond(GLOBAL_DCONTEXT, DR_PRED_EQ, opnd_create_instr(move));
        instr_t *nop = XINST_CREATE_nop(GLOBAL_DCONTEXT);
        instrlist_t *ilist = instrlist_create(GLOBAL_DCONTEXT);
        instrlist_append(ilist, cbr_to_move);
        instrlist_append(ilist, nop);
        instrlist_append(ilist, move);
        static constexpr addr_t BASE_ADDR = 0x123450;
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID, TRACE_MARKER_TYPE_VERSION, TRACE_ENTRY_VERSION_BRANCH_INFO),
              nullptr },
            { gen_marker(TID, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_instr_type(TRACE_TYPE_INSTR_TAKEN_JUMP, TID), cbr_to_move },
            { gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_EVENT, 0), move },
            { gen_instr(TID), nop },
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
        if (!run_checker(memrefs, false))
            return false;
    }
#endif
    // Taken branch target: incorrect.
    {
        instr_t *move = XINST_CREATE_move(GLOBAL_DCONTEXT, opnd_create_reg(REG1),
                                          opnd_create_reg(REG2));
        instr_t *cbr_to_move =
            XINST_CREATE_jump_cond(GLOBAL_DCONTEXT, DR_PRED_EQ, opnd_create_instr(move));
        instr_t *nop = XINST_CREATE_nop(GLOBAL_DCONTEXT);
        instrlist_t *ilist = instrlist_create(GLOBAL_DCONTEXT);
        instrlist_append(ilist, cbr_to_move);
        instrlist_append(ilist, nop);
        instrlist_append(ilist, move);
        static constexpr addr_t BASE_ADDR = 0x123450;
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID, TRACE_MARKER_TYPE_VERSION, TRACE_ENTRY_VERSION_BRANCH_INFO),
              nullptr },
            { gen_marker(TID, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_instr_type(TRACE_TYPE_INSTR_TAKEN_JUMP, TID), cbr_to_move },
            { gen_instr(TID), nop },
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
        if (!run_checker(memrefs, true,
                         { "Branch does not go to the correct target", /*tid=*/1,
                           /*ref_ordinal=*/4, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/2 },
                         "Failed to catch taken branch falling through"))
            return false;
    }
#ifdef UNIX
    // Taken branch target with kernel event: incorrect.
    {
        instr_t *move = XINST_CREATE_move(GLOBAL_DCONTEXT, opnd_create_reg(REG1),
                                          opnd_create_reg(REG2));
        instr_t *cbr_to_move =
            XINST_CREATE_jump_cond(GLOBAL_DCONTEXT, DR_PRED_EQ, opnd_create_instr(move));
        instr_t *nop = XINST_CREATE_nop(GLOBAL_DCONTEXT);
        instrlist_t *ilist = instrlist_create(GLOBAL_DCONTEXT);
        instrlist_append(ilist, cbr_to_move);
        instrlist_append(ilist, nop);
        instrlist_append(ilist, move);
        static constexpr addr_t BASE_ADDR = 0x123450;
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID, TRACE_MARKER_TYPE_VERSION, TRACE_ENTRY_VERSION_BRANCH_INFO),
              nullptr },
            { gen_marker(TID, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_instr_type(TRACE_TYPE_INSTR_TAKEN_JUMP, TID), cbr_to_move },
            { gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_EVENT, 0), nop },
            { gen_instr(TID), move },
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
        if (!run_checker(memrefs, true,
                         { "Branch does not go to the correct target", /*tid=*/1,
                           /*ref_ordinal=*/4, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/1 },
                         "Failed to catch taken branch falling through to signal"))
            return false;
    }
#endif
    // Untaken branch target: correct.
    {
        instr_t *move = XINST_CREATE_move(GLOBAL_DCONTEXT, opnd_create_reg(REG1),
                                          opnd_create_reg(REG2));
        instr_t *cbr_to_move =
            XINST_CREATE_jump_cond(GLOBAL_DCONTEXT, DR_PRED_EQ, opnd_create_instr(move));
        instr_t *nop = XINST_CREATE_nop(GLOBAL_DCONTEXT);
        instrlist_t *ilist = instrlist_create(GLOBAL_DCONTEXT);
        instrlist_append(ilist, cbr_to_move);
        instrlist_append(ilist, nop);
        instrlist_append(ilist, move);
        static constexpr addr_t BASE_ADDR = 0x123450;
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID, TRACE_MARKER_TYPE_VERSION, TRACE_ENTRY_VERSION_BRANCH_INFO),
              nullptr },
            { gen_marker(TID, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_instr_type(TRACE_TYPE_INSTR_UNTAKEN_JUMP, TID), cbr_to_move },
            { gen_instr(TID), nop },
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
        if (!run_checker(memrefs, false))
            return false;
    }
#ifdef UNIX
    // Untaken branch target with kernel event: correct.
    {
        instr_t *move = XINST_CREATE_move(GLOBAL_DCONTEXT, opnd_create_reg(REG1),
                                          opnd_create_reg(REG2));
        instr_t *cbr_to_move =
            XINST_CREATE_jump_cond(GLOBAL_DCONTEXT, DR_PRED_EQ, opnd_create_instr(move));
        instr_t *nop = XINST_CREATE_nop(GLOBAL_DCONTEXT);
        instrlist_t *ilist = instrlist_create(GLOBAL_DCONTEXT);
        instrlist_append(ilist, cbr_to_move);
        instrlist_append(ilist, nop);
        instrlist_append(ilist, move);
        static constexpr addr_t BASE_ADDR = 0x123450;
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID, TRACE_MARKER_TYPE_VERSION, TRACE_ENTRY_VERSION_BRANCH_INFO),
              nullptr },
            { gen_marker(TID, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_instr_type(TRACE_TYPE_INSTR_UNTAKEN_JUMP, TID), cbr_to_move },
            { gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_EVENT, 0), nop },
            { gen_instr(TID), move },
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
        if (!run_checker(memrefs, false))
            return false;
    }
#endif
    // Untaken branch target: incorrect.
    {
        instr_t *move = XINST_CREATE_move(GLOBAL_DCONTEXT, opnd_create_reg(REG1),
                                          opnd_create_reg(REG2));
        instr_t *cbr_to_move =
            XINST_CREATE_jump_cond(GLOBAL_DCONTEXT, DR_PRED_EQ, opnd_create_instr(move));
        instr_t *nop = XINST_CREATE_nop(GLOBAL_DCONTEXT);
        instrlist_t *ilist = instrlist_create(GLOBAL_DCONTEXT);
        instrlist_append(ilist, cbr_to_move);
        instrlist_append(ilist, nop);
        instrlist_append(ilist, move);
        static constexpr addr_t BASE_ADDR = 0x123450;
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID, TRACE_MARKER_TYPE_VERSION, TRACE_ENTRY_VERSION_BRANCH_INFO),
              nullptr },
            { gen_marker(TID, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_instr_type(TRACE_TYPE_INSTR_UNTAKEN_JUMP, TID), cbr_to_move },
            { gen_instr(TID), move },
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
        if (!run_checker(memrefs, true,
                         { "Branch does not go to the correct target", TID,
                           /*ref_ordinal=*/4, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/2 },
                         "Failed to catch untaken branch going to taken target"))
            return false;
    }
#ifdef UNIX
    // Untaken branch target with kernel event: incorrect.
    {
        instr_t *move = XINST_CREATE_move(GLOBAL_DCONTEXT, opnd_create_reg(REG1),
                                          opnd_create_reg(REG2));
        instr_t *cbr_to_move =
            XINST_CREATE_jump_cond(GLOBAL_DCONTEXT, DR_PRED_EQ, opnd_create_instr(move));
        instr_t *nop = XINST_CREATE_nop(GLOBAL_DCONTEXT);
        instrlist_t *ilist = instrlist_create(GLOBAL_DCONTEXT);
        instrlist_append(ilist, cbr_to_move);
        instrlist_append(ilist, nop);
        instrlist_append(ilist, move);
        static constexpr addr_t BASE_ADDR = 0x123450;
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID, TRACE_MARKER_TYPE_VERSION, TRACE_ENTRY_VERSION_BRANCH_INFO),
              nullptr },
            { gen_marker(TID, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_instr_type(TRACE_TYPE_INSTR_UNTAKEN_JUMP, TID), cbr_to_move },
            { gen_marker(TID, TRACE_MARKER_TYPE_KERNEL_EVENT, 0), move },
            { gen_instr(TID), nop },
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
        if (!run_checker(
                memrefs, true,
                { "Branch does not go to the correct target", TID,
                  /*ref_ordinal=*/4, /*last_timestamp=*/0,
                  /*instrs_since_last_timestamp=*/1 },
                "Failed to catch untaken branch going to taken target at signal"))
            return false;
    }
#endif
    return true;
}

bool
check_filter_endpoint()
{
    std::cerr << "Testing filter end-point marker and file type\n";
    static constexpr memref_tid_t TID = 1;
    // Matching marker and file type: correct.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID, TRACE_MARKER_TYPE_FILETYPE,
                       OFFLINE_FILE_TYPE_IFILTERED |
                           OFFLINE_FILE_TYPE_BIMODAL_FILTERED_WARMUP),
            gen_marker(TID, TRACE_MARKER_TYPE_INSTRUCTION_COUNT, 1),
            gen_marker(TID, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID, TRACE_MARKER_TYPE_FILTER_ENDPOINT, 0),
            gen_instr(TID),
            gen_exit(TID),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Missing TRACE_MARKER_TYPE_FILTER_ENDPOINT marker: incorrect.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID, TRACE_MARKER_TYPE_FILETYPE,
                       OFFLINE_FILE_TYPE_IFILTERED |
                           OFFLINE_FILE_TYPE_BIMODAL_FILTERED_WARMUP),
            gen_marker(TID, TRACE_MARKER_TYPE_INSTRUCTION_COUNT, 1),
            gen_marker(TID, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID),
            gen_exit(TID),
        };
        if (!run_checker(
                memrefs, true,
                { "Expected to find TRACE_MARKER_TYPE_FILTER_ENDPOINT for the given "
                  "file type",
                  TID,
                  /*ref_ordinal=*/6, /*last_timestamp=*/0,
                  /*instrs_since_last_timestamp=*/1 },
                "Failed to catch missing TRACE_MARKER_TYPE_FILTER_ENDPOINT marker"))
            return false;
    }
    // Unexpected TRACE_MARKER_TYPE_FILTER_ENDPOINT marker: incorrect.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_IFILTERED),
            gen_marker(TID, TRACE_MARKER_TYPE_INSTRUCTION_COUNT, 1),
            gen_marker(TID, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID, TRACE_MARKER_TYPE_FILTER_ENDPOINT, 0),
            gen_instr(TID),
            gen_exit(TID),
        };
        if (!run_checker(memrefs, true,
                         { "Found TRACE_MARKER_TYPE_FILTER_ENDPOINT without the "
                           "correct file type",
                           /*tid=*/1,
                           /*ref_ordinal=*/5, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/0 },
                         "Failed to catch unexpected "
                         "TRACE_MARKER_TYPE_FILTER_ENDPOINT marker"))
            return false;
    }
    return true;
}

bool
check_timestamps_increase_monotonically(void)
{
    static constexpr memref_tid_t TID = 1;
    // Correct: timestamps increase monotonically.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID, TRACE_MARKER_TYPE_TIMESTAMP, 0),
            gen_marker(TID, TRACE_MARKER_TYPE_TIMESTAMP, 10),
            gen_marker(TID, TRACE_MARKER_TYPE_TIMESTAMP, 10),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Incorrect: timestamp does not increase monotonically.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID, TRACE_MARKER_TYPE_TIMESTAMP, 0),
            gen_marker(TID, TRACE_MARKER_TYPE_TIMESTAMP, 10),
            gen_marker(TID, TRACE_MARKER_TYPE_TIMESTAMP, 5),
        };
        if (!run_checker(memrefs, true,
                         { "Timestamp does not increase monotonically",
                           /*tid=*/1,
                           /*ref_ordinal=*/3, /*last_timestamp=*/10,
                           /*instrs_since_last_timestamp=*/0 },
                         "Failed to catch timestamps not increasing "
                         "monotonically"))
            return false;
    }
#ifdef X86_32
    // Correct: timestamp rollovers.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID, TRACE_MARKER_TYPE_TIMESTAMP,
                       (std::numeric_limits<uintptr_t>::max)() - 10),
            gen_marker(TID, TRACE_MARKER_TYPE_TIMESTAMP,
                       (std::numeric_limits<uintptr_t>::max)()),
            gen_marker(TID, TRACE_MARKER_TYPE_TIMESTAMP, 10),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
#endif
    return true;
}

int
test_main(int argc, const char *argv[])
{
    if (check_branch_target_after_branch() && check_sane_control_flow() &&
        check_kernel_xfer() && check_rseq() && check_function_markers() &&
        check_duplicate_syscall_with_same_pc() && check_false_syscalls() &&
        check_rseq_side_exit_discontinuity() && check_schedule_file() &&
        check_branch_decoration() && check_filter_endpoint() &&
        check_timestamps_increase_monotonically()) {
        std::cerr << "invariant_checker_test passed\n";
        return 0;
    }
    std::cerr << "invariant_checker_test FAILED\n";
    exit(1);
}

} // namespace drmemtrace
} // namespace dynamorio
