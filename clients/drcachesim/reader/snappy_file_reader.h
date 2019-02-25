/* **********************************************************
 * Copyright (c) 2019 Google, Inc.  All rights reserved.
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
#include "file_reader.h"

class snappy_reader_t {
public:
    snappy_reader_t(std::ifstream *stream);

    // Read 'size' bytes into the 'to'.
    int
    read(size_t size, OUT void *to);

    bool
    eof()
    {
        if (!fstream)
            return true;
        return fstream->eof();
    }

private:
    bool
    read_new_chunk();

    enum chunk_type_t : unsigned char {
        COMPRESSED_DATA = 0x00,
        UNCOMPRESSED_DATA = 0x01,
        SKIP_BEGIN = 0x80,
        SKIP_END = 0xfd,
        PADDING = 0xfe,
        STREAM_IDENTIFIER = 0xff,
    };

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
    std::unique_ptr<std::ifstream> fstream;
    // Reader into the decompressed buffer.
    std::unique_ptr<snappy::Source> src;
    // Buffer holding decompressed chunk data.
    std::vector<char> uncompressed_buf;
    // BUffer hodling the compressed chunks themselves.
    std::vector<char> compressed_buf;

    bool seen_magic;

    // Maximum uncompressed chunk size. Fixed by the framing format.
    static constexpr size_t max_block_size = 65536;
    // Maximum compressed chunk size. <= max_block_size, since the compressor only
    // emits a compressed chunk if sizes actually shrink.
    static constexpr size_t max_compressed_size = 65536;
    // Checksum is always 4 bytes. Buffers should reserve space for it as well.
    static constexpr size_t checksum_size = sizeof(uint32_t);
    // Magic string identifying the snappy chunked format.
    static constexpr char magic[] = "sNaPpY";
};

typedef file_reader_t<snappy_reader_t> snappy_file_reader_t;

#endif /* _SNAPPY_FILE_READER_H_ */
