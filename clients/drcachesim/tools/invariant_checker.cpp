/* **********************************************************
 * Copyright (c) 2017-2024 Google, Inc.  All rights reserved.
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

#include "invariant_checker.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <mutex>
#include <stack>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "analysis_tool.h"
#include "dr_api.h"
#include "memref.h"
#include "memtrace_stream.h"
#include "invariant_checker_create.h"
#include "trace_entry.h"
#include "utils.h"
#ifdef LINUX
#    include "../../core/unix/include/syscall_target.h"
#endif

namespace dynamorio {
namespace drmemtrace {

// We don't expose the alignment requirement for DR_ISA_REGDEPS instructions (4 bytes),
// so we duplicate it here.
#define REGDEPS_ALIGN_BYTES 4

analysis_tool_t *
invariant_checker_create(bool offline, unsigned int verbose)
{
    return new invariant_checker_t(offline, verbose, "");
}

invariant_checker_t::invariant_checker_t(bool offline, unsigned int verbose,
                                         std::string test_name,
                                         std::istream *serial_schedule_file,
                                         std::istream *cpu_schedule_file,
                                         bool abort_on_invariant_error)
    : knob_offline_(offline)
    , knob_verbose_(verbose)
    , knob_test_name_(test_name)
    , serial_schedule_file_(serial_schedule_file)
    , cpu_schedule_file_(cpu_schedule_file)
    , abort_on_invariant_error_(abort_on_invariant_error)
{
    if (knob_test_name_ == "kernel_xfer_app" || knob_test_name_ == "rseq_app")
        has_annotations_ = true;
}

invariant_checker_t::~invariant_checker_t()
{
}

std::string
invariant_checker_t::initialize_shard_type(shard_type_t shard_type)
{
    if (shard_type == SHARD_BY_CORE) {
        // We track state that is inherently tied to threads.
        //
        // XXX: If we did get kernel pieces stitching together context switches,
        // we could try to check PC continuity.  We could also try to enable
        // certain other checks for core-sharded.
        return "invariant_checker tool does not support sharding by core";
    }
    return "";
}

std::string
invariant_checker_t::initialize_stream(memtrace_stream_t *serial_stream)
{
    serial_stream_ = serial_stream;
    return "";
}

void
invariant_checker_t::report_if_false(per_shard_t *shard, bool condition,
                                     const std::string &invariant_name)
{
    if (!condition) {
        // TODO i#5505: There are some PC discontinuities in the instr traces
        // captured using Intel-PT. Since these are not trivial to solve, we
        // turn this into a non-fatal check for the test for now.
        if (TESTANY(OFFLINE_FILE_TYPE_KERNEL_SYSCALL_INSTR_ONLY, shard->file_type_) &&
            knob_test_name_ == "kernel_syscall_pt_trace" &&
            shard->between_kernel_syscall_trace_markers_ &&
            (invariant_name == "Non-explicit control flow has no marker" ||
             // Some discontinuities are flagged as the following. This is
             // a false positive of our heuristic to find rseq side exit
             // discontinuities.
             invariant_name == "PC discontinuity due to rseq side exit" ||
             invariant_name == "Branch does not go to the correct target")) {
            return;
        }
        std::cerr << "Trace invariant failure in T" << shard->tid_ << " at ref # "
                  << shard->stream->get_record_ordinal() << " ("
                  << shard->instr_count_since_last_timestamp_
                  << " instrs since timestamp " << shard->last_timestamp_
                  << "): " << invariant_name << "\n";
        if (abort_on_invariant_error_) {
            abort();
        } else {
            ++shard->error_count_;
        }
    }
}

bool
invariant_checker_t::parallel_shard_supported()
{
    return true;
}

void *
invariant_checker_t::parallel_shard_init_stream(int shard_index, void *worker_data,
                                                memtrace_stream_t *shard_stream)
{
    auto per_shard = std::unique_ptr<per_shard_t>(new per_shard_t);
    per_shard->stream = shard_stream;
    void *res = reinterpret_cast<void *>(per_shard.get());
    std::lock_guard<std::mutex> guard(init_mutex_);
    per_shard->tid_ = shard_stream->get_tid();
    shard_map_[shard_index] = std::move(per_shard);
    // If we are dealing with an OFFLINE_FILE_TYPE_ARCH_REGDEPS trace, we need to set the
    // dcontext ISA mode to the correct synthetic ISA (i.e., DR_ISA_REGDEPS).
    uint64_t filetype = shard_stream->get_filetype();
    if (TESTANY(OFFLINE_FILE_TYPE_ARCH_REGDEPS, filetype)) {
        dr_isa_mode_t isa_mode = dr_get_isa_mode(drcontext_);
        if (isa_mode != DR_ISA_REGDEPS) {
            dr_set_isa_mode(drcontext_, DR_ISA_REGDEPS, nullptr);
        }
    }
    return res;
}

// We have no stream interface in invariant_checker_test unit tests.
// XXX: Could we refactor the test to use a reader that takes a vector?
void *
invariant_checker_t::parallel_shard_init(int shard_index, void *worker_data)
{
    return parallel_shard_init_stream(shard_index, worker_data, nullptr);
}

bool
invariant_checker_t::parallel_shard_exit(void *shard_data)
{
    per_shard_t *shard = reinterpret_cast<per_shard_t *>(shard_data);
    report_if_false(shard,
                    shard->saw_thread_exit_
                        // XXX i#6733: For online we sometimes see threads
                        // exiting w/o the tracer inserting an exit.  Until we figure
                        // that out we disable this error to unblock testing.
                        || !knob_offline_,
                    "Thread is missing exit");
    if (!TESTANY(OFFLINE_FILE_TYPE_FILTERED | OFFLINE_FILE_TYPE_DFILTERED |
                     // In OFFLINE_FILE_TYPE_ARCH_REGDEPS we can have leftover
                     // shard->expected_[read | write]_records_ as we don't decrease this
                     // counter when we encounter a TRACE_TYPE_[READ | WRITE]. We cannot
                     // decrease the counter because the precise number of reads and
                     // writes a DR_ISA_REGDEPS instruction performs cannot be determined.
                     OFFLINE_FILE_TYPE_ARCH_REGDEPS,
                 shard->file_type_)) {
        report_if_false(shard, shard->expected_read_records_ == 0,
                        "Missing read records");
        report_if_false(shard, shard->expected_write_records_ == 0,
                        "Missing write records");
    }
    return true;
}

std::string
invariant_checker_t::parallel_shard_error(void *shard_data)
{
    per_shard_t *shard = reinterpret_cast<per_shard_t *>(shard_data);
    return shard->error_;
}

bool
invariant_checker_t::is_a_unit_test(per_shard_t *shard)
{
    // Look for a mock stream.
    return shard->stream == nullptr || shard->stream->get_input_interface() == nullptr;
}

#ifdef X86
bool
invariant_checker_t::relax_expected_write_count_check_for_kernel(per_shard_t *shard)
{
    if (!shard->between_kernel_syscall_trace_markers_ &&
        !shard->between_kernel_context_switch_markers_)
        return false;
    bool relax =
        // Xsave does multiple writes. Write count cannot be determined using the
        // decoder. System call trace templates collected on QEMU would show all writes.
        // XXX: Perhaps we can query the expected size using cpuid and add it as a
        // marker in the system call trace template, and then adjust it according to the
        // cpuid value for the trace being injected into.
        shard->prev_instr_.decoding.is_xsave;
    return relax;
}

bool
invariant_checker_t::relax_expected_read_count_check_for_kernel(per_shard_t *shard)
{
    if (!shard->between_kernel_syscall_trace_markers_ &&
        !shard->between_kernel_context_switch_markers_)
        return false;
    bool relax =
        // iret pops bunch of data from the stack at interrupt returns. System call
        // trace templates collected on QEMU would show all reads.
        // TODO i#6742: iret has different behavior in user vs protected mode. This
        // difference in iret behavior can be detected in the decoder and perhaps be
        // accounted for here in a way better than relying on
        // between_kernel_syscall_trace_markers_.
        shard->prev_instr_.decoding.opcode == OP_iret

        // Xrstor does multiple reads. Read count cannot be determined using the
        // decoder. System call trace templates collected on QEMU would show all reads.
        // XXX: Perhaps we can query the expected size using cpuid and add it as a
        // marker in the system call trace template, and then adjust it according to the
        // cpuid value for the trace being injected into.
        || shard->prev_instr_.decoding.is_xrstor

        // xsave variants also read some fields from the xsave header
        // (https://www.felixcloutier.com/x86/xsaveopt).
        // XXX, i#6769: Same as above, can we store any metadata in the trace to allow
        // us to adapt the decoder to expect this?
        || shard->prev_instr_.decoding.is_xsave;
    return relax;
}
#endif

bool
invariant_checker_t::parallel_shard_memref(void *shard_data, const memref_t &memref)
{
    per_shard_t *shard = reinterpret_cast<per_shard_t *>(shard_data);
    report_if_false(shard, shard->tid_ == memref.data.tid, "Shard tid != memref tid");
    // We check the memtrace_stream_t counts with our own, unless there was an
    // instr skip from the start where we cannot compare, or we're in a unit
    // test with no stream interface, or we're in serial mode (since we want
    // per-shard counts for error reporting; XXX: we could add our own global
    // counts to compare to the serial stream).
    ++shard->ref_count_;
    if (type_is_instr(memref.instr.type)) {
        ++shard->instr_count_;
        ++shard->instr_count_since_last_timestamp_;
    }
    // XXX: We also can't verify counts with a skip invoked from the middle, but
    // we have no simple way to detect that here.
    if (shard->instr_count_ <= 1 && !shard->skipped_instrs_ && !is_a_unit_test(shard) &&
        shard->stream->get_instruction_ordinal() > 1) {
        shard->skipped_instrs_ = true;
        if (!shard->saw_filetype_) {
            shard->file_type_ =
                static_cast<offline_file_type_t>(shard->stream->get_filetype());
        }
    }
    if (!shard->skipped_instrs_ && !is_a_unit_test(shard) &&
        (shard->stream != serial_stream_ || shard_map_.size() == 1)) {
        report_if_false(shard, shard->ref_count_ == shard->stream->get_record_ordinal(),
                        "Stream record ordinal inaccurate");
        report_if_false(shard,
                        shard->instr_count_ == shard->stream->get_instruction_ordinal(),
                        "Stream instr ordinal inaccurate");
    }
    bool prev_was_syscall_marker_saved = shard->prev_was_syscall_marker_;
    shard->prev_was_syscall_marker_ = false;
#ifdef UNIX
    if (has_annotations_) {
        // Check conditions specific to the signal_invariants app, where it
        // has annotations in prefetch instructions telling us how many instrs
        // and/or memrefs until a signal should arrive.
        if ((shard->instrs_until_interrupt_ == 0 &&
             shard->memrefs_until_interrupt_ == -1) ||
            (shard->instrs_until_interrupt_ == -1 &&
             shard->memrefs_until_interrupt_ == 0) ||
            (shard->instrs_until_interrupt_ == 0 &&
             shard->memrefs_until_interrupt_ == 0)) {
            report_if_false(
                shard,
                // I-filtered traces don't have all instrs so this doesn't apply.
                TESTANY(OFFLINE_FILE_TYPE_FILTERED | OFFLINE_FILE_TYPE_IFILTERED,
                        shard->file_type_) ||
                    (memref.marker.type == TRACE_TYPE_MARKER &&
                     (memref.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT ||
                      memref.marker.marker_type == TRACE_MARKER_TYPE_RSEQ_ABORT)) ||
                    // TODO i#3937: Online instr bundles currently violate this.
                    !knob_offline_,
                "Interruption marker mis-placed");
            shard->instrs_until_interrupt_ = -1;
            shard->memrefs_until_interrupt_ = -1;
        }
        if (shard->memrefs_until_interrupt_ >= 0 &&
            (memref.data.type == TRACE_TYPE_READ ||
             memref.data.type == TRACE_TYPE_WRITE)) {
            report_if_false(shard, shard->memrefs_until_interrupt_ != 0,
                            "Interruption marker too late");
            --shard->memrefs_until_interrupt_;
        }
        // Check that the signal delivery marker is immediately followed by the
        // app's signal handler, which we have annotated with "prefetcht0 [1]".
        if (memref.data.type == TRACE_TYPE_PREFETCHT0 && memref.data.addr == 1) {
            report_if_false(shard,
                            type_is_instr(shard->prev_entry_.instr.type) &&
                                shard->prev_prev_entry_.marker.type ==
                                    TRACE_TYPE_MARKER &&
                                shard->last_xfer_marker_.marker.marker_type ==
                                    TRACE_MARKER_TYPE_KERNEL_EVENT,
                            "Signal handler not immediately after signal marker");
            shard->app_handler_pc_ = shard->prev_entry_.instr.addr;
        }
        // Look for annotations where signal_invariants.c and rseq.c pass info to us on
        // what to check for.  We assume the app does not have prefetch instrs w/ low
        // addresses.
        if (memref.data.type == TRACE_TYPE_PREFETCHT2 && memref.data.addr < 1024) {
            shard->instrs_until_interrupt_ = static_cast<int>(memref.data.addr);
        }
        if (memref.data.type == TRACE_TYPE_PREFETCHT1 && memref.data.addr < 1024) {
            shard->memrefs_until_interrupt_ = static_cast<int>(memref.data.addr);
        }
    }
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        shard->prev_entry_.marker.type == TRACE_TYPE_MARKER &&
        shard->prev_entry_.marker.marker_type == TRACE_MARKER_TYPE_RSEQ_ABORT) {
        // The rseq marker must be immediately prior to the kernel event marker.
        report_if_false(shard,
                        memref.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT,
                        "Rseq marker not immediately prior to kernel marker");
    }
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_RSEQ_ENTRY) {
        shard->in_rseq_region_ = true;
        shard->rseq_start_pc_ = 0;
        shard->rseq_end_pc_ = memref.marker.marker_value;
    } else if (shard->in_rseq_region_) {
        if (type_is_instr(memref.instr.type)) {
            if (shard->rseq_start_pc_ == 0)
                shard->rseq_start_pc_ = memref.instr.addr;
            if (memref.instr.addr + memref.instr.size == shard->rseq_end_pc_) {
                // Completed normally.
                shard->in_rseq_region_ = false;
            } else if (memref.instr.addr >= shard->rseq_start_pc_ &&
                       memref.instr.addr < shard->rseq_end_pc_) {
                // Still in the region.
            } else {
                // We should see an abort marker or a side exit if we leave the region.
                report_if_false(
                    shard, type_is_instr_branch(shard->prev_instr_.memref.instr.type),
                    "Rseq region exit requires marker, branch, or commit");
                shard->in_rseq_region_ = false;
            }
        } else {
            report_if_false(
                shard,
                memref.marker.type != TRACE_TYPE_MARKER ||
                    memref.marker.marker_type != TRACE_MARKER_TYPE_KERNEL_EVENT ||
                    // Side exit.
                    type_is_instr_branch(shard->prev_instr_.memref.instr.type),
                "Signal in rseq region should have abort marker");
        }
    }
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_RSEQ_ABORT) {
        // Check that the rseq final instruction was not executed: that raw2trace
        // rolled it back, unless it was a fault in the instrumented execution in which
        // case the marker value will point to it.
        report_if_false(shard,
                        shard->rseq_end_pc_ == 0 ||
                            shard->prev_instr_.memref.instr.addr +
                                    shard->prev_instr_.memref.instr.size !=
                                shard->rseq_end_pc_ ||
                            shard->prev_instr_.memref.instr.addr ==
                                memref.marker.marker_value,
                        "Rseq post-abort instruction not rolled back");
        shard->in_rseq_region_ = false;
    }
#endif

    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_FILETYPE) {
        shard->file_type_ = static_cast<offline_file_type_t>(memref.marker.marker_value);
        shard->saw_filetype_ = true;
        report_if_false(shard,
                        is_a_unit_test(shard) ||
                            shard->file_type_ == shard->stream->get_filetype(),
                        "Stream interface filetype != trace marker");
    }
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_INSTRUCTION_COUNT) {
        shard->found_instr_count_marker_ = true;
        report_if_false(shard,
                        memref.marker.marker_value >= shard->last_instr_count_marker_,
                        "Instr count markers not increasing");
        shard->last_instr_count_marker_ = memref.marker.marker_value;
    }
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_CACHE_LINE_SIZE) {
        shard->found_cache_line_size_marker_ = true;
        report_if_false(shard,
                        is_a_unit_test(shard) ||
                            memref.marker.marker_value ==
                                shard->stream->get_cache_line_size(),
                        "Stream interface cache line size != trace marker");
    }
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_PAGE_SIZE) {
        shard->found_page_size_marker_ = true;
        report_if_false(shard,
                        is_a_unit_test(shard) || is_a_unit_test(shard) ||
                            memref.marker.marker_value == shard->stream->get_page_size(),
                        "Stream interface page size != trace marker");
    }
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_VECTOR_LENGTH) {
#ifdef AARCH64
        static const int MAX_VL_BYTES = 256; // SVE's maximum vector length is 2048-bit
        // Vector length must be a multiple of 16 bytes between 16 and 256.
        report_if_false(shard,
                        memref.marker.marker_value > 0 &&
                            memref.marker.marker_value <= MAX_VL_BYTES &&
                            memref.marker.marker_value % 16 == 0,
                        "Vector length marker has invalid size");

        const int new_vl_bits = memref.marker.marker_value * 8;
        if (dr_get_vector_length() != new_vl_bits) {
            dr_set_vector_length(new_vl_bits);
            // Changing the vector length can change the IR representation of some SVE
            // instructions but it doesn't effect any of the metadata that is stored
            // in decode_cache_ so we don't need to flush the cache.
        }
#else
        report_if_false(shard, false, "Unexpected vector length marker");
#endif
    }
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_VERSION) {
        shard->trace_version_ = memref.marker.marker_value;
        report_if_false(shard,
                        is_a_unit_test(shard) ||
                            memref.marker.marker_value == shard->stream->get_version(),
                        "Stream interface version != trace marker");
    }
    // Ensure each syscall instruction has a marker immediately afterward.  An
    // asynchronous signal could be delivered after the tracer recorded the syscall
    // instruction but before DR executed the syscall itself (xref i#5790) but raw2trace
    // removes the syscall instruction in such cases.
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_SYSCALL) {
        shard->found_syscall_marker_ = true;
        shard->prev_was_syscall_marker_ = true;
        shard->last_syscall_marker_value_ = static_cast<int>(memref.marker.marker_value);
        ++shard->syscall_count_;
        // TODO i#5949: For WOW64 instr_is_syscall() always returns false here as it
        // tries to check adjacent instrs; we disable this check until that is solved.
#if !defined(WINDOWS) || defined(X64)
        if (shard->prev_instr_.decoding.has_valid_decoding) {
            report_if_false(shard, shard->prev_instr_.decoding.is_syscall,
                            "Syscall marker not placed after syscall instruction");
        }
#endif
        shard->expect_syscall_marker_ = false;
        // TODO i#5949,i#6599: On WOW64 the "syscall" call is delayed by raw2trace
        // acros the 2nd timestamp+cpuid; we disable this check until that is solved.
#if !defined(WINDOWS) || defined(X64)
        // We expect an immediately preceding timestamp + cpuid.
        if (shard->trace_version_ >= TRACE_ENTRY_VERSION_FREQUENT_TIMESTAMPS) {
            report_if_false(
                shard,
                shard->prev_entry_.marker.type == TRACE_TYPE_MARKER &&
                    shard->prev_entry_.marker.marker_type == TRACE_MARKER_TYPE_CPU_ID &&
                    shard->prev_prev_entry_.marker.type == TRACE_TYPE_MARKER &&
                    shard->prev_prev_entry_.marker.marker_type ==
                        TRACE_MARKER_TYPE_TIMESTAMP,
                "Syscall marker not preceded by timestamp + cpuid");
        }
#endif
    }
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL) {
        shard->found_blocking_marker_ = true;
        // Re-assign the saved value to the shard state to allow this intervening
        // maybe_blocking marker.
        shard->prev_was_syscall_marker_ = prev_was_syscall_marker_saved;
        report_if_false(
            shard,
            (shard->prev_entry_.marker.type == TRACE_TYPE_MARKER &&
             shard->prev_entry_.marker.marker_type == TRACE_MARKER_TYPE_SYSCALL) ||
                // In OFFLINE_FILE_TYPE_ARCH_REGDEPS traces we remove
                // TRACE_MARKER_TYPE_SYSCALL markers, hence we disable this check for
                // these traces.
                TESTANY(OFFLINE_FILE_TYPE_ARCH_REGDEPS, shard->file_type_),
            "Maybe-blocking marker not preceded by syscall marker");
    }

    // Invariant: each chunk's instruction count must be identical and equal to
    // the value in the top-level marker.
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT) {
        shard->chunk_instr_count_ = memref.marker.marker_value;
        report_if_false(shard,
                        is_a_unit_test(shard) ||
                            shard->chunk_instr_count_ ==
                                shard->stream->get_chunk_instr_count(),
                        "Stream interface chunk instr count != trace marker");
    }
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_CHUNK_FOOTER) {
        report_if_false(shard,
                        shard->skipped_instrs_ ||
                            (shard->chunk_instr_count_ != 0 &&
                             shard->instr_count_ % shard->chunk_instr_count_ == 0),
                        "Chunk instruction counts are inconsistent");
    }

    // Invariant: a function marker should not appear between an instruction and its
    // memrefs or in the middle of a block (we assume elision is turned off and so a
    // callee entry will always be the top of a block).  (We don't check for other types
    // of markers b/c a virtual2physical one *could* appear in between.)
    if (shard->prev_entry_.marker.type == TRACE_TYPE_MARKER &&
        marker_type_is_function_marker(shard->prev_entry_.marker.marker_type)) {
        report_if_false(shard,
                        memref.data.type != TRACE_TYPE_READ &&
                            memref.data.type != TRACE_TYPE_WRITE &&
                            !type_is_prefetch(memref.data.type),
                        "Function marker misplaced between instr and memref");
    }
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_SYSCALL_TRACE_START) {
        report_if_false(shard,
                        TESTANY(OFFLINE_FILE_TYPE_KERNEL_SYSCALLS |
                                    OFFLINE_FILE_TYPE_KERNEL_SYSCALL_INSTR_ONLY |
                                    OFFLINE_FILE_TYPE_KERNEL_SYSCALL_TRACE_TEMPLATES,
                                shard->file_type_),
                        "Found kernel syscall trace without corresponding file type");
        report_if_false(shard, !shard->between_kernel_syscall_trace_markers_,
                        "Nested kernel syscall traces are not expected");

        // PT kernel syscall traces are inserted at the TRACE_MARKER_TYPE_SYSCALL_IDX
        // marker. The marker is deliberately added to the trace in the post-syscall
        // callback to ensure it is emitted together with the actual PT trace and not
        // before. If a signal interrupts the syscall, the
        // TRACE_MARKER_TYPE_SYSCALL_IDX marker may separate away from the
        // TRACE_MARKER_TYPE_SYSCALL marker.
        // TODO i#5505: This case needs more thought. Do we pause PT tracing on entry
        // to the signal handler? Do we discard the PT trace collected until then?
        if (!TESTANY(OFFLINE_FILE_TYPE_KERNEL_SYSCALL_TRACE_TEMPLATES |
                         OFFLINE_FILE_TYPE_KERNEL_SYSCALL_INSTR_ONLY,
                     shard->file_type_)) {
            report_if_false(shard, prev_was_syscall_marker_saved,
                            "System call trace found without prior syscall marker");
            report_if_false(shard,
                            shard->last_syscall_marker_value_ ==
                                static_cast<int>(memref.marker.marker_value),
                            "Mismatching syscall num in trace start and syscall marker");
            report_if_false(shard,
                            !TESTANY(OFFLINE_FILE_TYPE_ENCODINGS, shard->file_type_) ||
                                shard->prev_instr_.decoding.is_syscall,
                            "prev_instr at syscall trace start is not a syscall");
        }
        shard->pre_syscall_trace_instr_ = shard->prev_instr_;
        shard->between_kernel_syscall_trace_markers_ = true;
    }
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_SYSCALL_TRACE_END) {
        report_if_false(shard, shard->between_kernel_syscall_trace_markers_,
                        "Found kernel syscall trace end without start");
        report_if_false(shard,
                        shard->last_syscall_marker_value_ ==
                                static_cast<int>(memref.marker.marker_value) ||
                            TESTANY(OFFLINE_FILE_TYPE_KERNEL_SYSCALL_TRACE_TEMPLATES,
                                    shard->file_type_),
                        "Mismatching syscall num in trace end and syscall marker");
        shard->between_kernel_syscall_trace_markers_ = false;
        // For future checks, pretend that the previous instr was the instr just
        // before the system call trace start.
        if (shard->pre_syscall_trace_instr_.memref.instr.addr > 0) {
            // TODO i#5505: Ideally the last instruction in the system call PT trace
            // or the system call trace template would be an indirect CTI with a
            // TRACE_MARKER_TYPE_BRANCH_TARGET marker pointing to the next user-space
            // instr. For PT traces on x86, as also mentioned in the comment in
            // ir2trace.cpp, there are noise instructions at the end of the PT syscall
            // trace that need to be removed. Also check the kernel-to-user transition
            // when that is fixed.
            shard->prev_instr_ = shard->pre_syscall_trace_instr_;
            shard->pre_syscall_trace_instr_ = {};
        }
    }
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_CONTEXT_SWITCH_START) {
        report_if_false(shard, !shard->between_kernel_context_switch_markers_,
                        "Nested kernel context switch traces are not expected");
        shard->pre_context_switch_trace_instr_ = shard->prev_instr_;
        shard->between_kernel_context_switch_markers_ = true;
    }
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_CONTEXT_SWITCH_END) {
        report_if_false(shard, shard->between_kernel_context_switch_markers_,
                        "Found kernel context switch trace end without start");
        shard->between_kernel_context_switch_markers_ = false;
        // For future checks, pretend that the previous instr was the instr just
        // before the context switch trace start.
        if (shard->pre_context_switch_trace_instr_.memref.instr.addr > 0) {
            shard->prev_instr_ = shard->pre_context_switch_trace_instr_;
            shard->pre_context_switch_trace_instr_ = {};
        }
    }
    if (!is_a_unit_test(shard)) {
        report_if_false(shard,
                        (shard->between_kernel_syscall_trace_markers_ ||
                         shard->between_kernel_context_switch_markers_) ==
                            shard->stream->is_record_kernel(),
                        "Stream is_record_kernel() inaccurate");
    }
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        marker_type_is_function_marker(memref.marker.marker_type)) {
        if (memref.marker.marker_type == TRACE_MARKER_TYPE_FUNC_ID) {
            shard->prev_func_id_ = memref.marker.marker_value;
        }
        if (memref.marker.marker_type == TRACE_MARKER_TYPE_FUNC_RETADDR &&
            !shard->retaddr_stack_.empty()) {
            // Current check does not handle long jump, it may fail if a long
            // jump is used.
            report_if_false(shard,
                            memref.marker.marker_value == shard->retaddr_stack_.top(),
                            "Function marker retaddr should match prior call");
        }
        // Function markers may appear in the beginning of the trace before any
        // instructions are recorded, i.e. shard->instr_count_ == 0. In other to avoid
        // false positives, we assume the markers are placed correctly.
#ifdef UNIX
        report_if_false(
            shard,
            shard->prev_func_id_ >=
                    static_cast<uintptr_t>(func_trace_t::TRACE_FUNC_ID_SYSCALL_BASE) ||
                type_is_instr_branch(shard->prev_instr_.memref.instr.type) ||
                shard->instr_count_ == 0 ||
                (shard->prev_xfer_marker_.marker.marker_type ==
                     TRACE_MARKER_TYPE_KERNEL_XFER &&
                 (
                     // The last instruction is not known if the signal arrived before any
                     // instructions in the trace, or the trace started mid-signal. We
                     // assume the function markers are correct to avoid false positives.
                     shard->last_signal_context_.pre_signal_instr.memref.instr.addr ==
                         0 ||
                     // The last instruction of the outer-most scope was a branch.
                     type_is_instr_branch(
                         shard->last_instr_in_cur_context_.memref.instr.type))),
            "Function marker should be after a branch");
#else
        report_if_false(
            shard,
            shard->prev_func_id_ >=
                    static_cast<uintptr_t>(func_trace_t::TRACE_FUNC_ID_SYSCALL_BASE) ||
                type_is_instr_branch(shard->prev_instr_.memref.instr.type) ||
                shard->instr_count_ == 0,
            "Function marker should be after a branch");
#endif
    }

    if (memref.exit.type == TRACE_TYPE_THREAD_EXIT) {
        shard->saw_thread_exit_ = true;
        report_if_false(shard,
                        !TESTANY(OFFLINE_FILE_TYPE_FILTERED | OFFLINE_FILE_TYPE_IFILTERED,
                                 shard->file_type_) ||
                            shard->found_instr_count_marker_,
                        "Missing instr count markers");
        report_if_false(shard,
                        shard->found_cache_line_size_marker_ ||
                            (shard->skipped_instrs_ && !is_a_unit_test(shard) &&
                             shard->stream->get_cache_line_size() > 0),
                        "Missing cache line marker");
        report_if_false(shard,
                        shard->found_page_size_marker_ ||
                            (shard->skipped_instrs_ && !is_a_unit_test(shard) &&
                             shard->stream->get_page_size() > 0),
                        "Missing page size marker");
        report_if_false(
            shard,
            shard->found_syscall_marker_ ==
                    // Making sure this is a bool for a safe comparison.
                    static_cast<bool>(
                        TESTANY(OFFLINE_FILE_TYPE_SYSCALL_NUMBERS, shard->file_type_)) ||
                shard->syscall_count_ == 0,
            "System call numbers presence does not match filetype");
        // We can't easily identify blocking syscalls ourselves so we only check
        // one direction here.
        report_if_false(
            shard,
            !shard->found_blocking_marker_ ||
                TESTANY(OFFLINE_FILE_TYPE_BLOCKING_SYSCALLS, shard->file_type_),
            "Kernel scheduling marker presence does not match filetype");
        report_if_false(
            shard,
            !TESTANY(OFFLINE_FILE_TYPE_BIMODAL_FILTERED_WARMUP, shard->file_type_) ||
                shard->saw_filter_endpoint_marker_,
            "Expected to find TRACE_MARKER_TYPE_FILTER_ENDPOINT for the given file type");
        if (knob_test_name_ == "filter_asm_instr_count") {
            static constexpr int ASM_INSTR_COUNT = 133;
            report_if_false(shard, shard->last_instr_count_marker_ == ASM_INSTR_COUNT,
                            "Incorrect instr count marker value");
        }
        if (!TESTANY(OFFLINE_FILE_TYPE_FILTERED | OFFLINE_FILE_TYPE_IFILTERED,
                     shard->file_type_)) {
            report_if_false(
                shard,
                type_is_instr(shard->prev_instr_.memref.instr.type) ||
                    shard->prev_instr_.memref.instr.type == TRACE_TYPE_PREFETCH_INSTR ||
                    shard->prev_instr_.memref.instr.type == TRACE_TYPE_INSTR_NO_FETCH,
                "An unfiltered thread should have at least 1 instruction");
        }
    }
    if (shard->prev_entry_.marker.type == TRACE_TYPE_MARKER &&
        shard->prev_entry_.marker.marker_type == TRACE_MARKER_TYPE_PHYSICAL_ADDRESS) {
        // A physical address marker must be immediately prior to its virtual marker.
        report_if_false(shard,
                        memref.marker.type == TRACE_TYPE_MARKER &&
                            memref.marker.marker_type ==
                                TRACE_MARKER_TYPE_VIRTUAL_ADDRESS,
                        "Physical addr marker not immediately prior to virtual marker");
        // We don't have the actual page size, but it is always at least 4K, so
        // make sure the bottom 12 bits are the same.
        report_if_false(shard,
                        (memref.marker.marker_value & 0xfff) ==
                            (shard->prev_entry_.marker.marker_value & 0xfff),
                        "Physical addr bottom 12 bits do not match virtual");
    }

    if (type_is_instr(memref.instr.type) ||
        memref.instr.type == TRACE_TYPE_PREFETCH_INSTR ||
        memref.instr.type == TRACE_TYPE_INSTR_NO_FETCH) {
        // We'd prefer to report this error at the syscall instr but it is easier
        // to wait until here:
        report_if_false(shard,
                        !TESTANY(OFFLINE_FILE_TYPE_SYSCALL_NUMBERS, shard->file_type_) ||
                            !shard->expect_syscall_marker_,
                        "Syscall marker missing after syscall instruction");

        per_shard_t::instr_info_t cur_instr_info;
        const bool expect_encoding =
            TESTANY(OFFLINE_FILE_TYPE_ENCODINGS, shard->file_type_);
        if (expect_encoding) {
            const app_pc trace_pc = reinterpret_cast<app_pc>(memref.instr.addr);
            // Clear cache entry for new encodings.
            if (memref.instr.encoding_is_new) {
                shard->decode_cache_.erase(trace_pc);
            }
            auto cached = shard->decode_cache_.find(trace_pc);
            if (cached != shard->decode_cache_.end()) {
                cur_instr_info.decoding = cached->second;
            } else {
                instr_noalloc_t noalloc;
                instr_noalloc_init(drcontext_, &noalloc);
                instr_t *noalloc_instr = instr_from_noalloc(&noalloc);
                app_pc next_pc = decode_from_copy(
                    drcontext_, const_cast<app_pc>(memref.instr.encoding), trace_pc,
                    noalloc_instr);
                // Add decoding attributes to cur_instr_info.
                if (next_pc != nullptr) {
                    cur_instr_info.decoding.has_valid_decoding = true;
                    cur_instr_info.decoding.is_prefetch =
                        instr_is_prefetch(noalloc_instr);
                    cur_instr_info.decoding.opcode = instr_get_opcode(noalloc_instr);
#ifdef X86
                    cur_instr_info.decoding.is_xsave = instr_is_xsave(noalloc_instr);
                    cur_instr_info.decoding.is_xrstor = instr_is_xrstor(noalloc_instr);
#endif
                    cur_instr_info.decoding.is_syscall = instr_is_syscall(noalloc_instr);
                    cur_instr_info.decoding.writes_memory =
                        instr_writes_memory(noalloc_instr);
                    cur_instr_info.decoding.is_predicated =
                        instr_is_predicated(noalloc_instr);
                    // DR_ISA_REGDEPS instructions don't have an opcode, hence we use
                    // their category to determine whether they perform at least one read
                    // or write.
                    if (instr_get_isa_mode(noalloc_instr) == DR_ISA_REGDEPS) {
                        cur_instr_info.decoding.num_memory_read_access =
                            TESTANY(DR_INSTR_CATEGORY_LOAD,
                                    instr_get_category(noalloc_instr))
                            ? 1
                            : 0;
                        cur_instr_info.decoding.num_memory_write_access =
                            TESTANY(DR_INSTR_CATEGORY_STORE,
                                    instr_get_category(noalloc_instr))
                            ? 1
                            : 0;
                    } else {
                        cur_instr_info.decoding.num_memory_read_access =
                            instr_num_memory_read_access(noalloc_instr);
                        cur_instr_info.decoding.num_memory_write_access =
                            instr_num_memory_write_access(noalloc_instr);
                    }
                    if (type_is_instr_branch(memref.instr.type) &&
                        // DR_ISA_REGDEPS instructions don't have a branch target saved as
                        // instr.src[0], so we cannot retrieve this information.
                        !TESTANY(OFFLINE_FILE_TYPE_ARCH_REGDEPS, shard->file_type_)) {
                        const opnd_t target = instr_get_target(noalloc_instr);
                        if (opnd_is_pc(target)) {
                            cur_instr_info.decoding.branch_target =
                                reinterpret_cast<addr_t>(opnd_get_pc(target));
                        }
                    }
                }
                shard->decode_cache_[trace_pc] = cur_instr_info.decoding;
            }
#ifdef X86
            if (cur_instr_info.decoding.opcode == OP_sti)
                shard->instrs_since_sti = 0;
            else
                ++shard->instrs_since_sti;
#endif
            if (TESTANY(OFFLINE_FILE_TYPE_SYSCALL_NUMBERS, shard->file_type_) &&
                cur_instr_info.decoding.is_syscall)
                shard->expect_syscall_marker_ = true;
            if (cur_instr_info.decoding.has_valid_decoding &&
                !cur_instr_info.decoding.is_predicated &&
                !TESTANY(OFFLINE_FILE_TYPE_FILTERED | OFFLINE_FILE_TYPE_DFILTERED,
                         shard->file_type_)) {
                // Verify the number of read/write records matches the last
                // operand. Skip D-filtered traces which don't have every load or
                // store records.
                report_if_false(
                    shard,
                    (shard->expected_read_records_ == 0 ||
                     // Some prefetches did not have any corresponding
                     // memory access in system call trace templates
                     // collected on QEMU.
                     (shard->between_kernel_syscall_trace_markers_ &&
                      shard->prev_instr_.decoding.is_prefetch)) ||
                        // We cannot check for is_predicated in
                        // OFFLINE_FILE_TYPE_ARCH_REGDEPS traces (it's always false), so
                        // we disable this check.
                        TESTANY(OFFLINE_FILE_TYPE_ARCH_REGDEPS, shard->file_type_),
                    "Missing read records");
                shard->expected_read_records_ = 0;
                report_if_false(
                    shard,
                    shard->expected_write_records_ == 0 ||
                        // We cannot check for is_predicated in
                        // OFFLINE_FILE_TYPE_ARCH_REGDEPS traces (it's always false), so
                        // we disable this check.
                        TESTANY(OFFLINE_FILE_TYPE_ARCH_REGDEPS, shard->file_type_),
                    "Missing write records");

                if (!(shard->between_kernel_syscall_trace_markers_ &&
                      TESTANY(OFFLINE_FILE_TYPE_KERNEL_SYSCALL_INSTR_ONLY,
                              shard->file_type_))) {
                    shard->expected_read_records_ =
                        cur_instr_info.decoding.num_memory_read_access;
                    shard->expected_write_records_ =
                        cur_instr_info.decoding.num_memory_write_access;
                }
            }
        }
        // We need to assign the memref variable of cur_instr_info here. The memref
        // variable is not cached as it can dynamically vary based on data values.
        cur_instr_info.memref = memref;
        if (knob_verbose_ >= 3) {
            std::cerr << "::" << memref.data.pid << ":" << memref.data.tid << ":: "
                      << " @" << (void *)memref.instr.addr
                      << ((memref.instr.type == TRACE_TYPE_INSTR_NO_FETCH)
                              ? " non-fetched"
                              : "")
                      << " instr x" << memref.instr.size << "\n";
        }
#ifdef UNIX
        report_if_false(shard, shard->instrs_until_interrupt_ != 0,
                        "Interruption marker too late");
        if (shard->instrs_until_interrupt_ > 0)
            --shard->instrs_until_interrupt_;
#endif

        // retaddr_stack_ is used for verifying invariants related to the function
        // return markers; these are only user-space.
        if (!(shard->between_kernel_context_switch_markers_ ||
              shard->between_kernel_syscall_trace_markers_)) {
            if (memref.instr.type == TRACE_TYPE_INSTR_DIRECT_CALL ||
                memref.instr.type == TRACE_TYPE_INSTR_INDIRECT_CALL) {
                shard->retaddr_stack_.push(memref.instr.addr + memref.instr.size);
            }
            if (memref.instr.type == TRACE_TYPE_INSTR_RETURN) {
                if (!shard->retaddr_stack_.empty()) {
                    shard->retaddr_stack_.pop();
                }
            }
        }
        // Invariant: offline traces guarantee that a branch target must immediately
        // follow the branch w/ no intervening thread switch.
        // If we did serial analyses only, we'd just track the previous instr in the
        // interleaved stream.  Here we look for headers indicating where an interleaved
        // stream *could* switch threads, so we're stricter than necessary.
        if (knob_offline_ && type_is_instr_branch(shard->prev_instr_.memref.instr.type)) {
            report_if_false(
                shard,
                !shard->saw_timestamp_but_no_instr_ ||
                    // The invariant is relaxed for a signal.
                    // prev_xfer_marker_ is cleared on an instr, so if set to
                    // non-sentinel it means it is immediately prior, in
                    // between prev_instr_ and memref.
                    shard->prev_xfer_marker_.marker.marker_type ==
                        TRACE_MARKER_TYPE_KERNEL_EVENT ||
                    // Instruction-filtered are exempted.
                    TESTANY(OFFLINE_FILE_TYPE_FILTERED | OFFLINE_FILE_TYPE_IFILTERED,
                            shard->file_type_),
                "Branch target not immediately after branch");
        }
        // Invariant: non-explicit control flow (i.e., kernel-mediated) is indicated
        // by markers. We are using prev_instr_ here instead of last_instr_in_cur_context_
        // because after a signal the interruption and resumption checks are done
        // elsewhere.
        const std::string non_explicit_flow_violation_msg = check_for_pc_discontinuity(
            shard, shard->prev_instr_, cur_instr_info, expect_encoding,
            /*at_kernel_event=*/false);
        report_if_false(shard, non_explicit_flow_violation_msg.empty(),
                        non_explicit_flow_violation_msg);

