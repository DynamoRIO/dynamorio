/* **********************************************************
 * Copyright (c) 2015-2024 Google, Inc.  All rights reserved.
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

/* reader: virtual base class for an iterator that provides a single memory
 * stream for use by a cache simulator.
 */

#ifndef _READER_H_
#define _READER_H_ 1

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include <stack>
#include <deque>
#include <iterator>
#include <queue>
#include <unordered_map>
#include <unordered_set>

// For exporting we avoid "../common" and rely on -I.
#include "memref.h"
#include "memtrace_stream.h"
#include "trace_entry.h"
#include "utils.h"

namespace dynamorio {
namespace drmemtrace {

#ifndef DR_PARAM_OUT
#    define DR_PARAM_OUT /* just a marker */
#endif

#ifdef DEBUG
#    define VPRINT(reader, level, ...)                            \
        do {                                                      \
            if ((reader)->verbosity_ >= (level)) {                \
                fprintf(stderr, "%s ", (reader)->output_prefix_); \
                fprintf(stderr, __VA_ARGS__);                     \
            }                                                     \
        } while (0)
// clang-format off
#    define UNUSED(x) /* nothing */
// clang-format on
#else
#    define VPRINT(reader, level, ...) /* nothing */
#    define UNUSED(x) ((void)(x))
#endif

/**
 * Queue of #dynamorio::drmemtrace::trace_entry_t that have been read from the
 * input but are yet to be processed by the reader.
 *
 * These entries may have been:
 * - read in advance to allow us to figure out the next continuous pc in the
 *   trace.
 * - synthesized by the reader on a skip event (like the timestamp and cpu
 *   markers).
 * - seen by the reader but not yet processed (like the instr entry used
 *   to decide when a skip is done).
 * - trace header entries that were rearranged or modified but not yet used.
 */
class entry_queue_t {
public:
    /**
     * Removes all entries from the queue.
     */
    void
    clear();
    /**
     * Returns whether the queue is empty.
     */
    bool
    empty();
    /**
     * Returns whether the queue is non-empty and has some record that tells us
     * the next continuous pc in the trace for the record in the front.
     */
    bool
    has_record_and_next_pc_for_front();
    /**
     * Adds the given #dynamorio::drmemtrace::trace_entry_t that was read from
     * the input ahead of its time to the back of the queue.
     *
     * Note that for trace entries that need to be added back to the queue (maybe
     * because they cannot be returned just yet by the reader), the push_front
     * API below should be used, as there may be many readahead entries already
     * in this queue.
     */
    void
    push_back(const trace_entry_t &entry);
    /**
     * Adds the given #dynamorio::drmemtrace::trace_entry_t to the front of the
     * queue. This entry may have been synthesized by the reader (e.g., the timestamp
     * and cpu entries are synthesized after a skip), or the reader may have decided
     * it does not want to process it yet (e.g., the first instruction after a skip).
     *
     * If this operation changes the next pc in the trace, next_trace_pc will be updated.
     */
    void
    push_front(const trace_entry_t &entry, uint64_t &next_trace_pc);
    /**
     * Returns the next entry from the queue in the entry arg, and the next
     * continuous pc in the trace in the next_pc arg if it exists or zero
     * otherwise.
     */
    void
    pop_front(trace_entry_t &entry, uint64_t &next_pc);

