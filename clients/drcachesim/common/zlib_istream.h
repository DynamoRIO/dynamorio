/* **********************************************************
 * Copyright (c) 2020-2023 Google, Inc.  All rights reserved.
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

/* zlib_istream_t: a wrapper around zlib to match the parts of the
 * std::istream interface we use for raw2trace and file_reader_t.
 * Supports only limited seeking within the current internal buffer.
 */

#ifndef _ZLIB_ISTREAM_H_
#define _ZLIB_ISTREAM_H_ 1

#ifndef HAS_ZLIB
#    error HAS_ZLIB is required
#endif
#include <fstream>
#include <zlib.h>
#include <iostream>

namespace dynamorio {
namespace drmemtrace {

/* We need to override the stream buffer class which is where the file
 * reads happen.  The stream buffer base class reads from eback()..egptr()
 * with the next to read at gptr().
 */
class zlib_istreambuf_t : public std::basic_streambuf<char, std::char_traits<char>> {
public:
    zlib_istreambuf_t(const std::string &path)
    {
        file_ = fopen(path.c_str(), "rb");
        if (file_ == nullptr)
            return;
        memset(&zstream_, 0, sizeof(zstream_));
        if (inflateInit(&zstream_) != Z_OK) {
            fclose(file_);
            file_ = nullptr;
            return;
        }
        zstream_.avail_out = buffer_size_;
        buf_compressed_ = new char[buffer_size_];
        buf_uncompressed_ = new char[buffer_size_];
    }
    ~zlib_istreambuf_t() override
    {
        if (file_ != nullptr) {
            inflateEnd(&zstream_);
            fclose(file_);
        }
        delete[] buf_compressed_;
        delete[] buf_uncompressed_;
    }
    int
    underflow() override
    {
        if (file_ == nullptr)
            return traits_type::eof();
        if (gptr() == egptr()) {
            // If the last inflate didn't fill the output buffer, it
            // finished with the input buffer and we need to read more.
            if (zstream_.avail_out != 0) {
                zstream_.avail_in =
                    static_cast<uInt>(fread(buf_compressed_, 1, buffer_size_, file_));
                if (ferror(file_) || zstream_.avail_in == 0)
                    return traits_type::eof();
                zstream_.next_in = reinterpret_cast<Bytef *>(buf_compressed_);
            }
            zstream_.avail_out = buffer_size_;
            zstream_.next_out = reinterpret_cast<Bytef *>(buf_uncompressed_);
            int res = inflate(&zstream_, Z_NO_FLUSH);
            switch (res) {
            case Z_STREAM_ERROR:
            case Z_NEED_DICT:
            case Z_DATA_ERROR:
            case Z_MEM_ERROR: inflateEnd(&zstream_); return traits_type::eof();
            }
            size_t have = buffer_size_ - zstream_.avail_out;
            setg(buf_uncompressed_, buf_uncompressed_, buf_uncompressed_ + have);
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
    FILE *file_;
    z_stream zstream_;
    char *buf_uncompressed_ = nullptr;
    char *buf_compressed_ = nullptr;
};

class zlib_istream_t : public std::istream {
public:
    explicit zlib_istream_t(const std::string &path)
        : std::istream(new zlib_istreambuf_t(path))
    {
        if (!rdbuf())
            setstate(std::ios::badbit);
    }
    virtual ~zlib_istream_t() override
    {
        delete rdbuf();
    }
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _ZLIB_ISTREAM_H_ */
