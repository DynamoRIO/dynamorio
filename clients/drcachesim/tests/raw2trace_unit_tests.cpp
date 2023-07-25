/* **********************************************************
 * Copyright (c) 2021-2023 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Unit tests for raw2trace. */

#include "dr_api.h"
#include "memref_gen.h"
#include "tracer/raw2trace.h"
#include "tracer/raw2trace_directory.h"
#include <iostream>
#include <sstream>

namespace dynamorio {
namespace drmemtrace {

#undef ASSERT
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

// Module mapper for testing different module bounds but without encodings.
class test_multi_module_mapper_t : public module_mapper_t {
public:
    struct bounds_t {
        bounds_t(addr_t start, addr_t end)
            : start(start)
            , end(end)
        {
        }
        addr_t start;
        addr_t end;
    };
    test_multi_module_mapper_t(const std::vector<bounds_t> &modules)
        : module_mapper_t(nullptr)
        , bounds_(modules)
    {
        // Clear do_module_parsing error; we can't cleanly make virtual b/c it's
        // called from the constructor.
        last_error_ = "";
    }

protected:
    void
    read_and_map_modules() override
    {
        for (size_t i = 0; i < bounds_.size(); i++) {
            modvec_.push_back(module_t("fake_module",
                                       reinterpret_cast<app_pc>(bounds_[i].start),
                                       nullptr, 0, bounds_[i].end - bounds_[i].start,
                                       bounds_[i].end - bounds_[i].start, true));
        }
    }

private:
    std::vector<bounds_t> bounds_;
};

// Subclasses raw2trace_t and replaces the module_mapper_t with our own version.
class raw2trace_test_t : public raw2trace_t {
public:
    raw2trace_test_t(const std::vector<std::istream *> &input,
                     const std::vector<std::ostream *> &output, instrlist_t &instrs,
                     void *drcontext)
        : raw2trace_t(nullptr, input, output, {}, INVALID_FILE, nullptr, nullptr,
                      drcontext,
                      // The sequences are small so we print everything for easier
                      // debugging and viewing of what's going on.
                      4)
    {
        module_mapper_ = std::unique_ptr<module_mapper_t>(
            new test_module_mapper_t(&instrs, drcontext));
        set_modmap_(module_mapper_.get());
    }
    raw2trace_test_t(const std::vector<std::istream *> &input,
                     const std::vector<archive_ostream_t *> &output, instrlist_t &instrs,
                     void *drcontext, uint64_t chunk_instr_count = 10 * 1000 * 1000)
        : raw2trace_t(nullptr, input, {}, output, INVALID_FILE, nullptr, nullptr,
                      drcontext,
                      // The sequences are small so we print everything for easier
                      // debugging and viewing of what's going on.
                      4, /*worker_count=*/-1, /*alt_module_dir=*/"", chunk_instr_count)
    {
        module_mapper_ = std::unique_ptr<module_mapper_t>(
            new test_module_mapper_t(&instrs, drcontext));
        set_modmap_(module_mapper_.get());
    }
    raw2trace_test_t(const std::vector<std::istream *> &input,
                     const std::vector<std::ostream *> &output,
                     const std::vector<test_multi_module_mapper_t::bounds_t> &modules,
                     void *drcontext)
        : raw2trace_t(nullptr, input, output, {}, INVALID_FILE, nullptr, nullptr,
                      drcontext,
                      // The sequences are small so we print everything for easier
                      // debugging and viewing of what's going on.
                      4)
    {
        module_mapper_ =
            std::unique_ptr<module_mapper_t>(new test_multi_module_mapper_t(modules));
        set_modmap_(module_mapper_.get());
    }
};

class archive_ostream_test_t : public archive_ostream_t {
public:
    archive_ostream_test_t()
        : archive_ostream_t(new std::basic_stringbuf<char, std::char_traits<char>>)
    {
    }
    std::string
    open_new_component(const std::string &name) override
    {
        return "";
    }
    std::string
    str()
    {
        return reinterpret_cast<std::basic_stringbuf<char, std::char_traits<char>> *>(
                   rdbuf())
            ->str();
    }
};

offline_entry_t
make_header(int version = OFFLINE_FILE_VERSION)
{
    offline_entry_t entry;
    entry.extended.type = OFFLINE_TYPE_EXTENDED;
    entry.extended.ext = OFFLINE_EXT_TYPE_HEADER;
    entry.extended.valueA = OFFLINE_FILE_TYPE_DEFAULT | OFFLINE_FILE_TYPE_ENCODINGS |
        OFFLINE_FILE_TYPE_SYSCALL_NUMBERS;
    entry.extended.valueB = version;
    return entry;
}

offline_entry_t
make_pid()
{
    offline_entry_t entry;
    entry.pid.type = OFFLINE_TYPE_PID;
    entry.pid.pid = 1;
    return entry;
}

offline_entry_t
make_tid()
{
    offline_entry_t entry;
    entry.tid.type = OFFLINE_TYPE_THREAD;
    entry.tid.tid = 1;
    return entry;
}

offline_entry_t
make_line_size()
{
    offline_entry_t entry;
    entry.extended.type = OFFLINE_TYPE_EXTENDED;
    entry.extended.ext = OFFLINE_EXT_TYPE_MARKER;
    entry.extended.valueA = 64;
    entry.extended.valueB = TRACE_MARKER_TYPE_CACHE_LINE_SIZE;
    return entry;
}

offline_entry_t
make_exit()
{
    offline_entry_t entry;
    entry.extended.type = OFFLINE_TYPE_EXTENDED;
    entry.extended.ext = OFFLINE_EXT_TYPE_FOOTER;
    entry.extended.valueA = 0;
    entry.extended.valueB = 0;
    return entry;
}

offline_entry_t
make_block(uint64_t offs, uint64_t instr_count)
{
    offline_entry_t entry;
    entry.pc.type = OFFLINE_TYPE_PC;
    entry.pc.modidx = 0; // Just one "module" in this test.
    entry.pc.modoffs = offs;
    entry.pc.instr_count = instr_count;
    return entry;
}

offline_entry_t
make_memref(uint64_t addr)
{
    offline_entry_t entry;
    entry.addr.type = OFFLINE_TYPE_MEMREF;
    entry.addr.addr = addr;
    return entry;
}

offline_entry_t
make_timestamp(uint64_t value = 0)
{
    static int timecount;
    offline_entry_t entry;
    entry.timestamp.type = OFFLINE_TYPE_TIMESTAMP;
    entry.timestamp.usec = value == 0 ? ++timecount : value;
    return entry;
}

offline_entry_t
make_core()
{
    offline_entry_t entry;
    entry.extended.type = OFFLINE_TYPE_EXTENDED;
    entry.extended.ext = OFFLINE_EXT_TYPE_MARKER;
    entry.extended.valueA = 0;
    entry.extended.valueB = TRACE_MARKER_TYPE_CPU_ID;
    return entry;
}

offline_entry_t
make_window_id(uint64_t id)
{
    offline_entry_t entry;
    entry.extended.type = OFFLINE_TYPE_EXTENDED;
    entry.extended.ext = OFFLINE_EXT_TYPE_MARKER;
    entry.extended.valueA = id;
    entry.extended.valueB = TRACE_MARKER_TYPE_WINDOW_ID;
    return entry;
}

offline_entry_t
make_marker(uint64_t type, int64_t value)
{
    offline_entry_t entry;
    entry.extended.type = OFFLINE_TYPE_EXTENDED;
    entry.extended.ext = OFFLINE_EXT_TYPE_MARKER;
    entry.extended.valueA = value;
    entry.extended.valueB = type;
    return entry;
}

bool
check_entry(std::vector<trace_entry_t> &entries, int &idx, unsigned short expected_type,
            int expected_size, addr_t expected_addr = 0)
{
    if (expected_type != entries[idx].type ||
        (expected_size > 0 &&
         static_cast<unsigned short>(expected_size) != entries[idx].size) ||
        (expected_addr > 0 && expected_addr != entries[idx].addr)) {
        std::cerr << "Entry " << idx << " has type " << entries[idx].type << " and size "
                  << entries[idx].size << " and addr " << entries[idx].addr
                  << " != expected type " << expected_type << " and expected size "
                  << expected_size << " and expected addr " << expected_addr << "\n";
        return false;
    }
    ++idx;
    return true;
}

// Takes ownership of ilist and destroys it.
bool
run_raw2trace(void *drcontext, const std::vector<offline_entry_t> raw, instrlist_t *ilist,
              std::vector<trace_entry_t> &entries, int chunk_instr_count = 0,
              const std::vector<test_multi_module_mapper_t::bounds_t> &modules = {})
{
    // We need an istream so we use istringstream.
    std::ostringstream raw_out;
    for (const auto &entry : raw) {
        std::string as_string(reinterpret_cast<const char *>(&entry),
                              reinterpret_cast<const char *>(&entry + 1));
        raw_out << as_string;
    }
    std::istringstream raw_in(raw_out.str());
    std::vector<std::istream *> input;
    input.push_back(&raw_in);

    std::string result;

    if (chunk_instr_count > 0) {
        // We need an archive_ostream to enable chunking.
        archive_ostream_test_t result_stream;
        std::vector<archive_ostream_t *> output;
        output.push_back(&result_stream);

        // Run raw2trace with our subclass supplying our decodings.
        // Pass in our chunk instr count.
        raw2trace_test_t raw2trace(input, output, *ilist, drcontext, chunk_instr_count);
        std::string error = raw2trace.do_conversion();
        CHECK(error.empty(), error);
        result = result_stream.str();
    } else if (modules.empty()) {
        // We need an ostream to capture out.
        std::ostringstream result_stream;
        std::vector<std::ostream *> output;
        output.push_back(&result_stream);

        // Run raw2trace with our subclass supplying our decodings.
        raw2trace_test_t raw2trace(input, output, *ilist, drcontext);
        std::string error = raw2trace.do_conversion();
        CHECK(error.empty(), error);
        result = result_stream.str();
    } else {
        // We need an ostream to capture out.
        std::ostringstream result_stream;
        std::vector<std::ostream *> output;
        output.push_back(&result_stream);

        // Run raw2trace with our subclass supplying module bounds.
        raw2trace_test_t raw2trace(input, output, modules, drcontext);
        std::string error = raw2trace.do_conversion();
        CHECK(error.empty(), error);
        result = result_stream.str();
    }
    if (ilist != nullptr)
        instrlist_clear_and_destroy(drcontext, ilist);

    // Now check the results.
    char *start = &result[0];
    char *end = start + result.size();
    CHECK(result.size() % sizeof(trace_entry_t) == 0,
          "output is not a multiple of trace_entry_t");
    while (start < end) {
        entries.push_back(*reinterpret_cast<trace_entry_t *>(start));
        start += sizeof(trace_entry_t);
    }
    int idx = 0;
    for (const auto &entry : entries) {
        std::cerr << idx << " type: " << entry.type << " size: " << entry.size
                  << " val: " << entry.addr << "\n";
        ++idx;
    }
    return true;
}

bool
test_branch_delays(void *drcontext)
{
    std::cerr << "\n===============\nTesting branch delays\n";
    // Our synthetic test first constructs a list of instructions to be encoded into
    // a buffer for decoding by raw2trace.
    instrlist_t *ilist = instrlist_create(drcontext);
    // raw2trace doesn't like offsets of 0 so we shift with a nop.
    instr_t *nop = XINST_CREATE_nop(drcontext);
    instr_t *move =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instr_t *jmp = XINST_CREATE_jump(drcontext, opnd_create_instr(move));
    instr_t *jcc = XINST_CREATE_jump_cond(drcontext, DR_PRED_EQ, opnd_create_instr(jmp));
    instrlist_append(ilist, nop);
    instrlist_append(ilist, jcc);
    instrlist_append(ilist, jmp);
    instrlist_append(ilist, move);
    size_t offs_nop = 0;
    size_t offs_jz = offs_nop + instr_length(drcontext, nop);
    size_t offs_jmp = offs_jz + instr_length(drcontext, jcc);
    size_t offs_mov = offs_jmp + instr_length(drcontext, jmp);

    // Now we synthesize our raw trace itself, including a valid header sequence.
    std::vector<offline_entry_t> raw;
    raw.push_back(make_header());
    raw.push_back(make_tid());
    raw.push_back(make_pid());
    raw.push_back(make_line_size());
    raw.push_back(make_block(offs_jz, 1));
    raw.push_back(make_timestamp());
    raw.push_back(make_core());
    raw.push_back(make_block(offs_jmp, 1));
    raw.push_back(make_block(offs_mov, 1));
    raw.push_back(make_exit());

    std::vector<trace_entry_t> entries;
    if (!run_raw2trace(drcontext, raw, ilist, entries))
        return false;
    int idx = 0;
    return (
        check_entry(entries, idx, TRACE_TYPE_HEADER, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD, -1) &&
        check_entry(entries, idx, TRACE_TYPE_PID, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER,
                    TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        // Both branches should be delayed until after the timestamp+cpu markers:
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#ifdef X86_32
        // An extra encoding entry is needed.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#endif
        check_entry(entries, idx, TRACE_TYPE_INSTR_TAKEN_JUMP, -1) &&
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#ifdef X86_32
        // An extra encoding entry is needed.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#endif
        check_entry(entries, idx, TRACE_TYPE_INSTR_DIRECT_JUMP, -1) &&
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD_EXIT, -1) &&
        check_entry(entries, idx, TRACE_TYPE_FOOTER, -1));
}

bool
test_marker_placement(void *drcontext)
{
    std::cerr << "\n===============\nTesting marker placement\n";
    // Our synthetic test first constructs a list of instructions to be encoded into
    // a buffer for decoding by raw2trace.
    instrlist_t *ilist = instrlist_create(drcontext);
    // raw2trace doesn't like offsets of 0 so we shift with a nop.
    instr_t *nop = XINST_CREATE_nop(drcontext);
    // We test these scenarios:
    // 1) A block with an implicit instr to ensure the markers are not inserted
    //    between the instrs in the block.
    // 2) A block with an implicit memref for the first instr, to reproduce i#5620
    //    where markers should wait for the memref (and subsequent implicit instrs).
    instr_t *move1 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instr_t *move2 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
#ifdef AARCH64
    // XXX i#5628: opnd_create_mem_instr is not supported yet on AArch64.
    instr_t *load1 =
        INSTR_CREATE_ldr(drcontext, opnd_create_reg(REG1),
                         // Our addresses are 0-based so we pick a low value that a
                         // PC-relative offset can reach.
                         OPND_CREATE_ABSMEM(reinterpret_cast<void *>(1024ULL), OPSZ_PTR));
#else
    instr_t *load1 = XINST_CREATE_load(drcontext, opnd_create_reg(REG1),
                                       opnd_create_mem_instr(move1, 0, OPSZ_PTR));
#endif
    instr_t *move3 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instrlist_append(ilist, nop);
    // Block 1.
    instrlist_append(ilist, move1);
    instrlist_append(ilist, move2);
    // Block 2.
    instrlist_append(ilist, load1);
    instrlist_append(ilist, move3);
    size_t offs_nop = 0;
    size_t offs_move1 = offs_nop + instr_length(drcontext, nop);
    size_t offs_move2 = offs_move1 + instr_length(drcontext, move1);
    size_t offs_load1 = offs_move2 + instr_length(drcontext, move2);

    // Now we synthesize our raw trace itself, including a valid header sequence.
    std::vector<offline_entry_t> raw;
    raw.push_back(make_header());
    raw.push_back(make_tid());
    raw.push_back(make_pid());
    raw.push_back(make_line_size());
    raw.push_back(make_block(offs_move1, 2));
    raw.push_back(make_marker(TRACE_MARKER_TYPE_FUNC_ID, 0));
    raw.push_back(make_marker(TRACE_MARKER_TYPE_FUNC_RETADDR, 4));
    raw.push_back(make_marker(TRACE_MARKER_TYPE_FUNC_ARG, 2));
    raw.push_back(make_block(offs_load1, 2));
    raw.push_back(make_marker(TRACE_MARKER_TYPE_FUNC_ID, 0));
    raw.push_back(make_marker(TRACE_MARKER_TYPE_FUNC_RETADDR, 4));
    raw.push_back(make_marker(TRACE_MARKER_TYPE_FUNC_ARG, 2));
    raw.push_back(make_exit());

    std::vector<trace_entry_t> entries;
    if (!run_raw2trace(drcontext, raw, ilist, entries))
        return false;
    int idx = 0;
    return (
        check_entry(entries, idx, TRACE_TYPE_HEADER, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD, -1) &&
        check_entry(entries, idx, TRACE_TYPE_PID, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER,
                    TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT) &&
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_ID) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_RETADDR) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_ARG) &&
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#ifdef X86_32
        // An extra encoding entry is needed.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#endif
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        check_entry(entries, idx, TRACE_TYPE_READ, -1) &&
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_ID) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_RETADDR) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_ARG) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD_EXIT, -1) &&
        check_entry(entries, idx, TRACE_TYPE_FOOTER, -1));
}

