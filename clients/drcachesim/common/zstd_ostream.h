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

/* zstd_ostream_t: a wrapper around zstd to match the parts of the
 * std::ostream interface we use for raw2trace.
 */

#ifndef _ZSTD_OSTREAM_H_
#define _ZSTD_OSTREAM_H_

#ifndef HAS_ZSTD
#    error HAS_ZSTD is required
#endif

#include <fstream>
#include <streambuf>
#include <string>
#include <vector>
#include <zstd.h>

namespace dynamorio {
namespace drmemtrace {

class zstd_ostreambuf_t : public std::basic_streambuf<char, std::char_traits<char>> {
public:
    explicit zstd_ostreambuf_t(const std::string &path)
    {
        cctx_ = ZSTD_createCCtx();
        if (cctx_ == nullptr)
            return;
        // Level 3 keeps zstd's default 2MB window, so a reader needs only a few MB
        // per open file.  The checksum lets readers detect corrupted data when they
        // reach the end of the file.
        if (ZSTD_isError(
                ZSTD_CCtx_setParameter(cctx_, ZSTD_c_compressionLevel, level_)) ||
            ZSTD_isError(ZSTD_CCtx_setParameter(cctx_, ZSTD_c_checksumFlag, 1)))
            return;
        file_.open(path, std::ios::binary);
        if (!file_)
            return;
        src_buf_.resize(ZSTD_CStreamInSize());
        dst_buf_.resize(ZSTD_CStreamOutSize());
        // We leave an extra slot for extra_char on overflow.
        setp(src_buf_.data(), src_buf_.data() + src_buf_.size() - 1);
        ok_ = true;
    }

    ~zstd_ostreambuf_t() override
    {
        // Ending the frame flushes all pending data.  A failure here leaves a
        // truncated frame, which the reader reports as an error.
        if (ok_)
            compress(ZSTD_e_end);
        ZSTD_freeCCtx(cctx_);
    }

    bool
    is_open() const
    {
        return ok_;
    }

private:
    int
    overflow(int extra_char) override
    {
        if (!ok_)
            return traits_type::eof();
        if (extra_char != traits_type::eof()) {
            *pptr() = traits_type::to_char_type(extra_char);
            pbump(1);
        }
        if (!compress(ZSTD_e_continue))
            return traits_type::eof();
        return traits_type::not_eof(extra_char);
    }

    int
    sync() override
    {
        return overflow(traits_type::eof()) == traits_type::eof() ? -1 : 0;
    }

    // Compresses the buffered input and writes whatever output zstd produces.
    // ZSTD_e_end also finishes the frame.
    bool
    compress(ZSTD_EndDirective mode)
    {
        ZSTD_inBuffer in = { pbase(), static_cast<size_t>(pptr() - pbase()), 0 };
        bool done;
        do {
            ZSTD_outBuffer out = { dst_buf_.data(), dst_buf_.size(), 0 };
            size_t left_to_write = ZSTD_compressStream2(cctx_, &out, &in, mode);
            if (ZSTD_isError(left_to_write) ||
                !file_.write(dst_buf_.data(), static_cast<std::streamsize>(out.pos))) {
                ok_ = false;
                return false;
            }
            // With ZSTD_e_continue, zstd may keep some data internally, so we are
            // done once it has taken all our input.  With ZSTD_e_end, we are done
            // once it has written the whole frame, including the checksum.
            if (mode == ZSTD_e_end)
                done = left_to_write == 0;
            else
                done = in.pos == in.size;
        } while (!done);
        setp(src_buf_.data(), src_buf_.data() + src_buf_.size() - 1);
        return true;
    }

    static constexpr int level_ = 3;
    ZSTD_CCtx *cctx_ = nullptr;
    std::ofstream file_;
    std::vector<char> src_buf_;
    std::vector<char> dst_buf_;
    bool ok_ = false;
};

class zstd_ostream_t : public std::ostream {
public:
    explicit zstd_ostream_t(const std::string &path)
        : std::ostream(new zstd_ostreambuf_t(path))
    {
        if (!static_cast<zstd_ostreambuf_t *>(rdbuf())->is_open())
            setstate(std::ios::badbit);
    }

    ~zstd_ostream_t() override
    {
        delete rdbuf();
    }
};

} // namespace drmemtrace
} // namespace dynamorio

#endif // _ZSTD_OSTREAM_H_
