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

/* ir2trace: convert DynamoRIO's IR format to trace entries. */

#ifndef _IR2TRACE_H_
#define _IR2TRACE_H_ 1

/**
 * @file ir2trace.h
 * @brief Offline DynamoRIO's IR converter. Converts DynamoRIO's IR format to trace
 * entries.
 */

#include <vector>
#include <mutex>
#include "drir.h"
#include "../common/trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

#ifndef IN
#    define IN // nothing
#endif
#ifndef OUT
#    define OUT // nothing
#endif
#ifndef INOUT
#    define INOUT // nothing
#endif

/**
 * The type of ir2trace_t::convert() return value.
 */
enum ir2trace_convert_status_t {
    /** The conversion process succeeded. */
    IR2TRACE_CONV_SUCCESS = 0,
    /** The conversion process failed: invalid parameter. */
    IR2TRACE_CONV_ERROR_INVALID_PARAMETER
};

/**
 * ir2trace_t is a class that can convert DynamoRIO's IR format to trace entries.
 */
class ir2trace_t {
public:
    ir2trace_t()
    {
    }
    ~ir2trace_t()
    {
    }

    /**
     * Converts DynamoRIO's IR format to trace entries.
     * @param drir DynamoRIO's IR format.
     * @param trace The converted trace entries.
     * @param verbosity  The verbosity level for notifications. If set to 0, only error
     * logs are printed. If set to 1, all logs are printed. Default value is 0.
     * @return ir2trace_convert_status_t If the conversion is successful, the function
     * returns #IR2TRACE_CONV_SUCCESS. Otherwise, the function returns the corresponding
     * error code.
     */
    static ir2trace_convert_status_t
    convert(IN drir_t &drir, INOUT std::vector<trace_entry_t> &trace,
            IN int verbosity = 0);
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _IR2TRACE_H_ */
