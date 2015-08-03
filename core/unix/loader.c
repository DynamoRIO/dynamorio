/* *******************************************************************************
 * Copyright (c) 2011-2015 Google, Inc.  All rights reserved.
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
#include "../arch/instr.h" /* SEG_GS/SEG_FS */
#include "module.h"
#include "module_private.h"
#include "../heap.h"    /* HEAPACCT */
#ifdef LINUX
# include "include/syscall.h"
# include "memquery.h"
# define _GNU_SOURCE 1
# define __USE_GNU 1
# include <link.h> /* struct dl_phdr_info, must be prior to dlfcn.h */
#else
# include <sys/syscall.h>
#endif
#include "tls.h"

#include <dlfcn.h>      /* dlsym */
#ifdef LINUX
# include <sys/prctl.h>  /* PR_SET_NAME */
#endif
#include <string.h>     /* strcmp */
#include <stdlib.h>     /* getenv */
#include <dlfcn.h>      /* dlopen/dlsym */
#include <unistd.h>     /* __environ */
#include <stddef.h>     /* offsetof */

extern size_t wcslen(const wchar_t *str); /* in string.c */

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
#ifdef ANDROID
    "/system/lib",
#endif
#ifndef X64
    "/usr/lib32",
    "/lib32",
# ifdef X86
    "/lib32/tls/i686/cmov",
    /* 32-bit Ubuntu */
    "/lib/i386-linux-gnu",
    "/usr/lib/i386-linux-gnu",
# elif defined(ARM)
    "/lib/arm-linux-gnueabihf",
    "/usr/lib/arm-linux-gnueabihf",
# endif
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

#define RPATH_ORIGIN "$ORIGIN"

#define APP_BRK_GAP 64*1024*1024

static os_privmod_data_t *libdr_opd;
static bool privmod_initialized = false;
static size_t max_client_tls_size = 2 * PAGE_SIZE;

#ifdef LINUX /* XXX i#1285: implement MacOS private loader */
# if defined(INTERNAL) || defined(CLIENT_INTERFACE)
static bool printed_gdb_commands = false;
/* Global so visible in release build gdb */
static char gdb_priv_cmds[4096];
static size_t gdb_priv_cmds_sofar;
# endif
#endif

/* pointing to the I/O data structure in privately loaded libc,
 * They are used on exit when we need update file_no.
 */
stdfile_t **privmod_stdout;
stdfile_t **privmod_stderr;
stdfile_t **privmod_stdin;
#define LIBC_STDOUT_NAME "stdout"
#define LIBC_STDERR_NAME "stderr"
#define LIBC_STDIN_NAME  "stdin"

/* forward decls */

static void
privload_init_search_paths(void);

static bool
privload_locate(const char *name, privmod_t *dep, char *filename OUT, bool *client OUT);

static privmod_t *
privload_locate_and_load(const char *impname, privmod_t *dependent, bool reachable);

static void
privload_call_modules_entry(privmod_t *mod, uint reason);

static void
privload_call_lib_func(fp_t func);

static void
privload_relocate_mod(privmod_t *mod);

static void
privload_create_os_privmod_data(privmod_t *privmod, bool dyn_reloc);

static void
privload_delete_os_privmod_data(privmod_t *privmod);

#ifdef LINUX
static void
privload_mod_tls_init(privmod_t *mod);
#endif

/***************************************************************************/

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

#ifdef LINUX /* XXX i#1285: implement MacOS private loader */
# if defined(INTERNAL) || defined(CLIENT_INTERFACE)
static void
privload_add_gdb_cmd(elf_loader_t *loader, const char *filename, bool reachable)
{
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    /* Get the text addr to register the ELF with gdb.  The section headers
     * are not part of the mapped image, so we have to map the whole file.
     * XXX: seek to e_shoff and read the section headers to avoid this map.
     */
    if (elf_loader_map_file(loader, reachable) != NULL) {
        app_pc text_addr = (app_pc)module_get_text_section(loader->file_map,
                                                           loader->file_size);
        text_addr += loader->load_delta;
        print_to_buffer(gdb_priv_cmds, BUFFER_SIZE_ELEMENTS(gdb_priv_cmds),
                        &gdb_priv_cmds_sofar, "add-symbol-file '%s' %p\n",
                        filename, text_addr);
        /* Add debugging comment about how to get symbol information in gdb. */
        if (printed_gdb_commands) {
            /* This is a dynamically loaded auxlib, so we print here.
             * The client and its direct dependencies are batched up and
             * printed in os_loader_init_epilogue.
             */
            SYSLOG_INTERNAL_INFO("Paste into GDB to debug DynamoRIO clients:\n"
                                 "add-symbol-file '%s' %p\n",
                                 filename, text_addr);
        }
        LOG(GLOBAL, LOG_LOADER, 1,
            "for debugger: add-symbol-file %s %p\n", filename, text_addr);
        if (IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(privload_register_gdb), false)) {
            dr_gdb_add_symbol_file(filename, text_addr);
        }
    }
}
# endif
#endif

/* os specific loader initialization prologue before finalizing the load. */
void
os_loader_init_prologue(void)
{
#ifndef STATIC_LIBRARY
    privmod_t *mod;
#endif

    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);

    privload_init_search_paths();
#ifndef STATIC_LIBRARY
    /* insert libdynamorio.so */
    mod = privload_insert(NULL,
                          get_dynamorio_dll_start(),
                          get_dynamorio_dll_end() - get_dynamorio_dll_start(),
                          get_shared_lib_name(get_dynamorio_dll_start()),
                          get_dynamorio_library_path());
    ASSERT(mod != NULL);
    /* If DR was loaded by system ld.so, then .dynamic *was* relocated (i#1589) */
    privload_create_os_privmod_data(mod, !DYNAMO_OPTION(early_inject));
    libdr_opd = (os_privmod_data_t *) mod->os_privmod_data;
    if (DYNAMO_OPTION(early_inject)) {
        /* i#1659: fill in the text-data segment gap to ensure no mmaps in between.
         * The kernel does not do this.  Our private loader does, so if we reloaded
         * ourselves this is already in place, but it's expensive to query so
         * we blindly clobber w/ another no-access mapping.
         */
        int i;
        for (i = 0; i <libdr_opd->os_data.num_segments - 1; i++) {
            size_t sz = libdr_opd->os_data.segments[i+1].start -
                libdr_opd->os_data.segments[i].end;
            if (sz > 0) {
                DEBUG_DECLARE(byte *fill =)
                    os_map_file(-1, &sz, 0, libdr_opd->os_data.segments[i].end,
                                MEMPROT_NONE, MAP_FILE_COPY_ON_WRITE|MAP_FILE_FIXED);
                ASSERT(fill != NULL);
            }
        }
    }
    mod->externally_loaded = true;
