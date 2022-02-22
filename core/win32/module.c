/* **********************************************************
 * Copyright (c) 2011-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2010 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */

/* module.c - maintains information about modules (dll or executable images) */

#include "../globals.h"
#include "ntdll.h"
#include <stddef.h> /* for offsetof */

#ifdef RCT_IND_BRANCH
#    include "../rct.h"
#endif

#include "../utils.h"
#include "os_private.h"
#include "aslr.h"
#include "instrument.h"
#include "../perscache.h" /* for coarse_info_t.rct_loaded */
#include "../hashtable.h" /* section2file table */
#include "instr.h"
#include "decode.h"

/* used to hold the version information we get from the .rsrc section */
typedef struct _version_info_t {
    version_number_t file_version;
    version_number_t product_version;
    wchar_t *company_name;
    wchar_t *product_name;
    wchar_t *original_filename;
} version_info_t;

static const char *
get_module_original_filename(app_pc mod_base,
                             version_info_t *in_info /*OPTIONAL IN*/
                                 HEAPACCT(which_heap_t which));

static bool
module_area_free_IAT(module_area_t *ma);

/* NOTE the strings returned in info_out are pointing to the .rsrc version
 * directory and as such they're only valid while the module is loaded. */
static bool
get_module_resource_version_info(app_pc mod_base, version_info_t *info_out);

typedef struct _pe_module_import_iterator_t {
    dr_module_import_t module_import; /* module import returned by next() */

    byte *mod_base;
    size_t mod_size;
    /* Points into an array of IMAGE_IMPORT_DESCRIPTOR structs.  The last
     * element of the array is zeroed.
     */
    IMAGE_IMPORT_DESCRIPTOR *cur_module;
    IMAGE_IMPORT_DESCRIPTOR safe_module; /* safe_read copy of cur_module */
    byte *imports_end;                   /* end of the import descriptors */
    bool hasnext;                        /* set to false on error or end */
} pe_module_import_iterator_t;

typedef struct _pe_symbol_import_iterator_t {
    dr_symbol_import_t symbol_import; /* symbol import returned by next() */
    dr_symbol_import_t next_symbol;   /* next symbol import */

    byte *mod_base;
    dr_module_import_iterator_t *mod_iter; /* only for iterating all modules */
    IMAGE_IMPORT_DESCRIPTOR *cur_module;   /* always valid */
    /* Points into the OriginalFirstThunk array of mod_iter->cur_module. */
    IMAGE_THUNK_DATA *cur_thunk;
    bool hasnext; /* set to false on error or end */
} pe_symbol_import_iterator_t;

/****************************************************************************
 * Section-to-file table for i#138 and PR 213463 (case 9028)
 */

static generic_table_t *section2file_table;
#define INIT_HTABLE_SIZE_SECTION 6 /* should remain small */

typedef struct _section_to_file_t {
    HANDLE section_handle;
    const char *file_path; /* dr_strdup-ed */
} section_to_file_t;

static void
section_to_file_free(dcontext_t *dcontext, section_to_file_t *s2f)
{
    dr_strfree(s2f->file_path HEAPACCT(ACCT_VMAREAS));
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, s2f, section_to_file_t, ACCT_VMAREAS, PROTECTED);
}

/* Returns a dr_strdup-ed string which caller must dr_strfree w/ ACCT_VMAREAS */
const char *
section_to_file_lookup(HANDLE section_handle)
{
    section_to_file_t *s2f;
    const char *file = NULL;
    TABLE_RWLOCK(section2file_table, read, lock);
    s2f = generic_hash_lookup(GLOBAL_DCONTEXT, section2file_table,
                              (ptr_uint_t)section_handle);
    if (s2f != NULL)
        file = dr_strdup(s2f->file_path HEAPACCT(ACCT_VMAREAS));
    TABLE_RWLOCK(section2file_table, read, unlock);
    return file;
}

static bool
section_to_file_add_common(HANDLE section_handle, const char *filepath_dup)
{
    section_to_file_t *s2f;
    bool added = false;
    TABLE_RWLOCK(section2file_table, write, lock);
    s2f = generic_hash_lookup(GLOBAL_DCONTEXT, section2file_table,
                              (ptr_uint_t)section_handle);
    if (s2f != NULL) {
        /* update */
        dr_strfree(s2f->file_path HEAPACCT(ACCT_VMAREAS));
    } else {
        added = true;
        s2f =
            HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, section_to_file_t, ACCT_VMAREAS, PROTECTED);
        s2f->section_handle = section_handle;
        generic_hash_add(GLOBAL_DCONTEXT, section2file_table,
                         (ptr_uint_t)s2f->section_handle, (void *)s2f);
    }
    s2f->file_path = filepath_dup;
    LOG(GLOBAL, LOG_VMAREAS, 2, "section_to_file: section " PFX " => %s\n",
        section_handle, s2f->file_path);
    TABLE_RWLOCK(section2file_table, write, unlock);
    return added;
}

bool
section_to_file_add_wide(HANDLE section_handle, const wchar_t *file_path)
{
    return section_to_file_add_common(section_handle,
                                      dr_wstrdup(file_path HEAPACCT(ACCT_VMAREAS)));
}

bool
section_to_file_add(HANDLE section_handle, const char *file_path)
{
    return section_to_file_add_common(section_handle,
                                      dr_strdup(file_path HEAPACCT(ACCT_VMAREAS)));
}

bool
section_to_file_remove(HANDLE section_handle)
{
    bool found = false;
    TABLE_RWLOCK(section2file_table, write, lock);
    found = generic_hash_remove(GLOBAL_DCONTEXT, section2file_table,
                                (ptr_uint_t)section_handle);
    TABLE_RWLOCK(section2file_table, write, unlock);
    DODEBUG({
        if (found) {
            LOG(GLOBAL, LOG_VMAREAS, 2, "section_to_file: removed section " PFX "\n",
                section_handle);
        }
    });
    return found;
}

/****************************************************************************/
#ifdef DEBUG /* around symbols related part of the file */

#    include <windows.h>

/* Currently only uses dll export functions for DEBUG symbol information */

/* Private data types */
typedef struct export_entry_t {
    app_pc entry_point; /* exported function entry point */
    char *export_name;
} export_entry_t;

/* FIXME: a module can have multiple code sections and each should
 * have a separate searchable entry, yet all relevant per-module
 * structures should be thrown away when a module is unloaded.
 * Caution with data sections (or other invalidated vmareas) that may
 * be within the module region.
 */

/* use module_area_t for any release build needs */
typedef struct module_info_t {
    app_pc start;
    app_pc end; /* open end interval */
    char *module_name;
    size_t exports_size; /* initial export table length */
    uint exports_num;    /* number of unique exports */

    export_entry_t *exports_table; /* sorted array to allow range searches */
    /* FIXME: this is necessary only for debug symbol lookup, if
     * implementing export restrictions (case 286), we only need
     * membership tests so we only have to populate the entry points
     * into a hashtable
     */

    /* FIXME: case 3927
     * It will be quite useful for some debugging sessions to
     * use real symbols from PDB's.  It maybe easier if they are
     * already preprocessed with windbg -c 'x module!*' or some other
     * PDB parser.  Then we'll have an easier time debugging and
     * reverse engineering some external components.  We could even
     * think about security policies (or constraints) that would
     * require symbols, since at least M$ has at least public PDB's
     * (e.g. not private with variable names) but a good starting
     * point.
     */
} module_info_t;

/* This structure is parallel to the one kept by the loader -
 * yet may be safer to use.  See comment in vm_area_vector_t.
 *
 * FIXME: We may want to abstract this and vm_area_vector_t into a common data structure
 * with polymorphic elements with a start,end
 */

/* The module vector is kept sorted by area.  Since there are no overlaps allowed
 * among areas in the same vector, sorting by start_pc or by end_pc produce
 * identical results.
 */

typedef struct module_info_vector_t {
    struct module_info_t *buf;
    int capacity;
    int length;
    /* thread-shared, so needs a lock */
    /* FIXME: make this a read/write lock if readers contend too often */
    mutex_t lock;
} module_info_vector_t;

/* debug-only so we don't need to efficiently protect it */
DECLARE_CXTSWPROT_VAR(static module_info_vector_t process_module_vector,
                      { NULL, 0, 0, INIT_LOCK_FREE(process_module_vector_lock) });

static void
print_module_list(module_info_vector_t *v)
{
    int i;
    LOG(GLOBAL, LOG_SYMBOLS, 4,
        "print_module_list(" PFX ") capacity=%d, length=%d, lock=%d, buf=" PFX, v,
        v->capacity, v->length, v->lock, v->buf);

    d_r_mutex_lock(&v->lock);
    for (i = 0; i < v->length; i++) {
        LOG(GLOBAL, LOG_SYMBOLS, 3, "  " PFX "-" PFX " %s, %d exports [" SZFMT " size]\n",
            v->buf[i].start, v->buf[i].end, v->buf[i].module_name, v->buf[i].exports_num,
            v->buf[i].exports_size);
    }
    d_r_mutex_unlock(&v->lock);
}

/* For binary search */
int
module_info_compare(const void *vkey, const void *vel)
{
    module_info_t *key = ((module_info_t *)vkey);
    module_info_t *el = ((module_info_t *)vel);
    if (key->end <= el->start)
        return (-1); /* key less than element */

    if (key->start >= el->end)
        return (1); /* key greater than element */

    return (0); /* key equals (overlaps) element */
}

/* lookup a module by address,
   assumes the v->lock is held by caller!
   returned module_info_t *should not be used after releasing lock
   returns NULL if no module found
*/
static module_info_t *
lookup_module_info(module_info_vector_t *v, app_pc addr)
{
    /* BINARY SEARCH -- assumes the vector is kept sorted by add & remove! */
    module_info_t key = { addr, addr + 1 }; /* end is open */
    /* FIXME : copied from find_predecessor(), would be nice to share with
     * that routine and with binary range search (w/linear backsearch) in
     * vmareas.c */
    int min = 0;
    int max = v->length - 1;
    /* binary search */
    while (max >= min) {
        int i = (min + max) / 2;
        int cmp = module_info_compare(&key, &(v->buf[i]));
        if (cmp < 0)
            max = i - 1;
        else if (cmp > 0)
            min = i + 1;
        else {
            return &(v->buf[i]);
        }
    }
    return NULL;
}

#    define INITIAL_MODULE_NUMBER 4

/* Creates a new module info, allocates its exports table, and adds to module vector,
 * module_name is caller allocated (module_name is from the exports section for PE dlls)
 *
 * Returns a pointer to the module's export table
 */
static export_entry_t *
module_info_create(module_info_vector_t *v, app_pc start, app_pc end, char *module_name,
                   uint exports_num)
{
    struct module_info_t new_module = { start,       end,         module_name,
                                        exports_num, exports_num, 0 /* table */ };
    int i, j;

    if (exports_num > 0) {
        new_module.exports_table = global_heap_alloc(
            exports_num * sizeof(export_entry_t) HEAPACCT(ACCT_SYMBOLS));
    } else {
        new_module.exports_table = NULL;
    }

    d_r_mutex_lock(&v->lock);
    /* FIXME: the question is what to do when an overlap occurs.
     * If we assume that we should have removed the references from an old DLL.
     * A possibly new DLL overlapping the same range should not show up,
     * this indeed would be an error worth investigating.
     */
    /* FIXME: need a real overlap check */
    if (lookup_module_info(v, start) != NULL) {
        ASSERT_NOT_REACHED();
        return NULL;
    }

    for (i = 0; i < v->length; i++) {
        if (end <= v->buf[i].start)
            break;
    }
    /* check if at full capacity */
    if (v->capacity == v->length) {
        int new_size = v->capacity ? v->capacity * 2 : INITIAL_MODULE_NUMBER;
        v->buf = global_heap_realloc(v->buf, v->capacity, new_size,
                                     sizeof(struct module_info_t) HEAPACCT(ACCT_SYMBOLS));
        v->capacity = new_size;
        ASSERT(v->buf);
    }
    /* shift subsequent to i entries */
    for (j = v->length; j > i; j--)
        v->buf[j] = v->buf[j - 1];

    v->buf[i] = new_module;
    v->length++;
    d_r_mutex_unlock(&v->lock);
    DOLOG(3, LOG_SYMBOLS, { print_module_list(v); });

    /* we can not return &v->buf[i] since buf may get realloc-ed, or buf[i] may be
     * shifted
     */
    return new_module.exports_table;
}

/* remove from module vector and free up memory */
static void
remove_module_info_vector(module_info_vector_t *v, app_pc start, app_pc end)
{
    int i, j;

    export_entry_t *exports_table = NULL;
    size_t exports_size = 0;

    d_r_mutex_lock(&v->lock);
    /* linear search, we don't have a find_predecessor on module_info_t's to get i */
    for (i = 0; i < v->length; i++) {
        if (start == v->buf[i].start && end == v->buf[i].end) {
            exports_table = v->buf[i].exports_table;
            exports_size = v->buf[i].exports_size;
            break;
        }
    }

    LOG(GLOBAL, LOG_SYMBOLS, 2, "remove_module_info_vector(" PFX "," PFX ") dll=%s\n",
        start, end, v->buf[i].module_name);
    ASSERT_CURIOSITY(exports_table); /* curiosity */
    if (!exports_table) {
        /* it could have disappeared since we last checked */
        d_r_mutex_unlock(&v->lock);
        return;
    }

    /* shift subsequent to i entries over */
    for (j = i + 1; j < v->length; j++)
        v->buf[j - 1] = v->buf[j];
    v->length--;
    d_r_mutex_unlock(&v->lock);

    if (exports_size > 0) {
        global_heap_free(exports_table,
                         exports_size * sizeof(export_entry_t) HEAPACCT(ACCT_SYMBOLS));
    }

    DOLOG(3, LOG_SYMBOLS, { print_module_list(v); });
}

/* remove internal bookkeeping for unloaded module
   returns 1 if the range is a known module, and that is removed,
           0 otherwise
*/
int
remove_module_info(app_pc start, size_t size)
{
    module_info_t *pmod;
    d_r_mutex_lock(&process_module_vector.lock);
    pmod = lookup_module_info(&process_module_vector, start);
    d_r_mutex_unlock(&process_module_vector.lock);

    if (!pmod) { /* FIXME: need a real overlap check */
        LOG(GLOBAL, LOG_SYMBOLS, 2,
            "WARNING:remove_module_info called on unknown module " PFX ", size " PIFX
            "\n",
            start, size);
        /* my assert_curiosity was triggered, yet unexplained */
        return 0;
    }

    remove_module_info_vector(&process_module_vector, (app_pc)start,
                              (app_pc)(start + size));

    return 1;
}

void
module_cleanup(void)
{
    module_info_vector_t *v = &process_module_vector;
    int i;
    export_entry_t *exports_table = NULL;
    size_t exports_size = 0;
    d_r_mutex_lock(&v->lock);
    /* linear search, we don't have a find_predecessor on module_info_t's to get i */
    for (i = 0; i < v->length; i++) {
        exports_table = v->buf[i].exports_table;
        exports_size = v->buf[i].exports_size;
        if (exports_size > 0) {
            global_heap_free(exports_table,
                             exports_size *
                                 sizeof(export_entry_t) HEAPACCT(ACCT_SYMBOLS));
        }
    }
    if (v->buf != NULL) {
        global_heap_free(
            v->buf, v->capacity * sizeof(struct module_info_t) HEAPACCT(ACCT_SYMBOLS));
    }
    v->buf = NULL;
    v->capacity = 0;
    v->length = 0;
    d_r_mutex_unlock(&v->lock);
}

void
module_info_exit()
{
    module_cleanup();
    DELETE_LOCK(process_module_vector.lock);
}

int
export_entry_compare(const void *vkey, const void *vel)
{
    /* used for qsort so only care about sign; truncation is ok */
    return (int)(((export_entry_t *)vkey)->entry_point -
                 ((export_entry_t *)vel)->entry_point);
}

/* Returns the offset within table[] of the last element equal or smaller than key,
   table[] must be sorted in ascending order.
   Returns -1 when smaller than the first element, or array empty
*/
int
find_predecessor(export_entry_t table[], int n, app_pc tag)
{
    int min = 0;
    int max = n - 1;
    /* binary search */
    while (max >= min) {
        int i = (min + max) / 2;

        if (tag < table[i].entry_point)
            max = i - 1;
        else if (tag > table[i].entry_point)
            min = i + 1;
        else {
            return i;
        }
    }

    /* now max < min */
    return max; /* may be -1 */
}

/* remove duplicate export entries,
   returns number of unique entry points
   (assumes table is ordered by address)
*/
int
remove_export_duplicates(export_entry_t table[], int n)
{
    int i = 0, j = 1;

    if (n < 2)
        return n;

    while (j < n) {
        if (table[i].entry_point == table[j].entry_point) {
            LOG(GLOBAL, LOG_SYMBOLS, 3, "Export alias %s == %s\n", table[i].export_name,
                table[j].export_name);
        } else {
            i++;
            table[i] = table[j];
        }

        j++;
    }
    i++;

    return i;
}

/* prints a symbolic name, or best guess of it into a caller provided buffer */
void
print_symbolic_address(app_pc tag, char *buf, int max_chars, bool exact_only)
{
    module_info_t *pmod;       /* volatile pointer */
    module_info_t mod = { 0 }; /* copy of module info */

    /* FIXME: cannot grab this lock under internal_exception_lock */
    if (under_internal_exception()) {
        pmod = NULL;
    } else {
        d_r_mutex_lock(
            &process_module_vector.lock); /* FIXME: can be a shared read lock */
        {
            pmod = lookup_module_info(&process_module_vector, tag);
            if (pmod) {
                mod = *pmod; /* keep a copy in case of reallocations */
                /* the data will be invalid only in a race condition,
                   where some other thread frees the library */
            }
        }
        d_r_mutex_unlock(&process_module_vector.lock);
    }

    buf[0] = '\0';
    if (pmod != NULL) {
        int i = find_predecessor(mod.exports_table, mod.exports_num, tag);
        if (i < 0) { /* tag smaller than first exported function */
            /* convert to offset from base */
            if (!exact_only) {
                snprintf(buf, max_chars, "[%s~%s+" PIFX "]", mod.module_name, ".begin",
                         tag - mod.start);
            }
        } else {
            if (mod.exports_table[i].entry_point == tag) {
                /* tag matches an export like <ntdll!CsrIdentifyAlertableThread> */
                snprintf(buf, max_chars, "[%s!%s]", mod.module_name,
                         mod.exports_table[i].export_name);
            } else if (!exact_only) {
                uint prev = (uint)i;
                uint next = (uint)i + 1;

                ASSERT((uint)i < mod.exports_num);
                /* <KERNEL32.dll~CreateProcessW+0x1564,~RegisterWaitForInputIdle-0x9e> */
                snprintf(buf, max_chars, "[%s~%s+" PIFX ",~%s-" PIFX "]", mod.module_name,
                         mod.exports_table[prev].export_name,
                         tag - (ptr_uint_t)mod.exports_table[prev].entry_point,
                         next < mod.exports_num ? mod.exports_table[next].export_name
                                                : ".end",
                         (next < mod.exports_num ? mod.exports_table[next].entry_point
                                                 : mod.end) -
                             (ptr_uint_t)tag);
            }
        }
    } else {
        char modname_buf[MAX_MODNAME_INTERNAL];
        const char *short_name = NULL;
        if (under_internal_exception()) {
            /* We're called in fragile situations so we explicitly check here.
             * Will get lock rank order in accessing module_data_lock so just
             * use PE name.  This is for debugging only anyway.
             */
            app_pc base = get_allocation_base(tag);
            if (base != NULL && is_readable_pe_base(base))
                short_name = get_dll_short_name(base);
            if (short_name == NULL)
                short_name = "";
        } else {
            os_get_module_name_buf(tag, modname_buf, BUFFER_SIZE_ELEMENTS(modname_buf));
            short_name = modname_buf;
        }
        /* since currently we aren't working well w/ dynamically loaded dlls, and
         * certain things are disabled at lower loglevels, fall back to the short name
         */
        DODEBUG({
            get_module_name(tag, buf, max_chars);
            /* check if we get the same name */
            if (strcasecmp(get_short_name(buf), short_name) != 0 && buf[0] != '\0') {
                /* after a module is off the module list some code from it still
                 * gets executed */
                /* In addition there are modules with different file names,
                   e.g. wdmaud.drv != wdmaud.dll (export section name) */
                LOG(GLOBAL, LOG_SYMBOLS, 3,
                    "WARNING: print_symbolic_address(" PFX "): ldr name='%s' "
                    "pe name='%s'\n",
                    tag, get_short_name(buf), short_name);
            }
        });
        snprintf(buf, max_chars, "[%s]", short_name);
    }
    buf[max_chars - 1] = '\0'; /* to make sure */
    LOG(GLOBAL, LOG_SYMBOLS, 5, "print_symbolic_address(" PFX ")='%s'\n", tag, buf);
}

/* adds a module to the module_info_t list, and parses its exports table,
   this can be done as soon as the module is mapped in the address space */
/* returns 1 if successfully added
           0 if address range is not a PE file */
int
add_module_info(app_pc base_addr, size_t image_size)
{
    size_t size;
    IMAGE_EXPORT_DIRECTORY *exports =
        get_module_exports_directory_check(base_addr, &size, true);

    if (exports != NULL) {
        PULONG functions = (PULONG)(base_addr + exports->AddressOfFunctions);
        PUSHORT ordinals = (PUSHORT)(base_addr + exports->AddressOfNameOrdinals);
        PULONG fnames = (PULONG)(base_addr + exports->AddressOfNames);
        char *dll_name = (char *)(base_addr + exports->Name);
        uint exports_num = 0;
        uint i;

        export_entry_t *exports_table;

        LOG(GLOBAL, LOG_SYMBOLS, 4, "\tnumnames=%d numfunc=%d", exports->NumberOfNames,
            exports->NumberOfFunctions);

        if (exports->NumberOfFunctions != exports->NumberOfNames) {
            /* TODO: we should also use the knowledge about the noname [ordinal] entry
             * points.  shlwapi.dll or winspool.drv  are good examples.
             * find more with dumpbin /exports shlwapi.dll | grep "number of"
             */
            /* These are in fact much more important for
             * rct_add_exports() where we traverse functions otherwise
             * we'd have a .E in shlwapi on a noname export
             * SHLWAPI!Ordinal80
             */
            LOG(GLOBAL, LOG_SYMBOLS, 2, "add_module_info: %s functions %d != %d names\n",
                dll_name, exports->NumberOfFunctions, exports->NumberOfNames);
        }
        /* FIXME: Once we do use noname entry points this if should change to
         * check NumberOfFunctions, but for now we only look at names
         */
        if (exports->NumberOfNames == 0) {
            /* riched32.dll from mmc.exe actually has NumberOfFunctions==0 */
            LOG(GLOBAL, LOG_SYMBOLS, 1, "dll_name=%s has no exported symbols\n",
                dll_name);
            return 1;
        }

        /* FIXME: old comment, should work now.
         * Dynamically loading modules is still not working very
         * well, e.g. the itss.dll when loaded in notepad.exe on
         * F1 seems to point to a zero page for its exports
         * directory..  We are reading the right exports address
         * but there is nothing there.  There may be some VA/RVA
         * issue, and note we try to do this on a MapViewOfSection
         * before the loader has done anything to the file */

        LOG(GLOBAL, LOG_SYMBOLS, 3,
            "dll_name=%s exports=" PFX " functions=" PFX " ordinals=" PFX " fnames=" PFX
            " numnames=%d numfunc=%d %s"
            "baseord=%d\n", /* the linker adds this to the ords we see, safe to ignore */
            dll_name, exports, functions, ordinals, fnames, exports->NumberOfNames,
            exports->NumberOfFunctions,
            (exports->NumberOfFunctions == exports->NumberOfNames) ? "" : "NONAMES ",
            exports->Base);

        DOLOG(6, LOG_SYMBOLS, { dump_buffer_as_bytes(GLOBAL, exports, size, 16); });

        exports_table = module_info_create(&process_module_vector, (app_pc)base_addr,
                                           (app_pc)(base_addr + image_size), dll_name,
                                           exports->NumberOfNames);
        /* FIXME: for a security policy to restrict transfers to exports only,
         * we actually need all functions and they simply need to be put in a hash
         * table
         */
        /* FIXME: for RCT_IND_BRANCH we don't need to travel
         * through the string names or forwarders - we should only
         * scan through all functions[] instead of
         * functions[ordinals[i]]
         */
        ASSERT(exports_table != NULL);
        for (i = 0; i < exports->NumberOfNames; i++) {
            PSTR name = (PSTR)(base_addr + fnames[i]);
            ULONG ord = ordinals[i];
            app_pc func = (app_pc)(base_addr + functions[ord]); /* ord here, not i */

            /* check if it points within exports section in real address space, not RVA */
            if (func < (app_pc)exports || func >= (app_pc)exports + size) {
                LOG(GLOBAL, LOG_SYMBOLS, 3, "\t%s -> " PFX "\n", name, func);
                /* insert in exports table, coming sorted by name order */
                exports_table[exports_num].export_name = name;
                exports_table[exports_num].entry_point = func;
                exports_num++;
            } else {
                char *forwardto = (char *)(functions[ord] + base_addr);
                // skip forwarded function if it forwards to a named import
                // i.e. NTDLL.RtlAllocateHeap will be reported instead of HeapAlloc

                LOG(GLOBAL, LOG_SYMBOLS, 3,
                    "Forward found for %s -> " PFX " %s.  Skipping...\n", name,
                    functions[ord], forwardto);

                /* FIXME: Report the name under which it should show up if it is
                 * an ordinal import if it is referenced as ordinal
                 * DLLNAME.#232, then we'll get more from the current name.  The
                 * problem though is that now the address range of the forwarded
                 * function is not going to give us the module name... haven't
                 * seen any so far
                 */
            }
        }

        /* FIXME: take this post processing step out of this function */
        /* the exports_table now needs to be sorted by function address instead of name */
        qsort(exports_table, exports_num, /* non skipped entries only */
              sizeof(export_entry_t), export_entry_compare);

        /* need to remove duplicates and update entry in process_module_vector */
        d_r_mutex_lock(&process_module_vector.lock);
        {
            module_info_t *pmod;
            int unique_num = remove_export_duplicates(exports_table, exports_num);

            /* FIXME: need a real overlap check */
            pmod = lookup_module_info(&process_module_vector, (app_pc)base_addr);
            ASSERT(pmod);
            pmod->exports_num = unique_num;
        }
        d_r_mutex_unlock(&process_module_vector.lock);
        return 1;
    } else {
        DOLOG(SYMBOLS_LOGLEVEL, LOG_SYMBOLS, {
            char short_name[MAX_MODNAME_INTERNAL];
            os_get_module_name_buf(base_addr, short_name,
                                   BUFFER_SIZE_ELEMENTS(short_name));

            /* the executable itself is OK */
            if (base_addr != get_own_peb()->ImageBaseAddress) {
                if (short_name)
                    LOG(GLOBAL, LOG_SYMBOLS, 2, "No exports %s\n", short_name);
                else
                    LOG(GLOBAL, LOG_SYMBOLS, 2, "Not a PE at " PFX "\n", base_addr);
            }
        });
        return 0;
    }
}

/* The following functions depend on traversing loader data */

/* This routine is here so we know how to walk all 3 loader lists */
static void
print_ldr_data()
{
    PEB *peb = get_own_peb();
    PEB_LDR_DATA *ldr = peb->LoaderData;
    LIST_ENTRY *e, *mark;
    LDR_MODULE *mod;
    int i;
    LOG(GLOBAL, LOG_ALL, 1, "PEB LoaderData:\n");
    LOG(GLOBAL, LOG_ALL, 1, "\tLength = %d\n", ldr->Length);
    LOG(GLOBAL, LOG_ALL, 1, "\tInitialized = %d\n", ldr->Initialized);
    LOG(GLOBAL, LOG_ALL, 1, "\tSsHandle = " PFX "\n", ldr->SsHandle);
    LOG(GLOBAL, LOG_ALL, 1, "InLoadOrder:\n");
    mark = &ldr->InLoadOrderModuleList;
    for (i = 0, e = mark->Flink; e != mark; i++, e = e->Flink) {
        LOG(GLOBAL, LOG_ALL, 5,
            "  %d  e=" PFX " => " PFX " " PFX " " PFX " " PFX " " PFX " " PFX "\n", i, e,
            *((ptr_uint_t *)e), *((ptr_uint_t *)e + 1), *((ptr_uint_t *)e + 2),
            *((ptr_uint_t *)e + 3), *((ptr_uint_t *)e + 4), *((ptr_uint_t *)e + 5));
        mod = (LDR_MODULE *)e;
        LOG(GLOBAL, LOG_ALL, 1, "\t%d  " PFX " " PFX " 0x%x %S %S\n", i, mod->BaseAddress,
            mod->EntryPoint, mod->SizeOfImage, mod->FullDllName.Buffer,
            mod->BaseDllName.Buffer);
        if (i > MAX_MODULE_LIST_INFINITE_LOOP_THRESHOLD) {
            SYSLOG_INTERNAL_WARNING_ONCE("print_ldr_data: too many modules, maybe "
                                         "in a race");
            break;
        }
    }
    LOG(GLOBAL, LOG_ALL, 1, "InMemoryOrder:\n");
    /* FIXME: why doesn't this turn out to be in memory order? */
    mark = &ldr->InMemoryOrderModuleList;
    for (i = 0, e = mark->Flink; e != mark; i++, e = e->Flink) {
        LOG(GLOBAL, LOG_ALL, 5,
            "  %d  e=" PFX " => " PFX " " PFX " " PFX " " PFX " " PFX " " PFX "\n", i, e,
            *((ptr_uint_t *)e), *((ptr_uint_t *)e + 1), *((ptr_uint_t *)e + 2),
            *((ptr_uint_t *)e + 3), *((ptr_uint_t *)e + 4), *((ptr_uint_t *)e + 5));
        mod = (LDR_MODULE *)((char *)e - offsetof(LDR_MODULE, InMemoryOrderModuleList));
        LOG(GLOBAL, LOG_ALL, 1, "\t%d  " PFX " " PFX " 0x%x %S %S\n", i, mod->BaseAddress,
            mod->EntryPoint, mod->SizeOfImage, mod->FullDllName.Buffer,
            mod->BaseDllName.Buffer);
        if (i > MAX_MODULE_LIST_INFINITE_LOOP_THRESHOLD) {
            SYSLOG_INTERNAL_WARNING_ONCE("print_ldr_data: too many modules, maybe "
                                         "in a race");
            break;
        }
    }
    LOG(GLOBAL, LOG_ALL, 1, "InInitOrder:\n");
    mark = &ldr->InInitializationOrderModuleList;
    for (i = 0, e = mark->Flink; e != mark; i++, e = e->Flink) {
        LOG(GLOBAL, LOG_ALL, 5,
            "  %d  e=" PFX " => " PFX " " PFX " " PFX " " PFX " " PFX " " PFX "\n", i, e,
            *((ptr_uint_t *)e), *((ptr_uint_t *)e + 1), *((ptr_uint_t *)e + 2),
            *((ptr_uint_t *)e + 3), *((ptr_uint_t *)e + 4), *((ptr_uint_t *)e + 5));
        mod = (LDR_MODULE *)((char *)e -
                             offsetof(LDR_MODULE, InInitializationOrderModuleList));
        LOG(GLOBAL, LOG_ALL, 1, "\t%d  " PFX " " PFX " 0x%x %S %S\n", i, mod->BaseAddress,
            mod->EntryPoint, mod->SizeOfImage, mod->FullDllName.Buffer,
            mod->BaseDllName.Buffer);
        if (i > MAX_MODULE_LIST_INFINITE_LOOP_THRESHOLD) {
            SYSLOG_INTERNAL_WARNING_ONCE("print_ldr_data: too many modules, maybe "
                                         "in a race");
            break;
        }
    }
}

#endif /* DEBUG */
/****************************************************************************/
/* release build routines */

/* remember our struct in case we want to put it back */
static LDR_MODULE *DR_module;

static RTL_RB_TREE *
find_ntdll_mod_rbtree(module_handle_t ntdllh, RTL_RB_TREE *tomatch)
{
    /* Several internal routines reference ntdll!LdrpModuleBaseAddressIndex like so:
     *   mov     rax,qword ptr [ntdll!LdrpModuleBaseAddressIndex (000007ff`7995eaa0)]
     * On Win8, but not Win8.1, the exported LdrGetProcedureAddressForCaller does.
     * On both Win8 and Win8.1, the exported LdrDisableThreadCalloutsForDll calls
     * the internal LdrpFindLoadedDllByHandle which then has the ref we want.
     */
#define RBTREE_MAX_DECODE 0x180 /* it's at +0xe1 on win8 */
    RTL_RB_TREE *found = NULL;
    instr_t inst;
    bool found_call = false;
    byte *pc;
    byte *start = (byte *)d_r_get_proc_address(ntdllh, "LdrDisableThreadCalloutsForDll");
    if (start == NULL)
        return NULL;
    instr_init(GLOBAL_DCONTEXT, &inst);
    for (pc = start; pc < start + RBTREE_MAX_DECODE;) {
        instr_reset(GLOBAL_DCONTEXT, &inst);
        pc = decode(GLOBAL_DCONTEXT, pc, &inst);
        if (!instr_valid(&inst) || instr_is_return(&inst))
            break;
        if (!found_call && instr_get_opcode(&inst) == OP_call) {
            /* We assume the first call is the one to the internal routine.
             * Switch to that routine.
             */
            found_call = true;
            pc = opnd_get_pc(instr_get_target(&inst));
        } else if (instr_get_opcode(&inst) == OP_mov_ld) {
            opnd_t src = instr_get_src(&inst, 0);
            if (opnd_is_abs_addr(src) IF_X64(|| opnd_is_rel_addr(src))) {
                byte *addr = opnd_get_addr(src);
                if (is_in_ntdll(addr)) {
                    RTL_RB_TREE local;
                    if (d_r_safe_read(addr, sizeof(local), &local) &&
                        local.Root == tomatch->Root && local.Min == tomatch->Min) {
                        LOG(GLOBAL, LOG_ALL, 2,
                            "Found LdrpModuleBaseAddressIndex @" PFX "\n", addr);
                        found = (RTL_RB_TREE *)addr;
                        break;
                    }
                }
            }
        }
    }
    instr_free(GLOBAL_DCONTEXT, &inst);
    return found;
}

