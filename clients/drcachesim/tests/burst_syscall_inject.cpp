/* **********************************************************
 * Copyright (c) 2016-2025 Google, Inc.  All rights reserved.
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

// This application links in drmemtrace_static and acquires a trace during
// a "burst" of execution that includes some system calls, constructs "dummy"
// trace templates for some system calls, then injects those templates into
// the collected trace using raw2trace.

// This is set globally in CMake for other tests so easier to undef here.
#undef DR_REG_ENUM_COMPATIBILITY

#include "test_helpers.h"
#include "analyzer.h"
#include "tools/basic_counts.h"
#include "tools/invariant_checker.h"
#include "tools/syscall_mix.h"
#include "dr_api.h"
#include "drmemtrace/drmemtrace.h"
#include "drmemtrace/raw2trace.h"
#include "mock_reader.h"
#include "raw2trace_directory.h"
#include "scheduler.h"

#include <cassert>
#include <errno.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <string>
#include <sys/syscall.h>
#include <sys/types.h>
#ifndef LINUX
#    error Only Linux is supported
#endif

namespace dynamorio {
namespace drmemtrace {

#define PC_SYSCALL_MEMBARRIER 0xdeadbe00
#define PC_SYSCALL_GETTID 0x8badf000
#define PC_SYSCALL_DEFAULT_TRACE 0xf00d8bad
#define READ_MEMADDR_GETTID 0xdecafbad
#define REP_MOVS_COUNT 1024
#define SYSCALL_INSTR_COUNT 2
#define DEFAULT_INSTR_COUNT 1

static instr_t *instrs_in_membarrier[SYSCALL_INSTR_COUNT];
static instr_t *instrs_in_gettid[SYSCALL_INSTR_COUNT];
static instr_t *instrs_in_default_trace[DEFAULT_INSTR_COUNT];

#define FATAL_ERROR(msg, ...)                               \
    do {                                                    \
        fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__); \
        fflush(stderr);                                     \
        exit(1);                                            \
    } while (0)

static int
do_membarrier(void)
{
    // MEMBARRIER_CMD_QUERY is always supported, and returns
    // a non-zero result.
    return syscall(SYS_membarrier, /*MEMBARRIER_CMD_QUERY*/ 0,
                   /*flags=*/0, /*cpuid=*/0);
}

static pid_t
do_gettid(void)
{
    return syscall(SYS_gettid);
}

static pid_t
do_getpid(void)
{
    return syscall(SYS_getpid);
}

static int
do_some_syscalls()
{
    // Considered as a "maybe blocking" syscall by raw2trace.
    do_membarrier();
    // Considered as a regular non-blocking syscall by raw2trace; we also
    // specify it in -record_syscall for this test.
    do_gettid();
    // Will be injected with the default syscall trace.
    do_getpid();
    // Make some failing sigaction syscalls, which we record such that
    // syscall_mix will count the failure codes.
    // We bypass libc to ensure these make it to the syscall itself.
    // We deliberately do not include templates for these, as a test of
    // syscalls without templates.
    constexpr int KERNEL_SIGSET_SIZE = 8;
    struct sigaction act;
    int res =
        syscall(SYS_rt_sigaction, /*too large*/ 1280, &act, nullptr, KERNEL_SIGSET_SIZE);
    assert(res != 0 && errno == EINVAL);
    res =
        syscall(SYS_rt_sigaction, /*too large*/ 12800, &act, nullptr, KERNEL_SIGSET_SIZE);
    assert(res != 0 && errno == EINVAL);
    res = syscall(SYS_rt_sigaction, SIGUSR1, nullptr,
                  /*bad address*/ reinterpret_cast<struct sigaction *>(4),
                  KERNEL_SIGSET_SIZE);
    assert(res != 0 && errno == EFAULT);

    return 1;
}

// Checks that the failure counts match those in do_some_syscalls().
static void
check_syscall_stats(syscall_mix_t::statistics_t &syscall_stats)
{
    assert(syscall_stats.syscall_errno_counts.size() == 1);
    assert(syscall_stats.syscall_errno_counts[SYS_rt_sigaction].size() == 2);
    assert(syscall_stats.syscall_errno_counts[SYS_rt_sigaction][EINVAL] == 2);
    assert(syscall_stats.syscall_errno_counts[SYS_rt_sigaction][EFAULT] == 1);
}

