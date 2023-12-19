/* **********************************************************
 * Copyright (c) 2016-2023 Google, Inc.  All rights reserved.
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

#include "analyzer.h"
#include "tools/basic_counts.h"
#include "dr_api.h"
#include "drmemtrace/drmemtrace.h"
#include "drmemtrace/raw2trace.h"
#include "mock_reader.h"
#include "raw2trace_directory.h"
#include "scheduler.h"

#include <cassert>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/syscall.h>

namespace dynamorio {
namespace drmemtrace {

static instr_t *instr_in_getpid = nullptr;
static instr_t *instr_in_gettid = nullptr;

#define PC_SYSCALL_GETPID 0xdeadbe00
#define PC_SYSCALL_GETTID 0x8badf000
#define READ_MEMADDR_GETTID 0xdecafbad

#define FATAL_ERROR(msg, ...)                               \
    do {                                                    \
        fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__); \
        fflush(stderr);                                     \
        exit(1);                                            \
    } while (0)

static int
do_some_syscalls()
{
    getpid();
    gettid();
    std::cerr << "Done with system calls\n";
    return 1;
}

static void
write_trace_entry(std::unique_ptr<std::ostream> &writer, const trace_entry_t &entry)
{
    if (!writer->write((char *)&entry, sizeof(trace_entry_t)))
        FATAL_ERROR("Failed to write to system call trace template file.");
}

static void
write_instr_entry(void *dr_context, std::unique_ptr<std::ostream> &writer, instr_t *instr,
                  app_pc instr_app_pc)
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
        writer,
        make_instr(reinterpret_cast<addr_t>(instr_app_pc), TRACE_TYPE_INSTR, len));
}

static std::string
write_system_call_template(void *dr_context)
{
    std::cerr << "Going to write system call trace templates\n";
    // Get path to write the template and an ostream to it.
    const char *raw_dir;
    drmemtrace_status_t mem_res = drmemtrace_get_output_path(&raw_dir);
    assert(mem_res == DRMEMTRACE_SUCCESS);
    std::string syscall_trace_template_file =
        std::string(raw_dir) + DIRSEP + "syscall_trace_template";
    auto writer =
        std::unique_ptr<std::ostream>(new std::ofstream(syscall_trace_template_file));

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

    // Write the trace template for SYS_getpid.
    write_trace_entry(writer, make_marker(TRACE_MARKER_TYPE_SYSCALL, SYS_getpid));
    // Just a random instruction.
    instr_in_getpid = XINST_CREATE_nop(dr_context);
    write_instr_entry(dr_context, writer, instr_in_getpid,
                      reinterpret_cast<app_pc>(PC_SYSCALL_GETPID));

    // Write the trace template for SYS_gettid.
    write_trace_entry(writer, make_marker(TRACE_MARKER_TYPE_SYSCALL, SYS_gettid));
    // Just a random instruction.
#ifdef X86
#    define TEST_REG DR_REG_XDX
#elif defined(ARM)
#    define TEST_REG DR_REG_R12
#elif defined(AARCH64)
#    define TEST_REG DR_REG_X4
#endif
    instr_in_gettid =
        XINST_CREATE_load(dr_context, opnd_create_reg(TEST_REG),
                          opnd_create_base_disp(TEST_REG, DR_REG_NULL, 0, 0, OPSZ_PTR));
    write_instr_entry(dr_context, writer, instr_in_gettid,
                      reinterpret_cast<app_pc>(PC_SYSCALL_GETTID));
    write_trace_entry(
        writer,
        make_memref(READ_MEMADDR_GETTID, TRACE_TYPE_READ, opnd_size_in_bytes(OPSZ_PTR)));

    write_trace_entry(writer, make_footer());
    std::cerr << "Done writing system call trace template\n";
    return syscall_trace_template_file;
}

static std::string
postprocess(void *dr_context, std::string syscall_trace_template_file)
{
    std::cerr
        << "Going to post-process raw trace and add system call trace templates to it\n";
    // Get path to write the final trace to.
    const char *raw_dir;
    drmemtrace_status_t mem_res = drmemtrace_get_output_path(&raw_dir);
    assert(mem_res == DRMEMTRACE_SUCCESS);
    std::string outdir = std::string(raw_dir) + DIRSEP + "post_processed";

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
    if (injected_syscall_count != 2) {
        std::cerr << "Incorrect injected syscall count (" << injected_syscall_count
                  << ")\n";
    }
    std::cerr << "Done post-processing the raw trace\n";
    return outdir;
}

basic_counts_t::counters_t
get_basic_counts(const std::string &trace_dir)
{
    auto basic_counts_tool =
        std::unique_ptr<basic_counts_t>(new basic_counts_t(/*verbose=*/0));
    std::vector<analysis_tool_t *> tools;
    tools.push_back(basic_counts_tool.get());
    analyzer_t analyzer(trace_dir, &tools[0], static_cast<int>(tools.size()));
    if (!analyzer) {
        FATAL_ERROR("failed to initialize analyzer: %s",
                    analyzer.get_error_string().c_str());
    }
    if (!analyzer.run()) {
        FATAL_ERROR("failed to run analyzer: %s", analyzer.get_error_string().c_str());
    }
    return basic_counts_tool->get_total_counts();
}