bool
test_marker_delays(void *drcontext)
{
    std::cerr << "\n===============\nTesting marker delays\n";
    // Our synthetic test first constructs a list of instructions to be encoded into
    // a buffer for decoding by raw2trace.
    instrlist_t *ilist = instrlist_create(drcontext);
    // raw2trace doesn't like offsets of 0 so we shift with a nop.
    instr_t *nop = XINST_CREATE_nop(drcontext);
    // We test these scenarios:
    // 1) Ensure that markers are delayed along with branches but timestamps and cpu
    //    headers are not delayed along with branches.
    // 2) Ensure that markers are not delayed across timestamp+cpu headers if there is no
    //    branch also being delayed.
    // 3) Ensure that markers along with branches are not delayed across window boundaries
    //    (TRACE_MARKER_TYPE_WINDOW_ID with a new id).
    instr_t *move1 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instr_t *jmp1 = XINST_CREATE_jump(drcontext, opnd_create_instr(move1));
    instr_t *move2 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instr_t *move3 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instr_t *move4 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instr_t *move5 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instr_t *jmp2 = XINST_CREATE_jump(drcontext, opnd_create_instr(move5));
    instrlist_append(ilist, nop);
    // Block 1.
    instrlist_append(ilist, move1);
    instrlist_append(ilist, jmp1);
    // Block 2.
    instrlist_append(ilist, move2);
    instrlist_append(ilist, move3);
    // Block 3.
    instrlist_append(ilist, move4);
    instrlist_append(ilist, move5);
    instrlist_append(ilist, jmp2);

    size_t offs_nop = 0;
    size_t offs_move1 = offs_nop + instr_length(drcontext, nop);
    size_t offs_jmp1 = offs_move1 + instr_length(drcontext, move1);
    size_t offs_move2 = offs_jmp1 + instr_length(drcontext, jmp1);
    size_t offs_move3 = offs_move2 + instr_length(drcontext, move2);
    size_t offs_move4 = offs_move3 + instr_length(drcontext, move3);

    // Now we synthesize our raw trace itself, including a valid header sequence.
    std::vector<offline_entry_t> raw;
    raw.push_back(make_header());
    raw.push_back(make_tid());
    raw.push_back(make_pid());
    raw.push_back(make_line_size());
    // 1: Branch at the end of this block will be delayed until the next block
    //    is found: but it should cross the timestap+cpu headers below, and carry the 3
    //    func markers with it and not pass over those.
    raw.push_back(make_block(offs_move1, 2));
    raw.push_back(make_timestamp());
    raw.push_back(make_core());
    raw.push_back(make_marker(TRACE_MARKER_TYPE_FUNC_ID, 0));
    raw.push_back(make_marker(TRACE_MARKER_TYPE_FUNC_RETADDR, 4));
    raw.push_back(make_marker(TRACE_MARKER_TYPE_FUNC_ARG, 2));
    // 2: Markers with no branch followed by timestamp+cpu headers are not
    //    delayed if there is no branch also being delayed.
    raw.push_back(make_block(offs_move2, 2));
    raw.push_back(make_marker(TRACE_MARKER_TYPE_FUNC_ID, 0));
    raw.push_back(make_marker(TRACE_MARKER_TYPE_FUNC_RETADDR, 4));
    raw.push_back(make_marker(TRACE_MARKER_TYPE_FUNC_ARG, 2));
    raw.push_back(make_timestamp());
    raw.push_back(make_core());
    // 3: Markers and branches are not delayed across window boundaries.
    raw.push_back(make_block(offs_move4, 3));
    raw.push_back(make_marker(TRACE_MARKER_TYPE_FUNC_ID, 0));
    raw.push_back(make_window_id(1));
    raw.push_back(make_exit());

    std::vector<trace_entry_t> entries;
    if (!run_raw2trace(drcontext, raw, ilist, entries))
        return false;
    int idx = 0;
    return (
        check_entry(entries, idx, TRACE_TYPE_HEADER, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD, -1) &&
        check_entry(entries, idx, TRACE_TYPE_PID, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER,
                    TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT) &&
        // Case 1.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#ifdef X86_32
        // An extra encoding entry is needed.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#endif
        check_entry(entries, idx, TRACE_TYPE_INSTR_DIRECT_JUMP, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_ID) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_RETADDR) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_ARG) &&
        // Case 2.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_ID) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_RETADDR) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_ARG) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        // Case 3.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#ifdef X86_32
        // An extra encoding entry is needed.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#endif
        check_entry(entries, idx, TRACE_TYPE_INSTR_DIRECT_JUMP, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FUNC_ID) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_WINDOW_ID) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD_EXIT, -1) &&
        check_entry(entries, idx, TRACE_TYPE_FOOTER, -1));
}

