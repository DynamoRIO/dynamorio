/* **********************************************************
 * Copyright (c) 2017-2023 Google, Inc.  All rights reserved.
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

/* This application processes a set of existing raw files with raw2trace_t.
 * It uses ptrace to make sure raw2trace only interacts with the filesystem when expected
 * -- it doesn't open/close any files outside of mapping modules (which should be local).
 *
 * XXX: We use ptrace as opposed to running under memtrace with replaced file operations
 * because raw2trace uses drmodtrack, which doesn't isolate under static memtrace.
 */

#include "droption.h"
#include "tracer/raw2trace.h"
#include "tracer/raw2trace_directory.h"
#include <cassert>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <syscall.h>
#include <unistd.h>

namespace dynamorio {
namespace drmemtrace {

using ::dynamorio::droption::droption_parser_t;
using ::dynamorio::droption::DROPTION_SCOPE_ALL;
using ::dynamorio::droption::DROPTION_SCOPE_FRONTEND;
using ::dynamorio::droption::droption_t;

static droption_t<std::string> op_indir(DROPTION_SCOPE_FRONTEND, "indir", "",
                                        "[Required] Directory with trace input files",
                                        "Specifies a directory with raw files.");

#if defined(X64)
#    define SYS_NUM(r) (r).orig_rax
#elif defined(X86)
#    define SYS_NUM(r) (r).orig_eax
#else
#    error "Test only supports x86"
#endif

#ifndef LINUX
#    error "Test only supports Linux"
#endif

#define EXPECT(expr, msg)                               \
    do {                                                \
        if (!(expr)) {                                  \
            std::cerr << "Error: " << msg << std::endl; \
            return false;                               \
        }                                               \
    } while (0)

#define REPORT(msg)                    \
    do {                               \
        std::cerr << msg << std::endl; \
    } while (0)

bool
test_raw2trace(raw2trace_directory_t *dir)
{
    pid_t pid = fork();
    if (pid == -1) {
        std::cerr << "Fork failed\n";
        return false;
    }
    if (pid != 0) {
        /* parent_ */
        long res;
        int status;
        while (true) {
            int wait_res = waitpid(pid, &status, __WALL);
            if (wait_res < 0) {
                if (errno == EINTR)
                    continue;
                perror("Failed waiting");
                return false;
            }
            if (WIFEXITED(status))
                break;
            user_regs_struct regs;
            res = ptrace(PTRACE_GETREGS, pid, NULL, &regs);
            if (res < 0) {
                perror("ptrace failed");
                return 1;
            }
            /* We don't distinguish syscall entry/exit because orig_rax is set
             * on both.
             */
            if (SYS_NUM(regs) == __NR_open || SYS_NUM(regs) == __NR_openat ||
                SYS_NUM(regs) == __NR_creat) {
                std::cerr << "open\n";
            } else if (SYS_NUM(regs) == __NR_close) {
                std::cerr << "close\n";
            }
            res = ptrace(PTRACE_SYSCALL, pid, NULL, 0);
            if (res < 0) {
                perror("ptrace failed");
                return false;
            }
        }
        if (WIFEXITED(status)) {
            return true;
        }
        res = ptrace(PTRACE_DETACH, pid, NULL, NULL);
        if (res < 0) {
            perror("ptrace failed");
            return false;
        }
        std::cerr << "Detached\n";
        return true;
    } else {
        /* child */
        long res = ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        if (res < 0) {
            perror("ptrace me failed");
            return false;
        }
        /* Force a wait until parent_ attaches, so we don't race on the fork. */
        raise(SIGSTOP);

        /* Sycalls below will be ptraced. We don't expect any open/close calls outside of
         * raw2trace::read_and_map_modules().
         */
        raw2trace_t raw2trace(dir->modfile_bytes_, dir->in_files_, dir->out_files_,
                              dir->out_archives_, dir->encoding_file_,
                              dir->serial_schedule_file_, dir->cpu_schedule_file_,
                              GLOBAL_DCONTEXT, 1);
        std::string error = raw2trace.do_conversion();
        if (!error.empty()) {
            std::cerr << "raw2trace failed " << error << "\n";
            return false;
        }
        std::cerr << "Processed\n";
        exit(0);
        // Unreachable
        return false;
    }
}

void
my_user_free(void *data)
{
    REPORT("Custom user_free was called");
}

bool
test_module_mapper(const raw2trace_directory_t *dir)
{
    std::unique_ptr<module_mapper_t> mapper = module_mapper_t::create(
        dir->modfile_bytes_, nullptr, nullptr, nullptr, &my_user_free);
    EXPECT(mapper->get_last_error().empty(), "Module mapper construction failed");
    REPORT("About to load modules");
    const auto &loaded_modules = mapper->get_loaded_modules();
    EXPECT(!loaded_modules.empty(), "Expected module entries");
    EXPECT(mapper->get_last_error().empty(), "Module loading failed");
    REPORT("Loaded modules successfully");
    bool found_simple_app = false;
    for (const module_t &m : mapper->get_loaded_modules()) {
        std::string path = m.path;
        if (path.rfind("simple_app") != std::string::npos) {
            found_simple_app = true;
            break;
        }
    }
    EXPECT(found_simple_app, "Expected app entry not found in module map");
    REPORT("Successfully found app entry");
    return true;
}

bool
test_trace_timestamp_reader(const raw2trace_directory_t *dir)
{
    std::istream *file = dir->in_files_[0];
    // Seek back to the beginning to undo raw2trace_directory_t's validation
    file->seekg(0);

    // XXX: Some parts of this test are brittle wrt trace header changes, like the
    // size of this buffer, and the first read for timestamp2 below that checks the
    // exact position of the timestamp entry. Consider removing some checks or making
    // them flexible in some way.
    offline_entry_t buffer[6];
    file->read((char *)buffer, BUFFER_SIZE_BYTES(buffer));

    std::string error;
    if (!trace_metadata_reader_t::is_thread_start(buffer, &error, nullptr, nullptr) &&
        !error.empty())
        return false;
    uint64 timestamp = 0;
    if (drmemtrace_get_timestamp_from_offline_trace(buffer, BUFFER_SIZE_BYTES(buffer),
                                                    &timestamp) != DRMEMTRACE_SUCCESS)
        return false;
    if (timestamp == 0)
        return false;
    REPORT("Read timestamp from thread header");

    uint64 timestamp2 = 0;
    if (drmemtrace_get_timestamp_from_offline_trace(buffer + 5, sizeof(offline_entry_t),
                                                    &timestamp2) != DRMEMTRACE_SUCCESS)
        return false;
    if (timestamp != timestamp2)
        return false;
    REPORT("Read timestamp without thread header");

    if (drmemtrace_get_timestamp_from_offline_trace(nullptr, 0, &timestamp2) ==
        DRMEMTRACE_SUCCESS)
        return false;
    if (drmemtrace_get_timestamp_from_offline_trace(buffer, 0, &timestamp2) ==
        DRMEMTRACE_SUCCESS)
        return false;
    if (drmemtrace_get_timestamp_from_offline_trace(buffer, sizeof(offline_entry_t),
                                                    &timestamp2) == DRMEMTRACE_SUCCESS)
        return false;
    if (drmemtrace_get_timestamp_from_offline_trace(buffer, BUFFER_SIZE_BYTES(buffer),
                                                    nullptr) == DRMEMTRACE_SUCCESS)
        return false;
    if (timestamp != timestamp2)
        return false;
    REPORT("Verified boundary conditions");

    offline_entry_t invalid_buffer[4];
    memset(invalid_buffer, 0, BUFFER_SIZE_BYTES(invalid_buffer));
    if (drmemtrace_get_timestamp_from_offline_trace(invalid_buffer,
                                                    BUFFER_SIZE_BYTES(invalid_buffer),
                                                    &timestamp2) == DRMEMTRACE_SUCCESS)
        return false;
    if (timestamp != timestamp2)
        return false;
    REPORT("Verified invalid buffer");
    return true;
}

int
test_main(int argc, const char *argv[])
{
    std::string parse_err;
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_FRONTEND, argc, (const char **)argv,
                                       &parse_err, NULL) ||
        op_indir.get_value().empty()) {
        std::cerr << "Usage error: " << parse_err << "\nUsage:\n"
                  << droption_parser_t::usage_short(DROPTION_SCOPE_ALL);
        return 1;
    }

    /* Open input/output files outside of traced region. And explicitly don't destroy dir,
     * so they never get closed.
     */
    raw2trace_directory_t *dir = new raw2trace_directory_t;
    std::string dir_err = dir->initialize(op_indir.get_value(), "");
    if (!dir_err.empty())
        std::cerr << "Directory setup failed: " << dir_err;

    bool test1_ret = test_raw2trace(dir);
    bool test2_ret = test_module_mapper(dir);
    bool test3_ret = test_trace_timestamp_reader(dir);
    if (!(test1_ret && test2_ret && test3_ret))
        return 1;
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
