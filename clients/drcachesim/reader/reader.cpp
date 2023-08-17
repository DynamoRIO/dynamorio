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

#include "reader.h"

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "memref.h"
#include "utils.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

// Work around clang-format bug: no newline after return type for single-char operator.
// clang-format off
const memref_t &
reader_t::operator*()
// clang-format on
{
    return cur_ref_;
}

trace_entry_t *
reader_t::read_queued_entry()
{
    if (queue_.empty())
        return nullptr;
    entry_copy_ = queue_.front();
    queue_.pop();
    return &entry_copy_;
}

reader_t &
reader_t::operator++()
{
    // We bail if we get a partial read, or EOF, or any error.
    while (true) {
        if (bundle_idx_ == 0 /*not in instr bundle*/)
            input_entry_ = read_next_entry();
        if (input_entry_ == NULL) {
            if (!at_eof_) {
                ERRMSG("Trace is truncated\n");
                assert(false);
                at_eof_ = true; // bail
            }
            break;
        }
        if (input_entry_->type == TRACE_TYPE_FOOTER) {
            VPRINT(this, 2, "At thread EOF\n");
            // We've already presented the thread exit entry to the analyzer.
            continue;
        }
        VPRINT(this, 5, "RECV: type=%s (%d), size=%d, addr=0x%zx\n",
               trace_type_names[input_entry_->type], input_entry_->type,
               input_entry_->size, input_entry_->addr);
        if (process_input_entry())
            break;
    }

    return *this;
}

