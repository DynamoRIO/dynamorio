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

#include "globals.h"
#include "instrument.h"
#include <string.h> /* for memset */

/* Used for maintaining our module list.  Custom field points to
 * further module information from PE/ELF headers.
 * module_data_lock needs to be held when accessing the custom data fields.
 * Kept on the heap for selfprot (case 7957).
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
        read_lock(&module_data_lock);
    /* else we assume past exit: FIXME: best to have exited bool */
}

void
os_get_module_info_unlock(void)
{
    if (loaded_module_areas != NULL) {
        ASSERT_OWN_READ_LOCK(true, &module_data_lock);
        read_unlock(&module_data_lock);
    }
}

void
os_get_module_info_write_lock(void)
{
    if (loaded_module_areas != NULL)
        write_lock(&module_data_lock);
    /* else we assume past exit: FIXME: best to have exited bool */
}

void
os_get_module_info_write_unlock(void)
{
    if (loaded_module_areas != NULL)
        write_unlock(&module_data_lock);
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

static module_area_t *
module_area_create(app_pc base, size_t view_size, bool at_map _IF_LINUX(uint64 inode)
                   _IF_LINUX(const char *filename))
{
    module_area_t *ma =
        HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, module_area_t, ACCT_VMAREAS, PROTECTED);
    memset(ma, 0, sizeof(*ma));
    ma->start = base;
    ma->end = base + view_size;
    os_module_area_init(ma, base, view_size, at_map _IF_LINUX(inode)
                        _IF_LINUX(filename) HEAPACCT(ACCT_VMAREAS));
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
                          VECTOR_SHARED | VECTOR_NEVER_MERGE
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
        module_area_t *ma = (module_area_t*)vmvector_iterator_next(&vmvi, &start, &end);
        ASSERT(ma != NULL);
        ASSERT(ma->start == start && ma->end == end);
        ma->flags |= MODULE_BEING_UNLOADED;
        module_area_delete(ma);
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

void
module_list_add(app_pc base, size_t view_size, bool at_map _IF_LINUX(uint64 inode)
                _IF_LINUX(const char *filename))
{
#ifdef CLIENT_INTERFACE
    module_data_t *client_data = NULL;
    bool inform_client = false;
#endif

    ASSERT(loaded_module_areas != NULL);
    ASSERT(!vmvector_overlap(loaded_module_areas, base, base+view_size));
    os_get_module_info_write_lock();
    /* defensively checking */
    if (!vmvector_overlap(loaded_module_areas, base, base+view_size)) {
        module_area_t *ma = module_area_create(base, view_size, at_map _IF_LINUX(inode)
                                               _IF_LINUX(filename));
        ASSERT(ma != NULL);
        
        LOG(GLOBAL, LOG_INTERP|LOG_VMAREAS, 1, "module %s ["PFX","PFX"] added\n",
            (GET_MODULE_NAME(&ma->names) == NULL) ? "<no name>" :
            GET_MODULE_NAME(&ma->names), base, base+view_size);
        
        /* note that there is normally no need to hold even a
         * read_lock to make sure that nobody is about to remove
         * this entry.  While next to impossible that the
         * currently added module will get unloaded by another
         * thread, we do grab a full write lock around this safe
         * lookup/add.
         */
        vmvector_add(loaded_module_areas, base, base+view_size, ma);
        
#ifdef CLIENT_INTERFACE
        /* inform clients of module loads, we copy the data now and wait to
         * call the client till after we've released the module areas lock */
        if (!IS_STRING_OPTION_EMPTY(client_lib)) {
            client_data = copy_module_area_to_module_data(ma);
            inform_client = true;
        }
#endif
    } else {
        /* already added! */
        /* only possible for manual NtMapViewOfSection, loader 
         * can't be doing this to us */
        ASSERT_CURIOSITY(false && "image load race");
        /* do nothing */
    }
    os_get_module_info_write_unlock();
#ifdef CLIENT_INTERFACE
    if (inform_client) {
        instrument_module_load(client_data, false /*loading now*/);
        dr_free_module_data(client_data);
    }
#endif
}

void
module_list_remove(app_pc base, size_t view_size)
{
    /* lookup and free module */
#ifdef CLIENT_INTERFACE
    module_data_t *client_data = NULL;
    bool inform_client = false;
#endif
    module_area_t *ma;
    
    /* note that vmvector_lookup doesn't protect the custom data,
     * and we need to bracket a lookup and remove in an unlikely
     * application race (note we pre-process unmap)
     */
    ASSERT(loaded_module_areas != NULL);
    os_get_module_info_write_lock();
    ASSERT(vmvector_overlap(loaded_module_areas, base, base+view_size));
    ma = (module_area_t*)vmvector_lookup(loaded_module_areas, base);
    ASSERT_CURIOSITY(ma != NULL); /* loader can't have a race */
    
#ifdef CLIENT_INTERFACE
    /* inform clients of module unloads, we copy the data now and wait to
     * call the client till after we've released the module areas lock */
    if (!IS_STRING_OPTION_EMPTY(client_lib)) {
        client_data = copy_module_area_to_module_data(ma);
        inform_client = true;
    }
#endif
    /* defensively checking */
    if (ma != NULL) {
        vmvector_remove(loaded_module_areas, base, base+view_size);
        /* extra nice, free only after removing from vector */
        module_area_delete(ma);
    }
    ASSERT(!vmvector_overlap(loaded_module_areas, base, base+view_size));
    os_get_module_info_write_unlock();
#ifdef CLIENT_INTERFACE
    if (inform_client) {
        instrument_module_unload(client_data);
        dr_free_module_data(client_data);
    }
#endif
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
    return vmvector_overlap(loaded_module_areas, pc, pc+len);
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
    return os_get_module_name_internal(pc, NULL, 0, false/*no truncate*/,
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
    os_get_module_name_internal(pc, buf, buf_len, true/*truncate*/,
                                &copied HEAPACCT(ACCT_OTHER));
    return copied;
}

/* Copies the module name into buf and returns a pointer to buf,
 * unless buf is too small, in which case the module name is strdup-ed
 * and a pointer to it returned (which the caller must strfree).
 * If there is no module name, returns NULL.
 */
const char *
os_get_module_name_buf_strdup(const app_pc pc, char *buf, size_t buf_len
                              HEAPACCT(which_heap_t which))
{
    return os_get_module_name_internal(pc, buf, buf_len, false/*no truncate*/,
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
    module_iterator_t *mi = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, module_iterator_t,
                                            ACCT_OTHER, UNPROTECTED);
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
    ASSERT(os_get_module_info_locked());
    return vmvector_iterator_hasnext(&mi->vmvi);
}

/* Retrieves the module_data_t for a loaded module */
module_area_t *
module_iterator_next(module_iterator_t *mi)
{
    app_pc start, end;
    module_area_t *ma = (module_area_t *)
        vmvector_iterator_next(&mi->vmvi, &start, &end);
    ASSERT(os_get_module_info_locked());
    ASSERT(ma != NULL);
    ASSERT(ma->start == start && ma->end == end);
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

