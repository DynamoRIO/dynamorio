/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
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

#include "globals.h"
#include "instrument.h"
#include "native_exec.h"
#ifdef WINDOWS
#    include "ntdll.h" /* for protect_virtual_memory */
#endif

/* Used for maintaining our module list.  Custom field points to
 * further module information from PE/ELF headers.
 * module_data_lock needs to be held when accessing the custom data fields.
 * Kept on the heap for selfprot (case 7957).
 * For Linux this is a vector of segments to handle non-contiguous
 * modules (i#160/PR 562667).
 */
vm_area_vector_t *loaded_module_areas;

/* To avoid breaking further the abstraction of vm_area_vector_t's
 * currently grabbing a separate lock.  In addition to protecting each
 * entry's data, this lock also makes atomic a lookup & remove or a
 * lookup & add sequences.  LOOKUP is read and user can use any
 * fields, REMOVE is a write and nobody should be able to lookup a
 * custom data that is going to get removed, ADD is a write only to
 * avoid a memory leak of readding a module.
 */
DECLARE_CXTSWPROT_VAR(read_write_lock_t module_data_lock,
                      INIT_READWRITE_LOCK(module_data_lock));

/**************** module_data_lock routines *****************/

void
os_get_module_info_lock(void)
{
    if (loaded_module_areas != NULL)
        d_r_read_lock(&module_data_lock);
    /* else we assume past exit: FIXME: best to have exited bool */
}

void
os_get_module_info_unlock(void)
{
    if (loaded_module_areas != NULL) {
        ASSERT_OWN_READ_LOCK(true, &module_data_lock);
        d_r_read_unlock(&module_data_lock);
    }
}

void
os_get_module_info_write_lock(void)
{
    if (loaded_module_areas != NULL)
        d_r_write_lock(&module_data_lock);
    /* else we assume past exit: FIXME: best to have exited bool */
}

void
os_get_module_info_write_unlock(void)
{
    if (loaded_module_areas != NULL)
        d_r_write_unlock(&module_data_lock);
    /* else we assume past exit: FIXME: best to have exited bool */
}

bool
os_get_module_info_locked(void)
{
    if (loaded_module_areas != NULL)
        return READWRITE_LOCK_HELD(&module_data_lock);
    return false;
}

bool
os_get_module_info_write_locked(void)
{
    if (loaded_module_areas != NULL)
        return self_owns_write_lock(&module_data_lock);
    return false;
}

/**************** module_area routines *****************/

/* view_size can be the size of the first mapping, to handle non-contiguous
 * modules -- we'll update the module's size in os_module_area_init()
 */
static module_area_t *
module_area_create(app_pc base, size_t view_size, bool at_map,
                   const char *filepath _IF_UNIX(uint64 inode))
{
    module_area_t *ma =
        HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, module_area_t, ACCT_VMAREAS, PROTECTED);
    memset(ma, 0, sizeof(*ma));
    ma->start = base;
    ma->end = base + view_size; /* updated in os_module_area_init () */
    os_module_area_init(ma, base, view_size, at_map,
                        filepath _IF_UNIX(inode) HEAPACCT(ACCT_VMAREAS));
    return ma;
}

static void
module_area_delete(module_area_t *ma)
{
    os_module_area_reset(ma HEAPACCT(ACCT_VMAREAS));
    free_module_names(&ma->names HEAPACCT(ACCT_VMAREAS));
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, ma, module_area_t, ACCT_VMAREAS, PROTECTED);
}

/**************** init/exit routines *****************/

void
modules_init(void)
{
    VMVECTOR_ALLOC_VECTOR(loaded_module_areas, GLOBAL_DCONTEXT,
                          VECTOR_SHARED |
                              VECTOR_NEVER_MERGE
                              /* case 10335: we always use module_data_lock */
                              | VECTOR_NO_LOCK,
                          loaded_module_areas);
    os_modules_init();
}

bool
is_module_list_initialized(void)
{
    return loaded_module_areas != NULL;
}

