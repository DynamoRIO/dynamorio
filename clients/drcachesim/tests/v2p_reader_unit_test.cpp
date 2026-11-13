/* **********************************************************
 * Copyright (c) 2024-2026 Google, LLC  All rights reserved.
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

#include "v2p_reader_unit_test.h"

#include <cerrno>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <vector>

#include "reader/v2p_reader.h"
#if defined(LINUX) && (defined(X64) || defined(__x86_64__) || defined(__aarch64__)) && \
    !defined(RISCV64) && !defined(__riscv)
#    include <unistd.h>
#    include "tracer/v2p_tracefs/v2p_tracker.h"
#endif

namespace dynamorio {
namespace drmemtrace {

static void
check_v2p_info(const v2p_info_t &v2p_info)
{
    // Change NUM_ENTRIES if clients/drcachesim/tests/v2p_example.textproto is updated.
    // Must be equal to the number of "address_mapping {...}" blocks in the textproto.
    constexpr size_t NUM_ENTRIES = 4;
    if (v2p_info.v2p_map.size() != NUM_ENTRIES) {
        std::cerr << "v2p_map incorrect number of entries. Expected " << NUM_ENTRIES
                  << " got " << v2p_info.v2p_map.size() << ".\n";
        exit(1);
    }

    // Virtual and physical addresses must be aligned with
    // clients/drcachesim/tests/v2p_example.textproto.
    const std::vector<addr_t> virtual_addresses = { 0x123, 0x456, 0x0, 0x789 };
    const std::vector<addr_t> physical_addresses = { 0x3, 0x4, 0x6, 0x5 };
    for (size_t i = 0; i < virtual_addresses.size(); ++i) {
        auto key_val = v2p_info.v2p_map.find(virtual_addresses[i]);
        if (key_val != v2p_info.v2p_map.end()) {
            if (key_val->second != physical_addresses[i]) {
                std::cerr << "v2p_map incorrect physical address. Expected "
                          << physical_addresses[i] << " got " << key_val->second << ".\n";
                exit(1);
            }
        } else {
            std::cerr << "v2p_map incorrect virtual address. Expected "
                      << virtual_addresses[i] << " not found.\n";
            exit(1);
        }
    }

    // Check page_size.
    constexpr uint64_t EXPECTED_PAGE_SIZE = 0x200000;
    if (v2p_info.page_size != EXPECTED_PAGE_SIZE) {
        std::cerr << "Incorrect page size. Expected " << EXPECTED_PAGE_SIZE << " got "
                  << v2p_info.page_size << ".\n";
        exit(1);
    }

    // Check page_count.
    constexpr uint64_t PAGE_COUNT = 0x1;
    if (v2p_info.page_count != PAGE_COUNT) {
        std::cerr << "Incorrect page count. Expected " << PAGE_COUNT << " got "
                  << v2p_info.page_count << ".\n";
        exit(1);
    }

    // Check number of bytes_mapped.
    constexpr uint64_t BYTES_MAPPED = 0x18;
    if (v2p_info.bytes_mapped != BYTES_MAPPED) {
        std::cerr << "Incorrect number of bytes mapped. Expected " << BYTES_MAPPED
                  << " got " << v2p_info.bytes_mapped << ".\n";
        exit(1);
    }
}

#if defined(LINUX) && (defined(X64) || defined(__x86_64__) || defined(__aarch64__)) && \
    !defined(RISCV64) && !defined(__riscv)

namespace {

// Fails the unit test immediately when `cond` is false.
void
require_cond(bool cond, const std::string &msg)
{
    if (!cond) {
        std::cerr << "v2p_tracefs unit test failure: " << msg << "\n";
        exit(1);
    }
}

// Replays synthetic `trace_pipe` lines covering 4 KB anonymous faults, CoW overwrites,
// duplicate-mapping suppression, non-matching `mm` filtering, first-touch 2 MB THP
// faults, background `khugepaged` 2 MB THP collapses (`anon_pmd` and `anon_ptes`), and
// replaceable `drmemtrace` file callbacks without touching disk.
static std::vector<char> g_custom_io_bytes;
static bool g_custom_io_closed = false;

static file_t
custom_open_v2p_file(const char *fname, uint mode_flags)
{
    require_cond(fname != nullptr && std::string(fname) == "virtual://v2p.bin",
                 "unexpected custom open fname");
    require_cond((mode_flags & DR_FILE_WRITE_OVERWRITE) != 0,
                 "expected DR_FILE_WRITE_OVERWRITE");
    g_custom_io_bytes.clear();
    g_custom_io_closed = false;
    return static_cast<file_t>(77);
}

static ssize_t
custom_write_v2p_file(file_t file, const void *buf, size_t count)
{
    require_cond(file == static_cast<file_t>(77) && !g_custom_io_closed,
                 "unexpected file handle in custom_write_v2p_file");
    require_cond(errno == EAGAIN, "expected incoming errno to be preserved");
    // Exercise partial writes (16 bytes at a time) and verify `v2p_tracker_t` retries
    // until all fixed-stride 32-byte records are written without clobbering `errno`.
    size_t step = count > 16 ? 16 : count;
    const char *p = static_cast<const char *>(buf);
    g_custom_io_bytes.insert(g_custom_io_bytes.end(), p, p + step);
    errno = ENOENT;
    return static_cast<ssize_t>(step);
}

static void
custom_close_v2p_file(file_t file)
{
    require_cond(file == static_cast<file_t>(77) && !g_custom_io_closed,
                 "unexpected file handle in custom_close_v2p_file");
    g_custom_io_closed = true;
}

void
unit_test_v2p_tracefs_thp(const std::string &v2p_bin_path)
{
    constexpr uint64_t kVmemmap = 0xfffffdffc0000840ULL;
    constexpr uint64_t kTargetMm = 0xffff888012340000ULL;
    auto page_ptr_hex = [&](uint64_t pfn) {
        char buf[32];
        snprintf(buf, sizeof(buf), "0x%llx",
                 static_cast<unsigned long long>(kVmemmap + (pfn << 6)));
        return std::string(buf);
    };

    auto feed_events = [&](v2p_tracker_t &tracker) {
        // 1. 4 KB first-touch fault, CoW overwrite, duplicate suppression, and foreign
        //    `mm`.
        tracker.feed_trace_line_for_testing(
            " app-1000 [000] ....     0.000010: anon_new_1000: (0x0) page=" +
            page_ptr_hex(0x100) + " mm=0xffff888012340000 vaddr=0x20123");
        tracker.feed_trace_line_for_testing(
            " app-1000 [000] ....     0.000020: anon_new_1000: (0x0) page=" +
            page_ptr_hex(0x200) + " mm=0xffff888012340000 vaddr=0x20123");
        tracker.feed_trace_line_for_testing(
            " app-1000 [000] ....     0.000021: anon_new_1000: (0x0) page=" +
            page_ptr_hex(0x200) + " mm=0xffff888012340000 vaddr=0x20123");
        tracker.feed_trace_line_for_testing(
            " other-2000 [001] ....   0.000025: anon_new_1000: (0x0) page=" +
            page_ptr_hex(0x999) + " mm=0xffff888099990000 vaddr=0x20000");

        // 2. Background `khugepaged` 2 MB THP collapse (`anon_pmd` on `khugepaged` PID
        //    55) and batched `anon_ptes` (`nr=4`).
        tracker.feed_trace_line_for_testing(
            " khugepaged-55 [002] .... 0.000030: anon_pmd_1000: (0x0) page=" +
            page_ptr_hex(0x1000) + " mm=0xffff888012340000 vaddr=0x0");
        tracker.feed_trace_line_for_testing(
            " app-1000 [000] ....     0.000040: anon_ptes_1000: (0x0) page=" +
            page_ptr_hex(0x300) + " mm=0xffff888012340000 vaddr=0x400000 nr=4");
    };

    auto verify_records = [&](const std::vector<v2p_record_t> &recs) {
        require_cond(recs.size() == 5,
                     "expected 5 v2p records, got " + std::to_string(recs.size()));
        require_cond(recs[0].vaddr == 0x20000 && recs[0].paddr == 0x100000 &&
                         recs[1].vaddr == 0x20000 && recs[1].paddr == 0x200000 &&
                         recs[2].vaddr == 0x0 && recs[2].paddr == 0x1000000 &&
                         recs[2].page_count == 512 && recs[3].vaddr == 0x400000 &&
                         recs[3].paddr == 0x300000 && recs[3].page_count == 4 &&
                         recs[4].type == V2P_RECORD_END,
                     "unexpected THP or anon_rmap record contents");
    };

    // Case A: Default local file output.
    v2p_tracker_t tracker;
    require_cond(
        tracker.init_for_testing(/*pid=*/1000, v2p_bin_path, kVmemmap, kTargetMm),
        "init_for_testing failed");
    feed_events(tracker);
    tracker.finish_for_testing();

    std::ifstream in(v2p_bin_path, std::ios::binary);
    std::vector<v2p_record_t> recs;
    for (v2p_record_t r = {}; in.read(reinterpret_cast<char *>(&r), sizeof(r));)
        recs.push_back(r);
    in.close();
    std::remove(v2p_bin_path.c_str());
    verify_records(recs);

    // Case B: Replaceable file callbacks redirecting output into memory without touching
    // local disk (preserving `errno` across partial writes).
    require_cond(tracker.init_for_testing(/*pid=*/1000, "virtual://v2p.bin", kVmemmap,
                                          kTargetMm, custom_open_v2p_file,
                                          custom_write_v2p_file, custom_close_v2p_file),
                 "init_for_testing with custom file ops failed");
    errno = EAGAIN;
    feed_events(tracker);
    tracker.finish_for_testing();
    require_cond(errno == EAGAIN && g_custom_io_closed &&
                     g_custom_io_bytes.size() == 5 * sizeof(v2p_record_t),
                 "custom file ops failed to preserve errno or record count");
    std::vector<v2p_record_t> custom_recs(5);
    std::memcpy(custom_recs.data(), g_custom_io_bytes.data(), g_custom_io_bytes.size());
    verify_records(custom_recs);
}

} // namespace
#endif /* LINUX && X64 && !RISCV64 */

