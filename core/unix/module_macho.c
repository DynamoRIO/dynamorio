/* *******************************************************************************
 * Copyright (c) 2013-2014 Google, Inc.  All rights reserved.
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
 * + export iterator and forwarded exports (i#1360)
 * + imports
 * + relocations
 *
 * We deliberately do not statically partition into single types that
 * map to _64 for X64 and 32-bit versions for 32-bit, to support 64-bit
 * DR handling 32-bit modules.  The Mac headers containing both structs
 * make this easier.
 */

#include "../globals.h"
#include "../module_shared.h"
#include "os_private.h"
#include "module_private.h"
#include "memquery_macos.h"
#include <mach-o/ldsyms.h> /* _mh_dylib_header */
#include <mach-o/loader.h> /* mach_header */
#include <mach/thread_status.h> /* i386_thread_state_t */
#include <stddef.h> /* offsetof */
#include <string.h> /* strcmp */
#include <dlfcn.h>

#ifdef NOT_DYNAMORIO_CORE_PROPER
# undef LOG
# define LOG(...) /* nothing */
#endif

/* XXX i#1345: support mixed-mode 32-bit and 64-bit in one process.
 * There is no official support for that on Linux or Windows and for now we do
 * not support it either, especially not mixing libraries.
 */
#ifdef X64
typedef struct mach_header_64 mach_header_t;
typedef struct segment_command_64 segment_command_t;
#else
typedef struct mach_header mach_header_t;
typedef struct segment_command segment_command_t;
#endif

/* Like is_elf_so_header(), if size == 0 then safe-reads the header; else
 * assumes that [base, base+size) is readable.
 */
static bool
is_macho_header(app_pc base, size_t size)
{
    struct mach_header hdr_safe;
    struct mach_header *hdr;
    if (base == NULL)
        return false;
    if (size >= sizeof(hdr_safe)) {
        hdr = (struct mach_header *) base;
    } else {
        if (!safe_read(base, sizeof(hdr_safe), &hdr_safe))
            return false;
        hdr = &hdr_safe;
    }
    ASSERT(offsetof(struct mach_header, filetype) ==
           offsetof(struct mach_header_64, filetype));
    if ((hdr->magic == MH_MAGIC && hdr->cputype == CPU_TYPE_X86) ||
        (hdr->magic == MH_MAGIC_64 && hdr->cputype == CPU_TYPE_X86_64)) {
        /* XXX: should we include MH_PRELOAD or MH_FVMLIB? */
        if (hdr->filetype == MH_EXECUTE ||
            hdr->filetype == MH_DYLIB ||
            hdr->filetype == MH_BUNDLE) {
            return true;
        }
    }
    return false;
}

bool
module_file_has_module_header(const char *filename)
{
    bool res = false;
    struct mach_header hdr;
    file_t fd = os_open(filename, OS_OPEN_READ);
    if (fd == INVALID_FILE)
        return false;
    if (os_read(fd, &hdr, sizeof(hdr)) == sizeof(hdr) &&
        is_macho_header((app_pc)&hdr, sizeof(hdr)))
        res = true;
    os_close(fd);
    return res;
}

bool
module_is_partial_map(app_pc base, size_t size, uint memprot)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: implement MachO support */
    return false;
}

