/* **********************************************************
 * Copyright (c) 2025 Google, LLC  All rights reserved.
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

// Needs to be included before memref.h or build_target_arch_type() will not
// be defined by trace_entry.h.
#include "dr_api.h"

#include "decode_cache.h"
#include "../common/memref.h"
#include "memref_gen.h"

namespace dynamorio {
namespace drmemtrace {

#define CHECK(cond, msg, ...)             \
    do {                                  \
        if (!(cond)) {                    \
            fprintf(stderr, "%s\n", msg); \
            exit(1);                      \
        }                                 \
    } while (0)

static constexpr addr_t TID_A = 1;
static constexpr offline_file_type_t ENCODING_FILE_TYPE =
    static_cast<offline_file_type_t>(OFFLINE_FILE_TYPE_ENCODINGS);
static constexpr char FAKE_ERROR[] = "fake_error";

class test_decode_info_t : public decode_info_base_t {
public:
    test_decode_info_t()
    {
        // No lock needed because this test is single-threaded.
        object_idx_ = next_object_idx_++;
    }

    bool is_nop_ = false;
    bool is_ret_ = false;
    bool is_interrupt_ = false;
    bool decode_info_set_ = false;
    // This allows us to properly ensure that a new object was
    // created or not created by decode_cache_t.
    int object_idx_ = 0;

    static bool expect_decoded_instr_;

private:
    static int next_object_idx_;
    std::string
    set_decode_info_derived(void *dcontext,
                            const dynamorio::drmemtrace::_memref_instr_t &memref_instr,
                            instr_t *instr, app_pc decode_pc) override
    {
        CHECK(!decode_info_set_,
              "decode_cache_t should call set_decode_info only one time per object");
        instr_t my_decoded_instr;
        instr_init(dcontext, &my_decoded_instr);
        app_pc next_pc = decode_from_copy(dcontext, decode_pc,
                                          reinterpret_cast<app_pc>(memref_instr.addr),
                                          &my_decoded_instr);
        CHECK(next_pc != nullptr && instr_valid(&my_decoded_instr),
              "Expected to see a valid instr decoded from provided decode_pc");
        if (expect_decoded_instr_) {
            CHECK(instr != nullptr, "Expected to see a decoded instr_t");
            CHECK(instr_same(instr, &my_decoded_instr),
                  "Expected provided decoded instr_t and self decoded instr_t to be the "
                  "same");
        } else {
            CHECK(instr == nullptr, "Expected to see a null decoded instr");
        }

        // To test scenarios with an error during set_decode_info_derived, we
        // always return an error for ubr instrs.
        if (instr_is_ubr(&my_decoded_instr)) {
            instr_free(dcontext, &my_decoded_instr);
            return FAKE_ERROR;
        }

        is_nop_ = instr_is_nop(&my_decoded_instr);
        is_ret_ = instr_is_return(&my_decoded_instr);
        is_interrupt_ = instr_is_interrupt(&my_decoded_instr);
        decode_info_set_ = true;

        instr_free(dcontext, &my_decoded_instr);
        return "";
    }
};
bool test_decode_info_t::expect_decoded_instr_ = true;
int test_decode_info_t::next_object_idx_ = 1;

std::string
check_decode_caching(void *drcontext, bool use_module_mapper, bool include_decoded_instr,
                     bool persist_decoded_instr)
{
    static constexpr addr_t BASE_ADDR = 0x123450;
    instr_t *nop = XINST_CREATE_nop(drcontext);
    instr_t *ret = XINST_CREATE_return(drcontext);
    instr_t *interrupt = XINST_CREATE_interrupt(drcontext, OPND_CREATE_INT8(10));
    instr_t *jump = XINST_CREATE_jump(drcontext, opnd_create_instr(nop));
    instrlist_t *ilist = instrlist_create(drcontext);
    instrlist_append(ilist, nop);
    instrlist_append(ilist, ret);
    instrlist_append(ilist, interrupt);
    instrlist_append(ilist, jump);
    std::vector<memref_with_IR_t> memref_setup = {
        { gen_instr(TID_A), nop },  { gen_instr(TID_A), ret },
        { gen_instr(TID_A), nop },  { gen_instr(TID_A), interrupt },
        { gen_instr(TID_A), jump },
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

    test_decode_info_t test_decode_info;
    if (test_decode_info.is_valid()) {
        return "Unexpected valid default-constructed decode info";
    }

    if (persist_decoded_instr) {
        CHECK(include_decoded_instr, "persist_decoded_instr needs the decoded instr_t");
        // These are tests to verify the operation of instr_decode_info_t: that it stores
        // the instr_t correctly.
        // Tests for decode_cache_t are done when persist_decoded_instr = false (see
        // the else part below).
        test_decode_cache_t<instr_decode_info_t> decode_cache(
            drcontext, include_decoded_instr,
            /*persist_decoded_instr=*/true, ilist_for_test_decode_cache);
        std::string err =
            decode_cache.init(ENCODING_FILE_TYPE, module_file_for_test_decode_cache, "");
        if (!err.empty())
            return err;
        for (const memref_t &memref : memrefs) {
            instr_decode_info_t *unused_cached_decode_info;
            err = decode_cache.add_decode_info(memref.instr, unused_cached_decode_info);
            if (!err.empty())
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
        test_decode_info_t::expect_decoded_instr_ = include_decoded_instr;
        // These are tests to verify the operation of instr_decode_cache_t, including
        // whether it caches decode info correctly.
        test_decode_cache_t<test_decode_info_t> decode_cache(
            drcontext, include_decoded_instr,
            /*persist_decoded_instr=*/false, ilist_for_test_decode_cache);
        std::string err =
            decode_cache.init(ENCODING_FILE_TYPE, module_file_for_test_decode_cache, "");
        if (!err.empty())
            return err;
        // Test: Lookup non-existing pc.
        if (decode_cache.get_decode_info(
                reinterpret_cast<app_pc>(memrefs[0].instr.addr)) != nullptr) {
            return "Unexpected test_decode_info_t for never-seen pc";
        }

        test_decode_info_t *cached_decode_info;

        // Test: Lookup existing pc.
        err = decode_cache.add_decode_info(memrefs[0].instr, cached_decode_info);
        if (!err.empty())
            return err;
        test_decode_info_t *decode_info_nop =
            decode_cache.get_decode_info(reinterpret_cast<app_pc>(memrefs[0].instr.addr));
        if (decode_info_nop == nullptr || decode_info_nop != cached_decode_info ||
            !decode_info_nop->is_valid() || !decode_info_nop->is_nop_) {
            return "Unexpected test_decode_info_t for nop instr";
        }

        // Test: Lookup another existing pc.
        err = decode_cache.add_decode_info(memrefs[1].instr, cached_decode_info);
        if (!err.empty())
            return err;
        test_decode_info_t *decode_info_ret =
            decode_cache.get_decode_info(reinterpret_cast<app_pc>(memrefs[1].instr.addr));
        if (decode_info_ret == nullptr || decode_info_ret != cached_decode_info ||
            !decode_info_ret->is_valid() || !decode_info_ret->is_ret_) {
            return "Unexpected test_decode_info_t for ret instr";
        }

        // Test: Lookup existing pc but from a different memref.
        // Set up the second nop memref to reuse the same encoding as the first nop.
        int decode_info_nop_idx = decode_info_nop->object_idx_;
        memrefs[2].instr.encoding_is_new = false;
        err = decode_cache.add_decode_info(memrefs[2].instr, cached_decode_info);
        if (!err.empty())
            return err;
        test_decode_info_t *decode_info_nop_2 =
            decode_cache.get_decode_info(reinterpret_cast<app_pc>(memrefs[2].instr.addr));
        if (decode_info_nop_2 == nullptr || decode_info_nop_2 != cached_decode_info ||
            !decode_info_nop_2->is_valid() || !decode_info_nop_2->is_nop_) {
            return "Unexpected decode info instance for second instance of nop";
        }
        // decode_cache_t should not have added a new test_decode_info_t object.
        // We need to compare object_idx_ because sometimes the same address is
        // reassigned by the heap.
        if (decode_info_nop_2 != decode_info_nop ||
            decode_info_nop_2->object_idx_ != decode_info_nop_idx) {
            return "Did not expect a new test_decode_info_t to be created on re-add "
                   "for nop";
        }

        if (!use_module_mapper) {
            // Test: Overwrite existing decode info for a pc. Works only with embedded
            // encodings.
            // Pretend the interrupt is at the same trace pc as the ret.
            // Encodings have been added to the memref already so this still remains
            // an interrupt instruction even though we've modified addr.
            memrefs[3].instr.addr = memrefs[1].instr.addr;
            err = decode_cache.add_decode_info(memrefs[3].instr, cached_decode_info);
            if (!err.empty())
                return err;
            test_decode_info_t *decode_info_interrupt = decode_cache.get_decode_info(
                reinterpret_cast<app_pc>(memrefs[3].instr.addr));
            if (decode_info_interrupt == nullptr ||
                decode_info_interrupt != cached_decode_info ||
                !decode_info_interrupt->is_valid() ||
                !decode_info_interrupt->is_interrupt_ || decode_info_interrupt->is_ret_) {
                return "Unexpected test_decode_info_t for interrupt instr";
            }
            test_decode_info_t *decode_info_old_ret_pc = decode_cache.get_decode_info(
                reinterpret_cast<app_pc>(memrefs[1].instr.addr));
            if (decode_info_old_ret_pc != decode_info_interrupt) {
                return "Expected ret and interrupt memref pcs to return the same decode "
                       "info";
            }
        }

        // Test: Verify behavior on error. test_decode_info_t is set up to return an
        // error on XINST_CREATE_jump.
        err = decode_cache.add_decode_info(memrefs[4].instr, cached_decode_info);
        if (err.empty())
            return "Expected error for jump";
        test_decode_info_t *decode_info_jump =
            decode_cache.get_decode_info(reinterpret_cast<app_pc>(memrefs[4].instr.addr));
        if (decode_info_jump == nullptr || decode_info_jump != cached_decode_info ||
            decode_info_jump->is_valid() ||
            decode_info_jump->get_error_string() != FAKE_ERROR) {
            return "Unexpected test_decode_info_t for jump instr";
        }

        // Test: Verify behavior on second attempt to add a pc that encountered an
        // error previously.
        int decode_info_jump_idx = decode_info_jump->object_idx_;
        // For this test, we must say that the encoding is not new (or else it would
        // force a readd to the cache).
        memrefs[4].instr.encoding_is_new = false;
        err = decode_cache.add_decode_info(memrefs[4].instr, cached_decode_info);
        if (err.empty())
            return "Expected error for second attempt to add jump";
        test_decode_info_t *decode_info_jump_2 =
            decode_cache.get_decode_info(reinterpret_cast<app_pc>(memrefs[4].instr.addr));
        if (decode_info_jump_2 == nullptr || decode_info_jump_2 != cached_decode_info ||
            decode_info_jump_2->is_valid() ||
            decode_info_jump_2->get_error_string() != FAKE_ERROR) {
            return "Unexpected decode info instance for second instance of jump";
        }
        // decode_cache_t should not have reattempted decoding by creating a
        // new test_decode_info_t object.
        // We need to compare object_idx_ because sometimes the same address is
        // reassigned by the heap.
        if (decode_info_jump != decode_info_jump_2 ||
            decode_info_jump_2->object_idx_ != decode_info_jump_idx) {
            return "Did not expect a new test_decode_info_t to be created on "
                   " retry after prior failure for jump.";
        }

        // Test: Verify all cached decode info gets cleared.
        decode_cache.clear_cache();
        decode_info_nop =
            decode_cache.get_decode_info(reinterpret_cast<app_pc>(memrefs[0].instr.addr));
        decode_info_ret =
            decode_cache.get_decode_info(reinterpret_cast<app_pc>(memrefs[1].instr.addr));
        if (decode_info_nop != nullptr || decode_info_ret != nullptr) {
            return "Cached decode info not cleared after clear_cache()";
        }
    }
    instrlist_clear_and_destroy(drcontext, ilist);
    std::cerr << "check_decode_caching with use_module_mapper: " << use_module_mapper
              << ", include_decoded_instr: " << include_decoded_instr
              << ", persist_decoded_instr: " << persist_decoded_instr << " passed\n";
    return "";
}