# if defined(LINUX)/*i#1285*/ && (defined(INTERNAL) || defined(CLIENT_INTERFACE))
    if (DYNAMO_OPTION(early_inject)) {
        /* libdynamorio isn't visible to gdb so add to the cmd list */
        elf_loader_t dr_ld;
        IF_DEBUG(bool success = )
            elf_loader_read_headers(&dr_ld, get_dynamorio_library_path());
        ASSERT(success);
        privload_add_gdb_cmd(&dr_ld, get_dynamorio_library_path(), false/*!reach*/);
        elf_loader_destroy(&dr_ld);
    }
# endif
#endif
}

/* os specific loader initialization epilogue after finalizing the load. */
void
os_loader_init_epilogue(void)
{
#ifdef LINUX /* XXX i#1285: implement MacOS private loader */
# if defined(INTERNAL) || defined(CLIENT_INTERFACE)
    /* Print the add-symbol-file commands so they can be copy-pasted into gdb.
     * We have to do it in a single syslog so they can be copy pasted.
     * For non-internal builds, or for private libs loaded after this point,
     * the user must look at the global gdb_priv_cmds buffer in gdb.
     * FIXME i#531: Support attaching from the gdb script.
     */
    ASSERT(!printed_gdb_commands);
    printed_gdb_commands = true;
    if (gdb_priv_cmds_sofar > 0) {
        SYSLOG_INTERNAL_INFO("Paste into GDB to debug DynamoRIO clients:\n"
                             /* Need to turn off confirm for paste to work. */
                             "set confirm off\n"
                             "%s", gdb_priv_cmds);
    }
# endif /* INTERNAL || CLIENT_INTERFACE */
#endif
}

void
os_loader_exit(void)
{
    if (libdr_opd != NULL) {
        HEAP_ARRAY_FREE(GLOBAL_DCONTEXT,
                        libdr_opd->os_data.segments,
                        module_segment_t,
                        libdr_opd->os_data.alloc_segments,
                        ACCT_OTHER, PROTECTED);
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, libdr_opd,
                       os_privmod_data_t, ACCT_OTHER, PROTECTED);
    }
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

    /* create and init the os_privmod_data for privmod.
     * The os_privmod_data can only be created after heap is ready and
     * should be done before adding in vmvector_add,
     * so it can be either right before calling to privload_add_areas
     * in the privload_load_finalize, or in here.
     * We prefer here because it avoids changing the code in
     * loader_shared.c, which affects windows too.
      */
    privload_create_os_privmod_data(privmod,  false/* i#1589: .dynamic not relocated */);
    opd = (os_privmod_data_t *) privmod->os_privmod_data;
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

/* This only maps, as relocation for ELF requires processing imports first,
 * which we have to delay at init time at least.
 */
app_pc
privload_map_and_relocate(const char *filename, size_t *size OUT, bool reachable)
{
#ifdef LINUX
    map_fn_t map_func;
    unmap_fn_t unmap_func;
    prot_fn_t prot_func;
    app_pc base = NULL;
    elf_loader_t loader;

    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    /* get appropriate function */
    /* NOTE: all but the client lib will be added to DR areas list b/c using
     * map_file()
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

    if (!elf_loader_read_headers(&loader, filename)) {
        /* We may want to move the bitwidth check out if is_elf_so_header_common()
         * but for now we keep that there and do another check here.
         * If loader.buf was not read into it will be all zeroes.
         */
        ELF_HEADER_TYPE *elf_header = (ELF_HEADER_TYPE *) loader.buf;
        ELF_ALTARCH_HEADER_TYPE *altarch = (ELF_ALTARCH_HEADER_TYPE *) elf_header;
        if (elf_header->e_version == 1 &&
            altarch->e_ehsize == sizeof(ELF_ALTARCH_HEADER_TYPE) &&
            altarch->e_machine == IF_X64_ELSE(EM_386, EM_X86_64)) {
            SYSLOG(SYSLOG_ERROR, CLIENT_LIBRARY_WRONG_BITWIDTH, 3,
                   get_application_name(), get_application_pid(), filename);
        }
        return NULL;
    }

    base = elf_loader_map_phdrs(&loader, false /* fixed */, map_func,
                                unmap_func, prot_func, reachable);
    if (base != NULL) {
        if (size != NULL)
            *size = loader.image_size;

#if defined(INTERNAL) || defined(CLIENT_INTERFACE)
        privload_add_gdb_cmd(&loader, filename, reachable);
#endif
    }
    elf_loader_destroy(&loader);

    return base;
#else
    /* XXX i#1285: implement MacOS private loader */
    return NULL;
#endif
}

