/* **********************************************************
 * Copyright (c) 2011-2021 Google, Inc.  All rights reserved.
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

#ifndef MODULE_SHARED_H
#define MODULE_SHARED_H

#include "heap.h" /* for HEAPACCT */
#include "module_api.h"
#include "module.h" /* include os_specific header */

#ifdef WINDOWS
/* Introduced as part of case 9842; see module_names_t above.  Shouldn't
 * put case number in exported parts (CLIENT api), so adding it here.
 *
 * The name precedence order is as follows,
 *      Choice #1: PE exports name.
 *      Choice #2: executable qualified name (This would be the last choice
 *                  except historically it's been #2 so we'll stick with that).
 *      Choice #3: .rsrc original filename, already strduped.
 *      Choice #4: file name.
 */
#    define GET_MODULE_NAME(mod_names)                                                  \
        (((mod_names)->module_name != NULL)                                             \
             ? (mod_names)->module_name                                                 \
             : (((mod_names)->exe_name != NULL)                                         \
                    ? (mod_names)->exe_name                                             \
                    : (((mod_names)->rsrc_name != NULL)                                 \
                           ? (mod_names)->rsrc_name                                     \
                           : (((mod_names)->file_name != NULL) ? (mod_names)->file_name \
                                                               : NULL))))
#else
/* For Linux the precedence order is as follows,
 * Choice #1: the module_name (SONAME entry from the DYNAMIC program_header, if it
 * exists) Choice #2: the filename of the file mapped in (from the maps file)
 */
#    define GET_MODULE_NAME(mod_names)      \
        (((mod_names)->module_name != NULL) \
             ? (mod_names)->module_name     \
             : (((mod_names)->file_name != NULL) ? (mod_names)->file_name : NULL))
#endif

/* Used to augments the basic vm_area_vector_t interval, all fields that
 * we get from the loader or PE/ELF header should be maintained here. This is what
 * we store in the loaded_module_areas vector. */
typedef struct _module_area_t {
    /* vm_area_t has start and end fields, but we're duplicating them in module_data_t so
     * the information is available to the module iterator. */
    /* On windows start-end bound the view of the module that was mapped. This view size
     * is almost always the same as the internal_size == pe size (the full size of the
     * module).  On Vista we've seen drivers mapped into user processes (leading to
     * view size = PAGE_ALIGN(pe_size) > pe size) and partial mappings of child
     * executables (leading to view size < pe_size). */
    /* To support non-contiguous library mappings on Linux (i#160/PR 562667)
     * we have the os-specific routines add each module segment to the
     * vmvector.  We store no data here on that but rely on the vector and
     * on checking whether the vector entry's start == this start to know
     * which entry is the primary entry for a module.
     * We still store the maximum endpoint in the end field (after this
     * data structure is fully initialized).  Use module_contains_addr() to
     * check for overlap, rather than checking [start..end).
     */
    app_pc start;
    app_pc end;

    app_pc entry_point;

    uint flags;

    module_names_t names;
    char *full_path;

    os_module_data_t os_data; /* os specific data for this module */
} module_area_t;

/* Flags used in module_area_t.flags */
enum {
    /* case 9653: only the 1st coarse unit in a module's +x region(s) is persisted */
    MODULE_HAS_PRIMARY_COARSE = 0x00000001,
#if defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH)
    /* case 8648: did we load persisted RCT data for the whole module? */
    MODULE_RCT_LOADED = 0x00000002,
#endif
#ifdef RETURN_AFTER_CALL /* should include RCT_IND_BRANCH but matching other code */
    MODULE_HAS_BORLAND_SEH = 0x00000004,
#endif
    /* case 9672: used to detect whether to preserve persisted RCT on a flush */
    MODULE_BEING_UNLOADED = 0x00000008,
    /* case 9799: used to ensure persistent caches are safe to use */
    MODULE_WAS_EXEMPTED = 0x00000010,
#if defined(X64) && (defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH))
    /* PR 277064/277044: have we scanned the module yet? */
    MODULE_RCT_SCANNED = 0x00000020,
#endif
#ifdef WINDOWS
    /* do not created a persistent cache from this module */
    MODULE_DO_NOT_PERSIST = 0x00000040,
#endif
    MODULE_NULL_INSTRUMENT = 0x00000080,
    /* we use this to send just one module load event on 1st exec (i#884) */
    MODULE_LOAD_EVENT = 0x00000100,
};

