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

#ifndef _INSTR_DECODE_CACHE_H_
#define _INSTR_DECODE_CACHE_H_ 1

#include "dr_api.h"
#include "memref.h"

namespace dynamorio {
namespace drmemtrace {

/**
 * Base class for decode info. Users should derive from this class and implement
 * set_decode_info() to derive and store the decode info they need.
 *
 * Derived classes must provide a default constructor to make objects
 * corresponding to an invalid decoding.
 */
class decode_info_base_t {
public:
    /**
     * Sets the required decoding info fields based on the provided instr_t which
     * was allocated using the provided opaque dcontext_t for the provided
     * memref_instr.
     *
     * Returns whether the provided instr_t should be destroyed by the caller. If
     * the stored decode info includes the provided instr, it should be not be
     * destroyed by the caller, and the derived class should call instr_destroy on
     * it when done using it.
     */
    virtual bool
    set_decode_info(void *dcontext,
                    const dynamorio::drmemtrace::_memref_instr_t &memref_instr,
                    instr_t *instr) = 0;
};

/**
 * Decode info including the full decoded instr_t.
 */
class instr_decode_info_t : public decode_info_base_t {
public:
    ~instr_decode_info_t()
    {
        if (instr_ != nullptr) {
            instr_destroy(dcontext_, instr_);
        }
    }
    bool
    set_decode_info(void *dcontext,
                    const dynamorio::drmemtrace::_memref_instr_t &memref_instr,
                    instr_t *instr) override
    {
        dcontext_ = dcontext;
        instr_ = instr;
        return true;
    }

private:
    instr_t *instr_ = nullptr;
    void *dcontext_ = nullptr;
};

template <class DecodeInfo> class instr_decode_cache_t {
    static_assert(std::is_base_of<decode_info_base_t, DecodeInfo>::value,
                  "Derived not derived from decode_info_base_t");

public:
    instr_decode_cache_t(void *dcontext)
        : dcontext_(dcontext) { };

    /**
     * Returns a pointer to the DecodeInfo available for the instruction at pc. Returns
     * nullptr if no instruction is known at that pc. Returns the default-constructed
     * DecodeInfo if there was a decoding error for the instruction at the pc.
     */
    DecodeInfo *
    get_decode_info(app_pc pc)
    {
        auto it = decode_cache_.find(pc);
        if (it == decode_cache_.end()) {
            return nullptr;
        }
        return &it->second;
    }
    /**
     * Adds decode info for the given memref.instr if it is not yet recorded.
     */
    void
    add_decode_info(app_pc pc, const dynamorio::drmemtrace::_memref_instr_t &memref_instr)
    {
        const app_pc trace_pc = reinterpret_cast<app_pc>(memref_instr.addr);
        if (memref_instr.encoding_is_new) {
            decode_cache_.erase(trace_pc);
        }
        if (decode_cache_.find(trace_pc) != decode_cache_.end()) {
            return;
        }
        instr_t *instr = instr_create(dcontext_);
        app_pc next_pc = decode_from_copy(
            dcontext_, const_cast<app_pc>(memref_instr.encoding), trace_pc, instr);
        decode_cache_[trace_pc] = DecodeInfo();
        if (next_pc == nullptr || !instr_valid(instr)) {
            std::cerr << "AAA error in decoding\n";
            return;
        }
        bool destroy_instr =
            decode_cache_[trace_pc].set_decode_info(dcontext_, memref_instr, instr);
        if (destroy_instr) {
            instr_destroy(dcontext_, instr);
        }
    }

private:
    std::unordered_map<app_pc, DecodeInfo> decode_cache_;
    void *dcontext_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _INSTR_DECODE_CACHE_H_ */
