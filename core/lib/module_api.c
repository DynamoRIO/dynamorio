/* ******************************************************************************
 * Copyright (c) 2010-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2010-2011 Massachusetts Institute of Technology  All rights reserved.
 * Copyright (c) 2002-2010 VMware, Inc.  All rights reserved.
 * ******************************************************************************/

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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2002-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2002 Hewlett-Packard Company */

#include "../globals.h"
#include "../module_shared.h"
#ifdef UNIX
#    include "../unix/module.h"
#endif
#include "../module_api.h"

/***************************************************************************
 * MODULES
 */

static module_data_t *
create_and_initialize_module_data(app_pc start, app_pc end, app_pc entry_point,
                                  uint flags, const module_names_t *names,
                                  const char *full_path,
#ifdef WINDOWS
                                  version_number_t file_version,
                                  version_number_t product_version, uint checksum,
                                  uint timestamp, size_t mod_size,
#else
                                  bool contiguous, uint num_segments,
                                  module_segment_t *os_segments,
                                  module_segment_data_t *segments, uint timestamp,
#    ifdef MACOS
                                  uint current_version, uint compatibility_version,
                                  const byte uuid[16],
#    endif
#endif
                                  app_pc preferred_base)
{
#ifndef WINDOWS
    uint i;
#endif
    module_data_t *copy = (module_data_t *)HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, module_data_t,
                                                           ACCT_CLIENT, UNPROTECTED);
    memset(copy, 0, sizeof(module_data_t));

    copy->start = start;
    copy->end = end;
    copy->entry_point = entry_point;
    copy->flags = flags;

    if (full_path != NULL)
        copy->full_path = dr_strdup(full_path HEAPACCT(ACCT_CLIENT));
    if (names->module_name != NULL)
        copy->names.module_name = dr_strdup(names->module_name HEAPACCT(ACCT_CLIENT));
    if (names->file_name != NULL)
        copy->names.file_name = dr_strdup(names->file_name HEAPACCT(ACCT_CLIENT));
#ifdef WINDOWS
    if (names->exe_name != NULL)
        copy->names.exe_name = dr_strdup(names->exe_name HEAPACCT(ACCT_CLIENT));
    if (names->rsrc_name != NULL)
        copy->names.rsrc_name = dr_strdup(names->rsrc_name HEAPACCT(ACCT_CLIENT));

    copy->file_version = file_version;
    copy->product_version = product_version;
    copy->checksum = checksum;
    copy->timestamp = timestamp;
    copy->module_internal_size = mod_size;
#else
    copy->contiguous = contiguous;
    copy->num_segments = num_segments;
    copy->segments = (module_segment_data_t *)HEAP_ARRAY_ALLOC(
        GLOBAL_DCONTEXT, module_segment_data_t, num_segments, ACCT_VMAREAS, PROTECTED);
    if (os_segments != NULL) {
        ASSERT(segments == NULL);
        for (i = 0; i < num_segments; i++) {
            copy->segments[i].start = os_segments[i].start;
            copy->segments[i].end = os_segments[i].end;
            copy->segments[i].prot = os_segments[i].prot;
            copy->segments[i].offset = os_segments[i].offset;
        }
    } else {
        ASSERT(segments != NULL);
        if (segments != NULL) {
            memcpy(copy->segments, segments,
                   num_segments * sizeof(module_segment_data_t));
        }
    }
    copy->timestamp = timestamp;
#    ifdef MACOS
    copy->current_version = current_version;
    copy->compatibility_version = compatibility_version;
    memcpy(copy->uuid, uuid, sizeof(copy->uuid));
#    endif
#endif
    copy->preferred_base = preferred_base;
    return copy;
}

