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

/* Tests for the decode_cache_t library. */

#include <iostream>
#include <vector>

#include "decode_cache.h"
#include "../common/memref.h"
#include "memref_gen.h"

namespace dynamorio {
namespace drmemtrace {

static constexpr addr_t TID_A = 1;
static constexpr offline_file_type_t ENCODING_FILE_TYPE =
    static_cast<offline_file_type_t>(OFFLINE_FILE_VERSION_ENCODINGS);

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
check_decode_caching(void *drcontext, bool persist_instrs, bool use_module_mapper)
{
    static constexpr addr_t BASE_ADDR = 0x123450;
    instr_t *nop = XINST_CREATE_nop(drcontext);
    instr_t *ret = XINST_CREATE_return(drcontext);
    instrlist_t *ilist = instrlist_create(drcontext);
    instrlist_append(ilist, nop);
    instrlist_append(ilist, ret);
    std::vector<memref_with_IR_t> memref_setup = {
        { gen_instr(TID_A), nop },
        { gen_instr(TID_A), ret },
        { gen_instr(TID_A), nop },
    };
    std::vector<memref_t> memrefs;
    instrlist_t *ilist_for_test_decode_cache = nullptr;
    std::string module_file_for_test_decode_cache = "";
    if (use_module_mapper) {
        // This does not set encodings in the memref.instr.
        memrefs = add_encodings_to_memrefs(ilist, memref_setup, 0,
                                           /*set_only_instr_addr=*/true);
        // We pass the instrs to construct the test_module_mapper_t in the
        // test_decode_cache_t;
        ilist_for_test_decode_cache = ilist;
        module_file_for_test_decode_cache = "some_mod_file";
    } else {
        memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
    }
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
        test_decode_cache_t<instr_decode_info_t> decode_cache(
            drcontext,
            /*persist_decoded_instr=*/true, ilist_for_test_decode_cache);
        decode_cache.init(ENCODING_FILE_TYPE, module_file_for_test_decode_cache, "");
        for (const memref_t &memref : memrefs) {
            std::string err = decode_cache.add_decode_info(memref.instr);
            if (err != "")
                return err;
        }
        instr_decode_info_t *decode_info_nop =
            decode_cache.get_decode_info(reinterpret_cast<app_pc>(memrefs[0].instr.addr));
        if (decode_info_nop == nullptr || !decode_info_nop->is_valid() ||
            !instr_is_nop(decode_info_nop->get_decoded_instr())) {
            return "Unexpected instr_decode_info_t for nop instr";
        }
        instr_decode_info_t *decode_info_ret =
            decode_cache.get_decode_info(reinterpret_cast<app_pc>(memrefs[1].instr.addr));
        if (decode_info_ret == nullptr || !decode_info_ret->is_valid() ||
            !instr_is_return(decode_info_ret->get_decoded_instr())) {
            return "Unexpected instr_decode_info_t for ret instr";
        }
    } else {
        // These are tests to verify the operation of instr_decode_cache_t: that it caches
        // decode info correctly.
        test_decode_cache_t<test_decode_info_t> decode_cache(
            drcontext,
            /*persist_decoded_instrs=*/false, ilist_for_test_decode_cache);
        decode_cache.init(ENCODING_FILE_TYPE, module_file_for_test_decode_cache, "");
        if (decode_cache.get_decode_info(
                reinterpret_cast<app_pc>(memrefs[0].instr.addr)) != nullptr) {
            return "Unexpected test_decode_info_t for never-seen pc";
        }
        std::string err = decode_cache.add_decode_info(memrefs[0].instr);
        if (err != "")
            return err;
        test_decode_info_t *decode_info_nop =
            decode_cache.get_decode_info(reinterpret_cast<app_pc>(memrefs[0].instr.addr));
        if (decode_info_nop == nullptr || !decode_info_nop->is_valid() ||
            !decode_info_nop->is_nop) {
            return "Unexpected test_decode_info_t for nop instr";
        }
        err = decode_cache.add_decode_info(memrefs[1].instr);
        if (err != "")
            return err;
        test_decode_info_t *decode_info_ret =
            decode_cache.get_decode_info(reinterpret_cast<app_pc>(memrefs[1].instr.addr));
        if (decode_info_ret == nullptr || !decode_info_ret->is_valid() ||
            !decode_info_ret->is_ret) {
            return "Unexpected test_decode_info_t for ret instr";
        }
        err = decode_cache.add_decode_info(memrefs[2].instr);
        if (err != "")
            return err;
        test_decode_info_t *decode_info_nop_2 =
            decode_cache.get_decode_info(reinterpret_cast<app_pc>(memrefs[2].instr.addr));
        if (decode_info_nop_2 != decode_info_nop) {
            return "Did not see same decode info instance for second instance of nop";
        }
    }
    instrlist_clear_and_destroy(drcontext, ilist);
    std::cerr << "check_decode_caching with persist_instrs: " << persist_instrs
              << ", use_module_mapper: " << use_module_mapper << " passed\n";
    return "";
}

std::string
check_missing_module_mapper_and_no_encoding(void *drcontext)
{
    memref_t instr = gen_instr(TID_A);
    test_decode_cache_t<instr_decode_info_t> decode_cache(
        drcontext,
        /*persist_decoded_instr=*/true, /*ilist_for_test_module_mapper=*/nullptr);
    std::string err = decode_cache.add_decode_info(instr.instr);
    if (err == "") {
        return "Expected error at add_decode_info but did not get any";
    }
    err = decode_cache.init(
        static_cast<offline_file_type_t>(OFFLINE_FILE_TYPE_SYSCALL_NUMBERS), "", "");
    if (err == "") {
        return "Expected error at init but did not get any";
    }
    std::cerr << "check_missing_module_mapper passed\n";
    return "";
}

int
test_main(int argc, const char *argv[])
{
    void *drcontext = dr_standalone_init();
    std::string err = check_decode_caching(drcontext, /*persist_instrs=*/false,
                                           /*use_module_mapper=*/false);
    if (err != "") {
        std::cerr << err << "\n";
        exit(1);
    }
    err = check_decode_caching(drcontext, /*persist_instrs=*/true,
                               /*use_module_mapper=*/false);
    if (err != "") {
        std::cerr << err << "\n";
        exit(1);
    }
    err = check_decode_caching(drcontext, /*persist_instrs=*/false,
                               /*use_module_mapper=*/true);
    if (err != "") {
        std::cerr << err << "\n";
        exit(1);
    }
    err = check_decode_caching(drcontext, /*persist_instrs=*/true,
                               /*use_module_mapper=*/true);
    if (err != "") {
        std::cerr << err << "\n";
        exit(1);
    }
    err = check_missing_module_mapper_and_no_encoding(drcontext);
    if (err != "") {
        std::cerr << err << "\n";
        exit(1);
    }

    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
