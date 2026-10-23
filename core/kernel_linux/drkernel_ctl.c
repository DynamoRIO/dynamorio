/* **********************************************************
 * Copyright (c) 2026 Google, LLC  All rights reserved.
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
 * * Neither the name of Google, LLC nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, LLC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "dynamorio_ioctl.h"

static void
print_usage(FILE *stream)
{
    fprintf(stream,
            "Usage: drkernel_ctl <command> [args]\n"
            "Commands:\n"
            "  init [OPTIONS_STRING]   Initialize a kernel tracing session.\n"
            "                          Pass all options as one quoted argument.\n"
            "Example:\n"
            "  drkernel_ctl init \"-loglevel 2\"\n");
}

static int
open_device(void)
{
    int fd = open(DYNAMORIO_DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        err(EXIT_FAILURE, "open %s", DYNAMORIO_DEVICE_PATH);
    }
    return fd;
}

static int
cmd_init(int argc, const char *argv[])
{
    if (argc > 2) {
        print_usage(stderr);
        return EXIT_FAILURE;
    }

    const char *options = argc == 2 ? argv[1] : "";
    struct dynamorio_init_args args = { .options = options };
    int fd = open_device();
    int res = ioctl(fd, DYNAMORIO_IOCTL_INIT, &args);
    if (res < 0) {
        err(EXIT_FAILURE, "init");
    }
    close(fd);
    return EXIT_SUCCESS;
}

static const struct {
    const char *name;
    int (*fn)(int argc, const char *argv[]);
} commands[] = {
    { "init", cmd_init },
};

int
main(int argc, const char *argv[])
{
    if (argc < 2) {
        print_usage(stderr);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_usage(stdout);
        return EXIT_SUCCESS;
    }

    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        if (strcmp(commands[i].name, argv[1]) == 0) {
            return commands[i].fn(argc - 1, argv + 1);
        }
    }

    warnx("unknown command %s", argv[1]);
    print_usage(stderr);
    return EXIT_FAILURE;
}
