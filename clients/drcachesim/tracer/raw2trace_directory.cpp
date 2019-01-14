/* **********************************************************
 * Copyright (c) 2017-2018 Google, Inc.  All rights reserved.
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

/* raw2trace helper to iterate directories and open files.
 * Separate from raw2trace_t, so that raw2trace doesn't depend on dr_frontend.
 */

#include <algorithm>
#include <cstring>
#include <iostream>
#include <vector>

#ifdef UNIX
#    include <sys/stat.h>
#    include <sys/types.h>
#else
#    define UNICODE
#    define _UNICODE
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#endif
#ifdef HAS_ZLIB
#    include "common/gzip_ostream.h"
#endif

#include "dr_api.h"
#include "dr_frontend.h"
#include "raw2trace.h"
#include "raw2trace_directory.h"
#include "directory_iterator.h"
#include "utils.h"

#define FATAL_ERROR(msg, ...)                               \
    do {                                                    \
        fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__); \
        fflush(stderr);                                     \
        exit(1);                                            \
    } while (0)

#define CHECK(val, msg, ...)                 \
    do {                                     \
        if (!(val))                          \
            FATAL_ERROR(msg, ##__VA_ARGS__); \
    } while (0)

#define VPRINT(level, ...)                     \
    do {                                       \
        if (this->verbosity >= (level)) {      \
            fprintf(stderr, "[drmemtrace]: "); \
            fprintf(stderr, __VA_ARGS__);      \
        }                                      \
    } while (0)

std::string
raw2trace_directory_t::open_thread_files()
{
    VPRINT(1, "Iterating dir %s\n", indir.c_str());
    directory_iterator_t end;
    directory_iterator_t iter(indir);
    if (!iter) {
        return "Failed to list directory " + indir + ": " + iter.error_string();
    }
    for (; iter != end; ++iter)
        open_thread_log_file((*iter).c_str());
    return "";
}

std::string
raw2trace_directory_t::open_thread_log_file(const char *basename)
{
    char path[MAXIMUM_PATH];
    CHECK(basename[0] != '/', "dir iterator entry %s should not be an absolute path\n",
          basename);
    // Skip the module list log.
    if (strcmp(basename, DRMEMTRACE_MODULE_LIST_FILENAME) == 0)
        return "";
    // Skip any non-.raw in case someone put some other file in there.
    const char *basename_pre_suffix = strrchr(basename, '.');
    if (basename_pre_suffix != nullptr)
        basename_pre_suffix = strstr(basename_pre_suffix, OUTFILE_SUFFIX);
    if (basename_pre_suffix == nullptr)
        return "";
    if (dr_snprintf(path, BUFFER_SIZE_ELEMENTS(path), "%s%s%s", indir.c_str(), DIRSEP,
                    basename) <= 0) {
        return "Failed to get full path of file " + std::string(basename);
    }
    NULL_TERMINATE_BUFFER(path);
    in_files.push_back(new std::ifstream(path, std::ifstream::binary));
    if (!(*in_files.back()))
        return "Failed to open thread log file " + std::string(path);
    std::string error = raw2trace_t::check_thread_file(in_files.back());
    if (!error.empty()) {
        return "Failed sanity checks for thread log file " + std::string(path) + ": " +
            error;
    }
    VPRINT(1, "Opened input file %s\n", path);

    // Now open the corresponding output file.
    char outname[MAXIMUM_PATH];
    if (dr_snprintf(outname, BUFFER_SIZE_ELEMENTS(outname), "%.*s",
                    basename_pre_suffix - 1 - basename, basename) <= 0) {
        return "Failed to compute output name for file " + std::string(basename);
    }
    if (dr_snprintf(path, BUFFER_SIZE_ELEMENTS(path), "%s%s%s.%s", outdir.c_str(), DIRSEP,
                    outname, TRACE_SUFFIX) <= 0) {
        return "Failed to compute full path of output file for " + std::string(basename);
    }
    std::ostream *ofile;
#ifdef HAS_ZLIB
    ofile = new gzip_ostream_t(path);
#else
    ofile = new std::ofstream(path, std::ofstream::binary);
#endif
    out_files.push_back(ofile);
    if (!(*out_files.back()))
        return "Failed to open output file " + std::string(path);
    VPRINT(1, "Opened output file %s\n", path);
    return "";
}

std::string
raw2trace_directory_t::read_module_file(const std::string &modfilename)
{
    modfile = dr_open_file(modfilename.c_str(), DR_FILE_READ);
    if (modfile == INVALID_FILE)
        return "Failed to open module file " + modfilename;
    uint64 modfile_size;
    if (!dr_file_size(modfile, &modfile_size))
        return "Failed to get module file size: " + modfilename;
    size_t modfile_size_ = (size_t)modfile_size;
    modfile_bytes = new char[modfile_size_];
    if (dr_read_file(modfile, modfile_bytes, modfile_size_) < (ssize_t)modfile_size_)
        return "Didn't read whole module file " + modfilename;
    return "";
}

std::string
raw2trace_directory_t::tracedir_from_rawdir(const std::string &rawdir_in)
{
    std::string rawdir = rawdir_in;
#ifdef WINDOWS
    std::replace(rawdir.begin(), rawdir.end(), ALT_DIRSEP[0], DIRSEP[0]);
#endif
    // First remove trailing slashes.
    while (rawdir.back() == DIRSEP[0])
        rawdir.pop_back();
    std::string trace_sub(DIRSEP + std::string(TRACE_SUBDIR));
    std::string raw_sub(DIRSEP + std::string(OUTFILE_SUBDIR));
    // If it ends in "/trace", use it directly.
    if (rawdir.size() > trace_sub.size() &&
        rawdir.compare(rawdir.size() - trace_sub.size(), trace_sub.size(), trace_sub) ==
            0)
        return rawdir;
    // If it ends in "/raw", replace with "/trace".
    if (rawdir.size() > raw_sub.size() &&
        rawdir.compare(rawdir.size() - raw_sub.size(), raw_sub.size(), raw_sub) == 0) {
        std::string tracedir = rawdir;
        size_t pos = rawdir.rfind(raw_sub);
        if (pos == std::string::npos)
            CHECK(false, "Internal error: should have returned already");
        tracedir.erase(pos, raw_sub.size());
        tracedir.insert(pos, trace_sub);
        return tracedir;
    }
    // If it contains a "/raw" or "/trace" subdir, add "/trace" to it.
    if (directory_iterator_t::is_directory(rawdir + raw_sub) ||
        directory_iterator_t::is_directory(rawdir + trace_sub)) {
        return rawdir + trace_sub;
    }
    // Use it directly.
    return rawdir;
}

std::string
raw2trace_directory_t::initialize(const std::string &indir_in,
                                  const std::string &outdir_in)
{
    indir = indir_in;
    outdir = outdir_in;
#ifdef WINDOWS
    // Canonicalize.
    std::replace(indir.begin(), indir.end(), ALT_DIRSEP[0], DIRSEP[0]);
#endif
    // Remove trailing slashes.
    while (indir.back() == DIRSEP[0])
        indir.pop_back();
    if (!directory_iterator_t::is_directory(indir))
        return "Directory does not exist: " + indir;
    // Support passing both base dir and raw/ subdir.
    if (indir.find(OUTFILE_SUBDIR) == std::string::npos) {
        indir += std::string(DIRSEP) + OUTFILE_SUBDIR;
    }
    // Support a default outdir.
    if (outdir.empty()) {
        outdir = tracedir_from_rawdir(indir);
        if (!directory_iterator_t::is_directory(outdir)) {
            if (!directory_iterator_t::create_directory(outdir)) {
                return "Failed to create output dir " + outdir;
            }
        }
    }
    std::string modfilename =
        indir + std::string(DIRSEP) + DRMEMTRACE_MODULE_LIST_FILENAME;
    read_module_file(modfilename);

    open_thread_files();
    return "";
}

std::string
raw2trace_directory_t::initialize_module_file(const std::string &module_file_path)
{
    return read_module_file(module_file_path);
}

raw2trace_directory_t::~raw2trace_directory_t()
{
    if (modfile_bytes != nullptr)
        delete[] modfile_bytes;
    if (modfile != INVALID_FILE)
        dr_close_file(modfile);
    for (std::vector<std::istream *>::iterator fi = in_files.begin();
         fi != in_files.end(); ++fi) {
        delete *fi;
    }
    for (std::vector<std::ostream *>::iterator fo = out_files.begin();
         fo != out_files.end(); ++fo) {
        delete *fo;
    }
}
