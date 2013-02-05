/* **********************************************************
 * Copyright (c) 2011-2012 Google, Inc.  All rights reserved.
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

#ifndef MODULE_H
#define MODULE_H

#include <elf.h> /* for ELF types */

/* FIXME - can we have 32bit and 64bit elf files in the same process like we see in
 * windows WOW?  What if anything should we do to accommodate that? */
#ifdef X64
# define ELF_HEADER_TYPE Elf64_Ehdr
# define ELF_PROGRAM_HEADER_TYPE Elf64_Phdr
# define ELF_SECTION_HEADER_TYPE Elf64_Shdr
# define ELF_DYNAMIC_ENTRY_TYPE Elf64_Dyn
# define ELF_ADDR Elf64_Addr
# define ELF_WORD Elf64_Xword
# define ELF_HALF Elf64_Half
# define ELF_SYM_TYPE Elf64_Sym
# define ELF_ST_TYPE ELF64_ST_TYPE
# define ELF_WORD_SIZE 64 /* __ELF_NATIVE_CLASS */
# define ELF_ST_BIND ELF64_ST_BIND
# define ELF_ST_VISIBILITY ELF64_ST_VISIBILITY
# define ELF_REL_TYPE Elf64_Rel
# define ELF_RELA_TYPE Elf64_Rela
# define ELF_AUXV_TYPE Elf64_auxv_t
#else
# define ELF_HEADER_TYPE Elf32_Ehdr
# define ELF_PROGRAM_HEADER_TYPE Elf32_Phdr
# define ELF_SECTION_HEADER_TYPE Elf32_Shdr
# define ELF_DYNAMIC_ENTRY_TYPE Elf32_Dyn
# define ELF_ADDR Elf32_Addr
# define ELF_WORD Elf32_Word
# define ELF_HALF Elf32_Half
# define ELF_SYM_TYPE Elf32_Sym
# define ELF_ST_TYPE ELF32_ST_TYPE
# define ELF_WORD_SIZE 32 /* __ELF_NATIVE_CLASS */
# define ELF_ST_BIND ELF32_ST_BIND
# define ELF_ST_VISIBILITY ELF32_ST_VISIBILITY
# define ELF_REL_TYPE Elf32_Rel
# define ELF_RELA_TYPE Elf32_Rela
# define ELF_AUXV_TYPE Elf32_auxv_t
#endif

#ifdef X64 
/* AMD x86-64 relocations.  */
# define ELF_R_TYPE   ELF64_R_TYPE
# define ELF_R_SYM    ELF64_R_SYM
# define ELF_R_INFO   ELF64_R_INFO
/* relocation type */
# define ELF_R_NONE      R_X86_64_NONE        /* No reloc */
# define ELF_R_DIRECT    R_X86_64_64          /* Direct 64 bit */
# define ELF_R_PC32      R_X86_64_PC32        /* PC relative 32-bit signed */
# define ELF_R_COPY      R_X86_64_COPY        /* copy symbol at runtime */
# define ELF_R_GLOB_DAT  R_X86_64_GLOB_DAT    /* GOT entry */
# define ELF_R_JUMP_SLOT R_X86_64_JUMP_SLOT   /* PLT entry */
# define ELF_R_RELATIVE  R_X86_64_RELATIVE    /* Adjust by program delta */
# ifndef R_X86_64_IRELATIVE
#  define R_X86_64_IRELATIVE 37
# endif
# define ELF_R_IRELATIVE R_X86_64_IRELATIVE   /* Adjust indirectly by program base */
/* TLS hanlding */
# define ELF_R_TLS_DTPMOD   R_X86_64_DTPMOD64 /* Module ID */
# define ELF_R_TLS_TPOFF    R_X86_64_TPOFF64  /* Offset in module's TLS block */
# define ELF_R_TLS_DTPOFF   R_X86_64_DTPOFF64 /* Offset in initial TLS block */
# ifndef R_X86_64_TLSDESC
#  define R_X86_64_TLSDESC   36
# endif
# define ELF_R_TLS_DESC     R_X86_64_TLSDESC  /* TLS descriptor containing
                                               * pointer to code and to
                                               * argument, returning the TLS
                                               * offset for the symbol. 
                                               */