    /**
     * Returns whether the given #dynamorio::drmemtrace::trace_entry_t has a PC, and if
     * so, sets it at the given *pc.
     */
    static bool
    entry_has_pc(const trace_entry_t &entry, uint64_t *pc = nullptr);

private:
    // Trace entries queued up to be returned.
    std::deque<trace_entry_t> entries_;
    // PCs for the trace entries in entries_ that have a PC (see entry_has_pc()). This
    // allows efficient lookup of the next trace pc.
    // The elements here will be in the same order as the corresponding one in entries_,
    // but there may not be an element here for each one in entries_.
    std::deque<uint64_t> pcs_;
};

/**
 * Helper to share logic for reading ahead #dynamorio::drmemtrace::trace_entry_t records
 * from the input in a way that we always know the next continuous pc in the trace after
 * the current record.
 */
class trace_entry_readahead_helper_t {
public:
    /**
     * #dynamorio::drmemtrace::trace_entry_readahead_helper_t needs to manage some state
     * of the #dynamorio::drmemtrace::reader_t (or
     * #dynamorio::drmemtrace::record_reader_t) that's using it for readahead. The
     * #dynamorio::drmemtrace::file_reader_t (or
     * #dynamorio::drmemtrace::record_file_reader_t) communicates some state to the base
     * #dynamorio::drmemtrace::reader_t class using data members in the
     * #dynamorio::drmemtrace::reader_t; we need to manage that state to take into account
     * our read-ahead queue.
     *
     * Specifically:
     * - reader_at_eof points to the bool in #dynamorio::drmemtrace::reader_t that
     *   denotes when the input has reached its end. Even though the input may have
     *   reached the end, the read-ahead queue may not have.
     * - reader_cur_entry points to the last read #dynamorio::drmemtrace::trace_entry_t.
     *   The last #dynamorio::drmemtrace::trace_entry_t actually read from the input is
     *   likely not the one we want the #dynamorio::drmemtrace::reader_t to use next
     *   (because we perform readahead).
     *
     * Here we accept pointers to such #dynamorio::drmemtrace::reader_t state; we know
     * what the #dynamorio::drmemtrace::file_reader_t returned, and can combine it with
     * the read-ahead queue's state so the #dynamorio::drmemtrace::reader_t sees the
     * correct thing.
     *
     * Other parameters:
     * - reader_next_trace_pc points to the reader_t data member where we'll write the
     *   next trace pc.
     * - reader_entry_queue points to the #dynamorio::drmemtrace::entry_queue_t that
     *   will be used to store the readahead entries. This needs to be shared with
     *   the #dynamorio::drmemtrace::reader_t because it creates some synthesized
     *   entries in some cases.
     * - online denotes whether we're in the online mode, as opposed to offline.
     *   #dynamorio::drmemtrace::trace_entry_t readahead is disabled in the online mode.
     *
     * We could potentially just store the #dynamorio::drmemtrace::reader_t* here,
     * instead of multiple state object pointers, but the current way makes the contract
     * more explicit.
     */
    trace_entry_readahead_helper_t(bool *online, trace_entry_t *reader_cur_entry,
                                   bool *reader_at_eof, uint64_t *reader_next_trace_pc,
                                   entry_queue_t *reader_entry_queue, int verbosity);

    trace_entry_readahead_helper_t() = default;

    /**
     * Returns the next entry for the input stream, and ensures that we also know the
     * next continuous pc in the trace (if it exists). May read however many extra
     * records that need to be read from the input stream for this; if the next trace
     * pc is already known, it may not read any more records at all.
     *
     * This puts the next entry for the reader in *reader_cur_entry_, the _effective_
     * state of the input (which takes into account the presence of read-ahead entries)
     * in *reader_at_eof_, and the next trace pc in *reader_next_trace_pc_.
     *
     * For convenience of the #dynamorio::drmemtrace::reader_t, it returns either
     * reader_cur_entry_ or nullptr.
     */
    trace_entry_t *
    read_next_entry_and_trace_pc();

private:
    // This implementation is supposed to be provided by the reader_t or record_reader_t
    // that is using the trace_entry_readahead_helper_t.
    virtual trace_entry_t *
    read_next_entry() = 0;

    // Cannot take this by value because it is set by file_reader_t after the base
    // reader_t (and hence this reader_readahead_helper_t) has been constructed.
    bool *online_ = nullptr;

    // State of the reader_t that is using this object.
    trace_entry_t *reader_cur_entry_ = nullptr;
    bool *reader_at_eof_ = nullptr;
    uint64_t *reader_next_trace_pc_ = nullptr;
    entry_queue_t *reader_entry_queue_ = nullptr;

    // The internal state for the underlying input.
    bool at_null_ = false;
    bool at_eof_ = false;