#ifdef UNIX
        // Ensure signal handlers return to the interruption point.
        if (shard->prev_xfer_marker_.marker.marker_type ==
            TRACE_MARKER_TYPE_KERNEL_XFER) {
            // For the following checks, we use the values popped from the
            // signal_stack_ into last_signal_context_ at the last
            // TRACE_MARKER_TYPE_KERNEL_XFER marker.
            bool kernel_event_marker_equality =
                // Skip this check if we did not see the corresponding
                // kernel_event marker in the trace because the trace
                // started mid-signal.
                shard->last_signal_context_.xfer_int_pc == 0 ||
                // Regular check for equality with kernel event marker pc.
                memref.instr.addr == shard->last_signal_context_.xfer_int_pc ||
                // DR hands us a different address for sysenter than the
                // resumption point.
                shard->last_signal_context_.pre_signal_instr.memref.instr.type ==
                    TRACE_TYPE_INSTR_SYSENTER // clang-format off
                // The AArch64 scatter/gather expansion test skips faulting
                // instructions in the signal handler by incrementing the PC.
                IF_AARCH64(||
                           (knob_test_name_ == "scattergather" &&
                            memref.instr.addr ==
                                shard->last_signal_context_.xfer_int_pc + 4));
            // clang-format on
            report_if_false(
                shard,
                kernel_event_marker_equality ||
                    // Nested signal.  XXX: This only works for our annotated test
                    // signal_invariants where we know shard->app_handler_pc_.
                    memref.instr.addr == shard->app_handler_pc_ ||
                    // Marker for rseq abort handler. Not as unique as a prefetch, but
                    // we need an instruction and not a data type.
                    memref.instr.type == TRACE_TYPE_INSTR_DIRECT_JUMP ||
                    // Instruction-filtered can easily skip the return point.
                    TESTANY(OFFLINE_FILE_TYPE_FILTERED | OFFLINE_FILE_TYPE_IFILTERED,
                            shard->file_type_),
                "Signal handler return point incorrect");
        }
        // last_instr_in_cur_context_ is recorded as the pre-signal instr when we see a
        // TRACE_MARKER_TYPE_KERNEL_EVENT marker. Note that we cannot perform this
        // book-keeping using prev_instr_ on the TRACE_MARKER_TYPE_KERNEL_EVENT marker.
        // E.g. if there was no instr between two nested signals, we do not want to
        // record any pre-signal instr for the second signal.
        shard->last_instr_in_cur_context_ = cur_instr_info;
