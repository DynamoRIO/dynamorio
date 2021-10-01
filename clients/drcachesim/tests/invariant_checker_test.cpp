/* **********************************************************
 * Copyright (c) 2021 Google, LLC  All rights reserved.
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

namespace {

class checker_no_abort_t : public invariant_checker_t {
public:
    checker_no_abort_t(bool offline)
        : invariant_checker_t(offline)
    {
    }
    std::vector<std::string> errors;

protected:
    void
    report_if_false(bool condition, const std::string &message) override
    {
        if (!condition)
            errors.push_back(message);
    }
};

memref_t
gen_instr_type(trace_type_t type, memref_tid_t tid, addr_t pc)
{
    memref_t memref = {};
    memref.instr.type = type;
    memref.instr.tid = tid;
    memref.instr.addr = pc;
    memref.instr.size = 1;
    return memref;
}

memref_t
gen_instr(memref_tid_t tid, addr_t pc)
{
    return gen_instr_type(TRACE_TYPE_INSTR, tid, pc);
}

memref_t
gen_branch(memref_tid_t tid, addr_t pc)
{
    return gen_instr_type(TRACE_TYPE_INSTR_CONDITIONAL_JUMP, tid, pc);
}

memref_t
gen_marker(memref_tid_t tid, trace_marker_type_t type, uintptr_t val)
{
    memref_t memref = {};
    memref.marker.type = TRACE_TYPE_MARKER;
    memref.marker.tid = tid;
    memref.marker.marker_type = type;
    memref.marker.marker_value = val;
    return memref;
}

memref_t
gen_exit(memref_tid_t tid)
{
    memref_t memref = {};
    memref.instr.type = TRACE_TYPE_THREAD_EXIT;
    memref.instr.tid = tid;
    return memref;
}

bool
check_branch_target_after_branch()
{
    // Positive simple test.
    {
        checker_no_abort_t checker(true /*offline*/);
        std::vector<memref_t> memrefs = {
            gen_instr(1, 1), gen_branch(1, 2),
            gen_instr(1, 3), gen_marker(2, TRACE_MARKER_TYPE_TIMESTAMP, 0),
            gen_instr(2, 1),
        };
        for (const auto &memref : memrefs) {
            checker.process_memref(memref);
        }
        for (const auto &error : checker.errors) {
            std::cerr << "Unexpected error: " << error << "\n";
        }
        if (!checker.errors.empty())
            return false;
    }
    // Negative simple test.
    {
        checker_no_abort_t checker(true /*offline*/);
        std::vector<memref_t> memrefs = {
            gen_instr(1, 1),
            gen_branch(1, 2),
            gen_marker(2, TRACE_MARKER_TYPE_TIMESTAMP, 0),
            gen_instr(2, 1),
        };
        for (const auto &memref : memrefs) {
            checker.process_memref(memref);
        }
        if (checker.errors.size() != 1 ||
            checker.errors[0] != "Branch target not immediately after branch") {
            std::cerr << "Failed to catch bad branch target position\n";
            return false;
        }
    }
    // Invariant relaxed for thread exit or signal.
    {
        checker_no_abort_t checker(true /*offline*/);
        std::vector<memref_t> memrefs = {
            gen_marker(3, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_branch(3, 2),
            gen_exit(3),
            gen_instr(1, 1),
            gen_branch(1, 2),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_EVENT, 3),
            gen_marker(2, TRACE_MARKER_TYPE_TIMESTAMP, 0),
            gen_instr(2, 4),
        };
        for (const auto &memref : memrefs) {
            checker.process_memref(memref);
        }
        for (const auto &error : checker.errors) {
            std::cerr << "Unexpected error: " << error << "\n";
        }
        if (!checker.errors.empty())
            return false;
    }
    return true;
}

