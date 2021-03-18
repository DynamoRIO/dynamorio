/* *******************************************************************************
 * Copyright (c) 2012-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2011 Massachusetts Institute of Technology  All rights reserved.
 * Copyright (c) 2008-2010 VMware, Inc.  All rights reserved.
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

#include "../globals.h"
#include "../module_shared.h"
#include "module_private.h"
#include "os_private.h"
#include "../utils.h"
#include "instrument.h"
#include <stddef.h> /* offsetof */
#ifdef LINUX
#    include "rseq_linux.h"
#endif

#ifdef NOT_DYNAMORIO_CORE_PROPER
#    undef LOG
#    define LOG(...) /* nothing */
#else                /* !NOT_DYNAMORIO_CORE_PROPER */

/* XXX; perhaps make a module_list interface to check for overlap? */
extern vm_area_vector_t *loaded_module_areas;

void
os_modules_init(void)
{
    /* Nothing. */
}

void
os_modules_exit(void)
{
    /* Nothing. */
}

/* view_size can be the size of the first mapping, to handle non-contiguous
 * modules -- we'll update the module's size here
 */
void
os_module_area_init(module_area_t *ma, app_pc base, size_t view_size, bool at_map,
                    const char *filepath, uint64 inode HEAPACCT(which_heap_t which))
{
    app_pc mod_base, mod_end;
    ptr_int_t load_delta;
    char *soname = NULL;
    ASSERT(module_is_header(base, view_size));

    /* i#1589: use privload data if it exists (for client lib) */
    if (!privload_fill_os_module_info(base, &mod_base, &mod_end, &soname, &ma->os_data)) {
        /* XXX i#1860: on Android we'll fail to fill in info from .dynamic, so
         * we'll have incomplete data until the loader maps the segment with .dynamic.
         * ma->os_data.have_dynamic_info indicates whether we have the info.
         */
        module_walk_program_headers(base, view_size, at_map,
                                    !at_map, /* i#1589: ld.so relocates .dynamic */
                                    &mod_base, NULL, &mod_end, &soname, &ma->os_data);
    }
    if (ma->os_data.contiguous) {
        app_pc map_end = ma->os_data.segments[ma->os_data.num_segments - 1].end;
        module_list_add_mapping(ma, base, map_end);
        /* update, since may just be 1st segment size */
        ma->end = map_end;
    } else {
        /* Add the non-contiguous segments (i#160/PR 562667).  We could just add
         * them all separately but vmvectors are more efficient with fewer
         * entries so we merge.  We don't want general merging in our vector
         * either.
         */
        app_pc seg_base;
        uint i;
        ASSERT(ma->os_data.num_segments > 0 && ma->os_data.segments != NULL);
        seg_base = ma->os_data.segments[0].start;
        for (i = 1; i < ma->os_data.num_segments; i++) {
            if (ma->os_data.segments[i].start > ma->os_data.segments[i - 1].end ||
                /* XXX: for shared we just add the first one.  But if the first
                 * module is unloaded we'll be missing an entry for the others.
                 * We assume this won't happen b/c our only use of this now is
                 * the MacOS dyld shared cache's shared __LINKEDIT segment.  If
                 * it could happen we should switch to a refcount in the vector.
                 */
                ma->os_data.segments[i - 1].shared) {
                if (!ma->os_data.segments[i - 1].shared ||
                    !vmvector_overlap(loaded_module_areas, seg_base,
                                      ma->os_data.segments[i - 1].end)) {
                    module_list_add_mapping(ma, seg_base,
                                            ma->os_data.segments[i - 1].end);
                }
                seg_base = ma->os_data.segments[i].start;
            }
        }
        if (!ma->os_data.segments[i - 1].shared ||
            !vmvector_overlap(loaded_module_areas, seg_base,
                              ma->os_data.segments[i - 1].end))
            module_list_add_mapping(ma, seg_base, ma->os_data.segments[i - 1].end);
        DOLOG(2, LOG_VMAREAS, {
            LOG(GLOBAL, LOG_INTERP | LOG_VMAREAS, 2, "segment list\n");
            for (i = 0; i < ma->os_data.num_segments; i++) {
                LOG(GLOBAL, LOG_INTERP | LOG_VMAREAS, 2,
                    "\tsegment %d: [" PFX "," PFX ") prot=%x\n", i,
                    ma->os_data.segments[i].start, ma->os_data.segments[i].end,
                    ma->os_data.segments[i].prot);
            }
        });
        /* update to max end (view_size may just be 1st segment end) */
        ma->end = ma->os_data.segments[ma->os_data.num_segments - 1].end;
    }

#    ifdef LINUX
    LOG(GLOBAL, LOG_SYMBOLS, 2,
        "%s: hashtab=" PFX ", dynsym=" PFX ", dynstr=" PFX ", strsz=" SZFMT
        ", symsz=" SZFMT "\n",
        __func__, ma->os_data.hashtab, ma->os_data.dynsym, ma->os_data.dynstr,
        ma->os_data.dynstr_size, ma->os_data.symentry_size);
#    endif

#    ifdef LINUX /* on Mac the entire dyld shared cache has split __TEXT and __DATA */
    /* expect to map whole module */
    /* XREF 307599 on rounding module end to the next PAGE boundary */
    ASSERT_CURIOSITY(mod_end - mod_base == at_map ? ALIGN_FORWARD(view_size, PAGE_SIZE)
                                                  : view_size);
#    endif

    ma->os_data.base_address = mod_base;
    load_delta = base - mod_base;

    ma->entry_point = module_entry_point(base, load_delta);

    /* names - note os.c callers don't distinguish between no filename and an empty
     * filename, we treat both as NULL, but leave the distinction for SONAME. */
    if (filepath == NULL || filepath[0] == '\0') {
        ma->names.file_name = NULL;
#    ifdef VMX86_SERVER
        /* XXX: provide a targeted query to avoid full walk */
        void *iter = vmk_mmaps_iter_start();
        if (iter != NULL) { /* backward compatibility: support lack of iter */
            byte *start;
            size_t length;
            char name[MAXIMUM_PATH];
            while (vmk_mmaps_iter_next(iter, &start, &length, NULL, name,
                                       BUFFER_SIZE_ELEMENTS(name))) {
                if (base == start) {
                    ma->names.file_name = dr_strdup(name HEAPACCT(which));
                    break;
                }
            }
            vmk_mmaps_iter_stop(iter);
        }
#    endif
        ma->full_path = NULL;
    } else {
        ma->names.file_name = dr_strdup(get_short_name(filepath) HEAPACCT(which));
        /* We could share alloc w/ names.file_name but simpler to separate */
        ma->full_path = dr_strdup(filepath HEAPACCT(which));
    }
    ma->names.inode = inode;
    if (soname == NULL)
        ma->names.module_name = NULL;
    else
        ma->names.module_name = dr_strdup(soname HEAPACCT(which));

    /* Fields for pcaches (PR 295534).  These entries are not present in
     * all libs: I see DT_CHECKSUM and the prelink field on FC12 but not
     * on Ubuntu 9.04.
     */
    if (ma->os_data.checksum == 0 &&
        (DYNAMO_OPTION(coarse_enable_freeze) || DYNAMO_OPTION(use_persisted))) {
        /* Use something so we have usable pcache names */
        ma->os_data.checksum = d_r_crc32((const char *)ma->start, PAGE_SIZE);
    }
    /* Timestamp we just leave as 0 */

#    ifdef LINUX
    rseq_module_init(ma, at_map);
#    endif
}

