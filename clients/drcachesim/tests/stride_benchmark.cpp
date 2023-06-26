/* **********************************************************
 * Copyright (c) 2018-2023 Google LLC  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE LLC OR CONTRIBUTORS BE LIABLE
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
 *
 * The cache miss analyzer can be used to identify the load instruction that
 * is suffering from most of the LLC misses in this microbenchmark. The analyzer
 * can also produce prefetching hints for this microbenchmark. To run the
 * analyzer on this microbenchmark and write the prefetching hints in a text
 * file called "rec.csv", perform the following:
 * * Compile the microbenchmark. Assuming g++ is the compiler being used:
 *   $ g++ -O3 -o stride_benchmark stride_benchmark.cpp
 * * Run the analyzer:
 *   $ bin64/drrun -t drcachesim -simulator_type miss_analyzer -LL_miss_file rec.csv -- \
 *     stride_benchmark
 *
 */

#include <stdint.h>
#include <string.h>
#include <iostream>

#define MEM_BARRIER() __asm__ __volatile__("" ::: "memory")

int
main(int argc, const char *argv[])
{
    // Cache line size in bytes.
    const int kLineSize = 64;
    // Number of cache lines skipped by the stream every iteration.
    const int kStride = 7;
    // Number of 1-byte elements in the array.
    const size_t kArraySize = 16 * 1024 * 1024;
    // Number of iterations in the main loop.
    const int kIterations = 20000;
    // The main vector/array used for emulating pointer chasing.
    unsigned char *buffer = new unsigned char[kArraySize];
    memset(buffer, kStride, kArraySize);

    // Add a memory barrier so the call doesn't get optimized away or
    // reordered with respect to callers.
    MEM_BARRIER();

    int position = 0;

    // Here the code will pointer chase through the array skipping forward
    // kStride cache lines at a time. Since kStride is an odd number, the main
    // loop will touch different cache lines as it wraps around.
    for (int loop = 0; loop < kIterations; ++loop) {
        // This prefetching instruction results in a speedup of >2x
        // on a Skylake machine running Linux when compiled with g++ -O3.
        // const int prefetch_distance = 5 * kStride * kLineSize;
        // __builtin_prefetch(&buffer[position + prefetch_distance], 0, 0);

        position += (buffer[position] * kLineSize);
        position &= (kArraySize - 1);
    }

    // Add a memory barrier so the call doesn't get optimized away or
    // reordered with respect to callers.
    MEM_BARRIER();

    std::cerr << "Value = " << position << std::endl;

    return 0;
}
