/* **********************************************************
 * Copyright (c) 2015-2023 Google, LLC  All rights reserved.
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
#undef NDEBUG
#include <assert.h>

#include "../tools/reuse_distance.h"
#include "../tools/reuse_distance_create.h"
#include "../common/memref.h"

namespace dynamorio {
namespace drmemtrace {

#define TEST_VERBOSE(N) (test_verbosity >= N)
unsigned int test_verbosity = 0;

// Helper routine to generate a basic memref for a specific address.
static memref_t
generate_memref(const addr_t addr, trace_type_t trace_type = TRACE_TYPE_READ)
{
    memref_t memref;
    memref.data.type = trace_type;
    memref.data.pid = 1;
    memref.data.tid = 2;
    memref.data.addr = addr;
    memref.data.size = 4;
    memref.data.pc = 0;
    return memref;
}

class reuse_distance_test_t : public reuse_distance_t {
public:
    explicit reuse_distance_test_t(const reuse_distance_knobs_t &knobs)
        : reuse_distance_t(knobs)
    {
        test_verbosity = knobs.verbose; // Set the file verbosity.
    }

    ~reuse_distance_test_t()
    {
        test_verbosity = 0; // Reset the file verbosity.
    }

    bool
    process_memref(const memref_t &memref) override
    {
        if (TEST_VERBOSE(1)) {
            std::cerr << "process_memref(addr0x" << std::hex << memref.data.addr
                      << std::dec << ")\n";
        }
        return reuse_distance_t::process_memref(memref);
    };

    // Make these methods public for testing.
    using reuse_distance_t::get_aggregated_results;
    using reuse_distance_t::print_histogram;

    std::unordered_map<memref_tid_t, shard_data_t *> &
    get_shard_map()
    {
        return shard_map_;
    }
};

// Helper class to return a non-repeating(*) sequence of addresses.
// (*) Overflow will cause the address to wrap, so technically it
// can repeat for large enough step sizes or generated address counts.
class address_generator_t {
public:
    address_generator_t(addr_t start_addr, int step_size)
        : address_(start_addr)
        , step_size_(step_size)
    {
    }

    addr_t
    next_address()
    {
        address_ += step_size_;
        return address_;
    }

private:
    addr_t address_ = 0;
    int step_size_ = 0;
};

// Helper routine to use a provided address_generator_t to process a series of memrefs
// with a reuse_distance_test object, generating a single reuse at <target_distance>.
bool
generate_target_distance_memrefs(int target_distance,
                                 reuse_distance_test_t &reuse_distance,
                                 address_generator_t &address_generator,
                                 trace_type_t trace_type = TRACE_TYPE_READ)
{
    auto start_addr = address_generator.next_address();
    bool success = reuse_distance.process_memref(generate_memref(start_addr, trace_type));
    for (int i = 0; success && i < target_distance; ++i) {
        success = reuse_distance.process_memref(
            generate_memref(address_generator.next_address(), trace_type));
    }
    return success &&
        reuse_distance.process_memref(generate_memref(start_addr, trace_type));
}

// Helper routine to verify all items in a vector of expected lines are in an istream,
// in the expected order.
bool
find_strings_in_stream(const std::vector<std::string> &expected_strings, std::istream &in)
{
    for (const auto &expect : expected_strings) {
        if (TEST_VERBOSE(1)) {
            std::cerr << "Expect: '" << expect << "'\n";
        }
        bool found = false;
        for (std::string line; std::getline(in, line);) {
            if (line == expect) {
                found = true;
                if (TEST_VERBOSE(1)) {
                    std::cerr << "  --> Got it!\n";
                }
                break;
            } else if (TEST_VERBOSE(1)) {
                std::cerr << "   Skip '" << line << "'\n";
            }
        }
        if (!found) {
            std::cerr << "FAIL: Did not find: '" << expect << "'\n";
            return false;
        }
    }
    return true;
}

// Test basic reuse-distance.
void
simple_reuse_distance_test()
{
    std::cerr << "simple_reuse_distance_test()\n";

    constexpr uint32_t LINE_SIZE = 64;

    constexpr uint32_t TEST_ADDRESS = 0x1000; // Arbitrary.
    constexpr uint32_t TEST_DISTANCE_START = 50;
    constexpr uint32_t TEST_DISTANCE_END = 2000;
    constexpr uint32_t TEST_DISTANCE_INCREMENT = 75;

    // Create a reuse_distance test object.
    reuse_distance_knobs_t knobs;
    knobs.line_size = LINE_SIZE;
    knobs.report_histogram = true;
    knobs.verbose = 0;
    reuse_distance_test_t reuse_distance(knobs);

    // Create address generator with a predictable access pattern.
    address_generator_t agen(TEST_ADDRESS, LINE_SIZE);

    int expected_histogram_size = 0;
    for (int tgt_dist = TEST_DISTANCE_START; tgt_dist < TEST_DISTANCE_END;
         tgt_dist += TEST_DISTANCE_INCREMENT) {
        ++expected_histogram_size;
        if (TEST_VERBOSE(1))
            std::cerr << "Testing reuse dist=" << tgt_dist << "\n";
        bool success = generate_target_distance_memrefs(tgt_dist, reuse_distance, agen);
        assert(success);
    }
    if (TEST_VERBOSE(1)) {
        reuse_distance.print_results();
    }

    assert(reuse_distance.get_shard_map().size() == 1);

    auto *shard = reuse_distance.get_aggregated_results();
    assert(shard->dist_map.size() == expected_histogram_size);
    // All test accesses are data accesses.
    assert(shard->dist_map_data.size() == expected_histogram_size);
    for (int tgt_dist = TEST_DISTANCE_START; tgt_dist < TEST_DISTANCE_END;
         tgt_dist += TEST_DISTANCE_INCREMENT) {
        const auto it = shard->dist_map.find(tgt_dist);
        // Should be exactly one access at each target distance.
        assert(it != shard->dist_map.end());
        assert(it->second == 1);
        // This same entry should be in the data histogram.
        assert(it->second == shard->dist_map_data.at(tgt_dist));
    }

    // When debugging, print the raw histogram data (unsorted).
    if (TEST_VERBOSE(2)) {
        for (const auto &dist_count : shard->dist_map) {
            std::cerr << "Dist: " << std::setw(12) << dist_count.first
                      << "  Count: " << std::setw(8) << dist_count.second << "\n";
        }
    }
}

// Test distance_limit on reuse-distance.
void
reuse_distance_limit_test()
{
    std::cerr << "reuse_distance_limit_test()\n";
    constexpr uint32_t LINE_SIZE = 32;
    constexpr uint32_t SKIP_LIST_DISTANCE = 75;
    constexpr uint32_t DISTANCE_LIMIT = 500;

    constexpr uint32_t TEST_ADDRESS = 0x2000;
    constexpr uint32_t TEST_DISTANCE_START = 150;
    constexpr uint32_t TEST_DISTANCE_END = 1000;
    constexpr uint32_t TEST_DISTANCE_INCREMENT = 100;

    // Create the reuse_distance test object.
    reuse_distance_knobs_t knobs;
    knobs.line_size = LINE_SIZE;
    knobs.report_histogram = true;
    knobs.skip_list_distance = SKIP_LIST_DISTANCE;
    knobs.distance_limit = DISTANCE_LIMIT;
    reuse_distance_test_t reuse_distance(knobs);

    // Generate a simple stream of references with a predictable reuse pattern.
    // Any multiple of LINE_SIZE for the stride is fine.
    address_generator_t agen(TEST_ADDRESS, 5 * LINE_SIZE);
    int expected_pruned_address_hits = 0;
    for (int tgt_dist = TEST_DISTANCE_START; tgt_dist < TEST_DISTANCE_END;
         tgt_dist += TEST_DISTANCE_INCREMENT) {
        if (TEST_VERBOSE(1))
            std::cerr << "Testing reuse dist=" << tgt_dist << "\n";
        bool success = generate_target_distance_memrefs(tgt_dist, reuse_distance, agen);
        expected_pruned_address_hits += (tgt_dist > DISTANCE_LIMIT);
        assert(success);
    }
    assert(reuse_distance.get_shard_map().size() == 1);

    auto *shard = reuse_distance.get_aggregated_results();
    if (TEST_VERBOSE(1)) {
        reuse_distance.print_results();
        std::cerr << "List Size Limit Reuse Distance.\n";
        std::cerr << " Aggregated Shard:  "
                  << " total_refs=" << shard->total_refs
                  << " pruned_address_count=" << shard->pruned_address_count
                  << " pruned_address_hits=" << shard->pruned_address_hits << "\n";
        if (TEST_VERBOSE(2)) {
            for (const auto &addr_count : shard->dist_map) {
                std::cerr << "Addr: " << std::hex << std::setw(12) << addr_count.first
                          << "  Count: " << std::dec << std::setw(8) << addr_count.second
                          << "\n";
            }
        }
    }
    // The pruning logic operates on the cache_map only, so it should be
    // exactly at the size limit given the input stream for this test.
    // This prevents the distance map from ever exceeding this limit, but
    // the distance map should be shorter in this test due to the sparse distance
    // hits in the loop.
    assert(shard->cache_map.size() == DISTANCE_LIMIT);
    assert(shard->dist_map.size() < DISTANCE_LIMIT);
    assert(shard->dist_map_data.size() < DISTANCE_LIMIT);
    assert(shard->pruned_address_hits == expected_pruned_address_hits);
    assert(shard->pruned_address_count > expected_pruned_address_hits);
    // Verify the distance histogram.
    for (int tgt_dist = TEST_DISTANCE_START; tgt_dist < TEST_DISTANCE_END;
         tgt_dist += TEST_DISTANCE_INCREMENT) {
        const auto it = shard->dist_map.find(tgt_dist);
        if (tgt_dist < DISTANCE_LIMIT) {
            assert(it != shard->dist_map.end());
            assert(it->second == 1);
        } else {
            assert(it == shard->dist_map.end());
        }
    }
}

// Test print_histogram with empty input vector.
void
print_histogram_empty_test()
{
    std::cerr << "print_histogram_empty_test()\n";

    // Create a reuse_distance test object.
    reuse_distance_knobs_t knobs;
    reuse_distance_test_t reuse_distance(knobs);

    // Create an empty histogram vector and distance histogram.
    std::vector<std::pair<int64_t, int64_t>> sorted;
    reuse_distance_t::distance_histogram_t distance_map_data;

    // Make sure print_histogram handles this case without crashing.
    std::stringstream output_string;
    reuse_distance.print_histogram(output_string, /*count=*/0, sorted, distance_map_data);

    // If the title string is in the output, that's good enough.
    std::vector<std::string> expected_strings = {
        "Reuse distance histogram:",
    };
    bool test_passes = find_strings_in_stream(expected_strings, output_string);
    assert(test_passes);
}

