/* *******************************************************************************
 * Copyright (c) 2013-2020 Google, Inc.  All rights reserved.
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
#include "module_macos_dyld.h"
#include <mach-o/ldsyms.h> /* _mh_dylib_header */
#include <mach-o/loader.h> /* mach_header */
#include <mach-o/dyld_images.h>
#include <mach/thread_status.h> /* i386_thread_state_t */
#include <mach-o/nlist.h>
#include <mach-o/fat.h>
#include <sys/syscall.h>
#include <stddef.h> /* offsetof */
#include <dlfcn.h>

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

bool
module_walk_program_headers(app_pc base, size_t view_size, bool at_map, bool dyn_reloc,
                            OUT app_pc *out_base /* relative pc */,
                            OUT app_pc *out_first_end /* relative pc */,
                            OUT app_pc *out_max_end /* relative pc */,
                            OUT char **out_soname, OUT os_module_data_t *out_data)
{
    mach_header_t *hdr = (mach_header_t *)base;
    struct load_command *cmd, *cmd_stop;
    app_pc seg_min_start = (app_pc)POINTER_MAX;
    app_pc seg_max_end = NULL, seg_first_end;
    bool found_seg = false;
    size_t linkedit_file_off = 0, linkedit_mem_off, exports_file_off = 0;
    ASSERT(is_macho_header(base, view_size));
    cmd = (struct load_command *)(hdr + 1);
    cmd_stop = (struct load_command *)((byte *)cmd + hdr->sizeofcmds);
    while (cmd < cmd_stop) {
        if (cmd->cmd == LC_SEGMENT || cmd->cmd == LC_SEGMENT_64) {
            segment_command_t *seg = (segment_command_t *)cmd;
            found_seg = true;
            LOG(GLOBAL, LOG_VMAREAS, 4, "%s: segment %s addr=0x%x sz=0x%x file=0x%x\n",
                __FUNCTION__, seg->segname, seg->vmaddr, seg->vmsize, seg->fileoff);
            if ((app_pc)seg->vmaddr + seg->vmsize > seg_max_end)
                seg_max_end = (app_pc)seg->vmaddr + seg->vmsize;
            if (strcmp(seg->segname, "__PAGEZERO") == 0 && seg->initprot == 0) {
                /* Skip it: zero page for executable, and it's hard to identify
                 * that page as part of the module.
                 */
            } else if ((app_pc)seg->vmaddr < seg_min_start) {
                seg_min_start = (app_pc)seg->vmaddr;
                seg_first_end = (app_pc)seg->vmaddr + seg->vmsize;
            }
            if (strcmp(seg->segname, "__LINKEDIT") == 0) {
                linkedit_file_off = seg->fileoff;
                linkedit_mem_off = seg->vmaddr;
            }
        } else if (cmd->cmd == LC_DYLD_INFO || cmd->cmd == LC_DYLD_INFO_ONLY) {
            struct dyld_info_command *di = (struct dyld_info_command *)cmd;
            LOG(GLOBAL, LOG_VMAREAS, 4, "%s: exports addr=0x%x sz=0x%x\n", __FUNCTION__,
                di->export_off, di->export_size);
            exports_file_off = di->export_off;
            if (out_data != NULL)
                out_data->exports_sz = di->export_size;
        } else if (cmd->cmd == LC_ID_DYLIB) {
            struct dylib_command *dy = (struct dylib_command *)cmd;
            char *soname = (char *)cmd + dy->dylib.name.offset;
            /* XXX: we assume these strings are always null-terminated */
            /* They seem to have full paths on Mac.  We drop to basename, as
             * that's what many clients expect for module_name.
             */
            char *slash = strrchr(soname, '/');
            if (slash != NULL)
                soname = slash + 1;
            LOG(GLOBAL, LOG_VMAREAS, 4, "%s: lib identity %s\n", __FUNCTION__, soname);
            if (out_soname != NULL)
                *out_soname = soname;
            if (out_data != NULL) {
                out_data->timestamp = dy->dylib.timestamp;
                out_data->current_version = dy->dylib.current_version;
                out_data->compatibility_version = dy->dylib.compatibility_version;
            }
        }
        cmd = (struct load_command *)((byte *)cmd + cmd->cmdsize);
    }
    if (found_seg) {
        ptr_int_t load_delta = base - seg_min_start;
        ptr_int_t linkedit_delta = 0;
        if (linkedit_file_off > 0) {
            linkedit_delta = ((ssize_t)linkedit_mem_off - linkedit_file_off);
        }
        LOG(GLOBAL, LOG_VMAREAS, 4, "%s: bounds " PFX "-" PFX "\n", __FUNCTION__,
            seg_min_start, seg_max_end);
        if (out_base != NULL)
            *out_base = seg_min_start;
        if (out_first_end != NULL)
            *out_first_end = seg_first_end;
        if (out_max_end != NULL)
            *out_max_end = seg_max_end;
        if (out_data != NULL) {
            app_pc shared_start, shared_end;
            bool have_shared = module_dyld_shared_region(&shared_start, &shared_end);
            size_t max_align = 0;
            if (have_shared && base >= shared_start && base < shared_end) {
                out_data->in_shared_cache = true;
            }
            /* Now that we have the load delta, we can add the abs addr segments */
            cmd = (struct load_command *)(hdr + 1);
            while (cmd < cmd_stop) {
                if (cmd->cmd == LC_SEGMENT || cmd->cmd == LC_SEGMENT_64) {
                    segment_command_t *seg = (segment_command_t *)cmd;
                    if (strcmp(seg->segname, "__PAGEZERO") == 0 && seg->initprot == 0) {
                        /* skip */
                    } else {
                        app_pc seg_start = (app_pc)seg->vmaddr + load_delta;
                        size_t seg_size = seg->vmsize;
                        bool shared = false;
                        if (strcmp(seg->segname, "__LINKEDIT") == 0 && have_shared &&
                            out_data->in_shared_cache) {
                            /* We assume that all __LINKEDIT segments in the
                             * dyld cache are shared as one single segment.
                             */
                            shared = true;
                            /* XXX: seg->vmsize is too large for these: it extends
                             * off the end of the mapping.  I have no idea why.
                             * So we truncate it.  We leave max_end above.
                             * For 10.14+ shared_end is actually the end of the libs,
                             * not the cache, so we do not truncate there.  There we have
                             * not seen this too-large size.
                             */
                            if (os_get_version() < MACOS_VERSION_MOJAVE &&
                                seg_start < shared_end &&
                                seg_start + seg->vmsize > shared_end) {
                                LOG(GLOBAL, LOG_VMAREAS, 4,
                                    "%s: truncating __LINKEDIT size from " PIFX
                                    " to " PIFX "\n",
                                    __FUNCTION__, seg->vmsize, shared_end - seg_start);
                                seg_size = shared_end - seg_start;
                            }
                        }
                        /* We compute alignment as the max section alignment. */
                        size_t align = 0;
                        section_t *sec, *sec_stop;
                        sec_stop = (section_t *)((byte *)seg + seg->cmdsize);
                        sec = (section_t *)(seg + 1);
                        while (sec < sec_stop) {
                            align = MAX(align, 1 << sec->align);
                            sec++;
                        }
                        LOG(GLOBAL, LOG_VMAREAS, 4,
                            "%s: %s max section alignment is " PIFX "\n", __FUNCTION__,
                            seg->segname, align);
                        module_add_segment_data(
                            out_data, 0 /*don't know*/, seg_start, seg_size,
                            /* we want initprot, not maxprot, right? */
                            vmprot_to_memprot(seg->initprot), align, shared,
                            seg->fileoff);
                        max_align = MAX(max_align, align);
                    }
                } else if (cmd->cmd == LC_SYMTAB) {
                    /* even if stripped, dynamic symbols are in this table */
                    struct symtab_command *symtab = (struct symtab_command *)cmd;
                    out_data->symtab =
                        (app_pc)(symtab->symoff + load_delta + linkedit_delta);
                    out_data->num_syms = symtab->nsyms;
                    out_data->strtab =
                        (app_pc)(symtab->stroff + load_delta + linkedit_delta);
                    out_data->strtab_sz = symtab->strsize;
                } else if (cmd->cmd == LC_UUID) {
                    memcpy(out_data->uuid, ((struct uuid_command *)cmd)->uuid,
                           sizeof(out_data->uuid));
                }
                cmd = (struct load_command *)((byte *)cmd + cmd->cmdsize);
            }
            /* FIXME i#58: we need to fill in more of out_data, like preferred
             * base.  For alignment: it's per-section, so we pass the max.
             */
            out_data->base_address = seg_min_start;
            out_data->alignment = max_align;
            if (linkedit_file_off > 0 && exports_file_off > 0) {
                out_data->exports =
                    (app_pc)load_delta + exports_file_off + linkedit_delta;
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

bool
module_read_program_header(app_pc base, uint segment_num,
                           OUT app_pc *segment_base /* relative pc */,
                           OUT app_pc *segment_end /* relative pc */,
                           OUT uint *segment_prot, OUT size_t *segment_align)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: implement MachO support */
    return false;
}

app_pc
module_entry_point(app_pc base, ptr_int_t load_delta)
{
    mach_header_t *hdr = (mach_header_t *)base;
    struct load_command *cmd, *cmd_stop;
    ASSERT(is_macho_header(base, PAGE_SIZE));
    cmd = (struct load_command *)(hdr + 1);
    cmd_stop = (struct load_command *)((byte *)cmd + hdr->sizeofcmds);
    while (cmd < cmd_stop) {
        if (cmd->cmd == LC_UNIXTHREAD) {
            /* There's no nice struct for this: see thread_command in loader.h. */
#define LC_UNIXTHREAD_REGS_OFFS 16
#ifdef X64
#    ifdef X86
            const x86_thread_state64_t *reg =
                (const x86_thread_state64_t *)((char *)cmd + LC_UNIXTHREAD_REGS_OFFS);
            return (app_pc)reg->__rip + load_delta;
#    else
            const arm_thread_state64_t *reg =
                (const arm_thread_state64_t *)((char *)cmd + LC_UNIXTHREAD_REGS_OFFS);
            return (app_pc)reg->__pc + load_delta;
#    endif
#else
            const i386_thread_state_t *reg =
                (const i386_thread_state_t *)((byte *)cmd + LC_UNIXTHREAD_REGS_OFFS);
            return (app_pc)reg->__eip + load_delta;
#endif
        }
        /* XXX: should we have our own headers so we can build on an older machine? */
#ifdef LC_MAIN
        else if (cmd->cmd == LC_MAIN) {
            struct entry_point_command *ec = (struct entry_point_command *)cmd;
            /* Offset is from start of __TEXT so we just add to base (which has
             * skipped __PAGEZERO)
             */
            return base + (ptr_uint_t)ec->entryoff;
        }
#endif
        cmd = (struct load_command *)((byte *)cmd + cmd->cmdsize);
    }
    return NULL;
}

bool
module_is_header(app_pc base, size_t size /*optional*/)
{
    return is_macho_header(base, size);
}

bool
module_is_executable(app_pc base)
{
    struct mach_header *hdr = (struct mach_header *)base;
    if (!is_macho_header(base, 0))
        return false;
    /* We shouldn't see MH_PRELOAD as it can't be loaded by the kernel.
     * PIE is still MH_EXECUTE (+ flags MH_PIE) so we can distinguish
     * an executable from a library.
     */
    return (hdr->filetype == MH_EXECUTE);
}

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
get_proc_address_from_os_data(os_module_data_t *os_data, ptr_int_t load_delta,
                              const char *name, OUT bool *is_indirect_code)
{
    /* Walk the Mach-O export trie.  We don't support < 10.6 which is when
     * they put this scheme in place.
     * XXX: should we go ahead and look in symtab if we don't find it in
     * the trie?  That could include internal symbols too.  Plus our
     * current lookup_in_symtab() is a linear walk.  Xref drsyms which sorts
     * it and does a binary search.
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

    LOG(GLOBAL, LOG_SYMBOLS, 4, "%s %s: trie " PFX "-" PFX "\n", __FUNCTION__, name, ptr,
        max);
    while (ptr < max) {
        bool match = false;
        node_sz = read_uleb128(ptr, max, &ptr);
        if (*name_loc == '\0')
            break; /* matched */
        /* Skip symbol info, until we find a match */
        ptr += node_sz;
        children = *ptr++;
        LOG(GLOBAL, LOG_SYMBOLS, 4, "  node @" PFX " size=%d children=%d\n",
            ptr - 1 - node_sz - 1 /*can be wrong*/, node_sz, children);
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
                LOG(GLOBAL, LOG_SYMBOLS, 4, "\tmatched child #%d offs=" PIFX "\n", i,
                    node_offs);
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
        if (is_indirect_code != NULL)
            *is_indirect_code = false;
        if (TEST(EXPORT_SYMBOL_FLAGS_REEXPORT, flags)) {
            /* Forwarder */
            read_uleb128(ptr, max, &ptr); /* ordinal */
            const char *forw_name = (const char *)ptr;
            if (forw_name[0] == '\0') /* see if has different name */
                forw_name = name;
            LOG(GLOBAL, LOG_SYMBOLS, 4, "\tforwarder %s\n", forw_name);
            /* FIXME i#1360: handle forwards */
        } else if (TEST(EXPORT_SYMBOL_FLAGS_STUB_AND_RESOLVER, flags)) {
            /* Lazy or non-lazy pointer */
            DEBUG_DECLARE(size_t stub_offs =)
            read_uleb128(ptr, max, &ptr);
            size_t resolver_offs = read_uleb128(ptr, max, &ptr);
            res = (app_pc)(resolver_offs + load_delta);
            if (is_indirect_code != NULL)
                *is_indirect_code = true;
            LOG(GLOBAL, LOG_SYMBOLS, 4, "\tstub=" PFX ", resolver=" PFX "\n",
                (app_pc)(stub_offs + load_delta), res);
        } else {
            size_t sym_offs = read_uleb128(ptr, max, &ptr);
            res = (app_pc)(sym_offs + load_delta);
            LOG(GLOBAL, LOG_SYMBOLS, 4, "\tmatch offs=" PIFX " => " PFX "\n", sym_offs,
                res);
        }
    }
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
                                            (ma->os_data.in_shared_cache)
                                                ?
                                                /* segment starts are rebased, but not
                                                 * the trie offsets
                                                 */
                                                (ptr_int_t)ma->start
                                                : ma->start - ma->os_data.base_address,
                                            name, is_indirect_code);
    }
    os_get_module_info_unlock();
    LOG(GLOBAL, LOG_SYMBOLS, 2, "%s: %s => " PFX "\n", __func__, name, res);
    return convert_data_to_function(res);
}

