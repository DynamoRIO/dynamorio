/* **********************************************************
 * Copyright (c) 2026 Bryan Berns.  All rights reserved.
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

#include <cassert>
#include <future>
#include <string>
#include <thread>
#include <unistd.h>

#include "ipc_reader.h"

namespace dynamorio {
namespace drmemtrace {

int
test_main(int argc, const char *argv[])
{
    const std::string name = "drcachesim-ipc-eof-" + std::to_string(getpid());
    ipc_reader_t reader(name.c_str(), 0);
    ipc_reader_t end;
    assert(!!reader);
    std::promise<void> first_instruction_read;
    auto ready = first_instruction_read.get_future();

    // Keep the final read buffer separate from the thread and process headers.
    std::thread writer([&]() {
        named_pipe_t pipe(name.c_str());
        assert(pipe.open_for_write());
        trace_entry_t header[] = { { TRACE_TYPE_THREAD, 0, { 1 } },
                                   { TRACE_TYPE_PID, 0, { 1 } },
                                   { TRACE_TYPE_INSTR, 1, { 0x1000 } } };
        assert(pipe.write(header, sizeof(header)) ==
               static_cast<ssize_t>(sizeof(header)));
        ready.wait();

        // Leave an instruction bundle after the first entry in the final buffer.
        trace_entry_t tail[] = { { TRACE_TYPE_INSTR, 1, { 0x2000 } },
                                 { TRACE_TYPE_INSTR_BUNDLE, 2, { 0 } },
                                 { TRACE_TYPE_THREAD_EXIT, 0, { 1 } } };
        tail[1].length[0] = 1;
        tail[1].length[1] = 1;
        assert(pipe.write(tail, sizeof(tail)) == static_cast<ssize_t>(sizeof(tail)));
    });
    assert(reader.init());
    assert((*reader).instr.addr == 0x1000);
    first_instruction_read.set_value();

    // EOF must end iteration after the four instructions and one thread exit.
    int records = 0;
    for (; reader != end; ++reader)
        assert(++records <= 5);
    writer.join();
    assert(records == 5);
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
