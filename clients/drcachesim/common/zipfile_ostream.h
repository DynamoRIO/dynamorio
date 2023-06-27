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

// zipfile_osetream_t: an instance of archive_ostream_t for zipfiles.
// We use minizip, which is in the contrib/minizip directory in the zlib
// sources.  The docs are the header files:
// https://github.com/madler/zlib/blob/master/contrib/minizip/zip.h

#ifndef _ZIPFILE_OSTREAM_H_
#define _ZIPFILE_OSTREAM_H_ 1

#include <iostream>
#include <fstream>
#include <sstream>
#include "minizip/zip.h"

namespace dynamorio {
namespace drmemtrace {

// We need to override the stream buffer class which is where the file
// writes happen.  We go ahead and use a simple buffer.  The stream
// buffer base class writes to pbase()..epptr() with the next slot at
// pptr().
class zipfile_streambuf_t : public std::basic_streambuf<char, std::char_traits<char>> {
public:
    explicit zipfile_streambuf_t(const std::string &path)
    {
        zip_ = zipOpen2_64(path.c_str(), APPEND_STATUS_CREATE, nullptr, nullptr);
        if (zip_ == nullptr)
            return;
        buf_ = new char[buffer_size_];
        // We call setp() to set pbase() and epptr() (the buffer bounds).
        // We leave an extra slot for extra_char on overflow.
        setp(buf_, buf_ + buffer_size_ - 1);
        // Caller should invoke open_new_component() for first component.
    }
    ~zipfile_streambuf_t() override
    {
        sync();
        delete[] buf_;
        if (zip_ == nullptr)
            return;
        // We do not bother with the zipfile comment: it doesn't seem to show
        // up when I do set it anyway ("unzip -z" prints the .zip path instead).
        if ((!first_component_ && zipCloseFileInZip(zip_) != ZIP_OK) ||
            zipClose(zip_, nullptr) != ZIP_OK) {
#ifdef DEBUG
            // Let's at least have something visible in debug build.
            std::cerr << "zipfile_ostream failed to close zipfile\n";
#endif
        }
    }
    int
    overflow(int extra_char) override
    {
        if (zip_ == nullptr)
            return traits_type::eof();
        if (extra_char != traits_type::eof()) {
            // Put the extra char into the buffer.  We left an extra slot for it.
            *pptr() = traits_type::to_char_type(extra_char);
            pbump(1);
        }
        int res = traits_type::not_eof(extra_char);
        if (pptr() > pbase()) {
            if (zipWriteInFileInZip(
                    zip_, pbase(), static_cast<unsigned int>(pptr() - pbase())) != ZIP_OK)
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
    std::string
    open_new_component(const std::string &name)
    {
        if (!first_component_) {
            sync();
            if (zipCloseFileInZip(zip_) != ZIP_OK)
                return "Failed to close prior component";
        } else
            first_component_ = false;
        // XXX: We should set the date in a zip_fileinfo struct (3rd param)
        // so it's not 1980 in the file.
        if (zipOpenNewFileInZip(zip_, name.c_str(), nullptr, nullptr, 0, nullptr, 0,
                                nullptr, Z_DEFLATED, Z_DEFAULT_COMPRESSION) != ZIP_OK) {
            return "Failed to add new component to zipfile";
        }
        return "";
    }

private:
    static const int buffer_size_ = 4096;
    zipFile zip_ = nullptr;
    char *buf_ = nullptr;
    bool first_component_ = true;
};

// open_new_component() should be called to create an initial component before
// doing any writing.
class zipfile_ostream_t : public archive_ostream_t {
public:
    explicit zipfile_ostream_t(const std::string &path)
        : archive_ostream_t(new zipfile_streambuf_t(path))
    {
        if (!rdbuf())
            setstate(std::ios::badbit);
    }
    ~zipfile_ostream_t() override
    {
        delete rdbuf();
    }
    std::string
    open_new_component(const std::string &name) override
    {
        zipfile_streambuf_t *zbuf = reinterpret_cast<zipfile_streambuf_t *>(rdbuf());
        return zbuf->open_new_component(name);
    }
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _ZIPFILE_OSTREAM_H_ */
