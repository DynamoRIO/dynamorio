/* **********************************************************
 * Copyright (c) 2026 Bryan Berns  All rights reserved.
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

#include "test_helpers.h"
#include "reader/ipc_reader.h"

#include <assert.h>
#include <chrono>
#include <future>
#include <string>
#include <thread>

namespace dynamorio {
namespace drmemtrace {

static void
test_eof(bool send_footer)
{
#ifdef WINDOWS
    const DWORD pid = GetCurrentProcessId();
#else
    const pid_t pid = getpid();
#endif
    const std::string name = "drtestpipe_ipc_reader_" + std::to_string(pid) + "_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    ipc_reader_t reader(name.c_str(), 0);
    ipc_reader_t end;
    assert(!!reader);
    std::promise<void> headers_read, records_read;
    std::future<void> headers_done = headers_read.get_future();
    std::future<void> records_done = records_read.get_future();
    std::thread writer([&]() {
        named_pipe_t pipe(name.c_str());
#ifdef WINDOWS
        // Wait until the server is listening before opening the client handle.
        BOOL ready =
            WaitNamedPipeA(reader.get_stream_name().c_str(), NMPWAIT_WAIT_FOREVER);
        assert(ready != 0);
#endif
        bool opened = pipe.open_for_write();
        assert(opened);
        trace_entry_t headers[] = {
            { TRACE_TYPE_THREAD, 0, { 1 } },
            { TRACE_TYPE_PID, 0, { 1 } },
            { TRACE_TYPE_MARKER, TRACE_MARKER_TYPE_TIMESTAMP, { 1 } }
        };
        ssize_t written = pipe.write(headers, sizeof(headers));
        assert(written == sizeof(headers));

        // Force a new read buffer beginning with an instruction and its bundle.
        headers_done.wait();
        trace_entry_t records[] = { { TRACE_TYPE_INSTR, 4, { 0x1000 } },
                                    { TRACE_TYPE_INSTR_BUNDLE, 2, { 0 } },
                                    { TRACE_TYPE_THREAD_EXIT, 0, { 1 } } };
        records[1].length[0] = 4;
        records[1].length[1] = 4;
        written = pipe.write(records, sizeof(records));
        assert(written == sizeof(records));

        // Close only after all records are consumed, leaving the last buffer intact.
        records_done.wait();
        if (send_footer) {
            trace_entry_t footer = { TRACE_TYPE_FOOTER, 0, { 0 } };
            written = pipe.write(&footer, sizeof(footer));
            assert(written == sizeof(footer));
        }
    });

    // Read and validate the complete stream before asking for EOF.
    bool initialized = reader.init();
    assert(initialized);
    assert((*reader).marker.type == TRACE_TYPE_MARKER);
    assert((*reader).marker.marker_type == TRACE_MARKER_TYPE_TIMESTAMP);
    headers_read.set_value();
    for (addr_t pc = 0x1000; pc < 0x100c; pc += 4) {
        ++reader;
        assert(reader != end);
        assert((*reader).instr.type == TRACE_TYPE_INSTR);
        assert((*reader).instr.addr == pc);
        assert((*reader).instr.size == 4);
    }
    ++reader;
    assert(reader != end);
    assert((*reader).exit.type == TRACE_TYPE_THREAD_EXIT);
    assert((*reader).exit.tid == 1);
    records_read.set_value();
    writer.join();

    // EOF must not replay buffered entries or alter either ordinal.
    for (int i = 0; i < 3; ++i) {
        ++reader;
        assert(reader == end);
        assert(reader.get_record_ordinal() == 5);
        assert(reader.get_instruction_ordinal() == 3);
        assert((*reader).exit.type == TRACE_TYPE_THREAD_EXIT);
    }
}

int
test_main(int argc, const char *argv[])
{
    test_eof(false);
    test_eof(true);
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