// Test print_histogram with multiplier of 1.0 (no geometric scaling).
void
print_histogram_mult_1p0_test()
{
    std::cerr << "print_histogram_mult_1p0_test()\n";

    // Create the reuse_distance test object.
    reuse_distance_knobs_t knobs;
    knobs.histogram_bin_multiplier = 1.0;
    reuse_distance_test_t reuse_distance(knobs);

    // Fill in a sorted histogram vector.
    std::vector<std::pair<int64_t, int64_t>> sorted;
    // Also put some matching entries in a data histogram.
    reuse_distance_t::distance_histogram_t distance_map_data;

    constexpr int N = 100;

    int64_t count = 0;
    for (int i = 0; i < N; ++i) {
        sorted.emplace_back(i, 1);
        count += sorted.back().second;
        // Put 1/3 of these in the data map as well.
        if (i % 3 == 0) {
            distance_map_data[i] += sorted.back().second;
        }
    }

    std::stringstream output_string;
    reuse_distance.print_histogram(output_string, count, sorted, distance_map_data);
    if (TEST_VERBOSE(2)) {
        std::cout << output_string.str() << "\n";
    }

    // Look for a few key strings in the output.
    std::vector<std::string> expected_strings = {
        "Distance       Count  Percent  Cumulative  :       Count  Percent  Cumulative",
        "       0           1       1%       1%     :           1       1%       1%",
        "       1           1       1%       2%     :           0       0%       1%",
        "      99           1       1%     100%     :           1       1%      34%",
    };
    bool test_passes = find_strings_in_stream(expected_strings, output_string);
    assert(test_passes);
}