bool
privload_process_imports(privmod_t *mod)
{
#ifdef LINUX
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
            LOG(GLOBAL, LOG_LOADER, 2, "%s: %s imports from %s\n",
                __FUNCTION__, mod->name, name);
#ifdef ANDROID
            /* FIXME i#1701: support Android libc, which requires special init
             * from the loader.
             */
            CLIENT_ASSERT(strcmp(name, "libc.so") != 0,
                          "client using libc not yet supported on Android");
#endif
            if (privload_lookup(name) == NULL) {
                privmod_t *impmod = privload_locate_and_load(name, mod,
                                                             false/*client dir=>true*/);
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
    /* Relocate library's symbols after load dependent libraries (so that we
     * can resolve symbols in the global ELF namespace).
     */
    if (!mod->externally_loaded)
        privload_relocate_mod(mod);
    return true;
#else
    /* XXX i#1285: implement MacOS private loader */
    if (!mod->externally_loaded)
        privload_relocate_mod(mod);
    return false;
#endif
}

bool
privload_call_entry(privmod_t *privmod, uint reason)
{
    os_privmod_data_t *opd = (os_privmod_data_t *) privmod->os_privmod_data;
    if (os_get_priv_tls_base(NULL, TLS_REG_LIB) == NULL) {
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
                if (opd->init_array[i] != NULL) /* be paranoid */
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
                 i < opd->fini_arraysz / sizeof(opd->fini_array[0]);
                 i++) {
                if (opd->fini_array[i] != NULL) /* be paranoid */
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

void
privload_os_finalize(privmod_t *privmod_t)
{
    /* nothing */
}

static void
privload_init_search_paths(void)
{
    privload_add_drext_path();
    ld_library_path = getenv(SYSTEM_LIBRARY_PATH_VAR);
}

static privmod_t *
privload_locate_and_load(const char *impname, privmod_t *dependent, bool reachable)
{
    char filename[MAXIMUM_PATH];
    if (privload_locate(impname, dependent, filename, &reachable))
        return privload_load(filename, dependent, reachable);
    return NULL;
}

app_pc
privload_load_private_library(const char *name, bool reachable)
{
    privmod_t *newmod;
    app_pc res = NULL;
    acquire_recursive_lock(&privload_lock);
    newmod = privload_lookup(name);
    if (newmod == NULL)
        newmod = privload_locate_and_load(name, NULL, reachable);
    else
        newmod->ref_count++;
    if (newmod != NULL)
        res = newmod->base;
    release_recursive_lock(&privload_lock);
    return res;
}

void
privload_load_finalized(privmod_t *mod)
{
    /* nothing further to do */
}

static bool
privload_search_rpath(privmod_t *mod, const char *name,
                      char *filename OUT /* buffer size is MAXIMUM_PATH */)
{
#ifdef LINUX
    os_privmod_data_t *opd;
    ELF_DYNAMIC_ENTRY_TYPE *dyn;
    ASSERT(mod != NULL && "can't look for rpath without a dependent module");
    /* get the loading module's dir for RPATH_ORIGIN */
    opd = (os_privmod_data_t *) mod->os_privmod_data;
    const char *moddir_end = strrchr(mod->path, '/');
    size_t moddir_len = (moddir_end == NULL ? strlen(mod->path) : moddir_end - mod->path);
    const char *strtab;
    ASSERT(opd != NULL);
    dyn = (ELF_DYNAMIC_ENTRY_TYPE *) opd->dyn;
    strtab = (char *) opd->os_data.dynstr;
    /* support $ORIGIN expansion to lib's current directory */
    while (dyn->d_tag != DT_NULL) {
        /* FIXME i#460: we should also support DT_RUNPATH: if we see it,
         * ignore DT_RPATH and search DT_RUNPATH after LD_LIBRARY_PATH.
         */
        if (dyn->d_tag == DT_RPATH) {
            /* DT_RPATH is a colon-separated list of paths */
            const char *list = strtab + dyn->d_un.d_val;
            const char *sep, *origin;
            size_t len;
            while (*list != '\0') {
                /* really we want strchrnul() */
                sep = strchr(list, ':');
                if (sep == NULL)
                    len = strlen(list);
                else
                    len = sep - list;
                /* support $ORIGIN expansion to lib's current directory */
                origin = strstr(list, RPATH_ORIGIN);
                if (origin != NULL && origin < list + len) {
                    size_t pre_len = origin - list;
                    snprintf(filename, MAXIMUM_PATH, "%.*s%.*s%.*s/%s",
                             pre_len, list,
                             moddir_len, mod->path,
                             /* the '/' should already be here */
                             len - strlen(RPATH_ORIGIN) - pre_len,
                             origin + strlen(RPATH_ORIGIN),
                             name);
                } else {
                    snprintf(filename, MAXIMUM_PATH, "%.*s/%s", len, list, name);
                }
                filename[MAXIMUM_PATH - 1] = 0;
                LOG(GLOBAL, LOG_LOADER, 2, "%s: looking for %s\n",
                    __FUNCTION__, filename);
                if (os_file_exists(filename, false/*!is_dir*/) &&
                    module_file_has_module_header(filename)) {
                    return true;
                }
                list += len + 1;
            }
        }
        ++dyn;
    }
#else
    /* XXX i#1285: implement MacOS private loader */
#endif
    return false;
}

static bool
privload_locate(const char *name, privmod_t *dep,
                char *filename OUT /* buffer size is MAXIMUM_PATH */,
                bool *reachable INOUT)
{
    uint i;
    char *lib_paths;

    /* We may be passed a full path. */
    if (name[0] == '/' && os_file_exists(name, false/*!is_dir*/)) {
        snprintf(filename, MAXIMUM_PATH, "%s", name);
        filename[MAXIMUM_PATH - 1] = 0;
        return true;
    }

    /* FIXME: We have a simple implementation of library search.
     * libc implementation can be found at elf/dl-load.c:_dl_map_object.
     */
    /* the loader search order: */
    /* 0) DT_RPATH */
    if (dep != NULL && privload_search_rpath(dep, name, filename))
        return true;

    /* 1) client lib dir */
    for (i = 0; i < search_paths_idx; i++) {
        snprintf(filename, MAXIMUM_PATH, "%s/%s", search_paths[i], name);
        /* NULL_TERMINATE_BUFFER(filename) */
        filename[MAXIMUM_PATH - 1] = 0;
        LOG(GLOBAL, LOG_LOADER, 2, "%s: looking for %s\n",
            __FUNCTION__, filename);
        if (os_file_exists(filename, false/*!is_dir*/) &&
            module_file_has_module_header(filename)) {
            /* If in client or extension dir, always map it reachable */
            *reachable = true;
            return true;
        }
    }

    /* 2) curpath */
    snprintf(filename, MAXIMUM_PATH, "./%s", name);
    /* NULL_TERMINATE_BUFFER(filename) */
    filename[MAXIMUM_PATH - 1] = 0;
    LOG(GLOBAL, LOG_LOADER, 2, "%s: looking for %s\n", __FUNCTION__, filename);
    if (os_file_exists(filename, false/*!is_dir*/) &&
        module_file_has_module_header(filename))
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
            module_file_has_module_header(filename))
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
            module_file_has_module_header(filename))
            return true;
    }

    /* Cannot find the library */
    /* There's a syslog in loader_init() but we want to provide the lib name */
    SYSLOG(SYSLOG_ERROR, CLIENT_LIBRARY_UNLOADABLE, 4,
           get_application_name(), get_application_pid(), name,
           "\n\tUnable to locate library! Try adding path to LD_LIBRARY_PATH");
    return false;
}

#pragma weak dlsym