#ifndef NOT_DYNAMORIO_CORE_PROPER
bool
module_walk_program_headers(app_pc base, size_t view_size, bool at_map,
                            OUT app_pc *out_base /* relative pc */,
                            OUT app_pc *out_first_end /* relative pc */,
                            OUT app_pc *out_max_end /* relative pc */,
                            OUT char **out_soname,
                            OUT os_module_data_t *out_data)
{
    mach_header_t *hdr = (mach_header_t *) base;
    struct load_command *cmd, *cmd_stop;
    app_pc seg_min_start = (app_pc) POINTER_MAX;
    app_pc seg_max_end = NULL, seg_first_end;
    bool found_seg = false;
    size_t linkedit_file_off = 0, linkedit_mem_off, exports_file_off = 0;
    ASSERT(is_macho_header(base, view_size));
    cmd = (struct load_command *)(hdr + 1);
    cmd_stop = (struct load_command *)((byte *)cmd + hdr->sizeofcmds);
    while (cmd < cmd_stop) {
        if (cmd->cmd == LC_SEGMENT) {
            segment_command_t *seg = (segment_command_t *) cmd;
            found_seg = true;
            LOG(GLOBAL, LOG_VMAREAS, 4,
                "%s: segment %s addr=0x%x sz=0x%x file=0x%x\n", __FUNCTION__,
                seg->segname, seg->vmaddr, seg->vmsize, seg->fileoff);
            if ((app_pc)seg->vmaddr + seg->vmsize > seg_max_end)
                seg_max_end = (app_pc)seg->vmaddr + seg->vmsize;
            if (strcmp(seg->segname, "__PAGEZERO") == 0 &&
                seg->initprot == 0) {
                /* Skip it: zero page for executable, and it's hard to identify
                 * that page as part of the module.
                 */
            } else if ((app_pc)seg->vmaddr < seg_min_start) {
                seg_min_start = (app_pc) seg->vmaddr;
                seg_first_end = (app_pc)seg->vmaddr + seg->vmsize;
                if (out_data != NULL) {
                    module_add_segment_data(out_data, 0/*don't know*/,
                                            (app_pc) seg->vmaddr, seg->vmsize,
                                            /* assuming we want initprot, not maxprot */
                                            vmprot_to_memprot(seg->initprot),
                                            /* XXX: alignment is specified per section --
                                             * ignoring for now
                                             */
                                            PAGE_SIZE);
                }
            }
            if (strcmp(seg->segname, "__LINKEDIT") == 0) {
                linkedit_file_off = seg->fileoff;
                linkedit_mem_off = seg->vmaddr;
            }
        } else if (cmd->cmd == LC_DYLD_INFO || cmd->cmd == LC_DYLD_INFO_ONLY) {
            struct dyld_info_command *di = (struct dyld_info_command *) cmd;
            LOG(GLOBAL, LOG_VMAREAS, 4,
                "%s: exports addr=0x%x sz=0x%x\n", __FUNCTION__,
                di->export_off, di->export_size);
            exports_file_off = di->export_off;
            if (out_data != NULL)
                out_data->exports_sz = di->export_size;
        } else if (cmd->cmd == LC_ID_DYLIB) {
            struct dylib_command *dy = (struct dylib_command *) cmd;
            char *soname = (char *)cmd + dy->dylib.name.offset;
            /* XXX: we assume these strings are always null-terminated */
            LOG(GLOBAL, LOG_VMAREAS, 4, "%s: lib identity %s\n", __FUNCTION__, soname);
            if (out_soname != NULL)
                *out_soname = soname;
            if (out_data != NULL)
                out_data->timestamp = dy->dylib.timestamp;
        }
        cmd = (struct load_command *)((byte *)cmd + cmd->cmdsize);
    }
    if (found_seg) {
        LOG(GLOBAL, LOG_VMAREAS, 4, "%s: bounds "PFX"-"PFX"\n", __FUNCTION__,
            seg_min_start, seg_max_end);
        if (out_base != NULL)
            *out_base = seg_min_start;
        if (out_first_end != NULL)
            *out_first_end = seg_first_end;
        if (out_max_end != NULL)
            *out_max_end = seg_max_end;
        if (out_data != NULL) {
            /* FIXME i#58: we need to fill in more of out_data, like preferred
             * base.  For alignment: it's per-section, so how handle it?
             */
            out_data->base_address = seg_min_start;
            out_data->alignment = PAGE_SIZE; /* FIXME i#58: need min section align? */
            if (linkedit_file_off > 0) {
                out_data->exports = base - (ptr_int_t)seg_min_start +
                    ((ssize_t)linkedit_mem_off - linkedit_file_off) + exports_file_off;
            } else
                out_data->exports = NULL;
        }
    }
    return found_seg;
}

