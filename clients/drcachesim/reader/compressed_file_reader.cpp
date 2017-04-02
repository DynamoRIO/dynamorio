/* **********************************************************
 * Copyright (c) 2017 Google, Inc.  All rights reserved.
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

#include <assert.h>
#include <zlib.h>
#include "compressed_file_reader.h"
#include "../common/memref.h"
#include "../common/utils.h"

#ifdef VERBOSE
# include <iostream>
#endif

compressed_file_reader_t::compressed_file_reader_t() : file(NULL)
{
    /* Empty. */
}

compressed_file_reader_t::compressed_file_reader_t(const char *file_name)
{
    file = gzopen(file_name, "rb");
}

bool
compressed_file_reader_t::init()
{
    at_eof = false;
    if (file == NULL)
        return false;
    trace_entry_t *first_entry = read_next_entry();
    if (first_entry == NULL)
        return false;
    if (first_entry->type != TRACE_TYPE_HEADER ||
        first_entry->addr != TRACE_ENTRY_VERSION) {
        ERRMSG("missing header or version mismatch\n");
        return false;
    }
    ++*this;
    return true;
}

compressed_file_reader_t::~compressed_file_reader_t()
{
    if (file != NULL)
        gzclose(file);
}

trace_entry_t *
compressed_file_reader_t::read_next_entry()
{
    int len = gzread(file, (char*)&entry_copy, sizeof(entry_copy));
    // Returns less than asked-for for end of file, or –1 for error.
    if (len < (int)sizeof(entry_copy))
        return NULL;
    return &entry_copy;
}

bool
compressed_file_reader_t::is_complete()
{
    // The gzip reading interface does not support seeking to SEEK_END so there
    // is no efficient way to read the footer.
    // We could have the trace file writer seek back and set a bit at the start.
    // Or we can just not supported -indir with a gzipped file, which is what we
    // do for now: the user must pass in -infile for a gzipped file.
    return false;
}