void
modules_reset_list(void)
{
    vmvector_iterator_t vmvi;
    /* need to free each entry */
    os_get_module_info_write_lock();
    /* note our iterator doesn't support remove,
     * anyways we need to free all entries here */
    vmvector_iterator_start(loaded_module_areas, &vmvi);
    while (vmvector_iterator_hasnext(&vmvi)) {
        app_pc start, end;
        module_area_t *ma = (module_area_t *)vmvector_iterator_next(&vmvi, &start, &end);
        ASSERT(ma != NULL);
#ifdef WINDOWS
        ASSERT(ma->start == start && ma->end == end);
#else
        ASSERT(ma->start <= start && ma->end >= end);
        /* ignore all but the first segment */
        if (ma->start != start)
            continue;
#endif
        ma->flags |= MODULE_BEING_UNLOADED;
        module_area_delete(ma);
        /* we've removed from the vector so we must reset the iterator */
        vmvector_iterator_startover(&vmvi);
    }
    vmvector_iterator_stop(&vmvi);
    vmvector_reset_vector(GLOBAL_DCONTEXT, loaded_module_areas);
    os_get_module_info_write_unlock();
}

void
modules_exit(void)
{
    LOG(GLOBAL, LOG_VMAREAS, 2, "Module list at exit\n");
    DOLOG(2, LOG_VMAREAS, { print_modules(GLOBAL, DUMP_NOT_XML); });

    os_modules_exit();

    modules_reset_list();
    vmvector_delete_vector(GLOBAL_DCONTEXT, loaded_module_areas);
    loaded_module_areas = NULL;
    DELETE_READWRITE_LOCK(module_data_lock);
}

/**************** module_list updating routines *****************/

/* Can only be called from os_module_area_init() called from module_list_add(),
 * which holds the module lock
 */
void
module_list_add_mapping(module_area_t *ma, app_pc map_start, app_pc map_end)
{
    /* note that there is normally no need to hold even a
     * read_lock to make sure that nobody is about to remove
     * this entry.  While next to impossible that the
     * currently added module will get unloaded by another
     * thread, we do grab a full write lock around this safe
     * lookup/add.
     */
    ASSERT(os_get_module_info_write_locked());
    vmvector_add(loaded_module_areas, map_start, map_end, ma);
    LOG(GLOBAL, LOG_INTERP | LOG_VMAREAS, 2,
        "\tmodule %s segment [" PFX "," PFX "] added\n",
        (GET_MODULE_NAME(&ma->names) == NULL) ? "<no name>" : GET_MODULE_NAME(&ma->names),
        map_start, map_end);
}

/* Can only be called from os_module_area_reset() called from module_list_remove(),
 * which holds the module lock
 */
void
module_list_remove_mapping(module_area_t *ma, app_pc map_start, app_pc map_end)
{
    ASSERT(os_get_module_info_write_locked());
    vmvector_remove(loaded_module_areas, map_start, map_end);
    LOG(GLOBAL, LOG_INTERP | LOG_VMAREAS, 2,
        "\tmodule %s %s segment [" PFX "," PFX "] removed\n",
        ma->full_path == NULL ? "<no path>" : ma->full_path,
        (GET_MODULE_NAME(&ma->names) == NULL) ? "<no name>" : GET_MODULE_NAME(&ma->names),
        map_start, map_end);
}

/* view_size can be the size of the first mapping, to handle non-contiguous
 * modules -- we'll update the module's size in os_module_area_init()
 */
void
module_list_add(app_pc base, size_t view_size, bool at_map,
                const char *filepath _IF_UNIX(uint64 inode))
{
    ASSERT(loaded_module_areas != NULL);
    ASSERT(!vmvector_overlap(loaded_module_areas, base, base + view_size));
    os_get_module_info_write_lock();
    /* defensively checking */
    if (!vmvector_overlap(loaded_module_areas, base, base + view_size)) {
        /* module_area_create() calls os_module_area_init() which calls
         * module_list_add_mapping() to add the module's mappings to
         * the loaded_module_areas vector, to support non-contiguous
         * modules (i#160/PR 562667)
         */
        module_area_t *ma =
            module_area_create(base, view_size, at_map, filepath _IF_UNIX(inode));
        ASSERT(ma != NULL);

        LOG(GLOBAL, LOG_INTERP | LOG_VMAREAS, 1,
            "module %s %s |%s| [" PFX "," PFX "] added\n",
            ma->full_path == NULL ? "<no path>" : ma->full_path,
            (GET_MODULE_NAME(&ma->names) == NULL) ? "<no name>"
                                                  : GET_MODULE_NAME(&ma->names),
            ma->names.file_name == NULL ? "<no file>" : ma->names.file_name, base,
            base + view_size);

        /* note that while it would be natural to invoke the client module
         * load event since we have the data for it right here, the
         * module has not been processed for executable areas yet by DR,
         * which can cause problems if the client calls dr_memory_protect()
         * or other routines: so we delay and invoke the client event only
         * when DR's module state is consistent
         */

        native_exec_module_load(ma, at_map);
    } else {
        /* already added! */
        /* only possible for manual NtMapViewOfSection, loader
         * can't be doing this to us */
        ASSERT_CURIOSITY(false && "image load race");
        /* do nothing */
    }
    os_get_module_info_write_unlock();
}

