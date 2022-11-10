/* **********************************************************
 * Copyright (c) 2022 Google, Inc.  All rights reserved.
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
#include "memtrace_stream.h"
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

/* Even though we plan to support only record_file_reader_t for
 * reading trace_entry_t (and no ipc reader like ipc_reader_t) we still need
 * this input-file-type-agnostic record_reader_t base class.
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
        ++cur_ref_count_;
        if (type_is_instr((trace_type_t)cur_entry_.type))
            ++cur_instr_count_;
        return *this;
    }

    uint64_t
    get_record_ordinal() override
    {
        return cur_ref_count_;
    }
    uint64_t
    get_instruction_ordinal() override
    {
        return cur_instr_count_;
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
};

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
    get_stream_name() override
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

    T *input_file_ = nullptr;
    std::string input_path_;

    bool
    open_single_file(const std::string &path) override;
    bool
    read_next_entry() override;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _RECORD_FILE_READER_H_ */
