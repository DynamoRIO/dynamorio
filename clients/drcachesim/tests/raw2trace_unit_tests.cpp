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
#include "tracer/raw2trace.h"
#include "tracer/raw2trace_directory.h"
#include <iostream>
#include <sstream>

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

#ifdef X86
#    define REG1 DR_REG_XAX
#    define REG2 DR_REG_XDX
#elif defined(AARCHXX)
#    define REG1 DR_REG_R0
#    define REG2 DR_REG_R1
#elif defined(RISCV64)
#    define REG1 DR_REG_A0
#    define REG2 DR_REG_A1
#else
#    error Unsupported arch
#endif

// Subclasses module_mapper_t and replaces the module loading with a
// buffer of encoded instr_t.
class module_mapper_test_t : public module_mapper_t {
public:
    module_mapper_test_t(instrlist_t &instrs, void *drcontext)
        : module_mapper_t(nullptr)
    {
        byte *pc = instrlist_encode(drcontext, &instrs, decode_buf_, true);
        ASSERT(pc != nullptr, "encoding failed");
        ASSERT(pc - decode_buf_ < MAX_DECODE_SIZE, "decode buffer overflow");
    }

protected:
    void
    read_and_map_modules() override
    {
        modvec_.push_back(module_t("fake_exe", 0, decode_buf_, 0, MAX_DECODE_SIZE,
                                   MAX_DECODE_SIZE, true));
    }

private:
    static const int MAX_DECODE_SIZE = 1024;
    byte decode_buf_[MAX_DECODE_SIZE];
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
        module_mapper_ =
            std::unique_ptr<module_mapper_t>(new module_mapper_test_t(instrs, drcontext));
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
        module_mapper_ =
            std::unique_ptr<module_mapper_t>(new module_mapper_test_t(instrs, drcontext));
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
make_header()
{
    offline_entry_t entry;
    entry.extended.type = OFFLINE_TYPE_EXTENDED;
    entry.extended.ext = OFFLINE_EXT_TYPE_HEADER;
    entry.extended.valueA = OFFLINE_FILE_TYPE_DEFAULT;
    entry.extended.valueB = OFFLINE_FILE_VERSION;
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
make_load(uint64_t addr)
{
    offline_entry_t entry;
    entry.addr.type = OFFLINE_TYPE_MEMREF;
    entry.addr.addr = addr;
    return entry;
}

offline_entry_t
make_timestamp()
{
    static int timecount;
    offline_entry_t entry;
    entry.timestamp.type = OFFLINE_TYPE_TIMESTAMP;
    entry.timestamp.usec = ++timecount;
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
            int expected_size)
{
    if (expected_type != entries[idx].type ||
        (expected_size > 0 &&
         static_cast<unsigned short>(expected_size) != entries[idx].size)) {
        std::cerr << "Entry " << idx << " has type " << entries[idx].type << " and size "
                  << entries[idx].size << " != expected type " << expected_type
                  << " and expected size " << expected_size << "\n";
        return false;
    }
    ++idx;
    return true;
}

bool
test_branch_delays(void *drcontext)
{
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
    // We need an ostream to capture out.
    std::ostringstream result_stream;
    std::vector<std::ostream *> output;
    output.push_back(&result_stream);

    // Run raw2trace with our subclass supplying our decodings.
    raw2trace_test_t raw2trace(input, output, *ilist, drcontext);
    std::string error = raw2trace.do_conversion();
    CHECK(error.empty(), error);
    instrlist_clear_and_destroy(drcontext, ilist);

    // Now check the results.
    std::string result = result_stream.str();
    char *start = &result[0];
    char *end = start + result.size();
    CHECK(result.size() % sizeof(trace_entry_t) == 0,
          "output is not a multiple of trace_entry_t");
    std::vector<trace_entry_t> entries;
    while (start < end) {
        entries.push_back(*reinterpret_cast<trace_entry_t *>(start));
        start += sizeof(trace_entry_t);
    }
    for (const auto &entry : entries) {
        std::cout << "type: " << entry.type << " size: " << entry.size << "\n";
    }
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
        check_entry(entries, idx, TRACE_TYPE_INSTR_CONDITIONAL_JUMP, -1) &&
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
    instr_t *load1 = INSTR_CREATE_ldr(drcontext, opnd_create_reg(REG1),
                                      OPND_CREATE_ABSMEM(move2, OPSZ_PTR));
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
    // We need an ostream to capture out.
    std::ostringstream result_stream;
    std::vector<std::ostream *> output;
    output.push_back(&result_stream);

    // Run raw2trace with our subclass supplying our decodings.
    raw2trace_test_t raw2trace(input, output, *ilist, drcontext);
    std::string error = raw2trace.do_conversion();
    CHECK(error.empty(), error);
    instrlist_clear_and_destroy(drcontext, ilist);

    // Now check the results.
    std::string result = result_stream.str();
    char *start = &result[0];
    char *end = start + result.size();
    CHECK(result.size() % sizeof(trace_entry_t) == 0,
          "output is not a multiple of trace_entry_t");
    std::vector<trace_entry_t> entries;
    while (start < end) {
        entries.push_back(*reinterpret_cast<trace_entry_t *>(start));
        start += sizeof(trace_entry_t);
    }
    for (const auto &entry : entries) {
        std::cout << "type: " << entry.type << " size: " << entry.size << "\n";
    }
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
    // We need an ostream to capture out.
    std::ostringstream result_stream;
    std::vector<std::ostream *> output;
    output.push_back(&result_stream);

    // Run raw2trace with our subclass supplying our decodings.
    raw2trace_test_t raw2trace(input, output, *ilist, drcontext);
    std::string error = raw2trace.do_conversion();
    CHECK(error.empty(), error);
    instrlist_clear_and_destroy(drcontext, ilist);

    // Now check the results.
    std::string result = result_stream.str();
    char *start = &result[0];
    char *end = start + result.size();
    CHECK(result.size() % sizeof(trace_entry_t) == 0,
          "output is not a multiple of trace_entry_t");
    std::vector<trace_entry_t> entries;
    while (start < end) {
        entries.push_back(*reinterpret_cast<trace_entry_t *>(start));
        start += sizeof(trace_entry_t);
    }
    for (const auto &entry : entries) {
        std::cout << "type: " << entry.type << " size: " << entry.size << "\n";
    }
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
    // We need an archive_ostream to enable chunking.
    archive_ostream_test_t result_stream;
    std::vector<archive_ostream_t *> output;
    output.push_back(&result_stream);

    // Run raw2trace with our subclass supplying our decodings.
    // Use a chunk instr count of 2 to split the 2 jumps.
    raw2trace_test_t raw2trace(input, output, *ilist, drcontext, 2);
    std::string error = raw2trace.do_conversion();
    CHECK(error.empty(), error);
    instrlist_clear_and_destroy(drcontext, ilist);

    // Now check the results.
    std::string result = result_stream.str();
    char *start = &result[0];
    char *end = start + result.size();
    CHECK(result.size() % sizeof(trace_entry_t) == 0,
          "output is not a multiple of trace_entry_t");
    std::vector<trace_entry_t> entries;
    while (start < end) {
        entries.push_back(*reinterpret_cast<trace_entry_t *>(start));
        start += sizeof(trace_entry_t);
    }
    int idx = 0;
    for (const auto &entry : entries) {
        std::cout << idx << " type: " << entry.type << " size: " << entry.size
                  << " val: " << entry.addr << "\n";
        ++idx;
    }
    idx = 0;
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
    // We need an archive_ostream to enable chunking.
    archive_ostream_test_t result_stream;
    std::vector<archive_ostream_t *> output;
    output.push_back(&result_stream);

    // Run raw2trace with our subclass supplying our decodings.
    // Use a chunk instr count of 6 to split the 2nd set of 2 jumps.
    raw2trace_test_t raw2trace(input, output, *ilist, drcontext, 6);
    std::string error = raw2trace.do_conversion();
    CHECK(error.empty(), error);
    instrlist_clear_and_destroy(drcontext, ilist);

    // Now check the results.
    std::string result = result_stream.str();
    char *start = &result[0];
    char *end = start + result.size();
    CHECK(result.size() % sizeof(trace_entry_t) == 0,
          "output is not a multiple of trace_entry_t");
    std::vector<trace_entry_t> entries;
    while (start < end) {
        entries.push_back(*reinterpret_cast<trace_entry_t *>(start));
        start += sizeof(trace_entry_t);
    }
    int idx = 0;
    for (const auto &entry : entries) {
        std::cout << idx << " type: " << entry.type << " size: " << entry.size
                  << " val: " << entry.addr << "\n";
        ++idx;
    }
    idx = 0;
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
    std::vector<offline_entry_t> raw;
    raw.push_back(make_header());
    raw.push_back(make_tid());
    raw.push_back(make_pid());
    raw.push_back(make_line_size());
    raw.push_back(make_timestamp());
    raw.push_back(make_core());
    raw.push_back(make_block(offs_move1, 2));
    raw.push_back(make_timestamp());
    raw.push_back(make_core());
    // Repeat the syscall that was the second instr in the size-2 block above,
    // in its own separate block. This is the signature of the duplicate
    // system call invariant error seen in i#5934.
    raw.push_back(make_block(offs_sys, 1));
    raw.push_back(make_timestamp());
    raw.push_back(make_core());
    raw.push_back(make_block(offs_move2, 1));
    raw.push_back(make_exit());

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
    // We need an ostream to capture out.
    std::ostringstream result_stream;
    std::vector<std::ostream *> output;
    output.push_back(&result_stream);

    // Run raw2trace with our subclass supplying our decodings.
    raw2trace_test_t raw2trace(input, output, *ilist, drcontext);
    std::string error = raw2trace.do_conversion();
    CHECK(error.empty(), error);
    instrlist_clear_and_destroy(drcontext, ilist);

    // Now check the results.
    std::string result = result_stream.str();
    char *start = &result[0];
    char *end = start + result.size();
    CHECK(result.size() % sizeof(trace_entry_t) == 0,
          "output is not a multiple of trace_entry_t");
    std::vector<trace_entry_t> entries;
    while (start < end) {
        entries.push_back(*reinterpret_cast<trace_entry_t *>(start));
        start += sizeof(trace_entry_t);
    }
    int idx = 0;
    for (const auto &entry : entries) {
        std::cout << idx << " type: " << entry.type << " size: " << entry.size
                  << " val: " << entry.addr << "\n";
        ++idx;
    }
    idx = 0;
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
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        // The sys instr.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        // Prev block ends.
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        // No duplicate sys instr.
        // We keep the extraneous timestamp+cpu markers above.
        // Prev block ends.
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP) &&
        check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID) &&
        // The move2 instr.
        check_entry(entries, idx, TRACE_TYPE_ENCODING, -1) &&
        check_entry(entries, idx, TRACE_TYPE_INSTR, -1) &&
        check_entry(entries, idx, TRACE_TYPE_THREAD_EXIT, -1) &&
        check_entry(entries, idx, TRACE_TYPE_FOOTER, -1));
}

int
main(int argc, const char *argv[])
{

    void *drcontext = dr_standalone_init();
    if (!test_branch_delays(drcontext) || !test_marker_placement(drcontext) ||
        !test_marker_delays(drcontext) || !test_chunk_boundaries(drcontext) ||
        !test_chunk_encodings(drcontext) || !test_duplicate_syscalls(drcontext))
        return 1;
    return 0;
}