// Test print_histogram with multiplier of 1.2 (geometric scaling).
void
print_histogram_mult_1p2_test()
{
    std::cerr << "print_histogram_mult_1p2_test()\n";

    // Create the reuse_distance test object.
    reuse_distance_knobs_t knobs;
    knobs.histogram_bin_multiplier = 1.2;
    reuse_distance_test_t reuse_distance(knobs);

    // Fill in a sorted histogram vector.
    std::vector<std::pair<int64_t, int64_t>> sorted;
    reuse_distance_t::distance_histogram_t distance_map_data;

    constexpr int N = 100;

    int64_t count = 0;
    for (int i = 0; i < N; ++i) {
        sorted.emplace_back(i, 2);
        count += sorted.back().second;
        // Put 1/4 of these in the data map as well.
        if (i % 4 == 0) {
            distance_map_data[i] += sorted.back().second;
        }
    }

    std::stringstream output_string;
    reuse_distance.print_histogram(output_string, count, sorted, distance_map_data);
    if (TEST_VERBOSE(2)) {
        std::cout << output_string.str() << "\n";
    }

    // Look for a few key strings in the output.
    std::vector<std::string> expected_strings = {
        "Reuse distance histogram bin multiplier: 1.2",
        "Distance [min-max]        Count  Percent  Cumulative  :"
        "       Count  Percent  Cumulative",
        "       0 -        0           2       1%       1%     :"
        "           2       1%       1%",
        "       1 -        1           2       1%       2%     :"
        "           0       0%       1%",
        "      80 -       97          36      18%      98%     :"
        "          10       5%      25%",
        "      98 -       99           4       2%     100%     :"
        "           0       0%      25%",
    };
    bool test_passes = find_strings_in_stream(expected_strings, output_string);
    assert(test_passes);
}

