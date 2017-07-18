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

#include <cstring>
#include <iostream>
#include <vector>

#ifdef UNIX
# include <dirent.h> /* opendir, readdir */
# include <unistd.h> /* getcwd */
#else
# define UNICODE
# define _UNICODE
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <direct.h> /* _getcwd */
# pragma comment(lib, "User32.lib")
#endif

#include "dr_api.h"
#include "dr_frontend.h"
#include "raw2trace.h"
#include "raw2trace_directory.h"
#include "utils.h"

#define FATAL_ERROR(msg, ...) do { \
    fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__);    \
    fflush(stderr); \
    exit(1); \
} while (0)

#define CHECK(val, msg, ...) do { \
    if (!(val)) FATAL_ERROR(msg, ##__VA_ARGS__); \
} while (0)

#define VPRINT(level, ...) do { \
    if (this->verbosity >= (level)) { \
        fprintf(stderr, "[drmemtrace]: "); \
        fprintf(stderr, __VA_ARGS__); \
    } \
} while (0)

#ifdef UNIX
void
raw2trace_directory_t::open_thread_files()
{
    struct dirent *ent;
    DIR *dir = opendir(indir.c_str());
    VPRINT(1, "Iterating dir %s\n", indir.c_str());
    if (dir == NULL)
        FATAL_ERROR("Failed to list directory %s", indir.c_str());
    while ((ent = readdir(dir)) != NULL)
        open_thread_log_file(ent->d_name);
    closedir (dir);
}
#else
void
raw2trace_directory_t::open_thread_files()
{
    HANDLE find = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW data;
    char path[MAXIMUM_PATH];
    TCHAR wpath[MAXIMUM_PATH];
    VPRINT(1, "Iterating dir %s\n", indir.c_str());
    // Append \*
    dr_snprintf(path, BUFFER_SIZE_ELEMENTS(path), "%s\\*", indir.c_str());
    NULL_TERMINATE_BUFFER(path);
    if (drfront_char_to_tchar(path, wpath, BUFFER_SIZE_ELEMENTS(wpath)) !=
        DRFRONT_SUCCESS)
        FATAL_ERROR("Failed to convert from utf-8 to utf-16");
    find = FindFirstFileW(wpath, &data);
    if (find == INVALID_HANDLE_VALUE)
        FATAL_ERROR("Failed to list directory %s\n", indir.c_str());
    do {
        if (!TESTANY(data.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY)) {
            if (drfront_tchar_to_char(data.cFileName, path, BUFFER_SIZE_ELEMENTS(path)) !=
                DRFRONT_SUCCESS)
                FATAL_ERROR("Failed to convert from utf-16 to utf-8");
            open_thread_log_file(path);
        }
    } while (FindNextFile(find, &data) != 0);
    FindClose(find);
}
#endif

void
raw2trace_directory_t::open_thread_log_file(const char *basename)
{
    char path[MAXIMUM_PATH];
    CHECK(basename[0] != '/',
          "dir iterator entry %s should not be an absolute path\n", basename);
    // Skip the module list log.
    if (strcmp(basename, DRMEMTRACE_MODULE_LIST_FILENAME) == 0)
        return;
    // Skip any non-.raw in case someone put some other file in there.
    if (strstr(basename, OUTFILE_SUFFIX) == NULL)
        return;
    if (dr_snprintf(path, BUFFER_SIZE_ELEMENTS(path), "%s%s%s",
                    indir.c_str(), DIRSEP, basename) <= 0) {
        FATAL_ERROR("Failed to get full path of file %s", basename);
    }
    NULL_TERMINATE_BUFFER(path);
    thread_files.push_back(new std::ifstream(path, std::ifstream::binary));
    if (!(*thread_files.back()))
        FATAL_ERROR("Failed to open thread log file %s", path);
    std::string error = raw2trace_t::check_thread_file(thread_files.back());
    if (!error.empty()) {
        FATAL_ERROR("Failed sanity checks for thread log file %s: %s", path,
                    error.c_str());
    }
    VPRINT(1, "Opened thread log file %s\n", path);
}

raw2trace_directory_t::raw2trace_directory_t(const std::string &indir_in,
                                             const std::string &outname_in,
                                             unsigned int verbosity_in)
    : indir(indir_in), outname(outname_in), verbosity(verbosity_in)
{
    // Support passing both base dir and raw/ subdir.
    if (indir.find(OUTFILE_SUBDIR) == std::string::npos)
        indir += std::string(DIRSEP) + OUTFILE_SUBDIR;
    std::string modfilename = indir + std::string(DIRSEP) +
        DRMEMTRACE_MODULE_LIST_FILENAME;
    modfile = dr_open_file(modfilename.c_str(), DR_FILE_READ);
    if (modfile == INVALID_FILE)
        FATAL_ERROR("Failed to open module file %s", modfilename.c_str());
    uint64 modfile_size;
    if (!dr_file_size(modfile, &modfile_size))
        FATAL_ERROR("Failed to get module file size: %s", modfilename.c_str());
    size_t modfile_size_ = (size_t)modfile_size;
    modfile_bytes = new char[modfile_size_];
    if (dr_read_file(modfile, modfile_bytes, modfile_size_) < (ssize_t)modfile_size_)
        FATAL_ERROR("Didn't read whole module file %s", modfilename.c_str());

    out_file.open(outname.c_str(), std::ofstream::binary);
    if (!out_file)
        FATAL_ERROR("Failed to open output file %s", outname.c_str());
    VPRINT(1, "Writing to %s\n", outname.c_str());

    open_thread_files();
}

raw2trace_directory_t::~raw2trace_directory_t()
{
    delete[] modfile_bytes;
    dr_close_file(modfile);
    for (std::vector<std::istream*>::iterator fi = thread_files.begin();
         fi != thread_files.end(); ++fi) {
        delete *fi;
    }
}
