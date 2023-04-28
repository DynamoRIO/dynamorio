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

#include <iostream>
#include <vector>

#include "../tools/invariant_checker.h"
#include "../common/memref.h"
#include "memref_gen.h"

namespace {

#ifdef X86
#    define REG1 DR_REG_XAX
#    define REG2 DR_REG_XDX
#elif defined(AARCHXX)
#    define REG1 DR_REG_R0
#    define REG2 DR_REG_R1
#elif defined(RISCV64)
#    define REG1 DR_REG_A0
#    define REG2 DR_REG_A1
#else
#    error Unsupported arch
#endif

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
                    const std::string &invariant_name) override
    {
        if (!condition) {
            errors.push_back(invariant_name);
            error_tids.push_back(shard->tid_);
            error_refs.push_back(shard->ref_count_);
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
            for (int i = 0; i < checker.errors.size(); ++i) {
                std::cerr << "Unexpected error: " << checker.errors[i]
                          << " at ref: " << checker.error_refs[i] << "\n";
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
    // Positive test: branches with no encodings.
    {
        std::vector<memref_t> memrefs = {
            gen_instr(1, 1),   gen_branch(1, 2),  gen_instr(1, 3), // Not taken.
            gen_branch(1, 4),  gen_instr(1, 101),                  // Taken.
            gen_instr(1, 102),
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
            gen_marker(1, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
#    if defined(X86_64) || defined(X86_32)
            // 0x74 is "je" with the 2nd byte the offset.
            gen_branch_encoded(1, 0x71019dbc, { 0x74, 0x32 }),
            gen_instr_encoded(0x71019ded, { 0x01 }),
#    elif defined(ARM_64)
            // 71019dbc:   540001a1        b.ne    71019df0 <__executable_start+0x19df0>
            gen_branch_encoded(1, 0x71019dbc, 0x540001a1),
            gen_instr_encoded(0x71019ded, 0x01),
#    else
        // TODO i#5871: Add AArch32 (and RISC-V) encodings.
#    endif
        };

        if (!run_checker(memrefs, true, 1, 3,
                         "Direct branch does not go to the correct target",
                         "Failed to catch branch not going to its target")) {
            return false;
        }
    }
    // Positive test: branches with encodings which go to their targets.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(1, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
#    if defined(X86_64) || defined(X86_32)
            // 0x74 is "je" with the 2nd byte the offset.
            gen_branch_encoded(1, 0x71019dbc, { 0x74, 0x32 }),
#    elif defined(ARM_64)
            // 71019dbc:   540001a1        b.ne    71019df0 <__executable_start+0x19df0>
            gen_branch_encoded(1, 0x71019dbc, 0x540001a1),
#    else
        // TODO i#5871: Add AArch32 (and RISC-V) encodings.
#    endif
            gen_instr(1, 0x71019df0),
        };

        if (!run_checker(memrefs, false, 1)) {
            return false;
        }
    }
#endif
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
    // Signal before any instr in the trace.
    {
        std::vector<memref_t> memrefs = {
            // No instr in the beginning here. Should skip pre-signal instr check
            // on return.
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
    // Nested signals without any intervening instr.
    {
        std::vector<memref_t> memrefs = {
            gen_instr(1, 1),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            // No intervening instr here. Should skip pre-signal instr check on
            // return.
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_EVENT, 101),
            gen_instr(1, 201),
            // XXX: This marker value is actually not guaranteed, yet the checker
            // requires it and the view tool prints it.
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_XFER, 202),
            gen_instr(1, 101),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_XFER, 102),
            gen_instr(1, 2),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Nested signals without any intervening instr or initial instr.
    {
        std::vector<memref_t> memrefs = {
            // No initial instr. Should skip pre-signal instr check on return.
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            // No intervening instr here. Should skip pre-signal instr check on
            // return.
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_EVENT, 101),
            gen_instr(1, 201),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_XFER, 202),
            gen_instr(1, 101),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_XFER, 102),
            gen_instr(1, 2),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Consecutive signals (that are nested at the same depth) without any
    // intervening instr between them.
    {
        std::vector<memref_t> memrefs = {
            gen_instr(1, 1),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_instr(1, 101),
            // First signal.
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_EVENT, 102),
            gen_instr(1, 201),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_XFER, 202),
            // Second signal.
            // No intervening instr here. Should use instr at pc = 101 for
            // pre-signal instr check on return.
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_EVENT, 102),
            gen_instr(1, 201),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_XFER, 202),
            gen_instr(1, 102),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_XFER, 103),
            gen_instr(1, 2),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Consecutive signals (that are nested at the same depth) without any
    // intervening instr between them, and no instr before the first of them
    // and its outer signal.
    {
        std::vector<memref_t> memrefs = {
            gen_instr(1, 1),
            // Outer signal.
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            // First signal.
            // No intervening instr here. Should skip pre-signal instr check
            // on return.
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_EVENT, 102),
            gen_instr(1, 201),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_XFER, 202),
            // Second signal.
            // No intervening instr here. Since there's no pre-signal instr
            // for the first signal as well, we did not see any instr at this
            // signal-depth. So the pre-signal check should be skipped on return
            // of this signal too.
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_EVENT, 102),
            gen_instr(1, 201),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_XFER, 202),
            gen_instr(1, 102),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_XFER, 103),
            gen_instr(1, 2),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Trace starts in a signal.
    {
        std::vector<memref_t> memrefs = {
            // Already inside the first signal.
            gen_instr(1, 11),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_XFER, 12),
            // Should skip the pre-signal instr check and the kernel_event marker
            // equality check, since we did not see the beginning of the signal in
            // the trace.
            gen_instr(1, 2),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Trace starts in a signal with a back-to-back signal without any intervening
    // instr after we return from the first one.
    {
        std::vector<memref_t> memrefs = {
            // Already inside the first signal.
            gen_instr(1, 11),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_XFER, 12),
            // No intervening instr here. Should skip pre-signal instr check on
            // return; this is a special case as it would require *removing* the
            // pc = 11 instr from pre_signal_instr_ as it was not in this newly
            // discovered outermost scope.
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_instr(1, 21),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_XFER, 22),
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
            gen_marker(1, TRACE_MARKER_TYPE_RSEQ_ENTRY, 3),
            gen_instr(1, 1),
            // Rolled back instr at pc=2 size=1.
            // Point to the abort handler.
            gen_marker(1, TRACE_MARKER_TYPE_RSEQ_ABORT, 4),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_EVENT, 4),
            gen_instr(1, 4),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    {
        std::vector<memref_t> memrefs = {
            gen_marker(1, TRACE_MARKER_TYPE_RSEQ_ENTRY, 3),
            gen_instr(1, 1),
            gen_instr(1, 2),
            // A fault in the instrumented execution.
            gen_marker(1, TRACE_MARKER_TYPE_RSEQ_ABORT, 2),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_EVENT, 4),
            gen_instr(1, 10),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_XFER, 11),
            gen_instr(1, 4),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Fail to roll back rseq final instr.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(1, TRACE_MARKER_TYPE_RSEQ_ENTRY, 3),
            gen_instr(1, 1),
            gen_instr(1, 2),
            gen_marker(1, TRACE_MARKER_TYPE_RSEQ_ABORT, 4),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_EVENT, 4),
            gen_instr(1, 4),
        };
        if (!run_checker(memrefs, true, 1, 4,
                         "Rseq post-abort instruction not rolled back",
                         "Failed to catch bad rseq abort"))
            return false;
    }