/* i#934: remove from the rbtree added in Win8.
 * Our strategy is to call RtlRbRemoveNode and pass in either a fake rbtree
 * (if our dll is not the root or min node) or go and decode a routine
 * to find the real rbtree (ntdll!LdrpModuleBaseAddressIndex) to pass in.
 */
static void
hide_from_rbtree(LDR_MODULE *mod)
{
    RTL_RB_TREE *tree;
    RTL_RB_TREE tree_local;
    RTL_BALANCED_NODE *node;
    typedef VOID(NTAPI * RtlRbRemoveNode_t)(IN PRTL_RB_TREE Tree,
                                            IN PRTL_BALANCED_NODE Node);
    RtlRbRemoveNode_t RtlRbRemoveNode;
    module_handle_t ntdllh;

    if (get_os_version() < WINDOWS_VERSION_8)
        return;

    LOG(GLOBAL, LOG_ALL, 2, "Attempting to remove dll from rbtree\n");

    ntdllh = get_ntdll_base();
    RtlRbRemoveNode = (RtlRbRemoveNode_t)d_r_get_proc_address(ntdllh, "RtlRbRemoveNode");
    if (RtlRbRemoveNode == NULL) {
        SYSLOG_INTERNAL_WARNING("cannot remove dll from rbtree: no RtlRbRemoveNode");
        return;
    }

    tree = &tree_local;
    node = &mod->BaseAddressIndexNode;
    while (RTL_BALANCED_NODE_PARENT_VALUE(node) != NULL)
        node = RTL_BALANCED_NODE_PARENT_VALUE(node);
    tree->Root = node;
    node = node->Left;
    while (node->Left != NULL)
        node = node->Left;
    tree->Min = node;

    if (&mod->BaseAddressIndexNode == tree->Root ||
        &mod->BaseAddressIndexNode == tree->Min) {
        /* We decode a routine known to deref ntdll!LdrpModuleBaseAddressIndex.
         * An alternative could be to scan ntdll's data sec looking for root and min?
         */
        tree = find_ntdll_mod_rbtree(ntdllh, tree);
        if (tree == NULL) {
            SYSLOG_INTERNAL_WARNING("cannot remove dll from rbtree: at root/min + "
                                    "can't find real tree");
            return;
        }
    }

    /* Strangely this seems to have no return value so we don't know whether it
     * succeeded.
     */
    RtlRbRemoveNode(tree, &mod->BaseAddressIndexNode);
    LOG(GLOBAL, LOG_ALL, 2, "Removed dll from rbtree\n");
}

/* FIXME : to cleanly detach we need to add ourselves back on to the module
 * list so we can free library NYI, right now is memory leak but not a big
 * deal since vmmheap is already leaking a lot more then that */

/* NOTE We are walking the loader lists without holding the lock which is
 * potentially dangerous, however we are doing this at init time where we
 * expect to be single threaded and to be in a clean app state, is more of
 * a problem for unhide when it is implemented */

/* NOTE the loader lists appear to be doubly linked circular lists with
 * each element being a LDR_MODULE (cast to LIST_ENTRYs at various offsets
 * for actual inclusion on the lists) except the initial list entry in the
 * PEB_LDR_DATA. NOTE, we assume here (and everywhere else) that the forward
 * links are circularly linked for our iteration loops, we ASSERT that the
 * backwards pointer is valid before updating below, FIXME should we
 * be checking the forward pointers here and elsewhere for the loop? */

/* FIXME : also where is the unloaded module list kept? Might be nice to
 * remove our pre-inject dll from that. */

static void
hide_from_module_lists(void)
{
    /* remove us from the module lists! */
    PEB *peb = get_own_peb();
    PEB_LDR_DATA *ldr = peb->LoaderData;
    LIST_ENTRY *e, *mark;
    LDR_MODULE *mod;
    int i;
    app_pc DRbase;
    MEMORY_BASIC_INFORMATION mbi;
    size_t len;

    /* FIXME: have os find DR bounds earlier so we don't duplicate work */
    len = query_virtual_memory((app_pc)hide_from_module_lists, &mbi, sizeof(mbi));
    ASSERT(len == sizeof(mbi));
    ASSERT(mbi.State != MEM_FREE);
    DRbase = (app_pc)mbi.AllocationBase;
    LOG(GLOBAL, LOG_TOP, 1, "DR dll base = " PFX "\n", DRbase);

    /* FIXME: build iterator so all loopers aren't duplicating all this code */
    mark = &ldr->InLoadOrderModuleList;
    ASSERT(mark->Flink != NULL && mark->Blink != NULL); /* sanity check */
    ASSERT(offsetof(LDR_MODULE, InLoadOrderModuleList) == 0);
    for (i = 0, e = mark->Flink; e != mark; i++, e = e->Flink) {
        mod = (LDR_MODULE *)e;
        /* sanity check */
        ASSERT(e->Flink != NULL && e->Blink != NULL && e->Flink->Blink != NULL &&
               e->Blink->Flink != NULL);
        if ((app_pc)mod->BaseAddress == DRbase) {
            /* we store the LDR_MODULE struct and do not attempt to de-allocate it,
             * in case we want to put it back
             */
            DR_module = mod;
            LOG(GLOBAL, LOG_ALL, 1, "Removing " PFX " %S from load order module list\n",
                mod->BaseAddress, mod->FullDllName.Buffer);
            /* doubly linked circular list */
            e->Flink->Blink = e->Blink;
            e->Blink->Flink = e->Flink;
            if (get_os_version() >= WINDOWS_VERSION_8) {
                /* i#934: remove from the rbtree added in Win8 */
                hide_from_rbtree(mod);
            }
            break;
        }
        if (i > MAX_MODULE_LIST_INFINITE_LOOP_THRESHOLD) {
            SYSLOG_INTERNAL_WARNING_ONCE("modules_init: too many modules, maybe "
                                         "in a race");
            break;
        }
    }
    mark = &ldr->InMemoryOrderModuleList;
    ASSERT(mark->Flink != NULL && mark->Blink != NULL); /* sanity check */
    for (i = 0, e = mark->Flink; e != mark; i++, e = e->Flink) {
        mod = (LDR_MODULE *)((char *)e - offsetof(LDR_MODULE, InMemoryOrderModuleList));
        /* sanity check */
        ASSERT(e->Flink != NULL && e->Blink != NULL && e->Flink->Blink != NULL &&
               e->Blink->Flink != NULL);
        if ((app_pc)mod->BaseAddress == DRbase) {
            ASSERT(mod == DR_module);
            LOG(GLOBAL, LOG_ALL, 1, "Removing " PFX " %S from memory order module list\n",
                mod->BaseAddress, mod->FullDllName.Buffer);
            /* doubly linked circular list */
            e->Flink->Blink = e->Blink;
            e->Blink->Flink = e->Flink;
            break;
        }
        if (i > MAX_MODULE_LIST_INFINITE_LOOP_THRESHOLD) {
            SYSLOG_INTERNAL_WARNING_ONCE("modules_init: too many modules, maybe "
                                         "in a race");
            break;
        }
    }
    mark = &ldr->InInitializationOrderModuleList;
    ASSERT(mark->Flink != NULL && mark->Blink != NULL); /* sanity check */
    for (i = 0, e = mark->Flink; e != mark; i++, e = e->Flink) {
        mod = (LDR_MODULE *)((char *)e -
                             offsetof(LDR_MODULE, InInitializationOrderModuleList));
        /* sanity check */
        ASSERT(e->Flink != NULL && e->Blink != NULL && e->Flink->Blink != NULL &&
               e->Blink->Flink != NULL);
        if ((app_pc)mod->BaseAddress == DRbase) {
            ASSERT(mod == DR_module);
            LOG(GLOBAL, LOG_ALL, 1, "Removing " PFX " %S from init order module list\n",
                mod->BaseAddress, mod->FullDllName.Buffer);
            /* doubly linked circular list */
            e->Flink->Blink = e->Blink;
            e->Blink->Flink = e->Flink;
            break;
        }
        if (i > MAX_MODULE_LIST_INFINITE_LOOP_THRESHOLD) {
            SYSLOG_INTERNAL_WARNING_ONCE("modules_init: too many modules, maybe "
                                         "in a race");
            break;
        }
    }
    LOG(GLOBAL, LOG_ALL, 2, "After removing, module lists are:\n");
    DOLOG(2, LOG_ALL, { print_ldr_data(); });

    /* FIXME i#1429: also remove from hashtable used by GetModuleHandle */
}

/* N.B.: walking loader data structures at random times is dangerous!
 * Do not call this for non-debug reasons if you can help it!
 * See is_module_being_initialized for a safer approach to walking loader structs.
 * FIXME: other routines use the same iteration, should we abstract it out?
 * Other routines include check_for_unsupported_modules, loaded_modules_exports,
 * get_ldr_module_by_pc, and (in ntdll.c since used by pre-inject)
 * get_ldr_module_by_name (though the last two use the memory-order list)
 */
void
print_modules(file_t f, bool dump_xml)
{
    print_modules_ldrlist_and_ourlist(f, dump_xml, false /*not conservative*/);
}

/* conservative flag indicates we may have come here from a crash.  Print
 * information that do not need any allocations or lock acquisitions */
void
print_modules_ldrlist_and_ourlist(file_t f, bool dump_xml, bool conservative)
{
    /* We used to walk through every block in memory and call GetModuleFileName
     * That's not re-entrant, so instead we walk the loader's data structures in the PEB
     */
    PEB *peb = get_own_peb();
    PEB_LDR_DATA *ldr = peb->LoaderData;
    LIST_ENTRY *e, *mark;
    LDR_MODULE *mod;
    uint traversed = 0;
#ifdef DEBUG
    RTL_CRITICAL_SECTION *lock;
    thread_id_t owner;
#endif

    if (ldr == NULL) {
        ASSERT(dr_earliest_injected);
        return;
    }

#ifdef DEBUG
    lock = (RTL_CRITICAL_SECTION *)peb->LoaderLock;
    owner = (thread_id_t)lock->OwningThread;
    LOG(GLOBAL, LOG_ALL, 2, "LoaderLock owned by %d\n", owner);
    if (owner != 0 && owner != d_r_get_thread_id()) {
        LOG(GLOBAL, LOG_ALL, 1, "WARNING: print_modules called w/o holding LoaderLock\n");
        DOLOG_ONCE(2, LOG_ALL,
                   { SYSLOG_INTERNAL_WARNING("print_modules w/o holding LoaderLock"); });
    }
#endif

    print_file(f, dump_xml ? "<loaded-modules>\n" : "\nLoaded modules:\n");

    /* We use the memory order list instead of the init order list, as
     * it includes the .exe, and is updated first upon loading a new dll
     */
    mark = &ldr->InMemoryOrderModuleList;
    for (e = mark->Flink; e != mark; e = e->Flink) {
        uint checksum = 0;
        char *pe_name = NULL;
        app_pc preferred_base;
        version_info_t info;
        mod = (LDR_MODULE *)((char *)e - offsetof(LDR_MODULE, InMemoryOrderModuleList));
        get_module_info_pe(mod->BaseAddress, &checksum, NULL, NULL, &pe_name, NULL);
        preferred_base = get_module_preferred_base(mod->BaseAddress);
        print_file(f,
                   dump_xml ? "\t<dll range=\"" PFX "-" PFX "\" name=\"%ls\" "
                              "entry=\"" PFX "\" count=\"%-3d\"\n"
                              "\t     flags=\"0x%08x\" "
                              "timestamp=\"0x%08x\" checksum=\"0x%08x\" pe_name=\"%s\"\n"
                              "\t     path=\"%ls\" preferred_base=\"" PFX "\"\n"
                              "\t     dll_relocated=\"%s\" "
                            : "  " PFX "-" PFX " %-13ls entry=" PFX " count=%-3d\n"
                              "\tflags=0x%08x timestamp=0x%08x checksum=0x%08x\n"
                              "\tpe_name=%s  %ls\n\tpreferred_base=" PFX "\n"
                              "\tdll_relocated=%s\n",
                   mod->BaseAddress, (char *)mod->BaseAddress + mod->SizeOfImage - 1,
                   mod->BaseDllName.Buffer, mod->EntryPoint, mod->LoadCount, mod->Flags,
                   mod->TimeDateStamp, checksum, pe_name == NULL ? "(null)" : pe_name,
                   mod->FullDllName.Buffer, preferred_base,
                   preferred_base == (app_pc)mod->BaseAddress ? "no" : "yes");
        if (get_module_resource_version_info(mod->BaseAddress, &info)) {
            print_file(
                f,
                dump_xml
                    ? "file_version=\"%d.%d.%d.%d\" product_version=\"%d.%d.%d.%d\"\n"
                      "\t     original_filename=\"%S\" company_name=\"%S\"\n"
                      "\t     product_name=\"%S\" "
                    : "\tfile_version=%d.%d.%d.%d product_version=%d.%d.%d.%d"
                      "\toriginal_filename=%S\n\tcompany_name=%S"
                      " product_name=%S\n",
                info.file_version.version_parts.p1, info.file_version.version_parts.p2,
                info.file_version.version_parts.p3, info.file_version.version_parts.p4,
                info.product_version.version_parts.p1,
                info.product_version.version_parts.p2,
                info.product_version.version_parts.p3,
                info.product_version.version_parts.p4,
                info.original_filename == NULL ? L"none" : info.original_filename,
                info.company_name == NULL ? L"none" : info.company_name,
                info.product_name == NULL ? L"none" : info.product_name);
        } else {
            print_file(f,
                       dump_xml ? "no_version_information=\"true\" "
                                : "\tmodule_has_no_version_information\n");
        }
        if (dump_xml) {
            print_file(f, "/> \n");
        }
        if (traversed++ > MAX_MODULE_LIST_INFINITE_LOOP_THRESHOLD) {
            SYSLOG_INTERNAL_WARNING_ONCE("print_modules: too many modules");
            break;
        }
    }
    if (dump_xml)
        print_file(f, "</loaded-modules>\n");
    else
        print_file(f, "\n");

    /* FIXME: currently updated only under aslr_action */
    if (TEST(ASLR_DLL, DYNAMO_OPTION(aslr)) &&
        TEST(ASLR_TRACK_AREAS, DYNAMO_OPTION(aslr_action)) &&
        /* FIXME: xref case 10750: could print w/o lock inside a TRY  */
        !conservative) {
        print_file(f, "<print_modules_safe/>\n");
        if (is_module_list_initialized()) {
            print_modules_safe(f, dump_xml);
        }
    }
}

void
print_modules_safe(file_t f, bool dump_xml)
{
    module_iterator_t *mi;

    /* we walk our own module list that is populated on an initial walk through memory,
     * and further kept consistent on memory mappings of likely DLLs */
    print_file(f, dump_xml ? "<loaded-modules>\n" : "\nLoaded modules:\n");

    mi = module_iterator_start();
    while (module_iterator_hasnext(mi)) {
        module_area_t *ma = module_iterator_next(mi);
        print_file(f,
                   dump_xml ? "\t<dll range=\"" PFX "-" PFX "\" name=\"%ls\" "
                              "entry=\"" PFX "\" count=\"%-3d\"\n"
                              "\t     flags=\"0x%08x\" "
                              "timestamp=\"0x%08x\" checksum=\"0x%08x\" pe_name=\"%s\"\n"
                              "\t     path=\"%ls\" preferred_base=\"" PFX "\" />\n"
                            : "  " PFX "-" PFX " %-13ls entry=" PFX " count=%-3d\n"
                              "\tflags=0x%08x timestamp=0x%08x checksum=0x%08x\n"
                              "\tpe_name=%s  %ls\n\tpreferred_base=" PFX "\n",

                   ma->start, ma->end - 1, /* inclusive */
                   L"name",                /* FIXME: dll name is often quite useful */
                   ma->entry_point, 0 /* no LoadCount */, 0 /* no Flags */,
                   ma->os_data.timestamp, ma->os_data.checksum,
                   GET_MODULE_NAME(&ma->names) == NULL ? "(null)"
                                                       : GET_MODULE_NAME(&ma->names),
                   L"path", /* FIXME: path is often quite useful */
                   ma->os_data.preferred_base);
    }
    module_iterator_stop(mi);

    if (dump_xml)
        print_file(f, "</loaded-modules>\n");
    else
        print_file(f, "\n");
}

/* N.B.: see comments on print_modules about why this is a
 * dangerous routine, especially on a critical path like diagnostics...
 * FIXME!  Returns true if found an unsupported module, false otherwise
 */
bool
check_for_unsupported_modules()
{

    PEB *peb = get_own_peb();
    PEB_LDR_DATA *ldr = peb->LoaderData;
    LIST_ENTRY *e, *mark;
    LDR_MODULE *mod;
    char filter[MAXIMUM_PATH];
    char dllname[MAXIMUM_PATH];
    const char *short_name;
    uint traversed = 0;
    int retval =
        d_r_get_parameter(PARAM_STR(DYNAMORIO_VAR_UNSUPPORTED), filter, sizeof(filter));
    if (IS_GET_PARAMETER_FAILURE(retval) || filter[0] == 0 /* empty UNSUPPORTED list */) {
        /* no unsupported list, so nothing to look for */
        return false;
    }

    LOG(GLOBAL, LOG_ALL, 4, "check_for_unsupported_modules: %s\n", filter);
    /* FIXME: check peb->LoaderLock? */
    /* FIXME: share iteration w/ the other routines that do this? */
    mark = &ldr->InInitializationOrderModuleList;
    for (e = mark->Flink; e != mark; e = e->Flink) {
        mod = (LDR_MODULE *)((char *)e -
                             offsetof(LDR_MODULE, InInitializationOrderModuleList));
        wchar_to_char(dllname, MAXIMUM_PATH, mod->FullDllName.Buffer,
                      /* Length is size in bytes not counting final 0 */
                      mod->FullDllName.Length);
        short_name = get_short_name(dllname);
        LOG(GLOBAL, LOG_ALL, 4, "\tchecking %s => %s\n", dllname, short_name);
        if (check_filter(filter, short_name)) {
            /* critical since it's unrecoverable and to distinguish from attacks */
            /* dumpcore if warranted and not already dumped at the security
             * violation, options are already synchronized at the security
             * violation */
            SYSLOG(SYSLOG_CRITICAL, UNSUPPORTED_APPLICATION, 3, get_application_name(),
                   get_application_pid(), dllname);
            return true;
        }
        if (traversed++ > MAX_MODULE_LIST_INFINITE_LOOP_THRESHOLD) {
            SYSLOG_INTERNAL_WARNING_ONCE("check_for_unsupported_modules: "
                                         "too many modules");
            break;
        }
    }
    return false;
}

/* non-DEBUG routines for parsing PE files */

/* FIXME: make this a static inline function get_nt_header
 * that verifies and returns nt header */
#define DOS_HEADER(base) ((IMAGE_DOS_HEADER *)(base))
#define NT_HEADER(base) \
    ((IMAGE_NT_HEADERS *)((ptr_uint_t)(base) + DOS_HEADER(base)->e_lfanew))

#define VERIFY_DOS_HEADER(base)                                  \
    {                                                            \
        DEBUG_DECLARE(IMAGE_DOS_HEADER *dos = DOS_HEADER(base)); \
        ASSERT(dos->e_magic == IMAGE_DOS_SIGNATURE);             \
    }

#define VERIFY_NT_HEADER(base)                                                         \
    {                                                                                  \
        DEBUG_DECLARE(IMAGE_NT_HEADERS *nth = NT_HEADER(base));                        \
        VERIFY_DOS_HEADER(base);                                                       \
        ASSERT(nth != NULL && nth->Signature == IMAGE_NT_SIGNATURE);                   \
        ASSERT_CURIOSITY(nth->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC || \
                         nth->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC);  \
    }

/* returns true iff [start2, start2+size2] covers the same or a subset of the pages
 * covered by [start1, start1+size1]
 */
static inline bool
on_subset_of_pages(app_pc start1, size_t size1, app_pc start2, size_t size2)
{
    return (PAGE_START(start1) <= PAGE_START(start2) &&
            PAGE_START(start1 + size1) >= PAGE_START(start2 + size2));
}

bool
is_readable_pe_base(app_pc base)
{
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *nt;
    size_t size;
    /* would be nice to batch the is_readables into one, but we need
     * to dereference in turn...
     */
    if (!is_readable_without_exception((app_pc)dos, sizeof(*dos)) ||
        dos->e_magic != IMAGE_DOS_SIGNATURE)
        return false;
    nt = (IMAGE_NT_HEADERS *)(((ptr_uint_t)dos) + dos->e_lfanew);
    if (nt == NULL ||
        /* optimization: reduce number of system calls for safe reads */
        (!on_subset_of_pages((app_pc)dos, sizeof(*dos), (app_pc)nt, sizeof(*nt)) &&
         !is_readable_without_exception((app_pc)nt, sizeof(*nt))) ||
        nt->Signature != IMAGE_NT_SIGNATURE)
        return false;
    /* make sure section headers are readable */
    size = nt->FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER);
    if (/* optimization: reduce number of system calls for safe reads */
        !on_subset_of_pages((app_pc)dos, sizeof(*dos), (app_pc)IMAGE_FIRST_SECTION(nt),
                            size) &&
        !is_readable_without_exception((app_pc)IMAGE_FIRST_SECTION(nt), size))
        return false;
    return true;
}

/* Returns the size of the image section when loaded not counting alignment bytes
 * added by the image loader. */
static inline size_t
get_image_section_unpadded_size(IMAGE_SECTION_HEADER *sec _IF_DEBUG(IMAGE_NT_HEADERS *nt))
{
    ASSERT(sec != NULL && nt != NULL);
    /* Curiosity if VirtualSize/SizeOfRawData relationship doesn't match one of the
     * cases we've seen.  Note that this will fire for the (experimentally legal, but
     * never seen in practice) case of raw data much larger than virtual size (as in
     * past the next file alignment), though the various size routines here will handle
     * that correctly (see 5355, 9053). */
    ASSERT_CURIOSITY(
        sec->Misc.VirtualSize > sec->SizeOfRawData /* case 5355 */ ||
        sec->Misc.VirtualSize == 0 /* case 10501 */ ||
        ALIGN_FORWARD(sec->Misc.VirtualSize, nt->OptionalHeader.FileAlignment) ==
            ALIGN_FORWARD(sec->SizeOfRawData, /* case 8868 not always aligned */
                          nt->OptionalHeader.FileAlignment));
    ASSERT_CURIOSITY(sec->Misc.VirtualSize != 0 || sec->SizeOfRawData != 0);
    if (sec->Misc.VirtualSize == 0) /* case 10501 */
        return sec->SizeOfRawData;
    return sec->Misc.VirtualSize; /* case 5355 */
}

/* Returns the size in bytes of the image section when loaded, including image loader
 * allocated alignment/padding bytes.  To include non-allocated (MEM_RESERVE) padding
 * bytes align this value forward to nt->OptionalHeader.SectionAlignment (only comes
 * into play when SectionAlignment is > PAGE_SIZE). */
static inline size_t
get_image_section_size(IMAGE_SECTION_HEADER *sec, IMAGE_NT_HEADERS *nt)
{
    /* Xref case 9797, drivers (which we've seen mapped in on Vista) don't
     * usually use page size section alignment (use 0x80 alignment instead). */
    size_t unpadded_size = get_image_section_unpadded_size(sec _IF_DEBUG(nt));
    uint alignment = MIN((uint)PAGE_SIZE, nt->OptionalHeader.SectionAlignment);
    return ALIGN_FORWARD(unpadded_size, alignment);
}

/* Returns the size of the portion of the image file that's mapped into the image section
 * when it's loaded. */
static inline size_t
get_image_section_map_size(IMAGE_SECTION_HEADER *sec, IMAGE_NT_HEADERS *nt)
{
    /* Xref case 5355 - this is mapped in irregardless of sec->Characteristics flags
     * (including the UNINITIALIZED_DATA flag) so can ignore them. */
    size_t virtual_size = get_image_section_size(sec, nt);
    /* FileAlignment - the alignment factor (in bytes) that is used to align the raw data
     * of sections in the image file. The value should be a power of 2 between 512
     * (though note the lower bound is not enforced, xref 9798) and 64 K, inclusive. The
     * default is 512. If the SectionAlignment is less than the architecture's page size,
     * then FileAlignment must match SectionAlignment. */
    size_t raw_data_size =
        ALIGN_FORWARD(sec->SizeOfRawData, nt->OptionalHeader.FileAlignment);
    /* Xref 5355, the size of the mapping is the lesser of the virtual size (as
     * determined above) and the FileAlignment aligned size of SizeOfRawData. Any
     * extra space up to virtual size is 0 filled. */
    return MIN(virtual_size, raw_data_size);
}

/* returns the offset into the PE file at which the mapping for section sec starts */
static inline size_t
get_image_section_file_offs(IMAGE_SECTION_HEADER *sec, IMAGE_NT_HEADERS *nt)
{
    ASSERT(sec != NULL && nt != NULL);
    /* Xref 5355, despite PE specifications it appears that PointerToRawData is not
     * required to be aligned (the image loader apparently back aligns it before use). */
    return ALIGN_BACKWARD(sec->PointerToRawData, nt->OptionalHeader.FileAlignment);
}

void
print_module_section_info(file_t file, app_pc addr)
{
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    IMAGE_SECTION_HEADER *sec;
    uint i;
    app_pc module_base = get_module_base(addr);

    if (module_base == NULL)
        return;

    dos = (IMAGE_DOS_HEADER *)module_base;
    nt = (IMAGE_NT_HEADERS *)(((ptr_uint_t)dos) + dos->e_lfanew);
    sec = IMAGE_FIRST_SECTION(nt);
    /* FIXME : can we share this loop with is_in_executable_file_section? */
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        app_pc sec_start = module_base + sec->VirtualAddress;
        app_pc sec_end =
            module_base + sec->VirtualAddress + get_image_section_size(sec, nt);

        /* xref case 6799, section is [start, end) */
        if (sec_start <= addr && addr < sec_end) {
            print_file(file,
                       "\t\tmod_base=            \"" PFX "\"\n"
                       "\t\tsec_name=            \"%.*s\"\n"
                       "\t\tsec_start=           \"" PFX "\"\n"
                       "\t\tsec_end=             \"" PFX "\"\n"
                       "\t\tVirtualSize=         \"0x%08x\"\n"
                       "\t\tSizeOfRawData=       \"0x%08x\"\n"
                       "\t\tsec_characteristics= \"0x%08x\"\n",
                       module_base, IMAGE_SIZEOF_SHORT_NAME, sec->Name, sec_start,
                       sec_end, sec->Misc.VirtualSize, sec->SizeOfRawData,
                       sec->Characteristics);
        }
    }
}

/* Looks for a section or (if merge) group of sections that satisfies the following
 * criteria:
 * - if start_pc != NULL, that contains [start_pc, end_pc);
 * - if sec_characteristics_match != 0, that matches ANY of sec_characteristics_match;
 * - if name != NULL, that matches name.
 * - if nth > -1, the nth section, or nth segment if merge=true
 *
 * If a section or (if merge) group of sections are found that satisfy the above,
 * then returns the bounds of the section(s) in sec_start_out and sec_end_out
 * and sec_end_unpad_out (end w/o padding for alignment) (all 3 are
 * optional) and returns true.  If no matching section(s) are found returns false.
 * If !merge the actual characteristics are returned in sec_characteristics_out,
 * which is optional and must be NULL if merge.
 * If map_size, *sec_end_out will be the portion of the file that is mapped
 * (but sec_end_nopad_out will be unchanged).
 *
 * FIXME - with case 10526 fix letting the exemption polices trim to section boundaries
 * is there any reason we still need merging support?
 */
static bool
is_in_executable_file_section(app_pc module_base, app_pc start_pc, app_pc end_pc,
                              app_pc *sec_start_out /* OPTIONAL OUT */,
                              app_pc *sec_end_out /* OPTIONAL OUT */,
                              app_pc *sec_end_nopad_out /* OPTIONAL OUT */,
                              uint *sec_characteristics_out /* OPTIONAL OUT */,
                              IMAGE_SECTION_HEADER *sec_header_out /* OPTIONAL OUT */,
                              uint sec_characteristics_match /* TESTANY, 0 to ignore */,
                              const char *name /* OPTIONAL */, bool merge,
                              int nth /* -1 to ignore */, bool map_size)
{
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    IMAGE_SECTION_HEADER *sec;
    uint i, seg_num = 0, prev_chars = 0;
    bool prev_sec_same_chars = false, result = false, stop_at_next_non_matching = false;
    app_pc sec_start = NULL, sec_end = NULL, sec_end_nopad = NULL;

    /* See case 7998 where a NULL base was passed. */
    ASSERT_CURIOSITY(module_base != NULL);
    if (module_base == NULL)
        return false;

    dos = (IMAGE_DOS_HEADER *)module_base;
    nt = (IMAGE_NT_HEADERS *)(((ptr_uint_t)dos) + dos->e_lfanew);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || nt == NULL ||
        nt->Signature != IMAGE_NT_SIGNATURE)
        return false;
    /* must specify some criteria */
    ASSERT(start_pc != NULL || sec_characteristics_match != 0 || name != NULL ||
           nth > -1);
    ASSERT(start_pc == NULL || start_pc < end_pc);
    /* sec_characteristics_out & section_header_out only makes sense if !merge,
     * unless doing nth segment
     */
    ASSERT(sec_characteristics_out == NULL || !merge || nth > -1);
    ASSERT(sec_header_out == NULL || !merge);
    /* We cannot use the OptionalHeader fields BaseOfCode or SizeOfCode or SizeOfData
     * since for multiple sections the SizeOfCode is the sum of the
     * non-page-align-expanded sizes, and sections need not be contiguous!
     * Instead we walk all sections for ones that match our criteria. */
    LOG(GLOBAL, LOG_VMAREAS, 4, "module @ " PFX ":\n", module_base);
    sec = IMAGE_FIRST_SECTION(nt);
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        LOG(GLOBAL, LOG_VMAREAS, 4, "\tName = %.*s\n", IMAGE_SIZEOF_SHORT_NAME,
            sec->Name);
        LOG(GLOBAL, LOG_VMAREAS, 4, "\tVirtualSize    = " PFX "\n",
            sec->Misc.VirtualSize);
        LOG(GLOBAL, LOG_VMAREAS, 4, "\tVirtualAddress = " PFX "\n", sec->VirtualAddress);
        LOG(GLOBAL, LOG_VMAREAS, 4, "\tSizeOfRawData  = 0x%08x\n", sec->SizeOfRawData);
        LOG(GLOBAL, LOG_VMAREAS, 4, "\tCharacteristics= 0x%08x\n", sec->Characteristics);

        if ((sec_characteristics_match == 0 ||
             TESTANY(sec_characteristics_match, sec->Characteristics)) &&
            (name == NULL ||
             (sec->Name != NULL &&
              strncmp((const char *)sec->Name, name, strlen(name)) == 0)) &&
            (nth == -1 || nth == (int)seg_num)) {
            app_pc new_start = module_base + sec->VirtualAddress;
            if (prev_sec_same_chars && sec_end == new_start &&
                (nth == -1 || prev_chars == sec->Characteristics)) {
                /* os will merge adjacent regions w/ same privileges, so consider
                 * these one region by leaving sec_start at its old value if merge */
                ASSERT(merge);
                LOG(GLOBAL, LOG_VMAREAS, 2,
                    "is_in_executable_file_section: adjacent sections @" PFX " and " PFX
                    "\n",
                    sec_start, new_start);
            } else {
                if (stop_at_next_non_matching)
                    break;
                sec_start = new_start;
            }
            if (merge)
                prev_sec_same_chars = true;
            sec_end = module_base + sec->VirtualAddress + get_image_section_size(sec, nt);
            sec_end_nopad = module_base + sec->VirtualAddress +
                get_image_section_unpadded_size(sec _IF_DEBUG(nt));
            LOG(GLOBAL, LOG_VMAREAS, 2,
                "is_in_executable_file_section (module " PFX ", region " PFX "-" PFX "): "
                "%.*s == " PFX "-" PFX "\n",
                module_base, start_pc, end_pc, IMAGE_SIZEOF_SHORT_NAME, sec->Name,
                sec_start, sec_end);
            if (start_pc == NULL || (start_pc >= sec_start && start_pc <= sec_end)) {
                if (sec_start_out != NULL)
                    *sec_start_out = sec_start; /* merged section start */
                if (sec_end_out != NULL) {
                    if (map_size) {
                        *sec_end_out = module_base + sec->VirtualAddress +
                            get_image_section_map_size(sec, nt);
                    } else {
                        *sec_end_out = sec_end; /* merged section end */
                    }
                }
                if (sec_end_nopad_out != NULL)
                    *sec_end_nopad_out = sec_end_nopad; /* merged nopad section end */
                if (sec_characteristics_out != NULL)
                    *sec_characteristics_out = sec->Characteristics;
                if (sec_header_out != NULL)
                    *sec_header_out = *sec;
                if (start_pc == NULL || end_pc <= sec_end) {
                    /* we found what we were looking for, stop looping as soon as we
                     * finish merging into the current region. */
                    result = true;
                    if (merge)
                        stop_at_next_non_matching = true;
                    else
                        break;
                }
            }
        } else {
            prev_sec_same_chars = false;
            if (nth > -1 && i > 0) {
                /* count segments */
                app_pc new_start = module_base + sec->VirtualAddress;
                if (sec_end != new_start || prev_chars != sec->Characteristics)
                    seg_num++;
                sec_end =
                    module_base + sec->VirtualAddress + get_image_section_size(sec, nt);
                sec_end_nopad = module_base + sec->VirtualAddress +
                    get_image_section_unpadded_size(sec _IF_DEBUG(nt));
            }
        }
        prev_chars = sec->Characteristics;
    }
    return result;
}

