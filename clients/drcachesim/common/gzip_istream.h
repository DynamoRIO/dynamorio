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

/* gzip_istream_t: a wrapper around zlib gzFile to match the parts of the
 * std::istream interface we use for raw2trace and file_reader_t.
 * Supports only limited seeking within the current internal buffer.
 */

#ifndef _GZIP_ISTREAM_H_
#define _GZIP_ISTREAM_H_ 1

#ifndef HAS_ZLIB
#    error HAS_ZLIB is required
#endif
#include <fstream>
#include <zlib.h>

namespace dynamorio {
namespace drmemtrace {

/* We need to override the stream buffer class which is where the file
 * reads happen.  The stream buffer base class reads from eback()..egptr()
 * with the next to read at gptr().
 */
class gzip_istreambuf_t : public std::basic_streambuf<char, std::char_traits<char>> {
public:
    gzip_istreambuf_t(const std::string &path)
    {
        file_ = gzopen(path.c_str(), "rb");
        if (file_ != nullptr) {
            buf_ = new char[buffer_size_];
        }
    }
    ~gzip_istreambuf_t() override
    {
        delete[] buf_;
        if (file_ != nullptr)
            gzclose(file_);
    }
    int
    underflow() override
    {
        if (file_ == nullptr)
            return traits_type::eof();
        if (gptr() == egptr()) {
            int len = gzread(file_, buf_, buffer_size_);
            if (len <= 0)
                return traits_type::eof();
            setg(buf_, buf_, buf_ + len);
        }
        return *gptr();
    }
    std::iostream::pos_type
    seekoff(std::iostream::off_type off, std::ios_base::seekdir dir,
            std::ios_base::openmode which = std::ios_base::in) override
    {
        if (dir == std::ios_base::cur &&
            ((off >= 0 && gptr() + off < egptr()) ||
             (off < 0 && gptr() + off >= eback())))
            gbump(static_cast<int>(off));
        else {
            // Unsupported!
            return -1;
        }
        return gptr() - eback();
    }

private:
    static const int buffer_size_ = 4096;
    gzFile file_ = nullptr;
    char *buf_ = nullptr;
};

class gzip_istream_t : public std::istream {
public:
    explicit gzip_istream_t(const std::string &path)
        : std::istream(new gzip_istreambuf_t(path))
    {
        if (!rdbuf())
            setstate(std::ios::badbit);
    }
    virtual ~gzip_istream_t() override
    {
        delete rdbuf();
    }
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _GZIP_ISTREAM_H_ */