static void
write_trace_entry(std::unique_ptr<std::ostream> &writer, const trace_entry_t &entry)
{
    if (!writer->write((char *)&entry, sizeof(trace_entry_t)))
        FATAL_ERROR("Failed to write to system call trace template file.");
}

static void
write_instr_entry(void *dr_context, std::unique_ptr<std::ostream> &writer, instr_t *instr,
                  app_pc instr_app_pc, trace_type_t type = TRACE_TYPE_INSTR)
{
    if (instr == nullptr) {
        FATAL_ERROR("Cannot write a nullptr instr.");
    }
    unsigned short len = instr_length(dr_context, instr);
    trace_entry_t encoding = { TRACE_TYPE_ENCODING, len, 0 };
    if (len >= sizeof(encoding.encoding)) {
        // We want to keep the test setup logic simple.
        FATAL_ERROR("Instr encoding does not fit into a single encoding entry.");
    }
    instr_encode_to_copy(dr_context, instr, encoding.encoding, instr_app_pc);
    write_trace_entry(writer, encoding);
    write_trace_entry(
        writer, test_util::make_instr(reinterpret_cast<addr_t>(instr_app_pc), type, len));
}

static void
write_header_entries(std::unique_ptr<std::ostream> &writer)
{
    // Write a valid header so the trace can be used with the trace analyzer.
#define MAX_HEADER_ENTRIES 10
    trace_entry_t header_buf[MAX_HEADER_ENTRIES];
    byte *buf = reinterpret_cast<byte *>(header_buf);
    offline_file_type_t file_type = static_cast<offline_file_type_t>(
        OFFLINE_FILE_TYPE_KERNEL_SYSCALL_TRACE_TEMPLATES | OFFLINE_FILE_TYPE_ENCODINGS |
        IF_X86_ELSE(
            IF_X64_ELSE(OFFLINE_FILE_TYPE_ARCH_X86_64, OFFLINE_FILE_TYPE_ARCH_X86_32),
            IF_X64_ELSE(OFFLINE_FILE_TYPE_ARCH_AARCH64, OFFLINE_FILE_TYPE_ARCH_ARM32)));
    // We just want a sentinel other than zero for tid and pid.
    raw2trace_t::create_essential_header_entries(buf, TRACE_ENTRY_VERSION, file_type,
                                                 /*tid=*/1,
                                                 /*pid=*/1);
    if (buf >= reinterpret_cast<byte *>(header_buf + MAX_HEADER_ENTRIES)) {
        FATAL_ERROR("Too many header entries.");
    }
    for (byte *buf_at = reinterpret_cast<byte *>(header_buf); buf_at < buf;
         buf_at += sizeof(trace_entry_t)) {
        trace_entry_t to_write = *reinterpret_cast<trace_entry_t *>(buf_at);
        write_trace_entry(writer, to_write);
    }
    write_trace_entry(writer,
                      test_util::make_marker(TRACE_MARKER_TYPE_CACHE_LINE_SIZE, 64));
    write_trace_entry(writer, test_util::make_marker(TRACE_MARKER_TYPE_PAGE_SIZE, 4096));
    // Some header read-ahead logic uses the timestamp marker to know when
    // to stop. It is important to not read-ahead any kernel syscall trace
    // content, as then is_record_kernel() starts returning true on the stream.
    // Also, some scheduler logic wants non-zero timestamps.
    write_trace_entry(writer, test_util::make_marker(TRACE_MARKER_TYPE_TIMESTAMP, 1));
}

static void
write_footer_entries(std::unique_ptr<std::ostream> &writer)
{
    dynamorio::drmemtrace::trace_entry_t thread_exit = {
        dynamorio::drmemtrace::TRACE_TYPE_THREAD_EXIT, 0, { /*tid=*/1 }
    };
    write_trace_entry(writer, thread_exit);
    write_trace_entry(writer, test_util::make_footer());
}

static std::string
write_system_call_template(void *dr_context)
{
    // Get path to write the template and an ostream to it.
    const char *raw_dir;
    drmemtrace_status_t mem_res = drmemtrace_get_output_path(&raw_dir);
    assert(mem_res == DRMEMTRACE_SUCCESS);
    std::string syscall_trace_template_file =
        std::string(raw_dir) + DIRSEP + "syscall_trace_template";
    auto writer =
        std::unique_ptr<std::ostream>(new std::ofstream(syscall_trace_template_file));

    write_header_entries(writer);

    // Write the trace template for SYS_membarrier.
    write_trace_entry(
        writer,
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_TRACE_START, SYS_membarrier));
    // Just a random instruction.
    instrs_in_membarrier[0] = XINST_CREATE_nop(dr_context);
    write_instr_entry(dr_context, writer, instrs_in_membarrier[0],
                      reinterpret_cast<app_pc>(PC_SYSCALL_MEMBARRIER));

    constexpr uintptr_t SOME_VAL = 0xf00d;