#endif
        shard->prev_instr_ = cur_instr_info;
        // Clear prev_xfer_marker_ on an instr (not a memref which could come between an
        // instr and a kernel-mediated far-away instr) to ensure it's *immediately*
        // prior (i#3937).
        shard->prev_xfer_marker_.marker.marker_type = TRACE_MARKER_TYPE_VERSION;
        shard->saw_timestamp_but_no_instr_ = false;
        // Clear window transitions on instrs.
        shard->window_transition_ = false;
    } else if (knob_verbose_ >= 3) {
        std::cerr << "::" << memref.data.pid << ":" << memref.data.tid << ":: "
                  << " type " << memref.instr.type << "\n";
    }

    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP) {
#ifdef X86_32
        // i#5634: Truncated for 32-bit, as documented.
        // A 32 bit timestamp rolls over every 4294 seconds, so it needs to be
        // considered when timestamps are compared. The check assumes two
        // consecutive timestamps will never be more than 2^31 microseconds
        // (2147 seconds) apart.
        const uintptr_t last_timestamp = static_cast<uintptr_t>(shard->last_timestamp_);
        if (memref.marker.marker_value < last_timestamp) {
            report_if_false(shard,
                            last_timestamp >
                                (memref.marker.marker_value +
                                 (std::numeric_limits<uintptr_t>::max)() / 2),
                            "Timestamp does not increase monotonically");
        }
#else
        report_if_false(shard, memref.marker.marker_value >= shard->last_timestamp_,
                        "Timestamp does not increase monotonically");
#endif
        shard->last_timestamp_ = memref.marker.marker_value;
        shard->saw_timestamp_but_no_instr_ = true;
        // Reset this since we just saw a timestamp marker.
        shard->instr_count_since_last_timestamp_ = 0;
        if (knob_verbose_ >= 3) {
            std::cerr << "::" << memref.data.pid << ":" << memref.data.tid << ":: "
                      << " timestamp " << memref.marker.marker_value << "\n";
        }
    }
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_CPU_ID) {
        shard->sched_data_.record_cpu_id(shard->tid_, memref.marker.marker_value,
                                         shard->last_timestamp_, shard->instr_count_);
    }

