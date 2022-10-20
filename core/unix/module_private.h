/* **********************************************************
 * Copyright (c) 2011-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2008-2010 VMware, Inc.  All rights reserved.
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
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
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

#ifndef MODULE_PRIVATE_H
#define MODULE_PRIVATE_H

#include "configure.h"

struct _os_privmod_data_t;
typedef struct _os_privmod_data_t os_privmod_data_t;

#ifdef LINUX
#    include "module_elf.h"
#endif

typedef void (*fp_t)(int argc, char **argv, char **env);
/* data structure for loading and relocating private client,
 * most from PT_DYNAMIC segment
 */
struct _os_privmod_data_t {
    os_module_data_t os_data;
    ptr_int_t load_delta; /* delta from preferred base */
    app_pc max_end;       /* relative pc */
    char *soname;
#ifdef LINUX
    ELF_DYNAMIC_ENTRY_TYPE *dyn;
    size_t dynsz;
    ELF_ADDR pltgot;
    size_t pltrelsz;
    ELF_WORD pltrel;
    bool textrel;
    app_pc jmprel;
    ELF_REL_TYPE *rel;
    size_t relsz;
    size_t relent;
    ELF_RELA_TYPE *rela;
    size_t relasz;
    size_t relaent;
    ELF_WORD *relr;
    size_t relrsz;
    app_pc verneed;
    int verneednum;
    int relcount;
    ELF_HALF *versym;
#else
    /* XXX i#1285: MacOS private loader NYI */
#endif
    /* initialization/finalization function */
    fp_t init;
    fp_t fini;
    fp_t *init_array; /* an array of init func ptrs */
    fp_t *fini_array; /* an array of fini func ptrs */
    size_t init_arraysz;
    size_t fini_arraysz;
    /* tls info */
    uint tls_block_size; /* tls variables size in memory */
    uint tls_align;      /* alignment for tls variables  */
    uint tls_modid;      /* module id for get tls addr lookup */
    uint tls_offset;     /* offset in the TLS segment */
    uint tls_image_size; /* tls variables size in the file */
    uint tls_first_byte; /* aligned addr of the first tls variable */
    app_pc tls_image;    /* tls block address in memory */
    /* This is used to get libunwind walking app libraries. */
    bool use_app_imports;
};

#ifdef MACOS
/* XXX i#1345: support mixed-mode 32-bit and 64-bit in one process.
 * There is no official support for that on Linux or Windows and for now we do
 * not support it either, especially not mixing libraries.
 */
#    ifdef X64
typedef struct mach_header_64 mach_header_t;
typedef struct segment_command_64 segment_command_t;
typedef struct section_64 section_t;
typedef struct nlist_64 nlist_t;
#    else
typedef struct mach_header mach_header_t;
typedef struct segment_command segment_command_t;
typedef struct section section_t;
typedef struct nlist nlist_t;
#    endif
bool
is_macho_header(app_pc base, size_t size);
#endif

void
module_get_os_privmod_data(app_pc base, size_t size, bool relocated,
                           OUT os_privmod_data_t *pd);

ptr_uint_t
module_get_text_section(app_pc file_map, size_t file_size);

app_pc
get_proc_address_from_os_data(os_module_data_t *os_data, ptr_int_t delta,
                              const char *name, bool *is_indirect_code OUT);

bool
privload_redirect_sym(os_privmod_data_t *opd, ptr_uint_t *r_addr, const char *name);

#ifdef LINUX
void
module_init_os_privmod_data_from_dyn(os_privmod_data_t *opd, ELF_DYNAMIC_ENTRY_TYPE *dyn,
                                     ptr_int_t load_delta);
#endif

void
privload_mod_thread_tls_init(void);

#endif /* MODULE_PRIVATE_H */
