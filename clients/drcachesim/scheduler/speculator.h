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

/* Speculative execution path instruction and data access generation. */

#ifndef _DRMEMTRACE_SPECULATOR_H_
#define _DRMEMTRACE_SPECULATOR_H_ 1

/**
 * @file drmemtrace/speculator.h
 * @brief DrMemtrace trace speculative path generation.
 */

#include <string>

#include "memref.h"
#include "trace_entry.h"
#include "utils.h"

namespace dynamorio {
namespace drmemtrace {

/**
 * Provides instruction fetch and data access trace record generation for
 * speculative paths that were not actually traced.  Supports a variety of
 * methods for obtaining speculative content.
 */
template <typename RecordType> class speculator_tmpl_t {
public:
    /** Options controlling which speculation strategy to use. */
    enum speculator_flags_t {
        /**
         * Specifies that speculation should supply just NOP instructions.
         */
        USE_NOPS = 0x01,
        /**
         * Specifies that speculation should supply the last-seen instruction
         * and its data address.
         */
        LAST_FROM_TRACE = 0x02,
        /**
         * Specifies that speculation should supply an average (weighted, perhaps)
         * of the last N observed instructions at the given PC.
         */
        AVERAGE_FROM_TRACE = 0x04,
        /**
         * Specifies that speculation should obtain the instruction from the binary.
         * The address source is unspecified.
         */
        FROM_BINARY = 0x08,
    };

    /** Default constructor. */
    speculator_tmpl_t(speculator_flags_t flags, int verbosity = 0)
        : flags_(flags)
        , verbosity_(verbosity)
    {
    }
    virtual ~speculator_tmpl_t() = default;

    // Returns a record for the instruction at "pc" and updates "pc" to the
    // next fetch address.
    virtual std::string
    next_record(addr_t &pc, RecordType &record);

protected:
    speculator_flags_t flags_ = speculator_flags_t::SPECULATE_NOPS;
    int verbosity_ = 0;
};

/** See #dynamorio::drmemtrace::speculator_tmpl_t. */
typedef speculator_tmpl_t<memref_t> speculator_t;

/** See #dynamorio::drmemtrace::speculator_tmpl_t. */
typedef speculator_tmpl_t<trace_entry_t> record_speculator_t;

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _DRMEMTRACE_SPECULATOR_H_ */