uint
module_num_program_headers(app_pc base)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: implement MachO support */
    return 0;
}
#endif /* !NOT_DYNAMORIO_CORE_PROPER */

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
    mach_header_t *hdr = (mach_header_t *) base;
    struct load_command *cmd, *cmd_stop;
    ASSERT(is_macho_header(base, PAGE_SIZE));
    cmd = (struct load_command *)(hdr + 1);
    cmd_stop = (struct load_command *)((byte *)cmd + hdr->sizeofcmds);
    while (cmd < cmd_stop) {
        if (cmd->cmd == LC_UNIXTHREAD) {
            /* There's no nice struct for this: see thread_command in loader.h. */
#           define LC_UNIXTHREAD_REGS_OFFS 16
#ifdef X64
            const x86_thread_state64_t *reg = (const x86_thread_state64_t *)
                ((char*)cmd + LC_UNIXTHREAD_REGS_OFFS);
            return (app_pc)reg->__rip + load_delta
#else
            const i386_thread_state_t *reg = (const i386_thread_state_t *)
                ((byte *)cmd + LC_UNIXTHREAD_REGS_OFFS);
            return (app_pc)reg->__eip + load_delta;
#endif
        }
        cmd = (struct load_command *)((byte *)cmd + cmd->cmdsize);
    }
    return NULL;
}

bool
module_is_header(app_pc base, size_t size /*optional*/)
{
    return is_macho_header(base, size);
}

#ifndef NOT_DYNAMORIO_CORE_PROPER
/* ULEB128 is a little-endian 128-base encoding where msb is set if there's
 * another byte of data to add to the integer represented.
 */
static ptr_uint_t
read_uleb128(byte *start, byte *max, byte **next_entry OUT)
{
    ptr_uint_t val = 0;
    uint shift = 0;
    uint octet;
    byte *next = start;
    while (next < max) {
        /* Each byte ("octet") holds 7 bits of the integer.  If msb is 0,
         * we're done; else, there's another octet.
         */
        octet = *next;
        val |= ((octet & 0x7f) << shift);
        next++;
        if (octet < 128)
            break;
        shift += 7;
    }
    if (next_entry != NULL)
        *next_entry = next;
    return val;
}

app_pc
get_proc_address_from_os_data(os_module_data_t *os_data,
                              ptr_int_t load_delta,
                              const char *name,
                              OUT bool *is_indirect_code)
{
    /* Walk the Mach-O export trie.  We don't support < 10.6 which is when
     * they put this scheme in place.
     */
    app_pc res = NULL;
    ptr_uint_t node_sz, node_offs;
    byte *ptr = os_data->exports;
    byte *max = ptr + os_data->exports_sz;
    uint i, children;
    const char *name_loc = name;
    bool first_node = true;
    if (os_data->exports == NULL || name[0] == '0')
        return NULL;

    LOG(GLOBAL, LOG_SYMBOLS, 4, "%s: trie "PFX"-"PFX"\n", __FUNCTION__, ptr, max);
    while (ptr < max) {
        bool match = false;
        node_sz = read_uleb128(ptr, max, &ptr);
        if (*name_loc == '\0')
            break; /* matched */
        /* Skip symbol info, until we find a match */
        ptr += node_sz;
        children = *ptr++;
        LOG(GLOBAL, LOG_SYMBOLS, 4, "  node @"PFX" size=%d children=%d\n",
            ptr - 1 - node_sz - 1/*can be wrong*/, node_sz, children);
        for (i = 0; i < children; i++) {
            /* Each edge is a string followed by the offset of that edge's target node */
            char next_char;
            uint idx = 0;
            byte *pfx_start = ptr;
            bool no_inc = false;
            match = true;
            LOG(GLOBAL, LOG_SYMBOLS, 4, "\tchild #%d: %s vs %s\n", i, ptr, name_loc);
            while (true) {
                next_char = *(char *)ptr;
                ptr++;
                if (next_char == '\0')
                    break;
                /* Auto-add "_" -- we assume always looking up regular syms */
                if (first_node && next_char == '_' && name_loc == name && idx == 0 &&
                    ptr == pfx_start + 1)
                    no_inc = true;
                else if (match && *(name_loc + idx) != next_char)
                    match = false;
                if (no_inc)
                    no_inc = false;
                else
                    idx++;
            }
            node_offs = read_uleb128(ptr, max, &ptr);
            if (match) {
                LOG(GLOBAL, LOG_SYMBOLS, 4, "\tmatched child #%d offs="PIFX"\n",
                    i, node_offs);
                name_loc += idx;
                if (node_offs == 0) /* avoid infinite loop */
                    return NULL;
                ptr = os_data->exports + node_offs;
                break;
            }
        }
        if (first_node)
            first_node = false;
        if (!match)
            return NULL;
    }
    /* We have a match */
    if (node_sz > 0) {
        uint flags = read_uleb128(ptr, max, &ptr);
        if (TEST(EXPORT_SYMBOL_FLAGS_REEXPORT, flags)) {
            /* Forwarder */
            read_uleb128(ptr, max, &ptr); /* ordinal */
            const char *forw_name = (const char *) ptr;
            if (forw_name[0] == '\0') /* see if has different name */
                forw_name = name;
            LOG(GLOBAL, LOG_SYMBOLS, 4, "\tforwarder %s\n", forw_name);
            /* FIXME i#1360: handle forwards */
        } else {
            size_t sym_offs = read_uleb128(ptr, max, &ptr);
            res = (app_pc)(sym_offs + load_delta);
            LOG(GLOBAL, LOG_SYMBOLS, 4, "\tmatch offs="PIFX" => "PFX"\n",
                sym_offs, res);
        }
    }
    if (is_indirect_code != NULL)
        *is_indirect_code = false;
    return res;
}

