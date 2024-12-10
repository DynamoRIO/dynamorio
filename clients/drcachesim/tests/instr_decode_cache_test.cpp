/* **********************************************************
 * Copyright (c) 2024 Google, LLC  All rights reserved.
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

/* Tests for the instr_decode_cache_t library. */

#include <iostream>
#include <vector>

#include "instr_decode_cache.h"
#include "../common/memref.h"
#include "memref_gen.h"

namespace dynamorio {
namespace drmemtrace {

class test_decode_info_t : public decode_info_base_t {
public:
    bool is_nop = false;
    bool is_ret = false;

private:
    void
    set_decode_info_derived(void *dcontext,
                            const dynamorio::drmemtrace::_memref_instr_t &memref_instr,
                            instr_t *instr) override
    {
        is_nop = instr_is_nop(instr);
        is_ret = instr_is_return(instr);
    }
};

std::string
check_decode_caching(bool persist_instrs)
{
    static constexpr addr_t BASE_ADDR = 0x123450;
    static constexpr addr_t TID_A = 1;
    instr_t *nop = XINST_CREATE_nop(GLOBAL_DCONTEXT);
    instr_t *ret = XINST_CREATE_return(GLOBAL_DCONTEXT);
    instrlist_t *ilist = instrlist_create(GLOBAL_DCONTEXT);
    instrlist_append(ilist, nop);
    instrlist_append(ilist, ret);
    std::vector<memref_with_IR_t> memref_setup = {
        { gen_instr(TID_A), nop },
        { gen_instr(TID_A), ret },
        { gen_instr(TID_A), nop },
    };
    auto memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
    // Set up the second nop memref to reuse the same encoding as the first nop.
    memrefs[2].instr.encoding_is_new = false;

    test_decode_info_t test_decode_info;
    if (test_decode_info.is_valid()) {
        return "Unexpected valid default-constructed decode info";
    }

    if (persist_instrs) {
        // These are tests to verify the operation of instr_decode_info_t: that it stores
        // the instr_t correctly.
        // Tests for instr_decode_cache_t are done when persist_instrs = false (see
        // the else part below).
        instr_decode_cache_t<instr_decode_info_t> decode_cache(
            GLOBAL_DCONTEXT,
            /*persist_decoded_instr=*/true);
        for (const memref_t &memref : memrefs) {
            decode_cache.add_decode_info(memref.instr);
        }
        instr_decode_info_t *decode_info_nop =
            decode_cache.get_decode_info(reinterpret_cast<app_pc>(memrefs[0].instr.addr));
        if (decode_info_nop == nullptr || !decode_info_nop->is_valid() ||
            !instr_is_nop(decode_info_nop->get_decoded_instr())) {
            return "Unexpected decode info for nop instr";
        }
        instr_decode_info_t *decode_info_ret =
            decode_cache.get_decode_info(reinterpret_cast<app_pc>(memrefs[1].instr.addr));
        if (decode_info_ret == nullptr || !decode_info_ret->is_valid() ||
            !instr_is_return(decode_info_ret->get_decoded_instr())) {
            return "Unexpected decode info for ret instr";
        }
    } else {
        // These are tests to verify the operation of instr_decode_cache_t: that it caches
        // decode info correctly.
        instr_decode_cache_t<test_decode_info_t> decode_cache(
            GLOBAL_DCONTEXT,
            /*persist_decoded_instrs=*/false);
        if (decode_cache.get_decode_info(
                reinterpret_cast<app_pc>(memrefs[0].instr.addr)) != nullptr) {
            return "Unexpected decode info for never-seen pc";
        }
        decode_cache.add_decode_info(memrefs[0].instr);
        test_decode_info_t *decode_info_nop =
            decode_cache.get_decode_info(reinterpret_cast<app_pc>(memrefs[0].instr.addr));
        if (decode_info_nop == nullptr || !decode_info_nop->is_valid() ||
            !decode_info_nop->is_nop) {
            return "Unexpected decode info for nop instr";
        }
        decode_cache.add_decode_info(memrefs[1].instr);
        test_decode_info_t *decode_info_ret =
            decode_cache.get_decode_info(reinterpret_cast<app_pc>(memrefs[1].instr.addr));
        if (decode_info_ret == nullptr || !decode_info_ret->is_valid() ||
            !decode_info_ret->is_ret) {
            return "Unexpected decode info for ret instr";
        }
        decode_cache.add_decode_info(memrefs[2].instr);
        test_decode_info_t *decode_info_nop_2 =
            decode_cache.get_decode_info(reinterpret_cast<app_pc>(memrefs[2].instr.addr));
        if (decode_info_nop_2 != decode_info_nop) {
            return "Did not see same decode info instance for second instance of nop";
        }
    }
    instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
    std::cerr << "check_decode_caching with persist_instrs: " << persist_instrs
              << " passed\n";
    return "";
}

int
test_main(int argc, const char *argv[])
{
    std::string err = check_decode_caching(/*persist_instrs=*/false);
    if (err != "") {
        std::cerr << err << "\n";
        exit(1);
    }
    err = check_decode_caching(/*persist_instrs=*/true);
    if (err != "") {
        std::cerr << err << "\n";
        exit(1);
    }
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
