/* **********************************************************
 * Copyright (c) 2019-2023 Google, Inc.  All rights reserved.
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

/*
 * snappy_file_writer: writes small buffers to a snappy-compressed
 * file in a safe way for use during live tracing.
 *
 * Does not split up large buffers!  Will only write buffers guaranteed
 * to have a compressed size <= 64K: so a maximum uncompressed size
 * of ~53K, based on snappy::MaxCompressedLength.
 *
 * Files are written in the snappy framing format:
 *   https://github.com/google/snappy/blob/master/framing_format.txt
 *
 * This class it not thread-safe and the intent is for the user to create a
 * separate instance per thread.
 *
 * The snappy library allocates memory without parameterizing the allocator, meaning we
 * can't support it for static linking.  We give a warning in drmemtrace_client_main()
 * about this.  We could try to override the global allocator for some uses cases, but
 * that is not workable for statically linking into large C++ applications, and it can
 * be unsafe and seems to cause droption crashes in release build.
 * XXX: Send a patch to libsnappy to parameterize the allocator.
 */

#ifndef _SNAPPY_FILE_WRITER_H_
#define _SNAPPY_FILE_WRITER_H_ 1

#include <snappy.h>
#include "snappy_consts.h"
#include "dr_api.h"

namespace dynamorio {
namespace drmemtrace {

class snappy_file_writer_t : snappy_consts_t {
public:
    snappy_file_writer_t(file_t f,
                         ssize_t (*write_file)(file_t file, const void *data,
                                               size_t count),
                         bool include_checksums = true)
        : fd_(f)
        , write_func_(write_file)
        , include_checksums_(include_checksums)
    {
    }

    bool
    write_file_header();

    // Returns the count of uncompressed data written (i.e., returning anything
    // less than "count" indicates an error).
    ssize_t
    compress_and_write(const void *buf, size_t count);

private:
    file_t fd_;
    char compressed_buf_[header_size_ + checksum_size_ + max_compressed_size_];
    ssize_t (*write_func_)(file_t file, const void *data, size_t count);
    bool include_checksums_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _SNAPPY_FILE_WRITER_H_ */
