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

#include <gelf.h>
#include <libelf.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <fstream>

#include "../common/utils.h"
#include "dr_api.h"
#include "kernel_image.h"

#define MODULES_FILE_PATH "/proc/modules"
#define KALLSYMS_FILE_PATH "/proc/kallsyms"
#define KCORE_FILE_PATH "/proc/kcore"
#define KERNEL_IMAGE_FILE_NAME "kimage"
#define KERNEL_IMAGE_METADATA_FILE_NAME "kimage.metadata"

#define MODULE_NEME_MAX_LEN 100
#define SYMBOL_MAX_LEN 300

/* This struct type defines the information of every module read from /proc/module.
 * We store all information on a linked list.
 */
struct proc_module_t {
    proc_module_t *next;
    char name[MODULE_NEME_MAX_LEN];
    uint64_t start;
    uint64_t end;
};

/* This struct type defines the metadata of every code segment read from  /proc/kcore.
 * We store all metadata on a linked list.
 */
struct proc_kcore_code_segment_t {
    proc_kcore_code_segment_t *next;

    /* The start offset of the code segment. */
    uint64_t start;

    /* The length of the code segment. */
    ssize_t len;

    /* The base address of the code segment. */
    uint64_t base;
};

kernel_image_t::kernel_image_t()
    : modules_(nullptr)
    , kcore_code_segments_(nullptr)
{
}

kernel_image_t::~kernel_image_t()
{
    /* Free the module information linked list. */
    proc_module_t *module = modules_;
    while (module) {
        proc_module_t *next = module->next;
        dr_global_free(module, sizeof(proc_module_t));
        module = next;
    }
    modules_ = nullptr;

    /* Free the kcore code segment metadata linked list. */
    proc_kcore_code_segment_t *kcore_code_segment = kcore_code_segments_;
    while (kcore_code_segment) {
        proc_kcore_code_segment_t *next = kcore_code_segment->next;
        dr_global_free(kcore_code_segment, sizeof(proc_kcore_code_segment_t));
        kcore_code_segment = next;
    }
    kcore_code_segments_ = nullptr;
}

bool
kernel_image_t::read_modules()
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
         * We parse the first, the second, and the last field to construct a proc_module_t
         * type instance.
         */
        char mname[MODULE_NEME_MAX_LEN];
        uint64_t addr;
        int len;
        if (dr_sscanf(line.c_str(), "%99s %d %*d %*s %*s " HEX64_FORMAT_STRING, mname,
                      &len, &addr) != 3) {
            ASSERT(false, "failed to parse " MODULES_FILE_PATH);
            f.close();
            return false;
        }

        proc_module_t *module = (proc_module_t *)dr_global_alloc(sizeof(proc_module_t));
        dr_snprintf(module->name, BUFFER_SIZE_ELEMENTS(module->name), "%s", mname);
        NULL_TERMINATE_BUFFER(module->name);
        module->start = addr;
        module->end = addr + len;
        module->next = nullptr;
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
kernel_image_t::read_kallsyms()
{
    std::ifstream f(KALLSYMS_FILE_PATH, std::ios::in);
    if (!f.is_open()) {
        ASSERT(false, "failed to open " KALLSYMS_FILE_PATH);
        return false;
    }
    proc_module_t *kernel_module = nullptr;
    std::string line;
    while (std::getline(f, line)) {
        char name[SYMBOL_MAX_LEN];
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
            dr_snprintf(kernel_module->name, BUFFER_SIZE_ELEMENTS(kernel_module->name),
                        "kernel");
            NULL_TERMINATE_BUFFER(kernel_module->name);
            kernel_module->start = addr;
        } else if (!strcmp(name, "_etext")) {
            if (kernel_module == nullptr) {
                f.close();
                return false;
            }
            kernel_module->end = addr;
            kernel_module->next = modules_;
            modules_ = kernel_module;
            kernel_module = nullptr;
        }
    }
    f.close();
    return true;
}

