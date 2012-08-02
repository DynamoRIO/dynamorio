/* *******************************************************************************
 * Copyright (c) 2011-2012 Google, Inc.  All rights reserved.
 * Copyright (c) 2011 Massachusetts Institute of Technology  All rights reserved.
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
/*
 * loader.c: custom private library loader for Linux
 *
 * original case: i#157
 */

#include "../globals.h"
#include "../module_shared.h"
#include "os_private.h"
#include "os_exports.h"   /* os_get_dr_seg_base */
#include "../x86/instr.h" /* SEG_GS/SEG_FS */
#include "module.h"     /* elf */
#include "../heap.h"    /* HEAPACCT */

#include <dlfcn.h>      /* dlsym */
#include <string.h>     /* strcmp */
#include <stdlib.h>     /* getenv */
#include <dlfcn.h>      /* dlopen/dlsym */
#include <link.h>       /* Elf_Symndx */
#include <unistd.h>     /* __environ */

/* Written during initialization only */
/* FIXME: i#460, the path lookup itself is a complicated process, 
 * so we just list possible common but in-complete paths for now.
 */
#define SYSTEM_LIBRARY_PATH_VAR "LD_LIBRARY_PATH"
char *ld_library_path = NULL;
static const char *system_lib_paths[] = {
    "/lib/tls/i686/cmov",
    "/usr/lib",
    "/lib",
    "/usr/local/lib",       /* Ubuntu: /etc/ld.so.conf.d/libc.conf */
#ifndef X64
    "/lib32/tls/i686/cmov",
    "/usr/lib32",
    "/lib32",
    /* 32-bit Ubuntu */
    "/lib/i386-linux-gnu",
    "/usr/lib/i386-linux-gnu",
#else
    "/lib64/tls/i686/cmov",
    "/usr/lib64",
    "/lib64",
    /* 64-bit Ubuntu */
    "/lib/x86_64-linux-gnu",     /* /etc/ld.so.conf.d/x86_64-linux-gnu.conf */
    "/usr/lib/x86_64-linux-gnu", /* /etc/ld.so.conf.d/x86_64-linux-gnu.conf */
#endif
};
#define NUM_SYSTEM_LIB_PATHS \
  (sizeof(system_lib_paths) / sizeof(system_lib_paths[0]))


static os_privmod_data_t *libdr_opd;
static bool privmod_initialized = false;
static size_t max_client_tls_size = 2 * PAGE_SIZE;

#ifdef INTERNAL
static bool printed_gdb_commands = false;
#endif

/* pointing to the I/O data structure in privately loaded libc,
 * They are used on exit when we need update file_no.
 */
struct _IO_FILE  **privmod_stdout;
struct _IO_FILE  **privmod_stderr;
struct _IO_FILE  **privmod_stdin;
#define LIBC_STDOUT_NAME "stdout"
#define LIBC_STDERR_NAME "stderr"
#define LIBC_STDIN_NAME  "stdin"

/* forward decls */

static void
privload_init_search_paths(void);

static bool
privload_locate(const char *name, privmod_t *dep, char *filename OUT, bool *is_client OUT);

static privmod_t *
privload_locate_and_load(const char *impname, privmod_t *dependent);

static void
privload_call_modules_entry(privmod_t *mod, uint reason);

static void
privload_call_lib_func(fp_t func);

static void
privload_relocate_mod(privmod_t *mod);

static void
privload_create_os_privmod_data(privmod_t *privmod);

static void
privload_delete_os_privmod_data(privmod_t *privmod);

static void
privload_mod_tls_init(privmod_t *mod);

/***************************************************************************/

/* os specific loader initialization prologue before finalizing the load. */
void
os_loader_init_prologue(void)
{
    privmod_t *mod;

    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);

    privload_init_search_paths();
    /* insert libdynamorio.so */
    mod = privload_insert(NULL,
                          get_dynamorio_dll_start(),
                          get_dynamorio_dll_end() - get_dynamorio_dll_start(),
                          get_shared_lib_name(get_dynamorio_dll_start()),
                          get_dynamorio_library_path(),
                          NULL/*no opd*/);
    privload_create_os_privmod_data(mod);
    libdr_opd = (os_privmod_data_t *) mod->os_privmod_data;
    mod->externally_loaded = true;
    ASSERT(mod != NULL);
}

/* os specific loader initialization epilogue after finalizing the load. */
void
os_loader_init_epilogue(void)
{
#ifdef INTERNAL
    /* Print the add-symbol-file commands so they can be copy-pasted into gdb.
     * We have to do it in a single syslog so they can be copy pasted.  Since
     * info syslogs are only in internal builds, we only do this work in an
     * internal build.  To debug an external build, we rely on the gdb script to
     * find text_addr in opd.
     * FIXME i#531: Support attaching from the gdb script.
     */
    privmod_t *mod;
    size_t sofar = 0;
    size_t bufsz = 4096;  /* Should be enough, but too much for stack. */
    char *buf;

    /* FIXME: Skip this work if we're not going to print or log. */
    ASSERT(dynamo_heap_initialized);
    ASSERT(!printed_gdb_commands);
    buf = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, char, bufsz, ACCT_OTHER, PROTECTED);
    acquire_recursive_lock(&privload_lock);
    for (mod = privload_first_module(); mod != NULL;
         mod = privload_next_module(mod)) {
        os_privmod_data_t *opd = (os_privmod_data_t *) mod->os_privmod_data;
        /* GDB already finds externally loaded modules (e.g. DR). */
        if (mod->externally_loaded)
            continue;
        print_to_buffer(buf, bufsz, &sofar, "add-symbol-file '%s' %p\n",
                        mod->path, opd->text_addr);
    }
    printed_gdb_commands = true;
    release_recursive_lock(&privload_lock);
    if (sofar > 0) {
        SYSLOG_INTERNAL_INFO("Paste into GDB to debug DynamoRIO clients:\n"
                             /* Need to turn off confirm for paste to work. */
                             "set confirm off\n"
                             "%s", buf);
    }
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, buf, char, bufsz, ACCT_OTHER, PROTECTED);
#endif /* INTERNAL */
}

