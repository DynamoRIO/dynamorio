/* **********************************************************
 * Copyright (c) 2015-2018 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

 /* This microbenchmark suffers from a significant number of last-level cache
  * (LLC) misses. SW prefetching can significantly improve its performance.
  */

#include <iostream>
#include <vector>

class StrideBenchmark {
public:
    // streams -- number of streams/arrays.
    // iterations -- of the loop.
    explicit StrideBenchmark()
        : buffer(kArraySize, kStride) {}

    int64_t __attribute__((noinline)) run()
    {
        // Add a memory barrier so the call doesn't get optimized away or
        // reordered with respect to callers.
        __asm__ __volatile__("" ::: "memory");

        int64_t position = 0;

        // Here the code will pointer chase through each individual array
        // skipping forward kStride cache lines at a time. If kStride is
        // an odd number, then the main loop will touch different cache lines
        // as it wraps around.
        for (uint32_t loop = 0; loop < kIterations; ++loop) {
            // This prefetching instruction results in a speedup of ~3.2x
            // on a Skylake machine running Linux.
            // __builtin_prefetch(&buffer[position + (5*kStride*kLineSize)], 0, 0);

            position += (buffer[position] * kLineSize);
            position &= (kArraySize - 1);
        }

        // Add a memory barrier so the call doesn't get optimized away or
        // reordered with respect to callers.
        __asm__ __volatile__("" ::: "memory");

        return position;
    }

private:
    // Cache line size.
    static constexpr uint32_t kLineSize = 64;
    // How many lines are skipped by the stream.
    static constexpr int kStride = 7;
    // Number of 1-byte elements in the array.
    // (200+ MiB to guarantee the array doesn't fit in Skylake caches)
    static constexpr uint32_t kArraySize = 512 * 1024 * 1024;
    // Number of iterations in the main loop.
    static constexpr uint32_t kIterations = 50000000;

    std::vector<uint8_t> buffer;
};

int
main(int argc, const char *argv[])
{
    StrideBenchmark benchmark;
    std::cerr << "Value = " << benchmark.run() << std::endl;
}
