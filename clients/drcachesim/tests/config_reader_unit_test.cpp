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
#include "reader/config_reader_helpers.h"
#include "simulator/cache.h"
#include "simulator/cache_simulator_create.h"

#include <cassert>

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

// Return true if <a> and <b> are within epsilon of one-another.
// Assumes epsilon is non-negative.
bool
fp_near(double a, double b, double epsilon)
{
    return (a - b < epsilon) && (b - a < epsilon);
}

void
unit_test_config_reader(const std::string &testdir)
{
    cache_simulator_knobs_t knobs;
    std::map<std::string, cache_params_t> caches;
    std::string file_path = testdir + "/single_core.conf";

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
        knobs.warmup_refs != 0 || !fp_near(knobs.warmup_fraction, 0.8, 0.001) ||
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

void
unit_test_config_reader_basic()
{
    {
        // Incorrect: Parent specified as nested structure.
        cache_simulator_knobs_t knobs;
        std::map<std::string, cache_params_t> caches;

        std::stringstream ss;
        ss.str("num_cores 1\n"
               "L1I{type instruction core 0 parent {name L2}}\n"
               "L1D{type data core 0 parent L2}\n"
               "L2{type unified}\n");
        config_reader_t config;
        if (config.configure(&ss, knobs, caches)) {
            std::cerr << "drcachesim config_reader_basic test failed (parent)"
                      << std::endl;
            exit(1);
        }
    }

    {
        // Incorrect: Miss file specified as nested structure.
        cache_simulator_knobs_t knobs;
        std::map<std::string, cache_params_t> caches;

        std::stringstream ss;
        ss.str("num_cores 1\n"
               "L1I{type instruction core 0 parent L2}\n"
               "L1D{type data core 0 parent L2}\n"
               "L2{type unified miss_file {name 1.txt}}\n");
        config_reader_t config;
        if (config.configure(&ss, knobs, caches)) {
            std::cerr << "drcachesim config_reader_basic test failed (parent)"
                      << std::endl;
            exit(1);
        }
    }
}

void
unit_test_inclusion_policy()
{
    {
        // Correct: Inclusion policy not specified.
        cache_simulator_knobs_t knobs;
        std::map<std::string, cache_params_t> caches;

        std::stringstream ss;
        ss.str("num_cores 1\n"
               "L1I{type instruction core 0 parent L2}\n"
               "L1D{type data core 0 parent L2}\n"
               "L2{type unified}\n");
        config_reader_t config;
        if (!config.configure(&ss, knobs, caches)) {
            std::cerr << "drcachesim inclusion_policy_test failed (default NINE)"
                      << std::endl;
            exit(1);
        }
    }

    {
        // Correct: inclusive=false exclusive=false (Non-Inclusive Non-Exclusive).
        cache_simulator_knobs_t knobs;
        std::map<std::string, cache_params_t> caches;

        std::stringstream ss;
        ss.str("num_cores 1\n"
               "L1I{type instruction core 0 parent L2}\n"
               "L1D{type data core 0 parent L2}\n"
               "L2{type unified inclusive false exclusive false}\n");
        config_reader_t config;
        if (!config.configure(&ss, knobs, caches)) {
            std::cerr << "drcachesim inclusion_policy_test failed (explicit NINE)"
                      << std::endl;
            exit(1);
        }
    }

    {
        // Correct: inclusive.
        cache_simulator_knobs_t knobs;
        std::map<std::string, cache_params_t> caches;

        std::stringstream ss;
        ss.str("num_cores 1\n"
               "L1I{type instruction core 0 parent L2}\n"
               "L1D{type data core 0 parent L2}\n"
               "L2{type unified inclusive true}\n");
        config_reader_t config;
        if (!config.configure(&ss, knobs, caches)) {
            std::cerr << "drcachesim inclusion_policy_test failed (inclusive)"
                      << std::endl;
            exit(1);
        }
    }

    {
        // Correct: inclusive=true exclusive=false.
        cache_simulator_knobs_t knobs;
        std::map<std::string, cache_params_t> caches;

        std::stringstream ss;
        ss.str("num_cores 1\n"
               "L1I{type instruction core 0 parent L2}\n"
               "L1D{type data core 0 parent L2}\n"
               "L2{type unified inclusive true exclusive false}\n");
        config_reader_t config;
        if (!config.configure(&ss, knobs, caches)) {
            std::cerr
                << "drcachesim inclusion_policy_test failed (inclusive, not exclusive)"
                << std::endl;
            exit(1);
        }
    }

    {
        // Correct: exclusive.
        cache_simulator_knobs_t knobs;
        std::map<std::string, cache_params_t> caches;

        std::stringstream ss;
        ss.str("num_cores 1\n"
               "L1I{type instruction core 0 parent L2}\n"
               "L1D{type data core 0 parent L2}\n"
               "L2{type unified exclusive true}\n");
        config_reader_t config;
        if (!config.configure(&ss, knobs, caches)) {
            std::cerr << "drcachesim inclusion_policy_test failed (exclusive)"
                      << std::endl;
            exit(1);
        }
    }

    {
        // Correct: exclusive=true inclusive=false.
        cache_simulator_knobs_t knobs;
        std::map<std::string, cache_params_t> caches;

        std::stringstream ss;
        ss.str("num_cores 1\n"
               "L1I{type instruction core 0 parent L2}\n"
               "L1D{type data core 0 parent L2}\n"
               "L2{type unified exclusive true inclusive false}\n");
        config_reader_t config;
        if (!config.configure(&ss, knobs, caches)) {
            std::cerr
                << "drcachesim inclusion_policy_test failed (exclusive, not inclusive)"
                << std::endl;
            exit(1);
        }
    }

    {
        // Incorrect: Both inclusive and exclusive.
        cache_simulator_knobs_t knobs;
        std::map<std::string, cache_params_t> caches;

        std::stringstream ss;
        ss.str("num_cores 1\n"
               "L1I{type instruction core 0 parent L2}\n"
               "L1D{type data core 0 parent L2}\n"
               "L2{type unified inclusive true exclusive true}\n");
        config_reader_t config;
        if (config.configure(&ss, knobs, caches)) {
            // Check will fail: both inclusive and exclusive not allowed.
            std::cerr << "drcachesim inclusion_policy_test failed (conflicting exclusive "
                         "and inclusive)"
                      << std::endl;
            exit(1);
        }
    }
}

void
unit_test_get_type_name()
{
    std::cerr << "Testing get_type_name" << std::endl;
    assert(strcmp(get_type_name<bool>(), "bool") == 0);
    assert(strcmp(get_type_name<int>(), "int") == 0);
    assert(strcmp(get_type_name<unsigned int>(), "unsigned int") == 0);
    assert(strcmp(get_type_name<float>(), "float") == 0);
    assert(strcmp(get_type_name<double>(), "double") == 0);
}

void
unit_test_parse_value()
{
    std::cerr << "Testing parse_value" << std::endl;

    // Parse boolean values.
    bool dst_bool = false;
    // Supported values are: true, True, TRUE, false, False, FALSE.
    assert(parse_value("true", &dst_bool) && dst_bool);
    assert(parse_value("False", &dst_bool) && !dst_bool);
    assert(parse_value("True", &dst_bool) && dst_bool);
    assert(parse_value("false", &dst_bool) && !dst_bool);
    assert(parse_value("TRUE", &dst_bool) && dst_bool);
    assert(parse_value("FALSE", &dst_bool) && !dst_bool);
    // Non-supported values. parse_value returns false.
    assert(!parse_value("0", &dst_bool));   // Numbers not supported
    assert(!parse_value("1", &dst_bool));   // Numbers not supported
    assert(!parse_value("abc", &dst_bool)); // Random strings not supported

    // Parse signed integer values.
    int dst_int = -1;
    // Supported values, both positive and negative.
    assert(parse_value("0", &dst_int) && dst_int == 0);
    assert(parse_value("1", &dst_int) && dst_int == 1);
    assert(parse_value("-123", &dst_int) && dst_int == -123);
    // Non-supported values. parse_value returns false.
    assert(!parse_value("abc", &dst_int));
    assert(!parse_value("123f", &dst_int));
    assert(!parse_value("a123", &dst_int));

    // Parse unsigned integer values.
    unsigned int dst_uint = -1;
    assert(parse_value("0", &dst_uint) && dst_uint == 0);
    assert(parse_value("123", &dst_uint) && dst_uint == 123);
    // Negative values not supported. parse_value returns false.
    assert(!parse_value("-1", &dst_uint));

    // Parse double values.
    double dst_double = .0;
    assert(parse_value("123", &dst_double) && dst_double == 123);
    assert(parse_value("123.4", &dst_double) && dst_double == 123.4);
    assert(parse_value("-123.45", &dst_double) && dst_double == -123.45);
    // Non supported strings. parse_value returns false.
    assert(!parse_value("abc", &dst_double));
    assert(!parse_value("1.abc", &dst_double));
    assert(!parse_value("a1.23", &dst_double));
}

void
unit_test_read_parameter_map_simple()
{
    // Valid configurations.
    {
        // Simple key-value pair.
        config_t config;
        std::istringstream ss { "key 1" };
        assert(read_param_map(&ss, &config));
        assert(config["key"].type == config_param_node_t::SCALAR &&
               config["key"].value.compare("1") == 0);
    }

    {
        // Several key-value pairs.
        config_t config;
        std::istringstream ss { "key1 1 key2 2 key3 123" };
        assert(read_param_map(&ss, &config));
        assert(config["key1"].type == config_param_node_t::SCALAR &&
               config["key1"].value.compare("1") == 0 &&
               config["key2"].type == config_param_node_t::SCALAR &&
               config["key2"].value.compare("2") == 0 &&
               config["key3"].type == config_param_node_t::SCALAR &&
               config["key3"].value.compare("123") == 0);
    }

    {
        // Multiline configuration.
        config_t config;
        std::istringstream ss { "key1 1\nkey2 2\nkey3 123" };
        assert(read_param_map(&ss, &config));
        assert(config["key1"].type == config_param_node_t::SCALAR &&
               config["key1"].value.compare("1") == 0 &&
               config["key2"].type == config_param_node_t::SCALAR &&
               config["key2"].value.compare("2") == 0 &&
               config["key3"].type == config_param_node_t::SCALAR &&
               config["key3"].value.compare("123") == 0);
    }

    {
        // Multiline configuration with comments and extra spaces.
        config_t config;
        std::istringstream ss {
            "key1 1\nkey2 2 // This is the comment key3 123\n   key4   4\t "
        };
        assert(read_param_map(&ss, &config));
        assert(config["key1"].type == config_param_node_t::SCALAR &&
               config["key1"].value.compare("1") == 0 &&
               config["key2"].type == config_param_node_t::SCALAR &&
               config["key2"].value.compare("2") == 0 &&
               config.find("key3") == config.end() &&
               config["key4"].type == config_param_node_t::SCALAR &&
               config["key4"].value.compare("4") == 0);
    }

    // Invalid configuration.
    {
        // Not good stream.
        config_t config;
        std::istringstream ss { "key1 1" };
        ss.setstate(std::ios_base::failbit);
        assert(!read_param_map(&ss, &config));
    }

    {
        // Missed value.
        config_t config;
        std::istringstream ss { "key1 1\nkey2 // This is the comment key3 123" };
        assert(!read_param_map(&ss, &config));
    }
}

void
unit_test_read_parameter_map_nested()
{
    {
        // Simple nested configuration.
        config_t config;
        std::istringstream ss { "key0{key1 1 key2 2 key3 123}" };
        assert(read_param_map(&ss, &config));
        assert(config["key0"].type == config_param_node_t::MAP);
        auto &key0 = config["key0"].children;
        assert(key0["key1"].type == config_param_node_t::SCALAR &&
               key0["key1"].value.compare("1") == 0 &&
               key0["key2"].type == config_param_node_t::SCALAR &&
               key0["key2"].value.compare("2") == 0 &&
               key0["key3"].type == config_param_node_t::SCALAR &&
               key0["key3"].value.compare("123") == 0);
    }

    {
        // Multi-level nested configuration.
        config_t config;
        std::istringstream ss { "key0{key1 1 key2 {key3 123 key4 4}}" };
        assert(read_param_map(&ss, &config));
        assert(config["key0"].type == config_param_node_t::MAP);
        auto &key0 = config["key0"].children;
        assert(key0["key1"].type == config_param_node_t::SCALAR &&
               key0["key1"].value.compare("1") == 0);
        assert(key0["key2"].type == config_param_node_t::MAP);
        auto &key2 = key0["key2"].children;
        assert(key2["key3"].type == config_param_node_t::SCALAR &&
               key2["key3"].value.compare("123") == 0 &&
               key2["key4"].type == config_param_node_t::SCALAR &&
               key2["key4"].value.compare("4") == 0);
    }

    // Invalid configuration.
    {
        // Empty map.
        config_t config;
        std::istringstream ss { "key1 {}" };
        assert(!read_param_map(&ss, &config));
    }

    {
        // Missed enclosing brace.
        config_t config;
        std::istringstream ss { "key1 {key2 2" };
        assert(!read_param_map(&ss, &config));
    }

    {
        // Braces without parameter name.
        config_t config;
        std::istringstream ss { "key1 1 {key2 2}" };
        assert(!read_param_map(&ss, &config));
    }
}

void
unit_test_read_parameter_map()
{
    std::cerr << "Testing read_parameter_map" << std::endl;
    unit_test_read_parameter_map_simple();
    unit_test_read_parameter_map_nested();
}

} // namespace drmemtrace
} // namespace dynamorio