void
os_loader_exit(void)
{
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, 
                    libdr_opd->os_data.segments, 
                    module_segment_t,
                    libdr_opd->os_data.alloc_segments, 
                    ACCT_OTHER, PROTECTED);
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, libdr_opd,
                   os_privmod_data_t, ACCT_OTHER, PROTECTED);
}

void
os_loader_thread_init_prologue(dcontext_t *dcontext)
{
    if (!privmod_initialized) {
        /* Because TLS is not setup at loader_init, we cannot call 
         * loaded libraries' init functions there, so have to postpone
         * the invocation until here. 
         */
        acquire_recursive_lock(&privload_lock);
        privmod_t *mod = privload_first_module();
        privload_call_modules_entry(mod, DLL_PROCESS_INIT);
        release_recursive_lock(&privload_lock);
        privmod_initialized = true;
    }
}

void
os_loader_thread_init_epilogue(dcontext_t *dcontext)
{
    /* do nothing */
}

void
os_loader_thread_exit(dcontext_t *dcontext)
{
    /* do nothing */
}

void
privload_add_areas(privmod_t *privmod)
{
    os_privmod_data_t *opd;
    uint   i;
    app_pc text_addr;

    /* Save text_addr and store it in opd after we create it.  We need this to
     * support auto loading symbols on gdb attach (i#531).
     */
    text_addr = (app_pc)privmod->os_privmod_data;

    /* create and init the os_privmod_data for privmod.
     * The os_privmod_data can only be created after heap is ready and 
     * should be done before adding in vmvector_add,
     * so it can be either right before calling to privload_add_areas 
     * in the privload_load_finalize, or in here.
     * We prefer here because it avoids changing the code in
     * loader_shared.c, which affects windows too.
      */
    privload_create_os_privmod_data(privmod);
    opd = (os_privmod_data_t *) privmod->os_privmod_data;
    opd->text_addr = text_addr;
    for (i = 0; i < opd->os_data.num_segments; i++) {
        vmvector_add(modlist_areas, 
                     opd->os_data.segments[i].start,
                     opd->os_data.segments[i].end,
                     (void *)privmod);
    }
}

void
privload_remove_areas(privmod_t *privmod)
{
    uint i;
    os_privmod_data_t *opd = (os_privmod_data_t *) privmod->os_privmod_data;

    /* walk the program header to remove areas */
    for (i = 0; i < opd->os_data.num_segments; i++) {
        vmvector_remove(modlist_areas, 
                        opd->os_data.segments[i].start,
                        opd->os_data.segments[i].end);
    }
    /* NOTE: we create os_privmod_data in privload_add_areas but
     * do not delete here, non-symmetry.
     * This is because we still need the information in os_privmod_data
     * to unmap the segments in privload_unmap_file, which happens after
     * privload_remove_areas. 
     * The create of os_privmod_data should be done when mapping the file 
     * into memory, but the heap is not ready at that time, so postponed 
     * until privload_add_areas.
     */
}

void
privload_unmap_file(privmod_t *privmod)
{
    /* walk the program header to unmap files, also the tls data */
    uint i;
    os_privmod_data_t *opd = (os_privmod_data_t *) privmod->os_privmod_data;
    
    /* unmap segments */
    for (i = 0; i < opd->os_data.num_segments; i++) {
        unmap_file(opd->os_data.segments[i].start, 
                   opd->os_data.segments[i].end - 
                   opd->os_data.segments[i].start);
    }
    /* free segments */
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, opd->os_data.segments,
                    module_segment_t,
                    opd->os_data.alloc_segments,
                    ACCT_OTHER, PROTECTED);
    /* delete os_privmod_data */
    privload_delete_os_privmod_data(privmod);
}

bool
privload_unload_imports(privmod_t *privmod)
{
    /* FIXME: i#474 unload dependent libraries if necessary */
    return true;
}

/* Register a symbol file with gdb.  This symbol needs to be exported so that
 * gdb can find it even when full debug information is unavailable.  We do
 * *not* consider it part of DR's public API.
 * i#531: gdb support for private loader
 */
DYNAMORIO_EXPORT void
dr_gdb_add_symbol_file(const char *filename, app_pc textaddr)
{
    /* Do nothing.  If gdb is attached with libdynamorio.so-gdb.py loaded, it
     * will stop here and lift the argument values.
     */
    /* FIXME: This only passes the text section offset.  gdb can accept
     * additional "-s<section> <address>" arguments to locate data sections.
     * This would be useful for setting watchpoints on client global variables.
     */
}