/**************** init/exit routines *****************/
void
modules_init(void);
bool
is_module_list_initialized(void);
void
modules_exit(void);
void
modules_reset_list(void);

/**************** module_list updating routines *****************/
void
module_list_add(app_pc base, size_t view_size, bool at_map,
                const char *filepath _IF_UNIX(uint64 inode));
void
module_list_remove(app_pc base, size_t view_size);
void
module_list_add_mapping(module_area_t *ma, app_pc map_start, app_pc map_end);
void
module_list_remove_mapping(module_area_t *ma, app_pc map_start, app_pc map_end);

/**************** module_data_lock routines *****************/
void
os_get_module_info_lock(void);
void
os_get_module_info_unlock(void);
void
os_get_module_info_write_lock(void);
void
os_get_module_info_write_unlock(void);
bool
os_get_module_info_locked(void);
bool
os_get_module_info_write_locked(void);

/**************** module flag routines *****************/
bool
os_module_set_flag(app_pc module_base, uint flag);
bool
os_module_clear_flag(app_pc module_base, uint flag);
bool
os_module_get_flag(app_pc module_base, uint flag);

/**************** module_area accessor routines (os shared) *****************/

/* no lock required by caller */
bool
pc_is_in_module(byte *pc);

module_area_t *
module_pc_lookup(byte *pc);

bool
module_overlaps(byte *pc, size_t len);

/* Unlike os_get_module_info(), sets *name to NULL if return value is false */
bool
os_get_module_name(const app_pc pc, /*OUT*/ const char **name);

const char *
os_get_module_name_strdup(const app_pc pc HEAPACCT(which_heap_t which));

/* Returns the number of characters copied (maximum is buf_len -1).
 * If there is no module at pc, or no module name available, 0 is
 * returned and the buffer set to "".
 */
size_t
os_get_module_name_buf(const app_pc pc, char *buf, size_t buf_len);

/* Copies the module name into buf and returns a pointer to buf,
 * unless buf is too small, in which case the module name is strdup-ed
 * and a pointer to it returned (which the caller must strfree).
 * If there is no module name, returns NULL.
 */
const char *
os_get_module_name_buf_strdup(const app_pc pc, char *buf,
                              size_t buf_len HEAPACCT(which_heap_t which));

size_t
os_module_get_view_size(app_pc module_base);

bool
module_contains_addr(module_area_t *ma, app_pc pc);

/**************** module iterator routines *****************/
/* module iterator fields */
typedef struct _module_iterator_t module_iterator_t;
/* If plan to write to module_area fields must call os_get_module_info_write_[un]lock
 * around entire usage of the iterator. */
module_iterator_t *
module_iterator_start(void);
bool
module_iterator_hasnext(module_iterator_t *mi);
module_area_t *
module_iterator_next(module_iterator_t *mi);
void
module_iterator_stop(module_iterator_t *mi);

/***************** in os specific module.c *****************/
void
os_modules_init(void);
void
os_modules_exit(void);
void
os_module_area_init(module_area_t *ma, app_pc base, size_t view_size, bool at_map,
                    const char *filepath _IF_UNIX(uint64 inode)
                        HEAPACCT(which_heap_t which));
void
os_module_area_reset(module_area_t *ma HEAPACCT(which_heap_t which));
void
free_module_names(module_names_t *mod_names HEAPACCT(which_heap_t which));

#ifdef UNIX
/* returns true if the module is marked as having text relocations */
bool
module_has_text_relocs(app_pc base, bool at_map);
#endif

void
module_copy_os_data(os_module_data_t *dst, os_module_data_t *src);

/**************************************************************************************/
/* Moved from os_shared.h to use typedefs here, in <os>/module.c
 * Should clean up and see if these can be shared/obsoleted by the os shared mod list. */