app_pc
get_private_library_address(app_pc modbase, const char *name)
{
#ifdef LINUX
    privmod_t *mod;
    app_pc res;

    acquire_recursive_lock(&privload_lock);
    mod = privload_lookup_by_base(modbase);
    if (mod == NULL || mod->externally_loaded) {
        release_recursive_lock(&privload_lock);
#ifdef STATIC_LIBRARY
        /* externally loaded, use dlsym instead */
        ASSERT(!DYNAMO_OPTION(early_inject));
        return dlsym(modbase, name);
#else
        /* Only libdynamorio.so is externally_loaded and we should not be querying
         * for it.  Unknown libs shouldn't be queried here: get_proc_address should
         * be used instead.
         */
        ASSERT_NOT_REACHED();
        return NULL;
#endif
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
        if (!module_read_os_data(mod->base,
                                 false /* .dynamic not relocated (i#1589) */,
                                 &delta, &os_data, &soname)) {
            release_recursive_lock(&privload_lock);
            return NULL;
        }
        res = get_proc_address_from_os_data(&os_data, delta, name, NULL);
        release_recursive_lock(&privload_lock);
        return res;
    }
    ASSERT_NOT_REACHED();
#else
    /* XXX i#1285: implement MacOS private loader */
#endif
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
    char dummy_str[] = "dummy";
    char *dummy_argv[2];
    /* FIXME: i#475
     * The regular loader always passes argc, argv and env to libaries,
     * (see libc code elf/dl-init.c), which might be ignored by those
     * routines.
     * we create dummy argc and argv, and passed with the real __environ.
     */
    dummy_argv[0] = dummy_str;
    dummy_argv[1] = NULL;
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

#ifdef LINUX
static void
privload_relocate_os_privmod_data(os_privmod_data_t *opd, byte *mod_base)
{
    if (opd->rel != NULL) {
        module_relocate_rel(mod_base, opd,
                            opd->rel,
                            opd->rel + opd->relsz / opd->relent);
    }
    if (opd->rela != NULL) {
        module_relocate_rela(mod_base, opd,
                             opd->rela,
                             opd->rela + opd->relasz / opd->relaent);
    }
    if (opd->jmprel != NULL) {
        if (opd->pltrel == DT_REL) {
            module_relocate_rel(mod_base, opd, (ELF_REL_TYPE *)opd->jmprel,
                                (ELF_REL_TYPE *)(opd->jmprel + opd->pltrelsz));
        } else if (opd->pltrel == DT_RELA) {
            module_relocate_rela(mod_base, opd, (ELF_RELA_TYPE *)opd->jmprel,
                                 (ELF_RELA_TYPE *)(opd->jmprel + opd->pltrelsz));
        }
    }
}
#endif

static void
privload_relocate_mod(privmod_t *mod)
{
#ifdef LINUX
    os_privmod_data_t *opd = (os_privmod_data_t *) mod->os_privmod_data;

    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);

    LOG(GLOBAL, LOG_LOADER, 3, "relocating %s\n", mod->name);

    /* If module has tls block need update its tls offset value */
    if (opd->tls_block_size != 0)
        privload_mod_tls_init(mod);

    privload_relocate_os_privmod_data(opd, mod->base);

    /* special handling on I/O file */
    if (strstr(mod->name, "libc.so") == mod->name) {
        privmod_stdout =
            (FILE **)get_proc_address_from_os_data(&opd->os_data,
                                                   opd->load_delta,
                                                   LIBC_STDOUT_NAME,
                                                   NULL);
        privmod_stdin =
            (FILE **)get_proc_address_from_os_data(&opd->os_data,
                                                   opd->load_delta,
                                                   LIBC_STDIN_NAME,
                                                   NULL);
        privmod_stderr =
            (FILE **)get_proc_address_from_os_data(&opd->os_data,
                                                   opd->load_delta,
                                                   LIBC_STDERR_NAME,
                                                   NULL);
    }
#else
    /* XXX i#1285: implement MacOS private loader */
#endif
}

static void
privload_create_os_privmod_data(privmod_t *privmod, bool dyn_reloc)
{
    os_privmod_data_t *opd;

    opd = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, os_privmod_data_t,
                          ACCT_OTHER, PROTECTED);
    privmod->os_privmod_data = opd;

    memset(opd, 0, sizeof(*opd));

    /* walk the module's program header to get privmod information */
    module_walk_program_headers(privmod->base, privmod->size,
                                false, /* segments are remapped */
                                dyn_reloc,
                                &opd->os_data.base_address, NULL,
                                &opd->max_end, &opd->soname,
                                &opd->os_data);
    module_get_os_privmod_data(privmod->base, privmod->size,
                               false/*!relocated*/, opd);
}

static void
privload_delete_os_privmod_data(privmod_t *privmod)
{
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, privmod->os_privmod_data,
                   os_privmod_data_t,
                   ACCT_OTHER, PROTECTED);
    privmod->os_privmod_data = NULL;
}

/* i#1589: the client lib is already on the priv lib list, so we share its
 * data with loaded_module_areas (which also avoids problems with .dynamic
 * not being relocated for priv libs).
 */
bool
privload_fill_os_module_info(app_pc base,
                             OUT app_pc *out_base /* relative pc */,
                             OUT app_pc *out_max_end /* relative pc */,
                             OUT char **out_soname,
                             OUT os_module_data_t *out_data)
{
    bool res = false;
    privmod_t *privmod;
    acquire_recursive_lock(&privload_lock);
    privmod = privload_lookup_by_base(base);
    if (privmod != NULL) {
        os_privmod_data_t *opd = (os_privmod_data_t *) privmod->os_privmod_data;
        if (out_base != NULL)
            *out_base = opd->os_data.base_address;
        if (out_max_end != NULL)
            *out_max_end = opd->max_end;
        if (out_soname != NULL)
            *out_soname = opd->soname;
        if (out_data != NULL)
            module_copy_os_data(out_data, &opd->os_data);
        res = true;
    }
    release_recursive_lock(&privload_lock);
    return res;
}

/****************************************************************************
 *                  Thread Local Storage Handling Code                      *
 ****************************************************************************/

/* XXX i#1285: implement TLS for MacOS private loader */

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
/* On A32, the pthread is put before tcbhead instead tcbhead being part of pthread */
static size_t tcb_size = IF_X86_ELSE(IF_X64_ELSE(0x900, 0x490), 0x40);

/* thread contol block header type from
 * - sysdeps/x86_64/nptl/tls.h
 * - sysdeps/i386/nptl/tls.h
 * - sysdeps/arm/nptl/tls.h
 */
typedef struct _tcb_head_t {
#ifdef X86
    void *tcb;
    void *dtv;
    void *self;
    int multithread;
# ifdef X64
    int gscope_flag;
# endif
    ptr_uint_t sysinfo;
    /* Later fields are copied verbatim. */

    ptr_uint_t stack_guard;
    ptr_uint_t pointer_guard;
#elif defined(ARM)
# ifdef X64
#  error NYI on AArch64
# else
    void *dtv;
    void *private;
    byte  padding[2]; /* make it 16-byte align */
# endif
#endif /* X86/ARM */
} tcb_head_t;