module_data_t *
copy_module_area_to_module_data(const module_area_t *area)
{
    if (area == NULL)
        return NULL;

    return create_and_initialize_module_data(
        area->start, area->end, area->entry_point, 0, &area->names, area->full_path,
#ifdef WINDOWS
        area->os_data.file_version, area->os_data.product_version, area->os_data.checksum,
        area->os_data.timestamp, area->os_data.module_internal_size,
#else
        area->os_data.contiguous, area->os_data.num_segments, area->os_data.segments,
        NULL, area->os_data.timestamp,
#    ifdef MACOS
        area->os_data.current_version, area->os_data.compatibility_version,
        area->os_data.uuid,
#    endif
#endif
        IF_WINDOWS_ELSE(area->os_data.preferred_base, area->os_data.base_address));
}

DR_API
/* Makes a copy of a module_data_t for returning to the client.  We return a copy so
 * we don't have to hold the module areas list lock while in the client (xref PR 225020).
 * Note - dr_data is allowed to be NULL. */
module_data_t *
dr_copy_module_data(const module_data_t *data)
{
    if (data == NULL)
        return NULL;

    return create_and_initialize_module_data(
        data->start, data->end, data->entry_point, 0, &data->names, data->full_path,
#ifdef WINDOWS
        data->file_version, data->product_version, data->checksum, data->timestamp,
        data->module_internal_size,
#else
        data->contiguous, data->num_segments, NULL, data->segments, data->timestamp,
#    ifdef MACOS
        data->current_version, data->compatibility_version, data->uuid,
#    endif
#endif
        data->preferred_base);
}

DR_API
/* Used to free a module_data_t created by dr_copy_module_data() */
void
dr_free_module_data(module_data_t *data)
{
    dcontext_t *dcontext = get_thread_private_dcontext();

    if (data == NULL)
        return;

    if (dcontext != NULL && data == dcontext->client_data->no_delete_mod_data) {
        CLIENT_ASSERT(false,
                      "dr_free_module_data: don\'t free module_data passed to "
                      "the image load or image unload event callbacks.");
        return;
    }

#ifdef UNIX
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, data->segments, module_segment_data_t,
                    data->num_segments, ACCT_VMAREAS, PROTECTED);
#endif
    if (data->full_path != NULL)
        dr_strfree(data->full_path HEAPACCT(ACCT_CLIENT));
    free_module_names(&data->names HEAPACCT(ACCT_CLIENT));

    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, data, module_data_t, ACCT_CLIENT, UNPROTECTED);
}

DR_API
bool
dr_module_contains_addr(const module_data_t *data, app_pc addr)
{
    /* XXX: this duplicates module_contains_addr(), but we have two different
     * data structures (module_area_t and module_data_t) so it's hard to share.
     */
#ifdef WINDOWS
    return (addr >= data->start && addr < data->end);
#else
    if (data->contiguous)
        return (addr >= data->start && addr < data->end);
    else {
        uint i;
        for (i = 0; i < data->num_segments; i++) {
            if (addr >= data->segments[i].start && addr < data->segments[i].end)
                return true;
        }
    }
    return false;
#endif
}

DR_API
/* Looks up the module data containing pc.  Returns NULL if not found.
 * Returned module_data_t must be freed with dr_free_module_data(). */
module_data_t *
dr_lookup_module(byte *pc)
{
    module_area_t *area;
    module_data_t *client_data;
    os_get_module_info_lock();
    area = module_pc_lookup(pc);
    client_data = copy_module_area_to_module_data(area);
    os_get_module_info_unlock();
    return client_data;
}

DR_API
module_data_t *
dr_get_main_module(void)
{
    return dr_lookup_module(get_image_entry());
}

DR_API
/* Looks up the module with name matching name (ignoring case).  Returns NULL if not
 * found. Returned module_data_t must be freed with dr_free_module_data(). */
