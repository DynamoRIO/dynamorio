/* **********************************************************
 * Copyright (c) 2019-2020 Google, Inc.  All rights reserved.
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

#include "snappy_file_reader.h"
#include "crc32c.h"

namespace {
// Mask CRC32 checksum, as defined in framing_format.txt, sec. 3.
uint32_t
mask_crc32(uint32_t checksum)
{
    return ((checksum >> 15) | (checksum << 17)) + 0xa282ead8;
}

} // namespace

constexpr size_t snappy_reader_t::max_block_size_;
constexpr size_t snappy_reader_t::max_compressed_size_;
constexpr size_t snappy_reader_t::checksum_size_;
constexpr char snappy_reader_t::magic_[];

snappy_reader_t::snappy_reader_t(std::ifstream *stream)
    : fstream_(stream)
    , uncompressed_buf_(max_block_size_ + checksum_size_)
    , compressed_buf_(max_compressed_size_ + checksum_size_)
    , seen_magic_(false)
{
}
bool
snappy_reader_t::read_magic(uint32_t size)
{
    if (size > strlen(magic_)) {
        ERRMSG("Magic block size too large. Got %u, want %zu\n", size, strlen(magic_));
        return false;
    }

    fstream_->read(uncompressed_buf_.data(), size);
    if (!(*fstream_)) {
        return false;
    }
    if (strncmp(uncompressed_buf_.data(), magic_, size) != 0) {
        uncompressed_buf_[size] = '\0';
        ERRMSG("Unknown file type, got magic %s, want %s\n", uncompressed_buf_.data(),
               magic_);
        return false;
    }
    seen_magic_ = true;
    return true;
}

bool
snappy_reader_t::check_magic()
{
    if (!seen_magic_) {
        ERRMSG("Unknown file type, must start with magic chunk %s\n", magic_);
        return false;
    }
    return true;
}

bool
snappy_reader_t::read_data_chunk(uint32_t size, chunk_type_t type)
{
    char *buf =
        (type == COMPRESSED_DATA) ? compressed_buf_.data() : uncompressed_buf_.data();
    size_t max_size = (type == COMPRESSED_DATA) ? max_compressed_size_ : max_block_size_;
    max_size += checksum_size_;
    if (size < checksum_size_ || size > max_size) {
        ERRMSG("Corrupted chunk header. Size %u, want <= %zu >= %zu.\n", size, max_size,
               checksum_size_);
        return false;
    }

    fstream_->read(buf, size);
    if (!(*fstream_)) {
        return false;
    }

    uint32_t read_checksum = 0;
    memcpy(&read_checksum, buf, 4);

    src_.reset(new snappy::ByteArraySource(&uncompressed_buf_[checksum_size_],
                                           size - checksum_size_));

    // Potentially uncompress.
    if (type == COMPRESSED_DATA) {
        size_t uncompressed_chunk_size;
        if (!snappy::GetUncompressedLength(&compressed_buf_[checksum_size_],
                                           size - checksum_size_,
                                           &uncompressed_chunk_size)) {
            ERRMSG("Failed getting snappy-compressed chunk length.\n");
            return false;
        }
        if (uncompressed_chunk_size > max_block_size_) {
            ERRMSG("Uncompressed chunk larger than maximum size. Want <= %zu, got %zu\n",
                   max_block_size_, uncompressed_chunk_size);
            return false;
        }

        if (!snappy::RawUncompress(&compressed_buf_[checksum_size_],
                                   size - checksum_size_, uncompressed_buf_.data())) {
            ERRMSG("Failed decompressing snappy chunk\n");
            return false;
        }
        src_.reset(new snappy::ByteArraySource(uncompressed_buf_.data(),
                                               uncompressed_chunk_size));
    }

    size_t dummy;
    uint32_t checksum = mask_crc32(crc32c(src_->Peek(&dummy), src_->Available()));
    if (checksum != read_checksum) {
        ERRMSG("Checksum failure on snappy block. Want %x, got %x\n", read_checksum,
               checksum);
        return false;
    }
    return true;
}

bool
snappy_reader_t::read_new_chunk()
{
    char buf[4];
    fstream_->read(buf, 4);
    if (!(*fstream_)) {
        return false;
    }
    chunk_type_t type = static_cast<chunk_type_t>(buf[0]);
    uint32_t size = 0;
    memcpy(&size, buf + 1, 3);

    switch (type) {
    case STREAM_IDENTIFIER: return read_magic(size); break;
    case COMPRESSED_DATA:
    case UNCOMPRESSED_DATA:
        if (!check_magic())
            return false;
        return read_data_chunk(size, type);
        break;
    case PADDING:
        if (!check_magic())
            return false;
        fstream_->seekg(size, fstream_->cur);
        return !!fstream_;
        break;
    default:
        if (!check_magic())
            return false;
        if (type >= SKIP_BEGIN && type <= SKIP_END) {
            fstream_->seekg(size, fstream_->cur);
            return !!fstream_;
        } else {
            ERRMSG("Unknown chunk type %d\n", type);
            return false;
        }
    }
}

int
snappy_reader_t::read(size_t size, OUT void *to)
{
    char *to_buf = (char *)to;
    size_t to_read = size;
    while (to_read > 0) {
        size_t available = 0;
        if (src_)
            available = src_->Available();

        size_t will_read = std::min(available, to_read);
        if (will_read > 0) {
            size_t src_len;
            memcpy(to_buf, src_->Peek(&src_len), will_read);
            src_->Skip(will_read);
            to_read -= will_read;
            to_buf += will_read;
            continue;
        }
        if (!read_new_chunk()) {
            return size - to_read;
        }
    }
    return size - to_read;
}

/* clang-format off */ /* (make vera++ newline-after-type check happy) */
template <>
/* clang-format on */
file_reader_t<snappy_reader_t>::~file_reader_t<snappy_reader_t>()
{
    // Empty.
}

template <>
bool
file_reader_t<snappy_reader_t>::open_single_file(const std::string &path)
{
    std::ifstream *file = new std::ifstream(path, std::ifstream::binary);
    if (!*file)
        return false;
    VPRINT(this, 1, "Opened snappy input file %s\n", path.c_str());
    input_files_.emplace_back(file);
    return true;
}

template <>
bool
file_reader_t<snappy_reader_t>::read_next_thread_entry(size_t thread_index,
                                                       OUT trace_entry_t *entry,
                                                       OUT bool *eof)
{
    int len = input_files_[thread_index].read(sizeof(*entry), entry);
    // Returns less than asked-for for end of file, or –1 for error.
    if (len < (int)sizeof(*entry)) {
        *eof = input_files_[thread_index].eof();
        return false;
    }
    VPRINT(this, 4, "Read from thread #%zd file: type=%d, size=%d, addr=%zu\n",
           thread_index, entry->type, entry->size, entry->addr);
    return true;
}

template <>
bool
file_reader_t<snappy_reader_t>::is_complete()
{
    // Not supported, similar to gzip reader.
    return false;
}
