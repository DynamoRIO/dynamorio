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

#include <mutex>
#include <thread>
#include "mutex_dbg_owned.h"

namespace dynamorio {
namespace drmemtrace {

void
mutex_dbg_owned::lock()
{
#ifdef NDEBUG
    lock_.lock();
#else
    bool contended = true;
    if (lock_.try_lock())
        contended = false;
    else
        lock_.lock();
    owner_ = std::this_thread::get_id();
    ++count_acquired_;
    if (contended)
        ++count_contended_;
#endif
}

bool
mutex_dbg_owned::try_lock()
{
#ifdef NDEBUG
    return lock_.try_lock();
#else
    if (lock_.try_lock()) {
        owner_ = std::this_thread::get_id();
        return true;
    }
    return false;
#endif
}

void
mutex_dbg_owned::unlock()
{
#ifndef NDEBUG
    owner_ = std::thread::id(); // id() creates a no-thread sentinel value.
#endif
    lock_.unlock();
}

// This query should only be called when the lock is required to be held
// as it is racy when the lock is not held.
bool
mutex_dbg_owned::owned_by_cur_thread()
{
    return owner_ == std::this_thread::get_id();
}

// These statistics only count lock(): they do *not* count try_lock()
// (we could count try_lock with std::atomic on count_contended).
int64_t
mutex_dbg_owned::get_count_acquired()
{
    return count_acquired_;
}

int64_t
mutex_dbg_owned::get_count_contended()
{
    return count_contended_;
}

} // namespace drmemtrace
} // namespace dynamorio