#ifdef UNIX
    bool saw_rseq_abort = false;
#endif
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        // Ignore timestamp, etc. markers which show up at signal delivery boundaries
        // b/c the tracer does a buffer flush there.
        (memref.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT ||
         memref.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_XFER)) {
        if (knob_verbose_ >= 3) {
            std::cerr << "::" << memref.data.pid << ":" << memref.data.tid << ":: "
                      << "marker type " << memref.marker.marker_type << " value 0x"
                      << std::hex << memref.marker.marker_value << std::dec << "\n";
        }
        // Zero is pushed as a sentinel. This push matches the return used by post
        // signal handler to run the restorer code. It is assumed that all signal
        // handlers return normally and longjmp is not used.
        if (memref.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT) {
            // If the marker is preceded by an RSEQ ABORT marker, do not push the sentinel
            // since there will not be a corresponding return.
            if (shard->prev_entry_.marker.type != TRACE_TYPE_MARKER ||
                shard->prev_entry_.marker.marker_type != TRACE_MARKER_TYPE_RSEQ_ABORT) {
                shard->retaddr_stack_.push(0);
            }
        }
#ifdef UNIX
        report_if_false(shard, memref.marker.marker_value != 0,
                        "Kernel event marker value missing");
        if (memref.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_XFER) {
            // We assume paired signal entry-exit (so no longjmp and no rseq
            // inside signal handlers).
            if (shard->signal_stack_.empty()) {
                // This can happen if tracing started in the middle of a signal.
                // Try to continue by skipping the checks.
                shard->last_signal_context_ = { 0, {}, false };
                // We have not seen any instr in the outermost scope that we just
                // discovered.
                shard->last_instr_in_cur_context_ = {};
            } else {
                // The pre_signal_instr for this signal may be {} in some cases:
                // - for nested signals without any intervening instr
                // - if there's a signal at the very beginning of the trace
                // In both these cases the empty instr implies that it should not
                // be used for the pre-signal instr check.
                shard->last_signal_context_ = shard->signal_stack_.top();
                shard->signal_stack_.pop();
                // In the case where there's no instr between two consecutive signals
                // (at the same nesting depth), the pre-signal instr for the second
                // signal should be same as the pre-signal instr for the first one.
                // Here we restore last_instr_in_cur_context_ to the last instr we
                // saw *in the same nesting depth* before the first signal.
                shard->last_instr_in_cur_context_ =
                    shard->last_signal_context_.pre_signal_instr;
            }
        }
        if (memref.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT) {
            // If preceded by an RSEQ abort marker, this is not really a signal.
            if (shard->prev_entry_.marker.type == TRACE_TYPE_MARKER &&
                shard->prev_entry_.marker.marker_type == TRACE_MARKER_TYPE_RSEQ_ABORT) {
                saw_rseq_abort = true;
            } else {
                if (type_is_instr(shard->last_instr_in_cur_context_.memref.instr.type) &&
                    !shard->saw_rseq_abort_ &&
                    // XXX i#3937: Online traces do not place signal markers properly,
                    // so we can't precisely check for continuity there.
                    knob_offline_) {
                    per_shard_t::instr_info_t memref_info;
                    memref_info.memref = memref;
                    // Ensure no discontinuity between a prior instr and the interrupted
                    // PC, for non-rseq signals where we have the interrupted PC.
                    const std::string discontinuity = check_for_pc_discontinuity(
                        shard, shard->last_instr_in_cur_context_, memref_info,
                        TESTANY(OFFLINE_FILE_TYPE_ENCODINGS, shard->file_type_),
                        /*at_kernel_event=*/true);
                    const std::string error_msg_suffix = " @ kernel_event marker";
                    report_if_false(shard, discontinuity.empty(),
                                    discontinuity + error_msg_suffix);
                }
                shard->signal_stack_.push({ memref.marker.marker_value,
                                            shard->last_instr_in_cur_context_,
                                            shard->saw_rseq_abort_ });
                // XXX: if last_instr_in_cur_context_ is {} currently, it means this is
                // either a signal that arrived before the first instr in the trace, or
                // it's a nested signal without any intervening instr after its
                // outer-scope signal. For the latter case, we can check if the
                // TRACE_MARKER_TYPE_KERNEL_EVENT marker value is equal for both signals.

                // We start with an empty memref_t to denote absence of any pre-signal
                // instr for any subsequent nested signals.
                shard->last_instr_in_cur_context_ = {};
            }
        }
#endif
        shard->prev_xfer_marker_ = memref;
        shard->last_xfer_marker_ = memref;
    }
    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_WINDOW_ID) {
        if (shard->last_window_ != memref.marker.marker_value)
            shard->window_transition_ = true;
        shard->last_window_ = memref.marker.marker_value;
    }

    if (memref.marker.type == TRACE_TYPE_MARKER &&
        memref.marker.marker_type == TRACE_MARKER_TYPE_FILTER_ENDPOINT) {
        shard->saw_filter_endpoint_marker_ = true;
        report_if_false(
            shard, TESTANY(OFFLINE_FILE_TYPE_BIMODAL_FILTERED_WARMUP, shard->file_type_),
            "Found TRACE_MARKER_TYPE_FILTER_ENDPOINT without the correct file type");
    }

    if (knob_offline_ && shard->trace_version_ >= TRACE_ENTRY_VERSION_BRANCH_INFO) {
        bool is_indirect = false;
        if (type_is_instr_branch(memref.instr.type) &&
            // I-filtered traces don't mark branch targets.
            !TESTANY(OFFLINE_FILE_TYPE_FILTERED | OFFLINE_FILE_TYPE_IFILTERED,
                     shard->file_type_)) {
            report_if_false(
                shard, memref.instr.type != TRACE_TYPE_INSTR_CONDITIONAL_JUMP,
                "The CONDITIONAL_JUMP type is deprecated and should not appear");
            if (!type_is_instr_direct_branch(memref.instr.type)) {
                is_indirect = true;
                report_if_false(shard,
                                // We assume the app doesn't actually target PC=0.
                                memref.instr.indirect_branch_target != 0,
                                "Indirect branches must contain targets");
            }
        }
        if (type_is_instr(memref.instr.type) && !is_indirect) {
            report_if_false(shard, memref.instr.indirect_branch_target == 0,
                            "Indirect target should be 0 for non-indirect-branches");
        }
    }

