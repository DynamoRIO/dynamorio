/* **********************************************************
 * Copyright (c) 2021 Google, Inc.  All rights reserved.
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
#else
#    error Unsupported arch
#endif

// Subclasses raw2trace_t and replaces the module loading with a buffer
// of encoded instr_t.
class raw2trace_test_t : public raw2trace_t {
public:
    raw2trace_test_t(const std::vector<std::istream *> &input,
                     const std::vector<std::ostream *> &output, instrlist_t &instrs,
                     void *drcontext)
        : raw2trace_t(nullptr, input, output, drcontext,
                      // The sequences are small so we print everything for easier
                      // debugging and viewing of what's going on.
                      4)
    {
        byte *pc = instrlist_encode(drcontext, &instrs, decode_buf_, true);
        ASSERT(pc - decode_buf_ < MAX_DECODE_SIZE, "decode buffer overflow");
        set_modvec_(&modules_);
    }

protected:
    std::string
    read_and_map_modules() override
    {
        modules_.push_back(module_t("fake_exe", 0, decode_buf_, 0, MAX_DECODE_SIZE,
                                    MAX_DECODE_SIZE, true));
        return "";
    }

private:
    static const int MAX_DECODE_SIZE = 1024;
    byte decode_buf_[MAX_DECODE_SIZE];
    std::vector<module_t> modules_;
};

offline_entry_t
make_header()
{
    offline_entry_t entry;
    entry.extended.type = OFFLINE_TYPE_EXTENDED;
    entry.extended.ext = OFFLINE_EXT_TYPE_HEADER;
    entry.extended.valueA = OFFLINE_FILE_VERSION;
    entry.extended.valueB = OFFLINE_FILE_TYPE_DEFAULT;
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

void
check_entry(std::vector<trace_entry_t> &entries, int &idx, unsigned short expected_type,
            int expected_size)
{
    if (expected_type != entries[idx].type ||
        (expected_size > 0 &&
         static_cast<unsigned short>(expected_size) != entries[idx].size)) {
        std::cerr << "Entry " << idx << " has type " << entries[idx].type << " and size "
                  << entries[idx].size << " != expected type " << expected_type
                  << " and expected size " << expected_size << "\n";
    }
    ++idx;
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
    // We need an ostream to capature out.
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
    check_entry(entries, idx, TRACE_TYPE_HEADER, -1);
    check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_VERSION);
    check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_FILETYPE);
    check_entry(entries, idx, TRACE_TYPE_THREAD, -1);
    check_entry(entries, idx, TRACE_TYPE_PID, -1);
    check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CACHE_LINE_SIZE);
    check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP);
    check_entry(entries, idx, TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_CPU_ID);
    // Both branches should be delayed until after the timestamp+cpu markers:
    check_entry(entries, idx, TRACE_TYPE_INSTR_CONDITIONAL_JUMP, -1);
    check_entry(entries, idx, TRACE_TYPE_INSTR_DIRECT_JUMP, -1);
    check_entry(entries, idx, TRACE_TYPE_INSTR, -1);
    check_entry(entries, idx, TRACE_TYPE_THREAD_EXIT, -1);
    check_entry(entries, idx, TRACE_TYPE_FOOTER, -1);
    return true;
}

int
main(int argc, const char *argv[])
{

    void *drcontext = dr_standalone_init();
    if (!test_branch_delays(drcontext))
        return 1;
    return 0;
}