module_data_t *
dr_lookup_module_by_name(const char *name)
{
    /* We have no quick way of doing this since our module list is indexed by pc. We
     * could use get_module_handle() but that's dangerous to call at arbitrary times,
     * so we just walk our full list here. */
    module_iterator_t *mi = module_iterator_start();
    CLIENT_ASSERT((name != NULL), "dr_lookup_module_info_by_name: null name");
    while (module_iterator_hasnext(mi)) {
        module_area_t *area = module_iterator_next(mi);
        module_data_t *client_data;
        const char *modname = GET_MODULE_NAME(&area->names);
        if (modname != NULL && strcasecmp(modname, name) == 0) {
            client_data = copy_module_area_to_module_data(area);
            module_iterator_stop(mi);
            return client_data;
        }
    }
    module_iterator_stop(mi);
    return NULL;
}

typedef struct _client_mod_iterator_list_t {
    module_data_t *info;
    struct _client_mod_iterator_list_t *next;
} client_mod_iterator_list_t;

typedef struct {
    client_mod_iterator_list_t *current;
    client_mod_iterator_list_t *full_list;
} client_mod_iterator_t;

DR_API
/* Initialize a new client module iterator. */
dr_module_iterator_t *
dr_module_iterator_start(void)
{
    client_mod_iterator_t *client_iterator = (client_mod_iterator_t *)HEAP_TYPE_ALLOC(
        GLOBAL_DCONTEXT, client_mod_iterator_t, ACCT_CLIENT, UNPROTECTED);
    module_iterator_t *dr_iterator = module_iterator_start();

    memset(client_iterator, 0, sizeof(*client_iterator));
    while (module_iterator_hasnext(dr_iterator)) {
        module_area_t *area = module_iterator_next(dr_iterator);
        client_mod_iterator_list_t *list = (client_mod_iterator_list_t *)HEAP_TYPE_ALLOC(
            GLOBAL_DCONTEXT, client_mod_iterator_list_t, ACCT_CLIENT, UNPROTECTED);

        ASSERT(area != NULL);
        list->info = copy_module_area_to_module_data(area);
        list->next = NULL;
        if (client_iterator->current == NULL) {
            client_iterator->current = list;
            client_iterator->full_list = client_iterator->current;
        } else {
            client_iterator->current->next = list;
            client_iterator->current = client_iterator->current->next;
        }
    }
    module_iterator_stop(dr_iterator);
    client_iterator->current = client_iterator->full_list;

    return (dr_module_iterator_t)client_iterator;
}

DR_API
/* Returns true if there is another loaded module in the iterator. */
bool
dr_module_iterator_hasnext(dr_module_iterator_t *mi)
{
    CLIENT_ASSERT((mi != NULL), "dr_module_iterator_hasnext: null iterator");
    return ((client_mod_iterator_t *)mi)->current != NULL;
}

DR_API
/* Retrieves the module_data_t for the next loaded module in the iterator. */
module_data_t *
dr_module_iterator_next(dr_module_iterator_t *mi)
{
    module_data_t *data;
    client_mod_iterator_t *ci = (client_mod_iterator_t *)mi;
    CLIENT_ASSERT((mi != NULL), "dr_module_iterator_next: null iterator");
    CLIENT_ASSERT((ci->current != NULL),
                  "dr_module_iterator_next: has no next, use "
                  "dr_module_iterator_hasnext() first");
    if (ci->current == NULL)
        return NULL;
    data = ci->current->info;
    ci->current = ci->current->next;
    return data;
}

DR_API
/* Free the module iterator. */
void
dr_module_iterator_stop(dr_module_iterator_t *mi)
{
    client_mod_iterator_t *ci = (client_mod_iterator_t *)mi;
    CLIENT_ASSERT((mi != NULL), "dr_module_iterator_stop: null iterator");

    /* free module_data_t's we didn't give to the client */
    while (ci->current != NULL) {
        dr_free_module_data(ci->current->info);
        ci->current = ci->current->next;
    }

    ci->current = ci->full_list;
    while (ci->current != NULL) {
        client_mod_iterator_list_t *next = ci->current->next;
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, ci->current, client_mod_iterator_list_t,
                       ACCT_CLIENT, UNPROTECTED);
        ci->current = next;
    }
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, ci, client_mod_iterator_t, ACCT_CLIENT, UNPROTECTED);
}