#ifdef UNIX
    if (saw_rseq_abort) {
        shard->saw_rseq_abort_ = true;
    }
    // If a signal caused an rseq abort, the signal's KERNEL_EVENT marker
    // will be preceded by an RSEQ_ABORT-KERNEL_EVENT marker pair. There may
    // be a buffer switch (denoted by the timestamp+cpu pair) between the
    // RSEQ_ABORT-KERNEL_EVENT pair and the signal's KERNEL_EVENT marker. We
    // want to ignore such an intervening timestamp+cpu marker pair when
    // checking whether a signal caused an RSEQ abort.
    else if (!(memref.marker.type == TRACE_TYPE_MARKER &&
               (memref.marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP ||
                memref.marker.marker_type == TRACE_MARKER_TYPE_CPU_ID))) {
        shard->saw_rseq_abort_ = false;
    }
#endif
    shard->prev_prev_entry_ = shard->prev_entry_;
    shard->prev_entry_ = memref;
    if (type_is_instr_branch(shard->prev_entry_.instr.type))
        shard->last_branch_ = shard->prev_entry_;

    if (type_is_data(memref.data.type) &&
        shard->prev_instr_.decoding.has_valid_decoding) {
        // If the instruction is predicated, the check is skipped since we do
        // not have the data to determine how many memory accesses to expect.
        if (!shard->prev_instr_.decoding.is_predicated &&
            !TESTANY(OFFLINE_FILE_TYPE_FILTERED | OFFLINE_FILE_TYPE_DFILTERED,
                     shard->file_type_)) {
            if (type_is_read(memref.data.type)) {
                // Skip D-filtered traces which don't have every load or store records.
                report_if_false(
                    shard,
                    shard->expected_read_records_ >
                        0 IF_X86(|| relax_expected_read_count_check_for_kernel(shard)),
                    "Too many read records");
                if (shard->expected_read_records_ > 0 &&
                    // We cannot decrease the shard->expected_read_records_ counter in
                    // OFFLINE_FILE_TYPE_ARCH_REGDEPS traces because we cannot determine
                    // the exact number of loads a DR_ISA_REGDEPS instruction performs.
                    // If a DR_ISA_REGDEPS instruction has a DR_INSTR_CATEGORY_LOAD among
                    // its categories, we simply set
                    // cur_instr_info.decoding.num_memory_read_access to 1. We rely on the
                    // next instruction to re-set shard->expected_read_records_
                    // accordingly.
                    !TESTANY(OFFLINE_FILE_TYPE_ARCH_REGDEPS, shard->file_type_)) {
                    shard->expected_read_records_--;
                }
            } else {
                // Skip D-filtered traces which don't have every load or store records.
                report_if_false(
                    shard,
                    shard->expected_write_records_ >
                        0 IF_X86(|| relax_expected_write_count_check_for_kernel(shard)),
                    "Too many write records");
                if (shard->expected_write_records_ > 0 &&
                    // We cannot decrease the shard->expected_write_records_ counter in
                    // OFFLINE_FILE_TYPE_ARCH_REGDEPS traces because we cannot determine
                    // the exact number of stores a DR_ISA_REGDEPS instruction performs.
                    // If a DR_ISA_REGDEPS instruction has a DR_INSTR_CATEGORY_STORE among
                    // its categories, we simply set
                    // cur_instr_info.decoding.num_memory_write_access to 1. We rely on
                    // the next instruction to re-set shard->expected_write_records_
                    // accordingly.
                    !TESTANY(OFFLINE_FILE_TYPE_ARCH_REGDEPS, shard->file_type_)) {
                    shard->expected_write_records_--;
                }
            }
        }
    }
    // Run additional checks for OFFLINE_FILE_TYPE_ARCH_REGDEPS traces.
    if (TESTANY(OFFLINE_FILE_TYPE_ARCH_REGDEPS, shard->file_type_))
        check_regdeps_invariants(shard, memref);

    return true;
}

