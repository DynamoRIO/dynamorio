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

#ifndef MODULE_ELF_H
#define MODULE_ELF_H

#include <elf.h> /* for ELF types */
#include "../module_shared.h"

#ifndef DT_RELRSZ
#    define DT_RELRSZ 35
#    define DT_RELR 36
#endif

/* XXX i#1345: support mixed-mode 32-bit and 64-bit in one process.
 * There is no official support for that on Linux or Mac and for now we do
 * not support it either, especially not mixing libraries.
 * Update: We want this for i#1684 for multi-arch support in drdecode.
 */
#ifdef X64
#    define ELF_HEADER_TYPE Elf64_Ehdr
#    define ELF_ALTARCH_HEADER_TYPE Elf32_Ehdr
#    define ELF_PROGRAM_HEADER_TYPE Elf64_Phdr
#    define ELF_SECTION_HEADER_TYPE Elf64_Shdr
#    define ELF_DYNAMIC_ENTRY_TYPE Elf64_Dyn
#    define ELF_ADDR Elf64_Addr
#    define ELF_WORD Elf64_Xword
#    define ELF_SWORD Elf64_Sxword
#    define ELF_HALF Elf64_Half
#    define ELF_SYM_TYPE Elf64_Sym
#    define ELF_WORD_SIZE 64 /* __ELF_NATIVE_CLASS */
#    define ELF_ST_VISIBILITY ELF64_ST_VISIBILITY
#    define ELF_REL_TYPE Elf64_Rel
#    define ELF_RELA_TYPE Elf64_Rela
#    define ELF_AUXV_TYPE Elf64_auxv_t
/* system like android has ELF_ST_TYPE and ELF_ST_BIND */
#    ifndef ELF_ST_TYPE
#        define ELF_ST_TYPE ELF64_ST_TYPE
#        define ELF_ST_BIND ELF64_ST_BIND
#    endif
#else
#    define ELF_HEADER_TYPE Elf32_Ehdr
#    define ELF_ALTARCH_HEADER_TYPE Elf64_Ehdr
#    define ELF_PROGRAM_HEADER_TYPE Elf32_Phdr
#    define ELF_SECTION_HEADER_TYPE Elf32_Shdr
#    define ELF_DYNAMIC_ENTRY_TYPE Elf32_Dyn
#    define ELF_ADDR Elf32_Addr
#    define ELF_WORD Elf32_Word
#    define ELF_SWORD Elf32_Sword
#    define ELF_HALF Elf32_Half
#    define ELF_SYM_TYPE Elf32_Sym
#    define ELF_WORD_SIZE 32 /* __ELF_NATIVE_CLASS */
#    define ELF_ST_VISIBILITY ELF32_ST_VISIBILITY
#    define ELF_REL_TYPE Elf32_Rel
#    define ELF_RELA_TYPE Elf32_Rela
#    define ELF_AUXV_TYPE Elf32_auxv_t
/* system like android has ELF_ST_TYPE and ELF_ST_BIND */
#    ifndef ELF_ST_TYPE
#        define ELF_ST_TYPE ELF32_ST_TYPE
#        define ELF_ST_BIND ELF32_ST_BIND
#    endif
#endif

#ifdef X86
#    ifdef X64
/* AMD x86-64 relocations.  */
#        define ELF_R_TYPE ELF64_R_TYPE
#        define ELF_R_SYM ELF64_R_SYM
#        define ELF_R_INFO ELF64_R_INFO
/* relocation type */
#        define ELF_R_NONE R_X86_64_NONE           /* No reloc */
#        define ELF_R_DIRECT R_X86_64_64           /* Direct 64 bit */
#        define ELF_R_PC32 R_X86_64_PC32           /* PC relative 32-bit signed */
#        define ELF_R_COPY R_X86_64_COPY           /* copy symbol at runtime */
#        define ELF_R_GLOB_DAT R_X86_64_GLOB_DAT   /* GOT entry */
#        define ELF_R_JUMP_SLOT R_X86_64_JUMP_SLOT /* PLT entry */
#        define ELF_R_RELATIVE R_X86_64_RELATIVE   /* Adjust by program delta */
#        ifndef R_X86_64_IRELATIVE
#            define R_X86_64_IRELATIVE 37
#        endif
#        define ELF_R_IRELATIVE                                                         \
            R_X86_64_IRELATIVE                     /* Adjust indirectly by program base \
                                                    */
                                                   /* TLS hanlding */