app_pc
privload_map_and_relocate(const char *filename, size_t *size OUT,
                          void **os_privmod_data OUT, bool fixed,
                          app_pc *entry OUT, char **interp OUT)
{
    file_t fd;
    byte *(*map_func)  (file_t, size_t *, uint64, app_pc, uint, bool, bool, bool);
    bool  (*unmap_func)(byte *, size_t);
    bool  (*prot_func) (byte *pc, size_t length, uint prot);
    app_pc file_map, lib_base, lib_end, last_end;
    ELF_HEADER_TYPE *elf_hdr;
    app_pc  map_base, map_end;
    reg_t   pg_offs;
    size_t map_size;
    uint64 file_size;
    uint   seg_prot, i;
    ptr_int_t delta;
    app_pc text_addr;

    ASSERT(size != NULL);
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);

    if (entry != NULL)
        *entry = NULL;
    if (interp != NULL)
        *interp = NULL;
    
    /* open file for mmap later */
    fd = os_open(filename, OS_OPEN_READ);
    if (fd == INVALID_FILE) {
        LOG(GLOBAL, LOG_LOADER, 1, "%s: failed to open %s\n", __FUNCTION__, filename);
        return NULL;
    }
    
    /* get appropriate function */
    /* NOTE: all but the client lib will be added to DR areas list 
     * b/c using map_file() 
     */
    if (dynamo_heap_initialized) {
        map_func   = map_file;
        unmap_func = unmap_file;
        prot_func  = set_protection;
    } else {
        map_func   = os_map_file;
        unmap_func = os_unmap_file;
        prot_func  = os_set_protection;
    }

    /* get file size */
    if (!os_get_file_size_by_handle(fd, &file_size)) {
        os_close(fd);
        LOG(GLOBAL, LOG_LOADER, 1, "%s: failed to get library %s file size\n",
            __FUNCTION__, filename);
        return NULL;
    }

    /* map the library file into memory for parsing */
    *size = (size_t)file_size;
    file_map = (*map_func)(fd, size, 0/*offs*/, NULL/*base*/,
                           MEMPROT_READ /* for parsing only */,
                           true  /*writes should not change file*/,
                           false /*image*/,
                           false /*!fixed*/);
    if (file_map == NULL) {
        os_close(fd);
        LOG(GLOBAL, LOG_LOADER, 1, "%s: failed to map %s\n", __FUNCTION__, filename);
        return NULL;
    }

    /* verify if it is elf so header */
    if (!is_elf_so_header(file_map, *size)) {
        (*unmap_func)(file_map, file_size);
        os_close(fd);
        LOG(GLOBAL, LOG_LOADER, 1, "%s: %s is not a elf so header\n", 
            __FUNCTION__, filename);
        return NULL;
    }

    /* more sanity check */
    elf_hdr = (ELF_HEADER_TYPE *) file_map;
    ASSERT_CURIOSITY(elf_hdr->e_phoff != 0);
    ASSERT_CURIOSITY(elf_hdr->e_phentsize == 
                     sizeof(ELF_PROGRAM_HEADER_TYPE));

    /* get the library size and preferred base */
    map_base = 
        module_vaddr_from_prog_header(file_map + elf_hdr->e_phoff,
                                      elf_hdr->e_phnum,
                                      &map_end);
    map_size = map_end - map_base;

    /* reserve the memory from os for library */
    lib_base = (*map_func)(-1, &map_size, 0, map_base,
                           MEMPROT_WRITE | MEMPROT_READ /* prot */,
                           true  /* copy-on-write */,
                           true  /* image, make it reachable */,
                           fixed);
    ASSERT(lib_base != NULL);
    lib_end = lib_base + map_size;

    if (map_base != NULL && map_base != lib_base) {
        /* the mapped memory is not at preferred address, 
         * should be ok if it is still reachable for X64,
         * which will be checked later. 
         */
        LOG(GLOBAL, LOG_LOADER, 1, "%s: module not loaded at preferred address\n",
            __FUNCTION__);
    }
    delta = lib_base - map_base;
    if (entry != NULL)
        *entry = (app_pc)elf_hdr->e_entry + delta;

    /* walk over the program header to load the individual segments */
    last_end = lib_base;
    for (i = 0; i < elf_hdr->e_phnum; i++) {
        app_pc seg_base, seg_end, map, file_end;
        size_t seg_size;
        ELF_PROGRAM_HEADER_TYPE *prog_hdr = (ELF_PROGRAM_HEADER_TYPE *)
            (file_map + elf_hdr->e_phoff + i * elf_hdr->e_phentsize);
        if (interp != NULL && prog_hdr->p_type == PT_INTERP)
            *interp = (char *)prog_hdr->p_vaddr + delta;
        else if (prog_hdr->p_type == PT_LOAD) {
            seg_base = (app_pc)ALIGN_BACKWARD(prog_hdr->p_vaddr, PAGE_SIZE)
                       + delta;
            seg_end  = (app_pc)ALIGN_FORWARD(prog_hdr->p_vaddr +
                                             prog_hdr->p_filesz,
                                             PAGE_SIZE) 
                       + delta;
            seg_size = seg_end - seg_base;
            if (seg_base != last_end) {
                /* XXX: a hole, I reserve this space instead of unmap it */
                size_t hole_size = seg_base - last_end;
                (*prot_func)(last_end, hole_size, MEMPROT_NONE);
            }
            seg_prot = module_segment_prot_to_osprot(prog_hdr);
            pg_offs  = ALIGN_BACKWARD(prog_hdr->p_offset, PAGE_SIZE);
            /* FIXME: 
             * This function can be called after dynamorio_heap_initialized,
             * and we will use map_file instead of os_map_file.
             * However, map_file does not allow mmap with overlapped memory, 
             * so we have to unmap the old memory first.
             * This might be a problem, e.g. 
             * one thread unmaps the memory and before mapping the actual file,
             * another thread requests memory via mmap takes the memory here,
             * a racy condition.
             */
            (*unmap_func)(seg_base, seg_size);
            map = (*map_func)(fd, &seg_size, pg_offs,
                              seg_base /* base */,
                              seg_prot | MEMPROT_WRITE /* prot */,
                              true /* writes should not change file */,
                              true /* image */,
                              true /* fixed */);
            ASSERT(map != NULL);
            /* fill zeros at extend size */
            file_end = (app_pc)prog_hdr->p_vaddr + prog_hdr->p_filesz;
            if (seg_end > file_end + delta)
                memset(file_end + delta, 0, seg_end - (file_end + delta));
            seg_end  = (app_pc)ALIGN_FORWARD(prog_hdr->p_vaddr + 
                                             prog_hdr->p_memsz,
                                             PAGE_SIZE) + delta;
            seg_size = seg_end - seg_base;
            (*prot_func)(seg_base, seg_size, seg_prot);
            last_end = seg_end;
        } 
    }
    ASSERT(last_end == lib_end);

    text_addr = (void*)delta + module_get_text_section(file_map, file_size);
    /* Add debugging comment about how to get symbol information in gdb. */
#ifdef INTERNAL
    if (printed_gdb_commands) {
        /* This is a dynamically loaded auxlib, so we print here.  The client
         * and its direct dependencies are batched up and printed in
         * os_loader_init_epilogue.
         */
        SYSLOG_INTERNAL_INFO("Paste into GDB to debug DynamoRIO clients:\n"
                             "add-symbol-file '%s' %p\n", filename, text_addr);
    }
