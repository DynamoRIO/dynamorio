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

/**
 * @file elf_loader.h
 * @brief ELF file loader. Add the ELF section to the pt_image instance or the
 * pt_image_section_cache instance.
 */

#ifndef _ELF_LOADER_H_
#define _ELF_LOADER_H_ 1

#include <stdint.h>
#include <sys/stat.h>
#include <fstream>
#include <vector>

// libipt global types.
struct pt_image;
struct pt_image_section_cache;

namespace dynamorio {
namespace drmemtrace {

#ifndef IN
#    define IN // nothing
#endif
#ifndef OUT
#    define OUT // nothing
#endif
#ifndef INOUT
#    define INOUT // nothing
#endif

class elf_loader_t {
public:
    elf_loader_t()
    {
    }
    ~elf_loader_t()
    {
    }

    /**
     * Load the ELF file into the pt_image instance or the pt_image_section_cache
     * instance.
     * @param name The name of the ELF file.
     * @param base The base address of the ELF file.
     * @param iscache The pt_image_section_cache instance.
     * @param image The pt_image instance.
     * @return true if the ELF file is loaded successfully.
     */
    static bool
    load(IN const char *name, IN uint64_t base,
         INOUT struct pt_image_section_cache *iscache, INOUT struct pt_image *image);

private:
    /* The template function for loading the ELF file into either the pt_image instance or
     * the pt_image_section_cache.
     * This function supports loading ELF files in both 32-bit and 64-bit formats,
     * utilizing the defined Elf_Ehdr, Elf_Half, and Elf_Phdr types.
     */
    template <typename Elf_Ehdr, typename Elf_Half, typename Elf_Phdr>
    static bool
    load_elf(IN std::ifstream &f, IN const char *name, IN uint64_t base,
             INOUT struct pt_image_section_cache *iscache, INOUT struct pt_image *image);

    /* Load a single section into either the pt_image instance or the
     * pt_image_section_cache instance.
     */
    static bool
    load_section(IN const char *name, IN uint64_t offset, IN uint64_t size,
                 IN uint64_t vaddr, INOUT struct pt_image_section_cache *iscache,
                 INOUT struct pt_image *image);
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _ELF_LOADER_H_ */