void
free_module_names(module_names_t *mod_names HEAPACCT(which_heap_t which))
{
    ASSERT(mod_names != NULL);

    if (mod_names->module_name != NULL)
        dr_strfree(mod_names->module_name HEAPACCT(which));
    if (mod_names->file_name != NULL)
        dr_strfree(mod_names->file_name HEAPACCT(which));
}

void
module_copy_os_data(os_module_data_t *dst, os_module_data_t *src)
{
    memcpy(dst, src, sizeof(*dst));
    if (src->segments != NULL) {
        dst->segments = (module_segment_t *)HEAP_ARRAY_ALLOC(
            GLOBAL_DCONTEXT, module_segment_t, src->alloc_segments, ACCT_OTHER,
            PROTECTED);
        memcpy(dst->segments, src->segments,
               src->num_segments * sizeof(module_segment_t));
    }
}

void
print_modules(file_t f, bool dump_xml)
{
    module_iterator_t *mi;

    /* we walk our own module list that is populated on an initial walk through memory,
     * and further kept consistent on memory mappings of likely modules */
    print_file(f, dump_xml ? "<loaded-modules>\n" : "\nLoaded modules:\n");

    mi = module_iterator_start();
    while (module_iterator_hasnext(mi)) {
        module_area_t *ma = module_iterator_next(mi);
        print_file(f,
                   dump_xml ? "\t<so range=\"" PFX "-" PFX "\" "
                              "entry=\"" PFX "\" base_address=" PFX "\n"
                              "\tname=\"%s\" />\n"
                            : "  " PFX "-" PFX " entry=" PFX " base_address=" PFX "\n"
                              "\tname=\"%s\" \n",
                   ma->start, ma->end - 1, /* inclusive */
                   ma->entry_point, ma->os_data.base_address,
                   GET_MODULE_NAME(&ma->names) == NULL ? "(null)"
                                                       : GET_MODULE_NAME(&ma->names));
    }
    module_iterator_stop(mi);

    if (dump_xml)
        print_file(f, "</loaded-modules>\n");
    else
        print_file(f, "\n");
}