#endif /* INTERNAL */
    /* We save the text addr in os_privmod_data.  We can't recompute it later
     * (see module_get_text_section comments), and we can't allocate a proper
     * os_privmod_data_t yet.  Therefore, we store text_addr directly in
     * os_privmod_data and move it into the heap allocation later (see
     * privload_add_areas).
     */
    *os_privmod_data = (void*)text_addr;
    LOG(GLOBAL, LOG_LOADER, 1,
        "for debugger: add-symbol-file %s %p\n",
        filename, text_addr);
    if (IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(privload_register_gdb), false)) {
        dr_gdb_add_symbol_file(filename, text_addr);
    }
    /* unmap the file_map */
    (*unmap_func)(file_map, file_size);
    os_close(fd); /* no longer needed */
    fd = INVALID_FILE;
    *size = (reg_t)lib_end - (reg_t)lib_base;
    return lib_base;
}

bool
privload_process_imports(privmod_t *mod)
{
    ELF_DYNAMIC_ENTRY_TYPE *dyn;
    os_privmod_data_t *opd;
    char *strtab, *name;

    opd = (os_privmod_data_t *) mod->os_privmod_data;
    ASSERT(opd != NULL);
    /* 1. get DYNAMIC section pointer */
    dyn = (ELF_DYNAMIC_ENTRY_TYPE *)opd->dyn;
    /* 2. get dynamic string table */
    strtab = (char *)opd->os_data.dynstr;
    /* 3. depth-first recursive load, so add into the deps list first */
    while (dyn->d_tag != DT_NULL) {
        if (dyn->d_tag == DT_NEEDED) {
            name = strtab + dyn->d_un.d_val;
            if (privload_lookup(name) == NULL) {
                privmod_t *impmod = privload_locate_and_load(name, mod);
                if (impmod == NULL)
                    return false;
#ifdef CLIENT_INTERFACE
                /* i#852: identify all libs that import from DR as client libs */
                if (impmod->base == get_dynamorio_dll_start())
                    mod->is_client = true;
#endif
            }
        }
        ++dyn;
    }
    /* Relocate library's symbols after load dependent libraries. */
    if (!mod->externally_loaded)
        privload_relocate_mod(mod);
    return true;
}

bool
privload_call_entry(privmod_t *privmod, uint reason)
{
    os_privmod_data_t *opd = (os_privmod_data_t *) privmod->os_privmod_data;
    if (os_get_dr_seg_base(NULL, LIB_SEG_TLS) == NULL) {
        /* HACK: i#338
         * The privload_call_entry is called in privload_load_finalize
         * from loader_init.
         * Because the loader_init is before os_tls_init,
         * the tls is not setup yet, and cannot call init function,
         * but cannot return false either as it cause loading failure.
         * Cannot change the privload_load_finalize as it affects windows.
         * so can only return true, and call it later in lader_thread_init.
         * Also see comment from os_loader_thread_init_prologue.
         * Any other possible way?
         */
        return true;
    }
    if (reason == DLL_PROCESS_INIT) {
        /* calls init and init array */
        if (opd->init != NULL) {
            privload_call_lib_func(opd->init);
        }
        if (opd->init_array != NULL) {
            uint i;
            for (i = 0; 
                 i < opd->init_arraysz / sizeof(opd->init_array[i]); 
                 i++) {
                privload_call_lib_func(opd->init_array[i]);
            }
        }
        return true;
    } else if (reason == DLL_PROCESS_EXIT) {
        /* calls fini and fini array */
        if (opd->fini != NULL) {
            privload_call_lib_func(opd->fini);
        }
        if (opd->fini_array != NULL) {
            uint i;
            for (i = 0;
                 i < opd->init_arraysz / sizeof(opd->fini_array[0]);
                 i++) {
                privload_call_lib_func(opd->fini_array[i]);
            }
        }
        return true;
    }
    return false;
}

void
privload_redirect_setup(privmod_t *privmod)
{
    /* do nothing, the redirection is done when relocating */
}

static void
privload_init_search_paths(void)
{
    privload_add_drext_path();
    ld_library_path = getenv(SYSTEM_LIBRARY_PATH_VAR);
}

static privmod_t *
privload_locate_and_load(const char *impname, privmod_t *dependent)
{
    char filename[MAXIMUM_PATH];
    bool is_client;
    if (privload_locate(impname, dependent, filename, &is_client))
        return privload_load(filename, dependent);
    return NULL;
}

static bool
privload_locate(const char *name, privmod_t *dep, 
                char *filename OUT /* buffer size is MAXIMUM_PATH */,
                bool *is_client OUT)
{
    uint i;
    char *lib_paths;
    if (is_client != NULL)
        *is_client = false;

    /* FIXME: We have a simple implementation of library search.
     * libc implementation can be found at elf/dl-load.c:_dl_map_object.
     */
    /* the loader search order: */
    /* 0) DT_RPATH */
    /* FIXME: i#460 not implemented. */

    /* 1) client lib dir */
    for (i = 0; i < search_paths_idx; i++) {
        snprintf(filename, MAXIMUM_PATH, "%s/%s", search_paths[i], name);
        /* NULL_TERMINATE_BUFFER(filename) */
        filename[MAXIMUM_PATH - 1] = 0;
        LOG(GLOBAL, LOG_LOADER, 2, "%s: looking for %s\n",
            __FUNCTION__, filename);
        if (os_file_exists(filename, false/*!is_dir*/) &&
            os_file_has_elf_so_header(filename)) {
            if (is_client != NULL)
                *is_client = true;
            return true;
        }
    }

    /* 2) curpath */
    snprintf(filename, MAXIMUM_PATH, "./%s", name);
    /* NULL_TERMINATE_BUFFER(filename) */
    filename[MAXIMUM_PATH - 1] = 0;
    LOG(GLOBAL, LOG_LOADER, 2, "%s: looking for %s\n", __FUNCTION__, filename);
    if (os_file_exists(filename, false/*!is_dir*/) &&
        os_file_has_elf_so_header(filename))
        return true;

    /* 3) LD_LIBRARY_PATH */
    lib_paths = ld_library_path;
    while (lib_paths != NULL) {
        char *end = strstr(lib_paths, ":");
        if (end != NULL) 
            *end = '\0';
        snprintf(filename, MAXIMUM_PATH, "%s/%s", lib_paths, name);
        if (end != NULL) {
            *end = ':';
            end++;
        } 
        /* NULL_TERMINATE_BUFFER(filename) */
        filename[MAXIMUM_PATH - 1] = 0;
        LOG(GLOBAL, LOG_LOADER, 2, "%s: looking for %s\n",
            __FUNCTION__, filename);
        if (os_file_exists(filename, false/*!is_dir*/) &&
            os_file_has_elf_so_header(filename))
            return true;
        lib_paths = end;
    }

    /* 4) FIXME: i#460, we use our system paths instead of /etc/ld.so.cache. */
    for (i = 0; i < NUM_SYSTEM_LIB_PATHS; i++) {
        snprintf(filename, MAXIMUM_PATH, "%s/%s", system_lib_paths[i], name);
        /* NULL_TERMINATE_BUFFER(filename) */
        filename[MAXIMUM_PATH - 1] = 0;
        LOG(GLOBAL, LOG_LOADER, 2, "%s: looking for %s\n",
            __FUNCTION__, filename);
        if (os_file_exists(filename, false/*!is_dir*/) && 
            os_file_has_elf_so_header(filename))
            return true;
    }

    /* Cannot find the library */
    SYSLOG(SYSLOG_ERROR, CLIENT_LIBRARY_UNLOADABLE, 4,
           get_application_name(), get_application_pid(), name,
           "\n\tUnable to locate library! Try adding path to LD_LIBRARY_PATH");
    return false;
}

