/* **********************************************************
 * Copyright (c) 2010-2012 Google, Inc.  All rights reserved.
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

#include "ntdll.h"

#include "../fragment.h" /* for rct_module_table_t, is inlined so need struct definition
                       * FIXME - remove this header dependency */

#define OS_IMAGE_READ IMAGE_SCN_MEM_READ
#define OS_IMAGE_WRITE IMAGE_SCN_MEM_WRITE
#define OS_IMAGE_EXECUTE IMAGE_SCN_MEM_EXECUTE

#ifndef IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE
#    define IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE 0x0040
#endif

typedef struct _os_module_data_t {
    app_pc preferred_base;       /* module preferred base from the PE headers */
    uint checksum;               /* module checksum from the PE headers */
    uint timestamp;              /* module timestamp from the PE headers */
    size_t module_internal_size; /* module internal size (from PE headers SizeOfImage) */

    size_t code_size; /* sum of the size of all code sections */

    /* from the .rsrc section */
    version_number_t file_version;
    version_number_t product_version;
    char *company_name;
    char *product_name;

    /* ALSR sharing - we'll to keep a reference to the original
     * application section to maintain no-clobber transparency
     * disallowing modifications to the original file.
     *
     * Note: on detach we do NOT release any such handles: detached
     * processes will have to be killed for us to release the file
     * handles, non-transparent only with respect to DLLs that get
     * unloaded after detach while native.
     */
    HANDLE noclobber_section_handle;

    /* FIXME: the loader also maintains Name and Path that may be
     * different than PE name */

    /* FIXME: this structure should replace the now deprecated
     * module_info_t since all of its once advanced features have been
     * incorporated in vmarea vectors
     */

#if defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH)
    /* case 9672: we split our RCT and RAC targets into per-module tables.
     * FIXME: once we have a module list on linux, move these to module_data.
     */
    rct_module_table_t rct_table[RCT_NUM_TYPES];
#endif

    /* Case 8640: store original code on IAT page.  We store bounds here,
     * even though computable, to avoid a modified PE header causing
     * us to read beyond the bounds.
     */
    byte *iat_code;
    size_t iat_len;
} os_module_data_t;

#endif /* MODULE_H */
