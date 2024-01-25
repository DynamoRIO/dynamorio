/* **********************************************************
 * Copyright (c) 2021-2024 Google, LLC  All rights reserved.
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
    checker_no_abort_t(bool offline, bool serial, std::istream *serial_schedule_file)
        : invariant_checker_t(offline, 1, "invariant_checker_test", serial_schedule_file)
        , serial_(serial)
    {
    }
    std::vector<error_info_t> errors_;

    bool
    print_results() override
    {
        if (serial_) {
            for (const auto &keyval : shard_map_) {
                parallel_shard_exit(keyval.second.get());
            }
        }
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
    bool serial_ = false;
};

/* Assumes there are at most 3 threads with tids 1, 2, and 3 in memrefs. */
static constexpr memref_tid_t TID_BASE = 1;
static constexpr memref_tid_t TID_A = TID_BASE;
static constexpr memref_tid_t TID_B = TID_BASE + 1;
static constexpr memref_tid_t TID_C = TID_BASE + 2;
bool
run_checker(const std::vector<memref_t> &memrefs, bool expect_error,
            const error_info_t &expected_error_info = {},
            const std::string &toprint_if_fail = "",
            std::istream *serial_schedule_file = nullptr)
{
    // Serial.
    {
        checker_no_abort_t checker(/*offline=*/true, /*serial=*/true,
                                   serial_schedule_file);
        default_memtrace_stream_t stream;
        checker.initialize_stream(&stream);
        for (const auto &memref : memrefs) {
            int shard_index = static_cast<int>(memref.instr.tid - TID_BASE);
            stream.set_tid(memref.instr.tid);
            stream.set_shard_index(shard_index);
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
        checker_no_abort_t checker(/*offline=*/true, /*serial=*/false,
                                   serial_schedule_file);
        default_memtrace_stream_t stream;
        checker.initialize_stream(&stream);
        void *shardA = nullptr, *shardB = nullptr, *shardC = nullptr;
        for (const auto &memref : memrefs) {
            int shard_index = static_cast<int>(memref.instr.tid - TID_BASE);
            stream.set_tid(memref.instr.tid);
            stream.set_shard_index(shard_index);
            switch (memref.instr.tid) {
            case TID_A:
                if (shardA == nullptr) {
                    shardA =
                        checker.parallel_shard_init_stream(shard_index, nullptr, &stream);
                }
                checker.parallel_shard_memref(shardA, memref);
                break;
            case TID_B:
                if (shardB == nullptr) {
                    shardB =
                        checker.parallel_shard_init_stream(shard_index, nullptr, &stream);
                }
                checker.parallel_shard_memref(shardB, memref);
                break;
            case TID_C:
                if (shardC == nullptr) {
                    shardC =
                        checker.parallel_shard_init_stream(shard_index, nullptr, &stream);
                }
                checker.parallel_shard_memref(shardC, memref);
                break;
            default: std::cerr << "Internal test error: unknown tid\n"; return false;
            }
        }
        if (shardA != nullptr)
            checker.parallel_shard_exit(shardA);
        if (shardB != nullptr)
            checker.parallel_shard_exit(shardB);
        if (shardC != nullptr)
            checker.parallel_shard_exit(shardC);
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
    // Correct simple test.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_B, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_B, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, 1),
            gen_branch(TID_A, 2),
            gen_instr(TID_A, 3),
            gen_marker(TID_B, TRACE_MARKER_TYPE_TIMESTAMP, 0),
            gen_instr(TID_B, 1),
            gen_exit(TID_A),
            gen_exit(TID_B)
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Incorrect simple test.
    {
        constexpr uintptr_t TIMESTAMP = 3;
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_B, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_B, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, 1),
            gen_branch(TID_A, 2),
            gen_marker(TID_B, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP),
            gen_instr(TID_B, 1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP),
            gen_instr(TID_A, 3),
            gen_exit(TID_A),
            gen_exit(TID_B)
        };
        if (!run_checker(memrefs, true,
                         { "Branch target not immediately after branch", TID_A,
                           /*ref_ordinal=*/6, /*last_timestamp=*/TIMESTAMP,
                           /*instrs_since_last_timestamp=*/1 },
                         "Failed to catch bad branch target position")) {
            return false;
        }
    }
    // Invariant relaxed for thread exit or signal.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_B, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_B, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_C, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_C, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_branch(TID_C, 2),
            gen_exit(TID_C),
            gen_instr(TID_A, 1),
            gen_branch(TID_A, 2),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 3),
            gen_marker(TID_B, TRACE_MARKER_TYPE_TIMESTAMP, 0),
            gen_instr(TID_B, 4),
            gen_exit(TID_A),
            gen_exit(TID_B)
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
    // Incorrect simple test.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), gen_instr(TID_A, 1),
            gen_instr(TID_A, 3), gen_exit(TID_A)
        };
        if (!run_checker(memrefs, true,
                         { "Non-explicit control flow has no marker", TID_A,
                           /*ref_ordinal=*/4, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/2 },
                         "Failed to catch bad control flow"))
            return false;
    }
    // Incorrect test with timestamp markers.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 2),
            gen_instr(TID_A, 1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 3),
            gen_instr(TID_A, 3),
            gen_exit(TID_A)
        };
        if (!run_checker(memrefs, true,
                         { "Non-explicit control flow has no marker", TID_A,
                           /*ref_ordinal=*/6, /*last_timestamp=*/3,
                           /*instrs_since_last_timestamp=*/1 },
                         "Failed to catch bad control flow")) {
            return false;
        }
    }
    // Correct test: branches with no encodings.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, 1),
            gen_branch(TID_A, 2),
            gen_instr(TID_A, 3), // Not taken.
            gen_branch(TID_A, 4),
            gen_instr(TID_A, 101), // Taken.
            gen_instr(TID_A, 102),
            gen_exit(TID_A)
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Tests with encodings:
    // We use these client defines which are the target and so drdecode's target arch.
#if defined(X86_64) || defined(X86_32) || defined(ARM_64)
    // XXX: We hardcode encodings here.  If we need many more we should generate them
    // from DR IR.

    // Incorrect test: branches with encodings which do not go to their targets.
    {
        instr_t *move1 = XINST_CREATE_move(GLOBAL_DCONTEXT, opnd_create_reg(REG1),
                                           opnd_create_reg(REG2));
        instr_t *move2 = XINST_CREATE_move(GLOBAL_DCONTEXT, opnd_create_reg(REG1),
                                           opnd_create_reg(REG2));
        instr_t *cond_jmp =
            XINST_CREATE_jump_cond(GLOBAL_DCONTEXT, DR_PRED_EQ, opnd_create_instr(move1));

        instrlist_t *ilist = instrlist_create(GLOBAL_DCONTEXT);
        instrlist_append(ilist, cond_jmp);
        instrlist_append(ilist, move1);
        instrlist_append(ilist, move2);

        std::vector<memref_with_IR_t> memref_instr_vec = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_branch(TID_A), cond_jmp },
            { gen_instr(TID_A), move2 },
            { gen_exit(TID_A), nullptr }
        };
        static constexpr addr_t BASE_ADDR = 0xeba4ad4;
        auto memrefs = add_encodings_to_memrefs(ilist, memref_instr_vec, BASE_ADDR);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
        if (!run_checker(memrefs, true,
                         { "Branch does not go to the correct target", TID_A,
                           /*ref_ordinal=*/5, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/2 },
                         "Failed to catch branch not going to its target")) {
            return false;
        }
    }
    // Correct test: branches with encodings which go to their targets.
    {
        instr_t *move1 = XINST_CREATE_move(GLOBAL_DCONTEXT, opnd_create_reg(REG1),
                                           opnd_create_reg(REG2));
        instr_t *move2 = XINST_CREATE_move(GLOBAL_DCONTEXT, opnd_create_reg(REG1),
                                           opnd_create_reg(REG2));
        instr_t *cond_jmp =
            XINST_CREATE_jump_cond(GLOBAL_DCONTEXT, DR_PRED_EQ, opnd_create_instr(move1));

        instrlist_t *ilist = instrlist_create(GLOBAL_DCONTEXT);
        instrlist_append(ilist, cond_jmp);
        instrlist_append(ilist, move1);
        instrlist_append(ilist, move2);

        std::vector<memref_with_IR_t> memref_instr_vec = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_branch(TID_A), cond_jmp },
            { gen_instr(TID_A), move1 },
            { gen_exit(TID_A), nullptr }
        };
        static constexpr addr_t BASE_ADDR = 0xeba4ad4;
        auto memrefs = add_encodings_to_memrefs(ilist, memref_instr_vec, BASE_ADDR);
        if (!run_checker(memrefs, false)) {
            return false;
        }
    }
#endif
    // String loop.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr_type(TRACE_TYPE_INSTR_NO_FETCH, TID_A, 1),
            gen_instr_type(TRACE_TYPE_INSTR_NO_FETCH, TID_A, 1),
            gen_instr_type(TRACE_TYPE_INSTR_NO_FETCH, TID_A, 1),
            gen_instr_type(TRACE_TYPE_INSTR_NO_FETCH, TID_A, 1),
            gen_instr(TID_A, 2),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Kernel-mediated.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, /*pc=*/1, /*size=*/1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_instr(TID_A, /*pc=*/101, /*size=*/1),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
