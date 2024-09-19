/* **********************************************************
 * Copyright (c) 2020 Google, Inc.  All rights reserved.
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

/* Unit tests for class reg_id_set_t, which is used for trace optimizations.
 * Linked into the burst_traceopts executable which covers trace optimizations
 * (fewer executables reduces the limited-resource CI time).
 */

#include <assert.h>
#include "tracer/instru.h"

namespace dynamorio {
namespace drmemtrace {

void
reg_id_set_unit_tests()
{
    reg_id_set_t set;
    assert(set.begin() == set.end());
    // Test non-GPR.
    auto insert_res = set.insert(IF_X86_ELSE(DR_REG_XMM0, DR_REG_Q0));
    assert(insert_res.first == set.end());
    assert(!insert_res.second);
    assert(set.begin() == set.end());
    // Test adding a GPR.
    insert_res = set.insert(DR_REG_START_GPR + 1);
    assert(insert_res.first != set.end());
    assert(insert_res.second);
    assert(set.begin() != set.end());
    // Test find not-present.
    auto find_res = set.find(DR_REG_START_GPR);
    assert(find_res == set.end());
    // Test find present.
    find_res = set.find(DR_REG_START_GPR + 1);
    assert(find_res != set.end());
    assert(*find_res == DR_REG_START_GPR + 1);
    // Test erase with two entries.
    insert_res = set.insert(DR_REG_START_GPR + 4);
    assert(insert_res.second);
    auto iter = set.begin();
    bool found_next = false;
    while (iter != set.end()) {
        if (*iter == DR_REG_START_GPR + 1)
            iter = set.erase(iter);
        else {
            if (*iter == DR_REG_START_GPR + 4)
                found_next = true;
            ++iter;
        }
    }
    assert(found_next);
    assert(set.find(DR_REG_START_GPR + 1) == set.end());
    assert(set.find(DR_REG_START_GPR + 4) != set.end());
    assert(*set.find(DR_REG_START_GPR + 4) == DR_REG_START_GPR + 4);
    // Test adding duplicates.
    insert_res = set.insert(DR_REG_START_GPR + 3);
    assert(insert_res.second);
    insert_res = set.insert(DR_REG_START_GPR + 3);
    assert(!insert_res.second);
    iter = set.erase(insert_res.first);
    find_res = set.find(DR_REG_START_GPR + 3);
    assert(find_res == set.end());
}

} // namespace drmemtrace
} // namespace dynamorio
