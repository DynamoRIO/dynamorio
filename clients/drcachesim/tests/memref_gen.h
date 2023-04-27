/* **********************************************************
 * Copyright (c) 2021-2023 Google, LLC  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, LLC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef _MEMREF_GEN_
#define _MEMREF_GEN_ 1

#include "../common/memref.h"
#include "dr_api.h"
#include <assert.h>
#include <cstring>

namespace {

constexpr addr_t BASE_ADDR = 0xeba4ad4;

struct memref_instr_t {
    memref_t memref;
    instr_t *instr;
};

inline memref_t
gen_data(memref_tid_t tid, bool load, addr_t addr, size_t size)
{
    memref_t memref = {};
    memref.instr.type = load ? TRACE_TYPE_READ : TRACE_TYPE_WRITE;
    memref.instr.tid = tid;
    memref.instr.addr = addr;
    memref.instr.size = size;
    return memref;
}

inline memref_t
gen_instr_type(trace_type_t type, memref_tid_t tid, addr_t pc, size_t size = 1)
{
    memref_t memref = {};
    memref.instr.type = type;
    memref.instr.tid = tid;
    memref.instr.addr = pc;
    memref.instr.size = size;
    return memref;
}

inline memref_t
gen_instr(memref_tid_t tid, addr_t pc = 1, size_t size = 1,
          trace_type_t type = TRACE_TYPE_INSTR)
{
    return gen_instr_type(type, tid, pc, size);
}

inline memref_t
gen_branch(memref_tid_t tid, addr_t pc = 1)
{
    return gen_instr_type(TRACE_TYPE_INSTR_CONDITIONAL_JUMP, tid, pc);
}

// We use these client defines which are the target and so drdecode's target arch.
#if defined(ARM_64) || defined(ARM_32)
// Variant for aarchxx encodings.
inline memref_t
gen_instr_encoded(addr_t pc, int encoding, memref_tid_t tid = 1)
{
    memref_t memref = gen_instr_type(TRACE_TYPE_INSTR, tid, pc, 4);
    memcpy(memref.instr.encoding, &encoding, sizeof(encoding));
    memref.instr.encoding_is_new = true;
    return memref;
}

inline memref_t
gen_branch_encoded(memref_tid_t tid, addr_t pc, int encoding)
{
    memref_t memref = gen_instr_type(TRACE_TYPE_INSTR_CONDITIONAL_JUMP, tid, pc);
    memref.instr.size = 4;
    memcpy(memref.instr.encoding, &encoding, sizeof(encoding));
    memref.instr.encoding_is_new = true;
    return memref;
}

#elif defined(X86_64) || defined(X86_32)
inline memref_t
gen_instr_encoded(addr_t pc, const std::vector<char> &encoding, memref_tid_t tid = 1)
{
    memref_t memref = gen_instr_type(TRACE_TYPE_INSTR, tid, pc, encoding.size());
    memcpy(memref.instr.encoding, encoding.data(), encoding.size());
    memref.instr.encoding_is_new = true;
    return memref;
}

inline memref_t
gen_instr_encoded_with_ir(void *drcontext, instr_t *instr, addr_t addr,
                          trace_type_t type = TRACE_TYPE_INSTR, memref_tid_t tid = 1)
{
    byte *pc;
    byte buf[MAX_ENCODING_LENGTH];
    pc = instr_encode(drcontext, instr, buf);
    memref_t memref = {};
    memref.instr.type = type;
    memref.instr.tid = tid;
    memref.instr.addr = addr;
    memref.instr.size = instr_length(GLOBAL_DCONTEXT, instr);
    memcpy(memref.instr.encoding, buf, sizeof(buf));
    memref.instr.encoding_is_new = true;
    return memref;
}

// Variant for x86 encodings.
inline memref_t
gen_branch_encoded(memref_tid_t tid, addr_t pc, const std::vector<char> &encoding)
{
    memref_t memref = gen_instr_type(TRACE_TYPE_INSTR_CONDITIONAL_JUMP, tid, pc);
    memref.instr.size = encoding.size();
    memcpy(memref.instr.encoding, encoding.data(), encoding.size());
    memref.instr.encoding_is_new = true;
    return memref;
}
#else
#endif

inline memref_t
gen_marker(memref_tid_t tid, trace_marker_type_t type, uintptr_t val)
{
    memref_t memref = {};
    memref.marker.type = TRACE_TYPE_MARKER;
    memref.marker.tid = tid;
    memref.marker.marker_type = type;
    memref.marker.marker_value = val;
    return memref;
}

inline memref_t
gen_exit(memref_tid_t tid)
{
    memref_t memref = {};
    memref.instr.type = TRACE_TYPE_THREAD_EXIT;
    memref.instr.tid = tid;
    return memref;
}

inline std::vector<memref_t>
get_memrefs_from_ir(instrlist_t *ilist, std::vector<memref_instr_t> &memref_instr_vec,
                    const addr_t base_addr = BASE_ADDR)
{
    static const int MAX_DECODE_SIZE = 1024;
    byte decode_buf[MAX_DECODE_SIZE];
    byte *pc =
        instrlist_encode_to_copy(GLOBAL_DCONTEXT, ilist, decode_buf,
                                 reinterpret_cast<app_pc>(base_addr), nullptr, true);
    assert(pc != nullptr);
    std::vector<memref_t> memrefs = {};
    memrefs.push_back(
        gen_marker(1, TRACE_MARKER_TYPE_FILETYPE, OFFLINE_FILE_TYPE_ENCODINGS));
    for (auto pair : memref_instr_vec) {
        if (pair.instr != nullptr && type_is_instr(pair.memref.instr.type)) {
            pair.memref.instr.addr = instr_get_offset(pair.instr) + base_addr;
            pair.memref.instr.size = instr_length(GLOBAL_DCONTEXT, pair.instr);
            byte buf[MAX_ENCODING_LENGTH];
            byte *next_pc = instr_encode(GLOBAL_DCONTEXT, pair.instr, buf);
            assert(next_pc != nullptr);
            memcpy(pair.memref.instr.encoding, buf, sizeof(buf));
            pair.memref.instr.encoding_is_new = true;
        }
        memrefs.push_back(pair.memref);
    }
    return memrefs;
}

} // namespace

#endif /* _MEMREF_GEN_ */
