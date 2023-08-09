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

#include "kcore_copy.h"

#include <elf.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include <fstream>
#include <string>

#include "dr_api.h"
#include "trace_entry.h"
#include "utils.h"
#include "drmemtrace.h"

namespace dynamorio {
namespace drmemtrace {

#define MODULES_FILE_NAME "modules"
#define MODULES_FILE_PATH "/proc/" MODULES_FILE_NAME
#define KALLSYMS_FILE_PATH "/proc/" DRMEMTRACE_KALLSYMS_FILENAME
#define KCORE_FILE_PATH "/proc/" DRMEMTRACE_KCORE_FILENAME

#define KERNEL_SYMBOL_MAX_LEN 300

/* This struct type defines the information of every module read from /proc/module.
 * We store all information on a linked list.
 */
struct proc_module_t {
    proc_module_t *next;

    /* The start virtual address of the module in memory. */
    uint64_t start;

    /* The end virtual address of the module in memory. */
    uint64_t end;
};

/* This struct type defines the metadata of every code segment read from  /proc/kcore. */
struct proc_kcore_code_segment_t {
    /* The start offset of the code segment in /proc/kcore. */
    uint64_t start;

    /* The length of the code segment. */
    ssize_t len;

    /* The virtual address of the code segment in memory. */
    uint64_t vaddr;

    /* The buffer to store the code segment. */
    char *buf;
};

/* The auto close wrapper of file_t.
 * This can ensure the file is closed when it is out of scope. And it is also allowed to
 * customize file manipulation functions.
 */
class file_autoclose_t {
public:
    file_autoclose_t(const char *file_name, int flags,
                     drmemtrace_open_file_func_t open_file_func,
                     drmemtrace_close_file_func_t close_file_func,
                     drmemtrace_read_file_func_t read_file_func,
                     drmemtrace_write_file_func_t write_file_func,
                     bool (*seek_file_func)(file_t f, int64 offset, int origin))
        : fd_(INVALID_FILE)
        , open_file_func_(open_file_func)
        , close_file_func_(close_file_func)
        , read_file_func_(read_file_func)
        , write_file_func_(write_file_func)
        , seek_file_func_(seek_file_func)
    {
        ASSERT(open_file_func_ != NULL, "open_file_func_ cannot be NULL");
        ASSERT(close_file_func_ != NULL, "close_file_func_ cannot be NULL");

        ASSERT(file_name != NULL, "file_name cannot be NULL");
        fd_ = open_file_func_(file_name, flags);
    }

    ~file_autoclose_t()
    {
        ASSERT(close_file_func_ != NULL, "close_file_func_ cannot be NULL");
        if (fd_ != INVALID_FILE) {
            close_file_func_(fd_);
            fd_ = INVALID_FILE;
        }
    }

    bool
    is_open()
    {
        return fd_ != INVALID_FILE;
    }

    bool
    write(IN const void *buf, IN size_t count)
    {
        if (fd_ == INVALID_FILE || write_file_func_ == NULL) {
            return false;
        }
        ssize_t written = write_file_func_(fd_, buf, count);
        return written > 0 && (size_t)written == count;
    }

    ssize_t
    read(INOUT void *buf, IN size_t count)
    {
        if (fd_ == INVALID_FILE || read_file_func_ == NULL) {
            return -1;
        }
        return read_file_func_(fd_, buf, count);
    }