#ifdef X86
    instrs_in_membarrier[1] = INSTR_CREATE_sysret(dr_context);
#elif defined(AARCHXX)
    instrs_in_membarrier[1] = INSTR_CREATE_eret(dr_context);
#endif
    // The value doesn't really matter as it will be replaced during trace injection.
    write_trace_entry(writer,
                      test_util::make_marker(TRACE_MARKER_TYPE_BRANCH_TARGET, SOME_VAL));
    write_instr_entry(
        dr_context, writer, instrs_in_membarrier[1],
        reinterpret_cast<app_pc>(PC_SYSCALL_MEMBARRIER +
                                 instr_length(dr_context, instrs_in_membarrier[0])),
        TRACE_TYPE_INSTR_INDIRECT_JUMP);
    write_trace_entry(
        writer,
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_TRACE_END, SYS_membarrier));

    // Write the trace template for SYS_gettid.
    write_trace_entry(
        writer,
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_TRACE_START, SYS_gettid));
    // Just a random instruction.
#ifdef X86
#    define TEST_REG DR_REG_XDX
#elif defined(ARM)
#    define TEST_REG DR_REG_R12
#elif defined(AARCH64)
#    define TEST_REG DR_REG_X4
#endif
    instrs_in_gettid[0] =
        XINST_CREATE_load(dr_context, opnd_create_reg(TEST_REG),
                          opnd_create_base_disp(TEST_REG, DR_REG_NULL, 0, 0, OPSZ_PTR));
    write_instr_entry(dr_context, writer, instrs_in_gettid[0],
                      reinterpret_cast<app_pc>(PC_SYSCALL_GETTID));
    write_trace_entry(writer,
                      test_util::make_memref(READ_MEMADDR_GETTID, TRACE_TYPE_READ,
                                             opnd_size_in_bytes(OPSZ_PTR)));

#ifdef X86
    instrs_in_gettid[1] = INSTR_CREATE_sysret(dr_context);
#elif defined(AARCHXX)
    instrs_in_gettid[1] = INSTR_CREATE_eret(dr_context);
#endif
    // The value doesn't really matter as it will be replaced during trace injection.
    write_trace_entry(writer,
                      test_util::make_marker(TRACE_MARKER_TYPE_BRANCH_TARGET, SOME_VAL));
    write_instr_entry(
        dr_context, writer, instrs_in_gettid[1],
        reinterpret_cast<app_pc>(PC_SYSCALL_GETTID +
                                 instr_length(dr_context, instrs_in_gettid[0])),
        TRACE_TYPE_INSTR_INDIRECT_JUMP);

    write_trace_entry(
        writer, test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_TRACE_END, SYS_gettid));

    // Write the default trace template.
    write_trace_entry(writer,
                      test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_TRACE_START,
                                             DEFAULT_SYSCALL_TRACE_TEMPLATE_NUM));
#ifdef X86
    instrs_in_default_trace[0] = INSTR_CREATE_sysret(dr_context);
#elif defined(AARCHXX)
    instrs_in_default_trace[0] = INSTR_CREATE_eret(dr_context);
#endif
    // The value doesn't really matter as it will be replaced during trace injection.
    write_trace_entry(writer,
                      test_util::make_marker(TRACE_MARKER_TYPE_BRANCH_TARGET, SOME_VAL));
    write_instr_entry(dr_context, writer, instrs_in_default_trace[0],
                      reinterpret_cast<app_pc>(PC_SYSCALL_DEFAULT_TRACE),
                      TRACE_TYPE_INSTR_INDIRECT_JUMP);
    write_trace_entry(writer,
                      test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_TRACE_END,
                                             DEFAULT_SYSCALL_TRACE_TEMPLATE_NUM));

    write_footer_entries(writer);
    return syscall_trace_template_file;
}