generic_func_t
d_r_get_proc_address(module_base_t lib, const char *name)
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
module_read_os_data(app_pc base, bool dyn_reloc, OUT ptr_int_t *load_delta,
                    OUT os_module_data_t *os_data, OUT char **soname)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: implement MachO support */
    return false;
}

char *
get_shared_lib_name(app_pc map)
{
    char *soname;
    if (!module_walk_program_headers(map, PAGE_SIZE /*at least*/, false,
                                     true /*doesn't matter for soname*/, NULL, NULL, NULL,
                                     &soname, NULL))
        return NULL;
    return soname;
}

void
module_get_os_privmod_data(app_pc base, size_t size, bool dyn_reloc,
                           OUT os_privmod_data_t *pd)
{
    pd->load_delta = 0; /* FIXME i#58: need preferred base */
    module_walk_program_headers(base, size, false,
                                true, /* i#1589: ld.so relocated .dynamic */
                                NULL, NULL, NULL, &pd->soname, NULL);
    /* XXX i#1285: fill in the rest of the fields */
    return;
}

byte *
module_dynamorio_lib_base(void)
{
#if defined(STATIC_LIBRARY) || defined(STANDALONE_UNIT_TEST)
    return (byte *)&_mh_execute_header;
#else
    return (byte *)&_mh_dylib_header;
#endif
}