bool
os_get_module_info_all_names(const app_pc pc,
                             /* FIXME PR 215890: does ELF64 use 64-bit timestamp
                              * or checksum?
                              */
                             uint *checksum, uint *timestamp, size_t *size,
                             module_names_t **names, size_t *code_size,
                             uint64 *file_version);

/* We'd like to have these get_proc_address* routines take a module_handle_t for
 * type safety, but we have too much DR internal code that passes HMODULE,
 * HANDLE, and app_pc values to these routines.  Instead we use this
 * module_base_t typedef internally, so we can freely convert from handles to
 * void*'s and app_pcs.
 */
typedef void *module_base_t;

generic_func_t
d_r_get_proc_address(module_base_t lib, const char *name);

#ifdef UNIX
/* if we add any more values, switch to a globally-defined dr_export_info_t
 * and use it here
 */
generic_func_t
get_proc_address_ex(module_base_t lib, const char *name, bool *is_indirect_code OUT);
#else /* WINDOWS */

generic_func_t
get_proc_address_ex(module_base_t lib, const char *name, const char **forwarder OUT);

generic_func_t
get_proc_address_by_ordinal(module_base_t lib, uint ordinal, const char **forwarder OUT);

generic_func_t
get_proc_address_resolve_forward(module_base_t lib, const char *name);

#endif /* WINDOWS */

#ifdef WINDOWS
uint64
get_remote_process_entry(HANDLE process_handle, OUT bool *x86_code);
#endif

void
print_modules(file_t f, bool dump_xml);

const char *
get_module_short_name(app_pc pc HEAPACCT(which_heap_t which));

app_pc
get_module_base(app_pc pc);

/* Returns true if [start_pc, end_pc) is within a single code section.
 * Returns the bounds of the enclosing section in sec_start and sec_end.
 * Note that unlike is_in_*_section routines, does NOT merge adjacent sections. */
bool
is_range_in_code_section(app_pc module_base, app_pc start_pc, app_pc end_pc,
                         app_pc *sec_start /* OPTIONAL OUT */,
                         app_pc *sec_end /* OPTIONAL OUT */);
/* Returns true if addr is in a code section and if so returns in sec_start and sec_end
 * the bounds of the section containing addr (MERGED with adjacent code sections). */
bool
is_in_code_section(app_pc module_base, app_pc addr, app_pc *sec_start /* OPTIONAL OUT */,
                   app_pc *sec_end /* OPTIONAL OUT */);
/* Same as above only for initialized data sections instead of code. */
bool
is_in_dot_data_section(app_pc module_base, app_pc addr,
                       app_pc *sec_start /* OPTIONAL OUT */,
                       app_pc *sec_end /* OPTIONAL OUT */);
/* Same as above only for any section instead of code. */
bool
is_in_any_section(app_pc module_base, app_pc addr, app_pc *sec_start /* OPTIONAL OUT */,
                  app_pc *sec_end /* OPTIONAL OUT */);

bool
is_mapped_as_image(app_pc module_base);

bool
os_get_module_info(const app_pc pc, uint *checksum, uint *timestamp, size_t *size,
                   const char **name, size_t *code_size, uint64 *file_version);
static inline bool
module_info_exists(const app_pc pc)
{
    return os_get_module_info(pc, NULL, NULL, NULL, NULL, NULL, NULL);
}

bool
get_named_section_bounds(app_pc module_base, const char *name, app_pc *start /*OUT*/,
                         app_pc *end /*OUT*/);
bool
get_module_company_name(app_pc mod_base, char *out_buf, size_t out_buf_size);

#if defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH)
rct_module_table_t *
os_module_get_rct_htable(app_pc pc, rct_type_t which);
#endif

bool
module_get_nth_segment(app_pc module_base, uint n, app_pc *start /*OPTIONAL OUT*/,
                       app_pc *end /*OPTIONAL OUT*/, uint *chars /*OPTIONAL OUT*/);

size_t
module_get_header_size(app_pc module_base);