    int verbosity_ = 0;
    const char *output_prefix_ = "[readahead_helper]";
};

/**
 * Iterator over #dynamorio::drmemtrace::memref_t trace entries. This class converts a
 * trace (offline or online) into a stream of #dynamorio::drmemtrace::memref_t entries. It
 * also provides more information about the trace using the
 * #dynamorio::drmemtrace::memtrace_stream_t API.
 */
class reader_t : public memtrace_stream_t {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = memref_t;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type *;
    using reference = value_type &;
    reader_t()
        // Need this initialized for the mock_reader_t.
        : readahead_helper_(this)
    {
        cur_ref_ = {};
    }
    reader_t(int verbosity, const char *prefix)
        : verbosity_(verbosity)
        , output_prefix_(prefix)
        , readahead_helper_(this)
    {
        cur_ref_ = {};
    }
    virtual ~reader_t()
    {
    }

    // This may block.
    virtual bool
    init() = 0;

    virtual const memref_t &
    operator*();

    // To avoid double-dispatch (requires listing all derived types in the base here)
    // and RTTI in trying to get the right operators called for subclasses, we
    // instead directly check at_eof here.  If we end up needing to run code
    // and a bool field is not enough we can change this to invoke a virtual
    // method is_at_eof().
    virtual bool
    operator==(const reader_t &rhs) const
    {
        return BOOLS_MATCH(at_eof_, rhs.at_eof_);
    }
    virtual bool
    operator!=(const reader_t &rhs) const
    {
        return !BOOLS_MATCH(at_eof_, rhs.at_eof_);
    }

    virtual reader_t &
    operator++();

    // Skips records until "count" instruction records have been passed.  If any
    // timestamp (plus cpuid) is skipped, the most recent skipped timestamp will be
    // duplicated prior to the target instruction.  Top-level headers will still be
    // observed.  This generally should call pre_skip_instructions() to observe the
    // headers, perform any fast skipping, and then should call
    // skip_instructions_with_timestamp() to properly duplicate the prior timestamp.
    // Skipping 0 instructions is supported and will skip ahead to right before the
    // next instruction.
    virtual reader_t &
    skip_instructions(uint64_t instruction_count);

    // Supplied for subclasses that may fail in their constructors.
    virtual bool
    operator!()
    {
        return false;
    }

    // We do not support the post-increment operator for two reasons:
    // 1) It prevents pure virtual functions here, as it cannot
    //    return an abstract type;
    // 2) It is difficult to implement for file_reader_t as streams do not
    //    have a copy constructor.

    uint64_t
    get_record_ordinal() const override
    {
        return cur_ref_count_;
    }
    uint64_t
    get_instruction_ordinal() const override
    {
        return cur_instr_count_;
    }
    uint64_t
    get_last_timestamp() const override
    {
        return last_timestamp_;
    }
    uint64_t
    get_first_timestamp() const override
    {
        return first_timestamp_;
    }
    uint64_t
    get_version() const override
    {
        return version_;
    }
    uint64_t
    get_filetype() const override
    {
        return filetype_;
    }
    uint64_t
    get_cache_line_size() const override
    {
        return cache_line_size_;
    }
    uint64_t
    get_chunk_instr_count() const override
    {
        return chunk_instr_count_;
    }
    uint64_t
    get_page_size() const override
    {
        return page_size_;
    }
    bool
    is_record_kernel() const override
    {
        return in_kernel_trace_;
    }
    uint64_t
    get_next_trace_pc() const override
    {
        return next_trace_pc_;
    }
    bool
    is_record_synthetic() const override
    {
        if (cur_ref_.marker.type == TRACE_TYPE_MARKER &&
            (cur_ref_.marker.marker_type == TRACE_MARKER_TYPE_CORE_WAIT ||
             cur_ref_.marker.marker_type == TRACE_MARKER_TYPE_CORE_IDLE)) {
            // These are synthetic records not part of the input and not
            // counting toward ordinals.
            return true;
        }
        return suppress_ref_count_ >= 0;
    }

protected:
    // This reads the next entry from the single stream of entries (or from the
    // local queue if non-empty).  If it returns false it will set at_eof_ to distinguish
    // end-of-file from an error.  It should call read_queued_entry() first before
    // reading a new entry from the input stream.
    virtual trace_entry_t *
    read_next_entry() = 0;
    // Returns and removes the entry (nullptr if none) from the local queue.
    // This should be called by read_next_entry() prior to reading a new record
    // from the input stream.
    virtual trace_entry_t *
    read_queued_entry();
    // This updates internal state for the just-read input_entry_.
    // Returns whether a new memref record is now available.
    virtual bool
    process_input_entry();