bool
kernel_image_t::read_kcore()
{
    proc_module_t *module = modules_;
    if (modules_ == nullptr) {
        return false;
    }

    elf_version(EV_CURRENT);
    file_t fd = dr_open_file(KCORE_FILE_PATH, DR_FILE_READ);
    if (fd < 0) {
        ASSERT(false, "failed to open" KCORE_FILE_PATH);
        return false;
    }
    Elf *kcore_elf = elf_begin(fd, ELF_C_READ, nullptr);
    if (kcore_elf == nullptr) {
        ASSERT(false, "failed to read kcore elf");
        dr_close_file(fd);
        return false;
    }

    /* Read phdrs from kcore. */
    size_t knumphdr;
    elf_getphdrnum(kcore_elf, &knumphdr);
    GElf_Phdr *kphdr = (GElf_Phdr *)dr_global_alloc(knumphdr * sizeof(GElf_Phdr));
    memset(kphdr, 0, knumphdr * sizeof(GElf_Phdr));
    for (size_t i = 0; i < knumphdr; i++) {
        if (!gelf_getphdr(kcore_elf, i, &kphdr[i])) {
            dr_global_free(kphdr, knumphdr * sizeof(GElf_Phdr));
            elf_end(kcore_elf);
            dr_close_file(fd);
            return false;
        }
    }

    /* Read code segment metadata from kcore. */
    proc_kcore_code_segment_t *last_kcore_code_segment = kcore_code_segments_;
    do {
        GElf_Phdr *phdr = nullptr;
        for (size_t i = 0; i < knumphdr; i++) {
            if (module->start >= kphdr[i].p_vaddr &&
                module->end < kphdr[i].p_vaddr + kphdr[i].p_filesz) {
                phdr = &kphdr[i];
            }
        }
        if (phdr == nullptr) {
            module = module->next;
            continue;
        }
        proc_kcore_code_segment_t *kcore_code_segment =
            (proc_kcore_code_segment_t *)dr_global_alloc(
                sizeof(proc_kcore_code_segment_t));
        kcore_code_segment->start = module->start - phdr->p_vaddr + phdr->p_offset;
        kcore_code_segment->len = module->end - module->start;
        kcore_code_segment->base = module->start;
        kcore_code_segment->next = nullptr;
        if (last_kcore_code_segment == nullptr) {
            kcore_code_segments_ = kcore_code_segment;
            last_kcore_code_segment = kcore_code_segment;
        } else {
            last_kcore_code_segment->next = kcore_code_segment;
            last_kcore_code_segment = kcore_code_segment;
        }
    } while ((module = module->next) != nullptr);

    dr_global_free(kphdr, knumphdr * sizeof(GElf_Phdr));
    elf_end(kcore_elf);
    dr_close_file(fd);
    return true;
}

bool
kernel_image_t::init()
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
kernel_image_t::dump(const char *to_dir)
{
    file_t kcore_fd = dr_open_file(KCORE_FILE_PATH, DR_FILE_READ);
    if (kcore_fd < 0) {
        ASSERT(false, "failed to open " KCORE_FILE_PATH);
        return false;
    }

    char image_file_name[MAXIMUM_PATH];
    char metadata_file_name[MAXIMUM_PATH];
    dr_snprintf(image_file_name, BUFFER_SIZE_ELEMENTS(image_file_name), "%s%s%s", to_dir,
                DIRSEP, KERNEL_IMAGE_FILE_NAME);
    NULL_TERMINATE_BUFFER(image_file_name);
    dr_snprintf(metadata_file_name, BUFFER_SIZE_ELEMENTS(metadata_file_name), "%s%s%s",
                to_dir, DIRSEP, KERNEL_IMAGE_METADATA_FILE_NAME);
    NULL_TERMINATE_BUFFER(metadata_file_name);
    file_t image_fd = dr_open_file(image_file_name, DR_FILE_WRITE_OVERWRITE);
    if (image_fd < 0) {
        ASSERT(false, "failed to open kernel image file");
        dr_close_file(kcore_fd);
        return false;
    }
    std::ofstream metadata_fd(metadata_file_name, std::ios::out);
    if (!metadata_fd.is_open()) {
        ASSERT(false, "failed to open kernel image metadata file");
        dr_close_file(image_fd);
        dr_close_file(kcore_fd);
        return false;
    }

    uint64_t offset = 0;
    bool dump_success = true;
    proc_kcore_code_segment_t *kcore_code_segment = kcore_code_segments_;
    while (kcore_code_segment != nullptr) {
        dr_file_seek(kcore_fd, kcore_code_segment->start, DR_SEEK_SET);
        char *buf = (char *)dr_global_alloc(kcore_code_segment->len);
        if (dr_read_file(kcore_fd, buf, kcore_code_segment->len) !=
            kcore_code_segment->len) {
            ASSERT(false, "failed to read " KCORE_FILE_PATH);
            dump_success = false;
            break;
        }
        if (dr_write_file(image_fd, buf, kcore_code_segment->len) !=
            kcore_code_segment->len) {
            ASSERT(false, "failed to write code segment to kernel image file");
            dump_success = false;
            break;
        }
        dr_global_free(buf, kcore_code_segment->len);
        metadata_fd << std::hex << offset << " " << kcore_code_segment->len << " "
                    << kcore_code_segment->base << std::endl;
        offset += kcore_code_segment->len;
        kcore_code_segment = kcore_code_segment->next;
    }

    metadata_fd.close();
    dr_close_file(image_fd);
    dr_close_file(kcore_fd);
    return dump_success;
}