#ifdef UNIX
    // Incorrect test (PC discontinuity): Transition from instr to kernel_xfer event
    // marker.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, /*pc=*/1, /*size=*/1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 3),
            gen_exit(TID_A),
        };
        if (!run_checker(
                memrefs, true,
                { "Non-explicit control flow has no marker @ kernel_event marker", TID_A,
                  /*ref_ordinal=*/4, /*last_timestamp=*/0,
                  /*instrs_since_last_timestamp=*/1 },
                "Failed to catch PC discontinuity for an instruction followed by "
                "kernel xfer marker")) {
            return false;
        }
    }
    // Correct test: Transition from instr to kernel_xfer event marker, goes to the next
    // instruction.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, /*pc=*/1, /*size=*/1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_instr(TID_A, /*pc=*/101, /*size=*/1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, 102),
            gen_instr(TID_A, /*pc=*/2, /*size=*/1),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false)) {
            return false;
        }
    }
    // Correct test: We should skip the check if there is no instruction before the
    // kernel event.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 3),
            gen_instr(TID_A),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false)) {
            return false;
        }
    }
    // Correct test: Pre-signal instr continues after signal.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, /*pc=*/2, /*size=*/1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_instr(TID_A, /*pc=*/101, /*size=*/1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, 102),
            gen_instr(TID_A, /*pc=*/2, /*size=*/1),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false)) {
            return false;
        }
    }
    // Correct test: We should not report a PC discontinuity when the previous instr is
    // of type TRACE_TYPE_INSTR_SYSENTER.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, /*pc=*/5, /*size=*/1),
            gen_instr_type(TRACE_TYPE_INSTR_SYSENTER, TID_A, /*pc=*/6, /*size=*/1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 2),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, 3),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_instr(TID_A, /*pc=*/101, /*size=*/1),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false)) {
            return false;
        }
    }
    // Correct test: RSEQ abort in last signal context.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, /*pc=*/1, /*size=*/1),
            // The RSEQ_ABORT marker is always follwed by a KERNEL_EVENT marker.
            gen_marker(TID_A, TRACE_MARKER_TYPE_RSEQ_ABORT, 40),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 40),
            // We get a signal after the RSEQ abort.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 4),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false)) {
            return false;
        }
    }
    // Correct test: Branch before signal. This is correct only because the branch doesn't
    // have an encoding with it. This case will only occur in legacy or stripped traces.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, /*pc=*/1, /*size=*/1),
            gen_branch(TID_A, 2),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 50),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false)) {
            return false;
        }
    }
    // Correct test: back-to-back signals without any intervening instruction.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, /*pc=*/101, /*size=*/1),
            // First signal.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 102),
            gen_instr(TID_A, /*pc=*/201, /*size=*/1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, 202),
            // Second signal.
            // The Marker value for this signal needs to be 102.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 102),
            gen_instr(TID_A, /*pc=*/201, /*size=*/1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, 202),
            gen_instr(TID_A, /*pc=*/102, /*size=*/1),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false)) {
            return false;
        }
    }
    // Correct test: back-to-back signals after an RSEQ abort.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE,
                       OFFLINE_FILE_TYPE_SYSCALL_NUMBERS),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, /*pc=*/101, /*size=*/1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_RSEQ_ABORT, 102),
            // This is the signal which caused the RSEQ abort.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 102),
            // We get a signal after the RSEQ abort.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 301),
            gen_instr(TID_A, /*pc=*/201, /*size=*/1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 15),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, 202),
            // The kernel event marker has the same value as the previous one.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 301),
            gen_instr(TID_A, /*pc=*/201, /*size=*/1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, 202),
            gen_instr(TID_A, /*pc=*/301, /*size=*/1),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false)) {
            return false;
        }
    }
    // Incorrect test: back-to-back signals with an intervening instruction after an RSEQ
    // abort.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE,
                       OFFLINE_FILE_TYPE_SYSCALL_NUMBERS),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, /*pc=*/101, /*size=*/1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_RSEQ_ABORT, 102),
            // This is the signal which caused the RSEQ abort.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 102),
            // We get a signal after the RSEQ abort.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 301),
            gen_instr(TID_A, /*pc=*/201, /*size=*/1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 15),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, 202),
            gen_instr(TID_A, /*pc=*/301, /*size=*/1),
            gen_instr(TID_A, /*pc=*/302, /*size=*/1),
            // The kernel event marker should point to the previous instruction
            // at PC 302, instead of 301.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 301),
            gen_exit(TID_A),
        };
        if (!run_checker(
                memrefs, true,
                { "Non-explicit control flow has no marker @ kernel_event marker", TID_A,
                  /*ref_ordinal=*/13, /*last_timestamp=*/0,
                  /*instrs_since_last_timestamp=*/4 },
                "Failed to catch PC discontinuity for an instruction followed by "
                "kernel xfer marker")) {
            return false;
        }
    }
    // Incorrect test: back-to-back signals without any intervening instruction.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, /*pc=*/101, /*size=*/1),
            // First signal.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 102),
            gen_instr(TID_A, /*pc=*/201, /*size=*/1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, 202),
            // Second signal.
            // There will be a PC discontinuity here since the marker value is 500, and
            // the previous PC is 101.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 500),
            gen_instr(TID_A, /*pc=*/201, /*size=*/1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, 202),
            gen_exit(TID_A),
        };
        if (!run_checker(
                memrefs, true,
                { "Non-explicit control flow has no marker @ kernel_event marker", TID_A,
                  /*ref_ordinal=*/7, /*last_timestamp=*/0,
                  /*instrs_since_last_timestamp=*/2 },
                "Failed to catch PC discontinuity for back-to-back signals without any "
                "intervening instruction")) {
            return false;
        }
    }
    // Correct test: Taken branch with signal in between branch and its target.
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
        static constexpr uintptr_t WILL_BE_REPLACED = 0;
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_VERSION,
                         TRACE_ENTRY_VERSION_BRANCH_INFO),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr_type(TRACE_TYPE_INSTR_TAKEN_JUMP, TID_A), cbr_to_move },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, WILL_BE_REPLACED), move },
            /* TODO i#6316: The nop PC is incorrect. We need to add a check for equality
               beteen the KERNEL_XFER marker and the prev instr fall-through. */
            { gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, WILL_BE_REPLACED), nop },
            { gen_instr(TID_A), move },
            { gen_exit(TID_A), nullptr },
        };
        std::vector<memref_t> memrefs =
            add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
        if (!run_checker(memrefs, false)) {
            return false;
        }
    }
    // Incorrect test: Taken branch with signal in between branch and its target. Return
    // to the wrong place after the signal.
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
        static constexpr uintptr_t WILL_BE_REPLACED = 0;
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_VERSION,
                         TRACE_ENTRY_VERSION_BRANCH_INFO),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr_type(TRACE_TYPE_INSTR_TAKEN_JUMP, TID_A), cbr_to_move },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, WILL_BE_REPLACED), move },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, WILL_BE_REPLACED), nop },
            { gen_instr(TID_A), nop },
            { gen_exit(TID_A), nullptr },
        };
        std::vector<memref_t> memrefs =
            add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
        if (!run_checker(memrefs, true,
                         { "Signal handler return point incorrect", TID_A,
                           /*ref_ordinal=*/8, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/2 },
                         "Failed to catch bad signal handler return")) {
            return false;
        }
    }

#endif
    return true;
}

