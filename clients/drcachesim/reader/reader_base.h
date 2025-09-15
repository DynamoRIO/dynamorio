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
 * Base class for #dynamorio::drmemtrace:reader_t and
 * #dynamorio::drmemtrace::record_reader_t. This contains some interfaces and
 * implementations that are shared between the two types of readers.
 *
 * This base class is intended for logic close to reading the entries, and the
 * reader interface common to the two types of readers; not so much for
 * reader-specific logic for what to do with the entries.
 *
 * This subclasses #dynamorio::drmemtrace::memtrace_stream_t because the readers
 * derived from it are expected to implement that interface. We want to avoid
 * subclasses having to inherit from multiple base classes
 * (#dynamorio::drmemtrace::reader_base_t and
 * #dynamorio::drmemtrace::memtrace_stream_t), so it is better for us to
 * inherit from #dynamorio::drmemtrace::memtrace_stream_t here.
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

    bool
    operator==(const reader_base_t &rhs) const;
    bool
    operator!=(const reader_base_t &rhs) const;

protected:
    /**
     * Returns the next entry for this reader.
     *
     * If it returns nullptr, it will set the #at_eof_ field to distinguish end-of-file
     * from an error.
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
     * We store into this queue records already read from the input but not
     * yet returned to the iterator. E.g., #dynamorio::drmemtrace::reader_t
     * needs to read ahead when skipping to include the post-instr records.
     */
    std::queue<trace_entry_t> queue_;
    trace_entry_t entry_copy_;

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

    bool online_ = true;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _READER_BASE_H_ */
