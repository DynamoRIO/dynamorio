/* **********************************************************
 * Copyright (c) 2026 Meta Platforms, Inc.  All rights reserved.
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
 * * Neither the name of Meta Platforms, Inc. nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "configure.h"
#include "dr_api.h"
#include "tools.h"

#include <elf.h>
#include <fcntl.h>
#include <link.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

static void *elf_view;
static void *full_elf_view;
static size_t full_elf_view_size;
static void *shared_module;
static size_t shared_module_size;

/* i#8031: Map flat views and a complete PT_LOAD layout from the same copied ELF.
 * The flat views must be rejected while the PT_LOAD layout must remain a module.
 * Writable MAP_SHARED segments require an O_RDWR file, so use a temporary copy
 * instead of the running executable.
 */

static uintptr_t
align_down(uintptr_t value, size_t alignment)
{
    return value & ~(alignment - 1);
}

static uintptr_t
align_up(uintptr_t value, size_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

static int
copy_self(char path[])
{
    char buffer[64 * 1024];
    int source_fd = open("/proc/self/exe", O_RDONLY);
    assert(source_fd >= 0);
    int copy_fd = mkstemp(path);
    assert(copy_fd >= 0);

    ssize_t bytes_read;
    while ((bytes_read = read(source_fd, buffer, sizeof(buffer))) > 0) {
        ssize_t written = 0;
        while (written < bytes_read) {
            ssize_t result = write(copy_fd, buffer + written, bytes_read - written);
            assert(result > 0);
            written += result;
        }
    }
    assert(bytes_read == 0);
    close(source_fd);
    return copy_fd;
}

static void *
map_shared_module(int fd, size_t page_size, size_t *mapped_size)
{
    ElfW(Ehdr) ehdr;
    assert(pread(fd, &ehdr, sizeof(ehdr), 0) == (ssize_t)sizeof(ehdr));
    /* DR supports load-bias relocation for both native ELF image types. */
    assert(ehdr.e_type == ET_DYN || ehdr.e_type == ET_EXEC);
    assert(ehdr.e_phentsize == sizeof(ElfW(Phdr)));
    size_t phdr_size = ehdr.e_phnum * sizeof(ElfW(Phdr));
    ElfW(Phdr) *phdrs = malloc(phdr_size);
    assert(phdrs != NULL);
    assert(pread(fd, phdrs, phdr_size, ehdr.e_phoff) == (ssize_t)phdr_size);

    uintptr_t min_vaddr = UINTPTR_MAX;
    uintptr_t max_end = 0;
    for (uint i = 0; i < ehdr.e_phnum; ++i) {
        if (phdrs[i].p_type != PT_LOAD || phdrs[i].p_memsz == 0)
            continue;
        uintptr_t start = align_down(phdrs[i].p_vaddr, page_size);
        uintptr_t end = align_up(phdrs[i].p_vaddr + phdrs[i].p_memsz, page_size);
        if (start < min_vaddr)
            min_vaddr = start;
        if (end > max_end)
            max_end = end;
    }
    assert(min_vaddr != UINTPTR_MAX && max_end > min_vaddr);
    size_t span = max_end - min_vaddr;
    byte *base = mmap(NULL, span, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(base != MAP_FAILED);

    for (uint i = 0; i < ehdr.e_phnum; ++i) {
        if (phdrs[i].p_type != PT_LOAD || phdrs[i].p_filesz == 0)
            continue;
        uintptr_t page_vaddr = align_down(phdrs[i].p_vaddr, page_size);
        uintptr_t page_offset = align_down(phdrs[i].p_offset, page_size);
        size_t page_delta = phdrs[i].p_vaddr - page_vaddr;
        size_t file_size = align_up(page_delta + phdrs[i].p_filesz, page_size);
        int prot = 0;
        if ((phdrs[i].p_flags & PF_R) != 0)
            prot |= PROT_READ;
        if ((phdrs[i].p_flags & PF_W) != 0)
            prot |= PROT_WRITE;
        if ((phdrs[i].p_flags & PF_X) != 0)
            prot |= PROT_EXEC;
        void *address = base + (page_vaddr - min_vaddr);
        assert(mmap(address, file_size, prot, MAP_SHARED | MAP_FIXED, fd, page_offset) ==
               address);
    }
    free(phdrs);
    *mapped_size = span;
    return base;
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    (void)id;
    (void)argc;
    (void)argv;
    module_data_t *view_module = dr_lookup_module(elf_view);
    print("shared ELF view is %sa module\n", view_module == NULL ? "not " : "");
    assert(view_module == NULL);

    module_data_t *full_view_module = dr_lookup_module(full_elf_view);
    print("full shared ELF view is %sa module\n", full_view_module == NULL ? "not " : "");
    assert(full_view_module == NULL);

    module_data_t *mapped_module = dr_lookup_module(shared_module);
    print("shared ELF module is %sa module\n", mapped_module != NULL ? "" : "not ");
    assert(mapped_module != NULL);
    dr_free_module_data(mapped_module);
}

int
main(int argc, const char *argv[])
{
    (void)argc;
    (void)argv;
    char copy_path[] = "/tmp/dr-static-elf-XXXXXX";
    long page_size = sysconf(_SC_PAGESIZE);
    assert(page_size > 0);
    size_t view_size = (size_t)page_size;
    int copy_fd = copy_self(copy_path);
    elf_view = mmap(NULL, view_size, PROT_READ, MAP_SHARED, copy_fd, 0);
    assert(elf_view != MAP_FAILED);
    shared_module = map_shared_module(copy_fd, view_size, &shared_module_size);
    full_elf_view_size = shared_module_size;
    full_elf_view = mmap(NULL, full_elf_view_size, PROT_READ, MAP_SHARED, copy_fd, 0);
    assert(full_elf_view != MAP_FAILED);
    close(copy_fd);
    unlink(copy_path);

    print("pre-DR start\n");
    assert(dr_app_setup_and_start() == 0);
    assert(dr_app_running_under_dynamorio());

    print("pre-DR stop\n");
    dr_app_stop_and_cleanup();
    assert(!dr_app_running_under_dynamorio());

    munmap(elf_view, view_size);
    munmap(full_elf_view, full_elf_view_size);
    munmap(shared_module, shared_module_size);
    print("all done\n");
    return 0;
}
