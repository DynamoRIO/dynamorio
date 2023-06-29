/* **********************************************************
 * Copyright (c) 2023 Google, Inc.  All rights reserved.
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

/* lz4_ostream_t: a wrapper around lz4 to match the parts of the
 * std::ostream interface we use for raw2trace and file_reader_t.
 */

#ifndef _LZ4_OSTREAM_H_
#define _LZ4_OSTREAM_H_ 1

#ifndef HAS_LZ4
#    error HAS_LZ4 is required
#endif

#include <array>
#include <streambuf>
#include <vector>
#include <lz4frame.h>

namespace dynamorio {
namespace drmemtrace {

class lz4_ostreambuf_t : public std::basic_streambuf<char, std::char_traits<char>> {
public:
    lz4_ostreambuf_t(const std::string &path)
    {
        auto res = LZ4F_createCompressionContext(&lzctx_, LZ4F_VERSION);
        if (LZ4F_isError(res)) {
            return;
        }

        dest_buf_.reserve(LZ4F_compressBound(src_buf_.size(), nullptr));
        file_ = new std::ofstream(path);

        char *base = &src_buf_.front();
        setp(base, base + src_buf_.size() - 1);

        write_header();
    }

    ~lz4_ostreambuf_t()
    {
        sync();
        write_footer();
        if (file_ != nullptr) {
            delete file_;
            file_ = nullptr;
        }
        LZ4F_freeCompressionContext(lzctx_);
    }

private:
    int
    overflow(int extra_char) override
    {
        if (file_ == nullptr)
            return traits_type::eof();

        if (extra_char != traits_type::eof()) {
            *pptr() = traits_type::to_char_type(extra_char);
            pbump(1);
        }

        int size = static_cast<int>(pptr() - pbase());
        pbump(-size);
        auto ret = LZ4F_compressUpdate(lzctx_, &dest_buf_.front(), dest_buf_.capacity(),
                                       pbase(), size, nullptr);
        if (LZ4F_isError(ret)) {
            return traits_type::eof();
        }

        file_->write(&dest_buf_.front(), ret);
        return traits_type::not_eof(extra_char);
    }

    int
    sync() override
    {
        return overflow(traits_type::eof());
    }

    void
    write_header()
    {
        auto res =
            LZ4F_compressBegin(lzctx_, &dest_buf_.front(), dest_buf_.capacity(), nullptr);
        if (LZ4F_isError(res)) {
            return;
        }
        file_->write(&dest_buf_.front(), res);
    }

    void
    write_footer()
    {
        auto res =
            LZ4F_compressEnd(lzctx_, &dest_buf_.front(), dest_buf_.capacity(), nullptr);
        if (LZ4F_isError(res)) {
            return;
        }
        file_->write(&dest_buf_.front(), res);
    }

private:
    static const int buffer_size_ = 1024 * 1024;
    std::ostream *file_ = nullptr;
    std::array<char, buffer_size_> src_buf_;
    std::vector<char> dest_buf_;
    LZ4F_compressionContext_t lzctx_ = nullptr;
};

class lz4_ostream_t : public std::ostream {
public:
    explicit lz4_ostream_t(const std::string &path)
        : std::ostream(new lz4_ostreambuf_t(path))
    {
        if (!rdbuf())
            setstate(std::ios::badbit);
    }

    ~lz4_ostream_t() override
    {
        delete rdbuf();
    }
};

} // namespace drmemtrace
} // namespace dynamorio

#endif // _LZ4_OSTREAM_H_