app_pc
get_private_library_address(app_pc modbase, const char *name)
{
    privmod_t *mod;
    app_pc res;

    acquire_recursive_lock(&privload_lock);
    mod = privload_lookup_by_base(modbase);
    if (mod == NULL || mod->externally_loaded) {
        release_recursive_lock(&privload_lock);
        /* externally loaded, use dlsym instead */
        return dlsym(modbase, name);
    }
    /* Before the heap is initialized, we store the text address in opd, so we
     * can't check if opd != NULL to know whether it's valid.
     */
    if (dynamo_heap_initialized) {
        /* opd is initialized */
        os_privmod_data_t *opd = (os_privmod_data_t *) mod->os_privmod_data;
        res = get_proc_address_from_os_data(&opd->os_data, 
                                            opd->load_delta, 
                                            name, NULL);
        release_recursive_lock(&privload_lock);
        return res;
    } else {
        /* opd is not initialized */
        /* get_private_library_address is first called on looking up
         * USES_DR_VERSION_NAME right after loading client_lib, 
         * The os_privmod_data is not setup yet then because the heap
         * is not initialized, so it is possible opd to be NULL.
         * For this case, we have to compute the temporary os_data instead.
         */
        ptr_int_t delta;
        char *soname;
        os_module_data_t os_data;
        memset(&os_data, 0, sizeof(os_data));
        if (!module_read_os_data(mod->base, &delta, &os_data, &soname)) {
            release_recursive_lock(&privload_lock);
            return NULL;
        }
        res = get_proc_address_from_os_data(&os_data, delta, name, NULL);
        release_recursive_lock(&privload_lock);
        return res;
    }
    ASSERT_NOT_REACHED();
    return NULL;
}

static void
privload_call_modules_entry(privmod_t *mod, uint reason)
{
    if (reason == DLL_PROCESS_INIT) {
        /* call the init function in the reverse order, to make sure the 
         * dependent libraries are inialized first.
         * We recursively call privload_call_modules_entry to call
         * the privload_call_entry in the reverse order.
         * The stack should be big enough since it loaded all libraries 
         * recursively. 
         * XXX: we can change privmod to be double-linked list to avoid
         * recursion.
         */
        privmod_t *next = privload_next_module(mod);
        if (next != NULL)
            privload_call_modules_entry(next, reason);
        if (!mod->externally_loaded)
            privload_call_entry(mod, reason);
    } else {
        ASSERT(reason == DLL_PROCESS_EXIT);
        /* call exit in the module list order. */
        while (mod != NULL) {
            if (!mod->externally_loaded)
                privload_call_entry(mod, reason);
            mod = privload_next_module(mod);
        }
    }
}


static void
privload_call_lib_func(fp_t func)
{
    char *dummy_argv[1];
    /* FIXME: i#475
     * The regular loader always passes argc, argv and env to libaries,
     * (see libc code elf/dl-init.c), which might be ignored by those
     * routines. 
     * we create dummy argc and argv, and passed with the real __environ.
     */
    dummy_argv[0] = "dummy";
    func(1, dummy_argv, our_environ);
}

bool
get_private_library_bounds(IN app_pc modbase, OUT byte **start, OUT byte **end)
{
    privmod_t *mod;
    bool found = false;

    ASSERT(start != NULL && end != NULL);
    acquire_recursive_lock(&privload_lock);
    mod = privload_lookup_by_base(modbase);
    if (mod != NULL) {
        *start = mod->base;
        *end   = mod->base + mod->size;
        found = true;
    }
    release_recursive_lock(&privload_lock);
    return found;
}

static void
privload_relocate_mod(privmod_t *mod)
{
    os_privmod_data_t *opd = (os_privmod_data_t *) mod->os_privmod_data;

    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);

    /* If module has tls block need update its tls offset value */
    if (opd->tls_block_size != 0) 
        privload_mod_tls_init(mod);

    if (opd->rel != NULL) {
        module_relocate_rel(mod->base, opd,
                            opd->rel,
                            opd->rel + opd->relsz / opd->relent);
    }
    if (opd->rela != NULL) {
        module_relocate_rela(mod->base, opd,
                             opd->rela,
                             opd->rela + opd->relasz / opd->relaent);
    }
    if (opd->jmprel != NULL) {
        if (opd->pltrel == DT_REL) {
            module_relocate_rel(mod->base, opd, (ELF_REL_TYPE *)opd->jmprel, 
                                (ELF_REL_TYPE *)(opd->jmprel + opd->pltrelsz));
        } else if (opd->pltrel == DT_RELA) {
            module_relocate_rela(mod->base, opd, (ELF_RELA_TYPE *)opd->jmprel,
                                 (ELF_RELA_TYPE *)(opd->jmprel + opd->pltrelsz));
        }
    }
    /* special handling on I/O file */
    if (strstr(mod->name, "libc.so") == mod->name) {
        privmod_stdout = 
            (struct _IO_FILE **)get_proc_address_from_os_data(&opd->os_data,
                                                              opd->load_delta,
                                                              LIBC_STDOUT_NAME,
                                                              NULL);
        privmod_stdin = 
            (struct _IO_FILE **)get_proc_address_from_os_data(&opd->os_data,
                                                              opd->load_delta,
                                                              LIBC_STDIN_NAME,
                                                              NULL);
        privmod_stderr = 
            (struct _IO_FILE **)get_proc_address_from_os_data(&opd->os_data,
                                                              opd->load_delta,
                                                              LIBC_STDERR_NAME,
                                                              NULL);
    }
}