static std::string
postprocess(void *dr_context, std::string syscall_trace_template_file,
            int expected_min_injected_syscall_count, std::string suffix = "")
{
    // Get path to write the final trace to.
    const char *raw_dir;
    drmemtrace_status_t mem_res = drmemtrace_get_output_path(&raw_dir);
    assert(mem_res == DRMEMTRACE_SUCCESS);
    std::string outdir = std::string(raw_dir) + DIRSEP + "post_processed." + suffix;

    raw2trace_directory_t dir;
    if (!dr_create_dir(outdir.c_str()))
        FATAL_ERROR("Failed to create output dir.");
    std::string dir_err = dir.initialize(raw_dir, outdir, DEFAULT_TRACE_COMPRESSION_TYPE,
                                         syscall_trace_template_file);
    assert(dir_err.empty());
    raw2trace_t raw2trace(dir.modfile_bytes_, dir.in_files_, dir.out_files_,
                          dir.out_archives_, dir.encoding_file_,
                          dir.serial_schedule_file_, dir.cpu_schedule_file_, dr_context,
                          /*verbosity=*/0, /*worker_count=*/-1,
                          /*alt_module_dir=*/"",
                          /*chunk_instr_count=*/10 * 1000 * 1000,
                          /*kthread_files_map=*/ {},
                          /*kcore_path=*/"", /*kallsyms_path=*/"",
                          std::move(dir.syscall_template_file_reader_));
    std::string error = raw2trace.do_conversion();
    if (!error.empty())
        FATAL_ERROR("raw2trace failed: %s\n", error.c_str());
    uint64 injected_syscall_count =
        raw2trace.get_statistic(RAW2TRACE_STAT_SYSCALL_TRACES_INJECTED);
    if (injected_syscall_count < expected_min_injected_syscall_count) {
        std::cerr << "Incorrect injected syscall count (found: " << injected_syscall_count
                  << " vs expected: >= " << expected_min_injected_syscall_count << ")\n";
    }
    return outdir;
}

static void
get_tool_results(const std::string &trace_dir, basic_counts_t::counters_t &basic_counts,
                 syscall_mix_t::statistics_t &syscall_stats)
{
    auto basic_counts_tool =
        std::unique_ptr<basic_counts_t>(new basic_counts_t(/*verbose=*/0));
    auto syscall_mix_tool =
        std::unique_ptr<syscall_mix_t>(new syscall_mix_t(/*verbose=*/0));
    auto invariant_checker_tool =
        std::unique_ptr<invariant_checker_t>(new invariant_checker_t());
    std::vector<analysis_tool_t *> tools;
    tools.push_back(basic_counts_tool.get());
    tools.push_back(syscall_mix_tool.get());
    tools.push_back(invariant_checker_tool.get());
    analyzer_t analyzer(trace_dir, &tools[0], static_cast<int>(tools.size()));
    if (!analyzer) {
        FATAL_ERROR("failed to initialize analyzer: %s",
                    analyzer.get_error_string().c_str());
    }
    if (!analyzer.run()) {
        FATAL_ERROR("failed to run analyzer: %s", analyzer.get_error_string().c_str());
    }
    syscall_stats = syscall_mix_tool->get_total_statistics();
    basic_counts = basic_counts_tool->get_total_counts();
}

static void
gather_trace()
{
    std::cerr << "Collecting a trace...\n";
    std::string ops = "-stderr_mask 0xc -client_lib ';;-offline -record_syscall " +
        // TODO i#7482: Specifying 0 args for any syscall to -record_syscall crashes.
        // We specify 1 arg for gettid just to get around this, which is okay for this
        // test as we'd like some func_arg markers.
        std::to_string(SYS_rt_sigaction) + "|4&" + std::to_string(SYS_gettid) + "|1";
    if (setenv("DYNAMORIO_OPTIONS", ops.c_str(), 1 /*override*/) != 0)
        std::cerr << "failed to set env var!\n";
    dr_app_setup();
    assert(!dr_app_running_under_dynamorio());
    dr_app_start();
    do_some_syscalls();
    dr_app_stop_and_cleanup();
    std::cerr << "Done collecting trace\n";
    return;
}

static bool
check_instr_same(void *dr_context, memref_t &memref, instr_t *expected_instr)
{
    assert(type_is_instr(memref.instr.type));
    instr_t instr;
    instr_init(dr_context, &instr);
    app_pc next_pc =
        decode_from_copy(dr_context, memref.instr.encoding,
                         reinterpret_cast<byte *>(memref.instr.addr), &instr);
    assert(next_pc != nullptr && instr_valid(&instr));
    bool res = true;
    if (!instr_same(expected_instr, &instr)) {
        std::cerr << "Unexpected instruction: |";
        instr_disassemble(dr_context, &instr, STDERR);
        std::cerr << "| expected: |";
        instr_disassemble(dr_context, expected_instr, STDERR);
        std::cerr << "|\n";
        res = false;
    }
    instr_free(dr_context, &instr);
    return res;
}

