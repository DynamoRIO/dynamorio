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

/**
 * instr_decode_cache.h: Library that supports caching of instruction decode
 * information.
 */

#ifndef _INSTR_DECODE_CACHE_H_
#define _INSTR_DECODE_CACHE_H_ 1

#include "dr_api.h"
#include "memref.h"

#include <unordered_map>

namespace dynamorio {
namespace drmemtrace {

/**
 * Base class for storing instruction decode info. Users should sub-class this
 * base class and implement set_decode_info() to derive and store the decode info
 * they need.
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
     * This is meant for use with \p instr_decode_cache_t, which will invoke
     * set_decode_info() for each new decoded instruction.
     *
     * The responsibility for invoking instr_destroy() on the provided \p instr
     * lies with this \p decode_info_base_t object, unless
     * \p instr_decode_cache_t was constructed with \p persist_decoded_instrs_
     * set to false, in which case no heap allocation takes place.
     */
    virtual void
    set_decode_info(void *dcontext,
                    const dynamorio::drmemtrace::_memref_instr_t &memref_instr,
                    instr_t *instr) = 0;
};

/**
 * Decode info including the full decoded instr_t. This should be used with a
 * \p instr_decode_cache_t constructed with \p persist_decoded_instrs_ set to
 * true.
 */
class instr_decode_info_t : public decode_info_base_t {
public:
    ~instr_decode_info_t()
    {
        if (instr_ != nullptr) {
            instr_destroy(dcontext_, instr_);
        }
    }
    void
    set_decode_info(void *dcontext,
                    const dynamorio::drmemtrace::_memref_instr_t &memref_instr,
                    instr_t *instr) override
    {
        dcontext_ = dcontext;
        instr_ = instr;
    }

    instr_t *instr_ = nullptr;

private:
    void *dcontext_ = nullptr;
};

/**
 * A cache to store decode info for instructions per observed app pc. The template arg
 * DecodeInfo is a class derived from \p decode_info_base_t which implements the
 * set_decode_info() function that derives the required decode info from an \p instr_t
 * object provided by \p instr_decode_cache_t. This class handles the heavy-lifting of
 * actually producing the decoded \p instr_t. The decoded \p instr_t may be made to
 * persist beyond the set_decode_info() calls by constructing the
 * \p instr_decode_cache_t objects with \p persist_decoded_instrs_ set to true.
 */
template <class DecodeInfo> class instr_decode_cache_t {
    static_assert(std::is_base_of<decode_info_base_t, DecodeInfo>::value,
                  "DecodeInfo not derived from decode_info_base_t");

public:
    instr_decode_cache_t(void *dcontext, bool persist_decoded_instrs)
        : dcontext_(dcontext)
        , persist_decoded_instrs_(persist_decoded_instrs) {};

    /**
     * Returns a pointer to the DecodeInfo available for the instruction at \p pc.
     * Returns nullptr if no instruction is known at that \t pc. Returns the
     * default-constructed DecodeInfo if there was a decoding error for the
     * instruction.
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
     * Adds decode info for the given \p memref_instr if it is not yet recorded.
     */
    void
    add_decode_info(const dynamorio::drmemtrace::_memref_instr_t &memref_instr)
    {
        const app_pc trace_pc = reinterpret_cast<app_pc>(memref_instr.addr);
        if (memref_instr.encoding_is_new) {
            decode_cache_.erase(trace_pc);
        }
        if (decode_cache_.find(trace_pc) != decode_cache_.end()) {
            return;
        }
        decode_cache_[trace_pc] = DecodeInfo();
        instr_t *instr = nullptr;
        instr_noalloc_t noalloc;
        if (persist_decoded_instrs_) {
            instr = instr_create(dcontext_);
        } else {
            instr_noalloc_init(dcontext_, &noalloc);
            instr = instr_from_noalloc(&noalloc);
        }
        app_pc next_pc = decode_from_copy(
            dcontext_, const_cast<app_pc>(memref_instr.encoding), trace_pc, instr);
        if (next_pc == nullptr || !instr_valid(instr)) {
            if (persist_decoded_instrs_) {
                instr_destroy(dcontext_, instr);
            }
            return;
        }
        decode_cache_[trace_pc].set_decode_info(dcontext_, memref_instr, instr);
    }

private:
    std::unordered_map<app_pc, DecodeInfo> decode_cache_;
    void *dcontext_;
    bool persist_decoded_instrs_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _INSTR_DECODE_CACHE_H_ */
