/* **********************************************************
 * Copyright (c) 2015-2025 Google, Inc.  All rights reserved.
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

/* reader_base: virtual base class for an iterator that provides a single memory
 * stream for use by a cache simulator; the iterator could be on either memref_t
 * records or trace_entry_t records.
 */

#ifndef _READER_BASE_H_
#define _READER_BASE_H_ 1

#include <queue>

// For exporting we avoid "../common" and rely on -I.
#include "memtrace_stream.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

#ifndef DR_PARAM_OUT
#    define DR_PARAM_OUT /* just a marker */
#endif

#define OUT /* just a marker */

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
 * - read in advance to allow the reader to figure out when a skip operation
 *   is complete (i.e., the post-skip instr entry)
 * - read in advance header markers to figure out the stream tid and pid.
 * - synthesized by the reader on a skip event (like the timestamp and cpu
 *   markers).
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
     * the next continuous pc in the trace after the record in the front.
     */
    bool
    has_record_and_next_pc_after_front();
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
     * Adds the given #dynamorio::drmemtrace::trace_entry_t that was read from
     * the input ahead of its time to the back of the queue.
     *
     * Note that for trace entries that need to be added back to the queue by
     * the #dynamorio::drmemtrace::reader_t,
     * #dynamorio::drmemtrace::record_reader_t, or their derived classes (maybe
     * because the entry cannot be returned just yet by the reader), the push_front
     * API should be used, as there may be many readahead entries already
     * in this queue.
     */
    void
    push_back(const trace_entry_t &entry);

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
 * Base class for #dynamorio::drmemtrace:reader_t and
 * #dynamorio::drmemtrace::record_reader_t. This contains some interfaces and
 * implementations that are shared between the two types of readers.
 *
 * This base class is intended for logic close to reading the entries, and the
 * reader interface common to the two types of readers; not so much for
 * reader-specific logic for what to do with the entries.
 *
 * This subclasses #dynamorio::drmemtrace::memtrace_stream_t because all readers
 * derived from it are expected to implement that interface, but it leaves the
 * implementation of most of the stream APIs to each derived class.
 *
 * XXX i#5727: Can we potentially move other logic or interface definitions here?
 */
class reader_base_t : public memtrace_stream_t {
public:
    reader_base_t() = default;
    reader_base_t(int online, int verbosity, const char *output_prefix);
    virtual ~reader_base_t() = default;

    /**
     * Initializes various state for the reader. E.g., subclasses should remember to
     * set #at_eof_ to false here. Also reads the first entry by invoking
     * operator++() so that operator*() is ready to provide one after init().
     *
     * May block for reading the first entry.
     */
    virtual bool
    init() = 0;

    virtual bool
    operator==(const reader_base_t &rhs) const;
    virtual bool
    operator!=(const reader_base_t &rhs) const;

    uint64_t
    get_next_trace_pc() const override;

protected:
    /**
     * Returns the next entry for this reader.
     *
     * If it returns nullptr, it will set the #at_eof_ field to distinguish end-of-file
     * from an error.
     *
     * Also sets the next continuous pc in the trace at the next_trace_pc_ data member.
     *
     * An invocation of this API may or may not cause an actual read from the underlying
     * source using the derived class implementation of
     * #dynamorio::drmemtrace::reader_base_t::read_next_entry().
     */
    trace_entry_t *
    get_next_entry();

    /**
     * Returns whether the reader is operating in the online mode, which involves reading
     * trace entries from an IPC pipe, as opposed to reading them from a more persistent
     * media like a file on a disk.
     */
    bool
    is_online();

    /**
     * Clears all #dynamorio::drmemtrace::trace_entry_t that are buffered in the
     * #dynamorio::drmemtrace::entry_queue_t, either for read-ahead or deliberately using
     * #dynamorio::drmemtrace::reader_base_t::queue_to_return_next().
     */
    void
    clear_entry_queue();

    /**
     * Adds the given entries to the #dynamorio::drmemtrace::entry_queue_t to be returned
     * from the next call to #dynamorio::drmemtrace::reader_base_t::get_next_entry()
     * in the same order as the provided queue.
     *
     * If this routine (or its overload) is used another time, before all records from
     * the prior invocation are passed on to the user, the records queued in the later
     * call will be returned first.
     */
    void
    queue_to_return_next(std::queue<trace_entry_t> &queue);
    /**
     * Adds the given entry to the #dynamorio::drmemtrace::entry_queue_t to be returned
     * from the next call to #dynamorio::drmemtrace::reader_base_t::get_next_entry().
     *
     * If this routine (or its overload) is used another time, before all records from
     * the prior invocation are passed on to the user, the records queued in the later
     * call will be returned first.
     */
    void
    queue_to_return_next(trace_entry_t &entry);

    /**
     * Denotes whether the reader is at EOF.
     *
     * This should be set to false by subclasses in init() and set back to true when
     * actual EOF is hit.
     *
     * Following typical stream iterator convention, the default constructor
     * produces an EOF object.
     */
    bool at_eof_ = true;

    int verbosity_ = 0;
    const char *output_prefix_ = "[reader_base_t]";

    /**
     * Used to hold the memory corresponding to #dynamorio::drmemtrace::trace_entry_t*
     * returned by #dynamorio::drmemtrace::reader_base_t::get_next_entry() and
     * #dynamorio::drmemtrace::reader_base_t::read_next_entry() in some cases.
     */
    trace_entry_t entry_copy_;

    /**
     * Holds the next continuous pc in the trace, which may either be the pc of the
     * next instruction or the value of the next #TRACE_MARKER_TYPE_KERNEL_EVENT marker.
     *
     * This is set to its proper value when the
     * #dynamorio::drmemtrace::reader_base_t::get_next_entry() API returns.
     */
    uint64_t next_trace_pc_ = 0;

private:
    /**
     * This reads the next single entry from the underlying single stream of entries.
     *
     * If it returns nullptr, it will set the EOF bit to distinguish end-of-file from an
     * error.
     *
     * This is used only by #dynamorio::drmemtrace::reader_base_t::get_next_entry()
     * when needed to access the underlying source of entries. Subclasses that
     * need the next entry should use
     * #dynamorio::drmemtrace::reader_base_t::get_next_entry() instead.
     */
    virtual trace_entry_t *
    read_next_entry() = 0;

    /**
     * We store into this queue records already read from the input but not
     * yet returned to the iterator. E.g., #dynamorio::drmemtrace::reader_t
     * needs to read ahead when skipping to include the post-instr records,
     * and #dynamorio::drmemtrace::reader_base_t may read-ahead records from
     * the input source to discover the next continuous pc in the trace.
     *
     * #dynamorio::drmemtrace::reader_base_t::get_next_entry() automatically
     * returns entries from this queue when it's non-empty.
     */
    entry_queue_t queue_;

    bool online_ = true;

    // State of the underlying trace entry source.
    bool at_null_internal_ = false;
    bool at_eof_internal_ = false;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _READER_BASE_H_ */
