/* **********************************************************
 * Copyright (c) 2022 Google, Inc.  All rights reserved.
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

#include <elf.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <fstream>

#include "../common/utils.h"
#include "dr_api.h"
#include "kimage.h"
#include "kcore_copy.h"

#define KIMAGE_FILE_NAME "kimage"
#define MODULES_FILE_NAME "modules"
#define MODULES_FILE_PATH "/proc/" MODULES_FILE_NAME
#define KALLSYMS_FILE_NAME "kallsyms"
#define KALLSYMS_FILE_PATH "/proc/" KALLSYMS_FILE_NAME
#define KCORE_FILE_NAME "kcore"
#define KCORE_FILE_PATH "/proc/" KCORE_FILE_NAME

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

kcore_copy_t::kcore_copy_t()
    : modules_(nullptr)
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
            dr_global_free(kcore_code_segments_[i].buf,
                           kcore_code_segments_[i].len);
        }
        dr_global_free(kcore_code_segments_,
                       sizeof(proc_kcore_code_segment_t) * kcore_code_segments_num_);
    }
    kcore_code_segments_ = nullptr;
}

bool
kcore_copy_t::copy(const char *to_dir)
{
    kcore_copy_t kcore_copy;
    if (!kcore_copy.read_code_segments()) {
        ASSERT(false, "failed to read code segments");
        return false;
    }
    if (!kcore_copy.dump_kimage(to_dir)) {
        ASSERT(false, "failed to dump kimage");
        return false;
    }
    if (!kcore_copy.copy_kallsyms(to_dir)) {
        ASSERT(false, "failed to copy kallsyms");
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
kcore_copy_t::dump_kimage(const char *to_dir)
{
    char kimage_path[MAXIMUM_PATH];
    dr_snprintf(kimage_path, BUFFER_SIZE_ELEMENTS(kimage_path), "%s/%s", to_dir,
                KIMAGE_FILE_NAME);
    file_t fd = dr_open_file(kimage_path, DR_FILE_WRITE_OVERWRITE);
    if (fd == INVALID_FILE) {
        ASSERT(false, "failed to open " KIMAGE_FILE_NAME);
        return false;
    }

    uint64_t offset = 0;
    kimage_hdr_t hdr;
    hdr.code_segments_offset = sizeof(kimage_hdr_t);
    hdr.code_segments_num = kcore_code_segments_num_;
    if (dr_write_file(fd, &hdr, sizeof(kimage_hdr_t)) != (ssize_t)sizeof(kimage_hdr_t)) {
        ASSERT(false, "failed to write the kimage header to " KIMAGE_FILE_NAME);
        dr_close_file(fd);
        return false;
    }
    offset += sizeof(kimage_hdr_t);

    kimage_code_segment_hdr_t *code_segment_hdrs =
        (kimage_code_segment_hdr_t *)dr_global_alloc(sizeof(kimage_code_segment_hdr_t) *
                                                     kcore_code_segments_num_);
    uint64_t code_segment_offset =
        offset + sizeof(kimage_code_segment_hdr_t) * kcore_code_segments_num_;
    for (int i = 0; i < kcore_code_segments_num_; ++i) {
        code_segment_hdrs[i].offset = code_segment_offset;
        code_segment_hdrs[i].len = kcore_code_segments_[i].len;
        code_segment_hdrs[i].vaddr = kcore_code_segments_[i].vaddr;
        code_segment_offset += code_segment_hdrs[i].len;
    }
    if (dr_write_file(fd, code_segment_hdrs,
                      sizeof(kimage_code_segment_hdr_t) * kcore_code_segments_num_) !=
        (ssize_t)(sizeof(kimage_code_segment_hdr_t) * kcore_code_segments_num_)) {
        ASSERT(false,
               "failed to write the header of all kernel code segments "
               "to " KIMAGE_FILE_NAME);
        dr_global_free(code_segment_hdrs,
                       sizeof(kimage_code_segment_hdr_t) * kcore_code_segments_num_);
        dr_close_file(fd);
        return false;
    }
    dr_global_free(code_segment_hdrs,
                   sizeof(kimage_code_segment_hdr_t) * kcore_code_segments_num_);
    offset += sizeof(kimage_code_segment_hdr_t) * kcore_code_segments_num_;

    for (int i = 0; i < kcore_code_segments_num_; ++i) {
        if (dr_write_file(fd, kcore_code_segments_[i].buf, kcore_code_segments_[i].len) !=
            (ssize_t)(kcore_code_segments_[i].len)) {
            ASSERT(false, "failed to write the kernel code segment to " KIMAGE_FILE_NAME);
            dr_close_file(fd);
            return false;
        }
        offset += kcore_code_segments_[i].len;
    }

    dr_close_file(fd);
    return true;
}

bool
kcore_copy_t::copy_kallsyms(const char *to_dir)
{
    char to_kallsyms_file_path[MAXIMUM_PATH];
    dr_snprintf(to_kallsyms_file_path, BUFFER_SIZE_ELEMENTS(to_kallsyms_file_path),
                "%s%s%s", to_dir, DIRSEP, KALLSYMS_FILE_NAME);
    NULL_TERMINATE_BUFFER(to_kallsyms_file_path);

    file_t from_kallsyms_fd = dr_open_file(KALLSYMS_FILE_PATH, DR_FILE_READ);
    if (from_kallsyms_fd < 0) {
        ASSERT(false, "failed to open " KALLSYMS_FILE_PATH);
        return false;
    }
    file_t to_kallsyms_fd = dr_open_file(to_kallsyms_file_path, DR_FILE_WRITE_OVERWRITE);
    if (to_kallsyms_fd < 0) {
        ASSERT(false, "failed to open kallsyms file");
        dr_close_file(from_kallsyms_fd);
        return false;
    }

    char buf[1024];
    ssize_t bytes_read;
    while ((bytes_read = dr_read_file(from_kallsyms_fd, buf, sizeof(buf))) > 0) {
        if (dr_write_file(to_kallsyms_fd, buf, bytes_read) != bytes_read) {
            ASSERT(false, "failed to write kallsyms file");
            dr_close_file(to_kallsyms_fd);
            dr_close_file(from_kallsyms_fd);
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
                f.close();
                return false;
            }
            kernel_module = (proc_module_t *)dr_global_alloc(sizeof(proc_module_t));
            kernel_module->start = addr;
        } else if (strcmp(name, "_etext") == 0) {
            if (kernel_module == nullptr) {
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
    f.close();
    return true;
}

bool
kcore_copy_t::read_kcore()
{
    file_t fd = dr_open_file(KCORE_FILE_PATH, DR_FILE_READ);
    if (fd < 0) {
        ASSERT(false, "failed to open" KCORE_FILE_PATH);
        return false;
    }

    uint8_t e_ident[EI_NIDENT];
    if (dr_read_file(fd, e_ident, sizeof(e_ident)) != sizeof(e_ident)) {
        ASSERT(false, "failed to read the e_ident array of " KCORE_FILE_PATH);
        dr_close_file(fd);
        return false;
    }

    for (int idx = 0; idx < SELFMAG; ++idx) {
        if (e_ident[idx] != ELFMAG[idx]) {
            ASSERT(false, KCORE_FILE_PATH " is not an ELF file");
            dr_close_file(fd);
            return false;
        }
    }
    if (e_ident[EI_CLASS] != ELFCLASS64) {
        ASSERT(false, KCORE_FILE_PATH " is not a 64-bit ELF file");
        dr_close_file(fd);
        return false;
    }

    /* Read phdrs from kcore. */
    if (!dr_file_seek(fd, 0, DR_SEEK_SET)) {
        ASSERT(false, "failed to seek to the begin of " KCORE_FILE_PATH);
        dr_close_file(fd);
        return false;
    }

    Elf64_Ehdr ehdr;
    if (dr_read_file(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
        ASSERT(false, "failed to read the ehdr of " KCORE_FILE_PATH);
        dr_close_file(fd);
        return false;
    }
    if (LONG_MAX < ehdr.e_phoff) {
        ASSERT(false, "the ELF header of " KCORE_FILE_PATH " is too big");
        dr_close_file(fd);
        return false;
    }

    if (!dr_file_seek(fd, (long)ehdr.e_phoff, DR_SEEK_SET)) {
        ASSERT(false, "failed to seek the program header's end of " KCORE_FILE_PATH);
        dr_close_file(fd);
        return false;
    }

    kcore_code_segments_ = (proc_kcore_code_segment_t *)dr_global_alloc(
        sizeof(proc_kcore_code_segment_t) * kcore_code_segments_num_);
    int idx = 0;
    /* Read code segment metadata from kcore. */
    for (Elf64_Half pidx = 0; pidx < ehdr.e_phnum; ++pidx) {
        Elf64_Phdr phdr;
        if (dr_read_file(fd, &phdr, sizeof(phdr)) != sizeof(phdr)) {
            ASSERT(false, "failed to read the Phdr of " KCORE_FILE_PATH);
            dr_close_file(fd);
            return false;
        }

        if (phdr.p_type != PT_LOAD || phdr.p_filesz == 0)
            continue;

        proc_module_t *module = modules_;
        if (module == nullptr) {
            ASSERT(false, "no module found in " MODULES_FILE_PATH);
            dr_close_file(fd);
            return false;
        }
        while (module != nullptr) {
            if (module->start >= phdr.p_vaddr &&
                module->end < phdr.p_vaddr + phdr.p_filesz) {
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
        if (!dr_file_seek(fd, kcore_code_segments_[i].start, DR_SEEK_SET)) {
            ASSERT(false, "failed to seek to the start of a kcore code segment");
            dr_close_file(fd);
            return false;
        }
        if (dr_read_file(fd, kcore_code_segments_[i].buf, kcore_code_segments_[i].len) !=
            kcore_code_segments_[i].len) {
            ASSERT(false, "failed to read a kcore code segment");
            dr_close_file(fd);
            return false;
        }
    }

    dr_close_file(fd);
    return true;
}
