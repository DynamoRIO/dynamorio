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

#ifdef UNIX
#    include <sys/types.h>
#else
#    define UNICODE
#    define _UNICODE
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "archive_ostream.h"
#include "directory_iterator.h"
#include "dr_api.h"              // Must be after windows.h.
#include "raw2trace_directory.h" // Includes dr_api.h which must be after windows.h.
#include "reader.h"
#include "raw2trace.h"
#include "trace_entry.h"
#include "utils.h"
#ifdef HAS_ZLIB
#    include "common/gzip_istream.h"
#    include "common/gzip_ostream.h"
#    include "common/zlib_istream.h"
#    ifdef HAS_ZIP
#        include "common/zipfile_ostream.h"
#    endif
#endif
#ifdef HAS_SNAPPY
#    include "common/snappy_istream.h"
#endif
#ifdef HAS_LZ4
#    include "common/lz4_istream.h"
#    include "common/lz4_ostream.h"
#endif

namespace dynamorio {
namespace drmemtrace {

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

#undef VPRINT
#define VPRINT(level, ...)                     \
    do {                                       \
        if (this->verbosity_ >= (level)) {     \
            fprintf(stderr, "[drmemtrace]: "); \
            fprintf(stderr, __VA_ARGS__);      \
        }                                      \
    } while (0)

std::string
raw2trace_directory_t::open_thread_files()
{
    VPRINT(1, "Iterating dir %s\n", indir_.c_str());
    directory_iterator_t end;
    directory_iterator_t iter(indir_);
    if (!iter) {
        return "Failed to list directory " + indir_ + ": " + iter.error_string();
    }
    for (; iter != end; ++iter) {
        std::string error = open_thread_log_file((*iter).c_str());
        if (!error.empty())
            return error;
    }
    return "";
}

std::string
raw2trace_directory_t::trace_suffix()
{
    if (compress_type_ == "zip") {
#ifdef HAS_ZIP
        return TRACE_SUFFIX_ZIP;
#endif
    } else if (compress_type_ == "gzip") {
#ifdef HAS_ZLIB
        return TRACE_SUFFIX_GZ;
#endif
    } else if (compress_type_ == "lz4") {
#ifdef HAS_LZ4
        return TRACE_SUFFIX_LZ4;
#endif
    }
    return TRACE_SUFFIX;
}

std::string
raw2trace_directory_t::open_thread_log_file(const char *basename)
{
    char path[MAXIMUM_PATH];
    CHECK(basename[0] != '/', "dir iterator entry %s should not be an absolute path\n",
          basename);
    // Skip the auxiliary files.
    if (strcmp(basename, DRMEMTRACE_MODULE_LIST_FILENAME) == 0 ||
        strcmp(basename, DRMEMTRACE_FUNCTION_LIST_FILENAME) == 0 ||
        strcmp(basename, DRMEMTRACE_ENCODING_FILENAME) == 0)
        return "";
    // Skip any non-.raw in case someone put some other file in there.
    const char *basename_dot = strrchr(basename, '.');
    if (basename_dot == nullptr)
        return "";
    const char *basename_pre_suffix = nullptr;
#ifdef HAS_ZLIB
    bool is_gzipped = false, is_zlib = false;
    if (strlen(basename) > strlen(OUTFILE_SUFFIX_GZ) + 1) {
        basename_pre_suffix = strstr(
            basename + strlen(basename) - strlen(OUTFILE_SUFFIX_GZ), OUTFILE_SUFFIX_GZ);
        if (basename_pre_suffix != nullptr) {
            is_gzipped = true;
        } else {
            if (strlen(basename) > strlen(OUTFILE_SUFFIX_ZLIB) + 1) {
                basename_pre_suffix =
                    strstr(basename + strlen(basename) - strlen(OUTFILE_SUFFIX_ZLIB),
                           OUTFILE_SUFFIX_ZLIB);
                if (basename_pre_suffix != nullptr) {
                    is_zlib = true;
                }
            }
        }
    }
#endif
#ifdef HAS_SNAPPY
    bool is_snappy = false;
    if (strlen(basename) > strlen(OUTFILE_SUFFIX_SZ) + 1) {
        if (basename_pre_suffix == nullptr) {
            basename_pre_suffix =
                strstr(basename + strlen(basename) - strlen(OUTFILE_SUFFIX_SZ),
                       OUTFILE_SUFFIX_SZ);
            if (basename_pre_suffix != nullptr) {
                is_snappy = true;
            }
        }
    }
#endif
#ifdef HAS_LZ4
    bool is_lz4 = false;
    if (strlen(basename) > strlen(OUTFILE_SUFFIX_LZ4) + 1) {
        if (basename_pre_suffix == nullptr) {
            basename_pre_suffix =
                strstr(basename + strlen(basename) - strlen(OUTFILE_SUFFIX_LZ4),
                       OUTFILE_SUFFIX_LZ4);
            if (basename_pre_suffix != nullptr) {
                is_lz4 = true;
            }
        }
    }
#endif

    if (basename_pre_suffix == nullptr)
        basename_pre_suffix = strstr(basename_dot, OUTFILE_SUFFIX);
    if (basename_pre_suffix == nullptr)
        return "";
    if (dr_snprintf(path, BUFFER_SIZE_ELEMENTS(path), "%s%s%s", indir_.c_str(), DIRSEP,
                    basename) <= 0) {
        return "Failed to get full path of file " + std::string(basename);
    }
    NULL_TERMINATE_BUFFER(path);
    std::istream *ifile = nullptr;
#ifdef HAS_ZLIB
    if (is_gzipped)
        ifile = new gzip_istream_t(path);
    else if (is_zlib)
        ifile = new zlib_istream_t(path);
#endif
#ifdef HAS_SNAPPY
    if (is_snappy) {
        if (ifile != nullptr)
            return "Internal Error in determining input file type.";
        ifile = new snappy_istream_t(path);
    }
#endif
#ifdef HAS_LZ4
    if (is_lz4) {
        if (ifile != nullptr)
            return "Internal Error in determining input file type.";
        ifile = new lz4_istream_t(path);
    }
#endif
    if (ifile == nullptr)
        ifile = new std::ifstream(path, std::ifstream::binary);
    in_files_.push_back(ifile);
    if (!(*in_files_.back()))
        return "Failed to open thread log file " + std::string(path);
    std::string error = raw2trace_t::check_thread_file(in_files_.back());
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
    if (dr_snprintf(path, BUFFER_SIZE_ELEMENTS(path), "%s%s%s.%s", outdir_.c_str(),
                    DIRSEP, outname, trace_suffix().c_str()) <= 0) {
        return "Failed to compute full path of output file for " + std::string(basename);
    }

    std::ostream *ofile = nullptr;
    if (compress_type_ == "zip") {
#ifdef HAS_ZIP
        ofile = new zipfile_ostream_t(path);
        out_archives_.push_back(reinterpret_cast<archive_ostream_t *>(ofile));
        if (!(*out_archives_.back()))
            return "Failed to open output file " + std::string(path);

        VPRINT(1, "Opened output file %s\n", path);
        return "";
#endif
    } else if (compress_type_ == "gzip") {
#ifdef HAS_ZLIB
        ofile = new gzip_ostream_t(path);
#endif
    } else if (compress_type_ == "lz4") {
#ifdef HAS_LZ4
        ofile = new lz4_ostream_t(path);
#endif
    }
    if (!ofile) {
        ofile = new std::ofstream(path, std::ofstream::binary);
    }
    out_files_.push_back(ofile);
    if (!(*out_files_.back()))
        return "Failed to open output file " + std::string(path);

    VPRINT(1, "Opened output file %s\n", path);
    return "";
}

#ifdef BUILD_PT_POST_PROCESSOR
std::string
raw2trace_directory_t::open_kthread_files()
{
    VPRINT(1, "Iterating dir %s\n", kernel_indir_.c_str());
    directory_iterator_t end;
    directory_iterator_t iter(kernel_indir_);
    if (!iter) {
        return "Failed to list directory " + kernel_indir_ + ": " + iter.error_string();
    }
    for (; iter != end; ++iter) {
        const char *basename = (*iter).c_str();
        char path[MAXIMUM_PATH];
        CHECK(basename[0] != '/',
              "dir iterator entry %s should not be an absolute path\n", basename);

        /* Skip kcore and kallsys. */
        if (strcmp(basename, DRMEMTRACE_KCORE_FILENAME) == 0 ||
            strcmp(basename, DRMEMTRACE_KALLSYMS_FILENAME) == 0)
            continue;

        /* Skip any non-.raw.pt in case someone put some other file in there. */
        const char *basename_pre_suffix = nullptr;
        if (strlen(basename) > strlen(OUTFILE_SUFFIX_PT) + 1) {
            basename_pre_suffix =
                strstr(basename + strlen(basename) - strlen(OUTFILE_SUFFIX_PT),
                       OUTFILE_SUFFIX_PT);
        }
        if (basename_pre_suffix == nullptr)
            continue;

        /* Get the complete file path for this kernel file. */
        if (dr_snprintf(path, BUFFER_SIZE_ELEMENTS(path), "%s%s%s", kernel_indir_.c_str(),
                        DIRSEP, basename) <= 0) {
            return "Failed to get full path of file " + std::string(basename);
        }
        NULL_TERMINATE_BUFFER(path);

        std::istream *ifile = nullptr;
        if (ifile == nullptr)
            ifile = new std::ifstream(path, std::ifstream::binary);
        if (!((std::ifstream *)ifile)->is_open()) {
            delete ifile;
            return "Failed to open kernel thread file " + std::string(path);
        }
        std::string error = raw2trace_t::check_kthread_file(ifile);
        if (!error.empty()) {
            delete ifile;
            return "Failed sanity checks for kernel thread file " + std::string(path) +
                ": " + error;
        }
        thread_id_t tid = INVALID_THREAD_ID;
        error = raw2trace_t::get_kthread_file_tid(ifile, &tid);
        if (!error.empty()) {
            delete ifile;
            return "Failed to get thread id for kernel thread file " + std::string(path) +
                ": " + error;
        }

        in_kfiles_map_[tid] = ifile;
        VPRINT(1, "Opened input kernel thread file %s\n", path);
    }
    return "";
}
#endif

std::string
raw2trace_directory_t::open_serial_schedule_file()
{
#ifndef HAS_ZIP
    // We could support writing this out by refactoring the raw2trace code,
    // but it's mostly for fast skipping which requires zip files anyway: thus we
    // just leave serial_schedule_file_ as nullptr and don't write out a file.
    return "";
#else
#    ifdef HAS_ZLIB
    const char *suffix = ".gz";
#    else
    const char *suffix = "";
#    endif
    char path[MAXIMUM_PATH];
    if (dr_snprintf(path, BUFFER_SIZE_ELEMENTS(path), "%s%s%s%s", outdir_.c_str(), DIRSEP,
                    DRMEMTRACE_SERIAL_SCHEDULE_FILENAME, suffix) <= 0) {
        return "Failed to compute full path for " +
            std::string(DRMEMTRACE_SERIAL_SCHEDULE_FILENAME);
    }
#    ifdef HAS_ZLIB
    serial_schedule_file_ = new gzip_ostream_t(path);
#    else
    serial_schedule_file_ = new std::ofstream(path, std::ofstream::binary);
#    endif
    if (!*serial_schedule_file_)
        return "Failed to open serial schedule file " + std::string(path);
    VPRINT(1, "Opened serial schedule file %s\n", path);
    return "";
#endif
}

std::string
raw2trace_directory_t::open_cpu_schedule_file()
{
#ifndef HAS_ZIP
    // Not supported; just leave cpu_schedule_file_ as nullptr.
    return "";
#else
    char path[MAXIMUM_PATH];
    if (dr_snprintf(path, BUFFER_SIZE_ELEMENTS(path), "%s%s%s", outdir_.c_str(), DIRSEP,
                    DRMEMTRACE_CPU_SCHEDULE_FILENAME) <= 0) {
        return "Failed to compute full path for " +
            std::string(DRMEMTRACE_CPU_SCHEDULE_FILENAME);
    }
    cpu_schedule_file_ = new zipfile_ostream_t(path);
    if (!*cpu_schedule_file_)
        return "Failed to open cpu schedule file " + std::string(path);
    VPRINT(1, "Opened cpu schedule file %s\n", path);
    return "";
#endif
}

std::string
raw2trace_directory_t::read_module_file(const std::string &modfilename)
{
    modfile_ = dr_open_file(modfilename.c_str(), DR_FILE_READ);
    if (modfile_ == INVALID_FILE)
        return "Failed to open module file " + modfilename;
    uint64 modfile_size;
    if (!dr_file_size(modfile_, &modfile_size))
        return "Failed to get module file size: " + modfilename;
    size_t modfile_size_ = (size_t)modfile_size;
    modfile_bytes_ = new char[modfile_size_];
    if (dr_read_file(modfile_, modfile_bytes_, modfile_size_) < (ssize_t)modfile_size_)
        return "Didn't read whole module file " + modfilename;
    return "";
}

bool
raw2trace_directory_t::is_window_subdir(const std::string &dir)
{
    return dir.rfind(WINDOW_SUBDIR_PREFIX) != std::string::npos &&
        dir.rfind(WINDOW_SUBDIR_PREFIX) >= dir.size() - strlen(WINDOW_SUBDIR_FIRST);
}

std::string
raw2trace_directory_t::window_subdir_if_present(const std::string &dir)
{
    // Support window subdirs.  If the base is passed, target the first.
    if (is_window_subdir(dir))
        return dir;
    std::string windir = dir + std::string(DIRSEP) + WINDOW_SUBDIR_FIRST;
    if (directory_iterator_t::is_directory(windir))
        return windir;
    return dir;
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
        return window_subdir_if_present(rawdir);
    // If it ends in "/raw" or a window subdir, replace with "/trace".
    if ((rawdir.size() > raw_sub.size() &&
         rawdir.compare(rawdir.size() - raw_sub.size(), raw_sub.size(), raw_sub) == 0) ||
        is_window_subdir(rawdir)) {
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
        return window_subdir_if_present(rawdir + trace_sub);
    }
    // Use it directly.
    return rawdir;
}

std::string
raw2trace_directory_t::initialize(const std::string &indir, const std::string &outdir,
                                  const std::string &compress)
{
    indir_ = indir;
    outdir_ = outdir;
    compress_type_ = compress;
#ifdef WINDOWS
    // Canonicalize.
    std::replace(indir_.begin(), indir_.end(), ALT_DIRSEP[0], DIRSEP[0]);
#endif
    // Remove trailing slashes.
    while (indir_.back() == DIRSEP[0])
        indir_.pop_back();
    if (!directory_iterator_t::is_directory(indir_))
        return "Directory does not exist: " + indir_;
    // Support passing both base dir and raw/ subdir.
    if (!is_window_subdir(indir_) &&
        (indir_.rfind(OUTFILE_SUBDIR) == std::string::npos ||
         indir_.rfind(OUTFILE_SUBDIR) < indir_.size() - strlen(OUTFILE_SUBDIR))) {
        indir_ += std::string(DIRSEP) + OUTFILE_SUBDIR;
    }
    std::string modfile_dir = indir_;
    // Support window subdirs.
    indir_ = window_subdir_if_present(indir_);
    if (is_window_subdir(indir_)) {
        // If we're operating on a specific window, point at the parent for the modfile.
        // Windows dr_open_file() doesn't like "..".
        modfile_dir = indir_;
        auto pos = modfile_dir.rfind(DIRSEP);
        if (pos == std::string::npos)
            return "Window subdir missing slash";
        modfile_dir.erase(pos);
    }

    // Support a default outdir_.
    if (outdir_.empty()) {
        outdir_ = tracedir_from_rawdir(indir_);
        if (!directory_iterator_t::is_directory(outdir_)) {
            if (!directory_iterator_t::create_directory(outdir_)) {
                return "Failed to create output dir " + outdir_;
            }
        }
    }
    std::string modfilename =
        modfile_dir + std::string(DIRSEP) + DRMEMTRACE_MODULE_LIST_FILENAME;
    std::string err = read_module_file(modfilename);
    if (!err.empty())
        return err;

    std::string encoding_filename =
        modfile_dir + std::string(DIRSEP) + DRMEMTRACE_ENCODING_FILENAME;
    // Older traces do not have encoding files.
    // If we had the version we could check OFFLINE_FILE_VERSION_ENCODINGS but
    // we don't currently read that; raw2trace will check it for us.
    // TODO i#2062: When raw2trace support is added, check the version.
    if (dr_file_exists(encoding_filename.c_str())) {
        encoding_file_ = dr_open_file(encoding_filename.c_str(), DR_FILE_READ);
        if (encoding_file_ == INVALID_FILE)
            return "Failed to open encoding file " + encoding_filename;
    }

    // Open the schedule output files.
    err = open_serial_schedule_file();
    if (!err.empty())
        return err;
    err = open_cpu_schedule_file();
    if (!err.empty())
        return err;

    kcoredir_ = "";
    kallsymsdir_ = "";
#ifdef BUILD_PT_POST_PROCESSOR
    /* Open the kernel files. */
    kernel_indir_ = modfile_dir + std::string(DIRSEP) + ".." + std::string(DIRSEP) +
        DRMEMTRACE_KERNEL_PT_SUBDIR;
    /* If the -enable_kernel_tracing option is not specified during tracing, the output
     * directory will not include a kernel directory, and raw2trace will not process it.
     */
    if (directory_iterator_t::is_directory(kernel_indir_)) {
        kcoredir_ = kernel_indir_ + std::string(DIRSEP) + DRMEMTRACE_KCORE_FILENAME;
        kallsymsdir_ = kernel_indir_ + std::string(DIRSEP) + DRMEMTRACE_KALLSYMS_FILENAME;
        err = open_kthread_files();
        if (!err.empty())
            return err;
    }
#else
    kernel_indir_ = "";
#endif

    return open_thread_files();
}

std::string
raw2trace_directory_t::initialize_module_file(const std::string &module_file_path)
{
    return read_module_file(module_file_path);
}

std::string
raw2trace_directory_t::initialize_funclist_file(
    const std::string &funclist_file_path,
    OUT std::vector<std::vector<std::string>> *entries)
{
    std::ifstream stream(funclist_file_path);
    if (!stream.good())
        return "Failed to open " + funclist_file_path;
    std::string line;
    while (std::getline(stream, line)) {
        std::vector<std::string> fields;
        size_t comma;
        do {
            comma = line.find(',');
            fields.push_back(line.substr(0, comma));
            line.erase(0, comma + 1);
        } while (comma != std::string::npos);
        entries->push_back(fields);
    }
    return "";
}

raw2trace_directory_t::~raw2trace_directory_t()
{
    if (modfile_bytes_ != nullptr)
        delete[] modfile_bytes_;
    if (modfile_ != INVALID_FILE)
        dr_close_file(modfile_);
    if (encoding_file_ != INVALID_FILE)
        dr_close_file(encoding_file_);
    for (std::vector<std::istream *>::iterator fi = in_files_.begin();
         fi != in_files_.end(); ++fi) {
        delete *fi;
    }
    for (std::vector<std::ostream *>::iterator fo = out_files_.begin();
         fo != out_files_.end(); ++fo) {
        delete *fo;
    }
    for (auto *archive : out_archives_) {
        delete archive;
    }
    if (serial_schedule_file_ != nullptr)
        delete serial_schedule_file_;
    if (cpu_schedule_file_ != nullptr)
        delete cpu_schedule_file_;
    for (auto kfi : in_kfiles_map_) {
        delete kfi.second;
    }
    dr_standalone_exit();
}

} // namespace drmemtrace
} // namespace dynamorio