void
module_list_remove(app_pc base, size_t view_size)
{
    /* lookup and free module */
    module_data_t *client_data = NULL;
    bool inform_client = false;
    module_area_t *ma;

    /* note that vmvector_lookup doesn't protect the custom data,
     * and we need to bracket a lookup and remove in an unlikely
     * application race (note we pre-process unmap)
     */
    ASSERT(loaded_module_areas != NULL);
    os_get_module_info_write_lock();
    ASSERT(vmvector_overlap(loaded_module_areas, base, base + view_size));
    ma = (module_area_t *)vmvector_lookup(loaded_module_areas, base);
    ASSERT_CURIOSITY(ma != NULL); /* loader can't have a race */

    LOG(GLOBAL, LOG_INTERP | LOG_VMAREAS, 2, "module_list_remove %s\n",
        (GET_MODULE_NAME(&ma->names) == NULL) ? "<no name>"
                                              : GET_MODULE_NAME(&ma->names));

    /* inform clients of module unloads, we copy the data now and wait to
     * call the client till after we've released the module areas lock */
    if (CLIENTS_EXIST()
        /* don't notify for drearlyhelper* or other during-init modules */
        && dynamo_initialized
        /* don't notify for modules that were not executed (i#884) */
        && TEST(MODULE_LOAD_EVENT, ma->flags)) {
        client_data = copy_module_area_to_module_data(ma);
        inform_client = true;
    }
    os_get_module_info_write_unlock();
    if (inform_client) {
        instrument_module_unload(client_data);
        dr_free_module_data(client_data);
    }
    os_get_module_info_write_lock();
    ma = (module_area_t *)vmvector_lookup(loaded_module_areas, base);
    ASSERT_CURIOSITY(ma != NULL); /* loader can't have a race */

    native_exec_module_unload(ma);

    /* defensively checking */
    if (ma != NULL) {
        /* os_module_area_reset() calls module_list_remove_mapping() to
         * remove the segments from the vector
         */
        module_area_delete(ma);
    }
    ASSERT(!vmvector_overlap(loaded_module_areas, base, base + view_size));
    os_get_module_info_write_unlock();
}

/**************** module flag routines *****************/

static bool
os_module_set_flag_value(app_pc module_base, uint flag, bool set)
{
    module_area_t *ma;
    bool found = false;
    bool own_lock = os_get_module_info_write_locked();
    if (!own_lock)
        os_get_module_info_write_lock();
    ma = module_pc_lookup(module_base);
    if (ma != NULL) {
        if (set)
            ma->flags |= flag;
        else
            ma->flags &= ~flag;
        found = true;
    }
    if (!own_lock)
        os_get_module_info_write_unlock();
    return found;
}

bool
os_module_set_flag(app_pc module_base, uint flag)
{
    return os_module_set_flag_value(module_base, flag, true);
}

bool
os_module_clear_flag(app_pc module_base, uint flag)
{
    return os_module_set_flag_value(module_base, flag, false);
}

bool
os_module_get_flag(app_pc module_base, uint flag)
{
    module_area_t *ma;
    bool has_flag = false;
    os_get_module_info_lock();
    ma = module_pc_lookup(module_base);
    if (ma != NULL) {
        /* interface is for just one flag so no documentation of ANY vs ALL */
        has_flag = TESTANY(flag, ma->flags);
    }
    os_get_module_info_unlock();
    return has_flag;
}

bool
pc_is_in_module(byte *pc)
{
    module_area_t *ma;
    bool in_module = false;
    os_get_module_info_lock();
    ma = module_pc_lookup(pc);
    if (ma != NULL)
        in_module = true;
    os_get_module_info_unlock();
    return in_module;
}

/**************** module_area accessor routines (os shared) *****************/

/* Returns the module_area_ for the module containing pc (NULL if no such module is found)
 */
