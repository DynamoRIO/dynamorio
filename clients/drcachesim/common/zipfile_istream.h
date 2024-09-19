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

/* zipfile_istream_t: provides a std::istream interface for reading components
 * of a zipfile. It provides a continuous stream that cycles through all
 * components. Supports only limited seeking within the current internal buffer.
 */

#ifndef _ZIPFILE_ISTREAM_H_
#define _ZIPFILE_ISTREAM_H_ 1

#ifndef HAS_ZIP
#    error HAS_ZIP is required
#endif
#include <fstream>
#include <zlib.h>
#include "minizip/unzip.h"
#include "archive_istream.h"

namespace dynamorio {
namespace drmemtrace {

/* We need to override the stream buffer class which is where the file
 * reads happen.  The stream buffer base class reads from eback()..egptr()
 * with the next to read at gptr().
 */
class zipfile_istreambuf_t : public std::basic_streambuf<char, std::char_traits<char>> {
public:
    zipfile_istreambuf_t(const std::string &path)
    {
        file_ = unzOpen(path.c_str());
        if (file_ != nullptr && unzGoToFirstFile(file_) == UNZ_OK &&
            unzOpenCurrentFile(file_) == UNZ_OK) {
            buf_ = new char[buffer_size_];
        }
    }
    ~zipfile_istreambuf_t() override
    {
        delete[] buf_;
        if (file_ != nullptr)
            unzClose(file_);
    }
    int
    underflow() override
    {
        if (file_ == nullptr)
            return traits_type::eof();
        if (gptr() == egptr()) {
            int len = unzReadCurrentFile(file_, buf_, buffer_size_);
            if (len == 0) {
                if (unzCloseCurrentFile(file_) != UNZ_OK)
                    return traits_type::eof();
                if (unzGoToNextFile(file_) != UNZ_OK) {
                    // We treat errors just like real eof UNZ_END_OF_LIST_OF_FILE.
                    return traits_type::eof();
                }
                if (unzOpenCurrentFile(file_) != UNZ_OK)
                    return traits_type::eof();
                len = unzReadCurrentFile(file_, buf_, buffer_size_);
            }
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
    std::string
    result_to_string(int code)
    {
        // For now we just turn the number into a string.
        std::ostringstream oss;
        oss << code;
        return oss.str();
    }
    std::string
    open_component(const std::string &name)
    {
        int res;
        if (file_ != nullptr) {
            res = unzCloseCurrentFile(file_);
            if (res != UNZ_OK)
                return "Failed to close current component: " + result_to_string(res);
        }
        res = unzLocateFile(file_, name.c_str(), /*iCaseSensitivity=*/true);
        if (res != UNZ_OK) {
            return "Failed to locate zipfile component " + name + ": " +
                result_to_string(res);
        }
        res = unzOpenCurrentFile(file_);
        if (res != UNZ_OK) {
            return "Failed to open zipfile component " + name + ": " +
                result_to_string(res);
        }
        return "";
    }

private:
    static const int buffer_size_ = 4096;
    unzFile file_ = nullptr;
    char *buf_ = nullptr;
};

class zipfile_istream_t : public archive_istream_t {
public:
    explicit zipfile_istream_t(const std::string &path)
        : archive_istream_t(new zipfile_istreambuf_t(path))
    {
        if (!rdbuf())
            setstate(std::ios::badbit);
    }
    ~zipfile_istream_t() override
    {
        delete rdbuf();
    }
    std::string
    open_component(const std::string &name) override
    {
        zipfile_istreambuf_t *zbuf = reinterpret_cast<zipfile_istreambuf_t *>(rdbuf());
        return zbuf->open_component(name);
    }
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _ZIPFILE_ISTREAM_H_ */