bool
test_chunk_boundaries(void *drcontext)
{
    std::cerr << "\n===============\nTesting chunk bounds\n";
    instrlist_t *ilist = instrlist_create(drcontext);
    // raw2trace doesn't like offsets of 0 so we shift with a nop.
    instr_t *nop = XINST_CREATE_nop(drcontext);
    // Test i#5724 where a chunk boundary between consecutive branches results
    // in an incorrect count.
    instr_t *move1 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instr_t *move2 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instr_t *jmp2 = XINST_CREATE_jump(drcontext, opnd_create_instr(move2));
    instr_t *jmp1 = XINST_CREATE_jump(drcontext, opnd_create_instr(jmp2));
    instr_t *move3 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instrlist_append(ilist, nop);
    // Block 1.
    instrlist_append(ilist, move1);
    instrlist_append(ilist, jmp1);
    // Block 2.
    instrlist_append(ilist, jmp2);
    // Block 3.
    instrlist_append(ilist, move2);
    instrlist_append(ilist, move3);

    size_t offs_nop = 0;
    size_t offs_move1 = offs_nop + instr_length(drcontext, nop);
    size_t offs_jmp1 = offs_move1 + instr_length(drcontext, move1);
    size_t offs_jmp2 = offs_jmp1 + instr_length(drcontext, jmp1);
    size_t offs_move2 = offs_jmp2 + instr_length(drcontext, jmp2);

    // Now we synthesize our raw trace itself, including a valid header sequence.
    std::vector<offline_entry_t> raw;
    raw.push_back(make_header());
    raw.push_back(make_tid());
    raw.push_back(make_pid());
    raw.push_back(make_line_size());
    raw.push_back(make_timestamp());
    raw.push_back(make_core());
    raw.push_back(make_block(offs_move1, 2));
    raw.push_back(make_block(offs_jmp2, 1));
    raw.push_back(make_block(offs_move2, 2));
    // TODO i#5724: Add repeats of the same instrs to test re-emitting encodings
    // in new chunks.
    raw.push_back(make_exit());

    std::vector<trace_entry_t> entries;
    // Use a chunk instr count of 2 to split the 2 jumps.
    if (!run_raw2trace(drcontext, raw, ilist, entries, 2))
        return false;
    int idx = 0;
    return (
        check_entry(entries, idx, TRACE_TYPE_HEADER, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD, -1) &&
        check_entry(entries, idx, TRACE_TYPE_PID, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER,
                    TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        // Block 1.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#ifdef X86_32
        // An extra encoding entry is needed.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#endif
        check_entry(entries, idx, TRACE_TYPE_INSTR_DIRECT_JUMP, -1) &&
        // Chunk should split the two jumps.
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_FOOTER) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RECORD_ORDINAL) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        // Block 2.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#ifdef X86_32
        // An extra encoding entry is needed.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#endif
        check_entry(entries, idx, TRACE_TYPE_INSTR_DIRECT_JUMP, -1) &&
        // Block 3.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        // Second chunk split.
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_FOOTER) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RECORD_ORDINAL) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD_EXIT, -1) &&
        check_entry(entries, idx, TRACE_TYPE_FOOTER, -1));
}

bool
test_chunk_encodings(void *drcontext)
{
    std::cerr << "\n===============\nTesting chunk encoding\n";
    instrlist_t *ilist = instrlist_create(drcontext);
    // raw2trace doesn't like offsets of 0 so we shift with a nop.
    instr_t *nop = XINST_CREATE_nop(drcontext);
    // Test i#5724 where a chunk boundary between consecutive branches results
    // in a missing encoding entry.
    instr_t *move1 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instr_t *move2 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instr_t *jmp2 = XINST_CREATE_jump(drcontext, opnd_create_instr(move2));
    instr_t *jmp1 = XINST_CREATE_jump(drcontext, opnd_create_instr(jmp2));
    instrlist_append(ilist, nop);
    // Block 1.
    instrlist_append(ilist, move1);
    instrlist_append(ilist, jmp1);
    // Block 2.
    instrlist_append(ilist, jmp2);
    // Block 3.
    instrlist_append(ilist, move2);

    size_t offs_nop = 0;
    size_t offs_move1 = offs_nop + instr_length(drcontext, nop);
    size_t offs_jmp1 = offs_move1 + instr_length(drcontext, move1);
    size_t offs_jmp2 = offs_jmp1 + instr_length(drcontext, jmp1);
    size_t offs_move2 = offs_jmp2 + instr_length(drcontext, jmp2);

    // Now we synthesize our raw trace itself, including a valid header sequence.
    std::vector<offline_entry_t> raw;
    raw.push_back(make_header());
    raw.push_back(make_tid());
    raw.push_back(make_pid());
    raw.push_back(make_line_size());
    raw.push_back(make_timestamp());
    raw.push_back(make_core());
    raw.push_back(make_block(offs_move1, 2));
    raw.push_back(make_block(offs_jmp2, 1));
    raw.push_back(make_block(offs_move2, 1));
    // Repeat the jmp,jmp to test re-emitting encodings in new chunks.
    raw.push_back(make_block(offs_move1, 2));
    raw.push_back(make_block(offs_jmp2, 1));
    raw.push_back(make_block(offs_move2, 1));
    raw.push_back(make_exit());

    std::vector<trace_entry_t> entries;
    // Use a chunk instr count of 6 to split the 2nd set of 2 jumps.
    if (!run_raw2trace(drcontext, raw, ilist, entries, 6))
        return false;
    int idx = 0;
    return (
        check_entry(entries, idx, TRACE_TYPE_HEADER, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD, -1) &&
        check_entry(entries, idx, TRACE_TYPE_PID, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER,
                    TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        // Block 1.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#ifdef X86_32
        // An extra encoding entry is needed.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#endif
        check_entry(entries, idx, TRACE_TYPE_INSTR_DIRECT_JUMP, -1) &&
        // Block 2.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#ifdef X86_32
        // An extra encoding entry is needed.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#endif
        check_entry(entries, idx, TRACE_TYPE_INSTR_DIRECT_JUMP, -1) &&
        // Block 3.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        // Now we have repeated instrs which do not need encodings, except in new chunks.
        // Block 1.
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR_DIRECT_JUMP, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_FOOTER) &&
        // Chunk splits pair of jumps.
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RECORD_ORDINAL) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        // Block 2.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#ifdef X86_32
        // An extra encoding entry is needed.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#endif
        check_entry(entries, idx, TRACE_TYPE_INSTR_DIRECT_JUMP, -1) &&
        // Block 3.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD_EXIT, -1) &&
        check_entry(entries, idx, TRACE_TYPE_FOOTER, -1));
}

bool
test_duplicate_syscalls(void *drcontext)
{
    std::cerr << "\n===============\nTesting dup syscalls\n";
    // Our synthetic test first constructs a list of instructions to be encoded into
    // a buffer for decoding by raw2trace.
    instrlist_t *ilist = instrlist_create(drcontext);
    // raw2trace doesn't like offsets of 0 so we shift with a nop.
    instr_t *nop = XINST_CREATE_nop(drcontext);
    instr_t *move1 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
    // XXX: Adding an XINST_CREATE_syscall macro will simplify this but there are
    // complexities (xref create_syscall_instr()).
#ifdef X86
    instr_t *sys = INSTR_CREATE_syscall(drcontext);
#elif defined(AARCHXX)
    instr_t *sys = INSTR_CREATE_svc(drcontext, opnd_create_immed_int((sbyte)0x0, OPSZ_1));
#elif defined(RISCV64)
    instr_t *sys = INSTR_CREATE_ecall(drcontext);
#else
#    error Unsupported architecture.
#endif
    instr_t *move2 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG2), opnd_create_reg(REG1));
    instrlist_append(ilist, nop);
    instrlist_append(ilist, move1);
    instrlist_append(ilist, sys);
    instrlist_append(ilist, move2);
    size_t offs_nop = 0;
    size_t offs_move1 = offs_nop + instr_length(drcontext, nop);
    size_t offs_sys = offs_move1 + instr_length(drcontext, move1);
    size_t offs_move2 = offs_sys + instr_length(drcontext, sys);

    // Now we synthesize our raw trace itself, including a valid header sequence.
    static constexpr int SYSCALL_NUM = 42; // Doesn't really matter.
    std::vector<offline_entry_t> raw;
    raw.push_back(make_header());
    raw.push_back(make_tid());
    raw.push_back(make_pid());
    raw.push_back(make_line_size());
    raw.push_back(make_timestamp(1));
    raw.push_back(make_core());
    raw.push_back(make_block(offs_move1, 2));
    raw.push_back(make_marker(TRACE_MARKER_TYPE_SYSCALL, SYSCALL_NUM));
    raw.push_back(make_timestamp(2));
    raw.push_back(make_core());
    // Repeat the syscall that was the second instr in the size-2 block above,
    // in its own separate block. This is the signature of the duplicate
    // system call invariant error seen in i#5934.
    raw.push_back(make_block(offs_sys, 1));
    // New traces have a syscall marker, of which we test removal.
    raw.push_back(make_timestamp(3));
    raw.push_back(make_core());
    raw.push_back(make_marker(TRACE_MARKER_TYPE_SYSCALL, SYSCALL_NUM));
    raw.push_back(make_timestamp(4));
    raw.push_back(make_core());
    raw.push_back(make_block(offs_move2, 1));
    raw.push_back(make_exit());

    std::vector<trace_entry_t> entries;
    if (!run_raw2trace(drcontext, raw, ilist, entries))
        return false;
    int idx = 0;
    return (
        check_entry(entries, idx, TRACE_TYPE_HEADER, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD, -1) &&
        check_entry(entries, idx, TRACE_TYPE_PID, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER,
                    TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, 1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        // The move1 instr.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        // The sys instr.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_SYSCALL,
                    SYSCALL_NUM) &&
        // Prev block ends.
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, 2) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        // No duplicate sys instr, and the following timestamp==3 and syscall marker
        // are removed.
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, 4) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        // The move2 instr.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD_EXIT, -1) &&
        check_entry(entries, idx, TRACE_TYPE_FOOTER, -1));
}

