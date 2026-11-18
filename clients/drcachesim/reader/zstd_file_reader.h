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

/* zstd_file_reader: reads zstd-compressed files containing memory traces. */

#ifndef _ZSTD_FILE_READER_H_
#define _ZSTD_FILE_READER_H_

#include "zstd_istream.h"
#include "file_reader.h"

namespace dynamorio {
namespace drmemtrace {

struct zstd_reader_t {
    zstd_reader_t()
        : file(nullptr) {};
    explicit zstd_reader_t(zstd_istream_t *file)
        : file(file)
    {
    }
    zstd_istream_t *file;
    trace_entry_t buf[4096];
    trace_entry_t *cur_buf = buf;
    trace_entry_t *max_buf = buf;
};

typedef file_reader_t<zstd_reader_t> zstd_file_reader_t;

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _ZSTD_FILE_READER_H_ */