bool
check_kernel_xfer()
{
#ifdef UNIX
    std::cerr << "Testing kernel xfers\n";
    // Return to recorded interruption point.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, 1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_instr(TID_A, 101),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, 102),
            gen_instr(TID_A, 2),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Signal before any instr in the trace.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            // No instr in the beginning here. Should skip pre-signal instr check
            // on return.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_instr(TID_A, 101),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, 102),
            gen_instr(TID_A, 2),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Nested signals without any intervening instr.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, 1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            // No intervening instr here. Should skip pre-signal instr check on
            // return.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 101),
            gen_instr(TID_A, 201),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, 202),
            gen_instr(TID_A, 101),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, 102),
            gen_instr(TID_A, 2),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Nested signals without any intervening instr or initial instr.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            // No initial instr. Should skip pre-signal instr check on return.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            // No intervening instr here. Should skip pre-signal instr check on
            // return.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 101),
            gen_instr(TID_A, 201),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, 202),
            gen_instr(TID_A, 101),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, 102),
            gen_instr(TID_A, 2),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Consecutive signals (that are nested at the same depth) without any
    // intervening instr between them.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, 1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_instr(TID_A, 101),
            // First signal.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 102),
            gen_instr(TID_A, 201),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, 202),
            // Second signal.
            // No intervening instr here. Should use instr at pc = 101 for
            // pre-signal instr check on return.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 102),
            gen_instr(TID_A, 201),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, 202),
            gen_instr(TID_A, 102),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, 103),
            gen_instr(TID_A, 2),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Consecutive signals (that are nested at the same depth) without any
    // intervening instr between them, and no instr before the first of them
    // and its outer signal.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, 1),
            // Outer signal.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            // First signal.
            // No intervening instr here. Should skip pre-signal instr check
            // on return.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 102),
            gen_instr(TID_A, 201),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, 202),
            // Second signal.
            // No intervening instr here. Since there's no pre-signal instr
            // for the first signal as well, we did not see any instr at this
            // signal-depth. So the pre-signal check should be skipped on return
            // of this signal too.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 102),
            gen_instr(TID_A, 201),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, 202),
            gen_instr(TID_A, 102),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, 103),
            gen_instr(TID_A, 2),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Trace starts in a signal.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            // Already inside the first signal.
            gen_instr(TID_A, 11),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, 12),
            // Should skip the pre-signal instr check and the kernel_event marker
            // equality check, since we did not see the beginning of the signal in
            // the trace.
            gen_instr(TID_A, 2),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Trace starts in a signal with a back-to-back signal without any intervening
    // instr after we return from the first one.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            // Already inside the first signal.
            gen_instr(TID_A, 11),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, 12),
            // No intervening instr here. Should skip pre-signal instr check on
            // return; this is a special case as it would require *removing* the
            // pc = 11 instr from pre_signal_instr_ as it was not in this newly
            // discovered outermost scope.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_instr(TID_A, 21),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, 22),
            gen_instr(TID_A, 2),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Fail to return to recorded interruption point.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, 1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_instr(TID_A, 101),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, 102),
            gen_instr(TID_A, 3),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, true,
                         { "Signal handler return point incorrect", TID_A,
                           /*ref_ordinal=*/7, /*last_timestamp=*/0,
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
    // Roll back rseq final instr.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_A, TRACE_MARKER_TYPE_RSEQ_ENTRY, 3),
            gen_instr(TID_A, 1),
            // Rolled back instr at pc=2 size=1.
            // Point to the abort handler.
            gen_marker(TID_A, TRACE_MARKER_TYPE_RSEQ_ABORT, 4),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 4),
            gen_instr(TID_A, 4),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_A, TRACE_MARKER_TYPE_RSEQ_ENTRY, 3),
            gen_instr(TID_A, 1),
            gen_instr(TID_A, 2),
            // A fault in the instrumented execution.
            gen_marker(TID_A, TRACE_MARKER_TYPE_RSEQ_ABORT, 2),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 4),
            gen_instr(TID_A, 10),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, 11),
            gen_instr(TID_A, 4),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Fail to roll back rseq final instr.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_A, TRACE_MARKER_TYPE_RSEQ_ENTRY, 3),
            gen_instr(TID_A, 1),
            gen_instr(TID_A, 2),
            gen_marker(TID_A, TRACE_MARKER_TYPE_RSEQ_ABORT, 4),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 4),
            gen_instr(TID_A, 4),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, true,
                         { "Rseq post-abort instruction not rolled back", TID_A,
                           /*ref_ordinal=*/6, /*last_timestamp=*/0,
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
    constexpr addr_t CALL_PC = 2;
    constexpr size_t CALL_SZ = 2;
    // Incorrectly between instr and memref.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr_type(TRACE_TYPE_INSTR_DIRECT_CALL, TID_A, CALL_PC, CALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ID, 2),
            // There should be just one error.
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_RETADDR, CALL_PC + CALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ARG, 2),
            gen_data(TID_A, true, 42, 8),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, true,
                         { "Function marker misplaced between instr and memref", TID_A,
                           /*ref_ordinal=*/7, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/1 },
                         "Failed to catch misplaced function marker"))
            return false;
    }
    // Incorrectly not after a branch.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, 1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ID, 2),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, true,
                         { "Function marker should be after a branch", TID_A,
                           /*ref_ordinal=*/4, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/1 },
                         "Failed to catch function marker not after branch"))
            return false;
    }
    // Incorrect return address.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr_type(TRACE_TYPE_INSTR_DIRECT_CALL, TID_A, CALL_PC, CALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ID, 2),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_RETADDR, CALL_PC + CALL_SZ + 1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ARG, 2),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, true,
                         { "Function marker retaddr should match prior call", TID_A,
                           /*ref_ordinal=*/5, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/1 },
                         "Failed to catch wrong function retaddr"))
            return false;
    }
    // Incorrectly not after a branch with a load in between.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, 1),
            gen_data(TID_A, true, 42, 8),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ID, 2),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, true,
                         { "Function marker should be after a branch", TID_A,
                           /*ref_ordinal=*/5, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/1 },
                         "Failed to catch function marker after non-branch with load"))
            return false;
    }
    // Correctly after a branch.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, 1),
            gen_instr_type(TRACE_TYPE_INSTR_DIRECT_CALL, TID_A, CALL_PC, CALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ID, 2),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_RETADDR, CALL_PC + CALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ARG, 2),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Correctly after a branch with memref for the branch.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, 1),
            gen_instr_type(TRACE_TYPE_INSTR_DIRECT_CALL, TID_A, CALL_PC, CALL_SZ),
            gen_instr_type(TRACE_TYPE_INSTR_INDIRECT_JUMP, TID_A, 3),
            gen_data(TID_A, true, 42, 8),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ID, 2),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_RETADDR, CALL_PC + CALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ARG, 2),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Correctly at the beginning of the trace.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ID, 2),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_RETADDR, CALL_PC + CALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ARG, 2),
            gen_instr(TID_A),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Correctly skip return address check when the return address is
    // unavailable.
    {
        constexpr addr_t JUMP_PC = 2;
        constexpr size_t JUMP_SZ = 2;
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, 1),
            gen_instr_type(TRACE_TYPE_INSTR_DIRECT_JUMP, TID_A, JUMP_PC, JUMP_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ID, 2),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_RETADDR, /*pc=*/123456),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Correctly handle nested function calls including tailcalls.
    {
        constexpr addr_t BASE_PC = 100;
        constexpr addr_t FUNC1_PC = 200;
        constexpr addr_t FUNC2_PC = 300;
        constexpr size_t INSTR_SZ = 8;
        constexpr size_t RETURN_SZ = 3;

        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            // The sequence is based on the following functions:
            // BASE_PC:
            //   call FUNC1_PC
            // ..
            // FUNC1_PC:
            //   call FUNC2_PC
            //   xx
            //   jz FUNC1_PC
            // ...
            // FUNC2_PC:
            //   xx
            //   ret
            //
            // Call function 1.
            gen_instr_type(TRACE_TYPE_INSTR_DIRECT_CALL, TID_A, BASE_PC, CALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ID, 1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_RETADDR, BASE_PC + CALL_SZ),

            // Call function 2.
            gen_instr_type(TRACE_TYPE_INSTR_DIRECT_CALL, TID_A, FUNC1_PC, CALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ID, 2),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_RETADDR, FUNC1_PC + CALL_SZ),

            gen_instr(TID_A, FUNC2_PC, INSTR_SZ),
            // Return from function 2.
            gen_instr_type(TRACE_TYPE_INSTR_RETURN, TID_A, FUNC2_PC + INSTR_SZ,
                           RETURN_SZ),

            gen_instr(TID_A, FUNC1_PC + CALL_SZ, INSTR_SZ),
            // A tail recursion that jumps back to the beginning of function 1.
            gen_instr_type(TRACE_TYPE_INSTR_TAKEN_JUMP, TID_A,
                           FUNC1_PC + CALL_SZ + INSTR_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ID, 1),
            // The return address should be the same as the return address of
            // the original call to function 1.
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_RETADDR, BASE_PC + CALL_SZ),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Correctly handle kernel transfer, sigreturn, nested function calls
    // including tailcalls.
    {
        constexpr addr_t BASE_PC = 100;
        constexpr addr_t FUNC1_PC = 200;
        constexpr addr_t FUNC2_PC = 300;
        constexpr addr_t SIG_HANDLER_PC = 400;
        constexpr addr_t SYSCALL_PC = 500;
        constexpr size_t RETURN_SZ = 3;
        constexpr size_t SYSCALL_SZ = 2;

        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE,
                       OFFLINE_FILE_TYPE_SYSCALL_NUMBERS),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, BASE_PC, 1),
            // kernel xfer.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, BASE_PC + 1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 6),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, 3),
            // Call function 1.
            gen_instr_type(TRACE_TYPE_INSTR_DIRECT_CALL, TID_A, SIG_HANDLER_PC, CALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ID, 1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_RETADDR, SIG_HANDLER_PC + CALL_SZ),
            // Call function 2.
            gen_instr_type(TRACE_TYPE_INSTR_DIRECT_CALL, TID_A, FUNC1_PC, CALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ID, 2),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_RETADDR, FUNC1_PC + CALL_SZ),
            // Return from function 2.
            gen_instr_type(TRACE_TYPE_INSTR_RETURN, TID_A, FUNC2_PC, RETURN_SZ),
            // Return from function 1.
            gen_instr_type(TRACE_TYPE_INSTR_RETURN, TID_A, FUNC1_PC + CALL_SZ, RETURN_SZ),
            // Return from the signal handler.
            gen_instr_type(TRACE_TYPE_INSTR_RETURN, TID_A, SIG_HANDLER_PC + CALL_SZ,
                           RETURN_SZ),
            // Sigreturn.
            gen_instr(TID_A, SYSCALL_PC, SYSCALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 15),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 16),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, 3),
            // Syscall xfer.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, SYSCALL_PC + SYSCALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 17),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, 3),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Correctly handle nested signals without any intervening instr.
    {
        constexpr addr_t BASE_PC = 100;
        constexpr addr_t SIG1_PC = 200;
        constexpr addr_t SIG2_PC = 300;
        constexpr size_t INSTR_SZ = 1;
        constexpr size_t RETURN_SZ = 3;
        constexpr size_t SYSCALL_SZ = 2;

        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE,
                       OFFLINE_FILE_TYPE_SYSCALL_NUMBERS),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, BASE_PC, INSTR_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, BASE_PC + INSTR_SZ),
            // No intervening instr here. Should skip pre-signal instr check on
            // return.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, SIG1_PC),
            gen_instr(TID_A, SIG2_PC, INSTR_SZ),
            gen_instr_type(TRACE_TYPE_INSTR_RETURN, TID_A, SIG2_PC + INSTR_SZ, RETURN_SZ),
            // Sigreturn.
            gen_instr(TID_A, SIG2_PC + INSTR_SZ + RETURN_SZ, SYSCALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 15),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER,
                       SIG2_PC + INSTR_SZ + RETURN_SZ + SYSCALL_SZ),
            gen_instr(TID_A, SIG1_PC, INSTR_SZ),
            gen_instr_type(TRACE_TYPE_INSTR_RETURN, TID_A, SIG1_PC + INSTR_SZ, RETURN_SZ),
            // Sigreturn.
            gen_instr(TID_A, SIG1_PC + INSTR_SZ + RETURN_SZ, SYSCALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 15),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER,
                       SIG1_PC + INSTR_SZ + RETURN_SZ + SYSCALL_SZ),
            gen_instr(TID_A, BASE_PC + INSTR_SZ, INSTR_SZ),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Correctly handle consecutive signals (that are nested at the same depth) without
    // any intervening instr between them.
    {
        constexpr addr_t BASE_PC = 100;
        constexpr addr_t SIG1_PC = 200;
        constexpr addr_t SIG2_PC = 300;
        constexpr size_t INSTR_SZ = 1;
        constexpr size_t RETURN_SZ = 3;
        constexpr size_t SYSCALL_SZ = 2;

        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE,
                       OFFLINE_FILE_TYPE_SYSCALL_NUMBERS),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, BASE_PC, INSTR_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, BASE_PC + INSTR_SZ),
            gen_instr(TID_A, SIG1_PC, INSTR_SZ),
            // First signal.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, SIG1_PC + INSTR_SZ),
            gen_instr(TID_A, SIG2_PC, INSTR_SZ),
            gen_instr_type(TRACE_TYPE_INSTR_RETURN, TID_A, SIG2_PC + INSTR_SZ, RETURN_SZ),
            // Sigreturn.
            gen_instr(TID_A, SIG2_PC + INSTR_SZ + RETURN_SZ, SYSCALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 15),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER,
                       SIG2_PC + INSTR_SZ + RETURN_SZ + SYSCALL_SZ),
            // Second signal.
            // No intervening instr here. Should use instr at pc = 101 for
            // pre-signal instr check on return.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, SIG1_PC + INSTR_SZ),
            gen_instr(TID_A, SIG2_PC, INSTR_SZ),
            gen_instr_type(TRACE_TYPE_INSTR_RETURN, TID_A, SIG2_PC + INSTR_SZ, RETURN_SZ),
            // Sigreturn.
            gen_instr(TID_A, SIG2_PC + INSTR_SZ + RETURN_SZ, SYSCALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 15),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER,
                       SIG2_PC + INSTR_SZ + RETURN_SZ + SYSCALL_SZ),

            gen_instr(TID_A, SIG1_PC + INSTR_SZ, INSTR_SZ),
            gen_instr_type(TRACE_TYPE_INSTR_RETURN, TID_A, SIG1_PC + INSTR_SZ * 2,
                           RETURN_SZ),
            // Sigreturn.
            gen_instr(TID_A, SIG1_PC + INSTR_SZ + RETURN_SZ, SYSCALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 15),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER,
                       SIG1_PC + INSTR_SZ + INSTR_SZ),
            gen_instr(TID_A, BASE_PC + INSTR_SZ, INSTR_SZ),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Correctly handle rseq abort.
    {
        constexpr addr_t BASE_PC = 100;
        constexpr addr_t FUNC_PC = 200;
        constexpr size_t INSTR_SZ = 8;
        constexpr size_t ABORT_HANDLER_OFFSET = 10;

        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            // Call function 1.
            gen_instr_type(TRACE_TYPE_INSTR_DIRECT_CALL, TID_A, BASE_PC, CALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ID, 1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_RETADDR, BASE_PC + CALL_SZ),

            gen_instr(TID_A, FUNC_PC, INSTR_SZ),
            // Rolled back instr at pc=FUNC_PC+INSTR_SZ size=CALL_SZ.
            // Point to the abort handler.
            gen_marker(TID_A, TRACE_MARKER_TYPE_RSEQ_ABORT,
                       FUNC_PC + ABORT_HANDLER_OFFSET),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT,
                       FUNC_PC + ABORT_HANDLER_OFFSET),
            gen_instr(TID_A, FUNC_PC + ABORT_HANDLER_OFFSET, INSTR_SZ),

            // A tail recursion that jumps back to the beginning of function 1.
            gen_instr_type(TRACE_TYPE_INSTR_TAKEN_JUMP, TID_A,
                           FUNC_PC + ABORT_HANDLER_OFFSET + INSTR_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ID, 1),
            // The return address should be the same as the return address of
            // the original call to function 1.
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_RETADDR, BASE_PC + CALL_SZ),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
#ifdef UNIX
    // Correctly handle signal arriving between a branch instruction and the function
    // entry.
    {
        constexpr addr_t SIG_HANDLER_PC = 400;
        constexpr addr_t SYSCALL_PC = 500;
        constexpr addr_t FUNC_PC = 200;
        constexpr size_t SYSCALL_SZ = 2;
        constexpr size_t RETURN_SZ = 3;

        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE,
                       OFFLINE_FILE_TYPE_SYSCALL_NUMBERS),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, 1),
            gen_instr_type(TRACE_TYPE_INSTR_DIRECT_CALL, TID_A, CALL_PC, CALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, FUNC_PC),
            gen_instr(TID_A, SIG_HANDLER_PC),
            // Return from the signal handler.
            gen_instr_type(TRACE_TYPE_INSTR_RETURN, TID_A, SIG_HANDLER_PC + 1, RETURN_SZ),
            // Sigreturn.
            gen_instr(TID_A, SYSCALL_PC, SYSCALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 15),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 16),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, 3),
            // Syscall xfer.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, SYSCALL_PC + SYSCALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 17),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, 3),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ID, 2),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_RETADDR, CALL_PC + CALL_SZ),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Correctly handle function markers without the corresponding kernel xfer
    // marker.
    {
        constexpr addr_t SIG_HANDLER_PC = 400;
        constexpr addr_t SYSCALL_PC = 500;
        constexpr size_t SYSCALL_SZ = 2;
        constexpr size_t RETURN_SZ = 3;

        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE,
                       OFFLINE_FILE_TYPE_SYSCALL_NUMBERS),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, SIG_HANDLER_PC),
            // Return from the signal handler.
            gen_instr_type(TRACE_TYPE_INSTR_RETURN, TID_A, SIG_HANDLER_PC + 1, RETURN_SZ),
            // Sigreturn.
            gen_instr(TID_A, SYSCALL_PC, SYSCALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 15),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 16),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, 3),
            // Syscall xfer.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, SYSCALL_PC + SYSCALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 17),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, 3),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ID, 2),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_RETADDR, CALL_PC + CALL_SZ),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Correctly handle signal event at the beginning of a trace before any instructions
    // were recorded.
    {
        constexpr addr_t SIG_HANDLER_PC = 400;
        constexpr addr_t SYSCALL_PC = 500;
        constexpr addr_t FUNC_PC = 200;
        constexpr size_t SYSCALL_SZ = 2;
        constexpr size_t RETURN_SZ = 3;

        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE,
                       OFFLINE_FILE_TYPE_SYSCALL_NUMBERS),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, FUNC_PC),
            gen_instr(TID_A, SIG_HANDLER_PC),
            // Return from the signal handler.
            gen_instr_type(TRACE_TYPE_INSTR_RETURN, TID_A, SIG_HANDLER_PC + 1, RETURN_SZ),
            // Sigreturn.
            gen_instr(TID_A, SYSCALL_PC, SYSCALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 15),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 16),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, 3),
            // Syscall xfer.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, SYSCALL_PC + SYSCALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 17),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, 3),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ID, 2),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_RETADDR, CALL_PC + CALL_SZ),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Incorrect: signal not arriving between a branch instruction and the function
    // entry should not have a function ID marker after syscall xfer.
    {
        constexpr addr_t SIG_HANDLER_PC = 400;
        constexpr addr_t SYSCALL_PC = 500;
        constexpr size_t SYSCALL_SZ = 2;
        constexpr size_t RETURN_SZ = 3;

        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE,
                       OFFLINE_FILE_TYPE_SYSCALL_NUMBERS),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A, 1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 2),
            gen_instr(TID_A, SIG_HANDLER_PC),
            // Return from the signal handler.
            gen_instr_type(TRACE_TYPE_INSTR_RETURN, TID_A, SIG_HANDLER_PC + 1, RETURN_SZ),
            // Sigreturn.
            gen_instr(TID_A, SYSCALL_PC, SYSCALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 15),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 16),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, 3),
            // Syscall xfer.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, SYSCALL_PC + SYSCALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 17),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, 3),
            // There should not be a function ID marker here.
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ID, 2),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, true,
                         { "Function marker should be after a branch", TID_A,
                           /*ref_ordinal=*/15, /*last_timestamp=*/17,
                           /*instrs_since_last_timestamp=*/0 },
                         "Failed to catch function marker not after branch"))
            return false;
    }
    // Correctly handle nested signals with the first one arriving between a branch
    // instruction and the function entry.
    {
        constexpr addr_t BASE_PC = 100;
        constexpr addr_t FUNC_PC = 200;
        constexpr addr_t SIG1_PC = 300;
        constexpr addr_t SIG2_PC = 400;
        constexpr size_t INSTR_SZ = 1;
        constexpr size_t RETURN_SZ = 3;
        constexpr size_t SYSCALL_SZ = 2;

        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE,
                       OFFLINE_FILE_TYPE_SYSCALL_NUMBERS),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr_type(TRACE_TYPE_INSTR_DIRECT_CALL, TID_A, BASE_PC, CALL_SZ),
            // First signal.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, FUNC_PC),
            // Second signal.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, SIG1_PC),
            gen_instr(TID_A, SIG2_PC, INSTR_SZ),
            gen_instr_type(TRACE_TYPE_INSTR_RETURN, TID_A, SIG2_PC + INSTR_SZ, RETURN_SZ),
            // Sigreturn of the second signal.
            gen_instr(TID_A, SIG2_PC + INSTR_SZ + RETURN_SZ, SYSCALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 15),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER,
                       SIG2_PC + INSTR_SZ + RETURN_SZ + SYSCALL_SZ),
            gen_instr(TID_A, SIG1_PC, INSTR_SZ),
            gen_instr_type(TRACE_TYPE_INSTR_RETURN, TID_A, SIG1_PC + INSTR_SZ, RETURN_SZ),
            // Sigreturn of the first signal.
            gen_instr(TID_A, SIG1_PC + INSTR_SZ + RETURN_SZ, SYSCALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 15),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER,
                       SIG1_PC + INSTR_SZ + RETURN_SZ + SYSCALL_SZ),
            // Function marker of the call before the first signal.
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ID, 2),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Correctly handle consecutive signals (that are nested at the same depth) without
    // any intervening instr between them.
    {
        constexpr addr_t BASE_PC = 100;
        constexpr addr_t FUNC_PC = 200;
        constexpr addr_t SIG1_PC = 300;
        constexpr addr_t SIG2_PC = 400;
        constexpr size_t INSTR_SZ = 1;
        constexpr size_t RETURN_SZ = 3;
        constexpr size_t SYSCALL_SZ = 2;

        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE,
                       OFFLINE_FILE_TYPE_SYSCALL_NUMBERS),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr_type(TRACE_TYPE_INSTR_DIRECT_CALL, TID_A, BASE_PC, CALL_SZ),
            // First signal.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, FUNC_PC),
            gen_instr(TID_A, SIG1_PC, INSTR_SZ),
            gen_instr_type(TRACE_TYPE_INSTR_RETURN, TID_A, SIG1_PC + INSTR_SZ, RETURN_SZ),
            // Sigreturn.
            gen_instr(TID_A, SIG1_PC + INSTR_SZ + RETURN_SZ, SYSCALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 15),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER,
                       SIG1_PC + INSTR_SZ + RETURN_SZ + SYSCALL_SZ),
            // Second signal with no intervening instr.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, FUNC_PC),
            gen_instr(TID_A, SIG2_PC, INSTR_SZ),
            gen_instr_type(TRACE_TYPE_INSTR_RETURN, TID_A, SIG2_PC + INSTR_SZ, RETURN_SZ),
            // Sigreturn.
            gen_instr(TID_A, SIG2_PC + INSTR_SZ + RETURN_SZ, SYSCALL_SZ),
            gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 15),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER,
                       SIG2_PC + INSTR_SZ + RETURN_SZ + SYSCALL_SZ),
            // Function marker of the call before the first signal.
            gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ID, 2),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