bool
invariant_checker_t::process_memref(const memref_t &memref)
{
    per_shard_t *per_shard;
    int shard_index = serial_stream_->get_shard_index();
    const auto &lookup = shard_map_.find(shard_index);
    if (lookup == shard_map_.end()) {
        auto per_shard_unique = std::unique_ptr<per_shard_t>(new per_shard_t);
        per_shard = per_shard_unique.get();
        per_shard->stream = serial_stream_;
        per_shard->tid_ = serial_stream_->get_tid();
        shard_map_[shard_index] = std::move(per_shard_unique);
    } else
        per_shard = lookup->second.get();
    if (!parallel_shard_memref(reinterpret_cast<void *>(per_shard), memref)) {
        error_string_ = per_shard->error_;
        return false;
    }
    return true;
}

void
invariant_checker_t::check_schedule_data(per_shard_t *global)
{
    if (serial_schedule_file_ == nullptr && cpu_schedule_file_ == nullptr)
        return;
    // Check that the scheduling data in the files written by raw2trace match
    // the data in the trace.
    // Use a synthetic stream object to allow report_if_false to work normally.
    auto stream = std::unique_ptr<memtrace_stream_t>(
        new default_memtrace_stream_t(&global->ref_count_));
    global->stream = stream.get();

    std::string err;
    schedule_file_t sched;
    for (auto &shard_keyval : shard_map_) {
        err = sched.merge_shard_data(shard_keyval.second->sched_data_);
        if (!err.empty()) {
            report_if_false(global, false, "Failed to merge schedule data: " + err);
            return;
        }
    }
    if (serial_schedule_file_ != nullptr) {
        const std::vector<schedule_entry_t> &serial = sched.get_full_serial_records();
        const std::vector<schedule_entry_t> &serial_redux = sched.get_serial_records();
        // For entries with the same timestamp, the order can differ.  We could
        // identify each such sequence and collect it into a set but it is simpler to
        // read the whole file and sort it the same way.
        schedule_file_t serial_reader;
        err = serial_reader.read_serial_file(serial_schedule_file_);
        if (!err.empty()) {
            report_if_false(global, false, "Failed to read serial schedule file: " + err);
            return;
        }
        const std::vector<schedule_entry_t> &serial_file =
            serial_reader.get_full_serial_records();
        if (knob_verbose_ >= 1) {
            std::cerr << "Serial schedule: read " << serial_file.size()
                      << " records from the file and observed " << serial.size()
                      << " transitions in the trace\n";
        }
        // We created both types of schedule and select which to compare against.
        const std::vector<schedule_entry_t> *tomatch;
        if (serial_file.size() == serial.size())
            tomatch = &serial;
        else if (serial_file.size() == serial_redux.size())
            tomatch = &serial_redux;
        else {
            report_if_false(global, false,
                            "Serial schedule entry count does not match trace");
            return;
        }
        for (int i = 0; i < static_cast<int>(tomatch->size()) &&
             i < static_cast<int>(serial_file.size());
             ++i) {
            global->ref_count_ = serial_file[i].start_instruction;
            global->tid_ = serial_file[i].thread;
            if (knob_verbose_ >= 1) {
                std::cerr << "Saw T" << (*tomatch)[i].thread << " on "
                          << (*tomatch)[i].cpu << " @" << (*tomatch)[i].timestamp << " "
                          << (*tomatch)[i].start_instruction << " vs file T"
                          << serial_file[i].thread << " on " << serial_file[i].cpu << " @"
                          << serial_file[i].timestamp << " "
                          << serial_file[i].start_instruction << "\n";
            }
            report_if_false(
                global,
                memcmp(&(*tomatch)[i], &serial_file[i], sizeof((*tomatch)[i])) == 0,
                "Serial schedule entry does not match trace");
        }
    }
    if (cpu_schedule_file_ == nullptr)
        return;

    const std::unordered_map<uint64_t, std::vector<schedule_entry_t>> &cpu2sched =
        sched.get_full_cpu_records();
    const std::unordered_map<uint64_t, std::vector<schedule_entry_t>> &cpu2sched_redux =
        sched.get_cpu_records();
    schedule_file_t cpu_reader;
    err = cpu_reader.read_cpu_file(cpu_schedule_file_);
    if (!err.empty()) {
        report_if_false(global, false, "Failed to read cpu schedule file: " + err);
        return;
    }
    const std::unordered_map<uint64_t, std::vector<schedule_entry_t>> &cpu2sched_file =
        cpu_reader.get_full_cpu_records();
    for (const auto &keyval : cpu2sched_file) {
        const std::vector<schedule_entry_t> *tomatch;
        if (keyval.second.size() == cpu2sched.find(keyval.first)->second.size())
            tomatch = &cpu2sched.find(keyval.first)->second;
        else if (keyval.second.size() ==
                 cpu2sched_redux.find(keyval.first)->second.size())
            tomatch = &cpu2sched_redux.find(keyval.first)->second;
        else {
            report_if_false(global, false,
                            "Cpu schedule entry count does not match trace");
            return;
        }
        for (int i = 0; i < static_cast<int>(tomatch->size()) &&
             i < static_cast<int>(keyval.second.size());
             ++i) {
            global->ref_count_ = keyval.second[i].start_instruction;
            global->tid_ = keyval.second[i].thread;
            report_if_false(
                global,
                memcmp(&(*tomatch)[i], &keyval.second[i], sizeof(keyval.second[i])) == 0,
                "Cpu schedule entry does not match trace");
        }
    }
}