DR_API
/* Get the name dr uses for this module. */
const char *
dr_module_preferred_name(const module_data_t *data)
{
    if (data == NULL)
        return NULL;

    return GET_MODULE_NAME(&data->names);
}

#ifdef WINDOWS

DR_API
/* If pc is within a section of module lib returns true and (optionally) a copy of
 * the IMAGE_SECTION_HEADER in section_out.  If pc is not within a section of the
 * module mod return false. */
bool
dr_lookup_module_section(module_handle_t lib, byte *pc, IMAGE_SECTION_HEADER *section_out)
{
    CLIENT_ASSERT((lib != NULL), "dr_lookup_module_section: null module_handle_t");
    return module_pc_section_lookup((app_pc)lib, pc, section_out);
}
#endif

/* i#805: Instead of exposing multiple instruction levels, we expose a way for
 * clients to turn off instrumentation.  Then DR can avoid a full decode and we
 * can save some time on modules that are not interesting.
 * XXX: This breaks other clients and extensions, in particular drwrap, which
 * can miss call and return sites in the uninstrumented module.
 */
DR_API
bool
dr_module_set_should_instrument(module_handle_t handle, bool should_instrument)
{
    module_area_t *ma;
    DEBUG_DECLARE(dcontext_t *dcontext = get_thread_private_dcontext());
    IF_DEBUG(executable_areas_lock());
    os_get_module_info_write_lock();
    ma = module_pc_lookup((byte *)handle);
    if (ma != NULL) {
        /* This kind of obviates the need for handle, but it makes the API more
         * explicit.
         */
        CLIENT_ASSERT(dcontext->client_data->no_delete_mod_data->handle == handle,
                      "Do not call dr_module_set_should_instrument() outside "
                      "of the module's own load event");
        ASSERT(!executable_vm_area_executed_from(ma->start, ma->end));
        if (should_instrument) {
            ma->flags &= ~MODULE_NULL_INSTRUMENT;
        } else {
            ma->flags |= MODULE_NULL_INSTRUMENT;
        }
    }
    os_get_module_info_write_unlock();
    IF_DEBUG(executable_areas_unlock());
    return (ma != NULL);
}

DR_API
bool
dr_module_should_instrument(module_handle_t handle)
{
    bool should_instrument = true;
    module_area_t *ma;
    os_get_module_info_lock();
    ma = module_pc_lookup((byte *)handle);
    CLIENT_ASSERT(ma != NULL, "invalid module handle");
    if (ma != NULL) {
        should_instrument = !TEST(MODULE_NULL_INSTRUMENT, ma->flags);
    }
    os_get_module_info_unlock();
    return should_instrument;
}

DR_API
/* Returns the entry point of the function with the given name in the module
 * with the given handle.
 * We're not taking in module_data_t to make it simpler for the client
 * to iterate or lookup the module_data_t, store the single-field
 * handle, and then free the data right away: besides, module_data_t
 * is not an opaque type.
 */
generic_func_t
dr_get_proc_address(module_handle_t lib, const char *name)
{
#ifdef WINDOWS
    return get_proc_address_resolve_forward(lib, name);
#else
    return d_r_get_proc_address(lib, name);
#endif
}

DR_API
bool
dr_get_proc_address_ex(module_handle_t lib, const char *name, dr_export_info_t *info OUT,
                       size_t info_len)
{
    /* If we add new fields we'll check various values of info_len */
    if (info == NULL || info_len < sizeof(*info))
        return false;
#ifdef WINDOWS
    info->address = get_proc_address_resolve_forward(lib, name);
    info->is_indirect_code = false;
#else
    info->address = get_proc_address_ex(lib, name, &info->is_indirect_code);
#endif
    return (info->address != NULL);
}
