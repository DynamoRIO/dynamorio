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
 * windows WOW?  What if anything should we do to accomodate that? */
#ifdef X64
# define ELF_HEADER_TYPE Elf64_Ehdr
# define ELF_PROGRAM_HEADER_TYPE Elf64_Phdr
# define ELF_DYNAMIC_ENTRY_TYPE Elf64_Dyn
#else
# define ELF_HEADER_TYPE Elf32_Ehdr
# define ELF_PROGRAM_HEADER_TYPE Elf32_Phdr
# define ELF_DYNAMIC_ENTRY_TYPE Elf32_Dyn
#endif

/* FIXME NYI */
#define OS_IMAGE_READ    (ASSERT_NOT_IMPLEMENTED(false), 0)
#define OS_IMAGE_WRITE   (ASSERT_NOT_IMPLEMENTED(false), 0)
#define OS_IMAGE_EXECUTE (ASSERT_NOT_IMPLEMENTED(false), 0)

typedef struct _os_module_data_t {
    /* To compute the base address, one determines the memory address associated with
     * the lowest p_vaddr value for a PT_LOAD segment. One then obtains the base
     * address by truncating the memory load address to the nearest multiple of the
     * maximum page size and subtracting the truncated lowest p_vaddr value. 
     * Thus, this is better thought of as the "base_offset", the relocation
     * offset.
     */
    app_pc base_address; 
    size_t alignment; /* the alignment between segments */
} os_module_data_t;

#endif /* MODULE_H */