bool
reader_t::process_input_entry()
{
    bool have_memref = false;
    switch (input_entry_->type) {
    case TRACE_TYPE_READ:
    case TRACE_TYPE_WRITE:
    case TRACE_TYPE_PREFETCH:
    case TRACE_TYPE_PREFETCH_READ_L1:
    case TRACE_TYPE_PREFETCH_READ_L2:
    case TRACE_TYPE_PREFETCH_READ_L3:
    case TRACE_TYPE_PREFETCHNTA:
    case TRACE_TYPE_PREFETCH_READ:
    case TRACE_TYPE_PREFETCH_WRITE:
    case TRACE_TYPE_PREFETCH_INSTR:
    case TRACE_TYPE_PREFETCH_READ_L1_NT:
    case TRACE_TYPE_PREFETCH_READ_L2_NT:
    case TRACE_TYPE_PREFETCH_READ_L3_NT:
    case TRACE_TYPE_PREFETCH_INSTR_L1:
    case TRACE_TYPE_PREFETCH_INSTR_L1_NT:
    case TRACE_TYPE_PREFETCH_INSTR_L2:
    case TRACE_TYPE_PREFETCH_INSTR_L2_NT:
    case TRACE_TYPE_PREFETCH_INSTR_L3:
    case TRACE_TYPE_PREFETCH_INSTR_L3_NT:
    case TRACE_TYPE_PREFETCH_WRITE_L1:
    case TRACE_TYPE_PREFETCH_WRITE_L1_NT:
    case TRACE_TYPE_PREFETCH_WRITE_L2:
    case TRACE_TYPE_PREFETCH_WRITE_L2_NT:
    case TRACE_TYPE_PREFETCH_WRITE_L3:
    case TRACE_TYPE_PREFETCH_WRITE_L3_NT:
        have_memref = true;
        assert(cur_tid_ != 0 && cur_pid_ != 0);
        cur_ref_.data.pid = cur_pid_;
        cur_ref_.data.tid = cur_tid_;
        cur_ref_.data.type = (trace_type_t)input_entry_->type;
        cur_ref_.data.size = input_entry_->size;
        cur_ref_.data.addr = input_entry_->addr;
        // The trace stream always has the instr fetch first, which we
        // use to obtain the PC for subsequent data references.
        cur_ref_.data.pc = cur_pc_;
        break;
    case TRACE_TYPE_ENCODING:
        if (last_encoding_.size + input_entry_->size > MAX_ENCODING_LENGTH) {
            ERRMSG("Invalid too-large encoding size %zu + %d\n", last_encoding_.size,
                   input_entry_->size);
            assert(false);
            at_eof_ = true;
            break;
        }
        memcpy(last_encoding_.bits + last_encoding_.size, input_entry_->encoding,
               input_entry_->size);
        last_encoding_.size += input_entry_->size;
        break;
    case TRACE_TYPE_INSTR_MAYBE_FETCH:
        // While offline traces can convert rep string per-iter instrs into
        // no-fetch entries, online can't w/o extra work, so we do the work
        // here:
        if (prev_instr_addr_ == input_entry_->addr)
            input_entry_->type = TRACE_TYPE_INSTR_NO_FETCH;
        else
            input_entry_->type = TRACE_TYPE_INSTR;
        ANNOTATE_FALLTHROUGH;
    case TRACE_TYPE_INSTR:
    case TRACE_TYPE_INSTR_DIRECT_JUMP:
    case TRACE_TYPE_INSTR_INDIRECT_JUMP:
    case TRACE_TYPE_INSTR_CONDITIONAL_JUMP:
    case TRACE_TYPE_INSTR_TAKEN_JUMP:
    case TRACE_TYPE_INSTR_UNTAKEN_JUMP:
    case TRACE_TYPE_INSTR_DIRECT_CALL:
    case TRACE_TYPE_INSTR_INDIRECT_CALL:
    case TRACE_TYPE_INSTR_RETURN:
    case TRACE_TYPE_INSTR_SYSENTER:
    case TRACE_TYPE_INSTR_NO_FETCH:
        assert(cur_tid_ != 0 && cur_pid_ != 0);
        if (input_entry_->size == 0) {
            // Just an entry to tell us the PC of the subsequent memref,
            // used with -L0_filter where we don't reliably have icache
            // entries prior to data entries.
            cur_pc_ = input_entry_->addr;
        } else {
            have_memref = true;
            cur_ref_.instr.pid = cur_pid_;
            cur_ref_.instr.tid = cur_tid_;
            cur_ref_.instr.type = (trace_type_t)input_entry_->type;
            cur_ref_.instr.size = input_entry_->size;
            if (type_is_instr_branch(cur_ref_.instr.type) &&
                !type_is_instr_direct_branch(cur_ref_.instr.type)) {
                cur_ref_.instr.indirect_branch_target = last_branch_target_;
            } else {
                cur_ref_.instr.indirect_branch_target = 0;
            }
            cur_pc_ = input_entry_->addr;
            cur_ref_.instr.addr = cur_pc_;
            next_pc_ = cur_pc_ + cur_ref_.instr.size;
            prev_instr_addr_ = input_entry_->addr;
            if (cur_ref_.instr.type != TRACE_TYPE_INSTR_NO_FETCH)
                ++cur_instr_count_;
            // Look for encoding bits that belong to this instr.
            if (last_encoding_.size > 0) {
                if (last_encoding_.size != cur_ref_.instr.size) {
                    ERRMSG("Encoding size %zu != instr size %zu\n", last_encoding_.size,
                           cur_ref_.instr.size);
                    assert(false);
                }
                memcpy(cur_ref_.instr.encoding, last_encoding_.bits, last_encoding_.size);
                cur_ref_.instr.encoding_is_new = true;
                encodings_[cur_ref_.instr.addr] = last_encoding_;
            } else {
                cur_ref_.instr.encoding_is_new = false;
                const auto &it = encodings_.find(cur_ref_.instr.addr);
                if (it != encodings_.end()) {
                    memcpy(cur_ref_.instr.encoding, it->second.bits, it->second.size);
                } else if (!expect_no_encodings_) {
                    ERRMSG("Missing encoding for 0x%zx\n", cur_ref_.instr.addr);
                    assert(false);
                }
            }
            last_encoding_.size = 0;
        }
        break;
    case TRACE_TYPE_INSTR_BUNDLE:
        have_memref = true;
        // The trace stream always has the instr fetch first, which we
        // use to compute the starting PC for the subsequent instructions.
        if (!(type_is_instr(cur_ref_.instr.type) ||
              cur_ref_.instr.type == TRACE_TYPE_INSTR_NO_FETCH)) {
            // XXX i#3320: Diagnostics to track down the elusive remaining case of
            // this assert on Appveyor.  We'll remove and replace with just the
            // assert once we have a fix.
            ERRMSG("Invalid trace entry type %s (%d) before a bundle\n",
                   trace_type_names[cur_ref_.instr.type], cur_ref_.instr.type);
            assert(type_is_instr(cur_ref_.instr.type) ||
                   cur_ref_.instr.type == TRACE_TYPE_INSTR_NO_FETCH);
        }
        cur_ref_.instr.size = input_entry_->length[bundle_idx_++];
        cur_pc_ = next_pc_;
        cur_ref_.instr.addr = cur_pc_;
        next_pc_ = cur_pc_ + cur_ref_.instr.size;
        ++cur_instr_count_;
        // input_entry_->size stores the number of instrs in this bundle
        assert(input_entry_->size <= sizeof(input_entry_->length));
        if (bundle_idx_ == input_entry_->size)
            bundle_idx_ = 0;
        break;
    case TRACE_TYPE_INSTR_FLUSH:
    case TRACE_TYPE_DATA_FLUSH:
        assert(cur_tid_ != 0 && cur_pid_ != 0);
        cur_ref_.flush.pid = cur_pid_;
        cur_ref_.flush.tid = cur_tid_;
        cur_ref_.flush.type = (trace_type_t)input_entry_->type;
        cur_ref_.flush.size = input_entry_->size;
        cur_ref_.flush.addr = input_entry_->addr;
        if (cur_ref_.flush.size != 0)
            have_memref = true;
        break;
    case TRACE_TYPE_INSTR_FLUSH_END:
    case TRACE_TYPE_DATA_FLUSH_END:
        cur_ref_.flush.size = input_entry_->addr - cur_ref_.flush.addr;
        have_memref = true;
        break;
    case TRACE_TYPE_THREAD:
        cur_tid_ = (memref_tid_t)input_entry_->addr;
        // tid2pid might not be filled in yet: if so, we expect a
        // TRACE_TYPE_PID entry right after this one, and later asserts
        // will complain if it wasn't there.
        cur_pid_ = tid2pid_[cur_tid_];
        break;
    case TRACE_TYPE_THREAD_EXIT:
        cur_tid_ = (memref_tid_t)input_entry_->addr;
        cur_pid_ = tid2pid_[cur_tid_];
        assert(cur_tid_ != 0 && cur_pid_ != 0);
        // We do pass this to the caller but only some fields are valid:
        cur_ref_.exit.pid = cur_pid_;
        cur_ref_.exit.tid = cur_tid_;
        cur_ref_.exit.type = (trace_type_t)input_entry_->type;
        have_memref = true;
        break;
    case TRACE_TYPE_PID:
        cur_pid_ = (memref_pid_t)input_entry_->addr;
        // We do want to replace, in case of tid reuse.
        tid2pid_[cur_tid_] = cur_pid_;
        break;
    case TRACE_TYPE_MARKER:
        cur_ref_.marker.type = (trace_type_t)input_entry_->type;
        assert(cur_tid_ != 0 && cur_pid_ != 0);
        cur_ref_.marker.pid = cur_pid_;
        cur_ref_.marker.tid = cur_tid_;
        cur_ref_.marker.marker_type = (trace_marker_type_t)input_entry_->size;
        cur_ref_.marker.marker_value = input_entry_->addr;
        // Look for timestamp+cpu duplicated from the prior chunk.  Skip them on
        // a linear walk.  File readers that support seeking will read them
        // and use them to start post-seek iteration.
        if (chunk_instr_count_ > 0 &&
            cur_ref_.marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP &&
            skip_chunk_header_.find(cur_tid_) != skip_chunk_header_.end()) {
            VPRINT(this, 2, "skipping start-of-chunk dup timestamp\n");
        } else if (chunk_instr_count_ > 0 &&
                   cur_ref_.marker.marker_type == TRACE_MARKER_TYPE_CPU_ID &&
                   skip_chunk_header_.find(cur_tid_) != skip_chunk_header_.end()) {
            VPRINT(this, 2, "skipping start-of-chunk dup cpu\n");
            skip_chunk_header_.erase(cur_tid_);
        } else if (cur_ref_.marker.marker_type == TRACE_MARKER_TYPE_RECORD_ORDINAL) {
            // Not exposed to tools.
        } else if (cur_ref_.marker.marker_type == TRACE_MARKER_TYPE_BRANCH_TARGET) {
            // Not exposed to tools.
            last_branch_target_ = cur_ref_.marker.marker_value;
        } else {
            have_memref = true;
        }
        if (cur_ref_.marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP) {
            // Today, a skipped memref is just a duplicate of one that we've
            // already seen, so this condition is not really needed. But to
            // be future-proof, we want to avoid looking at timestamps that
            // won't be passed to the user as well.
            if (have_memref) {
                last_timestamp_ = cur_ref_.marker.marker_value;
                if (first_timestamp_ == 0)
                    first_timestamp_ = last_timestamp_;
            }
        } else if (cur_ref_.marker.marker_type == TRACE_MARKER_TYPE_VERSION)
            version_ = cur_ref_.marker.marker_value;
        else if (cur_ref_.marker.marker_type == TRACE_MARKER_TYPE_FILETYPE) {
            filetype_ = cur_ref_.marker.marker_value;
            if (TESTANY(OFFLINE_FILE_TYPE_ENCODINGS, filetype_) &&
                !TESTANY(OFFLINE_FILE_TYPE_KERNEL_SYSCALLS, filetype_)) {
                expect_no_encodings_ = false;
            }
        } else if (cur_ref_.marker.marker_type == TRACE_MARKER_TYPE_CACHE_LINE_SIZE)
            cache_line_size_ = cur_ref_.marker.marker_value;
        else if (cur_ref_.marker.marker_type == TRACE_MARKER_TYPE_PAGE_SIZE)
            page_size_ = cur_ref_.marker.marker_value;
        else if (cur_ref_.marker.marker_type == TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT)
            chunk_instr_count_ = cur_ref_.marker.marker_value;
        else if (cur_ref_.marker.marker_type == TRACE_MARKER_TYPE_CHUNK_FOOTER)
            skip_chunk_header_.insert(cur_tid_);
        break;
    default:
        ERRMSG("Unknown trace entry type %s (%d)\n", trace_type_names[input_entry_->type],
               input_entry_->type);
        assert(false);
        at_eof_ = true; // bail
        break;
    }
    if (have_memref) {
        if (suppress_ref_count_ > 0) {
            VPRINT(this, 4, "suppressing %" PRIu64 " ref counts @%" PRIu64 "\n",
                   suppress_ref_count_, cur_ref_count_);
            --suppress_ref_count_;
        } else {
            if (suppress_ref_count_ == 0) {
                // Ensure is_record_synthetic() ignores it.
                suppress_ref_count_ = -1;
            }
            ++cur_ref_count_;
            VPRINT(this, 5, "ref count is now @%" PRIu64 "\n", cur_ref_count_);
        }
    }
    return have_memref;
}