bool
test_false_syscalls(void *drcontext)
{
    std::cerr << "\n===============\nTesting false syscalls\n";
    // Our synthetic test first constructs a list of instructions to be encoded into
    // a buffer for decoding by raw2trace.
    instrlist_t *ilist = instrlist_create(drcontext);
    // raw2trace doesn't like offsets of 0 so we shift with a nop.
    instr_t *nop = XINST_CREATE_nop(drcontext);
    instr_t *move1 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
    // XXX: Adding an XINST_CREATE_syscall macro will simplify this but there are
    // complexities (xref create_syscall_instr()).
#ifdef X86
    instr_t *sys = INSTR_CREATE_syscall(drcontext);
#elif defined(AARCHXX)
    instr_t *sys = INSTR_CREATE_svc(drcontext, opnd_create_immed_int((sbyte)0x0, OPSZ_1));
#elif defined(RISCV64)
    instr_t *sys = INSTR_CREATE_ecall(drcontext);
#else
#    error Unsupported architecture.
#endif
    instr_t *move2 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG2), opnd_create_reg(REG1));
    instrlist_append(ilist, nop);
    instrlist_append(ilist, move1);
    instrlist_append(ilist, sys);
    instrlist_append(ilist, move2);
    size_t offs_nop = 0;
    size_t offs_move1 = offs_nop + instr_length(drcontext, nop);
    size_t offs_sys = offs_move1 + instr_length(drcontext, move1);
    size_t offs_move2 = offs_sys + instr_length(drcontext, sys);

    // Now we synthesize our raw trace itself, including a valid header sequence.
    static constexpr int SYSCALL_NUM = 42; // Doesn't really matter.
    std::vector<offline_entry_t> raw;
    raw.push_back(make_header());
    raw.push_back(make_tid());
    raw.push_back(make_pid());
    raw.push_back(make_line_size());
    raw.push_back(make_timestamp(1));
    raw.push_back(make_core());
    raw.push_back(make_block(offs_move1, 2));
    // There is no syscall marker here, so the syscall should be removed.
    raw.push_back(make_timestamp(2));
    raw.push_back(make_core());
    // Repeat the syscall but with a marker this time.
    // This should not trigger dup-syscall removal.
    raw.push_back(make_block(offs_sys, 1));
    raw.push_back(make_marker(TRACE_MARKER_TYPE_SYSCALL, SYSCALL_NUM));
    // Test a syscall with a marker across headers.
    raw.push_back(make_block(offs_move1, 2));
    raw.push_back(make_timestamp(3));
    raw.push_back(make_core());
    raw.push_back(make_marker(TRACE_MARKER_TYPE_SYSCALL, SYSCALL_NUM));
    raw.push_back(make_timestamp(4));
    raw.push_back(make_core());
    raw.push_back(make_block(offs_move2, 1));
    raw.push_back(make_exit());

    std::vector<trace_entry_t> entries;
    if (!run_raw2trace(drcontext, raw, ilist, entries))
        return false;
    int idx = 0;
    return (
        check_entry(entries, idx, TRACE_TYPE_HEADER, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD, -1) &&
        check_entry(entries, idx, TRACE_TYPE_PID, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER,
                    TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, 1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        // The move1 instr.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        // The sys instr was removed!
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, 2) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        // A sys instr that was not removed.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_SYSCALL,
                    SYSCALL_NUM) &&
        // The move1 instr, with no encoding (2nd occurrence).
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        // A sys instr that was not removed, with no encoding (2nd occurrence).
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, 3) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_SYSCALL,
                    SYSCALL_NUM) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, 4) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        // The move2 instr.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD_EXIT, -1) &&
        check_entry(entries, idx, TRACE_TYPE_FOOTER, -1));
}

bool
test_rseq_fallthrough(void *drcontext)
{
    std::cerr << "\n===============\nTesting rseq fallthrough\n";
    instrlist_t *ilist = instrlist_create(drcontext);
    // raw2trace doesn't like offsets of 0 so we shift with a nop.
    instr_t *nop = XINST_CREATE_nop(drcontext);
    instr_t *move1 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instr_t *store =
        XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(REG2, 0), opnd_create_reg(REG1));
    instr_t *move2 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG2), opnd_create_reg(REG1));
    instrlist_append(ilist, nop);
    instrlist_append(ilist, move1);
    instrlist_append(ilist, store);
    instrlist_append(ilist, move2);
    size_t offs_nop = 0;
    size_t offs_move1 = offs_nop + instr_length(drcontext, nop);
    size_t offs_store = offs_move1 + instr_length(drcontext, move1);
    size_t offs_move2 = offs_store + instr_length(drcontext, store);

    std::vector<offline_entry_t> raw;
    raw.push_back(make_header());
    raw.push_back(make_tid());
    raw.push_back(make_pid());
    raw.push_back(make_line_size());
    raw.push_back(make_timestamp());
    raw.push_back(make_core());
    raw.push_back(make_marker(TRACE_MARKER_TYPE_RSEQ_ENTRY, offs_move2));
    // The end of our rseq sequence, ending in a committing store.
    raw.push_back(make_block(offs_move1, 2));
    raw.push_back(make_memref(42));
    // No abort or side exit: we just fall through.
    raw.push_back(make_block(offs_move2, 1));
    raw.push_back(make_exit());

    std::vector<trace_entry_t> entries;
    if (!run_raw2trace(drcontext, raw, ilist, entries))
        return false;
    int idx = 0;
    return (
        check_entry(entries, idx, TRACE_TYPE_HEADER, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD, -1) &&
        check_entry(entries, idx, TRACE_TYPE_PID, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER,
                    TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RSEQ_ENTRY) &&
        // The move1 instr.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_move1) &&
        // The committing store.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_store) &&
        check_entry(entries, idx, TRACE_TYPE_WRITE, -1) &&
        // The move2 instr.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_move2) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD_EXIT, -1) &&
        check_entry(entries, idx, TRACE_TYPE_FOOTER, -1));
}

/* Tests rseq rollback without the new entry marker. */
bool
test_rseq_rollback_legacy(void *drcontext)
{
    std::cerr << "\n===============\nTesting legacy rseq rollback\n";
    instrlist_t *ilist = instrlist_create(drcontext);
    // raw2trace doesn't like offsets of 0 so we shift with a nop.
    instr_t *nop = XINST_CREATE_nop(drcontext);
    instr_t *move1 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instr_t *store =
        XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(REG2, 0), opnd_create_reg(REG1));
    instr_t *move2 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG2), opnd_create_reg(REG1));
    instrlist_append(ilist, nop);
    instrlist_append(ilist, move1);
    instrlist_append(ilist, store);
    instrlist_append(ilist, move2);
    size_t offs_nop = 0;
    size_t offs_move1 = offs_nop + instr_length(drcontext, nop);
    size_t offs_store = offs_move1 + instr_length(drcontext, move1);
    size_t offs_move2 = offs_store + instr_length(drcontext, store);

    std::vector<offline_entry_t> raw;
    raw.push_back(make_header());
    raw.push_back(make_tid());
    raw.push_back(make_pid());
    raw.push_back(make_line_size());
    raw.push_back(make_timestamp());
    raw.push_back(make_core());
    // The end of our rseq sequence, ending in a committing store.
    raw.push_back(make_block(offs_move1, 2));
    raw.push_back(make_memref(42));
    raw.push_back(make_marker(TRACE_MARKER_TYPE_RSEQ_ABORT, offs_store));
    raw.push_back(make_marker(TRACE_MARKER_TYPE_KERNEL_EVENT, offs_store));
    raw.push_back(make_block(offs_move2, 1));
    raw.push_back(make_exit());

    std::vector<trace_entry_t> entries;
    if (!run_raw2trace(drcontext, raw, ilist, entries))
        return false;
    int idx = 0;
    return (
        check_entry(entries, idx, TRACE_TYPE_HEADER, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD, -1) &&
        check_entry(entries, idx, TRACE_TYPE_PID, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER,
                    TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        // The move1 instr.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_move1) &&
        // The committing store should not be here.
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RSEQ_ABORT) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_KERNEL_EVENT) &&
        // The move2 instr.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_move2) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD_EXIT, -1) &&
        check_entry(entries, idx, TRACE_TYPE_FOOTER, -1));
}

bool
test_rseq_rollback(void *drcontext)
{
    std::cerr << "\n===============\nTesting rseq rollback\n";
    instrlist_t *ilist = instrlist_create(drcontext);
    // raw2trace doesn't like offsets of 0 so we shift with a nop.
    instr_t *nop = XINST_CREATE_nop(drcontext);
    instr_t *move1 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instr_t *store =
        XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(REG2, 0), opnd_create_reg(REG1));
    instr_t *move2 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG2), opnd_create_reg(REG1));
    instrlist_append(ilist, nop);
    instrlist_append(ilist, move1);
    instrlist_append(ilist, store);
    instrlist_append(ilist, move2);
    size_t offs_nop = 0;
    size_t offs_move1 = offs_nop + instr_length(drcontext, nop);
    size_t offs_store = offs_move1 + instr_length(drcontext, move1);
    size_t offs_move2 = offs_store + instr_length(drcontext, store);

    std::vector<offline_entry_t> raw;
    raw.push_back(make_header());
    raw.push_back(make_tid());
    raw.push_back(make_pid());
    raw.push_back(make_line_size());
    raw.push_back(make_timestamp());
    raw.push_back(make_core());
    raw.push_back(make_marker(TRACE_MARKER_TYPE_RSEQ_ENTRY, offs_move2));
    // The end of our rseq sequence, ending in a committing store.
    raw.push_back(make_block(offs_move1, 2));
    raw.push_back(make_memref(42));
    raw.push_back(make_marker(TRACE_MARKER_TYPE_RSEQ_ABORT, offs_move2));
    raw.push_back(make_marker(TRACE_MARKER_TYPE_KERNEL_EVENT, offs_move2));
    raw.push_back(make_block(offs_move2, 1));
    raw.push_back(make_exit());

    std::vector<trace_entry_t> entries;
    if (!run_raw2trace(drcontext, raw, ilist, entries))
        return false;
    int idx = 0;
    return (
        check_entry(entries, idx, TRACE_TYPE_HEADER, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD, -1) &&
        check_entry(entries, idx, TRACE_TYPE_PID, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER,
                    TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RSEQ_ENTRY) &&
        // The move1 instr.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_move1) &&
        // The committing store should not be here.
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RSEQ_ABORT) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_KERNEL_EVENT) &&
        // The move2 instr.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_move2) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD_EXIT, -1) &&
        check_entry(entries, idx, TRACE_TYPE_FOOTER, -1));
}

