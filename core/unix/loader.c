/* *******************************************************************************
 * Copyright (c) 2011-2023 Google, Inc.  All rights reserved.
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
#include "../ir/instr.h" /* SEG_GS/SEG_FS */
#include "module.h"
#include "module_private.h"
#include "../heap.h" /* HEAPACCT */
#ifdef LINUX
#    include "include/syscall.h"
#    include "memquery.h"
#    define _GNU_SOURCE 1
#    define __USE_GNU 1
#    include <link.h> /* struct dl_phdr_info, must be prior to dlfcn.h */
#else
#    include <sys/syscall.h>
#endif
#include "tls.h"

#include <dlfcn.h> /* dlsym */
#ifdef LINUX
#    include <sys/prctl.h> /* PR_SET_NAME */
#endif
#include <stdlib.h> /* getenv */
#include <dlfcn.h>  /* dlopen/dlsym */
#include <unistd.h> /* __environ */
#include <stddef.h> /* offsetof */

extern size_t
wcslen(const wchar_t *str); /* in string.c */

/* Written during initialization only */
/* FIXME: i#460, the path lookup itself is a complicated process,
 * so we just list possible common but in-complete paths for now.
 */
#define SYSTEM_LIBRARY_PATH_VAR "LD_LIBRARY_PATH"
static char *ld_library_path = NULL;
static const char *const system_lib_paths[] = {
#ifdef X86
    "/lib/tls/i686/cmov",
#endif
    "/usr/lib",
    "/lib",
    "/usr/local/lib", /* Ubuntu: /etc/ld.so.conf.d/libc.conf */
#ifdef ANDROID
    "/system/lib",
#endif
#ifndef X64
    "/usr/lib32",
    "/lib32",
#    ifdef X86
    "/lib32/tls/i686/cmov",
    /* 32-bit Ubuntu */
    "/lib/i386-linux-gnu",
    "/usr/lib/i386-linux-gnu",
#    elif defined(ARM)
    "/lib/arm-linux-gnueabihf",
    "/usr/lib/arm-linux-gnueabihf",
    "/lib/arm-linux-gnueabi",
    "/usr/lib/arm-linux-gnueabi",
#    endif
#else
/* 64-bit Ubuntu */
#    ifdef X86
    "/lib64/tls/i686/cmov",
#    endif
    "/usr/lib64",
    "/lib64",
#    ifdef X86
    "/lib/x86_64-linux-gnu",     /* /etc/ld.so.conf.d/x86_64-linux-gnu.conf */
    "/usr/lib/x86_64-linux-gnu", /* /etc/ld.so.conf.d/x86_64-linux-gnu.conf */
#    elif defined(AARCH64)
    "/lib/aarch64-linux-gnu",
    "/usr/lib/aarch64-linux-gnu",
#    elif defined(RISCV64)
    "/lib/riscv64-linux-gnu",
    "/usr/lib/riscv64-linux-gnu",
#    endif
#endif
};
#define NUM_SYSTEM_LIB_PATHS (sizeof(system_lib_paths) / sizeof(system_lib_paths[0]))

#define RPATH_ORIGIN "$ORIGIN"

#define APP_BRK_GAP 64 * 1024 * 1024

static os_privmod_data_t *libdr_opd;

#ifdef LINUX /* XXX i#1285: implement MacOS private loader */
static bool printed_gdb_commands = false;
/* Global so visible in release build gdb */
static char gdb_priv_cmds[4096];
static size_t gdb_priv_cmds_sofar;
#endif

/* pointing to the I/O data structure in privately loaded libc,
 * They are used on exit when we need update file_no.
 */
stdfile_t **privmod_stdout;
stdfile_t **privmod_stderr;
stdfile_t **privmod_stdin;
#define LIBC_STDOUT_NAME "stdout"
#define LIBC_STDERR_NAME "stderr"
#define LIBC_STDIN_NAME "stdin"
#define LIBC_EARLY_INIT_NAME "__libc_early_init"

/* We save the original sp from the kernel, for use by TLS setup on Android */
void *kernel_init_sp;

/* forward decls */

static void
privload_init_search_paths(void);

static bool
privload_locate(const char *name, privmod_t *dep, char *filename OUT, bool *client OUT);

static privmod_t *
privload_locate_and_load(const char *impname, privmod_t *dependent, bool reachable);

static void
privload_call_lib_func(fp_t func);

static void
privload_relocate_mod(privmod_t *mod);

static void
privload_create_os_privmod_data(privmod_t *privmod, bool dyn_reloc);

static void
privload_delete_os_privmod_data(privmod_t *privmod);

#ifdef LINUX
void
privload_mod_tls_init(privmod_t *mod);

void
privload_mod_tls_primary_thread_init(privmod_t *mod);
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
static void
privload_add_gdb_cmd(elf_loader_t *loader, const char *filename, bool reachable)
{
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    /* Get the text addr to register the ELF with gdb.  The section headers
     * are not part of the mapped image, so we have to map the whole file.
     * XXX: seek to e_shoff and read the section headers to avoid this map.
     */
    if (elf_loader_map_file(loader, reachable) != NULL) {
        app_pc text_addr =
            (app_pc)module_get_text_section(loader->file_map, loader->file_size);
        text_addr += loader->load_delta;
        print_to_buffer(gdb_priv_cmds, BUFFER_SIZE_ELEMENTS(gdb_priv_cmds),
                        &gdb_priv_cmds_sofar, "add-symbol-file '%s' %p\n", filename,
                        text_addr);
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
        LOG(GLOBAL, LOG_LOADER, 1, "for debugger: add-symbol-file %s %p\n", filename,
            text_addr);
        if (INTERNAL_OPTION(privload_register_gdb)) {
            dr_gdb_add_symbol_file(filename, text_addr);
        }
    }
}
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
    mod = privload_insert(NULL, get_dynamorio_dll_start(),
                          get_dynamorio_dll_end() - get_dynamorio_dll_start(),
                          get_shared_lib_name(get_dynamorio_dll_start()),
                          get_dynamorio_library_path());
    ASSERT(mod != NULL);
    /* If DR was loaded by system ld.so, then .dynamic *was* relocated (i#1589) */
    privload_create_os_privmod_data(mod, !DYNAMO_OPTION(early_inject));
    libdr_opd = (os_privmod_data_t *)mod->os_privmod_data;
    DODEBUG({
        if (DYNAMO_OPTION(early_inject)) {
            /* We've already filled the gap in dynamorio_lib_gap_empty().  We just
             * verify here now that we have segment info.
             */
            int i;
            for (i = 0; i < libdr_opd->os_data.num_segments - 1; i++) {
                size_t sz = libdr_opd->os_data.segments[i + 1].start -
                    libdr_opd->os_data.segments[i].end;
                if (sz > 0) {
                    dr_mem_info_t info;
                    bool ok = query_memory_ex_from_os(libdr_opd->os_data.segments[i].end,
                                                      &info);
                    ASSERT(ok);
                    ASSERT(info.base_pc == libdr_opd->os_data.segments[i].end &&
                           info.size == sz &&
                           (info.type == DR_MEMTYPE_FREE ||
                            /* If we reloaded DR, our own loader filled it in. */
                            info.prot == DR_MEMPROT_NONE));
                }
            }
        }
    });
    mod->externally_loaded = true;
#    ifdef LINUX /*i#1285*/
    if (DYNAMO_OPTION(early_inject)) {
        /* libdynamorio isn't visible to gdb so add to the cmd list */
        byte *dr_base = get_dynamorio_dll_start(), *pref_base;
        elf_loader_t dr_ld;
        IF_DEBUG(bool success =)
        elf_loader_read_headers(&dr_ld, get_dynamorio_library_path());
        ASSERT(success);
        module_walk_program_headers(dr_base, get_dynamorio_dll_end() - dr_base, false,
                                    false, (byte **)&pref_base, NULL, NULL, NULL, NULL);
        dr_ld.load_delta = dr_base - pref_base;
        privload_add_gdb_cmd(&dr_ld, get_dynamorio_library_path(), false /*!reach*/);
        elf_loader_destroy(&dr_ld);
    }
#    endif
#endif
}

/* os specific loader initialization epilogue after finalizing the load. */
void
os_loader_init_epilogue(void)
{
#ifdef LINUX /* XXX i#1285: implement MacOS private loader */
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
                             "%s",
                             gdb_priv_cmds);
    }
#endif
}

void
os_loader_exit(void)
{
    if (libdr_opd != NULL) {
        HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, libdr_opd->os_data.segments, module_segment_t,
                        libdr_opd->os_data.alloc_segments, ACCT_OTHER, PROTECTED);
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, libdr_opd, os_privmod_data_t, ACCT_OTHER,
                       PROTECTED);
    }

#ifdef LINUX
    /* Put printed_gdb_commands into its original state for potential
     * re-attaching and os_loader_init_epilogue().
     */
    printed_gdb_commands = false;
#endif
}

/* These are called before loader_init for the primary thread for UNIX. */
void
os_loader_thread_init_prologue(dcontext_t *dcontext)
{
    /* do nothing */
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
    uint i;

    /* create and init the os_privmod_data for privmod.
     * The os_privmod_data can only be created after heap is ready and
     * should be done before adding in vmvector_add,
     * so it can be either right before calling to privload_add_areas
     * in the privload_load_finalize, or in here.
     * We prefer here because it avoids changing the code in
     * loader_shared.c, which affects windows too.
     */
    privload_create_os_privmod_data(privmod, false /* i#1589: .dynamic not relocated */);
    opd = (os_privmod_data_t *)privmod->os_privmod_data;
    for (i = 0; i < opd->os_data.num_segments; i++) {
        vmvector_add(modlist_areas, opd->os_data.segments[i].start,
                     opd->os_data.segments[i].end, (void *)privmod);
    }
}

