/* **********************************************************
 * Copyright (c) 2023 Google, Inc.  All rights reserved.
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

#include "../common/utils.h"
#include "elf_loader.h"
#include "intel-pt.h"

namespace dynamorio {
namespace drmemtrace {

#define ERRMSG_HEADER "[elf_loader] "

bool
elf_loader_t::load(IN const char *name, IN uint64_t base,
                   INOUT struct pt_image_section_cache *iscache,
                   INOUT struct pt_image *image)
{
    if (iscache == nullptr || image == nullptr || name == nullptr) {
        ERRMSG(ERRMSG_HEADER "Invalid arguments to load.\n");
        return false;
    }

    std::ifstream f(name, std::ios::binary);
    if (!f.is_open()) {
        ERRMSG(ERRMSG_HEADER "Failed to open elf file %s.\n", name);
    }

    char e_ident[EI_NIDENT];
    f.read(e_ident, EI_NIDENT);
    if (f.gcount() != EI_NIDENT) {
        ERRMSG(ERRMSG_HEADER "Failed to load memory cached section: invalid ELF file.\n");
        return false;
    }

    if (e_ident[EI_CLASS] == ELFCLASS32) {
        return load_elf<Elf32_Ehdr, Elf32_Half, Elf32_Phdr>(f, name, base, iscache,
                                                            image);
    } else if (e_ident[EI_CLASS] == ELFCLASS64) {
        return load_elf<Elf64_Ehdr, Elf64_Half, Elf64_Phdr>(f, name, base, iscache,
                                                            image);
    } else {
        ERRMSG(ERRMSG_HEADER "Failed to load memory cached section: invalid ELF file.\n");
        return false;
    }
}

template <typename Elf_Ehdr, typename Elf_Half, typename Elf_Phdr>
bool
elf_loader_t::load_elf(IN std::ifstream &f, IN const char *name, IN uint64_t base,
                       INOUT struct pt_image_section_cache *iscache,
                       INOUT struct pt_image *image)
{
    if (!f.is_open()) {
        ERRMSG(ERRMSG_HEADER "Failed to load ELF: invalid arguments to load_elf.\n");
    }
    f.seekg(0, std::ios::beg);

    /* Read the ELF header. */
    Elf_Ehdr ehdr;
    f.read(reinterpret_cast<char *>(&ehdr), sizeof(ehdr));
    if (f.gcount() != sizeof(ehdr)) {
        ERRMSG(ERRMSG_HEADER "Failed to load ELF header: invalid ELF file.\n");
        return false;
    }

    f.seekg(ehdr.e_phoff, std::ios::beg);

    /* Determine the load offset. */
    uint64_t offset = 0;
    if (base != 0) {
        uint64_t minaddr = UINT64_MAX;
        for (Elf_Half pidx = 0; pidx < ehdr.e_phnum; ++pidx) {
            Elf_Phdr phdr;
            f.read(reinterpret_cast<char *>(&phdr), sizeof(phdr));
            if (f.gcount() != sizeof(phdr)) {
                ERRMSG(ERRMSG_HEADER
                       "Failed to load ELF program header: invalid ELF file.\n");
                return false;
            }
            if (phdr.p_type != PT_LOAD)
                continue;
            if (phdr.p_vaddr < minaddr)
                minaddr = phdr.p_vaddr;
        }
        offset = base - minaddr;
    }

    int sections = 0;
    for (Elf_Half pidx = 0; pidx < ehdr.e_phnum; ++pidx) {
        Elf_Phdr phdr;
        f.seekg(ehdr.e_phoff + pidx * sizeof(phdr), std::ios::beg);
        f.read(reinterpret_cast<char *>(&phdr), sizeof(phdr));
        if (f.gcount() != sizeof(phdr)) {
            ERRMSG(ERRMSG_HEADER
                   "Failed to load ELF program header: invalid ELF file.\n");
            return false;
        }

        if (phdr.p_type != PT_LOAD || !phdr.p_filesz)
            continue;

        if (!load_section(name, phdr.p_offset, phdr.p_filesz, phdr.p_vaddr + offset,
                          iscache, image)) {
            ERRMSG(ERRMSG_HEADER "Failed to load ELF file: failed to load section.\n");
            return false;
        }
        sections += 1;
    }

    if (sections == 0) {
        ERRMSG(ERRMSG_HEADER
               "Failed to load ELF file: did not find any load sections.\n");
        return false;
    }
    return true;
}

bool
elf_loader_t::load_section(IN const char *name, IN uint64_t offset, IN uint64_t size,
                           IN uint64_t vaddr,
                           INOUT struct pt_image_section_cache *iscache,
                           INOUT struct pt_image *image)
{
    int errcode = 0;
    if (!iscache) {
        errcode = pt_image_add_file(image, name, offset, size, NULL, vaddr);
        if (errcode < 0) {
            ERRMSG(ERRMSG_HEADER "Failed to add section to image.\n");
        }
    } else {
        int isid = pt_iscache_add_file(iscache, name, offset, size, vaddr);
        if (isid >= 0) {
            errcode = pt_image_add_cached(image, iscache, isid, NULL);
            if (errcode < 0) {
                ERRMSG(ERRMSG_HEADER "Failed to add section to image.\n");
            }
        } else {
            ERRMSG(ERRMSG_HEADER "Failed to find section in cache.\n");
            errcode = isid;
        }
    }
    return errcode == 0;
}

} // namespace drmemtrace
} // namespace dynamorio