void
os_module_area_reset(module_area_t *ma HEAPACCT(which_heap_t which))
{
    if (ma->os_data.contiguous)
        module_list_remove_mapping(ma, ma->start, ma->end);
    else {
        /* Remove the non-contiguous segments (i#160/PR 562667) */
        app_pc seg_base;
        uint i;
        ASSERT(ma->os_data.num_segments > 0 && ma->os_data.segments != NULL);
        seg_base = ma->os_data.segments[0].start;
        for (i = 1; i < ma->os_data.num_segments; i++) {
            if (ma->os_data.segments[i].start > ma->os_data.segments[i - 1].end) {
                module_list_remove_mapping(ma, seg_base, ma->os_data.segments[i - 1].end);
                seg_base = ma->os_data.segments[i].start;
            }
        }
        module_list_remove_mapping(ma, seg_base, ma->os_data.segments[i - 1].end);
    }
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, ma->os_data.segments, module_segment_t,
                    ma->os_data.alloc_segments, ACCT_OTHER, PROTECTED);
    if (ma->full_path != NULL)
        dr_strfree(ma->full_path HEAPACCT(which));
}

/* Returns the bounds of the first section with matching name. */
bool
get_named_section_bounds(app_pc module_base, const char *name, app_pc *start /*OUT*/,
                         app_pc *end /*OUT*/)
{
    /* FIXME: not implemented */
    ASSERT(module_is_header(module_base, 0));
    if (start != NULL)
        *start = NULL;
    if (end != NULL)
        *end = NULL;
    return false;
}

bool
rct_is_exported_function(app_pc tag)
{
    /* FIXME: not implemented */
    return false;
}