generic_func_t
get_proc_address_ex(module_base_t lib, const char *name, bool *is_indirect_code OUT)
{
    app_pc res = NULL;
    module_area_t *ma;
    os_get_module_info_lock();
    ma = module_pc_lookup((app_pc)lib);
    if (ma != NULL) {
        res = get_proc_address_from_os_data(&ma->os_data,
                                            ma->start - ma->os_data.base_address,
                                            name, is_indirect_code);
    }
    os_get_module_info_unlock();
    LOG(GLOBAL, LOG_SYMBOLS, 2, "%s: %s => "PFX"\n", __func__, name, res);
    return convert_data_to_function(res);
}

generic_func_t
get_proc_address(module_base_t lib, const char *name)
{
    return get_proc_address_ex(lib, name, NULL);
}
#endif /* !NOT_DYNAMORIO_CORE_PROPER */

size_t
module_get_header_size(app_pc module_base)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: implement MachO support */
    return 0;
}

bool
module_get_platform(file_t f, dr_platform_t *platform)
{
    struct mach_header hdr;
    if (os_read(f, &hdr, sizeof(hdr)) != sizeof(hdr))
        return false;
    if (!is_macho_header((app_pc)&hdr, sizeof(hdr)))
        return false;
    switch (hdr.cputype) {
    case CPU_TYPE_X86_64: *platform = DR_PLATFORM_64BIT; break;
    case CPU_TYPE_X86:    *platform = DR_PLATFORM_32BIT; break;
    default:
        return false;
    }
    return true;
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

#ifndef NOT_DYNAMORIO_CORE_PROPER
char *
get_shared_lib_name(app_pc map)
{
    char *soname;
    if (!module_walk_program_headers(map, PAGE_SIZE/*at least*/, false,
                                     NULL, NULL, NULL, &soname, NULL))
        return NULL;
    return soname;
}

void
module_get_os_privmod_data(app_pc base, size_t size, bool relocated,
                           OUT os_privmod_data_t *pd)
{
    pd->load_delta = 0; /* FIXME i#58: need preferred base */
    module_walk_program_headers(base, size, false, NULL, NULL, NULL, &pd->soname, NULL);
    /* XXX i#1285: fill in the rest of the fields */
    return;
}
#endif /* !NOT_DYNAMORIO_CORE_PROPER */
 
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
