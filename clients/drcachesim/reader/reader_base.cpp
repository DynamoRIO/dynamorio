/* **********************************************************
 * Copyright (c) 2016-2025 Google, Inc.  All rights reserved.
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

#include "reader_base.h"

#include <cassert>
#include <cinttypes>
#include <queue>
#include <stack>

#include "trace_entry.h"
#include "utils.h"

namespace dynamorio {
namespace drmemtrace {

/****************************************************************************
 * Implementation for entry_queue_t.
 */

void
entry_queue_t::clear()
{
    entries_.clear();
    pcs_.clear();
}

bool
entry_queue_t::empty()
{
    return entries_.empty();
}

bool
entry_queue_t::has_record_and_next_pc_after_front()
{
    if (entries_.empty())
        return false;
    // If the record at the front is already a record with a PC, we want
    // there to be yet another record in the queue that holds the next PC.
    bool front_has_pc = entry_has_pc(entries_.front());
    int non_first_entries_with_pc =
        static_cast<int>(pcs_.size()) - (front_has_pc ? 1 : 0);
    return non_first_entries_with_pc >= 1;
}

void
entry_queue_t::push_back(const trace_entry_t &entry)
{
    uint64_t pc;
    if (entry_has_pc(entry, &pc)) {
        pcs_.push_back(pc);
    }
    entries_.push_back(entry);
}

void
entry_queue_t::push_front(const trace_entry_t &entry, uint64_t &next_trace_pc)
{
    uint64_t pc;
    if (entry_has_pc(entry, &pc)) {
        pcs_.push_front(pc);
        next_trace_pc = pc;
    }
    entries_.push_front(entry);
}

void
entry_queue_t::pop_front(trace_entry_t &entry, uint64_t &next_pc)
{
    entry = entries_.front();
    entries_.pop_front();
    if (entry_has_pc(entry)) {
        pcs_.pop_front();
    }
    next_pc = 0;
    if (!pcs_.empty()) {
        next_pc = pcs_.front();
    }
}

bool
entry_queue_t::entry_has_pc(const trace_entry_t &entry, uint64_t *pc)
{
    if (type_is_instr(static_cast<trace_type_t>(entry.type))) {
        if (pc != nullptr)
            *pc = entry.addr;
        return true;
    }
    if (static_cast<trace_type_t>(entry.type) == TRACE_TYPE_MARKER &&
        static_cast<trace_marker_type_t>(entry.size) == TRACE_MARKER_TYPE_KERNEL_EVENT) {
        if (pc != nullptr)
            *pc = entry.addr;
        return true;
    }
    return false;
}

/****************************************************************************
 * Implementation for reader_base_t.
 */

reader_base_t::reader_base_t(int online, int verbosity, const char *output_prefix)
    : verbosity_(verbosity)
    , output_prefix_(output_prefix)
    , online_(online)
{
}

bool
reader_base_t::is_online()
{
    return online_;
}

trace_entry_t *
reader_base_t::get_next_entry()
{
    if (is_online()) {
        // We don't support any readahead in the online mode. We simply invoke the
        // reader's logic.
        // XXX: Add read-ahead support for online mode. Needs more thought to
        // determine feasibility and cost of read-ahead, and whether we want to
        // always read-ahead or only when memtrace_stream_t::get_next_trace_pc()
        // asks for it.
        return read_next_entry();
    }
    // Continue reading ahead until we have a record and the next continuous
    // pc in the trace, or if the input stops returning new records.
    while (!queue_.has_record_and_next_pc_after_front() && !at_null_internal_) {
        trace_entry_t *entry = read_next_entry();
        if (entry == nullptr) {
            // Ensure we don't repeatedly call read_next_entry after we know
            // it has finished.
            assert(!at_null_internal_);
            at_null_internal_ = true;
            at_eof_internal_ = at_eof_;
            // Pretend we're not at eof since we may have records
            // buffered in entry_queue.
            at_eof_ = false;
        } else {
            VPRINT(this, 4, "queued: type=%s (%d), size=%d, addr=0x%zx\n",
                   trace_type_names[entry->type], entry->type, entry->size, entry->addr);
            // We deliberately make a copy of *entry here.
            queue_.push_back(*entry);
        }
    }
    trace_entry_t *ret_entry = nullptr;
    if (!queue_.empty()) {
        // If we're at the end of the trace and there's no next continuous pc
        // in the trace, entry_queue_t will simply set next pc to zero.
        queue_.pop_front(entry_copy_, next_trace_pc_);
        ret_entry = &entry_copy_;
    } else {
        assert(at_null_internal_);
        // Now that the queued entries have been drained, let the user
        // know we're done.
        ret_entry = nullptr;
        next_trace_pc_ = 0;
        // at_eof may or may not be true, which is used to signal errors.
        at_eof_ = at_eof_internal_;
    }
    if (ret_entry != nullptr) {
        VPRINT(this, 4,
               "returning: type=%s (%d), size=%d, addr=0x%zx, next_pc=0x%" PRIx64 "\n",
               trace_type_names[ret_entry->type], ret_entry->type, ret_entry->size,
               ret_entry->addr, next_trace_pc_);
    } else {
        VPRINT(this, 4, "finished: at_eof_: %d\n", at_eof_);
    }
    return ret_entry;
}

// To avoid double-dispatch (requires listing all derived types in the base here)
// and RTTI in trying to get the right operators called for subclasses, we
// instead directly check at_eof here.  If we end up needing to run code
// and a bool field is not enough we can change this to invoke a virtual
// method is_at_eof().
bool
reader_base_t::operator==(const reader_base_t &rhs) const
{
    return BOOLS_MATCH(at_eof_, rhs.at_eof_);
}

bool
reader_base_t::operator!=(const reader_base_t &rhs) const
{
    return !BOOLS_MATCH(at_eof_, rhs.at_eof_);
}

void
reader_base_t::clear_entry_queue()
{
    queue_.clear();
}

void
reader_base_t::queue_to_return_next(std::queue<trace_entry_t> &queue)
{
    // Since there may already be some records in queue_ (from our readahead to find the
    // next trace pc), we need to insert in the reverse order in its front.
    std::stack<trace_entry_t> stack;
    while (!queue.empty()) {
        stack.push(queue.front());
        queue.pop();
    }
    while (!stack.empty()) {
        queue_.push_front(stack.top(), next_trace_pc_);
        stack.pop();
    }
}

void
reader_base_t::queue_to_return_next(trace_entry_t &entry)
{
    queue_.push_front(entry, next_trace_pc_);
}

uint64_t
reader_base_t::get_next_trace_pc() const
{
    return next_trace_pc_;
}

} // namespace drmemtrace
} // namespace dynamorio
