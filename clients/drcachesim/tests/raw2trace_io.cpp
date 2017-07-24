/* **********************************************************
 * Copyright (c) 2017 Google, Inc.  All rights reserved.
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

static droption_t<std::string> op_indir(DROPTION_SCOPE_FRONTEND, "indir", "",
                                        "[Required] Directory with trace input files",
                                        "Specifies a directory with raw files.");

static droption_t<std::string> op_out(DROPTION_SCOPE_FRONTEND, "out", "",
                                      "[Required] Path to output file",
                                      "Specifies the path to the output file.");
#if defined(X64)
# define SYS_NUM(r) (r).orig_rax
#elif defined(X86)
# define SYS_NUM(r) (r).orig_eax
#else
# error "Test only supports x86"
#endif

#ifndef LINUX
# error "Test only supports Linux"
#endif

int
main(int argc, const char *argv[])
{
    std::string parse_err;
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_FRONTEND, argc, (const char **)argv,
                                       &parse_err, NULL) ||
        op_indir.get_value().empty() || op_out.get_value().empty()) {
        std::cerr << "Usage error: " << parse_err << "\nUsage:\n"
                  << droption_parser_t::usage_short(DROPTION_SCOPE_ALL);
        return 1;
    }

    /* Open input/output files outside of traced region. And explicitly don't destroy dir,
     * so they never get closed.
     */
    raw2trace_directory_t *dir =
        new raw2trace_directory_t(op_indir.get_value(), op_out.get_value());

    pid_t pid = fork();
    if (pid == -1) {
        std::cerr << "Fork failed\n";
        return 1;
    }
    if (pid != 0) {
        /* parent */
        long res;
        int status;
        while (true) {
            int wait_res = waitpid(pid, &status, __WALL);
            if (wait_res < 0) {
                if (errno == EINTR)
                    continue;
                perror("Failed waiting");
                return 1;
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
                return 1;
            }
        }
        if (WIFEXITED(status)) {
            return 0;
        }
        res = ptrace(PTRACE_DETACH, pid, NULL, NULL);
        if (res < 0) {
            perror("ptrace failed");
            return 1;
        }
        std::cerr << "Detached\n";
        return 0;
    } else {
        /* child */
        long res = ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        if (res < 0) {
            perror("ptrace me failed");
            return 1;
        }
        /* Force a wait until parent attaches, so we don't race on the fork. */
        raise(SIGSTOP);

        /* Sycalls below will be ptraced. We don't expect any open/close calls outside of
         * raw2trace::read_and_map_modules().
         */
        raw2trace_t raw2trace(dir->modfile_bytes, dir->thread_files, &dir->out_file,
                              GLOBAL_DCONTEXT, 1);
        std::string error = raw2trace.do_conversion();
        if (!error.empty()) {
            std::cerr << "raw2trace failed " << error << "\n";
            return 1;
        }
        std::cerr << "Processed\n";
        return 0;
    }
    return 0;
}
