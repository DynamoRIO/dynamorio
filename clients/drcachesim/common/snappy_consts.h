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
 * snappy_consts: shared constants between the reader and writer for
 * the snappy framing format:
 * https://github.com/google/snappy/blob/master/framing_format.txt
 */

#ifndef _SNAPPY_CONSTS_H_
#define _SNAPPY_CONSTS_H_ 1

#include <cstddef>
#include "crc32c.h"

namespace dynamorio {
namespace drmemtrace {

class snappy_consts_t {
protected:
    enum chunk_type_t : unsigned char {
        COMPRESSED_DATA = 0x00,
        UNCOMPRESSED_DATA = 0x01,
        // We've added these no-CRC types only locally.
        // XXX i#5427: Propose adding these to the public spec.
        COMPRESSED_DATA_NO_CRC = 0x02,
        UNCOMPRESSED_DATA_NO_CRC = 0x03,
        SKIP_BEGIN = 0x80,
        SKIP_END = 0xfd,
        PADDING = 0xfe,
        STREAM_IDENTIFIER = 0xff,
    };

    // Mask CRC32 checksum, as defined in
    // https://github.com/google/snappy/blob/main/framing_format.txt, sec. 3.
    uint32_t
    mask_crc32(const char *buf, const uint32_t len)
    {
        uint32_t checksum = crc32c(buf, len);
        return ((checksum >> 15) | (checksum << 17)) + 0xa282ead8;
    }

    // Maximum uncompressed chunk size. Fixed by the framing format.
    static constexpr size_t max_block_size_ = 65536;
    // Maximum compressed chunk size. <= max_block_size, since the compressor only
    // emits a compressed chunk if sizes actually shrink.
    static constexpr size_t max_compressed_size_ = 65536;
    // Checksum is always 4 bytes. Buffers should reserve space for it as well.
    static constexpr size_t checksum_size_ = sizeof(uint32_t);
    // Chunk header size is always 4 bytes.  This is followed by the checksum for
    // data chunks.
    static constexpr size_t header_size_ = sizeof(uint32_t);
    // Magic string identifying the snappy chunked format.
    static constexpr char magic_[] = "sNaPpY";
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _SNAPPY_CONSTS_H_ */