#endif
    return true;
}

bool
check_duplicate_syscall_with_same_pc()
{
    std::cerr << "Testing duplicate syscall\n";
    // Incorrect: syscalls with the same PC.
#if defined(X86_64) || defined(X86_32) || defined(ARM_64)
    constexpr addr_t ADDR = 0x7fcf3b9d;
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
#    if defined(X86_64) || defined(X86_32)
            gen_instr_encoded(ADDR, { 0x0f, 0x05 }), // 0x7fcf3b9d: 0f 05 syscall
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 6),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, 3),
            gen_instr_encoded(ADDR, { 0x0f, 0x05 }), // 0x7fcf3b9d: 0f 05 syscall
#    elif defined(ARM_64)
            gen_instr_encoded(ADDR,
                              0xd4000001), // 0x7fcf3b9d: 0xd4000001 svc #0x0
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 6),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, 3),
            gen_instr_encoded(ADDR,
                              0xd4000001), // 0x7fcf3b9d: 0xd4000001 svc #0x0
#    else
        // TODO i#5871: Add AArch32 (and RISC-V) encodings.
#    endif
            gen_exit(TID_A)
        };
        if (!run_checker(memrefs, true,
                         { "Duplicate syscall instrs with the same PC", /*tid=*/1,
                           /*ref_ordinal=*/7, /*last_timestamp=*/6,
                           /*instrs_since_last_timestamp=*/1 },
                         "Failed to catch duplicate syscall instrs with the same PC"))
            return false;
    }

    // Correct: syscalls with different PCs.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
