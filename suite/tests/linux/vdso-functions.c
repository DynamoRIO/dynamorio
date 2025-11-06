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
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
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

#include "tools.h"

#include <errno.h>
#ifdef X86
#    include <sched.h>
#endif
#include <syscall.h>
#include <sys/time.h>
#include <time.h>

int
main(int argc, char **argv)
{
    struct timeval tv;
    if (gettimeofday(&tv, /*tz=*/NULL) != 0) {
        print("gettimeofday failed, errno %d", errno);
        return 1;
    }
    print("gettimeofday returns %d seconds and %d microseconds\n", tv.tv_sec, tv.tv_usec);
    if (syscall(SYS_gettimeofday, &tv, /*tz=*/NULL) != 0) {
        print("syscall gettimeofday failed, errno %d", errno);
        return 1;
    }
    print("syscall SYS_gettimeofday returns %d seconds and %d microseconds\n", tv.tv_sec,
          tv.tv_usec);

    struct timespec tp;
    if (clock_gettime(CLOCK_MONOTONIC, &tp) != 0) {
        print("clock_gettime failed, errno %d", errno);
        return 1;
    }
    print("clock_gettime returns %d seconds and %d nanoseconds\n", tp.tv_sec, tp.tv_nsec);
    if (syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &tp) != 0) {
        print("syscall SYS_clock_gettime failed, errno %d", errno);
        return 1;
    }
    print("syscall SYS_clock_gettime returns %d seconds and %d nanoseconds\n", tp.tv_sec,
          tp.tv_nsec);

#ifdef X86
#    ifdef X64
    unsigned int cpu, node;
    if (getcpu(&cpu, &node) != 0) {
        print("getcpu failed, errno %d", errno);
        return 1;
    }
    print("getcpu returns %d cpu and %d node\n", cpu, node);
    if (syscall(SYS_getcpu, &cpu, &node) != 0) {
        print("syscall SYS_getcpu failed, errno %d", errno);
        return 1;
    }
    print("syscall SYS_getcpu returns %d cpu and %d node\n", cpu, node);
#    endif
    time_t epoch_time = time(/*tloc=*/NULL);
    if (epoch_time == -1) {
        print("time failed, errno %d", errno);
        return 1;
    }
    print("time returns %d seconds since the Epoch\n", epoch_time);
    epoch_time = syscall(SYS_time, /*tloc=*/NULL);
    if (epoch_time == -1) {
        print("syscall SYS_time failed, errno %d", errno);
        return 1;
    }
    print("syscall SYS_time returns %d seconds since the Epoch\n", epoch_time);
#elif defined(AARCH64)
    struct timespec res;
    if (clock_getres(CLOCK_MONOTONIC, &res) != 0) {
        print("clock_getres failed, errno %d", errno);
        return 1;
    }
    print("clock_getres returns %d seconds and %d nanoseconds\n", res.tv_sec,
          res.tv_nsec);
    if (syscall(SYS_clock_getres, CLOCK_MONOTONIC, &res) != 0) {
        print("syscall SYS_clock_getres failed, errno %d", errno);
        return 1;
    }
    print("clock_getres returns %d seconds and %d nanoseconds\n", res.tv_sec,
          res.tv_nsec);
#else
#    error Unsupported arch
#endif
    return 0;
}
