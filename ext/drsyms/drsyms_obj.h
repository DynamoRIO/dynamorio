/* **********************************************************
 * Copyright (c) 2011-2020 Google, Inc.  All rights reserved.
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

/* drsyms UNIX arch-specific (ELF or PECOFF) header, separated so we don't
 * have to include dwarf.h everywhere
 */

#ifndef DRSYMS_ARCH_H
#define DRSYMS_ARCH_H

#include "drsyms.h"
#include "dwarf.h"
#include "libdwarf.h"

/***************************************************************************
 * Platform-specific: Linux (ELF) or Cygwin/MinGW (PECOFF)
 */

void
drsym_obj_init(void);

void *
drsym_obj_mod_init_pre(byte *map_base, size_t file_size);

#ifdef WINDOWS
/* This is called between init_pre and init_post */
bool
drsym_obj_remap_as_image(void *mod_in);
#endif

/* This takes a new map_base which may not match the one passed to init_pre,
 * if drsym_obj_remap_as_image() returned true.
 */
bool
drsym_obj_mod_init_post(void *mod_in, byte *map_base, void *dwarf_info);

bool
drsym_obj_dwarf_init(void *mod_in, Dwarf_Debug *dbg);

void
drsym_obj_mod_exit(void *mod_in);

drsym_debug_kind_t
drsym_obj_info_avail(void *mod_in);

byte *
drsym_obj_load_base(void *mod_in);

const char *
drsym_obj_debuglink_section(void *mod_in, const char *modpath);

uint
drsym_obj_num_symbols(void *mod_in);

const char *
drsym_obj_symbol_name(void *mod_in, uint idx);

/* For a symbol that should be skipped (e.g., it's an import symbol in
 * the same table being indexed), returns DRSYM_ERROR_SYMBOL_NOT_FOUND
 * and 0 for *offs_start.
 */
drsym_error_t
drsym_obj_symbol_offs(void *mod_in, uint idx, size_t *offs_start OUT,
                      size_t *offs_end OUT);

drsym_error_t
drsym_obj_addrsearch_symtab(void *mod_in, size_t modoffs, uint *idx OUT);

bool
drsym_obj_same_file(const char *path1, const char *path2);

const char *
drsym_obj_debug_path(void);

const char *
drsym_obj_build_id(void *mod_in);

/***************************************************************************
 * DWARF
 */

void *
drsym_dwarf_init(Dwarf_Debug dbg);

void
drsym_dwarf_exit(void *mod_in);

void
drsym_dwarf_set_obj_offs(void *mod_in, ssize_t adjust);

void
drsym_dwarf_set_load_base(void *mod_in, byte *load_base);

bool
drsym_dwarf_search_addr2line(void *mod_in, Dwarf_Addr pc, drsym_info_t *sym_info INOUT);

drsym_error_t
drsym_dwarf_enumerate_lines(void *mod_in, drsym_enumerate_lines_cb callback, void *data);

#endif /* DRSYMS_ARCH_H */