bool
reader_t::pre_skip_instructions()
{
    // If the user asks to skip from the very start, we still need to find the chunk
    // count marker and drain the header queue and populate the stream header values.
    // XXX: We assume the page size is the final header; it is complex to wait for
    // the timestamp as we don't want to read it yet.
    while (page_size_ == 0) {
        input_entry_ = read_next_entry();
        if (input_entry_ == nullptr) {
            at_eof_ = true;
            return false;
        }
        VPRINT(this, 4, "PRE-SKIP: type=%s (%d), size=%d, addr=0x%zx\n",
               trace_type_names[input_entry_->type], input_entry_->type,
               input_entry_->size, input_entry_->addr);
        if (input_entry_->type != TRACE_TYPE_MARKER ||
            input_entry_->size == TRACE_MARKER_TYPE_TIMESTAMP) {
            // Likely some mock in a test with no page size header: just move on.
            queue_.push(*input_entry_);
            break;
        }
        process_input_entry();
    }
    return true;
}

reader_t &
reader_t::skip_instructions(uint64_t instruction_count)
{
    // We do not support skipping with instr bundles.
    if (bundle_idx_ != 0) {
        ERRMSG("Skipping with instr bundles is not supported.\n");
        assert(false);
        at_eof_ = true;
        return *this;
    }
    if (!pre_skip_instructions())
        return *this;
    return skip_instructions_with_timestamp(cur_instr_count_ + instruction_count);
}

