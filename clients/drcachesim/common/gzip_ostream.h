/* **********************************************************
 * Copyright (c) 2018-2023 Google, Inc.  All rights reserved.
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

/* gzip_ostream_t: a wrapper around zlib gzFile to match the parts of the
 * std::ostream interface we use for raw2trace and file_reader_t.
 * Seeking is not supported.
 */

#ifndef _GZIP_OSTREAM_H_
#define _GZIP_OSTREAM_H_ 1

#ifndef HAS_ZLIB
#    error HAS_ZLIB is required
#endif
#include <fstream>
#include <zlib.h>

namespace dynamorio {
namespace drmemtrace {

/* We need to override the stream buffer class which is where the file
 * writes happen.  We go ahead and use a simple buffer.  The stream
 * buffer base class writes to pbase()..epptr() with the next slot at
 * pptr().
 */
class gzip_streambuf_t : public std::basic_streambuf<char, std::char_traits<char>> {
public:
    explicit gzip_streambuf_t(const std::string &path)
    {
        file_ = gzopen(path.c_str(), "wb");
        if (file_ != nullptr) {
            buf_ = new char[buffer_size_];
            // We leave an extra slot for extra_char on overflow.
            setp(buf_, buf_ + buffer_size_ - 1);
        }
    }
    ~gzip_streambuf_t() override
    {
        sync();
        delete[] buf_;
        if (file_ != nullptr)
            gzclose(file_);
    }
    int
    overflow(int extra_char) override
    {
        if (file_ == nullptr)
            return traits_type::eof();
        if (extra_char != traits_type::eof()) {
            // Put the extra char into the buffer.  We left an extra slot for it.
            *pptr() = traits_type::to_char_type(extra_char);
            pbump(1);
        }
        int res = traits_type::not_eof(extra_char);
        if (pptr() > pbase()) {
            int len =
                gzwrite(file_, pbase(), static_cast<unsigned int>(pptr() - pbase()));
            if (len < pptr() - pbase())
                res = traits_type::eof();
        }
        setp(buf_, buf_ + buffer_size_ - 1);
        return res;
    }
    int
    sync() override
    {
        return overflow(traits_type::eof());
    }

private:
    static const int buffer_size_ = 4096;
    gzFile file_ = nullptr;
    char *buf_ = nullptr;
};

class gzip_ostream_t : public std::ostream {
public:
    explicit gzip_ostream_t(const std::string &path)
        : std::ostream(new gzip_streambuf_t(path))
    {
        if (!rdbuf())
            setstate(std::ios::badbit);
    }
    ~gzip_ostream_t() override
    {
        delete rdbuf();
    }
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _GZIP_OSTREAM_H_ */