ptr_uint_t
module_get_text_section(app_pc file_map, size_t file_size)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: implement MachO support */
    return 0;
}

bool
module_dyld_shared_region(app_pc *start OUT, app_pc *end OUT)
{
    uint64 cache_start;
    struct dyld_cache_header *hdr;
    struct dyld_cache_mapping_info *map;
    uint i;
    app_pc cache_end;
    if (dynamorio_syscall(SYS_shared_region_check_np, 1, &cache_start) != 0) {
        LOG(GLOBAL, LOG_VMAREAS, 2, "could not find dyld shared cache\n");
        return false;
    }
    hdr = (struct dyld_cache_header *)cache_start;
    map = (struct dyld_cache_mapping_info *)(cache_start + hdr->mappingOffset);
    cache_end = (app_pc)cache_start;
    /* Find the max endpoint.  We assume the gap in between the +ro and
     * +rw mappings will never hold anything else.
     */
    for (i = 0; i < hdr->mappingCount; i++) {
        if (map->address + map->size > (ptr_uint_t)cache_end)
            cache_end = (app_pc)(ptr_uint_t)(map->address + map->size);
        map++;
    }
    LOG(GLOBAL, LOG_VMAREAS, 2, "dyld shared cache is " PFX "-" PFX "\n",
        (app_pc)cache_start, cache_end);
    if (start != NULL)
        *start = (app_pc)cache_start;
    if (end != NULL)
        *end = cache_end;
    return true;
}