bool
module_pc_section_lookup(app_pc module_base, app_pc pc, IMAGE_SECTION_HEADER *section_out)
{
    ASSERT(is_readable_pe_base(module_base));
    if (section_out != NULL)
        memset(section_out, 0, sizeof(*section_out));
    return is_in_executable_file_section(module_base, pc, pc + 1, NULL, NULL, NULL, NULL,
                                         section_out, 0 /* any section */, NULL, false,
                                         -1, false);
}

/* Returns true if [start_pc, end_pc) is within a single code section.
 * Returns the bounds of the enclosing section in sec_start and sec_end.
 * Note that unlike is_in_*_section routines, does not merge sections. */
bool
is_range_in_code_section(app_pc module_base, app_pc start_pc, app_pc end_pc,
                         app_pc *sec_start /* OPTIONAL OUT */,
                         app_pc *sec_end /* OPTIONAL OUT */)
{
    return is_in_executable_file_section(module_base, start_pc, end_pc, sec_start,
                                         sec_end, NULL, NULL, NULL, IMAGE_SCN_CNT_CODE,
                                         NULL, false /* don't merge */, -1, false);
}

/* Returns true if addr is in a code section and if so returns in sec_start and sec_end
 * the bounds of the section containing addr (merged with adjacent code sections). */
bool
is_in_code_section(app_pc module_base, app_pc addr, app_pc *sec_start /* OPTIONAL OUT */,
                   app_pc *sec_end /* OPTIONAL OUT */)
{
    return is_in_executable_file_section(module_base, addr, addr + 1, sec_start, sec_end,
                                         NULL, NULL, NULL, IMAGE_SCN_CNT_CODE, NULL,
                                         true /* merge */, -1, false);
}

/* Same as above only for initialized data sections instead of code. */
bool
is_in_dot_data_section(app_pc module_base, app_pc addr,
                       app_pc *sec_start /* OPTIONAL OUT */,
                       app_pc *sec_end /* OPTIONAL OUT */)
{
    return is_in_executable_file_section(
        module_base, addr, addr + 1, sec_start, sec_end, NULL, NULL, NULL,
        IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_CNT_UNINITIALIZED_DATA, NULL,
        true /* merge */, -1, false);
}

/* Same as above only for xdata sections (see below) instead of code. */
bool
is_in_xdata_section(app_pc module_base, app_pc addr, app_pc *sec_start /* OPTIONAL OUT */,
                    app_pc *sec_end /* OPTIONAL OUT */)
{
    /* .xdata is present in .NET2.0 .ni.dll files
     * it is marked as +rwx initialized data
     */
    uint sec_flags = 0;
    if (is_in_executable_file_section(module_base, addr, addr + 1, sec_start, sec_end,
                                      NULL, &sec_flags, NULL,
                                      IMAGE_SCN_CNT_INITIALIZED_DATA, ".xdata",
                                      false /* don't merge */, -1, false)) {
        bool xdata_prot_match = TESTALL(
            IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE, sec_flags);
        ASSERT_CURIOSITY(xdata_prot_match && "unexpected xdata section characteristics");
        return xdata_prot_match;
    }
    return false;
}

/* This is a more restrictive test than (get_module_base() != NULL) because it
 * checks for the start of the PE and examines at least one section in it before
 * concluding that addr belongs to that module. */
bool
is_in_any_section(app_pc module_base, app_pc addr, app_pc *sec_start /* OPTIONAL OUT */,
                  app_pc *sec_end /* OPTIONAL OUT */)
{
    return is_in_executable_file_section(module_base, addr, addr + 1, sec_start, sec_end,
                                         NULL, NULL, NULL, 0 /* any section */, NULL,
                                         true /* merge */, -1, false);
}

bool
get_executable_segment(app_pc module_base, app_pc *sec_start /* OPTIONAL OUT */,
                       app_pc *sec_end /* OPTIONAL OUT */,
                       app_pc *sec_end_nopad /* OPTIONAL OUT */)
{
    return is_in_executable_file_section(module_base, NULL, NULL, sec_start, sec_end,
                                         sec_end_nopad, NULL, NULL, IMAGE_SCN_MEM_EXECUTE,
                                         NULL, true /* merge */, -1, false);
}

/* allow only true MEM_IMAGE mappings */
bool
is_mapped_as_image(app_pc module_base)
{
    MEMORY_BASIC_INFORMATION mbi;

    if (query_virtual_memory(module_base, &mbi, sizeof(mbi)) == sizeof(mbi) &&
        mbi.State == MEM_COMMIT /* header should always be committed */
        && mbi.Type == MEM_IMAGE) {
        return true;
    }

    /* although mbi.Type may be undefined, most callers should get
     * this far only if not MEM_FREE, so ok to ASSERT.  Note all
     * Type's are MEM_FREE, MEM_PRIVATE, MEM_MAPPED, and MEM_IMAGE */
    ASSERT_CURIOSITY(mbi.Type == MEM_PRIVATE || mbi.Type == MEM_MAPPED);

    return false;
}

/* Returns true if the module has an nth segment, false otherwise. */
bool
module_get_nth_segment(app_pc module_base, uint n, app_pc *start /*OPTIONAL OUT*/,
                       app_pc *end /*OPTIONAL OUT*/, uint *chars /*OPTIONAL OUT*/)
{
    if (!is_in_executable_file_section(
            module_base, NULL, NULL, start, end, NULL, chars, NULL, 0 /* any section */,
            NULL, true /* merge to make segments */, n, true /*mapped size*/)) {
        return false;
    }
    return true;
}

size_t
module_get_header_size(app_pc module_base)
{
    IMAGE_NT_HEADERS *nt;
    ASSERT(is_readable_pe_base(module_base));
    nt = NT_HEADER(module_base);
    return nt->OptionalHeader.SizeOfHeaders;
}

/* returns true if a matching section is found, false otherwise */
bool
get_named_section_bounds(app_pc module_base, const char *name,
                         app_pc *start /*OPTIONAL OUT*/, app_pc *end /*OPTIONAL OUT*/)
{
    if (!is_in_executable_file_section(module_base, NULL, NULL, start, end, NULL, NULL,
                                       NULL, 0 /* any section */, name, true /* merge */,
                                       -1, false)) {
        if (start != NULL)
            *start = NULL;
        if (end != NULL)
            *end = NULL;
        return false;
    }
    return true;
}

bool
get_IAT_section_bounds(app_pc module_base, app_pc *iat_start, app_pc *iat_end)
{
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)module_base;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(((ptr_uint_t)dos) + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY *dir;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || nt == NULL ||
        nt->Signature != IMAGE_NT_SIGNATURE)
        return false;
    dir = &OPT_HDR(nt, DataDirectory)[IMAGE_DIRECTORY_ENTRY_IAT];
    *iat_start = module_base + dir->VirtualAddress;
    *iat_end = module_base + dir->VirtualAddress + dir->Size;
    return true;
}

bool
is_IAT(app_pc start, app_pc end, bool page_align, app_pc *iat_start, app_pc *iat_end)
{
    app_pc IAT_start, IAT_end;
    app_pc base = get_module_base(start);
    if (base == NULL)
        return false;
    if (!get_IAT_section_bounds(base, &IAT_start, &IAT_end))
        return false;
    if (iat_start != NULL)
        *iat_start = IAT_start;
    if (iat_end != NULL)
        *iat_end = IAT_end;
    if (page_align) {
        IAT_start = (app_pc)ALIGN_BACKWARD(IAT_start, PAGE_SIZE);
        IAT_end = (app_pc)ALIGN_FORWARD(IAT_end, PAGE_SIZE);
    }
    LOG(THREAD_GET, LOG_VMAREAS, 3,
        "is_IAT(" PFX "," PFX ") vs (" PFX "," PFX ") == %d\n", start, end, IAT_start,
        IAT_end, IAT_start == start && IAT_end == end);
    return (IAT_start == start && IAT_end == end);
}

bool
is_in_IAT(app_pc addr)
{
    app_pc IAT_start, IAT_end;
    app_pc base = get_module_base(addr);
    if (base == NULL)
        return false;
    if (!get_IAT_section_bounds(base, &IAT_start, &IAT_end))
        return false;
    return (IAT_start <= addr && addr < IAT_end);
}

app_pc
get_module_entry(app_pc module_base)
{
    /* N.B.: do not use imagehlp routines like ImageNtHeader here, since the
     * dependency on that dll causes sqlsrvr to crash.
     * It's not that hard to directly read the headers.
     */
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)module_base;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(((ptr_uint_t)dos) + dos->e_lfanew);
    ASSERT(is_readable_pe_base(module_base));
    ASSERT(dos->e_magic == IMAGE_DOS_SIGNATURE);
    ASSERT(nt != NULL && nt->Signature == IMAGE_NT_SIGNATURE);
    /* note that the entry point for .NET executables is clobbered by
     * mscoree.dll to point directly at either mscoree!_CorDllMain or
     * mscoree!_CorExeMain (though the LDR_MODULE struct entry is still the
     * original one), so don't assume that it's inside the PE module itself
     * (see case 3714)
     */
    return ((app_pc)dos) + nt->OptionalHeader.AddressOfEntryPoint;
}

app_pc
get_module_base(app_pc pc)
{
    /* We get the base from the allocation region.  We cannot simply back-align
     * to 64K, the Windows allocation granularity on all platforms, since some
     * modules have code sections beyond 64K from the start of the module.
     */
    app_pc base = get_allocation_base(pc);
    if (!is_readable_pe_base(base)) {
        /* not readable, or not PE */
        return NULL;
    }
    return base;
}

/* gets the preferred base of the module containing pc from PE header */
app_pc
get_module_preferred_base(app_pc pc)
{
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    app_pc module_base = get_allocation_base(pc);

    if (!is_readable_pe_base(module_base))
        return NULL;
    dos = (IMAGE_DOS_HEADER *)module_base;
    nt = (IMAGE_NT_HEADERS *)(((ptr_uint_t)dos) + dos->e_lfanew);
    /* we return NULL on error above, make sure no one actually sets their
     * preferred base address to NULL */
    ASSERT_CURIOSITY(OPT_HDR(nt, ImageBase) != 0);
    return (app_pc)(ptr_int_t)OPT_HDR(nt, ImageBase);
}

/* we simply test if allocation bases of a region are the same */
bool
in_same_module(app_pc target, app_pc source)
{
    app_pc target_base = get_allocation_base(target);
    app_pc source_base = get_allocation_base(source);
    LOG(THREAD_GET, LOG_VMAREAS, 2,
        "in_same_module(" PFX "," PFX ") => (" PFX "," PFX ") == %d\n", target, source,
        target_base, source_base, (target_base == source_base));
    /* all unallocated memory regions will get a base of 0 */
    return (target_base != NULL) && (target_base == source_base);
}

/* Use get_module_short_name for arbitrary pcs -- only call this if
 * you KNOW this is the base addr of a non-executable module, as it
 * bypasses some safety checks in get_module_short_name to avoid 4
 * system calls.
 * Returns the short module name from the PE exports section, or NULL if invalid
 */
char *
get_dll_short_name(app_pc base_addr)
{
    /* FIXME: We'll have a name pointer in a DLL that may get unloaded
       by another thread, so it would be nice to synchronize this call
       with UnmapViewOfSection so that we can get a safe copy of the name.

       FIXME: How can make sure we can't fail on strncpy(buf, name, max_chars);
       we can't test is_readable_without_exception(dll_name, max_chars)
       because our max_chars may be too long and of course we can't use strlen(),
       a TRY block would work.

       For now we avoid copying altogether and callers are expected to
       synchronize with DLL unloads, or otherwise to be ready to take the risk.

       Nearly all callers should be looking up in the loaded_module_areas vector
       and using the copy there, which is copied under TRY/EXCEPT, so the
       racy window while update_module_list() copies from here is now safe.
    */
    IMAGE_EXPORT_DIRECTORY *exports;
    ASSERT(base_addr == get_allocation_base(base_addr) && is_readable_pe_base(base_addr));
    exports = get_module_exports_directory(base_addr, NULL);
    if (exports != NULL) {
        char *dll_name = (char *)(base_addr + exports->Name /* RVA */);
        /* sanity check whether really MEM_IMAGE, but too late */
        if (!is_string_readable_without_exception(dll_name, NULL)) {
            ASSERT_CURIOSITY(false && "Exports name not readable, partial map?" ||
                             EXEMPT_TEST("win32.partial_map.exe"));
            dll_name = NULL;
        }
        LOG(THREAD_GET, LOG_SYMBOLS, 3,
            "get_dll_short_name(base_addr=" PFX ") exports=%d dll_name=%s\n", base_addr,
            exports, dll_name == NULL ? "<invalid>" : dll_name);
        return dll_name;
    }

    return NULL;
}

/* Get all possible names for the module corresponding to pc.  Part of fix for
 * case 9842.  We have to maintain all different module names, can't just use
 * a precedence rule for deciding at all points.
 * The ma parameter is optional: if set, ma->full_path is set.
 */
static void
get_all_module_short_names_uncached(dcontext_t *dcontext, app_pc pc, bool at_map,
                                    module_names_t *names, module_area_t *ma,
                                    version_info_t *info, /*OPTIONAL IN*/
                                    const char *file_path HEAPACCT(which_heap_t which))
{
    const char *name;
    app_pc base;
    char buf[MAXIMUM_PATH];

    ASSERT(names != NULL);
    if (names == NULL)
        return;
    memset(names, 0, sizeof(*names));

    base = get_allocation_base(pc);
    LOG(THREAD_GET, LOG_VMAREAS, 5,
        "get_all_module_short_names_uncached: start " PFX " -> base " PFX "\n", pc, base);
    if (!is_readable_pe_base(base)) {
        LOG(THREAD_GET, LOG_VMAREAS, 5,
            "get_all_module_short_names_uncached: not a module\n");
        return;
    }
#ifndef X64
    if (module_is_64bit(base)) {
        /* For 32-bit dr we ignore 64-bit dlls in a wow64 process. */
        ASSERT_CURIOSITY(is_wow64_process(NT_CURRENT_PROCESS));
        LOG(THREAD_GET, LOG_VMAREAS, 5,
            "get_all_module_short_names_uncached: ignoring 64-bit module in "
            "wow64 process\n");
        return;
    }
#endif
    /* FIXME: we do have a race here where the module can be unloaded
     * before we finish making a copy of its name
     */
    if (dynamo_exited)
        return; /* no heap for strdup */

    /* Ensure we don't crash if a dll is unloaded racily underneath us */
    TRY_EXCEPT_ALLOW_NO_DCONTEXT(
        dcontext,
        {
            app_pc process_image;

            /* Choice #1: PE exports name */
            name = get_dll_short_name(base);
            if (name != NULL)
                names->module_name = dr_strdup(name HEAPACCT(which));
            else
                names->module_name = NULL;

            /* Choice #2: executable qualified name
             * This would be the last choice except historically it's been #2 so
             * we'll stick with that.
             * check if target is in process image -
             * in which case we use our unqualified name for the executable
             */
            process_image = get_own_peb()->ImageBaseAddress;

            /* check if pc region base matches the image base  */
            /* FIXME: they should be aligned anyways, can remove this */
            ASSERT(ALIGNED(process_image, PAGE_SIZE) && ALIGNED(base, PAGE_SIZE));
            if (process_image == base) {
                name = get_short_name(get_application_name());
                if (name != NULL)
                    names->exe_name = dr_strdup(name HEAPACCT(which));
                else
                    names->exe_name = NULL;
            }

            /* Choice #3: .rsrc original filename, already strduped */
            names->rsrc_name =
                (char *)get_module_original_filename(base, info HEAPACCT(which));

            /* Choice #4: file name
             * At init time it's safe enough to walk loader list.  At run time
             * we rely on being at_map and using -track_module_filenames which
             * will result in a non-NULL file_path parameter.
             */
            if (file_path != NULL) {
                name = get_short_name(file_path);
                if (ma != NULL)
                    ma->full_path = dr_strdup(file_path HEAPACCT(which));
            } else if (!dynamo_initialized) {
                const char *path = buf;
                buf[0] = '\0';
                get_module_name(base, buf, BUFFER_SIZE_ELEMENTS(buf));
                if (buf[0] == '\0' && is_in_dynamo_dll(base))
                    path = get_dynamorio_library_path();
                if (path[0] == '\0' && is_in_client_lib(base))
                    path = get_client_path_from_addr(base);
                if (path[0] == '\0' && INTERNAL_OPTION(private_loader)) {
                    privmod_t *privmod;
                    acquire_recursive_lock(&privload_lock);
                    privmod = privload_lookup_by_base(base);
                    if (privmod != NULL) {
                        dr_snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%s", privmod->path);
                        path = buf;
                    }
                    release_recursive_lock(&privload_lock);
                }
                if (path[0] != '\0')
                    name = get_short_name(path);
                /* Set the path too.  We could avoid a strdup by sharing the
                 * same alloc w/ the short name, but simpler to separate.
                 */
                if (ma != NULL)
                    ma->full_path = dr_strdup(path HEAPACCT(which));
            }
            if (name != NULL)
                names->file_name = dr_strdup(name HEAPACCT(which));
            else
                names->file_name = NULL;

            DOLOG(3, LOG_VMAREAS, {
                LOG(GLOBAL, LOG_INTERP | LOG_VMAREAS, 1,
                    "get_all_module_short_names_uncached " PFX ":\n", base);
                LOG(GLOBAL, LOG_INTERP | LOG_VMAREAS, 1, "\tPE name=%s\n",
                    names->module_name == NULL ? "<unavailable>" : names->module_name);
                LOG(GLOBAL, LOG_INTERP | LOG_VMAREAS, 1, "\texe name=%s\n",
                    names->exe_name == NULL ? "<unavailable>" : names->exe_name);
                LOG(GLOBAL, LOG_INTERP | LOG_VMAREAS, 1, "\t.rsrc original filename=%s\n",
                    names->rsrc_name == NULL ? "<unavailable>" : names->rsrc_name);
                if (at_map && DYNAMO_OPTION(track_module_filenames) && dcontext != NULL) {
                    LOG(GLOBAL, LOG_INTERP | LOG_VMAREAS, 1, "\tfilename=%s\n",
                        names->file_name == NULL ? "<unavailable>" : names->file_name);
                }
            });
        },
        {
            /* Free all allocations in the event of an exception and return NULL
             * for all names. */
            free_module_names(names HEAPACCT(which));
            memset(names, 0, sizeof(*names));
        });

    /* theoretically possible to fail, since section matching can be thwarted,
     * or if we came in late
     */
    ASSERT_CURIOSITY(
        names->module_name != NULL || names->exe_name != NULL ||
        names->rsrc_name != NULL || names->file_name != NULL || !at_map ||
        /* PR 229284: a partial map can cause this */
        check_filter("win32.partial_map.exe", get_short_name(get_application_name())));
}

/* Caller should use get_module_short_name() unless calling before or after
 * we set up the loaded_module_areas vector.
 *
 * If map is true, this routine finds our official internal name for a
 * module, which is done in this priority order:
 * 1) PE exports name
 * 2) If pc is in the main executable image we use our fully qualified name
 * 3) .rsrc original file name
 * 4) if at_map, file name; else unavailable
 *    FIXME: is PEB->SubSystemData = PathFileName.Buffer (see
 *    notes in aslr_generate_relocated_section()) available w/o a debugger,
 *    and should we check whether it equals our stored name?
 *
 * 1 and 2 need not be present, and 3 can be invalid if the app creates multiple
 * sections before mapping any, so we can have a NULL name for a module.
 * Also returns NULL if pc is not in a valid module.
 * This name precedence is established by GET_MODULE_NAME().
 *
 * The name string is dr_strduped with HEAPACCT(which) and must be freed
 * by the caller calling dr_strfree.
 */
const char *
get_module_short_name_uncached(dcontext_t *dcontext, app_pc pc,
                               bool at_map HEAPACCT(which_heap_t which))
{
    module_names_t names = { 0 };
    const char *res;

    get_all_module_short_names_uncached(dcontext, pc, at_map, &names, NULL, NULL,
                                        NULL HEAPACCT(which));
    res = dr_strdup(GET_MODULE_NAME(&names) HEAPACCT(which));
    free_module_names(&names HEAPACCT(which));
    return res;
}

/* All internal uses of module names should be calling this routine,
 * which not only looks up the cached name but uses the priority-order
 * naming scheme that avoids modules without names, rather than
 * explicitly get_dll_short_name() (PE name only) or the other
 * individual name gathering routines.
 * For safety this routine makes a copy of the name.
 * For more-performant uses, use os_get_module_name(), which allows the caller
 * to hold a lock and use the original string.
 */
const char *
get_module_short_name(app_pc pc HEAPACCT(which_heap_t which))
{
    /* Our module list name is the short name */
    return os_get_module_name_strdup(pc HEAPACCT(which));
}

/* If the PC resides in a module that has been relocated from
 * its preferred base, get_module_preferred_base_delta() returns
 * the delta of the preferred base and its actual base (used to
 * normalize the Threat ID).  If the PC does not reside in a
 * module or it is invalid, the function returns 0 (since there
 * is no need to normalize the Threat ID in those cases).
 */
ssize_t
get_module_preferred_base_delta(app_pc pc)
{
    app_pc preferred_base_addr = get_module_preferred_base(pc);
    app_pc current_base_addr = get_allocation_base(pc);
    /* FIXME : optimization add out argument to get_module_preferred_base to
     * return the allocation base */

    if (preferred_base_addr == NULL || current_base_addr == NULL)
        return 0;

    return (preferred_base_addr - current_base_addr);
}

/* returns 0 if no loader module is found
 * N.B.: walking loader data structures at random times is dangerous!
 */
LDR_MODULE *
get_ldr_module_by_pc(app_pc pc)
{
    PEB *peb = get_own_peb();
    PEB_LDR_DATA *ldr = peb->LoaderData;
    LIST_ENTRY *e, *mark;
    LDR_MODULE *mod;
    uint traversed = 0; /* a simple infinite loop break out */
#ifdef DEBUG
    RTL_CRITICAL_SECTION *lock;
    thread_id_t owner;
#endif

    if (ldr == NULL) {
        ASSERT(dr_earliest_injected);
        return NULL;
    }

#ifdef DEBUG
    lock = (RTL_CRITICAL_SECTION *)peb->LoaderLock;
    owner = (thread_id_t)lock->OwningThread;
    if (owner != 0 && owner != d_r_get_thread_id()) {
        /* This will be a risky operation but we'll live with it.
           In case we walk in a list in an inconsistent state
           1) we may get trapped in an infinite loop when following a partially updated
              list so we'll bail out in case of a deep loop
           2) list entries and pointed data may be removed and even deallocated
              we can't just check for is_readable_without_exception
              since it won't help if we're in a race
           FIXME: we should mark we started this routine,
              and if we get an exception retry or give up gracefully
        */
        LOG(GLOBAL, LOG_ALL, 3, "WARNING: get_ldr_module_by_pc w/o holding LoaderLock\n");
        DOLOG_ONCE(2, LOG_ALL, {
            SYSLOG_INTERNAL_WARNING("get_ldr_module_by_pc w/o holding LoaderLock");
        });
    }
#endif

    /* Now, you'd think these would actually be in memory order, but they
     * don't seem to be for me!
     */
    mark = &ldr->InMemoryOrderModuleList;

    for (e = mark->Flink; e != mark; e = e->Flink) {
        app_pc start, end;
        mod = (LDR_MODULE *)((char *)e - offsetof(LDR_MODULE, InMemoryOrderModuleList));
        start = (app_pc)mod->BaseAddress;
        end = start + mod->SizeOfImage;
        if (pc >= start && pc < end) {
            return mod;
        }

        if (traversed++ > MAX_MODULE_LIST_INFINITE_LOOP_THRESHOLD) {
            LOG(GLOBAL, LOG_ALL, 1,
                "WARNING: get_ldr_module_by_pc too many modules, or an infinte loop due "
                "to a race\n");
            SYSLOG_INTERNAL_WARNING_ONCE("get_ldr_module_by_pc too many modules");
            /* TODO: In case we ever hit this we may want to retry the traversal
             * once more.
             */
            return NULL;
        }
    }
    return NULL;
}

/* N.B.: walking loader data structures at random times is dangerous!
 * Do not call this for non-debug reasons if you can help it!
 * See get_module_status for a safer approach to walking loader structs.
 */
void
get_module_name(app_pc pc, char *buf, int max_chars)
{
    LDR_MODULE *mod = get_ldr_module_by_pc(pc);
    /* FIXME i#812: at earliest inject point this doesn't work: hardcode ntdll.dll? */
    if (mod != NULL) {
        wchar_to_char(buf, max_chars, mod->FullDllName.Buffer, mod->FullDllName.Length);
        return;
    }
    buf[0] = '\0';
}

static IMAGE_BASE_RELOCATION *
get_module_base_reloc(app_pc module_base, size_t *base_reloc_size /* OPTIONAL OUT */)
{
    IMAGE_NT_HEADERS *nt;
    IMAGE_DATA_DIRECTORY *base_reloc_dir = NULL;
    app_rva_t base_reloc_vaddr = 0;
    size_t size = 0;
    IMAGE_BASE_RELOCATION *base_reloc = NULL;

    VERIFY_NT_HEADER(module_base);
    /* callers should have done this in release builds */
    ASSERT(is_readable_pe_base(module_base));

    nt = NT_HEADER(module_base);
    base_reloc_dir = OPT_HDR(nt, DataDirectory) + IMAGE_DIRECTORY_ENTRY_BASERELOC;

    if (base_reloc_size != NULL)
        *base_reloc_size = 0;

    /* Don't expect base_reloc_dir to be NULL, but to be safe */
    if (base_reloc_dir == NULL) {
        ASSERT_CURIOSITY(false && "DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC] NULL");
        /* base_reloc_size set to 0 */
        return NULL;
    }

    /* sanity check */
    ASSERT(is_readable_without_exception((app_pc)base_reloc_dir, 8));

    base_reloc_vaddr = base_reloc_dir->VirtualAddress;
    size = base_reloc_dir->Size;

    /* /FIXED dlls have the vaddr as 0, but size may be garbage */
    if (base_reloc_vaddr == 0) {
        /* $ dumpbin /headers xpsp2res.dll
         *             210E characteristics
         *      0 [       0] RVA [size] of Base Relocation Directory
         * has only one section .rsrc
         */
        return NULL; /* base_reloc_size set to 0 */
    }

    if (base_reloc_vaddr != 0 && size == 0) {
        ASSERT_CURIOSITY(false && "expect non-zero base_reloc");
        return NULL; /* base_reloc_size set to 0 */
    }

    LOG(GLOBAL, LOG_RCT, 2,
        "reloc: get_module_base_reloc: module_base=" PFX ", "
        "base_reloc_dir=" PFX ", base_reloc_vaddr=" PFX ", size=" PFX ")\n",
        module_base, base_reloc_dir, base_reloc_vaddr, size);

    base_reloc = (IMAGE_BASE_RELOCATION *)RVA_TO_VA(module_base, base_reloc_vaddr);

    if (is_readable_without_exception((app_pc)base_reloc, size)) {
        if (base_reloc_size != NULL)
            *base_reloc_size = size;
        return base_reloc;
    } else {
        ASSERT_CURIOSITY(false && "bad base relocation" ||
                         /* expected for partial map */
                         EXEMPT_TEST("win32.partial_map.exe"));
    }

    return NULL;
}

/* returns FileHeader.Characteristics */
/* should be used only under after is_readable_pe_base */
uint
get_module_characteristics(app_pc module_base)
{
    IMAGE_NT_HEADERS *nt = NULL;
    IMAGE_DATA_DIRECTORY *com_desc_dir = NULL;

    VERIFY_NT_HEADER(module_base);
    /* callers should have done this in release builds */
    ASSERT(is_readable_pe_base(module_base));

    nt = NT_HEADER(module_base);
    /* note this is not the same as OptionalHeader.DllCharacteristics */
    return nt->FileHeader.Characteristics;
}

/* Parse PE and return IMAGE_COR20_HEADER * if it has a valid COM header.
 * Optional OUT: cor20_header_size.
 * NOTE: returning a pointer into a dll that could be unloaded could be racy
 * (though we don't expect a race, see case 1272 for _safe/_unsafe routines)
 */
IMAGE_COR20_HEADER *
get_module_cor20_header(app_pc module_base, size_t *cor20_header_size)
{
    IMAGE_NT_HEADERS *nt = NULL;
    IMAGE_DATA_DIRECTORY *com_desc_dir = NULL;

    VERIFY_NT_HEADER(module_base);
    /* callers should have done this in release builds */
    ASSERT(is_readable_pe_base(module_base));

    nt = NT_HEADER(module_base);

    /* IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR < IMAGE_NUMBEROF_DIRECTORY_ENTRIES */
    com_desc_dir = OPT_HDR(nt, DataDirectory) + IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR;
    /* sanity check */
    ASSERT(is_readable_without_exception((app_pc)com_desc_dir, 8));

    LOG(GLOBAL, LOG_RCT, 3,
        "get_module_cor20_header: module_base=" PFX ", com_desc_dir=" PFX ")\n",
        module_base, com_desc_dir);

    if (cor20_header_size != NULL)
        *cor20_header_size = 0;

    if (com_desc_dir != NULL) {
        app_rva_t com_desc_vaddr = com_desc_dir->VirtualAddress;
        size_t size = com_desc_dir->Size; /* only a dword but we plan for future */

        LOG(GLOBAL, LOG_RCT, 3,
            "get_module_cor20_header: module_base=" PFX ", "
            "com_desc_dir=" PFX ", com_desc_vaddr=" PFX ", size=" PFX ")\n",
            module_base, com_desc_dir, com_desc_vaddr, size);

        if (com_desc_vaddr != 0 && size == 0 || com_desc_vaddr == 0 && size > 0) {
            ASSERT_CURIOSITY(false && "bad cor20 header");
            /* cor20_header_size set to 0 */
            return NULL;
        }

        if (size > 0) {
            IMAGE_COR20_HEADER *cor20_header = (IMAGE_COR20_HEADER *)RVA_TO_VA(
                module_base, com_desc_dir->VirtualAddress);

            if (is_readable_without_exception((app_pc)cor20_header,
                                              sizeof(IMAGE_COR20_HEADER))) {
                if (cor20_header_size != NULL) {
                    *cor20_header_size = size;
                }
                return cor20_header;
            } else
                ASSERT_CURIOSITY(false && "bad cor20 header");
        }
    } else
        ASSERT_CURIOSITY(false && "no cor20_header directory entry");

    return NULL;
}

/* PE files, for exes and dlls, with managed code have IMAGE_COR20_HEADER
 * defined in their PE.  Return if PE has cor20 header or not.
 */
bool
module_has_cor20_header(app_pc module_base)
{
    size_t cor20_header_size = 0;
    IMAGE_COR20_HEADER *cor20_header =
        get_module_cor20_header(module_base, &cor20_header_size);

    return (cor20_header != NULL && cor20_header_size > 0);
}

static WORD
get_module_magic(app_pc module_base)
{
    IMAGE_NT_HEADERS *nt = NULL;
    if (!is_readable_pe_base(module_base))
        return false;
    VERIFY_NT_HEADER(module_base);
    nt = NT_HEADER(module_base);
    return nt->OptionalHeader.Magic;
}

bool
module_is_32bit(app_pc module_base)
{
    return (get_module_magic(module_base) == IMAGE_NT_OPTIONAL_HDR32_MAGIC);
}

bool
module_is_64bit(app_pc module_base)
{
    return (get_module_magic(module_base) == IMAGE_NT_OPTIONAL_HDR64_MAGIC);
}

/* WARNING: this routine relies on observed behavior and data structures that
 * may change in future versions of Windows
 *
 * Returns true if start:end matches a code/IAT section of a module that the loader
 * would legitimately update, AND the module is currently being initialized
 * by this thread (or a guess as to that effect for 2003).
 * If conservative is true, makes fewer guesses and uses stricter guidelines,
 * so may have false negatives but should have no false positives after
 * the image entry point.
 * Caller must distinguish IAT in .rdata from IAT in .text.
 */
