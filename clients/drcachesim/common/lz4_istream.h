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

/* lz4_istream_t: a wrapper around lz4 to match the parts of the
 * std::istream interface we use for raw2trace and file_reader_t.
 * Supports only limited seeking within the current internal buffer.
 */

#ifndef _LZ4_ISTREAM_H_
#define _LZ4_ISTREAM_H_ 1

#ifndef HAS_LZ4
#    error HAS_LZ4 is required
#endif
#include <fstream>
#include <lz4frame.h>
#include <iostream>

namespace dynamorio {
namespace drmemtrace {

/* We need to override the stream buffer class which is where the file
 * reads happen.  The stream buffer base class reads from eback()..egptr()
 * with the next to read at gptr().
 */
class lz4_istreambuf_t : public std::basic_streambuf<char, std::char_traits<char>> {
public:
    lz4_istreambuf_t(const std::string &path)
    {
        if (buffer_size_ <= LZ4F_HEADER_SIZE_MAX)
            return;
        file_ = fopen(path.c_str(), "rb");
        if (file_ == nullptr)
            return;
        size_t res = LZ4F_createDecompressionContext(&lzcxt_, LZ4F_VERSION);
        if (LZ4F_isError(res)) {
            fclose(file_);
            file_ = nullptr;
            return;
        }
        buf_compressed_ = new char[buffer_size_];
        buf_uncompressed_ = new char[buffer_size_];
        cur_src_ = buf_compressed_;
    }
    ~lz4_istreambuf_t() override
    {
        if (file_ != nullptr) {
            fclose(file_);
        }
        delete[] buf_compressed_;
        delete[] buf_uncompressed_;
        LZ4F_freeDecompressionContext(lzcxt_);
    }
    int
    underflow() override
    {
        if (file_ == nullptr)
            return traits_type::eof();
        if (gptr() == egptr()) {
            size_t dst_sz;
            do {
                if (src_left_ == 0) {
                    src_left_ = fread(buf_compressed_, 1, buffer_size_, file_);
                    if (ferror(file_) || src_left_ == 0)
                        return traits_type::eof();
                    cur_src_ = buf_compressed_;
                }
                size_t src_sz = src_left_;
                dst_sz = buffer_size_;
                size_t res = LZ4F_decompress(lzcxt_, buf_uncompressed_, &dst_sz, cur_src_,
                                             &src_sz, nullptr);
                if (LZ4F_isError(res))
                    return traits_type::eof();
                src_left_ -= src_sz;
                cur_src_ += src_sz;
            } while (dst_sz == 0);
            setg(buf_uncompressed_, buf_uncompressed_, buf_uncompressed_ + dst_sz);
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
            gbump(off);
        else {
            // Unsupported!
            return -1;
        }
        return gptr() - eback();
    }

private:
    static const int buffer_size_ = 1024 * 1024;
    LZ4F_dctx *lzcxt_ = nullptr;
    FILE *file_;
    char *cur_src_ = nullptr;
    char *buf_uncompressed_ = nullptr;
    char *buf_compressed_ = nullptr;
    size_t src_left_ = 0;
};

class lz4_istream_t : public std::istream {
public:
    explicit lz4_istream_t(const std::string &path)
        : std::istream(new lz4_istreambuf_t(path))
    {
        if (!rdbuf())
            setstate(std::ios::badbit);
    }
    virtual ~lz4_istream_t() override
    {
        delete rdbuf();
    }
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _LZ4_ISTREAM_H_ */