/* FIXME PR 295529: NYI, here so code origins policies aren't all ifdef WINDOWS */
const char *
get_module_short_name(app_pc pc HEAPACCT(which_heap_t which))
{
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

/* FIXME PR 295529: NYI, here so moduledb code isn't all ifdef WINDOWS */
bool
get_module_company_name(app_pc mod_base, char *out_buf, size_t out_buf_size)
{
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

app_pc
get_module_base(app_pc pc)
{
    app_pc base = NULL;
    module_area_t *ma;
    os_get_module_info_lock();
    ma = module_pc_lookup(pc);
    if (ma != NULL)
        base = ma->start;
    os_get_module_info_unlock();
    return base;
}

/* FIXME PR 212458: NYI, here so code origins policies aren't all ifdef WINDOWS */
bool
is_range_in_code_section(app_pc module_base, app_pc start_pc, app_pc end_pc,
                         app_pc *sec_start /* OUT */, app_pc *sec_end /* OUT */)
{
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

/* FIXME PR 212458: NYI, here so code origins policies aren't all ifdef WINDOWS */
bool
is_in_code_section(app_pc module_base, app_pc addr, app_pc *sec_start /* OUT */,
                   app_pc *sec_end /* OUT */)
{
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

/* FIXME PR 212458: NYI, here so code origins policies aren't all ifdef WINDOWS */
bool
is_in_dot_data_section(app_pc module_base, app_pc addr, app_pc *sec_start /* OUT */,
                       app_pc *sec_end /* OUT */)
{
    return false;
    ASSERT_NOT_IMPLEMENTED(false);
}

/* FIXME PR 212458: NYI, here so code origins policies aren't all ifdef WINDOWS */
bool
is_in_any_section(app_pc module_base, app_pc addr, app_pc *sec_start /* OUT */,
                  app_pc *sec_end /* OUT */)
{
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

bool
is_mapped_as_image(app_pc module_base)
{
    return module_is_header(module_base, 0);
}

/* Gets module information of module containing pc, cached from our module list.
 * Returns false if not in module; none of the OUT arguments are set in that case.
 *
 * FIXME: share code w/ win32/module.c's os_get_module_info()
 *
 * Note: this function returns only one module name using the rule established
 * by GET_MODULE_NAME(); for getting all possible ones use
 * os_get_module_info_all_names() directly.  Part of fix for case 9842.
 *
 * If name != NULL, caller must acquire the module_data_lock beforehand
 * and call os_get_module_info_unlock() when finished with the name
 * (caller can use dr_strdup to make a copy first if necessary; validity of the
 * name is guaranteed only as long as the caller holds module_data_lock).
 * If name == NULL, this routine acquires and releases the lock and the
 * caller has no obligations.
 */
bool
os_get_module_info(const app_pc pc, uint *checksum, uint *timestamp, size_t *size,
                   const char **name, size_t *code_size, uint64 *file_version)
{
    module_area_t *ma;
    if (!is_module_list_initialized())
        return false;

    /* read lock to protect custom data */
    if (name == NULL)
        os_get_module_info_lock();
    ASSERT(os_get_module_info_locked());
    ;

    ma = module_pc_lookup(pc);
    if (ma != NULL) {
        if (checksum != NULL)
            *checksum = ma->os_data.checksum;
        if (timestamp != NULL)
            *timestamp = ma->os_data.timestamp;
        if (size != NULL)
            *size = ma->end - ma->start;
        if (name != NULL)
            *name = GET_MODULE_NAME(&ma->names);
        if (code_size != NULL) {
            /* Using rx segment size since don't want to impl section
             * iterator (i#76/PR 212458)
             */
            uint i;
            size_t rx_sz = 0;
            ASSERT(ma->os_data.num_segments > 0 && ma->os_data.segments != NULL);
            for (i = 0; i < ma->os_data.num_segments; i++) {
                if (ma->os_data.segments[i].prot == (MEMPROT_EXEC | MEMPROT_READ)) {
                    rx_sz = ma->os_data.segments[i].end - ma->os_data.segments[i].start;
                    break;
                }
            }
            *code_size = rx_sz;
        }
        if (file_version != NULL) {
            /* FIXME: NYI: make windows-only everywhere if no good linux source */
            *file_version = 0;
        }
    }

    if (name == NULL)
        os_get_module_info_unlock();
    return (ma != NULL);
}

bool
os_get_module_info_all_names(const app_pc pc, uint *checksum, uint *timestamp,
                             size_t *size, module_names_t **names, size_t *code_size,
                             uint64 *file_version)
{
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

#    if defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH)
extern rct_module_table_t rct_global_table;

/* Caller must hold module_data_lock */
rct_module_table_t *
os_module_get_rct_htable(app_pc pc, rct_type_t which)
{
    /* FIXME: until we have a module list we use global rct and rac tables */
    if (which == RCT_RCT)
        return &rct_global_table;
    return NULL; /* we use rac_non_module_table */
}
#    endif

/* Adds an entry for a segment to the out_data->segments array */
void
module_add_segment_data(OUT os_module_data_t *out_data, uint num_segments /*hint only*/,
                        app_pc segment_start, size_t segment_size,
                        uint segment_prot, /* MEMPROT_ */
                        size_t alignment, bool shared, uint64 offset)
{
    uint seg, i;
    LOG(GLOBAL, LOG_INTERP | LOG_VMAREAS, 3, "%s: #=%d " PFX "-" PFX " 0x%x\n",
        __FUNCTION__, out_data->num_segments, segment_start, segment_start + segment_size,
        segment_prot);
    if (out_data->alignment == 0) {
        out_data->alignment = alignment;
    } else {
        /* We expect all segments to have the same alignment for ELF. */
        IF_LINUX(ASSERT_CURIOSITY(out_data->alignment == alignment));
    }
    /* Add segments to the module vector (i#160/PR 562667).
     * For !HAVE_MEMINFO we should combine w/ the segment
     * walk done in dl_iterate_get_areas_cb().
     */
    if (out_data->num_segments + 1 >= out_data->alloc_segments) {
        /* over-allocate to avoid 2 passes to count PT_LOAD */
        uint newsz;
        module_segment_t *newmem;
        if (out_data->alloc_segments == 0)
            newsz = (num_segments == 0 ? 4 : num_segments);
        else
            newsz = out_data->alloc_segments * 2;
        newmem = (module_segment_t *)HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, module_segment_t,
                                                      newsz, ACCT_OTHER, PROTECTED);
        if (out_data->alloc_segments > 0) {
            memcpy(newmem, out_data->segments,
                   out_data->alloc_segments * sizeof(*out_data->segments));
            HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, out_data->segments, module_segment_t,
                            out_data->alloc_segments, ACCT_OTHER, PROTECTED);
        }
        out_data->segments = newmem;
        out_data->alloc_segments = newsz;
        out_data->contiguous = true;
    }
    /* Keep array sorted in addr order.  I'm assuming segments are disjoint! */
    for (i = 0; i < out_data->num_segments; i++) {
        if (out_data->segments[i].start > segment_start)
            break;
    }
    seg = i;
    /* Shift remaining entries */
    for (i = out_data->num_segments; i > seg; i--) {
        out_data->segments[i] = out_data->segments[i - 1];
    }
    out_data->num_segments++;
    ASSERT(out_data->num_segments <= out_data->alloc_segments);
#    ifdef MACOS
    /* Some libraries have sub-page segments so do not page-align.  We assume
     * these are already aligned.
     */
    out_data->segments[seg].start = segment_start;
    out_data->segments[seg].end = segment_start + segment_size;
#    else
    /* ELF requires p_vaddr to already be aligned to p_align */
    out_data->segments[seg].start = (app_pc)ALIGN_BACKWARD(segment_start, PAGE_SIZE);
    out_data->segments[seg].end =
        (app_pc)ALIGN_FORWARD(segment_start + segment_size, PAGE_SIZE);
#    endif
    out_data->segments[seg].prot = segment_prot;
    out_data->segments[seg].shared = shared;
    out_data->segments[seg].offset = offset;
    if (seg > 0) {
        ASSERT(out_data->segments[seg].start >= out_data->segments[seg - 1].end);
        if (out_data->segments[seg].start > out_data->segments[seg - 1].end)
            out_data->contiguous = false;
    }
    if (seg < out_data->num_segments - 1) {
        ASSERT(out_data->segments[seg + 1].start >= out_data->segments[seg].end);
        if (out_data->segments[seg + 1].start > out_data->segments[seg].end)
            out_data->contiguous = false;
    }
}

/* Returns true if the module has an nth segment, false otherwise. */
bool
module_get_nth_segment(app_pc module_base, uint n, app_pc *start /*OPTIONAL OUT*/,
                       app_pc *end /*OPTIONAL OUT*/, uint *chars /*OPTIONAL OUT*/)
{
    module_area_t *ma;
    bool res = false;
    if (!is_module_list_initialized())
        return false;
    os_get_module_info_lock();
    ma = module_pc_lookup(module_base);
    if (ma != NULL && n < ma->os_data.num_segments) {
        LOG(GLOBAL, LOG_INTERP | LOG_VMAREAS, 3, "%s: [" PFX "-" PFX ") %x\n",
            __FUNCTION__, ma->os_data.segments[n].start, ma->os_data.segments[n].end,
            ma->os_data.segments[n].prot);
        if (start != NULL)
            *start = ma->os_data.segments[n].start;
        if (end != NULL)
            *end = ma->os_data.segments[n].end;
        if (chars != NULL)
            *chars = ma->os_data.segments[n].prot;
        res = true;
    }
    os_get_module_info_unlock();
    return res;
}

#endif /* !NOT_DYNAMORIO_CORE_PROPER */

/* XXX: We could implement import iteration of PE files in Wine, so we provide
 * these stubs.
 */
dr_module_import_iterator_t *
dr_module_import_iterator_start(module_handle_t handle)
{
    CLIENT_ASSERT(false,
                  "No imports on Linux, use "
                  "dr_symbol_import_iterator_t instead");
    return NULL;
}

bool
dr_module_import_iterator_hasnext(dr_module_import_iterator_t *iter)
{
    return false;
}

dr_module_import_t *
dr_module_import_iterator_next(dr_module_import_iterator_t *iter)
{
    return NULL;
}

void
dr_module_import_iterator_stop(dr_module_import_iterator_t *iter)
{
}

bool
at_dl_runtime_resolve_ret(dcontext_t *dcontext, app_pc source_fragment, int *ret_imm)
{
    /* source_fragment is the start pc of the fragment to be run under DR
     * offset is the location of the address dl_runtime_resolve
     * will return to, relative to xsp
     */

    /* It works for the UNIX loader hack in  _dl_runtime_resolve */
    /* The offending sequence in ld-linux.so is
     * <_dl_runtime_resolve>:
     * c270: 5a                      pop    %edx
     * c271: 59                      pop    %ecx
     * c272: 87 04 24                xchg   %eax,(%esp)
     * c275: c2 08 00                ret    $0x8
     */
    /* The same code also is in 0000c280 <_dl_runtime_profile>
     * It maybe that either one or the other is ever used.
     * Although performancewise this pattern matching is very cheap,
     * for stricter security we assume only one is used in a session.
     */
    /* FIXME: This may change with future versions of libc, tested on
     * RH8 and RH9 only.  Also works for whatever libc was in ubuntu 7.10.
     */
    /* However it does not work for ubuntu 8.04 where the code sequence has
     * changed to the still similar :
     * 2c50:  5a                   pop    %edx
     * 2c51:  8b 0c 24             mov    (%esp) -> %ecx
     * 2c54:  89 04 24             mov    %eax -> (%esp)
     * 2c57:  8b 44 24 04          mov    0x04(%esp) -> %eax
     * 2c5b:  c2 0c 00             ret    $0xc
     * So we check for that sequence too.
     */
    static const byte DL_RUNTIME_RESOLVE_MAGIC_1[8] =
        /* pop edx, pop ecx; xchg eax, (esp) ret 8 */
        { 0x5a, 0x59, 0x87, 0x04, 0x24, 0xc2, 0x08, 0x00 };
    static const byte DL_RUNTIME_RESOLVE_MAGIC_2[14] =
        /* pop edx, mov (esp)->ecx, mov eax->(esp), mov 4(esp)->eax, ret 12 */
        { 0x5a, 0x8b, 0x0c, 0x24, 0x89, 0x04, 0x24,
          0x8b, 0x44, 0x24, 0x04, 0xc2, 0x0c, 0x00 };
    byte buf[MAX(sizeof(DL_RUNTIME_RESOLVE_MAGIC_1),
                 sizeof(DL_RUNTIME_RESOLVE_MAGIC_2))] = { 0 };

    if (d_r_safe_read(source_fragment, sizeof(DL_RUNTIME_RESOLVE_MAGIC_1), buf) &&
        memcmp(buf, DL_RUNTIME_RESOLVE_MAGIC_1, sizeof(DL_RUNTIME_RESOLVE_MAGIC_1)) ==
            0) {
        *ret_imm = 0x8;
        return true;
    }
    if (d_r_safe_read(source_fragment, sizeof(DL_RUNTIME_RESOLVE_MAGIC_2), buf) &&
        memcmp(buf, DL_RUNTIME_RESOLVE_MAGIC_2, sizeof(DL_RUNTIME_RESOLVE_MAGIC_2)) ==
            0) {
        LOG(THREAD, LOG_INTERP, 1,
            "RCT: KNOWN exception this is "
            "_dl_runtime_resolve --ok \n");
        *ret_imm = 0xc;
        return true;
    }
    return false;
}

bool
module_file_is_module64(file_t f, bool *is64 OUT, bool *also32 OUT)
{
    dr_platform_t platform, alt_platform;
    if (module_get_platform(f, &platform, &alt_platform)) {
        *is64 = (platform == DR_PLATFORM_64BIT);
        if (also32 != NULL)
            *also32 = (platform == DR_PLATFORM_32BIT);
        return true;
    }
    return false;
}

bool
module_contains_addr(module_area_t *ma, app_pc pc)
{
    if (ma->os_data.contiguous)
        return (pc >= ma->start && pc < ma->end);
    else {
        uint i;
        ASSERT(ma->os_data.num_segments > 0 && ma->os_data.segments != NULL);
        for (i = 0; i < ma->os_data.num_segments; i++) {
            if (pc >= ma->os_data.segments[i].start && pc < ma->os_data.segments[i].end)
                return true;
        }
    }
    return false;
}