module_area_t *
module_pc_lookup(byte *pc)
{
    ASSERT(loaded_module_areas != NULL);
    ASSERT(os_get_module_info_locked());
    return (module_area_t *)vmvector_lookup(loaded_module_areas, pc);
}

/* Returns true if the region overlaps any module areas. */
bool
module_overlaps(byte *pc, size_t len)
{
    ASSERT(loaded_module_areas != NULL);
    ASSERT(os_get_module_info_locked());
    return vmvector_overlap(loaded_module_areas, pc, pc + len);
}

/* Some callers want strdup, some want a passed-in buffer, and some want
 * a buffer but if it's too small they then want strdup.
 */
static const char *
os_get_module_name_internal(const app_pc pc, char *buf, size_t buf_len, bool truncate,
                            size_t *copied HEAPACCT(which_heap_t which))
{
    const char *name = NULL;
    size_t num = 0;
    os_get_module_info_lock();
    if (os_get_module_name(pc, &name) && name != NULL) {
        if (buf == NULL || (!truncate && strlen(name) >= buf_len)) {
            DOSTATS({
                if (buf != NULL)
                    STATS_INC(app_modname_too_long);
            });
            name = dr_strdup(name HEAPACCT(which));
        } else {
            strncpy(buf, name, buf_len);
            buf[buf_len - 1] = '\0';
            num = MIN(buf_len - 1, strlen(name));
            name = buf;
        }
    } else if (buf != NULL)
        buf[0] = '\0';
    os_get_module_info_unlock();
    if (copied != NULL)
        *copied = num;
    return name;
}

/* Convenience wrapper so we don't have to remember arg position of name
 * in os_get_module_info().  Caller must hold module_data_lock.
 * Unlike os_get_module_info(), sets *name to NULL if return value is false;
 */
bool
os_get_module_name(const app_pc pc, /* OUT */ const char **name)
{
    module_area_t *ma;

    ASSERT(os_get_module_info_locked());
    ma = module_pc_lookup(pc);

    if (ma != NULL)
        *name = GET_MODULE_NAME(&ma->names);
    else
        *name = NULL;
    return ma != NULL;
}

const char *
os_get_module_name_strdup(const app_pc pc HEAPACCT(which_heap_t which))
{
    return os_get_module_name_internal(pc, NULL, 0, false /*no truncate*/,
                                       NULL HEAPACCT(which));
}

/* Returns the number of characters copied (maximum is buf_len -1).
 * If there is no module at pc, or no module name available, 0 is
 * returned and the buffer set to "".
 */
size_t
os_get_module_name_buf(const app_pc pc, char *buf, size_t buf_len)
{
    size_t copied;
    os_get_module_name_internal(pc, buf, buf_len, true /*truncate*/,
                                &copied HEAPACCT(ACCT_OTHER));
    return copied;
}

/* Copies the module name into buf and returns a pointer to buf,
 * unless buf is too small, in which case the module name is strdup-ed
 * and a pointer to it returned (which the caller must strfree).
 * If there is no module name, returns NULL.
 */
const char *
os_get_module_name_buf_strdup(const app_pc pc, char *buf,
                              size_t buf_len HEAPACCT(which_heap_t which))
{
    return os_get_module_name_internal(pc, buf, buf_len, false /*no truncate*/,
                                       NULL HEAPACCT(which));
}

size_t
os_module_get_view_size(app_pc mod_base)
{
    module_area_t *ma;
    size_t view_size = 0;
    os_get_module_info_lock();
    ma = module_pc_lookup(mod_base);
    if (ma != NULL) {
        view_size = ma->end - ma->start;
    }
    os_get_module_info_unlock();
    return view_size;
}

/**************** module iterator routines *****************/

struct _module_iterator_t {
    vmvector_iterator_t vmvi;
};

/* Initialize a new module_iterator */
module_iterator_t *
module_iterator_start(void)
{
    module_iterator_t *mi =
        HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, module_iterator_t, ACCT_OTHER, UNPROTECTED);
    ASSERT(loaded_module_areas != NULL);
    /* loaded_module_areas doesn't use the vector lock */
    os_get_module_info_lock();
    vmvector_iterator_start(loaded_module_areas, &mi->vmvi);
    return mi;
}

