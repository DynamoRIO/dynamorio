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

#ifndef DR_FAST_IR
#    define DR_FAST_IR 1
#endif
#include "dr_api.h"
#include "utils.h"

#include <cstring>
#include <memory>
#include <unordered_map>

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

    // Appends the given instr to the internal ilist, and records (replaces if
    // one already exists) the given encoding for the orig_pc.
    void
    append(instr_t *instr, app_pc orig_pc, int instr_length, uint8_t *encoding)
    {
        ASSERT(drcontext_ != nullptr, "drir_t: invalid drcontext_");
        ASSERT(ilist_ != nullptr, "drir_t: invalid ilist_");
        if (instr == nullptr) {
            ASSERT(false, "drir_t: invalid instr");
            return;
        }
        instrlist_append(ilist_, instr);
        record_encoding(orig_pc, instr_length, encoding);
    }

    // Returns the opaque pointer to the dcontext_t used to construct this
    // object.
    void *
    get_drcontext()
    {
        return drcontext_;
    }

    // Returns the instrlist_t of instrs accumulated so far.
    instrlist_t *
    get_ilist()
    {
        return ilist_;
    }

    // Clears the instrs accumulated in the ilist. Note that this does
    // not clear the encodings accumulated.
    void
    clear_ilist()
    {
        instrlist_clear(drcontext_, ilist_);
    }

    // Returns the address of the encoding recorded for the given orig_pc.
    // Encodings are persisted across clear_ilist() calls, so we will
    // return the same decode_pc for the same orig_pc unless a new encoding
    // is added for the same orig_pc.
    app_pc
    get_decode_pc(app_pc orig_pc)
    {
        if (decode_pc_.find(orig_pc) == decode_pc_.end()) {
            return nullptr;
        }
        return decode_pc_[orig_pc].first;
    }

private:
    void *drcontext_;
    instrlist_t *ilist_;
#define SYSCALL_PT_ENCODING_BUF_SIZE (1024 * 1024)
    // For each original app pc key, this stores a pair value: the first
    // element is the address where the encoding is stored for the instruction
    // at that app pc, the second element is the length of the encoding.
    std::unordered_map<app_pc, std::pair<app_pc, int>> decode_pc_;
    // A vector of buffers of size SYSCALL_PT_ENCODING_BUF_SIZE. Each buffer
    // stores some encoded instructions back-to-back. Note that each element
    // in the buffer is a single byte, so one instr's encoding occupies possibly
    // multiple consecutive elements.
    // We allocate new memory to store kernel instruction encodings in
    // increments of SYSCALL_PT_ENCODING_BUF_SIZE. We do not treat this like a
    // cache and clear previously stored encodings because we want to ensure
    // decode_pc uniqueness to callers of get_decode_pc.
    std::vector<std::unique_ptr<uint8_t[]>> instr_encodings_;
    // Next available offset into instr_encodings_.back().
    size_t next_encoding_offset_ = 0;

    void
    record_encoding(app_pc orig_pc, int instr_len, uint8_t *encoding)
    {
        auto it = decode_pc_.find(orig_pc);
        // We record the encoding only if we don't already have the same encoding for
        // the given orig_pc.
        if (it != decode_pc_.end() &&
            // We confirm that the instruction encoding has not changed. Just in case
            // the kernel is doing JIT.
            it->second.second == instr_len &&
            memcmp(it->second.first, encoding, it->second.second) == 0) {
            return;
        }
        if (instr_encodings_.empty() ||
            next_encoding_offset_ + instr_len >= SYSCALL_PT_ENCODING_BUF_SIZE) {
            instr_encodings_.emplace_back(new uint8_t[SYSCALL_PT_ENCODING_BUF_SIZE]);
            next_encoding_offset_ = 0;
        }
        app_pc encode_pc = &instr_encodings_.back()[next_encoding_offset_];
        memcpy(encode_pc, encoding, instr_len);
        decode_pc_[orig_pc] = std::make_pair(encode_pc, instr_len);
        next_encoding_offset_ += instr_len;
    }
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _DRIR_H_ */
