/* **********************************************************
 * Copyright (c) 2025 Google, Inc.  All rights reserved.
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

#include <iostream>
#include <unistd.h>

#define DEFAULT_HEAP_SIZE 0x400000

const intptr_t program_break_increments[] = {
    0x10000, 0x10000, 0x10000, -0x10000, -0x10000, -0x10000, DEFAULT_HEAP_SIZE / 2,
    0x10000, 0x10000, 0x10000, -0x10000, -0x10000, -0x10000, DEFAULT_HEAP_SIZE / 2,
    0x10000, 0x10000, 0x10000, -0x10000, -0x10000, -0x10000, 0x10000, 0x10000, 0x10000
};

int
main(int argc, const char *argv[])
{
    char *current_program_break = reinterpret_cast<char *>(sbrk(0));
    std::cerr << "current program break 0x" << std::hex
              << reinterpret_cast<intptr_t>(current_program_break) << "\n";
    for (intptr_t increment : program_break_increments) {
        if (brk(static_cast<char *>(current_program_break + increment)) != 0) {
            std::cerr << "brk(0x"
                      << reinterpret_cast<intptr_t>(current_program_break + increment)
                      << ") failed to increment program break by 0x" << increment << "\n";
            return 1;
        }
        char *new_program_break = static_cast<char *>(sbrk(0));
        std::cerr << "incremented program break by "
                  << (increment > 0 ? "0x" : "-0x")
                  << (increment > 0 ? increment : -increment)
                  << ", new program break 0x"
                  << reinterpret_cast<intptr_t>(new_program_break) << "\n";
        if (new_program_break != current_program_break + increment) {
            std::cerr << "brk failed to increment program break\n";
            return 1;
        }
        current_program_break = new_program_break;
    }
    std::cerr << "all done\n";
    return 0;
}