std::string
check_init_error_cases(void *drcontext)
{
    instrlist_t *ilist = instrlist_create(drcontext);
    memref_t instr = gen_instr(TID_A);
    test_decode_cache_t<instr_decode_info_t> decode_cache(
        drcontext, /*include_decoded_instr=*/true,
        /*persist_decoded_instr=*/true, ilist);
    instr_decode_info_t dummy;
    // Initialize to non-nullptr for the test.
    instr_decode_info_t *cached_decode_info = &dummy;

    // Missing init before add_decode_info.
    std::string err = decode_cache.add_decode_info(instr.instr, cached_decode_info);
    if (err.empty()) {
        return "Expected error at add_decode_info but did not get any";
    }
    if (cached_decode_info != nullptr) {
        return "Expected returned reference cached_decode_info to be nullptr";
    }

    // init for a filetype without encodings, with no module file path either.
    err = decode_cache.init(
        static_cast<offline_file_type_t>(OFFLINE_FILE_TYPE_SYSCALL_NUMBERS), "", "");
    if (err.empty()) {
        return "Expected error at init but did not get any";
    }

    // Multiple init calls on the same decode cache instance.
    err = decode_cache.init(
        static_cast<offline_file_type_t>(OFFLINE_FILE_TYPE_SYSCALL_NUMBERS),
        "some_module_file_path", "");
    if (!err.empty()) {
        return "Expected successful init, got error: " + err;
    }
    err = decode_cache.init(
        static_cast<offline_file_type_t>(OFFLINE_FILE_TYPE_SYSCALL_NUMBERS),
        "some_module_file_path", "");
    if (err.empty()) {
        return "Expected error at re-init";
    }

    // Different module_file_path provided to a different decode cache instance.
    test_decode_cache_t<instr_decode_info_t> another_decode_cache(
        drcontext, /*include_decoded_instr=*/true,
        /*persist_decoded_instr=*/true, ilist);
    err = another_decode_cache.init(
        static_cast<offline_file_type_t>(OFFLINE_FILE_TYPE_SYSCALL_NUMBERS),
        "some_other_module_file_path", "");
    if (err.empty()) {
        return "Expected error at init with different module file path";
    }
    err = another_decode_cache.init(
        static_cast<offline_file_type_t>(OFFLINE_FILE_TYPE_SYSCALL_NUMBERS),
        "some_module_file_path", "");
    if (!err.empty()) {
        return "Expected successful init on another decode cache instance, got error: " +
            err;
    }

    // Decode cache that specifies a different module_file_path but it works since
    // it's empty.
    test_decode_cache_t<instr_decode_info_t> decode_cache_no_mod(
        drcontext, /*include_decoded_instr=*/true,
        /*persist_decoded_instr=*/true, nullptr);
    err = decode_cache_no_mod.init(
        static_cast<offline_file_type_t>(OFFLINE_FILE_TYPE_ENCODINGS), "", "");
    if (!err.empty()) {
        return "Expected no error for empty module file path, got: " + err;
    }

    // Decode cache init with wrong arch.
    offline_file_type_t file_type_with_arch = build_target_arch_type();
    offline_file_type_t file_type_with_wrong_arch = file_type_with_arch;
    if (file_type_with_arch == OFFLINE_FILE_TYPE_ARCH_AARCH64) {
        file_type_with_wrong_arch = OFFLINE_FILE_TYPE_ARCH_X86_64;
    } else {
        file_type_with_wrong_arch = OFFLINE_FILE_TYPE_ARCH_AARCH64;
    }
    test_decode_cache_t<instr_decode_info_t> decode_cache_wrong_arch(
        drcontext, /*include_decoded_instr=*/true,
        /*persist_decoded_instr=*/true, nullptr);
    err = decode_cache_wrong_arch.init(file_type_with_wrong_arch, "some_module_file_path",
                                       "");
    if (err.empty()) {
        return "Expected error on file type with wrong arch";
    }

    // Decode cache init with wrong arch but with include_decoded_instr_
    // set to false;
    test_decode_cache_t<instr_decode_info_t> decode_cache_wrong_arch_no_decode(
        drcontext, /*include_decoded_instr=*/false,
        /*persist_decoded_instr=*/false, nullptr);
    err = decode_cache_wrong_arch_no_decode.init(file_type_with_wrong_arch,
                                                 "some_module_file_path", "");
    if (!err.empty()) {
        return "Expected no error on file type with wrong arch when not decoding, got: " +
            err;
    }

    instrlist_clear_and_destroy(drcontext, ilist);
    std::cerr << "check_missing_module_mapper passed\n";
    return "";
}