bool
is_module_patch_region(dcontext_t *dcontext, app_pc start, app_pc end, bool conservative)
{
    PEB *peb = get_own_peb();
    LDR_MODULE *mod;
    RTL_CRITICAL_SECTION *lock = (RTL_CRITICAL_SECTION *)peb->LoaderLock;
    app_pc IAT_start, IAT_end;
    bool match_IAT = false;
    app_pc base = get_module_base(start);
    LOG(THREAD, LOG_VMAREAS, 2, "is_module_patch_region: start " PFX " -> base " PFX "\n",
        start, base);
    if (base == NULL) {
        LOG(THREAD, LOG_VMAREAS, 2,
            "is_module_patch_region: not readable or not PE => NO\n");
        return false;
    }
    /* The only module changes we recognize are rebasing, where the entire
     * code section should be written to, and rebinding, where only
     * the IAT should be written to.  We ignore relocation of other data.
     * We allow for page rounding at end.
     */
    if (is_IAT(start, (app_pc)ALIGN_FORWARD(end, PAGE_SIZE), true /*page align*/,
               &IAT_start, &IAT_end)) {
        LOG(THREAD, LOG_VMAREAS, 2,
            "is_module_patch_region: matches IAT " PFX "-" PFX "\n", IAT_start, IAT_end);
        match_IAT = true;
    } else {
        /* ASSUMPTION: if multiple code sections, they are always protected separately.
         * We walk the code sections and see if our region is inside one of them.
         */
        app_pc sec_start = NULL, sec_end = NULL;
        if (!is_range_in_code_section(base, start, end, &sec_start, &sec_end)) {
            LOG(THREAD, LOG_VMAREAS, 2,
                "is_module_patch_region: not IAT or inside code section => NO\n");
            return false;
        }
        LOG(THREAD, LOG_VMAREAS, 2,
            "is_module_patch_region: target " PFX "-" PFX " => section " PFX "-" PFX "\n",
            start, end, sec_start, sec_end);
        /* FIXME - check what alignment the loader uses when section alignment is
         * < than page size (check on all platforms) to tighten this up. According to
         * Derek the IAT requests are very percise, but the loader may do exact
         * (rebase restore) or page-aligned (rebind restore) on xpsp2 at least. */
        if (ALIGN_BACKWARD(start, PAGE_SIZE) != ALIGN_BACKWARD(sec_start, PAGE_SIZE) ||
            ALIGN_FORWARD(end, PAGE_SIZE) != ALIGN_FORWARD(sec_end, PAGE_SIZE)) {
            LOG(THREAD, LOG_VMAREAS, 2,
                "is_module_patch_region: not targeting whole code or IAT section => "
                "NO\n");
            return false;
        }
    }

    /* On 2K and XP, the LoaderLock is always held when loading a module,
     * but on 2003 it is not held for loads prior to the image entry point!
     * Even worse, we've seen apps that create a 2nd thread prior to the entry
     * point, meaning we cannot safely walk the list.
     */
    if ((thread_id_t)lock->OwningThread == d_r_get_thread_id()) {
        /* Walk the list
         * FIXME: just look at the last entry, since it's appended to the memory-order
         * list?
         */
        mod = get_ldr_module_by_pc(start);
        if (mod != NULL) {
            /* How do we know if module is initialized?  LoadCount is 0 for a while, but
             * on win2003 it becomes 1 and the loader is still mucking around.  But when
             * it does become 1, the flags have 0x1000 set.  So we have this hack.
             * ASSUMPTION: module is uninitialized if either LoadCount is 0 or
             * flags have 0x1000 set.
             * Note that LoadCount is -1 for statically linked dlls and the exe itself.
             * We also see cases where a module's IAT is patched, and later is re-patched
             * once the module's count and flags indicate it's initialized.
             * We go ahead and allow that, since it's only data and not much of a security
             * risk.
             * FIXME: figure out what the loader is doing there -- I saw it on sqlservr
             * on 2003 loading msdtcprx.dll and then a series of dependent dlls was loaded
             * and patched and re-patched, perhaps due to forwarding?
             */
            LOG(THREAD, LOG_VMAREAS, 2,
                "is_module_patch_region: count=%d, flags=0x%x, %s\n", mod->LoadCount,
                mod->Flags, match_IAT ? "IAT" : "not IAT");
            if (mod->LoadCount == 0 || TEST(LDR_LOAD_IN_PROGRESS, mod->Flags) ||
                /* case 10180: executable itself has unknown flag 0x00004000 set; we
                 * relax to consider it the loader if the lock is held and we are
                 * before the image entry, but only when we track the image entry */
                (!reached_image_entry_yet() && !RUNNING_WITHOUT_CODE_CACHE()) ||
                (!conservative && match_IAT))
                return true;
            else
                return false;
        }
    } else {
        if (get_os_version() >= WINDOWS_VERSION_2003 && !reached_image_entry_yet()) {
#ifdef HOT_PATCHING_INTERFACE
            /* This one of the uses of reached_image_entry that may conflict
             * with -hotp_only not setting it because interp is not done. Others
             * are safe.  If this is hit, then the image_entry hook will have
             * to placed in callback_interception_init for -hotp_only.  A TODO.
             */
            if (DYNAMO_OPTION(hotp_only)) {
                LOG(GLOBAL, LOG_HOT_PATCHING, 1,
                    "Warning: On w2k3, for "
                    "hotp_only, image entry won't be detected because no "
                    "interp is done and hook is placed late");
            }
#endif
            /* On 2003, we cannot safely walk the module list (grabbing
             * the LoaderLock is fraught with deadlock problems...)
             * We haven't put in the effort in analyzing the
             * LdrLockLoaderLock routine that decides not to grab it
             * to find the flag the loader is using to decide when to
             * start using the lock, but it coincides with the image
             * entry point (or loader finishing initialization), so we
             * use that.
             * FIXME: this isn't as narrow as we'd like --
             * we're letting anyone modify a .text section prior to
             * image entry on 2003!
             */
            return true;
        }
    }
    return false;
}

#define IMAGE_REL_BASED_TYPE(x) ((x) >> 12)
#define IMAGE_REL_BASED_OFFSET_MASK 0x0FFF
#define IMAGE_REL_BASED_OFFSET(x) ((ushort)((x)&IMAGE_REL_BASED_OFFSET_MASK))

/* Processes a single relocation and returns the relocated address.  If
 * apply_reloc is false, the actual relocation isn't performed on the
 * image only the relocated address is returned.
 *
 * Note: this routine handles 32-bit dlls for both 32-bit dr and for 64-bit dr
 * (wow64 process), and 64-bit dlls for 64-bit dr.
 *
 * X86 relocation types can be: IMAGE_REL_BASED_HIGHLOW |
 * IMAGE_REL_BASED_ABSOLUTE --  offsets pointing to 32-bit
 * immediate (see case 6424).  For X64 it is IMAGE_REL_BASED_DIR64
 * or IMAGE_REL_BASED_ABSOLUTE.
 *
 * Returns relocated_addr for HIGHLOW & DIR64.
 */
static app_pc
process_one_relocation(const app_pc module_base, app_pc reloc_entry_p,
                       uint reloc_array_rva, ssize_t relocation_delta, bool apply_reloc,
                       bool *null_ref /* OUT */, bool *unsup_reloc /* OUT */,
                       app_pc *relocatee_addr /* OUT */,
                       bool is_module_32bit _IF_DEBUG(size_t module_size))
{
    ushort reloc_entry = *(ushort *)reloc_entry_p;
    int reloc_type = IMAGE_REL_BASED_TYPE(reloc_entry);
    ushort offset = IMAGE_REL_BASED_OFFSET(reloc_entry);
    app_pc cur_addr, addr_to_reloc, relocated_addr = NULL;
    DEBUG_DECLARE(char *rel_name = "unsupported";)

    cur_addr = (app_pc)RVA_TO_VA(module_base, (reloc_array_rva + offset));
    /* relocatee_addr is used to return the address of the value that is to be
     * relocated. */
    if (relocatee_addr != NULL)
        *relocatee_addr = cur_addr;
    /* curiosity: sometimes cur_addr is not within module */
    ASSERT_CURIOSITY(module_base <= cur_addr && cur_addr < (module_base + module_size));

#ifdef X64
    if (reloc_type == IMAGE_REL_BASED_DIR64) {
        /* this relocation is only on 64-bits */
        addr_to_reloc = (app_pc)(*(uint64 *)cur_addr);
        if (addr_to_reloc == NULL) {
            if (null_ref != NULL)
                *null_ref = true;
            ASSERT_CURIOSITY(false && "relocation entry for a null ref?");
        }
        relocated_addr = relocation_delta + addr_to_reloc;
        if (apply_reloc) {
            *(uint64 *)cur_addr = (uint64)relocated_addr;
        }
        DEBUG_DECLARE(rel_name = "DIR64");
    } else
#endif
        if (reloc_type == IMAGE_REL_BASED_HIGHLOW) {
        /* This is a 32-bit relocation type and can found only in a 32-bit dll.
         * If it is found in a 64-bit process then the process must be wow64.
         */
        IF_X64(ASSERT(is_wow64_process(NT_CURRENT_PROCESS));)
        IF_X64(ASSERT(is_module_32bit);)
        if (!is_module_32bit) {
            if (unsup_reloc != NULL)
                *unsup_reloc = true;
            return NULL;
        }

        /* Relocation delta can't be greater than 32-bits!  It is a bug if so
         * because 32-bit dlls only have 32-bit quantities to be relocated,
         * i.e., only within a 2 GB space - so can't add a relocation delta
         * that is greater than 31 bits (which is possible to generate while
         * rebasing in a 64-bit address space).
         */
        ASSERT(CHECK_TRUNCATE_TYPE_int(relocation_delta));

        /* IMAGE_REL_BASED_HIGHLOW relocations are defined to operate
         * on 32-bits, irrespective of 32-bit or 64-bit DR, so read
         * exactly 32-bits.
         */
        ASSERT(sizeof(uint) == 4); /* this relocation is only on 32-bits */
        addr_to_reloc = (app_pc)(ptr_uint_t)(*(uint *)cur_addr);
        if (addr_to_reloc == NULL) {
            if (null_ref != NULL)
                *null_ref = true;
            ASSERT_CURIOSITY(false && "relocation entry for a null ref?");
        }
        relocated_addr = relocation_delta + addr_to_reloc;

        /* Just like relocation delta, relocated addr can't be greater than
         * 32-bits too.
         */
        IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint((ptr_uint_t)relocated_addr));)
        if (apply_reloc) {
            *(uint *)cur_addr = (uint)(ptr_uint_t)relocated_addr;
        }
        DEBUG_DECLARE(rel_name = "HIGHLOW");
    } else if (reloc_type == IMAGE_REL_BASED_ABSOLUTE) {
        /* This is just a padding type so ignore. */
        DEBUG_DECLARE(rel_name = "ABS");
    } else {
        /* unsupported types */
        /* LOW:  *(ushort*) cur_addr += LOWORD(relocation_delta)
         * HIGH: *(ushort*) cur_addr += HIWORD(relocation_delta)
         */
        /* FIXME: case 8515: it is better to implement these
         * in case some stupid compiler is generating them for random reasons
         */
        ASSERT_CURIOSITY("Unsupported relocation encountered");
        if (unsup_reloc != NULL)
            *unsup_reloc = true;
    }
    LOG(GLOBAL, LOG_RCT, 6, "\t%8x %s\n", offset, rel_name);
    return relocated_addr;
}

/****************************************************************************/
#ifdef RCT_IND_BRANCH

#    ifdef X64
static void
add_SEH_address(dcontext_t *dcontext, app_pc addr, app_pc modbase, size_t modsize)
{
    /* If except_handler isn't within the image, don't add to RCT table */
    if (addr > modbase && addr < modbase + modsize) {
        if (rct_add_valid_ind_branch_target(dcontext, addr)) {
            STATS_INC(rct_ind_branch_valid_targets);
            STATS_INC(rct_ind_seh64_new);
        } else
            STATS_INC(rct_ind_seh64_old);
    } else {
        ASSERT_CURIOSITY(false && "SEH address out of module");
    }
}

/* This routine analyzes the .pdata section in a PE32+ module and adds
 * exception handler addresses to rct table.  Note, this isn't applicable for
 * 32-bit dlls as SEH aren't specified like this in those dlls.  PR 250395.
 */
static void
add_SEH_to_rct_table(dcontext_t *dcontext, app_pc module_base)
{
    IMAGE_NT_HEADERS *nt = NT_HEADER(module_base);
    IMAGE_DATA_DIRECTORY *except_dir;
    PIMAGE_RUNTIME_FUNCTION_ENTRY func_entry, func_entry_end;
    byte *pdata_start;
    uint image_size;
    ASSERT_OWN_MUTEX(true, &rct_module_lock);

    ASSERT(module_base != NULL);
    if (module_base == NULL)
        return;

    /* Ignore 32-bit dlls in a wow64 process. */
    if (!module_is_64bit(module_base))
        return;

    nt = NT_HEADER(module_base);

    /* Exception directories can be NULL if the compiler didn't put it in, but
     * unusual for PE32+ dlls to not have an exception directory.
     */
    except_dir = OPT_HDR(nt, DataDirectory) + IMAGE_DIRECTORY_ENTRY_EXCEPTION;
    if (except_dir == NULL) {
        ASSERT_CURIOSITY(false && "no exception data directory (no .pdata?)");
        return;
    }
    image_size = nt->OptionalHeader.SizeOfImage;
    /* Exception directory entry must lie within the image */
    ASSERT((app_pc)except_dir > module_base &&
           (app_pc)except_dir < module_base + image_size);

    /* Exception directory, i.e., function table entries, must lie within the
     * image.
     */
    func_entry =
        (PIMAGE_RUNTIME_FUNCTION_ENTRY)(module_base + except_dir->VirtualAddress);
    pdata_start = (byte *)func_entry;
    func_entry_end = (PIMAGE_RUNTIME_FUNCTION_ENTRY)(
        module_base + except_dir->VirtualAddress + except_dir->Size);
    /* spec says func_entry must be dword-aligned */
    ASSERT_CURIOSITY(ALIGNED(func_entry, sizeof(DWORD)));
    ASSERT((app_pc)func_entry_end >= module_base &&
           (app_pc)func_entry < module_base + image_size);

    LOG(GLOBAL, LOG_RCT, 2, "parsing .pdata of pe32+ module " PFX "\n", module_base);
    for (; func_entry < func_entry_end; func_entry++) {
        unwind_info_t *info =
            (unwind_info_t *)(module_base + func_entry->UnwindInfoAddress);
        /* Spec says unwind info must be dword-aligned, but we also have
         * special entries that point at other RUNTIME_FUNCTION slots in
         * the .pdata array, but with a 1-byte offset.  It seems to be a
         * way to share unwind info for non-contiguous pieces of the same
         * function without wasting space on a chained info structure
         * (see PR 250395).
         */
        if (info > (unwind_info_t *)pdata_start &&
            info < (unwind_info_t *)func_entry_end) {
            /* All the ones I've seen have been 1 byte in */
            ASSERT_CURIOSITY(ALIGNED(((byte *)info) - 1, sizeof(DWORD)));
            /* We just skip this entry, as it's subsumed by the one it points at */
            STATS_INC(rct_ind_seh64_plus1);
            continue;
        }
        ASSERT_CURIOSITY(ALIGNED(info, sizeof(DWORD)));

        /* If it is a chain entry, then walk the chain to get the exception
         * handler.
         */
        while (TEST(UNW_FLAG_CHAININFO, info->Flags)) {
            /* Note that while one page of the specs, and the
             * GetChainedFunctionEntry() macro, say that there is a pointer to a
             * RUNTIME_FUNCTION, another page, and all the instances I've seen, have
             * the RUNTIME_FUNCTION inlined.  We handle both.
             */
            byte *ptr;
            uint rva;
            PIMAGE_RUNTIME_FUNCTION_ENTRY chain_func;
            /* if chained, can't have handler flags set */
            ASSERT_CURIOSITY(
                !TESTANY(UNW_FLAG_EHANDLER | UNW_FLAG_UHANDLER, info->Flags));
            ptr = module_base + UNWIND_INFO_PTR_RVA(info);
            if (ptr > pdata_start && ptr < (byte *)func_entry_end)
                chain_func = (PIMAGE_RUNTIME_FUNCTION_ENTRY)ptr;
            else /* inlined */
                chain_func = (PIMAGE_RUNTIME_FUNCTION_ENTRY)UNWIND_INFO_PTR_ADDR(info);
            if (!d_r_safe_read(&chain_func->UnwindInfoAddress, sizeof(rva), &rva)) {
                ASSERT_CURIOSITY(false && "unwind_info_t corrupted/misinterpreted");
                continue;
            }
            info = (unwind_info_t *)(module_base + rva /*chain_func->UnwindInfoAddress*/);
            /* spec says unwind info must be dword-aligned */
            ASSERT_CURIOSITY(ALIGNED(info, sizeof(DWORD)));
        }

        /* If unwind info is either UNW_FLAG_EHANDLER or UNW_FLAG_UHANDLER,
         * then it has an exception handler address.
         */
        if (TESTANY(UNW_FLAG_EHANDLER | UNW_FLAG_UHANDLER, info->Flags)) {
            app_pc handler = module_base + UNWIND_INFO_PTR_RVA(info);
            /* PR 276527: also process the scope table addresses */
            scope_table_t *scope;
            uint i;
            bool is_scope = true;
            add_SEH_address(dcontext, handler, module_base, image_size);
            LOG(GLOBAL, LOG_RCT, 4, "added RCT SEH64 handler " PFX " (from " PFX ")\n",
                handler, info);
            /* Note that I'm seeing user32 and kernel32 each having a single
             * main handler, which calls the ntdll main handler, which uses
             * the scope table to dive down into details.  Given the single
             * main, why not provide an efficient way to have a single main
             * handler, which would allow handling misaligned stack pointers?
             */
            /* Like the chained info, the scope table is described as being
             * out-of-line, but I'm seeing it inlined.
             */
            scope = (scope_table_t *)UNWIND_INFO_DATA_ADDR(info);
            /* Not all entries have this: e.g., calc.exe's _CxxFrameHandler doesn't
             * use this setup; only the _C_specific_handler routines do.
             * We use a heuristic where we assume there won't be over 4K entries;
             * most other fields in these tables are > 0x1000.  That breaks down
             * when we hit other lang-specific structs, though, so we have
             * further checks below.
             */
            if (scope->Count == 0 || scope->Count >= 0x1000)
                is_scope = false;
            else {
                /* Do one pass through to make sure it all looks right.  FIXME: we
                 * need a stronger way to tell when there's a scope table and when
                 * not.  It would be nice to check the scope entry's range, but it
                 * seems to not need to be a subset of func_entry's range: many
                 * func_entries point at the same unwind_info, which then re-expands
                 * via the scope table.  We could check that [Begin, End) is in a
                 * code section.
                 */
                for (i = 0; i < scope->Count; i++) {
                    if (scope->ScopeRecord[i].EndAddress <=
                            scope->ScopeRecord[i].BeginAddress ||
                        /* Yes, you can have tiny dlls, but we'll adjust when we hit
                         * that: I'm seeing other lang-specific structs and I need
                         * heuristics to distinguish from the scope table we know */
                        scope->ScopeRecord[i].BeginAddress < PAGE_SIZE ||
                        scope->ScopeRecord[i].EndAddress > image_size ||
                        (scope->ScopeRecord[i].HandlerAddress >
                             EXCEPTION_EXECUTE_HANDLER &&
                         scope->ScopeRecord[i].HandlerAddress < PAGE_SIZE) ||
                        scope->ScopeRecord[i].HandlerAddress > image_size ||
                        scope->ScopeRecord[i].JumpTarget > image_size) {
                        LOG(GLOBAL, LOG_RCT, 4,
                            "NOT a scope table entry %d info " PFX "\n", i, info);
                        is_scope = false;
                        break;
                    }
                }
            }
            if (is_scope) {
                for (i = 0; i < scope->Count; i++) {
                    /* Add the filter address */
                    if (scope->ScopeRecord[i].HandlerAddress !=
                            EXCEPTION_EXECUTE_HANDLER &&
                        /* Often they're all the same */
                        (i == 0 ||
                         scope->ScopeRecord[i].HandlerAddress !=
                             scope->ScopeRecord[i - 1].HandlerAddress)) {
                        add_SEH_address(
                            dcontext, module_base + scope->ScopeRecord[i].HandlerAddress,
                            module_base, image_size);
                        LOG(GLOBAL, LOG_RCT, 4, "added RCT SEH64 filter %d " PFX "\n", i,
                            module_base + scope->ScopeRecord[i].HandlerAddress);
                    }
                    if (scope->ScopeRecord[i].JumpTarget != 0 &&
                        /* Often they're all the same */
                        (i == 0 ||
                         scope->ScopeRecord[i].JumpTarget !=
                             scope->ScopeRecord[i - 1].JumpTarget)) {
                        /* Add the catch block entry address */
                        add_SEH_address(dcontext,
                                        module_base + scope->ScopeRecord[i].JumpTarget,
                                        module_base, image_size);
                        LOG(GLOBAL, LOG_RCT, 4, "added RCT SEH64 catch %d " PFX "\n", i,
                            module_base + scope->ScopeRecord[i].JumpTarget);
                    }
                }
            } else {
                LOG(GLOBAL, LOG_RCT, 4,
                    "assuming scope " PFX " w/ count %d is not a scope table\n", scope,
                    scope->Count);
            }
        }
    }
}

bool
rct_add_rip_rel_addr(dcontext_t *dcontext, app_pc tgt _IF_DEBUG(app_pc src))
{
    /* PR 215408: Whether we scan or use relocations, for x64 we also need to add
     * rip-rel references (unless we update the scan to consider those as well).  We
     * need to look up the section and if it matches the characteristics checked by
     * add_rct_module() add it like rct_check_ref_and_add() does.  Faster to do a
     * scan than all these section lookups?  Keep a vmvector of appropriate sections?
     * It should be faster to check the rct table before doing the section walk,
     * so we do that.
     */
    app_pc modbase = get_module_base(tgt);
    uint secchar;
    bool res = false;
    if (modbase != NULL && rct_ind_branch_target_lookup(dcontext, tgt) == NULL &&
        is_in_executable_file_section(modbase, tgt, tgt + 1, NULL, NULL, NULL, &secchar,
                                      NULL, 0, NULL, false, -1, false)) {
        ASSERT(DYNAMO_OPTION(rct_section_type) != 0 &&
               !TESTANY(~(IMAGE_SCN_CNT_CODE | IMAGE_SCN_CNT_INITIALIZED_DATA |
                          IMAGE_SCN_CNT_UNINITIALIZED_DATA),
                        DYNAMO_OPTION(rct_section_type)));
        if (TESTANY(DYNAMO_OPTION(rct_section_type), secchar) &&
            (DYNAMO_OPTION(rct_section_type_exclude) == 0 ||
             !TESTALL(DYNAMO_OPTION(rct_section_type_exclude), secchar))) {
            DOLOG(3, LOG_RCT, {
                char symbuf[MAXIMUM_SYMBOL_LENGTH];
                LOG(GLOBAL, LOG_RCT, 3,
                    "rct_add_rip_rel_addr: " PFX " rip-rel addr referenced at " PFX "\n",
                    tgt, src);
                print_symbolic_address(tgt, symbuf, sizeof(symbuf), true);
                LOG(GLOBAL, LOG_SYMBOLS, 3, "\t%s\n", symbuf);
            });
            d_r_mutex_lock(&rct_module_lock);
            if (rct_add_valid_ind_branch_target(dcontext, tgt)) {
                STATS_INC(rct_ind_branch_valid_targets);
                STATS_INC(rct_ind_rip_rel_new);
                res = true;
            } else {
                STATS_INC(rct_ind_rip_rel_old);
                ASSERT_CURIOSITY(false && "TOCTOU race");
            }
            d_r_mutex_unlock(&rct_module_lock);
        }
    } else {
        DOSTATS({
            if (rct_ind_branch_target_lookup(dcontext, tgt) != NULL)
                STATS_INC(rct_ind_rip_rel_old);
        });
    }
    return res;
}
#    endif /* X64 */

/* The exported functions of a particular module are in fact not
 * absolute references - the linker keeps them in RVA format therefore
 * we do need to walk the exports table for them.  For the same
 * reason, we can do this walk whether the actual module is relocated or
 * as soon as the module is mapped in the address space
 */
static void
rct_add_exports(dcontext_t *dcontext, app_pc module_base, size_t module_size)
{
    /* a more complicated version that also walks the parallel function
     * names is done in add_module_info().
     */
    size_t size;
    IMAGE_EXPORT_DIRECTORY *exports = get_module_exports_directory_check(
        module_base, &size, false /* only check functions array */);

    if (exports != NULL) {
        /* RVA array of all exported addresses */
        PULONG functions = (PULONG)(module_base + exports->AddressOfFunctions);
        uint i;

        LOG(GLOBAL, LOG_SYMBOLS, 3, "\tnumnames=%d numfunc=%d", exports->NumberOfNames,
            exports->NumberOfFunctions);

        if (exports->NumberOfFunctions == 0) {
            /* no functions to add */
            /* riched32.dll from mmc.exe actually hits this */
            return;
        }

        LOG(GLOBAL, LOG_RCT, 3,
            "rct_add_exports: dll_name=%s exports=" PFX " numnames=%d numfunc=%d %s",
            (char *)(module_base + exports->Name), exports, exports->NumberOfNames,
            exports->NumberOfFunctions,
            (exports->NumberOfFunctions == exports->NumberOfNames) ? "" : "NONAMES ");

        /* for our security policy to restrict transfers to exports
         * only, we need all functions (no matter whether named or not)
         * just watch out for forwarders
         */
        for (i = 0; i < exports->NumberOfFunctions; i++) {
            /* plain walk through AddressOfFunctions array */
            app_pc func = module_base + functions[i];

            /* check if it points within the exports section in real address space */
            if ((func < (app_pc)exports || func >= (app_pc)exports + size) &&
                /* ensure points within this module: resolved forwarder might point
                 * at another module
                 */
                (func >= module_base && func < (module_base + module_size))) {
                /* FIXME: use print_symbolic_address() */
                LOG(GLOBAL, LOG_RCT, 3, "\tadding i=%d " PFX "\n", i, func);
                /* interestingly there are ordinals in shell32.dll
                 * that are at module_base, so can't make this point
                 * to code sections only
                 */
                /* FIXME: note that we may add not only functions but
                 * export data as well!
                 * FIXME: currently we leave on code origins to cover this up,
                 * FIXME: while instead we should check for code sections here
                 * just like we need to do in rct_analyze_module() anyways
                 */
                if (rct_add_valid_ind_branch_target(dcontext, func)) {
                    STATS_INC(rct_ind_branch_valid_targets);
                    STATS_INC(rct_ind_added_exports);
                } else {
                    LOG(GLOBAL, LOG_RCT, 3,
                        "\t already added export entry i=%d " PFX "\n", i, func);
                    /* most likely address taken -
                     * FIXME: verify that they are all really address taken,
                     * and not say forwards, although those can't possibly be added
                     */
                    STATS_INC(rct_ind_already_added_exports);
                }
            } else {
                /* skip forwarded function - it forwards to a named
                 * import e.g. NTDLL.RtlAllocateHeap which will be
                 * added in its own module's exports */
                if (func >= (app_pc)exports && func < (app_pc)exports + size) {
                    LOG(GLOBAL, LOG_RCT, 3, "Forward to " PFX " %s.  Skipping...\n", func,
                        (char *)func);
                } else {
                    LOG(GLOBAL, LOG_RCT, 3,
                        "Forward to outside module " PFX ": already resolved?\n", func);
                }
            }
        }

        DOLOG(2, LOG_RCT, {
            char short_name[MAX_MODNAME_INTERNAL];
            os_get_module_name_buf(module_base, short_name,
                                   BUFFER_SIZE_ELEMENTS(short_name));
            LOG(GLOBAL, LOG_RCT, 2, "rct_add_exports: %s : %d exports added\n",
                short_name, exports->NumberOfFunctions);
        });
    } else {
        DOLOG(SYMBOLS_LOGLEVEL, LOG_SYMBOLS, {
            char short_name[MAX_MODNAME_INTERNAL];
            os_get_module_name_buf(module_base, short_name,
                                   BUFFER_SIZE_ELEMENTS(short_name));

            /* the executable itself is OK */
            if (module_base != get_own_peb()->ImageBaseAddress) {
                if (short_name)
                    LOG(GLOBAL, LOG_SYMBOLS, 2, "No exports %s\n", short_name);
                else
                    LOG(GLOBAL, LOG_SYMBOLS, 2, "Not a PE at " PFX "\n", module_base);
            }
        });
    }
}

/* For each relocation entry, check if the address refers to code section
 * [referto_start, referto_end).  Add all such valid references to the indirect
 * branch hashtable.
 * returns: -1 if there were no relocation entries
 *           0 if there was a valid entry referring to some section
 *          references_found, otherwise
 */
/* FIXME: should switch to using module_reloc_iterator_start() */
static int
find_relocation_references(dcontext_t *dcontext, app_pc module_base, size_t module_size,
                           IMAGE_BASE_RELOCATION *base_reloc, size_t base_reloc_size,
                           ssize_t relocation_delta, app_pc referto_start,
                           app_pc referto_end)
{
    app_pc relocs = NULL;        /* reloc entries */
    app_pc relocs_end = 0;       /* to iterate through reloc entries */
    app_pc relocs_block_end = 0; /* to iterate through reloc blocks */
    bool is_module_32bit;

    int references_found = -1;
    DEBUG_DECLARE(uint references_already_known = 0; uint addresses_scanned = 0;
                  uint pages_touched = 0; char symbuf[MAXIMUM_SYMBOL_LENGTH] = "";);

    ASSERT(sizeof(uint) == 4);   /* size of rva and block_size - 32 bits */
    ASSERT(sizeof(ushort) == 2); /* size of reloc entry - 16 bits */

    ASSERT(module_base != NULL); /* caller should have verified base */

    /* callers set up base_reloc and base_reloc_size by calling
     * get_module_base_reloc */
    if (base_reloc == NULL || base_reloc_size == 0) {
        ASSERT(false && "expect relocations");
        return 0;
    }
    is_module_32bit = module_is_32bit(module_base);

    ASSERT(is_readable_without_exception((app_pc)base_reloc, base_reloc_size));
    ASSERT(referto_start <= referto_end); /* empty ok */

    DOLOG(2, LOG_RCT, {
        print_symbolic_address(module_base, symbuf, sizeof(symbuf), true);
        NULL_TERMINATE_BUFFER(symbuf);
        LOG(GLOBAL, LOG_RCT, 2,
            "reloc: find_relocation_references: "
            "module=%s, module_base=" PFX ", base_reloc=" PFX ", "
            "base_reloc_size=" PFX ", referto[" PFX ", " PFX ")\n",
            symbuf, module_base, base_reloc, base_reloc_size, referto_start, referto_end);
    });

    /* Iterate through relocation entries and check if they contain
     * references to code section
     */
    relocs = (app_pc)base_reloc;
    relocs_end = relocs + base_reloc_size;

    KSTART(rct_reloc);
    while (relocs < relocs_end) {
        /* Image based relocation is stored as:
         * DWORD RVA,
         * DWORD SizeOfBlock
         * followed by a WORD array relocation entries
         *   number of relocation entries =
         *       (SizeOfBlock - IMAGE_SIZEOF_BASE_RELOCATION) / sizeof(ushort)
         */
        uint rva = (uint)((IMAGE_BASE_RELOCATION *)relocs)->VirtualAddress;
        uint block_size = (uint)((IMAGE_BASE_RELOCATION *)relocs)->SizeOfBlock;

        LOG(GLOBAL, LOG_RCT, 6, "\t%s %8x RVA, %8x SizeofBlock\n", symbuf, rva,
            block_size);
        /* WORD array of relocation entries
         *   top 4-bits are type
         *       [IMAGE_REL_BASED_ABSOLUTE | IMAGE_REL_BASED_HIGHLOW] - PE32/x86
         *       [IMAGE_REL_BASED_ABSOLUTE | IMAGE_REL_BASED_DIR64] - PE32+/x64
         *   remaining 12-bits are offset relative to RVA
         *
         * 0:000> dw 77c10000 + 55000
         * 77c65000  1000 0000 0158 0000 327c 3288 328c 3290
         * 77c65010  3294 3298 32a4 32a8 32ac 32b8 32ec 32f0
         * ...
         */
        relocs_block_end = relocs + block_size;
        relocs = relocs + IMAGE_SIZEOF_BASE_RELOCATION;

        KSTART(rct_reloc_per_page);
        while (relocs < relocs_block_end) {
            DEBUG_DECLARE(bool known_ref = false;)
            bool null_ref = false;
            app_pc cur_addr = NULL;
            app_pc ref = process_one_relocation(
                module_base, relocs, rva, relocation_delta, false /* don't apply reloc */,
                &null_ref, NULL, &cur_addr, is_module_32bit _IF_DEBUG(module_size));
            if (!null_ref) {
                /* There is at least one valid entry, so no longer
                 * return -1 */
                if (references_found < 0)
                    references_found = 0;

                /* sanity check */
                DODEBUG({
                    app_pc module_end = module_base + module_size;
                    /* There are many cases where ref is outside
                     * module, but with early_injection and
                     * analzye_at_load it should be rare. Have seen
                     * this in USER32.dll, where 77d9d414 is outside
                     * module:
                     *   77D9D410: 82B60F09 77D3394F 836656EB C01BE0F9
                     USER32!UserIsFELineBreakEnd+0x9:
                     77d9d36b 0fb7d1           movzx   edx,cx
                     ...
                     77d9d40b 6683f99f         cmp     cx,0xff9f
                     77d9d40f 7709             ja      USER32!UserIsFELineBreakEnd+0xb9
                                                       (77d9d41a)
                     77d9d411 0fb6824f39d377   movzx   eax,byte ptr [edx+0x77d3394f]
                     */
                    if (ref < module_base || ref >= module_end) {
                        LOG(GLOBAL, LOG_RCT, 2,
                            "find_relocation_references: ref " PFX " taken at "
                            "addr " PFX " not in module [" PFX "," PFX ")\n",
                            ref, cur_addr, module_base, module_end);
                        STATS_INC(rct_ind_branch_ref_outside_module);
                    }
                });
            }

            if (rct_check_ref_and_add(dcontext, ref, referto_start,
                                      referto_end _IF_DEBUG(cur_addr)
                                          _IF_DEBUG(&known_ref)))
                references_found++;
            else {
                DODEBUG({
                    if (known_ref)
                        references_already_known++;
                });
            }

            DODEBUG(addresses_scanned++;);
            /* IMAGE_REL_BASED_ABSOLUTE is just used for padding, ignore */

            relocs = relocs + sizeof(ushort);
        } /* a block of relocation entries */
        KSTOP(rct_reloc_per_page);

        /* each block of relocation entries is for a 4k (PAGE_SIZE) page */
        DODEBUG(pages_touched++;);
    } /* all relocation entries */
    KSTOP(rct_reloc);

    LOG(GLOBAL, LOG_RCT, 1,
        "reloc: find_relocation_references:  "
        "scanned %u addresses, touched %u pages, "
        "added %d new, %u duplicate ind targets\n",
        addresses_scanned, pages_touched, references_found, references_already_known);

    return references_found;
}

/* Case 7275: an image with alignment > PAGE_SIZE can have reserved-but-not-committed
 * pages within its allocation region.  Thus we must scan each region individually.
 * This routine is normally called only on a single MEM_IMAGE allocation region,
 * but we don't restrict the caller and blindly process all committed pieces in
 * [text_start, text_end), calling find_address_references() on each.
 */
static uint
find_address_references_by_region(dcontext_t *dcontext, app_pc text_start,
                                  app_pc text_end, app_pc referto_start,
                                  app_pc referto_end)
{
    app_pc pc;
    MEMORY_BASIC_INFORMATION mbi;
    uint found = 0;
    LOG(GLOBAL, LOG_RCT, 2, "find_address_references_by_region [" PFX ", " PFX ")\n",
        text_start, text_end);
#    if defined(X64) && (defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH))
    os_module_set_flag(text_start, MODULE_RCT_SCANNED);
#    endif
    for (pc = text_start; pc < text_end; pc += mbi.RegionSize) {
        if (query_virtual_memory(pc, &mbi, sizeof(mbi)) != sizeof(mbi)) {
            ASSERT(false && "error querying memory for rct analysis");
            break;
        }
        if (POINTER_OVERFLOW_ON_ADD(pc, mbi.RegionSize))
            break;
        if (mbi.State == MEM_COMMIT) {
            /* safe to read */
            found +=
                find_address_references(dcontext, pc, MIN(pc + mbi.RegionSize, text_end),
                                        referto_start, referto_end);
        } else {
            LOG(GLOBAL, LOG_RCT, 2, "\t[" PFX ", " PFX ") not committed (state 0x%x)\n",
                pc, pc + mbi.RegionSize, mbi.State);
        }
    }
    return found;
}

