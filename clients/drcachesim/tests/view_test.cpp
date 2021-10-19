/* **********************************************************
 * Copyright (c) 2021 Google, LLC  All rights reserved.
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

/* Unit tests for the view_t tool. */

#include <algorithm>
#include <iostream>
#include <vector>

#include "../tools/view.h"
#include "../common/memref.h"
#include "../tracer/raw2trace.h"
#include "memref_gen.h"

#define ASSERT(cond, msg, ...)        \
    do {                              \
        if (!(cond)) {                \
            std::cerr << msg << "\n"; \
            abort();                  \
        }                             \
    } while (0)

#define CHECK(cond, msg, ...)         \
    do {                              \
        if (!(cond)) {                \
            std::cerr << msg << "\n"; \
            return false;             \
        }                             \
    } while (0)

namespace {

// Subclasses module_mapper_t and replaces the module loading with a buffer
// of encoded instr_t.
class module_mapper_test_t : public module_mapper_t {
public:
    module_mapper_test_t(void *drcontext, instrlist_t &instrs)
        : module_mapper_t(nullptr)
    {
        byte *pc = instrlist_encode(drcontext, &instrs, decode_buf_, true);
        ASSERT(pc - decode_buf_ < MAX_DECODE_SIZE, "decode buffer overflow");
        // Clear do_module_parsing error; we can't cleanly make virtual b/c it's
        // called from the constructor.
        last_error_ = "";
    }

protected:
    void
    read_and_map_modules(void) override
    {
        modvec_.push_back(module_t("fake_exe", 0, decode_buf_, 0, MAX_DECODE_SIZE,
                                   MAX_DECODE_SIZE, true));
    }

private:
    static const int MAX_DECODE_SIZE = 1024;
    byte decode_buf_[MAX_DECODE_SIZE];
};

class view_test_t : public view_t {
public:
    view_test_t(void *drcontext, instrlist_t &instrs, memref_tid_t thread,
                uint64_t skip_refs, uint64_t sim_refs)
        : view_t("", thread, skip_refs, sim_refs, "", 0)
    {
        module_mapper_ =
            std::unique_ptr<module_mapper_t>(new module_mapper_test_t(drcontext, instrs));
    }