typedef struct {
    /* MD5 of portions of files mapped as image sections */
    byte full_MD5[MD5_RAW_BYTES];
    /* a full digest uses all readable raw bytes (as also present in
     * the file) in sections loaded in memory */
    /* Note that for most files this should result in the same value
     * as md5sum, except for files with digital signatures or
     * debugging information that is not loaded in memory */

    /* an MD5 digest of only header and footer of file with lengths
     * specified by aslr_short_digest */
    byte short_MD5[MD5_RAW_BYTES];

    /* FIXME: case 4678 about possible NYI subregion checksums
     * amenable to lazy evaluation.  Such digests which will be of
     * dynamic size and file offset which will have to be specified
     * here. */
} module_digest_t;
/* FIXME: rename since being used for module-independent purposes? */

void
module_calculate_digest(/*OUT*/ module_digest_t *digest, app_pc module_base,
                        size_t module_size, bool full_digest, bool short_digest,
                        size_t short_digest_size, uint sec_char_include,
                        uint sec_char_exclude);

/* actually in utils.c since os-independent */
bool
module_digests_equal(const module_digest_t *calculated_digest,
                     const module_digest_t *matching_digest, bool check_short,
                     bool check_full);

/***************************************************************************/
/* DR's custom loader related data structure and functions,
 * which should be used by loader only.
 */

/* List of privately-loaded modules.
 * We assume there will only be a handful of privately-loaded modules,
 * so we do not bother to optimize: we use a linked list, search by
 * linear walk, and find exports by walking the PE structures each time.
 * The list is kept in reverse-dependent order so we can unload from the
 * front without breaking dependencies.
 */
typedef struct _privmod_t {
    app_pc base;
    size_t size;
    const char *name;
    char path[MAXIMUM_PATH];
    uint ref_count;
    bool externally_loaded;
    bool is_client; /* or Extension */
    bool called_proc_entry;
    bool called_proc_exit;
    struct _privmod_t *next;
    struct _privmod_t *prev;
    void *os_privmod_data;
} privmod_t;

/* more os independent name */
#ifdef WINDOWS
#    define DLL_PROCESS_INIT DLL_PROCESS_ATTACH
#    define DLL_PROCESS_EXIT DLL_PROCESS_DETACH
#    define DLL_THREAD_INIT DLL_THREAD_ATTACH
#    define DLL_THREAD_EXIT DLL_THREAD_DETACH
#else
#    define DLL_PROCESS_INIT 1
#    define DLL_PROCESS_EXIT 2
#    define DLL_THREAD_INIT 3
#    define DLL_THREAD_EXIT 4
#endif

/* We need to load client libs prior to having heap */
#define PRIVMOD_STATIC_NUM 8
/* It should have more entries than the PRIVMOD_STATIC_NUM,
 * as it may also contains the extension libraries and
 * externally loaded libraries, as well as our rpath-file search paths.
 */
#define SEARCH_PATHS_NUM (3 * PRIVMOD_STATIC_NUM)

extern recursive_lock_t privload_lock;
extern char search_paths[SEARCH_PATHS_NUM][MAXIMUM_PATH];
extern uint search_paths_idx;
extern vm_area_vector_t *modlist_areas;

/***************************************************************************
 * Public functions
 */

/* Flags for use with privload_map_and_relocate() */
typedef enum {
    MODLOAD_REACHABLE = 0x0001,
    /* These are for use with dr_map_executable_file(): */
    MODLOAD_NOT_PRIVLIB = 0x0002,
    MODLOAD_SKIP_WRITABLE = 0x0004, /* ignored on Windows */
    /* For use with privload_map_and_relocate() or elf_loader_map_phdrs().
     * Places an extra no-access page after .bss.
     */
    MODLOAD_SEPARATE_BSS = 0x0008,
    /* Indicates that the module is being loaded in another process. */
    MODLOAD_SEPARATE_PROCESS = 0x0010,
    /* For use with elf_loader_map_phdrs().  Avoids MAP_32BIT and other DR-mem-only
     * distortions for app mappings (e.g., early inject mapping the interpreter).
     */
    MODLOAD_IS_APP = 0x0020,
} modload_flags_t;

/* This function is used for loading non-private libs as well as private. */
app_pc
privload_map_and_relocate(const char *filename, size_t *size OUT, modload_flags_t flags);

/* returns whether they all fit */
bool
privload_print_modules(bool path, bool lock, char *buf, size_t bufsz, size_t *sofar);

