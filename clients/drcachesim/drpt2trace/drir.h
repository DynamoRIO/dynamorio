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

/* drir: the type wraps instrlist_t. */

#ifndef _DRIR_H_
#define _DRIR_H_ 1

#define DR_FAST_IR 1
#include "dr_api.h"
#include "../common/utils.h"

namespace dynamorio {
namespace drmemtrace {

class drir_t {
public:
    drir_t(void *drcontext)
        : drcontext_(drcontext)
    {
        ASSERT(drcontext_ != nullptr, "drir_t: invalid drcontext_");
        ilist_ = instrlist_create(drcontext_);
    }

    ~drir_t()
    {
        ASSERT(drcontext_ != nullptr, "drir_t: invalid drcontext_");
        if (ilist_ != nullptr) {
            instrlist_clear_and_destroy(drcontext_, ilist_);
            ilist_ = nullptr;
        }
    }

    void
    append(instr_t *instr)
    {
        ASSERT(drcontext_ != nullptr, "drir_t: invalid drcontext_");
        ASSERT(ilist_ != nullptr, "drir_t: invalid ilist_");
        if (instr == nullptr) {
            ASSERT(false, "drir_t: invalid instr");
            return;
        }
        instrlist_append(ilist_, instr);
    }

    void *
    get_drcontext()
    {
        return drcontext_;
    }

    instrlist_t *
    get_ilist()
    {
        return ilist_;
    }

private:
    void *drcontext_;
    instrlist_t *ilist_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _DRIR_H_ */
