/* **********************************************************
 * Copyright (c) 2026 Meta Platforms, Inc.  All rights reserved.
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
 * * Neither the name of Meta Platforms, Inc. nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* zstd_istream_t: a wrapper around zstd to match the parts of the
 * std::istream interface we use for file_reader_t.
 * Seeking is not supported.
 */

#ifndef _ZSTD_ISTREAM_H_
#define _ZSTD_ISTREAM_H_

#ifndef HAS_ZSTD
#    error HAS_ZSTD is required
#endif

#include <stdio.h>
#include <istream>
#include <streambuf>
#include <string>
#include <vector>
#include <zstd.h>

namespace dynamorio {
namespace drmemtrace {

class zstd_istreambuf_t : public std::basic_streambuf<char, std::char_traits<char>> {
public:
    explicit zstd_istreambuf_t(const std::string &path)
    {
        file_ = fopen(path.c_str(), "rb");
        if (file_ == nullptr)
            return;
        dctx_ = ZSTD_createDCtx();
        src_buf_.resize(ZSTD_DStreamInSize());
        dst_buf_.resize(ZSTD_DStreamOutSize());
        in_ = { src_buf_.data(), 0, 0 };
    }

    ~zstd_istreambuf_t() override
    {
        if (file_ != nullptr)
            fclose(file_);
        ZSTD_freeDCtx(dctx_);
    }

    bool
    is_open() const
    {
        return file_ != nullptr && dctx_ != nullptr;
    }

    // Returns true if reading stopped early: on a read or decompression error,
    // a checksum mismatch, or data that ends in the middle of a frame.
    bool
    has_error() const
    {
        return error_;
    }

private:
    int
    underflow() override
    {
        if (!is_open() || error_)
            return traits_type::eof();
        if (gptr() < egptr())
            return traits_type::to_int_type(*gptr());
        ZSTD_outBuffer out = { dst_buf_.data(), dst_buf_.size(), 0 };
        while (out.pos == 0) {
            if (in_.pos == in_.size) {
                in_.size = fread(src_buf_.data(), 1, src_buf_.size(), file_);
                in_.pos = 0;
                if (in_.size == 0) {
                    // A complete file ends right after the end of a frame.
                    error_ = ferror(file_) != 0;
                    if (error_ || frame_done_)
                        return traits_type::eof();
                    // The data ends inside a frame.  Hand out any output the
                    // decoder still holds before reporting the truncation.
                    size_t res = ZSTD_decompressStream(dctx_, &out, &in_);
                    if (ZSTD_isError(res) || out.pos == 0) {
                        error_ = true;
                        return traits_type::eof();
                    }
                    frame_done_ = res == 0;
                    break;
                }
            }
            size_t res = ZSTD_decompressStream(dctx_, &out, &in_);
            if (ZSTD_isError(res)) {
                error_ = true;
                return traits_type::eof();
            }
            // 0 means a frame was fully decoded, checksum included, and flushed.
            frame_done_ = res == 0;
        }
        setg(dst_buf_.data(), dst_buf_.data(), dst_buf_.data() + out.pos);
        return traits_type::to_int_type(*gptr());
    }

    FILE *file_ = nullptr;
    ZSTD_DCtx *dctx_ = nullptr;
    std::vector<char> src_buf_;
    std::vector<char> dst_buf_;
    ZSTD_inBuffer in_ = {};
    bool frame_done_ = false;
    bool error_ = false;
};

class zstd_istream_t : public std::istream {
public:
    explicit zstd_istream_t(const std::string &path)
        : std::istream(new zstd_istreambuf_t(path))
    {
        if (!buf()->is_open())
            setstate(std::ios::badbit);
    }

    ~zstd_istream_t() override
    {
        delete rdbuf();
    }

    // See zstd_istreambuf_t::has_error().
    bool
    has_error() const
    {
        return buf()->has_error();
    }

private:
    zstd_istreambuf_t *
    buf() const
    {
        return static_cast<zstd_istreambuf_t *>(rdbuf());
    }
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _ZSTD_ISTREAM_H_ */