/* ************************************************************************* *
 * os independent functions in loader_shared.c, can be called from loader.c  *
 * ************************************************************************* */
privmod_t *
privload_load(const char *filename, privmod_t *dependent, bool reachable);

bool
privload_unload(privmod_t *privmod);

privmod_t *
privload_lookup(const char *name);

privmod_t *
privload_lookup_by_base(app_pc modbase);

privmod_t *
privload_lookup_by_pc(app_pc modbase);

/* name is assumed to be in immutable persistent storage.
 * a copy of path is made.
 */
privmod_t *
privload_insert(privmod_t *after, app_pc base, size_t size, const char *name,
                const char *path);

bool
privload_search_path_exists(const char *path, size_t len);

/* ************************************************************************* *
 * os specific functions in loader.c, can be called from loader_shared.c     *
 * ************************************************************************* */

/* searches in standard paths instead of requiring abs path */
app_pc
privload_load_private_library(const char *name, bool reachable);

void
privload_redirect_setup(privmod_t *mod);

void
privload_os_finalize(privmod_t *privmod_t);

bool
privload_call_entry(dcontext_t *dcontext, privmod_t *privmod, uint reason);

bool
privload_process_imports(privmod_t *mod);

bool
privload_unload_imports(privmod_t *mod);

void
privload_unmap_file(privmod_t *mod);

void
privload_add_areas(privmod_t *privmod);

void
privload_remove_areas(privmod_t *privmod);

void
privload_add_drext_path(void);

privmod_t *
privload_next_module(privmod_t *mod);

privmod_t *
privload_first_module(void);

bool
privload_fill_os_module_info(app_pc base, OUT app_pc *out_base /* relative pc */,
                             OUT app_pc *out_max_end /* relative pc */,
                             OUT char **out_soname, OUT os_module_data_t *out_data);

/* os specific loader initialization prologue before finalize the load,
 * will also acquire privload_lock.
 */
void
os_loader_init_prologue(void);

/* os specific loader initialization epilogue after finalize the load,
 * will release privload_lock.
 */
void
os_loader_init_epilogue();

void
os_loader_exit(void);

/* os specific thread initilization prologue for loader, holding no lock */
void
os_loader_thread_init_prologue(dcontext_t *dcontext);

/* os specific thread initilization epilogue for loader, holding no lock */
void
os_loader_thread_init_epilogue(dcontext_t *dcontext);

/* os specific thread exit for loader, holding no lock */
void
os_loader_thread_exit(dcontext_t *dcontext);

char *
get_shared_lib_name(app_pc map);

app_pc
get_image_entry(void);

void
privload_load_finalized(privmod_t *mod);

#ifdef WINDOWS
bool
privload_console_share(app_pc priv_kernel32, app_pc app_kernel32);

bool
privload_attach_parent_console(app_pc app_kernel32);
#endif

/* Redirected functions for loaded module,
 * they are also used by __wrap_* functions in instrument.c
 */

#ifdef DEBUG
/* i#975: used for debug checks for static-link-ready clients. */
#    define DR_DISALLOW_UNSAFE_STATIC_NAME "_DR_DISALLOW_UNSAFE_STATIC_"
extern bool disallow_unsafe_static_calls;
#endif

/* For all heap allocation redirection routines:
 * The returned address is guaranteed to be double-pointer-aligned:
 * aligned to 16 bytes for 64-bit; aligned to 8 bytes for 32-bit.
 */
#define STANDARD_HEAP_ALIGNMENT IF_X64_ELSE(16, 8)

void *
redirect_calloc(size_t nmemb, size_t size);

void *
redirect_malloc(size_t size);

void
redirect_free(void *mem);

void *
redirect_realloc(void *mem, size_t size);

char *
redirect_strdup(const char *str);

size_t
redirect_malloc_requested_size(void *mem);

#ifdef DEBUG
void *
redirect_malloc_initonly(size_t size);
void *
redirect_realloc_initonly(void *mem, size_t size);
void *
redirect_calloc_initonly(size_t nmemb, size_t size);
void
redirect_free_initonly(void *mem);
char *
redirect_strdup_initonly(const char *str);
#endif

#endif /* MODULE_SHARED_H */
