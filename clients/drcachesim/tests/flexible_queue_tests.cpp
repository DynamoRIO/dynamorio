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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Unit tests for flexible_queue. */

#include "scheduler/flexible_queue.h"

#include <assert.h>

#include <iostream>

namespace dynamorio {
namespace drmemtrace {
namespace {

bool
test_basics()
{
    // Create a min-heap queue.
    flexible_queue_t<int, std::greater<int>> q(/*verbose=*/1);
    assert(!q.find(4));
    q.push(4);
    assert(q.find(4));
    assert(q.top() == 4);
    q.push(3);
    assert(q.top() == 3);
    q.push(5);
    assert(q.top() == 3);
    q.pop();
    assert(q.top() == 4);
    assert(!q.find(3));
    q.push(6);
    assert(q.find(5));
    q.erase(5);
    assert(!q.find(5));
    assert(q.top() == 4);
    q.pop();
    assert(!q.find(4));
    assert(q.top() == 6);
    return true;
}

} // namespace

int
test_main(int argc, const char *argv[])
{
    if (!test_basics())
        return 1;
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