static void
privload_create_os_privmod_data(privmod_t *privmod)
{
    os_privmod_data_t *opd;
    app_pc out_base, out_end;

    opd = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, os_privmod_data_t,
                          ACCT_OTHER, PROTECTED);
    privmod->os_privmod_data = opd;
    memset(opd, 0, sizeof(*opd));

    /* walk the module's program header to get privmod information */
    module_walk_program_headers(privmod->base, privmod->size, false,
                                &out_base, &out_end, &opd->soname,
                                &opd->os_data);
    module_get_os_privmod_data(privmod->base, privmod->size, opd);
}

static void
privload_delete_os_privmod_data(privmod_t *privmod)
{
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, privmod->os_privmod_data,
                   os_privmod_data_t,
                   ACCT_OTHER, PROTECTED);
    privmod->os_privmod_data = NULL;
}


/****************************************************************************
 *                  Thread Local Storage Handling Code                      *
 ****************************************************************************/

/* The description of Linux Thread Local Storage Implementation on x86 arch 
 * Following description is based on the understanding of glibc-2.11.2 code 
 */
/* TLS is achieved via memory reference using segment register on x86.
 * Each thread has its own memory segment whose base is pointed by [%seg:0x0],
 * so different thread can access thread private memory via the same memory 
 * reference opnd [%seg:offset]. 
 */
/* In Linux, FS and GS are used for TLS reference. 
 * In current Linux libc implementation, %gs/%fs is used for TLS access
 * in 32/64-bit x86 architecture, respectively.
 */
/* TCB (thread control block) is a data structure to describe the thread
 * information. which is actually struct pthread in x86 Linux.
 * In x86 arch, [%seg:0x0] is used as TP (thread pointer) pointing to
 * the TCB. Instead of allocating modules' TLS after TCB,
 * they are put before the TCB, which allows TCB to have any size.
 * Using [%seg:0x0] as the TP, all modules' static TLS are accessed
 * via negative offsets, and TCB fields are accessed via positive offsets.
 */
/* There are two possible TLS memory, static TLS and dynamic TLS.
 * Static TLS is the memory allocated in the TLS segment, and can be accessed
 * via direct [%seg:offset].
 * Dynamic TLS is the memory allocated dynamically when the process 
 * dynamically loads a shared library (e.g. via dl_open), which has its own TLS 
 * but cannot fit into the TLS segment created at beginning.
 * 
 * DTV (dynamic thread vector) is the data structure used to maintain and 
 * reference those modules' TLS. 
 * Each module has a id, which is the index into the DTV to check 
 * whether its tls is static or dynamic, and where it is.
 */

/* The maxium number modules that we support to have TLS here.
 * Because any libraries having __thread variable will have tls segment.
 * we pick 64 and hope it is large enough. 
 */
#define MAX_NUM_TLS_MOD 64
typedef struct _tls_info_t {
    uint num_mods;
    int  offset;
    int  max_align;
    int  offs[MAX_NUM_TLS_MOD];
    privmod_t *mods[MAX_NUM_TLS_MOD];
} tls_info_t;
static tls_info_t tls_info;

/* The actual tcb size is the size of struct pthread from nptl/descr.h, which is
 * a glibc internal header that we can't include.  We hardcode a guess for the
 * tcb size, and try to recover if we guessed too large.  This value was
 * recalculated by building glibc and printing sizeof(struct pthread) from
 * _dl_start() in elf/rtld.c.  The value can also be determined from the
 * assembly of _dl_allocate_tls_storage() in ld.so:
 * Dump of assembler code for function _dl_allocate_tls_storage:
 *    0x00007ffff7def0a0 <+0>:  push   %r12
 *    0x00007ffff7def0a2 <+2>:  mov    0x20eeb7(%rip),%rdi # _dl_tls_static_align
 *    0x00007ffff7def0a9 <+9>:  push   %rbp
 *    0x00007ffff7def0aa <+10>: push   %rbx
 *    0x00007ffff7def0ab <+11>: mov    0x20ee9e(%rip),%rbx # _dl_tls_static_size
 *    0x00007ffff7def0b2 <+18>: mov    %rbx,%rsi
 *    0x00007ffff7def0b5 <+21>: callq  0x7ffff7ddda88 <__libc_memalign@plt>
 * => 0x00007ffff7def0ba <+26>: test   %rax,%rax
 *    0x00007ffff7def0bd <+29>: mov    %rax,%rbp
 *    0x00007ffff7def0c0 <+32>: je     0x7ffff7def180 <_dl_allocate_tls_storage+224>
 *    0x00007ffff7def0c6 <+38>: lea    -0x900(%rax,%rbx,1),%rbx
 *    0x00007ffff7def0ce <+46>: mov    $0x900,%edx
 * This is typically an allocation larger than 4096 bytes aligned to 64 bytes.
 * The "lea -0x900(%rax,%rbx,1),%rbx" instruction computes the thread pointer to
 * install.  The allocator used by the loader has no headers, so we don't have a
 * good way to guess how big this allocation was.  Instead we use this estimate.
 */
static size_t tcb_size = IF_X64_ELSE(0x900, 0x490);