static bool
look_for_syscall_trace(void *dr_context, std::string trace_dir)
{
    scheduler_t scheduler;
    std::vector<scheduler_t::input_workload_t> sched_inputs;
    sched_inputs.emplace_back(trace_dir);
    if (scheduler.init(sched_inputs, 1, scheduler_t::make_scheduler_serial_options()) !=
        scheduler_t::STATUS_SUCCESS) {
        FATAL_ERROR("Failed to initialize scheduler: %s\n",
                    scheduler.get_error_string().c_str());
    }
    auto *stream = scheduler.get_stream(0);
    memref_t memref;
    int membarrier_instr_found = 0;
    int gettid_instr_found = 0;
    int membarrier_instr_len = 0;
    int gettid_instr_len = 0;
    int default_trace_instr_found = 0;
    bool found_gettid_read = false;
    bool have_syscall_trace_type = false;
    // Non-sentinel only when we're actually between the syscall_trace_start and
    // syscall_trace_end markers.
    int syscall_trace_num = -1;
    // Non-sentinel only when we've seen a syscall marker without any intervening
    // instr or any marker except the allowed ones till now.
    int prev_syscall_num_marker = -1;
    // Always holds the last seen syscall number.
    int last_syscall = -1;
    bool saw_aux_syscall_markers_for_membarrier = false;
    bool saw_aux_syscall_markers_for_gettid = false;
    for (scheduler_t::stream_status_t status = stream->next_record(memref);
         status != scheduler_t::STATUS_EOF; status = stream->next_record(memref)) {
        assert(status == scheduler_t::STATUS_OK);
        int prev_syscall_num_marker_saved = prev_syscall_num_marker;
        prev_syscall_num_marker = -1;
        // std::cerr << "AAA at: " << memref.instr.type << ", " <<
        // memref.marker.marker_type
        // << std::hex << " :: " << memref.instr.addr << ", " <<
        // memref.marker.marker_value
        // << std::dec << "\n";
        if (memref.marker.type == TRACE_TYPE_MARKER) {
            switch (memref.marker.marker_type) {
            case TRACE_MARKER_TYPE_FILETYPE:
                if (TESTANY(OFFLINE_FILE_TYPE_KERNEL_SYSCALLS,
                            memref.marker.marker_value)) {
                    have_syscall_trace_type = true;
                }
                break;
            case TRACE_MARKER_TYPE_SYSCALL_TRACE_START:
                syscall_trace_num = memref.marker.marker_value;
                if (syscall_trace_num != prev_syscall_num_marker_saved ||
                    prev_syscall_num_marker_saved == -1) {
                    std::cerr << "Found unexpected trace for system call "
                              << syscall_trace_num
                              << " when prev system call number marker was (-1 for not a "
                                 "sysnum marker) "
                              << prev_syscall_num_marker_saved << "\n";
                    return false;
                }
                break;
            case TRACE_MARKER_TYPE_SYSCALL_TRACE_END: syscall_trace_num = -1; break;
            case TRACE_MARKER_TYPE_SYSCALL:
                prev_syscall_num_marker = memref.marker.marker_value;
                last_syscall = prev_syscall_num_marker;
                break;
            case TRACE_MARKER_TYPE_FUNC_RETVAL:
                if (last_syscall == SYS_gettid && gettid_instr_found == 0) {
                    std::cerr << "gettid trace not injected before func_retval marker.";
                    return false;
                } else if (last_syscall == SYS_membarrier) {
                    std::cerr << "Did not expect func_retval marker for membarrier.";
                    return false;
                }
                break;
            case TRACE_MARKER_TYPE_MAYBE_BLOCKING_SYSCALL:
            case TRACE_MARKER_TYPE_FUNC_ARG:
                if (last_syscall == SYS_gettid) {
                    if (gettid_instr_found > 0) {
                        std::cerr << "Found func_arg marker or maybe_blocking marker "
                                  << "after the gettid trace.";
                        return false;
                    }
                    saw_aux_syscall_markers_for_gettid = true;
                } else if (last_syscall == SYS_membarrier) {
                    if (membarrier_instr_found > 0) {
                        std::cerr << "Found func_arg marker or maybe_blocking marker "
                                  << "after the membarrier trace.";
                        return false;
                    }
                    saw_aux_syscall_markers_for_membarrier = true;
                }
            case TRACE_MARKER_TYPE_FUNC_ID:
                // The above markers are expected to be seen between the syscall
                // marker and the injected trace, so we preserve prev_syscall_num_marker.
                prev_syscall_num_marker = prev_syscall_num_marker_saved;
            }
            continue;
        }
        if (syscall_trace_num == -1) {
            continue;
        }
        bool is_instr = type_is_instr(memref.instr.type);
        if (!is_instr && !type_is_data(memref.instr.type)) {
            std::cerr << "Found unexpected memref record " << memref.instr.type
                      << " inside inserted system call template\n";
            return false;
        }
        switch (syscall_trace_num) {
        case SYS_gettid:
            if (is_instr) {
                assert(gettid_instr_found < SYSCALL_INSTR_COUNT);
                if (memref.instr.addr != PC_SYSCALL_GETTID + gettid_instr_len) {
                    std::cerr << "Found incorrect addr (" << std::hex << memref.instr.addr
                              << " vs expected " << PC_SYSCALL_GETTID + gettid_instr_len
                              << std::dec << ") for gettid trace instr.\n";
                    return false;
                }
                if (!check_instr_same(dr_context, memref,
                                      instrs_in_gettid[gettid_instr_found])) {
                    return false;
                }
                gettid_instr_len += memref.instr.size;
                ++gettid_instr_found;
            } else {
                assert(gettid_instr_found == 1);
                found_gettid_read = true;
                if (memref.data.type != TRACE_TYPE_READ ||
                    memref.data.size != opnd_size_in_bytes(OPSZ_PTR) ||
                    memref.data.addr != READ_MEMADDR_GETTID) {
                    std::cerr << "Found incorrect entry (" << memref.data.type << ","
                              << memref.data.size << "," << std::hex << memref.data.addr
                              << ") vs expected ptr-sized read for "
                              << READ_MEMADDR_GETTID << std::dec
                              << ") for gettid trace.\n";
                    return false;
                }
            }
            break;
        case SYS_membarrier:
            if (is_instr) {
                assert(membarrier_instr_found < SYSCALL_INSTR_COUNT);
                if (memref.instr.addr != PC_SYSCALL_MEMBARRIER + membarrier_instr_len) {
                    std::cerr << "Found incorrect addr (" << std::hex << memref.instr.addr
                              << " vs expected "
                              << PC_SYSCALL_MEMBARRIER + membarrier_instr_len << std::dec
                              << ") for membarrier trace instr.\n";
                    return false;
                }
                if (!check_instr_same(dr_context, memref,
                                      instrs_in_membarrier[membarrier_instr_found])) {
                    return false;
                }
                membarrier_instr_len += memref.instr.size;
                ++membarrier_instr_found;
            } else {
                std::cerr << "Found unexpected data memref in membarrier trace\n";
                return false;
            }
            break;
        default:
            if (is_instr) {
                if (memref.instr.addr != PC_SYSCALL_DEFAULT_TRACE) {
                    std::cerr << "Found incorrect addr (" << std::hex << memref.instr.addr
                              << " vs expected "
                              << PC_SYSCALL_MEMBARRIER + membarrier_instr_len << std::dec
                              << ") for default trace instr.\n";
                    return false;
                }
                if (!check_instr_same(dr_context, memref, instrs_in_default_trace[0])) {
                    return false;
                }
                ++default_trace_instr_found;
            } else {
                std::cerr << "Found unexpected data memref in default trace\n";
                return false;
            }
        }
    }
    if (!have_syscall_trace_type) {
        std::cerr << "Trace did not have the expected file type\n";
    } else if (gettid_instr_found != SYSCALL_INSTR_COUNT) {
        std::cerr << "Did not find all instrs in gettid trace\n";
    } else if (membarrier_instr_found != SYSCALL_INSTR_COUNT) {
        std::cerr << "Did not find all instrs in membarrier trace\n";
    } else if (!found_gettid_read) {
        std::cerr << "Did not find read data memref in gettid trace\n";
    } else if (!saw_aux_syscall_markers_for_membarrier) {
        std::cerr << "Did not see any auxillary syscall markers for membarrier. "
                  << "Ensure test is setup properly\n";
    } else if (!saw_aux_syscall_markers_for_gettid) {
        std::cerr << "Did not see any auxillary syscall markers for gettid. "
                  << "Ensure test is setup properly\n";
    } else if (default_trace_instr_found == 0) {
        std::cerr << "Did not see any default trace instrs\n";
    } else {
        return true;
    }
    return false;
}