bool
check_sane_control_flow()
{
    // Positive test: branches.
    {
        checker_no_abort_t checker(true /*offline*/);
        std::vector<memref_t> memrefs = {
            gen_instr(1, 1),   gen_branch(1, 2),  gen_instr(1, 3), // Not taken.
            gen_branch(1, 4),  gen_instr(1, 101),                  // Taken.
            gen_instr(1, 102),
        };
        for (const auto &memref : memrefs) {
            checker.process_memref(memref);
        }
        for (const auto &error : checker.errors) {
            std::cerr << "Unexpected error: " << error << "\n";
        }
        if (!checker.errors.empty())
            return false;
    }
    // Negative simple test.
    {
        checker_no_abort_t checker(true /*offline*/);
        std::vector<memref_t> memrefs = {
            gen_instr(1, 1),
            gen_instr(1, 3),
        };
        for (const auto &memref : memrefs) {
            checker.process_memref(memref);
        }
        if (checker.errors.size() != 1 ||
            checker.errors[0] != "Non-explicit control flow has no marker") {
            std::cerr << "Failed to catch bad control flow\n";
            return false;
        }
    }
    // String loop.
    {
        checker_no_abort_t checker(true /*offline*/);
        std::vector<memref_t> memrefs = {
            gen_instr_type(TRACE_TYPE_INSTR_NO_FETCH, 1, 1),
            gen_instr_type(TRACE_TYPE_INSTR_NO_FETCH, 1, 1),
            gen_instr_type(TRACE_TYPE_INSTR_NO_FETCH, 1, 1),
            gen_instr_type(TRACE_TYPE_INSTR_NO_FETCH, 1, 1),
            gen_instr(1, 2),
        };
        for (const auto &memref : memrefs) {
            checker.process_memref(memref);
        }
        for (const auto &error : checker.errors) {
            std::cerr << "Unexpected error: " << error << "\n";
        }
        if (!checker.errors.empty())
            return false;
    }
    // Kernel-mediated.
    {
        checker_no_abort_t checker(true /*offline*/);
        std::vector<memref_t> memrefs = {
            gen_instr(1, 1),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_instr(1, 101),
        };
        for (const auto &memref : memrefs) {
            checker.process_memref(memref);
        }
        for (const auto &error : checker.errors) {
            std::cerr << "Unexpected error: " << error << "\n";
        }
        if (!checker.errors.empty())
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
        checker_no_abort_t checker(true /*offline*/);
        std::vector<memref_t> memrefs = {
            gen_instr(1, 1),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_instr(1, 101),
            // XXX: This marker value is actually not guaranteed, yet the checker
            // requires it and the view tool prints it.
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_XFER, 102),
            gen_instr(1, 2),
        };
        for (const auto &memref : memrefs) {
            checker.process_memref(memref);
        }
        for (const auto &error : checker.errors) {
            std::cerr << "Unexpected error: " << error << "\n";
        }
        if (!checker.errors.empty())
            return false;
    }
    // Fail to return to recorded interruption point.
    {
        checker_no_abort_t checker(true /*offline*/);
        std::vector<memref_t> memrefs = {
            gen_instr(1, 1),   gen_marker(1, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_instr(1, 101), gen_marker(1, TRACE_MARKER_TYPE_KERNEL_XFER, 102),
            gen_instr(1, 3),
        };
        for (const auto &memref : memrefs) {
            checker.process_memref(memref);
        }
        if (checker.errors.size() != 1 ||
            checker.errors[0] != "Signal handler return point incorrect") {
            std::cerr << "Failed to catch bad signal handler return\n";
            return false;
        }
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
        checker_no_abort_t checker(true /*offline*/);
        std::vector<memref_t> memrefs = {
            gen_instr(1, 1),
            gen_instr(1, 2),
            gen_marker(1, TRACE_MARKER_TYPE_RSEQ_ABORT, 1),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_EVENT, 1),
            gen_instr(1, 1),
        };
        for (const auto &memref : memrefs) {
            checker.process_memref(memref);
        }
        for (const auto &error : checker.errors) {
            std::cerr << "Unexpected error: " << error << "\n";
        }
        if (!checker.errors.empty())
            return false;
    }
    // Fail to roll back rseq final instr.
    {
        checker_no_abort_t checker(true /*offline*/);
        std::vector<memref_t> memrefs = {
            gen_instr(1, 1),
            gen_instr(1, 2),
            gen_marker(1, TRACE_MARKER_TYPE_RSEQ_ABORT, 2),
            gen_marker(1, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_instr(1, 1),
        };
        for (const auto &memref : memrefs) {
            checker.process_memref(memref);
        }
        if (checker.errors.size() != 1 ||
            checker.errors[0] != "Rseq post-abort instruction not rolled back") {
            std::cerr << "Failed to catch bad rseq abort\n";
            return false;
        }
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