/* Called while analyzing a module at load time or at the point of a
 * violation. For the latter, we defer until a failing indirect call/jmp
 * happens and then walk the corresponding code sections in the yet
 * undefined module.  Since the entry points of DLLs are targeted by
 * indirect calls we'll have all DLLs tested quickly anyways.
 *
 * As an optimization, we go through relocation table whenever present,
 * walk it and only compute addresses that are relevant.  Note that most
 * DLLs (other than /FIXED ones) will have a relocation table and even some
 * executables are relocatable.  Note that some modules may be quite large,
 * so going through the relocation list will be worthwhile.  If no
 * relocations are present we then scan code sections for valid ind branch
 * targets.
 */
static void
add_rct_module(dcontext_t *dcontext, app_pc module_base, size_t module_size,
               ssize_t relocation_delta, bool at_violation)
{
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)module_base;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(((ptr_uint_t)dos) + dos->e_lfanew);
    IMAGE_SECTION_HEADER *sec;
    uint i;

    /* FIXME: PRECISION/speed: limit searched range only to code and
     * initialized data section, although unclear whether resources
     * may or may not have functions or function pointers
     */
    app_pc text_start = module_base;
    app_pc text_end = module_base + module_size;
    uint found = 0;
    app_pc entry_point;
    DEBUG_DECLARE(uint code_sections = 0;)
    DEBUG_DECLARE(char modname[MAX_MODNAME_INTERNAL];)
    DODEBUG(
        { os_get_module_name_buf(module_base, modname, BUFFER_SIZE_ELEMENTS(modname)); });

    ASSERT(is_readable_pe_base(module_base));
    ASSERT_OWN_MUTEX(true, &rct_module_lock);

    STATS_INC(rct_ind_branch_modules_analyzed);

    ASSERT(DYNAMO_OPTION(rct_section_type) != 0 &&
           !TESTANY(~(IMAGE_SCN_CNT_CODE | IMAGE_SCN_CNT_INITIALIZED_DATA |
                      IMAGE_SCN_CNT_UNINITIALIZED_DATA),
                    DYNAMO_OPTION(rct_section_type)));

    /* IMAGE_SCN_CNT_CODE                   0x00000020 */
    /* IMAGE_SCN_CNT_INITIALIZED_DATA       0x00000040 */
    /* IMAGE_SCN_CNT_UNINITIALIZED_DATA     0x00000080 */

    /* For code, there can be multiple sections (.text, .orpc, perhaps others)
     * so we walk the section headers and check for the "code" flag.
     * iterator from is_in_executable_file_section()
     */
    LOG(GLOBAL, LOG_VMAREAS, 4, "module @ " PFX ":\n", module_base);
    /* PRECISION/memory: limit code only to a code section, and
     * process one section at a time.
     */
    sec = IMAGE_FIRST_SECTION(nt);
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        LOG(GLOBAL, LOG_VMAREAS, 4, "\tName = %.*s\n", IMAGE_SIZEOF_SHORT_NAME,
            sec->Name);
        LOG(GLOBAL, LOG_VMAREAS, 4, "\tVirtualSize    = " PFX "\n",
            sec->Misc.VirtualSize);
        LOG(GLOBAL, LOG_VMAREAS, 4, "\tVirtualAddress = " PFX "\n", sec->VirtualAddress);
        LOG(GLOBAL, LOG_VMAREAS, 4, "\tSizeOfRawData  = " PFX "\n", sec->SizeOfRawData);
        LOG(GLOBAL, LOG_VMAREAS, 4, "\tCharacteristics= " PFX "\n", sec->Characteristics);

        /* FIXME: case 5355, case 10526 - note we are not following the convoluted
         * section size matching from is_in_executable_file_section -
         * we don't have an OS region to be matching against. */

        if (TESTANY(DYNAMO_OPTION(rct_section_type), sec->Characteristics) &&
            (DYNAMO_OPTION(rct_section_type_exclude) == 0 ||
             !TESTALL(DYNAMO_OPTION(rct_section_type_exclude), sec->Characteristics))) {
            bool scan_all_addresses = true;
            app_pc code_start = module_base + sec->VirtualAddress;
            app_pc code_end =
                module_base + sec->VirtualAddress + get_image_section_size(sec, nt);
            LOG(GLOBAL, LOG_VMAREAS, 2,
                "add_rct_module (module " PFX "): %.*s == " PFX "-" PFX "\n", module_base,
                IMAGE_SIZEOF_SHORT_NAME, sec->Name, code_start, code_end);

            DODEBUG(code_sections++;);

            /* we don't expect too many code sections, otherwise we
             * can invert the loop and go through the whole file only
             * once.  Three sections in suite/tests/win32/multisec.
             */
            /* if we're enforcing on all section types we can get a few more sections */
            ASSERT_CURIOSITY((DYNAMO_OPTION(rct_section_type) == IMAGE_SCN_CNT_CODE &&
                              code_sections < 5) /* default */
                             || code_sections < 10);

            if (DYNAMO_OPTION(rct_reloc)) {
                IMAGE_BASE_RELOCATION *base_reloc = NULL;
                size_t base_reloc_size = 0;

                base_reloc = get_module_base_reloc(module_base, &base_reloc_size);
                LOG(GLOBAL, LOG_RCT, 2,
                    "reloc: add_rct_module: module_base=" PFX ", "
                    "base_reloc=" PFX ", base_reloc_size=" PFX ")\n",
                    module_base, base_reloc, base_reloc_size);

                /* FIXME: We walk through relocations for each code section
                 * and hence stats can be counted more than once.
                 */
                if (base_reloc != NULL && base_reloc_size > 0) {
                    int refs_found = find_relocation_references(
                        dcontext, module_base, module_size, base_reloc, base_reloc_size,
                        relocation_delta, code_start, code_end);
                    if (refs_found >= 0) {
                        found += refs_found;
                        /* PR 215408: even when we have reloc info, we need to scan
                         * for rip-rel lea for any module whose code we haven't
                         * executed.  We assume we don't need to scan the executable
                         * (not executed from until post-inject).  We assume that all
                         * modules loaded at inject time need to be scanned (even
                         * though they may not all have been executed from).  Since
                         * early inject should be the norm, we should usually only be
                         * scanning ntdll, which reduces the perf impact.  For
                         * native_exec (PR 277044) and for hardcoded hooks (PR 277064)
                         * we also scan on violation for modules we didn't scan
                         * proactively here.
                         */
                        if (IF_X64(at_violation ||)(
                                !dynamo_initialized && DYNAMO_OPTION(rct_scan_at_init) &&
                                module_base != get_own_peb()->ImageBaseAddress)) {
                            LOG(GLOBAL, LOG_RCT, 1,
                                "add_rct_module: scanning " PFX
                                " even though has relocs\n",
                                module_base);
                            DOSTATS({
                                if (IF_X64_ELSE(at_violation, false))
                                    STATS_INC(rct_scan_at_vio);
                                else
                                    STATS_INC(rct_scan_at_init);
                            });
                        } else
                            scan_all_addresses = false;
                    } else {
                        /* relocation section found, but no relocations?
                         * fall back to scanning all addresses, as backup
                         */
                        LOG(GLOBAL, LOG_RCT, 1,
                            "add_rct_module: relocation section found, but "
                            "no relocations.  Falling back to scanning all "
                            "addresses\n");
                        STATS_INC(rct_ind_branch_no_valid_targets);
                    }
                }
            }

            /* Don't use or couldn't find relocation info, scan all
             * addresses to find ind branch targets
             */
            if (scan_all_addresses) {
                found += find_address_references_by_region(dcontext, text_start, text_end,
                                                           code_start, code_end);
            }

            STATS_INC(rct_ind_branch_sections_analyzed);
        }
    }

    entry_point = get_module_entry(module_base);

    /* add the module entry point (executable's main and DllMain()) */
    /* since they don't have to be exported nor address taken */
    if (entry_point != NULL) {
        DODEBUG({
            if (entry_point < module_base || entry_point >= module_base + module_size) {
                const char *target_module_name = NULL;
                /* may not be added to our own module list yet as we often get
                 * here when processing the exe, often the 1st module we see
                 */
                if (module_info_exists(entry_point)) {
                    target_module_name =
                        os_get_module_name_strdup(entry_point HEAPACCT(ACCT_VMAREAS));
                    ASSERT(target_module_name != NULL);
                }
                if (target_module_name == NULL) {
                    target_module_name =
                        get_module_short_name_uncached(dcontext, entry_point,
                                                       false /*!at map*/
                                                       HEAPACCT(ACCT_VMAREAS));
                }
                ASSERT_CURIOSITY(target_module_name != NULL ||
                                 /* for partial map, entry point may not be mapped in */
                                 EXEMPT_TEST("win32.partial_map.exe"));

                /* case 5776 see if it was rerouted outside of the module, e.g. in .NET */
                /* FIXME: we still can't tell whether it was modified */
                LOG(GLOBAL, LOG_RCT, 1,
                    "entry point outside of module: %s " PFX "-" PFX ", entry point=" PFX
                    ", in %s\n",
                    modname, module_base, module_base + module_size, entry_point,
                    target_module_name == NULL ? "<null>" : target_module_name);

                if ((target_module_name == NULL ||
                     !check_filter("mscoree.dll", target_module_name)) &&
                    !check_filter("win32.partial_map.exe",
                                  get_short_name(get_application_name()))) {
                    SYSLOG_INTERNAL_WARNING(
                        "entry point outside of module: %s " PFX "-" PFX
                        ", entry point=" PFX " %s\n",
                        modname, module_base, module_base + module_size, entry_point,
                        target_module_name == NULL ? "<null>" : target_module_name);
                    /* Quiet for partial_map test case (where isn't actually modified,
                     * just not mapped in). */
                    ASSERT_CURIOSITY(false && "modified entry point" ||
                                     EXEMPT_TEST("win32.partial_map.exe"));
                }
                if (target_module_name != NULL)
                    dr_strfree(target_module_name HEAPACCT(ACCT_VMAREAS));
            }
        });
        rct_add_valid_ind_branch_target(dcontext, entry_point);
    } else {
        /* curious if any module other than ntdll.dll has no entry point,
         * see above note why this would be important
         */
        LOG(GLOBAL, LOG_RCT, 1, "add_rct_module: %s, NULL entry point=" PFX "\n", modname,
            entry_point);
    }

    LOG(GLOBAL, LOG_RCT, 2,
        "add_rct_module: %s : %d ind targets for %d size, entry=" PFX "\n", modname,
        found, module_size, entry_point);

    rct_add_exports(dcontext, module_base, module_size);
    /* FIXME: case 1948 curiosity: dump exported entries that are also address taken */

    /* PR 250395: add SEH handlers from .pdata section */
    IF_X64(add_SEH_to_rct_table(dcontext, module_base);)
}

/* Analyze a range in a possibly new module
 * and add all valid targets for rct_ind_branch_check
 */
static void
rct_analyze_module_at_load(dcontext_t *dcontext, app_pc module_base, size_t module_size,
                           ssize_t relocation_delta)
{
    ASSERT(module_size != 0 && is_readable_pe_base(module_base));
    DOCHECK(1, {
        MEMORY_BASIC_INFORMATION mbi;
        /* xref case 8192, expect to only be analyzing IMAGE memory */
        ASSERT(query_virtual_memory(module_base, &mbi, sizeof(mbi)) == sizeof(mbi) &&
               mbi.Type == MEM_IMAGE);
    });

    /* do not analyze and add targets in dynamorio.dll */
    if (is_in_dynamo_dll(module_base))
        return;

    LOG(GLOBAL, LOG_RCT, 1,
        "rct_analyze_module_at_load: module_base=" PFX ", module_size=%d, "
        "relocation_delta=%c" PFX "\n",
        module_base, module_size, relocation_delta < 0 ? '-' : ' ',
        relocation_delta < 0 ? -relocation_delta : relocation_delta);

    add_rct_module(dcontext, module_base, module_size, relocation_delta, false);
}

/* Analyze a range, in a possibly new module (if we don't analyze_at_load),
 * return false if not a code section in a module
 * otherwise return true and add all valid targets for rct_ind_branch_check
 * NOTE - under current default options (analyze_at_load) this routine is only
 * used for its is_in_code_section (&IMAGE) return value. */
bool
rct_analyze_module_at_violation(dcontext_t *dcontext, app_pc target_pc)
{
    app_pc module_base;
    MEMORY_BASIC_INFORMATION mbi;
    size_t module_size = get_allocation_size(target_pc, &module_base);
    uint sec_flags = 0;

    if (module_base == NULL || !is_readable_pe_base(module_base))
        return false;

    /* xref case 8192, don't consider target a code section if it's not MEM_IMAGE
     * (since if MEM_PRIVATE or MEM_MAPPED we won't have analyzed at_load) */
    if (query_virtual_memory(module_base, &mbi, sizeof(mbi)) == sizeof(mbi) &&
        mbi.Type != MEM_IMAGE) {
        SYSLOG_INTERNAL_WARNING_ONCE("Transfer to non-IMAGE memory " PFX " that looks "
                                     "like a pe module.",
                                     target_pc);
        return false;
    }

    LOG(GLOBAL, LOG_RCT, 1, "rct_analyze_module_at_violation: target_pc=" PFX "\n",
        target_pc);

    /* test what area is target_pc in and enforce and analyze only if
     * in desired section type.  Default rct_section_type is code section only.
     */
    ASSERT(DYNAMO_OPTION(rct_section_type) != 0); /* sentinel for is_in_executable_... */
    if (!is_in_executable_file_section(module_base, target_pc, target_pc + 1, NULL, NULL,
                                       NULL, &sec_flags, NULL,
                                       DYNAMO_OPTION(rct_section_type), NULL,
                                       false /* no need to merge */, -1, false) ||
        (DYNAMO_OPTION(rct_section_type_exclude) != 0 &&
         TESTALL(DYNAMO_OPTION(rct_section_type_exclude), sec_flags))) {
        SYSLOG_INTERNAL_WARNING_ONCE("RCT executing from non-analyzed module "
                                     "section at " PFX,
                                     target_pc);
        /* FIXME: heavy-weight check if done every time for execution
         * off .data section until it makes it into a trace
         */
        return false;
    }

    /* analyze the module if we haven't already done so at load and if it is
     * not dynamorio.dll (case 7266)
     */
    if ((!DYNAMO_OPTION(rct_analyze_at_load) && !is_in_dynamo_dll(module_base))
#    if defined(X64) && (defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH))
        /* We need to scan for rip-rel leas we didn't see execute:
         * for native_exec (PR 277044) and hardcoded hooks (PR 277064) */
        || !os_module_get_flag(module_base, MODULE_RCT_SCANNED)
#    endif
    ) {
        add_rct_module(dcontext, module_base, module_size, 0 /* already relocated */,
                       true);
    }

    return true;
}

/* module map/unmap processing relevant to the RCT policies */
void
rct_process_module_mmap(app_pc module_base, size_t module_size, bool add,
                        bool already_relocated)
{
    DEBUG_DECLARE(char modname[MAX_MODNAME_INTERNAL];)
    /* although we expect MEM_IMAGE regions to be indeed only PE's,
     * not taking chances
     */
    if (!is_readable_pe_base(module_base))
        return;
    DODEBUG(
        { os_get_module_name_buf(module_base, modname, BUFFER_SIZE_ELEMENTS(modname)); });

    if (add) {
        ssize_t delta = 0;

        /* we can't just check -no_use_persisted_rct since Borland overrides that */
        if (DYNAMO_OPTION(use_persisted) &&
            os_module_get_flag(module_base, MODULE_RCT_LOADED)) {
            /* We can only skip analyzing the module if the persisted data
             * covers the ENTIRE module, and we rely on the persisted cache
             * using a flag stored at persist time to indicate that, as the RCT
             * entries are not just drawn from the text section(s) but from
             * other sections (like .orpc) that do not correspond to the normal
             * persisted cache bounds.  If we persisted only some of the entries
             * we'll re-analyze the whole module and waste some work, but we do
             * check for duplicates before adding.
             *
             * FIXME case 8648: invisible IAT hooker (e.g., Kaspersky) could
             * cause problems, so perhaps we should also always do
             * rct_add_exports() here?
             */
            STATS_INC(rct_ind_branch_modules_persist_loaded);
            LOG(GLOBAL, LOG_RCT, 2,
                "rct_process_module_mmap: not processing " PFX " b/c persisted\n",
                module_base);
            return;
        }

        /* we need to know whether the module was relocated already
         * (e.g. when we're taking over), or it's just mapped and
         * therefore we should relocate.  If it has been relocated and we
         * assume it needs further relocation we'll have the wrong
         * offsets, so need to know that.
         */
        if (!already_relocated) {
            /* delta = OLD - NEW since usually used to add to an
             * address in a relocated file, while here we have not yet
             * relocated one.  We want to add to OLD addresses the
             * negated value NEW - OLD = - delta */
            delta = -get_module_preferred_base_delta(module_base);
            LOG(GLOBAL, LOG_RCT, 2,
                "rct_process_module_mmap: " PFX " relocation_delta=%c" PFX "\n",
                module_base, delta < 0 ? '-' : ' ', delta < 0 ? -delta : delta);
        }

        /* Grab the rct_module_lock to ensure no conflicts while
         * processing entries
         */
        d_r_mutex_lock(&rct_module_lock);
        if (DYNAMO_OPTION(rct_analyze_at_load)) {
            /* should use GLOBAL_DCONTEXT since called early */
            rct_analyze_module_at_load(GLOBAL_DCONTEXT, module_base, module_size, delta);
        }

        if (DYNAMO_OPTION(rct_modified_entry)) {
            /* case 5776 - where mscoree.dll modifies the image entry point */
            /* FIXME: case 5354: rct_analyze_at_load took care of most of this.
             * Clean up.
             */
            app_pc entry_point = get_module_entry(module_base);
            /* assume unmodified entry_point if within module or NULL.
             * NOTE that entry_point RVA of 0 is the module_base,
             * yet that can't possibly be valid NT header as well as entry point.
             * We'll assume we DON'T need to read the LDR structure in that case.
             */
            bool use_ldr = ((entry_point < module_base) ||
                            (entry_point >= module_base + module_size));

            if (already_relocated) {
                /* loaded before we were in control */
                if (use_ldr) {
                    /* modified PE image entry - we should look up the LDR entry point. */
                    /* Only seen for executables, but statically linked
                     * DLLs may have this too.  Though still unclear
                     * what's the point of modifying the PE entry if the
                     * loader uses the original entry point from LDR structure.
                     */
                    LDR_MODULE *mod = get_ldr_module_by_pc(module_base);
                    /* walking the loader list is, of course unsafe,
                     * unless we're the single thread at the time of
                     * DR initialization
                     */
                    ASSERT_CURIOSITY(check_sole_thread());

                    ASSERT(use_ldr || (mod != NULL && mod->EntryPoint == entry_point) ||
                           /* msvcrt40.dll is a good example of no entry point */
                           (mod != NULL && mod->EntryPoint == 0 &&
                            entry_point == module_base));
                    if (use_ldr && mod != NULL) {
                        entry_point = mod->EntryPoint;
                        LOG(GLOBAL, LOG_RCT, 1,
                            "rct_process_module_mmap: %s "
                            ".NET modified entry point=" PFX "\n",
                            modname, entry_point);
                        DODEBUG({
                            SYSLOG_INTERNAL_WARNING("rct_process_module_mmap: %s "
                                                    ".NET modified entry point=" PFX "\n",
                                                    modname, entry_point);
                            /* we expect mscoree!_CorDllMain or mscoree!_CorExeMain */
                        });
                    }
                }
            } else {
                /* Quiet assert for partial map test case where we are tricked above
                 * into thinking the entry point has been modified (it just hasn't been
                 * mapped in). */
                ASSERT_CURIOSITY(!use_ldr || EXEMPT_TEST("win32.partial_map.exe"));
                ASSERT(get_thread_private_dcontext() != NULL);
                /* newly loaded module - we should add the PE entry point now */
            }

            DODEBUG({
                /* FIXME: rct_analyze_module_at_violation counts on most DLLs
                 * having an entry point, so analysis is not deferred
                 * for too long - see case 5354
                 */
                if (entry_point == module_base) {
                    /* shdoclc.dll, xpsp2res.dll and msls31.dll are known
                     * instances in notepad on XP SP2 */
                    LOG(GLOBAL, LOG_RCT, 1,
                        "rct_process_module_mmap: %s, "
                        "entry point=NULL\n",
                        modname != NULL ? modname : "<null>");
                }
            });

            /* Add the module entry point DllMain() for mmap'ed region, before it
             * is modified.
             */
            if (entry_point != NULL) {
                /* should use GLOBAL_DCONTEXT since called early */
                rct_add_valid_ind_branch_target(GLOBAL_DCONTEXT, entry_point);
                LOG(GLOBAL, LOG_RCT, 2,
                    "rct_process_module_mmap: %s, entry point=" PFX "\n", modname,
                    entry_point);

            } else {
                LOG(GLOBAL, LOG_RCT, 1, "rct_process_module_mmap: %s, entry point=NULL\n",
                    modname);
                ASSERT_NOT_REACHED();
            }
        }

        d_r_mutex_unlock(&rct_module_lock);
    } else {
        /* case 9672: we now use per-module tables, so we don't need to
         * take any explicit action here; the tables will simply be removed.
         */
    }
}

#    ifdef DEBUG
/* FIXME: this is an inefficient hack using the find_predecessor data
 * structures. The right solution for this problem is to add all
 * entries to a hashtable when walking the module exports table.
 */
/* we're still using this to collect some statistics in rct_  */
bool
rct_is_exported_function(app_pc tag)
{
    module_info_t mod = { 0 }, *pmod;
    d_r_mutex_lock(
        &process_module_vector.lock); /* FIXME: this can be a shared read lock */
    {
        pmod = lookup_module_info(&process_module_vector, tag);
        if (pmod) {
            mod = *pmod; /* keep a copy in case of reallocations */
            /* the data will be invalid only in a race condition,
             * where some other thread frees the library
             */
        }
    }
    d_r_mutex_unlock(&process_module_vector.lock);

    if (pmod) {
        int i = find_predecessor(mod.exports_table, mod.exports_num, tag);
        if (i >= 0 && mod.exports_table[i].entry_point == tag) {
            return true;
        }
    }
    return false;
}
#    endif /* DEBUG */

#endif /* RCT_IND_BRANCH */
/****************************************************************************/

void
os_modules_init(void)
{
    section2file_table = generic_hash_create(
        GLOBAL_DCONTEXT, INIT_HTABLE_SIZE_SECTION,
        80 /* load factor: not perf-critical */, HASHTABLE_SHARED | HASHTABLE_PERSISTENT,
        (void (*)(dcontext_t *, void *))section_to_file_free _IF_DEBUG(
            "section-to-file table"));

#ifndef STATIC_LIBRARY
    if (DYNAMO_OPTION(hide) && !dr_earliest_injected) {
        /* retrieve path before hiding, since this is called before d_r_os_init() */
        get_dynamorio_library_path();
        hide_from_module_lists();
    }
#endif
}

void
os_modules_exit(void)
{
    generic_hash_destroy(GLOBAL_DCONTEXT, section2file_table);
}

void
free_module_names(module_names_t *mod_names HEAPACCT(which_heap_t which))
{
    ASSERT(mod_names != NULL);

    if (mod_names->module_name != NULL)
        dr_strfree(mod_names->module_name HEAPACCT(which));
    if (mod_names->file_name != NULL)
        dr_strfree(mod_names->file_name HEAPACCT(which));
    if (mod_names->exe_name != NULL)
        dr_strfree(mod_names->exe_name HEAPACCT(which));
    if (mod_names->rsrc_name != NULL)
        dr_strfree(mod_names->rsrc_name HEAPACCT(which));
}

void
module_copy_os_data(os_module_data_t *dst, os_module_data_t *src)
{
    memcpy(dst, src, sizeof(*dst));
}

/* destructor for os-specific module area fields */
void
os_module_area_reset(module_area_t *ma HEAPACCT(which_heap_t which))
{
    ASSERT(TEST(MODULE_BEING_UNLOADED, ma->flags));

    /* Modules are always contiguous (xref i#160/PR 562667) */
    module_list_remove_mapping(ma, ma->start, ma->end);

    if (ma->full_path != NULL)
        dr_strfree(ma->full_path HEAPACCT(which));
    if (ma->os_data.company_name != NULL)
        dr_strfree(ma->os_data.company_name HEAPACCT(which));
    if (ma->os_data.product_name != NULL)
        dr_strfree(ma->os_data.product_name HEAPACCT(which));

    if (ma->os_data.noclobber_section_handle != INVALID_HANDLE_VALUE) {
        bool ok = close_handle(ma->os_data.noclobber_section_handle);
        ASSERT_CURIOSITY(ok);
    }

#if defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH)
    LOG(GLOBAL, LOG_RCT, 1, "freeing RCT/RAC tables for %s " PFX "-" PFX "\n",
        GET_MODULE_NAME(&ma->names), ma->start, ma->end);
    {
        uint i;
        for (i = 0; i < RCT_NUM_TYPES; i++) {
            rct_module_table_free(GLOBAL_DCONTEXT, &ma->os_data.rct_table[i], ma->start);
        }
    }
#endif

    if (ma->os_data.iat_code != NULL) {
        /* iat code should be deleted earlier unless hooker that
         * fools our loader match */
        ASSERT_CURIOSITY(false && "iat code not deleted until unload");
        module_area_free_IAT(ma);
    }
}

/* In general, callers should use os_get_module_info() instead.
 *
 * Returns the timestamp from the PE file header, the checksum & image size from the
 * PE optional header for the module at the given base address. If pe_name is
 * non-null also returns the PE name as well (NOTE this can be NULL). code_size is
 * computed by summing up the unpadded sizes for all code or executable sections.
 * file_version is the 64-bit word from the resource section. All of the out arguments
 * can be NULL. Returns true on success.
 *
 * We use a combined routine for the checksum, timestamp, image size, code_size,
 * and pe_name since calls to is_readable_pe_base are expensive.
 *
 * FIXME : like many routines in module.c this routine is unsafe since the
 * module in question could be unloaded while we are still looking around its
 * header or before caller finishes using pe_name.  Need try-except.
 */
bool
get_module_info_pe(const app_pc module_base, uint *checksum, uint *timestamp,
                   size_t *image_size, char **pe_name, size_t *code_size)
{
    int i;
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt_hdr;
    IMAGE_SECTION_HEADER *sec;

    if (!is_readable_pe_base(module_base)) {
        /* TODO: dump some sort of message here?  for customer or dbg. engr.? */
        return false;
    }

    dos = (IMAGE_DOS_HEADER *)module_base;
    nt_hdr = (IMAGE_NT_HEADERS *)(((ptr_uint_t)dos) + dos->e_lfanew);
    if (timestamp != NULL)
        *timestamp = nt_hdr->FileHeader.TimeDateStamp;
    if (checksum != NULL)
        *checksum = nt_hdr->OptionalHeader.CheckSum;
    if (image_size != NULL)
        *image_size = nt_hdr->OptionalHeader.SizeOfImage;

    /* get_dll_short_name() usually shouldn't be called by itself, but through
     * get_module_short_name() which does checks.  In this particular case,
     * the exact PE name is needed and only get_dll_short_name() provides that;
     * get_module_short_name() can return the name from the command line
     * string, so we can't use it to find the PE name.
     *
     * CAUTION: If dll is unloaded the name is lost.  Also, ensure that checks
     *          performed by get_module_short_name() are replicated here.
     *
     * FIXME: if there is no exports section should we use the resource section
     * to get the file name?  alex thinks not, I agree; tim thinks we should;
     * I also think it is time to introduce distinct 'ignore' and 'not
     * available' values.
     */
    if (pe_name != NULL)
        *pe_name = get_dll_short_name(module_base);

    if (code_size != NULL) {
        *code_size = 0;
        sec = IMAGE_FIRST_SECTION(nt_hdr);
        for (i = 0; i < nt_hdr->FileHeader.NumberOfSections; i++, sec++) {
            /* Note a section may be executable but not marked as code; this
             * isn't common, but isn't rare - kbdus.dll.  See case 9053.
             */
            if (TESTANY(IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE,
                        sec->Characteristics)) {
                /* Executable sections should be loadable. */
                DODEBUG({
                    /* PR 214227: we do see INIT sections that are discardable */
                    if (TEST(IMAGE_SCN_MEM_DISCARDABLE, sec->Characteristics)) {
                        SYSLOG_INTERNAL_WARNING("found code section that is discardable");
                    }
                });
                *code_size += get_image_section_unpadded_size(sec _IF_DEBUG(nt_hdr));
            }
        }
        /* Can't assert on code_size > 0 because dlls can have no code sections. */
    }

    return true;
}

/* update our data structures that keep track of PE modules */
void
os_module_area_init(module_area_t *ma, app_pc base, size_t view_size, bool at_map,
                    const char *filepath HEAPACCT(which_heap_t which))
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    app_pc preferred_base = NULL;
    uint timestamp = 0;
    uint checksum = 0;
    size_t pe_size = 0;
    version_info_t info = { 0 };

    /* Modules are always contiguous (xref i#160/PR 562667) */
    module_list_add_mapping(ma, base, base + view_size);

    /* currently add is done post-map, and remove is pre-unmap
     * FIXME: we should remove at post-unmap, though unmap is unlikely to fail
     */
    ASSERT(is_readable_pe_base(base));

    /* FIXME: theoretically need to grab a lock to prevent
     * unmapping of a DLL that one thread is mapping and another
     * one is unmapping.  We don't know for sure that all
     * allocations that we see are really by the loader and
     * can't assume all to be synchronized.
     */
    ma->entry_point = get_module_entry(base);
    get_module_info_pe(base, &checksum, &timestamp, &pe_size, NULL,
                       &ma->os_data.code_size);
    get_module_resource_version_info(base, &info); /* we inited to zero so ok if fails */
    /* We pass in ma to get ma->full_path set, and &info to avoid
     * re-reading .rsrc for get_module_original_filename() (PR 536337)
     */
    get_all_module_short_names_uncached(dcontext, base, at_map, &ma->names, ma, &info,
                                        filepath HEAPACCT(which));
    ma->os_data.file_version = info.file_version;
    ma->os_data.product_version = info.product_version;
    /* This converts unicode to ascii which might not always be good, but we do select
     * the english version of the strings if available in the .rsrc section and
     * all current users compare with ascii strings anyways.  FIXME */
    if (info.company_name != NULL)
        ma->os_data.company_name = dr_wstrdup(info.company_name HEAPACCT(which));
    if (info.product_name != NULL)
        ma->os_data.product_name = dr_wstrdup(info.product_name HEAPACCT(which));

    if (TEST(ASLR_SHARED_CONTENTS, DYNAMO_OPTION(aslr_cache)) &&
        dcontext != NULL && /* if during initialization */
        dcontext->aslr_context.original_section_base != ASLR_INVALID_SECTION_BASE) {
        DEBUG_DECLARE(uint pe_timestamp = timestamp;)
        preferred_base = dcontext->aslr_context.original_section_base;
        /* note that modules loaded before we have taken over
         * are assumed to be using the native DLLs (unless we
         * have full detach and one day a reattach) */

        /* keep in parallel with aslr_track_areas() unmap logic,
         * to preserve _original_ app preferred base for shared mappings
         * which have PE.ImageBase set to the current mapping address
         */

        /* FIXME: case 1272 see also aslr_set_randomized_handle() for possibly
         * producing more identifying data when we are watching NtCreateSection
         */
        checksum = dcontext->aslr_context.original_section_checksum;
        timestamp = dcontext->aslr_context.original_section_timestamp;
        ASSERT(timestamp != 0 &&
               pe_timestamp == aslr_timestamp_transformation(timestamp));

        /* register handle that needs to be closed on unmap */
        ma->os_data.noclobber_section_handle =
            dcontext->aslr_context.original_image_section_handle;
        /* we could also duplicate the original we'd like to
         * emulate ObReferenceObjectByPointer() on the section
         * when a MapViewOfSection succeeds.
         */
        /* invalidate, so that we know we had a successful map */
        dcontext->aslr_context.original_image_section_handle = INVALID_HANDLE_VALUE;
    } else {
        preferred_base = get_module_preferred_base(base);
        ma->os_data.noclobber_section_handle = INVALID_HANDLE_VALUE;
    }

    /* Xref case 9782: pe_size isn't always page aligned.  Drivers don't require
     * page alignment and are sometimes mapped (though not loaded) into user
     * processes. According to Nebbett the kernel rounds up view size to the next
     * page multiple so since this is post syscall view size should already be
     * page aligned.
     * Xref case 9717: on Vista at least we sometimes see a view that isn't
     * the full image.  Shouldn't be a problem here, but is a problem for other
     * core components (reloc walking etc.). */
    ASSERT_CURIOSITY(ALIGN_FORWARD(pe_size, PAGE_SIZE) == view_size ||
                     EXEMPT_TEST("win32.partial_map.exe"));

    ASSERT(preferred_base != NULL);
    ma->os_data.preferred_base = preferred_base;
    ma->os_data.checksum = checksum;
    ma->os_data.timestamp = timestamp;
    ma->os_data.module_internal_size = pe_size;

    /* FIXME: case 9032 about getting MemorySectionName */
}

/* gets the preferred base of the module containing pc, cached from our module list,
 * returns NULL, if not in module
 */