#ifdef X86
# define TLS_PRE_TCB_SIZE 0
#elif defined(ARM)
# ifdef X64
#  error NYI on AArch64
# else
/* Data structure to match libc pthread.
 * GDB reads some slot in TLS, which is pid/tid of pthread, so we must make sure
 * the size and member locations match to avoid gdb crash.
 */
typedef struct _dr_pthread_t {
    byte data1[0x68];  /* # of bytes before tid within pthread */
    process_id_t tid;
    thread_id_t pid;
    byte data2[0x450]; /* # of bytes after pid within pthread */
} dr_pthread_t;
#  define TLS_PRE_TCB_SIZE sizeof(dr_pthread_t)
#  define LIBC_PTHREAD_SIZE 0x4c0
#  define LIBC_PTHREAD_TID_OFFSET 0x68
# endif
#endif /* X86/ARM */

#ifdef X86
/* An estimate of the size of the static TLS data before the thread pointer that
 * we need to copy on behalf of libc.  When loading modules that have variables
 * stored in static TLS space, the loader stores them prior to the thread
 * pointer and lets the app intialize them.  Until we stop using the app's libc
 * (i#46), we need to copy this data from before the thread pointer.
 */
# define APP_LIBC_TLS_SIZE 0x400
#elif defined(ARM)
/* FIXME i#1551: investigate the difference between ARM and X86 on TLS.
 * On ARM, it seems that TLS variables are not put before the thread pointer
 * as they are on X86.
 */
# define APP_LIBC_TLS_SIZE 0
#endif

#ifdef LINUX /* XXX i#1285: implement MacOS private loader */
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
#endif

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
    dr_tp = heap_mmap(max_client_tls_size);
    ASSERT(APP_LIBC_TLS_SIZE + TLS_PRE_TCB_SIZE + tcb_size <= max_client_tls_size);
#ifdef ARM
    /* GDB reads some pthread members (e.g., pid, tid), so we must make sure
     * the size and member locations match to avoid gdb crash.
     */
    ASSERT(TLS_PRE_TCB_SIZE == LIBC_PTHREAD_SIZE);
    ASSERT(LIBC_PTHREAD_TID_OFFSET == offsetof(dr_pthread_t, tid));
#endif
    LOG(GLOBAL, LOG_LOADER, 2, "%s: allocated %d at "PFX"\n",
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
    if (app_tp != NULL &&
        !safe_read_ex(app_tp - APP_LIBC_TLS_SIZE - TLS_PRE_TCB_SIZE,
                      APP_LIBC_TLS_SIZE + TLS_PRE_TCB_SIZE + tcb_size,
                      dr_tp  - APP_LIBC_TLS_SIZE - TLS_PRE_TCB_SIZE,
                      &tls_bytes_read)) {
        LOG(GLOBAL, LOG_LOADER, 2, "%s: read failed, tcb was 0x%lx bytes "
            "instead of 0x%lx\n", __FUNCTION__, tls_bytes_read -
            APP_LIBC_TLS_SIZE, tcb_size);
#ifdef ARM
    } else {
        dr_pthread_t *dp =
            (dr_pthread_t *)(dr_tp - APP_LIBC_TLS_SIZE - TLS_PRE_TCB_SIZE);
        dp->pid = get_process_id();
        dp->tid = get_sys_thread_id();
#endif
    }
    /* We do not assert or warn on a truncated read as it does happen when TCB
     * + our over-estimate crosses a page boundary (our estimate is for latest
     * libc and is larger than on older libc versions): i#855.
     */
    ASSERT(tls_info.offset <= max_client_tls_size - TLS_PRE_TCB_SIZE - tcb_size);
#ifdef X86
    /* Update two self pointers. */
    dr_tcb->tcb  = dr_tcb;
    dr_tcb->self = dr_tcb;
    /* i#555: replace app's vsyscall with DR's int0x80 syscall */
    dr_tcb->sysinfo = (ptr_uint_t)client_int_syscall;
#elif defined(ARM)
    dr_tcb->dtv = NULL;
    dr_tcb->private = NULL;
#endif

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
    return (os_get_priv_tls_base(NULL, TLS_REG_LIB) -
            tls_info.offs[ti->ti_module] + ti->ti_offset);
}

static void *
redirect____tls_get_addr()
{
    tls_index_t *ti;
    /* XXX: in some version of ___tls_get_addr, ti is passed via xax
     * How can I generalize it?
     */
#ifdef X86
    asm("mov %%"ASM_XAX", %0" : "=m"((ti)) : : ASM_XAX);
#elif defined(ARM)
    /* XXX: assuming ti is passed via r0? */
    asm("str r0, %0" : "=m"((ti)) : : "r0");
    ASSERT_NOT_REACHED();
#endif /* X86/ARM */
    LOG(GLOBAL, LOG_LOADER, 4, "__tls_get_addr: module: %d, offset: %d\n",
        ti->ti_module, ti->ti_offset);
    ASSERT(ti->ti_module < tls_info.num_mods);
    return (os_get_priv_tls_base(NULL, TLS_REG_LIB) -
            tls_info.offs[ti->ti_module] + ti->ti_offset);
}

#ifdef LINUX
int
redirect_dl_iterate_phdr(int (*callback)(struct dl_phdr_info *info,
                                         size_t size, void *data),
                         void *data)
{
    int res = 0;
    struct dl_phdr_info info;
    privmod_t *mod;
    acquire_recursive_lock(&privload_lock);
    for (mod = privload_first_module(); mod != NULL; mod = privload_next_module(mod)) {
        ELF_HEADER_TYPE *elf_hdr = (ELF_HEADER_TYPE *) mod->base;
        os_privmod_data_t *opd = (os_privmod_data_t *) mod->os_privmod_data;
        /* We do want to include externally loaded (if any) and clients as
         * clients can contain C++ exception code, which will call here.
         */
        if (mod->base == get_dynamorio_dll_start())
            continue;
        info.dlpi_addr = opd->load_delta;
        info.dlpi_name = mod->name;
        info.dlpi_phdr = (ELF_PROGRAM_HEADER_TYPE *)(mod->base + elf_hdr->e_phoff);
        info.dlpi_phnum = elf_hdr->e_phnum;
        res = callback(&info, sizeof(info), data);
        if (res != 0)
            break;
    }
    release_recursive_lock(&privload_lock);
    return res;
}
#endif

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
#ifdef LINUX
    /* i#1717: C++ exceptions call this */
    {"dl_iterate_phdr", (app_pc)redirect_dl_iterate_phdr},
