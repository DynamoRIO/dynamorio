/* *******************************************************************************
 * Copyright (c) 2013 Google, Inc.  All rights reserved.
 * *******************************************************************************/

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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 * module_macho.c - Mach-O file parsing support
 *
 * FIXME i#58: NYI (see comments below as well):
 * + pretty much the whole file
 */

#include "../globals.h"
#include "../module_shared.h"
#include "os_private.h"
#include "module_private.h"
#include <mach-o/ldsyms.h> /* _mh_dylib_header */

bool
module_file_has_module_header(const char *filename)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: implement MachO support */
    return false;
}

bool
module_is_partial_map(app_pc base, size_t size, uint memprot)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: implement MachO support */
    return false;
}

bool
module_walk_program_headers(app_pc base, size_t view_size, bool at_map,
                            OUT app_pc *out_base /* relative pc */,
                            OUT app_pc *out_end /* relative pc */,
                            OUT char **out_soname,
                            OUT os_module_data_t *out_data)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: implement MachO support */
    return false;
}

uint
module_num_program_headers(app_pc base)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: implement MachO support */
    return 0;
}

bool
module_read_program_header(app_pc base,
                           uint segment_num,
                           OUT app_pc *segment_base /* relative pc */,
                           OUT app_pc *segment_end /* relative pc */,
                           OUT uint *segment_prot,
                           OUT size_t *segment_align)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: implement MachO support */
    return false;
}

app_pc
module_entry_point(app_pc base, ptr_int_t load_delta)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: implement MachO support */
    return NULL;
}

bool
module_is_header(app_pc base, size_t size /*optional*/)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: implement MachO support */
    return false;
}

app_pc
get_proc_address_from_os_data(os_module_data_t *os_data,
                              ptr_int_t load_delta,
                              const char *name,
                              OUT bool *is_indirect_code)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: implement MachO support */
    return NULL;
}

generic_func_t
get_proc_address_ex(module_base_t lib, const char *name, bool *is_indirect_code OUT)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: implement MachO support */
    return NULL;
}

generic_func_t
get_proc_address(module_base_t lib, const char *name)
{
    return get_proc_address_ex(lib, name, NULL);
}

size_t
module_get_header_size(app_pc module_base)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: implement MachO support */
    return 0;
}

bool
module_file_is_module64(file_t f)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: implement MachO support */
    return false;
}

bool
module_has_text_relocs(app_pc base, bool at_map)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: implement MachO support */
    return false;
}

bool
module_has_text_relocs_ex(app_pc base, os_privmod_data_t *pd)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: implement MachO support */
    return false;
}

bool
module_read_os_data(app_pc base, 
                    OUT ptr_int_t *load_delta,
                    OUT os_module_data_t *os_data,
                    OUT char **soname)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: implement MachO support */
    return false;
}

char *
get_shared_lib_name(app_pc map)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: implement MachO support */
    return NULL;
}

void
module_get_os_privmod_data(app_pc base, size_t size, bool relocated,
                           OUT os_privmod_data_t *pd)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: implement MachO support */
    return;
}

ptr_uint_t
module_get_text_section(app_pc file_map, size_t file_size)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: implement MachO support */
    return 0;
}

byte *
module_dynamorio_lib_base(void)
{
    return (byte *) &_mh_dylib_header;
}
