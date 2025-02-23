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

/* Tests for the opcode_mix_t analysis tool. */

#include <iostream>
#include <vector>

#include "decode_cache.h"
#include "tools/opcode_mix.h"
#include "../common/memref.h"
#include "memref_gen.h"

namespace dynamorio {
namespace drmemtrace {

class test_opcode_mix_t : public opcode_mix_t {
public:
    // Pass a non-nullptr instrlist_t if the module mapper must be used.
    test_opcode_mix_t(instrlist_t *instrs)
        : opcode_mix_t(/*module_file_path=*/"",
                       /*verbose=*/0,
                       /*alt_module_dir=*/"")
        , instrs_(instrs)
    {
    }
    std::unordered_map<int, int64_t>
    get_opcode_mix(void *shard)
    {
        shard_data_t *shard_data = reinterpret_cast<shard_data_t *>(shard);
        return shard_data->opcode_counts;
    }

protected:
    bool
    init_decode_cache(shard_data_t *shard, void *dcontext,
                      offline_file_type_t filetype) override
    {
        shard->decode_cache = std::unique_ptr<decode_cache_t<opcode_data_t>>(
            new test_decode_cache_t<opcode_data_t>(dcontext,
                                                   /*include_decoded_instr=*/true,
                                                   /*persist_decoded_instrs=*/false,
                                                   instrs_));
        if (!TESTANY(OFFLINE_FILE_TYPE_ENCODINGS, filetype)) {
            shard->error = shard->decode_cache->init(filetype, "some_module_file", "");
        } else {
            shard->error = shard->decode_cache->init(filetype);
        }
        return shard->error.empty();
    }

private:
    instrlist_t *instrs_;
};

class test_stream_t : public default_memtrace_stream_t {
public:
    test_stream_t(uint64_t filetype)
        : filetype_(filetype)
    {
    }
    uint64_t
    get_filetype() const override
    {
        return filetype_;
    }

private:
    uint64_t filetype_;
};

std::string
check_opcode_mix(void *drcontext, bool use_module_mapper)
{
    static constexpr addr_t BASE_ADDR = 0x123450;
    static constexpr addr_t TID_A = 1;
    instr_t *nop = XINST_CREATE_nop(drcontext);
    instr_t *ret = XINST_CREATE_return(drcontext);
    instrlist_t *ilist = instrlist_create(drcontext);
    instrlist_append(ilist, nop);
    instrlist_append(ilist, ret);
    uint64_t filetype = OFFLINE_FILE_TYPE_SYSCALL_NUMBERS |
        (use_module_mapper ? 0 : OFFLINE_FILE_TYPE_ENCODINGS);
    std::vector<memref_with_IR_t> memref_setup = {
        { gen_marker(TID_A, TRACE_MARKER_TYPE_FILETYPE, static_cast<uintptr_t>(filetype)),
          nullptr },
        { gen_instr(TID_A), nop },
        { gen_instr(TID_A), ret },
        { gen_instr(TID_A), nop },
    };
    std::vector<memref_t> memrefs;
    instrlist_t *ilist_for_test = nullptr;

    if (use_module_mapper) {
        // This does not set encodings in the memref.instr.
        memrefs = add_encodings_to_memrefs(ilist, memref_setup, 0,
                                           /*set_only_instr_addr=*/true);
        // We pass the instrs to construct the test_module_mapper_t in the
        // test_decode_cache_t;
        ilist_for_test = ilist;
    } else {
        memrefs = add_encodings_to_memrefs(ilist, memref_setup, BASE_ADDR);
        // Set up the second nop memref to reuse the same encoding as the first nop.
        memrefs[3].instr.encoding_is_new = false;
    }
    test_stream_t stream(filetype);
    test_opcode_mix_t opcode_mix(ilist_for_test);
    opcode_mix.initialize_stream(/*serial_stream=*/nullptr);
    void *shard_data = opcode_mix.parallel_shard_init_stream(0, nullptr, &stream);
    for (const memref_t &memref : memrefs) {
        if (!opcode_mix.parallel_shard_memref(shard_data, memref)) {
            return opcode_mix.parallel_shard_error(shard_data);
        }
    }
    std::unordered_map<int, int64_t> mix = opcode_mix.get_opcode_mix(shard_data);
    if (mix.size() != 2) {
        return "Found incorrect count of opcodes";
    }
    int64_t nop_count = mix[instr_get_opcode(nop)];
    if (nop_count != 2) {
        return "Found incorrect nop count";
    }
    int64_t ret_count = mix[instr_get_opcode(ret)];
    if (ret_count != 1) {
        return "Found incorrect ret count";
    }
    std::cerr << "check_opcode_mix with use_module_mapper: " << use_module_mapper
              << " passed\n";
    return "";
}

int
test_main(int argc, const char *argv[])
{
    void *drcontext = dr_standalone_init();
    std::string err = check_opcode_mix(drcontext, /*use_module_mapper=*/false);
    if (err != "") {
        std::cerr << err << "\n";
        exit(1);
    }
#ifndef WINDOWS
    // TODO i#5960: Enable these tests after the test-only Windows issue is
    // fixed.
    err = check_opcode_mix(drcontext, /*use_module_mapper=*/true);
    if (err != "") {
        std::cerr << err << "\n";
        exit(1);
    }
#endif
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
