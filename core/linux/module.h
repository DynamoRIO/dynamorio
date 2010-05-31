/* **********************************************************
 * Copyright (c) 2008-2009 VMware, Inc.  All rights reserved.
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

/* FIXME - can we have 32bit and 64bit elf files in the same process like we see in
 * windows WOW?  What if anything should we do to accommodate that? */
#ifdef X64
# define ELF_HEADER_TYPE Elf64_Ehdr
# define ELF_PROGRAM_HEADER_TYPE Elf64_Phdr
# define ELF_DYNAMIC_ENTRY_TYPE Elf64_Dyn
# define ELF_ADDR Elf64_Addr
# define ELF_SYM_TYPE Elf64_Sym
# define ELF_ST_TYPE ELF64_ST_TYPE
# define ELF_WORD_SIZE 64 /* __ELF_NATIVE_CLASS */
#else
# define ELF_HEADER_TYPE Elf32_Ehdr
# define ELF_PROGRAM_HEADER_TYPE Elf32_Phdr
# define ELF_DYNAMIC_ENTRY_TYPE Elf32_Dyn
# define ELF_ADDR Elf32_Addr
# define ELF_SYM_TYPE Elf32_Sym
# define ELF_ST_TYPE ELF32_ST_TYPE
# define ELF_WORD_SIZE 32 /* __ELF_NATIVE_CLASS */
#endif

/* FIXME NYI */
#define OS_IMAGE_READ    (ASSERT_NOT_IMPLEMENTED(false), 0)
#define OS_IMAGE_WRITE   (ASSERT_NOT_IMPLEMENTED(false), 0)
#define OS_IMAGE_EXECUTE (ASSERT_NOT_IMPLEMENTED(false), 0)

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

    /* i#160/PR 562667: support non-contiguous library mappings */
    bool contiguous;
    uint num_segments;   /* number of valid entries in segments array */
    uint alloc_segments; /* capacity of segments array */
    module_segment_t *segments;
} os_module_data_t;

#endif /* MODULE_H */