#ifdef X86

static std::string
write_system_call_template_with_repstr(void *dr_context)
{
    // Get path to write the template and an ostream to it.
    const char *raw_dir;
    drmemtrace_status_t mem_res = drmemtrace_get_output_path(&raw_dir);
    assert(mem_res == DRMEMTRACE_SUCCESS);
    std::string syscall_trace_template_file =
        std::string(raw_dir) + DIRSEP + "syscall_trace_template_repstr";
    auto writer =
        std::unique_ptr<std::ostream>(new std::ofstream(syscall_trace_template_file));

    write_header_entries(writer);

    write_trace_entry(
        writer,
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_TRACE_START, SYS_gettid));
    instr_t *rep_movs = INSTR_CREATE_rep_movs_1(GLOBAL_DCONTEXT);
    for (int i = 0; i < REP_MOVS_COUNT; ++i) {
        write_instr_entry(dr_context, writer, rep_movs,
                          reinterpret_cast<app_pc>(PC_SYSCALL_GETTID),
                          i == 0 ? TRACE_TYPE_INSTR : TRACE_TYPE_INSTR_NO_FETCH);
        write_trace_entry(writer,
                          test_util::make_memref(READ_MEMADDR_GETTID, TRACE_TYPE_READ,
                                                 opnd_size_in_bytes(OPSZ_PTR)));
        write_trace_entry(writer,
                          test_util::make_memref(READ_MEMADDR_GETTID, TRACE_TYPE_WRITE,
                                                 opnd_size_in_bytes(OPSZ_PTR)));
    }

    constexpr uintptr_t SOME_VAL = 0xf00d;
    instr_t *sys_return;