/* Tests i#5954 where a timestamp precedes the abort marker. */
bool
test_rseq_rollback_with_timestamps(void *drcontext)
{
    std::cerr << "\n===============\nTesting rseq rollback with timestamps\n";
    instrlist_t *ilist = instrlist_create(drcontext);
    // raw2trace doesn't like offsets of 0 so we shift with a nop.
    instr_t *nop = XINST_CREATE_nop(drcontext);
    instr_t *move1 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instr_t *store =
        XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(REG2, 0), opnd_create_reg(REG1));
    instr_t *move2 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG2), opnd_create_reg(REG1));
    instrlist_append(ilist, nop);
    instrlist_append(ilist, move1);
    instrlist_append(ilist, store);
    instrlist_append(ilist, move2);
    size_t offs_nop = 0;
    size_t offs_move1 = offs_nop + instr_length(drcontext, nop);
    size_t offs_store = offs_move1 + instr_length(drcontext, move1);
    size_t offs_move2 = offs_store + instr_length(drcontext, store);

    std::vector<offline_entry_t> raw;
    raw.push_back(make_header());
    raw.push_back(make_tid());
    raw.push_back(make_pid());
    raw.push_back(make_line_size());
    raw.push_back(make_timestamp());
    raw.push_back(make_core());
    raw.push_back(make_marker(TRACE_MARKER_TYPE_RSEQ_ENTRY, offs_move2));
    // The end of our rseq sequence, ending in a committing store.
    raw.push_back(make_block(offs_move1, 2));
    raw.push_back(make_memref(42));
    raw.push_back(make_timestamp());
    raw.push_back(make_core());
    raw.push_back(make_marker(TRACE_MARKER_TYPE_RSEQ_ABORT, offs_move2));
    raw.push_back(make_marker(TRACE_MARKER_TYPE_KERNEL_EVENT, offs_move2));
    raw.push_back(make_block(offs_move2, 1));
    raw.push_back(make_exit());

    std::vector<trace_entry_t> entries;
    if (!run_raw2trace(drcontext, raw, ilist, entries))
        return false;
    int idx = 0;
    return (
        check_entry(entries, idx, TRACE_TYPE_HEADER, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD, -1) &&
        check_entry(entries, idx, TRACE_TYPE_PID, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER,
                    TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RSEQ_ENTRY) &&
        // The move1 instr.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_move1) &&
        // The committing store should not be here.
        // The timestamp+cpuid also get removed in case the prior instr is a branch.
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RSEQ_ABORT) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_KERNEL_EVENT) &&
        // The move2 instr.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_move2) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD_EXIT, -1) &&
        check_entry(entries, idx, TRACE_TYPE_FOOTER, -1));
}

/* Tests a rollback combined with a signal for the fault that caused the abort. */
bool
test_rseq_rollback_with_signal(void *drcontext)
{
    std::cerr << "\n===============\nTesting rseq rollback with signal\n";
    instrlist_t *ilist = instrlist_create(drcontext);
    // raw2trace doesn't like offsets of 0 so we shift with a nop.
    instr_t *nop = XINST_CREATE_nop(drcontext);
    instr_t *move1 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instr_t *store =
        XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(REG2, 0), opnd_create_reg(REG1));
    instr_t *move2 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG2), opnd_create_reg(REG1));
    instrlist_append(ilist, nop);
    instrlist_append(ilist, move1);
    instrlist_append(ilist, store);
    instrlist_append(ilist, move2);
    size_t offs_nop = 0;
    size_t offs_move1 = offs_nop + instr_length(drcontext, nop);
    size_t offs_store = offs_move1 + instr_length(drcontext, move1);
    size_t offs_move2 = offs_store + instr_length(drcontext, store);
    size_t offs_end = offs_move2 + instr_length(drcontext, move2);

    std::vector<offline_entry_t> raw;
    raw.push_back(make_header());
    raw.push_back(make_tid());
    raw.push_back(make_pid());
    raw.push_back(make_line_size());
    raw.push_back(make_timestamp());
    raw.push_back(make_core());
    raw.push_back(make_marker(TRACE_MARKER_TYPE_RSEQ_ENTRY, offs_move2));
    // The end of our rseq sequence, ending in a committing store.
    raw.push_back(make_block(offs_move1, 2));
    raw.push_back(make_memref(42));
    // The abort is after the revert-and-re-fix of i#4041 where the marker value
    // is the handler PC and not the committing store.
    raw.push_back(make_marker(TRACE_MARKER_TYPE_RSEQ_ABORT, offs_end));
    raw.push_back(make_marker(TRACE_MARKER_TYPE_KERNEL_EVENT, offs_end));
    raw.push_back(make_timestamp());
    raw.push_back(make_core());
    raw.push_back(make_marker(TRACE_MARKER_TYPE_KERNEL_EVENT, offs_end));
    raw.push_back(make_block(offs_move2, 1));
    raw.push_back(make_exit());

    std::vector<trace_entry_t> entries;
    if (!run_raw2trace(drcontext, raw, ilist, entries))
        return false;
    int idx = 0;
    return (
        check_entry(entries, idx, TRACE_TYPE_HEADER, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD, -1) &&
        check_entry(entries, idx, TRACE_TYPE_PID, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER,
                    TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RSEQ_ENTRY) &&
        // The move1 instr.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_move1) &&
        // The committing store should not be here.
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RSEQ_ABORT) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_KERNEL_EVENT) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_KERNEL_EVENT) &&
        // The move2 instr.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_move2) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD_EXIT, -1) &&
        check_entry(entries, idx, TRACE_TYPE_FOOTER, -1));
}

/* Tests rollback i#5954 where a chunk boundary splits an rseq region. */
bool
test_rseq_rollback_with_chunks(void *drcontext)
{
    std::cerr << "\n===============\nTesting rseq rollback with chunks\n";
    instrlist_t *ilist = instrlist_create(drcontext);
    // raw2trace doesn't like offsets of 0 so we shift with a nop.
    instr_t *nop = XINST_CREATE_nop(drcontext);
    instr_t *move1 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instr_t *store =
        XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(REG2, 0), opnd_create_reg(REG1));
    instr_t *move2 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG2), opnd_create_reg(REG1));
    instrlist_append(ilist, nop);
    instrlist_append(ilist, move1);
    instrlist_append(ilist, store);
    instrlist_append(ilist, move2);
    size_t offs_nop = 0;
    size_t offs_move1 = offs_nop + instr_length(drcontext, nop);
    size_t offs_store = offs_move1 + instr_length(drcontext, move1);
    size_t offs_move2 = offs_store + instr_length(drcontext, store);

    std::vector<offline_entry_t> raw;
    raw.push_back(make_header());
    raw.push_back(make_tid());
    raw.push_back(make_pid());
    raw.push_back(make_line_size());
    raw.push_back(make_timestamp());
    raw.push_back(make_core());
    // One completed rseq region to cache encodings.
    raw.push_back(make_marker(TRACE_MARKER_TYPE_RSEQ_ENTRY, offs_move2));
    raw.push_back(make_block(offs_move1, 2));
    raw.push_back(make_memref(42));
    raw.push_back(make_block(offs_move2, 1));
    // A second one which should not need encodings.
    raw.push_back(make_marker(TRACE_MARKER_TYPE_RSEQ_ENTRY, offs_move2));
    raw.push_back(make_block(offs_move1, 2));
    raw.push_back(make_memref(42));
    raw.push_back(make_block(offs_move2, 1));
    // Now a third split by a chunk boundary.
    raw.push_back(make_marker(TRACE_MARKER_TYPE_RSEQ_ENTRY, offs_move2));
    raw.push_back(make_block(offs_move1, 2));
    raw.push_back(make_memref(42));
    raw.push_back(make_marker(TRACE_MARKER_TYPE_RSEQ_ABORT, offs_move2));
    raw.push_back(make_marker(TRACE_MARKER_TYPE_KERNEL_EVENT, offs_move2));
    raw.push_back(make_block(offs_move2, 1));
    raw.push_back(make_exit());

    std::vector<trace_entry_t> entries;
    // 6 instrs puts a new chunk at the start of the 3rd region.
    if (!run_raw2trace(drcontext, raw, ilist, entries, 6))
        return false;
    int idx = 0;
    return (
        check_entry(entries, idx, TRACE_TYPE_HEADER, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD, -1) &&
        check_entry(entries, idx, TRACE_TYPE_PID, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER,
                    TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        // First sequence, with encodings.
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RSEQ_ENTRY) &&
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_move1) &&
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_store) &&
        check_entry(entries, idx, TRACE_TYPE_WRITE, -1) &&
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_move2) &&
        // Second sequence, without encodings.
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RSEQ_ENTRY) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_move1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_store) &&
        check_entry(entries, idx, TRACE_TYPE_WRITE, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_move2) &&
        // Third aborted sequence in new chunk with encodings.
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CHUNK_FOOTER) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RECORD_ORDINAL) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RSEQ_ENTRY) &&
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_move1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RSEQ_ABORT) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_KERNEL_EVENT) &&
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_move2) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD_EXIT, -1) &&
        check_entry(entries, idx, TRACE_TYPE_FOOTER, -1));
}

/* Tests a typical rseq side exit (i#5953).
 *
 * XXX: We could test even more variants, like having multiple potential
 * exits.
 */
bool
test_rseq_side_exit(void *drcontext)
{
    std::cerr << "\n===============\nTesting rseq side exit\n";
    instrlist_t *ilist = instrlist_create(drcontext);
    // raw2trace doesn't like offsets of 0 so we shift with a nop.
    instr_t *nop = XINST_CREATE_nop(drcontext);
    instr_t *move3 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG2), opnd_create_reg(REG1));
    instr_t *move1 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instr_t *jcc =
        XINST_CREATE_jump_cond(drcontext, DR_PRED_EQ, opnd_create_instr(move3));
    instr_t *store =
        XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(REG2, 0), opnd_create_reg(REG1));
    instr_t *move2 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG2), opnd_create_reg(REG1));
    instrlist_append(ilist, nop);
    instrlist_append(ilist, jcc);
    instrlist_append(ilist, move1);
    instrlist_append(ilist, store);
    instrlist_append(ilist, move2);
    instrlist_append(ilist, move3);
    size_t offs_nop = 0;
    size_t offs_jcc = offs_nop + instr_length(drcontext, nop);
    size_t offs_move1 = offs_jcc + instr_length(drcontext, jcc);
    size_t offs_store = offs_move1 + instr_length(drcontext, move1);
    size_t offs_move2 = offs_store + instr_length(drcontext, store);
    size_t offs_move3 = offs_move2 + instr_length(drcontext, move2);

    std::vector<offline_entry_t> raw;
    raw.push_back(make_header());
    raw.push_back(make_tid());
    raw.push_back(make_pid());
    raw.push_back(make_line_size());
    raw.push_back(make_timestamp());
    raw.push_back(make_core());
    raw.push_back(make_marker(TRACE_MARKER_TYPE_RSEQ_ENTRY, offs_move2));
    // Side exit is here; not taken in instrumented execution.
    raw.push_back(make_block(offs_jcc, 1));
    // The end of our rseq sequence, ending in a committing store.
    raw.push_back(make_block(offs_move1, 2));
    raw.push_back(make_memref(42));
    // A discontinuity as we continue with the side exit target.
    raw.push_back(make_block(offs_move3, 1));
    raw.push_back(make_exit());

    std::vector<trace_entry_t> entries;
    if (!run_raw2trace(drcontext, raw, ilist, entries))
        return false;
    int idx = 0;
    return (
        check_entry(entries, idx, TRACE_TYPE_HEADER, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD, -1) &&
        check_entry(entries, idx, TRACE_TYPE_PID, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER,
                    TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RSEQ_ENTRY) &&
        // The jcc instr.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#ifdef X86_32
        // An extra encoding entry is needed.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#endif
        check_entry(entries, idx, TRACE_TYPE_INSTR_TAKEN_JUMP, -1, offs_jcc) &&
        // The move2 + committing store should be gone.
        // We should go straight to the move3 instr.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_move3) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD_EXIT, -1) &&
        check_entry(entries, idx, TRACE_TYPE_FOOTER, -1));
}