bool
invariant_checker_t::print_results()
{
    if (serial_stream_ != nullptr) {
        for (const auto &keyval : shard_map_) {
            parallel_shard_exit(keyval.second.get());
        }
    }
    per_shard_t global;
    check_schedule_data(&global);
    uint64_t total_error_count = global.error_count_;
    if (!abort_on_invariant_error_) {
        for (const auto &keyval : shard_map_) {
            total_error_count += keyval.second->error_count_;
        }
    }
    if (total_error_count == 0) {
        std::cerr << "Trace invariant checks passed\n";
    } else {
        // XXX: Should the invariant checker cause an abort on finding invariant
        // errors? In some cases it's useful: unit tests where we want to fail
        // where the collected trace has invariant errors. This is currently
        // supported by the default setting of -abort_on_invariant_error which
        // aborts on finding the very first invariant error. In other cases,
        // aborting may not be appropriate as the invariant checker indeed
        // successfully did what it was supposed to (find/count invariant errors),
        // and there may be other tools that are running too; perhaps aborting
        // should be reserved only for cases when there's some internal fatal error
        // in the framework or the tool that cannot be recovered from. OTOH aborting,
        // even if at the end after finding all invariant errors, may be useful
        // to highlight that there were invariant errors in the trace. For now
        // we do not fail or return an error exit code on
        // -no_abort_on_invariant_error even if some invariant errors were found.
        std::cerr << "Found " << total_error_count << " invariant errors\n";
    }
    if (!shard_map_.empty()) {
        uint64_t filetype = shard_map_.begin()->second->file_type_;
        if (TESTANY(OFFLINE_FILE_TYPE_ARCH_REGDEPS, filetype)) {
            std::cerr << "WARNING: invariant_checker is being run on an "
                         "OFFLINE_FILE_TYPE_ARCH_REGDEPS trace.\nSome invariant checks "
                         "have been disabled.\n";
        }
    }
    return true;
}

std::string
invariant_checker_t::check_for_pc_discontinuity(
    per_shard_t *shard, const per_shard_t::instr_info_t &prev_instr_info,
    const per_shard_t::instr_info_t &cur_memref_info, bool expect_encoding,
    bool at_kernel_event)
{
    const memref_t prev_instr = prev_instr_info.memref;
    std::string error_msg = "";
    bool have_branch_target = false;
    addr_t branch_target = 0;
    const addr_t prev_instr_trace_pc = prev_instr.instr.addr;
    // cur_memref_info is a marker (not an instruction) if at_kernel_event is true.
    const addr_t cur_pc = at_kernel_event ? cur_memref_info.memref.marker.marker_value
                                          : cur_memref_info.memref.instr.addr;

    if (prev_instr_trace_pc == 0 /*first*/) {
        return "";
    }
    // We do not bother to support legacy traces without encodings.
    if (expect_encoding && type_is_instr_direct_branch(prev_instr.instr.type)) {
        if (!prev_instr_info.decoding.has_valid_decoding) {
            // Neither condition should happen but they could on an invalid
            // encoding from raw2trace or the reader so we report an
            // invariant rather than asserting.
            report_if_false(shard, false, "Branch target is not decodeable");
        } else {
            have_branch_target = true;
            branch_target = prev_instr_info.decoding.branch_target;
        }
    }
    // Check for all valid transitions except taken branches. We consider taken
    // branches later so that we can provide a different message for those
    // invariant violations.
    bool fall_through_allowed = !type_is_instr_branch(prev_instr.instr.type) ||
        prev_instr.instr.type == TRACE_TYPE_INSTR_CONDITIONAL_JUMP ||
        prev_instr.instr.type == TRACE_TYPE_INSTR_UNTAKEN_JUMP;
    const bool valid_nonbranch_flow =
        // Filtered.
        TESTANY(OFFLINE_FILE_TYPE_FILTERED | OFFLINE_FILE_TYPE_IFILTERED,
                shard->file_type_) ||
        // Regular fall-through.
        (fall_through_allowed && prev_instr_trace_pc + prev_instr.instr.size == cur_pc) ||
        // First instr of kernel system call trace.
        (shard->between_kernel_syscall_trace_markers_ &&
         (shard->prev_entry_.marker.type == TRACE_TYPE_MARKER &&
          shard->prev_entry_.marker.marker_type ==
              TRACE_MARKER_TYPE_SYSCALL_TRACE_START)) ||
        // First instr of kernel context switch trace.
        (shard->between_kernel_context_switch_markers_ &&
         (shard->prev_entry_.marker.type == TRACE_TYPE_MARKER &&
          shard->prev_entry_.marker.marker_type ==
              TRACE_MARKER_TYPE_CONTEXT_SWITCH_START)) ||
        // String loop.
        (prev_instr_trace_pc == cur_pc &&
         (cur_memref_info.memref.instr.type == TRACE_TYPE_INSTR_NO_FETCH ||
          // Online incorrectly marks the 1st string instr across a thread
          // switch as fetched.  We no longer emit timestamps in pipe splits so
          // we can't use saw_timestamp_but_no_instr_.  We can't just check for
          // prev_instr.instr_type being no-fetch as the prev might have been
          // a single instance, which is fetched.  We check the sizes for now.
          // TODO i#4915, #4948: Eliminate non-fetched and remove the
          // underlying instrs altogether, which would fix this for us.
          (!knob_offline_ &&
           cur_memref_info.memref.instr.size == prev_instr.instr.size))) ||
        // Same PC is allowed for a kernel interruption which may restart the
        // same instruction.
        (prev_instr_trace_pc == cur_pc && at_kernel_event) ||
        // Kernel-mediated, but we can't tell if we had a thread swap.
        (shard->prev_xfer_marker_.instr.tid != 0 && !at_kernel_event &&
         (shard->prev_xfer_marker_.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_EVENT ||
          shard->prev_xfer_marker_.marker.marker_type == TRACE_MARKER_TYPE_KERNEL_XFER ||
          shard->prev_xfer_marker_.marker.marker_type == TRACE_MARKER_TYPE_RSEQ_ABORT)) ||
#ifdef UNIX
        // In case of an RSEQ abort followed by a signal, the pre-signal-instr PC is
        // different from the interruption PC which is the RSEQ handler. If there is a
        // back-to-back signal without any intervening instructions, the kernel transfer
        // marker of the second signal should point to the same interruption PC, and not
        // the pre-signal-instr PC. The shard->last_signal_context_ has not been updated,
        // it still points to the previous signal context.
        (at_kernel_event && cur_pc == shard->last_signal_context_.xfer_int_pc &&
         prev_instr_trace_pc ==
             shard->last_signal_context_.pre_signal_instr.memref.instr.addr) ||
#endif
        // We expect a gap on a window transition.
        shard->window_transition_ ||
        prev_instr.instr.type ==
            TRACE_TYPE_INSTR_SYSENTER IF_X86(
                ||
                (shard->between_kernel_syscall_trace_markers_ &&
                 // hlt suspends processing until the next interrupt, which may be
                 // handled at a discontinuous PC.
                 (shard->prev_instr_.decoding.opcode == OP_hlt ||
                  // After interrupts are enabled, we may be interrupted. The tolerance
                  // of 2-instrs was found empirically on some x86 QEMU system call
                  // trace templates.
                  (shard->instrs_since_sti > 0 && shard->instrs_since_sti <= 2))));

    if (!valid_nonbranch_flow) {
        // Check if the type is a branch instruction and there is a branch target
        // mismatch.
        if (type_is_instr_branch(prev_instr.instr.type)) {
            if (knob_offline_ &&
                shard->trace_version_ >= TRACE_ENTRY_VERSION_BRANCH_INFO) {
                // We have precise branch target info.
                if (prev_instr.instr.type == TRACE_TYPE_INSTR_UNTAKEN_JUMP) {
                    branch_target = prev_instr_trace_pc + prev_instr.instr.size;
                    have_branch_target = true;
                } else if (!type_is_instr_direct_branch(prev_instr.instr.type)) {
                    branch_target = prev_instr.instr.indirect_branch_target;
                    have_branch_target = true;
                }
            }
            if (have_branch_target && branch_target != cur_pc &&
                // We cannot determine the branch target in OFFLINE_FILE_TYPE_ARCH_REGDEPS
                // traces because next_pc != cur_pc + instr.size and
                // indirect_branch_target is not saved in inst.src[0].
                // So, we skip this invariant error.
                !TESTANY(OFFLINE_FILE_TYPE_ARCH_REGDEPS, shard->file_type_)) {
                error_msg = "Branch does not go to the correct target";
            }
        } else if (cur_memref_info.decoding.has_valid_decoding &&
                   prev_instr_info.decoding.has_valid_decoding &&
                   cur_memref_info.decoding.is_syscall && cur_pc == prev_instr_trace_pc &&
                   prev_instr_info.decoding.is_syscall) {
            error_msg = "Duplicate syscall instrs with the same PC";
        } else if (prev_instr_info.decoding.has_valid_decoding &&
                   prev_instr_info.decoding.writes_memory &&
                   type_is_instr_conditional_branch(shard->last_branch_.instr.type)) {
            // This sequence happens when an rseq side exit occurs which
            // results in missing instruction in the basic block.
            error_msg = "PC discontinuity due to rseq side exit";
        } else {
            error_msg = "Non-explicit control flow has no marker";
        }
    }

    return error_msg;
}

