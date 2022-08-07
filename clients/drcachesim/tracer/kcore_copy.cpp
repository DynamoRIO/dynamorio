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
#include <stdio.h>

#include "../common/utils.h"
#include "dr_api.h"

#define MODULES_FILE_PATH "/proc/modules"
#define KALLSYMS_FILE_PATH "/proc/kallsyms"
#define KCORE_FILE_PATH "/proc/kcore"

#define NEW(x) ((x) = calloc(sizeof(*(x)), 1))

struct module_t {
    module_t *next;
    char *name;
    unsigned long long start, end;
    int numsym;
};

static module_t *modules = NULL;
static module_t *last_module = NULL;

static module *
create_module(Elf *elf, char *name, bool first)
{
    module_t *mod;
    mod = NEW(mod);
    mod->name = strdup(name);
    if (first) {
        mod->next = modules;
        modules = mod;
    } else {
        if (lastmod)
            lastmod->next = mod;
        if (!modules)
            modules = mod;
        lastmod = mod;
    }
    return mod;
}

static struct module *
findmod(char *name)
{
    struct module *mod;
    for (mod = modules; mod; mod = mod->next)
        if (!strcmp(name, mod->name))
            return mod;
    return NULL;
}

static void
read_modules(Elf *elf)
{
    std::ifstream f(MODULES_FILE_PATH, std::ios::in);
    if (!f.is_open()) {
        ASSERT(false, "failed to open " MODULES_FILE_PATH);
        return;
    }
    std::string line;
    while (std::getline(infile, line)) {
        char mname[100];
        unsigned long long addr;
        int len;
        dr_sscanf(line, "%99s %d %*d %*s %*s %llx", mname, &len, &addr) != 3)
        {
            ASSERT(false, "failed to parse " MODULES_FILE_PATH);
            return;
        }
        struct module *mod = create_module(elf, mname, false);
        mod->start = addr;
        mod->end = addr + len;
    }
    f.close();
    return;
}

void
copy_kernel(const char *to_dir)
{
    elf_version(EV_CURRENT);

    file_t kfd = dr_open_file(KCORE_FILE_PATH, DR_FILE_READ);
    if (kfd < 0) {
        ASSERT(false, "failed to open" KCORE_FILE_PATH);
        return;
    }

    Elf *kelf = elf_begin(kfd, ELF_C_READ, NULL);
    if (!kelf) {
        ASSERT(false, "failed to read kcore elf");
        return;
    }

    read_modules(kelf);
    read_symbols(kelf);
    read_kcore(kelf, image);

    elf_end(kelf);
    close(kfd);
}