/* Tests an rseq side exit with an arriving signal (i#5953). */
bool
test_rseq_side_exit_signal(void *drcontext)
{
    std::cerr << "\n===============\nTesting rseq side exit with signal\n";
    instrlist_t *ilist = instrlist_create(drcontext);
    // raw2trace doesn't like offsets of 0 so we shift with a nop.
    instr_t *nop = XINST_CREATE_nop(drcontext);
    instr_t *move3 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG2), opnd_create_reg(REG1));
    instr_t *move1 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instr_t *jcc =
        XINST_CREATE_jump_cond(drcontext, DR_PRED_EQ, opnd_create_instr(move3));
    instr_t *store =
        XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(REG2, 0), opnd_create_reg(REG1));
    instr_t *move2 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG2), opnd_create_reg(REG1));
    instrlist_append(ilist, nop);
    instrlist_append(ilist, jcc);
    instrlist_append(ilist, move1);
    instrlist_append(ilist, store);
    instrlist_append(ilist, move2);
    instrlist_append(ilist, move3);
    size_t offs_nop = 0;
    size_t offs_jcc = offs_nop + instr_length(drcontext, nop);
    size_t offs_move1 = offs_jcc + instr_length(drcontext, jcc);
    size_t offs_store = offs_move1 + instr_length(drcontext, move1);
    size_t offs_move2 = offs_store + instr_length(drcontext, store);
    size_t offs_move3 = offs_move2 + instr_length(drcontext, move2);

    std::vector<offline_entry_t> raw;
    raw.push_back(make_header());
    raw.push_back(make_tid());
    raw.push_back(make_pid());
    raw.push_back(make_line_size());
    raw.push_back(make_timestamp());
    raw.push_back(make_core());
    raw.push_back(make_marker(TRACE_MARKER_TYPE_RSEQ_ENTRY, offs_move2));
    // Side exit is here; not taken in instrumented execution.
    raw.push_back(make_block(offs_jcc, 1));
    // The end of our rseq sequence, ending in a committing store.
    raw.push_back(make_block(offs_move1, 2));
    raw.push_back(make_memref(42));
    // A discontinuity as we continue with the side exit target.
    // But, a signal arrived (whose interruption must be that target).
    raw.push_back(make_marker(TRACE_MARKER_TYPE_KERNEL_EVENT, offs_move3));
    raw.push_back(make_block(offs_move1, 1));
    raw.push_back(make_marker(TRACE_MARKER_TYPE_KERNEL_XFER, offs_store));
    raw.push_back(make_block(offs_move3, 1));
    raw.push_back(make_exit());

    std::vector<trace_entry_t> entries;
    if (!run_raw2trace(drcontext, raw, ilist, entries))
        return false;
    int idx = 0;
    return (
        check_entry(entries, idx, TRACE_TYPE_HEADER, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD, -1) &&
        check_entry(entries, idx, TRACE_TYPE_PID, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER,
                    TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RSEQ_ENTRY) &&
        // The jcc instr.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#ifdef X86_32
        // An extra encoding entry is needed.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#endif
        check_entry(entries, idx, TRACE_TYPE_INSTR_TAKEN_JUMP, -1, offs_jcc) &&
        // The move2 + committing store should be gone.
        // We should go straight to the signal and then the move3 instr.
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_KERNEL_EVENT) &&
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_move1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_KERNEL_XFER) &&
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_move3) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD_EXIT, -1) &&
        check_entry(entries, idx, TRACE_TYPE_FOOTER, -1));
}

/* Tests an inverted rseq side exit (i#5953). */
bool
test_rseq_side_exit_inverted(void *drcontext)
{
    std::cerr << "\n===============\nTesting inverted rseq side exit\n";
    instrlist_t *ilist = instrlist_create(drcontext);
    // raw2trace doesn't like offsets of 0 so we shift with a nop.
    instr_t *nop = XINST_CREATE_nop(drcontext);
    instr_t *move1 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instr_t *move3 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG2), opnd_create_reg(REG1));
    // Our conditional jumps over the jump which is the exit.
    instr_t *jcc =
        XINST_CREATE_jump_cond(drcontext, DR_PRED_EQ, opnd_create_instr(move1));
    instr_t *jmp = XINST_CREATE_jump(drcontext, opnd_create_instr(move3));
    instr_t *store =
        XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(REG2, 0), opnd_create_reg(REG1));
    instr_t *move2 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG2), opnd_create_reg(REG1));
    instrlist_append(ilist, nop);
    instrlist_append(ilist, jcc);
    instrlist_append(ilist, jmp);
    instrlist_append(ilist, move1);
    instrlist_append(ilist, store);
    instrlist_append(ilist, move2);
    instrlist_append(ilist, move3);
    size_t offs_nop = 0;
    size_t offs_jcc = offs_nop + instr_length(drcontext, nop);
    size_t offs_jmp = offs_jcc + instr_length(drcontext, jcc);
    size_t offs_move1 = offs_jmp + instr_length(drcontext, jmp);
    size_t offs_store = offs_move1 + instr_length(drcontext, move1);
    size_t offs_move2 = offs_store + instr_length(drcontext, store);
    size_t offs_move3 = offs_move2 + instr_length(drcontext, move2);

    std::vector<offline_entry_t> raw;
    raw.push_back(make_header());
    raw.push_back(make_tid());
    raw.push_back(make_pid());
    raw.push_back(make_line_size());
    raw.push_back(make_timestamp());
    raw.push_back(make_core());
    raw.push_back(make_marker(TRACE_MARKER_TYPE_RSEQ_ENTRY, offs_move2));
    // The jcc is taken and we don't see the side exit in instrumented execution.
    raw.push_back(make_block(offs_jcc, 1));
    // The end of our rseq sequence, ending in a committing store.
    raw.push_back(make_block(offs_move1, 2));
    raw.push_back(make_memref(42));
    // A discontinuity as we continue with the side exit target.
    raw.push_back(make_block(offs_move3, 1));
    raw.push_back(make_exit());

    std::vector<trace_entry_t> entries;
    if (!run_raw2trace(drcontext, raw, ilist, entries))
        return false;
    int idx = 0;
    return (
        check_entry(entries, idx, TRACE_TYPE_HEADER, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD, -1) &&
        check_entry(entries, idx, TRACE_TYPE_PID, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER,
                    TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RSEQ_ENTRY) &&
        // The jcc instr.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#ifdef X86_32
        // An extra encoding entry is needed.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#endif
        check_entry(entries, idx, TRACE_TYPE_INSTR_UNTAKEN_JUMP, -1, offs_jcc) &&
        // The jmp which raw2trace has to synthesize.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#ifdef X86_32
        // An extra encoding entry is needed.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#endif
        check_entry(entries, idx, TRACE_TYPE_INSTR_DIRECT_JUMP, -1, offs_jmp) &&
        // The move2 + committing store should be gone.
        // We should go straight to the move3 instr.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_move3) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD_EXIT, -1) &&
        check_entry(entries, idx, TRACE_TYPE_FOOTER, -1));
}

/* Tests an inverted rseq side exit with a timestamp (i#5986). */
bool
test_rseq_side_exit_inverted_with_timestamp(void *drcontext)
{
    std::cerr << "\n===============\nTesting inverted rseq side exit with timestamp\n";
    instrlist_t *ilist = instrlist_create(drcontext);
    // raw2trace doesn't like offsets of 0 so we shift with a nop.
    instr_t *nop = XINST_CREATE_nop(drcontext);
    instr_t *move1 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
    instr_t *move3 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG2), opnd_create_reg(REG1));
    // Our conditional jumps over the jump which is the exit.
    instr_t *jcc =
        XINST_CREATE_jump_cond(drcontext, DR_PRED_EQ, opnd_create_instr(move1));
    instr_t *jmp = XINST_CREATE_jump(drcontext, opnd_create_instr(move3));
    instr_t *store =
        XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(REG2, 0), opnd_create_reg(REG1));
    instr_t *move2 =
        XINST_CREATE_move(drcontext, opnd_create_reg(REG2), opnd_create_reg(REG1));
    instrlist_append(ilist, nop);
    instrlist_append(ilist, jcc);
    instrlist_append(ilist, jmp);
    instrlist_append(ilist, move1);
    instrlist_append(ilist, store);
    instrlist_append(ilist, move2);
    instrlist_append(ilist, move3);
    size_t offs_nop = 0;
    size_t offs_jcc = offs_nop + instr_length(drcontext, nop);
    size_t offs_jmp = offs_jcc + instr_length(drcontext, jcc);
    size_t offs_move1 = offs_jmp + instr_length(drcontext, jmp);
    size_t offs_store = offs_move1 + instr_length(drcontext, move1);
    size_t offs_move2 = offs_store + instr_length(drcontext, store);
    size_t offs_move3 = offs_move2 + instr_length(drcontext, move2);

    std::vector<offline_entry_t> raw;
    raw.push_back(make_header());
    raw.push_back(make_tid());
    raw.push_back(make_pid());
    raw.push_back(make_line_size());
    raw.push_back(make_timestamp());
    raw.push_back(make_core());
    raw.push_back(make_marker(TRACE_MARKER_TYPE_RSEQ_ENTRY, offs_move2));
    // The jcc is taken and we don't see the side exit in instrumented execution.
    raw.push_back(make_block(offs_jcc, 1));
    // The end of our rseq sequence, ending in a committing store.
    raw.push_back(make_block(offs_move1, 2));
    raw.push_back(make_memref(42));
    // A timestamp is added after the store due to filling our buffer.
    raw.push_back(make_timestamp());
    raw.push_back(make_core());
    // A discontinuity as we continue with the side exit target.
    raw.push_back(make_block(offs_move3, 1));
    // Test a completed rseq to ensure we add encodings to move1+store.
    raw.push_back(make_marker(TRACE_MARKER_TYPE_RSEQ_ENTRY, offs_move2));
    raw.push_back(make_block(offs_jcc, 1));
    raw.push_back(make_block(offs_move1, 2));
    raw.push_back(make_memref(42));
    raw.push_back(make_block(offs_move2, 1));
    raw.push_back(make_exit());

    std::vector<trace_entry_t> entries;
    if (!run_raw2trace(drcontext, raw, ilist, entries))
        return false;
    int idx = 0;
    return (
        check_entry(entries, idx, TRACE_TYPE_HEADER, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD, -1) &&
        check_entry(entries, idx, TRACE_TYPE_PID, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER,
                    TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RSEQ_ENTRY) &&
        // The jcc instr.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#ifdef X86_32
        // An extra encoding entry is needed.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#endif
        check_entry(entries, idx, TRACE_TYPE_INSTR_UNTAKEN_JUMP, -1, offs_jcc) &&
        // The jmp which raw2trace has to synthesize.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#ifdef X86_32
        // An extra encoding entry is needed.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#endif
        check_entry(entries, idx, TRACE_TYPE_INSTR_DIRECT_JUMP, -1, offs_jmp) &&
        // The move1 + committing store should be gone.
        // The timestamp+cpu should be rolled back along with the instructions.
        // We should go straight to the move3 instr.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_move3) &&
        // Our completed rseq execution should have encodings for move1+store.
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RSEQ_ENTRY) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR_TAKEN_JUMP, -1, offs_jcc) &&
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_move1) &&
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_store) &&
        check_entry(entries, idx, TRACE_TYPE_WRITE, -1) &&
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_move2) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD_EXIT, -1) &&
        check_entry(entries, idx, TRACE_TYPE_FOOTER, -1));
}