#    if defined(X86_64) || defined(X86_32)
            gen_instr_encoded(ADDR, { 0x0f, 0x05 }), // 0x7fcf3b9d: 0f 05 syscall
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 0),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, 3),
            gen_instr_encoded(ADDR + 2, { 0x0f, 0x05 }), // 0x7fcf3b9dd9eb: 0f 05 syscall
#    elif defined(ARM_64)
            gen_instr_encoded(ADDR, 0xd4000001,
                              TID_A), // 0x7fcf3b9d: 0xd4000001 svc #0x0
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 0),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, 3),
            gen_instr_encoded(ADDR + 4, 0xd4000001,
                              TID_A), // 0x7fcf3b9dd9eb: 0xd4000001 svc #0x0
#    else
        // TODO i#5871: Add AArch32 (and RISC-V) encodings.
#    endif
            gen_exit(TID_A)
        };
        if (!run_checker(memrefs, false)) {
            return false;
        }
    }
#endif
    return true;
}

bool
check_syscalls()
{
    // Ensure missing syscall markers (from "false syscalls") are detected.
    std::cerr << "Testing false syscalls\n";
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
        // Correct: syscall followed by marker (no timestamps; modeling versions
        // prior to TRACE_ENTRY_VERSION_FREQUENT_TIMESTAMPS).
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, FILE_TYPE), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr(TID_A), sys },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 42), nullptr },
            { gen_exit(TID_A), nullptr },
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        if (!run_checker(memrefs, false))
            res = false;
    }
    {
        // Correct: syscall followed by marker with timestamp+cpu in between with
        // subsequent function arg markers.
        uintptr_t sys_func_id =
            static_cast<uintptr_t>(func_trace_t::TRACE_FUNC_ID_SYSCALL_BASE) + 202;
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, FILE_TYPE), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr(TID_A), sys },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 101), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, 3), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 42), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ID, sys_func_id), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ARG, 0), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_ID, sys_func_id), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FUNC_RETVAL, 0), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 111), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, 3), nullptr },
            { gen_instr(TID_A), move1 },
            { gen_exit(TID_A), nullptr }
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        if (!run_checker(memrefs, false))
            res = false;
    }
    {
        // Correct: syscall followed by marker with timestamp+cpu in between.
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, FILE_TYPE), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr(TID_A), sys },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 101), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, 3), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 42), nullptr },
            { gen_exit(TID_A), nullptr },
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        if (!run_checker(memrefs, false))
            res = false;
    }
    {
        // Incorrect: syscall with no marker.
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, FILE_TYPE), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr(TID_A), sys },
            { gen_instr(TID_A), move1 },
            { gen_exit(TID_A), nullptr }
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        if (!run_checker(memrefs, true,
                         { "Syscall marker missing after syscall instruction", TID_A,
                           /*ref_ordinal=*/5, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/2 },
                         "Failed to catch syscall without number marker")) {
            res = false;
        }
    }
    {
        // Incorrect: marker with no syscall.
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, FILE_TYPE), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr(TID_A), move1 },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 42), nullptr },
            { gen_exit(TID_A), nullptr },
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        if (!run_checker(memrefs, true,
                         { "Syscall marker not placed after syscall instruction", TID_A,
                           /*ref_ordinal=*/5, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/1 },
                         "Failed to catch misplaced syscall marker")) {
            res = false;
        }
    }
    // Ensure timestamps are where we expect them.
    std::cerr << "Testing syscall timestamps\n";
    {
        // Correct: syscall preceded by timestamp+cpu.
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_VERSION,
                         TRACE_ENTRY_VERSION_FREQUENT_TIMESTAMPS),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, FILE_TYPE), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr(TID_A), sys },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 101), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, 3), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 42), nullptr },
            { gen_exit(TID_A), nullptr },
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        if (!run_checker(memrefs, false))
            res = false;
    }
    {
        // Incorrect: syscall with no preceding timestamp+cpu.
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_VERSION,
                         TRACE_ENTRY_VERSION_FREQUENT_TIMESTAMPS),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, FILE_TYPE), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr(TID_A), sys },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 42), nullptr },
            { gen_exit(TID_A), nullptr },
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        if (!run_checker(memrefs, true,
                         { "Syscall marker not preceded by timestamp + cpuid", TID_A,
                           /*ref_ordinal=*/6, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/1 },
                         "Failed to catch syscall without timestamp+cpuid")) {
            res = false;
        }
    }
    {
        // Incorrect: syscall with preceding cpu but no timestamp.
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_VERSION,
                         TRACE_ENTRY_VERSION_FREQUENT_TIMESTAMPS),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, FILE_TYPE), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr(TID_A), sys },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, 3), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 42), nullptr },
            { gen_exit(TID_A), nullptr },
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        if (!run_checker(memrefs, true,
                         { "Syscall marker not preceded by timestamp + cpuid", TID_A,
                           /*ref_ordinal=*/7, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/1 },
                         "Failed to catch syscall without timestamp")) {
            res = false;
        }
    }
    // We deliberately do not test for missing post-syscall timestamps as some syscalls
    // do not have a post-syscall event so we can't easily check that.
    instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
    return res;
#endif
}

