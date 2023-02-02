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

/* pt_mcache: . */

#ifndef _PT_MCACHE_H_
#define _PT_MCACHE_H_ 1

#include <stdint.h>
#include <sys/stat.h>
#include <fstream>
#include <vector>

#include "intel-pt.h"

extern "C" {
#include "pt_section.h"

/* Unmap a section.
 *
 * On success, clears @section's mapping, unmap, and read pointers.
 *
 * Returns zero on success, a negative error code otherwise.
 * Returns -pte_internal if @section is NULL.
 * Returns -pte_internal if @section has not been mapped.
 */
int
pt_mcached_sec_unmap(struct pt_section *section);

/* Read memory from an mmaped section.
 *
 * Reads at most @size bytes from @section at @offset into @buffer.
 *
 * Returns the number of bytes read on success, a negative error code otherwise.
 * Returns -pte_invalid if @section or @buffer are NULL.
 * Returns -pte_nomap if @offset is beyond the end of the section.
 */
int
pt_mcached_sec_read(const struct pt_section *section, uint8_t *buffer, uint16_t size,
                    uint64_t offset);

/* Compute the memory size of a section.
 *
 * On success, provides the amount of memory used for mapping @section in bytes
 * in @size.
 *
 * Returns zero on success, a negative error code otherwise.
 * Returns -pte_internal if @section or @size is NULL.
 * Returns -pte_internal if @section has not been mapped.
 */
int
pt_mcached_sec_memsize(const struct pt_section *section, uint64_t *size);
}

typedef struct _pt_mcache_sec_t {
    struct pt_section *section;
    uint64_t vaddr;
    struct pt_asid asid;
} pt_mcache_sec_t;

class pt_mcache_t {
public:
    pt_mcache_t()
    {
    }
    ~pt_mcache_t();

    bool
    load(std::istream *f, uint64_t base);

    const std::vector<pt_mcache_sec_t> &
    get_cached_sections()
    {
        return cached_sections_;
    }

private:
    template <typename T, typename U, typename M>
    bool
    load_elf(std::istream *f, uint64_t base);

    bool
    load_section(std::istream *f, uint64_t offset, uint64_t size, uint64_t vaddr);

    bool
    cache_section(struct pt_section *section, std::istream *f, uint64_t offset,
                  uint64_t size);

    std::vector<pt_mcache_sec_t> cached_sections_;
};

#endif /* _PT_MCACHE_H_ */
