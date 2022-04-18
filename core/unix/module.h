/* **********************************************************
 * Copyright (c) 2011-2020 Google, Inc.  All rights reserved.
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

/* used only in our own routines here which use PF_* converted to MEMPROT_* */
#define OS_IMAGE_READ (MEMPROT_READ)
#define OS_IMAGE_WRITE (MEMPROT_WRITE)
#define OS_IMAGE_EXECUTE (MEMPROT_EXEC)

/* i#160/PR 562667: support non-contiguous library mappings.  While we're at
 * it we go ahead and store info on each segment whether contiguous or not.
 */
typedef struct _module_segment_t {
    /* start and end are page-aligned beyond the section alignment */
    app_pc start;
    app_pc end;
    uint prot;
    bool shared; /* not unique to this module */
    uint64 offset;
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
    /* XXX: All segments are expected to have the same alignment, even though that it is
     * not a requirement for ELF. To allow a different alignment for each segment we will
     * need to move this field in the module_segment_t struct.
     */
    size_t alignment; /* the alignment between segments */

    /* Fields for pcaches (PR 295534) */
    size_t checksum;
    size_t timestamp;

#ifdef LINUX
    /* i#112: Dynamic section info for exported symbol lookup.  Not
     * using elf types here to avoid having to export those.
     */
    bool have_dynamic_info; /* are the fields below filled in yet? */
    bool hash_is_gnu;       /* gnu hash function? */
    app_pc hashtab;         /* absolute addr of .hash or .gnu.hash */
    size_t num_buckets;     /* number of bucket entries */
    app_pc buckets;         /* absolute addr of hash bucket table */
    size_t num_chain;       /* number of chain entries */
    app_pc chain;           /* absolute addr of hash chain table */
    app_pc dynsym;          /* absolute addr of .dynsym */
    app_pc dynstr;          /* absolute addr of .dynstr */
    size_t dynstr_size;     /* size of .dynstr */
    size_t symentry_size;   /* size of a .dynsym entry */
    bool has_runpath;       /* is DT_RUNPATH present? */
    /* for .gnu.hash */
    app_pc gnu_bitmask;
    ptr_uint_t gnu_shift;
    ptr_uint_t gnu_bitidx;
    size_t gnu_symbias; /* .dynsym index of first export */
#else                   /* MACOS */
    byte *exports;     /* absolute addr of exports trie */
    size_t exports_sz; /* size of exports trie */
    byte *symtab;
    uint num_syms;
    byte *strtab;
    size_t strtab_sz;
    bool in_shared_cache;
    uint current_version;
    uint compatibility_version;
    byte uuid[16];
#endif

    /* i#160/PR 562667: support non-contiguous library mappings */
    bool contiguous;
    uint num_segments;   /* number of valid entries in segments array */
    uint alloc_segments; /* capacity of segments array */
    module_segment_t *segments;
} os_module_data_t;

app_pc
module_entry_point(app_pc base, ptr_int_t load_delta);

bool
module_is_header(app_pc base, size_t size /*optional*/);

bool
module_is_executable(app_pc base);

bool
module_is_partial_map(app_pc base, size_t size, uint memprot);

bool
module_walk_program_headers(app_pc base, size_t view_size, bool at_map, bool dyn_reloc,
                            app_pc *out_base,
                            app_pc *out_first_end, /* first segment's end */
                            app_pc *out_max_end, char **out_soname,
                            os_module_data_t *out_data);

uint
module_num_program_headers(app_pc base);

void
os_module_update_dynamic_info(app_pc base, size_t size, bool at_map);

bool
module_file_has_module_header(const char *filename);

bool
module_file_is_module64(file_t f, bool *is64 OUT, bool *also32 OUT);

/* A Mach-O universal binary may have many bitwidths.  The one used on execve will be
 * returned in "platform" and the 2nd one in "alt_platform".
 */
bool
module_get_platform(file_t f, dr_platform_t *platform, dr_platform_t *alt_platform);

void
module_add_segment_data(OUT os_module_data_t *out_data, uint num_segments /*hint only*/,
                        app_pc segment_start, size_t segment_size, uint segment_prot,
                        size_t alignment, bool shared, uint64 offset);

#if defined(MACOS) || defined(ANDROID)
typedef FILE stdfile_t;
#    define STDFILE_FILENO _file
#elif defined(LINUX)
typedef struct _IO_FILE stdfile_t;
#    ifdef __GLIBC__
#        define STDFILE_FILENO _fileno
#    endif
#endif
extern stdfile_t **privmod_stdout;
extern stdfile_t **privmod_stderr;
extern stdfile_t **privmod_stdin;

/* loader.c */
app_pc
get_private_library_address(app_pc modbase, const char *name);

bool
get_private_library_bounds(IN app_pc modbase, OUT byte **start, OUT byte **end);

#ifdef MACOS
/* module_macho.c */
byte *
module_dynamorio_lib_base(void);

bool
module_dyld_shared_region(app_pc *start OUT, app_pc *end OUT);

void
module_walk_dyld_list(app_pc dyld_base);
#endif

#endif /* MODULE_H */