#        define ELF_R_TLS_DTPMOD R_X86_64_DTPMOD64 /* Module ID */
#        define ELF_R_TLS_TPOFF R_X86_64_TPOFF64   /* Offset in module's TLS block */
#        define ELF_R_TLS_DTPOFF R_X86_64_DTPOFF64 /* Offset in initial TLS block */
#        ifndef R_X86_64_TLSDESC
#            define R_X86_64_TLSDESC 36
#        endif
#        define ELF_R_TLS_DESC                              \
            R_X86_64_TLSDESC /* TLS descriptor containing   \
                              * pointer to code and to      \
                              * argument, returning the TLS \
                              * offset for the symbol.      \
                              */
#    else                    /* 32-bit */
#        define ELF_R_TYPE ELF32_R_TYPE
#        define ELF_R_SYM ELF32_R_SYM
#        define ELF_R_INFO ELF32_R_INFO
/* relocation type */
#        define ELF_R_NONE R_386_NONE          /* No reloc */
#        define ELF_R_DIRECT R_386_32          /* Direct 32 bit */
#        define ELF_R_PC32 R_386_PC32          /* PC relative 32 bit */
#        define ELF_R_COPY R_386_COPY          /* Copy symbol at runtime */
#        define ELF_R_GLOB_DAT R_386_GLOB_DAT  /* GOT entry */
#        define ELF_R_JUMP_SLOT R_386_JMP_SLOT /* PLT entry */
#        define ELF_R_RELATIVE R_386_RELATIVE  /* Adjust by program delta */
#        ifndef R_386_IRELATIVE
#            define R_386_IRELATIVE 42
#        endif
#        define ELF_R_IRELATIVE R_386_IRELATIVE /* Adjust indirectly by program base */
/* tls related */
#        define ELF_R_TLS_DTPMOD R_386_TLS_DTPMOD32 /* Module ID */
#        define ELF_R_TLS_TPOFF                                    \
            R_386_TLS_TPOFF /* Negated offsets in static TLS block \
                             */
#        define ELF_R_TLS_DTPOFF R_386_TLS_DTPOFF32 /* Offset in TLS block */
#        ifndef R_386_TLS_DESC
#            define R_386_TLS_DESC 41
#        endif
#        define ELF_R_TLS_DESC                            \
            R_386_TLS_DESC /* TLS descriptor containing   \
                            * pointer to code and to      \
                            * argument, returning the TLS \
                            * offset for the symbol.      \
                            */
#    endif
#elif defined(AARCH64)
#    define ELF_R_TYPE ELF64_R_TYPE
#    define ELF_R_SYM ELF64_R_SYM
/* relocation type */
#    define ELF_R_NONE R_AARCH64_NONE           /* No relocation. */
#    define ELF_R_DIRECT R_AARCH64_ABS64        /* Direct 64 bit. */
#    define ELF_R_COPY R_AARCH64_COPY           /* Copy symbol at runtime. */
#    define ELF_R_GLOB_DAT R_AARCH64_GLOB_DAT   /* Create GOT entry. */
#    define ELF_R_JUMP_SLOT R_AARCH64_JUMP_SLOT /* Create PLT entry. */
#    define ELF_R_RELATIVE R_AARCH64_RELATIVE   /* Adjust by program base. */
#    define ELF_R_IRELATIVE R_AARCH64_IRELATIVE /* STT_GNU_IFUNC relocation. */
/* tls related */
#    define ELF_R_TLS_DTPMOD 1028 /* R_AARCH64_TLS_DTPMOD64 Module number. */
#    define ELF_R_TLS_TPOFF 1030  /* R_AARCH64_TLS_TPREL64  TP-relative offset. */
#    define ELF_R_TLS_DTPOFF 1029 /* R_AARCH64_TLS_DTPREL64 Module-relative offset. */
#    define ELF_R_TLS_DESC 1031   /* R_AARCH64_TLSDESC      TLS Descriptor. */
#elif defined(ARM)
#    define ELF_R_TYPE ELF32_R_TYPE
#    define ELF_R_SYM ELF32_R_SYM
#    define ELF_R_INFO ELF32_R_INFO
/* relocation type */
#    define ELF_R_NONE R_ARM_NONE               /* No reloc */
#    define ELF_R_DIRECT R_ARM_ABS32            /* Direct 32 bit */
#    define ELF_R_COPY R_ARM_COPY               /* Copy symbol at runtime */
#    define ELF_R_GLOB_DAT R_ARM_GLOB_DAT       /* GOT entry */
#    define ELF_R_JUMP_SLOT R_ARM_JUMP_SLOT     /* PLT entry */
#    define ELF_R_RELATIVE R_ARM_RELATIVE       /* Adjust by program delta */
#    define ELF_R_IRELATIVE R_ARM_IRELATIVE     /* Adjust indirectly by program base */
/* tls related */
#    define ELF_R_TLS_DTPMOD R_ARM_TLS_DTPMOD32 /* Module ID */
#    define ELF_R_TLS_TPOFF R_ARM_TLS_TPOFF32   /* Negated offsets in static TLS block */
#    define ELF_R_TLS_DTPOFF R_ARM_TLS_DTPOFF32 /* Offset in TLS block */
#    ifndef ANDROID
#        define ELF_R_TLS_DESC                            \
            R_ARM_TLS_DESC /* TLS descriptor containing   \
                            * pointer to code and to      \
                            * argument, returning the TLS \
                            * offset for the symbol.      \
                            */