int
test_main(int argc, const char *argv[])
{
    void *drcontext = dr_standalone_init();
    std::string err = check_decode_caching(drcontext, /*use_module_mapper=*/false,
                                           /*include_decoded_instr=*/false,
                                           /*persist_decoded_instr=*/false);
    if (!err.empty()) {
        std::cerr << err << "\n";
        exit(1);
    }
    err = check_decode_caching(drcontext, /*use_module_mapper=*/false,
                               /*include_decoded_instr=*/true,
                               /*persist_decoded_instr=*/false);
    if (!err.empty()) {
        std::cerr << err << "\n";
        exit(1);
    }
    err = check_decode_caching(drcontext, /*use_module_mapper=*/false,
                               /*include_decoded_instr=*/true,
                               /*persist_decoded_instr=*/true);
    if (!err.empty()) {
        std::cerr << err << "\n";
        exit(1);
    }
#ifndef WINDOWS
    // TODO i#5960: Enable these tests after the test-only Windows issue is
    // fixed.
    err = check_decode_caching(drcontext, /*use_module_mapper=*/true,
                               /*include_decoded_instr=*/false,
                               /*persist_decoded_instr=*/false);
    if (!err.empty()) {
        std::cerr << err << "\n";
        exit(1);
    }
    err = check_decode_caching(drcontext, /*use_module_mapper=*/true,
                               /*include_decoded_instr=*/true,
                               /*persist_decoded_instr=*/false);
    if (!err.empty()) {
        std::cerr << err << "\n";
        exit(1);
    }
    err = check_decode_caching(drcontext, /*use_module_mapper=*/true,
                               /*include_decoded_instr=*/true,
                               /*persist_decoded_instr=*/true);
    if (!err.empty()) {
        std::cerr << err << "\n";
        exit(1);
    }
#endif
    err = check_init_error_cases(drcontext);
    if (!err.empty()) {
        std::cerr << err << "\n";
        exit(1);
    }

    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