/* Brute force linear walk lookup */
static app_pc
lookup_in_symtab(app_pc lib_base, const char *symbol)
{
    app_pc res = NULL;
    module_area_t *ma;
    os_get_module_info_lock();
    ma = module_pc_lookup(lib_base);
    if (ma != NULL) {
        uint i;
        for (i = 0; i < ma->os_data.num_syms; i++) {
            nlist_t *sym = ((nlist_t *)(ma->os_data.symtab)) + i;
            const char *name;
            if (sym->n_un.n_strx > 0 && sym->n_un.n_strx < ma->os_data.strtab_sz &&
                sym->n_value > 0) {
                name = (const char *)ma->os_data.strtab + sym->n_un.n_strx;
                if (name[0] == '_')
                    name++;
                LOG(GLOBAL, LOG_SYMBOLS, 5, "\tsym %d = %s\n", i, name);
                if (strcmp(name, symbol) == 0) {
                    res = (app_pc)(sym->n_value + ma->start - ma->os_data.base_address);
                    break;
                }
            }
        }
    }
    os_get_module_info_unlock();
    LOG(GLOBAL, LOG_SYMBOLS, 2, "%s: %s => " PFX "\n", __func__, symbol, res);
    return res;
}

void
module_walk_dyld_list(app_pc dyld_base)
{
    uint i;
    struct dyld_all_image_infos *dyinfo;
    /* The DYLD_ALL_IMAGE_INFOS_OFFSET_OFFSET added in 10.6 seems to not
     * exist in 10.9 anymore so we do not use it.  Instead we directly
     * look up "dyld_all_image_infos".  Unfortunately dyld has no exports
     * trie and so we must walk the symbol table.
     */
    dyinfo = (struct dyld_all_image_infos *)lookup_in_symtab(dyld_base,
                                                             "dyld_all_image_infos");
    /* We rely on this -- so until Mac support is more solid let's assert */
    if (dyinfo == NULL || !is_readable_without_exception((byte *)dyinfo, PAGE_SIZE)) {
        SYSLOG_INTERNAL_WARNING("failed to walk dyld shared cache libraries");
        return;
    }
    LOG(GLOBAL, LOG_VMAREAS, 2, "Walking %d modules in dyld module list\n",
        dyinfo->infoArrayCount);
    for (i = 0; i < dyinfo->infoArrayCount; i++) {
        const struct dyld_image_info *modinfo = &dyinfo->infoArray[i];
        bool already = false;
        os_get_module_info_lock();
        if (module_pc_lookup((app_pc)modinfo->imageLoadAddress) != NULL)
            already = true;
        os_get_module_info_unlock();
        if (already) {
            LOG(GLOBAL, LOG_VMAREAS, 2, "Module %d: " PFX " already seen %s\n", i,
                modinfo->imageLoadAddress, modinfo->imageFilePath);
            continue;
        }
        /* module_list_add() will call module_walk_program_headers() and find
         * the segments.  The dyld shared cache typically splits __TEXT from
         * __DATA, so we don't want to try to find a "size" of the module.
         */
        LOG(GLOBAL, LOG_VMAREAS, 2, "Module %d: " PFX " %s\n", i,
            modinfo->imageLoadAddress, modinfo->imageFilePath);
        // For aarch64, dyld will pack libraries in less than a page boundary.
        module_list_add((app_pc)modinfo->imageLoadAddress,
                        IF_AARCH64_ELSE(0x1000, PAGE_SIZE), false, modinfo->imageFilePath,
                        0);
    }
}