/* thread control block header type from 
 * nptl/sysdeps/x86_64/tls.h and nptl/sysdeps/i386/tls.h 
 */
typedef struct _tcb_head_t {
    void *tcb;
    void *dtv;
    void *self;
    int multithread;
#ifdef X64
    int gscope_flag;
#endif
    ptr_uint_t sysinfo;
    /* Later fields are copied verbatim. */

    ptr_uint_t stack_guard;
    ptr_uint_t pointer_guard;
} tcb_head_t;

/* An estimate of the size of the static TLS data before the thread pointer that
 * we need to copy on behalf of libc.  When loading modules that have variables
 * stored in static TLS space, the loader stores them prior to the thread
 * pointer and lets the app intialize them.  Until we stop using the app's libc
 * (i#46), we need to copy this data from before the thread pointer.
 */
#define APP_LIBC_TLS_SIZE 0x400

/* FIXME: add description here to talk how TLS is setup. */
static void
privload_mod_tls_init(privmod_t *mod)
{
    os_privmod_data_t *opd;
    size_t offset;
    int first_byte;

    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    opd = (os_privmod_data_t *) mod->os_privmod_data;
    ASSERT(opd != NULL && opd->tls_block_size != 0);
    if (tls_info.num_mods >= MAX_NUM_TLS_MOD) {
        CLIENT_ASSERT(false, "Max number of modules with tls variables reached");
        FATAL_USAGE_ERROR(TOO_MANY_TLS_MODS, 2,
                          get_application_name(), get_application_pid());
    }
    tls_info.mods[tls_info.num_mods] = mod;
    opd->tls_modid = tls_info.num_mods;
    offset = (opd->tls_modid == 0) ? APP_LIBC_TLS_SIZE : tls_info.offset;
    /* decide the offset of each module in the TLS segment from 
     * thread pointer.  
     * Because the tls memory is located before thread pointer, we use
     * [tp - offset] to get the tls block for each module later.
     * so the first_byte that obey the alignment is calculated by
     * -opd->tls_first_byte & (opd->tls_align - 1);
     */
    first_byte = -opd->tls_first_byte & (opd->tls_align - 1);
    /* increase offset size by adding current mod's tls size:
     * 1. increase the tls_block_size with the right alignment
     *    using ALIGN_FORWARD()
     * 2. add first_byte to make the first byte with right alighment.
     */
    offset = first_byte + 
        ALIGN_FORWARD(offset + opd->tls_block_size + first_byte,
                      opd->tls_align);
    opd->tls_offset = offset;
    tls_info.offs[tls_info.num_mods] = offset;
    tls_info.offset = offset;
    tls_info.num_mods++;
    if (opd->tls_align > tls_info.max_align) {
        tls_info.max_align = opd->tls_align;
    }
}

void *
privload_tls_init(void *app_tp)
{
    app_pc dr_tp;
    tcb_head_t *dr_tcb;
    uint i;
    size_t tls_bytes_read;

    /* FIXME: These should be a thread logs, but dcontext is not ready yet. */
    LOG(GLOBAL, LOG_LOADER, 2, "%s: app TLS segment base is "PFX"\n",
        __FUNCTION__, app_tp);
    if (app_tp == NULL) {
        LOG(GLOBAL, LOG_LOADER, 2, "%s: no app TLS, skipping private lib TLS\n",
            __FUNCTION__, app_tp);
        return NULL;
    }
    dr_tp = heap_mmap(max_client_tls_size);
    LOG(GLOBAL, LOG_LOADER, 2, "%s: allocates %d at "PFX"\n",
        __FUNCTION__, max_client_tls_size, dr_tp);
    dr_tp = dr_tp + max_client_tls_size - tcb_size;
    dr_tcb = (tcb_head_t *) dr_tp;
    LOG(GLOBAL, LOG_LOADER, 2, "%s: adjust thread pointer to "PFX"\n",
        __FUNCTION__, dr_tp);
    /* We copy the whole tcb to avoid initializing it by ourselves. 
     * and update some fields accordingly.
     */
    /* DynamoRIO shares the same libc with the application, 
     * so as the tls used by libc. Thus we need duplicate
     * those tls with the same offset after switch the segment.
     * This copy can be avoided if we remove the DR's dependency on
     * libc. 
     */
    if (!safe_read_ex(app_tp - APP_LIBC_TLS_SIZE, APP_LIBC_TLS_SIZE + tcb_size,
                      dr_tp  - APP_LIBC_TLS_SIZE, &tls_bytes_read)) {
        LOG(GLOBAL, LOG_LOADER, 2, "%s: read failed, tcb was 0x%lx bytes "
            "instead of 0x%lx\n", __FUNCTION__, tls_bytes_read -
            APP_LIBC_TLS_SIZE, tcb_size);
    }
    /* We do not assert or warn on a truncated read as it does happen when TCB
     * + our over-estimate crosses a page boundary (our estimate is for latest
     * libc and is larger than on older libc versions): i#855.
     */
    ASSERT(tls_info.offset <= max_client_tls_size - tcb_size);
    /* Update two self pointers. */
    dr_tcb->tcb  = dr_tcb;
    dr_tcb->self = dr_tcb;
    /* i#555: replace app's vsyscall with DR's int0x80 syscall */
    dr_tcb->sysinfo = (ptr_uint_t)client_int_syscall;

    for (i = 0; i < tls_info.num_mods; i++) {
        os_privmod_data_t *opd = tls_info.mods[i]->os_privmod_data;
        void *dest;
        /* now copy the tls memory from the image */
        dest = dr_tp - tls_info.offs[i];
        memcpy(dest, opd->tls_image, opd->tls_image_size);
        /* set all 0 to the rest of memory. 
         * tls_block_size refers to the size in memory, and
         * tls_image_size refers to the size in file. 
         * We use the same way as libc to name them. 
         */
        ASSERT(opd->tls_block_size >= opd->tls_image_size);
        memset(dest + opd->tls_image_size, 0, 
               opd->tls_block_size - opd->tls_image_size);
    }
    return dr_tp;
}

