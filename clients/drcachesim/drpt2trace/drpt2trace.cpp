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
 * command-line tool for decoding a PT trace, and converting it into an memtrace composed
 * of 'trace_entry_t's
 * XXX: Currently, it only counts and prints the instruction count in the trace data.
 */

#include <stdio.h>
#include <string.h>
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
    printf("  --cpu none|f/m[/s]        set cpu to the given value and decode "
           "according to:\n");
    printf("                               none     spec (default)\n");
    printf("                               f/m[/s]  family/model[/stepping]\n");
    printf("  --mtc-freq <val>             set the MTC frequency to <val>.");
    printf("  --nom-freq <val>             set the nominal frequency to <val>.\n");
    printf("  --cpuid-0x15.eax <val>       set the value of cpuid[0x15].eax.\n");
    printf("  --cpuid-0x15.ebx <val>       set the value of cpuid[0x15].ebx.\n");
    printf("  --sb:sysroot <path>          prepend <path> to sideband filenames.\n");
    printf("  --sb:sample-type <val>       set perf_event_attr.sample_type to <val> "
           "(default: 0).\n");
    printf("  --sb:time-zero <val>         set perf_event_mmap_page.time_zero to <val> "
           "(default: 0).\n");
    printf("  --sb:time-shift <val>        set perf_event_mmap_page.time_shift to <val> "
           "(default: 0).\n");
    printf("  --sb:time-mult <val>         set perf_event_mmap_page.time_mult to <val> "
           "(default: 1).\n");
    printf("  --sb:tsc-offset <val>        show perf events <val> ticks earlier"
           "(<val> must be a hexadecimal integer and default: 0x0).\n");
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
        } else if (strcmp(argv[argidx], "--cpu") == 0) {
            if (argc <= ++argidx) {
                std::cerr << CLIENT_NAME << ": --cpu: missing argument." << std::endl;
                return false;
            }
            if (strcmp(argv[argidx], "none") != 0) {
                config.pt_config.cpu.vendor = 1;
                int family = 0, model = 0, stepping = 0;
                int num = sscanf(argv[argidx], "%d/%d/%d", &family, &model, &stepping);
                config.pt_config.cpu.family = family;
                config.pt_config.cpu.model = model;
                config.pt_config.cpu.stepping = stepping;
                if (num < 2) {
                    std::cerr << CLIENT_NAME << ": Invalid cpu type: " << argv[argidx]
                              << "." << std::endl;
                    return false;
                } else if (num == 2) {
                    config.pt_config.cpu.stepping = 0;
                } else {
                    /* Nothing */
                }
            }
        } else if (strcmp(argv[argidx], "--mtc-freq") == 0) {
            if (argc <= ++argidx) {
                std::cerr << CLIENT_NAME << ": --mtc-freq: missing argument."
                          << std::endl;
                return false;
            }
            int val = 0;
            if (sscanf(argv[argidx], "%d", &val) > 0) {
                config.pt_config.mtc_freq = val;
            } else {
                std::cerr << CLIENT_NAME << ": Invalid MTC frequency: " << argv[argidx]
                          << "." << std::endl;
                return false;
            }
        } else if (strcmp(argv[argidx], "--nom-freq") == 0) {
            if (argc <= ++argidx) {
                std::cerr << CLIENT_NAME << ": --nom-freq: missing argument."
                          << std::endl;
                return false;
            }
            int val = 0;
            if (sscanf(argv[argidx], "%d", &val) > 0) {
                config.pt_config.nom_freq = val;
            } else {
                std::cerr << CLIENT_NAME
                          << ": Invalid nominal frequency: " << argv[argidx] << "."
                          << std::endl;
                return false;
            }
        } else if (strcmp(argv[argidx], "--cpuid-0x15.eax") == 0) {
            if (argc <= ++argidx) {
                std::cerr << CLIENT_NAME << ": --cpuid-0x15.eax: missing argument."
                          << std::endl;
                return false;
            }
            int val = 0;
            if (sscanf(argv[argidx], "%d", &val) > 0) {
                config.pt_config.cpuid_0x15_eax = val;
            } else {
                std::cerr << CLIENT_NAME << ": Invalid cpuid[0x15].eax: " << argv[argidx]
                          << "." << std::endl;
                return false;
            }
        } else if (strcmp(argv[argidx], "--cpuid-0x15.ebx") == 0) {
            if (argc <= ++argidx) {
                std::cerr << CLIENT_NAME << ": --cpuid-0x15.ebx: missing argument."
                          << std::endl;
                return false;
            }
            int val = 0;
            if (sscanf(argv[argidx], "%d", &val) > 0) {
                config.pt_config.cpuid_0x15_ebx = val;
            } else {
                std::cerr << CLIENT_NAME << ": Invalid cpuid[0x15].ebx: " << argv[argidx]
                          << "." << std::endl;
                return false;
            }
        } else if (strcmp(argv[argidx], "--sb:sysroot") == 0) {
            if (argc <= ++argidx) {
                std::cerr << CLIENT_NAME << ": --sb:sysroot: missing argument."
                          << std::endl;
                return false;
            }
            config.sb_config.sysroot = std::string(argv[argidx]);
        } else if (strcmp(argv[argidx], "--sb:sample-type") == 0) {
            if (argc <= ++argidx) {
                std::cerr << CLIENT_NAME << ": --sb:sample-type: missing argument."
                          << std::endl;
                return false;
            }
            uint64_t val = 0;
            if (sscanf(argv[argidx], "%" PRIx64, &val) > 0) {
                config.sb_config.sample_type = val;
            } else {
                std::cerr << CLIENT_NAME << ": Invalid sample type: " << argv[argidx]
                          << "." << std::endl;
                return false;
            }
        } else if (strcmp(argv[argidx], "--sb:time-zero") == 0) {
            if (argc <= ++argidx) {
                std::cerr << CLIENT_NAME << ": --sb:time-zero: missing argument."
                          << std::endl;
                return false;
            }
            uint64_t val = 0;
            if (sscanf(argv[argidx], "%" PRIu64, &val) > 0) {
                config.sb_config.time_zero = val;
            } else {
                std::cerr << CLIENT_NAME
                          << ": Invalid perf_event_mmap_page.time_zero: " << argv[argidx]
                          << "." << std::endl;
                return false;
            }
        } else if (strcmp(argv[argidx], "--sb:time-shift") == 0) {
            if (argc <= ++argidx) {
                std::cerr << CLIENT_NAME << ": --sb:time-shift: missing argument."
                          << std::endl;
                return false;
            }
            uint32_t val = 0;
            if (sscanf(argv[argidx], "%" PRIu32, &val) > 0) {
                config.sb_config.time_shift = val;
            } else {
                std::cerr << CLIENT_NAME
                          << ": Invalid perf_event_mmap_page.time_shift: " << argv[argidx]
                          << "." << std::endl;
                return false;
            }
        } else if (strcmp(argv[argidx], "--sb:time-mult") == 0) {
            if (argc <= ++argidx) {
                std::cerr << CLIENT_NAME << ": --sb:time-mult: missing argument."
                          << std::endl;
                return false;
            }
            uint32_t val = 0;
            if (sscanf(argv[argidx], "%" PRIu32, &val) > 0) {
                config.sb_config.time_mult = val;
            } else {
                std::cerr << CLIENT_NAME
                          << ": Invalid perf_event_mmap_page.time_mult: " << argv[argidx]
                          << "." << std::endl;
                return false;
            }
        } else if (strcmp(argv[argidx], "--sb:tsc-offset") == 0) {
            if (argc <= ++argidx) {
                std::cerr << CLIENT_NAME << ": --sb:tsc-offset: missing argument."
                          << std::endl;
                return false;
            }
            uint64_t val = 0;
            if (sscanf(argv[argidx], "%" PRIx64, &val) > 0) {
                config.sb_config.tsc_offset = val;
            } else {
                std::cerr << CLIENT_NAME
                          << ": Invalid perf_event_mmap_page.tsc_offset: " << argv[argidx]
                          << "." << std::endl;
                return false;
            }
        } else if (strcmp(argv[argidx], "--sb:primary") == 0) {
            if (argc <= ++argidx) {
                std::cerr << CLIENT_NAME << ": --sb:primary: missing argument."
                          << std::endl;
                return false;
            }
            config.sb_primary_file_path = std::string(argv[argidx]);
        } else if (strcmp(argv[argidx], "--sb:secondary") == 0) {
            if (argc <= ++argidx) {
                std::cerr << CLIENT_NAME << ": --sb:secondary: missing argument."
                          << std::endl;
                return false;
            }
            config.sb_secondary_file_path_list.push_back(std::string(argv[argidx]));
        } else if (strcmp(argv[argidx], "--kernel-start") == 0) {
            if (argc <= ++argidx) {
                std::cerr << CLIENT_NAME << ": --kernel-start: missing argument."
                          << std::endl;
                return false;
            }
            uint64_t val = 0;
            if (sscanf(argv[argidx], "%" PRIx64, &val) > 0) {
                config.sb_config.kernel_start = val;
            } else {
                std::cerr << CLIENT_NAME << ": Invalid kernel start: " << argv[argidx]
                          << "." << std::endl;
                return false;
            }
        } else if (strcmp(argv[argidx], "--kcore") == 0) {
            if (argc <= ++argidx) {
                std::cerr << CLIENT_NAME << ": --kcore: missing argument." << std::endl;
                return false;
            }
            config.sb_config.kcore_path = std::string(argv[argidx]);
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

    if (options.print_stats == 1) {
        print_stats(ptconverter);
    }

    delete ptconverter;
    return 0;
}