/* Returns true if there is another module in the list */
bool
module_iterator_hasnext(module_iterator_t *mi)
{
    app_pc start, end;
    module_area_t *ma;
    ASSERT(os_get_module_info_locked());
    while (vmvector_iterator_hasnext(&mi->vmvi)) {
        ma = (module_area_t *)vmvector_iterator_peek(&mi->vmvi, &start, &end);
        /* skip non-initial segments */
        if (start != ma->start)
            vmvector_iterator_next(&mi->vmvi, NULL, NULL);
        else
            return true;
    }
    return false;
}

/* Retrieves the module_data_t for a loaded module */
module_area_t *
module_iterator_next(module_iterator_t *mi)
{
    app_pc start, end;
    module_area_t *ma = (module_area_t *)vmvector_iterator_next(&mi->vmvi, &start, &end);
    ASSERT(os_get_module_info_locked());
    ASSERT(ma != NULL);
    ASSERT(ma->start == start && IF_WINDOWS_ELSE(ma->end == end, ma->end >= end));
    return ma;
}

/* User should call this routine to free the iterator */
void
module_iterator_stop(module_iterator_t *mi)
{
    vmvector_iterator_stop(&mi->vmvi);
    /* loaded_module_areas doesn't use the vector lock */
    ASSERT(os_get_module_info_locked());
    os_get_module_info_unlock();
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, mi, module_iterator_t, ACCT_OTHER, UNPROTECTED);
}

/**************** digest routines *****************/

/* add only the intersection of the two regions to the running MD5 sum */
static void
region_intersection_MD5update(struct MD5Context *ctx, app_pc region1_start,
                              size_t region1_len, app_pc region2_start,
                              size_t region2_len)
{
    app_pc intersection_start;
    size_t intersection_len;
    ASSERT(ctx != NULL);
    region_intersection(&intersection_start, &intersection_len, region1_start,
                        region1_len, region2_start, region2_len);
    if (intersection_len != 0) {
        LOG(GLOBAL, LOG_SYSCALLS, 2, "adding to short hash region " PFX "-" PFX "\n",
            intersection_start, intersection_start + intersection_len);
        d_r_md5_update(ctx, intersection_start, intersection_len);
    }
}

/* keeps track of both short and full digests on each region */
static void
module_calculate_digest_helper(struct MD5Context *md5_full_cxt /* OPTIONAL */,
                               struct MD5Context *md5_short_cxt /* OPTIONAL */,
                               app_pc region_start, size_t region_len,
                               app_pc start_header, size_t len_header,
                               app_pc start_footer, size_t len_footer)
{
    ASSERT(md5_full_cxt != NULL || md5_short_cxt != NULL);
    LOG(GLOBAL, LOG_VMAREAS, 2, "\t%s: segment " PFX "-" PFX "\n", __FUNCTION__,
        region_start, region_start + region_len);
    if (md5_full_cxt != NULL)
        d_r_md5_update(md5_full_cxt, region_start, region_len);
    if (md5_short_cxt == NULL)
        return;
    if (len_header != 0) {
        region_intersection_MD5update(md5_short_cxt, region_start, region_len,
                                      start_header, len_header);
    }
    if (len_footer != 0) {
        region_intersection_MD5update(md5_short_cxt, region_start, region_len,
                                      start_footer, len_footer);
    }
}

/* Verifies that according to section Characteristics its mapping is expected to be
 * readable (and if not VirtualProtects to makes it so).  NOTE this only operates on
 * the mapped portion of the section (via get_image_section_map_size()) which may be
 * smaller then the virtual size (get_image_section_size()) of the section (in which
 * case it was zero padded).
 *
 * Note this is NOT checking the current protection settings with
 * is_readable_without_exception(), so the actual current state may well vary.
 *
 * Returns true if no changes had to be made (the section is already readable).
 * Returns false if an unreadable section has been made readable (and the
 * caller should probably call restore_unreadable_section() afterward).
 */