#else /* 32-bit */
# define ELF_R_TYPE   ELF32_R_TYPE
# define ELF_R_SYM    ELF32_R_SYM
# define ELF_R_INFO   ELF32_R_INFO
/* relocation type */
# define ELF_R_NONE      R_386_NONE      /* No reloc */
# define ELF_R_DIRECT    R_386_32        /* Direct 32 bit */
# define ELF_R_PC32      R_386_PC32      /* PC relative 32 bit */ 
# define ELF_R_COPY      R_386_COPY      /* Copy symbol at runtime */
# define ELF_R_GLOB_DAT  R_386_GLOB_DAT  /* GOT entry */
# define ELF_R_JUMP_SLOT R_386_JMP_SLOT  /* PLT entry */
# define ELF_R_RELATIVE  R_386_RELATIVE  /* Adjust by program delta */
# ifndef R_386_IRELATIVE
#  define R_386_IRELATIVE 42
# endif
# define ELF_R_IRELATIVE R_386_IRELATIVE /* Adjust indirectly by program base */
/* tls related */
# define ELF_R_TLS_DTPMOD  R_386_TLS_DTPMOD32 /* Module ID */
# define ELF_R_TLS_TPOFF   R_386_TLS_TPOFF    /* Negated offsets in static TLS block */
# define ELF_R_TLS_DTPOFF  R_386_TLS_DTPOFF32 /* Offset in TLS block */
# ifndef R_386_TLS_DESC
#  define R_386_TLS_DESC   41
# endif
# define ELF_R_TLS_DESC    R_386_TLS_DESC     /* TLS descriptor containing
                                               * pointer to code and to
                                               * argument, returning the TLS
                                               * offset for the symbol.
                                               */
#endif

/* used only in our own routines here which use PF_* converted to MEMPROT_* */
#define OS_IMAGE_READ    (MEMPROT_READ)
#define OS_IMAGE_WRITE   (MEMPROT_WRITE)
#define OS_IMAGE_EXECUTE (MEMPROT_EXEC)

/* i#160/PR 562667: support non-contiguous library mappings.  While we're at
 * it we go ahead and store info on each segment whether contiguous or not.
 */
typedef struct _module_segment_t {
    /* start and end are page-aligned beyond the section alignment */
    app_pc start;
    app_pc end;
    uint prot;
} module_segment_t;

typedef struct _os_module_data_t {
    /* To compute the base address, one determines the memory address associated with
     * the lowest p_vaddr value for a PT_LOAD segment. One then obtains the base
     * address by truncating the memory load address to the nearest multiple of the
     * maximum page size and subtracting the truncated lowest p_vaddr value. 
     * Thus, this is not the load address but the base address used in
     * address references within the file.
     */
    app_pc base_address; 
    size_t alignment; /* the alignment between segments */

    /* Fields for pcaches (PR 295534) */
    size_t checksum;
    size_t timestamp;

    /* i#112: Dynamic section info for exported symbol lookup.  Not
     * using elf types here to avoid having to export those.
     */
    bool   hash_is_gnu;   /* gnu hash function? */
    app_pc hashtab;       /* absolute addr of .hash or .gnu.hash */
    size_t num_buckets;   /* number of bucket entries */
    app_pc buckets;       /* absolute addr of hash bucket table */
    size_t num_chain;     /* number of chain entries */
    app_pc chain;         /* absolute addr of hash chain table */
    app_pc dynsym;        /* absolute addr of .dynsym */
    app_pc dynstr;        /* absolute addr of .dynstr */
    size_t dynstr_size;   /* size of .dynstr */
    size_t symentry_size; /* size of a .dynsym entry */
    /* for .gnu.hash */
    app_pc gnu_bitmask;
    ptr_uint_t gnu_shift;
    ptr_uint_t gnu_bitidx;
    size_t gnu_symbias;   /* .dynsym index of first export */

    /* i#160/PR 562667: support non-contiguous library mappings */
    bool contiguous;
    uint num_segments;   /* number of valid entries in segments array */
    uint alloc_segments; /* capacity of segments array */
    module_segment_t *segments;
} os_module_data_t;

typedef void (*fp_t)(int argc, char **argv, char **env);
/* data structure for loading and relocating private client,
 * most from PT_DYNAMIC segment 
 */
typedef struct _os_privmod_data_t {
    os_module_data_t os_data;
    ELF_DYNAMIC_ENTRY_TYPE *dyn;
    ptr_int_t      load_delta;  /* delta from preferred base */
    char          *soname;
    ELF_ADDR       pltgot;
    size_t         pltrelsz;
    ELF_WORD       pltrel;
    bool           textrel;
    app_pc         jmprel;
    ELF_REL_TYPE  *rel;
    size_t         relsz;
    size_t         relent;
    ELF_RELA_TYPE *rela;
    size_t         relasz;
    size_t         relaent;
    app_pc         verneed;
    int            verneednum;
    int            relcount;
    ELF_HALF      *versym;
    /* initialization/finalization function */
    fp_t           init;
    fp_t           fini;
    fp_t          *init_array;  /* an array of init func ptrs */
    fp_t          *fini_array;  /* an array of fini func ptrs */
    size_t         init_arraysz;
    size_t         fini_arraysz;
    /* tls info */
    uint           tls_block_size; /* tls variables size in memory */
    uint           tls_align;      /* alignment for tls variables  */
    uint           tls_modid;      /* module id for get tls addr lookup */
    uint           tls_offset;     /* offset in the TLS segment */
    uint           tls_image_size; /* tls variables size in the file */
    uint           tls_first_byte; /* aligned addr of the first tls variable */
    app_pc         tls_image;      /* tls block address in memory */
} os_privmod_data_t;