void
privload_remove_areas(privmod_t *privmod)
{
    uint i;
    os_privmod_data_t *opd = (os_privmod_data_t *)privmod->os_privmod_data;

    /* walk the program header to remove areas */
    for (i = 0; i < opd->os_data.num_segments; i++) {
        vmvector_remove(modlist_areas, opd->os_data.segments[i].start,
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
    os_privmod_data_t *opd = (os_privmod_data_t *)privmod->os_privmod_data;

    /* unmap segments */
    IF_DEBUG(size_t size_unmapped = 0);
    for (i = 0; i < opd->os_data.num_segments; i++) {
        d_r_unmap_file(opd->os_data.segments[i].start,
                       opd->os_data.segments[i].end - opd->os_data.segments[i].start);
        DODEBUG({
            size_unmapped +=
                opd->os_data.segments[i].end - opd->os_data.segments[i].start;
        });
        if (i + 1 < opd->os_data.num_segments &&
            opd->os_data.segments[i + 1].start > opd->os_data.segments[i].end) {
            /* unmap the gap */
            d_r_unmap_file(opd->os_data.segments[i].end,
                           opd->os_data.segments[i + 1].start -
                               opd->os_data.segments[i].end);
            DODEBUG({
                size_unmapped +=
                    opd->os_data.segments[i + 1].start - opd->os_data.segments[i].end;
            });
        }
    }
    ASSERT(size_unmapped == privmod->size);
    /* XXX i#3570: Better to store the MODLOAD_SEPARATE_BSS flag but there's no
     * simple code path to do it so we check the option.
     */
    if (INTERNAL_OPTION(separate_private_bss)) {
        /* unmap the extra .bss-separating page */
        d_r_unmap_file(privmod->base + privmod->size, PAGE_SIZE);
        DODEBUG({ size_unmapped += PAGE_SIZE; });
    }
    /* free segments */
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, opd->os_data.segments, module_segment_t,
                    opd->os_data.alloc_segments, ACCT_OTHER, PROTECTED);
    /* delete os_privmod_data */
    privload_delete_os_privmod_data(privmod);
}

bool
privload_unload_imports(privmod_t *privmod)
{
    /* FIXME: i#474 unload dependent libraries if necessary */
    return true;
}

#ifdef LINUX
/* Core-specific functionality for elf_loader_map_phdrs(). */
static modload_flags_t
privload_map_flags(modload_flags_t init_flags)
{
    /* XXX: Keep this condition matching the check in privload_unmap_file()
     * (minus MODLOAD_NOT_PRIVLIB since non-privlibs don't reach our unmap).
     */
    if (INTERNAL_OPTION(separate_private_bss) && !TEST(MODLOAD_NOT_PRIVLIB, init_flags)) {
        /* place an extra no-access page after .bss */
        /* XXX: update privload_early_inject call to init_emulated_brk if this changes */
        /* XXX: should we avoid this for -early_inject's map of the app and ld.so? */
        return init_flags | MODLOAD_SEPARATE_BSS;
    }
    return init_flags;
}

/* Core-specific functionality for elf_loader_map_phdrs(). */
static void
privload_check_new_map_bounds(elf_loader_t *elf, byte *map_base, byte *map_end)
{
    /* This is only called for MAP_FIXED. */
    if (get_dynamorio_dll_start() < map_end && get_dynamorio_dll_end() > map_base) {
        FATAL_USAGE_ERROR(FIXED_MAP_OVERLAPS_DR, 3, get_application_name(),
                          get_application_pid(), elf->filename);
        ASSERT_NOT_REACHED();
    }
}
#endif

/* This only maps, as relocation for ELF requires processing imports first,
 * which we have to delay at init time at least.
 */
app_pc
privload_map_and_relocate(const char *filename, size_t *size OUT, modload_flags_t flags)
{
#ifdef LINUX
    map_fn_t map_func;
    unmap_fn_t unmap_func;
    prot_fn_t prot_func;
    app_pc base = NULL;
    elf_loader_t loader;

    ASSERT_OWN_RECURSIVE_LOCK(!TEST(MODLOAD_NOT_PRIVLIB, flags), &privload_lock);
    /* get appropriate function */
    /* NOTE: all but the client lib will be added to DR areas list b/c using
     * d_r_map_file()
     */
    if (dynamo_heap_initialized && !standalone_library) {
        map_func = d_r_map_file;
        unmap_func = d_r_unmap_file;
        prot_func = set_protection;
    } else {
        map_func = os_map_file;
        unmap_func = os_unmap_file;
        prot_func = os_set_protection;
    }

    if (!elf_loader_read_headers(&loader, filename)) {
        /* We may want to move the bitwidth check out if is_elf_so_header_common()
         * but for now we keep that there and do another check here.
         * If loader.buf was not read into it will be all zeroes.
         */
        ELF_HEADER_TYPE *elf_header = (ELF_HEADER_TYPE *)loader.buf;
        ELF_ALTARCH_HEADER_TYPE *altarch = (ELF_ALTARCH_HEADER_TYPE *)elf_header;
        if (!TEST(MODLOAD_NOT_PRIVLIB, flags) && elf_header->e_version == 1 &&
            altarch->e_ehsize == sizeof(ELF_ALTARCH_HEADER_TYPE) &&
            altarch->e_machine ==
                IF_X64_ELSE(IF_AARCHXX_ELSE(EM_ARM, EM_386),
                            IF_AARCHXX_ELSE(EM_AARCH64, EM_X86_64))) {
            /* XXX i#147: Should we try some path substs like s/lib32/lib64/?
             * Maybe it's better to error out to avoid loading some unintended lib.
             */
            SYSLOG(SYSLOG_ERROR, CLIENT_LIBRARY_WRONG_BITWIDTH, 3, get_application_name(),
                   get_application_pid(), filename);
        }
        return NULL;
    }
    base = elf_loader_map_phdrs(&loader, false /* fixed */, map_func, unmap_func,
                                prot_func, privload_check_new_map_bounds, memset,
                                privload_map_flags(flags));
    if (base != NULL) {
        if (size != NULL)
            *size = loader.image_size;

        if (!TEST(MODLOAD_NOT_PRIVLIB, flags))
            privload_add_gdb_cmd(&loader, filename, TEST(MODLOAD_REACHABLE, flags));
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

    opd = (os_privmod_data_t *)mod->os_privmod_data;
    ASSERT(opd != NULL);
    /* 1. get DYNAMIC section pointer */
    dyn = (ELF_DYNAMIC_ENTRY_TYPE *)opd->dyn;
    /* 2. get dynamic string table */
    strtab = (char *)opd->os_data.dynstr;
    /* 3. depth-first recursive load, so add into the deps list first */
    while (dyn->d_tag != DT_NULL) {
        if (dyn->d_tag == DT_NEEDED) {
            name = strtab + dyn->d_un.d_val;
            LOG(GLOBAL, LOG_LOADER, 2, "%s: %s imports from %s\n", __FUNCTION__,
                mod->name, name);
            if (privload_lookup(name) == NULL) {
                privmod_t *impmod =
                    privload_locate_and_load(name, mod, false /*client dir=>true*/);
                if (impmod == NULL)
                    return false;
                if (strstr(name, "libpthread") == name) {
                    /* i#956: A private libpthread is not fully supported, but many
                     * libraries import some utilities from it and do not use
                     * threading.  We load it and just do not guarantee things will
                     * work if thread-related routines are called.
                     */
                    SYSLOG_INTERNAL_WARNING(
                        "private libpthread.so loaded but not fully supported (i#956)");
                }
                /* i#852: identify all libs that import from DR as client libs.
                 * XXX: this code seems stale as libdynamorio.so is already loaded
                 * (xref #3850).
                 */
                if (impmod->base == get_dynamorio_dll_start())
                    mod->is_client = true;
            }
        }
        ++dyn;
    }
    /* Relocate library's symbols after load dependent libraries (so that we
     * can resolve symbols in the global ELF namespace).
     */
    if (!mod->externally_loaded) {
        privload_relocate_mod(mod);
    }
    return true;
#else
    /* XXX i#1285: implement MacOS private loader */
    if (!mod->externally_loaded) {
        privload_relocate_mod(mod);
    }
    return false;
#endif
}

bool
privload_call_entry(dcontext_t *dcontext, privmod_t *privmod, uint reason)
{
    os_privmod_data_t *opd = (os_privmod_data_t *)privmod->os_privmod_data;
    ASSERT(os_get_priv_tls_base(NULL, TLS_REG_LIB) != NULL);
    if (reason == DLL_PROCESS_INIT) {
        /* calls init and init array */
        LOG(GLOBAL, LOG_LOADER, 3, "%s: calling init routines of %s\n", __FUNCTION__,
            privmod->name);
        if (opd->init != NULL) {
            LOG(GLOBAL, LOG_LOADER, 4, "%s: calling %s init func " PFX "\n", __FUNCTION__,
                privmod->name, opd->init);
            privload_call_lib_func(opd->init);
        }
        if (opd->init_array != NULL) {
            uint i;
            for (i = 0; i < opd->init_arraysz / sizeof(opd->init_array[i]); i++) {
                if (opd->init_array[i] != NULL) { /* be paranoid */
                    LOG(GLOBAL, LOG_LOADER, 4, "%s: calling %s init array func " PFX "\n",
                        __FUNCTION__, privmod->name, opd->init_array[i]);
                    privload_call_lib_func(opd->init_array[i]);
                }
            }
        }
        return true;
    } else if (reason == DLL_PROCESS_EXIT) {
        /* calls fini and fini array */
#ifdef ANDROID
        /* i#1701: libdl.so fini routines call into libc somehow, which is
         * often already unmapped.  We just skip them as a workaround.
         */
        if (strcmp(privmod->name, "libdl.so") == 0) {
            LOG(GLOBAL, LOG_LOADER, 3, "%s: NOT calling fini routines of %s\n",
                __FUNCTION__, privmod->name);
            return true;
        }
#endif
        LOG(GLOBAL, LOG_LOADER, 3, "%s: calling fini routines of %s\n", __FUNCTION__,
            privmod->name);
        if (opd->fini != NULL) {
            LOG(GLOBAL, LOG_LOADER, 4, "%s: calling %s fini func " PFX "\n", __FUNCTION__,
                privmod->name, opd->fini);
            privload_call_lib_func(opd->fini);
        }
        if (opd->fini_array != NULL) {
            uint i;
            for (i = 0; i < opd->fini_arraysz / sizeof(opd->fini_array[0]); i++) {
                if (opd->fini_array[i] != NULL) { /* be paranoid */
                    LOG(GLOBAL, LOG_LOADER, 4, "%s: calling %s fini array func " PFX "\n",
                        __FUNCTION__, privmod->name, opd->fini_array[i]);
                    privload_call_lib_func(opd->fini_array[i]);
                }
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
privload_os_finalize(privmod_t *privmod)
{
#ifndef LINUX
    return; /* Nothing to do. */
#else
    static privmod_t *privmod_ld_linux;
    if (strstr(privmod->name, "ld-linux") == privmod->name) {
        /* We need to first get the libc version before we clobber ld vars.
         * (We could instead look for versioned symbols with "@GLIBC_2.34" in ld
         * but we do not have version parsing code in place.)
         * We assume ld will not be unloaded.
         */
        privmod_ld_linux = privmod;
        return;
    }
    if (strstr(privmod->name, "libc.so") != privmod->name)
        return;
    os_privmod_data_t *opd = (os_privmod_data_t *)privmod->os_privmod_data;
    /* Special handling for standard I/O file descriptors. */
    privmod_stdout = (FILE **)get_proc_address_from_os_data(
        &opd->os_data, opd->load_delta, LIBC_STDOUT_NAME, NULL);
    privmod_stdin = (FILE **)get_proc_address_from_os_data(&opd->os_data, opd->load_delta,
                                                           LIBC_STDIN_NAME, NULL);
    privmod_stderr = (FILE **)get_proc_address_from_os_data(
        &opd->os_data, opd->load_delta, LIBC_STDERR_NAME, NULL);
    /* i#5133: glibc 2.32+ has ld.so call a hardcoded initializer before calling the
     * regular ELF constructors.
     */
    void (*libc_early_init)(bool) = (void (*)(bool))get_proc_address_from_os_data(
        &opd->os_data, opd->load_delta, LIBC_EARLY_INIT_NAME, NULL);
    if (libc_early_init == NULL) {
        return;
    }
    /* XXX i#5437: Temporary workaround to avoid a SIGFPE in glibc 2.34+
     * __libc_early_init(). As we cannot let ld/libc initialize their own TLS with the
     * current design, we must explicitly initialize a few variables.  Unfortunately
     * we have to hardcode their offsets, making this fragile. Long-term we should try
     * to find a better solution.
     */
    /* Do not try to clobber vars unless we have to: get the libc version. */
#    define LIBC_GET_VERSION_NAME "gnu_get_libc_version"
    const char *(*libc_ver)(void) = (const char *(*)(void))get_proc_address_from_os_data(
        &opd->os_data, opd->load_delta, LIBC_GET_VERSION_NAME, NULL);
    if (libc_ver == NULL)
        return;
    LOG(GLOBAL, LOG_LOADER, 2, "%s: calling %s\n", __FUNCTION__, LIBC_GET_VERSION_NAME);
    const char *ver = (*libc_ver)();
    LOG(GLOBAL, LOG_LOADER, 2, "%s: libc version is |%s|\n", __FUNCTION__, ver);
    if ((ver[0] == '\0' || ver[0] < '2') || ver[1] != '.' || ver[2] < '3' ||
        (ver[2] == '3' && ver[3] < '4'))
        return;
    if (privmod_ld_linux == NULL) {
        SYSLOG_INTERNAL_WARNING("glibc 2.34+ i#5437 workaround failed: missed ld");
        return;
    }
    os_privmod_data_t *ld_opd = (os_privmod_data_t *)privmod_ld_linux->os_privmod_data;
    byte *glro = get_proc_address_from_os_data(&ld_opd->os_data, ld_opd->load_delta,
                                               "_rtld_global_ro", NULL);
    if (glro == NULL) {
        SYSLOG_INTERNAL_WARNING("glibc 2.34+ i#5437 workaround failed: missed glro");
        return;
    }
#    ifdef X64
    const int GLRO_dl_tls_static_size_OFFS = 0x2a8;
    const int GLRO_dl_tls_static_align_OFFS = 0x2b0;
#    else
    // The offsets changed between 2.35 and 2.36.
    const int GLRO_dl_tls_static_size_OFFS =
        (ver[2] == '3' && ver[3] == '5') ? 0x328 : 0x31c;
    const int GLRO_dl_tls_static_align_OFFS =
        (ver[2] == '3' && ver[3] == '5') ? 0x32c : 0x320;
#    endif
    size_t val = 4096, written;
    if (!safe_write_ex(glro + GLRO_dl_tls_static_size_OFFS, sizeof(val), &val,
                       &written) ||
        written != sizeof(val) ||
        !safe_write_ex(glro + GLRO_dl_tls_static_align_OFFS, sizeof(val), &val,
                       &written) ||
        written != sizeof(val)) {
        SYSLOG_INTERNAL_WARNING("glibc 2.34+ i#5437 workaround failed: missed write");
    } else {
        LOG(GLOBAL, LOG_LOADER, 2, "%s: glibc 2.34+ workaround succeeded\n",
            __FUNCTION__);
    }
    LOG(GLOBAL, LOG_LOADER, 2, "%s: calling %s\n", __FUNCTION__, LIBC_EARLY_INIT_NAME);
    (*libc_early_init)(true);
#endif /* LINUX */
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

/* If runpath, then DT_RUNPATH is searched; else, DT_RPATH. */
static bool
privload_search_rpath(privmod_t *mod, bool runpath, const char *name,
                      char *filename OUT /* buffer size is MAXIMUM_PATH */)
{
#ifdef LINUX
    os_privmod_data_t *opd;
    ELF_DYNAMIC_ENTRY_TYPE *dyn;
    ASSERT(mod != NULL && "can't look for rpath without a dependent module");
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    /* get the loading module's dir for RPATH_ORIGIN */
    opd = (os_privmod_data_t *)mod->os_privmod_data;
    /* i#460: if DT_RUNPATH exists we must ignore ignore DT_RPATH and
     * search DT_RUNPATH after LD_LIBRARY_PATH.
     */
    if (!runpath && opd->os_data.has_runpath)
        return false;
    const char *moddir_end = strrchr(mod->path, '/');
    size_t moddir_len = (moddir_end == NULL ? strlen(mod->path) : moddir_end - mod->path);
    const char *strtab;
    ASSERT(opd != NULL);
    dyn = (ELF_DYNAMIC_ENTRY_TYPE *)opd->dyn;
    strtab = (char *)opd->os_data.dynstr;
    bool lib_found = false;
    /* support $ORIGIN expansion to lib's current directory */
    while (dyn->d_tag != DT_NULL) {
        if (dyn->d_tag == (runpath ? DT_RUNPATH : DT_RPATH)) {
            /* DT_RPATH and DT_RUNPATH are each a colon-separated list of paths */
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
                char path[MAXIMUM_PATH];
                if (origin != NULL && origin < list + len) {
                    size_t pre_len = origin - list;
                    snprintf(path, BUFFER_SIZE_ELEMENTS(path), "%.*s%.*s%.*s", pre_len,
                             list, moddir_len, mod->path,
                             /* the '/' should already be here */
                             len - strlen(RPATH_ORIGIN) - pre_len,
                             origin + strlen(RPATH_ORIGIN));
                    NULL_TERMINATE_BUFFER(path);
                } else {
                    snprintf(path, BUFFER_SIZE_ELEMENTS(path), "%.*s", len, list);
                    NULL_TERMINATE_BUFFER(path);
                }
                if (mod->is_client) {
                    /* We are adding a client's lib rpath to the general search path. This
                     * is not bullet proof compliant with what the loader should really
                     * do. The real problem is that the loader is walking library
                     * dependencies depth-first, while it should really search
                     * breadth-first (xref i#3850). This can lead to libraries being
                     * unlocatable, if the original client library had the proper rpath of
                     * the library, but a dependency later in the chain did not. In order
                     * to avoid this, we consider adding the rpath here relatively safe.
                     * It only affects dependent libraries of the same name in different
                     * locations. We are only doing this for client libraries, so we are
                     * not at risk to search for the wrong system libraries.
                     */
                    if (!privload_search_path_exists(path, strlen(path))) {
                        snprintf(search_paths[search_paths_idx],
                                 BUFFER_SIZE_ELEMENTS(search_paths[search_paths_idx]),
                                 "%.*s", strlen(path), path);
                        NULL_TERMINATE_BUFFER(search_paths[search_paths_idx]);
                        LOG(GLOBAL, LOG_LOADER, 1, "%s: added search dir \"%s\"\n",
                            __FUNCTION__, search_paths[search_paths_idx]);
                        search_paths_idx++;
                    }
                }
                if (!lib_found) {
                    snprintf(filename, MAXIMUM_PATH, "%s/%s", path, name);
                    filename[MAXIMUM_PATH - 1] = 0;
                    LOG(GLOBAL, LOG_LOADER, 2, "%s: looking for %s\n", __FUNCTION__,
                        filename);
                    if (os_file_exists(filename, false /*!is_dir*/) &&
                        module_file_has_module_header(filename)) {
                        lib_found = true;
                    }
                }
                list += len;
                if (sep != NULL)
                    list += 1;
            }
        }
        ++dyn;
    }
    return lib_found;
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
    if (name[0] == '/' && os_file_exists(name, false /*!is_dir*/)) {
        snprintf(filename, MAXIMUM_PATH, "%s", name);
        filename[MAXIMUM_PATH - 1] = 0;
        return true;
    }

    /* FIXME: We have a simple implementation of library search.
     * libc implementation can be found at elf/dl-load.c:_dl_map_object.
     */
    /* the loader search order: */
    /* 0) DT_RPATH */
    if (dep != NULL && privload_search_rpath(dep, false /*rpath*/, name, filename))
        return true;

    /* 1) client lib dir */
    for (i = 0; i < search_paths_idx; i++) {
        snprintf(filename, MAXIMUM_PATH, "%s/%s", search_paths[i], name);
        /* NULL_TERMINATE_BUFFER(filename) */
        filename[MAXIMUM_PATH - 1] = 0;
        LOG(GLOBAL, LOG_LOADER, 2, "%s: looking for %s\n", __FUNCTION__, filename);
        if (os_file_exists(filename, false /*!is_dir*/) &&
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
    if (os_file_exists(filename, false /*!is_dir*/) &&
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
        LOG(GLOBAL, LOG_LOADER, 2, "%s: looking for %s\n", __FUNCTION__, filename);
        if (os_file_exists(filename, false /*!is_dir*/) &&
            module_file_has_module_header(filename))
            return true;
        lib_paths = end;
    }

    /* 4) DT_RUNPATH */
    if (dep != NULL && privload_search_rpath(dep, true /*runpath*/, name, filename))
        return true;

    /* 5) XXX i#460: We use our system paths instead of /etc/ld.so.cache. */
    for (i = 0; i < NUM_SYSTEM_LIB_PATHS; i++) {
        /* First try -xarch_root for emulation. */
        if (!IS_STRING_OPTION_EMPTY(xarch_root)) {
            string_option_read_lock();
            snprintf(filename, MAXIMUM_PATH, "%s/%s/%s", DYNAMO_OPTION(xarch_root),
                     system_lib_paths[i], name);
            filename[MAXIMUM_PATH - 1] = '\0';
            string_option_read_unlock();
            if (os_file_exists(filename, false /*!is_dir*/) &&
                module_file_has_module_header(filename))
                return true;
        }
        snprintf(filename, MAXIMUM_PATH, "%s/%s", system_lib_paths[i], name);
        /* NULL_TERMINATE_BUFFER(filename) */
        filename[MAXIMUM_PATH - 1] = 0;
        LOG(GLOBAL, LOG_LOADER, 2, "%s: looking for %s\n", __FUNCTION__, filename);
        if (os_file_exists(filename, false /*!is_dir*/) &&
            module_file_has_module_header(filename))
            return true;
    }

    /* Cannot find the library */
    /* There's a syslog in loader_init() but we want to provide the lib name */
    SYSLOG(SYSLOG_ERROR, CLIENT_LIBRARY_UNLOADABLE, 4, get_application_name(),
           get_application_pid(), name,
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
#    ifdef STATIC_LIBRARY
        /* externally loaded, use dlsym instead */
        ASSERT(!DYNAMO_OPTION(early_inject));
        return dlsym(modbase, name);
#    else
        /* Only libdynamorio.so is externally_loaded and we should not be querying
         * for it.  Unknown libs shouldn't be queried here: get_proc_address should
         * be used instead.
         */
        ASSERT_NOT_REACHED();
        return NULL;
#    endif
    }
    /* Before the heap is initialized, we store the text address in opd, so we
     * can't check if opd != NULL to know whether it's valid.
     */
    if (dynamo_heap_initialized) {
        /* opd is initialized */
        os_privmod_data_t *opd = (os_privmod_data_t *)mod->os_privmod_data;
        res = get_proc_address_from_os_data(&opd->os_data, opd->load_delta, name, NULL);
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
        if (!module_read_os_data(mod->base, false /* .dynamic not relocated (i#1589) */,
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
        *end = mod->base + mod->size;
        found = true;
    }
    release_recursive_lock(&privload_lock);
    return found;
}

#ifdef LINUX
#    if !defined(STANDALONE_UNIT_TEST) && !defined(STATIC_LIBRARY)
/* XXX: This routine is called before dynamorio relocation when we are in a
 * fragile state and thus no globals access or use of ASSERT/LOG/STATS!
 */
/* If we fail to relocate dynamorio, print the error msg and abort. */
static void
privload_report_relocate_error()
{
    /* The problem is that we can't call any normal routines here, or
     * even reference global vars like string literals.  We thus use
     * a char array:
     */
    const char aslr_msg[] = { 'E',  'R', 'R', 'O', 'R', ':', ' ', 'f', 'a', 'i', 'l', 'e',
                              'd',  ' ', 't', 'o', ' ', 'r', 'e', 'l', 'o', 'c', 'a', 't',
                              'e',  ' ', 'D', 'y', 'n', 'a', 'm', 'o', 'R', 'I', 'O', '!',
                              '\n', 'P', 'l', 'e', 'a', 's', 'e', ' ', 'f', 'i', 'l', 'e',
                              ' ',  'a', 'n', ' ', 'i', 's', 's', 'u', 'e', ' ', 'a', 't',
                              ' ',  'h', 't', 't', 'p', ':', '/', '/', 'd', 'y', 'n', 'a',
                              'm',  'o', 'r', 'i', 'o', '.', 'o', 'r', 'g', '/', 'i', 's',
                              's',  'u', 'e', 's', '.', '\n' };
#        define STDERR_FD 2
    os_write(STDERR_FD, aslr_msg, sizeof(aslr_msg));
    dynamorio_syscall(SYS_exit_group, 1, -1);
}

/* XXX: This routine is called before dynamorio relocation when we are in a
 * fragile state and thus no globals access or use of ASSERT/LOG/STATS!
 */
/* This routine is duplicated from module_relocate_symbol and simplified
 * for only relocating dynamorio symbols.
 */
static void
privload_relocate_symbol(ELF_REL_TYPE *rel, os_privmod_data_t *opd, bool is_rela)
{
    ELF_ADDR *r_addr;
    uint r_type;
    reg_t addend;

    /* XXX: we assume ELF_REL_TYPE and ELF_RELA_TYPE only differ at the end,
     * i.e. with or without r_addend.
     */
    if (is_rela)
        addend = ((ELF_RELA_TYPE *)rel)->r_addend;
    else
        addend = 0;

    /* assume everything is read/writable */
    r_addr = (ELF_ADDR *)(rel->r_offset + opd->load_delta);
    r_type = (uint)ELF_R_TYPE(rel->r_info);

    /* handle the most common case, i.e. ELF_R_RELATIVE */
    if (r_type == ELF_R_RELATIVE) {
        if (is_rela)
            *r_addr = addend + opd->load_delta;
        else
            *r_addr += opd->load_delta;
        return;
    } else if (r_type == ELF_R_NONE)
        return;

    /* XXX i#1708: support more relocation types in bootstrap stage */
    privload_report_relocate_error();
}

/* XXX: This routine is called before dynamorio relocation when we are in a
 * fragile state and thus no globals access or use of ASSERT/LOG/STATS!
 */
/* This routine is duplicated from module_relocate_rel for relocating dynamorio. */
static void
privload_relocate_rel(os_privmod_data_t *opd, ELF_REL_TYPE *start, ELF_REL_TYPE *end)
{
    ELF_REL_TYPE *rel;
    for (rel = start; rel < end; rel++)
        privload_relocate_symbol(rel, opd, false);
}

/* XXX: This routine is called before dynamorio relocation when we are in a
 * fragile state and thus no globals access or use of ASSERT/LOG/STATS!
 */
/* This routine is duplicated from module_relocate_rela for relocating dynamorio. */
static void
privload_relocate_rela(os_privmod_data_t *opd, ELF_RELA_TYPE *start, ELF_RELA_TYPE *end)
{
    ELF_RELA_TYPE *rela;
    for (rela = start; rela < end; rela++)
        privload_relocate_symbol((ELF_REL_TYPE *)rela, opd, true);
}

/* XXX: This routine is called before dynamorio relocation when we are in a
 * fragile state and thus no globals access or use of ASSERT/LOG/STATS!
 */
/* This routine is duplicated from module_relocate_relr for relocating dynamorio. */
static void
privload_relocate_relr(os_privmod_data_t *opd, const ELF_WORD *relr, size_t size)
{
    ELF_ADDR *r_addr = NULL;

    for (; size != 0; relr++, size -= sizeof(ELF_WORD)) {
        if (!TEST(1, relr[0])) {
            r_addr = (ELF_ADDR *)(relr[0] + opd->load_delta);
            *r_addr++ += opd->load_delta;
        } else {
            int i = 0;
            for (ELF_WORD bitmap = relr[0]; (bitmap >>= 1) != 0; i++) {
                if (TEST(1, bitmap))
                    r_addr[i] += opd->load_delta;
            }
            r_addr += CHAR_BIT * sizeof(ELF_WORD) - 1;
        }
    }
}

/* XXX: This routine may be called before dynamorio relocation when we are in a
 * fragile state and thus no globals access or use of ASSERT/LOG/STATS!
 */
/* This routine is duplicated from privload_relocate_os_privmod_data */
static void
privload_early_relocate_os_privmod_data(os_privmod_data_t *opd, byte *mod_base)
{
    if (opd->rel != NULL)
        privload_relocate_rel(opd, opd->rel, opd->rel + opd->relsz / opd->relent);

    if (opd->rela != NULL)
        privload_relocate_rela(opd, opd->rela, opd->rela + opd->relasz / opd->relaent);

    if (opd->relr != NULL)
        privload_relocate_relr(opd, opd->relr, opd->relrsz);

    if (opd->jmprel != NULL) {
        if (opd->pltrel == DT_REL) {
            privload_relocate_rel(opd, (ELF_REL_TYPE *)opd->jmprel,
                                  (ELF_REL_TYPE *)(opd->jmprel + opd->pltrelsz));
        } else if (opd->pltrel == DT_RELA) {
            privload_relocate_rela(opd, (ELF_RELA_TYPE *)opd->jmprel,
                                   (ELF_RELA_TYPE *)(opd->jmprel + opd->pltrelsz));
        } else {
            privload_report_relocate_error();
        }
    }
}
#    endif /* !defined(STANDALONE_UNIT_TEST) && !defined(STATIC_LIBRARY) */

/*  This routine is duplicated at privload_early_relocate_os_privmod_data. */
static void
privload_relocate_os_privmod_data(os_privmod_data_t *opd, byte *mod_base)
{
    if (opd->rel != NULL) {
        module_relocate_rel(mod_base, opd, opd->rel, opd->rel + opd->relsz / opd->relent);
    }
    if (opd->rela != NULL) {
        module_relocate_rela(mod_base, opd, opd->rela,
                             opd->rela + opd->relasz / opd->relaent);
    }
    if (opd->relr != NULL)
        module_relocate_relr(mod_base, opd, opd->relr, opd->relrsz);
    if (opd->jmprel != NULL) {
        app_pc jmprel_start = opd->jmprel;
        app_pc jmprel_end = opd->jmprel + opd->pltrelsz;
        /* i#5080: Some libs list JMPREL as overlapping with REL{,A} and it's implied
         * that really JMPREL comes after.
         */
        if (opd->rel != NULL && jmprel_start >= (app_pc)opd->rel &&
            jmprel_start < (app_pc)(opd->rel + opd->relsz / opd->relent))
            jmprel_start = (app_pc)(opd->rel + opd->relsz / opd->relent);
        if (opd->rela != NULL && jmprel_start >= (app_pc)opd->rela &&
            jmprel_start < (app_pc)(opd->rela + opd->relasz / opd->relaent))
            jmprel_start = (app_pc)(opd->rela + opd->relasz / opd->relaent);
        if (opd->pltrel == DT_REL) {
            module_relocate_rel(mod_base, opd, (ELF_REL_TYPE *)jmprel_start,
                                (ELF_REL_TYPE *)jmprel_end);
        } else if (opd->pltrel == DT_RELA) {
            module_relocate_rela(mod_base, opd, (ELF_RELA_TYPE *)jmprel_start,
                                 (ELF_RELA_TYPE *)jmprel_end);
        } else {
            ASSERT(false);
        }
    }
}
#endif /* LINUX */

static void
privload_relocate_mod(privmod_t *mod)
{
#ifdef LINUX
    os_privmod_data_t *opd = (os_privmod_data_t *)mod->os_privmod_data;

    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);

    LOG(GLOBAL, LOG_LOADER, 3, "relocating %s\n", mod->name);

    /* If module has tls block need update its tls offset value.
     * This must be done *before* relocating as relocating needs the os_privmod_data_t
     * TLS fields set here.
     */
    if (opd->tls_block_size != 0)
        privload_mod_tls_init(mod);

    privload_relocate_os_privmod_data(opd, mod->base);

    /* For the primary thread, we now perform TLS block copying, after relocating.
     * For subsequent threads this is done in privload_tls_init().
     */
    if (opd->tls_block_size != 0)
        privload_mod_tls_primary_thread_init(mod);
#else
    /* XXX i#1285: implement MacOS private loader */
#endif
}

static void
privload_create_os_privmod_data(privmod_t *privmod, bool dyn_reloc)
{
    os_privmod_data_t *opd;

    opd = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, os_privmod_data_t, ACCT_OTHER, PROTECTED);
    privmod->os_privmod_data = opd;

    memset(opd, 0, sizeof(*opd));

    /* walk the module's program header to get privmod information */
    module_walk_program_headers(privmod->base, privmod->size,
                                false, /* segments are remapped */
                                dyn_reloc, &opd->os_data.base_address, NULL,
                                &opd->max_end, &opd->soname, &opd->os_data);
    module_get_os_privmod_data(privmod->base, privmod->size, false /*!relocated*/, opd);
    /* We want libunwind to walk app libraries.
     * XXX: Is there a cleaner way to do this for some libraries but not others?
     */
    if (strstr(privmod->name, "libunwind") == privmod->name) {
        LOG(GLOBAL, LOG_LOADER, 2, "Using app imports for %s\n", privmod->name);
        opd->use_app_imports = true;
    }
}

static void
privload_delete_os_privmod_data(privmod_t *privmod)
{
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, privmod->os_privmod_data, os_privmod_data_t,
                   ACCT_OTHER, PROTECTED);
    privmod->os_privmod_data = NULL;
}

/* i#1589: the client lib is already on the priv lib list, so we share its
 * data with loaded_module_areas (which also avoids problems with .dynamic
 * not being relocated for priv libs).
 */
bool
privload_fill_os_module_info(app_pc base, OUT app_pc *out_base /* relative pc */,
                             OUT app_pc *out_max_end /* relative pc */,
                             OUT char **out_soname, OUT os_module_data_t *out_data)
{
    bool res = false;
    privmod_t *privmod;
    acquire_recursive_lock(&privload_lock);
    privmod = privload_lookup_by_base(base);
    if (privmod != NULL) {
        os_privmod_data_t *opd = (os_privmod_data_t *)privmod->os_privmod_data;
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
 * Function Redirection
 */

static void *
redirect_dlsym(void *handle, const char *symbol);

#if defined(LINUX) && !defined(ANDROID)
/* These are not yet supported by Android's Bionic */
void *
redirect___tls_get_addr();

void *
redirect____tls_get_addr();
#endif

#ifdef LINUX
static int
redirect_dl_iterate_phdr(int (*callback)(struct dl_phdr_info *info, size_t size,
                                         void *data),
                         void *data)
{
    int res = 0;
    struct dl_phdr_info info;
    privmod_t *mod;
    acquire_recursive_lock(&privload_lock);
    for (mod = privload_first_module(); mod != NULL; mod = privload_next_module(mod)) {
        ELF_HEADER_TYPE *elf_hdr = (ELF_HEADER_TYPE *)mod->base;
        os_privmod_data_t *opd = (os_privmod_data_t *)mod->os_privmod_data;
        /* We do want to include externally loaded (if any) and clients as
         * clients can contain C++ exception code, which will call here.
         */
        if (mod->base == get_dynamorio_dll_start())
            continue;
        info.dlpi_addr = opd->load_delta;
        info.dlpi_name = mod->path;
        info.dlpi_phdr = (ELF_PROGRAM_HEADER_TYPE *)(mod->base + elf_hdr->e_phoff);
        info.dlpi_phnum = elf_hdr->e_phnum;
        res = callback(&info, sizeof(info), data);
        if (res != 0)
            break;
    }
    release_recursive_lock(&privload_lock);
    return res;
}

/* For some cases we want the client library to walk the app libs: e.g., for
 * callstack walking (i#2414).
 */
static int
redirect_dl_iterate_phdr_app(int (*callback)(struct dl_phdr_info *info, size_t size,
                                             void *data),
                             void *data)
{
    int res = 0;
    struct dl_phdr_info info;
    module_iterator_t *iter = module_iterator_start();
    while (module_iterator_hasnext(iter)) {
        module_area_t *area = module_iterator_next(iter);
        ASSERT(area != NULL);
        ELF_HEADER_TYPE *elf_hdr = (ELF_HEADER_TYPE *)area->start;
        /* We do want to include externally loaded (if any) and clients as
         * clients can contain C++ exception code, which will call here.
         */
        if (area->start == get_dynamorio_dll_start())
            continue;
        app_pc preferred_base =
            IF_WINDOWS_ELSE(area->os_data.preferred_base, area->os_data.base_address);
        info.dlpi_addr = area->start - preferred_base;
        info.dlpi_name = area->full_path;
        info.dlpi_phdr = (ELF_PROGRAM_HEADER_TYPE *)(area->start + elf_hdr->e_phoff);
        info.dlpi_phnum = elf_hdr->e_phnum;
        /* XXX: Fill in new fields dlpi_{adds,subs,tls_modid,tls_data}.
         * For now we set the size to exclude them.
         */
        size_t size = offsetof(struct dl_phdr_info, dlpi_phnum) + sizeof(info.dlpi_phnum);
        res = callback(&info, size, data);
        if (res != 0)
            break;
    }
    module_iterator_stop(iter);
    // XXX: Pass the private ones too for callstacks or other purposes?  Sometimes
    // private code is used to replace app code, though we do already have the client
    // lib itself on the app list.
    return res;
}

#    if defined(ARM) && !defined(ANDROID)
typedef struct _unwind_callback_data_t {
    void *pc;
    void *base;
    int size;
} unwind_callback_data_t;

/* Find the exception unwind table (exidx) of the image that contains the
 * exception pc.
 */
int
exidx_lookup_callback(struct dl_phdr_info *info, size_t size, void *data)
{
    int i;
    int res = 0;
    unwind_callback_data_t *ucd;
    if (data == NULL || size != sizeof(*info))
        return res;
    ucd = (unwind_callback_data_t *)data;
    for (i = 0; i < info->dlpi_phnum; i++) {
        /* look for the table */
        if (info->dlpi_phdr[i].p_type == PT_ARM_EXIDX) {
            /* the location and size of the table for the image */
            ucd->base = (void *)(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr);
            ucd->size = info->dlpi_phdr[i].p_memsz;
        }
        /* look for the segment */
        if (res == 0 && info->dlpi_phdr[i].p_type == PT_LOAD) {
            if (ucd->pc >= (void *)(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr) &&
                ucd->pc < (void *)(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr +
                                   info->dlpi_phdr[i].p_memsz)) {
                res = 1;
            }
        }
    }
    return res;
}

/* find the exception unwind table that contains the PC during an exception */
void *
redirect___gnu_Unwind_Find_exidx(void *pc, int *count)
{
    unwind_callback_data_t ucd;
    memset(&ucd, 0, sizeof(ucd));
    ucd.pc = pc;
    if (redirect_dl_iterate_phdr(exidx_lookup_callback, &ucd) <= 0)
        return NULL;
    if (count != NULL)
        *count = ucd.size / 8 /* exidx table entry size */;
    return ucd.base;
}
#    endif /* ARM && !ANDROID */
#endif     /* LINUX */

typedef struct _redirect_import_t {
    const char *name;
    app_pc func;
    app_pc app_func; /* Used only for dl_iterate_phdr over app libs, so far. */
} redirect_import_t;

static const redirect_import_t redirect_imports[] = {
    { "calloc", (app_pc)redirect_calloc },
    { "malloc", (app_pc)redirect_malloc },
    { "free", (app_pc)redirect_free },
    { "realloc", (app_pc)redirect_realloc },
    { "strdup", (app_pc)redirect_strdup },
    /* TODO i#4243: we should also redirect functions including:
     * + malloc_usable_size, memalign, valloc, mallinfo, mallopt, etc.
     * + tcmalloc: tc_malloc, tc_free, etc.
     * + __libc_malloc, __libc_free, etc.
     * + OSX: malloc_zone_malloc, etc.?  Or just malloc_create_zone?
     * + C++ operators in case they don't just call libc malloc?
     */
    /* We redirect these for fd isolation. */
    { "open", (app_pc)os_open_protected },
    { "close", (app_pc)os_close_protected },
    /* These libc routines can call pthread functions and cause hangs (i#4928) so
     * we use our syscall wrappers instead.
     */
    { "read", (app_pc)os_read },
    { "write", (app_pc)os_write },
#if defined(LINUX) && !defined(ANDROID)
    { "__tls_get_addr", (app_pc)redirect___tls_get_addr },
    { "___tls_get_addr", (app_pc)redirect____tls_get_addr },
#endif
#ifdef LINUX
    /* i#1717: C++ exceptions call this */
    { "dl_iterate_phdr", (app_pc)redirect_dl_iterate_phdr,
      (app_pc)redirect_dl_iterate_phdr_app },
#    if defined(ARM) && !defined(ANDROID)
    /* i#1717: C++ exceptions call this on ARM Linux */
    { "__gnu_Unwind_Find_exidx", (app_pc)redirect___gnu_Unwind_Find_exidx },
#    endif
#endif
    { "dlsym", (app_pc)redirect_dlsym },
    /* We need these for clients that don't use libc (i#1747) */
    { "strlen", (app_pc)strlen },
    { "wcslen", (app_pc)wcslen },
    { "strchr", (app_pc)strchr },
    { "strrchr", (app_pc)strrchr },
    { "strncpy", (app_pc)strncpy },
    { "memcpy", (app_pc)memcpy },
    { "memset", (app_pc)memset },
    { "memmove", (app_pc)memmove },
    { "strncat", (app_pc)strncat },
    { "strcmp", (app_pc)strcmp },
    { "strncmp", (app_pc)strncmp },
    { "memcmp", (app_pc)memcmp },
    { "strstr", (app_pc)strstr },
    { "strcasecmp", (app_pc)strcasecmp },
    /* Also redirect the _chk versions (i#1747, i#46) */
    { "memcpy_chk", (app_pc)memcpy },
    { "memset_chk", (app_pc)memset },
    { "memmove_chk", (app_pc)memmove },
    { "strncpy_chk", (app_pc)strncpy },
};
#define REDIRECT_IMPORTS_NUM (sizeof(redirect_imports) / sizeof(redirect_imports[0]))

#ifdef DEBUG
static const redirect_import_t redirect_debug_imports[] = {
    { "calloc", (app_pc)redirect_calloc_initonly },
    { "malloc", (app_pc)redirect_malloc_initonly },
    { "free", (app_pc)redirect_free_initonly },
    { "realloc", (app_pc)redirect_realloc_initonly },
    { "strdup", (app_pc)redirect_strdup_initonly },
};
#    define REDIRECT_DEBUG_IMPORTS_NUM \
        (sizeof(redirect_debug_imports) / sizeof(redirect_debug_imports[0]))
#endif

bool
privload_redirect_sym(os_privmod_data_t *opd, ptr_uint_t *r_addr, const char *name)
{
    int i;
    /* iterate over all symbols and redirect syms when necessary, e.g. malloc */
#ifdef DEBUG
    if (disallow_unsafe_static_calls) {
        for (i = 0; i < REDIRECT_DEBUG_IMPORTS_NUM; i++) {
            if (strcmp(redirect_debug_imports[i].name, name) == 0) {
                *r_addr = (ptr_uint_t)redirect_debug_imports[i].func;
                return true;
            }
        }
    }
#endif
    for (i = 0; i < REDIRECT_IMPORTS_NUM; i++) {
        if (strcmp(redirect_imports[i].name, name) == 0) {
            if (opd->use_app_imports && redirect_imports[i].app_func != NULL)
                *r_addr = (ptr_uint_t)redirect_imports[i].app_func;
            else
                *r_addr = (ptr_uint_t)redirect_imports[i].func;
            return true;
        }
    }
    return false;
}

static void *
redirect_dlsym(void *handle, const char *symbol)
{
    for (int i = 0; i < REDIRECT_IMPORTS_NUM; i++) {
        if (strcmp(redirect_imports[i].name, symbol) == 0)
            return redirect_imports[i].func;
    }
    /* TODO: Look in other libs via module_lookup_symbol() from module_elf.c. */
    SYSLOG_INTERNAL_WARNING("dlsym(%s) called by private lib; returning NULL", symbol);
    return NULL;
}

/***************************************************************************
 * DynamoRIO Early Injection Code
 */

#ifdef LINUX
#    if !defined(STANDALONE_UNIT_TEST) && !defined(STATIC_LIBRARY)
/* Find the auxiliary vector and adjust it to look as if the kernel had set up
 * the stack for the ELF mapped at map.  The auxiliary vector starts after the
 * terminating NULL pointer in the envp array.
 */
static void
privload_setup_auxv(char **envp, app_pc map, ptr_int_t delta, app_pc interp_map,
                    const char *exe_path /*must be persistent*/)
{
    ELF_AUXV_TYPE *auxv;
    ELF_HEADER_TYPE *elf = (ELF_HEADER_TYPE *)map;

    /* The aux vector is after the last environment pointer. */
    while (*envp != NULL)
        envp++;
    auxv = (ELF_AUXV_TYPE *)(envp + 1);

    /* fix up the auxv entries that refer to the executable */
    for (; auxv->a_type != AT_NULL; auxv++) {
        /* the actual addr should be: (base + offs) or (v_addr + delta) */
        switch (auxv->a_type) {
        case AT_ENTRY:
            auxv->a_un.a_val = (ptr_int_t)elf->e_entry + delta;
            LOG(GLOBAL, LOG_LOADER, 2, "AT_ENTRY: " PFX "\n", auxv->a_un.a_val);
            break;
        case AT_PHDR:
            auxv->a_un.a_val = (ptr_int_t)map + elf->e_phoff;
            LOG(GLOBAL, LOG_LOADER, 2, "AT_PHDR: " PFX "\n", auxv->a_un.a_val);
            break;
        case AT_PHENT: auxv->a_un.a_val = (ptr_int_t)elf->e_phentsize; break;
        case AT_PHNUM: auxv->a_un.a_val = (ptr_int_t)elf->e_phnum; break;
        case AT_BASE: /* Android loader reads this */
            auxv->a_un.a_val = (ptr_int_t)interp_map;
            LOG(GLOBAL, LOG_LOADER, 2, "AT_BASE: " PFX "\n", auxv->a_un.a_val);
            break;
        case AT_EXECFN: /* Android loader references this, unclear what for */
            auxv->a_un.a_val = (ptr_int_t)exe_path;
            LOG(GLOBAL, LOG_LOADER, 2, "AT_EXECFN: " PFX " %s\n", auxv->a_un.a_val,
                (char *)auxv->a_un.a_val);
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
    static char home_var[MAXIMUM_PATH + 6 /*HOME=path\0*/];
    static char *fake_envp[] = { home_var, NULL };

    /* When we come in via ptrace, we have no idea where the environment
     * pointer is.  We could use /proc/self/environ to read it or go searching
     * near the stack base.  However, both are fragile and we don't really need
     * the environment for anything except for option passing.  In the initial
     * ptraced process, we can assume our options are in a config file and not
     * the environment, so we just set an environment with HOME.
     */
    snprintf(home_var, BUFFER_SIZE_ELEMENTS(home_var), "HOME=%s", args->home_dir);
    NULL_TERMINATE_BUFFER(home_var);
    dynamorio_set_envp(fake_envp);

    dynamo_control_via_attach = true;

    dynamorio_app_init();

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

byte *
map_exe_file_and_brk(file_t f, size_t *size INOUT, uint64 offs, app_pc addr, uint prot,
                     map_flags_t map_flags)
{
    /* A little hacky: we assume the MEMPROT_NONE is the overall mmap for the whole
     * region, where our goal is to push it back for top-down PIE filling to leave
     * room for a reasonable brk.
     */
    if (prot == MEMPROT_NONE && offs == 0) {
        size_t sz_with_brk = *size + APP_BRK_GAP;
        byte *res = os_map_file(f, &sz_with_brk, offs, addr, prot, map_flags);
        if (res != NULL)
            os_unmap_file(res + sz_with_brk - APP_BRK_GAP, APP_BRK_GAP);
        *size = sz_with_brk - APP_BRK_GAP;
        return res;
    } else
        return os_map_file(f, size, offs, addr, prot, map_flags);
}

/* XXX: This routine is called before dynamorio relocation when we are in a
 * fragile state and thus no globals access or use of ASSERT/LOG/STATS!
 */
/* This routine is partially duplicated from module_get_os_privmod_data.
 * It paritially fills the os_privmod_data for dynamorio relocation.
 * Return true if relocation is required.
 */
static bool
privload_get_os_privmod_data(app_pc base, OUT os_privmod_data_t *opd)
{
    app_pc mod_base, mod_end;
    ELF_HEADER_TYPE *elf_hdr = (ELF_HEADER_TYPE *)base;
    ELF_PROGRAM_HEADER_TYPE *prog_hdr;
    uint i;

    /* walk program headers to get mod_base mod_end and delta */
    mod_base = module_vaddr_from_prog_header(base + elf_hdr->e_phoff, elf_hdr->e_phnum,
                                             NULL, &mod_end);
    /* delta from preferred address, used for calcuate real address */
    opd->load_delta = base - mod_base;

    /* At this point one could consider returning false if the load_delta
     * is zero. However, this optimisation was found to give only a small
     * benefit, and is not safe if RELA relocations are in use. In particular,
     * it did not work on AArch64 when libdynamorio.so was built with the BFD
     * linker from Debian's binutils 2.26-8.
     */

    /* walk program headers to get dynamic section pointer */
    prog_hdr = (ELF_PROGRAM_HEADER_TYPE *)(base + elf_hdr->e_phoff);
    for (i = 0; i < elf_hdr->e_phnum; i++) {
        if (prog_hdr->p_type == PT_DYNAMIC) {
            opd->dyn = (ELF_DYNAMIC_ENTRY_TYPE *)(prog_hdr->p_vaddr + opd->load_delta);
            opd->dynsz = prog_hdr->p_memsz;
#        ifdef DEBUG
        } else if (prog_hdr->p_type == PT_TLS && prog_hdr->p_memsz > 0) {
            /* XXX: we assume libdynamorio has no tls block b/c we're not calling
             * privload_relocate_mod().
             */
            privload_report_relocate_error();
#        endif /* DEBUG */
        }
        ++prog_hdr;
    }
    if (opd->dyn == NULL)
        return false;

    module_init_os_privmod_data_from_dyn(opd, opd->dyn, opd->load_delta);
    return true;
}

/* XXX: This routine is called before dynamorio relocation when we are in a
 * fragile state and thus no globals access or use of ASSERT/LOG/STATS!
 */
/* This routine is duplicated from is_elf_so_header_common. */
static bool
privload_mem_is_elf_so_header(byte *mem)
{
    /* assume we can directly read from mem */
    ELF_HEADER_TYPE *elf_hdr = (ELF_HEADER_TYPE *)mem;

    /* ELF magic number */
    if (elf_hdr->e_ident[EI_MAG0] != ELFMAG0 || elf_hdr->e_ident[EI_MAG1] != ELFMAG1 ||
        elf_hdr->e_ident[EI_MAG2] != ELFMAG2 || elf_hdr->e_ident[EI_MAG3] != ELFMAG3)
        return false;
    /* libdynamorio should be ET_DYN */
    if (elf_hdr->e_type != ET_DYN)
        return false;
    /* ARM or X86 */
    /* i#1684: We do allow mixing arches of the same bitwidth. See the i#1684
     * comment in is_elf_so_header_common().
     */
    if (
#        ifdef X64
        elf_hdr->e_machine != EM_X86_64 && elf_hdr->e_machine != EM_AARCH64 &&
        elf_hdr->e_machine != EM_RISCV
#        else
        elf_hdr->e_machine != EM_386 && elf_hdr->e_machine != EM_ARM
#        endif
    )
        return false;
    if (elf_hdr->e_ehsize != sizeof(ELF_HEADER_TYPE))
        return false;
    return true;
}

/* Returns false if the text-data gap is not empty.  Else, fills the gap with
 * no-access mappings and returns true.
 */
static bool
dynamorio_lib_gap_empty(void)
{
    /* XXX: get_dynamorio_dll_start() is already calling
     * memquery_library_bounds_by_iterator() which is doing this maps walk: can we
     * avoid this extra walk by somehow passing info back to us?  Have an
     * "interrupted" output param or sthg and is_dynamorio_dll_interrupted()?
     */
    memquery_iter_t iter;
    bool res = true;
    if (memquery_iterator_start(&iter, NULL, false /*no heap*/)) {
        byte *dr_start = get_dynamorio_dll_start();
        byte *dr_end = get_dynamorio_dll_end();
        byte *gap_start = dr_start;
        const char *dynamorio_library_path = get_dynamorio_library_path();
        while (memquery_iterator_next(&iter) && iter.vm_start < dr_end) {
            if (iter.vm_start >= dr_start && iter.vm_end <= dr_end &&
                iter.comment[0] != '\0' &&
                /* i#3799: ignore the kernel labeling DR's .bss as "[heap]". */
                strcmp(iter.comment, "[heap]") != 0 &&
                strcmp(iter.comment, dynamorio_library_path) != 0) {
                /* There's a non-anon mapping inside: probably vvar and/or vdso. */
                res = false;
                break;
            }
            /* i#1659: fill in the text-data segment gap to ensure no mmaps in between.
             * The kernel does not do this.  Our private loader does, so if we reloaded
             * ourselves this is already in place.  We do this now rather than in
             * os_loader_init_prologue() to prevent our brk mmap from landing here.
             */
            if (iter.vm_start > gap_start) {
                size_t sz = iter.vm_start - gap_start;
                ASSERT(sz > 0);
                DEBUG_DECLARE(byte *fill =)
                os_map_file(-1, &sz, 0, gap_start, MEMPROT_NONE,
                            MAP_FILE_COPY_ON_WRITE | MAP_FILE_FIXED);
                ASSERT(fill != NULL);
                gap_start = iter.vm_end;
            } else if (iter.vm_end > gap_start) {
                gap_start = iter.vm_end;
            }
        }
        memquery_iterator_stop(&iter);
    }
    return res;
}

/* XXX: This routine is called before dynamorio relocation when we are in a
 * fragile state and thus no globals access or use of ASSERT/LOG/STATS!
 */
void
relocate_dynamorio(byte *dr_map, size_t dr_size, byte *sp)
{
    ptr_uint_t argc = *(ptr_uint_t *)sp;
    /* Plus 2 to skip argc and null pointer that terminates argv[]. */
    const char **env = (const char **)sp + argc + 2;
    os_privmod_data_t opd = { { 0 } };

    /* We can't use PAGE_SIZE as that may require relocations to access. */
    const int min_page_size = 4096;

    if (dr_map == NULL) {
        /* We can't start with the address of relocate_dynamorio or something as that
         * may require relocations to access!
         */
        GET_CUR_PC(dr_map);
        /* we do not know where dynamorio is, so check backward page by page */
        dr_map = (app_pc)ALIGN_BACKWARD(dr_map, min_page_size);
        while (dr_map != NULL && !privload_mem_is_elf_so_header(dr_map)) {
            dr_map -= min_page_size;
        }
    }
    if (dr_map == NULL)
        privload_report_relocate_error();

    /* Relocate it */
    if (privload_get_os_privmod_data(dr_map, &opd))
        privload_early_relocate_os_privmod_data(&opd, dr_map);

    os_page_size_init(env, true);
}

/* i#1227: on a conflict with the app we reload ourselves.
 * Does not return.
 */
static void
reload_dynamorio(void **init_sp, app_pc conflict_start, app_pc conflict_end)
{
    elf_loader_t dr_ld;
    os_privmod_data_t opd;
    byte *dr_map;
    /* We expect at most vvar+vdso+stack+vsyscall => 5 different mappings
     * even if they were all in the conflict area.
     */
#        define MAX_TEMP_MAPS 16
    byte *temp_map[MAX_TEMP_MAPS];
    size_t temp_size[MAX_TEMP_MAPS];
    uint num_temp_maps = 0, i;
    memquery_iter_t iter;
    app_pc entry;
    byte *cur_dr_map = get_dynamorio_dll_start();
    byte *cur_dr_end = get_dynamorio_dll_end();
    size_t dr_size = cur_dr_end - cur_dr_map;
    IF_DEBUG(bool success =)
    elf_loader_read_headers(&dr_ld, get_dynamorio_library_path());
    ASSERT(success);

    /* XXX: have better strategy for picking base: currently we rely on
     * the kernel picking an address, so we have to block out the conflicting
     * region first, avoiding any existing mappings (like vvar+vdso: i#2641).
     */
    if (memquery_iterator_start(&iter, NULL, false /*no heap*/)) {
        /* Strategy: track the leading edge ("tocover_start") of the conflict region.
         * Find the next block beyond that edge so we know the safe endpoint for a
         * temp mmap.
         */
        byte *tocover_start = conflict_start;
        while (memquery_iterator_next(&iter)) {
            if (iter.vm_start > tocover_start) {
                temp_map[num_temp_maps] = tocover_start;
                temp_size[num_temp_maps] =
                    MIN(iter.vm_start, conflict_end) - tocover_start;
                tocover_start = iter.vm_end;
                if (temp_size[num_temp_maps] > 0) {
                    temp_map[num_temp_maps] = os_map_file(
                        -1, &temp_size[num_temp_maps], 0, temp_map[num_temp_maps],
                        MEMPROT_NONE, MAP_FILE_COPY_ON_WRITE | MAP_FILE_FIXED);
                    ASSERT(temp_map[num_temp_maps] != NULL);
                    num_temp_maps++;
                }
            } else if (iter.vm_end > tocover_start) {
                tocover_start = iter.vm_end;
            }
            if (iter.vm_start >= conflict_end)
                break;
        }
        memquery_iterator_stop(&iter);
        if (tocover_start < conflict_end) {
            temp_map[num_temp_maps] = tocover_start;
            temp_size[num_temp_maps] = conflict_end - tocover_start;
            temp_map[num_temp_maps] =
                os_map_file(-1, &temp_size[num_temp_maps], 0, temp_map[num_temp_maps],
                            MEMPROT_NONE, MAP_FILE_COPY_ON_WRITE | MAP_FILE_FIXED);
            ASSERT(temp_map[num_temp_maps] != NULL);
            num_temp_maps++;
        }
    }

    /* Now load the 2nd libdynamorio.so */
    dr_map = elf_loader_map_phdrs(&dr_ld, false /*!fixed*/, os_map_file, os_unmap_file,
                                  os_set_protection, privload_check_new_map_bounds,
                                  memset, privload_map_flags(0 /*!reachable*/));
    ASSERT(dr_map != NULL);
    ASSERT(is_elf_so_header(dr_map, 0));

    /* Relocate it */
    memset(&opd, 0, sizeof(opd));
    module_get_os_privmod_data(dr_map, dr_size, false /*!relocated*/, &opd);
    /* XXX: we assume libdynamorio has no tls block b/c we're not calling
     * privload_relocate_mod().
     */
    ASSERT(opd.tls_block_size == 0);
    privload_relocate_os_privmod_data(&opd, dr_map);

    for (i = 0; i < num_temp_maps; i++)
        os_unmap_file(temp_map[i], temp_size[i]);

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
 * that the kernel puts on the stack.  The 2nd & 3rd args must be 0 in
 * the initial call.
 *
 * We assume that _start has already called relocate_dynamorio() for us and
 * that it is now safe to access globals.
 */
void
privload_early_inject(void **sp, byte *old_libdr_base, size_t old_libdr_size)
{
    ptr_int_t *argc = (ptr_int_t *)sp; /* Kernel writes an elf_addr_t. */
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

    if (*argc == ARGC_PTRACE_SENTINEL) {
        /* XXX: Teach the injector to look up takeover_ptrace() and call it
         * directly instead of using this sentinel.  We come here because we
         * can easily find the address of _start in the ELF header.
         */
        takeover_ptrace((ptrace_stack_args_t *)sp);
        ASSERT_NOT_REACHED();
    }

    kernel_init_sp = (void *)sp;

    /* XXX i#47: for Linux, we can't easily have this option on by default as
     * code like get_application_short_name() called from drpreload before
     * even _init is run needs to have a non-early default.
     */
    dynamo_options.early_inject = true;

    /* i#1227: if we reloaded ourselves, unload the old libdynamorio */
    if (old_libdr_base != NULL) {
        /* i#2641: we can't blindly unload the whole region as vvar+vdso may be
         * in the text-data gap.
         */
        const char *dynamorio_library_path = get_dynamorio_library_path();
        if (memquery_iterator_start(&iter, NULL, false /*no heap*/)) {
            while (memquery_iterator_next(&iter)) {
                if (iter.vm_start >= old_libdr_base &&
                    iter.vm_end <= old_libdr_base + old_libdr_size &&
                    (iter.comment[0] == '\0' /* .bss */ ||
                     /* The kernel sometimes mis-labels our .bss as "[heap]". */
                     strcmp(iter.comment, "[heap]") == 0 ||
                     strcmp(iter.comment, dynamorio_library_path) == 0)) {
                    os_unmap_file(iter.vm_start, iter.vm_end - iter.vm_start);
                }
                if (iter.vm_start >= old_libdr_base + old_libdr_size)
                    break;
            }
            memquery_iterator_stop(&iter);
        }
    }

    dynamorio_set_envp(envp);

    /* argv[0] doesn't actually have to be the path to the exe, so we put the
     * real exe path in an environment variable.
     */
    exe_path = getenv(DYNAMORIO_VAR_EXE_PATH);
    /* i#1677: this happens upon re-launching within gdb, so provide a nice error */
    if (exe_path == NULL) {
        /* i#1677: avoid assert in get_application_name_helper() */
        set_executable_path("UNKNOWN");
        apicheck(exe_path != NULL,
                 DYNAMORIO_VAR_EXE_PATH " env var is not set.  "
                                        "Are you re-launching within gdb?");
    }

    /* i#907: We can't rely on /proc/self/exe for the executable path, so we
     * have to tell get_application_name() to use this path.
     */
    set_executable_path(exe_path);

    /* XXX i#2662: Currently, we only support getting args for early injection.
     * Add support for late injection.
     */
    set_app_args((int *)argc, argv);

    success = elf_loader_read_headers(&exe_ld, exe_path);
    apicheck(success,
             "Failed to read app ELF headers.  Check path and "
             "architecture.");

    /* Initialize DR's options to avoid syslogs in get_dynamo_library_bounds() and
     * for the -xarch_root option below.
     */
    dynamorio_app_init_part_one_options();

    /* Find range of app */
    exe_map = module_vaddr_from_prog_header((app_pc)exe_ld.phdrs, exe_ld.ehdr->e_phnum,
                                            NULL, &exe_end);
    /* i#1227: on a conflict with the app (+ room for the brk): reload ourselves */
    if (get_dynamorio_dll_start() < exe_end + APP_BRK_GAP &&
        get_dynamorio_dll_end() > exe_map) {
        elf_loader_destroy(&exe_ld);
        reload_dynamorio(sp, exe_map, exe_end + APP_BRK_GAP);
        ASSERT_NOT_REACHED();
    }
    /* i#2641: we can't handle something in the text-data gap.
     * Various parts of DR assume there's nothing inside (and we even fill the
     * gap with a PROT_NONE mmap later: i#1659), so we reload to avoid it,
     * under the assumption that it's rare and we're not paying this cost
     * very often.
     */
    if (!dynamorio_lib_gap_empty()) {
        elf_loader_destroy(&exe_ld);
        reload_dynamorio(sp, get_dynamorio_dll_start(), get_dynamorio_dll_end());
        ASSERT_NOT_REACHED();
    }

    exe_map = elf_loader_map_phdrs(&exe_ld,
                                   /* fixed at preferred address,
                                    * will be overridden if preferred base is 0
                                    */
                                   true,
                                   /* ensure there's space for the brk */
                                   map_exe_file_and_brk, os_unmap_file, os_set_protection,
                                   privload_check_new_map_bounds, memset,
                                   privload_map_flags(MODLOAD_IS_APP /*!reachable*/));
    apicheck(exe_map != NULL,
             "Failed to load application.  "
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
    if (memquery_iterator_start(&iter, exe_map, false /*no heap*/)) {
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
    dynamorio_syscall(SYS_prctl, 5, PR_SET_NAME, (ptr_uint_t)exe_basename, 0, 0, 0);

    reserve_brk(exe_map + exe_ld.image_size +
                (INTERNAL_OPTION(separate_private_bss) ? PAGE_SIZE : 0));

    interp = elf_loader_find_pt_interp(&exe_ld);
    if (interp != NULL) {
        char buf[MAXIMUM_PATH];
        if (!IS_STRING_OPTION_EMPTY(xarch_root) && !os_file_exists(interp, false)) {
            string_option_read_lock();
            snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%s/%s", DYNAMO_OPTION(xarch_root),
                     interp);
            NULL_TERMINATE_BUFFER(buf);
            string_option_read_unlock();
            if (os_file_exists(buf, false)) {
                LOG(GLOBAL, LOG_SYSCALLS, 2, "replacing interpreter |%s| with |%s|\n",
                    interp, buf);
                interp = buf;
            }
        }
        /* Load the ELF pointed at by PT_INTERP, usually ld.so. */
        elf_loader_t interp_ld;
        success = elf_loader_read_headers(&interp_ld, interp);
        apicheck(success, "Failed to read ELF interpreter headers.");
        interp_map = elf_loader_map_phdrs(
            &interp_ld, false /* fixed */, os_map_file, os_unmap_file, os_set_protection,
            privload_check_new_map_bounds, memset,
            privload_map_flags(MODLOAD_IS_APP /*!reachable*/));
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

    /* Initialize the rest of DR *after* we map the app and interp images.  This is
     * consistent with our old behavior, and allows the client to do things like call
     * dr_get_proc_address() on the app from dr_client_main().  We let
     * find_executable_vm_areas re-discover the mappings we made for the app and
     * interp images.  We do not do the full init before mapping the interp image
     * as it complicates recording the mappings for the interp.
     */
    if (dynamorio_app_init_part_two_finalize() != SUCCESS)
        apicheck(false, "Failed to initialize part two.");

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
#        ifdef X86
        asm("mov %0, %%" ASM_XSP "\n\t"
            "jmp *%1\n\t"
            :
            : "r"(sp), "r"(entry));
#        elif defined(ARM)
        /* FIXME i#1551: NYI on ARM */
        ASSERT_NOT_REACHED();
#        endif
    }

    memset(&mc, 0, sizeof(mc));
    mc.xsp = (reg_t)sp;
    mc.pc = entry;
    dynamo_start(&mc);
}
#    endif /* !defined(STANDALONE_UNIT_TEST) && !defined(STATIC_LIBRARY) */
#else
/* XXX i#1285: implement MacOS private loader */
void
relocate_dynamorio(byte *dr_map, size_t dr_size, byte *sp)
{
    ASSERT_NOT_IMPLEMENTED(false);
}

void
privload_early_inject(void **sp, byte *old_libdr_base, size_t old_libdr_size)
{
    ASSERT_NOT_IMPLEMENTED(false);
}
#endif