void
unit_test_v2p_reader(const std::string &testdir)
{
    std::string v2p_file_path = testdir + "/v2p_example.textproto";
    v2p_info_t v2p_info;
    v2p_reader_t v2p_reader;
    std::ifstream fin;
    fin.open(v2p_file_path);
    if (!fin.is_open()) {
        std::cerr << "Failed to open the v2p file '" << v2p_file_path << "'\n";
        exit(1);
    }
    std::string error_str = v2p_reader.create_v2p_info_from_file(fin, v2p_info);
    fin.close();
    if (!error_str.empty()) {
        std::cerr << "v2p_reader failed with: " << error_str << "\n";
        exit(1);
    }

    check_v2p_info(v2p_info);

#if defined(LINUX) && (defined(X64) || defined(__x86_64__) || defined(__aarch64__)) && \
    !defined(RISCV64) && !defined(__riscv)
    const char *tmp = std::getenv("TEST_TMPDIR");
    std::string out = (tmp != nullptr && tmp[0] != '\0')
        ? (std::string(tmp) + "/v2p_thp_unit.bin")
        : "v2p_thp_unit.bin";
    unit_test_v2p_tracefs_thp(out);
#endif /* LINUX && X64 && !RISCV64 */
}

} // namespace drmemtrace
} // namespace dynamorio
