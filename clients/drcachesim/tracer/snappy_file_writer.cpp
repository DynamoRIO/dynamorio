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

#include "snappy_file_writer.h"
#include <string.h>

namespace dynamorio {
namespace drmemtrace {

bool
snappy_file_writer_t::write_file_header()
{
    char header[10]; // 4-byte header + 6-byte magic string.
    header[0] = STREAM_IDENTIFIER;
    size_t len = strlen(magic_);
    if (sizeof(header) != 4 + len)
        return false;
    memcpy(header + 1, &len, 3);
    memcpy(header + 4, magic_, len);
    ssize_t wrote = write_func_(fd_, header, sizeof(header));
    return wrote == sizeof(header);
}

ssize_t
snappy_file_writer_t::compress_and_write(const void *buf, size_t count)
{
    if (count > sizeof(compressed_buf_))
        return -1;
    size_t compressed_count;
    size_t crc_size = include_checksums_ ? checksum_size_ : 0;
    // We only support single-output-buffer inputs.
    if (snappy::MaxCompressedLength(count) + header_size_ + crc_size >
        sizeof(compressed_buf_))
        return -1;
    snappy::RawCompress(static_cast<const char *>(buf), count,
                        compressed_buf_ + header_size_ + crc_size, &compressed_count);
    if (compressed_count + header_size_ + crc_size > sizeof(compressed_buf_))
        return -1;
    uint32_t checksum = 0;
    if (include_checksums_)
        checksum = mask_crc32(static_cast<const char *>(buf), count);
    if (compressed_count >= count) {
        // Leave it uncompressed.
        size_t data_size = count + crc_size;
        char header[8];
        header[0] = include_checksums_ ? UNCOMPRESSED_DATA : UNCOMPRESSED_DATA_NO_CRC;
        memcpy(header + 1, &data_size, 3);
        size_t header_size = 4 + crc_size;
        if (include_checksums_)
            memcpy(header + 4, &checksum, crc_size);
        ssize_t wrote = write_func_(fd_, header, header_size);
        if (wrote < static_cast<ssize_t>(header_size))
            return wrote;
        return write_func_(fd_, buf, count);
    } else {
        size_t data_size = compressed_count + crc_size;
        compressed_buf_[0] =
            include_checksums_ ? COMPRESSED_DATA : COMPRESSED_DATA_NO_CRC;
        memcpy(compressed_buf_ + 1, &data_size, 3);
        if (include_checksums_)
            memcpy(compressed_buf_ + 4, &checksum, crc_size);
        ssize_t wrote =
            write_func_(fd_, compressed_buf_, compressed_count + header_size_ + crc_size);
        if (wrote <= 0)
            return wrote;
        // The caller wants the count of uncompressed data written.
        return count;
    }
}

} // namespace drmemtrace
} // namespace dynamorio