bool
check_rseq_side_exit_discontinuity()
{
    std::cerr << "Testing rseq side exits\n";
    // Incorrect test: Seemingly missing instructions in a basic block due to rseq side
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
        { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
          nullptr },
        { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
        { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
        // Rseq entry marker not added to make the sequence look like a legacy
        // trace.
        { gen_branch(TID_A), cond_jmp },
        { gen_instr(TID_A), store },
        { gen_data(TID_A, false, 42, 4), nullptr },
        // move1 instruction missing due to the 'side-exit' at move2 which is
        // the target of cond_jmp.
        { gen_instr(TID_A), move2 },
        { gen_exit(TID_A), nullptr }
    };

    // TODO i#6023: Use this IR based encoder in other tests as well.
    static constexpr addr_t BASE_ADDR = 0xeba4ad4;
    auto memrefs = add_encodings_to_memrefs(ilist, memref_instr_vec, BASE_ADDR);
    instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
    if (!run_checker(memrefs, true,
                     { "PC discontinuity due to rseq side exit", /*tid=*/1,
                       /*ref_ordinal=*/7, /*last_timestamp=*/0,
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
    static constexpr memref_tid_t TIMESTAMP_BASE = 100;
    static constexpr int CPU_BASE = 6;
    std::string serial_fname = "tmp_inv_check_serial.bin";
    std::vector<schedule_entry_t> sched;
    sched.emplace_back(TID_A, TIMESTAMP_BASE, CPU_BASE, 0);
    // Include same-timestamp records to stress handling that.
    sched.emplace_back(TID_C, TIMESTAMP_BASE, CPU_BASE + 1, 0);
    sched.emplace_back(TID_B, TIMESTAMP_BASE, CPU_BASE + 2, 0);
    sched.emplace_back(TID_A, TIMESTAMP_BASE + 1, CPU_BASE + 1, 2);
    sched.emplace_back(TID_B, TIMESTAMP_BASE + 2, CPU_BASE, 1);
    // Include records with the same thread ID, timestamp, and CPU, but
    // different start_instruction is being used for comparison.
    sched.emplace_back(TID_C, TIMESTAMP_BASE + 3, CPU_BASE + 2, 3);
    sched.emplace_back(TID_C, TIMESTAMP_BASE + 3, CPU_BASE + 2, 4);
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
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE),
            gen_instr(TID_A, 1),
            gen_instr(TID_A, 2),
            gen_marker(TID_C, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_C, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_C, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE),
            gen_marker(TID_C, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE + 1),
            gen_instr(TID_C, 1),
            gen_instr(TID_C, 2),
            gen_instr(TID_C, 3),
            gen_marker(TID_B, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_B, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_B, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE),
            gen_marker(TID_B, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE + 2),
            gen_instr(TID_B, 1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE + 1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE + 1),
            gen_instr(TID_A, 3),
            gen_instr(TID_A, 4),
            gen_marker(TID_B, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE + 2),
            gen_marker(TID_B, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE),
            gen_instr(TID_B, 2),
            gen_marker(TID_C, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE + 3),
            gen_marker(TID_C, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE + 2),
            gen_instr(TID_C, 4),
            // Markers for the second schedule entry with the same thread ID,
            // timestamp, and CPU as the previous one with a different start_instruction.
            gen_marker(TID_C, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE + 3),
            gen_marker(TID_C, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE + 2),
            gen_exit(TID_A),
            gen_exit(TID_B),
            gen_exit(TID_C),
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
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE),
            gen_instr(TID_A, 1),
            gen_instr(TID_A, 2),
            gen_marker(TID_C, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_C, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_C, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE),
            gen_marker(TID_C, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE + 1),
            gen_instr(TID_C, 1),
            gen_instr(TID_C, 2),
            gen_instr(TID_C, 3),
            gen_marker(TID_B, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_B, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_B, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE),
            gen_marker(TID_B, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE + 2),
            gen_instr(TID_B, 1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE + 1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE + 1),
            gen_instr(TID_A, 3),
            gen_instr(TID_A, 4),
            gen_marker(TID_B, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE + 2),
            gen_marker(TID_B, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE),
            gen_instr(TID_B, 2),
            // Missing the final timestamp+cpu.
            gen_exit(TID_A),
            gen_exit(TID_B),
            gen_exit(TID_C),
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
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE),
            gen_instr(TID_A, 1),
            gen_instr(TID_A, 2),
            gen_marker(TID_C, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_C, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_C, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE),
            gen_marker(TID_C, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE + 1),
            gen_instr(TID_C, 1),
            gen_instr(TID_C, 2),
            // Missing one instruction here.
            gen_marker(TID_B, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_B, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_B, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE),
            gen_marker(TID_B, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE + 2),
            gen_instr(TID_B, 1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE + 1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE + 1),
            gen_instr(TID_A, 3),
            gen_instr(TID_A, 4),
            gen_marker(TID_B, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE + 2),
            gen_marker(TID_B, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE),
            gen_instr(TID_B, 2),
            gen_marker(TID_C, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE + 3),
            gen_marker(TID_C, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE + 2),
            gen_instr(TID_C, 3),
            gen_instr(TID_C, 4),
            gen_marker(TID_C, TRACE_MARKER_TYPE_TIMESTAMP, TIMESTAMP_BASE + 3),
            gen_marker(TID_C, TRACE_MARKER_TYPE_CPU_ID, CPU_BASE + 2),
            gen_exit(TID_A),
            gen_exit(TID_B),
            gen_exit(TID_C),
        };
        std::ifstream serial_read(serial_fname, std::ifstream::binary);
        if (!serial_read)
            return false;
        if (!run_checker(memrefs, true,
                         { "Serial schedule entry does not match trace", TID_C,
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
    // Indirect branch target: correct.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_A, TRACE_MARKER_TYPE_VERSION, TRACE_ENTRY_VERSION_BRANCH_INFO),
            gen_instr(TID_A, /*pc=*/1),
            gen_instr_type(TRACE_TYPE_INSTR_INDIRECT_CALL, TID_A, /*pc=*/2, /*size=*/1,
                           /*target=*/32),
            gen_instr(TID_A, /*pc=*/32),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
#ifdef UNIX
    // Indirect branch target with kernel event: correct.
    // We ensure the next PC is obtained from the kernel event interruption.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_A, TRACE_MARKER_TYPE_VERSION, TRACE_ENTRY_VERSION_BRANCH_INFO),
            gen_instr(TID_A, /*pc=*/1),
            gen_instr_type(TRACE_TYPE_INSTR_INDIRECT_CALL, TID_A, /*pc=*/2, /*size=*/1,
                           /*target=*/32),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 32),
            gen_instr(TID_A, /*pc=*/999),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
#endif
    // Indirect branch target: incorrect zero target PC.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_A, TRACE_MARKER_TYPE_VERSION, TRACE_ENTRY_VERSION_BRANCH_INFO),
            gen_instr(TID_A, /*pc=*/1),
            gen_instr_type(TRACE_TYPE_INSTR_INDIRECT_CALL, TID_A, /*pc=*/2, /*size=*/1,
                           /*target=*/0),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, true,
                         { "Indirect branches must contain targets", TID_A,
                           /*ref_ordinal=*/5, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/2 },
                         "Failed to catch missing indirect branch target field"))
            return false;
    }
    // Indirect branch target: incorrect target value.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_A, TRACE_MARKER_TYPE_VERSION, TRACE_ENTRY_VERSION_BRANCH_INFO),
            gen_instr(TID_A, /*pc=*/1),
            gen_instr_type(TRACE_TYPE_INSTR_INDIRECT_CALL, TID_A, /*pc=*/2, /*size=*/1,
                           /*target=*/32),
            gen_instr(TID_A, /*pc=*/33),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, true,
                         { "Branch does not go to the correct target", TID_A,
                           /*ref_ordinal=*/6, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/3 },
                         "Failed to catch bad indirect branch target field"))
            return false;
    }
#ifdef UNIX
    // Indirect branch target with kernel event: marker value incorrect.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_A, TRACE_MARKER_TYPE_VERSION, TRACE_ENTRY_VERSION_BRANCH_INFO),
            gen_instr(TID_A, /*pc=*/1),
            gen_instr_type(TRACE_TYPE_INSTR_INDIRECT_CALL, TID_A, /*pc=*/2, /*size=*/1,
                           /*target=*/32),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 999),
            gen_instr(TID_A, /*pc=*/32),
            gen_exit(TID_A),
        };
        if (!run_checker(
                memrefs, true,
                { "Branch does not go to the correct target @ kernel_event marker", TID_A,
                  /*ref_ordinal=*/6, /*last_timestamp=*/0,
                  /*instrs_since_last_timestamp=*/2 },
                "Failed to catch bad indirect branch target field")) {
            return false;
        }
    }
    // Correct test: back-to-back signals after an RSEQ abort.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE,
                       OFFLINE_FILE_TYPE_SYSCALL_NUMBERS),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_A, TRACE_MARKER_TYPE_VERSION, TRACE_ENTRY_VERSION_BRANCH_INFO),
            gen_instr_type(TRACE_TYPE_INSTR_UNTAKEN_JUMP, TID_A, /*pc=*/101, /*size=*/1,
                           /*target=*/0),
            gen_marker(TID_A, TRACE_MARKER_TYPE_RSEQ_ABORT, 102),
            // This is the signal which caused the RSEQ abort.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 102),
            // We get a signal after the RSEQ abort.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 301),
            gen_instr(TID_A, /*pc=*/201, /*size=*/1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 15),
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_XFER, 202),
            // The kernel event marker has the same value as the previous one.
            gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 301),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false)) {
            return false;
        }
    }
#endif
    // Deprecated branch type.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_A, TRACE_MARKER_TYPE_VERSION, TRACE_ENTRY_VERSION_BRANCH_INFO),
            gen_instr(TID_A, /*pc=*/1),
            gen_instr_type(TRACE_TYPE_INSTR_CONDITIONAL_JUMP, TID_A, /*pc=*/2),
            gen_exit(TID_A),
        };
        if (!run_checker(
                memrefs, true,
                { "The CONDITIONAL_JUMP type is deprecated and should not appear", TID_A,
                  /*ref_ordinal=*/5, /*last_timestamp=*/0,
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
            { gen_marker(TID_A, TRACE_MARKER_TYPE_VERSION,
                         TRACE_ENTRY_VERSION_BRANCH_INFO),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr_type(TRACE_TYPE_INSTR_TAKEN_JUMP, TID_A), cbr_to_move },
            { gen_instr(TID_A), move },
            { gen_exit(TID_A), nullptr },
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
            { gen_marker(TID_A, TRACE_MARKER_TYPE_VERSION,
                         TRACE_ENTRY_VERSION_BRANCH_INFO),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr_type(TRACE_TYPE_INSTR_TAKEN_JUMP, TID_A), cbr_to_move },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 0), move },
            { gen_instr(TID_A), nop },
            { gen_exit(TID_A), nullptr },
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
            { gen_marker(TID_A, TRACE_MARKER_TYPE_VERSION,
                         TRACE_ENTRY_VERSION_BRANCH_INFO),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr_type(TRACE_TYPE_INSTR_TAKEN_JUMP, TID_A), cbr_to_move },
            { gen_instr(TID_A), nop },
            { gen_exit(TID_A), nullptr },
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
        if (!run_checker(memrefs, true,
                         { "Branch does not go to the correct target", /*tid=*/1,
                           /*ref_ordinal=*/6, /*last_timestamp=*/0,
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
            { gen_marker(TID_A, TRACE_MARKER_TYPE_VERSION,
                         TRACE_ENTRY_VERSION_BRANCH_INFO),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr_type(TRACE_TYPE_INSTR_TAKEN_JUMP, TID_A), cbr_to_move },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 0), nop },
            { gen_instr(TID_A), move },
            { gen_exit(TID_A), nullptr },
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
        if (!run_checker(
                memrefs, true,
                { "Branch does not go to the correct target @ kernel_event marker",
                  /*tid=*/1,
                  /*ref_ordinal=*/6, /*last_timestamp=*/0,
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
            { gen_marker(TID_A, TRACE_MARKER_TYPE_VERSION,
                         TRACE_ENTRY_VERSION_BRANCH_INFO),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr_type(TRACE_TYPE_INSTR_UNTAKEN_JUMP, TID_A), cbr_to_move },
            { gen_instr(TID_A), nop },
            { gen_exit(TID_A), nullptr },
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
            { gen_marker(TID_A, TRACE_MARKER_TYPE_VERSION,
                         TRACE_ENTRY_VERSION_BRANCH_INFO),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr_type(TRACE_TYPE_INSTR_UNTAKEN_JUMP, TID_A), cbr_to_move },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 0), nop },
            { gen_instr(TID_A), move },
            { gen_exit(TID_A), nullptr },
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
            { gen_marker(TID_A, TRACE_MARKER_TYPE_VERSION,
                         TRACE_ENTRY_VERSION_BRANCH_INFO),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr_type(TRACE_TYPE_INSTR_UNTAKEN_JUMP, TID_A), cbr_to_move },
            { gen_instr(TID_A), move },
            { gen_exit(TID_A), nullptr },
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
        if (!run_checker(memrefs, true,
                         { "Branch does not go to the correct target", TID_A,
                           /*ref_ordinal=*/6, /*last_timestamp=*/0,
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
            { gen_marker(TID_A, TRACE_MARKER_TYPE_VERSION,
                         TRACE_ENTRY_VERSION_BRANCH_INFO),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr_type(TRACE_TYPE_INSTR_UNTAKEN_JUMP, TID_A), cbr_to_move },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_KERNEL_EVENT, 0), move },
            { gen_instr(TID_A), nop },
            { gen_exit(TID_A), nullptr },
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
        if (!run_checker(
                memrefs, true,
                { "Branch does not go to the correct target @ kernel_event marker", TID_A,
                  /*ref_ordinal=*/6, /*last_timestamp=*/0,
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
    // Matching marker and file type: correct.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE,
                       OFFLINE_FILE_TYPE_IFILTERED |
                           OFFLINE_FILE_TYPE_BIMODAL_FILTERED_WARMUP),
            gen_marker(TID_A, TRACE_MARKER_TYPE_INSTRUCTION_COUNT, 1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FILTER_ENDPOINT, 0),
            gen_instr(TID_A),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Missing TRACE_MARKER_TYPE_FILTER_ENDPOINT marker: incorrect.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE,
                       OFFLINE_FILE_TYPE_IFILTERED |
                           OFFLINE_FILE_TYPE_BIMODAL_FILTERED_WARMUP),
            gen_marker(TID_A, TRACE_MARKER_TYPE_INSTRUCTION_COUNT, 1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A),
            gen_exit(TID_A),
        };
        if (!run_checker(
                memrefs, true,
                { "Expected to find TRACE_MARKER_TYPE_FILTER_ENDPOINT for the given "
                  "file type",
                  TID_A,
                  /*ref_ordinal=*/8, /*last_timestamp=*/0,
                  /*instrs_since_last_timestamp=*/1 },
                "Failed to catch missing TRACE_MARKER_TYPE_FILTER_ENDPOINT marker"))
            return false;
    }
    // Unexpected TRACE_MARKER_TYPE_FILTER_ENDPOINT marker: incorrect.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_IFILTERED),
            gen_marker(TID_A, TRACE_MARKER_TYPE_INSTRUCTION_COUNT, 1),
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_A, TRACE_MARKER_TYPE_FILTER_ENDPOINT, 0),
            gen_instr(TID_A),
            gen_exit(TID_A),
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
    std::cerr << "Testing timestamp ordering\n";
    // Correct: timestamps increase monotonically.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 0),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 10),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 10),
            gen_instr(TID_A),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Incorrect: timestamp does not increase monotonically.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 0),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 10),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 5),
            gen_instr(TID_A),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, true,
                         { "Timestamp does not increase monotonically",
                           /*tid=*/1,
                           /*ref_ordinal=*/5, /*last_timestamp=*/10,
                           /*instrs_since_last_timestamp=*/0 },
                         "Failed to catch timestamps not increasing "
                         "monotonically"))
            return false;
    }