#    ifdef X86
    sys_return = INSTR_CREATE_sysret(dr_context);
#    elif defined(AARCHXX)
    sys_return = INSTR_CREATE_eret(dr_context);
#    endif

    write_trace_entry(writer,
                      test_util::make_marker(TRACE_MARKER_TYPE_BRANCH_TARGET, SOME_VAL));
    write_instr_entry(
        dr_context, writer, sys_return,
        reinterpret_cast<app_pc>(PC_SYSCALL_GETTID + instr_length(dr_context, rep_movs)),
        TRACE_TYPE_INSTR_INDIRECT_JUMP);

    write_trace_entry(
        writer,
        test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_TRACE_END, SYS_membarrier));

    // Write the default trace template.
    write_trace_entry(writer,
                      test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_TRACE_START,
                                             DEFAULT_SYSCALL_TRACE_TEMPLATE_NUM));
    // The value doesn't really matter as it will be replaced during trace injection.
    write_trace_entry(writer,
                      test_util::make_marker(TRACE_MARKER_TYPE_BRANCH_TARGET, SOME_VAL));
    write_instr_entry(dr_context, writer, sys_return,
                      reinterpret_cast<app_pc>(PC_SYSCALL_DEFAULT_TRACE),
                      TRACE_TYPE_INSTR_INDIRECT_JUMP);
    write_trace_entry(writer,
                      test_util::make_marker(TRACE_MARKER_TYPE_SYSCALL_TRACE_END,
                                             DEFAULT_SYSCALL_TRACE_TEMPLATE_NUM));

    write_footer_entries(writer);

    instr_destroy(dr_context, sys_return);
    instr_destroy(dr_context, rep_movs);
    return syscall_trace_template_file;
}