app_pc
get_module_preferred_base_safe(app_pc pc)
{
    /* FIXME: currently just a little safer */
    module_area_t *ma;
    app_pc preferred_base = NULL;

    /* read lock to protect custom data */
    os_get_module_info_lock();
    ma = module_pc_lookup(pc);
    if (ma != NULL) {
        preferred_base = ma->os_data.preferred_base;
    }
    os_get_module_info_unlock();

    return preferred_base;
}

/* Gets module information of module containing pc, cached from our module list.
 * Returns false if not in module; none of the OUT arguments are set in that case.
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
os_get_module_info(const app_pc pc, OUT uint *checksum, OUT uint *timestamp,
                   OUT size_t *module_size, OUT const char **name, size_t *code_size,
                   uint64 *file_version)
{
    bool ok = false;
    module_names_t *names = NULL;

    /* read lock to protect custom data */
    if (name == NULL)
        os_get_module_info_lock();

    ASSERT(os_get_module_info_locked());
    ;

    ok = os_get_module_info_all_names(pc, checksum, timestamp, module_size, &names,
                                      code_size, file_version);
    if (name != NULL) {
        if (ok) {
            /* os_get_module_info_all_names() may pass and return NULL because
             * of a bug, guard against it! */
            ASSERT(names != NULL);
            if (names != NULL)
                *name = GET_MODULE_NAME(names);
        } else
            *name = NULL;
    }

    if (name == NULL)
        os_get_module_info_unlock();

    return ok;
}

/* Gets module information of module containing pc, cached in our module list.
 * Returns false if not in module; none of the OUT arguments are set in that case.
 * Note: this function returns all types of module names as fix for case 9842.
 */
/* cf. get_module_info_pe() which should be called on the original PE
 * to gather this information */
/* If names != NULL, caller must acquire the module_data_lock beforehand
 * and call os_get_module_info_unlock() when finished with the names
 * (caller can use dr_strdup to make a copy first if necessary; validity of the
 * names are guaranteed only as long as the caller holds module_data_lock).
 * If names == NULL, this routine acquires and releases the lock and the
 * caller has no obligations.
 * NOTE: though there is no use of this routine with names == NULL, this was
 * don't to make sure that both os_get_module_info*() have a consistent interface.
 */
bool
os_get_module_info_all_names(const app_pc pc, OUT uint *checksum, OUT uint *timestamp,
                             OUT size_t *module_size, OUT module_names_t **names,
                             size_t *code_size, uint64 *file_version)
{
    /* FIXME: currently just a little safer than looking up in PE itself */
    module_area_t *ma;
    bool ok = false;

    /* Lock rank order if holding report_buf_lock */
    ASSERT(!under_internal_exception());

    if (!is_module_list_initialized())
        return false;

    /* read lock to protect custom data */
    if (names == NULL)
        os_get_module_info_lock();

    ASSERT(os_get_module_info_locked());
    ;
    ma = module_pc_lookup(pc);
    if (ma != NULL) {
        ok = true;
        if (checksum != NULL)
            *checksum = ma->os_data.checksum;
        if (timestamp != NULL)
            *timestamp = ma->os_data.timestamp;
        if (module_size != NULL)
            *module_size = ma->os_data.module_internal_size; /* pe_size */
        if (names != NULL)
            *names = &(ma->names);
        if (code_size != NULL)
            *code_size = ma->os_data.code_size;
        if (file_version != NULL)
            *file_version = ma->os_data.file_version.version;
    } else {
        /* hotpatch DLLs show up here */
        /* FIXME case 5381: assert these are really only hotpatch dlls */
    }

    if (names == NULL)
        os_get_module_info_unlock();
    else {
        DODEBUG({
            /* Try to ensure nobody is calling us prior to adding or after removing */
            const char *tmp =
                get_module_short_name_uncached(get_thread_private_dcontext(), pc,
                                               false /*!at map*/
                                               HEAPACCT(ACCT_VMAREAS));
            /* Unfortunately we can't tell a coding error (calling too early/late)
             * from an app race so we syslog instead of asserting.
             * An unloaded list (case 6061) would help us out here.
             * Also, we don't want to complain about modules that need the
             * filename, so we only check one direction.
             */
            if (ma == NULL && tmp != NULL) {
                SYSLOG_INTERNAL_WARNING_ONCE("os_get_module_info: module list data "
                                             "mismatch w/ image: DR error, or race");
            }
            if (tmp != NULL)
                dr_strfree(tmp HEAPACCT(ACCT_VMAREAS));
        });
    }

    return ok;
}

#if defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH)
/* Caller must hold module_data_lock */
rct_module_table_t *
os_module_get_rct_htable(app_pc pc, rct_type_t which)
{
    module_area_t *ma;
    ASSERT(which >= 0 && which < RCT_NUM_TYPES);
    ma = module_pc_lookup(pc);
    if (ma != NULL)
        return &ma->os_data.rct_table[which];
    return NULL;
}
#endif

bool
os_module_store_IAT_code(app_pc addr)
{
    module_area_t *ma;
    bool found = false;
    app_pc IAT_start, IAT_end;
    os_get_module_info_write_lock();
    ma = module_pc_lookup(addr);
    ASSERT_CURIOSITY((ma == NULL || ma->os_data.iat_code == NULL) && "double store");
    if (ma != NULL && ma->os_data.iat_code == NULL /* no double store */ &&
        get_IAT_section_bounds(ma->start, &IAT_start, &IAT_end)) {
        ma->os_data.iat_len = (app_pc)ALIGN_FORWARD(IAT_end, PAGE_SIZE) - IAT_end;
        ma->os_data.iat_code =
            (byte *)global_heap_alloc(ma->os_data.iat_len HEAPACCT(ACCT_VMAREAS));
        memcpy(ma->os_data.iat_code, IAT_end, ma->os_data.iat_len);
        found = true;
    }
    os_get_module_info_write_unlock();
    return found;
}

bool
os_module_cmp_IAT_code(app_pc addr)
{
    module_area_t *ma;
    bool match = false;
    app_pc IAT_start, IAT_end;
    os_get_module_info_lock();
    ma = module_pc_lookup(addr);
    if (ma != NULL && ma->os_data.iat_code != NULL &&
        get_IAT_section_bounds(ma->start, &IAT_start, &IAT_end)) {
#ifdef INTERNAL
        DEBUG_DECLARE(app_pc text_start;)
        DEBUG_DECLARE(app_pc text_end;)
#endif
        size_t IAT_len = ((app_pc)ALIGN_FORWARD(IAT_end, PAGE_SIZE)) - IAT_end;
        match = (ma->os_data.iat_len == IAT_len &&
                 memcmp(ma->os_data.iat_code, IAT_end, ma->os_data.iat_len) == 0);
        LOG(GLOBAL, LOG_VMAREAS, 2,
            "comparing stored " PFX "-" PFX " with IAT " PFX "-" PFX "\n",
            ma->os_data.iat_code, ma->os_data.iat_code + ma->os_data.iat_len, IAT_end,
            IAT_end + IAT_len);
        /* in all uses so far we always expect to match, except when we mistake
         * a rebase of a single-page .text section for a rebind (case 10830).
         * note that we can't use the check here to distinguish those.
         */
        ASSERT_CURIOSITY(match ||
                         (ma->os_data.preferred_base != ma->start &&
                          is_in_code_section(ma->start, addr, &text_start, &text_end) &&
                          /* IAT and .text occupying same pages is what counts */
                          PAGE_START(text_start) == PAGE_START(IAT_start) &&
                          PAGE_START(text_end - 1) == PAGE_START(IAT_end - 1)));
    } else /* assert if we have stored code but fail to get iat bounds */
        ASSERT(ma == NULL || ma->os_data.iat_code == NULL);
    os_get_module_info_unlock();
    return match;
}

static bool
module_area_free_IAT(module_area_t *ma)
{
    ASSERT(os_get_module_info_write_locked());
    if (ma != NULL && ma->os_data.iat_code != NULL /* no double free */) {
        DOCHECK(1, {
            app_pc IAT_start;
            app_pc IAT_end;
            get_IAT_section_bounds(ma->start, &IAT_start, &IAT_end);
            ASSERT(ALIGN_FORWARD(IAT_end, PAGE_SIZE) - (ptr_uint_t)IAT_end ==
                   ma->os_data.iat_len);
        });
        global_heap_free(ma->os_data.iat_code,
                         ma->os_data.iat_len HEAPACCT(ACCT_VMAREAS));
        ma->os_data.iat_code = NULL;
        ma->os_data.iat_len = 0;
        return true;
    }
    return false;
}

bool
os_module_free_IAT_code(app_pc addr)
{
    module_area_t *ma;
    bool found = false;
    os_get_module_info_write_lock();
    ma = module_pc_lookup(addr);
    if (ma != NULL && ma->os_data.iat_code != NULL) {
        found = module_area_free_IAT(ma);
        ASSERT(found);
    }
    os_get_module_info_write_unlock();
    return found;
}

/* applies relocations to PE (SEC_IMAGE) file.
 * If !protect_incrementally, assumes all sections
 * have been made writable, similarly caller is responsible for
 * restoring any section protection if needed; else,
 * makes pages writable and restores prot as it goes.
 *
 * returns false if some unhandled error condition (e.g. unknown
 * relocation type), should signal failure to relocate.
 *
 * FIXME: need to generalize this as a callback interface to fit
 * find_relocation_references() which it is mostly copied from.
 * should start using module_reloc_iterator_start()
 * Currently used only for ASLR_SHARED_CONTENTS
 */
static bool
module_apply_relocations(app_pc module_base, size_t module_size,
                         IMAGE_BASE_RELOCATION *base_reloc, size_t base_reloc_size,
                         ssize_t relocation_delta, bool protect_incrementally)
{
    app_pc relocs = NULL;        /* reloc entries */
    app_pc relocs_end = 0;       /* to iterate through reloc entries */
    app_pc relocs_block_end = 0; /* to iterate through reloc blocks */
    bool is_module_32bit;

    int references_found = -1;
    DEBUG_DECLARE(uint addresses_fixedup = 0; uint pages_touched = 0;
                  app_pc module_end = module_base + module_size;
                  app_pc original_preferred_base =
                      get_module_preferred_base(module_base););

    ASSERT(sizeof(uint) == 4);   /* size of rva and block_size - 32 bits */
    ASSERT(sizeof(ushort) == 2); /* size of reloc entry - 16 bits */

    ASSERT(module_base != NULL); /* caller should have verified base */

    /* callers set up base_reloc and base_reloc_size by calling
     * get_module_base_reloc */
    if (base_reloc == NULL || base_reloc_size == 0) {
        ASSERT(false && "expect relocations");
        return 0;
    }
    is_module_32bit = module_is_32bit(module_base);

    ASSERT(is_readable_without_exception((app_pc)base_reloc, base_reloc_size));

    /* Iterate through relocation entries and check if they contain
     * references to code section
     */
    relocs = (app_pc)base_reloc;
    relocs_end = relocs + base_reloc_size;

    while (relocs < relocs_end) {
        /* Image based relocation is stored as:
         * DWORD RVA,
         * DWORD SizeOfBlock
         * followed by a WORD array relocation entries
         *   number of relocation entries =
         *       (SizeOfBlock - IMAGE_SIZEOF_BASE_RELOCATION) / sizeof(ushort)
         */
        uint rva = (uint)((IMAGE_BASE_RELOCATION *)relocs)->VirtualAddress;
        uint block_size = (uint)((IMAGE_BASE_RELOCATION *)relocs)->SizeOfBlock;
        app_pc prot_pc = NULL;
        size_t prot_size = 0;
        uint orig_prot = 0;

        LOG(GLOBAL, LOG_RCT, 6, "\t %8x RVA, %8x SizeofBlock\n", rva, block_size);
        /* WORD array of relocation entries
         *   top 4-bits are type
         *       [IMAGE_REL_BASED_ABSOLUTE | IMAGE_REL_BASED_HIGHLOW]
         *   remaining 12-bits are offset relative to RVA
         *
         * 0:000> dw 77c10000 + 55000
         * 77c65000  1000 0000 0158 0000 327c 3288 328c 3290
         * 77c65010  3294 3298 32a4 32a8 32ac 32b8 32ec 32f0
         * ...
         */
        relocs_block_end = relocs + block_size;
        relocs = relocs + IMAGE_SIZEOF_BASE_RELOCATION;

        if (protect_incrementally) {
            /* Make target page writable.  Each relocation block is for one
             * page, but the final ref can touch the next page, so to do one
             * page at a time which may be more performant on x64 (marking
             * writable costs pagefile usage up front, and x64 has few
             * relocations) would require checking whether next page is in same
             * region anyway: for simplicity we do whole region at once.
             */
            byte *first_pc = (app_pc)RVA_TO_VA(
                module_base, (rva + IMAGE_REL_BASED_OFFSET(*(ushort *)relocs)));
            if (!get_memory_info(first_pc, &prot_pc, &prot_size, NULL))
                return false;
            if (!protect_virtual_memory((void *)prot_pc, prot_size, PAGE_READWRITE,
                                        &orig_prot))
                return false; /* failed to make writable */
        }

        while (relocs < relocs_block_end) {
            bool unsup_reloc = false;
            app_pc ref = process_one_relocation(
                module_base, relocs, rva, relocation_delta, true /* apply reloc */, NULL,
                &unsup_reloc, NULL, is_module_32bit _IF_DEBUG(module_size));
            if (unsup_reloc)
                return false; /* unsupported fixup */

            DODEBUG({
                /*
                 * curiosity: sometimes ref is not within module,
                 * is this in result of some compiler optimization on pointer arithmetic?
                 * or something else we don't get
                 * 75f80000 7607d000   browseui   I:\WINDOWS\System32\browseui.dll
                 *
                 * browseui!Ordinal103+0x741:
                 * 75fa5e0a ff34bd44e9f775   push    dword ptr [75f7e944+edi*4]
                 */

                /* ref has fixup applied so subract it to get original */
                app_pc original_ref = ref - relocation_delta;
                if (original_ref < original_preferred_base ||
                    original_ref >= original_preferred_base + module_size) {
                    LOG(GLOBAL, LOG_RCT, 1,
                        "  ref " PFX " outside module " PFX "-" PFX "\n", original_ref,
                        original_preferred_base, original_preferred_base + module_size);
                }
            });
            DODEBUG(addresses_fixedup++;);

            relocs = relocs + sizeof(ushort);
        } /* a block of relocation entries */

        /* each block of relocation entries is for a 4k (PAGE_SIZE) page */
        DODEBUG(pages_touched++;);

        if (protect_incrementally) {
            if (!protect_virtual_memory((void *)prot_pc, prot_size, orig_prot,
                                        &orig_prot))
                return false; /* failed to restore prot */
        }
    } /* all relocation entries */

    LOG(GLOBAL, LOG_RCT, 2,
        "reloc: module_apply_relocations:  "
        "fixed up %u addresses, touched %u pages\n",
        addresses_fixedup, pages_touched);

    return true;
}

/* iterator over a PE .reloc section */
/*
 * Currently used only for ASLR_SHARED_CONTENTS validation.
 * FIXME: see module_apply_relocations() and
 * find_relocation_references() which can take advantage of this.
 * Note that inner loop may be somewhat slower than a custom iterator but
 * considering on average there are about 100 relocations per 4K page
 * the overhead of read, or worse yet write, page-in faults may dwarf it.
 */
typedef struct reloc_iterator_t {
    app_pc relocs;           /* current reloc entry pointer */
    app_pc relocs_end;       /* end of all reloc entries */
    app_pc relocs_block_end; /* end of current reloc blocks */
    uint rva_page;           /* current page RVA */
    app_pc module_base;

    /* FIXME: note that we currently assume clients need sequential
     * iteration in sorted order of requests.  We also assume that the
     * .reloc entries themselves are in sorted order.  If any of these
     * is not true - e.g. we want to lookup any random page on-demand
     * or if a DLL exists with relocations not in order, we'd have to
     * do a search through the .reloc block entries on backwards
     * accesses, or would keep in a sorted array pointers to each
     * reloc block for really fast random access.
     */
    DEBUG_DECLARE(app_pc last_addr;) /* helping verify requests are sorted */

    /* FIXME: some of the above types should be improved to allow
     * pointer arithmetic instead of explicit conversions, e.g. relocs
     * should be ushort*, yet the first implementation is copy&paste
     * from find_relocation_references() and for now is better left
     * matching the well tested code.
     */
} reloc_iterator_t;

static void
module_reloc_iterator_next_block_internal(reloc_iterator_t *ri)
{
    while (ri->relocs >= ri->relocs_block_end && ri->relocs < ri->relocs_end) {
        /* checking if relocs are really sorted in known DLLs */
        DEBUG_DECLARE(uint last_rva_page = ri->rva_page;)
        uint block_size = (uint)((IMAGE_BASE_RELOCATION *)ri->relocs)->SizeOfBlock;
        ri->rva_page = (uint)((IMAGE_BASE_RELOCATION *)ri->relocs)->VirtualAddress;

        ASSERT((ri->rva_page > last_rva_page ||
                last_rva_page == 0 /* odbcint.dll has an empty .reloc */) &&
               ".reloc RVA blocks not sorted");
        LOG(GLOBAL, LOG_RCT, 6, "\t %8x RVA, %8x SizeofBlock\n", ri->rva_page,
            block_size);

        ri->relocs_block_end = ri->relocs + block_size;
        ri->relocs = ri->relocs + IMAGE_SIZEOF_BASE_RELOCATION;

        ASSERT(ri->relocs <= ri->relocs_block_end);
        ASSERT(ri->relocs_block_end <= ri->relocs_end);
        /* the while loop handles the probably never to be seen corner
         * case when there are no reloc entries, yet there is a block
         * entry */
    }
}

/* returns false if iterator cannot be started, if there are no
 * relocations, but even then allows module_reloc_iterator_next() to
 * be safely called
 */
bool
module_reloc_iterator_start(reloc_iterator_t *ri /* OUT */, app_pc module_base,
                            size_t module_size)
{
    IMAGE_BASE_RELOCATION *base_reloc;
    size_t base_reloc_size;
    ASSERT(sizeof(uint) == 4);   /* size of rva and block_size - 32 bits */
    ASSERT(sizeof(ushort) == 2); /* size of reloc entry - 16 bits */

    ASSERT(module_base != NULL); /* caller should have verified base */

    base_reloc = get_module_base_reloc(module_base, &base_reloc_size);

    if (base_reloc == NULL || base_reloc_size == 0) {
        /* we may currently process DLLs that don't have relocations
         * e.g. xpsp2res.dll
         */
        SYSLOG_INTERNAL_WARNING_ONCE("module_reloc_iterator_start: no relocations");

        /* allows one to call module_reloc_iterator_next() even if
         * there are no relocations */
        ri->relocs = ri->relocs_end = NULL;

        return false;
    }

    ASSERT(is_readable_without_exception((app_pc)base_reloc, base_reloc_size));

    ri->module_base = module_base;
    ri->relocs = (app_pc)base_reloc;
    ri->relocs_end = ri->relocs + base_reloc_size;
    ri->rva_page = 0;
    DODEBUG({ ri->last_addr = 0; });
    ri->relocs_block_end = 0; /* to iterate through reloc blocks */
    /* need to set up first block */
    module_reloc_iterator_next_block_internal(ri);
    if (ri->relocs >= ri->relocs_end) {
        /* bad .reloc section */
        /* odbcint.dll is a good example here */
        /* BASE RELOCATIONS #2        ; (section #2)
         * 0 RVA,        8 SizeOfBlock
         */
        /* note we can still call module_reloc_iterator_next() */
        ASSERT_NOT_TESTED();
        return false;
    }

    return true;
}

/* FIXME: if need arises should add an iterator destructor
 * module_reloc_iterator_stop(reloc_iterator_t *ri)
 * currently all state is local
 */

/* returns location for next relocation to be applied
 * Note it is ok for someone to ask multiple times for the same relocation
 * (e.g. if it was beyond a section bound in a previous request).
 * Yet once an address is skipped we don't go back.
 * currently IMAGE_REL_BASED_HIGHLOW is the only supported type on x86-32 &
 * IMAGE_REL_BASED_DIR64 is the only supported type on x86-64
 */
/* FIXME: [perf] we could also pass the section
 * boundary so that we move the iterator in the common
 * case, and not move only when reaching next section.
 */
/* Note this interface currently doesn't provide random access.  All
 * users are expected to only need sequential access, with a single
 * read-ahead, and we only we support randomly skipping forward.*/
static app_pc
module_reloc_iterator_next(reloc_iterator_t *ri,
                           app_pc successor_of /* greater or equal */)
{
    DEBUG_DECLARE(int skipped = 0;)
    DEBUG_DECLARE(size_t module_size = get_allocation_size(ri->module_base, NULL);)
    bool is_module_32bit = module_is_32bit(ri->module_base);
    /* make sure requests are in increasing order, otherwise we'd have
     * to be able to start searching from the beginning.  At least
     * PEcoff v.8 Section 4 prescribes that VAs for sections must be
     * in ascending order and adjacent.
     */
    ASSERT_CURIOSITY(ri->last_addr < successor_of);
    DODEBUG({ ri->last_addr = successor_of; });

    /* Image based relocation is stored as:
     * DWORD RVA,
     * DWORD SizeOfBlock
     * followed by a WORD array relocation entries
     *   number of relocation entries =
     *       (SizeOfBlock - IMAGE_SIZEOF_BASE_RELOCATION) / sizeof(ushort)
     */
    /* WORD array of relocation entries
     *   top 4-bits are type
     *       [IMAGE_REL_BASED_ABSOLUTE | IMAGE_REL_BASED_HIGHLOW]
     *   remaining 12-bits are offset relative to RVA
     *
     * 0:000> dw 77c10000 + 55000
     * 77c65000  1000 0000 0158 0000 327c 3288 328c 3290
     * 77c65010  3294 3298 32a4 32a8 32ac 32b8 32ec 32f0
     * ...
     */

    /* all relocation entries */
    while (ri->relocs < ri->relocs_end) {
        /* FIXME: [minor perf] we could take this check out of the
         * innermost loop, but will need another routine to check if
         * requests are after the end. */

        /* a block of relocation entries */
        do {
            /* module_reloc_iterator_next_block_internal() makes sure
             * we're always within a block
             */
            bool unsup_reloc = false;
            app_pc cur_addr;
            app_pc ref = process_one_relocation(
                ri->module_base, ri->relocs, ri->rva_page, 0 /* not relocating here */,
                false /* don't apply reloc */, NULL, &unsup_reloc, &cur_addr,
                is_module_32bit _IF_DEBUG(module_size));
            if (unsup_reloc)
                return NULL; /* unsupported fixup */

            if (cur_addr >= successor_of) {
                /* found an address larger than requested */
                return cur_addr;
            }

            DODEBUG({ skipped++; });
            /* otherwise keep churning */

            /* we don't normally expect anyone to skip a relocation
             * unless we're using a short ASLR_PERSISTENT_PARANOID_PREFIX
             */
            ASSERT_CURIOSITY(
                skipped == 1 ||
                TEST(ASLR_PERSISTENT_PARANOID_PREFIX, DYNAMO_OPTION(aslr_validation)));

            ri->relocs = ri->relocs + sizeof(ushort);
        } while (ri->relocs < ri->relocs_block_end); /* page block */

        module_reloc_iterator_next_block_internal(ri);
    }

    /* once we are beyond the end - (or if there were no relocations
     * at all) all future requests will always get NULL.  We don't
     * need a module_reloc_iterator_hasnext() in this scheme.
     */
    return NULL; /* not found */
}

bool
module_make_writable(app_pc module_base, size_t module_size)
{
    /* We can't always change the protections for the whole module
     * with a single call, instead we have to do it image section by
     * section.  Although, when used for a module that we have just
     * mapped and nobody has written to, there should be no copy on
     * write (PAGE_WRITECOPY) to complicate matters.  Only chink is that
     * this doesn't work for sections with alignments larger than page
     * that leave reserved memory holes.
     *
     * Note we get charged page file usage as soon as we make a page
     * privately writable, even if we do not write to it to bring in
     * a private copy.  FIXME: case 8683 Which means this call can in
     * fact fail in out of commit memory ~= pagefile situations.
     */

    /* we could copy the section walking code from
     * add_rct_module() so that we make each section writable.  or
     * even most efficient in terms of peak page file use we could
     * mark private only one page at a time
     */
    return make_writable(module_base, module_size);

    /* note we don't care about restoring section protection bits, so
     * we don't save them here.  We can restore them based on section
     * headers attributes if we need to in
     * module_restore_permissions().
     */
}

bool
module_restore_permissions(app_pc module_base, size_t module_size)
{
    /* FIXME: necessary for a real loader */
    /* FIXME: need to walk the sections and restore their requested
     * permissions
     */

    /* optional: if we are to execute from this mapping, we may want
     * to do a NtFlushInstructionCache(0,0) just as the loader keeps
     * doing on x86
     */

    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

/* verifies section characteristics before we have started relocating the file */
/* returns true if none of the sections have properties precluding
 * correct use of our mirror file.  Currently only .shared sections
 * are presumed to be such a problematic example. */
/* module_base should be of valid module */
bool
module_file_relocatable(app_pc module_base)
{
    /* .shared sections - do not allow us to produce a different copy
     * unless we can guarantee that the original DLLs aren't used by
     * any process.  Otherwise original DLL users may see a different
     * value in the shared section than the fresh mapping we'll get
     * from the other sections.  Note that such sections are
     * non-securable therefore should in general be avoided if a high
     * privilege process is to use such a DLL.
     */

    /* note we handle the rare non-readable sections so we don't need
     * to give up on such
     */
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt_hdr;
    IMAGE_SECTION_HEADER *sec;
    bool relocatable = true;
    int i;

    ASSERT(is_readable_pe_base(module_base));
    dos = (IMAGE_DOS_HEADER *)module_base;
    nt_hdr = (IMAGE_NT_HEADERS *)(((ptr_uint_t)dos) + dos->e_lfanew);

    sec = IMAGE_FIRST_SECTION(nt_hdr);
    for (i = 0; i < nt_hdr->FileHeader.NumberOfSections; i++, sec++) {
        if (TEST(IMAGE_SCN_MEM_SHARED, sec->Characteristics)) {
            relocatable = false;
        } else {
            /* FIXME: probably best is to list all section flags that we
             * understand and assume that with all others we are looking
             * for trouble
             */
            ASSERT_CURIOSITY(
                !TESTANY(
                    ~(IMAGE_SCN_CNT_CODE | IMAGE_SCN_CNT_INITIALIZED_DATA |
                      IMAGE_SCN_CNT_UNINITIALIZED_DATA | IMAGE_SCN_LNK_OTHER |
                      IMAGE_SCN_LNK_INFO | IMAGE_SCN_LNK_REMOVE | IMAGE_SCN_ALIGN_MASK |
                      IMAGE_SCN_LNK_NRELOC_OVFL | IMAGE_SCN_MEM_DISCARDABLE |
                      IMAGE_SCN_MEM_NOT_CACHED | IMAGE_SCN_MEM_NOT_PAGED |
                      /* IMAGE_SCN_MEM_SHARED known bad */
                      IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE),
                    sec->Characteristics) &&
                "not seen section characteristic");
        }
    }
    return relocatable;
}

/* returns true if successful.
 * if !protect_incrementally, note the module mapping is left
 * writable on success, and it is up to callers to call
 * module_restore_permissions() to make unwritable.
 * FIXME: may change this interface to require users to guarantee writability.
 * especially if module_restore_permissions() can get away as stateless
 */
bool
module_rebase(app_pc module_base, size_t module_size,
              ssize_t relocation_delta /* value will be added to each relocation */,
              bool protect_incrementally)
{
    IMAGE_BASE_RELOCATION *base_reloc = NULL;
    size_t base_reloc_size = 0;
    bool ok;

    ASSERT(module_base != NULL);
    ASSERT(module_size != 0);
    ASSERT_CURIOSITY(relocation_delta != 0);
    ASSERT(ALIGNED(relocation_delta, PAGE_SIZE));
    if (!is_readable_pe_base(module_base)) {
        return false;
    }

    if (!protect_incrementally) {
        /* unprotect all sections - even if there are no relocations to apply */
        ok = module_make_writable(module_base, module_size);
        ASSERT_CURIOSITY(ok && "out of commit space?");
        if (!ok) {
            ASSERT_NOT_TESTED();
            return false;
        }
    }

    base_reloc = get_module_base_reloc(module_base, &base_reloc_size);
    LOG(GLOBAL, LOG_RCT, 2,
        "reloc: add_rct_module: module_base=" PFX ", "
        "base_reloc=" PFX ", base_reloc_size=" PFX ")\n",
        module_base, base_reloc, base_reloc_size);

    /* unless a module is IMAGE_FILE_RELOCS_STRIPPED, even when there
     * are no relocations apparently there usually is a relocation
     * directory, other than DLLs with .rsrc only like xpsp2res.dll.
     *
     * For example:
     * $ dumpbin /headers ms05-002-ani-41006.lss
     *             3000 [       8] RVA [size] of Base Relocation Directory
     * $ dumpbin /relocations ms05-002-ani-41006.lss
     *     BASE RELOCATIONS #3
     *     0 RVA,        8 SizeOfBlock
     *     Summary...
     */

    if (base_reloc != NULL && base_reloc_size > 0) {
        ok = module_apply_relocations(module_base, module_size, base_reloc,
                                      base_reloc_size, relocation_delta,
                                      protect_incrementally);
        ASSERT(ok);
        if (!protect_incrementally) {
            /* note we don't care about requested permissions here, but if
             * part of a real loader we'd need to restore the original
             * section permissions.  Or alternatively protect and
             * unprotect page by page.
             */
        }
        if (!ok)
            return false;
    } else {
        if (TESTALL(IMAGE_FILE_DLL | IMAGE_FILE_RELOCS_STRIPPED,
                    get_module_characteristics(module_base))) {
            /* /FIXED dll: can't rebase! */
            return false;
        }

        /* for example xpsp2res.dll */
        /* no relocations needed - even better for us */

        /* FIXME: we may want skip this DLL from sharing.  Since it
         * has no relocations no memory is wasted when it is
         * 'privately' ASLRed, while we have to pay a heavy cost to
         * validate.  The only reason to use sharing would be for
         * interoperability benefits for having it at the same address.
         */
    }

    return true;
}

/* dump PE components that are loaded in memory to a file */
/* Note that only sections that are loaded in memory will be
 * dumped to the new file.
 * e.g. "Certificate Table entry points to a table of attribute
 * certificates. These certificates are not loaded into memory as
 * part of the image" pecoff v6.
 * We expect all sections to be marked readable.
 */

/* Note that if new_file exists we would
 * overwrite only the appropriate portions we have in memory, so
 * in fact we could first copy the whole file and then call this
 * routine to overwrite our changes.
 */
bool
module_dump_pe_file(HANDLE new_file, app_pc module_base, size_t module_size)
{
    IMAGE_NT_HEADERS *nt;

    uint64 file_position;
    DEBUG_DECLARE(uint64 last_written_position;)
    size_t num_written;
    bool ok;

    IMAGE_SECTION_HEADER *sec;
    uint i;

    ASSERT(module_base != NULL);
    ASSERT(module_size != 0);
    ASSERT(new_file != INVALID_HANDLE_VALUE);

    if (!is_readable_pe_base(module_base)) {
        ASSERT_NOT_REACHED();
        return false;
    }
    nt = NT_HEADER(module_base);

    /* SizeOfHeaders The combined size of an MS DOS stub, PE header,
     * and section headers rounded up to a multiple of
     * FileAlignment.
     */
    file_position = 0;
    ok = write_file(new_file, module_base + 0, nt->OptionalHeader.SizeOfHeaders,
                    &file_position, &num_written);
    if (!ok || num_written != nt->OptionalHeader.SizeOfHeaders) {
        ASSERT_NOT_TESTED();
        /* we don't delete the file here assuming we'll retry to produce */
        return false;
    }
    DODEBUG({ last_written_position = file_position + num_written; });

    sec = IMAGE_FIRST_SECTION(nt);
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        LOG(GLOBAL, LOG_VMAREAS, 4, "\tName = %.*s\n", IMAGE_SIZEOF_SHORT_NAME,
            sec->Name);
        LOG(GLOBAL, LOG_VMAREAS, 2, "\tVirtualAddress = 0x%08x\n", sec->VirtualAddress);
        LOG(GLOBAL, LOG_VMAREAS, 2, "\tPointerToRawData  = 0x%08x\n",
            sec->PointerToRawData);
        LOG(GLOBAL, LOG_VMAREAS, 2, "\tSizeOfRawData  = 0x%08x\n", sec->SizeOfRawData);

        /* comres.dll for an example of an empty physical section
         *   .data name
         *      0 size of raw data
         *      0 file pointer to raw data
         */
        if (get_image_section_map_size(sec, nt) == 0) {
            LOG(GLOBAL, LOG_VMAREAS, 1, "skipping empty physical section %.*s\n",
                IMAGE_SIZEOF_SHORT_NAME, sec->Name);
            /* note that such sections will still get 0-filled
             * according to their VirtualAddress VirtualSize, as
             * normally happens whenever SizeOfRawData < VirtualSize (case 5355) */
            continue;
        }

        /* we simply emulate what was already calculated in the original file */
        file_position = get_image_section_file_offs(sec, nt);
        /* ASSERT the PE specification prescribes that sections are linearly
         * consecutive, but apparently the header doesn't have to be adjacent.
         * We don't care anyways since we seek to the correct PointerToRawData. Only
         * a curioisty since, while not strictly legal, non-adjacent or even overlapping
         * raw data is allowed, though not yet seen once alignment (xref case 8868)
         * is taken into account (xref case 5355). */
        ASSERT_CURIOSITY(last_written_position == file_position ||
                         i == 0 /* allowing header to be zero padded */);
        DODEBUG({
            /* seen in an HP printer driver HPBAFD32.DLL-a19ef539
             * produced by 2.25 linker version
             * file header ends at RVA 400,
             * but first section starts at RVA 600, with 0s inbetween,
             */
            if (last_written_position != file_position) {
                SYSLOG_INTERNAL_WARNING_ONCE("header or section padded\n");
            }
        });
        /* Yes, strangely, NtWriteFile takes ULONG instead of size_t */
        IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(get_image_section_map_size(sec, nt))));
        ok = write_file(new_file, module_base + sec->VirtualAddress,
                        (uint)get_image_section_map_size(sec, nt), &file_position,
                        &num_written);
        if (!ok || num_written != get_image_section_map_size(sec, nt)) {
            /* we don't delete the file here assuming we'll retry to produce */
            /* Note that with aslr_safe_save the temporary file will in fact get
             * orphaned!  We have to count on nodemgr cleaning up.
             */
            return false;
        }
        DODEBUG({ last_written_position = file_position + num_written; });
    }
    return true;
}