#ifdef X86_32
    // Correct: timestamp rollovers.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP,
                       (std::numeric_limits<uintptr_t>::max)() - 10),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP,
                       (std::numeric_limits<uintptr_t>::max)()),
            gen_marker(TID_A, TRACE_MARKER_TYPE_TIMESTAMP, 10),
            gen_instr(TID_A),
            gen_exit(TID_A),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
#endif
    return true;
}

bool
check_read_write_records_match_operands()
{
    // Only the number of memory read and write records is checked against the
    // operands. Address and size are not used.
    std::cerr << "Testing number of memory read/write records matching operands\n";

    // Correct: number of read records matches the operand.
    {
        instr_t *load = XINST_CREATE_load(GLOBAL_DCONTEXT, opnd_create_reg(REG1),
                                          OPND_CREATE_MEMPTR(REG1, /*disp=*/0));
        instrlist_t *ilist = instrlist_create(GLOBAL_DCONTEXT);
        instrlist_append(ilist, load);

        std::vector<memref_with_IR_t> memref_instr_vec = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr(TID_A), load },
            { gen_data(TID_A, /*load=*/true, /*addr=*/0, /*size=*/0), nullptr },
            { gen_exit(TID_A), nullptr }
        };
        static constexpr addr_t BASE_ADDR = 0xeba4ad4;
        auto memrefs = add_encodings_to_memrefs(ilist, memref_instr_vec, BASE_ADDR);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
        if (!run_checker(memrefs, false)) {
            return false;
        }
    }
    // Incorrect: too many read records.
    {
        instr_t *load = XINST_CREATE_load(GLOBAL_DCONTEXT, opnd_create_reg(REG1),
                                          OPND_CREATE_MEMPTR(REG1, /*disp=*/0));
        instrlist_t *ilist = instrlist_create(GLOBAL_DCONTEXT);
        instrlist_append(ilist, load);

        std::vector<memref_with_IR_t> memref_instr_vec = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr(TID_A), load },
            { gen_data(TID_A, /*load=*/true, /*addr=*/0, /*size=*/0), nullptr },
            { gen_data(TID_A, /*load=*/true, /*addr=*/0, /*size=*/0), nullptr },
            { gen_exit(TID_A), nullptr }
        };
        static constexpr addr_t BASE_ADDR = 0xeba4ad4;
        auto memrefs = add_encodings_to_memrefs(ilist, memref_instr_vec, BASE_ADDR);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
        if (!run_checker(memrefs, true,
                         { "Too many read records", TID_A, /*ref_ordinal=*/6,
                           /*last_timestamp=*/0, /*instrs_since_last_timestamp=*/1 },
                         "Failed to catch too many read records"))
            return false;
    }
    // Incorrect: missing read records.
    {
        instr_t *nop = XINST_CREATE_nop(GLOBAL_DCONTEXT);
        instr_t *load = XINST_CREATE_load(GLOBAL_DCONTEXT, opnd_create_reg(REG1),
                                          OPND_CREATE_MEMPTR(REG1, /*disp=*/0));
        instrlist_t *ilist = instrlist_create(GLOBAL_DCONTEXT);
        instrlist_append(ilist, load);
        instrlist_append(ilist, nop);

        std::vector<memref_with_IR_t> memref_instr_vec = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr(TID_A), load },
            { gen_instr(TID_A), nop },
            { gen_exit(TID_A), nullptr },
        };
        static constexpr addr_t BASE_ADDR = 0xeba4ad4;
        auto memrefs = add_encodings_to_memrefs(ilist, memref_instr_vec, BASE_ADDR);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
        if (!run_checker(memrefs, true,
                         { "Missing read records", TID_A, /*ref_ordinal=*/5,
                           /*last_timestamp=*/0, /*instrs_since_last_timestamp=*/2 },
                         "Failed to catch missing read records"))
            return false;
    }
    // Correct: number of write records matches the operand.
    {
        instr_t *store = XINST_CREATE_store(
            GLOBAL_DCONTEXT, OPND_CREATE_MEMPTR(REG2, /*disp=*/0), opnd_create_reg(REG1));
        instrlist_t *ilist = instrlist_create(GLOBAL_DCONTEXT);
        instrlist_append(ilist, store);

        std::vector<memref_with_IR_t> memref_instr_vec = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr(TID_A), store },
            { gen_data(TID_A, /*load=*/false, /*addr=*/0, /*size=*/0), nullptr },
            { gen_exit(TID_A), nullptr }
        };

        static constexpr addr_t BASE_ADDR = 0xeba4ad4;
        auto memrefs = add_encodings_to_memrefs(ilist, memref_instr_vec, BASE_ADDR);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
        if (!run_checker(memrefs, false)) {
            return false;
        }
    }
    // Incorrect: too many write records.
    {
        instr_t *store = XINST_CREATE_store(
            GLOBAL_DCONTEXT, OPND_CREATE_MEMPTR(REG2, /*disp=*/0), opnd_create_reg(REG1));
        instrlist_t *ilist = instrlist_create(GLOBAL_DCONTEXT);
        instrlist_append(ilist, store);

        std::vector<memref_with_IR_t> memref_instr_vec = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr(TID_A), store },
            { gen_data(TID_A, /*load=*/false, /*addr=*/0, /*size=*/0), nullptr },
            { gen_data(TID_A, /*load=*/false, /*addr=*/0, /*size=*/0), nullptr },
            { gen_exit(TID_A), nullptr }
        };

        static constexpr addr_t BASE_ADDR = 0xeba4ad4;
        auto memrefs = add_encodings_to_memrefs(ilist, memref_instr_vec, BASE_ADDR);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
        if (!run_checker(memrefs, true,
                         { "Too many write records", TID_A, /*ref_ordinal=*/6,
                           /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/1 },
                         "Failed to catch too many write records"))
            return false;
    }
    // Incorrect: missing write records.
    {
        instr_t *nop = XINST_CREATE_nop(GLOBAL_DCONTEXT);
        instr_t *store = XINST_CREATE_store(
            GLOBAL_DCONTEXT, OPND_CREATE_MEMPTR(REG2, /*disp=*/0), opnd_create_reg(REG1));
        instrlist_t *ilist = instrlist_create(GLOBAL_DCONTEXT);
        instrlist_append(ilist, store);
        instrlist_append(ilist, nop);

        std::vector<memref_with_IR_t> memref_instr_vec = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr(TID_A), store },
            { gen_instr(TID_A), nop },
            { gen_exit(TID_A), nullptr },
        };

        static constexpr addr_t BASE_ADDR = 0xeba4ad4;
        auto memrefs = add_encodings_to_memrefs(ilist, memref_instr_vec, BASE_ADDR);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
        if (!run_checker(memrefs, true,
                         { "Missing write records", TID_A, /*ref_ordinal=*/5,
                           /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/2 },
                         "Fail to catch missing write records"))
            return false;
    }