static int
test_template_with_repstr(void *dr_context)
{
    std::cerr << "Testing system call trace template injection with repstr...\n";

    std::string syscall_trace_template =
        write_system_call_template_with_repstr(dr_context);
    syscall_mix_t::statistics_t syscall_stats;
    basic_counts_t::counters_t template_counts;
    get_tool_results(syscall_trace_template, template_counts, syscall_stats);
    int distinct_instrs_in_tmpl = SYSCALL_INSTR_COUNT + 1;
    if (!(template_counts.instrs == distinct_instrs_in_tmpl &&
          template_counts.instrs_nofetch == REP_MOVS_COUNT - 1 &&
          template_counts.encodings == REP_MOVS_COUNT + SYSCALL_INSTR_COUNT &&
          template_counts.loads == REP_MOVS_COUNT &&
          template_counts.stores == REP_MOVS_COUNT)) {
        std::cerr << "Unexpected counts in system call trace template with repstr ("
                  << syscall_trace_template << "): #instrs: " << template_counts.instrs
                  << ", #instrs_nofetch: " << template_counts.instrs_nofetch
                  << ", #encodings: " << template_counts.encodings
                  << ", #loads: " << template_counts.loads
                  << ", #stores: " << template_counts.stores << "\n";
        return 1;
    }
    assert(syscall_stats.syscall_errno_counts.empty());

    std::string trace_dir =
        postprocess(dr_context, syscall_trace_template,
                    /*expected_min_injected_syscall_count=*/2, "repstr");

    basic_counts_t::counters_t final_trace_counts;
    get_tool_results(trace_dir, final_trace_counts, syscall_stats);
    if (final_trace_counts.kernel_instrs < distinct_instrs_in_tmpl ||
        final_trace_counts.kernel_nofetch_instrs != REP_MOVS_COUNT - 1) {
        std::cerr << "Unexpected counts in the final trace with repstr (#instr="
                  << final_trace_counts.kernel_instrs
                  << ",#nofetch_instr=" << final_trace_counts.kernel_nofetch_instrs
                  << "\n";
        return 1;
    }
    check_syscall_stats(syscall_stats);

    std::cerr << "Done with test.\n";
    return 0;
}
#endif

static int
test_trace_templates(void *dr_context)
{
    std::cerr << "Testing system call trace template injection...\n";
    // The following template is also used by the postcmd in cmake which runs the
    // invariant checker on a trace injected with these templates.
    std::string syscall_trace_template = write_system_call_template(dr_context);
    syscall_mix_t::statistics_t syscall_stats;
    basic_counts_t::counters_t template_counts;
    get_tool_results(syscall_trace_template, template_counts, syscall_stats);

    // We have two templates of two instrs each, and one default template with
    // just one instr.
    int distinct_instrs_in_tmpl = SYSCALL_INSTR_COUNT * 2 + 1;
    if (!(template_counts.instrs == distinct_instrs_in_tmpl &&
          template_counts.instrs_nofetch == 0 &&
          template_counts.encodings == distinct_instrs_in_tmpl &&
          template_counts.loads == 1 && template_counts.stores == 0 &&
          // We only have trace start and end markers, no syscall number markers.
          template_counts.syscall_number_markers == 0)) {
        std::cerr << "Unexpected counts in system call trace template ("
                  << syscall_trace_template << "): #instrs: " << template_counts.instrs
                  << ", #instrs_nofetch: " << template_counts.instrs_nofetch
                  << ", #encodings: " << template_counts.encodings
                  << ", #loads: " << template_counts.loads
                  << ", #stores: " << template_counts.stores
                  << ", #syscall_number_markers: "
                  << template_counts.syscall_number_markers << "\n";
        return 1;
    }
    assert(syscall_stats.syscall_errno_counts.empty());

    std::string trace_dir = postprocess(dr_context, syscall_trace_template,
                                        /*expected_min_injected_syscall_count=*/3, "");
    bool success = look_for_syscall_trace(dr_context, trace_dir);
    for (int i = 0; i < SYSCALL_INSTR_COUNT; ++i) {
        instr_destroy(dr_context, instrs_in_membarrier[i]);
        instr_destroy(dr_context, instrs_in_gettid[i]);
    }
    instr_destroy(dr_context, instrs_in_default_trace[0]);
    if (!success) {
        return 1;
    }
    basic_counts_t::counters_t final_trace_counts;
    get_tool_results(trace_dir, final_trace_counts, syscall_stats);
    if (final_trace_counts.kernel_instrs < distinct_instrs_in_tmpl) {
        std::cerr << "Unexpected kernel instr count in the final trace ("
                  << final_trace_counts.kernel_instrs << ")\n";
        return 1;
    }
    check_syscall_stats(syscall_stats);

    std::cerr << "Done with test.\n";
    return 0;
}

int
test_main(int argc, const char *argv[])
{

    gather_trace();
    void *dr_context = dr_standalone_init();
    if (test_trace_templates(dr_context)) {
        return 1;
    }
#ifdef X86
    if (test_template_with_repstr(dr_context)) {
        return 1;
    }
#endif
    dr_standalone_exit();
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
