/* **********************************************************
 * Copyright (c) 2015-2023 Google, Inc.  All rights reserved.
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

#define OUT /* just a marker */

#ifdef DEBUG
#    define VPRINT(reader, level, ...)                            \
        do {                                                      \
            if ((reader)->verbosity_ >= (level)) {                \
                fprintf(stderr, "%s ", (reader)->output_prefix_); \
                fprintf(stderr, __VA_ARGS__);                     \
            }                                                     \
        } while (0)
#else
#    define VPRINT(reader, level, ...) /* nothing */
#endif

/**
 * Iterator over #dynamorio::drmemtrace::memref_t trace entries. This class converts a
 * trace (offline or online) into a stream of #dynamorio::drmemtrace::memref_t entries. It
 * also provides more information about the trace using the
 * #dynamorio::drmemtrace::memtrace_stream_t API.
 */
class reader_t : public std::iterator<std::input_iterator_tag, memref_t>,
                 public memtrace_stream_t {
public:
    reader_t()
    {
        cur_ref_ = {};
    }
    reader_t(int verbosity, const char *prefix)
        : verbosity_(verbosity)
        , output_prefix_(prefix)
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
    is_record_synthetic() const override
    {
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
    // Remember top-level headers for the memtrace_stream_t interface.
    uint64_t version_ = 0;
    uint64_t filetype_ = 0;
    uint64_t cache_line_size_ = 0;
    uint64_t chunk_instr_count_ = 0;
    uint64_t page_size_ = 0;
    // We need to read ahead when skipping to include post-instr records.
    // We store into this queue records already read from the input but not
    // yet returned to the iterator.
    std::queue<trace_entry_t> queue_;
    trace_entry_t entry_copy_; // For use in returning a queue entry.

private:
    struct encoding_info_t {
        size_t size = 0;
        unsigned char bits[MAX_ENCODING_LENGTH];
    };

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
    std::unordered_map<addr_t, encoding_info_t> encodings_;
    addr_t last_branch_target_ = 0;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _READER_H_ */