static bool
ensure_section_readable(app_pc module_base, app_pc seg_start, size_t seg_len,
                        uint seg_chars, OUT uint *old_prot, app_pc view_start,
                        size_t view_len)
{
    app_pc intersection_start;
    size_t intersection_len;

    region_intersection(&intersection_start, &intersection_len, view_start, view_len,
                        seg_start, ALIGN_FORWARD(seg_len, PAGE_SIZE));
    if (intersection_len == 0)
        return true;

    /* on X86-32 as long as any of RWX is set the contents is readable */
    if (TESTANY(OS_IMAGE_EXECUTE | OS_IMAGE_READ | OS_IMAGE_WRITE, seg_chars)) {
        /* We're mid-load and on recent ld.so segments spanning a gap are mprotected
         * to noaccess *before* their contents are mapped.  The text segment of
         * interest should be mapped but we haven't yet updated allmem.
         * Thus we must query the OS.
         */
        ASSERT(
            is_readable_without_exception_query_os(intersection_start, intersection_len));
        return true;
    }
    /* such a mapping could potentially be used for some protection
     * scheme in which sections are made readable only on demand */

    /* Otherwise we just mark readable the raw bytes of the section,
     * NOTE: we'll leave readable, so only users of our private
     * mappings should use this function!
     */
    SYSLOG_INTERNAL_WARNING("unreadable section @" PFX "\n", seg_start);
#ifdef WINDOWS
    /* Preserve COW flags */
    DEBUG_DECLARE(bool ok =)
    protect_virtual_memory(intersection_start, intersection_len, PAGE_READONLY, old_prot);
    ASSERT(ok);
    ASSERT_CURIOSITY(*old_prot == PAGE_NOACCESS ||
                     *old_prot == PAGE_WRITECOPY); /* expecting unmodifed even
                                                    * if writable */
#else
    /* No other flags to preserve, should be no-access, so we ignore old_prot */
    DEBUG_DECLARE(bool ok =)
    os_set_protection(intersection_start, intersection_len, MEMPROT_READ);
    ASSERT(ok);
#endif
    return false;
}

static bool
restore_unreadable_section(app_pc module_base, app_pc seg_start, size_t seg_len,
                           uint seg_chars, uint restore_prot, app_pc view_start,
                           size_t view_len)
{
    bool ok;
    app_pc intersection_start;
    size_t intersection_len;
#ifdef WINDOWS
    uint old_prot;
#endif

    ASSERT(!TESTANY(OS_IMAGE_EXECUTE | OS_IMAGE_READ | OS_IMAGE_WRITE, seg_chars));

    region_intersection(&intersection_start, &intersection_len, view_start, view_len,
                        seg_start, ALIGN_FORWARD(seg_start + seg_len, PAGE_SIZE));
    if (intersection_len == 0)
        return true;

#ifdef WINDOWS
    /* Preserve COW flags */
    ok = protect_virtual_memory(intersection_start, intersection_len, restore_prot,
                                &old_prot);
    ASSERT(ok);
    ASSERT(old_prot == PAGE_READONLY);
#else
    /* No other flags to preserve so we ignore old_prot */
    ok = os_set_protection(intersection_start, intersection_len, MEMPROT_NONE);
    ASSERT(ok);
#endif

    return ok;
}

/* note it operates on a PE mapping so it can be passed either a
 * relocated or the original file.  Either full or short digest or both can be requested.
 * If short_digest is set the short version of the digest is
 * calculated and set.  Note that if short_digest_size crosses an
 * unreadable boundary it is truncated to the smallest consecutive
 * memory region from each of the header and the footer.  If
 * short_digest_size is 0 or larger than half of the file size the
 * short and full digests are supposed to be equal.
 * If sec_char_include != 0, only sections TESTANY matching those
 * characteristics (and the PE headers) are considered.
 * If sec_char_exclude != 0, only sections !TESTANY matching those
 * characteristics (and the PE headers) are considered.
 * It is the caller's responsibility to ensure that module_size is not
 * larger than the mapped view size.
 */
