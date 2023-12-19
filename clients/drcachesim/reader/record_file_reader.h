/* **********************************************************
 * Copyright (c) 2022-2023 Google, Inc.  All rights reserved.
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

/* record_reader_t and record_file_reader_t provide access to the
 * stream of trace_entry_t exactly as present in a stored offline trace.
 */

#ifndef _RECORD_FILE_READER_H_
#define _RECORD_FILE_READER_H_ 1

#include <assert.h>
#include <iterator>
#include <memory>

#include "memtrace_stream.h"
#include "reader.h"
#include "trace_entry.h"

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

namespace dynamorio {
namespace drmemtrace {

/**
 * Trace reader that provides the stream of #dynamorio::drmemtrace::trace_entry_t exactly
 * as present in an offline trace stored on disk. The public API is similar to
 * #dynamorio::drmemtrace::reader_t, except that it is an iterator over
 * #dynamorio::drmemtrace::trace_entry_t entries instead of
 * #dynamorio::drmemtrace::memref_t. This does not yet support iteration over a serialized
 * stream of multiple traces.
 *
 * TODO i#5727: Convert #dynamorio::drmemtrace::reader_t and file_reader_t into templates:
 * `reader_tmpl_t<RecordType>` and file_reader_tmpl_t<T, RecordType> where T is one of
 * gzip_reader_t, zipfile_reader_t, snappy_reader_t, std::ifstream*, and RecordType is one
 * of #dynamorio::drmemtrace::memref_t, #dynamorio::drmemtrace::trace_entry_t. Then,
 * typedef the #dynamorio::drmemtrace::trace_entry_t specializations as record_reader_t
 * and record_file_reader_t<T> respectively. This will allow significant code reuse,
 * particularly for serializing multiple thread traces into a single stream.
 *
 * Since the current file_reader_t is already a template on T, adding the second template
 * parameter RecordType is complex. Note that we cannot have partial specialization of
 * member functions in C++. This complicates implementation of various
 * file_reader_tmpl_t<T, RecordType> specializations for T, as we would need to duplicate
 * the implementation for each candidate of RecordType.
 *
 * We have two options:
 * 1. For each member function specialized for some T, duplicate the definition for
 *    file_reader_tmpl_t<T, #dynamorio::drmemtrace::memref_t> and file_reader_tmpl_t<T,
 * trace_entry_t>. This has the obvious disadvantage of code duplication, which can be
 * mitigated to some extent by extracting common logic in static routines.
 * 2. For each specialization of T, create a subclass templatized on RecordType that
 *    inherits from file_reader_tmpl_t<_, RecordType>. E.g. for T = gzip_reader_t, create
 *    `class gzip_file_reader_t<RecordType>: public file_reader_tmpl_t<gzip_reader_t,
 *    RecordType>` This has the disadvantage of breaking backward-compatibility of the
 *    existing reader interface. Users that define their own readers outside DR will need
 *    to adapt to this change. The advantage of this approach is that it is somewhat
 *    cleaner to have proper classes instead of template specializations for file readers.
 *
 * We prefer Option 2, since it has higher merit.
 *
 * Currently we do not have any use-case that needs this design, but when we need to
 * support serial iteration over #dynamorio::drmemtrace::trace_entry_t, we would want to
 * do this to reuse the existing multiple trace serialization code in file_reader_t.
 * file_reader_t hides some #dynamorio::drmemtrace::trace_entry_t entries today (like
 * TRACE_TYPE_THREAD, TRACE_TYPE_PID, etc); we would also need to avoid doing that since
 * record_reader_t is expected to provide the exact stream of
 * #dynamorio::drmemtrace::trace_entry_t as stored on disk.
 */
class record_reader_t : public std::iterator<std::input_iterator_tag, trace_entry_t>,
                        public memtrace_stream_t {
public:
    record_reader_t(int verbosity, const char *prefix)
        : verbosity_(verbosity)
        , output_prefix_(prefix)
        , eof_(false)
    {
    }
    record_reader_t()
    {
    }
    virtual ~record_reader_t()
    {
    }
    bool
    init()
    {
        if (!open_input_file())
            return false;
        ++*this;
        return true;
    }

    const trace_entry_t &
    operator*()
    {
        return cur_entry_;
    }

    record_reader_t &
    operator++()
    {
        bool res = read_next_entry();
        assert(res || eof_);
        UNUSED(res);
        if (!eof_) {
            ++cur_ref_count_;
            if (type_is_instr(static_cast<trace_type_t>(cur_entry_.type)))
                ++cur_instr_count_;
            else if (cur_entry_.type == TRACE_TYPE_MARKER) {
                switch (cur_entry_.size) {
                case TRACE_MARKER_TYPE_VERSION: version_ = cur_entry_.addr; break;
                case TRACE_MARKER_TYPE_FILETYPE: filetype_ = cur_entry_.addr; break;
                case TRACE_MARKER_TYPE_CACHE_LINE_SIZE:
                    cache_line_size_ = cur_entry_.addr;
                    break;
                case TRACE_MARKER_TYPE_PAGE_SIZE: page_size_ = cur_entry_.addr; break;
                case TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT:
                    chunk_instr_count_ = cur_entry_.addr;
                    break;
                case TRACE_MARKER_TYPE_TIMESTAMP:
                    last_timestamp_ = cur_entry_.addr;
                    if (first_timestamp_ == 0)
                        first_timestamp_ = last_timestamp_;
                    break;
                case TRACE_MARKER_TYPE_SYSCALL_TRACE_START:
                case TRACE_MARKER_TYPE_CONTEXT_SWITCH_START:
                    in_kernel_trace_ = true;
                    break;
                case TRACE_MARKER_TYPE_SYSCALL_TRACE_END:
                case TRACE_MARKER_TYPE_CONTEXT_SWITCH_END:
                    in_kernel_trace_ = false;
                    break;
                }
            }
        }
        return *this;
    }

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

    virtual bool
    operator==(const record_reader_t &rhs) const
    {
        return BOOLS_MATCH(eof_, rhs.eof_);
    }
    virtual bool
    operator!=(const record_reader_t &rhs) const
    {
        return !BOOLS_MATCH(eof_, rhs.eof_);
    }

    virtual record_reader_t &
    skip_instructions(uint64_t instruction_count)
    {
        uint64_t stop_count = cur_instr_count_ + instruction_count + 1;
        while (cur_instr_count_ < stop_count) {
            ++(*this);
        }
        return *this;
    }

protected:
    virtual bool
    read_next_entry() = 0;
    virtual bool
    open_single_file(const std::string &input_path) = 0;
    virtual bool
    open_input_file() = 0;

    trace_entry_t cur_entry_ = {};
    int verbosity_;
    const char *output_prefix_;
    // Following typical stream iterator convention, the default constructor
    // produces an EOF object.
    bool eof_ = true;

private:
    uint64_t cur_ref_count_ = 0;
    uint64_t cur_instr_count_ = 0;
    uint64_t last_timestamp_ = 0;
    uint64_t first_timestamp_ = 0;

    // Remember top-level headers for the memtrace_stream_t interface.
    uint64_t version_ = 0;
    uint64_t filetype_ = 0;
    uint64_t cache_line_size_ = 0;
    uint64_t chunk_instr_count_ = 0;
    uint64_t page_size_ = 0;
    bool in_kernel_trace_ = false;
};

/**
 * Similar to file_reader_t, templatizes on the file type for specializing
 * for compression and different file types.
 */
template <typename T> class record_file_reader_t : public record_reader_t {
public:
    record_file_reader_t(const std::string &path, int verbosity = 0,
                         const char *prefix = "[record_file_reader_t]")
        : record_reader_t(verbosity, prefix)
        , input_path_(path)
    {
    }
    record_file_reader_t()
    {
    }
    std::string
    get_stream_name() const override
    {
        size_t ind = input_path_.find_last_of(DIRSEP);
        assert(ind != std::string::npos);
        return input_path_.substr(ind + 1);
    }
    virtual ~record_file_reader_t();

private:
    bool
    open_input_file() override
    {
        if (!input_path_.empty()) {
            if (!open_single_file(input_path_)) {
                ERRMSG("Failed to open %s\n", input_path_.c_str());
                return false;
            }
        }
        return true;
    }

    std::unique_ptr<T> input_file_ = nullptr;
    std::string input_path_;

    bool
    open_single_file(const std::string &path) override;
    bool
    read_next_entry() override;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _RECORD_FILE_READER_H_ */
