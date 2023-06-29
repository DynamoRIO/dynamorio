/* **********************************************************
 * Copyright (c) 2017-2023 Google, Inc.  All rights reserved.
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

/* zipfile_file_reader: reads zipfile files containing memory traces. */

#ifndef _ZIPFILE_FILE_READER_H_
#define _ZIPFILE_FILE_READER_H_ 1

#include <zlib.h>
#include "minizip/unzip.h"
#include "file_reader.h"

namespace dynamorio {
namespace drmemtrace {

struct zipfile_reader_t {
    zipfile_reader_t()
        : file(nullptr)
    {
    }
    explicit zipfile_reader_t(unzFile file)
        : file(file)
    {
    }
    unzFile file;
    // Without our own buffering, reading one trace_entry_t record at a time
    // is 60% slower.  This buffer size was picked through experimentation to
    // perform well without taking up too much memory.
    trace_entry_t buf[4096];
    trace_entry_t *cur_buf = buf;
    trace_entry_t *max_buf = buf;
};

typedef file_reader_t<zipfile_reader_t> zipfile_file_reader_t;

/* Declare this so the compiler knows not to use the default implementation in the
 * class declaration.
 */
template <>
reader_t &
file_reader_t<zipfile_reader_t>::skip_instructions(uint64_t instruction_count);

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _ZIPFILE_FILE_READER_H_ */