#endif
    /* We need these for clients that don't use libc (i#1747) */
    {"strlen", (app_pc)strlen},
    {"wcslen", (app_pc)wcslen},
    {"strchr", (app_pc)strchr},
    {"strrchr", (app_pc)strrchr},
    {"strncpy", (app_pc)strncpy},
    {"memcpy", (app_pc)memcpy},
    {"memset", (app_pc)memset},
    {"memmove", (app_pc)memmove},
    {"strncat", (app_pc)strncat},
    {"strcmp", (app_pc)strcmp},
    {"strncmp", (app_pc)strncmp},
    {"memcmp", (app_pc)memcmp},
    {"strstr", (app_pc)strstr},
    {"strcasecmp", (app_pc)strcasecmp},
    /* Also redirect the _chk versions (i#1747, i#46) */
    {"memcpy_chk", (app_pc)memcpy},
    {"memset_chk", (app_pc)memset},
    {"memmove_chk", (app_pc)memmove},
    {"strncpy_chk", (app_pc)strncpy},
};
#define REDIRECT_IMPORTS_NUM (sizeof(redirect_imports)/sizeof(redirect_imports[0]))

bool
privload_redirect_sym(ptr_uint_t *r_addr, const char *name)
{
    int i;
    /* iterate over all symbols and redirect syms when necessary, e.g. malloc */
    for (i = 0; i < REDIRECT_IMPORTS_NUM; i++) {
        if (strcmp(redirect_imports[i].name, name) == 0) {
            *r_addr = (ptr_uint_t)redirect_imports[i].func;
            return true;;
        }
    }
    return false;
}

/***************************************************************************
 * DynamoRIO Early Injection Code
 */

#ifdef LINUX
/* Find the auxiliary vector and adjust it to look as if the kernel had set up
 * the stack for the ELF mapped at map.  The auxiliary vector starts after the
 * terminating NULL pointer in the envp array.
 */
static void
privload_setup_auxv(char **envp, app_pc map, ptr_int_t delta, app_pc interp_map,
                    const char *exe_path/*must be persistent*/)
{
    ELF_AUXV_TYPE *auxv;
    ELF_HEADER_TYPE *elf = (ELF_HEADER_TYPE *) map;

    /* The aux vector is after the last environment pointer. */
    while (*envp != NULL)
        envp++;
    auxv = (ELF_AUXV_TYPE *)(envp + 1);

    /* fix up the auxv entries that refer to the executable */
    for (; auxv->a_type != AT_NULL; auxv++) {
        /* the actual addr should be: (base + offs) or (v_addr + delta) */
        switch (auxv->a_type) {
        case AT_ENTRY:
            auxv->a_un.a_val = (ptr_int_t) elf->e_entry + delta;
            LOG(GLOBAL, LOG_LOADER, 2, "AT_ENTRY: "PFX"\n", auxv->a_un.a_val);
            break;
        case AT_PHDR:
            auxv->a_un.a_val = (ptr_int_t) map + elf->e_phoff;
            LOG(GLOBAL, LOG_LOADER, 2, "AT_PHDR: "PFX"\n", auxv->a_un.a_val);
            break;
        case AT_PHENT:
            auxv->a_un.a_val = (ptr_int_t) elf->e_phentsize;
            break;
        case AT_PHNUM:
            auxv->a_un.a_val = (ptr_int_t) elf->e_phnum;
            break;
        case AT_BASE: /* Android loader reads this */
            auxv->a_un.a_val = (ptr_int_t) interp_map;
            LOG(GLOBAL, LOG_LOADER, 2, "AT_BASE: "PFX"\n", auxv->a_un.a_val);
            break;
        case AT_EXECFN: /* Android loader references this, unclear what for */
            auxv->a_un.a_val = (ptr_int_t) exe_path;
            LOG(GLOBAL, LOG_LOADER, 2, "AT_EXECFN: "PFX" %s\n",
                       auxv->a_un.a_val, (char*)auxv->a_un.a_val);
            break;

        /* The rest of these AT_* values don't seem to be important to the
         * loader, but we log them.
         */
        case AT_EXECFD:
            LOG(GLOBAL, LOG_LOADER, 2, "AT_EXECFD: %d\n", auxv->a_un.a_val);
            break;
        }
    }
}

/* Entry point for ptrace injection. */
static void
takeover_ptrace(ptrace_stack_args_t *args)
{
    static char home_var[MAXIMUM_PATH+6/*HOME=path\0*/];
    static char *fake_envp[] = {home_var, NULL};

    /* When we come in via ptrace, we have no idea where the environment
     * pointer is.  We could use /proc/self/environ to read it or go searching
     * near the stack base.  However, both are fragile and we don't really need
     * the environment for anything except for option passing.  In the initial
     * ptraced process, we can assume our options are in a config file and not
     * the environment, so we just set an environment with HOME.
     */
    snprintf(home_var, BUFFER_SIZE_ELEMENTS(home_var),
             "HOME=%s", args->home_dir);
    NULL_TERMINATE_BUFFER(home_var);
    dynamorio_set_envp(fake_envp);

    dynamorio_app_init();

    /* FIXME i#37: takeover other threads */

    /* We need to wait until dr_inject_process_run() is called to finish
     * takeover, and this is an easy way to stop and return control to the
     * injector.
     */
    dynamorio_syscall(SYS_kill, 2, get_process_id(), SIGTRAP);

    dynamo_start(&args->mc);
}

static void
reserve_brk(app_pc post_app)
{
    /* We haven't parsed the options yet, so we rely on drinjectlib
     * setting this env var if the user passed -no_emulate_brk:
     */
    if (getenv(DYNAMORIO_VAR_NO_EMULATE_BRK) == NULL) {
        /* i#1004: we're going to emulate the brk via our own mmap.
         * Reserve the initial brk now before any of DR's mmaps to avoid overlap.
         * XXX: reserve larger APP_BRK_GAP here and then unmap back to 1 page
         * in os_init() to ensure no DR mmap limits its size?
         */
        dynamo_options.emulate_brk = true; /* not parsed yet */
        init_emulated_brk(post_app);
    } else {
        /* i#1004: as a workaround, reserve some space for sbrk() during early injection
         * before initializing DR's heap.  With early injection, the program break comes
         * somewhere after DR's bss section, subject to some ASLR.  When we allocate our
         * heap, sometimes we mmap right over the break, so any brk() calls will fail.
         * When brk() fails, most malloc() implementations fall back to mmap().
         * However, sometimes libc startup code needs to allocate memory before libc is
         * initialized.  In this case it calls brk(), and will crash if it fails.
         *
         * Ideally we'd just set the break to follow the app's exe, but the kernel
         * forbids setting the break to a value less than the current break.  I also
         * tried to reserve memory by increasing the break by ~20 pages and then
         * resetting it, but the kernel unreserves it.  The current work around is to
         * increase the break by 1.  The loader needs to allocate more than a page of
         * memory, so this doesn't guarantee that further brk() calls will succeed.
         * However, I haven't observed any brk() failures after adding this workaround.
         */
        ptr_int_t start_brk;
        ASSERT(!dynamo_heap_initialized);
        start_brk = dynamorio_syscall(SYS_brk, 1, 0);
        dynamorio_syscall(SYS_brk, 1, start_brk + 1);
        /* I'd log the results, but logs aren't initialized yet. */
    }
}

