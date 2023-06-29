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
 * snappy_file_reader: reads snappy-compressed files containing memory traces. Files
 * should follow the snappy framing format:
 * https://github.com/google/snappy/blob/master/framing_format.txt
 */

#ifndef _SNAPPY_FILE_READER_H_
#define _SNAPPY_FILE_READER_H_ 1

#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <snappy.h>
#include <snappy-sinksource.h>
#include "snappy_consts.h"
#include "file_reader.h"

namespace dynamorio {
namespace drmemtrace {

class snappy_reader_t : snappy_consts_t {
public:
    snappy_reader_t() = default;
    snappy_reader_t(std::ifstream *stream);

    // Read 'size' bytes into the 'to'.
    int
    read(size_t size, OUT void *to);

    bool
    eof()
    {
        if (!fstream_)
            return true;
        return fstream_->eof();
    }

private:
    bool
    read_new_chunk();

    // Read a new data chunk in uncompressed_buf.
    bool
    read_data_chunk(uint32_t size, chunk_type_t type);

    // Have we seen the magic chunk identifying a snappy stream.
    bool
    check_magic();

    // Read and verify magic chunk.
    bool
    read_magic(uint32_t size);

    // The compressed file we're reading from.
    std::unique_ptr<std::ifstream> fstream_;
    // Reader into the decompressed buffer.
    std::unique_ptr<snappy::Source> src_;
    // Buffer holding decompressed chunk data.
    std::vector<char> uncompressed_buf_;
    // Buffer holding the compressed chunks themselves.
    std::vector<char> compressed_buf_;

    bool seen_magic_;
};

typedef file_reader_t<snappy_reader_t> snappy_file_reader_t;

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _SNAPPY_FILE_READER_H_ */