ELF_ADDR 
module_get_section_with_name(app_pc image, size_t img_size,
                             const char *sec_name);

void
module_relocate_rel(app_pc modbase,
                    os_privmod_data_t *pd,
                    ELF_REL_TYPE *start,
                    ELF_REL_TYPE *end);
                         
void
module_relocate_rela(app_pc modbase,
                     os_privmod_data_t *pd,
                     ELF_RELA_TYPE *start,
                     ELF_RELA_TYPE *end);

bool
module_get_relro(app_pc base, OUT app_pc *relro_base, OUT size_t *relro_size);

bool
module_read_os_data(app_pc base,
                    OUT ptr_int_t *delta,
                    OUT os_module_data_t *os_data,
                    OUT char **soname);

char *
get_shared_lib_soname(app_pc map);

/* Redirected functions for loaded module,
 * they are also used by __wrap_* functions in instrument.c
 */

void *
redirect_calloc(size_t nmemb, size_t size);

void *
redirect_malloc(size_t size);

void  
redirect_free(void *ptr);

void *
redirect_realloc(void *ptr, size_t size);

uint 
module_segment_prot_to_osprot(ELF_PROGRAM_HEADER_TYPE *prog_hdr);

void
module_get_os_privmod_data(app_pc base, size_t size, bool relocated,
                           OUT os_privmod_data_t *pd);

ELF_ADDR 
module_get_text_section(app_pc file_map, size_t file_size);

app_pc
get_proc_address_from_os_data(os_module_data_t *os_data,
                              ptr_int_t delta,
                              const char *name,
                              bool *is_indirect_code OUT);

app_pc
get_private_library_address(app_pc modbase, const char *name);

bool
get_private_library_bounds(IN app_pc modbase, OUT byte **start, OUT byte **end);


extern struct _IO_FILE  **privmod_stdout;
extern struct _IO_FILE  **privmod_stderr;
extern struct _IO_FILE  **privmod_stdin;

/* loader.c */
bool  privload_redirect_sym(ELF_ADDR *r_addr, const char *name);

/* Data structure for loading an ELF.
 */
typedef struct elf_loader_t {
    const char *filename;
    file_t fd;
    ELF_HEADER_TYPE *ehdr;              /* Points into buf. */
    ELF_PROGRAM_HEADER_TYPE *phdrs;     /* Points into buf or file_map. */
    app_pc load_base;                   /* Load base. */
    ptr_int_t load_delta;               /* Delta from preferred base. */
    size_t image_size;                  /* Size of the mapped image. */
    void *file_map;                     /* Whole file map, if needed. */
    size_t file_size;                   /* Size of the file map. */

    /* Static buffer sized to hold most headers in a single read.  A typical ELF
     * file has an ELF header followed by program headers.  On my workstation,
     * most ELFs in /usr/lib have 7 phdrs, and the maximum is 9.  We choose 12
     * as a good upper bound and to allow for padding.  If the headers don't
     * fit, we fall back to file mapping.
     */
    byte buf[sizeof(ELF_HEADER_TYPE) + sizeof(ELF_PROGRAM_HEADER_TYPE) * 12];
} elf_loader_t;

typedef byte *(*map_fn_t)(file_t f, size_t *size INOUT, uint64 offs,
                          app_pc addr, uint prot/*MEMPROT_*/, bool cow,
                          bool image, bool fixed);
typedef bool (*unmap_fn_t)(byte *map, size_t size);
typedef bool (*prot_fn_t)(byte *map, size_t size, uint prot/*MEMPROT_*/);

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
elf_loader_map_file(elf_loader_t *elf);

/* Maps in the PT_LOAD segments of an ELF file, returning the base.  Must be
 * called after reading program headers with elf_loader_read_phdrs() or the
 * elf_loader_read_headers() shortcut.  All image mappings are done via the
 * provided function pointers.
 */
app_pc
elf_loader_map_phdrs(elf_loader_t *elf, bool fixed, map_fn_t map_func,
                     unmap_fn_t unmap_func, prot_fn_t prot_func);

/* Iterate program headers of a mapped ELF image and find the string that
 * PT_INTERP points to.  Typically this comes early in the file and is always
 * included in PT_LOAD segments, so we safely do this after the initial
 * mapping.
 */
const char *
elf_loader_find_pt_interp(elf_loader_t *elf);

#endif /* MODULE_H */