/* i#1227: on a conflict with the app we reload ourselves.
 * Does not return.
 */
static void
reload_dynamorio(void **init_sp, app_pc conflict_start, app_pc conflict_end)
{
    elf_loader_t dr_ld;
    os_privmod_data_t opd;
    byte *dr_map, *temp1_map = NULL, *temp2_map = NULL;
    app_pc entry;
    byte *cur_dr_map = get_dynamorio_dll_start();
    byte *cur_dr_end = get_dynamorio_dll_end();
    size_t dr_size = cur_dr_end - cur_dr_map;
    size_t temp1_size, temp2_size;
    IF_DEBUG(bool success = )
        elf_loader_read_headers(&dr_ld, get_dynamorio_library_path());
    ASSERT(success);

    /* XXX: have better strategy for picking base: currently we rely on
     * the kernel picking an address, so we have to block out the conflicting
     * region first.
     */
    if (conflict_start < cur_dr_map) {
        temp1_size = cur_dr_map - conflict_start;
        temp1_map = os_map_file(-1, &temp1_size, 0, conflict_start, MEMPROT_NONE,
                                MAP_FILE_COPY_ON_WRITE | MAP_FILE_FIXED);
        ASSERT(temp1_map != NULL);
    }
    if (conflict_end > cur_dr_end) {
        /* Leave room for the brk */
        conflict_end += APP_BRK_GAP;
        temp2_size = conflict_end - cur_dr_end;
        temp2_map = os_map_file(-1, &temp2_size, 0, cur_dr_end, MEMPROT_NONE,
                                MAP_FILE_COPY_ON_WRITE | MAP_FILE_FIXED);
        ASSERT(temp2_map != NULL);
    }

    /* Now load the 2nd libdynamorio.so */
    dr_map = elf_loader_map_phdrs(&dr_ld, false /*!fixed*/, os_map_file,
                                  os_unmap_file, os_set_protection, false/*!reachable*/);
    ASSERT(dr_map != NULL);
    ASSERT(is_elf_so_header(dr_map, 0));

    /* Relocate it */
    memset(&opd, 0, sizeof(opd));
    module_get_os_privmod_data(dr_map, dr_size, false/*!relocated*/, &opd);
    /* XXX: we assume libdynamorio has no tls block b/c we're not calling
     * privload_relocate_mod().
     */
    ASSERT(opd.tls_block_size == 0);
    privload_relocate_os_privmod_data(&opd, dr_map);

    if (temp1_map != NULL)
        os_unmap_file(temp1_map, temp1_size);
    if (temp2_map != NULL)
        os_unmap_file(temp2_map, temp2_size);

    entry = (app_pc)dr_ld.ehdr->e_entry + dr_ld.load_delta;
    elf_loader_destroy(&dr_ld);

    /* Now we transfer control unconditionally to the new DR's _start, after
     * first restoring init_sp.  We pass along the current (old) DR's bounds
     * for removal.
     */
    xfer_to_new_libdr(entry, init_sp, cur_dr_map, dr_size);

    ASSERT_NOT_REACHED();
}

/* Called from _start in x86.asm.  sp is the initial app stack pointer that the
 * kernel set up for us, and it points to the usual argc, argv, envp, and auxv
 * that the kernel puts on the stack.
 */
