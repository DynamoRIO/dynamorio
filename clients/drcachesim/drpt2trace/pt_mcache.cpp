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

#include "../common/utils.h"
#include "pt_mcache.h"
extern "C" {
#include "pt_asid.h"
}

#define ERRMSG_HEADER "[pt_mcache] "

/* The mapping information of the memory cached section. */
typedef struct _pt_mcached_sec_mapping_t {
    /* The base address of the memory cached section. */
    uint8_t *base;

    /* The memory size of the memory cached section. */
    uint64_t size;

    /* The begin and end of the memory cached section. */
    const uint8_t *begin, *end;
} pt_mcached_sec_mapping_t;

int
pt_mcached_sec_unmap(struct pt_section *section)
{
    pt_mcached_sec_mapping_t *mapping;

    if (!section)
        return -pte_internal;

    mapping = (pt_mcached_sec_mapping_t *)section->mapping;
    if (!mapping || !section->unmap || !section->read || !section->memsize)
        return -pte_internal;

    section->mapping = nullptr;
    section->unmap = nullptr;
    section->read = nullptr;
    section->memsize = nullptr;

    free(mapping->base);
    free(mapping);

    return 0;
}

int
pt_mcached_sec_read(const struct pt_section *section, uint8_t *buffer, uint16_t size,
                    uint64_t offset)
{
    pt_mcached_sec_mapping_t *mapping;
    const uint8_t *begin;

    if (!buffer || !section)
        return -pte_internal;

    mapping = (pt_mcached_sec_mapping_t *)section->mapping;
    if (!mapping)
        return -pte_internal;

    begin = mapping->begin + offset;

    memcpy(buffer, begin, size);
    return (int)size;
}

int
pt_mcached_sec_memsize(const struct pt_section *section, uint64_t *size)
{
    pt_mcached_sec_mapping_t *mapping;
    const uint8_t *begin, *end;

    if (!section || !size)
        return -pte_internal;

    mapping = (pt_mcached_sec_mapping_t *)section->mapping;
    if (!mapping)
        return -pte_internal;

    begin = mapping->base;
    end = mapping->end;

    if (!begin || !end || end < begin)
        return -pte_internal;

    *size = (uint64_t)(end - begin);

    return 0;
}

pt_mcache_t::~pt_mcache_t()
{
    for (auto &sec : cached_sections_) {
        sec.section->unmap(sec.section);
        pt_section_put(sec.section);
    }
}

bool
pt_mcache_t::cache_section(struct pt_section *section, std::istream *f, uint64_t offset,
                           uint64_t size)
{
    if (section == nullptr || f == nullptr || size == 0) {
        ERRMSG(ERRMSG_HEADER "Invalid arguments to cache_section");
        return false;
    }

    pt_mcached_sec_mapping_t *mcache =
        static_cast<pt_mcached_sec_mapping_t *>(malloc(sizeof(pt_mcached_sec_mapping_t)));
    memset(mcache, 0, sizeof(*mcache));
    mcache->base = static_cast<uint8_t *>(malloc((size_t)size));
    mcache->size = size;
    mcache->begin = mcache->base;
    mcache->end = mcache->base + size;

    f->seekg(offset, std::ios::beg);
    f->read(reinterpret_cast<char *>(mcache->base), size);
    if (f == nullptr || f->gcount() != size) {
        ERRMSG(ERRMSG_HEADER "Failed to read memory cached section.\n");
        free(mcache->base);
        free(mcache);
        return false;
    }

    section->mapping = mcache;
    section->unmap = pt_mcached_sec_unmap;
    section->read = pt_mcached_sec_read;
    section->memsize = pt_mcached_sec_memsize;
    section->mcount = 1;

    return true;
}