/* FIXME: share the version in module_list.c */
/* Verifies that according to section Characteristics its mapping is expected to be
 * readable (and if not VirtualProtects to makes it so).  NOTE this only operates on
 * the mapped portion of the section (via get_image_section_map_size()) which may be
 * smaller then the virtual size (get_image_section_size()) of the section (in which
 * case it was zero padded).
 *
 * Note this is NOT checking the current protection settings with
 * is_readable_without_exception(), so the actual current state may well vary.
 *
 * returns false if an unreadable section has been made readable
 */
static bool
ensure_section_readable(app_pc module_base, IMAGE_SECTION_HEADER *sec,
                        IMAGE_NT_HEADERS *nt, OUT uint *old_prot, app_pc view_start,
                        size_t view_len)
{
    int ok;
    app_pc intersection_start;
    size_t intersection_len;
    VERIFY_NT_HEADER(module_base);

    region_intersection(&intersection_start, &intersection_len, view_start, view_len,
                        module_base + sec->VirtualAddress,
                        ALIGN_FORWARD(get_image_section_map_size(sec, nt), PAGE_SIZE));
    if (intersection_len == 0)
        return true;

    /* on X86-32 as long as any of RWX is set the contents is readable */
    if (TESTANY(IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE,
                sec->Characteristics)) {
        ASSERT(is_readable_without_exception(intersection_start, intersection_len));
        return true;
    }
    /* such a mapping could potentially be used for some protection
     * scheme in which sections are made readable only on demand */

    /* Otherwise we just mark readable the raw bytes of the section,
     * NOTE: we'll leave readable, so only users of our private
     * mappings should use this function!
     */
    SYSLOG_INTERNAL_WARNING("unreadable section %.*s\n", IMAGE_SIZEOF_SHORT_NAME,
                            sec->Name);
    ok = protect_virtual_memory(intersection_start, intersection_len, PAGE_READONLY,
                                old_prot);
    ASSERT(ok);
    ASSERT_CURIOSITY(*old_prot == PAGE_NOACCESS ||
                     *old_prot == PAGE_WRITECOPY); /* expect unmodifed even if writable */
    return false;
}

/* FIXME: share the version in module_list.c */
static bool
restore_unreadable_section(app_pc module_base, IMAGE_SECTION_HEADER *sec,
                           IMAGE_NT_HEADERS *nt, uint restore_prot, app_pc view_start,
                           size_t view_len)
{
    bool ok;
    app_pc intersection_start;
    size_t intersection_len;
    uint old_prot;
    VERIFY_NT_HEADER(module_base);

    ASSERT(!TESTANY(IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE,
                    sec->Characteristics));

    region_intersection(&intersection_start, &intersection_len, view_start, view_len,
                        module_base + sec->VirtualAddress,
                        ALIGN_FORWARD(get_image_section_map_size(sec, nt), PAGE_SIZE));
    if (intersection_len == 0)
        return true;

    ok = protect_virtual_memory(intersection_start, intersection_len, restore_prot,
                                &old_prot);
    ASSERT(ok);
    ASSERT(old_prot == PAGE_READONLY);

    return ok;
}

/* We don't want the compiler to use a byte comparison (rep cmpsb)
 * when the library version of memcmp can do a faster word comparison
 * as much as possible (rep cmpsd).  We'll end up using the
 * ntdll!memcmp, which is the same as msvcrt!memcmp,
 * ntdll!RtlCompareMemory.  case 9277 tracks exploring faster variants
 * Note the opposite of this is #pragma intrinsic(memcmp) which would
 * always force memcmp intrinsic.
 */
/* default release build behaviour of VC98 in the following uses
 * matches #pragma intrinsic(memcmp) */
/* FIXME: should experiment whether reps cmpsb or reps cmpsd is
 * faster if we are comparing shorter intervals between relocations
 */
#pragma function(memcmp) /* do not use memcmp intrinsic */

/* verbatim comparison of a region, simple wrapper around memcmp()
 * but with somewhat more convenient range expression
 */
static bool
module_region_compare(app_pc original_module_start,
                      app_pc original_module_end, /* not inclusive */
                      ssize_t suspect_module_mapped_delta
                      /* N.B. difference in mapped memory addresses
                       * _not_ preferred addresses */
)
{
    ASSERT(original_module_start <= original_module_end);
    /* empty region always matches */
    ASSERT(suspect_module_mapped_delta != 0);
    return memcmp(original_module_start,
                  original_module_start + suspect_module_mapped_delta,
                  original_module_end - original_module_start) == 0;
}

/* compare consecutive readable bytes mapped as a PE section */
static inline bool
module_pe_section_compare(app_pc original_module_section, app_pc suspect_module_section,
                          size_t matching_section_size, bool relocated,
                          /* preferred base delta of suspect */
                          ssize_t relocation_delta, /* used only if !relocated */
                          reloc_iterator_t *ri /* used only if !relocated */)
{
    LOG(GLOBAL, LOG_VMAREAS, 2, "module_pe_section_compare = %d bytes\n",
        matching_section_size);

    if (relocated) {
        /* as long as we check each section description from original
         * matching each section from suspect - base size and
         * characteristics we don't need to do explicit
         * is_readable_without_exception() or a real TRY block
         */
        return memcmp(original_module_section, suspect_module_section,
                      matching_section_size) == 0;
    } else {
        /* if matching_section_size == 0 we'd always return true here.
         * the caller we would in fact be only comparing the PE header.
         */

        /* note that sections don't have to be page aligned! */
        /* Note we'll assume sections ARE sorted */
        /* otherwise we'll have to be able to search for relocation entry
         * starting from the beginning
         */

        /* find RVA of next relocation entry
         * compare from last entry to the new one (or the end of the region)
         * match the relocated RVA contents if within the section
         */
        app_pc verbatim_start = original_module_section;
        ssize_t mapped_delta = suspect_module_section - original_module_section;

        do {
            /* note that iterator doesn't really consume an item until
             * a larger one is asked for */
            app_pc next_reloc_original = module_reloc_iterator_next(ri, verbatim_start);
            app_pc next_reloc_suspect;

            if (next_reloc_original > original_module_section + matching_section_size ||
                next_reloc_original == NULL /* beyond last relocation */) {
                /* set limit around whole region */
                next_reloc_original = original_module_section + matching_section_size;
                /* there is no real relocation entry to compare */
                next_reloc_suspect = NULL;
            } else {
                next_reloc_suspect = next_reloc_original + mapped_delta;
            }

            if (!module_region_compare(verbatim_start,
                                       next_reloc_original, /* not inclusive */
                                       mapped_delta)) {
                SYSLOG_INTERNAL_WARNING("mismatch in verbatim region [" PFX "-" PFX ")",
                                        verbatim_start, next_reloc_original);
                return false;
            }

            if (next_reloc_suspect != NULL &&
                /* full pointer value at next_reloc_original is relocated */
                (*(app_pc *)(next_reloc_original) + relocation_delta !=
                 *(app_pc *)(next_reloc_suspect))) {
                SYSLOG_INTERNAL_WARNING("mismatch at relocated entry " PFX " = " PFX,
                                        next_reloc_original,
                                        *(app_pc *)next_reloc_original);
                return false;
            }
            verbatim_start = next_reloc_original + sizeof(uint);
            /* FIXME: note that a very sneaky .reloc section may have
             * overlapping relocation entries.  Other than malicious
             * DLLs trying to thwart us, (we won't be able to match
             * that), there is no reason to expect anyone else would
             * have such a DLL.  Even if increment by 1 since we
             * wouldn't have applied the previous relocation, we'd
             * still fail.  We'll simply ASSERT if we skip any
             * entries.
             */

        } while (verbatim_start < original_module_section + matching_section_size);

        return true;
    }
}

/* compare PE header, verbatim with the exception of the fields which
 * may be modified if produced by aslr_generate_relocated_section(),
 * FileHeader.TimeDateStamp, OptionalHeader.ImageBase, and
 * OptionalHeader.CheckSum
 */
bool
aslr_compare_header(app_pc original_module_base, size_t original_header_len,
                    app_pc suspect_module_base)
{
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt_hdr;

    uint old_checksum;
    uint old_timestamp;

    uint new_checksum;
    uint new_timestamp;

    bool ok;

    /* FIXME: [perf] note that get_module_info_pe()'s and our similar
     * calls to is_readable_pe_base() are heavy weight. should use TRY
     * here */

    ASSERT(is_readable_pe_base(original_module_base));
    if (!is_readable_pe_base(original_module_base)) {
        return false;
    }

    dos = (IMAGE_DOS_HEADER *)original_module_base;
    nt_hdr = (IMAGE_NT_HEADERS *)(((ptr_uint_t)dos) + dos->e_lfanew);

    old_timestamp = nt_hdr->FileHeader.TimeDateStamp;
    old_checksum = nt_hdr->OptionalHeader.CheckSum;

    ok = get_module_info_pe(suspect_module_base, &new_checksum, &new_timestamp, NULL,
                            NULL, NULL);
    ASSERT(ok);
    if (!ok) {
        return false;
    }

    LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 2,
        "ASLR: aslr_compare_header checksum old " PFX ", new " PFX "\n", old_checksum,
        new_checksum);
    LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 2,
        "ASLR: aslr_compare_header TimeDateStamp old " PFX ", new " PFX "\n",
        old_timestamp, new_timestamp);

    /* aslr_generate_relocated_section() adjusts timestamp */
    if (new_timestamp != aslr_timestamp_transformation(old_timestamp)) {
        /* this in most cases will be due to benign update of source, */
        /* but could also indicate (malicious) modification of target */
        return false;
    }

    /* Note CheckSum is currently left identical to original, but in
     * the future we may have to set it to the correct checksum for
     * the current DLL
     */
    if (new_checksum != old_checksum) {
        ASSERT_CURIOSITY(false && "checksum tampering!");
        return false;
    }

    /* instead of aslr_write_header() we're supposed to compare here in place */
    /* we need to verify the order of these three variables */
    /* and then do memcmp() of the four regions that it splits the address space into */
    /* FileHeader.TimeDateStamp < OptionalHeader.ImageBase < OptionalHeader.CheckSum */
    ASSERT((byte *)&nt_hdr->FileHeader.TimeDateStamp <
           (byte *)OPT_HDR_P(nt_hdr, ImageBase));
    ASSERT((byte *)OPT_HDR_P(nt_hdr, ImageBase) <
           (byte *)&nt_hdr->OptionalHeader.CheckSum);

    if (original_header_len <
        (ptr_uint_t)(((app_pc)&nt_hdr->OptionalHeader.CheckSum) - original_module_base)) {
        ASSERT_NOT_TESTED();
        ASSERT_CURIOSITY(false && "bad DOS header?");
        return false;
    }

    ok = ok &&
        module_region_compare(original_module_base,
                              (app_pc)&nt_hdr->FileHeader.TimeDateStamp,
                              suspect_module_base - original_module_base);
    ASSERT_CURIOSITY(ok && "header tampered with");

    ok = ok &&
        module_region_compare(((app_pc)&nt_hdr->FileHeader.TimeDateStamp) +
                                  sizeof(nt_hdr->FileHeader.TimeDateStamp),
                              (app_pc)OPT_HDR_P(nt_hdr, ImageBase),
                              suspect_module_base - original_module_base);
    ASSERT_CURIOSITY(ok && "header tampered with");

    ok = ok &&
        module_region_compare(((app_pc)OPT_HDR_P(nt_hdr, ImageBase)) +
                                  sizeof(OPT_HDR(nt_hdr, ImageBase)),
                              (app_pc)&nt_hdr->OptionalHeader.CheckSum,
                              suspect_module_base - original_module_base);
    ASSERT_CURIOSITY(ok && "header tampered with");

    ok = ok &&
        module_region_compare(((app_pc)&nt_hdr->OptionalHeader.CheckSum) +
                                  sizeof(nt_hdr->OptionalHeader.CheckSum),
                              original_module_base + original_header_len,
                              suspect_module_base - original_module_base);
    ASSERT_CURIOSITY(ok && "header tampered with");
    return ok;
}

/* Compares the PE .sections of a Mapping of a Section of a file_t
 * note that original_module_base is presumed to be more trustworthy,
 * though best to be very careful with both
 *
 * if !relocated transparently applies relocations
 * without mappings breaking COW with private copies
 *
 * validation_section_prefix controls maximum per-section comparison
 * note that PE header is always compared in full.
 */
bool
module_contents_compare(app_pc original_module_base, app_pc suspect_module_base,
                        size_t matching_module_size, bool relocated,
                        ssize_t relocation_delta
                        /* preferred base delta of suspect */,
                        size_t validation_section_prefix)
{
    /* FIXME: probably a good time to provide an iterator
     * that optionally returns the PE header as well
     * and of course avoids any holes
     * just like module_calculate_digest() and module_dump_pe_file()
     */

    IMAGE_NT_HEADERS *nt_original;
    IMAGE_NT_HEADERS *nt_suspect;
    IMAGE_SECTION_HEADER *sec_original;
    IMAGE_SECTION_HEADER *sec_suspect;
    uint i;

    size_t region_offset;
    size_t region_len;
    size_t suspect_len;

    uint original_section_prot = 0;
    uint suspect_section_prot = 0;

    reloc_iterator_t reloc_iter;
    reloc_iterator_t *ri = &reloc_iter;

    ASSERT(original_module_base != NULL);
    ASSERT(suspect_module_base != NULL);
    ASSERT(matching_module_size != 0);

    if (!is_readable_pe_base(original_module_base)) {
        ASSERT_NOT_REACHED();
        return false;
    }
    /* all section headers should be readable now */
    nt_original = NT_HEADER(suspect_module_base);

    if (!is_readable_pe_base(suspect_module_base)) {
        ASSERT_CURIOSITY(false && "bad suspect PE header!");
        return false;
    }
    /* all suspect section headers should be readable */
    nt_suspect = NT_HEADER(suspect_module_base);

    /* first region to consider is module header */
    region_offset = 0;
    region_len = nt_original->OptionalHeader.SizeOfHeaders;
    suspect_len = nt_suspect->OptionalHeader.SizeOfHeaders;

    if (region_len != suspect_len) {
        ASSERT_CURIOSITY(false && "different header size!");
        return false;
    }

    if (!relocated) {
        /* header comparison has to match our header modifications in
         * aslr_generate_relocated_section() */
        if (!aslr_compare_header(original_module_base, region_len, suspect_module_base)) {
            /* commonly just a new version */
            ASSERT_CURIOSITY(false && "mismatched PE header, new version?");
            return false;
        }
    } else if (!module_pe_section_compare(original_module_base + region_offset,
                                          suspect_module_base + region_offset, region_len,
                                          relocated, relocation_delta,
                                          NULL /* no iterator */)) {
        ASSERT(relocated);
        /* commonly just a new version */
        ASSERT_CURIOSITY(false && "mismatched PE header, new version?");
        return false;
    }

    if (nt_original->FileHeader.NumberOfSections !=
        nt_suspect->FileHeader.NumberOfSections) {
        ASSERT_CURIOSITY(false && "not matching number of sections!");
        return false;
    }

    if (!relocated) {
        module_reloc_iterator_start(&reloc_iter, original_module_base,
                                    matching_module_size);
        ASSERT(ri == &reloc_iter);
    } else {
        ri = NULL;
    }

    sec_original = IMAGE_FIRST_SECTION(nt_original);
    sec_suspect = IMAGE_FIRST_SECTION(nt_suspect);
    for (i = 0; i < nt_original->FileHeader.NumberOfSections;
         i++, sec_original++, sec_suspect++) {
        bool readable;
        LOG(GLOBAL, LOG_VMAREAS, 4, "\tName = %.*s\n", IMAGE_SIZEOF_SHORT_NAME,
            sec_original->Name);
        LOG(GLOBAL, LOG_VMAREAS, 2, "\tVirtualAddress = 0x%08x\n",
            sec_original->VirtualAddress);
        LOG(GLOBAL, LOG_VMAREAS, 2, "\tPointerToRawData  = 0x%08x\n",
            sec_original->PointerToRawData);
        LOG(GLOBAL, LOG_VMAREAS, 2, "\tSizeOfRawData  = 0x%08x\n",
            sec_original->SizeOfRawData);

        if (get_image_section_map_size(sec_original, nt_original) == 0) {
            if (get_image_section_map_size(sec_suspect, nt_suspect) != 0) {
                ASSERT_CURIOSITY(false && "not matching empty section!");
                return false;
            }
            LOG(GLOBAL, LOG_VMAREAS, 1, "skipping empty physical section %.*s\n",
                IMAGE_SIZEOF_SHORT_NAME, sec_original->Name);
            /* note that such sections will still get 0-filled
             * but we only look at raw bytes */
            continue;
        }

        ASSERT(sec_original->VirtualAddress == sec_suspect->VirtualAddress);

        /* should be checked already in the header, but doublechecking */
        if ((sec_original->Characteristics != sec_suspect->Characteristics) ||
            (sec_original->VirtualAddress != sec_suspect->VirtualAddress)) {
            ASSERT(false && "mismatched PE section characteristics");
            /* DoS deflected, if section not readable or privilege
             * escalation if made shared to allow attacker to change
             * .data section!
             */
            return false;
        }

        region_offset = sec_original->VirtualAddress;
        region_len = get_image_section_map_size(sec_original, nt_original);
        suspect_len = get_image_section_map_size(sec_suspect, nt_suspect);

        readable = ensure_section_readable(
            original_module_base, sec_original, nt_original, &original_section_prot,
            /* FIXME case 9791: must pass view size! */
            original_module_base + region_offset, region_len);
        if (!readable) {
            bool also_unreadable = !ensure_section_readable(
                suspect_module_base, sec_suspect, nt_suspect, &suspect_section_prot,
                /* FIXME case 9791: must pass view size! */
                suspect_module_base + region_offset, suspect_len);
            ASSERT(also_unreadable);
        }

        DODEBUG({
            if (region_len > validation_section_prefix) {
                SYSLOG_INTERNAL_WARNING_ONCE("comparing section prefix %d"
                                             " instead of full %d\n",
                                             validation_section_prefix, region_len);
            }
        });

        if (region_len != suspect_len ||
            !module_pe_section_compare(original_module_base + region_offset,
                                       suspect_module_base + region_offset,
                                       MIN(region_len, validation_section_prefix),
                                       relocated, relocation_delta, ri)) {
            SYSLOG_INTERNAL_ERROR("mismatched PE section %.*s\n", IMAGE_SIZEOF_SHORT_NAME,
                                  sec_original->Name);
            /* we also want ldump */
            ASSERT_CURIOSITY(false && "mismatched PE section!");
            return false;
        }

        if (!readable) {
            /* both should not be */
            bool ok;
            ok = restore_unreadable_section(
                original_module_base, sec_original, nt_original, original_section_prot,
                /* FIXME case 9791: must pass view size! */
                original_module_base + region_offset, region_len);
            ok = restore_unreadable_section(
                suspect_module_base, sec_suspect, nt_suspect, suspect_section_prot,
                /* FIXME case 9791: must pass view size! */
                suspect_module_base + region_offset, suspect_len);
            ASSERT(ok);
        }
    }

    return true;
}

/***** Resource Directory routines ****
 *
 * See the various PE format guides for more details, types below are
 * defined in winnt.h with constants in winnt.h and winuser.h.  See also the
 * wine implementations in dlls/version/info.c etc.
 *
 * At the high level the resource directory is structured as a tree of
 * IMAGE_RESOURCE_DIRECTORY elements.  Each resource directory is followed
 * by an array of IMAGE_RESOURCE_DIRECTORY_ENTRY structs.  Each of which
 * either points to an IMAGE_RESOURCE_DIRECTORY subdir or an
 * IMAGE_RESOURCE_DATA_ENTRY leaf that points to the actual data.  The tree
 * is structured in layers with the top layer being the resource type, the
 * second layer being the resource id/name, the third layer being the
 * resource language id, and the fourth layer being the data entry leaf.
 */

/* FIXME - like many other module.c routines these are racy w/ respect to an
 * unmap. */

/* Checks if [read_start, read_start+read_size) is within
 * [valid_start, valid_size). NOTE evaluates args multiple times. */
#define CHECK_SAFE_READ(read_start, read_size, valid_start, valid_size) \
    ((ptr_uint_t)(read_start) + (ptr_uint_t)(read_size) >=              \
         (ptr_uint_t)(read_start) && /* overflow */                     \
     (ptr_uint_t)(valid_start) + (ptr_uint_t)(valid_size) >=            \
         (ptr_uint_t)(valid_start) && /* overflow */                    \
     (ptr_uint_t)(read_start) >= (ptr_uint_t)(valid_start) &&           \
     (ptr_uint_t)(read_start) + (ptr_uint_t)(read_size) <=              \
         (ptr_uint_t)(valid_start) + (ptr_uint_t)(valid_size))

/* Gets the address of the memory following the struct at ptr. Note this is not
 * a deref, only an address computation. */
#define GET_FOLLOWING_ADDRESS(ptr) (&(ptr)[1])

static inline IMAGE_RESOURCE_DIRECTORY_ENTRY *
get_resource_directory_entries(IMAGE_RESOURCE_DIRECTORY *dir)
{
    return (IMAGE_RESOURCE_DIRECTORY_ENTRY *)GET_FOLLOWING_ADDRESS(dir);
}

static IMAGE_RESOURCE_DIRECTORY *
get_module_resource_directory(app_pc mod_base, size_t *rsrc_size /* OUT */)
{
    IMAGE_NT_HEADERS *nt = NULL;
    IMAGE_DATA_DIRECTORY *resource_dir = NULL;

    /* callers should have already done this */
    ASSERT(is_readable_pe_base(mod_base));
    VERIFY_NT_HEADER(mod_base);

    nt = NT_HEADER(mod_base);

    resource_dir = (IMAGE_DATA_DIRECTORY *)(OPT_HDR(nt, DataDirectory) +
                                            IMAGE_DIRECTORY_ENTRY_RESOURCE);
    /* sanity check */
    if (!is_readable_without_exception((app_pc)resource_dir, sizeof(*resource_dir))) {
        ASSERT_CURIOSITY(false && ".rsrc section directory not readable");
        return NULL;
    }

    ASSERT_CURIOSITY((resource_dir->VirtualAddress == 0 && resource_dir->Size == 0) ||
                     (resource_dir->VirtualAddress != 0 && resource_dir->Size != 0));

    if (resource_dir->VirtualAddress != 0 && resource_dir->Size != 0) {
        IMAGE_RESOURCE_DIRECTORY *out_dir =
            (IMAGE_RESOURCE_DIRECTORY *)RVA_TO_VA(mod_base, resource_dir->VirtualAddress);
        /* xref case 8740 - we've seen at least once case where the size of the .rsrc
         * section was reported wrong in the pe header (was smaller 0x800 then the actual
         * section size 0x834).  Since at this level we only care about the size for
         * avoiding future ptr safety checks (by checking up front that the whole region
         * is readable), we'll push the size up to the next memory region boundary. */
        if (is_readable_without_exception((app_pc)out_dir, resource_dir->Size)) {
            if (rsrc_size != NULL) {
                byte *base;
                size_t size;
                uint prot;
                bool res = get_memory_info(((byte *)out_dir) + (resource_dir->Size - 1),
                                           &base, &size, &prot);
                ASSERT(res && base + size >= ((byte *)out_dir) + resource_dir->Size);
                *rsrc_size = (base + size) - (byte *)out_dir;
            }
            return out_dir;
        } else {
            ASSERT_CURIOSITY(false && "resource directory not readable" ||
                             /* for partial map, .rsrc dir was't mapped in */
                             EXEMPT_TEST("win32.partial_map.exe"));
        }
    } else {
        ASSERT_CURIOSITY(resource_dir->VirtualAddress == 0 && resource_dir->Size == 0);
    }
    return NULL;
}

static IMAGE_RESOURCE_DIRECTORY_ENTRY *
get_resource_directory_entry_by_id(IMAGE_RESOURCE_DIRECTORY *dir, uint id,
                                   byte *valid_start, size_t valid_size)
{
    int i, j;
    /* the resource directory entries immediately follow the directory */
    IMAGE_RESOURCE_DIRECTORY_ENTRY *entries = get_resource_directory_entries(dir);

    if (dir == NULL)
        return NULL;
    if (!CHECK_SAFE_READ(dir, sizeof(*dir), valid_start, valid_size)) {
        ASSERT_CURIOSITY(false && "unreadable resource directory");
        return NULL;
    }

    /* named entries are first and we're looking for a numbered entry */
    j = dir->NumberOfNamedEntries;

    /* FIXME entries are in order so we could binary search this, but we
     * only look for small entries (16 for VS_FILE_INFO & 1 for VS_VERSION_INFO) */
    for (i = 0; i < dir->NumberOfIdEntries; i++) {
        if (!CHECK_SAFE_READ(&entries[i + j], sizeof(entries[i + j]), valid_start,
                             valid_size)) {
            ASSERT_CURIOSITY(false && "rsrc dir parse error");
            return NULL;
        }
        if (entries[i + j].NameIsString) {
            /* only the first j entries should be indentified by name, the
             * rest should be indentified by id (!NameIsString) */
            ASSERT_CURIOSITY(false && "unexpected named entry");
            continue;
        }
        if (entries[i + j].Name == id) {
            return &entries[i + j];
        } else {
            LOG(GLOBAL, LOG_SYMBOLS, 3, "Skipping rsrc dir entry %d\n",
                entries[i + j].Name);
        }
    }
    return NULL;
}

/* returns pointer to the start of the VS_VERSIONINFO structure
 * also returns the size of the structure in version_size */
static void *
get_module_resource_version_data(app_pc mod_base, size_t *version_size)
{
    size_t rsrc_dir_size;
    IMAGE_RESOURCE_DIRECTORY *resource_base =
        get_module_resource_directory(mod_base, &rsrc_dir_size);
    IMAGE_RESOURCE_DIRECTORY_ENTRY *ver_entry = NULL;
    IMAGE_RESOURCE_DIRECTORY *subdir;
    IMAGE_RESOURCE_DATA_ENTRY *data;
    void *version_info;

    if (resource_base == NULL)
        return NULL;

    LOG(GLOBAL, LOG_SYMBOLS, 3, "Found rsrc section @" PFX "\n", resource_base);

    /* top level, look for VS_FILE_INFO type entry */
    /* actually we should be going down to a ushort but we'll stick w/ the uint
     * interface to get_resource_directory_entry_by_id()
     */
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint((ptr_uint_t)VS_FILE_INFO)));
    ver_entry =
        get_resource_directory_entry_by_id(resource_base, (uint)(ptr_uint_t)VS_FILE_INFO,
                                           (byte *)resource_base, rsrc_dir_size);
    if (ver_entry == NULL)
        return NULL;
    /* We found a Version type directory entry, recurse down to version leaf */
    LOG(GLOBAL, LOG_SYMBOLS, 3, "Found version rsrc entry\n");
    if (!ver_entry->DataIsDirectory) {
        ASSERT_CURIOSITY(false && "expected resource name/id subdirectory");
        return NULL;
    }

    /* Next level should be resource identifier/name, we are looking for
     * VS_VERSION_INFO (which is 1) */
    subdir = (IMAGE_RESOURCE_DIRECTORY *)RVA_TO_VA(resource_base,
                                                   ver_entry->OffsetToDirectory);
    ver_entry = get_resource_directory_entry_by_id(subdir, (uint)VS_VERSION_INFO,
                                                   (byte *)resource_base, rsrc_dir_size);
    if (ver_entry == NULL) {
        DOCHECK(1, {
            const char *short_name = get_dll_short_name(mod_base);
            /* xref case 9099 for detoured.dll exemption */
            if (short_name == NULL || strcmp(short_name, "detoured.dll") != 0)
                ASSERT_CURIOSITY(false && "expected VS_VERSION_INFO entry");
        });
        return NULL;
    }
    if (!ver_entry->DataIsDirectory) {
        ASSERT_CURIOSITY(false && "expected resource lang subdirectory");
        return false;
    }

    /* Next Level should be the language id. */
    /* There is usually only one entry (having version strings in multiple languages is
     * normally done via multiple string tables within the VS_VERSION_INFO which is more
     * space efficient), however xref case 8742 for an ex. dll that splits the languages
     * here instead. We check for English US first (would expect all MS dlls to include
     * this) and failing that we just take the first entry instead (the keys are always
     * in English, the fields we care about are usually also in english, and any entry
     * will work for version numbers). */
    subdir = (IMAGE_RESOURCE_DIRECTORY *)RVA_TO_VA(resource_base,
                                                   ver_entry->OffsetToDirectory);
    ver_entry = NULL;
    if (!CHECK_SAFE_READ(subdir, sizeof(*subdir) + sizeof(*ver_entry), resource_base,
                         rsrc_dir_size) ||
        subdir->NumberOfNamedEntries != 0 || subdir->NumberOfIdEntries < 1) {
        ASSERT_CURIOSITY(false && "rsrc dir parse error");
        return NULL;
    }
    if (subdir->NumberOfIdEntries > 1) {
        /* multiple entries, try for US English */
        ver_entry = get_resource_directory_entry_by_id(
            subdir, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), (byte *)resource_base,
            rsrc_dir_size);
        DODEBUG({
            if (ver_entry == NULL) {
                char name[MAX_MODNAME_INTERNAL];
                os_get_module_name_buf(mod_base, name, BUFFER_SIZE_ELEMENTS(name));
                SYSLOG_INTERNAL_WARNING("Module %s @" PFX " with multiple lang id dirs "
                                        "has no US english version info, 0x%04x.",
                                        name != NULL ? name : "<none>", mod_base,
                                        get_resource_directory_entries(subdir)->Name);
            }
        });
    }
    /* if only one entry or no us english entries, just take first */
    if (ver_entry == NULL) {
        ver_entry = get_resource_directory_entries(subdir);
    }
    if (ver_entry->DataIsDirectory) {
        ASSERT_CURIOSITY(false && "expected resource data entry");
        return NULL;
    }

    /* Now we are finally to the IMAGE_RESOURCE_DATA_ENTRY. */
    data = (IMAGE_RESOURCE_DATA_ENTRY *)RVA_TO_VA(resource_base,
                                                  ver_entry->OffsetToDirectory);
    if (!CHECK_SAFE_READ(data, sizeof(*data), resource_base, rsrc_dir_size)) {
        ASSERT_CURIOSITY(false && "rsrc dir parse error");
        return NULL;
    }
    ASSERT_CURIOSITY(data->OffsetToData != 0 && data->Size != 0);
    if (version_size != NULL)
        *version_size = data->Size;
    /* Yes this is relative to the module base, not the resource base like all
     * the above offsets */
    version_info = (void *)RVA_TO_VA(mod_base, data->OffsetToData);

    /* safety check */
    if (CHECK_SAFE_READ(version_info, data->Size, resource_base, rsrc_dir_size) ||
        /* xref case 10542, version info might be outside .rsrc section */
        is_readable_without_exception(version_info, data->Size)) {
        return version_info;
    } else {
        ASSERT_CURIOSITY(false && "rsrc version data not readable");
    }
    return NULL;
}

/* The resource version information structures aren't proper C (they contain
 * variable sized and alignment specific fields).  Pseudo descriptions can
 * be found on 'msdn -> library -> win32 & com development -> user interface ->
 * windows user interface -> resources -> version information' and are briefly
 * duplicated here.  See also the wine implementations in dlls/version/info.c
 * etc. for code that walks these, we add some extra complexity to ours to
 * make these reads of app memory more safe.
 *
 * VS_VERSIONINFO is the top level structure
 * {
 *   WORD wLength; // size in bytes of vs_versioninfo structure
 *   WORD wValueLength; // length of Value field below
 *   WORD wType; // type of data in Value (expect 0 -> binary)
 *   WCHAR szKey[]; // contains "VS_VERSION_INFO"
 *   WORD Padding1[]; // possible padding word to 32 bit align subsequent fields
 *   VS_FIXEDFILEINFO Value; // see winver.h
 *   WORD Padding2[]; // possible padding word to 32 bit align subsequent fields
 *   StringFileInfo/VarFileInfo info[]; // 0 or 1 StringFileInfo and 0 or 1
 *                                      // VarFileInfo in either order
 * }
 *
 * The StringFileInfo holds extra information in key/value string pairs (such
 * as "CompanyName" "Microsoft Corp" etc.) that is displayed by properties etc.
 * StringFileInfo {
 *   WORD wLength; // size in bytes of this struture
 *   WORD wValueLength; // 0
 *   WORD wType; // type of data (expect 1 -> string)
 *   WCHAR szKey; // contains "StringFileInfo"
 *   WORD Padding[]; // if necessary padding word to align 32 bit
 *   StringTable tables[]; // 1 or more
 * }
 * StringTable {
 *   WORD wLength; // size in bytes of this struture
 *   WORD wValueLength; // 0
 *   WORD wType; // type of data (expect 1 -> string)
 *   WCHAR szKey; // 8 digit hex string specifying lang id and code page
 *   WORD Padding[]; // if necessary padding word to align 32 bit
 *   String strings[]; // 1 or more
 * }
 * String {
 *   WORD wLength; // size in bytes of this struture
 *   WORD wValueLength; // size in bytes of value
 *   WORD wType; // type of data (expect 1 -> string)
 *   WCHAR szKey; // key (for ex. "CompanyName")
 *   WORD Padding[]; // if necessary padding word to align 32 bit
 *   WCHAR szValue; // value (for ex. "Microsoft Corporation")
 * }
 *
 * The VarFileInfo is used to specify which language/unicode code page pairs
 * this dll supports.
 * VarFileInfo {
 *   WORD wLength; // size in bytes of this struture
 *   WORD wValueLength; // 0
 *   WORD wType; // type of data (expect 1 -> string)
 *   WCHAR szKey; // contains "VarFileInfo"
 *   WORD Padding[]; // if necessary padding word to align 32 bit
 *   Var vars[]; // 1 or more
 * }
 * Var {
 *   WORD wLength; // size in bytes of this struture
 *   WORD wValueLength; // size in bytes of value member
 *   WORD wType; // type of data (expect 1 -> string)
 *   WCHAR szKey; // contains "Translation"
 *   WORD Padding[]; // if necessary padding word to align 32 bit
 *   DWORD Value[]; // array of 1 or more supported language code page pairs
 * }
 */