void
privload_early_inject(void **sp, byte *old_libdr_base, size_t old_libdr_size)
{
    ptr_int_t *argc = (ptr_int_t *)sp;  /* Kernel writes an elf_addr_t. */
    char **argv = (char **)sp + 1;
    char **envp = argv + *argc + 1;
    app_pc entry = NULL;
    char *exe_path;
    char *exe_basename;
    app_pc exe_map, exe_end;
    elf_loader_t exe_ld;
    const char *interp;
    priv_mcontext_t mc;
    bool success;
    memquery_iter_t iter;
    app_pc interp_map;

    /* i#1676: try to detect no-ALSR which happens if we're launched from within
     * gdb w/o 'set disable-randomization off'.  We assume here that our
     * preferred base does not start with 0x5... and that the non-ASLR base does:
     *   64-bit: 0x555555b323ab
     *   32-bit: 0x55555000
     *
     * FIXME i#1708: the fixed base is computed from the highest user address
     * and thus can take several different values, not just 0x5*.
     * We can also fail in the same way due to a user disabling ASLR on the system
     * (i.e., not just inside gdb).
     * Worst of all, a recent kernel change has broken support for exec of ET_DYN
     * ever being placed at its preferred address.
     * Thus we need to go back to an ET_EXEC bootstrap.
     */
    if (old_libdr_base == NULL) {
        ptr_uint_t match = (ptr_uint_t)0x5 << IF_X64_ELSE(44, 28);
        ptr_uint_t mask = (ptr_int_t)-1 << IF_X64_ELSE(44, 28);
        if (((ptr_uint_t)privload_early_inject & mask) == match) {
            /* The problem is that we can't call any normal routines here, or
             * even reference global vars like string literals.  We thus use
             * a char array:
             */
            const char aslr_msg[] = {
                'E','R','R','O','R',':',' ','r','u','n',' ',
                '\'','s','e','t',' ','d','i','s','a','b','l','e','-','r','a','n','d',
                'o','m','i','z','a','t','i','o','n',' ','o','f','f','\'',' ','t','o',
                ' ','r','u','n',' ','f','r','o','m',' ','g','d','b','\n'
            };
#           define STDERR_FD 2
            os_write(STDERR_FD, aslr_msg, sizeof(aslr_msg));
            dynamorio_syscall(SYS_exit_group, 1, -1);
        }
    }

    /* XXX i#47: for Linux, we can't easily have this option on by default as
     * code like get_application_short_name() called from drpreload before
     * even _init is run needs to have a non-early default.
     */
    dynamo_options.early_inject = true;

    if (*argc == ARGC_PTRACE_SENTINEL) {
        /* XXX: Teach the injector to look up takeover_ptrace() and call it
         * directly instead of using this sentinel.  We come here because we
         * can easily find the address of _start in the ELF header.
         */
        takeover_ptrace((ptrace_stack_args_t *) sp);
    }

    /* i#1227: if we reloaded ourselves, unload the old libdynamorio */
    if (old_libdr_base != NULL)
        os_unmap_file(old_libdr_base, old_libdr_size);

    dynamorio_set_envp(envp);

    /* argv[0] doesn't actually have to be the path to the exe, so we put the
     * real exe path in an environment variable.
     */
    exe_path = getenv(DYNAMORIO_VAR_EXE_PATH);
    /* i#1677: this happens upon re-launching within gdb, so provide a nice error */
    if (exe_path == NULL) {
        /* i#1677: avoid assert in get_application_name_helper() */
        set_executable_path("UNKNOWN");
        apicheck(exe_path != NULL, DYNAMORIO_VAR_EXE_PATH" env var is not set.  "
                 "Are you re-launching within gdb?");
    }

    /* i#907: We can't rely on /proc/self/exe for the executable path, so we
     * have to tell get_application_name() to use this path.
     */
    set_executable_path(exe_path);

    success = elf_loader_read_headers(&exe_ld, exe_path);
    apicheck(success, "Failed to read app ELF headers.  Check path and "
             "architecture.");

    /* Find range of app */
    exe_map = module_vaddr_from_prog_header((app_pc)exe_ld.phdrs,
                                            exe_ld.ehdr->e_phnum, NULL, &exe_end);
    /* i#1227: on a conflict with the app: reload ourselves */
    if (get_dynamorio_dll_start() < exe_end &&
        get_dynamorio_dll_end() > exe_map) {
        reload_dynamorio(sp, exe_map, exe_end);
        ASSERT_NOT_REACHED();
    }

    exe_map = elf_loader_map_phdrs(&exe_ld,
                                   /* fixed at preferred address,
                                    * will be overridden if preferred base is 0
                                    */
                                   true,
                                   os_map_file,
                                   os_unmap_file, os_set_protection, false/*!reachable*/);
    apicheck(exe_map != NULL, "Failed to load application.  "
             "Check path and architecture.");
    ASSERT(is_elf_so_header(exe_map, 0));

    /* i#1660: the app may have passed a relative path or a symlink to execve,
     * yet the kernel will put a resolved path into /proc/self/maps.
     * Rather than us here or in pre-execve, plus in drrun or drinjectlib,
     * making paths absolute and resolving symlinks to try and match what the
     * kernel does, we just read the kernel's resolved path.
     * This is prior to memquery_init() but that's fine (it's already being
     * called by is_elf_so_header() above).
     */
    if (memquery_iterator_start(&iter, exe_map, false/*no heap*/)) {
        while (memquery_iterator_next(&iter)) {
            if (iter.vm_start == exe_map) {
                set_executable_path(iter.comment);
                break;
            }
        }
        memquery_iterator_stop(&iter);
    }

    /* Set the process name with prctl PR_SET_NAME.  This makes killall <app>
     * work.
     */
    exe_basename = strrchr(exe_path, '/');
    if (exe_basename == NULL) {
        exe_basename = exe_path;
    } else {
        exe_basename++;
    }
    dynamorio_syscall(SYS_prctl, 5, PR_SET_NAME, (ptr_uint_t)exe_basename,
                      0, 0, 0);

    reserve_brk(exe_map + exe_ld.image_size +
                (INTERNAL_OPTION(separate_private_bss) ? PAGE_SIZE : 0));

    interp = elf_loader_find_pt_interp(&exe_ld);
    if (interp != NULL) {
        /* Load the ELF pointed at by PT_INTERP, usually ld.so. */
        elf_loader_t interp_ld;
        success = elf_loader_read_headers(&interp_ld, interp);
        apicheck(success, "Failed to read ELF interpreter headers.");
        interp_map = elf_loader_map_phdrs(&interp_ld, false /* fixed */,
                                          os_map_file, os_unmap_file,
                                          os_set_protection, false/*!reachable*/);
        apicheck(interp_map != NULL && is_elf_so_header(interp_map, 0),
                 "Failed to map ELF interpreter.");
        /* On Android, the system loader /system/bin/linker sets itself
         * as the interpreter in the ELF header .interp field.
        */
        ASSERT_CURIOSITY_ONCE((strcmp(interp, "/system/bin/linker") == 0 ||
                               elf_loader_find_pt_interp(&interp_ld) == NULL) &&
                              "The interpreter shouldn't have an interpreter");
        entry = (app_pc)interp_ld.ehdr->e_entry + interp_ld.load_delta;
        elf_loader_destroy(&interp_ld);
    } else {
        /* No PT_INTERP, so this is a static exe. */
        interp_map = NULL;
        entry = (app_pc)exe_ld.ehdr->e_entry + exe_ld.load_delta;
    }

    privload_setup_auxv(envp, exe_map, exe_ld.load_delta, interp_map, exe_path);

    elf_loader_destroy(&exe_ld);

    /* Initialize DR *after* we map the app image.  This is consistent with our
     * old behavior, and allows the client to do things like call
     * dr_get_proc_address() on the app from dr_client_main().  We let
     * find_executable_vm_areas re-discover the mappings we made for the app and
     * interp images.
     */
    dynamorio_app_init();

    LOG(GLOBAL, LOG_TOP, 1, "early injected into app with this cmdline:\n");
    DOLOG(1, LOG_TOP, {
        int i;
        for (i = 0; i < *argc; i++) {
            LOG(GLOBAL, LOG_TOP, 1, "%s ", argv[i]);
        }
        LOG(GLOBAL, LOG_TOP, 1, "\n");
    });

    if (RUNNING_WITHOUT_CODE_CACHE()) {
        /* Reset the stack pointer back to the beginning and jump to the entry
         * point to execute the app natively.  This is also useful for testing
         * if the app has been mapped correctly without involving DR's code
         * cache.
         */
#ifdef X86
        asm ("mov %0, %%"ASM_XSP"\n\t"
             "jmp *%1\n\t"
             : : "r"(sp), "r"(entry));
#elif defined(ARM)
        /* FIXME i#1551: NYI on ARM */
        ASSERT_NOT_REACHED();
#endif
    }

    memset(&mc, 0, sizeof(mc));
    mc.xsp = (reg_t) sp;
    mc.pc = entry;
    dynamo_start(&mc);
}
#else
/* XXX i#1285: implement MacOS private loader */
#endif
