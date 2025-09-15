/* **********************************************************
 * Copyright (c) 2016-2025 Google, Inc.  All rights reserved.
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

#include "reader_base.h"

#include <queue>

#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

reader_base_t::reader_base_t(int online, int verbosity, const char *output_prefix)
    : verbosity_(verbosity)
    , output_prefix_(output_prefix)
    , online_(online)
{
}

bool
reader_base_t::is_online()
{
    return online_;
}

trace_entry_t *
reader_base_t::get_next_entry()
{
    if (!queue_.empty()) {
        entry_copy_ = queue_.front();
        queue_.pop();
        return &entry_copy_;
    }
    return read_next_entry();
}

// To avoid double-dispatch (requires listing all derived types in the base here)
// and RTTI in trying to get the right operators called for subclasses, we
// instead directly check at_eof here.  If we end up needing to run code
// and a bool field is not enough we can change this to invoke a virtual
// method is_at_eof().
bool
reader_base_t::operator==(const reader_base_t &rhs) const
{
    return BOOLS_MATCH(at_eof_, rhs.at_eof_);
}

bool
reader_base_t::operator!=(const reader_base_t &rhs) const
{
    return !BOOLS_MATCH(at_eof_, rhs.at_eof_);
}

} // namespace drmemtrace
} // namespace dynamorio