#    endif                 /* ANDROID */
#elif defined(RISCV64)
#    define ELF_R_TYPE ELF64_R_TYPE
#    define ELF_R_SYM ELF64_R_SYM
/* relocation type */
#    define ELF_R_NONE R_RISCV_NONE               /* No relocation. */
#    define ELF_R_DIRECT R_RISCV_64               /* Direct 64 bit. */
#    define ELF_R_COPY R_RISCV_COPY               /* Copy symbol at runtime. */
/* FIXME i#3544: GOT and direct 64 bit both use R_RISCV_64. */
#    define ELF_R_GLOB_DAT R_RISCV_64             /* Create GOT entry. */
#    define ELF_R_JUMP_SLOT R_RISCV_JUMP_SLOT     /* Create PLT entry. */
#    define ELF_R_RELATIVE R_RISCV_RELATIVE       /* Adjust by program base. */
/* FIXME i#3544: R_RISCV_IRELATIVE was added after libc 2.31 and some distros
 * don't have it yet (i.e. Ubuntu 20.04). The official number has been defined
 * here: https://github.com/riscv/riscv-elf-psabi-doc/commit/d21ca40a.
 */
#    ifndef R_RISCV_IRELATIVE
#        define R_RISCV_IRELATIVE 58
#    endif
#    define ELF_R_IRELATIVE R_RISCV_IRELATIVE     /* STT_GNU_IFUNC relocation. */
/* tls related */
#    define ELF_R_TLS_DTPMOD R_RISCV_TLS_DTPMOD64 /* Module ID. */
#    define ELF_R_TLS_TPOFF R_RISCV_TLS_TPREL64   /* TP-relative offset. */
#    define ELF_R_TLS_DTPOFF R_RISCV_TLS_DTPREL64 /* Module-relative offset. */
#endif                                            /* X86/ARM/RISCV64 */

/* Define ARM ELF machine types to support compiling on old Linux distros. */
#ifndef EM_ARM
#    define EM_ARM 40
#endif
#ifndef EM_AARCH64
#    define EM_AARCH64 183
#endif

bool
get_elf_platform(file_t f, dr_platform_t *platform);

bool
is_elf_so_header(app_pc base, size_t size);

bool
is_elf_partial_map(app_pc base, size_t size, uint memprot);

app_pc
module_vaddr_from_prog_header(app_pc prog_header, uint num_segments,
                              OUT app_pc *first_end, /* first segment's end */
                              OUT app_pc *max_end);

bool
module_read_program_header(app_pc base, uint segment_num, OUT app_pc *segment_base,
                           OUT app_pc *segment_end, OUT uint *segment_prot,
                           OUT size_t *segment_align);

ELF_ADDR
module_get_section_with_name(app_pc image, size_t img_size, const char *sec_name);

void
module_relocate_rel(app_pc modbase, os_privmod_data_t *pd, ELF_REL_TYPE *start,
                    ELF_REL_TYPE *end);

void
module_relocate_rela(app_pc modbase, os_privmod_data_t *pd, ELF_RELA_TYPE *start,
                     ELF_RELA_TYPE *end);

