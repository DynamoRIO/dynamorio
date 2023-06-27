/* **********************************************************
 * Copyright (c) 2018 Google, LLC  All rights reserved.
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

#include <iostream>
#include "config_reader_unit_test.h"
#include "reader/config_reader.h"
#include "simulator/cache.h"
#include "simulator/cache_simulator_create.h"

namespace dynamorio {
namespace drmemtrace {

static void
check_cache(const std::map<std::string, cache_params_t> &caches, std::string name,
            std::string type, int core, uint64_t size, unsigned int assoc, bool inclusive,
            std::string parent_, std::string replace_policy, std::string prefetcher,
            std::string miss_file)
{
    auto cache_it = caches.find(name);
    if (cache_it == caches.end()) {
        std::cerr << "drcachesim config_reader_test failed (cache: " << name
                  << " unfound)\n";
        exit(1);
    }

    auto &cache = cache_it->second;

    if (cache.type != type || cache.core != core || cache.size != size ||
        cache.assoc != assoc || cache.inclusive != inclusive || cache.parent != parent_ ||
        cache.replace_policy != replace_policy || cache.prefetcher != prefetcher ||
        cache.miss_file != miss_file) {
        std::cerr << "drcachesim config_reader_test failed (cache: " << cache.name
                  << ")\n";
        exit(1);
    }
}

void
unit_test_config_reader(const char *testdir)
{
    cache_simulator_knobs_t knobs;
    std::map<std::string, cache_params_t> caches;
    std::string file_path = std::string(testdir) + "/single_core.conf";

    std::ifstream file_stream;
    file_stream.open(file_path);
    if (!file_stream.is_open()) {
        std::cerr << "Failed to open the config file: '" << file_path.c_str() << "'\n";
        exit(1);
    }

    config_reader_t config;
    if (!config.configure(&file_stream, knobs, caches)) {
        std::cerr << "drcachesim config_reader_test failed (config error)\n";
        exit(1);
    }

    if (knobs.num_cores != 1 || knobs.line_size != 64 || knobs.skip_refs != 1000000 ||
        knobs.warmup_refs != 0 || knobs.warmup_fraction != 0.8 ||
        knobs.sim_refs != 8888888 || knobs.cpu_scheduling != true || knobs.verbose != 0 ||
        knobs.model_coherence != true || knobs.use_physical != true) {
        std::cerr << "drcachesim config_reader_test failed (common params)\n";
        exit(1);
    }

    for (auto &cache_map : caches) {
        if (cache_map.first == "P0L1I") {
            check_cache(caches, "P0L1I", "instruction", 0, 65536, 8, false, "P0L2", "LRU",
                        "none", "");
        } else if (cache_map.first == "P0L1D") {
            check_cache(caches, "P0L1D", "data", 0, 65536, 8, false, "P0L2", "LRU",
                        "none", "");
        } else if (cache_map.first == "P0L2") {
            check_cache(caches, "P0L2", "unified", -1, 524288, 16, true, "LLC", "LRU",
                        "none", "");
        } else if (cache_map.first == "LLC") {
            check_cache(caches, "LLC", "unified", -1, 1048576, 16, true, "memory", "LRU",
                        "none", "misses.txt");
        } else {
            std::cerr << "drcachesim config_reader_test failed (unknown cache)\n";
            exit(1);
        }
    }
}

} // namespace drmemtrace
} // namespace dynamorio