    std::string
    initialize() override
    {
        module_mapper_->get_loaded_modules();
        dr_disasm_flags_t flags =
            IF_X86_ELSE(DR_DISASM_ATT, IF_AARCH64_ELSE(DR_DISASM_DR, DR_DISASM_ARM));
        disassemble_set_syntax(flags);
        return "";
    }
};

std::string
run_test_helper(view_test_t &view, const std::vector<memref_t> &memrefs)
{
    view.initialize();
    // Capture cerr.
    std::stringstream capture;
    std::streambuf *prior = std::cerr.rdbuf(capture.rdbuf());
    // Run the tool.
    for (const auto &memref : memrefs) {
        if (!view.process_memref(memref))
            std::cout << "Hit error: " << view.get_error_string() << "\n";
    }
    // Return the result.
    std::string res = capture.str();
    std::cerr.rdbuf(prior);
    return res;
}

bool
test_no_limit(void *drcontext, instrlist_t &ilist, const std::vector<memref_t> &memrefs)
{
    view_test_t view(drcontext, ilist, 0, 0, 0);
    std::string res = run_test_helper(view, memrefs);
    if (std::count(res.begin(), res.end(), '\n') != static_cast<int>(memrefs.size())) {
        std::cerr << "Incorrect line count\n";
        return false;
    }
    return true;
}

bool
test_num_memrefs(void *drcontext, instrlist_t &ilist,
                 const std::vector<memref_t> &memrefs, int num_memrefs)
{
    ASSERT(static_cast<size_t>(num_memrefs) < memrefs.size(),
           "need more memrefs to limit");
    view_test_t view(drcontext, ilist, 0, 0, num_memrefs);
    std::string res = run_test_helper(view, memrefs);
    if (std::count(res.begin(), res.end(), '\n') != num_memrefs) {
        std::cerr << "Incorrect num_memrefs count\n";
        return false;
    }
    return true;
}

bool
test_skip_memrefs(void *drcontext, instrlist_t &ilist,
                  const std::vector<memref_t> &memrefs, int skip_memrefs)
{
    const int num_memrefs = 2;
    // We do a simple check on the marker count.
    // XXX: To test precisely skipping the instrs and data we'll need to spend
    // more effort here, but the initial delayed markers are the corner cases.
    int all_count = 0, marker_count = 0;
    for (const auto &memref : memrefs) {
        if (all_count++ < skip_memrefs)
            continue;
        if (all_count - skip_memrefs > num_memrefs)
            break;
        if (memref.marker.type == TRACE_TYPE_MARKER)
            ++marker_count;
    }
    ASSERT(static_cast<size_t>(num_memrefs + skip_memrefs) <= memrefs.size(),
           "need more memrefs to skip");
    view_test_t view(drcontext, ilist, 0, skip_memrefs, num_memrefs);
    std::string res = run_test_helper(view, memrefs);
    if (std::count(res.begin(), res.end(), '\n') != num_memrefs) {
        std::cerr << "Incorrect skipped num_memrefs count\n";
        return false;
    }
    int found_markers = 0;
    size_t pos = 0;
    while (pos != std::string::npos) {
        pos = res.find("marker", pos);
        if (pos != std::string::npos) {
            ++found_markers;
            ++pos;
        }
    }
    if (found_markers != marker_count) {
        std::cerr << "Failed to skip proper number of markers\n";
        return false;
    }
    return true;
}

bool
test_thread_limit(instrlist_t &ilist, const std::vector<memref_t> &memrefs,
                  void *drcontext, int thread2_id)
{
    int thread2_count = 0;
    for (const auto &memref : memrefs) {
        if (memref.data.tid == thread2_id)
            ++thread2_count;
    }
    view_test_t view(drcontext, ilist, thread2_id, 0, 0);
    std::string res = run_test_helper(view, memrefs);
    // Count the Tnnnn prefixes.
    std::stringstream ss;
    ss << "T" << thread2_id;
    std::string prefix = ss.str();
    int found_prefixes = 0;
    size_t pos = 0;
    while (pos != std::string::npos) {
        pos = res.find(prefix, pos);
        if (pos != std::string::npos) {
            ++found_prefixes;
            ++pos;
        }
    }
    if (std::count(res.begin(), res.end(), '\n') != thread2_count ||
        found_prefixes != thread2_count) {
        std::cerr << "Incorrect thread2 count\n";
        return false;
    }
    return true;
}

bool
run_limit_tests(void *drcontext)
{
    bool res = true;

    instrlist_t *ilist = instrlist_create(drcontext);
    // raw2trace doesn't like offsets of 0 so we shift with a nop.
    instr_t *nop1 = XINST_CREATE_nop(drcontext);
    instr_t *nop2 = XINST_CREATE_nop(drcontext);
    instr_t *jcc = XINST_CREATE_jump_cond(drcontext, DR_PRED_EQ, opnd_create_instr(nop2));
    instrlist_append(ilist, nop1);
    instrlist_append(ilist, jcc);
    instrlist_append(ilist, nop2);
    size_t offs_nop1 = 0;
    size_t offs_jz = offs_nop1 + instr_length(drcontext, nop1);
    size_t offs_nop2 = offs_jz + instr_length(drcontext, jcc);

    const memref_tid_t t1 = 3;
    std::vector<memref_t> memrefs = {
        gen_marker(t1, TRACE_MARKER_TYPE_VERSION, 3),
        gen_marker(t1, TRACE_MARKER_TYPE_FILETYPE, 0),
        gen_marker(t1, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
        gen_instr(t1, offs_nop1),
        gen_data(t1, true, 0x42, 4),
        gen_branch(t1, offs_jz),
        gen_branch(t1, offs_nop2),
        gen_data(t1, true, 0x42, 4),
    };

    res = test_no_limit(drcontext, *ilist, memrefs) && res;
    for (int i = 1; i < static_cast<int>(memrefs.size()); ++i) {
        res = test_num_memrefs(drcontext, *ilist, memrefs, i) && res;
    }
    // We primarily test skipping the initial markers.
    for (int i = 1; i < 6; ++i) {
        res = test_skip_memrefs(drcontext, *ilist, memrefs, i) && res;
    }

    const memref_tid_t t2 = 21;
    std::vector<memref_t> thread_memrefs = {
        gen_marker(t1, TRACE_MARKER_TYPE_VERSION, 3),
        gen_marker(t1, TRACE_MARKER_TYPE_FILETYPE, 0),
        gen_marker(t1, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
        gen_instr(t1, offs_nop1),
        gen_data(t1, true, 0x42, 4),
        gen_branch(t1, offs_jz),
        gen_branch(t1, offs_nop2),
        gen_data(t1, true, 0x42, 4),
        gen_marker(t2, TRACE_MARKER_TYPE_VERSION, 3),
        gen_marker(t2, TRACE_MARKER_TYPE_FILETYPE, 0),
        gen_marker(t2, TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64),
        gen_marker(t2, TRACE_MARKER_TYPE_TIMESTAMP, 101),
        gen_instr(t2, offs_nop1),
        gen_data(t2, true, 0x42, 4),
        gen_branch(t2, offs_jz),
        gen_branch(t2, offs_nop2),
        gen_data(t2, true, 0x42, 4),
    };
    res = test_thread_limit(*ilist, thread_memrefs, drcontext, t2) && res;

    instrlist_clear_and_destroy(drcontext, ilist);
    return res;
}
} // namespace

int
main(int argc, const char *argv[])
{
    void *drcontext = dr_standalone_init();
    if (run_limit_tests(drcontext)) {
        std::cerr << "view_test passed\n";
        return 0;
    }
    std::cerr << "view_test FAILED\n";
    exit(1);
}
