/* **********************************************************
 * Copyright (c) 2022-2023 Google, Inc.  All rights reserved.
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

/* kcore.h: the header of the module for copying '/proc/kcore' and '/proc/kallsyms' to the
 * directory that stores raw trace. This module only works on Linux x86_64.
 */

#ifndef _KCORE_COPY_H_
#define _KCORE_COPY_H_ 1

#include <elf.h>

#include "dr_api.h"
#include "drmemtrace.h"

namespace dynamorio {
namespace drmemtrace {

struct proc_kcore_code_segment_t;
struct proc_module_t;

/* This class is used to copy kernel code segments and copy kallsyms.
 */
class kcore_copy_t {
public:
    kcore_copy_t(drmemtrace_open_file_func_t open_file_func,
                 drmemtrace_write_file_func_t write_file_func,
                 drmemtrace_close_file_func_t close_file_func);
    ~kcore_copy_t();

    /* This function is used to copy kcore and kallsyms to the directory passed in.
     */
    bool
    copy(const char *to_dir);

private:
    /* Read the kernel code segments from /proc/kcore to buffer.
     * This function will first read modules from /proc/modules, then a kernel module from
     * /proc/kallsyms. Then it will parse the kernel code segments from /proc/kcore.
     */
    bool
    read_code_segments();

    /* Copy the kernel code segments to one file.
     * This function will copy all kernel code segments to one ELF format file called
     * kcore.
     */
    bool
    copy_kcore(const char *to_dir);

    /* Copy kallsyms to the directory.
     */
    bool
    copy_kallsyms(const char *to_dir);

    /* Read the module information to module list from /proc/modules.
     */
    bool
    read_modules();

    /* Parse the kernel module information from /proc/kallsyms and insert them to the
     * start of module list.
     */
    bool
    read_kallsyms();

    /* Read the kernel code segments from /proc/kcore to buffer.
     */
    bool
    read_kcore();

    /* The shared file open function. */
    drmemtrace_open_file_func_t open_file_func_;

    /* The shared file write function. */
    drmemtrace_write_file_func_t write_file_func_;

    /* The shared file close function. */
    drmemtrace_close_file_func_t close_file_func_;

    /* The module list. */
    proc_module_t *modules_;

    /* The number of kernel code segments. */
    int kcore_code_segments_num_;

    /* The kernel code segments array. */
    proc_kcore_code_segment_t *kcore_code_segments_;

    /* The ELF header of '/proc/kcore'. */
    Elf64_Ehdr proc_kcore_ehdr_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _KCORE_COPY_H_ */