// Test the split of "everything" and "data" reuse-distance histogram.
void
data_histogram_test()
{
    std::cerr << "data_histogram_test()\n";

    constexpr uint32_t LINE_SIZE = 32;

    constexpr uint32_t TEST_ADDRESS = 0x1000; // Arbitrary.
    constexpr uint32_t TEST_DISTANCE_START = 50;
    constexpr uint32_t TEST_DISTANCE_END = 2000;
    constexpr uint32_t TEST_DISTANCE_INCREMENT = 75;

    // Create a reuse_distance test object.
    reuse_distance_knobs_t knobs;
    knobs.line_size = LINE_SIZE;
    knobs.report_histogram = true;
    knobs.verbose = 0;
    reuse_distance_test_t reuse_distance(knobs);

    // Create address generator with a predictable access pattern.
    address_generator_t agen(TEST_ADDRESS, LINE_SIZE);

    int instruction_hits = 0;
    int data_hits = 0;
    // Simple function to decide if a given access should be TRACE_TYPE_INSTR.
    auto use_instr_type = [&](int distance) {
        return (distance / TEST_DISTANCE_INCREMENT % 3) == 0;
    };
    for (int tgt_dist = TEST_DISTANCE_START; tgt_dist < TEST_DISTANCE_END;
         tgt_dist += TEST_DISTANCE_INCREMENT) {
        if (TEST_VERBOSE(1)) {
            std::cerr << "Testing reuse dist=" << tgt_dist << "\n";
        }
        auto memref_type = use_instr_type(tgt_dist) ? TRACE_TYPE_INSTR : TRACE_TYPE_READ;
        bool success =
            generate_target_distance_memrefs(tgt_dist, reuse_distance, agen, memref_type);
        if (use_instr_type(tgt_dist)) {
            ++instruction_hits;
        } else {
            ++data_hits;
        }
        assert(success);
    }
    if (TEST_VERBOSE(1)) {
        reuse_distance.print_results();
    }

    assert(reuse_distance.get_shard_map().size() == 1);

    auto *shard = reuse_distance.get_aggregated_results();

    assert(data_hits > 0);
    assert(shard->data_refs > data_hits);
    assert(shard->dist_map_data.size() == data_hits);
    assert(shard->dist_map.size() == instruction_hits + data_hits);
    for (int tgt_dist = TEST_DISTANCE_START; tgt_dist < TEST_DISTANCE_END;
         tgt_dist += TEST_DISTANCE_INCREMENT) {
        const auto it = shard->dist_map.find(tgt_dist);
        // Should be exactly one access at each target distance.
        assert(it != shard->dist_map.end());
        assert(it->second == 1);

        // If it's not an instruction, dist_map_data should have also
        // recorded exactly 1 hit.
        if (use_instr_type(tgt_dist)) {
            assert(shard->dist_map_data.find(tgt_dist) == shard->dist_map_data.end());
        } else {
            assert(shard->dist_map_data.find(tgt_dist) != shard->dist_map_data.end());
            assert(shard->dist_map_data.at(tgt_dist) == 1);
        }
    }

    // When debugging, print the raw histogram data (unsorted).
    if (TEST_VERBOSE(2)) {
        for (const auto &dist_count : shard->dist_map_data) {
            std::cerr << "Dist: " << std::setw(12) << dist_count.first
                      << "  Count: " << std::setw(8) << dist_count.second << "\n";
        }
    }
}

int
test_main(int argc, const char *argv[])
{
    print_histogram_empty_test();
    print_histogram_mult_1p0_test();
    print_histogram_mult_1p2_test();
    simple_reuse_distance_test();
    reuse_distance_limit_test();
    data_histogram_test();
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