void
invariant_checker_t::check_regdeps_invariants(per_shard_t *shard, const memref_t &memref)
{
    // Check instructions.
    if (type_is_instr(memref.instr.type)) {
        const bool expect_encoding =
            TESTANY(OFFLINE_FILE_TYPE_ENCODINGS, shard->file_type_);
        if (expect_encoding) {
            // Decode memref_t to get an instr_t we can analyze.
            const app_pc trace_pc = reinterpret_cast<app_pc>(memref.instr.addr);
            instr_noalloc_t noalloc;
            instr_noalloc_init(drcontext_, &noalloc);
            instr_t *noalloc_instr = instr_from_noalloc(&noalloc);
            const app_pc encoding_addr = const_cast<app_pc>(memref.instr.encoding);
            app_pc next_pc =
                decode_from_copy(drcontext_, encoding_addr, trace_pc, noalloc_instr);
            bool instr_is_decoded = next_pc != nullptr;
            report_if_false(
                shard, instr_is_decoded,
                "DR_ISA_REGDEPS instructions should always succeed during decoding");
            // We still check this condition in case we don't abort on invariant errors.
            if (instr_is_decoded) {
                // ISA mode should be DR_ISA_REGDEPS
                report_if_false(shard,
                                instr_get_isa_mode(noalloc_instr) == DR_ISA_REGDEPS,
                                "DR_ISA_REGDEPS instruction has incorrect ISA mode");
                // Only OP_UNDECODED as opcode are allowed.
                report_if_false(shard, instr_get_opcode(noalloc_instr) == OP_UNDECODED,
                                "DR_ISA_REGDEPS instruction opcode is not OP_UNDECODED");
                // Only register operands are allowed.
                int num_dsts = instr_num_dsts(noalloc_instr);
                for (int dst_index = 0; dst_index < num_dsts; ++dst_index) {
                    opnd_t dst_opnd = instr_get_dst(noalloc_instr, dst_index);
                    report_if_false(shard, opnd_is_reg(dst_opnd),
                                    "DR_ISA_REGDEPS instruction destination operand is "
                                    "not a register");
                }
                int num_srcs = instr_num_srcs(noalloc_instr);
                for (int src_index = 0; src_index < num_srcs; ++src_index) {
                    opnd_t src_opnd = instr_get_src(noalloc_instr, src_index);
                    report_if_false(
                        shard, opnd_is_reg(src_opnd),
                        "DR_ISA_REGDEPS instruction source operand is not a register");
                }
                // Arithmetic flags should either be 0 or all read or all written or both,
                // but nothing in between, as we don't expose individual flags.
                uint eflags = instr_get_arith_flags(noalloc_instr, DR_QUERY_DEFAULT);
                report_if_false(
                    shard,
                    eflags == 0 || eflags == EFLAGS_WRITE_ARITH ||
                        eflags == EFLAGS_READ_ARITH ||
                        eflags == (EFLAGS_WRITE_ARITH | EFLAGS_READ_ARITH),
                    "DR_ISA_REGDEPS instruction has incorrect arithmetic flags");
                // Instruction length should be a multiple of 4 bytes
                // (i.e., REGDEPS_ALIGN_BYTES).
                report_if_false(
                    shard, ((next_pc - encoding_addr) % REGDEPS_ALIGN_BYTES) == 0,
                    "DR_ISA_REGDEPS instruction has incorrect length, it's not a "
                    "multiple of REGDEPS_ALIGN_BYTES = 4");
            }
        }
    }

    // Check markers.
    if (memref.marker.type == TRACE_TYPE_MARKER) {
        switch (memref.marker.marker_type) {
        case TRACE_MARKER_TYPE_SYSCALL_IDX:
            report_if_false(shard, false,
                            "OFFLINE_FILE_TYPE_ARCH_REGDEPS traces cannot have "
                            "TRACE_MARKER_TYPE_SYSCALL_IDX markers");
            break;
        case TRACE_MARKER_TYPE_SYSCALL:
            report_if_false(shard, false,
                            "OFFLINE_FILE_TYPE_ARCH_REGDEPS traces cannot have "
                            "TRACE_MARKER_TYPE_SYSCALL markers");
            break;
        case TRACE_MARKER_TYPE_SYSCALL_TRACE_START:
            report_if_false(shard, false,
                            "OFFLINE_FILE_TYPE_ARCH_REGDEPS traces cannot have "
                            "TRACE_MARKER_TYPE_SYSCALL_TRACE_START markers");
            break;
        case TRACE_MARKER_TYPE_SYSCALL_TRACE_END:
            report_if_false(shard, false,
                            "OFFLINE_FILE_TYPE_ARCH_REGDEPS traces cannot have "
                            "TRACE_MARKER_TYPE_SYSCALL_TRACE_END markers");
            break;
        case TRACE_MARKER_TYPE_SYSCALL_FAILED:
            report_if_false(shard, false,
                            "OFFLINE_FILE_TYPE_ARCH_REGDEPS traces cannot have "
                            "TRACE_MARKER_TYPE_SYSCALL_FAILED markers");
            break;
        // This case also covers TRACE_MARKER_TYPE_FUNC_RETADDR,
        // TRACE_MARKER_TYPE_FUNC_RETVAL, and TRACE_MARKER_TYPE_FUNC_ARG, since these
        // markers are always preceed by TRACE_MARKER_TYPE_FUNC_ID.
        case TRACE_MARKER_TYPE_FUNC_ID: {
            // In OFFLINE_FILE_TYPE_ARCH_REGDEPS traces the only TRACE_MARKER_TYPE_FUNC_
            // markers allowed are those related to SYS_futex. We can only check that
            // for LINUX builds of DR, otherwise we disable this check and print a
            // warning instead.
#ifdef LINUX
            report_if_false(
                shard,
                memref.marker.marker_value ==
                    static_cast<uintptr_t>(func_trace_t::TRACE_FUNC_ID_SYSCALL_BASE) +
                        SYS_futex,
                "OFFLINE_FILE_TYPE_ARCH_REGDEPS traces cannot have "
                "TRACE_MARKER_TYPE_FUNC_ID markers related to functions that "
                "are not SYS_futex");
#else
            std::cerr << "WARNING: we cannot determine whether the function ID of "
                         "TRACE_MARKER_TYPE_FUNC_ID is allowed in an "
                         "OFFLINE_FILE_TYPE_ARCH_REGDEPS trace because DynamoRIO "
                         "was not built on a Linux platform.\n";
#endif
        } break;
        default:
            // All other markers are allowed.
            break;
        }
    }
}

} // namespace drmemtrace
} // namespace dynamorio