void
module_calculate_digest(OUT module_digest_t *digest, app_pc module_base,
                        size_t module_size, bool full_digest, bool short_digest,
                        size_t short_digest_size, uint sec_char_include,
                        uint sec_char_exclude)
{
    struct MD5Context md5_short_cxt;
    struct MD5Context md5_full_cxt;

    uint i;
    app_pc module_end = module_base + module_size;

    /* tentative starts */
    /* need to adjust these buffers in case of crossing unreadable areas,
     * or if overlapping
     */
    /* Note that simpler alternative would have been to only produce a
     * digest on the PE header (0x400), and maybe the last section.
     * However for better consistency guarantees, yet with a
     * predictable performance, used this more involved definition of
     * short digest.  While a 64KB digest may be acceptable, full
     * checks on some 8MB DLLs may be noticeable.
     */

    app_pc header_start = module_base;
    size_t header_len = MIN(module_size, short_digest_size);
    app_pc footer_start = module_end - short_digest_size;
    size_t footer_len;

    app_pc region_start;
    size_t region_len;

    ASSERT(digest != NULL);
    ASSERT(module_base != NULL);
    ASSERT(module_size != 0);

    LOG(GLOBAL, LOG_VMAREAS, 2, "module_calculate_digest: module " PFX "-" PFX "\n",
        module_base, module_base + module_size);

    if (short_digest_size == 0) {
        header_len = module_size;
    }

    footer_start = MAX(footer_start, header_start + header_len);
    footer_len = module_end - footer_start;
    /* footer region will be unused if footer_len is 0 - in case of
     * larger than file size short size, or if short_digest_size = 0
     * which also means unbounded */

    /* note that this function has significant overlap with
     * module_dump_pe_file(), and in fact we could avoid a second
     * traversal and associated cache pollution on producing a file if
     * we provide this functionality in module_dump_pe_file().  Of
     * course for verification we still need this separately
     */

    ASSERT(get_module_base(module_base) == module_base);

    if (short_digest)
        d_r_md5_init(&md5_short_cxt);
    if (full_digest)
        d_r_md5_init(&md5_full_cxt);

    /* first region to consider is module header.  on linux this is
     * usually part of 1st segment so perhaps we should skip for linux
     * (on windows module_get_nth_segment() starts w/ 1st section and
     * does not include header)
     */
    region_start = module_base + 0;
    region_len = module_get_header_size(module_base);

    /* FIXME: note that if we want to provide/match an Authenticode
     * hash we'd have to skip the Checksum field in the header - see
     * pecoff_v8 */

    /* at each step intersect with the possible short regions */
    module_calculate_digest_helper(
        full_digest ? &md5_full_cxt : NULL, short_digest ? &md5_short_cxt : NULL,
        region_start, region_len, header_start, header_len, footer_start, footer_len);

    for (i = 0; true; i++) {
        uint old_section_prot;
        bool readable;
        app_pc region_end;
        uint seg_chars;

        ASSERT(i < 1000); /* look for runaway loop */
        if (!module_get_nth_segment(module_base, i, &region_start, &region_end,
                                    &seg_chars))
            break;
        region_len = region_end - region_start;

        /* comres.dll for an example of an empty physical section
         *   .data name
         *      0 size of raw data
         *      0 file pointer to raw data
         */
        if (region_len == 0) {
            LOG(GLOBAL, LOG_VMAREAS, 1, "skipping empty physical segment @" PFX "\n",
                region_start);
            /* note that such sections will still get 0-filled
             * but we only look at raw bytes */
            continue;
        }
        if (!TESTANY(sec_char_include, seg_chars) ||
            TESTANY(sec_char_exclude, seg_chars)) {
            LOG(GLOBAL, LOG_VMAREAS, 2, "skipping non-matching segment @" PFX "\n",
                region_start);
            continue;
        }

        /* make sure region is readable. Alternatively, we could just
         * ignore unreadable (according to characteristics) portions
         */
        readable =
            ensure_section_readable(module_base, region_start, region_len, seg_chars,
                                    &old_section_prot, module_base, module_size);

        module_calculate_digest_helper(
            full_digest ? &md5_full_cxt : NULL, short_digest ? &md5_short_cxt : NULL,
            region_start, region_len, header_start, header_len, footer_start, footer_len);
        if (!readable) {
            DEBUG_DECLARE(bool ok =)
            restore_unreadable_section(module_base, region_start, region_len, seg_chars,
                                       old_section_prot, module_base, module_size);
            ASSERT(ok);
        }
    }

    if (short_digest)
        d_r_md5_final(digest->short_MD5, &md5_short_cxt);
    if (full_digest)
        d_r_md5_final(digest->full_MD5, &md5_full_cxt);

    DOCHECK(1, {
        if (full_digest && short_digest &&
            (short_digest_size == 0 || short_digest_size * 2 > module_size)) {
            ASSERT(md5_digests_equal(digest->short_MD5, digest->full_MD5));
        }
    });

    /* FIXME: Note that if we did want to have an md5sum-matching
     * digest we'd have to append the module bytes with the extra
     * bytes that are only present on disk in our digest.  Since
     * usually quite small that could be handled by a read_file()
     * instead of remapping the whole file as MEM_MAPPED. It would be
     * applicable only if we have the appropriate file handle of
     * course. */
}