void
privload_tls_exit(void *dr_tp)
{
    if (dr_tp == NULL)
        return;
    dr_tp = dr_tp + tcb_size - max_client_tls_size;
    heap_munmap(dr_tp, max_client_tls_size);
}

/****************************************************************************
 *                         Function Redirection Code                        *
 ****************************************************************************/

/* We did not create dtv, so we need redirect tls_get_addr */
typedef struct _tls_index_t {
  unsigned long int ti_module;
  unsigned long int ti_offset;
} tls_index_t;

static void *
redirect___tls_get_addr(tls_index_t *ti)
{
    LOG(GLOBAL, LOG_LOADER, 4, "__tls_get_addr: module: %d, offset: %d\n",
        ti->ti_module, ti->ti_offset);
    ASSERT(ti->ti_module < tls_info.num_mods);
    return (os_get_dr_seg_base(NULL, LIB_SEG_TLS) - 
            tls_info.offs[ti->ti_module] + ti->ti_offset);
}

static void *
redirect____tls_get_addr()
{
    tls_index_t *ti;
    /* XXX: in some version of ___tls_get_addr, ti is passed via xax 
     * How can I generalize it?
     */
    asm("mov %%"ASM_XAX", %0" : "=m"((ti)) : : ASM_XAX);
    LOG(GLOBAL, LOG_LOADER, 4, "__tls_get_addr: module: %d, offset: %d\n",
        ti->ti_module, ti->ti_offset);
    ASSERT(ti->ti_module < tls_info.num_mods);
    return (os_get_dr_seg_base(NULL, LIB_SEG_TLS) - 
            tls_info.offs[ti->ti_module] + ti->ti_offset);
}

typedef struct _redirect_import_t {
    const char *name;
    app_pc func;
} redirect_import_t;

static const redirect_import_t redirect_imports[] = {
    {"calloc",  (app_pc)redirect_calloc},
    {"malloc",  (app_pc)redirect_malloc},
    {"free",    (app_pc)redirect_free},
    {"realloc", (app_pc)redirect_realloc},
    /* FIXME: we should also redirect functions including:
     * malloc_usable_size, memalign, valloc, mallinfo, mallopt, etc.
     * Any other functions need to be redirected?
     */
    {"__tls_get_addr", (app_pc)redirect___tls_get_addr},
    {"___tls_get_addr", (app_pc)redirect____tls_get_addr},
};
#define REDIRECT_IMPORTS_NUM (sizeof(redirect_imports)/sizeof(redirect_imports[0]))

bool
privload_redirect_sym(ELF_ADDR *r_addr, const char *name)
{
    int i;
    /* iterate over all symbols and redirect syms when necessary, e.g. malloc */
    for (i = 0; i < REDIRECT_IMPORTS_NUM; i++) {
        if (strcmp(redirect_imports[i].name, name) == 0) {
            *r_addr = (ELF_ADDR)redirect_imports[i].func;
            return true;;
        }
    }
    return false;
}

/***************************************************************************
 * DynamoRIO Early Inection Code
 */
#define MAX_NUM_ARGS 0x100
static reg_t  app_init_xsp = 0;
static app_pc app_init_entry = NULL;

static void
privload_get_init_app_xsp(void)
{
    char *user = getenv("USER") - 5 /* "USER=..." */;
    IF_DEBUG(int num_args = 0;)
    reg_t *xsp = (reg_t *)ALIGN_BACKWARD(user, XSP_SZ);
    /* reverse scan to find the args and env */
    /* scan stack till xsp pointing to "USER=..." */
    for (; *xsp != (reg_t)user; xsp--);
    /* scan stack till xsp pointing to the NULL between argv and envp */
    for (; *xsp != 0; xsp--);
    /* scan stack till xsp pointing to argc */
    for (; *xsp <= 0 || *xsp > MAX_NUM_ARGS; xsp--)
        IF_DEBUG(num_args++);
    ASSERT((num_args-1) == *(int *)xsp);
    app_init_xsp = (reg_t)xsp;
}

static char *
privload_setup_app_stack(void)
{
    int *argc = 0;
    char **argv = NULL;
    int i;
    privload_get_init_app_xsp();
    ASSERT(app_init_xsp != 0);
    argc = (int *)app_init_xsp;
    argv = (char **)app_init_xsp + 1 /* argc */;
    ASSERT(*argc > 0 && argv != NULL);
    /* shift argv */
    for (i = 0; i < (*argc - 1); i++) {
        strcpy(argv[i], argv[i+1]);
        argv[i+1] = argv[i] + strlen(argv[i]) + 1;
    }
    memset(argv[i], 0, sizeof(argv[i]));
    /* change argc */
    *argc = *argc - 1;
    return argv[0];
}

void
privload_setup_app_mc(priv_mcontext_t *mc)
{
    memset(mc, 0, sizeof(*mc));
    mc->xip = app_init_entry;
    mc->xflags = 0x200; /* IF */
    ASSERT(app_init_xsp != 0);
    mc->xsp = app_init_xsp;
}

bool
privload_early_inject(void)
{
    size_t size;
    void *os_privmod_data;
    app_pc map;
    char *app_name;
    char *interp = NULL;

    app_name = privload_setup_app_stack();
    ASSERT(app_name != NULL);
    LOG(GLOBAL, LOG_LOADER, 2, "early_inject: load app %s\n", app_name);
    acquire_recursive_lock(&privload_lock);
    map = privload_map_and_relocate(app_name, &size, &os_privmod_data, true,
                                    &app_init_entry, &interp);
    if (map == NULL)
        apicheck(false, "Failed to load application.  Check path and architecture.");
    if (interp != NULL) {
        map = privload_map_and_relocate(interp, &size, &os_privmod_data,
                                        false /* fixed */, &app_init_entry,
                                        NULL /* interp */);
        ASSERT(map != NULL);
        /* FIXME i#47: more work than static linked executables */
        apicheck(false, "This -early prototype does not support dynamically linked executables.  Please re-run without -early.");
        ASSERT_NOT_IMPLEMENTED(false);
    }
    release_recursive_lock(&privload_lock);
    return false; /* Not complete yet */
}
