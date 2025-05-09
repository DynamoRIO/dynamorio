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

#include <fcntl.h>
#include <stdio.h>

#define MAX_ITER 99999

const char hello_world[] = "Hello World!";

static void
signal_handler(int sig)
{
    if (sig == SIGTERM) {
        /* runall.cmake for attach test requires "done" as last line once done. */
        print("done\n");
    }
    exit(1);
}

/*
 * This test captures a memory dump mid-run and records syscall arguments
 * along with memory regions to a file. Syscalls are invoked by open(),
 * write(), lseek(), read(), print(),  close() and remove(). Given the non-deterministic
 * termination of the while loop, our verification is limited to the syscall write()
 * invoked by print("done\n") in signal_handler().
 */
int
main(int argc, char **argv)
{
    int sum = 0;
    int counter = 0;
    char filename[MAXIMUM_PATH];

    intercept_signal(SIGTERM, (handler_3_t)signal_handler, /*sigstack=*/false);

    snprintf(filename, BUFFER_SIZE_ELEMENTS(filename),
             "syscall_record_attach_test.%d.txt", getpid());
    NULL_TERMINATE_BUFFER(filename);
    int fd = open(filename, O_CREAT | O_RDWR);
    if (fd < 0) {
        print("failed to open file %s to write\n", filename);
        return 1;
    }
    if (write(fd, hello_world, sizeof(hello_world)) != sizeof(hello_world)) {
        print("failed to write to file %s\n", filename);
        return 1;
    }
    while (true) {
        /* Don't spin forever to avoid hosing machines if test harness somehow
         * fails to kill.
         */
        counter++;
        if (counter > MAX_ITER) {
            print("hit max iters\n");
            break;
        }
        if (lseek(fd, 0, SEEK_SET) != 0) {
            print("failed to rewind the file %s\n", filename);
            return 1;
        }
        char buffer[sizeof(hello_world) + 1];
        if (read(fd, buffer, sizeof(hello_world)) != sizeof(hello_world)) {
            print("failed to read from file %s\n", filename);
            return 1;
        }
        print("%s\n", hello_world);
        for (int i = 0; i < sizeof(hello_world); ++i) {
            sum += hello_world[i];
        }
        sleep(1);
    }
    if (close(fd) != 0) {
        print("failed to close file %s after reading\n", filename);
        return 1;
    }
    if (remove(filename) != 0) {
        print("failed to remove file %s\n", filename);
        return 1;
    }
    print("sum: %d\n", sum);
    return sum;
}