void
module_relocate_relr(app_pc modbase, os_privmod_data_t *pd, const ELF_WORD *relr,
                     size_t size);

bool
module_get_relro(app_pc base, OUT app_pc *relro_base, OUT size_t *relro_size);

bool
module_read_os_data(app_pc base, bool dyn_reloc, OUT ptr_int_t *delta,
                    OUT os_module_data_t *os_data, OUT char **soname);

uint
module_segment_prot_to_osprot(ELF_PROGRAM_HEADER_TYPE *prog_hdr);

/* Data structure for loading an ELF.
 */
typedef struct elf_loader_t {
    const char *filename;
    file_t fd;
    ELF_HEADER_TYPE *ehdr;          /* Points into buf. */
    ELF_PROGRAM_HEADER_TYPE *phdrs; /* Points into buf or file_map. */
    app_pc load_base;               /* Load base. */
    ptr_int_t load_delta;           /* Delta from preferred base. */
    size_t image_size;              /* Size of the mapped image. */
    void *file_map;                 /* Whole file map, if needed. */
    size_t file_size;               /* Size of the file map. */

    /* Static buffer sized to hold most headers in a single read.  A typical ELF
     * file has an ELF header followed by program headers.  On my workstation,
     * most ELFs in /usr/lib have 7 phdrs, and the maximum is 9.  We choose 12
     * as a good upper bound and to allow for padding.  If the headers don't
     * fit, we fall back to file mapping.
     */
    byte buf[sizeof(ELF_HEADER_TYPE) + sizeof(ELF_PROGRAM_HEADER_TYPE) * 12];
} elf_loader_t;

typedef byte *(*map_fn_t)(file_t f, size_t *size INOUT, uint64 offs, app_pc addr,
                          uint prot /*MEMPROT_*/, map_flags_t map_flags);
typedef bool (*unmap_fn_t)(byte *map, size_t size);
typedef bool (*prot_fn_t)(byte *map, size_t size, uint prot /*MEMPROT_*/);
typedef void (*check_bounds_fn_t)(elf_loader_t *elf, byte *start, byte *end);
typedef void *(*memset_fn_t)(void *dst, int val, size_t size);

/* Initialized an ELF loader for use with the given file. */
bool
elf_loader_init(elf_loader_t *elf, const char *filename);

/* Frees resources needed to load the ELF, not the mapped image itself. */
void
elf_loader_destroy(elf_loader_t *elf);

/* Reads the main ELF header. */
ELF_HEADER_TYPE *
elf_loader_read_ehdr(elf_loader_t *elf);

/* Reads the ELF program headers, via read() or mmap() syscalls. */
ELF_PROGRAM_HEADER_TYPE *
elf_loader_read_phdrs(elf_loader_t *elf);

/* Shorthand to initialize the loader and read the ELF and program headers. */
bool
elf_loader_read_headers(elf_loader_t *elf, const char *filename);

/* Maps in the entire ELF file, including unmapped portions such as section
 * headers and debug info.  Does not re-map the same file if called twice.
 */
app_pc
elf_loader_map_file(elf_loader_t *elf, bool reachable);

/* Maps in the PT_LOAD segments of an ELF file, returning the base.  Must be
 * called after reading program headers with elf_loader_read_phdrs() or the
 * elf_loader_read_headers() shortcut.  All image mappings are done via the
 * provided function pointers.
 *
 * check_bounds_func is only called if fixed=true.
 *
 * XXX: fixed is only a hint as PIEs with a base of 0 should not use MAP_FIXED,
 * should we remove it?
 */
app_pc
elf_loader_map_phdrs(elf_loader_t *elf, bool fixed, map_fn_t map_func,
                     unmap_fn_t unmap_func, prot_fn_t prot_func,
                     check_bounds_fn_t check_bounds_func, memset_fn_t memset_func,
                     modload_flags_t flags);

/* Iterate program headers of a mapped ELF image and find the string that
 * PT_INTERP points to.  Typically this comes early in the file and is always
 * included in PT_LOAD segments, so we safely do this after the initial
 * mapping.
 */
const char *
elf_loader_find_pt_interp(elf_loader_t *elf);

#ifdef LINUX
bool
module_init_rseq(module_area_t *ma, bool at_map);
#endif

#endif /* MODULE_ELF_H */
