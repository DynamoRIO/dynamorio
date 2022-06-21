/* **********************************************************
 * Copyright (c) 2022 Google, Inc.  All rights reserved.
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

/* DrPT2Trace.c
 * command-line tool for decoding a PT trace, and converting it into an instruction-only
 * memtrace composed of 'memref_t's
 * XXX: Currently, it only counts and prints the instruction count in the trace data.
 */

#include <stdio.h>
#include <inttypes.h>
#include <iostream>

#include "pt2ir.h"

#ifndef IN
#    define IN // nothing
#endif
#ifndef OUT
#    define OUT // nothing
#endif
#ifndef INOUT
#    define INOUT // nothing
#endif

#define CLIENT_NAME "drpt2trace"

/* A collection of options. */
struct options_t {
    /* Print statistics. */
    uint32_t print_stats : 1;
};

static void
print_stats(pt2ir_t *pt_converter IN)
{
    std::cout << "Number of Instructions: " << pt_converter->get_instr_count()
              << std::endl;
}

static void
usage(const char *prog IN)
{
    printf("Usage: %s [<options>]", prog);
    printf("Command-line tool for decoding a PT trace, and converting it into an "
           "instruction-only memtrace composed of 'memref_t's.\n");
    printf("This version only counts and prints the instruction count in the trace "
           "data.\n\n");
    printf("Options:\n");
    printf("  --help|-h                    this text.\n");
    printf("  --stats                      print trace statistics.\n");
    printf("  --pt <file>                  load the processor trace data from <file>.\n");
    printf("  --cpu none|f/m[/s]           set cpu to the given value and decode "
           "according to:\n");
    printf("                               none     spec (default)\n");
    printf("                               f/m[/s]  family/model[/stepping]\n");
    printf("  --sysroot <path>             prepend <path> to sideband filenames.\n");
    printf("  --sample-type <val>          set perf_event_attr.sample_type to <val> "
           "(default: 0).\n");
    printf("  --sb:time-zero <val>         set perf_event_mmap_page.time_zero to <val> "
           "(default: 0).\n");
    printf("  --sb:time-shift <val>        set perf_event_mmap_page.time_shift to <val> "
           "(default: 0).\n");
    printf("  --sb:time-mult <val>         set perf_event_mmap_page.time_mult to <val> "
           "(default: 1).\n");
    printf("  --sb:tsc-offset <val>        show perf events <val> ticks earlier.\n");
    printf("  --sb:primary/secondary <file>\n");
    printf("                               load a perf_event sideband stream from "
           "<file>.\n");
    printf("                               the offset range begin and range end must be "
           "given.\n");
    printf("  --kernel-start <val>         the start address of the kernel.\n");
    printf("  --kcore <file>               load the kernel from a core dump.\n");
    printf("\n");
    printf("You must specify exactly one processor trace file (--pt).\n");
}

static bool
process_args(int argc IN, const char *argv[] IN, pt2ir_config_t &config OUT,
             options_t &options OUT)
{
    int argidx = 1;
    while (argidx < argc) {
        if (strcmp(argv[argidx], "--help") == 0 || strcmp(argv[argidx], "-h") == 0) {
            usage(CLIENT_NAME);
            return false;
        } else if (strcmp(argv[argidx], "--stats") == 0) {
            options.print_stats = 1;
        } else if (strcmp(argv[argidx], "--pt") == 0) {
            if (argc <= ++argidx) {
                std::cerr << CLIENT_NAME << ": --pt: missing argument." << std::endl;
                return false;
            }
            config.raw_file_path = std::string(argv[argidx]);
        } else if (strcmp(argv[argidx], "--sysroot") == 0) {
            if (argc <= ++argidx) {
                std::cerr << CLIENT_NAME << ": --sysroot: missing argument." << std::endl;
                return false;
            }
            config.sysroot = std::string(argv[argidx]);
        } else if (strcmp(argv[argidx], "--sample-type") == 0) {
            if (argc <= ++argidx) {
                std::cerr << CLIENT_NAME << ": --sample-type: missing argument."
                          << std::endl;
                return false;
            }
            config.sample_type = std::string(argv[argidx]);
        } else if (strcmp(argv[argidx], "--sb:time-zero") == 0) {
            if (argc <= ++argidx) {
                std::cerr << CLIENT_NAME << ": --sb:time-zero: missing argument."
                          << std::endl;
                return false;
            }
            config.time_zero = std::string(argv[argidx]);
        } else if (strcmp(argv[argidx], "--sb:time-shift") == 0) {
            if (argc <= ++argidx) {
                std::cerr << CLIENT_NAME << ": --sb:time-shift: missing argument."
                          << std::endl;
                return false;
            }
            config.time_shift = std::string(argv[argidx]);
        } else if (strcmp(argv[argidx], "--sb:time-mult") == 0) {
            if (argc <= ++argidx) {
                std::cerr << CLIENT_NAME << ": --sb:time-mult: missing argument."
                          << std::endl;
                return false;
            }
            config.time_mult = std::string(argv[argidx]);
        } else if (strcmp(argv[argidx], "--sb:tsc-offset") == 0) {
            if (argc <= ++argidx) {
                std::cerr << CLIENT_NAME << ": --sb:tsc-offset: missing argument."
                          << std::endl;
                return false;
            }
            config.tsc_offset = std::string(argv[argidx]);
        } else if (strcmp(argv[argidx], "--sb:primary") == 0) {
            if (argc <= ++argidx) {
                std::cerr << CLIENT_NAME << ": --sb:primary: missing argument."
                          << std::endl;
                return false;
            }
            config.sb_primary_file = std::string(argv[argidx]);
        } else if (strcmp(argv[argidx], "--sb:secondary") == 0) {
            if (argc <= ++argidx) {
                std::cerr << CLIENT_NAME << ": --sb:secondary: missing argument."
                          << std::endl;
                return false;
            }
            config.sb_secondary_files.push_back(std::string(argv[argidx]));
        } else if (strcmp(argv[argidx], "--kernel-start") == 0) {
            if (argc <= ++argidx) {
                std::cerr << CLIENT_NAME << ": --kernel-start: missing argument."
                          << std::endl;
                return false;
            }
            config.kernel_start = std::string(argv[argidx]);
        } else if (strcmp(argv[argidx], "--kcore") == 0) {
            if (argc <= ++argidx) {
                std::cerr << CLIENT_NAME << ": --kcore: missing argument." << std::endl;
                return false;
            }
            config.kcore_path = std::string(argv[argidx]);
        } else if (strcmp(argv[argidx], "--cpu") == 0) {
            if (argc <= ++argidx) {
                std::cerr << CLIENT_NAME << ": --cpu: missing argument." << std::endl;
                return false;
            }
            config.cpu = std::string(argv[argidx]);
        } else {
            std::cerr << CLIENT_NAME << ": unknown option:" << argv[argidx] << "."
                      << std::endl;
            return false;
        }
        argidx++;
    }

    return true;
}

int
main(int argc, const char *argv[])
{
    options_t options = {};
    pt2ir_config_t config = {};

    /* Parse the command line.*/
    if (!process_args(argc, argv, config, options)) {
        return 1;
    }

    /* Convert the pt raw data to DR ir */
    pt2ir_t *ptconverter = new pt2ir_t(config);
    ptconverter->convert();

    if (options.print_stats == 1)
        print_stats(ptconverter);

    delete ptconverter;
    return 0;
}
