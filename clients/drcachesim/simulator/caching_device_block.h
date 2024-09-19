/* **********************************************************
 * Copyright (c) 2015-2023 Google, Inc.  All rights reserved.
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

/* caching_device_block: represents a unit block of a caching device.
 */

#ifndef _CACHING_DEVICE_BLOCK_H_
#define _CACHING_DEVICE_BLOCK_H_ 1

#include <stdint.h>
#include "memref.h"

namespace dynamorio {
namespace drmemtrace {

// Assuming a block of a caching device represents a memory space of at least 4-byte,
// e.g., a CPU cache line or a virtual/physical page, we can use special value
// that cannot be computed from valid address as special tag for
// block status.
static const addr_t TAG_INVALID = (addr_t)-1; // block is invalid

class caching_device_block_t {
public:
    // Initializing counter to 0 is just to be safe and to make it easier to write new
    // replacement algorithms without errors (and we expect negligible perf cost).
    caching_device_block_t()
        : tag_(TAG_INVALID)
        , counter_(0)
    {
    }
    // Destructor must be virtual and default is not.
    virtual ~caching_device_block_t()
    {
    }

    addr_t tag_;

    // XXX: using int64_t here results in a ~4% slowdown for 32-bit apps.
    // A 32-bit counter should be sufficient but we may want to revisit.
    // We already have stdint.h so we can reinstate int64_t easily.
    int counter_; // for use by replacement policies
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _CACHING_DEVICE_BLOCK_H_ */