/* Tests pre-OFFLINE_FILE_VERSION_XFER_ABS_PC (module offset) handling. */
bool
test_xfer_modoffs(void *drcontext)
{
#ifndef X64
    // Modoffs was only ever used for X64.
    return true;
#else
    std::cerr << "\n===============\nTesting legacy kernel xfer values\n";
    std::vector<test_multi_module_mapper_t::bounds_t> modules = {
        { 100, 150 },
        { 400, 450 },
    };

    kernel_interrupted_raw_pc_t interrupt;
    interrupt.pc.modidx = 1;
    interrupt.pc.modoffs = 42;

    std::vector<offline_entry_t> raw;
    // Version is < OFFLINE_FILE_VERSION_XFER_ABS_PC.
    raw.push_back(make_header(OFFLINE_FILE_VERSION_ENCODINGS));
    raw.push_back(make_tid());
    raw.push_back(make_pid());
    raw.push_back(make_line_size());
    raw.push_back(make_timestamp());
    raw.push_back(make_core());
    raw.push_back(make_marker(TRACE_MARKER_TYPE_KERNEL_EVENT, interrupt.combined_value));
    raw.push_back(make_exit());

    std::vector<trace_entry_t> entries;
    if (!run_raw2trace(drcontext, raw, nullptr, entries, 0, modules))
        return false;
    int idx = 0;
    return (
        check_entry(entries, idx, TRACE_TYPE_HEADER, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD, -1) &&
        check_entry(entries, idx, TRACE_TYPE_PID, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER,
                    TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_KERNEL_EVENT,
                    static_cast<addr_t>(modules[interrupt.pc.modidx].start +
                                        interrupt.pc.modoffs)) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD_EXIT, -1) &&
        check_entry(entries, idx, TRACE_TYPE_FOOTER, -1));
#endif
}

/* Tests >=OFFLINE_FILE_VERSION_XFER_ABS_PC (absolute PC) handling. */
bool
test_xfer_absolute(void *drcontext)
{
    std::cerr << "\n===============\nTesting legacy kernel xfer values\n";
    std::vector<test_multi_module_mapper_t::bounds_t> modules = {
        { 100, 150 },
        { 400, 450 },
    };
    constexpr addr_t INT_PC = 442;

    std::vector<offline_entry_t> raw;
    raw.push_back(make_header(OFFLINE_FILE_VERSION_XFER_ABS_PC));
    raw.push_back(make_tid());
    raw.push_back(make_pid());
    raw.push_back(make_line_size());
    raw.push_back(make_timestamp());
    raw.push_back(make_core());
    raw.push_back(make_marker(TRACE_MARKER_TYPE_KERNEL_EVENT, INT_PC));
    raw.push_back(make_exit());

    std::vector<trace_entry_t> entries;
    if (!run_raw2trace(drcontext, raw, nullptr, entries, 0, modules))
        return false;
    int idx = 0;
    return (
        check_entry(entries, idx, TRACE_TYPE_HEADER, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD, -1) &&
        check_entry(entries, idx, TRACE_TYPE_PID, -1) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER,
                    TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_KERNEL_EVENT,
                    INT_PC) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD_EXIT, -1) &&
        check_entry(entries, idx, TRACE_TYPE_FOOTER, -1));
}