    bool
    seek(IN int64 offset, IN int origin)
    {
        if (fd_ == INVALID_FILE || seek_file_func_ == NULL) {
            return false;
        }
        return seek_file_func_(fd_, offset, origin);
    }

private:
    file_t fd_;
    drmemtrace_open_file_func_t open_file_func_;
    drmemtrace_close_file_func_t close_file_func_;
    drmemtrace_read_file_func_t read_file_func_;
    drmemtrace_write_file_func_t write_file_func_;
    bool (*seek_file_func_)(file_t f, int64 offset, int origin);
};

kcore_copy_t::kcore_copy_t(drmemtrace_open_file_func_t open_file_func,
                           drmemtrace_write_file_func_t write_file_func,
                           drmemtrace_close_file_func_t close_file_func)
    : open_file_func_(open_file_func)
    , write_file_func_(write_file_func)
    , close_file_func_(close_file_func)
    , modules_(nullptr)
    , kcore_code_segments_num_(0)
    , kcore_code_segments_(nullptr)
{
}

kcore_copy_t::~kcore_copy_t()
{
    /* Free the module information linked list. */
    proc_module_t *next_module = modules_;
    while (next_module) {
        proc_module_t *cur_module = next_module;
        next_module = next_module->next;
        dr_global_free(cur_module, sizeof(proc_module_t));
    }
    modules_ = nullptr;

    if (kcore_code_segments_ != nullptr) {
        /* Free the kcore code segment metadata array. */
        for (int i = 0; i < kcore_code_segments_num_; i++) {
            dr_global_free(kcore_code_segments_[i].buf, kcore_code_segments_[i].len);
        }
        dr_global_free(kcore_code_segments_,
                       sizeof(proc_kcore_code_segment_t) * kcore_code_segments_num_);
    }
    kcore_code_segments_ = nullptr;
}

bool
kcore_copy_t::copy(const char *to_dir)
{
    if (!read_code_segments()) {
        ASSERT(false, "failed to read code segments");
        return false;
    }
    if (!copy_kcore(to_dir)) {
        ASSERT(false, "failed to copy " DRMEMTRACE_KCORE_FILENAME);
        return false;
    }
    if (!copy_kallsyms(to_dir)) {
        ASSERT(false, "failed to copy " DRMEMTRACE_KALLSYMS_FILENAME);
        return false;
    }
    return true;
}

bool
kcore_copy_t::read_code_segments()
{
    if (!read_modules()) {
        return false;
    }
    if (!read_kallsyms()) {
        return false;
    }
    if (!read_kcore()) {
        return false;
    }
    return true;
}

bool
kcore_copy_t::copy_kcore(const char *to_dir)
{
    char to_kcore_path[MAXIMUM_PATH];
    dr_snprintf(to_kcore_path, BUFFER_SIZE_ELEMENTS(to_kcore_path), "%s/%s", to_dir,
                DRMEMTRACE_KCORE_FILENAME);
    NULL_TERMINATE_BUFFER(to_kcore_path);
    /* We use drmemtrace file operations functions to dump out code segments in kcore. */
    file_autoclose_t fd(to_kcore_path, DR_FILE_WRITE_OVERWRITE, open_file_func_,
                        close_file_func_, nullptr /*read_file_func*/, write_file_func_,
                        nullptr /* seek_file_func */);

    if (!fd.is_open()) {
        ASSERT(false, "failed to open " DRMEMTRACE_KCORE_FILENAME " for writing");
        return false;
    }

    Elf64_Ehdr to_ehdr;
    memcpy(&to_ehdr.e_ident, proc_kcore_ehdr_.e_ident, EI_NIDENT);
    to_ehdr.e_type = proc_kcore_ehdr_.e_type;
    to_ehdr.e_machine = proc_kcore_ehdr_.e_machine;
    to_ehdr.e_version = proc_kcore_ehdr_.e_version;
    to_ehdr.e_entry = 0;
    to_ehdr.e_shoff = 0;
    to_ehdr.e_flags = proc_kcore_ehdr_.e_flags;
    to_ehdr.e_phnum = kcore_code_segments_num_;
    to_ehdr.e_shentsize = 0;
    to_ehdr.e_shnum = 0;
    to_ehdr.e_shstrndx = 0;
    to_ehdr.e_phoff = sizeof(Elf64_Ehdr);
    to_ehdr.e_ehsize = sizeof(Elf64_Ehdr);
    to_ehdr.e_phentsize = sizeof(Elf64_Phdr);

    uint64_t offset = 0;
    if (!fd.write(&to_ehdr, sizeof(Elf64_Ehdr))) {
        ASSERT(false, "failed to write " DRMEMTRACE_KCORE_FILENAME " header");
        return false;
    }
    offset += sizeof(Elf64_Ehdr);

    Elf64_Phdr *to_phdrs =
        (Elf64_Phdr *)dr_global_alloc(sizeof(Elf64_Phdr) * kcore_code_segments_num_);
    uint64_t code_segment_offset = offset + sizeof(Elf64_Phdr) * kcore_code_segments_num_;
    for (int i = 0; i < kcore_code_segments_num_; ++i) {
        to_phdrs[i].p_type = PT_LOAD;
        to_phdrs[i].p_offset = code_segment_offset;
        to_phdrs[i].p_vaddr = kcore_code_segments_[i].vaddr;
        to_phdrs[i].p_paddr = 0;
        to_phdrs[i].p_filesz = kcore_code_segments_[i].len;
        to_phdrs[i].p_memsz = kcore_code_segments_[i].len;
        to_phdrs[i].p_flags = PF_R | PF_X;
        to_phdrs[i].p_align = 0;
        code_segment_offset += to_phdrs[i].p_filesz;
    }
    if (!fd.write(to_phdrs, sizeof(Elf64_Phdr) * kcore_code_segments_num_)) {
        ASSERT(false, "failed to write the program header to " DRMEMTRACE_KCORE_FILENAME);
        dr_global_free(to_phdrs, sizeof(Elf64_Phdr) * kcore_code_segments_num_);
        return false;
    }
    dr_global_free(to_phdrs, sizeof(Elf64_Phdr) * kcore_code_segments_num_);
    offset += sizeof(Elf64_Phdr) * kcore_code_segments_num_;

    for (int i = 0; i < kcore_code_segments_num_; ++i) {
        if (!fd.write(kcore_code_segments_[i].buf, kcore_code_segments_[i].len)) {
            ASSERT(
                false,
                "failed to write the kernel code segment to " DRMEMTRACE_KCORE_FILENAME);
            return false;
        }
    }

    return true;
}

bool
kcore_copy_t::copy_kallsyms(const char *to_dir)
{
    /* We use DynamoRIO default file operations functions to open and read /proc/kallsyms.
     */
    file_autoclose_t from_kallsyms_fd(
        KALLSYMS_FILE_PATH, DR_FILE_READ, dr_open_file, dr_close_file, dr_read_file,
        nullptr /* write_file_func */, nullptr /* seek_file_func */);
    if (!from_kallsyms_fd.is_open()) {
        ASSERT(false, "failed to open " KALLSYMS_FILE_PATH " for reading");
        return false;
    }

    char to_kallsyms_file_path[MAXIMUM_PATH];
    dr_snprintf(to_kallsyms_file_path, BUFFER_SIZE_ELEMENTS(to_kallsyms_file_path),
                "%s%s%s", to_dir, DIRSEP, DRMEMTRACE_KALLSYMS_FILENAME);
    NULL_TERMINATE_BUFFER(to_kallsyms_file_path);

    /* We use drmemtrace file operations functions to store the output kallsyms. */
    file_autoclose_t to_kallsyms_fd(
        to_kallsyms_file_path, DR_FILE_WRITE_OVERWRITE, open_file_func_, close_file_func_,
        nullptr /* read_file_func */, write_file_func_, nullptr /* seek_file_func */);
    if (!to_kallsyms_fd.is_open()) {
        ASSERT(false, "failed to open " DRMEMTRACE_KALLSYMS_FILENAME " for writing");
        return false;
    }

    char buf[1024];
    ssize_t bytes_read;
    while ((bytes_read = from_kallsyms_fd.read(buf, sizeof(buf))) > 0) {
        if (!to_kallsyms_fd.write(buf, bytes_read)) {
            ASSERT(false, "failed to copy data to " DRMEMTRACE_KALLSYMS_FILENAME);
            return false;
        }
    }

    return true;
}

bool
kcore_copy_t::read_modules()
{
    std::ifstream f(MODULES_FILE_PATH, std::ios::in);
    if (!f.is_open()) {
        ASSERT(false, "failed to open " MODULES_FILE_PATH);
        return false;
    }
    proc_module_t *last_module = modules_;
    std::string line;
    while (std::getline(f, line)) {
        /* Each line is similar to the following line:
         * 'scsi_dh_hp_sw 12895 0 - Live 0xffffffffa005e000'
         * We parse the second and the last field to construct a proc_module_t type
         * instance.
         */
        uint64_t addr = 0x0;
        int len = 0;
        if (dr_sscanf(line.c_str(), "%*s %d %*d %*s %*s " HEX64_FORMAT_STRING, &len,
                      &addr) != 2) {
            ASSERT(false, "failed to parse " MODULES_FILE_PATH);
            f.close();
            return false;
        }

        proc_module_t *module = (proc_module_t *)dr_global_alloc(sizeof(proc_module_t));
        module->start = addr;
        module->end = addr + len;
        module->next = nullptr;
        kcore_code_segments_num_++;
        if (last_module == nullptr) {
            modules_ = module;
            last_module = module;
        } else {
            last_module->next = module;
            last_module = module;
        }
    }
    f.close();
    return true;
}

bool
kcore_copy_t::read_kallsyms()
{
    std::ifstream f(KALLSYMS_FILE_PATH, std::ios::in);
    if (!f.is_open()) {
        ASSERT(false, "failed to open " KALLSYMS_FILE_PATH);
        return false;
    }
    proc_module_t *kernel_module = nullptr;
    std::string line;
    while (std::getline(f, line)) {
        char name[KERNEL_SYMBOL_MAX_LEN];
        uint64_t addr;
        if (dr_sscanf(line.c_str(), HEX64_FORMAT_STRING " %*1c %299s [%*99s", &addr,
                      name) < 2)
            continue;
        if (strcmp(name, "_stext") == 0) {
            if (kernel_module != nullptr) {
                ASSERT(false, "multiple kernel modules found");
                f.close();
                return false;
            }
            kernel_module = (proc_module_t *)dr_global_alloc(sizeof(proc_module_t));
            kernel_module->start = addr;
        } else if (strcmp(name, "_etext") == 0) {
            if (kernel_module == nullptr) {
                ASSERT(false, "failed to find kernel module");
                f.close();
                return false;
            }
            kernel_module->end = addr;
            kernel_module->next = modules_;
            kcore_code_segments_num_++;
            modules_ = kernel_module;
            kernel_module = nullptr;
        }
    }
    ASSERT(kernel_module == nullptr, "failed to find kernel module");
    f.close();
    return true;
}

bool
kcore_copy_t::read_kcore()
{
    ASSERT(modules_ != nullptr,
           "no module found in " MODULES_FILE_PATH " and " KALLSYMS_FILE_PATH);
    file_autoclose_t fd(KCORE_FILE_PATH, DR_FILE_READ, dr_open_file, dr_close_file,
                        dr_read_file, nullptr /* write_file_func */, dr_file_seek);
    if (!fd.is_open()) {
        ASSERT(false, "failed to open" KCORE_FILE_PATH);
        return false;
    }

    uint8_t e_ident[EI_NIDENT];
    if (fd.read(e_ident, sizeof(e_ident)) != sizeof(e_ident)) {
        ASSERT(false, "failed to read the e_ident array of " KCORE_FILE_PATH);
        return false;
    }

    for (int idx = 0; idx < SELFMAG; ++idx) {
        if (e_ident[idx] != ELFMAG[idx]) {
            ASSERT(false, KCORE_FILE_PATH " is not an ELF file");
            return false;
        }
    }
    if (e_ident[EI_CLASS] != ELFCLASS64) {
        ASSERT(false, KCORE_FILE_PATH " is not a 64-bit ELF file");
        return false;
    }

    /* Read phdrs from kcore. */
    if (!fd.seek(0, DR_SEEK_SET)) {
        ASSERT(false, "failed to seek to the begin of " KCORE_FILE_PATH);
        return false;
    }

    if (fd.read(&proc_kcore_ehdr_, sizeof(Elf64_Ehdr)) != sizeof(Elf64_Ehdr)) {
        ASSERT(false, "failed to read the ehdr of " KCORE_FILE_PATH);
        return false;
    }

    if (!fd.seek((long)proc_kcore_ehdr_.e_phoff, DR_SEEK_SET)) {
        ASSERT(false, "failed to seek the program header's end of " KCORE_FILE_PATH);
        return false;
    }

    kcore_code_segments_ = (proc_kcore_code_segment_t *)dr_global_alloc(
        sizeof(proc_kcore_code_segment_t) * kcore_code_segments_num_);
    int idx = 0;
    /* Read code segment metadata from kcore. */
    for (Elf64_Half pidx = 0; pidx < proc_kcore_ehdr_.e_phnum; ++pidx) {
        Elf64_Phdr phdr;
        if (fd.read(&phdr, sizeof(phdr)) != sizeof(phdr)) {
            ASSERT(false, "failed to read the Phdr of " KCORE_FILE_PATH);
            return false;
        }

        if (phdr.p_type != PT_LOAD || phdr.p_filesz == 0)
            continue;

        proc_module_t *module = modules_;
        while (module != nullptr) {
            if (module->start >= phdr.p_vaddr &&
                module->end <= phdr.p_vaddr + phdr.p_filesz) {
                kcore_code_segments_[idx].start =
                    module->start - phdr.p_vaddr + phdr.p_offset;
                kcore_code_segments_[idx].len = module->end - module->start;
                kcore_code_segments_[idx].vaddr = module->start;
                kcore_code_segments_[idx].buf =
                    (char *)dr_global_alloc(kcore_code_segments_[idx].len);
                idx++;
            }
            module = module->next;
        }
    }

    ASSERT(idx == kcore_code_segments_num_,
           "failed to read all kcore code segments' metadata");

    /* Copy code segments from kcore to the buffer in kcore_code_segments_. */
    for (int i = 0; i < kcore_code_segments_num_; ++i) {
        if (!fd.seek(kcore_code_segments_[i].start, DR_SEEK_SET)) {
            ASSERT(false, "failed to seek to the start of a kcore code segment");
            return false;
        }
        if (fd.read(kcore_code_segments_[i].buf, kcore_code_segments_[i].len) !=
            kcore_code_segments_[i].len) {
            ASSERT(false, "failed to read a kcore code segment");
            return false;
        }
    }

    return true;
}

} // namespace drmemtrace
} // namespace dynamorio