void
gather_trace()
{
    if (setenv("DYNAMORIO_OPTIONS", "-stderr_mask 0xc -client_lib ';;-offline",
               1 /*override*/) != 0)
        std::cerr << "failed to set env var!\n";
    std::cerr << "Pre-DR init\n";
    dr_app_setup();
    assert(!dr_app_running_under_dynamorio());
    std::cerr << "Pre-DR start\n";
    dr_app_start();
    do_some_syscalls();
    std::cerr << "Pre-DR detach\n";
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
    std::cerr << "Verifying resulting user+kernel trace\n";
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
    bool found_getpid_instr = false;
    bool found_gettid_instr = false;
    bool found_gettid_read = false;
    bool have_syscall_trace_type = false;
    int syscall_trace_num = -1;
    int prev_syscall_num_marker = -1;
    for (scheduler_t::stream_status_t status = stream->next_record(memref);
         status != scheduler_t::STATUS_EOF; status = stream->next_record(memref)) {
        assert(status == scheduler_t::STATUS_OK);
        int prev_syscall_num_marker_saved = prev_syscall_num_marker;
        prev_syscall_num_marker = -1;
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
                break;
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
                assert(!found_gettid_instr);
                found_gettid_instr = true;
                if (memref.instr.addr != PC_SYSCALL_GETTID) {
                    std::cerr << "Found incorrect addr (" << std::hex << memref.instr.addr
                              << " vs expected " << PC_SYSCALL_GETTID << std::dec
                              << ") for gettid trace instr.\n";
                    return false;
                }
                if (!check_instr_same(dr_context, memref, instr_in_gettid)) {
                    return false;
                }
            } else {
                assert(!found_gettid_read);
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
        case SYS_getpid:
            if (is_instr) {
                assert(!found_getpid_instr);
                found_getpid_instr = true;
                if (memref.instr.addr != PC_SYSCALL_GETPID) {
                    std::cerr << "Found incorrect addr (" << std::hex << memref.instr.addr
                              << " vs expected " << PC_SYSCALL_GETPID << std::dec
                              << ") for getpid instr.\n";
                    return false;
                }
                if (!check_instr_same(dr_context, memref, instr_in_getpid)) {
                    std::cerr << "Found unexpected instruction for getpid trace.\n";
                    return false;
                }
            } else {
                std::cerr << "Found unexpected data memref in getpid trace\n";
                return false;
            }
            break;
        }
    }
    if (!have_syscall_trace_type) {
        std::cerr << "Trace did not have the expected file type\n";
    } else if (!found_gettid_instr) {
        std::cerr << "Did not find instr in gettid trace\n";
    } else if (!found_getpid_instr) {
        std::cerr << "Did not find instr in getpid trace\n";
    } else if (!found_gettid_read) {
        std::cerr << "Did not find read data memref in gettid trace\n";
    } else {
        std::cerr << "Successfully completed checks\n";
        return true;
    }
    return false;
}

int
test_main(int argc, const char *argv[])
{
    gather_trace();
    void *dr_context = dr_standalone_init();
    std::string syscall_trace_template = write_system_call_template(dr_context);
    std::cerr << "Getting basic counts for system call trace template\n";
    basic_counts_t::counters_t template_counts = get_basic_counts(syscall_trace_template);
    if (!(template_counts.instrs == 2 && template_counts.encodings == 2 &&
          template_counts.syscall_number_markers == 2)) {
        std::cerr << "Unexpected counts in system call trace template: "
                  << syscall_trace_template << ": #instrs: " << template_counts.instrs
                  << ", #encodings: " << template_counts.encodings
                  << ", #syscall_number_markers: "
                  << template_counts.syscall_number_markers << "\n";
        return 1;
    }

    std::string trace_dir = postprocess(dr_context, syscall_trace_template);
    bool success = look_for_syscall_trace(dr_context, trace_dir);
    instr_destroy(dr_context, instr_in_getpid);
    instr_destroy(dr_context, instr_in_gettid);
    dr_standalone_exit();
    if (!success) {
        return 1;
    }
    basic_counts_t::counters_t final_trace_counts = get_basic_counts(trace_dir);
    if (final_trace_counts.kernel_instrs != 2) {
        std::cerr << "Unexpected kernel instr count in the final trace ("
                  << final_trace_counts.kernel_instrs << ")\n";
        return 1;
    }
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
