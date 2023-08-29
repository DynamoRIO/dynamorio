/* **********************************************************
 * Copyright (c) 2023 Google, Inc.  All rights reserved.
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

#include "speculator.h"

#include <string.h>

#include <string>

#include "memref.h"
#include "trace_entry.h"
#include "utils.h"

#undef VPRINT
#ifdef DEBUG
#    define VPRINT(obj, level, ...)                            \
        do {                                                   \
            if ((obj)->verbosity_ >= (level)) {                \
                fprintf(stderr, "%s ", (obj)->output_prefix_); \
                fprintf(stderr, __VA_ARGS__);                  \
            }                                                  \
        } while (0)
#else
#    define VPRINT(reader, level, ...) /* Nothing. */
#endif

namespace dynamorio {
namespace drmemtrace {

template <typename RecordType>
std::string
speculator_tmpl_t<RecordType>::next_record(addr_t &pc, RecordType &record)
{
    return "Not implemented";
}

template <>
std::string
speculator_tmpl_t<memref_t>::next_record(addr_t &pc, memref_t &memref)
{
    if (TESTANY(LAST_FROM_TRACE | AVERAGE_FROM_TRACE, flags_)) {
        // TODO i#5843: Add prior-seen-in-trace support by having the scheduler
        // pass us every record so we can track prior instructions.  Instead of
        // using the same data address as the most recent instance of a pc,
        // we should use some weighted average across the last N instances.
        return "Not implemented";
    } else if (TESTANY(FROM_BINARY, flags_)) {
        // TODO i#5843: Add support for grabbing never-seen instructions from the
        // binary, if available.  We'll need module map info passed to us.
    } else if (!TESTANY(USE_NOPS, flags_)) {
        return "Invalid flags";
    }

    // Supply nops.
    // Since this is just one encoding, we hardcoded it.
    // If we add more we'll want to pull in DR's encoder and use its IR.
    // XXX i#5843: Once we add more complex schemes, we'll need to either save
    // the last record for a given PC or have the scheduler do it, to ensure
    // resuming a nested speculation layer where the user asked to see the same
    // instruction again provides the right data.
    memref.instr.type = TRACE_TYPE_INSTR;
    memref.instr.addr = pc;

    int encoding;
    int len;
#if defined(X86_64) || defined(X86_32)
    static constexpr int NOP_ENCODING = 0x90;
    encoding = NOP_ENCODING;
    len = 1;
#elif defined(ARM_64)
    static constexpr int NOP_ENCODING = 0xd503201f;
    encoding = NOP_ENCODING;
    len = 4;
#elif defined(ARM_32)
    static constexpr int NOP_ENCODING_ARM = 0xe320f000;
    static constexpr int NOP_ENCODING_THUMB = 0xbf00;
    static constexpr int LEN_ARM = 4;
    static constexpr int LEN_THUMB = 2;
    // Trace PC values have LSB=1 for Thumb mode.
    if (TESTANY(1, pc)) {
        encoding = NOP_ENCODING_THUMB;
        len = LEN_THUMB;
    } else {
        encoding = NOP_ENCODING_ARM;
        len = LEN_ARM;
    }
#else
    return "Not implemented";
#endif
    memref.instr.size = len;
    memcpy(memref.instr.encoding, &encoding, len);
    // We do not try to figure out whether we've emitted this same PC before.
    memref.instr.encoding_is_new = true;

    pc += len;

    return "";
}

template class speculator_tmpl_t<memref_t>;
template class speculator_tmpl_t<trace_entry_t>;

} // namespace drmemtrace
} // namespace dynamorio