    // Meant to be called from skip_instructions();
    // Looks for headers prior to a skip in case it is from the start of the trace.
    // Returns whether the skip can continue (it might fail if at eof).
    virtual bool
    pre_skip_instructions();

    // Meant to be called from skip_instructions();
    // Performs a simple walk until it sees the instruction whose ordinal is
    // "stop_instruction_count" along with all of its associated records such as
    // memrefs.  If any timestamp (plus cpuid) is skipped, the most recent skipped
    // timestamp will be duplicated prior to the target instruction.
    virtual reader_t &
    skip_instructions_with_timestamp(uint64_t stop_instruction_count);

    // Following typical stream iterator convention, the default constructor
    // produces an EOF object.
    // This should be set to false by subclasses in init() and set
    // back to true when actual EOF is hit.
    bool at_eof_ = true;

    int verbosity_ = 0;
    bool online_ = true;
    const char *output_prefix_ = "[reader]";
    uint64_t cur_ref_count_ = 0;
    int64_t suppress_ref_count_ = -1;
    uint64_t cur_instr_count_ = 0;
    std::unordered_set<memref_tid_t> skip_chunk_header_;
    uint64_t last_timestamp_ = 0;
    uint64_t first_timestamp_ = 0;
    trace_entry_t *input_entry_ = nullptr;
    uint64_t last_cpuid_ = 0;
    // Remember top-level headers for the memtrace_stream_t interface.
    uint64_t version_ = 0;
    uint64_t filetype_ = 0;
    uint64_t cache_line_size_ = 0;
    uint64_t chunk_instr_count_ = 0;
    uint64_t page_size_ = 0;
    uint64_t next_trace_pc_ = 0;
    // We need to read ahead when skipping to include post-instr records.
    // We store into this queue records already read from the input but not
    // yet returned to the iterator.
    entry_queue_t entry_queue_;
    trace_entry_t entry_copy_; // For use in returning a queue entry.

    struct encoding_info_t {
        size_t size = 0;
        unsigned char bits[MAX_ENCODING_LENGTH];
    };

    std::unordered_map<addr_t, encoding_info_t> encodings_;
    // Whether this reader's input stream interleaves software threads and thus
    // some thread-based checks may not apply.
    bool core_sharded_ = false;
    bool found_filetype_ = false;

private:
    class reader_readahead_helper_t : public trace_entry_readahead_helper_t {
    public:
        reader_readahead_helper_t(reader_t *reader)
            : trace_entry_readahead_helper_t(&reader->online_, &reader->entry_copy_,
                                             &reader->at_eof_, &reader->next_trace_pc_,
                                             &reader->entry_queue_, reader->verbosity_)
            , reader_(reader)
        {
            assert(reader_ != nullptr);
        }
        reader_readahead_helper_t() = default;

    private:
        trace_entry_t *
        read_next_entry() override
        {
            return reader_->read_next_entry();
        }
        reader_t *reader_ = nullptr;
    };

    reader_readahead_helper_t readahead_helper_;
    memref_t cur_ref_;
    memref_tid_t cur_tid_ = 0;
    memref_pid_t cur_pid_ = 0;
    addr_t cur_pc_ = 0;
    addr_t next_pc_;
    addr_t prev_instr_addr_ = 0;
    int bundle_idx_ = 0;
    std::unordered_map<memref_tid_t, memref_pid_t> tid2pid_;
    bool expect_no_encodings_ = true;
    encoding_info_t last_encoding_;
    addr_t last_branch_target_ = 0;
    bool in_kernel_trace_ = false;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _READER_H_ */
