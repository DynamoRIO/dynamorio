/* **********************************************************
 * Copyright (c) 2023 Google, Inc.  All rights reserved.
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

#include <assert.h>
#include <iostream>
#include <regex>

#include "dr_api.h"
#include "droption.h"
#include "analyzer_multi.h"

namespace dynamorio {
namespace drmemtrace {

using ::dynamorio::droption::droption_parser_t;
using ::dynamorio::droption::DROPTION_SCOPE_FRONTEND;

#if (defined(X86_64) || defined(ARM_64)) && defined(HAS_ZIP)
static std::string
run_analyzer(int argc, const char *args[])
{
    // Avoid accumulation of option values across runs.
    droption_parser_t::clear_values();

    // Capture output.
    std::stringstream output;
    std::streambuf *prev_buf = std::cerr.rdbuf(output.rdbuf());

    std::string parse_err;
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_FRONTEND, argc, args, &parse_err,
                                       nullptr)) {
        std::cerr << "Failed to parse: " << parse_err << "\n";
    }
    analyzer_multi_t analyzer;
    assert(!!analyzer);
    bool res = analyzer.run();
    assert(res);
    res = analyzer.print_stats();
    assert(res);

    std::cerr.rdbuf(prev_buf);

#    if VERBOSE
    std::cerr << "Output: |" << output.str() << "|\n";
#    endif
    return output.str();
}
#endif

static void
test_real_files(const char *testdir)
{
    std::cerr << "\n----------------\nTesting real files\n";
    // Since 32-bit memref_t is a different size we limit these to 64-bit builds.
#if (defined(X86_64) || defined(ARM_64)) && defined(HAS_ZIP)
    std::string dir = std::string(testdir) + "/drmemtrace.threadsig.x64.tracedir";
    // We sanity-check the runtime options.  This larger multi-thread trace does
    // result in non-determinism so we can't do exact matches; we rely on the
    // scheduler_unit_tests and tests for the forthcoming schedule_stats tool
    // for that.
    {
        // Test thread-sharded with defaults.
        const char *args[] = { "<exe>", "-simulator_type", "basic_counts", "-indir",
                               dir.c_str() };
        std::string output = run_analyzer(sizeof(args) / sizeof(args[0]), args);
        assert(std::regex_search(output, std::regex(R"DELIM(Basic counts tool results:
Total counts:
      638938 total .*
(.|\n)*
Thread [0-9]+ counts:
)DELIM")));
        std::regex count("Thread");
        assert(std::distance(std::sregex_iterator(output.begin(), output.end(), count),
                             std::sregex_iterator()) == 8);
    }
    {
        // Test core-sharded with defaults.
        const char *args[] = { "<exe>",        "-core_sharded", "-simulator_type",
                               "basic_counts", "-indir",        dir.c_str() };
        std::string output = run_analyzer(sizeof(args) / sizeof(args[0]), args);
        assert(std::regex_search(output, std::regex(R"DELIM(Basic counts tool results:
Total counts:
      638938 total .*
(.|\n)*
Core [0-9] counts:
(.|\n)*
 *[0-9]+ threads
(.|\n)*
)DELIM")));
        std::regex count("Core");
        assert(std::distance(std::sregex_iterator(output.begin(), output.end(), count),
                             std::sregex_iterator()) == 4);
    }
    {
        // Test core-sharded with time quantum.
        const char *args[] = { "<exe>",        "-core_sharded", "-simulator_type",
                               "basic_counts", "-indir",        dir.c_str(),
                               "-sched_time" };
        std::string output = run_analyzer(sizeof(args) / sizeof(args[0]), args);
        assert(std::regex_search(output, std::regex(R"DELIM(Basic counts tool results:
Total counts:
      638938 total .*
(.|\n)*
Core [0-9] counts:
(.|\n)*
 *[0-9]+ threads
(.|\n)*
)DELIM")));
        std::regex count("Core");
        assert(std::distance(std::sregex_iterator(output.begin(), output.end(), count),
                             std::sregex_iterator()) == 4);
    }
    {
        // Test core-sharded with non-default core count and custom quantum and no
        // time ordering, which results in slightly more even core instr counts
        // but it's hard to check those here.
        // TODO i#5694: Add more targeted checks once we have schedule_stats.
        const char *args[] = { "<exe>",
                               "-core_sharded",
                               "-simulator_type",
                               "basic_counts",
                               "-indir",
                               dir.c_str(),
                               "-cores",
                               "3",
                               "-sched_quantum",
                               "60000",
                               "-no_sched_order_time" };
        std::string output = run_analyzer(sizeof(args) / sizeof(args[0]), args);
        assert(std::regex_search(output, std::regex(R"DELIM(Basic counts tool results:
Total counts:
      638938 total \(fetched\) instructions
(.|\n)*
Core [0-9] counts:
(.|\n)*
 *[0-9]+ threads
(.|\n)*
)DELIM")));
        std::regex count("Core");
        assert(std::distance(std::sregex_iterator(output.begin(), output.end(), count),
                             std::sregex_iterator()) == 3);
    }
    {
        // Test core-sharded with replay-as-traced.
        std::string cpu_file = std::string(testdir) +
            "/drmemtrace.threadsig.x64.tracedir/cpu_schedule.bin.zip";
        const char *args[] = { "<exe>",
                               "-core_sharded",
                               "-simulator_type",
                               "basic_counts",
                               "-indir",
                               dir.c_str(),
                               "-cores",
                               "7",
                               "-cpu_schedule_file",
                               cpu_file.c_str() };
        std::string output = run_analyzer(sizeof(args) / sizeof(args[0]), args);
        assert(std::regex_search(output, std::regex(R"DELIM(Basic counts tool results:
Total counts:
      638938 total \(fetched\) instructions
(.|\n)*
           8 total threads
(.|\n)*
Core 5 counts:
      175765 \(fetched\) instructions
(.|\n)*
           2 threads
(.|\n)*
Core 9 counts:
       87891 \(fetched\) instructions
(.|\n)*
           1 threads
(.|\n)*
Core 0 counts:
       87884 \(fetched\) instructions
(.|\n)*
           1 threads
(.|\n)*
Core 10? counts:
       87875 \(fetched\) instructions
(.|\n)*
           1 threads
(.|\n)*
Core 10? counts:
       87875 \(fetched\) instructions
(.|\n)*
           1 threads
(.|\n)*
Core 11 counts:
       82508 \(fetched\) instructions
(.|\n)*
           1 threads
(.|\n)*
Core 8 counts:
       29140 \(fetched\) instructions
(.|\n)*
           1 threads
(.|\n)*
)DELIM")));
    }
    {
        // Test record-replay.
        std::string record_file = "tmp_core_sharded_replay.zip";
        const char *record_args[] = { "<exe>",           "-core_sharded",
                                      "-simulator_type", "basic_counts",
                                      "-indir",          dir.c_str(),
                                      "-cores",          "3",
                                      "-record_file",    record_file.c_str() };
        std::string record_out =
            run_analyzer(sizeof(record_args) / sizeof(record_args[0]), record_args);
        assert(std::regex_search(record_out, std::regex(R"DELIM(Basic counts tool results:
Total counts:
      638938 total \(fetched\) instructions
(.|\n)*
Core .*
(.|\n)*
 *[0-9]+ threads
(.|\n)*
)DELIM")));
        const char *replay_args[] = { "<exe>",           "-core_sharded",
                                      "-simulator_type", "basic_counts",
                                      "-indir",          dir.c_str(),
                                      "-cores",          "3",
                                      "-replay_file",    record_file.c_str() };
        std::string replay_out =
            run_analyzer(sizeof(replay_args) / sizeof(replay_args[0]), replay_args);
        assert(replay_out == record_out);
    }
#endif
}

int
test_main(int argc, const char *argv[])
{
    // Takes in a path to the tests/ src dir.
    assert(argc == 2);
    dr_standalone_init();

    test_real_files(argv[1]);

    dr_standalone_exit();
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