#endif
    return true;
}

bool
check_function_markers()
{
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
        if (!run_checker(memrefs, true, TID, 5,
                         "Function marker misplaced between instr and memref",
                         "Failed to catch misplaced function marker"))
            return false;
    }
    // Incorrectly not after a branch.
    {
        std::vector<memref_t> memrefs = {
            gen_instr(TID, 1),
            gen_marker(TID, TRACE_MARKER_TYPE_FUNC_ID, 2),
        };
        if (!run_checker(memrefs, true, TID, 2,
                         "Function marker should be after a branch",
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
        if (!run_checker(memrefs, true, TID, 3,
                         "Function marker retaddr should match prior call",
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
        if (!run_checker(memrefs, true, TID, 3,
                         "Function marker should be after a branch",
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
    constexpr addr_t ADDR = 0x7fcf3b9dd9e9;
    // Negative: syscalls with the same PC.
#if defined(X86_64) || defined(X86_32) || defined(ARM_64)
    {
        std::vector<memref_t> memrefs = {
            gen_marker(1, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
#    if defined(X86_64) || defined(X86_32)
            gen_instr_encoded(ADDR, { 0x0f, 0x05 }), // 0x7fcf3b9dd9e9: 0f 05 syscall
            gen_marker(1, TRACE_MARKER_TYPE_TIMESTAMP, 0),
            gen_marker(1, TRACE_MARKER_TYPE_CPU_ID, 3),
            gen_instr_encoded(ADDR, { 0x0f, 0x05 }), // 0x7fcf3b9dd9e9: 0f 05 syscall
#    elif defined(ARM_64)
            gen_instr_encoded(ADDR,
                              0xd4000001), // 0x7fcf3b9dd9e9: 0xd4000001 svc #0x0
            gen_marker(1, TRACE_MARKER_TYPE_TIMESTAMP, 0),
            gen_marker(1, TRACE_MARKER_TYPE_CPU_ID, 3),
            gen_instr_encoded(ADDR,
                              0xd4000001), // 0x7fcf3b9dd9e9: 0xd4000001 svc #0x0
#    else
        // TODO i#5871: Add AArch32 (and RISC-V) encodings.
#    endif
        };
        if (!run_checker(memrefs, true, 1, 5, "Duplicate syscall instrs with the same PC",
                         "Failed to catch duplicate syscall instrs with the same PC"))
            return false;
    }

    // Positive test: syscalls with different PCs.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(1, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
#    if defined(X86_64) || defined(X86_32)
            gen_instr_encoded(ADDR, { 0x0f, 0x05 }), // 0x7fcf3b9dd9e9: 0f 05 syscall
            gen_marker(1, TRACE_MARKER_TYPE_TIMESTAMP, 0),
            gen_marker(1, TRACE_MARKER_TYPE_CPU_ID, 3),
            gen_instr_encoded(ADDR + 2, { 0x0f, 0x05 }), // 0x7fcf3b9dd9eb: 0f 05 syscall
#    elif defined(ARM_64)
            gen_instr_encoded(ADDR, 0xd4000001,
                              2), // 0x7fcf3b9dd9e9: 0xd4000001 svc #0x0
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
check_rseq_side_exit_discontinuity()
{
    // Negative test: Missing instructions in a basic block due to rseq side exit.
    instr_t *store = XINST_CREATE_store(GLOBAL_DCONTEXT, OPND_CREATE_MEMPTR(REG2, 0),
                                        opnd_create_reg(REG1));
    instr_t *move1 =
        XINST_CREATE_move(GLOBAL_DCONTEXT, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instr_t *move2 =
        XINST_CREATE_move(GLOBAL_DCONTEXT, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instr_t *jmp =
        XINST_CREATE_jump_cond(GLOBAL_DCONTEXT, DR_PRED_EQ, opnd_create_instr(move2));

    // Create an instrlist_t*.
    instrlist_t *ilist = instrlist_create(GLOBAL_DCONTEXT);
    instrlist_append(ilist, jmp);
    instrlist_append(ilist, store);
    instrlist_append(ilist, move1);
    instrlist_append(ilist, move2);

    // Create a std::vector<memref_instr_t>.
    std::vector<memref_instr_t> memref_instr_vec = { { gen_branch(1), jmp },
                                                     { gen_instr(1), store },
                                                     { gen_data(1, false, 42, 4),
                                                       nullptr },
                                                     { gen_instr(1), move2 } };

    auto memrefs = get_memrefs_from_ir(ilist, memref_instr_vec);
    if (!run_checker(memrefs, true, 1, 5, "PC discontinuity due to rseq side exit",
                     "Failed to catch PC discontinuity from rseq side exit")) {
        return false;
    }
    return true;
}

} // namespace

int
main(int argc, const char *argv[])
{
    if (check_branch_target_after_branch() && check_sane_control_flow() &&
        check_kernel_xfer() && check_rseq() && check_function_markers() &&
        check_duplicate_syscall_with_same_pc() && check_rseq_side_exit_discontinuity()) {
        std::cerr << "invariant_checker_test passed\n";
        return 0;
    }
    std::cerr << "invariant_checker_test FAILED\n";
    exit(1);
}