// Tests additional taken/untaken/indirect-target cases.
bool
test_branch_decoration(void *drcontext)
{
    std::cerr << "\n===============\nTesting branch decoration\n";
    // Simple cases and rseq side exits were already tested in existing tests.
    // We focus on signals, rseq rollbacks to branches, and terminal branches here.
    bool res = true;
    {
        // Taken branch before signal.
        instrlist_t *ilist = instrlist_create(drcontext);
        instr_t *nop1 = XINST_CREATE_nop(drcontext); // Avoid offset of 0.
        instr_t *move =
            XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
        instr_t *jcc =
            XINST_CREATE_jump_cond(drcontext, DR_PRED_EQ, opnd_create_instr(move));
        instr_t *nop2 = XINST_CREATE_nop(drcontext);
        instrlist_append(ilist, nop1);
        instrlist_append(ilist, jcc);
        instrlist_append(ilist, nop2);
        instrlist_append(ilist, move);
        size_t offs_nop1 = 0;
        size_t offs_jcc = offs_nop1 + instr_length(drcontext, nop1);
        size_t offs_nop2 = offs_jcc + instr_length(drcontext, jcc);
        size_t offs_mov = offs_nop2 + instr_length(drcontext, nop2);

        std::vector<offline_entry_t> raw;
        raw.push_back(make_header());
        raw.push_back(make_tid());
        raw.push_back(make_pid());
        raw.push_back(make_line_size());
        raw.push_back(make_block(offs_jcc, 1));
        raw.push_back(make_marker(TRACE_MARKER_TYPE_KERNEL_EVENT, offs_mov));
        raw.push_back(make_exit());

        std::vector<trace_entry_t> entries;
        if (!run_raw2trace(drcontext, raw, ilist, entries))
            return false;
        int idx = 0;
        res = res && check_entry(entries, idx, TRACE_TYPE_HEADER, -1) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE) &&
            check_entry(entries, idx, TRACE_TYPE_THREAD, -1) &&
            check_entry(entries, idx, TRACE_TYPE_PID, -1) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER,
                        TRACE_MARKER_TYPE_CACHE_LINE_SIZE) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER,
                        TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT) &&
            check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#ifdef X86_32
            // An extra encoding entry is needed.
            check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#endif
            check_entry(entries, idx, TRACE_TYPE_INSTR_TAKEN_JUMP, -1, offs_jcc) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_KERNEL_EVENT,
                        offs_mov) &&
            check_entry(entries, idx, TRACE_TYPE_THREAD_EXIT, -1) &&
            check_entry(entries, idx, TRACE_TYPE_FOOTER, -1);
    }
    {
        // Untaken branch before signal.
        instrlist_t *ilist = instrlist_create(drcontext);
        instr_t *nop1 = XINST_CREATE_nop(drcontext); // Avoid offset of 0.
        instr_t *move =
            XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
        instr_t *jcc =
            XINST_CREATE_jump_cond(drcontext, DR_PRED_EQ, opnd_create_instr(move));
        instr_t *nop2 = XINST_CREATE_nop(drcontext);
        instrlist_append(ilist, nop1);
        instrlist_append(ilist, jcc);
        instrlist_append(ilist, nop2);
        instrlist_append(ilist, move);
        size_t offs_nop1 = 0;
        size_t offs_jcc = offs_nop1 + instr_length(drcontext, nop1);
        size_t offs_nop2 = offs_jcc + instr_length(drcontext, jcc);

        std::vector<offline_entry_t> raw;
        raw.push_back(make_header());
        raw.push_back(make_tid());
        raw.push_back(make_pid());
        raw.push_back(make_line_size());
        raw.push_back(make_block(offs_jcc, 1));
        raw.push_back(make_marker(TRACE_MARKER_TYPE_KERNEL_EVENT, offs_nop2));
        raw.push_back(make_exit());

        std::vector<trace_entry_t> entries;
        if (!run_raw2trace(drcontext, raw, ilist, entries))
            return false;
        int idx = 0;
        res = res && check_entry(entries, idx, TRACE_TYPE_HEADER, -1) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE) &&
            check_entry(entries, idx, TRACE_TYPE_THREAD, -1) &&
            check_entry(entries, idx, TRACE_TYPE_PID, -1) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER,
                        TRACE_MARKER_TYPE_CACHE_LINE_SIZE) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER,
                        TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT) &&
            check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#ifdef X86_32
            // An extra encoding entry is needed.
            check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#endif
            check_entry(entries, idx, TRACE_TYPE_INSTR_UNTAKEN_JUMP, -1, offs_jcc) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_KERNEL_EVENT,
                        offs_nop2) &&
            check_entry(entries, idx, TRACE_TYPE_THREAD_EXIT, -1) &&
            check_entry(entries, idx, TRACE_TYPE_FOOTER, -1);
    }
    {
        // Untaken branch at and of rseq rollback.
        instrlist_t *ilist = instrlist_create(drcontext);
        instr_t *nop1 = XINST_CREATE_nop(drcontext); // Avoid offset of 0.
        instr_t *move =
            XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
        instr_t *jcc =
            XINST_CREATE_jump_cond(drcontext, DR_PRED_EQ, opnd_create_instr(move));
        instr_t *store = XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(REG2, 0),
                                            opnd_create_reg(REG1));
        instrlist_append(ilist, nop1);
        instrlist_append(ilist, jcc);
        instrlist_append(ilist, store);
        instrlist_append(ilist, move);
        size_t offs_nop1 = 0;
        size_t offs_jcc = offs_nop1 + instr_length(drcontext, nop1);
        size_t offs_store = offs_jcc + instr_length(drcontext, jcc);
        size_t offs_mov = offs_store + instr_length(drcontext, store);

        std::vector<offline_entry_t> raw;
        raw.push_back(make_header());
        raw.push_back(make_tid());
        raw.push_back(make_pid());
        raw.push_back(make_line_size());
        raw.push_back(make_marker(TRACE_MARKER_TYPE_RSEQ_ENTRY, offs_mov));
        raw.push_back(make_block(offs_jcc, 1));
        // The end of our rseq sequence, ending in a committing store.
        raw.push_back(make_block(offs_store, 1));
        raw.push_back(make_memref(42));
        raw.push_back(make_marker(TRACE_MARKER_TYPE_RSEQ_ABORT, offs_mov));
        raw.push_back(make_marker(TRACE_MARKER_TYPE_KERNEL_EVENT, offs_mov));
        raw.push_back(make_block(offs_mov, 1));
        raw.push_back(make_exit());

        std::vector<trace_entry_t> entries;
        if (!run_raw2trace(drcontext, raw, ilist, entries))
            return false;
        int idx = 0;
        res = res && check_entry(entries, idx, TRACE_TYPE_HEADER, -1) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE) &&
            check_entry(entries, idx, TRACE_TYPE_THREAD, -1) &&
            check_entry(entries, idx, TRACE_TYPE_PID, -1) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER,
                        TRACE_MARKER_TYPE_CACHE_LINE_SIZE) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER,
                        TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RSEQ_ENTRY) &&
            check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#ifdef X86_32
            // An extra encoding entry is needed.
            check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#endif
            check_entry(entries, idx, TRACE_TYPE_INSTR_UNTAKEN_JUMP, -1, offs_jcc) &&
            // The committing store should not be here.
            check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RSEQ_ABORT) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_KERNEL_EVENT,
                        offs_mov) &&
            check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
            check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_mov) &&
            check_entry(entries, idx, TRACE_TYPE_THREAD_EXIT, -1) &&
            check_entry(entries, idx, TRACE_TYPE_FOOTER, -1);
    }
    {
        // Taken branch at and of rseq rollback.
        instrlist_t *ilist = instrlist_create(drcontext);
        instr_t *nop1 = XINST_CREATE_nop(drcontext); // Avoid offset of 0.
        instr_t *nop2 = XINST_CREATE_nop(drcontext);
        instr_t *nop3 = XINST_CREATE_nop(drcontext);
        instr_t *move =
            XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
        instr_t *store = XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(REG2, 0),
                                            opnd_create_reg(REG1));
        instr_t *jcc =
            XINST_CREATE_jump_cond(drcontext, DR_PRED_EQ, opnd_create_instr(store));
        instrlist_append(ilist, nop1);
        instrlist_append(ilist, jcc);
        instrlist_append(ilist, nop2);
        instrlist_append(ilist, nop3);
        instrlist_append(ilist, store);
        instrlist_append(ilist, move);
        size_t offs_nop1 = 0;
        size_t offs_jcc = offs_nop1 + instr_length(drcontext, nop1);
        size_t offs_nop2 = offs_jcc + instr_length(drcontext, jcc);
        size_t offs_nop3 = offs_nop2 + instr_length(drcontext, nop2);
        size_t offs_store = offs_nop3 + instr_length(drcontext, nop3);
        size_t offs_mov = offs_store + instr_length(drcontext, store);

        std::vector<offline_entry_t> raw;
        raw.push_back(make_header());
        raw.push_back(make_tid());
        raw.push_back(make_pid());
        raw.push_back(make_line_size());
        raw.push_back(make_marker(TRACE_MARKER_TYPE_RSEQ_ENTRY, offs_mov));
        raw.push_back(make_block(offs_jcc, 1));
        // The end of our rseq sequence, ending in a committing store.
        raw.push_back(make_block(offs_store, 1));
        raw.push_back(make_memref(42));
        raw.push_back(make_marker(TRACE_MARKER_TYPE_RSEQ_ABORT, offs_mov));
        raw.push_back(make_marker(TRACE_MARKER_TYPE_KERNEL_EVENT, offs_mov));
        raw.push_back(make_block(offs_mov, 1));
        raw.push_back(make_exit());

        std::vector<trace_entry_t> entries;
        if (!run_raw2trace(drcontext, raw, ilist, entries))
            return false;
        int idx = 0;
        res = res && check_entry(entries, idx, TRACE_TYPE_HEADER, -1) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE) &&
            check_entry(entries, idx, TRACE_TYPE_THREAD, -1) &&
            check_entry(entries, idx, TRACE_TYPE_PID, -1) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER,
                        TRACE_MARKER_TYPE_CACHE_LINE_SIZE) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER,
                        TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RSEQ_ENTRY) &&
            check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#ifdef X86_32
            // An extra encoding entry is needed.
            check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#endif
            check_entry(entries, idx, TRACE_TYPE_INSTR_TAKEN_JUMP, -1, offs_jcc) &&
            // The committing store should not be here.
            check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_RSEQ_ABORT) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_KERNEL_EVENT,
                        offs_mov) &&
            check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
            check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_mov) &&
            check_entry(entries, idx, TRACE_TYPE_THREAD_EXIT, -1) &&
            check_entry(entries, idx, TRACE_TYPE_FOOTER, -1);
    }
    {
        // Trace-final branch.
        instrlist_t *ilist = instrlist_create(drcontext);
        instr_t *nop1 = XINST_CREATE_nop(drcontext); // Avoid offset of 0.
        instr_t *nop2 = XINST_CREATE_nop(drcontext);
        instr_t *store = XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(REG2, 0),
                                            opnd_create_reg(REG1));
        instr_t *jcc =
            XINST_CREATE_jump_cond(drcontext, DR_PRED_EQ, opnd_create_instr(store));
        instr_t *move =
            XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
        instrlist_append(ilist, nop1);
        instrlist_append(ilist, move);
        instrlist_append(ilist, jcc);
        instrlist_append(ilist, nop2);
        instrlist_append(ilist, store);
        size_t offs_nop1 = 0;
        size_t offs_mov = offs_nop1 + instr_length(drcontext, nop1);

        std::vector<offline_entry_t> raw;
        raw.push_back(make_header());
        raw.push_back(make_tid());
        raw.push_back(make_pid());
        raw.push_back(make_line_size());
        raw.push_back(make_block(offs_mov, 2));
        // The trace just ends here.
        raw.push_back(make_exit());

        std::vector<trace_entry_t> entries;
        if (!run_raw2trace(drcontext, raw, ilist, entries))
            return false;
        int idx = 0;
        res = res && check_entry(entries, idx, TRACE_TYPE_HEADER, -1) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE) &&
            check_entry(entries, idx, TRACE_TYPE_THREAD, -1) &&
            check_entry(entries, idx, TRACE_TYPE_PID, -1) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER,
                        TRACE_MARKER_TYPE_CACHE_LINE_SIZE) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER,
                        TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT) &&
            check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
            check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_mov) &&
            // The branch and its encoding should be removed.
            check_entry(entries, idx, TRACE_TYPE_THREAD_EXIT, -1) &&
            check_entry(entries, idx, TRACE_TYPE_FOOTER, -1);
    }
    {
        // Window-final branch.
        instrlist_t *ilist = instrlist_create(drcontext);
        instr_t *nop1 = XINST_CREATE_nop(drcontext); // Avoid offset of 0.
        instr_t *nop2 = XINST_CREATE_nop(drcontext);
        instr_t *store = XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(REG2, 0),
                                            opnd_create_reg(REG1));
        instr_t *jcc =
            XINST_CREATE_jump_cond(drcontext, DR_PRED_EQ, opnd_create_instr(store));
        instr_t *move =
            XINST_CREATE_move(drcontext, opnd_create_reg(REG1), opnd_create_reg(REG2));
        instrlist_append(ilist, nop1);
        instrlist_append(ilist, move);
        instrlist_append(ilist, jcc);
        instrlist_append(ilist, nop2);
        instrlist_append(ilist, store);
        size_t offs_nop1 = 0;
        size_t offs_mov = offs_nop1 + instr_length(drcontext, nop1);
        size_t offs_jcc = offs_mov + instr_length(drcontext, move);
        size_t offs_nop2 = offs_jcc + instr_length(drcontext, jcc);
        size_t offs_store = offs_nop2 + instr_length(drcontext, nop2);

        std::vector<offline_entry_t> raw;
        raw.push_back(make_header());
        raw.push_back(make_tid());
        raw.push_back(make_pid());
        raw.push_back(make_line_size());
        raw.push_back(make_block(offs_mov, 2));
        // Test a branch at the end of a window.
        raw.push_back(make_window_id(1));
        // Now repeat that branch to test encodings.
        raw.push_back(make_block(offs_mov, 2));
        raw.push_back(make_block(offs_store, 1));
        raw.push_back(make_exit());

        std::vector<trace_entry_t> entries;
        if (!run_raw2trace(drcontext, raw, ilist, entries))
            return false;
        int idx = 0;
        res = res && check_entry(entries, idx, TRACE_TYPE_HEADER, -1) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE) &&
            check_entry(entries, idx, TRACE_TYPE_THREAD, -1) &&
            check_entry(entries, idx, TRACE_TYPE_PID, -1) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER,
                        TRACE_MARKER_TYPE_CACHE_LINE_SIZE) &&
            check_entry(entries, idx, TRACE_TYPE_MARKER,
                        TRACE_MARKER_TYPE_CHUNK_INSTR_COUNT) &&
            check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
            check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_mov) &&
            // The branch and its encoding should be removed.
            check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_WINDOW_ID) &&
            check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_mov) &&
            // The branch should have an encoding here.
            check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#ifdef X86_32
            // An extra encoding entry is needed.
            check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
#endif
            check_entry(entries, idx, TRACE_TYPE_INSTR_TAKEN_JUMP, -1, offs_jcc) &&
            check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
            check_entry(entries, idx, TRACE_TYPE_INSTR, -1, offs_store) &&
            check_entry(entries, idx, TRACE_TYPE_THREAD_EXIT, -1) &&
            check_entry(entries, idx, TRACE_TYPE_FOOTER, -1);
    }
    return res;
}

int
test_main(int argc, const char *argv[])
{
    void *drcontext = dr_standalone_init();
    if (!test_branch_delays(drcontext) || !test_marker_placement(drcontext) ||
        !test_marker_delays(drcontext) || !test_chunk_boundaries(drcontext) ||
        !test_chunk_encodings(drcontext) || !test_duplicate_syscalls(drcontext) ||
        !test_false_syscalls(drcontext) || !test_rseq_fallthrough(drcontext) ||
        !test_rseq_rollback_legacy(drcontext) || !test_rseq_rollback(drcontext) ||
        !test_rseq_rollback_with_timestamps(drcontext) ||
        !test_rseq_rollback_with_signal(drcontext) ||
        !test_rseq_rollback_with_chunks(drcontext) || !test_rseq_side_exit(drcontext) ||
        !test_rseq_side_exit_signal(drcontext) ||
        !test_rseq_side_exit_inverted(drcontext) ||
        !test_rseq_side_exit_inverted_with_timestamp(drcontext) ||
        !test_xfer_modoffs(drcontext) || !test_xfer_absolute(drcontext) ||
        !test_branch_decoration(drcontext))
        return 1;
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