bool
pt_mcache_t::load_section(std::istream *f, uint64_t offset, uint64_t size, uint64_t vaddr)
{
    if (f == nullptr) {
        ERRMSG(ERRMSG_HEADER
               "Failed to load memory cached section: invalid arguments.\n");
        return false;
    }

    struct pt_asid asid;
    if (pt_asid_from_user(&asid, nullptr) < 0) {
        ERRMSG(ERRMSG_HEADER
               "Failed to load memory cached section: failed to get asid.\n");
        return false;
    }

    struct pt_section *section =
        static_cast<struct pt_section *>(malloc(sizeof(struct pt_section)));
    memset(section, 0, sizeof(*section));

    const char *filename = "mem_cached_section";
    size_t fname_len = strlen(filename);
    section->filename = static_cast<char *>(malloc(fname_len + 1));
    memcpy(section->filename, filename, fname_len);

    /* XXX: We don't use the status to hold the image file's stat. Instead, we let the
     * status field of pt_section_t point to an uint8_t instance to avoid invoking
     * free(nullptr) when freeing a pt_section_t's instance.
     */
    section->status = malloc(sizeof(uint8_t));
    section->offset = offset;
    section->size = size;
    section->ucount = 1;

    if (!cache_section(section, f, offset, size)) {
        ERRMSG(ERRMSG_HEADER "Failed to cache the new section.\n");
        pt_section_put(section);
        return false;
    }
    cached_sections_.push_back({ section, vaddr, asid });

    return true;
}

template <typename Elf_Ehdr, typename Elf_Half, typename Elf_Phdr>
bool
pt_mcache_t::load_elf(std::istream *f, uint64_t base)
{
    f->seekg(0, std::ios::beg);

    /* Read the ELF header. */
    Elf_Ehdr ehdr;
    f->read(reinterpret_cast<char *>(&ehdr), sizeof(ehdr));
    if (f == nullptr || f->gcount() != sizeof(ehdr)) {
        ERRMSG(ERRMSG_HEADER "Failed to load ELF header: invalid ELF file.\n");
        return false;
    }

    f->seekg(ehdr.e_phoff, std::ios::beg);

    /* Determine the load offset. */
    uint64_t offset = 0;
    if (base != 0) {
        uint64_t minaddr = UINT64_MAX;
        for (Elf_Half pidx = 0; pidx < ehdr.e_phnum; ++pidx) {
            Elf_Phdr phdr;
            f->read(reinterpret_cast<char *>(&phdr), sizeof(phdr));
            if (f == nullptr || f->gcount() != sizeof(phdr)) {
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
        f->seekg(ehdr.e_phoff + pidx * sizeof(phdr), std::ios::beg);
        f->read(reinterpret_cast<char *>(&phdr), sizeof(phdr));
        if (f == nullptr || f->gcount() != sizeof(phdr)) {
            ERRMSG(ERRMSG_HEADER
                   "Failed to load ELF program header: invalid ELF file.\n");
            return false;
        }

        if (phdr.p_type != PT_LOAD || !phdr.p_filesz)
            continue;

        if (!load_section(f, phdr.p_offset, phdr.p_filesz, phdr.p_vaddr + offset)) {
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
pt_mcache_t::load(std::istream *f, uint64_t base)
{
    if (f == nullptr) {
        ERRMSG(ERRMSG_HEADER
               "Failed to load memory cached section: invalid arguments.\n");
        return false;
    }

    char e_ident[EI_NIDENT];
    f->read(e_ident, EI_NIDENT);
    if (f == nullptr || f->gcount() != EI_NIDENT) {
        ERRMSG(ERRMSG_HEADER "Failed to load memory cached section: invalid ELF file.\n");
        return false;
    }

    switch (e_ident[EI_CLASS]) {
    default:
        ERRMSG(ERRMSG_HEADER
               "Failed to load memory cached section: unsupported ELF file.\n");
        return false;
    case ELFCLASS32: return load_elf<Elf32_Ehdr, Elf32_Half, Elf32_Phdr>(f, base);
    case ELFCLASS64: return load_elf<Elf64_Ehdr, Elf64_Half, Elf64_Phdr>(f, base);
    }
}
