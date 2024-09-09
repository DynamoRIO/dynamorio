/* **********************************************************
 * Copyright (c) 2024 Google, Inc.  All rights reserved.
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

/* mutex_dbg_owned.h: std::mutex plus an assertable owner and stats in debug builds. */

#ifndef _MUTEX_DBG_OWNED_H_
#define _MUTEX_DBG_OWNED_H_ 1

#include <mutex>
#include <thread>

namespace dynamorio {
namespace drmemtrace {

// A wrapper around std::mutex which adds an owner field for asserts on ownership
// when a lock is required to be held by the caller (where
// std::unique_lock::owns_lock() cannot easily be used).
// It also adds contention statistics.  These additional fields are only maintained
// when NDEBUG is not defined: i.e., they are targeted for asserts and diagnostics.
class mutex_dbg_owned {
public:
    void
    lock();
    bool
    try_lock();
    void
    unlock();
    // This query should only be called when the lock is required to be held
    // as it is racy when the lock is not held.
    bool
    owned_by_cur_thread();
    // These statistics only count lock(): they do *not* count try_lock()
    // (we could count try_lock with std::atomic on count_contended).
    int64_t
    get_count_acquired();
    int64_t
    get_count_contended();

private:
    std::mutex lock_;
    // We do not place these under ifndef NDEBUG as it is too easy to get two
    // compilation units with different values for NDEBUG conflicting.
    // We thus pay the space cost in NDEBUG build.
    std::thread::id owner_;
    int64_t count_acquired_ = 0;
    int64_t count_contended_ = 0;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _MUTEX_DBG_OWNED_H_ */