reader_t &
reader_t::skip_instructions_with_timestamp(uint64_t stop_instruction_count)
{
    // This base class has no fast seeking and must do a linear walk.
    // We have +1 because we need to skip the memrefs of the final skipped
    // instr, so we look for the 1st unskipped instr: but we do not want to
    // process it so we do not use the ++ operator function.
    uint64_t stop_count = stop_instruction_count + 1;
    trace_entry_t timestamp = {};
    // Use the most recent timestamp.
    if (last_timestamp_ != 0) {
        timestamp.type = TRACE_TYPE_MARKER;
        timestamp.size = TRACE_MARKER_TYPE_TIMESTAMP;
        timestamp.addr = static_cast<addr_t>(last_timestamp_);
    }
    trace_entry_t cpu = {};
    trace_entry_t next_instr = {};
    bool prev_was_record_ord = false;
    bool found_real_timestamp = false;
    VPRINT(this, 4, "Skipping from %" PRIu64 " until we reach %" PRIu64 "\n",
           cur_instr_count_, stop_count);
    while (cur_instr_count_ < stop_count) { // End condition is never reached.
        // Remember the prior entry to use as the cur entry when we hit the
        // too-far instr if we didn't find a timestamp.
        if (input_entry_ != nullptr) // Only at start: and we checked for skipping 0.
            entry_copy_ = *input_entry_;
        trace_entry_t *next = read_next_entry();
        if (next == nullptr || next->type == TRACE_TYPE_FOOTER) {
            VPRINT(this, 1,
                   next == nullptr ? "Failed to read next entry\n" : "Hit EOF\n");
            at_eof_ = true;
            return *this;
        }
        VPRINT(this, 4, "SKIP: type=%s (%d), size=%d, addr=0x%zx\n",
               trace_type_names[next->type], next->type, next->size, next->addr);
        // We need to pass up memrefs for the final skipped instr, but we don't
        // want to process_input_entry() on the first unskipped instr so we can
        // insert the timestamp+cpu first.
        if (cur_instr_count_ + 1 == stop_count &&
            type_is_instr(static_cast<trace_type_t>(next->type))) {
            next_instr = *next;
            break;
        }
        // To examine the produced memrefs we'd have to have the base reader
        // expose these hidden entries.  It is simpler for us to read the
        // trace_entry_t directly prior to processing by the base class.
        if (next->type == TRACE_TYPE_MARKER) {
            if (next->size == TRACE_MARKER_TYPE_RECORD_ORDINAL) {
                cur_ref_count_ = next->addr;
                prev_was_record_ord = true;
                VPRINT(this, 4, "Found record ordinal marker: new ord %" PRIu64 "\n",
                       cur_ref_count_);
            } else if (next->size == TRACE_MARKER_TYPE_TIMESTAMP) {
                timestamp = *next;
                if (prev_was_record_ord)
                    --cur_ref_count_; // Invisible to ordinals.
                else
                    found_real_timestamp = true;
            } else if (next->size == TRACE_MARKER_TYPE_CPU_ID) {
                cpu = *next;
                if (prev_was_record_ord)
                    --cur_ref_count_; // Invisible to ordinals.
            } else
                prev_was_record_ord = false;
        } else
            prev_was_record_ord = false;
        // Update core state.
        input_entry_ = next;
        process_input_entry();
    }
    if (timestamp.type == TRACE_TYPE_MARKER && cpu.type == TRACE_TYPE_MARKER) {
        // Insert the two markers.
        if (!found_real_timestamp) {
            VPRINT(this, 4, "Using duplicate timestamp\n");
            // These synthetic entries are not real records in the unskipped trace, so we
            // do not associate record counts with them.
            suppress_ref_count_ = 1;
            if (cpu.type == TRACE_TYPE_MARKER)
                ++suppress_ref_count_;
        } else {
            // These are not invisible but we already counted them in the loop above
            // so we need to avoid a double-count.
            VPRINT(this, 4, "Found real timestamp: walking back ord from %" PRIu64 "\n",
                   cur_ref_count_);
            --cur_ref_count_;
            if (cpu.type == TRACE_TYPE_MARKER)
                --cur_ref_count_;
        }
        entry_copy_ = timestamp;
        input_entry_ = &entry_copy_;
        process_input_entry();
        if (cpu.type == TRACE_TYPE_MARKER)
            queue_.push(cpu);
        queue_.push(next_instr);
    } else {
        // We missed the markers somehow.
        // next_instr is our target instr, so make that the next record.
        VPRINT(this, 1, "Skip failed to find both timestamp and cpu\n");
        entry_copy_ = next_instr;
        input_entry_ = &entry_copy_;
        process_input_entry();
    }
    return *this;
}

} // namespace drmemtrace
} // namespace dynamorio