#if defined(X86_64) || defined(X86_32)
    // Correct: number of read and write records matches the operand.
    {
        instr_t *movs = INSTR_CREATE_movs_1(GLOBAL_DCONTEXT);
        instrlist_t *ilist = instrlist_create(GLOBAL_DCONTEXT);
        instrlist_append(ilist, movs);

        std::vector<memref_with_IR_t> memref_instr_vec = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr(TID_A), movs },
            { gen_data(TID_A, /*load=*/true, /*addr=*/0, /*size=*/0), nullptr },
            { gen_data(TID_A, /*load=*/false, /*addr=*/0, /*size=*/0), nullptr },
            { gen_exit(TID_A), nullptr }
        };

        static constexpr addr_t BASE_ADDR = 0xeba4ad4;
        auto memrefs = add_encodings_to_memrefs(ilist, memref_instr_vec, BASE_ADDR);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
        if (!run_checker(memrefs, false)) {
            return false;
        }
    }
    // Correct: handle cache flush operand correctly.
    {
        instr_t *clflush = INSTR_CREATE_clflush(
            GLOBAL_DCONTEXT, OPND_CREATE_MEM_clflush(REG1, REG_NULL, 0, 0));
        instrlist_t *ilist = instrlist_create(GLOBAL_DCONTEXT);
        instrlist_append(ilist, clflush);
        static constexpr addr_t BASE_ADDR = 0xeba4ad4;
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr(TID_A), clflush },
            { gen_addr(TID_A, /*type=*/TRACE_TYPE_DATA_FLUSH, /*addr=*/0, /*size=*/0),
              nullptr },
            { gen_exit(TID_A), nullptr },
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
        if (!run_checker(memrefs, false)) {
            return false;
        }
    }
    // Correct: ignore predicated operands which may not have memory access.
    {
        instr_t *nop = XINST_CREATE_nop(GLOBAL_DCONTEXT);
        instr_t *rep_movs = INSTR_CREATE_rep_movs_1(GLOBAL_DCONTEXT);
        instrlist_t *ilist = instrlist_create(GLOBAL_DCONTEXT);
        instrlist_append(ilist, rep_movs);
        instrlist_append(ilist, nop);
        static constexpr addr_t BASE_ADDR = 0xeba4ad4;
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr(TID_A), rep_movs },
            { gen_instr(TID_A), nop },
            { gen_exit(TID_A), nullptr },
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
        if (!run_checker(memrefs, false)) {
            return false;
        }
    }
    // Correct: ignore operands with opcode which do not have real memory
    // access.
    {
        instr_t *lea =
            INSTR_CREATE_lea(GLOBAL_DCONTEXT, opnd_create_reg(REG1),
                             opnd_create_base_disp(REG1, REG_NULL, 0, 1, OPSZ_lea));
        instr_t *nop = XINST_CREATE_nop(GLOBAL_DCONTEXT);
        instrlist_t *ilist = instrlist_create(GLOBAL_DCONTEXT);
        instrlist_append(ilist, lea);
        instrlist_append(ilist, nop);
        static constexpr addr_t BASE_ADDR = 0xeba4ad4;
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr(TID_A), lea },
            { gen_instr(TID_A), nop },
            { gen_exit(TID_A), nullptr },
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
        if (!run_checker(memrefs, false)) {
            return false;
        }
    }
#endif
    return true;
}

bool
check_exit_found(void)
{
    std::cerr << "Testing thread exits\n";
    // Correct: all threads have exits.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A),
            gen_exit(TID_A),
            gen_marker(TID_B, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_B, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_B),
            gen_exit(TID_B),
            gen_marker(TID_C, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_C, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_C),
            gen_exit(TID_C),
        };
        if (!run_checker(memrefs, false))
            return false;
    }
    // Incorrect: a thread is missing an exit.
    {
        std::vector<memref_t> memrefs = {
            gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_A),
            gen_exit(TID_A),
            gen_marker(TID_B, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_B, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_B),
            // Missing exit.
            gen_marker(TID_C, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
            gen_marker(TID_C, TRACE_MARKER_TYPE_PAGE_SIZE, 4096),
            gen_instr(TID_C),
            gen_exit(TID_C),
        };
        if (!run_checker(memrefs, true,
                         { "Thread is missing exit", /*tid=*/TID_B,
                           /*ref_ordinal=*/3, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/1 },
                         "Failed to catch missing thread exit"))
            return false;
    }
    return true;
}

bool
check_kernel_syscall_trace(void)
{
    std::cerr << "Testing kernel syscall traces\n";
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
    instr_t *move =
        XINST_CREATE_move(GLOBAL_DCONTEXT, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instr_t *load = XINST_CREATE_load(GLOBAL_DCONTEXT, opnd_create_reg(REG1),
                                      OPND_CREATE_MEMPTR(REG1, /*disp=*/0));
    instrlist_t *ilist = instrlist_create(GLOBAL_DCONTEXT);
    instrlist_append(ilist, sys);
    instrlist_append(ilist, move);
    instrlist_append(ilist, load);
    static constexpr addr_t BASE_ADDR = 0x123450;
    static constexpr uintptr_t FILE_TYPE = OFFLINE_FILE_TYPE_ENCODINGS |
        OFFLINE_FILE_TYPE_SYSCALL_NUMBERS | OFFLINE_FILE_TYPE_KERNEL_SYSCALL_INSTR_ONLY;
    bool res = true;
    {
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, FILE_TYPE), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr(TID_A), sys },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 42), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL_TRACE_START, 42), nullptr },
            { gen_instr(TID_A), move },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL_TRACE_END, 42), nullptr },
            { gen_exit(TID_A), nullptr }
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        if (!run_checker(memrefs, false))
            res = false;
    }
    {
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, FILE_TYPE), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr(TID_A), sys },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 42), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL_TRACE_START, 42), nullptr },
            { gen_instr(TID_A), move },
            { gen_instr(TID_A), load },
            // No data memref for the above load, but it should not be an invariant
            // violation because the trace type is
            // OFFLINE_FILE_TYPE_KERNEL_SYSCALL_INSTR_ONLY.
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL_TRACE_END, 42), nullptr },
            { gen_exit(TID_A), nullptr }
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        if (!run_checker(memrefs, false))
            res = false;
    }
    {
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE,
                         OFFLINE_FILE_TYPE_ENCODINGS | OFFLINE_FILE_TYPE_SYSCALL_NUMBERS |
                             OFFLINE_FILE_TYPE_KERNEL_SYSCALLS),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr(TID_A), sys },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 42), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL_TRACE_START, 42), nullptr },
            { gen_instr(TID_A), move },
            { gen_instr(TID_A), load },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL_TRACE_END, 42), nullptr },
            { gen_exit(TID_A), nullptr }
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        if (!run_checker(memrefs, true,
                         { "Missing read records",
                           /*tid=*/TID_A,
                           /*ref_ordinal=*/10, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/3 },
                         "Failed to catch missing data ref"))
            res = false;
    }
    {
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE,
                         OFFLINE_FILE_TYPE_ENCODINGS | OFFLINE_FILE_TYPE_SYSCALL_NUMBERS),
              nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr(TID_A), sys },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 42), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL_TRACE_START, 42), nullptr },
            { gen_instr(TID_A), move },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL_TRACE_END, 42), nullptr },
            { gen_exit(TID_A), nullptr }
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        if (!run_checker(memrefs, true,
                         { "Found kernel syscall trace without corresponding file type",
                           /*tid=*/TID_A,
                           /*ref_ordinal=*/6, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/1 },
                         "Failed to catch mismatching file type"))
            res = false;
    }
    {
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, FILE_TYPE), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr(TID_A), sys },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 42), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL_TRACE_START, 41), nullptr },
            { gen_instr(TID_A), move },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL_TRACE_END, 42), nullptr },
            { gen_exit(TID_A), nullptr }
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        if (!run_checker(memrefs, true,
                         { "Mismatching syscall num in trace start and syscall marker",
                           /*tid=*/TID_A,
                           /*ref_ordinal=*/6, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/1 },
                         "Failed to catch mismatching trace start marker value"))
            res = false;
    }
    {
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, FILE_TYPE), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr(TID_A), sys },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 42), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL_TRACE_START, 42), nullptr },
            { gen_instr(TID_A), move },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL_TRACE_END, 41), nullptr },
            { gen_exit(TID_A), nullptr }
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        if (!run_checker(memrefs, true,
                         { "Mismatching syscall num in trace end and syscall marker",
                           /*tid=*/TID_A,
                           /*ref_ordinal=*/8, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/2 },
                         "Failed to catch mismatching trace end marker value"))
            res = false;
    }
    {
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, FILE_TYPE), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr(TID_A), sys },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 42), nullptr },
            { gen_instr(TID_A), move },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL_TRACE_END, 42), nullptr },
            { gen_exit(TID_A), nullptr }
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        if (!run_checker(memrefs, true,
                         { "Found kernel syscall trace end without start",
                           /*tid=*/TID_A,
                           /*ref_ordinal=*/7, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/2 },
                         "Failed to catch missing kernel trace start marker"))
            res = false;
    }
    {
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, FILE_TYPE), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr(TID_A), sys },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 42), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CPU_ID, 11), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL_TRACE_START, 42), nullptr },
            { gen_instr(TID_A), move },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL_TRACE_END, 42), nullptr },
            { gen_exit(TID_A), nullptr }
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        if (!run_checker(memrefs, true,
                         { "System call trace found without prior syscall marker",
                           /*tid=*/TID_A,
                           /*ref_ordinal=*/7, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/1 },
                         "Failed to catch missing prior sysnum marker"))
            res = false;
    }
    {
        std::vector<memref_with_IR_t> memref_setup = {
            { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, FILE_TYPE), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_PAGE_SIZE, 4096), nullptr },
            { gen_instr(TID_A), sys },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 42), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL_TRACE_START, 42), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL, 42), nullptr },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL_TRACE_START, 42), nullptr },
            { gen_instr(TID_A), move },
            { gen_marker(TID_A, TRACE_MARKER_TYPE_SYSCALL_TRACE_END, 42), nullptr },
            { gen_exit(TID_A), nullptr }
        };
        auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        if (!run_checker(memrefs, true,
                         { "Nested kernel syscall traces are not expected",
                           /*tid=*/TID_A,
                           /*ref_ordinal=*/8, /*last_timestamp=*/0,
                           /*instrs_since_last_timestamp=*/1 },
                         "Failed to catch nested syscall traces"))
            res = false;
    }
    return res;
#endif
}

int
test_main(int argc, const char *argv[])
{
    if (check_branch_target_after_branch() && check_sane_control_flow() &&
        check_kernel_xfer() && check_rseq() && check_function_markers() &&
        check_duplicate_syscall_with_same_pc() && check_syscalls() &&
        check_rseq_side_exit_discontinuity() && check_schedule_file() &&
        check_branch_decoration() && check_filter_endpoint() &&
        check_timestamps_increase_monotonically() &&
        check_read_write_records_match_operands() && check_exit_found() &&
        check_kernel_syscall_trace()) {
        std::cerr << "invariant_checker_test passed\n";
        return 0;
    }
    std::cerr << "invariant_checker_test FAILED\n";
    exit(1);
}

} // namespace drmemtrace
} // namespace dynamorio