/* We define our own versions of the version info types (since they aren't
 * valid C) using ptrs to the variable sized parts. NOTE all ptrs are
 * into the module .rsrc directory so don't count on them persisting beyond
 * the lifetime of the module. */
typedef struct ver_rsrc_header {
    size_t length;       /* length of structure in bytes */
    size_t value_length; /* length of value in bytes */
    uint type;
    wchar_t *key;
    size_t key_length; /* in bytes & including NULL terminator */
} ver_rsrc_header_t;
typedef struct vs_version_info {
    VS_FIXEDFILEINFO *info;   /* see winver.h */
    void *string_or_var_info; /* can be NULL, note either var info or string
                               * info can be first, have to disambiguate based
                               * on the internal key strings and of course both
                               * are optional. */
} vs_version_info_t;
typedef struct string_file_info {
    size_t size;        /* size in bytes of the string table(s) */
    void *string_table; /* ptr to first string table */
} string_file_info_t;
typedef struct string_table {
    size_t size;   /* size in bytes of the string(s) */
    wchar_t *lang; /* language identifier */
    void *string;  /* ptr to first rsrc string */
} string_table_t;
typedef struct rsrc_string {
    size_t key_length;   /* in bytes & including NULL terminator */
    size_t value_length; /* in bytes & including NULL terminator */
    wchar_t *key;
    wchar_t *value;
} rsrc_string_t;
/* currently don't need var fields, or missing entries from above structs */

#define RSRC_TYPE_STRING 1
#define RSRC_TYPE_BINARY 0

/* all version info structs are aligned 32 bit */
#define RSRC_ALIGNMENT 4

/* Since String, StringTable, StringFileInfo and VS_VERSIONINFO all start out
 * with the same fields we use this common routine to read them into the above
 * struct.  valid_start and valid_size delineate the memory region it is safe to
 * access.  The header to read is at start and a ptr to the value field is
 * returned (or NULL if read off the valid region).  If key_ref and bool are not
 * NULL the key string is compared with key_ref and the result is returned in
 * match. */
static byte *
read_version_struct_header(byte *start, byte *valid_start, size_t valid_size,
                           ver_rsrc_header_t *head /* OUT */, wchar_t *key_ref,
                           bool *match /* OUT */)
{
    size_t space_needed =
        sizeof(head->length) + sizeof(head->value_length) + sizeof(head->type);
    size_t max_wchars_left;
    size_t key_length = 0;
    byte *cur = start;
    ushort *cur_u;

    ASSERT(head != NULL &&
           ((key_ref == NULL && match == NULL) || (key_ref != NULL && match != NULL)));
    if (key_ref != NULL) {
        key_length = sizeof(wchar_t) * (wcslen(key_ref) + 1);
        space_needed += key_length;
    }
    /* i#1853: on win10 we see final entries with just 2 zero fields and no
     * further space.  We return NULL for those.
     */
    if (!CHECK_SAFE_READ(cur, space_needed, valid_start, valid_size))
        return NULL;
    cur_u = (ushort *)cur;
    head->length = *cur_u++;
    head->value_length = *cur_u++;
    head->type = *cur_u++;
    cur = (byte *)cur_u;
    head->key = (wchar_t *)cur;

    if (key_ref != NULL) {
        if (wcscmp(key_ref, (wchar_t *)cur) == 0) {
            *match = true;
        } else {
            key_length = 0;
            *match = false;
        }
    }
    if (key_length == 0) {
        max_wchars_left = (valid_size - (cur - start)) / sizeof(wchar_t);
        if (wcsnlen((wchar_t *)cur, max_wchars_left) >= max_wchars_left) {
            return NULL;
        }
        key_length = sizeof(wchar_t) * (wcslen((wchar_t *)cur) + 1);
    }
    head->key_length = key_length;

    /* advance cur past key string and alignment padding */
    cur = (byte *)ALIGN_FORWARD(cur + key_length, RSRC_ALIGNMENT);
    if (head->type == RSRC_TYPE_STRING && head->value_length != 0) {
        /* If type == string then value_length is in wchars instead of bytes
         * for the Microsoft Compiler, but not the Borland Compiler. However,
         * xref case 10588, sometimes the value_length is just plain wrong.
         * We just set the length to be the rest of the struct, we'll let
         * the null termination tell us where the actual end is. */
        head->value_length = head->length - (cur - (byte *)start);
    }
    LOG(GLOBAL, LOG_SYMBOLS, 3,
        "Read rsrc version structure header @" PFX ":\n\t"
        "length=" PFX " value_length=" PFX " type=0x%x\n\tkey=\"%S\" value @" PFX "\n",
        start, head->length, head->value_length, head->type, head->key, cur);
    return cur;
}

/* version_info - ptr to VS_VERSIONINFO to read
 * version_info_size - size of said VS_VERSIONINFO
 * info - OUT gets populated with the VS_VERSIONINFO data
 * returns true if successfully read version info */
static bool
read_vs_version_info(void *version_info, size_t version_info_size,
                     vs_version_info_t *info /* OUT */)
{
    ver_rsrc_header_t head;
    byte *cur = (byte *)version_info;
    bool match;

    ASSERT(version_info != NULL && info != NULL);
    LOG(GLOBAL, LOG_SYMBOLS, 3, "Reading VS_VERSIONINFO @" PFX "\n", version_info);

    cur = read_version_struct_header(cur, (byte *)version_info, version_info_size, &head,
                                     L"VS_VERSION_INFO", &match);
    if (cur == NULL) {
        ASSERT_CURIOSITY(false && "read off end of rsrc version info");
        return false;
    }
    if (!match) {
        ASSERT_CURIOSITY(false && "invalid version info structure");
        return false;
    }
    ASSERT_CURIOSITY(head.type == RSRC_TYPE_BINARY);

    ASSERT_CURIOSITY(head.value_length == sizeof(VS_FIXEDFILEINFO));
    if (!CHECK_SAFE_READ(cur, sizeof(VS_FIXEDFILEINFO), version_info,
                         version_info_size)) {
        ASSERT_CURIOSITY(false && "read off end of rsrc version info");
        return false;
    }
    info->info = (VS_FIXEDFILEINFO *)cur;
    info->string_or_var_info =
        (void *)ALIGN_FORWARD(cur + head.value_length, RSRC_ALIGNMENT);
    if ((byte *)info->string_or_var_info >= (byte *)version_info + head.length) {
        /* has no string or var info */
        LOG(GLOBAL, LOG_SYMBOLS, 2,
            "Rsrc VS_VERSIONINFO @" PFX " has no String/VarFileInfo structs\n",
            version_info);
        info->string_or_var_info = NULL;
    }
    return true;
}

/* string_or_var_info - ptr to String/VarFileInfo to read
 * version_info - ptr to the VS_VERSIONINFO containing this StringFileInfo
 * version_info_size - size of said VS_VERSIONINFO
 * info - OUT gets populated with the StringFileInfo data if string_or_var_info
 *        is a StringFileInfo
 * returns the address of the following StringFileInfo or VarFileInfo struct */
static void *
read_string_or_var_info(void *string_or_var_info, void *version_info,
                        size_t version_info_size, string_file_info_t *info /* OUT */)
{
    ver_rsrc_header_t head;
    size_t bytes_left;
    byte *cur = (byte *)string_or_var_info;
    bool match;

    ASSERT(ALIGNED(string_or_var_info, RSRC_ALIGNMENT));
    ASSERT(string_or_var_info != NULL && info != NULL);
    memset(info, 0, sizeof(*info));
    LOG(GLOBAL, LOG_SYMBOLS, 3, "Reading String/VarFileInfo @" PFX "\n",
        string_or_var_info);

    /* we check for VarFileInfo below */
    cur = read_version_struct_header(cur, (byte *)version_info, version_info_size, &head,
                                     L"StringFileInfo", &match);
    if (cur == NULL) {
        /* i#1853: on Win10 we see final entries with just 2 zero fields */
        ASSERT_CURIOSITY((byte *)string_or_var_info >= (byte *)version_info &&
                         (byte *)string_or_var_info + sizeof(uint) <=
                             (byte *)version_info + version_info_size &&
                         /* we read 2 ushort fields at once */
                         *(uint *)string_or_var_info == 0 &&
                         "read off end of rsrc version");
        return NULL;
    }
    if (!match) {
        wchar_t *ref_var = L"VarFileInfo";
        if (wcscmp(ref_var, head.key) == 0) {
            /* is a VarFileInfo field */
            LOG(GLOBAL, LOG_SYMBOLS, 3, "Ignoring version rsrc VarFileInfo struct\n");
            ASSERT_CURIOSITY(head.value_length == 0); /* no value field */
        } else {
            /* Xref case 9276, resvc.dll strangely has zero padding after its string and
             * var infos.  It doesn't pose a problem for us so relax the assert here. */
            /* happens many times in AcroRd32.exe 8.0 */
            DODEBUG({
                if (!is_region_memset_to_char(
                        string_or_var_info,
                        version_info_size -
                            ((byte *)string_or_var_info - (byte *)version_info),
                        0)) {
                    SYSLOG_INTERNAL_WARNING_ONCE(".rsrc @" PFX ": expected var or string "
                                                 "info, or padding",
                                                 string_or_var_info);
                }
            });
            return NULL;
        }
    } else {
        ASSERT_CURIOSITY(head.value_length == 0); /* no value field */
    }

    if (head.length < (size_t)(cur - (byte *)string_or_var_info)) {
        ASSERT_CURIOSITY(false && "FileInfo length too short");
        return NULL;
    }

    bytes_left = head.length - (cur - (byte *)string_or_var_info);

    if (match) {
        if (!CHECK_SAFE_READ(cur, bytes_left, version_info, version_info_size)) {
            ASSERT_CURIOSITY(false && "string file info too large");
            return NULL;
        }
        info->size = bytes_left;
        info->string_table = cur;
    }
    /* else is VarFileInfo so nothing to fill in */

    /* advance to the next String or Var FileInfo */
    cur = (byte *)ALIGN_FORWARD(cur + bytes_left, RSRC_ALIGNMENT);
    if (cur >= (byte *)version_info + version_info_size)
        return NULL; /* last info */
    return cur;
}

/* string_table - ptr to StringTable to read
 * remaining_string_table_size - IN/OUT bytes left in the StringTable array
 * table - OUT gets populated with the StringTable data
 * Returns a ptr to the next string table in the array (or NULL if last one) */
static void *
read_string_table(void *string_table, size_t *remaining_table_size,
                  string_table_t *table /* OUT */)
{
    ver_rsrc_header_t head;
    byte *cur = (byte *)string_table;

    ASSERT(ALIGNED(string_table, RSRC_ALIGNMENT));
    ASSERT(string_table != NULL && remaining_table_size != NULL && table != NULL);
    memset(table, 0, sizeof(*table)); /* in case return early */
    LOG(GLOBAL, LOG_SYMBOLS, 3, "Reading StringTable @" PFX "\n", string_table);

    cur = read_version_struct_header(cur, (byte *)string_table, *remaining_table_size,
                                     &head, NULL, NULL);
    if (cur == NULL) {
        ASSERT_CURIOSITY(false && "read off end of string table array");
        return NULL;
    }
    ASSERT_CURIOSITY(head.value_length == 0); /* no value field */
    /* check expected length of lang string */
    ASSERT_CURIOSITY(head.key_length == (8 + 1 /* NULL */) * sizeof(wchar_t));

    if (head.length > *remaining_table_size) {
        ASSERT_CURIOSITY(false && "string table too large");
        return NULL;
    }
    if (head.length < (size_t)(cur - (byte *)string_table)) {
        ASSERT_CURIOSITY(false && "string table too small");
        return NULL;
    }

    /* checked for underflow above */
    table->size = head.length - (cur - (byte *)string_table);
    table->lang = head.key;
    table->string = cur;
    if (ALIGN_FORWARD(head.length, RSRC_ALIGNMENT) >= *remaining_table_size) {
        *remaining_table_size = 0;
        return NULL;
    } else {
        *remaining_table_size -= ALIGN_FORWARD(head.length, RSRC_ALIGNMENT);
        return (byte *)string_table + ALIGN_FORWARD(head.length, RSRC_ALIGNMENT);
    }
}

/* rsrc_string - ptr to the String to read
 * remaining_rsrc_string_size - IN/OUT bytes left in the String array
 * string - OUT gets populated with the String data
 * Returns a ptr to the next String in the array (or NULL if last one) */
static void *
read_rsrc_string(void *rsrc_string, size_t *remaining_rsrc_string_size,
                 rsrc_string_t *string /* OUT */)
{
    ver_rsrc_header_t head;
    byte *cur = (byte *)rsrc_string;

    ASSERT(ALIGNED(rsrc_string, RSRC_ALIGNMENT));
    ASSERT(rsrc_string != NULL && remaining_rsrc_string_size != NULL && string != NULL);
    memset(string, 0, sizeof(*string)); /* in case return early */
    LOG(GLOBAL, LOG_SYMBOLS, 3, "Reading Rsrc String @" PFX "\n", rsrc_string);

    cur = read_version_struct_header(cur, (byte *)rsrc_string,
                                     *remaining_rsrc_string_size, &head, NULL, NULL);
    if (cur == NULL) {
        ASSERT_CURIOSITY(false && "read off end of rsrc version string array");
        return NULL;
    }

    if (head.length < (size_t)(cur - (byte *)rsrc_string)) {
        ASSERT_CURIOSITY(false && "Rsrc string length too short");
        return NULL;
    }

    /* Expect the type to always be string, but xref case 8797 for an instance where it
     * isn't (in that case value was 0, so could still be read as a string, though
     * was sized in bytes instead of wchars so presumably ment to be binary). We'll just
     * ignore non string type rsrc Strings. */
    if (head.type == RSRC_TYPE_STRING && head.value_length > 0) {
        if (!CHECK_SAFE_READ(cur, head.value_length, rsrc_string,
                             *remaining_rsrc_string_size)) {
            ASSERT_CURIOSITY(false && "rsrc string value extends too far");
            return NULL;
        }
        if (wcsnlen((wchar_t *)cur, head.value_length / sizeof(wchar_t)) >=
            (head.value_length / sizeof(wchar_t))) {
            ASSERT_CURIOSITY(false && "rsrc value string isn't null terminated");
            return NULL;
        }
        /* Don't normally expect dead value space after then end of the string, but xref
         * case 8797 for an ex. where the string has a NULL in the middle of it so we
         * can't have an assert curiosity here. */
        string->value = (wchar_t *)cur;
        LOG(GLOBAL, LOG_SYMBOLS, 3, "\tvalue=\"%S\"\n", string->value);
    } else {
        /* make sure that if it isn't string type, it's not one of the fields we want */
        ASSERT_CURIOSITY(head.type == RSRC_TYPE_STRING ||
                         (wcscmp(L"CompanyName", head.key) != 0 &&
                          wcscmp(L"ProductName", head.key) != 0 &&
                          wcscmp(L"OriginalFilename", head.key) != 0));
        string->value = NULL;
    }

    string->value_length = head.value_length;
    string->key_length = (head.key_length + 1) * sizeof(wchar_t);
    string->key = head.key;

    if (ALIGN_FORWARD(head.length, RSRC_ALIGNMENT) >= *remaining_rsrc_string_size) {
        *remaining_rsrc_string_size = 0;
        return NULL;
    } else {
        *remaining_rsrc_string_size -= ALIGN_FORWARD(head.length, RSRC_ALIGNMENT);
        return (byte *)rsrc_string + ALIGN_FORWARD(head.length, RSRC_ALIGNMENT);
    }
}

/* NOTE the strings returned in info_out are pointing to the .rsrc version
 * directory and as such they're only valid while the module is loaded. */
static bool
get_module_resource_version_info(app_pc mod_base, version_info_t *info_out)
{
    size_t size, remaining_table, remaining_string;
    vs_version_info_t ver_info;
    string_file_info_t string_info;
    string_table_t string_table;
    rsrc_string_t string;
    void *version_rsrc, *cur_table, *cur_string;
    DEBUG_DECLARE(uint num_string_file_info = 0;)
    DEBUG_DECLARE(const char *mod_name = "";) /* only set at loglevel 2+ */

    ASSERT(info_out != NULL);
    memset(info_out, 0, sizeof(version_info_t));
    DOLOG(2, LOG_SYMBOLS, {
        /* We will have an infinite loop if we call get_module_short_name()
         * so we go w/ PE name, just for debugging */
        mod_name = get_dll_short_name(mod_base);
        if (mod_name == NULL)
            mod_name = "";
    });
    LOG(GLOBAL, LOG_SYMBOLS, 3,
        "Reading rsrc version information for module %s @" PFX "\n", mod_name, mod_base);

    version_rsrc = get_module_resource_version_data(mod_base, &size);
    if (version_rsrc == NULL) {
        LOG(GLOBAL, LOG_SYMBOLS, 2, "Module %s has no rsrc section\n", mod_name);
        return false;
    }

    if (!read_vs_version_info(version_rsrc, size, &ver_info)) {
        LOG(GLOBAL, LOG_SYMBOLS, 2, "Module %s has no version rsrc\n", mod_name);
        return false;
    }

    info_out->file_version.version_uint.ms = ver_info.info->dwFileVersionMS;
    info_out->file_version.version_uint.ls = ver_info.info->dwFileVersionLS;
    info_out->product_version.version_uint.ms = ver_info.info->dwProductVersionMS;
    info_out->product_version.version_uint.ls = ver_info.info->dwProductVersionLS;
    LOG(GLOBAL, LOG_SYMBOLS, 3,
        "Module %s file_version=%d.%d.%d.%d product_version=%d.%d.%d.%d\n"
        "\tflags_mask=0x%08x flags=0x%08x\n",
        mod_name, info_out->file_version.version_parts.p1,
        info_out->file_version.version_parts.p2, info_out->file_version.version_parts.p3,
        info_out->file_version.version_parts.p4,
        info_out->product_version.version_parts.p1,
        info_out->product_version.version_parts.p2,
        info_out->product_version.version_parts.p3,
        info_out->product_version.version_parts.p4, ver_info.info->dwFileFlagsMask,
        ver_info.info->dwFileFlags);
    LOG(GLOBAL, LOG_SYMBOLS, 3, "rsrc bounds: " PFX "-" PFX "\n", version_rsrc,
        (byte *)version_rsrc + size);

    while (ver_info.string_or_var_info != NULL) {
        /* PR 536337: xpsp3 dlls have a dword with 0 at the end */
        if ((byte *)ver_info.string_or_var_info + sizeof(DWORD) >=
            ((byte *)version_rsrc) + size) {
#ifdef INTERNAL
            DOCHECK(1, {
                DWORD val;
                ASSERT_CURIOSITY(
                    d_r_safe_read(ver_info.string_or_var_info, sizeof(val), &val) &&
                    val == 0 && "unknown data at end of .rsrc");
            });
#endif
            LOG(GLOBAL, LOG_SYMBOLS, 3, "skipping 0 dword at .rsrc end " PFX "\n",
                ver_info.string_or_var_info);
            break;
        }
        ver_info.string_or_var_info = read_string_or_var_info(
            ver_info.string_or_var_info, version_rsrc, size, &string_info);
        if (string_info.string_table == NULL)
            continue;

        /* should be only 0 or 1 string_file_info structs */
        ASSERT_CURIOSITY(num_string_file_info++ == 0);

        remaining_table = string_info.size;
        cur_table = string_info.string_table;
        while (cur_table != NULL && remaining_table > 0) {
            cur_table = read_string_table(cur_table, &remaining_table, &string_table);
            /* FIXME - there can be several tables (for different languages),
             * right now we just scan all of them and for the fields we care
             * about use the last value we find. Prob. better to give pref.
             * to english though (note the key strings always appear to be in
             * English, at least the standard ones, only the values vary). */
            remaining_string = string_table.size;
            cur_string = string_table.string;
            while (cur_string != NULL && remaining_string > 0) {
                cur_string = read_rsrc_string(cur_string, &remaining_string, &string);
                if (string.key != NULL) {
                    if (string.value == NULL)
                        string.value = L""; /* has key, but empty value */
                    if (wcscmp(L"CompanyName", string.key) == 0) {
                        info_out->company_name = string.value;
                    } else if (wcscmp(L"ProductName", string.key) == 0) {
                        info_out->product_name = string.value;
                    } else if (wcscmp(L"OriginalFilename", string.key) == 0) {
                        info_out->original_filename = string.value;
                    }
                    LOG(GLOBAL, LOG_SYMBOLS, 4,
                        "read .rsrc version string key=\"%S\" value=\"%S\"\n", string.key,
                        string.value);
                }
            }
        }
    }
    return true;
}

bool
get_module_company_name(app_pc mod_base, char *out_buf, size_t out_buf_size)
{
    version_info_t info;
    if (get_module_resource_version_info(mod_base, &info) && info.company_name != NULL) {
        snprintf(out_buf, out_buf_size, "%S", info.company_name);
        out_buf[out_buf_size - 1] = '\0';
        return true;
    }
    return false;
}

/* Using strdup rather than a passed-in buffer b/c of module_area_t needs:
 * we've already exported module_data_t that does not have an inlined buffer.
 * Caller is responsible for freeing the string heap space, of course!
 */
static const char *
get_module_original_filename(app_pc mod_base,
                             version_info_t *in_info /*OPTIONAL IN*/
                                 HEAPACCT(which_heap_t which))
{
    version_info_t my_info;
    version_info_t *info = NULL;
    if (in_info == NULL && get_module_resource_version_info(mod_base, &my_info))
        info = &my_info;
    else
        info = in_info;
    if (info != NULL && info->original_filename != NULL) {
        return (const char *)dr_wstrdup(info->original_filename HEAPACCT(which));
    }
    return NULL;
}

#ifdef DEBUG /* currently no release build user */
thread_id_t
get_loader_lock_owner()
{
    PEB *peb = get_own_peb();
    RTL_CRITICAL_SECTION *lock = (RTL_CRITICAL_SECTION *)peb->LoaderLock;
    return (thread_id_t)lock->OwningThread;
}
#endif

char *
get_shared_lib_name(app_pc map)
{
    return get_dll_short_name(map);
}

bool
os_module_has_dynamic_base(app_pc module_base)
{
    IMAGE_NT_HEADERS *nt;
    ASSERT(is_readable_pe_base(module_base));
    nt = NT_HEADER(module_base);
    return TEST(IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE,
                nt->OptionalHeader.DllCharacteristics);
}

bool
module_contains_addr(module_area_t *ma, app_pc pc)
{
    return (pc >= ma->start && pc < ma->end);
}

bool
module_get_tls_info(app_pc module_base, OUT void ***callbacks, OUT int **index,
                    OUT byte **data_start, OUT byte **data_end)
{
    IMAGE_NT_HEADERS *nt;
    IMAGE_DATA_DIRECTORY *data_dir;
    IMAGE_TLS_DIRECTORY *tls_dir = NULL;
    VERIFY_NT_HEADER(module_base);
    ASSERT(is_readable_pe_base(module_base));
    nt = NT_HEADER(module_base);
    data_dir = OPT_HDR(nt, DataDirectory) + IMAGE_DIRECTORY_ENTRY_TLS;
    if (data_dir->VirtualAddress == 0)
        return false;
    if (data_dir->Size < sizeof(*tls_dir)) {
        SYSLOG_INTERNAL_WARNING("Module " PFX " TLS dir has invalid size %d", module_base,
                                data_dir->Size);
        return false;
    }
    tls_dir = (IMAGE_TLS_DIRECTORY *)(module_base + data_dir->VirtualAddress);
    ASSERT(is_readable_without_exception((app_pc)tls_dir, sizeof(*tls_dir)));
    /* We don't need RVA_TO_VA: the addresses here are all virtual and are relocated. */
    if (callbacks != NULL)
        *callbacks = (void **)tls_dir->AddressOfCallBacks;
    if (index != NULL)
        *index = (int *)tls_dir->AddressOfIndex;
    if (data_start != NULL)
        *data_start = (byte *)tls_dir->StartAddressOfRawData;
    if (data_end != NULL)
        *data_end = (byte *)tls_dir->EndAddressOfRawData;
    /* Apparently SizeOfZeroFill is ignored by the Windows loader so we do as well.
     * There are no Characteristics for x86 or arm.
     */
    return true;
}

/* Returns true if the next module import was read and is valid.
 */
static bool
safe_read_cur_module(pe_module_import_iterator_t *iter)
{
    /* Modules with no imports, such as ntdll, hit this check and not the
     * OriginalFirstThunk sentinel check below.
     */
    if ((byte *)(iter->cur_module + 1) > iter->imports_end ||
        /* Look out for partial maps -- although we now exclude them from the
         * module list (i#1172), better safe than sorry.
         */
        (byte *)(iter->cur_module + 1) >= iter->mod_base + iter->mod_size)
        return false;
    if (!SAFE_READ_VAL(iter->safe_module, iter->cur_module)) {
        memset(&iter->safe_module, 0, sizeof(iter->safe_module));
        return false;
    }
    /* The last module import is zeroed. */
    if (iter->safe_module.OriginalFirstThunk == 0)
        return false;
    return true;
}

dr_module_import_iterator_t *
dr_module_import_iterator_start(module_handle_t handle)
{
    pe_module_import_iterator_t *iter;
    IMAGE_NT_HEADERS *nt;
    IMAGE_DATA_DIRECTORY *dir;
    app_pc base = (app_pc)handle;

    if (!is_readable_pe_base(base))
        return NULL;
    iter = global_heap_alloc(sizeof(*iter) HEAPACCT(ACCT_CLIENT));

    /* Should be safe after is_readable_pe_base(). */
    /* XXX: Share with privload_get_import_descriptor()? */
    nt = NT_HEADER(base);
    dir = OPT_HDR(nt, DataDirectory) + IMAGE_DIRECTORY_ENTRY_IMPORT;
    iter->mod_base = (byte *)base;
    iter->mod_size = OPT_HDR(nt, SizeOfImage);
    iter->cur_module = (IMAGE_IMPORT_DESCRIPTOR *)RVA_TO_VA(base, dir->VirtualAddress);
    iter->imports_end = (byte *)RVA_TO_VA(base, dir->VirtualAddress) + dir->Size;
    iter->hasnext = safe_read_cur_module(iter);

    iter->module_import.modname = NULL;
    iter->module_import.module_import_desc = NULL;
    return (dr_module_import_iterator_t *)iter;
}

bool
dr_module_import_iterator_hasnext(dr_module_import_iterator_t *dr_iter)
{
    pe_module_import_iterator_t *iter = (pe_module_import_iterator_t *)dr_iter;
    return (iter != NULL && iter->hasnext);
}

dr_module_import_t *
dr_module_import_iterator_next(dr_module_import_iterator_t *dr_iter)
{
    pe_module_import_iterator_t *iter = (pe_module_import_iterator_t *)dr_iter;
    DEBUG_DECLARE(dcontext_t *dcontext = get_thread_private_dcontext();)

    CLIENT_ASSERT(iter != NULL, "invalid parameter");
    CLIENT_ASSERT(iter->hasnext, "dr_module_import_iterator_next: !hasnext");
    iter->module_import.modname =
        (const char *)RVA_TO_VA(iter->mod_base, iter->safe_module.Name);
    iter->module_import.module_import_desc = (dr_module_import_desc_t *)iter->cur_module;
    LOG(THREAD, LOG_LOADER, 3, "%s: yielding module " PFX ", %s\n", __FUNCTION__,
        iter->cur_module, iter->module_import.modname);

    iter->cur_module++;
    iter->hasnext = safe_read_cur_module(iter);
    /* FIXME i#931: Iterate delay-load imports after normal imports. */

    return &iter->module_import;
}

void
dr_module_import_iterator_stop(dr_module_import_iterator_t *dr_iter)
{
    pe_module_import_iterator_t *iter = (pe_module_import_iterator_t *)dr_iter;
    if (iter == NULL)
        return;
    global_heap_free(iter, sizeof(*iter) HEAPACCT(ACCT_CLIENT));
}

/* Reads iter->cur_thunk and sets iter->next_symbol.  Returns false if there are
 * no more imports.
 */
static bool
pe_symbol_import_iterator_read_thunk(pe_symbol_import_iterator_t *iter)
{
    IMAGE_THUNK_DATA safe_thunk;
    if (!SAFE_READ_VAL(safe_thunk, iter->cur_thunk))
        return false;
    if (safe_thunk.u1.Function == 0)
        return false;
    iter->next_symbol.delay_load = false;
    iter->next_symbol.by_ordinal =
        CAST_TO_bool(TEST(IMAGE_ORDINAL_FLAG, safe_thunk.u1.Function));
    if (iter->next_symbol.by_ordinal) {
        iter->next_symbol.ordinal = safe_thunk.u1.AddressOfData & (~IMAGE_ORDINAL_FLAG);
        iter->next_symbol.name = NULL;
    } else {
        IMAGE_IMPORT_BY_NAME *by_name = (IMAGE_IMPORT_BY_NAME *)RVA_TO_VA(
            iter->mod_base, safe_thunk.u1.AddressOfData);
        /* Name is an array, so no safe_read. */
        iter->next_symbol.name = (const char *)by_name->Name;
        iter->next_symbol.ordinal = 0;
    }
    return true;
}

/* Initializes cur_thunk to refer to the OriginalFirstThunk of iter->cur_module.
 */
static bool
pe_symbol_import_iterator_first_thunk(pe_symbol_import_iterator_t *iter)
{
    DWORD original_first_thunk;
    if (!SAFE_READ_VAL(original_first_thunk, &iter->cur_module->OriginalFirstThunk)) {
        return false;
    }
    iter->cur_thunk = (IMAGE_THUNK_DATA *)RVA_TO_VA(iter->mod_base, original_first_thunk);
    if (!pe_symbol_import_iterator_read_thunk(iter))
        return false;
    return true;
}

/* If we're iterating all module imports, go to the next imported module.
 * Return false if we're iterating symbols from a specific module.
 */
static bool
pe_symbol_import_iterator_next_module(pe_symbol_import_iterator_t *iter)
{
    if (iter->mod_iter == NULL) {
        /* We're getting imports from a specific module, so we're done now. */
        return false;
    } else {
        dr_module_import_t *mod_import;
        if (!dr_module_import_iterator_hasnext(iter->mod_iter))
            return false;
        mod_import = dr_module_import_iterator_next(iter->mod_iter);
        iter->cur_module = (IMAGE_IMPORT_DESCRIPTOR *)mod_import->module_import_desc;
        iter->next_symbol.modname = mod_import->modname;
        if (!pe_symbol_import_iterator_first_thunk(iter))
            return false;
        return true;
    }
}

dr_symbol_import_iterator_t *
dr_symbol_import_iterator_start(module_handle_t handle,
                                dr_module_import_desc_t *from_module)
{
    pe_symbol_import_iterator_t *iter;

    iter = global_heap_alloc(sizeof(*iter) HEAPACCT(ACCT_CLIENT));
    memset(iter, 0, sizeof(*iter));
    iter->mod_base = (byte *)handle;
    iter->cur_thunk = NULL;

    if (from_module == NULL) {
        iter->mod_iter = dr_module_import_iterator_start(handle);
        if (iter->mod_iter == NULL)
            goto error;
        iter->hasnext = pe_symbol_import_iterator_next_module(iter);
    } else {
        DWORD modname_rva;
        iter->mod_iter = NULL;
        iter->cur_module = (IMAGE_IMPORT_DESCRIPTOR *)from_module;
        if (!SAFE_READ_VAL(modname_rva, &iter->cur_module->Name))
            goto error;
        iter->next_symbol.modname = (const char *)RVA_TO_VA(iter->mod_base, modname_rva);
        iter->hasnext = pe_symbol_import_iterator_first_thunk(iter);
    }

    return (dr_symbol_import_iterator_t *)iter;

error:
    global_heap_free(iter, sizeof(*iter) HEAPACCT(ACCT_CLIENT));
    return NULL;
}

bool
dr_symbol_import_iterator_hasnext(dr_symbol_import_iterator_t *dr_iter)
{
    pe_symbol_import_iterator_t *iter = (pe_symbol_import_iterator_t *)dr_iter;
    return (iter != NULL && iter->hasnext);
}

dr_symbol_import_t *
dr_symbol_import_iterator_next(dr_symbol_import_iterator_t *dr_iter)
{
    pe_symbol_import_iterator_t *iter = (pe_symbol_import_iterator_t *)dr_iter;
    dcontext_t *dcontext = get_thread_private_dcontext();
    bool partial = false;
    DWORD original_thunk_rva = 0;
    DWORD symbol_rva = 0;

    CLIENT_ASSERT(iter != NULL, "invalid parameter");
    CLIENT_ASSERT(iter->hasnext, "dr_symbol_import_iterator_next: !hasnext");
    /* Copy the data to return before we advance next_symbol. */
    memcpy(&iter->symbol_import, &iter->next_symbol, sizeof(iter->symbol_import));

    iter->cur_thunk++;
    iter->hasnext = pe_symbol_import_iterator_read_thunk(iter);
    if (!iter->hasnext)
        iter->hasnext = pe_symbol_import_iterator_next_module(iter);
    /* FIXME i#931: Iterate delay-load imports after normal imports. */

    return &iter->symbol_import;
}

void
dr_symbol_import_iterator_stop(dr_symbol_import_iterator_t *dr_iter)
{
    pe_symbol_import_iterator_t *iter = (pe_symbol_import_iterator_t *)dr_iter;
    if (iter == NULL)
        return;
    if (iter->mod_iter != NULL)
        dr_module_import_iterator_stop(iter->mod_iter);
    global_heap_free(iter, sizeof(*iter) HEAPACCT(ACCT_CLIENT));
}
